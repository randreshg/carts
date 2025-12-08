///==========================================================================///
/// File: DbTransforms.cpp
/// Unified transformations for datablock operations.
///==========================================================================///

#include "arts/Transforms/DbTransforms.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <cstdlib>

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Coordinate Localization - Global to Local Index Transformation
///
/// DbAcquireOp specifies a view into a datablock:
///   indices: pinned dimensions (single element selection)
///   offsets: slice start position
///   sizes:   slice extent
///
/// Transformation: indexed dims -> 0, sliced dims -> global - offset
///
/// Examples:
///   Fine-grained: indices=[%i], offsets=[0]
///     db_ref[%i] -> db_ref[0]
///
///   Chunking: indices=[], offsets=[%start]
///     db_ref[%start+%j] -> db_ref[%j]
///
///   2D slice: indices=[%row], offsets=[%col_start]
///     db_ref[%row, %col] -> db_ref[0, %col - %col_start]
///===----------------------------------------------------------------------===///

/// Check if value is visible in block (for safe arithmetic generation).
static bool isVisibleIn(Value val, Block *block) {
  if (!val || !block)
    return false;
  Block *defBlock = val.isa<BlockArgument>()
                        ? val.cast<BlockArgument>().getOwner()
                        : val.getDefiningOp()->getBlock();
  for (Block *b = block; b;
       b = b->getParentOp() ? b->getParentOp()->getBlock() : nullptr)
    if (b == defBlock)
      return true;
  return false;
}

/// Get indices from operation.
static ValueRange getIndices(Operation *op) {
  if (auto load = dyn_cast<memref::LoadOp>(op))
    return load.getIndices();
  if (auto store = dyn_cast<memref::StoreOp>(op))
    return store.getIndices();
  if (auto ref = dyn_cast<DbRefOp>(op))
    return ref.getIndices();
  return {};
}

/// Get memref from operation.
static Value getMemref(Operation *op) {
  if (auto load = dyn_cast<memref::LoadOp>(op))
    return load.getMemref();
  if (auto store = dyn_cast<memref::StoreOp>(op))
    return store.getMemref();
  if (auto ref = dyn_cast<DbRefOp>(op))
    return ref.getSource();
  return nullptr;
}

DbTransforms::DbTransforms(MLIRContext *context) : context_(context) {}

/// Check if allocation is coarse-grained (all sizes == 1).
bool DbTransforms::isCoarseGrained(DbAllocOp alloc) {
  return llvm::all_of(alloc.getSizes(), [](Value v) {
    int64_t val;
    return arts::getConstantIndex(v, val) && val == 1;
  });
}

/// Create coordinate map from acquire operation.
ViewCoordinateMap ViewCoordinateMap::fromAcquire(DbAcquireOp acquire) {
  ViewCoordinateMap map;
  map.numIndexedDims = acquire.getIndices().size();
  map.sliceOffsets.assign(acquire.getOffsets().begin(),
                          acquire.getOffsets().end());
  return map;
}

/// Create db_ref + load/store pattern from outer/inner indices.
bool DbTransforms::createDbRefPattern(
    Operation *op, Value dbPtr, Type elementType, ArrayRef<Value> outerIndices,
    ArrayRef<Value> innerIndices, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  if (!isa<memref::LoadOp, memref::StoreOp>(op))
    return false;

  OpBuilder::InsertionGuard IG(builder);
  builder.setInsertionPoint(op);
  Location loc = op->getLoc();

  auto dbRef = builder.create<DbRefOp>(loc, elementType, dbPtr, outerIndices);
  if (auto load = dyn_cast<memref::LoadOp>(op)) {
    auto newLoad = builder.create<memref::LoadOp>(loc, dbRef, innerIndices);
    load.replaceAllUsesWith(newLoad.getResult());
  } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
    builder.create<memref::StoreOp>(loc, store.getValue(), dbRef, innerIndices);
  }

  opsToRemove.insert(op);
  return true;
}

