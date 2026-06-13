///==========================================================================///
/// File: LayoutAssignment.cpp
///
/// Module-scoped affine-driven per-array BLOCK layout assignment.
///
/// This is the SDE DATA-LAYOUT engine in the spirit of HPF DISTRIBUTE/ALIGN.
/// For every external array root accessed by the module's `sde.su_iterate`
/// scheduling units it chooses ONE element-space BLOCK layout (owner positions
/// + per-block extents) from the array's affine access relations, minimizing an
/// ABSTRACT communication-volume cost (~ N*(P-1)/P).
///
/// It is strictly DATA-LAYOUT: pattern-agnostic (driven only by affine maps,
/// iterator types, and static shapes), it NAMES NO COLLECTIVE, and it knows
/// nothing about concrete storage, tasks, epochs, or runtime placement. It only
/// adds transitional `arrayLayout` and `commVolumeBytes` SDE attrs on
/// disagreeing readers and writers. Redistribution edges are detected later by
/// comparing committed producer/consumer layout facts (or rank-expanded
/// `mu_alloc` types), not a `layoutsDisagree` marker.
///==========================================================================///

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/CuMuGraphPartitioning.h"
#include "carts/dialect/sde/Utils/IterationSizingUtils.h"
#include "carts/dialect/sde/Utils/SdeOwnerLoopPromotion.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallBitVector.h"
#include <limits>

