#ifndef CARTS_DIALECT_SDE_UTILS_SDEPLANUTILS_H
#define CARTS_DIALECT_SDE_UTILS_SDEPLANUTILS_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "mlir/IR/Operation.h"

namespace mlir::carts::sde {

/// True once SDE has authored CU/MU partition evidence that downstream and
/// later SDE passes must preserve rather than silently recomputing.
inline bool hasCommittedCuMuPartitionEvidence(Operation *op) {
  if (!op)
    return false;

  return op->hasAttr(AttrNames::PartitionGraph) ||
         op->hasAttr(AttrNames::PartitionScore);
}

/// True once an op carries downstream-visible CU/MU partition decisions.
inline bool hasCommittedCuMuPartitionPlan(Operation *op) {
  if (hasCommittedCuMuPartitionEvidence(op))
    return true;

  if (!op)
    return false;
  // Physical CU/MU layout facts are downstream-visible partition decisions.
  // Later passes may add missing evidence around them, but must not silently
  // recompute, coarsen, or strip the plan once these attrs exist, except for
  // explicit final SDE-owned refinement before partition evidence is stamped.
  // Distribution wrappers may still be added around the committed plan because
  // they carry scheduling intent without changing the physical CU/MU layout.
  return op->hasAttr("physicalOwnerDims") ||
         op->hasAttr("physicalBlockShape") ||
         op->hasAttr("logicalWorkerSlice") ||
         op->hasAttr("physicalHaloShape") ||
         op->hasAttr("iterationTopology") ||
         op->hasAttr("distributionKind");
}

/// True when a stencil carries a semantic N-D owner contract whose owner/access
/// dimensions cannot be realized by the current SDE loop rank. Physical tiling
/// passes use this to fail closed instead of replacing an upstream N-D stencil
/// contract with a one-dimensional fallback plan.
inline bool hasNestedStencilOwnerContract(SdeSuIterateOp op) {
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

#endif // CARTS_DIALECT_SDE_UTILS_SDEPLANUTILS_H
