///==========================================================================///
/// File: WriterLayoutCommit.cpp
///
/// Shared driver + passes for SDE per-array BLOCK layout assignment (the SDE
/// DATA-LAYOUT engine in the spirit of HPF DISTRIBUTE/ALIGN). It runs
/// owner-loop promotion, builds module access relations (PhaseA), asks
/// LayoutCandidateChoose for the chosen owner layout per array (PhaseB/PhaseC),
/// then reconciles per-SU writer/reader geometry and commits the layout facts
/// (PhaseD): transitional `arrayLayout` attrs, `sde.array_layout_root` ops, and
/// writer physical MU-type layout commits.
///
/// `runLayoutAssignment` is the single atomic driver: choice and commit happen
/// in one pass run (no metadata-promise handoff). It backs three pass entry
/// points:
///   * `sde-writer-layout-commit` — commits writer AND reader facts (the
///     production pass; replaces the monolithic name).
///   * `sde-layout-assignment` — DEPRECATED alias, identical full behavior,
///     retained so existing pipelines/lit keep resolving.
///   * `sde-layout-candidate-choose` — defined in LayoutCandidateChoose.cpp,
///     drives the same function with reader commit disabled.
///
/// It is strictly DATA-LAYOUT: it NAMES NO COLLECTIVE, and knows nothing about
/// concrete storage, tasks, epochs, or runtime placement. Redistribution edges
/// are detected later by comparing committed producer/consumer layout facts.
///==========================================================================///

#include "LayoutAssignmentInternal.h"

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/CuMuGraphPartitioning.h"
#include "carts/dialect/sde/Utils/IterationSizingUtils.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/dialect/sde/Utils/SdeOwnerLoopPromotion.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallBitVector.h"
#include <limits>

