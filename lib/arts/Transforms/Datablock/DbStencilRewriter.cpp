///==========================================================================///
/// File: DbStencilRewriter.cpp
///
/// Stencil-aware index localization for the ESD (Ephemeral Slice Dependencies)
/// partitioning strategy.
///
/// ESD creates 3 separate acquires per stencil array:
///   1. Owned chunk - the worker's assigned rows
///   2. Left halo  - partial slice from previous chunk (via element_offsets)
///   3. Right halo - partial slice from next chunk (via element_offsets)
///
/// Index Localization:
///   localRow = globalRow - elemOffset_
///
/// Where elemOffset_ accounts for halo extension (e.g., if owned rows start
/// at 25 and haloLeft=1, elemOffset_=24 so globalRow=24 maps to localRow=0).
///
/// Boundary Safety:
///   At array boundaries, stencil accesses may reference invalid indices.
///   We clamp to valid bounds; the kernel's boundary guard prevents actual
///   use of clamped values.
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

//===----------------------------------------------------------------------===//
// Constructor
//===----------------------------------------------------------------------===//

DbStencilRewriter::DbStencilRewriter(Value chunkSize, Value startChunk,
                                     Value haloLeft, Value haloRight,
                                     Value totalRows, unsigned outerRank,
                                     unsigned innerRank, Value elemOffset,
                                     Value elemSize, ValueRange oldElementSizes)
    : DbRewriterBase(outerRank, innerRank), elemOffset_(elemOffset),
      elemSize_(elemSize) {
}

//===----------------------------------------------------------------------===//
// Helper: Clamp index to valid bounds
//===----------------------------------------------------------------------===//

/// Clamp localRow to [0, elemSize-1] to prevent out-of-bounds access.
static Value clampToValidBounds(Value localRow, Value elemSize,
                                OpBuilder &builder, Location loc) {
  if (!elemSize)
    return localRow;

  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
  Value lastValidIdx = builder.create<arith::SubIOp>(loc, elemSize, one);

  /// clamp(localRow, 0, lastValidIdx)
  Value clamped = builder.create<arith::MaxSIOp>(loc, localRow, zero);
  return builder.create<arith::MinSIOp>(loc, clamped, lastValidIdx);
}

//===----------------------------------------------------------------------===//
// Helper: Detect and get stride for linearized access
//===----------------------------------------------------------------------===//

/// Returns stride if the access pattern is linearized (single index into
/// multi-dimensional memref), otherwise returns nullptr.
static Value getLinearizedStride(ValueRange indices, Type elementType,
                                 OpBuilder &builder, Location loc) {
  if (indices.size() != 1)
    return nullptr;

  auto memrefType = elementType.dyn_cast<MemRefType>();
  if (!memrefType || memrefType.getRank() < 2)
    return nullptr;

  auto staticStride = DatablockUtils::getStaticStride(memrefType);
  if (!staticStride || *staticStride <= 1)
    return nullptr;

  return builder.create<arith::ConstantIndexOp>(loc, *staticStride);
}

//===----------------------------------------------------------------------===//
// Index Localization
//===----------------------------------------------------------------------===//

LocalizedIndices DbStencilRewriter::localize(ArrayRef<Value> globalIndices,
                                             OpBuilder &builder, Location loc) {
  LocalizedIndices result;
  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);

  ARTS_DEBUG("DbStencilRewriter::localize - " << globalIndices.size()
                                              << " indices");

  if (globalIndices.empty()) {
    result.dbRefIndices.push_back(zero);
    result.memrefIndices.push_back(zero);
    return result;
  }

  /// Each worker owns a single extended chunk, so dbRef index is always 0
  result.dbRefIndices.push_back(zero);

  /// localRow = globalRow - elemOffset_
  Value globalRow = globalIndices[0];
  Value localRow =
      elemOffset_ ? builder.create<arith::SubIOp>(loc, globalRow, elemOffset_)
                  : globalRow;

  /// Clamp for boundary safety
  localRow = clampToValidBounds(localRow, elemSize_, builder, loc);
  result.memrefIndices.push_back(localRow);

  /// Pass through remaining dimensions unchanged
  for (size_t i = 1; i < globalIndices.size(); ++i)
    result.memrefIndices.push_back(globalIndices[i]);

  return result;
}

