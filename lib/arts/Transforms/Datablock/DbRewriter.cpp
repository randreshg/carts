///==========================================================================///
/// File: DbRewriter.cpp
///
/// Implementation of DbRewriter for allocation rewrite transformation.
///==========================================================================///

#include "arts/Transforms/Datablock/DbRewriter.h"
#include "arts/Transforms/Datablock/DbChunkedRewriter.h"
#include "arts/Transforms/Datablock/DbElementWiseRewriter.h"
#include "arts/Transforms/Datablock/DbStencilRewriter.h"
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
#include <cassert>
#include <functional>

#define DEBUG_TYPE "db_transforms"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

namespace {
} // namespace

DbRewriter::DbRewriter(DbAllocOp oldAlloc, ValueRange newOuterSizes,
                       ValueRange newInnerSizes,
                       ArrayRef<DbRewriteAcquire> acquires,
                       const DbRewritePlan &plan)
    : oldAlloc_(oldAlloc), newOuterSizes_(newOuterSizes),
      newInnerSizes_(newInnerSizes), acquires_(acquires),
      plan_(plan) {
}

bool DbRewriter::rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                                      Value startChunk) {
  ARTS_DEBUG("DbRewriter::rebaseEdtUsers - mode="
             << (plan_.mode == RewriterMode::Stencil    ? "Stencil"
                 : plan_.mode == RewriterMode::Chunked  ? "Chunked"
                                                       : "ElementWise"));

  auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
  if (!blockArg)
    return false;

  LLVM_DEBUG(llvm::dbgs() << "DbRewriter::rebaseEdtUsers - mode="
                          << static_cast<int>(plan_.mode) << "\n");

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
      /// Create the appropriate rewriter based on mode
      auto rewriter = createRewriter(
          plan_, plan_.chunkSize, startChunk, elemOffset, elemSize,
          newOuterSizes_.size(), newInnerSizes_.size(),
          oldAlloc_.getElementSizes(), builder, ref.getLoc());

      rewriter->rewriteDbRefUsers(ref, blockArg, derivedType, builder,
                                  opsToRemove);
      rewritten = true;
    }
  }

  for (Operation *op : opsToRemove)
    if (op->use_empty())
      op->erase();

  return rewritten;
}

