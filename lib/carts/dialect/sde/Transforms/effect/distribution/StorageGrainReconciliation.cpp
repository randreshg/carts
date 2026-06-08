///==========================================================================///
/// File: StorageGrainReconciliation.cpp
///
/// SDE storage-grain authority: makes the data-parallel writers of an array
/// agree on ONE block DB grain so the array distributes instead of coarsening
/// to a host_whole DB. Runs after DistributionPlanning and before the
/// CODIR/ARTS consumers of physicalBlockShape.
///
/// Phase A (author): DistributionPlanning leaves some data-parallel su_iterate
/// writers unplanned (notably single-owner-dim arrays, which its
/// stampBudgetReconciledPlan rejects via the `ownerDims.size() < 2` guard).
/// Author an owner/block plan for them from the committed node-agnostic budget
/// grain, clamped per owner dim to the realized loop step.
///
/// Phase B (reconcile): when several writers of one array committed DIFFERENT
/// physicalBlockShapes (e.g. an init tiled to step 4 and a pipeline kernel to
/// step 8), the ARTS DB materializer cannot share one per-block DB and the
/// array coarsens. Rewrite the writers to the per-dim GCD of their blocks and
/// keep the committed partition evidence consistent.
///
/// The GCD is <= every writer's step (the stampers pin block == step), so
/// block <= step still holds and no loop retile is needed. Only
/// physicalBlockShape (DB/MU grain) is rewritten; logicalWorkerSlice (CU grain)
/// is left per-writer.
///
/// Fail-closed: matmul/stencil families, in-place reuse, any array a
/// stencil/matmul SU touches, mixed-orientation arrays, single-writer arrays,
/// and already-coherent arrays are byte-identical no-ops.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_STORAGEGRAINRECONCILIATION
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/StructuredOpAnalysis.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/Debug.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/MapVector.h"

#include <numeric>

ARTS_DEBUG_SETUP(sde_storage_grain_reconciliation);

using namespace mlir;
using namespace mlir::carts;

namespace {

// A committed-plan su_iterate writer of one array.
struct Writer {
  sde::SdeSuIterateOp op;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> blockShape;
};

// Families that own a bespoke, layout-irreducible grain (matmul contraction,
// stencil owner-tile + halo) and must never be folded into a shared block DB.
static bool isHardExcludedFamily(sde::SdeSuIterateOp op) {
  if (auto cls = op.getStructuredClassification())
    if (*cls == sde::SdeStructuredClassification::matmul ||
        *cls == sde::SdeStructuredClassification::stencil)
      return true;
  auto pat = op.getPattern();
  return pat && *pat == sde::SdePattern::matmul;
}

// The op's single write-role array fact, or nullopt if it writes zero or more
// than one distributable array (matches DistributionPlanning's selector, so the
// join id is the same one it saw).
static std::optional<sde::LayoutGraphFact>
singleWriteFact(sde::SdeSuIterateOp op) {
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return std::nullopt;
  std::optional<sde::LayoutGraphFact> selected;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    if (fact.role != sde::LayoutGraphRole::write || fact.ownerDims.empty() ||
        fact.blockShape.empty())
      continue;
    if (selected)
      return std::nullopt;
    selected = fact;
  }
  return selected;
}

// Every external store the SU performs indexes its owner dims through the loop
// IV (a full-coverage affine-disjoint write, not a scatter or partial write).
// Mirrors the owner-coverage gate the DistributionPlanning/Tiling budget
// stampers apply before committing a block grain.
static bool allExternalStoresCoverOwnerDims(sde::SdeSuIterateOp op,
                                            ArrayRef<int64_t> ownerDims) {
  if (!op || ownerDims.empty() || op.getBody().empty())
    return false;
  auto loopIvs = op.getLoopInductionVars();
  if (!loopIvs || loopIvs->empty() || ownerDims.size() > loopIvs->size())
    return false;

  bool sawExternalStore = false;
  bool rejected = false;
  op.getBody().walk([&](memref::StoreOp storeOp) {
    if (rejected)
      return;
    Value root =
        ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storeOp.getMemref());
    if (!root || sde::isDefinedInside(op.getOperation(), root))
      return;
    auto memrefType = dyn_cast<MemRefType>(root.getType());
    if (!memrefType || memrefType.getRank() == 0) {
      rejected = true;
      return;
    }
    sawExternalStore = true;
    OperandRange indices = storeOp.getIndices();
    for (auto [ownerSlot, ownerDim] : llvm::enumerate(ownerDims)) {
      if (ownerDim < 0 || static_cast<unsigned>(ownerDim) >= indices.size() ||
          !sde::isOwnerDependentIndex(indices[ownerDim], (*loopIvs)[ownerSlot])) {
        rejected = true;
        return;
      }
    }
  });
  return sawExternalStore && !rejected;
}

