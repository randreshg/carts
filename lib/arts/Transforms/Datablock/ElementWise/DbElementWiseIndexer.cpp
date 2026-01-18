///==========================================================================///
/// File: DbElementWiseIndexer.cpp
///
/// Element-wise index localization for datablock partitioning.
/// Example transformation (N=100 elements, 4 workers):
///
///   BEFORE (single allocation, global indices):
///     %db = arts.db_alloc memref<100xf64>
///     arts.edt {
///       %ref = arts.db_ref %db[%i]           /// global element i
///       memref.load %ref[%j]                 /// element-local index j
///     }
///
///   AFTER (partitioned, local indices):
///     %db = arts.db_alloc memref<100x1xf64>  /// outer=100, inner=1
///     arts.edt(%elemOffset, %elemSize) {
///       %local = %i - %elemOffset            /// localize index
///       %ref = arts.db_ref %db[%local]       /// worker-local element
///       memref.load %ref[%j]                 /// unchanged element-local
///     }
///
/// Index localization formula: local = global - elemOffset
///
/// For linearized access (single index into multi-dim element):
///   localLinear = globalLinear - (elemOffset * stride)
///==========================================================================///

#include "arts/Transforms/Datablock/ElementWise/DbElementWiseIndexer.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

DbElementWiseIndexer::DbElementWiseIndexer(Value elemOffset, Value elemSize,
                                           unsigned outerRank,
                                           unsigned innerRank,
                                           ValueRange oldElementSizes)
    : DbIndexerBase(outerRank, innerRank), elemOffset(elemOffset),
      elemSize(elemSize),
      oldElementSizes(oldElementSizes.begin(), oldElementSizes.end()) {}

LocalizedIndices DbElementWiseIndexer::splitIndices(ValueRange globalIndices,
                                                    OpBuilder &builder,
                                                    Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  if (globalIndices.empty()) {
    result.dbRefIndices.push_back(zero());
    result.memrefIndices.push_back(zero());
    return result;
  }

  for (unsigned i = 0; i < globalIndices.size(); ++i) {
    if (i < outerRank)
      result.dbRefIndices.push_back(globalIndices[i]);
    else
      result.memrefIndices.push_back(globalIndices[i]);
  }

  if (result.dbRefIndices.empty())
    result.dbRefIndices.push_back(zero());
  if (result.memrefIndices.empty())
    result.memrefIndices.push_back(zero());

  return result;
}

LocalizedIndices DbElementWiseIndexer::localize(ArrayRef<Value> globalIndices,
                                                OpBuilder &builder,
                                                Location loc) {
  ARTS_DEBUG("DbElementWiseIndexer::localize with " << globalIndices.size()
                                                    << " indices");

  if (outerRank == 0)
    return splitIndices(ValueRange(globalIndices), builder, loc);

  if (globalIndices.empty())
    return splitIndices(ValueRange(globalIndices), builder, loc);

  /// Element-wise: split indices by outer/inner rank and subtract elemOffset
  LocalizedIndices result =
      splitIndices(ValueRange(globalIndices), builder, loc);
  if (!result.dbRefIndices.empty()) {
    result.dbRefIndices[0] =
        builder.create<arith::SubIOp>(loc, result.dbRefIndices[0], elemOffset);
  }

  ARTS_DEBUG("  -> dbRef: " << result.dbRefIndices.size()
                            << ", memref: " << result.memrefIndices.size());
  return result;
}

LocalizedIndices
DbElementWiseIndexer::localizeLinearized(Value globalLinearIndex, Value stride,
                                         OpBuilder &builder, Location loc) {
  LocalizedIndices result;

  ARTS_DEBUG(
      "DbElementWiseIndexer::localizeLinearized - scaling offset by stride");

  ///   globalLinear = (elemOffset + localRow) * stride + col
  ///   localLinear  = localRow * stride + col
  ///   Therefore:
  ///   localLinear  = globalLinear - (elemOffset * stride)

  Value scaledOffset = builder.create<arith::MulIOp>(loc, elemOffset, stride);
  Value localLinear =
      builder.create<arith::SubIOp>(loc, globalLinearIndex, scaledOffset);

  /// For element-wise, dbRef index = row index relative to start
  Value globalRow =
      builder.create<arith::DivUIOp>(loc, globalLinearIndex, stride);
  Value dbRefIdx = builder.create<arith::SubIOp>(loc, globalRow, elemOffset);

  result.dbRefIndices.push_back(dbRefIdx);
  result.memrefIndices.push_back(localLinear);

  return result;
}

LocalizedIndices DbElementWiseIndexer::localizeForFineGrained(
    ValueRange globalIndices, ValueRange acquireIndices,
    ValueRange acquireOffsets, OpBuilder &builder, Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  ARTS_DEBUG("DbElementWiseIndexer::localizeForFineGrained with "
             << globalIndices.size() << " indices");

  if (globalIndices.empty()) {
    result.dbRefIndices.push_back(zero());
    result.memrefIndices.push_back(zero());
    return result;
  }

  for (unsigned i = 0; i < globalIndices.size(); ++i) {
    Value globalIdx = globalIndices[i];

    if (i < outerRank) {
      Value localIdx = globalIdx;
      if (i < acquireIndices.size()) {
        localIdx =
            builder.create<arith::SubIOp>(loc, globalIdx, acquireIndices[i]);
      } else if (!acquireOffsets.empty() && i == acquireIndices.size()) {
        localIdx =
            builder.create<arith::SubIOp>(loc, globalIdx, acquireOffsets[0]);
      }
      result.dbRefIndices.push_back(localIdx);
    } else {
      result.memrefIndices.push_back(globalIdx);
    }
  }

  if (result.dbRefIndices.empty())
    result.dbRefIndices.push_back(zero());
  if (result.memrefIndices.empty())
    result.memrefIndices.push_back(zero());

  return result;
}