namespace mlir::carts::sde {
#define GEN_PASS_DEF_LAYOUTASSIGNMENT
#define GEN_PASS_DEF_WRITERLAYOUTCOMMIT
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

using namespace mlir;
using namespace mlir::carts;

namespace {

//===----------------------------------------------------------------------===//
// PhaseD — commit per-SU layout facts
//===----------------------------------------------------------------------===//

static StringRef layoutKindString(sde::ArrayLayoutKind kind) {
  switch (kind) {
  case sde::ArrayLayoutKind::blockParallel:
    return sde::AttrNames::LayoutGraph::BlockParallel;
  case sde::ArrayLayoutKind::blockContraction:
    return sde::AttrNames::LayoutGraph::BlockContraction;
  case sde::ArrayLayoutKind::replicated:
    return sde::AttrNames::LayoutGraph::Replicated;
  }
  return sde::AttrNames::LayoutGraph::Replicated;
}

// One `arrayLayout` dictionary entry for an array on a scheduling unit.
static DictionaryAttr buildLayoutEntry(MLIRContext *ctx, int64_t arrayId,
                                       ArrayRef<int64_t> staticShape,
                                       const sde::ArrayLayoutCandidate &layout,
                                       StringRef role, int64_t elemBytes) {
  Builder b(ctx);
  SmallVector<NamedAttribute, 8> fields;
  fields.push_back(b.getNamedAttr(sde::AttrNames::LayoutGraph::ArrayId,
                                  b.getI64IntegerAttr(arrayId)));
  fields.push_back(
      b.getNamedAttr(sde::AttrNames::LayoutGraph::Role, b.getStringAttr(role)));
  fields.push_back(
      b.getNamedAttr(sde::AttrNames::LayoutGraph::Kind,
                     b.getStringAttr(layoutKindString(layout.kind))));
  fields.push_back(
      b.getNamedAttr(sde::AttrNames::LayoutGraph::OwnerDims,
                     buildI64ArrayAttr(ctx, layout.ownerPositions)));
  fields.push_back(b.getNamedAttr(sde::AttrNames::LayoutGraph::BlockShape,
                                  buildI64ArrayAttr(ctx, layout.blockShape)));
  fields.push_back(b.getNamedAttr(
      sde::AttrNames::LayoutGraph::MuBlockCount,
      b.getI64IntegerAttr(sde::detail::computeMuBlockCount(
          staticShape, layout.ownerPositions, layout.blockShape))));
  // Node-agnostic budget grain consumed by SDE loop tiling and distribution
  // transforms to seed the physical tile block shape. For block layouts only;
  // replicated/contraction keep the abstract grain mirrored so the field is
  // always present.
  SmallVector<int64_t, 4> budgetShape(layout.blockShape.begin(),
                                      layout.blockShape.end());
  if (layout.kind == sde::ArrayLayoutKind::blockParallel &&
      !layout.ownerPositions.empty())
    budgetShape = sde::detail::blockShapeFromBudget(
        staticShape, elemBytes, layout.ownerPositions,
        sde::detail::kTargetBlockBytes);
  fields.push_back(b.getNamedAttr(sde::AttrNames::LayoutGraph::BudgetBlockShape,
                                  buildI64ArrayAttr(ctx, budgetShape)));
  return b.getDictionaryAttr(fields);
}

static bool schedulingUnitWritesRoot(const sde::ArrayAccessProfile &profile,
                                     unsigned suId) {
  for (const auto &posUses : profile.positionUses)
    for (const sde::ArrayPositionUse &use : posUses)
      if (use.suId == suId && use.isWrite)
        return true;
  return false;
}

static bool
schedulingUnitWritesAnyRoot(const sde::ModuleSuAccessRelations &relations,
                            unsigned suId) {
  for (const auto &kv : relations.profiles) {
    if (schedulingUnitWritesRoot(kv.second, suId))
      return true;
  }
  return false;
}

static bool sameOwnerDimSet(ArrayRef<int64_t> lhs, ArrayRef<int64_t> rhs) {
  if (lhs.size() != rhs.size())
    return false;
  SmallVector<int64_t, 4> lhsSorted(lhs.begin(), lhs.end());
  SmallVector<int64_t, 4> rhsSorted(rhs.begin(), rhs.end());
  llvm::sort(lhsSorted);
  llvm::sort(rhsSorted);
  return lhsSorted == rhsSorted;
}

static bool readerHasOwnerReduction(const sde::ArrayAccessProfile &profile,
                                    unsigned suId,
                                    ArrayRef<int64_t> ownerPositions) {
  llvm::SmallBitVector ownerBits(profile.rank);
  for (int64_t pos : ownerPositions)
    if (pos >= 0 && static_cast<size_t>(pos) < profile.rank)
      ownerBits.set(pos);
  for (unsigned pos = 0; pos < profile.rank; ++pos) {
    if (!ownerBits.test(pos))
      continue;
    for (const sde::ArrayPositionUse &use : profile.positionUses[pos]) {
      if (use.suId == suId && !use.isWrite &&
          use.kind == sde::ArrayDimKind::reductionIndexed)
        return true;
    }
  }
  return false;
}

static bool
readerParallelIndexesPosition(const sde::ArrayAccessProfile &profile,
                              unsigned suId, unsigned pos,
                              bool includeHalo = false) {
  for (const sde::ArrayPositionUse &use : profile.positionUses[pos]) {
    if (use.suId != suId || use.isWrite)
      continue;
    if (use.kind == sde::ArrayDimKind::parallelIndexed)
      return true;
    if (includeHalo && use.kind == sde::ArrayDimKind::parallelHalo)
      return true;
  }
  return false;
}

static bool isStencilReader(ArrayRef<sde::SdeSuIterateOp> schedulingUnits,
                            unsigned suId) {
  if (suId >= schedulingUnits.size())
    return false;
  auto classification =
      sde::queryStructuredClassification(schedulingUnits[suId]);
  return classification &&
         *classification == sde::SdeStructuredClassification::stencil;
}

// The consumer's required read layout: the block geometry that aligns with how
// this scheduling unit indexes the array. Cross-owner reductions and
// owner-preserving halo reads keep the module home layout; repartition readers
// commit the parallel axes they actually traverse.
static sde::ArrayLayoutCandidate
inferReaderRequiredLayout(const sde::ArrayAccessProfile &profile, unsigned suId,
                          ArrayRef<sde::SdeSuIterateOp> schedulingUnits,
                          const sde::ArrayLayoutCandidate &homeLayout) {
  if (readerHasOwnerReduction(profile, suId, homeLayout.ownerPositions)) {
    return sde::detail::makeBlockCandidate(
        profile, homeLayout.ownerPositions,
        sde::ArrayLayoutKind::blockContraction);
  }

  SmallVector<int64_t, 4> parallelOwner;
  bool includeHalo = isStencilReader(schedulingUnits, suId);
  for (unsigned pos = 0; pos < profile.rank; ++pos)
    if (readerParallelIndexesPosition(profile, suId, pos, includeHalo))
      parallelOwner.push_back(static_cast<int64_t>(pos));

  if (parallelOwner.empty() ||
      sameOwnerDimSet(parallelOwner, homeLayout.ownerPositions))
    return homeLayout;

  return sde::detail::makeBlockCandidate(profile, parallelOwner,
                                         sde::ArrayLayoutKind::blockParallel);
}

static bool hasStencilReader(const sde::ArrayAccessProfile &profile,
                             ArrayRef<sde::SdeSuIterateOp> schedulingUnits) {
  for (const auto &posUses : profile.positionUses) {
    for (const sde::ArrayPositionUse &use : posUses) {
      if (use.isWrite || use.suId >= schedulingUnits.size())
        continue;
      sde::SdeSuIterateOp reader = schedulingUnits[use.suId];
      auto classification = sde::queryStructuredClassification(reader);
      if (classification &&
          *classification == sde::SdeStructuredClassification::stencil)
        return true;
    }
  }
  return false;
}

// A block-parallel WRITER may only own a physical dim it indexes with a single
// SU loop IV consistently across EVERY store to the root, and distinct owner
// dims must use distinct IVs. A symmetric/cross-row writer (correlation's
// corr[i][j] AND corr[j][i] from a 1-D i-loop) indexes owner dim 0 with i in
// one store and j in the other: owning that dim would route corr[j][i] to a DB
// block the SU does not own. Returns the realizable subset of `ownerPositions`
// (in input order). Positions not witnessed are dropped so the committed owner
// rank never exceeds what the SU loop can actually realize.
static SmallVector<int64_t, 4>
witnessedWriterOwnerPositions(sde::SdeSuIterateOp writerOp, Value root,
                              ArrayRef<int64_t> ownerPositions) {
  SmallVector<int64_t, 4> result;
  if (!writerOp || !root || ownerPositions.empty())
    return result;
  std::optional<SmallVector<Value>> ivs = writerOp.getLoopInductionVars();
  if (!ivs || ivs->empty())
    return result;

  // Per owner position: the single loop-IV index used at that memref dim, or a
  // sentinel state. -2 = unseen, -1 = disqualified, >=0 = loop-IV slot index.
  llvm::SmallDenseMap<int64_t, int> posToIv;
  for (int64_t pos : ownerPositions)
    posToIv[pos] = -2;

  auto ivSlot = [&](Value index) -> int {
    Value base = ::mlir::carts::ValueAnalysis::stripNumericCasts(index);
    int64_t off = 0;
    base = ::mlir::carts::ValueAnalysis::stripConstantOffset(base, &off);
    if (off != 0)
      return -1;
    base = ::mlir::carts::ValueAnalysis::stripNumericCasts(base);
    for (auto [slot, iv] : llvm::enumerate(*ivs))
      if (::mlir::carts::ValueAnalysis::sameValue(base, iv))
        return static_cast<int>(slot);
    return -1;
  };

  auto inspect = [&](Value memref, OperandRange indices) {
    Value mbase = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(memref);
    if (mbase != root)
      return;
    for (int64_t pos : ownerPositions) {
      int &state = posToIv[pos];
      if (state == -1)
        continue;
      if (pos < 0 || static_cast<size_t>(pos) >= indices.size()) {
        state = -1;
        continue;
      }
      int slot = ivSlot(indices[static_cast<size_t>(pos)]);
      if (slot < 0)
        state = -1;
      else if (state == -2)
        state = slot;
      else if (state != slot)
        state = -1; // inconsistent IV across stores
    }
  };

  writerOp.getBody().walk([&](Operation *nested) {
    if (auto st = dyn_cast<memref::StoreOp>(nested)) {
      if (!isa<MemRefType>(st.getValueToStore().getType()))
        inspect(st.getMemref(), st.getIndices());
    } else if (auto ld = dyn_cast<memref::LoadOp>(nested)) {
      // Reads of the root at an owner position must also be owner-local, else
      // the owner-tile is not a single-writer/owner-local region.
      if (!isa<MemRefType>(ld.getResult().getType()))
        inspect(ld.getMemref(), ld.getIndices());
    }
  });

  // Keep positions with a consistent loop-IV witness; enforce distinct IVs.
  llvm::SmallDenseSet<int, 4> usedSlots;
  for (int64_t pos : ownerPositions) {
    int state = posToIv[pos];
    if (state >= 0 && usedSlots.insert(state).second)
      result.push_back(pos);
  }
  return result;
}

} // namespace

namespace mlir::carts::sde::detail {

void runLayoutAssignment(::mlir::Operation *moduleOp,
                         sde::SDECostModel *costModel, bool commitReaders) {
  // Owner-loop promotion (rank-1→rank-N su_iterate rebuilds) runs here,
  // after raise-to-sde / cu-normalization and before layout assignment.
  if (auto module = dyn_cast<ModuleOp>(moduleOp))
    sde::promoteModuleOwnerLoops(module);

  // Layout assignment is only meaningful when there is more than one logical
  // worker to distribute across. With no cost model (textual pass pipeline)
  // or a single worker, mirror the physical-layout committers and do nothing
  // — keeping single-worker IR untouched.
  if (!costModel || costModel->getLogicalWorkerCapacity() <= 1)
    return;

  MLIRContext *ctx = moduleOp->getContext();

  // PhaseA — module access relations.
  sde::ModuleSuAccessRelations relations =
      sde::buildModuleSuAccessRelations(moduleOp);
  if (relations.profiles.empty())
    return;

  // Explicit BlockContraction input (PhaseB): per array root, the physical
  // position through which a SIBLING consumer reduces it. Built from
  // findContractionTilingCandidate, gated on the contraction input being a
  // sibling-distributed intermediate written by a different scheduling unit.
  // This is the same gate the contraction-tiling intent uses. The contraction
  // position is derived from the consumer's actual input access map.
  llvm::DenseMap<Value, unsigned> contractionPositionByRoot;
  for (auto [consumerId, consumer] :
       llvm::enumerate(relations.schedulingUnits)) {
    std::optional<sde::ContractionTilingCandidate> cand =
        sde::findContractionTilingCandidate(consumer);
    if (!cand || !cand->contractionInputRoot)
      continue;
    auto it = relations.profiles.find(cand->contractionInputRoot);
    if (it == relations.profiles.end())
      continue;
    const sde::ArrayAccessProfile &inputProfile = it->second;
    // Sibling-distributed intermediate: produced by some OTHER scheduling
    // unit.
    if (!inputProfile.hasWriter || !inputProfile.hasFullRankWriter ||
        !inputProfile.writerSuId || *inputProfile.writerSuId == consumerId)
      continue;
    if (!cand->contractionInputPhysicalDim)
      continue;
    contractionPositionByRoot.try_emplace(cand->contractionInputRoot,
                                          *cand->contractionInputPhysicalDim);
  }

  // Stable arrayId per array root, shared with redistribution realization via
  // a single numbering so producer and consumer never drift.
  llvm::MapVector<Value, int64_t> arrayIds =
      sde::assignStableArrayIds(relations);

  // Accumulate per-SU layout updates before applying so each su_iterate gets
  // one combined `arrayLayout` array of all its accessed roots.
  struct RootProvenance {
    Value root;
    int64_t arrayId = -1;
    sde::SdeAccessMode mode = sde::SdeAccessMode::read;
  };
  struct WriterPhysicalCommit {
    SmallVector<int64_t, 4> ownerDims;
    SmallVector<int64_t, 4> blockShape;
    SmallVector<int64_t, 4> logicalShape;
  };
  struct SchedulingUnitLayoutUpdate {
    SmallVector<DictionaryAttr, 4> entries;
    SmallVector<RootProvenance, 4> roots;
  };
  SmallVector<SchedulingUnitLayoutUpdate> updates(
      relations.schedulingUnits.size());
  llvm::SmallDenseMap<unsigned, SmallVector<WriterPhysicalCommit, 2>>
      writerCommits;

  for (auto &kv : relations.profiles) {
    const sde::ArrayAccessProfile &profile = kv.second;
    if (profile.rank == 0 || profile.staticShape.empty())
      continue;
    // Only assign layouts to arrays with at least one block-distributable
    // access; arrays read only as scalars/broadcasts get the replicated
    // candidate but no disagree edges.
    int64_t arrayId = arrayIds.lookup(profile.root);

    // PhaseB + PhaseC.
    std::optional<unsigned> contractionPosition;
    if (auto it = contractionPositionByRoot.find(profile.root);
        it != contractionPositionByRoot.end())
      contractionPosition = it->second;
    bool preserveFullWriterOwnerTile =
        profile.hasFullRankWriter &&
        hasStencilReader(profile, relations.schedulingUnits);
    sde::detail::ChosenLayout chosen = sde::detail::assignLayout(
        profile, contractionPosition, preserveFullWriterOwnerTile);

    // PhaseD — commit on EVERY scheduling unit that accesses this root
    // (writer AND readers — the input generalization), keyed by arrayId so
    // later SDE transforms can join them.
    llvm::SmallDenseSet<unsigned, 4> accessors;
    for (const auto &posUses : profile.positionUses)
      for (const sde::ArrayPositionUse &use : posUses)
        accessors.insert(use.suId);

    for (unsigned suId : accessors) {
      if (suId >= updates.size())
        continue;
      bool isWrite = schedulingUnitWritesRoot(profile, suId);
      // sde-layout-candidate-choose commits only the chosen WRITER home
      // layouts; the reader transitional facts are committed atomically by
      // sde-writer-layout-commit (and the deprecated sde-layout-assignment).
      if (!isWrite && !commitReaders)
        continue;
      bool readerDisagrees = chosen.disagreeingReaders.contains(suId);
      sde::ArrayLayoutCandidate layoutForSu = chosen.layout;
      if (!isWrite && readerDisagrees)
        layoutForSu = inferReaderRequiredLayout(
            profile, suId, relations.schedulingUnits, chosen.layout);
      // Clamp a block-parallel writer's owner dims to the ones its SU can
      // realize as owner-local single-writer regions (see
      // witnessedWriterOwnerPositions). Drops the unrealizable owner dims of
      // a symmetric/cross-row writer (correlation) instead of committing a
      // silently-wrong owner-tile; an empty residual leaves the array
      // un-owned so downstream distribution fails closed rather than
      // miscompile.
      if (isWrite && layoutForSu.kind == sde::ArrayLayoutKind::blockParallel &&
          !layoutForSu.ownerPositions.empty() &&
          suId < relations.schedulingUnits.size()) {
        SmallVector<int64_t, 4> witnessed = witnessedWriterOwnerPositions(
            relations.schedulingUnits[suId], profile.root,
            layoutForSu.ownerPositions);
        if (witnessed.size() != layoutForSu.ownerPositions.size()) {
          for (int64_t pos : layoutForSu.ownerPositions) {
            if (llvm::is_contained(witnessed, pos))
              continue;
            if (pos >= 0 &&
                static_cast<size_t>(pos) < layoutForSu.blockShape.size() &&
                static_cast<size_t>(pos) < profile.staticShape.size())
              layoutForSu.blockShape[pos] = profile.staticShape[pos];
          }
          layoutForSu.ownerPositions.assign(witnessed.begin(), witnessed.end());
          if (layoutForSu.ownerPositions.empty())
            layoutForSu.kind = sde::ArrayLayoutKind::replicated;
        }
      }
      DictionaryAttr entry = buildLayoutEntry(
          ctx, arrayId, profile.staticShape, layoutForSu,
          isWrite ? sde::AttrNames::LayoutGraphValues::RoleWrite
                  : sde::AttrNames::LayoutGraphValues::RoleRead,
          std::max<int64_t>(1, sde::detail::elementBytes(profile.root)));
      // Aligned readers carry no transitional arrayLayout unless they share a
      // scheduling unit with a writer that still needs local block structure
      // (e.g. coupled read/write in the same reduction nest).
      if (!isWrite && !readerDisagrees &&
          !schedulingUnitWritesAnyRoot(relations, suId))
        continue;
      bool writerViaMuType =
          isWrite && !layoutForSu.ownerPositions.empty() &&
          (layoutForSu.kind == sde::ArrayLayoutKind::blockParallel ||
           layoutForSu.kind == sde::ArrayLayoutKind::blockContraction);
      if (writerViaMuType) {
        writerCommits[suId].push_back(
            {SmallVector<int64_t, 4>(layoutForSu.ownerPositions.begin(),
                                     layoutForSu.ownerPositions.end()),
             SmallVector<int64_t, 4>(layoutForSu.blockShape.begin(),
                                     layoutForSu.blockShape.end()),
             SmallVector<int64_t, 4>(profile.staticShape.begin(),
                                     profile.staticShape.end())});
      }
      // Block-parallel non-reduction, non-stencil writers also pin their
      // owner rank for the 2n boundary; reductions/stencils keep their own
      // grain.
      bool blockParallelWriter =
          writerViaMuType &&
          layoutForSu.kind == sde::ArrayLayoutKind::blockParallel;
      bool reductionWriter = false;
      if (blockParallelWriter && suId < relations.schedulingUnits.size()) {
        std::optional<sde::SdeStructuredClassification> wc =
            sde::queryStructuredClassification(relations.schedulingUnits[suId]);
        reductionWriter =
            wc && *wc == sde::SdeStructuredClassification::reduction;
      }
      if (!writerViaMuType ||
          (blockParallelWriter && !preserveFullWriterOwnerTile &&
           !reductionWriter))
        updates[suId].entries.push_back(entry);
      updates[suId].roots.push_back(
          {profile.root, arrayId,
           isWrite ? sde::SdeAccessMode::write : sde::SdeAccessMode::read});
    }
  }

  // Apply accumulated layout updates.
  for (auto [suId, op] : llvm::enumerate(relations.schedulingUnits)) {
    SchedulingUnitLayoutUpdate &update = updates[suId];
    if (update.entries.empty() && !writerCommits.count(suId) &&
        update.roots.empty())
      continue;
    if (!update.entries.empty()) {
      SmallVector<Attribute, 4> entryAttrs(update.entries.begin(),
                                           update.entries.end());
      op.setArrayLayoutAttr(ArrayAttr::get(ctx, entryAttrs));
    }

    OpBuilder builder(&op.getBody().front(), op.getBody().front().begin());
    for (const RootProvenance &root : update.roots) {
      bool exists = false;
      for (sde::SdeArrayLayoutRootOp existing :
           op.getBody().front().getOps<sde::SdeArrayLayoutRootOp>()) {
        exists |= existing.getRoot() == root.root &&
                  static_cast<int64_t>(existing.getArrayId()) == root.arrayId &&
                  existing.getMode() == root.mode;
      }
      if (exists)
        continue;
      sde::SdeArrayLayoutRootOp::create(
          builder, op.getLoc(), root.root,
          sde::SdeAccessModeAttr::get(ctx, root.mode),
          IntegerAttr::get(IntegerType::get(ctx, 64), root.arrayId));
    }
    auto writerCommitIt = writerCommits.find(suId);
    if (writerCommitIt != writerCommits.end())
      for (const WriterPhysicalCommit &commit : writerCommitIt->second)
        (void)sde::commitWriterPhysicalLayoutViaMuType(
            op, commit.ownerDims, commit.blockShape, commit.logicalShape);
  }
}

} // namespace mlir::carts::sde::detail

namespace {

// Production pass: choose and commit writer AND reader layout facts atomically.
struct WriterLayoutCommitPass
    : public sde::impl::WriterLayoutCommitBase<WriterLayoutCommitPass> {
  explicit WriterLayoutCommitPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    sde::detail::runLayoutAssignment(getOperation(), costModel,
                                     /*commitReaders=*/true);
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

// DEPRECATED alias: identical full behavior, retained so existing pipelines and
// lit RUN lines that name `sde-layout-assignment` keep resolving.
struct LayoutAssignmentPass
    : public sde::impl::LayoutAssignmentBase<LayoutAssignmentPass> {
  explicit LayoutAssignmentPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    sde::detail::runLayoutAssignment(getOperation(), costModel,
                                     /*commitReaders=*/true);
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass>
createWriterLayoutCommitPass(sde::SDECostModel *costModel) {
  return std::make_unique<WriterLayoutCommitPass>(costModel);
}

std::unique_ptr<Pass> createLayoutAssignmentPass(sde::SDECostModel *costModel) {
  return std::make_unique<LayoutAssignmentPass>(costModel);
}

} // namespace mlir::carts::sde
