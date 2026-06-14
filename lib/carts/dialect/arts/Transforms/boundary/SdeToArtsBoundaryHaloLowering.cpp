///==========================================================================///
/// File: SdeToArtsBoundaryHaloLowering.cpp
/// Compact halo helper queries for SDE-to-ARTS boundary lowering.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHaloLowering.h"

#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"
#include "llvm/ADT/STLExtras.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {

FailureOr<SmallVector<int64_t, 4>> getOwnerHaloRadii(ArrayAttr haloShape,
                                                     unsigned ownerDimCount,
                                                     Operation *context) {
  SmallVector<int64_t, 4> radii(ownerDimCount, 0);
  if (!haloShape)
    return radii;

  std::optional<SmallVector<int64_t, 4>> parsed = readI64ArrayAttr(haloShape);
  if (!parsed) {
    context->emitError()
        << "has non-integer halo shape on a committed read dependency";
    return failure();
  }
  if (parsed->size() < ownerDimCount) {
    context->emitError()
        << "has halo shape with fewer entries than owner dimensions";
    return failure();
  }

  for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
    int64_t radius = (*parsed)[slot];
    if (radius < 0) {
      context->emitError()
          << "has negative halo radius on a committed read dependency";
      return failure();
    }
    radii[slot] = radius;
  }
  return radii;
}

bool hasGroupedOwnerBlocks(ArrayRef<int64_t> groupBlockCounts) {
  return llvm::any_of(groupBlockCounts,
                      [](int64_t count) { return count > 1; });
}

FailureOr<int64_t> requireStaticPositiveIndex(Value value, Operation *context,
                                              StringRef name) {
  std::optional<int64_t> folded = ValueAnalysis::tryFoldConstantIndex(
      ValueAnalysis::stripNumericCasts(value));
  if (!folded || *folded <= 0) {
    context->emitError() << "requires static positive " << name
                         << " for ARTS compact halo face realization";
    return failure();
  }
  return *folded;
}

void attachStencilHaloAcquireFacts(sde::SdeSuIterateOp source,
                                   arts::DbAcquireOp acquire,
                                   ArrayRef<int64_t> minOffsets,
                                   ArrayRef<int64_t> maxOffsets) {
  acquire.setDepPatternAttr(
      ArtsDepPatternAttr::get(source.getContext(), ArtsDepPattern::stencil));
  acquire.setDistributionPatternAttr(EdtDistributionPatternAttr::get(
      source.getContext(), EdtDistributionPattern::stencil));
  OpBuilder attrBuilder(source.getContext());
  acquire->setAttr(acquire.getStencilMinOffsetsAttrName(),
                   attrBuilder.getI64ArrayAttr(minOffsets));
  acquire->setAttr(acquire.getStencilMaxOffsetsAttrName(),
                   attrBuilder.getI64ArrayAttr(maxOffsets));
  if (auto ownerDims = source.getOwnerDimsAttr())
    acquire->setAttr(acquire.getStencilOwnerDimsAttrName(), ownerDims);
  if (auto spatialDims = source.getSpatialDimsAttr())
    acquire->setAttr(acquire.getStencilSpatialDimsAttrName(), spatialDims);
  acquire->setAttr(acquire.getStencilSupportedBlockHaloAttrName(),
                   UnitAttr::get(source.getContext()));
}

} // namespace mlir::carts::arts::boundary
