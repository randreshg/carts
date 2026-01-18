///==========================================================================///
/// File: DbChunkedRewriter.cpp
///
/// Chunked rewriter: uniform chunks with div/mod localization.
///
/// Example transformation (N=100 rows, chunkSize=25, 4 workers):
///   BEFORE:
///     %db = arts.db_alloc memref<100x10xf64>
///     arts.db_acquire %db [0] [100]            /// worker acquires all rows
///     %ref = arts.db_ref %db[%i]
///     memref.load %ref[%j]                     /// global row j
///
///   AFTER:
///     %db = arts.db_alloc memref<4x25x10xf64>  /// 4 chunks of 25 rows
///     arts.db_acquire %db [%chunk] [1]         /// worker 1: chunk[1]
///     %ref = arts.db_ref %db[%j / 25]          /// chunk = j / chunkSize
///     memref.load %ref[%j % 25]                /// local = j % chunkSize
///
/// Index localization: chunk = global / chunkSize, local = global % chunkSize
///==========================================================================///

#include "arts/Transforms/Datablock/Chunked/DbChunkedRewriter.h"
#include "arts/Transforms/Datablock/Chunked/DbChunkedIndexer.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/RemovalUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

void DbChunkedRewriter::transformAcquire(const DbRewriteAcquire &info,
                                         DbAllocOp newAlloc,
                                         OpBuilder &builder) {
  DbAcquireOp acquire = info.acquire;
  Value elemOffset = info.elemOffset;
  Value elemSize = info.elemSize;

  ARTS_DEBUG("DbChunkedRewriter::transformAcquire");

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(acquire);
  Location loc = acquire.getLoc();
  Value originalChunkIndex = acquire.getChunkIndex();
  Value originalChunkSize = acquire.getChunkSize();

  /// Update source to new allocation
  acquire.getSourceGuidMutable().assign(newAlloc.getGuid());
  acquire.getSourcePtrMutable().assign(newAlloc.getPtr());

  /// Update acquire's ptr result type to match new source
  MemRefType newPtrType = newAlloc.getPtr().getType().cast<MemRefType>();
  Type oldAcqPtrType = acquire.getPtr().getType();
  if (oldAcqPtrType != newPtrType) {
    acquire.getPtr().setType(newPtrType);

    /// Also update EDT block argument type if this acquire feeds an EDT
    auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
    if (blockArg && blockArg.getType() != newPtrType)
      blockArg.setType(newPtrType);
  }

  /// Mixed mode: full-range coarse acquire on a chunked allocation.
  if (info.isFullRange) {
    Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
    Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
    Value chunkCount = plan.numChunks
                           ? plan.numChunks
                           : (!newOuterSizes.empty() ? newOuterSizes[0] : one);

    SmallVector<Value> newOffsets{zero};
    SmallVector<Value> newSizes{chunkCount};
    acquire.getOffsetsMutable().assign(newOffsets);
    acquire.getSizesMutable().assign(newSizes);
    // Clear chunk hints for full-range acquire
    acquire.getChunkIndexMutable().clear();
    acquire.getChunkSizeMutable().clear();

    ARTS_DEBUG("  Mixed mode: coarse acquire gets full range [0, " << chunkCount
                                                                   << ")");
    if (!info.skipRebase)
      rebaseEdtUsers(acquire, builder, /*startChunk=*/zero);
    return;
  }

  /// CHUNKED MODE: div/mod model where partitions may span multiple chunks
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
  Value chunkSize = plan.chunkSize ? plan.chunkSize : one;
  chunkSize = builder.create<arith::MaxUIOp>(loc, chunkSize, one);

  Value startChunk = builder.create<arith::DivUIOp>(loc, elemOffset, chunkSize);
  Value endPos = builder.create<arith::AddIOp>(loc, elemOffset, elemSize);
  endPos = builder.create<arith::SubIOp>(loc, endPos, one);
  Value endChunk = builder.create<arith::DivUIOp>(loc, endPos, chunkSize);

  /// Clamp endChunk to valid range
  if (!newOuterSizes.empty()) {
    Value maxChunk = builder.create<arith::SubIOp>(loc, newOuterSizes[0], one);
    endChunk = builder.create<arith::MinUIOp>(loc, endChunk, maxChunk);
  }

  Value chunkCount = builder.create<arith::SubIOp>(loc, endChunk, startChunk);
  chunkCount = builder.create<arith::AddIOp>(loc, chunkCount, one);

  SmallVector<Value> newOffsets{startChunk};
  SmallVector<Value> newSizes{chunkCount};

  acquire.getOffsetsMutable().assign(newOffsets);
  acquire.getSizesMutable().assign(newSizes);

  /// Preserve original chunk hints or fall back to element-space values
  acquire.getChunkIndexMutable().assign(originalChunkIndex ? originalChunkIndex
                                                           : elemOffset);
  acquire.getChunkSizeMutable().assign(originalChunkSize ? originalChunkSize
                                                         : elemSize);

  /// Use mode-aware rebase for chunked mode EDT users
  if (!info.skipRebase)
    rebaseEdtUsers(acquire, builder, startChunk);
}

