///==========================================================================///
/// File: DbChunkedRewriter.cpp
///==========================================================================///

#include "arts/Transforms/Datablock/DbChunkedRewriter.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "db_transforms"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Helper to get indices from load/store operations
static ValueRange getIndicesFromOp(Operation *op) {
  if (auto load = dyn_cast<memref::LoadOp>(op))
    return load.getIndices();
  if (auto store = dyn_cast<memref::StoreOp>(op))
    return store.getIndices();
  if (auto ref = dyn_cast<DbRefOp>(op))
    return ref.getIndices();
  return {};
}

} // namespace

DbChunkedRewriter::DbChunkedRewriter(Value chunkSize, Value startChunk,
                                     Value elemOffset, unsigned outerRank,
                                     unsigned innerRank)
    : chunkSize_(chunkSize), startChunk_(startChunk), elemOffset_(elemOffset),
      outerRank_(outerRank), innerRank_(innerRank) {}

DbChunkedRewriter::LocalizedIndices
DbChunkedRewriter::localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  ARTS_DEBUG("DbChunkedRewriter::localize with " << globalIndices.size()
                                                 << " indices");
  LLVM_DEBUG(llvm::dbgs() << "DbChunkedRewriter::localize with "
                          << globalIndices.size() << " indices\n");

  if (globalIndices.empty()) {
    result.dbRefIndices.push_back(zero());
    result.memrefIndices.push_back(zero());
    return result;
  }

  // CHUNKED: dim 0 gets div/mod, rest pass through
  Value globalRow = globalIndices[0];

  // dbRefIdx = (globalRow / chunkSize) - startChunk
  Value physChunk = builder.create<arith::DivUIOp>(loc, globalRow, chunkSize_);
  Value relChunk = builder.create<arith::SubIOp>(loc, physChunk, startChunk_);
  result.dbRefIndices.push_back(relChunk);

  // memrefIdx[0] = globalRow % chunkSize
  Value localRow = builder.create<arith::RemUIOp>(loc, globalRow, chunkSize_);
  result.memrefIndices.push_back(localRow);

  // Remaining dimensions pass through unchanged
  for (unsigned i = 1; i < globalIndices.size(); ++i)
    result.memrefIndices.push_back(globalIndices[i]);

  LLVM_DEBUG(llvm::dbgs() << "  -> dbRef: " << result.dbRefIndices.size()
                          << ", memref: " << result.memrefIndices.size()
                          << "\n");
  return result;
}

DbChunkedRewriter::LocalizedIndices
DbChunkedRewriter::localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder, Location loc) {
  LocalizedIndices result;

  ARTS_DEBUG("DbChunkedRewriter::localizeLinearized - using div/mod");
  LLVM_DEBUG(llvm::dbgs() << "DbChunkedRewriter::localizeLinearized\n");

  // De-linearize: globalLinear = globalRow * stride + col
  Value globalRow =
      builder.create<arith::DivUIOp>(loc, globalLinearIndex, stride);
  Value col = builder.create<arith::RemUIOp>(loc, globalLinearIndex, stride);

  // dbRefIdx = (globalRow / chunkSize) - startChunk
  Value physChunk = builder.create<arith::DivUIOp>(loc, globalRow, chunkSize_);
  Value relChunk = builder.create<arith::SubIOp>(loc, physChunk, startChunk_);
  result.dbRefIndices.push_back(relChunk);

  // localRow = globalRow % chunkSize
  Value localRow = builder.create<arith::RemUIOp>(loc, globalRow, chunkSize_);

  // Re-linearize: localLinear = localRow * stride + col
  Value localLinear = builder.create<arith::MulIOp>(loc, localRow, stride);
  localLinear = builder.create<arith::AddIOp>(loc, localLinear, col);
  result.memrefIndices.push_back(localLinear);

  return result;
}