// An affine-disjoint multi-store data-parallel writer (e.g. an init loop
// `a[i]=..;b[i]=..;c[i]=..`): it writes more than one DISTINCT distributable
// array, every write block-parallel with identical owner dims and the same
// node-agnostic budget grain. singleWriteFact deliberately declines these (>1
// write fact), so DistributionPlanning's per-array budget stamper and Phase A
// skip them and the writer keeps a coarse worker-tiling grain while the single
// write kernels of the SAME arrays commit the finer budget grain; the divergent
// grain splits the host bridge. Returns a representative fact carrying the
// shared owner dims + budget grain, or nullopt (fail closed) when the writes are
// not a uniform set of distinct block-parallel budget arrays.
static std::optional<sde::LayoutGraphFact>
affineDisjointMultiStoreBudgetFact(sde::SdeSuIterateOp op) {
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return std::nullopt;
  std::optional<sde::LayoutGraphFact> rep;
  llvm::SmallDenseSet<int64_t, 4> writtenIds;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    if (fact.role != sde::LayoutGraphRole::write)
      continue;
    if (fact.id < 0 || fact.layoutKind != sde::ArrayLayoutKind::blockParallel ||
        fact.ownerDims.empty() || fact.budgetBlockShape.empty())
      return std::nullopt;
    if (!writtenIds.insert(fact.id).second)
      return std::nullopt; // same id written twice => true multi-writer
    if (!rep) {
      rep = fact;
      continue;
    }
    if (rep->ownerDims != fact.ownerDims ||
        rep->budgetBlockShape != fact.budgetBlockShape)
      return std::nullopt; // arrays disagree on grain/owner => no shared block
  }
  if (!rep || writtenIds.size() < 2)
    return std::nullopt;
  return rep;
}

static bool hasPhysicalPlan(sde::SdeSuIterateOp op) {
  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(op.getPhysicalOwnerDimsAttr());
  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(op.getPhysicalBlockShapeAttr());
  return ownerDims && !ownerDims->empty() && blockShape && !blockShape->empty();
}

static bool isDataParallel(sde::SdeSuIterateOp op) {
  auto cls = op.getStructuredClassification();
  return cls &&
         (*cls == sde::SdeStructuredClassification::elementwise ||
          *cls == sde::SdeStructuredClassification::elementwise_pipeline);
}

// Stamp an owner/block physical plan. No partition evidence is written (none
// exists for an unplanned SU; the verifier only checks evidence when present);
// CODIR/ARTS distribute the DB from physicalOwnerDims + physicalBlockShape.
static void authorPlan(sde::SdeSuIterateOp op, ArrayRef<int64_t> ownerDims,
                       ArrayRef<int64_t> block, MLIRContext *ctx) {
  op.setPhysicalOwnerDimsAttr(buildI64ArrayAttr(ctx, ownerDims));
  op.setPhysicalBlockShapeAttr(buildI64ArrayAttr(ctx, block));
  op.setLogicalWorkerSliceAttr(buildI64ArrayAttr(ctx, block));
  op.setIterationTopologyAttr(sde::SdeIterationTopologyAttr::get(
      ctx, ownerDims.size() > 1 ? sde::SdeIterationTopology::owner_tile
                                : sde::SdeIterationTopology::owner_strip));
}

// How many reconciled MU blocks tile one of the writer's old blocks: product of
// oldBlock[d] / B[d] over the owner dims (B is the per-dim GCD, so each ratio
// is an exact integer >= 1). This scales the writer's committed muBlockCount.
static int64_t ownerBlockScaleFactor(ArrayRef<int64_t> ownerDims,
                                     ArrayRef<int64_t> oldBlock,
                                     ArrayRef<int64_t> newBlock) {
  int64_t factor = 1;
  for (int64_t od : ownerDims)
    if (od >= 0 && static_cast<size_t>(od) < oldBlock.size() &&
        static_cast<size_t>(od) < newBlock.size() && newBlock[od] > 0)
      factor *= oldBlock[od] / newBlock[od];
  return factor <= 0 ? 1 : factor;
}