/// Split indices at splitPoint: (outer[0:split], inner[split:end])
std::pair<SmallVector<Value>, SmallVector<Value>>
DbTransforms::splitIndices(ValueRange indices, unsigned splitPoint,
                           OpBuilder &builder, Location loc) {
  SmallVector<Value> outer, inner;
  for (auto indexed : llvm::enumerate(indices)) {
    if (indexed.index() < splitPoint)
      outer.push_back(indexed.value());
    else
      inner.push_back(indexed.value());
  }

  auto addZeroIfEmpty = [&](SmallVector<Value> &vec) {
    if (vec.empty())
      vec.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
  };

  addZeroIfEmpty(outer);
  addZeroIfEmpty(inner);
  return {outer, inner};
}

/// Subtract offsets from indices element-wise.
/// Same SSA value -> constant 0. Cross-scope values unchanged.
SmallVector<Value> DbTransforms::applyOffsets(ArrayRef<Value> indices,
                                              ArrayRef<Value> offsets,
                                              OpBuilder &builder,
                                              Location loc) {
  SmallVector<Value> adjusted;
  Block *insertBlock = builder.getInsertionBlock();

  for (auto indexed : llvm::enumerate(indices)) {
    Value idx = indexed.value();
    if (indexed.index() < offsets.size()) {
      Value offset = offsets[indexed.index()];
      if (idx == offset)
        idx = builder.create<arith::ConstantIndexOp>(loc, 0);
      else if (isVisibleIn(idx, insertBlock) &&
               isVisibleIn(offset, insertBlock))
        idx = builder.create<arith::SubIOp>(loc, idx, offset);
    }
    adjusted.push_back(idx);
  }

  if (adjusted.empty())
    adjusted.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
  return adjusted;
}

/// Transform global indices to local coordinates using ViewCoordinateMap.
/// Indexed dims -> 0, sliced dims -> global - offset.
SmallVector<Value>
DbTransforms::localizeCoordinates(ArrayRef<Value> globalIndices,
                                  const ViewCoordinateMap &map,
                                  OpBuilder &builder, Location loc) {
  SmallVector<Value> localIndices;
  Block *insertBlock = builder.getInsertionBlock();

  for (unsigned d = 0; d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];

    if (d < map.numIndexedDims) {
      localIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
    } else {
      unsigned offsetIdx = d - map.numIndexedDims;
      if (offsetIdx < map.sliceOffsets.size()) {
        Value offset = map.sliceOffsets[offsetIdx];
        if (globalIdx == offset) {
          localIndices.push_back(
              builder.create<arith::ConstantIndexOp>(loc, 0));
        } else if (isVisibleIn(globalIdx, insertBlock) &&
                   isVisibleIn(offset, insertBlock)) {
          localIndices.push_back(
              builder.create<arith::SubIOp>(loc, globalIdx, offset));
        } else {
          localIndices.push_back(globalIdx);
        }
      } else {
        localIndices.push_back(globalIdx);
      }
    }
  }

  if (localIndices.empty())
    localIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));

  return localIndices;
}

/// Rewrite memref load/store to db_ref pattern.
/// Before: load %mem[%i, %j]
/// After:  %ref = db_ref %db[%i]; load %ref[%j]  (outerCount=1)
bool DbTransforms::rewriteAccessWithDbPattern(
    Operation *op, Value dbPtr, Type elementType, unsigned outerCount,
    OpBuilder &builder, llvm::SetVector<Operation *> &opsToRemove) {
  if (!isa<memref::LoadOp, memref::StoreOp>(op))
    return false;

  OpBuilder::InsertionGuard IG(builder);
  builder.setInsertionPoint(op);
  Location loc = op->getLoc();
  auto [outerIndices, innerIndices] =
      splitIndices(getIndices(op), outerCount, builder, loc);

  return createDbRefPattern(op, dbPtr, elementType, outerIndices, innerIndices,
                            builder, opsToRemove);
}