namespace mlir::carts::sde {
#define GEN_PASS_DEF_LAYOUTASSIGNMENT
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

using namespace mlir;
using namespace mlir::carts;

namespace {

// Default physical worker grain used to size the abstract block extent. The
// layout engine is element-space only; this is a coarse proxy for "the array is
// split across some workers" so block extents are smaller than full extent and
// the cost model can distinguish aligned from full-extent edges. It encodes no
// runtime node/worker count.
static constexpr int64_t kAbstractBlockFactor = 2;

static int64_t productOf(ArrayRef<int64_t> shape) {
  int64_t total = 1;
  for (int64_t dim : shape)
    total = (dim > 0) ? total * dim : total;
  return total;
}

static int64_t computeMuBlockCount(ArrayRef<int64_t> shape,
                                   ArrayRef<int64_t> ownerPositions,
                                   ArrayRef<int64_t> blockShape) {
  if (ownerPositions.empty())
    return 1;
  return sde::inferCuCountFromMuPartition(shape, ownerPositions, blockShape);
}

static int64_t elementBytes(Value root) {
  auto memrefTy = dyn_cast_or_null<MemRefType>(root.getType());
  if (!memrefTy)
    return 0;
  Type elt = memrefTy.getElementType();
  if (!elt.isIntOrFloat())
    return 0;
  return llvm::divideCeil(elt.getIntOrFloatBitWidth(), 8);
}

static int64_t integerLog2Ceil(int64_t value) {
  int64_t bits = 0;
  int64_t acc = 1;
  while (acc < value) {
    acc *= 2;
    ++bits;
  }
  return std::max<int64_t>(1, bits);
}

//===----------------------------------------------------------------------===//
// PhaseB — candidate layouts per array
//===----------------------------------------------------------------------===//

// True when at least one parallel-indexed WRITE use (offset 0) selects this
// position: the writer's owner-computes axis. This is the primary (HPF
// DISTRIBUTE) seed.
static bool
isWriterParallelOwnerPosition(const sde::ArrayAccessProfile &profile,
                              unsigned pos) {
  for (const sde::ArrayPositionUse &use : profile.positionUses[pos]) {
    if (use.isWrite && use.fullRankWrite && use.isSchedulingLoopDim &&
        use.kind == sde::ArrayDimKind::parallelIndexed)
      return true;
  }
  return false;
}

// True when some parallel-indexed use (writer OR reader) selects this position.
// For a host input with no in-module writer, the owner-dim candidate is the
// ALIGN target: the position a consumer reads with a single parallel IV.
// Generalizes the output-only rule by also offering readers a block layout.
static bool isParallelOwnerPosition(const sde::ArrayAccessProfile &profile,
                                    unsigned pos) {
  // A writer owner position always wins as the seed.
  if (isWriterParallelOwnerPosition(profile, pos))
    return true;
  // No writer for this array (pure input): seed from a reader's parallel index.
  if (profile.hasWriter)
    return false;
  for (const sde::ArrayPositionUse &use : profile.positionUses[pos]) {
    if (!use.isWrite && use.isSchedulingLoopDim &&
        use.kind == sde::ArrayDimKind::parallelIndexed)
      return true;
  }
  return false;
}

// True when some reader indexes this position with a reduction IV: a
// contraction axis. (Reused as one input alongside
// findContractionTilingCandidate for the BlockContraction case.)
static bool isReductionPosition(const sde::ArrayAccessProfile &profile,
                                unsigned pos) {
  for (const sde::ArrayPositionUse &use : profile.positionUses[pos]) {
    if (!use.isWrite && use.kind == sde::ArrayDimKind::reductionIndexed)
      return true;
  }
  return false;
}

// Node-agnostic DB/MU block-byte budget: block count grows with problem size,
// not node/worker count. Runtime ownership routing is derived later in ARTS.
static constexpr int64_t kTargetBlockBytes = 2 * 1024 * 1024;

// Owner-block shape sized so each block's footprint nears kTargetBlockBytes.
static SmallVector<int64_t, 4>
blockShapeFromBudget(ArrayRef<int64_t> staticShape, int64_t elemBytes,
                     ArrayRef<int64_t> ownerPositions, int64_t targetBytes) {
  SmallVector<int64_t, 4> blockShape(staticShape.begin(), staticShape.end());
  if (ownerPositions.empty() || elemBytes <= 0 || targetBytes <= 0)
    return blockShape;
  int64_t totalBytes = productOf(staticShape) * elemBytes;
  int64_t desiredBlocks =
      std::max<int64_t>(1, llvm::divideCeil(totalBytes, targetBytes));
  SmallVector<int64_t, 4> ownerExtents;
  for (int64_t pos : ownerPositions)
    if (pos >= 0 && static_cast<size_t>(pos) < staticShape.size())
      ownerExtents.push_back(staticShape[pos]);
  if (ownerExtents.empty())
    return blockShape;
  // Reuse the tested owner-dim factoring: spread desiredBlocks across owner
  // dims.
  SmallVector<int64_t, 4> grid =
      sde::factorWorkersAcrossDims(desiredBlocks, ownerExtents);
  for (auto [idx, pos] : llvm::enumerate(ownerPositions)) {
    if (pos < 0 || static_cast<size_t>(pos) >= blockShape.size())
      continue;
    int64_t g = (idx < grid.size()) ? std::max<int64_t>(1, grid[idx]) : 1;
    blockShape[pos] =
        std::max<int64_t>(1, llvm::divideCeil(staticShape[pos], g));
  }
  return blockShape;
}

static sde::ArrayLayoutCandidate
makeBlockCandidate(const sde::ArrayAccessProfile &profile,
                   ArrayRef<int64_t> ownerPositions,
                   sde::ArrayLayoutKind kind) {
  sde::ArrayLayoutCandidate candidate;
  candidate.kind = kind;
  candidate.ownerPositions.assign(ownerPositions.begin(), ownerPositions.end());
  candidate.blockShape.assign(profile.staticShape.begin(),
                              profile.staticShape.end());
  for (int64_t pos : ownerPositions) {
    if (pos < 0 || static_cast<size_t>(pos) >= candidate.blockShape.size())
      continue;
    int64_t extent = profile.staticShape[pos];
    candidate.blockShape[pos] =
        std::max<int64_t>(1, llvm::divideCeil(extent, kAbstractBlockFactor));
  }
  return candidate;
}

static sde::ArrayLayoutCandidate
makeReplicatedCandidate(const sde::ArrayAccessProfile &profile) {
  sde::ArrayLayoutCandidate candidate;
  candidate.kind = sde::ArrayLayoutKind::replicated;
  candidate.blockShape.assign(profile.staticShape.begin(),
                              profile.staticShape.end());
  return candidate;
}

// Enumerate candidate layouts for one array, pattern-free.
//
// `contractionPosition`, when set, is the physical position on which some
// sibling consumer reduces this array (from findContractionTilingCandidate +
// isSiblingDistributedIntermediate). It is the explicit BlockContraction input:
// a sibling-distributed intermediate consumed on its contraction axis gets a
// contraction-axis owner candidate even though its own writer indexes it in
// parallel.
static SmallVector<sde::ArrayLayoutCandidate, 4>
enumerateCandidates(const sde::ArrayAccessProfile &profile,
                    std::optional<unsigned> contractionPosition,
                    bool preserveFullWriterOwnerTile) {
  SmallVector<sde::ArrayLayoutCandidate, 4> candidates;

  // BlockParallel: owner = parallel-indexed positions (writer's owner-computes
  // axis, or — for a pure input — the positions consumers read in parallel).
  SmallVector<int64_t, 4> parallelOwner;
  for (unsigned pos = 0; pos < profile.rank; ++pos)
    if (isParallelOwnerPosition(profile, pos))
      parallelOwner.push_back(static_cast<int64_t>(pos));
  if (!parallelOwner.empty()) {
    // The full owner-tile candidate (every parallel position distributed). Best
    // when one consumer aligns on all of them.
    candidates.push_back(makeBlockCandidate(
        profile, parallelOwner, sde::ArrayLayoutKind::blockParallel));
    // Single-position owner candidates (row-block / col-block). For an input
    // read with different parallel positions by different consumers, no single
    // all-positions block aligns both; offering each axis lets PhaseC pick the
    // block layout that aligns the dominant consumer and leaves only one
    // redistribution edge. The candidate is only meaningful when more than one
    // parallel position exists.
    if (parallelOwner.size() > 1 && !preserveFullWriterOwnerTile) {
      for (int64_t pos : parallelOwner)
        candidates.push_back(makeBlockCandidate(
            profile, {pos}, sde::ArrayLayoutKind::blockParallel));
    }
  }

  // BlockContraction (explicit findContractionTilingCandidate input): a
  // sibling-distributed intermediate consumed on its contraction axis. This
  // owner is the contraction position regardless of the writer's parallel
  // index.
  if (contractionPosition && *contractionPosition < profile.rank) {
    candidates.push_back(makeBlockCandidate(
        profile, {static_cast<int64_t>(*contractionPosition)},
        sde::ArrayLayoutKind::blockContraction));
  } else if (profile.hasWriter) {
    // Generic contraction candidate: a reduction-indexed position with no
    // parallel writer owner.
    for (unsigned pos = 0; pos < profile.rank; ++pos) {
      if (isParallelOwnerPosition(profile, pos))
        continue;
      if (!isReductionPosition(profile, pos))
        continue;
      candidates.push_back(
          makeBlockCandidate(profile, {static_cast<int64_t>(pos)},
                             sde::ArrayLayoutKind::blockContraction));
    }
  }

  // Replicated / host-whole: always available as the highest-cost candidate.
  candidates.push_back(makeReplicatedCandidate(profile));
  return candidates;
}

//===----------------------------------------------------------------------===//
// PhaseC — cost model + assignment
//===----------------------------------------------------------------------===//

// Abstract communication volume (bytes) a candidate layout costs the module,
// closed-form and pattern-free:
//   - replicated: full array must be present everywhere => ~ N*(P-1)/P bytes of
//     fill, charged once.
//   - block layouts: each reader whose access geometry disagrees with the owner
//     positions pays a redistribution edge. An owner-aligned read costs 0; a
//     full-extent / owner-permuted read costs ~ N*(P-1)/P; a cross-owner
//     reduction read (the contraction case) costs ~ output*logP.
// P is the abstract block factor; aligned edges are exactly 0.
static int64_t estimateCommVolume(
    const sde::ArrayAccessProfile &profile,
    const sde::ArrayLayoutCandidate &candidate,
    SmallVectorImpl<std::pair<unsigned, int64_t>> &disagreeingReaderBytes) {
  disagreeingReaderBytes.clear();
  int64_t elemBytes = std::max<int64_t>(1, elementBytes(profile.root));
  int64_t totalElements = productOf(profile.staticShape);
  int64_t totalBytes = totalElements * elemBytes;
  int64_t blockFactor = kAbstractBlockFactor;
  // N*(P-1)/P.
  int64_t fullExtentEdge =
      (totalBytes * (blockFactor - 1)) / std::max<int64_t>(1, blockFactor);

  if (candidate.kind == sde::ArrayLayoutKind::replicated)
    return fullExtentEdge;

  // Owner positions of the candidate.
  llvm::SmallBitVector ownerBits(profile.rank);
  for (int64_t pos : candidate.ownerPositions)
    if (pos >= 0 && static_cast<size_t>(pos) < profile.rank)
      ownerBits.set(pos);

  // The "home" loop-dim per owner position from the writer's parallel index.
  // A reader is aligned when, for every owner position, it indexes that
  // position with the same parallel loop-dim kind as the owner. We approximate
  // alignment geometrically: the reader is aligned if it parallel-indexes every
  // owner position with offset 0; it disagrees otherwise.
  int64_t total = 0;
  // Distinct reader scheduling units and their worst-case per-position use.
  llvm::DenseMap<unsigned, bool> readerAligned;
  llvm::DenseMap<unsigned, bool> readerCrossOwnerReduction;
  for (unsigned pos = 0; pos < profile.rank; ++pos) {
    bool ownerPos = ownerBits.test(pos);
    for (const sde::ArrayPositionUse &use : profile.positionUses[pos]) {
      if (use.isWrite)
        continue;
      bool &aligned = readerAligned.try_emplace(use.suId, true).first->second;
      bool &crossReduce =
          readerCrossOwnerReduction.try_emplace(use.suId, false).first->second;
      if (!ownerPos)
        continue;
      // This reader touches an owner position: alignment depends on how.
      switch (use.kind) {
      case sde::ArrayDimKind::parallelIndexed:
        // Owner-aligned read of the owned axis: cost 0.
        break;
      case sde::ArrayDimKind::parallelHalo:
        // Neighborhood read: geometrically a (small) disagreement.
        aligned = false;
        break;
      case sde::ArrayDimKind::reductionIndexed:
        // The owned axis is consumed as a contraction axis: cross-owner
        // reduction edge.
        aligned = false;
        crossReduce = true;
        break;
      case sde::ArrayDimKind::broadcast:
        aligned = false;
        break;
      }
    }
  }

  int64_t outputBytes = std::max<int64_t>(1, fullExtentEdge);
  int64_t crossOwnerReduceEdge = outputBytes * integerLog2Ceil(blockFactor);

  for (auto &entry : readerAligned) {
    unsigned suId = entry.first;
    bool aligned = entry.second;
    if (aligned)
      continue;
    bool crossReduce = readerCrossOwnerReduction.lookup(suId);
    int64_t edgeBytes = crossReduce ? crossOwnerReduceEdge : fullExtentEdge;
    disagreeingReaderBytes.push_back({suId, edgeBytes});
    total += edgeBytes;
  }

  // A block layout that no reader can align to is no better than replicated for
  // those readers; if every reader disagrees and the array has no aligned
  // consumer, the block layout still beats replicated by the writer's locality,
  // so we keep the block cost (sum of edges) which is naturally <= replicated
  // when at least one reader aligns.
  return total;
}

struct ChosenLayout {
  sde::ArrayLayoutCandidate layout;
  int64_t commVolumeBytes = 0;
  SmallVector<std::pair<unsigned, int64_t>, 2> disagreeingReaderBytes;
};

// PhaseC: pick the minimum-cost candidate. Greedy seed = the writer's owner
// dims (owner-computes) is naturally expressed because the BlockParallel
// candidate is built from the writer's parallel-indexed positions; the cost
// model then confirms it against readers, and Replicated is selected only when
// block layouts cost more (e.g. every consumer disagrees and the array is
// tiny).
//
// When `contractionPosition` is set the array is a sibling-distributed
// intermediate consumed on its contraction axis; the contraction consumption is
// the dominant edge, so a BlockContraction candidate aligned to it wins ties
// and is preferred over a parallel home that would force every contraction read
// to redistribute. Otherwise ties prefer block over replicated, and parallel
// over contraction (the simpler edge).
static ChosenLayout assignLayout(const sde::ArrayAccessProfile &profile,
                                 std::optional<unsigned> contractionPosition,
                                 bool preserveFullWriterOwnerTile) {
  SmallVector<sde::ArrayLayoutCandidate, 4> candidates = enumerateCandidates(
      profile, contractionPosition, preserveFullWriterOwnerTile);
  bool preferContraction = contractionPosition.has_value();
  bool preferFullWriterBlock =
      preserveFullWriterOwnerTile && profile.hasFullRankWriter;

  ChosenLayout best;
  bool haveBest = false;
  int64_t bestSelectionCost = 0;
  for (const sde::ArrayLayoutCandidate &candidate : candidates) {
    SmallVector<std::pair<unsigned, int64_t>, 2> disagree;
    int64_t cost = estimateCommVolume(profile, candidate, disagree);

    // When the contraction gate fired, this array is a sibling-distributed
    // intermediate consumed on its contraction axis: the contraction
    // read is the INTENDED tiling, not a redistribution. The abstract cost
    // model otherwise undercounts a block_parallel[j] home that "aligns" the
    // parallel-j read while silently forcing the k-contraction to gather across
    // owners. Discount the contraction candidate's own contraction edge for the
    // SELECTION so the home layout matches the contraction-tiling decision; the
    // committed commVolumeBytes still reports the real abstract estimate.
    int64_t selectionCost = cost;
    if (preferContraction &&
        candidate.kind == sde::ArrayLayoutKind::blockContraction)
      selectionCost = 0;
    // A full-rank writer feeding stencil readers is owned distributed state.
    // Replication would erase the SDE layout fact and force SDE/ARTS to
    // repair state placement instead of realizing the communication edge.
    if (preferFullWriterBlock &&
        candidate.kind == sde::ArrayLayoutKind::replicated)
      selectionCost = std::numeric_limits<int64_t>::max() / 4;

    bool better = !haveBest || selectionCost < bestSelectionCost;
    if (!better && selectionCost == bestSelectionCost) {
      // Tie-break by layout-kind preference.
      auto rank = [&](sde::ArrayLayoutKind kind) -> int {
        switch (kind) {
        case sde::ArrayLayoutKind::blockParallel:
          return preferContraction ? 1 : 0;
        case sde::ArrayLayoutKind::blockContraction:
          return preferContraction ? 0 : 1;
        case sde::ArrayLayoutKind::replicated:
          return 2;
        }
        return 3;
      };
      better = rank(candidate.kind) < rank(best.layout.kind);
    }

    if (better) {
      best.layout = candidate;
      best.commVolumeBytes = cost;
      best.disagreeingReaderBytes.assign(disagree.begin(), disagree.end());
      bestSelectionCost = selectionCost;
      haveBest = true;
    }
  }
  return best;
}

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
                                       StringRef role, int64_t edgeCommBytes,
                                       int64_t elemBytes) {
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
      b.getI64IntegerAttr(computeMuBlockCount(
          staticShape, layout.ownerPositions, layout.blockShape))));
  fields.push_back(b.getNamedAttr(sde::AttrNames::LayoutGraph::CommVolumeBytes,
                                  b.getI64IntegerAttr(edgeCommBytes)));
  // Node-agnostic budget grain consumed by SDE loop tiling and distribution
  // transforms to seed the physical tile block shape. For block layouts only;
  // replicated/contraction keep the abstract grain mirrored so the field is
  // always present.
  SmallVector<int64_t, 4> budgetShape(layout.blockShape.begin(),
                                      layout.blockShape.end());
  if (layout.kind == sde::ArrayLayoutKind::blockParallel &&
      !layout.ownerPositions.empty())
    budgetShape = blockShapeFromBudget(
        staticShape, elemBytes, layout.ownerPositions, kTargetBlockBytes);
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

