///==========================================================================///
/// File: MuLayout.cpp
///
/// Implementation of the SDE MU block-grid layout geometry (see MuLayout.h).
///==========================================================================///

#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/utils/ArrayAttrUtils.h"

using namespace mlir;

namespace mlir::carts::sde {

static int64_t ceilDivPositive(int64_t value, int64_t divisor) {
  if (divisor <= 0)
    return value;
  return (value + divisor - 1) / divisor;
}

std::optional<MuPhysicalLayout>
resolveMuPhysicalLayout(MemRefType logicalType, ArrayAttr physicalOwnerDims,
                        ArrayAttr physicalBlockShape) {
  if (!logicalType || !logicalType.hasStaticShape())
    return std::nullopt;
  if (!physicalOwnerDims || !physicalBlockShape)
    return std::nullopt;

  std::optional<SmallVector<int64_t, 4>> ownerVals =
      ::mlir::carts::readI64ArrayAttr(physicalOwnerDims);
  std::optional<SmallVector<int64_t, 4>> blockVals =
      ::mlir::carts::readI64ArrayAttr(physicalBlockShape);
  if (!ownerVals || !blockVals || ownerVals->empty())
    return std::nullopt;

  ArrayRef<int64_t> shape = logicalType.getShape();
  const unsigned rank = shape.size();

  MuPhysicalLayout plan;
  plan.logicalShape.assign(shape.begin(), shape.end());

  // Owner dims: in range, unique, ascending position is not required by the
  // attr but each must be a distinct valid dim.
  llvm::SmallVector<bool, 4> isOwner(rank, false);
  for (int64_t od : *ownerVals) {
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
    if (blockVals->size() == rank)
      return (*blockVals)[dim];
    if (blockVals->size() == plan.ownerDims.size())
      return (*blockVals)[ownerSlot];
    return std::nullopt;
  };

  // Rank-length block shape must equal the full extent on every non-owner dim
  // (a block that tiles a non-owner dim is not the single-owner normal form).
  if (blockVals->size() == rank) {
    for (unsigned d = 0; d < rank; ++d) {
      if (!isOwner[d] && (*blockVals)[d] != shape[d])
        return std::nullopt;
    }
  } else if (blockVals->size() != plan.ownerDims.size()) {
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

  return plan;
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

} // namespace mlir::carts::sde
