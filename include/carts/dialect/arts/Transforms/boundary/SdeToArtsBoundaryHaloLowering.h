///==========================================================================///
/// File: SdeToArtsBoundaryHaloLowering.h
/// Compact halo helper queries for SDE-to-ARTS boundary lowering.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYHALOLOWERING_H
#define CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYHALOLOWERING_H

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryTypes.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"

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
                                unsigned ownerDimCount,
                                ArrayRef<Value> elementIndices);

FailureOr<SmallVector<Value, 4>>
buildPayloadElementIndices(OpBuilder &builder, Location loc, Value payload,
                           unsigned ownerDimCount,
                           ArrayRef<Value> elementIndices);

LogicalResult emitCompactHaloCopy(OpBuilder &builder, Location loc,
                                  unsigned ownerDimCount,
                                  ArrayRef<unsigned> ownerPayloadDims,
                                  ArrayRef<int64_t> sourceOffsets,
                                  ArrayRef<Value> elementExtents,
                                  Value sourcePayload, Value compactPayload);

FailureOr<SmallVector<Value, 4>>
getCompactHaloOwnerLoopIvs(sde::SdeSuIterateOp source, memref::LoadOp load,
                           ArrayRef<unsigned> ownerLoopDims,
                           llvm::StringRef diagnosticRank);

FailureOr<std::optional<HaloLoadRewrite>>
classify2DUnitHaloLoad(memref::LoadOp load, unsigned haloWorkIndex,
                       const Halo2DTaskWork &work, Value rowIv, Value colIv);

FailureOr<std::optional<HaloNdLoadRewrite>>
classifyNdUnitHaloLoad(memref::LoadOp load, unsigned haloWorkIndex,
                       const HaloNdTaskWork &work,
                       ArrayRef<Value> ownerLoopIvs);

FailureOr<SmallVector<SmallVector<int64_t, 4>, 8>>
collectRequiredNdUnitHaloSourceOffsets(sde::SdeSuIterateOp source,
                                       DirectDepSpec dep, Block *computeBlock,
                                       ArrayRef<unsigned> ownerLoopDims,
                                       ArrayRef<unsigned> ownerPayloadDims,
                                       ArrayRef<Value> elementExtents);

FailureOr<bool> needsExactNdHaloFor2D(sde::SdeSuIterateOp source,
                                      DirectDepSpec dep, Block *computeBlock,
                                      ArrayRef<unsigned> ownerLoopDims);

LogicalResult rewriteCloned2DUnitHaloLoads(
    arts::EdtOp task, const DenseMap<Operation *, HaloLoadRewrite> &rewrites,
    ArrayRef<Halo2DTaskWork> haloWorks, ArrayRef<Value> payloads,
    ArrayRef<SmallVector<Value, 4>> depBlockOffsetArgs);

LogicalResult rewriteClonedNdUnitHaloLoads(
    arts::EdtOp task, const DenseMap<Operation *, HaloNdLoadRewrite> &rewrites,
    ArrayRef<HaloNdTaskWork> haloWorks, ArrayRef<Value> payloads,
    ArrayRef<SmallVector<Value, 4>> depBlockOffsetArgs);

} // namespace carts::arts::boundary
} // namespace mlir

#endif
