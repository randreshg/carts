///==========================================================================///
/// File: DbChunkedIndexer.cpp
///
/// Chunked index localization for datablock partitioning.
///
/// Example transformation (N=100 rows, chunkSize=25, 4 workers):
///
///   BEFORE (single allocation, global indices):
///     %db = arts.db_alloc memref<100xf64>
///     arts.edt {
///       %ref = arts.db_ref %db[0]
///       memref.load %ref[%j]                 // global row j
///     }
///
///   AFTER (chunked, div/mod localization):
///     %db = arts.db_alloc memref<4x25xf64>   // 4 chunks of 25 rows
///     arts.edt(%startChunk, %numChunks) {
///       %chunk = %j / 25                     // physical chunk
///       %relChunk = %chunk - %startChunk     // worker-relative chunk
///       %localRow = %j % 25                  // row within chunk
///       %ref = arts.db_ref %db[%relChunk]
///       memref.load %ref[%localRow]
///     }
///
/// Index localization formulas:
///   chunkIdx = (globalRow / chunkSize) - startChunk
///   localRow = globalRow % chunkSize
///
/// For linearized access (single index into multi-dim memref):
///   1. De-linearize: globalRow = linear / stride, col = linear % stride
///   2. Apply div/mod: localRow = globalRow % chunkSize
///   3. Re-linearize: localLinear = localRow * stride + col
///
///==========================================================================///

#include "arts/Transforms/Datablock/Chunked/DbChunkedIndexer.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

DbChunkedIndexer::DbChunkedIndexer(Value chunkSize, Value startChunk,
                                   Value elemOffset, unsigned outerRank,
                                   unsigned innerRank)
    : DbIndexerBase(outerRank, innerRank), chunkSize_(chunkSize),
      startChunk_(startChunk), elemOffset_(elemOffset) {}

LocalizedIndices DbChunkedIndexer::localize(ArrayRef<Value> globalIndices,
                                            OpBuilder &builder, Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };
  auto one = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 1); };

  ARTS_DEBUG("DbChunkedIndexer::localize with " << globalIndices.size()
                                                << " indices");

  if (globalIndices.empty()) {
    result.dbRefIndices.push_back(zero());
    result.memrefIndices.push_back(zero());
    return result;
  }

  /// Pick the index that tracks the partition offset (defaults to 0)
  unsigned partitionDim = 0;
  if (elemOffset_) {
    for (unsigned i = 0; i < globalIndices.size(); ++i) {
      if (ValueUtils::dependsOn(globalIndices[i], elemOffset_)) {
        partitionDim = i;
        break;
      }
    }
  }
  Value globalRow = globalIndices[partitionDim];
  Value cs = chunkSize_ ? chunkSize_ : one();
  cs = builder.create<arith::MaxUIOp>(loc, cs, one());

  /// dbRefIdx = (globalRow / chunkSize) - startChunk
  Value physChunk = builder.create<arith::DivUIOp>(loc, globalRow, cs);
  Value relChunk = builder.create<arith::SubIOp>(loc, physChunk, startChunk_);
  result.dbRefIndices.push_back(relChunk);

  /// memrefIdx[partitionDim] = globalRow % chunkSize
  Value localRow = builder.create<arith::RemUIOp>(loc, globalRow, cs);
  for (unsigned i = 0; i < globalIndices.size(); ++i) {
    if (i == partitionDim)
      result.memrefIndices.push_back(localRow);
    else
      result.memrefIndices.push_back(globalIndices[i]);
  }

  ARTS_DEBUG("  -> dbRef: " << result.dbRefIndices.size()
                            << ", memref: " << result.memrefIndices.size());
  return result;
}

LocalizedIndices DbChunkedIndexer::localizeLinearized(Value globalLinearIndex,
                                                      Value stride,
                                                      OpBuilder &builder,
                                                      Location loc) {
  LocalizedIndices result;

  ARTS_DEBUG("DbChunkedIndexer::localizeLinearized - using div/mod");

  /// De-linearize: globalLinear = globalRow * stride + col
  Value globalRow =
      builder.create<arith::DivUIOp>(loc, globalLinearIndex, stride);
  Value col = builder.create<arith::RemUIOp>(loc, globalLinearIndex, stride);

  Value cs =
      chunkSize_ ? chunkSize_ : builder.create<arith::ConstantIndexOp>(loc, 1);
  cs = builder.create<arith::MaxUIOp>(
      loc, cs, builder.create<arith::ConstantIndexOp>(loc, 1));

  /// dbRefIdx = (globalRow / chunkSize) - startChunk
  Value physChunk = builder.create<arith::DivUIOp>(loc, globalRow, cs);
  Value relChunk = builder.create<arith::SubIOp>(loc, physChunk, startChunk_);
  result.dbRefIndices.push_back(relChunk);

  /// localRow = globalRow % chunkSize
  Value localRow = builder.create<arith::RemUIOp>(loc, globalRow, cs);

  /// Re-linearize: localLinear = localRow * stride + col
  Value localLinear = builder.create<arith::MulIOp>(loc, localRow, stride);
  localLinear = builder.create<arith::AddIOp>(loc, localLinear, col);
  result.memrefIndices.push_back(localLinear);

  return result;
}

