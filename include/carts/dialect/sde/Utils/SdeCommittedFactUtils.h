#ifndef CARTS_DIALECT_SDE_UTILS_SDECOMMITTEDFACTUTILS_H
#define CARTS_DIALECT_SDE_UTILS_SDECOMMITTEDFACTUTILS_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "mlir/IR/Operation.h"

namespace mlir::carts::sde {

/// True once an op carries committed CU/MU physical layout facts.
inline bool hasCommittedCuMuPartitionFacts(Operation *op) {
  auto iterate = dyn_cast_or_null<SdeSuIterateOp>(op);
  if (!iterate)
    return false;
  return iterate.getPhysicalOwnerDimsAttr() ||
         iterate.getPhysicalBlockShapeAttr() ||
         iterate.getLogicalWorkerSliceAttr() ||
         iterate.getPhysicalHaloShapeAttr() ||
         iterate.getIterationTopologyAttr() ||
         iterate.getDistributionKindAttr();
}

/// True when a stencil carries N-D owner/access facts that cannot be realized
/// by the current SDE loop rank. Physical tiling passes use this to fail closed
/// instead of replacing those facts with one-dimensional fallback grain.
inline bool requiresNestedStencilOwnerPromotion(SdeSuIterateOp op) {
  auto classification = op.getStructuredClassification();
  if (!classification ||
      *classification != SdeStructuredClassification::stencil)
    return false;

  unsigned loopRank = op.getLowerBounds().size();
  auto ownerDims = readI64ArrayAttr(op.getOwnerDimsAttr());
  auto minOffsets = readI64ArrayAttr(op.getAccessMinOffsetsAttr());
  auto maxOffsets = readI64ArrayAttr(op.getAccessMaxOffsetsAttr());
  if (!ownerDims || !minOffsets || !maxOffsets)
    return false;

  if (ownerDims->size() > loopRank || minOffsets->size() > loopRank ||
      maxOffsets->size() > loopRank)
    return true;
  for (int64_t ownerDim : *ownerDims)
    if (ownerDim < 0 || static_cast<unsigned>(ownerDim) >= loopRank)
      return true;
  return false;
}

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_SDECOMMITTEDFACTUTILS_H
