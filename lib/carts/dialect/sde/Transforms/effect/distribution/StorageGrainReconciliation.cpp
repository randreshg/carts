///==========================================================================///
/// File: StorageGrainReconciliation.cpp
///
/// SDE storage-grain reconciliation across the multiple su_iterate writers of
/// one array.
///
/// DistributionPlanning stamps a physical plan (`physicalOwnerDims` +
/// `physicalBlockShape` + `logicalWorkerSlice`) on each `sde.su_iterate`
/// independently, per its own structured classification and realized loop step.
/// When an array is written by two STRUCTURALLY DIFFERENT scheduling units --
/// e.g. a data-parallel init (classified `elementwise`, tiled to step 4 =>
/// block [4, N]) and a pipeline kernel (`elementwise_pipeline`, step 8 =>
/// block [8, N]) -- the two writers commit DIFFERENT `physicalBlockShape`s for
/// the SAME array. The ARTS DB materializer cannot then share one per-block
/// distributed DB across the writers (their `ceilDiv(extent, block)` block
/// counts disagree), so the array falls back to a COARSE `host_whole` DB plus a
/// `host_whole_to_compute_block` bridge -- the dominant megalarge anti-scaling
/// funnel.
///
/// This is NOT fixed inside DistributionPlanning's per-op stampers (each only
/// sees one writer; the single-writer selector returns nullopt on divergence by
/// design, and the >=2-owner-dim guard cannot be relaxed globally without
/// regressing 1-D-owner stencils). Instead this dedicated pass runs AFTER the
/// physical plans exist and reconciles them:
///
///   1. Bucket every committed-plan `sde.su_iterate` writer by its array join id
///      (the `arrayLayout` write-role fact id, stable across writers/readers).
///   2. Fire only when a bucket has 2+ writers whose committed
///      `physicalBlockShape` DIVERGES and whose `physicalOwnerDims` AGREE.
///   3. Hard-exclude families that own a bespoke grain (matmul, stencil) -- leave
///      them untouched. In-place elementwise is NOT excluded at the bucket gate:
///      its single-DB protection only matters for a sole writer, which step 2's
///      2+-writer requirement already handles; a divergent multi-writer in-place
///      array would coarsen to host_whole anyway, so reconciliation is strictly
///      better than the coarse fallback.
///   4. Reconcile the DB grain to the per-dim GCD of the writer blocks. The
///      stampers pin block == owner-loop step, so the GCD is <= every writer's
///      step: `block <= step` still holds for all writers (no loop retile
///      needed, step-order invariant preserved) and the array gets the FINEST
///      grain common to every writer (maximal distribution). Each step is an
///      integer multiple of the GCD, so every write still lands on block
///      boundaries.
///   5. Rewrite ONLY `physicalBlockShape` (the DB/MU grain). `logicalWorkerSlice`
///      (the CU/worker grain) is left per-writer, keeping DB grain and CU grain
///      separate per the project charter; the bridge-hoist / shared-DB contract
///      (`hasSameHostBridgePlan`) compares only owner dims + block shape, which
///      now match.
///
/// Padding/masking of a non-dividing tail needs NO new IR here: the downstream
/// physical-DB resolver already over-allocates `ceilDiv(extent, block) * block`
/// cells, the loop bound stays at the true extent (the padding tail is never
/// iterated), and the host bridge clamps copies to the logical extent, so whole-
/// array reductions/checksums never observe padding. This pass only has to make
/// every writer agree on one block so those block counts are consistent.
///
/// Invariant: fail-closed. Single-writer arrays, already-coherent arrays,
/// incompatible owner dims, and excluded families are byte-identical no-ops.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_STORAGEGRAINRECONCILIATION
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/Debug.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"

#include "llvm/ADT/MapVector.h"

#include <numeric>

ARTS_DEBUG_SETUP(sde_storage_grain_reconciliation);

using namespace mlir;
using namespace mlir::carts;

