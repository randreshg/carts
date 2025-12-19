///==========================================================================///
/// File: DbAllocPromotion.cpp
///
/// Implementation of DbAllocPromotion for allocation promotion transformation.
///==========================================================================///

#include "arts/Transforms/Datablock/DbAllocPromotion.h"
#include "arts/Transforms/Datablock/DbChunkedRewriter.h"
#include "arts/Transforms/Datablock/DbElementWiseRewriter.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/RemovalUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/Debug.h"
#include <functional>

#define DEBUG_TYPE "db_transforms"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Check if 'value' is provably derived from 'base' via addition.
/// Returns delta if value = base + delta, nullptr otherwise.
///
/// Recognized patterns:
///   1. value == base -> delta = 0
///   2. value = arith.addi(base, delta) -> delta
///   3. value = arith.addi(delta, base) -> delta
static Value getOffsetDelta(Value value, Value base, OpBuilder &builder,
                            Location loc) {
  if (value == base)
    return builder.create<arith::ConstantIndexOp>(loc, 0);

  if (auto addOp = value.getDefiningOp<arith::AddIOp>()) {
    if (addOp.getLhs() == base)
      return addOp.getRhs();
    if (addOp.getRhs() == base)
      return addOp.getLhs();
  }
  /// Cannot prove derivation - do NOT subtract
  return nullptr;
}

/// Element-wise localization helper for load/store indices.
static std::pair<SmallVector<Value>, SmallVector<Value>>
localizeElementWiseIndices(ValueRange indices, Value elemOffset, Value stride,
                           OpBuilder &builder, Location loc) {
  SmallVector<Value> outer, inner;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  if (indices.empty()) {
    outer.push_back(zero());
    inner.push_back(zero());
    return {outer, inner};
  }

  if (stride && indices.size() == 1) {
    Value scaledOffset =
        builder.create<arith::MulIOp>(loc, elemOffset, stride);
    Value localLinear =
        builder.create<arith::SubIOp>(loc, indices[0], scaledOffset);
    Value globalRow =
        builder.create<arith::DivUIOp>(loc, indices[0], stride);
    Value dbRefIdx =
        builder.create<arith::SubIOp>(loc, globalRow, elemOffset);
    outer.push_back(dbRefIdx);
    inner.push_back(localLinear);
    return {outer, inner};
  }

  Value globalRow = indices[0];
  Value localRow =
      builder.create<arith::SubIOp>(loc, globalRow, elemOffset);
  outer.push_back(localRow);
  for (unsigned i = 1; i < indices.size(); ++i)
    inner.push_back(indices[i]);
  if (inner.empty())
    inner.push_back(zero());
  return {outer, inner};
}

} // namespace

DbAllocPromotion::DbAllocPromotion(DbAllocOp oldAlloc, ValueRange newOuterSizes,
                                   ValueRange newInnerSizes,
                                   ArrayRef<DbAcquireOp> acquires,
                                   ArrayRef<Value> elementOffsets,
                                   ArrayRef<Value> elementSizes)
    : oldAlloc_(oldAlloc), newOuterSizes_(newOuterSizes),
      newInnerSizes_(newInnerSizes), acquires_(acquires),
      elementOffsets_(elementOffsets), elementSizes_(elementSizes) {
  /// Derive mode from rank comparison and element size:
  /// - Element-wise: newInnerSizes = [1] (single element per DB)
  /// - Chunked: same inner rank, size > 1 (div/mod on first index)
  /// - Otherwise: redistribute indices
  unsigned oldInnerRank = oldAlloc.getElementSizes().size();
  unsigned newInnerRank = newInnerSizes.size();

  /// Check if this is element-wise allocation (1 element per DB)
  /// This takes priority over rank comparison since 1D element-wise
  /// has the same rank but requires different index handling.
  bool isElementWise = false;
  if (newInnerRank == 1) {
    int64_t innerVal;
    if (ValueUtils::getConstantIndex(newInnerSizes[0], innerVal) &&
        innerVal == 1)
      isElementWise = true;
  }

  /// Element-wise uses index redistribution (outer=globalIdx, inner=0)
  /// Chunked uses coordinate localization (outer=idx/chunk, inner=idx%chunk)
  isChunked_ = !isElementWise && (oldInnerRank == newInnerRank);
  chunkSize_ =
      (isChunked_ && !newInnerSizes.empty()) ? newInnerSizes[0] : Value();
}

