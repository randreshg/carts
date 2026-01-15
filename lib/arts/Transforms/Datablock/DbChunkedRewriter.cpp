///==========================================================================///
/// File: DbChunkedRewriter.cpp
///
/// Chunked rewriter: uniform chunks with div/mod localization.
///
/// Example transformation (N=100 rows, chunkSize=25, 4 workers):
///
///   BEFORE:
///     %db = arts.db_alloc memref<100x10xf64>
///     arts.db_acquire %db [0] [100]            // worker acquires all rows
///     %ref = arts.db_ref %db[%i]
///     memref.load %ref[%j]                     // global row j
///
///   AFTER:
///     %db = arts.db_alloc memref<4x25x10xf64>  // 4 chunks of 25 rows
///     arts.db_acquire %db [%chunk] [1]         // worker 1: chunk[1]
///     %ref = arts.db_ref %db[%j / 25]          // chunk = j / chunkSize
///     memref.load %ref[%j % 25]                // local = j % chunkSize
///
/// Index localization: chunk = global / chunkSize, local = global % chunkSize
///==========================================================================///

#include "arts/Transforms/Datablock/DbChunkedRewriter.h"
#include "arts/Transforms/Datablock/DbChunkedIndexer.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"

#define DEBUG_TYPE "db_transforms"

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
  SmallVector<Value> originalOffsetHints(acquire.getOffsetHints().begin(),
                                         acquire.getOffsetHints().end());
  SmallVector<Value> originalSizeHints(acquire.getSizeHints().begin(),
                                       acquire.getSizeHints().end());

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

  /// CHUNKED MODE: Multi-chunk div/mod model
  /// DISJOINT MODEL: Physical chunks are disjoint and indexed by
  /// plan_.chunkSize. Chunk layout: chunk 0 -> rows [0, plan_.chunkSize),
  /// chunk 1 -> rows [plan_.chunkSize, 2*plan_.chunkSize), etc.
  ///
  /// A partition may span MULTIPLE physical chunks if chunk boundary
  /// doesn't align with partition boundary.
  ///
  /// Example: plan_.chunkSize=66, worker 1 needs rows 63-128
  ///   - startChunk = 63/66 = 0
  ///   - endChunk = 128/66 = 1
  ///   - chunkCount = 2

  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);

  /// Compute start chunk index using physical chunk size
  Value startChunk =
      builder.create<arith::DivUIOp>(loc, elemOffset, plan_.chunkSize);

  /// Compute end position (elemOffset + elemSize - 1)
  Value endPos = builder.create<arith::AddIOp>(loc, elemOffset, elemSize);
  endPos = builder.create<arith::SubIOp>(loc, endPos, one);

  /// Compute end chunk index
  Value endChunk =
      builder.create<arith::DivUIOp>(loc, endPos, plan_.chunkSize);

  /// Clamp endChunk to valid range (numChunks - 1)
  if (!newOuterSizes_.empty()) {
    Value numChunks = newOuterSizes_[0];
    Value maxChunk = builder.create<arith::SubIOp>(loc, numChunks, one);
    endChunk = builder.create<arith::MinUIOp>(loc, endChunk, maxChunk);
  }

  /// Compute chunk count = endChunk - startChunk + 1
  Value chunkCount = builder.create<arith::SubIOp>(loc, endChunk, startChunk);
  chunkCount = builder.create<arith::AddIOp>(loc, chunkCount, one);

  /// Set offsets/sizes for chunked mode
  SmallVector<Value> newOffsets, newSizes;
  newOffsets.push_back(startChunk);
  newSizes.push_back(chunkCount);

  acquire.getOffsetsMutable().assign(newOffsets);
  acquire.getSizesMutable().assign(newSizes);

  /// Preserve original loop bounds hints when available; otherwise fall back
  /// to element-space offsets/sizes for localization.
  SmallVector<Value> offsetHints = originalOffsetHints.empty()
                                       ? SmallVector<Value>{elemOffset}
                                       : originalOffsetHints;
  SmallVector<Value> sizeHints = originalSizeHints.empty()
                                     ? SmallVector<Value>{elemSize}
                                     : originalSizeHints;
  acquire.getOffsetHintsMutable().assign(offsetHints);
  acquire.getSizeHintsMutable().assign(sizeHints);

  /// Use mode-aware rebase for chunked mode EDT users
  /// Pass startChunk directly to avoid recomputation
  rebaseEdtUsers(acquire, builder, startChunk);
}

void DbChunkedRewriter::transformDbRef(DbRefOp ref, DbAllocOp newAlloc,
                                        OpBuilder &builder) {
  Location loc = ref.getLoc();
  Type newElementType = newAlloc.getAllocatedElementType();
  Value newSource = (ref.getSource() == oldAlloc_.getPtr()) ? newAlloc.getPtr()
                                                            : ref.getSource();

  /// Collect load/store users of this db_ref
  SmallVector<Operation *> refUsers(ref.getResult().getUsers().begin(),
                                    ref.getResult().getUsers().end());
  llvm::SetVector<Operation *> opsToRemove;

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
    auto zero = [&]() {
      return builder.create<arith::ConstantIndexOp>(userLoc, 0);
    };

    /// CHUNKED: div/mod for chunk selection
    Value cs = plan_.chunkSize ? plan_.chunkSize : zero();
    if (!loadStoreIndices.empty()) {
      Value idx0 = loadStoreIndices.front();
      newOuter.push_back(builder.create<arith::DivUIOp>(userLoc, idx0, cs));
      newInner.push_back(builder.create<arith::RemUIOp>(userLoc, idx0, cs));
      for (unsigned i = 1; i < loadStoreIndices.size(); ++i)
        newInner.push_back(loadStoreIndices[i]);
    }

    if (newOuter.empty())
      newOuter.push_back(zero());
    if (newInner.empty())
      newInner.push_back(zero());

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
    opsToRemove.insert(user);
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
  for (Operation *op : opsToRemove)
    if (op->use_empty())
      op->erase();
}

bool DbChunkedRewriter::rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                                        Value startChunk) {
  ARTS_DEBUG("DbChunkedRewriter::rebaseEdtUsers");

  auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
  if (!blockArg)
    return false;

  /// Determine element type from users or allocation
  Type targetType;
  for (Operation *user : blockArg.getUsers()) {
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      targetType = ref.getResult().getType();
      break;
    }
  }
  if (!targetType) {
    targetType = blockArg.getType();
    if (auto outer = targetType.dyn_cast<MemRefType>())
      if (auto inner = outer.getElementType().dyn_cast<MemRefType>())
        targetType = inner;
  }
  if (!targetType || !targetType.isa<MemRefType>())
    return false;

  Type derivedType = targetType;
  if (auto dbAlloc = dyn_cast_or_null<DbAllocOp>(
          DatablockUtils::getUnderlyingDbAlloc(blockArg)))
    derivedType = dbAlloc.getAllocatedElementType();

  /// Get element offset from hints
  Value elemOffset;
  if (!acquire.getOffsetHints().empty())
    elemOffset = acquire.getOffsetHints()[0];

  /// Create chunked indexer
  auto indexer = std::make_unique<DbChunkedIndexer>(
      plan_.chunkSize, startChunk, elemOffset, newOuterSizes_.size(),
      newInnerSizes_.size());

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

  for (Operation *op : opsToRemove)
    if (op->use_empty())
      op->erase();

  return rewritten;
}
