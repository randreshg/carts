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

StringRef SimplePatternContract::getName() const {
  switch (family) {
  case ArtsDepPattern::uniform:
    return "uniform";
  case ArtsDepPattern::stencil:
    return "stencil";
  case ArtsDepPattern::matmul:
    return "matmul";
  case ArtsDepPattern::triangular:
    return "triangular";
  case ArtsDepPattern::wavefront_2d:
    return "wavefront-2d";
  case ArtsDepPattern::jacobi_alternating_buffers:
    return "jacobi-alternating-buffers";
  case ArtsDepPattern::elementwise_pipeline:
    return "elementwise-pipeline";
  case ArtsDepPattern::stencil_tiling_nd:
    return "stencil-tiling-nd";
  case ArtsDepPattern::cross_dim_stencil_3d:
    return "cross-dim-stencil-3d";
  case ArtsDepPattern::higher_order_stencil:
    return "higher-order-stencil";
  case ArtsDepPattern::unknown:
    return "unknown";
  }
  return "unknown";
}

void SimplePatternContract::stamp(Operation *op) const {
  PatternContract::stamp(op);
  if (!op)
    return;
  // Also stamp the derived distribution pattern if one exists
  if (auto distPattern = getDistributionPatternForDepPattern(family)) {
    setEdtDistributionPattern(op, *distPattern);
  }
}

void StencilNDPatternContract::stamp(Operation *op) const {
  PatternContract::stamp(op);
  if (!op)
    return;
  setEdtDistributionPattern(op, EdtDistributionPattern::stencil);
  setSupportedBlockHalo(op);
  ArrayRef<int64_t> dims = spatialDims.empty() ? ArrayRef<int64_t>(ownerDims)
                                               : ArrayRef<int64_t>(spatialDims);
  setStencilSpatialDims(op, dims);
  setStencilOwnerDims(op, ownerDims);
  setStencilMinOffsets(op, minOffsets);
  setStencilMaxOffsets(op, maxOffsets);
  setStencilWriteFootprint(op, writeFootprint);
  if (!blockShape.empty())
    setStencilBlockShape(op, blockShape);
}