/// Promote dimensions from elementSizes to sizes.
/// promoteCount=1: sizes=[1], elementSizes=[N] -> sizes=[N], elementSizes=[1]
/// trimLeadingOnes: remove leading size=1 values
DbAllocOp DbTransforms::promoteAllocation(DbAllocOp alloc, int promoteCount,
                                          OpBuilder &builder,
                                          bool trimLeadingOnes) {
  if (!alloc || promoteCount == 0)
    return nullptr;

  SmallVector<Value> sizes(alloc.getSizes());
  SmallVector<Value> elemSizes(alloc.getElementSizes());
  unsigned count = std::abs(promoteCount);
  if ((promoteCount > 0 && count > elemSizes.size()) ||
      (promoteCount < 0 && count > sizes.size()))
    return nullptr;

  Location loc = alloc.getLoc();
  OpBuilder::InsertionGuard IG(builder);
  builder.setInsertionPoint(alloc);

  if (promoteCount > 0) {
    for (unsigned i = 0; i < count; ++i) {
      sizes.push_back(elemSizes.front());
      elemSizes.erase(elemSizes.begin());
    }
  } else {
    for (unsigned i = 0; i < count; ++i) {
      elemSizes.insert(elemSizes.begin(), sizes.back());
      sizes.pop_back();
    }
  }

  /// Trim leading size=1 values if requested
  if (trimLeadingOnes) {
    SmallVector<Value> trimmedSizes;
    for (Value size : sizes) {
      int64_t cst;
      if (trimmedSizes.empty() && arts::getConstantIndex(size, cst) && cst == 1)
        continue;
      trimmedSizes.push_back(size);
    }
    sizes = std::move(trimmedSizes);
  }

  auto ensureNonEmpty = [&](SmallVector<Value> &vec) {
    if (vec.empty())
      vec.push_back(builder.create<arith::ConstantIndexOp>(loc, 1));
  };
  ensureNonEmpty(sizes);
  ensureNonEmpty(elemSizes);

  Value address = alloc.getAddress();
  auto newAlloc = builder.create<DbAllocOp>(
      loc, alloc.getMode(), alloc.getRoute(), alloc.getAllocType(),
      alloc.getDbMode(), alloc.getElementType(), address, sizes, elemSizes);

  for (auto attr : alloc->getAttrs()) {
    if (attr.getName().getValue() == "operandSegmentSizes")
      continue;
    newAlloc->setAttr(attr.getName(), attr.getValue());
  }

  return newAlloc;
}