DbAllocPromotion::AcquireViewMap
DbAllocPromotion::AcquireViewMap::fromAcquire(DbAcquireOp acquire,
                                              bool isEdtBlockArg) {
  AcquireViewMap map;
  map.numIndexedDims = acquire.getIndices().size();
  map.sliceOffsets.assign(acquire.getOffsets().begin(),
                          acquire.getOffsets().end());
  /// Capture element-level offsets from offset_hints (element-space only)
  /// offset_hints[0] = elemOffset for localization
  map.elementOffsets.assign(acquire.getOffsetHints().begin(),
                            acquire.getOffsetHints().end());
  map.isEdtBlockArg = isEdtBlockArg;

  /// NOTE: startChunk and chunkSize are no longer stored in hints.
  /// For chunked mode, they are passed directly to rebaseEdtUsers() from
  /// rewriteAcquire() to avoid recomputation and coordinate space mixing.
  /// The chunkSize is available as a class member (chunkSize_).

  return map;
}

SmallVector<Value>
DbAllocPromotion::localizeIndices(ArrayRef<Value> globalIndices,
                                  const AcquireViewMap &map, OpBuilder &builder,
                                  Location loc) const {
  SmallVector<Value> localIndices;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  for (unsigned d = 0; d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];

    if (d < map.numIndexedDims) {
      /// Indexed dimension: local index is always 0
      localIndices.push_back(zero());
      continue;
    }

    unsigned offsetIdx = d - map.numIndexedDims;
    if (offsetIdx >= map.sliceOffsets.size()) {
      /// No offset for this dimension, keep as-is
      localIndices.push_back(globalIdx);
      continue;
    }

    Value offset = map.sliceOffsets[offsetIdx];

    ///===------------------------------------------------------------------===///
    /// MODE-AWARE LOCALIZATION - DISJOINT Multi-Chunk Model
    ///===------------------------------------------------------------------===///
    if (isChunked_ && map.isEdtBlockArg) {
      /// CHUNKED MODE + EDT BLOCK ARG with DISJOINT model:
      /// Compute relative chunk index from global row index.
      ///
      /// db_ref index = (globalRow / chunkSize) - startChunk
      ///
      /// Example: globalRow=66, chunkSize=66, startChunk=0
      ///   physChunk = 66/66 = 1
      ///   relChunk = 1 - 0 = 1  (second acquired chunk)
      if (map.chunkSize && map.startChunk) {
        Value physChunk =
            builder.create<arith::DivUIOp>(loc, globalIdx, map.chunkSize);
        Value relChunk =
            builder.create<arith::SubIOp>(loc, physChunk, map.startChunk);
        localIndices.push_back(relChunk);
      } else {
        /// Fallback: single chunk, index is 0
        localIndices.push_back(zero());
      }
    } else {
      /// ALL OTHER CASES: Transform global index to local
      /// local = global - offset
      ///
      /// Try to recognize "offset + delta" patterns for cleaner IR
      if (Value delta = getOffsetDelta(globalIdx, offset, builder, loc)) {
        localIndices.push_back(delta);
      } else {
        /// Explicit subtraction for general case
        localIndices.push_back(
            builder.create<arith::SubIOp>(loc, globalIdx, offset));
      }
    }
  }

  if (localIndices.empty())
    localIndices.push_back(zero());

  return localIndices;
}