LocalizedIndices DbStencilRewriter::localizeLinearized(Value globalLinearIdx,
                                                       Value stride,
                                                       OpBuilder &builder,
                                                       Location loc) {
  LocalizedIndices result;
  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);

  ARTS_DEBUG("DbStencilRewriter::localizeLinearized");

  /// De-linearize: row = linear / stride, col = linear % stride
  Value globalRow =
      builder.create<arith::DivUIOp>(loc, globalLinearIdx, stride);
  Value col = builder.create<arith::RemUIOp>(loc, globalLinearIdx, stride);

  /// Each worker owns a single extended chunk
  result.dbRefIndices.push_back(zero);

  /// localRow = globalRow - elemOffset_
  Value localRow =
      elemOffset_ ? builder.create<arith::SubIOp>(loc, globalRow, elemOffset_)
                  : globalRow;

  /// Clamp for boundary safety
  localRow = clampToValidBounds(localRow, elemSize_, builder, loc);

  /// Re-linearize: localLinear = localRow * stride + col
  Value localLinear = builder.create<arith::MulIOp>(loc, localRow, stride);
  localLinear = builder.create<arith::AddIOp>(loc, localLinear, col);
  result.memrefIndices.push_back(localLinear);

  return result;
}

//===----------------------------------------------------------------------===//
// Operation Rewriting
//===----------------------------------------------------------------------===//

void DbStencilRewriter::rewriteDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbStencilRewriter::rewriteDbRefUsers");

  SmallVector<Operation *> users(ref.getResult().getUsers().begin(),
                                 ref.getResult().getUsers().end());

  for (Operation *user : users) {
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPoint(user);
    Location loc = user->getLoc();

    /// Extract indices from load/store
    ValueRange indices;
    if (auto load = dyn_cast<memref::LoadOp>(user))
      indices = load.getIndices();
    else if (auto store = dyn_cast<memref::StoreOp>(user))
      indices = store.getIndices();
    else
      continue;

    if (indices.empty())
      continue;

    /// Localize indices (handles linearized access automatically)
    LocalizedIndices localized;
    if (Value stride =
            getLinearizedStride(indices, newElementType, builder, loc))
      localized = localizeLinearized(indices[0], stride, builder, loc);
    else
      localized = localize(SmallVector<Value>(indices), builder, loc);

    /// Create new db_ref with localized indices
    auto newRef = builder.create<DbRefOp>(loc, newElementType, blockArg,
                                          localized.dbRefIndices);

    /// Replace load/store with new version using localized memref indices
    if (auto load = dyn_cast<memref::LoadOp>(user)) {
      auto newLoad = builder.create<memref::LoadOp>(loc, newRef.getResult(),
                                                    localized.memrefIndices);
      load.replaceAllUsesWith(newLoad.getResult());
      opsToRemove.insert(load);
    } else if (auto store = dyn_cast<memref::StoreOp>(user)) {
      builder.create<memref::StoreOp>(loc, store.getValue(), newRef.getResult(),
                                      localized.memrefIndices);
      opsToRemove.insert(store);
    }
  }

  opsToRemove.insert(ref.getOperation());
}

void DbStencilRewriter::rebaseOps(ArrayRef<Operation *> ops, Value dbPtr,
                                  Type elementType, ArtsCodegen &AC,
                                  llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbStencilRewriter::rebaseOps - " << ops.size() << " ops");

  for (Operation *op : ops) {
    OpBuilder::InsertionGuard guard(AC.getBuilder());
    AC.setInsertionPoint(op);
    Location loc = op->getLoc();

    /// DbRefOp: delegate to rewriteDbRefUsers
    if (auto dbRef = dyn_cast<DbRefOp>(op)) {
      rewriteDbRefUsers(dbRef, dbPtr, elementType, AC.getBuilder(),
                        opsToRemove);
      continue;
    }

    /// Only handle load/store operations
    if (!isa<memref::LoadOp, memref::StoreOp>(op))
      continue;

    ValueRange indices = getIndicesFromOp(op);
    if (indices.empty())
      continue;

    /// Localize indices
    LocalizedIndices localized;
    if (Value stride =
            getLinearizedStride(indices, elementType, AC.getBuilder(), loc))
      localized = localizeLinearized(indices[0], stride, AC.getBuilder(), loc);
    else
      localized = localize(SmallVector<Value>(indices), AC.getBuilder(), loc);

    /// Create db_ref + load/store
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
