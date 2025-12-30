///==========================================================================///
/// File: DbStencilRewriter.cpp
///
/// Implementation of stencil-aware index localization.
/// Current default for stencil is element-wise; ESD is the planned path.
///==========================================================================///

#include "arts/Transforms/Datablock/DbStencilRewriter.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Transforms/Datablock/DbElementWiseRewriter.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"

#define DEBUG_TYPE "db_stencil"

ARTS_DEBUG_SETUP(db_stencil);

using namespace mlir;
using namespace mlir::arts;

/// Note: getIndicesFromOp() is now in DbRewriterBase.h

DbStencilRewriter::DbStencilRewriter(Value baseChunkSize, Value startChunk,
                                     Value haloLeft, Value haloRight,
                                     Value totalRows, unsigned outerRank,
                                     unsigned innerRank, Value elemOffset,
                                     Value elemSize, ValueRange oldElementSizes)
    : DbRewriterBase(outerRank, innerRank), baseChunkSize_(baseChunkSize),
      startChunk_(startChunk), haloLeft_(haloLeft), haloRight_(haloRight),
      totalRows_(totalRows), elemOffset_(elemOffset), elemSize_(elemSize),
      oldElementSizes_(oldElementSizes.begin(), oldElementSizes.end()) {}

Value DbStencilRewriter::clampToBounds(Value globalIdx, OpBuilder &builder,
                                       Location loc) {
  /// Clamp global index to valid array bounds [0, totalRows-1]
  /// This handles boundary workers where stencil would access out-of-bounds.
  /// The kernel's boundary guard (if condition) prevents actual use of
  /// clamped values - we just need to avoid invalid memory accesses.

  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);

  /// Clamp lower bound: max(globalIdx, 0)
  /// Use signed comparison since globalIdx could be negative for u[i-1] at i=0
  Value clamped = builder.create<arith::MaxSIOp>(loc, globalIdx, zero);

  /// Clamp upper bound: min(clamped, totalRows - 1)
  if (totalRows_) {
    Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
    Value lastRow = builder.create<arith::SubIOp>(loc, totalRows_, one);
    clamped = builder.create<arith::MinSIOp>(loc, clamped, lastRow);
  }

  ARTS_DEBUG("DbStencilRewriter::clampToBounds");

  return clamped;
}

Value DbStencilRewriter::computeLocalIndex(Value globalIdx, Value chunkStart,
                                           OpBuilder &builder, Location loc) {
  /// Compute local index within the stencil-local chunk:
  ///   localIdx = globalIdx - chunkStart + haloLeft
  ///
  /// Example: Worker 1 (rows 64-127), haloLeft=1
  ///   globalIdx=64 -> 64 - 64 + 1 = 1 (first local data row)
  ///   globalIdx=63 -> 63 - 64 + 1 = 0 (left halo row)

  Value shifted = builder.create<arith::SubIOp>(loc, globalIdx, chunkStart);
  Value localIdx = builder.create<arith::AddIOp>(loc, shifted, haloLeft_);

  ARTS_DEBUG("DbStencilRewriter::computeLocalIndex");

  return localIdx;
}

LocalizedIndices DbStencilRewriter::localize(ArrayRef<Value> globalIndices,
                                             OpBuilder &builder, Location loc) {
  ARTS_DEBUG("DbStencilRewriter::localize delegating to element-wise");

  /// TODO: Replace with proper stencil/ESD logic when ready.
  /// For now, delegate to element-wise rewriter as placeholder.
  DbElementWiseRewriter delegate(elemOffset_, elemSize_, outerRank_, innerRank_,
                                 oldElementSizes_);
  return delegate.localize(globalIndices, builder, loc);
}

LocalizedIndices DbStencilRewriter::localizeLinearized(Value globalLinearIndex,
                                                       Value stride,
                                                       OpBuilder &builder,
                                                       Location loc) {
  ARTS_DEBUG(
      "DbStencilRewriter::localizeLinearized delegating to element-wise");

  /// TODO: Replace with proper stencil/ESD logic when ready.
  /// For now, delegate to element-wise rewriter as placeholder.
  DbElementWiseRewriter delegate(elemOffset_, elemSize_, outerRank_, innerRank_,
                                 oldElementSizes_);
  return delegate.localizeLinearized(globalLinearIndex, stride, builder, loc);
}

void DbStencilRewriter::rebaseOps(ArrayRef<Operation *> ops, Value dbPtr,
                                  Type elementType, ArtsCodegen &AC,
                                  llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbStencilRewriter::rebaseOps delegating to element-wise with "
             << ops.size() << " ops");

  /// TODO: Replace with proper stencil/ESD logic when ready.
  /// For now, delegate to element-wise rewriter as placeholder.
  DbElementWiseRewriter delegate(elemOffset_, elemSize_, outerRank_, innerRank_,
                                 oldElementSizes_);
  delegate.rebaseOps(ops, dbPtr, elementType, AC, opsToRemove);
}

void DbStencilRewriter::rewriteDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbStencilRewriter::rewriteDbRefUsers delegating to element-wise");

  /// TODO: Replace with proper stencil/ESD logic when ready.
  /// For now, delegate to element-wise rewriter as placeholder.
  DbElementWiseRewriter delegate(elemOffset_, elemSize_, outerRank_, innerRank_,
                                 oldElementSizes_);
  delegate.rewriteDbRefUsers(ref, blockArg, newElementType, builder,
                             opsToRemove);
}
