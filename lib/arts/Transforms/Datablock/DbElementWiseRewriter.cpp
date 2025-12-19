///==========================================================================///
/// File: DbElementWiseRewriter.cpp
///==========================================================================///

#include "arts/Transforms/Datablock/DbElementWiseRewriter.h"
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

DbElementWiseRewriter::DbElementWiseRewriter(Value elemOffset, Value elemSize,
                                             unsigned outerRank,
                                             unsigned innerRank,
                                             ValueRange oldElementSizes)
    : elemOffset_(elemOffset), elemSize_(elemSize), outerRank_(outerRank),
      innerRank_(innerRank),
      oldElementSizes_(oldElementSizes.begin(), oldElementSizes.end()) {}

DbElementWiseRewriter::LocalizedIndices
DbElementWiseRewriter::splitIndices(ValueRange globalIndices,
                                    OpBuilder &builder, Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  if (globalIndices.empty()) {
    result.dbRefIndices.push_back(zero());
    result.memrefIndices.push_back(zero());
    return result;
  }

  for (unsigned i = 0; i < globalIndices.size(); ++i) {
    if (i < outerRank_)
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

DbElementWiseRewriter::LocalizedIndices
DbElementWiseRewriter::localize(ArrayRef<Value> globalIndices,
                                OpBuilder &builder, Location loc) {
  ARTS_DEBUG("DbElementWiseRewriter::localize with " << globalIndices.size()
                                                     << " indices");
  LLVM_DEBUG(llvm::dbgs() << "DbElementWiseRewriter::localize with "
                          << globalIndices.size() << " indices\n");

  if (outerRank_ == 0)
    return splitIndices(ValueRange(globalIndices), builder, loc);

  if (globalIndices.empty())
    return splitIndices(ValueRange(globalIndices), builder, loc);

  /// Element-wise: split indices by outer/inner rank and subtract elemOffset
  LocalizedIndices result =
      splitIndices(ValueRange(globalIndices), builder, loc);
  if (!result.dbRefIndices.empty()) {
    result.dbRefIndices[0] =
        builder.create<arith::SubIOp>(loc, result.dbRefIndices[0], elemOffset_);
  }

  LLVM_DEBUG(llvm::dbgs() << "  -> dbRef: " << result.dbRefIndices.size()
                          << ", memref: " << result.memrefIndices.size()
                          << "\n");
  return result;
}

DbElementWiseRewriter::LocalizedIndices
DbElementWiseRewriter::localizeLinearized(Value globalLinearIndex, Value stride,
                                          OpBuilder &builder, Location loc) {
  LocalizedIndices result;

  ARTS_DEBUG(
      "DbElementWiseRewriter::localizeLinearized - scaling offset by stride");
  LLVM_DEBUG(llvm::dbgs() << "DbElementWiseRewriter::localizeLinearized\n"
                          << "  THE KEY FIX: scaling offset by stride!\n");

  ///   globalLinear = (elemOffset + localRow) * stride + col
  ///   localLinear  = localRow * stride + col
  ///   Therefore:
  ///   localLinear  = globalLinear - (elemOffset * stride)

  Value scaledOffset = builder.create<arith::MulIOp>(loc, elemOffset_, stride);
  Value localLinear =
      builder.create<arith::SubIOp>(loc, globalLinearIndex, scaledOffset);

  /// For element-wise, dbRef index = row index relative to start
  Value globalRow =
      builder.create<arith::DivUIOp>(loc, globalLinearIndex, stride);
  Value dbRefIdx = builder.create<arith::SubIOp>(loc, globalRow, elemOffset_);

  result.dbRefIndices.push_back(dbRefIdx);
  result.memrefIndices.push_back(localLinear);

  return result;
}

DbElementWiseRewriter::LocalizedIndices
DbElementWiseRewriter::localizeForFineGrained(ValueRange globalIndices,
                                              ValueRange acquireIndices,
                                              ValueRange acquireOffsets,
                                              OpBuilder &builder,
                                              Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  ARTS_DEBUG("DbElementWiseRewriter::localizeForFineGrained with "
             << globalIndices.size() << " indices");
  LLVM_DEBUG(
      llvm::dbgs() << "DbElementWiseRewriter::localizeForFineGrained with "
                   << globalIndices.size() << " indices\n");

  if (globalIndices.empty()) {
    result.dbRefIndices.push_back(zero());
    result.memrefIndices.push_back(zero());
    return result;
  }

  for (unsigned i = 0; i < globalIndices.size(); ++i) {
    Value globalIdx = globalIndices[i];

    if (i < outerRank_) {
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

void DbElementWiseRewriter::rewriteAccessWithDbPattern(
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

void DbElementWiseRewriter::rebaseOps(
    ArrayRef<Operation *> ops, Value dbPtr, Type elementType, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbElementWiseRewriter::rebaseOps with " << ops.size() << " ops");
  LLVM_DEBUG(llvm::dbgs() << "DbElementWiseRewriter::rebaseOps: processing "
                          << ops.size() << " operations\n");

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

    /// Detect linearized access for multi-dimensional elements
    bool isLinearized = false;
    Value stride;
    if (outerRank_ == 1 && indices.size() == 1 &&
        oldElementSizes_.size() >= 2) {
      /// Multi-dimensional old element with single index = linearized access
      stride = DatablockUtils::getStrideValue(AC.getBuilder(), loc,
                                              oldElementSizes_);
      if (stride) {
        if (auto staticStride =
                DatablockUtils::getStaticStride(oldElementSizes_)) {
          if (*staticStride > 1) {
            isLinearized = true;
            LLVM_DEBUG(llvm::dbgs()
                       << "DbElementWiseRewriter: linearized stride="
                       << *staticStride << " from oldElementSizes\n");
          }
        } else {
          /// Dynamic stride - always use it
          isLinearized = true;
          LLVM_DEBUG(llvm::dbgs() << "DbElementWiseRewriter: using dynamic "
                                     "stride from oldElementSizes\n");
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

void DbElementWiseRewriter::rewriteDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  LLVM_DEBUG(llvm::dbgs() << "DbElementWiseRewriter::rewriteDbRefUsers\n");

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
    if (outerRank_ == 1 && elementIndices.size() == 1 &&
        oldElementSizes_.size() >= 2) {
      stride =
          DatablockUtils::getStrideValue(builder, userLoc, oldElementSizes_);
      if (stride) {
        if (auto staticStride =
                DatablockUtils::getStaticStride(oldElementSizes_)) {
          if (*staticStride > 1) {
            isLinearized = true;
            LLVM_DEBUG(llvm::dbgs()
                       << "DbElementWiseRewriter: linearized stride="
                       << *staticStride << " from oldElementSizes\n");
          }
        } else {
          /// Dynamic stride - always use it
          isLinearized = true;
          LLVM_DEBUG(llvm::dbgs() << "DbElementWiseRewriter: using dynamic "
                                     "stride from oldElementSizes\n");
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

SmallVector<Value> DbElementWiseRewriter::localizeCoordinates(
    ArrayRef<Value> globalIndices, ArrayRef<Value> sliceOffsets,
    unsigned numIndexedDims, Type elementType, OpBuilder &builder,
    Location loc) {
  SmallVector<Value> result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  LLVM_DEBUG(llvm::dbgs() << "DbElementWiseRewriter::localizeCoordinates with "
                          << globalIndices.size() << " indices\n");

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
    if (oldElementSizes_.size() >= 2) {
      Value stride =
          DatablockUtils::getStrideValue(builder, loc, oldElementSizes_);
      if (stride) {
        /// localLinear = globalLinear - (offset * stride)
        Value scaledOffset = builder.create<arith::MulIOp>(loc, offset, stride);
        Value localLinear =
            builder.create<arith::SubIOp>(loc, globalLinear, scaledOffset);

        if (auto staticStride =
                DatablockUtils::getStaticStride(oldElementSizes_)) {
          LLVM_DEBUG(llvm::dbgs()
                     << "  LINEARIZED: scaling offset by stride="
                     << *staticStride << " from oldElementSizes\n");
        } else {
          LLVM_DEBUG(
              llvm::dbgs()
              << "  LINEARIZED: using dynamic stride from oldElementSizes\n");
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

void DbElementWiseRewriter::rewriteUsesInParentRegion(
    Operation *alloc, DbAllocOp dbAlloc, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbElementWiseRewriter::rewriteUsesInParentRegion");

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
    rewriteAccessWithDbPattern(op, dbAlloc.getPtr(), elementMemRefType, AC,
                               opsToRemove);
}