/// Rewrite all uses after allocation promotion.
/// Collects load/store/db_ref users and rewrites them with new index mapping.
LogicalResult DbTransforms::rewriteAllUses(DbAllocOp oldAlloc,
                                           DbAllocOp newAlloc) {
  if (!oldAlloc || !newAlloc)
    return failure();

  bool oldCoarse = isCoarseGrained(oldAlloc);
  bool newCoarse = isCoarseGrained(newAlloc);
  unsigned newOuterRank = newAlloc.getSizes().size();
  unsigned newInnerRank = newAlloc.getElementSizes().size();

  OpBuilder builder(context_);
  llvm::SetVector<Operation *> opsToRemove, loadStoreUsers;
  SmallVector<DbAcquireOp> acquires;
  SmallPtrSet<Value, 8> visited;

  std::function<void(Value)> collectUses = [&](Value value) {
    if (!value || !visited.insert(value).second)
      return;

    for (OpOperand &use : value.getUses()) {
      Operation *user = use.getOwner();
      if (auto load = dyn_cast<memref::LoadOp>(user)) {
        if (load.getMemref() == value)
          loadStoreUsers.insert(load.getOperation());
      } else if (auto store = dyn_cast<memref::StoreOp>(user)) {
        if (store.getMemref() == value)
          loadStoreUsers.insert(store.getOperation());
      } else if (auto ref = dyn_cast<DbRefOp>(user)) {
        if (ref.getSource() == value)
          collectUses(ref.getResult());
      } else if (auto acquire = dyn_cast<DbAcquireOp>(user)) {
        acquires.push_back(acquire);
        auto [edt, blockArg] = arts::getEdtBlockArgumentForAcquire(acquire);
        if (blockArg)
          collectUses(blockArg);
      }
    }
  };

  collectUses(oldAlloc.getPtr());

  auto buildIndexMapping =
      [&](ArrayRef<Value> oldOuter, ArrayRef<Value> oldInner,
          Location loc) -> std::pair<SmallVector<Value>, SmallVector<Value>> {
    SmallVector<Value> newOuter, newInner;

    auto zero = [&]() {
      return builder.create<arith::ConstantIndexOp>(loc, 0);
    };

    if (oldCoarse && !newCoarse) {
      /// Coarse -> Fine: move inner indices to outer
      for (unsigned i = 0; i < newOuterRank; ++i) {
        if (i < oldInner.size())
          newOuter.push_back(oldInner[i]);
        else
          newOuter.push_back(zero());
      }
      for (unsigned i = 0; i < newInnerRank; ++i)
        newInner.push_back(zero());
    } else if (!oldCoarse && newCoarse) {
      /// Fine -> Coarse: move outer indices to inner
      for (unsigned i = 0; i < newOuterRank; ++i)
        newOuter.push_back(zero());
      for (unsigned i = 0; i < newInnerRank; ++i) {
        if (i < oldOuter.size())
          newInner.push_back(oldOuter[i]);
        else if (i - oldOuter.size() < oldInner.size())
          newInner.push_back(oldInner[i - oldOuter.size()]);
        else
          newInner.push_back(zero());
      }
    } else {
      /// Same granularity
      for (unsigned i = 0; i < newOuterRank; ++i) {
        if (i < oldOuter.size())
          newOuter.push_back(oldOuter[i]);
        else
          newOuter.push_back(zero());
      }
      for (unsigned i = 0; i < newInnerRank; ++i) {
        if (i < oldInner.size())
          newInner.push_back(oldInner[i]);
        else
          newInner.push_back(zero());
      }
    }

    if (newOuter.empty())
      newOuter.push_back(zero());
    if (newInner.empty())
      newInner.push_back(zero());
    return {newOuter, newInner};
  };

  Type elementType = newAlloc.getAllocatedElementType().getElementType();
  for (Operation *op : loadStoreUsers) {
    Location loc = op->getLoc();
    OpBuilder::InsertionGuard IG(builder);
    builder.setInsertionPoint(op);

    SmallVector<Value> oldOuter, oldInner;
    Value memref = getMemref(op);
    if (!memref)
      continue;

    DbRefOp sourceRef;
    if (auto ref = memref.getDefiningOp<DbRefOp>()) {
      oldOuter.append(ref.getIndices().begin(), ref.getIndices().end());
      memref = ref.getSource();
      sourceRef = ref;
    }

    ValueRange indices = getIndices(op);
    oldInner.append(indices.begin(), indices.end());

    auto [newOuter, newInner] = buildIndexMapping(oldOuter, oldInner, loc);
    bool created = createDbRefPattern(op, memref, elementType, newOuter,
                                      newInner, builder, opsToRemove);
    if (created && sourceRef && sourceRef.getResult().hasOneUse())
      opsToRemove.insert(sourceRef.getOperation());
  }

  for (DbAcquireOp acquire : acquires) {
    acquire.getSourceGuidMutable().assign(newAlloc.getGuid());
    acquire.getSourcePtrMutable().assign(newAlloc.getPtr());
  }

  oldAlloc.getGuid().replaceAllUsesWith(newAlloc.getGuid());
  oldAlloc.getPtr().replaceAllUsesWith(newAlloc.getPtr());
  if (oldAlloc.use_empty())
    opsToRemove.insert(oldAlloc.getOperation());

  for (auto it = opsToRemove.rbegin(); it != opsToRemove.rend(); ++it) {
    if ((*it)->use_empty())
      (*it)->erase();
  }

  return success();
}

