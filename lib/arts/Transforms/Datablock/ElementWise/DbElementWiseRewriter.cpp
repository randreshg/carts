///==========================================================================///
/// File: DbElementWiseRewriter.cpp
///
/// Element-wise rewriter: each element is a separate datablock.
///
/// Example transformation (N=100 elements, 4 workers):
///
///   BEFORE:
///     %db = arts.db_alloc memref<100xf64>
///     arts.db_acquire %db [0] [100]          /// worker acquires all
///     %ref = arts.db_ref %db[%i]
///     memref.load %ref[%j]                   /// global index j
///
///   AFTER:
///     %db = arts.db_alloc memref<100x1xf64>  /// outer=100, inner=1
///     arts.db_acquire %db [%offset] [%size]  /// worker 0: [0][25]
///     %ref = arts.db_ref %db[%i]
///     memref.load %ref[0]                    /// local index (always 0)
///
/// Index localization: local = global - elemOffset
///==========================================================================///

#include "arts/Transforms/Datablock/ElementWise/DbElementWiseRewriter.h"
#include "arts/Transforms/Datablock/ElementWise/DbElementWiseIndexer.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/RemovalUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

void DbElementWiseRewriter::transformAcquire(const DbRewriteAcquire &info,
                                             DbAllocOp newAlloc,
                                             OpBuilder &builder) {
  DbAcquireOp acquire = info.acquire;

  ARTS_DEBUG("DbElementWiseRewriter::transformAcquire (dims="
             << info.elemOffsets.size() << ")");

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(acquire);

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

  /// ELEMENT-WISE: Update indices to the element offsets for fine_grained mode.
  /// Supports multi-dimensional fine-grained (e.g., A[i][j] -> indices [i, j]).
  /// Keep offsets at 0 and sizes at 1 to avoid double-indexing during lowering.
  Location loc = acquire.getLoc();
  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);

  /// Set indices to element offsets (for multi-dim fine_grained indexing)
  SmallVector<Value> newIndices(info.elemOffsets.begin(),
                                info.elemOffsets.end());
  acquire.getIndicesMutable().assign(newIndices);

  /// Set offsets to 0 and sizes to 1 for each dimension
  SmallVector<Value> newOffsets, newSizes;
  for (size_t i = 0; i < info.elemOffsets.size(); ++i) {
    newOffsets.push_back(zero);
    newSizes.push_back(one);
  }
  acquire.getOffsetsMutable().assign(newOffsets);
  acquire.getSizesMutable().assign(newSizes);

  /// Clear partition hints - they've been consumed by partitioning.
  /// The runtime indices now carry the element coordinates.
  acquire.getPartitionIndicesMutable().clear();
  acquire.getPartitionOffsetsMutable().clear();
  acquire.getPartitionSizesMutable().clear();
  if (acquire.getPartitionMode().has_value())
    acquire.removePartitionModeAttr();

  /// Store original stride for linearized access (stride = D1 * D2 * ...)
  if (auto staticStride = DatablockUtils::getStaticElementStride(oldAlloc)) {
    if (*staticStride > 1) {
      acquire->setAttr("element_stride", builder.getIndexAttr(*staticStride));
      ARTS_DEBUG("Element-wise rewrite: stored stride=" << *staticStride);
    }
  }

  /// Rebase EDT users via the element-wise indexer
  rebaseEdtUsers(acquire, builder);
}

void DbElementWiseRewriter::transformDbRef(DbRefOp ref, DbAllocOp newAlloc,
                                           OpBuilder &builder) {
  Location loc = ref.getLoc();
  Type newElementType = newAlloc.getAllocatedElementType();
  Value newSource = (ref.getSource() == oldAlloc.getPtr()) ? newAlloc.getPtr()
                                                           : ref.getSource();

  unsigned newOuterRank = newOuterSizes.size();
  unsigned newInnerRank = newInnerSizes.size();

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
    auto zero = [&]() {
      return builder.create<arith::ConstantIndexOp>(userLoc, 0);
    };

    /// ElementWise: direct mapping of indices
    for (unsigned i = 0; i < newOuterRank; ++i) {
      if (i < loadStoreIndices.size())
        newOuter.push_back(loadStoreIndices[i]);
      else
        newOuter.push_back(zero());
    }
    for (unsigned i = 0; i < newInnerRank; ++i) {
      unsigned src = newOuterRank + i;
      if (src < loadStoreIndices.size())
        newInner.push_back(loadStoreIndices[src]);
      else
        newInner.push_back(zero());
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

bool DbElementWiseRewriter::rebaseEdtUsers(DbAcquireOp acquire,
                                           OpBuilder &builder,
                                           Value /*startBlock*/,
                                           bool /*isSingleChunk*/) {
  ARTS_DEBUG("DbElementWiseRewriter::rebaseEdtUsers");

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

  /// Get element offsets from indices (for fine_grained mode).
  /// The offsets/sizes are now 0/1 after transformAcquire, so we get
  /// the actual element indices from indices (supports multi-dimensional).
  /// For 2D fine-grained (e.g., A[i][j]), indices will be [%i, %j].
  SmallVector<Value> elemOffsets(acquire.getIndices().begin(),
                                 acquire.getIndices().end());

  /// Create element-wise indexer with multi-dimensional offsets
  auto indexer = std::make_unique<DbElementWiseIndexer>(
      elemOffsets, newOuterSizes.size(), newInnerSizes.size(),
      oldAlloc.getElementSizes());

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
