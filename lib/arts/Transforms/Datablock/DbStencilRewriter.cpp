///==========================================================================///
/// File: DbStencilRewriter.cpp
///
/// Implementation of stencil-aware index localization.
/// Current default for stencil is element-wise; ESD is the planned path.
///==========================================================================///

#include "arts/Transforms/Datablock/DbStencilRewriter.h"
#include "arts/Codegen/ArtsCodegen.h"
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
                                     unsigned innerRank)
    : DbRewriterBase(outerRank, innerRank), baseChunkSize_(baseChunkSize),
      startChunk_(startChunk), haloLeft_(haloLeft), haloRight_(haloRight),
      totalRows_(totalRows) {}

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

LocalizedIndices
DbStencilRewriter::localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  ARTS_DEBUG("DbStencilRewriter::localize with " << globalIndices.size()
                                                 << " indices");

  if (globalIndices.empty()) {
    result.dbRefIndices.push_back(zero());
    result.memrefIndices.push_back(zero());
    return result;
  }

  Value globalRow = globalIndices[0];

  /// For stencil mode, each worker acquires a SINGLE chunk with halo space.
  /// The dbRefIdx is always 0 (relative to the single acquired chunk).
  result.dbRefIndices.push_back(zero());

  /// Clamp global index to valid bounds for boundary workers
  Value clampedRow = clampToBounds(globalRow, builder, loc);

  /// Compute chunk start (base row for this worker's partition)
  /// chunkStart = startChunk * baseChunkSize
  Value chunkStart =
      builder.create<arith::MulIOp>(loc, startChunk_, baseChunkSize_);

  /// Compute local index with halo offset
  Value localRow = computeLocalIndex(clampedRow, chunkStart, builder, loc);
  result.memrefIndices.push_back(localRow);

  /// Remaining dimensions pass through unchanged
  for (unsigned i = 1; i < globalIndices.size(); ++i)
    result.memrefIndices.push_back(globalIndices[i]);

  ARTS_DEBUG("DbStencilRewriter::localize -> dbRef: "
             << result.dbRefIndices.size()
             << ", memref: " << result.memrefIndices.size());
  return result;
}

LocalizedIndices
DbStencilRewriter::localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder, Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  ARTS_DEBUG("DbStencilRewriter::localizeLinearized");

  /// De-linearize: globalLinear = globalRow * stride + col
  Value globalRow =
      builder.create<arith::DivUIOp>(loc, globalLinearIndex, stride);
  Value col = builder.create<arith::RemUIOp>(loc, globalLinearIndex, stride);

  /// For stencil mode, dbRefIdx is always 0 (single chunk)
  result.dbRefIndices.push_back(zero());

  /// Clamp global row to valid bounds
  Value clampedRow = clampToBounds(globalRow, builder, loc);

  /// Compute chunk start
  Value chunkStart =
      builder.create<arith::MulIOp>(loc, startChunk_, baseChunkSize_);

  /// Compute local row with halo offset
  Value localRow = computeLocalIndex(clampedRow, chunkStart, builder, loc);

  /// Re-linearize: localLinear = localRow * stride + col
  Value localLinear = builder.create<arith::MulIOp>(loc, localRow, stride);
  localLinear = builder.create<arith::AddIOp>(loc, localLinear, col);
  result.memrefIndices.push_back(localLinear);

  return result;
}