void DbChunkedRewriter::transformDbRef(DbRefOp ref, DbAllocOp newAlloc,
                                       OpBuilder &builder) {
  Location loc = ref.getLoc();
  Type newElementType = newAlloc.getAllocatedElementType();
  Value newSource = (ref.getSource() == oldAlloc.getPtr()) ? newAlloc.getPtr()
                                                           : ref.getSource();

  /// Collect load/store users of this db_ref
  SmallVector<Operation *> refUsers(ref.getResult().getUsers().begin(),
                                    ref.getResult().getUsers().end());
  RemovalUtils removal;

  bool hasLoadStoreUsers = false;
  for (Operation *user : refUsers) {
    if (!isa<memref::LoadOp, memref::StoreOp>(user))
      continue;

    hasLoadStoreUsers = true;
    OpBuilder::InsertionGuard ig(builder);
    builder.setInsertionPoint(user);
    Location userLoc = user->getLoc();

    /// Get load/store indices
    ValueRange loadStoreIndices;
    if (auto load = dyn_cast<memref::LoadOp>(user))
      loadStoreIndices = load.getIndices();
    else if (auto store = dyn_cast<memref::StoreOp>(user))
      loadStoreIndices = store.getIndices();

    SmallVector<Value> newOuter, newInner;
    Value one = builder.create<arith::ConstantIndexOp>(userLoc, 1);
    Value cs = plan.chunkSize ? plan.chunkSize : one;
    cs = builder.create<arith::MaxUIOp>(userLoc, cs, one);

    /// Div/mod for chunk selection
    if (!loadStoreIndices.empty()) {
      Value idx0 = loadStoreIndices.front();
      newOuter.push_back(builder.create<arith::DivUIOp>(userLoc, idx0, cs));
      newInner.push_back(builder.create<arith::RemUIOp>(userLoc, idx0, cs));
      for (unsigned i = 1; i < loadStoreIndices.size(); ++i)
        newInner.push_back(loadStoreIndices[i]);
    }

    if (newOuter.empty() || newInner.empty()) {
      Value zero = builder.create<arith::ConstantIndexOp>(userLoc, 0);
      if (newOuter.empty())
        newOuter.push_back(zero);
      if (newInner.empty())
        newInner.push_back(zero);
    }

    /// Create new db_ref with transformed outer indices
    auto newRef =
        builder.create<DbRefOp>(userLoc, newElementType, newSource, newOuter);

    /// Create new load/store with transformed inner indices
    if (auto load = dyn_cast<memref::LoadOp>(user)) {
      auto newLoad = builder.create<memref::LoadOp>(userLoc, newRef, newInner);
      load.replaceAllUsesWith(newLoad.getResult());
    } else if (auto store = dyn_cast<memref::StoreOp>(user)) {
      builder.create<memref::StoreOp>(userLoc, store.getValue(), newRef,
                                      newInner);
    }
    removal.markForRemoval(user);
  }

  /// For db_refs without load/store users, just update the db_ref type
  if (!hasLoadStoreUsers && !ref.getResult().use_empty()) {
    OpBuilder::InsertionGuard ig(builder);
    builder.setInsertionPoint(ref);
    SmallVector<Value> indices(ref.getIndices().begin(),
                               ref.getIndices().end());
    if (indices.empty())
      indices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
    auto newRef =
        builder.create<DbRefOp>(loc, newElementType, newSource, indices);
    ref.replaceAllUsesWith(newRef.getResult());
  }

  /// Remove old load/store operations
  removal.removeAllMarked();
}

bool DbChunkedRewriter::rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                                       Value startChunk) {
  ARTS_DEBUG("DbChunkedRewriter::rebaseEdtUsers");

  auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
  if (!blockArg)
    return false;

  /// Determine element type from DbRefOp users or block argument
  Type targetType;
  for (Operation *user : blockArg.getUsers())
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      targetType = ref.getResult().getType();
      break;
    }

  if (!targetType) {
    targetType = blockArg.getType();
    if (auto outer = targetType.dyn_cast<MemRefType>())
      if (auto inner = outer.getElementType().dyn_cast<MemRefType>())
        targetType = inner;
  }
  if (!targetType.isa_and_nonnull<MemRefType>())
    return false;

  Type derivedType = targetType;
  if (auto dbAlloc = dyn_cast_or_null<DbAllocOp>(
          DatablockUtils::getUnderlyingDbAlloc(blockArg)))
    derivedType = dbAlloc.getAllocatedElementType();

  /// Get element offset from chunk hint
  Value elemOffset = acquire.getChunkIndex();

  /// Create chunked indexer
  auto indexer = std::make_unique<DbChunkedIndexer>(
      plan.chunkSize, startChunk, elemOffset, newOuterSizes.size(),
      newInnerSizes.size());

  SmallVector<Operation *> users(blockArg.getUsers().begin(),
                                 blockArg.getUsers().end());
  llvm::SetVector<Operation *> opsToRemove;
  bool rewritten = false;

  for (Operation *user : users) {
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      indexer->transformDbRefUsers(ref, blockArg, derivedType, builder,
                                   opsToRemove);
      rewritten = true;
    }
  }

  RemovalUtils removal;
  for (Operation *op : opsToRemove)
    removal.markForRemoval(op);
  removal.removeAllMarked();

  return rewritten;
}