SmallVector<Value> DbAllocPromotion::localizeElementIndices(
    ArrayRef<Value> globalIndices, const AcquireViewMap &map,
    OpBuilder &builder, Location loc) const {
  SmallVector<Value> localIndices;

  /// For chunked mode inside EDT with DISJOINT model:
  /// - First dimension (chunked): use MOD by chunkSize
  /// - Other dimensions: pass through unchanged
  ///
  /// DISJOINT model localization:
  ///   memref index = globalRow % chunkSize
  ///
  /// Example: globalRow=128, chunkSize=66
  ///   localRow = 128 % 66 = 62
  ///
  if (isChunked_ && map.isEdtBlockArg && map.chunkSize) {
    for (size_t i = 0; i < globalIndices.size(); ++i) {
      Value globalIdx = globalIndices[i];

      if (i == 0) {
        /// First dimension: localRow = globalRow % chunkSize
        localIndices.push_back(
            builder.create<arith::RemUIOp>(loc, globalIdx, map.chunkSize));
      } else {
        localIndices.push_back(globalIdx);
      }
    }
    return localIndices;
  }

  /// Non-chunked or no chunk info: indices unchanged
  localIndices.assign(globalIndices.begin(), globalIndices.end());
  return localIndices;
}

bool DbAllocPromotion::rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                                      Value startChunk) {
  ARTS_DEBUG("DbAllocPromotion::rebaseEdtUsers - isChunked=" << isChunked_);

  auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
  if (!blockArg)
    return false;

  LLVM_DEBUG(llvm::dbgs() << "DbAllocPromotion::rebaseEdtUsers - isChunked="
                          << isChunked_ << "\n");

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

  /// Get element offset from hints for element-wise mode
  Value elemOffset;
  Value elemSize;
  if (!acquire.getOffsetHints().empty())
    elemOffset = acquire.getOffsetHints()[0];
  if (!acquire.getSizeHints().empty())
    elemSize = acquire.getSizeHints()[0];

  /// Create the appropriate rewriter based on mode - fully standalone, no base class
  /// Each rewriter owns its own index localization logic
  /// Pass old element sizes for proper stride computation (static or dynamic)

  SmallVector<Operation *> users(blockArg.getUsers().begin(),
                                 blockArg.getUsers().end());
  llvm::SetVector<Operation *> opsToRemove;
  bool rewritten = false;

  for (Operation *user : users) {
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      /// Use the appropriate rewriter based on mode - fully standalone, no base class
      if (isChunked_) {
        DbChunkedRewriter rewriter(chunkSize_, startChunk, elemOffset,
                                   newOuterSizes_.size(), newInnerSizes_.size());
        rewriter.rewriteDbRefUsers(ref, blockArg, derivedType, builder,
                                   opsToRemove);
      } else {
        DbElementWiseRewriter rewriter(elemOffset, elemSize,
                                       newOuterSizes_.size(), newInnerSizes_.size(),
                                       oldAlloc_.getElementSizes());
        rewriter.rewriteDbRefUsers(ref, blockArg, derivedType, builder,
                                   opsToRemove);
      }
      rewritten = true;
    }
  }

  for (Operation *op : opsToRemove)
    if (op->use_empty())
      op->erase();

  return rewritten;
}