void DbElementWiseIndexer::transformAccess(
    Operation *op, Value dbPtr, Type elementType, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  if (!isa<memref::LoadOp, memref::StoreOp>(op))
    return;

  OpBuilder::InsertionGuard ig(AC.getBuilder());
  AC.setInsertionPoint(op);
  Location loc = op->getLoc();

  ValueRange indices = getIndicesFromOp(op);
  LocalizedIndices localized = splitIndices(indices, AC.getBuilder(), loc);

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

void DbElementWiseIndexer::transformOps(
    ArrayRef<Operation *> ops, Value dbPtr, Type elementType, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbElementWiseIndexer::transformOps with " << ops.size()
                                                        << " ops");

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

    /// Detect linearized access for multi-dimensional elements
    bool isLinearized = false;
    Value stride;
    if (outerRank == 1 && indices.size() == 1 && oldElementSizes.size() >= 2) {
      /// Multi-dimensional old element with single index = linearized access
      stride =
          DatablockUtils::getStrideValue(AC.getBuilder(), loc, oldElementSizes);
      if (stride) {
        if (auto staticStride =
                DatablockUtils::getStaticStride(oldElementSizes)) {
          if (*staticStride > 1) {
            isLinearized = true;
            ARTS_DEBUG("transformOps: linearized stride=" << *staticStride);
          }
        } else {
          /// Dynamic stride - always use it
          isLinearized = true;
          ARTS_DEBUG("transformOps: using dynamic stride from oldElementSizes");
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

void DbElementWiseIndexer::transformDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbElementWiseIndexer::transformDbRefUsers");

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
    if (outerRank == 1 && elementIndices.size() == 1 &&
        oldElementSizes.size() >= 2) {
      stride =
          DatablockUtils::getStrideValue(builder, userLoc, oldElementSizes);
      if (stride) {
        if (auto staticStride =
                DatablockUtils::getStaticStride(oldElementSizes)) {
          if (*staticStride > 1) {
            isLinearized = true;
            ARTS_DEBUG(
                "transformDbRefUsers: linearized stride=" << *staticStride);
          }
        } else {
          /// Dynamic stride - always use it
          isLinearized = true;
          ARTS_DEBUG(
              "transformDbRefUsers: using dynamic stride from oldElementSizes");
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

SmallVector<Value> DbElementWiseIndexer::localizeCoordinates(
    ArrayRef<Value> globalIndices, ArrayRef<Value> sliceOffsets,
    unsigned numIndexedDims, Type elementType, OpBuilder &builder,
    Location loc) {
  SmallVector<Value> result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  ARTS_DEBUG("DbElementWiseIndexer::localizeCoordinates with "
             << globalIndices.size() << " indices");

  /// Handle empty case
  if (globalIndices.empty()) {
    result.push_back(zero());
    return result;
  }

  /// Detect linearized access
  if (globalIndices.size() == 1 && !sliceOffsets.empty()) {
    Value globalLinear = globalIndices[0];
    Value offset = sliceOffsets[0];

    /// Check if this needs stride scaling (old element has multiple dimensions)
    if (oldElementSizes.size() >= 2) {
      Value stride =
          DatablockUtils::getStrideValue(builder, loc, oldElementSizes);
      if (stride) {
        /// localLinear = globalLinear - (offset * stride)
        Value scaledOffset = builder.create<arith::MulIOp>(loc, offset, stride);
        Value localLinear =
            builder.create<arith::SubIOp>(loc, globalLinear, scaledOffset);

        if (auto staticStride =
                DatablockUtils::getStaticStride(oldElementSizes)) {
          ARTS_DEBUG(
              "  localizeCoordinates: LINEARIZED, stride=" << *staticStride);
        } else {
          ARTS_DEBUG("  localizeCoordinates: LINEARIZED, dynamic stride");
        }
        result.push_back(localLinear);
        return result;
      }
    }

    /// Non-linearized single index: simple subtraction
    Value local = builder.create<arith::SubIOp>(loc, globalLinear, offset);
    result.push_back(local);
    return result;
  }

  /// Multi-dimensional: subtract offset from first sliced dimension only
  for (unsigned d = 0; d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];

    if (d < numIndexedDims) {
      /// Indexed dimension: local index is always 0
      result.push_back(zero());
    } else if (d == numIndexedDims && !sliceOffsets.empty()) {
      /// First sliced dimension: subtract element offset
      Value offset = sliceOffsets[0];
      Value local = builder.create<arith::SubIOp>(loc, globalIdx, offset);
      result.push_back(local);
    } else {
      /// Pass through unchanged
      result.push_back(globalIdx);
    }
  }

  return result;
}

void DbElementWiseIndexer::transformUsesInParentRegion(
    Operation *alloc, DbAllocOp dbAlloc, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbElementWiseIndexer::transformUsesInParentRegion");

  /// Collect users of the original allocation
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

  ARTS_DEBUG("  Rewriting "
             << users.size()
             << " main body accesses with element-wise indexing");

  Type elementMemRefType = dbAlloc.getAllocatedElementType();

  for (Operation *op : users)
    transformAccess(op, dbAlloc.getPtr(), elementMemRefType, AC, opsToRemove);
}
