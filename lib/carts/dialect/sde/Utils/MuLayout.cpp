///==========================================================================///
/// File: MuLayout.cpp
///
/// Implementation of the SDE MU block-grid layout geometry (see MuLayout.h).
///==========================================================================///

#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/utils/Numeric.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "llvm/ADT/STLExtras.h"
#include <functional>

using namespace mlir;

namespace mlir::carts::sde {

using carts::ceilDivPositive;

std::optional<MuPhysicalLayout>
resolveMuPhysicalLayout(MemRefType logicalType, ArrayRef<int64_t> ownerVals,
                        ArrayRef<int64_t> blockVals) {
  if (!logicalType || !logicalType.hasStaticShape())
    return std::nullopt;
  if (ownerVals.empty() || blockVals.empty())
    return std::nullopt;

  ArrayRef<int64_t> shape = logicalType.getShape();
  const unsigned rank = shape.size();

  MuPhysicalLayout plan;
  plan.logicalShape.assign(shape.begin(), shape.end());

  // Owner dims: in range, unique, ascending position is not required by the
  // attr but each must be a distinct valid dim.
  llvm::SmallVector<bool, 4> isOwner(rank, false);
  for (int64_t od : ownerVals) {
    if (od < 0 || static_cast<unsigned>(od) >= rank)
      return std::nullopt;
    if (isOwner[od])
      return std::nullopt; // duplicated owner dim
    isOwner[od] = true;
    plan.ownerDims.push_back(static_cast<unsigned>(od));
  }

  // Block extents. physicalBlockShape is accepted rank-length (full on
  // non-owner dims, the authored form) or owner-dim-length.
  auto blockExtentForOwner = [&](unsigned ownerSlot,
                                 unsigned dim) -> std::optional<int64_t> {
    if (blockVals.size() == rank)
      return blockVals[dim];
    if (blockVals.size() == plan.ownerDims.size())
      return blockVals[ownerSlot];
    return std::nullopt;
  };

  // Rank-length block shape must equal the full extent on every non-owner dim
  // (a block that tiles a non-owner dim is not the single-owner normal form).
  if (blockVals.size() == rank) {
    for (unsigned d = 0; d < rank; ++d) {
      if (!isOwner[d] && blockVals[d] != shape[d])
        return std::nullopt;
    }
  } else if (blockVals.size() != plan.ownerDims.size()) {
    return std::nullopt;
  }

  for (auto [slot, dim] : llvm::enumerate(plan.ownerDims)) {
    std::optional<int64_t> be = blockExtentForOwner(slot, dim);
    if (!be || *be <= 0 || *be > shape[dim])
      return std::nullopt;
    int64_t count = ceilDivPositive(shape[dim], *be);
    if (count <= 1)
      return std::nullopt; // no real grid; nothing to expand
    plan.blockExtents.push_back(*be);
    plan.blockCounts.push_back(count);
  }

  // CANONICAL ORDER (C0): owner dims are normalized ASCENDING here so the grid
  // prefix emitted by buildExpandedMuType (one block-count per owner dim, in
  // vector order) agrees with the ascending tiled-dim scan in recoverOwnerDims.
  // ownerDims/blockExtents/blockCounts are permuted together in lockstep.
  {
    llvm::SmallVector<unsigned, 4> order(plan.ownerDims.size());
    for (unsigned i = 0; i < order.size(); ++i)
      order[i] = i;
    llvm::sort(order, [&](unsigned a, unsigned b) {
      return plan.ownerDims[a] < plan.ownerDims[b];
    });
    llvm::SmallVector<unsigned, 2> sortedOwnerDims;
    llvm::SmallVector<int64_t, 2> sortedBlockExtents;
    llvm::SmallVector<int64_t, 2> sortedBlockCounts;
    sortedOwnerDims.reserve(order.size());
    sortedBlockExtents.reserve(order.size());
    sortedBlockCounts.reserve(order.size());
    for (unsigned idx : order) {
      sortedOwnerDims.push_back(plan.ownerDims[idx]);
      sortedBlockExtents.push_back(plan.blockExtents[idx]);
      sortedBlockCounts.push_back(plan.blockCounts[idx]);
    }
    plan.ownerDims = std::move(sortedOwnerDims);
    plan.blockExtents = std::move(sortedBlockExtents);
    plan.blockCounts = std::move(sortedBlockCounts);
  }

  return plan;
}

std::optional<MuPhysicalLayout>
resolveMuPhysicalLayout(MemRefType logicalType, ArrayAttr physicalOwnerDims,
                        ArrayAttr physicalBlockShape) {
  if (!physicalOwnerDims || !physicalBlockShape)
    return std::nullopt;

  std::optional<SmallVector<int64_t, 4>> ownerVals =
      ::mlir::carts::readI64ArrayAttr(physicalOwnerDims);
  std::optional<SmallVector<int64_t, 4>> blockVals =
      ::mlir::carts::readI64ArrayAttr(physicalBlockShape);
  if (!ownerVals || !blockVals)
    return std::nullopt;
  return resolveMuPhysicalLayout(logicalType, *ownerVals, *blockVals);
}

MemRefType buildExpandedMuType(MemRefType logicalType,
                               const MuPhysicalLayout &plan) {
  ArrayRef<int64_t> shape = logicalType.getShape();

  // Prefix grid dims (one block-count per owner dim, in committed order), then
  // the full logical rank with owner dims replaced by their block extent.
  SmallVector<int64_t, 6> expanded;
  expanded.reserve(plan.expandedRank());
  for (int64_t count : plan.blockCounts)
    expanded.push_back(count);

  llvm::SmallVector<std::optional<int64_t>, 4> ownerBlockForDim(shape.size());
  for (auto [slot, dim] : llvm::enumerate(plan.ownerDims))
    ownerBlockForDim[dim] = plan.blockExtents[slot];

  for (unsigned d = 0; d < shape.size(); ++d)
    expanded.push_back(ownerBlockForDim[d] ? *ownerBlockForDim[d] : shape[d]);

  return MemRefType::get(expanded, logicalType.getElementType(),
                         MemRefLayoutAttrInterface(),
                         logicalType.getMemorySpace());
}

std::optional<llvm::SmallVector<unsigned, 2>>
recoverOwnerDims(MemRefType expandedType,
                 llvm::ArrayRef<int64_t> logicalShape) {
  if (!expandedType || !expandedType.hasStaticShape())
    return std::nullopt;

  const unsigned logicalRank = logicalShape.size();
  const unsigned expandedRank = expandedType.getRank();
  if (expandedRank <= logicalRank)
    return std::nullopt;

  const unsigned numGrid = expandedRank - logicalRank;
  ArrayRef<int64_t> eshape = expandedType.getShape();
  ArrayRef<int64_t> grid = eshape.take_front(numGrid);
  ArrayRef<int64_t> tiles = eshape.drop_front(numGrid);

  // Each tiled trailing dim (tile extent strictly smaller than the logical
  // extent) must pair, in order, with one prefix grid dim whose extent is the
  // ceilDiv of that logical extent.
  llvm::SmallVector<unsigned, 2> owners;
  for (unsigned d = 0; d < logicalRank; ++d) {
    if (tiles[d] >= logicalShape[d])
      continue;
    unsigned slot = owners.size();
    if (slot >= numGrid)
      return std::nullopt;
    int64_t expectedCount = (logicalShape[d] + tiles[d] - 1) / tiles[d];
    if (grid[slot] != expectedCount)
      return std::nullopt;
    owners.push_back(d);
  }

  if (owners.size() != numGrid)
    return std::nullopt; // ambiguous: grid dims without a matching tiled dim
  return owners;
}

std::optional<RecoveredMuPhysicalLayout>
recoverMuPhysicalLayoutFromExpandedShape(ArrayRef<int64_t> shape,
                                         Type elementType) {
  if (shape.empty())
    return std::nullopt;
  MemRefType expandedType = MemRefType::get(shape, elementType);
  return recoverMuPhysicalLayoutFromExpandedType(expandedType);
}

std::optional<RecoveredMuPhysicalLayout>
recoverMuPhysicalLayoutFromExpandedType(MemRefType expandedType) {
  if (!expandedType || !expandedType.hasStaticShape())
    return std::nullopt;

  const unsigned rank = expandedType.getRank();
  if (rank <= 1)
    return std::nullopt;
  ArrayRef<int64_t> shape = expandedType.getShape();

  auto tryOwnerAssignment = [&](unsigned logicalRank, unsigned numGrid,
                                ArrayRef<int64_t> grid, ArrayRef<int64_t> tiles,
                                ArrayRef<unsigned> owners)
      -> std::optional<RecoveredMuPhysicalLayout> {
    if (owners.size() != numGrid)
      return std::nullopt;
    SmallVector<unsigned, 4> sortedOwners(owners.begin(), owners.end());
    llvm::sort(sortedOwners);

    SmallVector<int64_t, 4> logicalShape(tiles.begin(), tiles.end());
    for (auto [slot, dim] : llvm::enumerate(sortedOwners)) {
      if (grid[slot] <= 0 || tiles[dim] <= 0)
        return std::nullopt;
      logicalShape[dim] = grid[slot] * tiles[dim];
    }

    std::optional<SmallVector<unsigned, 2>> recovered =
        recoverOwnerDims(expandedType, logicalShape);
    if (!recovered || *recovered != sortedOwners)
      return std::nullopt;

    for (auto [slot, dim] : llvm::enumerate(sortedOwners)) {
      if (grid[slot] != ceilDivPositive(logicalShape[dim], tiles[dim]))
        return std::nullopt;
    }
    for (unsigned dim = 0; dim < logicalRank; ++dim) {
      if (llvm::is_contained(sortedOwners, dim))
        continue;
      if (tiles[dim] != logicalShape[dim])
        return std::nullopt;
    }

    RecoveredMuPhysicalLayout result;
    result.ownerDims.assign(sortedOwners.begin(), sortedOwners.end());
    result.logicalShape = std::move(logicalShape);
    result.physicalBlockShape.assign(tiles.begin(), tiles.end());
    return result;
  };

  std::function<std::optional<RecoveredMuPhysicalLayout>(
      unsigned, unsigned, SmallVector<unsigned, 4> &)>
      searchOwners = [&](unsigned logicalRank, unsigned numGrid,
                         SmallVector<unsigned, 4> &current)
      -> std::optional<RecoveredMuPhysicalLayout> {
    ArrayRef<int64_t> grid = shape.take_front(numGrid);
    ArrayRef<int64_t> tiles = shape.drop_front(numGrid);
    if (current.size() == numGrid)
      return tryOwnerAssignment(logicalRank, numGrid, grid, tiles, current);
    unsigned start = current.empty() ? 0 : current.back() + 1;
    for (unsigned dim = start; dim < logicalRank; ++dim) {
      current.push_back(dim);
      if (std::optional<RecoveredMuPhysicalLayout> found =
              searchOwners(logicalRank, numGrid, current))
        return found;
      current.pop_back();
    }
    return std::nullopt;
  };

  for (unsigned logicalRank = 1; logicalRank < rank; ++logicalRank) {
    const unsigned numGrid = rank - logicalRank;
    SmallVector<unsigned, 4> current;
    if (std::optional<RecoveredMuPhysicalLayout> found =
            searchOwners(logicalRank, numGrid, current))
      return found;
  }

  return std::nullopt;
}

} // namespace mlir::carts::sde