FailureOr<DbAllocOp> DbAllocPromotion::apply(OpBuilder &builder) {
  ARTS_DEBUG("DbAllocPromotion::apply - isChunked="
             << isChunked_ << ", acquires=" << acquires_.size());

  if (!oldAlloc_)
    return failure();

  Location loc = oldAlloc_.getLoc();
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(oldAlloc_);

  /// 1. Create new allocation with the given sizes
  auto newAlloc = builder.create<DbAllocOp>(
      loc, oldAlloc_.getMode(), oldAlloc_.getRoute(), oldAlloc_.getAllocType(),
      oldAlloc_.getDbMode(), oldAlloc_.getElementType(), oldAlloc_.getAddress(),
      newOuterSizes_, newInnerSizes_);

  /// 2. Transfer metadata/attributes from old to new allocation
  transferAttributes(oldAlloc_, newAlloc);

  /// 3. Rewrite all acquires with their partition info
  for (unsigned i = 0; i < acquires_.size(); ++i) {
    rewriteAcquire(acquires_[i], elementOffsets_[i], elementSizes_[i], newAlloc,
                   builder);
  }

  /// 4. Collect and rewrite DbRefs outside of EDTs
  llvm::SetVector<Operation *> opsToRemove;
  SmallVector<DbRefOp> dbRefUsers;
  llvm::SmallPtrSet<Value, 8> visited;

  std::function<void(Value)> collectDbRefs = [&](Value value) {
    if (!value || !visited.insert(value).second)
      return;
    for (OpOperand &use : value.getUses()) {
      Operation *user = use.getOwner();
      if (auto ref = dyn_cast<DbRefOp>(user)) {
        if (ref.getSource() == value) {
          dbRefUsers.push_back(ref);
          collectDbRefs(ref.getResult());
        }
      }
    }
  };
  collectDbRefs(oldAlloc_.getPtr());

  for (DbRefOp ref : dbRefUsers) {
    rewriteDbRef(ref, newAlloc, builder);
    opsToRemove.insert(ref.getOperation());
  }

  /// 5. Replace all uses of old allocation with new
  oldAlloc_.getGuid().replaceAllUsesWith(newAlloc.getGuid());
  oldAlloc_.getPtr().replaceAllUsesWith(newAlloc.getPtr());
  if (oldAlloc_.use_empty())
    opsToRemove.insert(oldAlloc_.getOperation());

  /// 6. Clean up removed operations
  RemovalUtils removalMgr;
  for (Operation *op : opsToRemove)
    removalMgr.markForRemoval(op);
  removalMgr.removeAllMarked();

  return newAlloc;
}