FailureOr<DbAllocOp> DbRewriter::apply(OpBuilder &builder) {
  ARTS_DEBUG("DbRewriter::apply - mode="
             << (plan_.mode == RewriterMode::Stencil    ? "Stencil"
                 : plan_.mode == RewriterMode::Chunked  ? "Chunked"
                                                       : "ElementWise")
             << ", acquires=" << acquires_.size());

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
  for (const auto &info : acquires_)
    rewriteAcquire(info, newAlloc, builder);

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

void DbRewriter::rewriteAcquire(const DbRewriteAcquire &info, DbAllocOp newAlloc,
                                OpBuilder &builder) {
  DbAcquireOp acquire = info.acquire;
  Value elemOffset = info.elemOffset;
  Value elemSize = info.elemSize;

  ARTS_DEBUG("DbRewriter::rewriteAcquire - mode="
             << (plan_.mode == RewriterMode::Stencil    ? "Stencil"
                 : plan_.mode == RewriterMode::Chunked  ? "Chunked"
                                                       : "ElementWise"));

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

  /// Transform offset/size based on mode
  SmallVector<Value> newOffsets, newSizes;

  /// Both Chunked and Stencil modes use chunk-based allocation
  bool usesChunks = (plan_.mode == RewriterMode::Chunked ||
                     plan_.mode == RewriterMode::Stencil) && plan_.chunkSize;

  if (usesChunks) {
    /// CHUNKED/STENCIL: element-space → chunk-space
    Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
    Value startChunk;
    Value chunkCount;

    if (plan_.mode == RewriterMode::Stencil && plan_.stencilInfo &&
        plan_.stencilInfo->hasHalo()) {
      /// STENCIL MODE: Single-chunk model
      /// Each worker acquires exactly ONE stencil-local chunk.
      /// Chunk index is computed using BASE coordinate space, not extended.
      ///
      /// plan_.chunkSize is EXTENDED (e.g., 27 = 25 base + 1 left + 1 right halo)
      /// elemOffset is EXTENDED (e.g., 24 = max(0, 25 - 1) for Worker 1)
      ///
      /// We need to recover base offset and use base chunk size for division:
      ///   baseChunkSize = plan_.chunkSize - haloLeft - haloRight
      ///   baseOffset = elemOffset + haloLeft (if elemOffset > 0)
      ///              = 0 (if elemOffset == 0, may be clamped Worker 0)
      ///   startChunk = baseOffset / baseChunkSize

      Value haloLeft = builder.create<arith::ConstantIndexOp>(
          loc, plan_.stencilInfo->haloLeft);
      Value haloRight = builder.create<arith::ConstantIndexOp>(
          loc, plan_.stencilInfo->haloRight);

      /// Compute base chunk size
      Value baseChunkSize = builder.create<arith::SubIOp>(loc, plan_.chunkSize, haloLeft);
      baseChunkSize = builder.create<arith::SubIOp>(loc, baseChunkSize, haloRight);

      /// Recover base offset from extended offset
      /// elemOffset is extended: max(0, baseOffset - haloLeft)
      /// If elemOffset == 0, baseOffset = 0 (Worker 0 or actual 0 start)
      /// If elemOffset > 0, baseOffset = elemOffset + haloLeft
      Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
      Value isZero = builder.create<arith::CmpIOp>(
          loc, arith::CmpIPredicate::eq, elemOffset, zero);
      Value adjustedOffset = builder.create<arith::AddIOp>(loc, elemOffset, haloLeft);
      Value baseOffset = builder.create<arith::SelectOp>(loc, isZero, zero, adjustedOffset);

      /// Compute chunk index using base coordinates
      startChunk = builder.create<arith::DivUIOp>(loc, baseOffset, baseChunkSize);

      /// Single-chunk model: each worker acquires exactly 1 chunk
      chunkCount = one;

      ARTS_DEBUG("  Stencil: baseChunkSize=" << baseChunkSize
                 << ", baseOffset=" << baseOffset
                 << ", startChunk=" << startChunk);
    } else {
      /// CHUNKED MODE: Multi-chunk div/mod model
      /// DISJOINT MODEL: Physical chunks are disjoint and indexed by plan_.chunkSize.
      /// Chunk layout: chunk 0 -> rows [0, plan_.chunkSize), chunk 1 -> rows
      /// [plan_.chunkSize, 2*plan_.chunkSize), etc.
      ///
      /// A partition may span MULTIPLE physical chunks if chunk boundary
      /// doesn't align with partition boundary.
      ///
      /// Example: plan_.chunkSize=66, worker 1 needs rows 63-128
      ///   - startChunk = 63/66 = 0
      ///   - endChunk = 128/66 = 1
      ///   - chunkCount = 2

      /// Compute start chunk index using physical chunk size
      startChunk = builder.create<arith::DivUIOp>(loc, elemOffset, plan_.chunkSize);

      /// Compute end position (elemOffset + elemSize - 1)
      Value endPos = builder.create<arith::AddIOp>(loc, elemOffset, elemSize);
      endPos = builder.create<arith::SubIOp>(loc, endPos, one);

      /// Compute end chunk index
      Value endChunk = builder.create<arith::DivUIOp>(loc, endPos, plan_.chunkSize);

      /// Clamp endChunk to valid range (numChunks - 1)
      if (!newOuterSizes_.empty()) {
        Value numChunks = newOuterSizes_[0];
        Value maxChunk = builder.create<arith::SubIOp>(loc, numChunks, one);
        endChunk = builder.create<arith::MinUIOp>(loc, endChunk, maxChunk);
      }

      /// Compute chunk count = endChunk - startChunk + 1
      chunkCount = builder.create<arith::SubIOp>(loc, endChunk, startChunk);
      chunkCount = builder.create<arith::AddIOp>(loc, chunkCount, one);
    }

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
  } else {
    /// ELEMENT-WISE: already in datablock space, no offset transformation
    newOffsets.push_back(elemOffset);
    newSizes.push_back(elemSize);

    acquire.getOffsetsMutable().assign(newOffsets);
    acquire.getSizesMutable().assign(newSizes);
    acquire.getOffsetHintsMutable().assign(newOffsets);
    acquire.getSizeHintsMutable().assign(newSizes);

    /// CRITICAL FIX: Store the OLD allocation's stride for linearized access!
    /// After element-wise rewrite, the new element type has size=1, but
    /// linearized accesses need the ORIGINAL stride to properly scale offsets.
    ///
    /// For element_sizes = [D0, D1, D2, ...], stride = D1 * D2 * ...
    /// This is the product of TRAILING dimensions, NOT all dimensions!
    if (auto staticStride = DatablockUtils::getStaticElementStride(oldAlloc_)) {
      if (*staticStride > 1) {
        acquire->setAttr("element_stride", builder.getIndexAttr(*staticStride));
        LLVM_DEBUG(
            llvm::dbgs()
            << "Element-wise rewrite: stored stride=" << *staticStride
            << " (trailing elementSizes product) in acquire attribute\n");
      }
    }

    /// Rebase EDT users via the element-wise rewriter.
    rebaseEdtUsers(acquire, builder);
  }
}

void DbRewriter::rewriteDbRef(DbRefOp ref, DbAllocOp newAlloc,
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

    /// Both Chunked and Stencil modes use div/mod for main body accesses
    bool usesChunks = (plan_.mode == RewriterMode::Chunked ||
                       plan_.mode == RewriterMode::Stencil);

    if (usesChunks) {
      Value cs = plan_.chunkSize ? plan_.chunkSize : zero();
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

std::unique_ptr<DbRewriterBase> DbRewriter::createRewriter(
    const DbRewritePlan &plan, Value chunkSize, Value startChunk,
    Value elemOffset, Value elemSize, unsigned outerRank, unsigned innerRank,
    ValueRange oldElementSizes, OpBuilder &builder, Location loc) {

  ARTS_DEBUG("DbRewriter::createRewriter mode=" << getRewriterModeName(plan.mode));

  switch (plan.mode) {
  case RewriterMode::Stencil: {
    assert(plan.stencilInfo && "Stencil mode requires stencil info");

    /// Create halo values from plan.stencilInfo
    Value haloLeft = builder.create<arith::ConstantIndexOp>(
        loc, plan.stencilInfo->haloLeft);
    Value haloRight = builder.create<arith::ConstantIndexOp>(
        loc, plan.stencilInfo->haloRight);

    /// plan.chunkSize is the EXTENDED size (includes halo).
    /// DbStencilRewriter needs the BASE chunk size for computing chunkStart.
    /// baseChunkSize = extendedChunkSize - haloLeft - haloRight
    Value baseChunkSize = builder.create<arith::SubIOp>(loc, chunkSize, haloLeft);
    baseChunkSize = builder.create<arith::SubIOp>(loc, baseChunkSize, haloRight);

    ARTS_DEBUG("  Stencil: haloLeft=" << plan.stencilInfo->haloLeft
               << ", haloRight=" << plan.stencilInfo->haloRight);

    return std::make_unique<DbStencilRewriter>(
        baseChunkSize, startChunk, haloLeft, haloRight,
        plan.stencilInfo->totalRows, outerRank, innerRank);
  }

  case RewriterMode::Chunked: {
    ARTS_DEBUG("  Chunked: outerRank=" << outerRank << ", innerRank=" << innerRank);
    return std::make_unique<DbChunkedRewriter>(
        chunkSize, startChunk, elemOffset, outerRank, innerRank);
  }

  case RewriterMode::ElementWise: {
    ARTS_DEBUG("  ElementWise: outerRank=" << outerRank << ", innerRank=" << innerRank);
    return std::make_unique<DbElementWiseRewriter>(
        elemOffset, elemSize, outerRank, innerRank, oldElementSizes);
  }
  }

  llvm_unreachable("Unknown RewriterMode");
}