// Rebuild a partition-evidence dict with the reconciled block and a rescaled
// muBlockCount, so the committed evidence stays consistent with the new grain
// (VerifySdePartitionPlan requires blockShape == physicalBlockShape).
static DictionaryAttr reconcileEvidenceDict(DictionaryAttr dict,
                                            ArrayAttr blockAttr, int64_t factor,
                                            StringRef blockKey,
                                            StringRef muCountKey,
                                            MLIRContext *ctx) {
  NamedAttrList entries(dict);
  entries.set(blockKey, blockAttr);
  if (auto mu = dyn_cast_or_null<IntegerAttr>(dict.get(muCountKey)))
    entries.set(muCountKey,
                Builder(ctx).getI64IntegerAttr(mu.getInt() * factor));
  return entries.getDictionary(ctx);
}

} // namespace

namespace mlir::carts::sde {

struct StorageGrainReconciliationPass
    : public sde::impl::StorageGrainReconciliationBase<
          StorageGrainReconciliationPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = module.getContext();

    // Arrays an unplanned data-parallel writer must NOT be allowed to block:
    // (a) any array a stencil/matmul SU touches (its layout must stay
    // compatible
    //     with that SU's owner-tile/halo or contraction grain), and
    // (b) mixed-orientation arrays (different SUs use conflicting non-empty
    //     ownerDims, so no single block owner dim is correct for every access).
    llvm::DenseSet<int64_t> protectedArrayIds;
    llvm::DenseMap<int64_t, SmallVector<int64_t, 4>> firstOwnerDims;
    module.walk([&](sde::SdeSuIterateOp op) {
      ArrayAttr layout = op.getArrayLayoutAttr();
      if (!layout)
        return;
      bool hardExcluded = isHardExcludedFamily(op);
      for (const sde::LayoutGraphFact &fact :
           sde::parseArrayLayoutFacts(layout)) {
        if (fact.id < 0)
          continue;
        if (hardExcluded)
          protectedArrayIds.insert(fact.id);
        if (fact.ownerDims.empty())
          continue;
        auto it = firstOwnerDims.find(fact.id);
        if (it == firstOwnerDims.end())
          firstOwnerDims.try_emplace(fact.id, fact.ownerDims);
        else if (it->second != fact.ownerDims)
          protectedArrayIds.insert(fact.id);
      }
    });

    // ---- Phase A: author a plan for unplanned data-parallel writers ----
    module.walk([&](sde::SdeSuIterateOp op) {
      if (hasPhysicalPlan(op) || isHardExcludedFamily(op) ||
          !isDataParallel(op) || op.getInPlaceSafeAttr())
        return;
      std::optional<sde::LayoutGraphFact> wf = singleWriteFact(op);
      if (!wf || wf->id < 0 || protectedArrayIds.count(wf->id) ||
          wf->layoutKind != sde::ArrayLayoutKind::blockParallel ||
          wf->ownerDims.empty() || wf->budgetBlockShape.empty() ||
          wf->ownerDims.size() > op.getSteps().size())
        return;
      // Block = budget grain, clamped per owner dim to the realized loop step
      // so block <= step holds (no loop retile, step-order invariant
      // preserved).
      SmallVector<int64_t, 4> block(wf->budgetBlockShape.begin(),
                                    wf->budgetBlockShape.end());
      bool ok = true;
      for (auto [slot, ownerDim] : llvm::enumerate(wf->ownerDims)) {
        int64_t step = 0;
        if (ownerDim < 0 || static_cast<size_t>(ownerDim) >= block.size() ||
            !::mlir::carts::ValueAnalysis::getConstantIndex(op.getSteps()[slot],
                                                            step) ||
            step <= 0) {
          ok = false;
          break;
        }
        if (block[ownerDim] > step)
          block[ownerDim] = step;
        if (block[ownerDim] <= 0) {
          ok = false;
          break;
        }
      }
      if (!ok)
        return;
      ARTS_DEBUG("Authoring storage plan for array id " << wf->id);
      authorPlan(op, wf->ownerDims, block, ctx);
    });

