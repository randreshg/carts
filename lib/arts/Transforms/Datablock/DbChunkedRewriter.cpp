///==========================================================================///
/// File: DbChunkedRewriter.cpp
///
/// Implementation of DbChunkedRewriter for chunked mode index localization.
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

DbChunkedRewriter::DbChunkedRewriter(Value chunkSize, Value startChunk,
                                 Value elemOffset, unsigned outerRank,
                                 unsigned innerRank)
    : chunkSize_(chunkSize), startChunk_(startChunk), elemOffset_(elemOffset),
      outerRank_(outerRank), innerRank_(innerRank) {}

DbRewriter::LocalizedIndices
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

DbRewriter::LocalizedIndices
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
    // CRITICAL FIX: For linearization stride, we need product of TRAILING dims
    // (not total elements). For [D0, D1, ...], stride = D1 * D2 * ...
    bool isLinearized = false;
    Value stride;
    if (elementIndices.size() == 1) {
      if (auto memrefType = newElementType.dyn_cast<MemRefType>()) {
        if (memrefType.getRank() >= 2) {
          // Multi-dimensional memref with single index = linearized access
          if (auto staticStride =
                  DatablockUtils::DatablockUtils::getStaticStride(memrefType)) {
            if (*staticStride > 1) {
              isLinearized = true;
              stride = builder.create<arith::ConstantIndexOp>(userLoc,
                                                              *staticStride);
            }
          }
          // Note: Dynamic dimensions not handled here - caller should
          // provide stride via element_stride attribute on the acquire
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
DbChunkedRewriter::localizeCoordinates(ArrayRef<Value> globalIndices,
                                     ArrayRef<Value> sliceOffsets,
                                     unsigned numIndexedDims, Type elementType,
                                     OpBuilder &builder, Location loc) {
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

bool DbChunkedRewriter::rebaseToAcquireViewImpl(
    Operation *op, DbAcquireOp acquire, Value dbPtr, Type elementType,
    ArtsCodegen &AC, llvm::SetVector<Operation *> &opsToRemove) {
  // Delegate to the existing standalone function for now
  // The chunked mode handling is already correct in the standalone
  // as it uses div/mod which doesn't need stride scaling
  return DbRewriter::rebaseToAcquireView(op, acquire, dbPtr, elementType, AC,
                                         opsToRemove);
}

bool DbChunkedRewriter::rebaseAllUsersToAcquireViewImpl(DbAcquireOp acquire,
                                                      ArtsCodegen &AC) {
  // Delegate to existing function - chunked mode is already handled correctly
  return DbRewriter::rebaseAllUsersToAcquireView(acquire, AC);
}
