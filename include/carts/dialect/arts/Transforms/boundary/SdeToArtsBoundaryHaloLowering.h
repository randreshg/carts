///==========================================================================///
/// File: SdeToArtsBoundaryHaloLowering.h
/// Compact halo helper queries for SDE-to-ARTS boundary lowering.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYHALOLOWERING_H
#define CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYHALOLOWERING_H

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryTypes.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/ArrayRef.h"

namespace mlir {
namespace carts::arts::boundary {

FailureOr<SmallVector<int64_t, 4>> getOwnerHaloRadii(ArrayAttr haloShape,
                                                     unsigned ownerDimCount,
                                                     Operation *context);

bool hasGroupedOwnerBlocks(ArrayRef<int64_t> groupBlockCounts);

FailureOr<int64_t> requireStaticPositiveIndex(Value value, Operation *context,
                                              StringRef name);

void attachStencilHaloAcquireFacts(sde::SdeSuIterateOp source,
                                   arts::DbAcquireOp acquire,
                                   ArrayRef<int64_t> minOffsets,
                                   ArrayRef<int64_t> maxOffsets);

void enumerateUnitHaloSourceOffsets(
    unsigned rank, SmallVectorImpl<SmallVector<int64_t, 4>> &offsets);

SmallVector<Value, 4>
buildRankExpandedElementIndices(OpBuilder &builder, Location loc,
                                ArrayRef<Value> elementIndices);

void emitCompactHaloCopy(OpBuilder &builder, Location loc,
                         ArrayRef<int64_t> sourceOffsets,
                         ArrayRef<Value> elementExtents, Value sourcePayload,
                         Value compactPayload);

} // namespace carts::arts::boundary
} // namespace mlir

#endif