void DbChunkedIndexer::transformOps(ArrayRef<Operation *> ops, Value dbPtr,
                                    Type elementType, ArtsCodegen &AC,
                                    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbChunkedIndexer::transformOps with " << ops.size() << " ops");

  for (Operation *op : ops) {
    OpBuilder::InsertionGuard IG(AC.getBuilder());
    AC.setInsertionPoint(op);
    Location loc = op->getLoc();

    /// Handle DbRefOp specially - rewrite its users
    if (auto dbRef = dyn_cast<DbRefOp>(op)) {
      transformDbRefUsers(dbRef, dbPtr, elementType, AC.getBuilder(),
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

void DbChunkedIndexer::transformDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbChunkedIndexer::transformDbRefUsers");

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

    /// Detect linearized access: single index accessing multi-element memref
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

SmallVector<Value>
DbChunkedIndexer::localizeCoordinates(ArrayRef<Value> globalIndices,
                                      ArrayRef<Value> sliceOffsets,
                                      unsigned numIndexedDims, Type elementType,
                                      OpBuilder &builder, Location loc) {
  SmallVector<Value> result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  ARTS_DEBUG("DbChunkedIndexer::localizeCoordinates with "
             << globalIndices.size() << " indices");

  /// Handle empty case
  if (globalIndices.empty()) {
    result.push_back(zero());
    return result;
  }

  /// CHUNKED MODE: first sliced dimension uses div by chunkSize
  for (unsigned d = 0; d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];

    if (d < numIndexedDims) {
      /// Indexed dimension: local index is always 0
      result.push_back(zero());
    } else if (d == numIndexedDims && chunkSize_) {
      /// First sliced dimension: compute chunk-relative index
      /// dbRefIdx = (globalIdx / chunkSize) - startChunk
      Value physChunk =
          builder.create<arith::DivUIOp>(loc, globalIdx, chunkSize_);
      Value relChunk =
          builder.create<arith::SubIOp>(loc, physChunk, startChunk_);
      result.push_back(relChunk);
      ARTS_DEBUG("  Dim " << d << ": div/mod by chunkSize");
    } else if (d - numIndexedDims < sliceOffsets.size()) {
      /// Other sliced dimensions: subtract offset
      unsigned offsetIdx = d - numIndexedDims;
      Value offset = sliceOffsets[offsetIdx];
      Value local = builder.create<arith::SubIOp>(loc, globalIdx, offset);
      result.push_back(local);
    } else {
      /// Pass through unchanged
      result.push_back(globalIdx);
    }
  }

  return result;
}

void DbChunkedIndexer::transformUsesInParentRegion(
    Operation *alloc, DbAllocOp dbAlloc, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbChunkedIndexer::transformUsesInParentRegion");

  /// Collect users of the original allocation (excluding DbAllocOp and EDT
  /// uses)
  SmallVector<Operation *> users;
  for (auto &use : alloc->getUses()) {
    Operation *user = use.getOwner();
    /// Skip the DbAllocOp itself
    if (user == dbAlloc.getOperation())
      continue;
    /// Skip uses inside EDTs - those are rewritten via acquires separately
    if (user->getParentOfType<EdtOp>())
      continue;
    users.push_back(user);
  }

  if (users.empty()) {
    ARTS_DEBUG("  No users to rewrite in parent region");
    return;
  }

  ARTS_DEBUG("  Rewriting " << users.size()
                            << " main body accesses with chunked indexing");

  /// Use transformOps with the collected users - chunked div/mod transformation
  Type elementMemRefType = dbAlloc.getAllocatedElementType();
  transformOps(users, dbAlloc.getPtr(), elementMemRefType, AC, opsToRemove);
}