namespace {

// A committed-plan su_iterate writer of one array: its op plus the physical
// owner dims and block shape DistributionPlanning stamped on it.
struct Writer {
  sde::SdeSuIterateOp op;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> blockShape;
};

// Families that own a dedicated, layout-irreducible grain and must never be
// folded into a shared block DB: matmul keeps its reduction-aware contraction
// tiling, stencil keeps its owner_tile + halo grain.
//
// Note: DistributionPlanning::stampBudgetReconciledPlan ALSO excludes in-place
// elementwise here, to protect the single-DB grain of a genuine in-place reuse.
// That protection only matters for a SINGLE writer -- which this pass already
// leaves untouched via the writers.size() < 2 early return. Once an array has
// 2+ structurally different writers that have ALREADY committed divergent
// blocks, it is going to coarsen to a host_whole DB regardless, so reconciling
// those writers to one shared GCD grain is strictly better than the coarse
// fallback. The in-place clause is therefore deliberately omitted from this
// bucket-level guard (see the firing analysis in the pass header).
static bool isHardExcludedFamily(sde::SdeSuIterateOp op) {
  if (auto cls = op.getStructuredClassification()) {
    if (*cls == sde::SdeStructuredClassification::matmul ||
        *cls == sde::SdeStructuredClassification::stencil)
      return true;
  }
  if (auto pat = op.getPattern(); pat && *pat == sde::SdePattern::matmul)
    return true;
  return false;
}

// The single write-role array fact (id) for an op, or nullopt if the op writes
// zero or more than one distributable array. Mirrors the per-op stamper's
// single-writer selector so the join key matches what DistributionPlanning saw.
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

// Per-writer integer scale factor from its committed block to the reconciled
// block B over the owner dims: product of oldBlock[d] / B[d]. B is the per-dim
// GCD of all writer blocks, so B[d] divides oldBlock[d] and each ratio is an
// exact integer >= 1. This is how many reconciled MU blocks now tile one of the
// writer's old blocks, i.e. the factor by which its muBlockCount grows.
static int64_t ownerBlockScaleFactor(ArrayRef<int64_t> ownerDims,
                                     ArrayRef<int64_t> oldBlock,
                                     ArrayRef<int64_t> newBlock) {
  int64_t factor = 1;
  for (int64_t od : ownerDims) {
    if (od < 0 || static_cast<size_t>(od) >= oldBlock.size() ||
        static_cast<size_t>(od) >= newBlock.size() || newBlock[od] <= 0)
      continue;
    factor *= oldBlock[od] / newBlock[od];
  }
  return factor <= 0 ? 1 : factor;
}

// Rebuild a partition-evidence dict with blockShape set to the reconciled block
// and muBlockCount scaled by `factor` (the committed CU/MU partition evidence
// must stay consistent with the rewritten physical plan: the verifier requires
// blockShape == physicalBlockShape, and a stale muBlockCount would describe the
// wrong number of MU blocks for the new grain).
static DictionaryAttr reconcileEvidenceDict(DictionaryAttr dict,
                                            ArrayAttr blockAttr, int64_t factor,
                                            StringRef blockKey,
                                            StringRef muCountKey,
                                            MLIRContext *ctx) {
  Builder builder(ctx);
  NamedAttrList entries(dict);
  entries.set(blockKey, blockAttr);
  if (auto mu = dyn_cast_or_null<IntegerAttr>(dict.get(muCountKey)))
    entries.set(muCountKey,
                builder.getI64IntegerAttr(mu.getInt() * factor));
  return entries.getDictionary(ctx);
}

} // namespace

namespace mlir::carts::sde {

struct StorageGrainReconciliationPass
    : public sde::impl::StorageGrainReconciliationBase<
          StorageGrainReconciliationPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    // Bucket every committed-plan su_iterate writer by its array join id.
    llvm::MapVector<int64_t, SmallVector<Writer, 4>> byArray;
    module.walk([&](sde::SdeSuIterateOp op) {
      std::optional<sde::LayoutGraphFact> wf = singleWriteFact(op);
      if (!wf || wf->id < 0)
        return;
      std::optional<SmallVector<int64_t, 4>> ownerDims =
          readI64ArrayAttr(op.getPhysicalOwnerDimsAttr());
      std::optional<SmallVector<int64_t, 4>> blockShape =
          readI64ArrayAttr(op.getPhysicalBlockShapeAttr());
      if (!ownerDims || ownerDims->empty() || !blockShape || blockShape->empty())
        return; // no committed physical plan -> nothing to reconcile
      byArray[wf->id].push_back(
          Writer{op, std::move(*ownerDims), std::move(*blockShape)});
    });

