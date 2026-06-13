///==========================================================================///
/// File: SdeToArtsBoundaryHelpers.h
/// Small SDE→ARTS boundary helper predicates and fact attachment.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYHELPERS_H
#define CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYHELPERS_H

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"

namespace mlir {
namespace carts::arts::boundary {

bool isScalarParamType(Type type);
bool isConstantLikeValue(Value value);
bool isDefinedInside(Value value, Operation *scope);
bool isStackScratchMemref(Value memref);

FailureOr<ArtsMode> convertAccessMode(sde::SdeAccessMode mode, Operation *context);
FailureOr<ArtsDepPattern> convertPattern(sde::SdePattern pattern,
                                         Operation *context);
FailureOr<EdtDistributionKind>
convertDistributionKind(sde::SdeDistributionKind kind, Operation *context);

bool hasCommittedPartialReductionFacts(sde::SdeSuIterateOp source);
LogicalResult attachCommittedSdeFacts(sde::SdeSuIterateOp source,
                                      arts::EdtOp edt);
LogicalResult attachUnpartitionedSdeFacts(sde::SdeSuIterateOp source,
                                          arts::EdtOp edt);

ArrayAttr ownerDimsForExpandedWindow(MLIRContext *ctx, unsigned ownerDimCount);
FailureOr<ArrayAttr>
blockShapeForExpandedWindow(const sde::MuAccessWindowGeometry &geom,
                            MemRefType memrefType, MLIRContext *ctx);

std::optional<SmallVector<int64_t, 4>>
readCommittedPhysicalOwnerDims(
    sde::SdeSuIterateOp source,
    const std::optional<SmallVector<int64_t, 4>> &arrayOwnerDims = std::nullopt);

} // namespace carts::arts::boundary
} // namespace mlir

#endif