void DbStencilRewriter::rebaseOps(ArrayRef<Operation *> ops, Value dbPtr,
                                  Type elementType, ArtsCodegen &AC,
                                  llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbStencilRewriter::rebaseOps with " << ops.size() << " ops");

  for (Operation *op : ops) {
    OpBuilder::InsertionGuard IG(AC.getBuilder());
    AC.setInsertionPoint(op);
    Location loc = op->getLoc();

    /// Handle DbRefOp specially - rewrite its users
    if (auto dbRef = dyn_cast<DbRefOp>(op)) {
      rewriteDbRefUsers(dbRef, dbPtr, elementType, AC.getBuilder(),
                        opsToRemove);
      continue;
    }

    /// Handle load/store operations
    if (!isa<memref::LoadOp, memref::StoreOp>(op))
      continue;

    ValueRange indices = getIndicesFromOp(op);
    if (indices.empty())
      continue;

    /// Detect linearized access for multi-dimensional memrefs
    bool isLinearized = false;
    Value stride;
    if (indices.size() == 1) {
      if (auto memrefType = elementType.dyn_cast<MemRefType>()) {
        if (memrefType.getRank() >= 2) {
          if (auto staticStride = DatablockUtils::getStaticStride(memrefType)) {
            if (*staticStride > 1) {
              isLinearized = true;
              stride = AC.createIndexConstant(*staticStride, loc);
            }
          }
        }
      }
    }

    LocalizedIndices localized;
    if (isLinearized && stride) {
      localized = localizeLinearized(indices[0], stride, AC.getBuilder(), loc);
    } else {
      SmallVector<Value> indicesVec(indices.begin(), indices.end());
      localized = localize(indicesVec, AC.getBuilder(), loc);
    }

    /// Create db_ref + load/store pattern
    auto dbRef =
        AC.create<DbRefOp>(loc, elementType, dbPtr, localized.dbRefIndices);

    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      auto newLoad = AC.create<memref::LoadOp>(loc, dbRef.getResult(),
                                               localized.memrefIndices);
      load.replaceAllUsesWith(newLoad.getResult());
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      AC.create<memref::StoreOp>(loc, store.getValue(), dbRef.getResult(),
                                 localized.memrefIndices);
    }
    opsToRemove.insert(op);
  }
}

void DbStencilRewriter::rewriteDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbStencilRewriter::rewriteDbRefUsers");

  SmallVector<Operation *> refUsers(ref.getResult().getUsers().begin(),
                                    ref.getResult().getUsers().end());
  for (Operation *refUser : refUsers) {
    OpBuilder::InsertionGuard IG(builder);
    builder.setInsertionPoint(refUser);
    Location userLoc = refUser->getLoc();

    ValueRange elementIndices;
    if (auto load = dyn_cast<memref::LoadOp>(refUser))
      elementIndices = load.getIndices();
    else if (auto store = dyn_cast<memref::StoreOp>(refUser))
      elementIndices = store.getIndices();
    else
      continue;

    if (elementIndices.empty())
      continue;

    /// Detect linearized access
    bool isLinearized = false;
    Value stride;
    if (elementIndices.size() == 1) {
      if (auto memrefType = newElementType.dyn_cast<MemRefType>()) {
        if (memrefType.getRank() >= 2) {
          if (auto staticStride = DatablockUtils::getStaticStride(memrefType)) {
            if (*staticStride > 1) {
              isLinearized = true;
              stride = builder.create<arith::ConstantIndexOp>(userLoc,
                                                              *staticStride);
            }
          }
        }
      }
    }

    LocalizedIndices localized;
    if (isLinearized && stride) {
      localized =
          localizeLinearized(elementIndices[0], stride, builder, userLoc);
    } else {
      SmallVector<Value> indices(elementIndices.begin(), elementIndices.end());
      localized = localize(indices, builder, userLoc);
    }

    auto newRef = builder.create<DbRefOp>(userLoc, newElementType, blockArg,
                                          localized.dbRefIndices);

    if (auto load = dyn_cast<memref::LoadOp>(refUser)) {
      auto newLoad = builder.create<memref::LoadOp>(userLoc, newRef.getResult(),
                                                    localized.memrefIndices);
      load.replaceAllUsesWith(newLoad.getResult());
      opsToRemove.insert(load);
    } else if (auto store = dyn_cast<memref::StoreOp>(refUser)) {
      builder.create<memref::StoreOp>(userLoc, store.getValue(),
                                      newRef.getResult(),
                                      localized.memrefIndices);
      opsToRemove.insert(store);
    }
  }
  opsToRemove.insert(ref.getOperation());
}