    for (auto &entry : byArray) {
      SmallVector<Writer, 4> &writers = entry.second;
      if (writers.size() < 2)
        continue; // single coherent writer -> untouched

      // Owner dims must agree across writers and the block shapes must be the
      // same rank; the grain only needs reconciling if the blocks diverge.
      const SmallVector<int64_t, 4> &owner0 = writers.front().ownerDims;
      const SmallVector<int64_t, 4> &block0 = writers.front().blockShape;
      bool homogeneous = true;
      bool diverges = false;
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

      // Hard-excluded families own a bespoke grain; leave the whole array as
      // stamped. (In-place elementwise is intentionally NOT hard-excluded here:
      // its single-DB protection is already covered by the writers.size() < 2
      // early return above.)
      if (llvm::any_of(writers,
                       [](const Writer &w) { return isHardExcludedFamily(w.op); }))
        continue;

      // Reconciled DB block = per-dim GCD of the writer blocks (<= every step).
      SmallVector<int64_t, 4> block(block0.begin(), block0.end());
      for (const Writer &w : writers)
        for (unsigned d = 0; d < block.size(); ++d)
          block[d] = std::gcd(block[d], w.blockShape[d]);
      if (llvm::any_of(block, [](int64_t b) { return b <= 0; }))
        continue;
      // `diverges` guarantees at least one writer differs from this GCD, so the
      // rewrite is never a whole-bucket no-op even when block == block0.

      ARTS_DEBUG("Reconciling DB grain for array id "
                 << entry.first << " across " << writers.size() << " writers");
      MLIRContext *ctx = module.getContext();
      ArrayAttr blockAttr = buildI64ArrayAttr(ctx, block);
      for (Writer &w : writers) {
        // The DB/MU grain is physicalBlockShape (reconciled). The CU/worker
        // grain is logicalWorkerSlice -- left untouched, so DB grain and CU
        // grain stay separate per the project charter.
        w.op.setPhysicalBlockShapeAttr(blockAttr);

        // Keep the committed CU/MU partition evidence consistent with the new
        // grain: blockShape must equal physicalBlockShape (verifier) and
        // muBlockCount grows by oldBlock/B over the owner dims.
        int64_t factor = ownerBlockScaleFactor(owner0, w.blockShape, block);
        if (auto score = dyn_cast_or_null<DictionaryAttr>(
                w.op->getAttr(sde::AttrNames::PartitionScore)))
          w.op->setAttr(
              sde::AttrNames::PartitionScore,
              reconcileEvidenceDict(
                  score, blockAttr, factor,
                  sde::AttrNames::PartitionScoreKeys::BlockShape,
                  sde::AttrNames::PartitionScoreKeys::MuBlockCount, ctx));

        if (auto graph = dyn_cast_or_null<ArrayAttr>(
                w.op->getAttr(sde::AttrNames::PartitionGraph))) {
          SmallVector<Attribute, 4> rebuilt;
          rebuilt.reserve(graph.size());
          for (Attribute attr : graph) {
            auto dict = dyn_cast<DictionaryAttr>(attr);
            auto kind = dict ? dyn_cast_or_null<StringAttr>(dict.get(
                                   sde::AttrNames::PartitionGraphKeys::LayoutKind))
                             : nullptr;
            // Only the primary owner_block MU entries carry the storage grain
            // the verifier ties to physicalBlockShape; leave other edges as-is.
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
  }
};

std::unique_ptr<Pass> createStorageGrainReconciliationPass() {
  return std::make_unique<StorageGrainReconciliationPass>();
}

} // namespace mlir::carts::sde
