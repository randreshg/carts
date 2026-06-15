///==========================================================================///
/// File: LayoutCandidateChoose.cpp
///
/// PhaseB/PhaseC of SDE per-array BLOCK layout assignment: enumerate candidate
/// element-space layouts for one array (block-parallel owner-dim,
/// block-contraction, replicated) from its affine access relations and
/// structurally choose one owner layout, recording the readers whose geometry
/// disagrees. Also owns the element-space block-sizing helpers.
///
/// Hosts the `sde-layout-candidate-choose` pass (pass 1 of the layout engine):
/// it promotes owner loops, builds access relations, chooses each array's owner
/// layout, and COMMITS that chosen logical layout as a real authoritative SDE
/// choice fact. `sde-writer-layout-commit` (pass 2) consumes the committed fact
/// and realizes the per-SU writer/reader/physical facts over it.
///
/// This is strictly DATA-LAYOUT: pattern-agnostic (driven only by affine maps,
/// iterator types, and static shapes), it NAMES NO COLLECTIVE.
///==========================================================================///

#include "LayoutAssignmentInternal.h"

#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/CuMuGraphPartitioning.h"
#include "carts/dialect/sde/Utils/IterationSizingUtils.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallBitVector.h"

#include <limits>

namespace mlir::carts::sde {
#define GEN_PASS_DEF_LAYOUTCANDIDATECHOOSE
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
    candidates.push_back(sde::detail::makeBlockCandidate(
        profile, parallelOwner, sde::ArrayLayoutKind::blockParallel));
    // Single-position owner candidates (row-block / col-block). For an input
    // read with different parallel positions by different consumers, no single
    // all-positions block aligns both; offering each axis lets PhaseC pick the
    // block layout that aligns the dominant consumer and leaves only one
    // redistribution edge. The candidate is only meaningful when more than one
    // parallel position exists.
    if (parallelOwner.size() > 1 && !preserveFullWriterOwnerTile) {
      for (int64_t pos : parallelOwner)
        candidates.push_back(sde::detail::makeBlockCandidate(
            profile, {pos}, sde::ArrayLayoutKind::blockParallel));
    }
  }

  // BlockContraction (explicit findContractionTilingCandidate input): a
  // sibling-distributed intermediate consumed on its contraction axis. This
  // owner is the contraction position regardless of the writer's parallel
  // index.
  if (contractionPosition && *contractionPosition < profile.rank) {
    candidates.push_back(sde::detail::makeBlockCandidate(
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
      candidates.push_back(sde::detail::makeBlockCandidate(
          profile, {static_cast<int64_t>(pos)},
          sde::ArrayLayoutKind::blockContraction));
    }
  }

  // Replicated / host-whole: always available as the highest-cost candidate.
  candidates.push_back(makeReplicatedCandidate(profile));
  return candidates;
}

//===----------------------------------------------------------------------===//
// PhaseC — structural layout assignment
//===----------------------------------------------------------------------===//

static int layoutKindRank(sde::ArrayLayoutKind kind, bool preferContraction) {
  switch (kind) {
  case sde::ArrayLayoutKind::blockParallel:
    return preferContraction ? 1 : 0;
  case sde::ArrayLayoutKind::blockContraction:
    return preferContraction ? 0 : 1;
  case sde::ArrayLayoutKind::replicated:
    return 2;
  }
  return 3;
}

static int candidatePriority(const sde::ArrayLayoutCandidate &candidate,
                             bool preferContraction,
                             bool preferFullWriterBlock) {
  if (preferFullWriterBlock &&
      candidate.kind == sde::ArrayLayoutKind::replicated)
    return 1000;
  if (preferContraction &&
      candidate.kind == sde::ArrayLayoutKind::blockContraction)
    return 0;
  return layoutKindRank(candidate.kind, preferContraction);
}