void DbAllocPromotion::rewriteAcquire(DbAcquireOp acquire, Value elemOffset,
                                      Value elemSize, DbAllocOp newAlloc,
                                      OpBuilder &builder) {
  ARTS_DEBUG("DbAllocPromotion::rewriteAcquire - isChunked=" << isChunked_);

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(acquire);
  Location loc = acquire.getLoc();

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

  /// Transform offset/size based on mode
  SmallVector<Value> newOffsets, newSizes;

  if (isChunked_ && chunkSize_) {
    /// CHUNKED: element-space → chunk-space
    ///
    /// DISJOINT MODEL: Physical chunks are disjoint and indexed by chunkSize_.
    /// Chunk layout: chunk 0 -> rows [0, chunkSize_), chunk 1 -> rows
    /// [chunkSize_, 2*chunkSize_), etc.
    ///
    /// For stencil patterns, a partition may span MULTIPLE physical chunks
    /// because the chunk boundary doesn't align with partition boundary.
    ///
    /// Example: chunkSize_=66, worker 1 needs rows 63-128
    ///   - startChunk = 63/66 = 0
    ///   - endChunk = 128/66 = 1
    ///   - chunkCount = 2

    /// Compute start chunk index using physical chunk size
    Value startChunk =
        builder.create<arith::DivUIOp>(loc, elemOffset, chunkSize_);

    /// Compute end position (elemOffset + elemSize - 1)
    Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
    Value endPos = builder.create<arith::AddIOp>(loc, elemOffset, elemSize);
    endPos = builder.create<arith::SubIOp>(loc, endPos, one);

    /// Compute end chunk index
    Value endChunk = builder.create<arith::DivUIOp>(loc, endPos, chunkSize_);

    /// Compute chunk count = endChunk - startChunk + 1
    Value chunkCount = builder.create<arith::SubIOp>(loc, endChunk, startChunk);
    chunkCount = builder.create<arith::AddIOp>(loc, chunkCount, one);

    newOffsets.push_back(startChunk);
    newSizes.push_back(chunkCount);

    acquire.getOffsetsMutable().assign(newOffsets);
    acquire.getSizesMutable().assign(newSizes);

    /// Store element-space offsets in hints for localization
    /// offset_hints = {elemOffset} - pure element-space, no coordinate mixing
    /// size_hints = {elemSize} - pure element-space
    SmallVector<Value> offsetHints = {elemOffset};
    SmallVector<Value> sizeHints = {elemSize};
    acquire.getOffsetHintsMutable().assign(offsetHints);
    acquire.getSizeHintsMutable().assign(sizeHints);

    /// Use mode-aware rebase for chunked mode EDT users
    /// Pass startChunk directly to avoid recomputation
    rebaseEdtUsers(acquire, builder, startChunk);
  } else {
    /// ELEMENT-WISE: already in datablock space, no offset transformation
    newOffsets.push_back(elemOffset);
    newSizes.push_back(elemSize);

    acquire.getOffsetsMutable().assign(newOffsets);
    acquire.getSizesMutable().assign(newSizes);
    acquire.getOffsetHintsMutable().assign(newOffsets);
    acquire.getSizeHintsMutable().assign(newSizes);

    /// CRITICAL FIX: Store the OLD allocation's stride for linearized access!
    /// After element-wise promotion, the new element type has size=1, but
    /// linearized accesses need the ORIGINAL stride to properly scale offsets.
    ///
    /// For element_sizes = [D0, D1, D2, ...], stride = D1 * D2 * ...
    /// This is the product of TRAILING dimensions, NOT all dimensions!
    if (auto staticStride = DatablockUtils::getStaticElementStride(oldAlloc_)) {
      if (*staticStride > 1) {
        acquire->setAttr("element_stride", builder.getIndexAttr(*staticStride));
        LLVM_DEBUG(
            llvm::dbgs()
            << "Element-wise promotion: stored stride=" << *staticStride
            << " (trailing elementSizes product) in acquire attribute\n");
      }
    }

    /// For element-wise promotion, we need to redistribute indices between
    /// db_ref and load/store operations because the rank changed.
    /// Example: old inner rank=2 (memref<?x?xf64>), new inner rank=1
    /// (memref<?xf64>)
    ///   Old: db_ref[0] -> memref<?x?xf64>, load %ref[row,col]
    ///   New: db_ref[row] -> memref<?xf64>, load %ref[col]
    auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
    if (!blockArg)
      return;

    Type newElementType = newAlloc.getAllocatedElementType();

    llvm::SetVector<Operation *> opsToRemove;

    /// Collect all db_ref users
    SmallVector<DbRefOp> dbRefs;
    for (Operation *user : blockArg.getUsers()) {
      if (auto ref = dyn_cast<DbRefOp>(user))
        dbRefs.push_back(ref);
    }

    /// Helper to create zero index
    auto zero = [&](OpBuilder &b, Location l) {
      return b.create<arith::ConstantIndexOp>(l, 0);
    };

    /// Transform each db_ref and its load/store users
    for (DbRefOp ref : dbRefs) {
      /// Collect load/store users of this db_ref
      SmallVector<Operation *> refUsers(ref.getResult().getUsers().begin(),
                                        ref.getResult().getUsers().end());

      bool hasLoadStoreUsers = false;
      for (Operation *user : refUsers) {
        if (!isa<memref::LoadOp, memref::StoreOp>(user))
          continue;

        hasLoadStoreUsers = true;
        OpBuilder::InsertionGuard ig(builder);
        builder.setInsertionPoint(user);
        Location userLoc = user->getLoc();

        /// Get load/store indices
        ValueRange oldInner;
        if (auto load = dyn_cast<memref::LoadOp>(user))
          oldInner = load.getIndices();
        else if (auto store = dyn_cast<memref::StoreOp>(user))
          oldInner = store.getIndices();

        ARTS_DEBUG("rewriteAcquire: load/store user has "
                   << oldInner.size() << " indices (1=linearized)");
        Value stride;
        auto oldElementSizes = oldAlloc_.getElementSizes();
        if (oldInner.size() == 1 && oldElementSizes.size() >= 2) {
          if (auto staticStride =
                  DatablockUtils::getStaticElementStride(oldAlloc_)) {
            if (*staticStride > 1)
              stride =
                  builder.create<arith::ConstantIndexOp>(userLoc, *staticStride);
          } else {
            stride =
                DatablockUtils::getElementStrideValue(builder, userLoc, oldAlloc_);
          }
        }

        auto [outerIdx, innerIdx] = localizeElementWiseIndices(
            oldInner, elemOffset, stride, builder, userLoc);

        /// Create new db_ref with transformed outer indices
        auto newRef = builder.create<DbRefOp>(userLoc, newElementType, blockArg,
                                              outerIdx);

        /// Create new load/store with transformed inner indices
        if (auto load = dyn_cast<memref::LoadOp>(user)) {
          auto newLoad =
              builder.create<memref::LoadOp>(userLoc, newRef, innerIdx);
          load.replaceAllUsesWith(newLoad.getResult());
        } else if (auto store = dyn_cast<memref::StoreOp>(user)) {
          builder.create<memref::StoreOp>(userLoc, store.getValue(), newRef,
                                          innerIdx);
        }
        opsToRemove.insert(user);
      }

      /// For db_refs without load/store users (or after all load/stores are
      /// replaced), we still need to update the db_ref type to match the new
      /// allocation element type. Create a replacement with correct type.
      if (!hasLoadStoreUsers && !ref.getResult().use_empty()) {
        OpBuilder::InsertionGuard ig(builder);
        builder.setInsertionPoint(ref);
        Location refLoc = ref.getLoc();

        /// Keep original indices but update result type
        SmallVector<Value> indices(ref.getIndices().begin(),
                                   ref.getIndices().end());
        if (indices.empty())
          indices.push_back(zero(builder, refLoc));

        auto newRef =
            builder.create<DbRefOp>(refLoc, newElementType, blockArg, indices);
        ref.replaceAllUsesWith(newRef.getResult());
      }

      /// Mark old db_ref for removal
      opsToRemove.insert(ref.getOperation());
    }

    /// Remove old operations
    for (Operation *op : opsToRemove)
      if (op->use_empty())
        op->erase();
  }
}