    // ---- Phase A2: unify affine-disjoint multi-store data-parallel writers ----
    // A multi-store init (a[i]=..;b[i]=..;c[i]=..) is declined by singleWriteFact
    // everywhere, so it keeps a coarse worker-tiling grain while the single-write
    // kernels of the same arrays commit the finer budget grain; the divergent
    // grain prevents the CODIR host bridge from merging, so the arrays relay
    // through a coarse host_whole master. Re-author the writer at the shared
    // budget grain (clamped to the realized step, exactly like Phase A) so every
    // co-written array matches its compute kernels.
    module.walk([&](sde::SdeSuIterateOp op) {
      if (isHardExcludedFamily(op) || !isDataParallel(op) ||
          op.getInPlaceSafeAttr() || op.getReductionAccumulators().size() != 0)
        return;
      if (singleWriteFact(op))
        return; // single-write: Phase A / DistributionPlanning own it
      std::optional<sde::LayoutGraphFact> wf =
          affineDisjointMultiStoreBudgetFact(op);
      if (!wf || wf->ownerDims.size() > op.getSteps().size())
        return;
      // A multi-store SU is atomic: decline if ANY co-written array is protected
      // (touched by a stencil/matmul SU, or mixed-orientation).
      bool anyProtected = false;
      for (const sde::LayoutGraphFact &fact :
           sde::parseArrayLayoutFacts(op.getArrayLayoutAttr())) {
        if (fact.role == sde::LayoutGraphRole::write &&
            protectedArrayIds.count(fact.id)) {
          anyProtected = true;
          break;
        }
      }
      if (anyProtected)
        return;
      if (!allExternalStoresCoverOwnerDims(op, wf->ownerDims))
        return;
      // Block = budget grain clamped per owner dim to the realized loop step, so
      // block <= step and no loop retile is needed: the SDE->CODIR dispatch
      // retiles the cloned body to the finer block window (the same path the
      // single-write budget kernels already take).
      SmallVector<int64_t, 4> block(wf->budgetBlockShape.begin(),
                                    wf->budgetBlockShape.end());
      bool ok = true;
      for (auto [slot, ownerDim] : llvm::enumerate(wf->ownerDims)) {
        int64_t step = 0;
        if (ownerDim < 0 || static_cast<size_t>(ownerDim) >= block.size() ||
            !::mlir::carts::ValueAnalysis::getConstantIndex(op.getSteps()[slot],
                                                            step) ||
            step <= 0) {
          ok = false;
          break;
        }
        if (block[ownerDim] > step)
          block[ownerDim] = step;
        if (block[ownerDim] <= 0) {
          ok = false;
          break;
        }
      }
      if (!ok)
        return;
      // Re-author only when there is no plan yet or the committed plan is
      // strictly coarser on some owner dim; never coarsen a finer committed plan.
      if (std::optional<SmallVector<int64_t, 4>> existing =
              readI64ArrayAttr(op.getPhysicalBlockShapeAttr())) {
        if (existing->size() != block.size())
          return;
        bool coarser = false, finer = false;
        for (int64_t ownerDim : wf->ownerDims) {
          if (ownerDim < 0 || static_cast<size_t>(ownerDim) >= existing->size())
            return;
          if ((*existing)[ownerDim] > block[ownerDim])
            coarser = true;
          else if ((*existing)[ownerDim] < block[ownerDim])
            finer = true;
        }
        if (finer || !coarser)
          return;
      }
      ARTS_DEBUG("Unifying multi-store writer to budget grain (rep id "
                 << wf->id << ")");
      authorPlan(op, wf->ownerDims, block, ctx);
      // The re-authored grain invalidates any committed partition evidence: it
      // was sized from the coarse worker grain and the rescale ratio need not be
      // integral. Drop it; CODIR/ARTS distribute from physicalOwnerDims +
      // physicalBlockShape, and the partition verifier only checks evidence when
      // present.
      op->removeAttr(sde::AttrNames::PartitionGraph);
      op->removeAttr(sde::AttrNames::PartitionScore);
    });