// Readers whose access geometry disagrees with the chosen owner layout.
static void collectDisagreeingReaders(
    const sde::ArrayAccessProfile &profile,
    const sde::ArrayLayoutCandidate &candidate,
    llvm::SmallDenseSet<unsigned, 4> &disagreeingReaders) {
  disagreeingReaders.clear();
  if (candidate.kind == sde::ArrayLayoutKind::replicated)
    return;

  llvm::SmallBitVector ownerBits(profile.rank);
  for (int64_t pos : candidate.ownerPositions)
    if (pos >= 0 && static_cast<size_t>(pos) < profile.rank)
      ownerBits.set(pos);

  llvm::DenseMap<unsigned, bool> readerAligned;
  for (unsigned pos = 0; pos < profile.rank; ++pos) {
    if (!ownerBits.test(pos))
      continue;
    for (const sde::ArrayPositionUse &use : profile.positionUses[pos]) {
      if (use.isWrite)
        continue;
      bool &aligned = readerAligned.try_emplace(use.suId, true).first->second;
      switch (use.kind) {
      case sde::ArrayDimKind::parallelIndexed:
        break;
      case sde::ArrayDimKind::parallelHalo:
      case sde::ArrayDimKind::reductionIndexed:
      case sde::ArrayDimKind::broadcast:
        aligned = false;
        break;
      }
    }
  }

  for (auto &entry : readerAligned)
    if (!entry.second)
      disagreeingReaders.insert(entry.first);
}

//===----------------------------------------------------------------------===//
// sde-layout-candidate-choose pass — choose + commit writer home layouts only
//===----------------------------------------------------------------------===//

struct LayoutCandidateChoosePass
    : public sde::impl::LayoutCandidateChooseBase<LayoutCandidateChoosePass> {
  explicit LayoutCandidateChoosePass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    // Pass 1 of the layout engine: owner-loop promotion + PhaseA +
    // PhaseB/PhaseC choice, then COMMIT the chosen logical layout per array as
    // a real authoritative SDE choice fact for sde-writer-layout-commit to
    // consume.
    sde::detail::chooseAndCommitChoiceFact(getOperation(), costModel);
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::carts::sde::detail {

int64_t computeMuBlockCount(ArrayRef<int64_t> shape,
                            ArrayRef<int64_t> ownerPositions,
                            ArrayRef<int64_t> blockShape) {
  if (ownerPositions.empty())
    return 1;
  return sde::inferCuCountFromMuPartition(shape, ownerPositions, blockShape);
}

int64_t elementBytes(Value root) {
  auto memrefTy = dyn_cast_or_null<MemRefType>(root.getType());
  if (!memrefTy)
    return 0;
  Type elt = memrefTy.getElementType();
  if (!elt.isIntOrFloat())
    return 0;
  return llvm::divideCeil(elt.getIntOrFloatBitWidth(), 8);
}

// Owner-block shape sized so each block's footprint nears targetBytes.
SmallVector<int64_t, 4> blockShapeFromBudget(ArrayRef<int64_t> staticShape,
                                             int64_t elemBytes,
                                             ArrayRef<int64_t> ownerPositions,
                                             int64_t targetBytes) {
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

ArrayLayoutCandidate makeBlockCandidate(const sde::ArrayAccessProfile &profile,
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

// PhaseC: derive layout from access relations (owner-computes / contraction /
// stencil / replicated), not from an abstract byte-cost search.
ChosenLayout assignLayout(const sde::ArrayAccessProfile &profile,
                          std::optional<unsigned> contractionPosition,
                          bool preserveFullWriterOwnerTile) {
  SmallVector<sde::ArrayLayoutCandidate, 4> candidates = enumerateCandidates(
      profile, contractionPosition, preserveFullWriterOwnerTile);
  bool preferContraction = contractionPosition.has_value();
  bool preferFullWriterBlock =
      preserveFullWriterOwnerTile && profile.hasFullRankWriter;

  const sde::ArrayLayoutCandidate *chosen = nullptr;
  int bestPriority = std::numeric_limits<int>::max();
  for (const sde::ArrayLayoutCandidate &candidate : candidates) {
    int priority =
        candidatePriority(candidate, preferContraction, preferFullWriterBlock);
    if (priority < bestPriority) {
      chosen = &candidate;
      bestPriority = priority;
    }
  }
  if (!chosen && !candidates.empty())
    chosen = &candidates.back();

  ChosenLayout result;
  if (chosen)
    result.layout = *chosen;
  if (chosen)
    collectDisagreeingReaders(profile, result.layout,
                              result.disagreeingReaders);
  return result;
}

} // namespace mlir::carts::sde::detail

namespace mlir::carts::sde {

std::unique_ptr<Pass>
createLayoutCandidateChoosePass(sde::SDECostModel *costModel) {
  return std::make_unique<LayoutCandidateChoosePass>(costModel);
}

} // namespace mlir::carts::sde