void DbAllocPromotion::rewriteDbRef(DbRefOp ref, DbAllocOp newAlloc,
                                    OpBuilder &builder) {
  Location loc = ref.getLoc();
  Type newElementType = newAlloc.getAllocatedElementType();
  Value newSource = (ref.getSource() == oldAlloc_.getPtr()) ? newAlloc.getPtr()
                                                            : ref.getSource();

  unsigned newOuterRank = newOuterSizes_.size();
  unsigned newInnerRank = newInnerSizes_.size();

  /// Collect load/store users of this db_ref
  SmallVector<Operation *> refUsers(ref.getResult().getUsers().begin(),
                                    ref.getResult().getUsers().end());
  llvm::SetVector<Operation *> opsToRemove;

  auto zero = [&](OpBuilder &b, Location l) {
    return b.create<arith::ConstantIndexOp>(l, 0);
  };

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
    auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(userLoc, 0); };

    if (isChunked_) {
      Value cs = chunkSize_ ? chunkSize_ : zero();
      if (!loadStoreIndices.empty()) {
        Value idx0 = loadStoreIndices.front();
        newOuter.push_back(builder.create<arith::DivUIOp>(userLoc, idx0, cs));
        newInner.push_back(builder.create<arith::RemUIOp>(userLoc, idx0, cs));
        for (unsigned i = 1; i < loadStoreIndices.size(); ++i)
          newInner.push_back(loadStoreIndices[i]);
      }
    } else {
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
      indices.push_back(zero(builder, loc));
    auto newRef =
        builder.create<DbRefOp>(loc, newElementType, newSource, indices);
    ref.replaceAllUsesWith(newRef.getResult());
  }

  /// Remove old load/store operations
  for (Operation *op : opsToRemove)
    if (op->use_empty())
      op->erase();
}