static bool schedulingUnitWritesAnyRoot(
    const sde::ModuleSuAccessRelations &relations, unsigned suId) {
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

static bool readerParallelIndexesPosition(const sde::ArrayAccessProfile &profile,
                                          unsigned suId, unsigned pos) {
  for (const sde::ArrayPositionUse &use : profile.positionUses[pos]) {
    if (use.suId == suId && !use.isWrite &&
        use.kind == sde::ArrayDimKind::parallelIndexed)
      return true;
  }
  return false;
}

// The consumer's required read layout: the block geometry that aligns with how
// this scheduling unit indexes the array. Cross-owner reductions and
// owner-preserving halo reads keep the module home layout; repartition readers
// commit the parallel axes they actually traverse.
static sde::ArrayLayoutCandidate
inferReaderRequiredLayout(const sde::ArrayAccessProfile &profile,
                          unsigned suId,
                          const sde::ArrayLayoutCandidate &homeLayout) {
  if (readerHasOwnerReduction(profile, suId, homeLayout.ownerPositions)) {
    sde::ArrayLayoutKind kind =
        homeLayout.kind == sde::ArrayLayoutKind::blockContraction
            ? sde::ArrayLayoutKind::blockContraction
            : sde::ArrayLayoutKind::blockParallel;
    return makeBlockCandidate(profile, homeLayout.ownerPositions, kind);
  }

  SmallVector<int64_t, 4> parallelOwner;
  for (unsigned pos = 0; pos < profile.rank; ++pos)
    if (readerParallelIndexesPosition(profile, suId, pos))
      parallelOwner.push_back(static_cast<int64_t>(pos));

  if (parallelOwner.empty() ||
      sameOwnerDimSet(parallelOwner, homeLayout.ownerPositions))
    return homeLayout;

  return makeBlockCandidate(profile, parallelOwner,
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

struct LayoutAssignmentPass
    : public sde::impl::LayoutAssignmentBase<LayoutAssignmentPass> {
  explicit LayoutAssignmentPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    // Owner-loop promotion (rank-1→rank-N su_iterate rebuilds) runs here,
    // after raise-to-sde / cu-normalization and before layout assignment.
    if (auto module = dyn_cast<ModuleOp>(getOperation()))
      sde::promoteModuleOwnerLoops(module);

    // Layout assignment is only meaningful when there is more than one logical
    // worker to distribute across. With no cost model (textual pass pipeline)
    // or a single worker, mirror the physical-layout committers and do nothing
    // — keeping single-worker IR untouched.
    if (!costModel || costModel->getLogicalWorkerCapacity() <= 1)
      return;

    MLIRContext *ctx = &getContext();
    Operation *moduleOp = getOperation();

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
      int64_t commVolumeBytes = 0;
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
      ChosenLayout chosen = assignLayout(profile, contractionPosition,
                                         preserveFullWriterOwnerTile);

      // PhaseD — commit on EVERY scheduling unit that accesses this root
      // (writer AND readers — the input generalization), keyed by arrayId so
      // later SDE transforms can join them.
      llvm::SmallDenseSet<unsigned, 4> accessors;
      for (const auto &posUses : profile.positionUses)
        for (const sde::ArrayPositionUse &use : posUses)
          accessors.insert(use.suId);

      llvm::DenseMap<unsigned, int64_t> edgeBytesByReader;
      for (auto [suId, edgeBytes] : chosen.disagreeingReaderBytes)
        edgeBytesByReader[suId] += edgeBytes;

      for (unsigned suId : accessors) {
        if (suId >= updates.size())
          continue;
        bool isWrite = schedulingUnitWritesRoot(profile, suId);
        int64_t edgeBytes = edgeBytesByReader.lookup(suId);
        sde::ArrayLayoutCandidate layoutForSu = chosen.layout;
        if (!isWrite && edgeBytes > 0)
          layoutForSu = inferReaderRequiredLayout(profile, suId, chosen.layout);
        DictionaryAttr entry = buildLayoutEntry(
            ctx, arrayId, profile.staticShape, layoutForSu,
            isWrite ? sde::AttrNames::LayoutGraphValues::RoleWrite
                    : sde::AttrNames::LayoutGraphValues::RoleRead,
            edgeBytes, std::max<int64_t>(1, elementBytes(profile.root)));
        // Aligned readers carry no transitional arrayLayout unless they share a
        // scheduling unit with a writer that still needs local block structure
        // (e.g. coupled read/write in the same reduction nest).
        if (!isWrite && edgeBytes == 0 &&
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
        } else {
          updates[suId].entries.push_back(entry);
        }
        updates[suId].roots.push_back(
            {profile.root, arrayId,
             isWrite ? sde::SdeAccessMode::write : sde::SdeAccessMode::read});
        updates[suId].commVolumeBytes += edgeBytes;
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
      if (update.commVolumeBytes > 0)
        op.setCommVolumeBytesAttr(
            IntegerAttr::get(IntegerType::get(ctx, 64), update.commVolumeBytes));

      OpBuilder builder(&op.getBody().front(), op.getBody().front().begin());
      for (const RootProvenance &root : update.roots) {
        bool exists = false;
        for (sde::SdeArrayLayoutRootOp existing :
             op.getBody().front().getOps<sde::SdeArrayLayoutRootOp>()) {
          exists |=
              existing.getRoot() == root.root &&
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

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass> createLayoutAssignmentPass(sde::SDECostModel *costModel) {
  return std::make_unique<LayoutAssignmentPass>(costModel);
}

} // namespace mlir::carts::sde