/// Rebase operation indices to acquired view's local coordinates.
bool DbTransforms::rebaseToAcquireView(
    Operation *op, DbAcquireOp acquire, Value dbPtr, Type elementType,
    OpBuilder &builder, llvm::SetVector<Operation *> &opsToRemove) {

  ViewCoordinateMap map = ViewCoordinateMap::fromAcquire(acquire);
  unsigned expectedOuterCount = acquire.getSizes().size();

  OpBuilder::InsertionGuard IG(builder);
  builder.setInsertionPoint(op);
  Location loc = op->getLoc();
  Block *insertBlock = builder.getInsertionBlock();

  if (auto dbRef = dyn_cast<DbRefOp>(op)) {
    SmallVector<Value> refIndices(dbRef.getIndices().begin(),
                                  dbRef.getIndices().end());
    if (!llvm::all_of(refIndices,
                      [&](Value v) { return isVisibleIn(v, insertBlock); }))
      return false;

    auto localIndices = localizeCoordinates(refIndices, map, builder, loc);
    auto newRef = builder.create<DbRefOp>(loc, dbRef.getResult().getType(),
                                          dbPtr ? dbPtr : dbRef.getSource(),
                                          localIndices);
    dbRef.replaceAllUsesWith(newRef.getResult());
    opsToRemove.insert(op);
    return true;
  }

  if (!isa<memref::LoadOp, memref::StoreOp>(op))
    return false;

  ValueRange opIndices = getIndices(op);
  if (!llvm::all_of(opIndices,
                    [&](Value v) { return isVisibleIn(v, insertBlock); }))
    return false;

  Operation *underlyingDb = arts::getUnderlyingDb(getMemref(op));
  if (underlyingDb) {
    bool allZero = llvm::all_of(opIndices, [](Value v) {
      int64_t cst;
      return arts::getConstantIndex(v, cst) && cst == 0;
    });
    if (allZero)
      return false;
  }

  SmallVector<Value> allIndices(opIndices.begin(), opIndices.end());
  auto localIndices = localizeCoordinates(allIndices, map, builder, loc);

  SmallVector<Value> outerIndices, innerIndices;
  for (unsigned i = 0; i < localIndices.size(); ++i) {
    if (i < expectedOuterCount)
      outerIndices.push_back(localIndices[i]);
    else
      innerIndices.push_back(localIndices[i]);
  }

  if (outerIndices.empty())
    outerIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
  if (innerIndices.empty())
    innerIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));

  return createDbRefPattern(op, dbPtr, elementType, outerIndices, innerIndices,
                            builder, opsToRemove);
}

/// Rebase all users of acquired blockArg to local coordinates.
bool DbTransforms::rebaseAllUsersToAcquireView(DbAcquireOp acquire,
                                               OpBuilder &builder) {
  auto [edt, blockArg] = arts::getEdtBlockArgumentForAcquire(acquire);
  if (!blockArg)
    return false;

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

  ViewCoordinateMap map = ViewCoordinateMap::fromAcquire(acquire);

  SmallVector<Operation *> users(blockArg.getUsers().begin(),
                                 blockArg.getUsers().end());
  llvm::SetVector<Operation *> opsToRemove;
  bool rewritten = false;

  for (Operation *user : users) {
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      OpBuilder::InsertionGuard IG(builder);
      builder.setInsertionPoint(ref);
      SmallVector<Value> refIndices(ref.getIndices().begin(),
                                    ref.getIndices().end());
      auto localIndices =
          localizeCoordinates(refIndices, map, builder, ref.getLoc());
      auto newRef = builder.create<DbRefOp>(ref.getLoc(), targetType, blockArg,
                                            localIndices);
      ref.replaceAllUsesWith(newRef.getResult());
      opsToRemove.insert(ref.getOperation());
      rewritten = true;
    } else {
      if (rebaseToAcquireView(user, acquire, blockArg, targetType, builder,
                              opsToRemove))
        rewritten = true;
    }
  }

  for (Operation *op : opsToRemove)
    if (op->use_empty())
      op->erase();

  return rewritten;
}
