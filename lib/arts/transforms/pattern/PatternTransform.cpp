///==========================================================================///
/// File: PatternTransform.cpp
/// Shared helpers for pre-DB pattern contracts and transforms.
///==========================================================================///

#include "arts/transforms/pattern/PatternTransform.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"

using namespace mlir;
using namespace mlir::arts;

void PatternContract::stamp(Operation *op) const {
  if (!op)
    return;
  setDepPattern(op, getFamily());
  setPatternRevision(op, getRevision());
}

void PatternContract::stamp(ArrayRef<Operation *> ops) const {
  for (Operation *op : ops)
    stamp(op);
}

void StencilNDPatternContract::stamp(Operation *op) const {
  PatternContract::stamp(op);
  if (!op)
    return;
  setEdtDistributionPattern(op, EdtDistributionPattern::stencil);
  setSupportedBlockHalo(op);
  setStencilSpatialDims(op, ownerDims);
  setStencilOwnerDims(op, ownerDims);
  setStencilMinOffsets(op, minOffsets);
  setStencilMaxOffsets(op, maxOffsets);
  setStencilWriteFootprint(op, writeFootprint);
}