    // ---- Phase B: reconcile divergent multi-writer grains ----
    llvm::MapVector<int64_t, SmallVector<Writer, 4>> byArray;
    module.walk([&](sde::SdeSuIterateOp op) {
      std::optional<sde::LayoutGraphFact> wf = singleWriteFact(op);
      if (!wf || wf->id < 0)
        return;
      std::optional<SmallVector<int64_t, 4>> ownerDims =
          readI64ArrayAttr(op.getPhysicalOwnerDimsAttr());
      std::optional<SmallVector<int64_t, 4>> blockShape =
          readI64ArrayAttr(op.getPhysicalBlockShapeAttr());
      if (!ownerDims || ownerDims->empty() || !blockShape ||
          blockShape->empty())
        return;
      byArray[wf->id].push_back(
          Writer{op, std::move(*ownerDims), std::move(*blockShape)});
    });

    for (auto &entry : byArray) {
      SmallVector<Writer, 4> &writers = entry.second;
      if (writers.size() < 2)
        continue;

      // Owner dims must agree and block shapes be same-rank; reconcile only if
      // the blocks actually diverge.
      const SmallVector<int64_t, 4> &owner0 = writers.front().ownerDims;
      const SmallVector<int64_t, 4> &block0 = writers.front().blockShape;
      bool homogeneous = true, diverges = false;
      for (const Writer &w : writers) {
        if (w.ownerDims != owner0 || w.blockShape.size() != block0.size()) {
          homogeneous = false;
          break;
        }
        if (w.blockShape != block0)
          diverges = true;
      }
      if (!homogeneous || !diverges)
        continue;
      if (llvm::any_of(writers, [](const Writer &w) {
            return isHardExcludedFamily(w.op);
          }))
        continue;

      // Reconciled DB block = per-dim GCD of the writer blocks (<= every step).
      SmallVector<int64_t, 4> block(block0.begin(), block0.end());
      for (const Writer &w : writers)
        for (unsigned d = 0; d < block.size(); ++d)
          block[d] = std::gcd(block[d], w.blockShape[d]);
      if (llvm::any_of(block, [](int64_t b) { return b <= 0; }))
        continue;

      ARTS_DEBUG("Reconciling DB grain for array id "
                 << entry.first << " across " << writers.size() << " writers");
      ArrayAttr blockAttr = buildI64ArrayAttr(ctx, block);
      for (Writer &w : writers) {
        w.op.setPhysicalBlockShapeAttr(blockAttr);

        // Keep the committed CU/MU partition evidence consistent with the
        // grain.
        int64_t factor = ownerBlockScaleFactor(owner0, w.blockShape, block);
        if (auto score = dyn_cast_or_null<DictionaryAttr>(
                w.op->getAttr(sde::AttrNames::PartitionScore)))
          w.op->setAttr(sde::AttrNames::PartitionScore,
                        reconcileEvidenceDict(
                            score, blockAttr, factor,
                            sde::AttrNames::PartitionScoreKeys::BlockShape,
                            sde::AttrNames::PartitionScoreKeys::MuBlockCount,
                            ctx));

        auto graph = dyn_cast_or_null<ArrayAttr>(
            w.op->getAttr(sde::AttrNames::PartitionGraph));
        if (!graph)
          continue;
        SmallVector<Attribute, 4> rebuilt;
        rebuilt.reserve(graph.size());
        for (Attribute attr : graph) {
          auto dict = dyn_cast<DictionaryAttr>(attr);
          auto kind = dict
                          ? dyn_cast_or_null<StringAttr>(dict.get(
                                sde::AttrNames::PartitionGraphKeys::LayoutKind))
                          : nullptr;
          // Only the primary owner_block MU entries carry the storage grain.
          if (dict && kind &&
              kind.getValue() ==
                  sde::AttrNames::PartitionGraphValues::OwnerBlock)
            rebuilt.push_back(reconcileEvidenceDict(
                dict, blockAttr, factor,
                sde::AttrNames::PartitionGraphKeys::BlockShape,
                sde::AttrNames::PartitionGraphKeys::MuBlockCount, ctx));
          else
            rebuilt.push_back(attr);
        }
        w.op->setAttr(sde::AttrNames::PartitionGraph,
                      ArrayAttr::get(ctx, rebuilt));
      }
    }
  }
};

std::unique_ptr<Pass> createStorageGrainReconciliationPass() {
  return std::make_unique<StorageGrainReconciliationPass>();
}

} // namespace mlir::carts::sde