void DbChunkedRewriter::rebaseOps(ArrayRef<Operation *> ops, Value dbPtr,
                                  Type elementType, ArtsCodegen &AC,
                                  llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbChunkedRewriter::rebaseOps with " << ops.size() << " ops");
  LLVM_DEBUG(llvm::dbgs() << "DbChunkedRewriter::rebaseOps: processing "
                          << ops.size() << " operations\n");

  for (Operation *op : ops) {
    OpBuilder::InsertionGuard IG(AC.getBuilder());
    AC.setInsertionPoint(op);
    Location loc = op->getLoc();

    // Handle DbRefOp specially - rewrite its users
    if (auto dbRef = dyn_cast<DbRefOp>(op)) {
      rewriteDbRefUsers(dbRef, dbPtr, elementType, AC.getBuilder(),
                        opsToRemove);
      continue;
    }

    // Handle load/store operations
    if (!isa<memref::LoadOp, memref::StoreOp>(op))
      continue;

    ValueRange indices = getIndicesFromOp(op);
    if (indices.empty())
      continue;

    // Detect linearized access for multi-dimensional memrefs
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

    // Create db_ref + load/store pattern
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

void DbChunkedRewriter::rewriteDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  LLVM_DEBUG(llvm::dbgs() << "DbChunkedRewriter::rewriteDbRefUsers\n");

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

    // Detect linearized access: single index accessing multi-element memref
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

SmallVector<Value> DbChunkedRewriter::localizeCoordinates(
    ArrayRef<Value> globalIndices, ArrayRef<Value> sliceOffsets,
    unsigned numIndexedDims, Type elementType, OpBuilder &builder,
    Location loc) {
  SmallVector<Value> result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  LLVM_DEBUG(llvm::dbgs() << "DbChunkedRewriter::localizeCoordinates with "
                          << globalIndices.size() << " indices\n");

  // Handle empty case
  if (globalIndices.empty()) {
    result.push_back(zero());
    return result;
  }

  // CHUNKED MODE: first sliced dimension uses div by chunkSize
  for (unsigned d = 0; d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];

    if (d < numIndexedDims) {
      // Indexed dimension: local index is always 0
      result.push_back(zero());
    } else if (d == numIndexedDims && chunkSize_) {
      // First sliced dimension: compute chunk-relative index
      // dbRefIdx = (globalIdx / chunkSize) - startChunk
      Value physChunk =
          builder.create<arith::DivUIOp>(loc, globalIdx, chunkSize_);
      Value relChunk =
          builder.create<arith::SubIOp>(loc, physChunk, startChunk_);
      result.push_back(relChunk);
      LLVM_DEBUG(llvm::dbgs() << "  Dim " << d << ": div/mod by chunkSize\n");
    } else if (d - numIndexedDims < sliceOffsets.size()) {
      // Other sliced dimensions: subtract offset
      unsigned offsetIdx = d - numIndexedDims;
      Value offset = sliceOffsets[offsetIdx];
      Value local = builder.create<arith::SubIOp>(loc, globalIdx, offset);
      result.push_back(local);
    } else {
      // Pass through unchanged
      result.push_back(globalIdx);
    }
  }

  return result;
}

void DbChunkedRewriter::rewriteUsesInParentRegion(
    Operation *alloc, DbAllocOp dbAlloc, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbChunkedRewriter::rewriteUsesInParentRegion");

  // Collect users of the original allocation (excluding DbAllocOp and EDT uses)
  SmallVector<Operation *> users;
  for (auto &use : alloc->getUses()) {
    Operation *user = use.getOwner();
    // Skip the DbAllocOp itself
    if (user == dbAlloc.getOperation())
      continue;
    // Skip uses inside EDTs - those are rewritten via acquires separately
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

  // Use rebaseOps with the collected users - chunked div/mod transformation
  Type elementMemRefType = dbAlloc.getAllocatedElementType();
  rebaseOps(users, dbAlloc.getPtr(), elementMemRefType, AC, opsToRemove);
}
