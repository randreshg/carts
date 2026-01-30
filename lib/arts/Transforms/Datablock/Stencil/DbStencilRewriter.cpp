///==========================================================================///
/// File: DbStencilRewriter.cpp
///
/// Stencil rewriter: 3-buffer halo for stencil patterns (e.g., jacobi).
///
/// Example transformation (N=100 rows, blockSize=25, halo=1):
///
///   BEFORE:
///     %db = arts.db_alloc memref<100x10xf64>
///     arts.db_acquire %db [0] [100]
///     %ref = arts.db_ref %db[%i]
///     memref.load %ref[%j-1], %ref[%j], %ref[%j+1]         /// stencil access
///
///   AFTER (worker 1, rows 25-49):
///     %db = arts.db_alloc memref<4x25x10xf64>
///     %owned = arts.db_acquire %db [1] [1]                 /// chunk 1
///     %left  = arts.db_acquire %db [0] [1] elem_offset=24  /// row 24
///     %right = arts.db_acquire %db [2] [1] elem_offset=0   /// row 50
///
///     /// Load selects buffer based on localRow:
///     ///   localRow < 0        -> left halo
///     ///   0 <= localRow < 25  -> owned
///     ///   localRow >= 25      -> right halo
///
/// Index localization: 3-way select based on (globalRow - baseOffset)
///==========================================================================///

#include "arts/Transforms/Datablock/Stencil/DbStencilRewriter.h"
#include "arts/Transforms/Datablock/Stencil/DbStencilIndexer.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/RemovalUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"

#define DEBUG_TYPE "db_transforms"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

void DbStencilRewriter::transformAcquire(const DbRewriteAcquire &info,
                                         DbAllocOp newAlloc,
                                         OpBuilder &builder) {
  DbAcquireOp acquire = info.acquire;
  /// Prefer explicit offsets for stencil mode; indices are element coordinates.
  Value elemOffset = info.getOffsets().empty() ? info.getElemOffset()
                                               : info.getOffsets().front();

  ARTS_DEBUG("DbStencilRewriter::transformAcquire");

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(acquire);
  Location loc = acquire.getLoc();

  /// Update source to new allocation
  acquire.getSourceGuidMutable().assign(newAlloc.getGuid());
  acquire.getSourcePtrMutable().assign(newAlloc.getPtr());

  /// Stencil mode uses offsets/sizes only; clear fine-grained indices and
  /// partition hints from the original acquire.
  acquire.getIndicesMutable().clear();
  acquire.clearPartitionHints();
  setPartitionMode(acquire.getOperation(), PartitionMode::stencil);

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

  /// STENCIL MODE with ESD: 3-acquire pattern
  /// Each worker acquires:
  ///   1. Owned chunk (full chunk)
  ///   2. Left halo (partial slice from previous chunk)
  ///   3. Right halo (partial slice from next chunk)
  assert(plan.stencilInfo && plan.stencilInfo->hasHalo() &&
         "Stencil mode requires stencil info with halo");

  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
  int64_t haloLeftVal = plan.stencilInfo->haloLeft;
  int64_t haloRightVal = plan.stencilInfo->haloRight;

  /// Base-offset semantics: elemOffset is the owned chunk base.
  Value baseOffset = elemOffset ? elemOffset : zero;

  /// Compute chunk index for owned chunk
  Value startBlock =
      builder.create<arith::DivUIOp>(loc, baseOffset, plan.getBlockSize(0));
  Value chunkCount = one;

  ARTS_DEBUG("  Stencil ESD: chunkIdx=" << startBlock
                                        << ", haloLeft=" << haloLeftVal
                                        << ", haloRight=" << haloRightVal);

  /// Get inner dimension size for partial acquisition
  auto buildHaloOffsets = [&](Value rowOffset) {
    SmallVector<Value> offsets;
    offsets.push_back(rowOffset);
    for (size_t i = 1; i < plan.innerSizes.size(); ++i)
      offsets.push_back(zero);
    return offsets;
  };

  auto buildHaloSizes = [&](int64_t haloRows) {
    SmallVector<Value> sizes;
    sizes.push_back(builder.create<arith::ConstantIndexOp>(loc, haloRows));
    for (size_t i = 1; i < plan.innerSizes.size(); ++i)
      sizes.push_back(plan.innerSizes[i]);
    return sizes;
  };

  /// Get number of chunks for boundary checking
  Value numBlocks = plan.outerSizes.empty()
                        ? builder.create<arith::ConstantIndexOp>(loc, 1)
                        : plan.outerSizes[0];
  Value lastChunkIdx = builder.create<arith::SubIOp>(loc, numBlocks, one);

  /// Create LEFT HALO acquire (partial: last haloLeft rows of prev chunk)
  unsigned leftHaloArgIdx = ~0u;
  if (haloLeftVal > 0) {
    Value leftChunkIdx = builder.create<arith::SubIOp>(loc, startBlock, one);

    /// element_offsets: start at (blockSize - haloLeft) within prev chunk
    Value leftElemOffset = builder.create<arith::SubIOp>(
        loc, plan.getBlockSize(0),
        builder.create<arith::ConstantIndexOp>(loc, haloLeftVal));

    /// bounds_valid = (startBlock != 0): valid only if not first chunk
    Value leftValid = builder.create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::ne, startBlock, zero);

    auto leftHalo = builder.create<DbAcquireOp>(
        loc, ArtsMode::in, newAlloc.getGuid(), newAlloc.getPtr(),
        PartitionMode::stencil, /*indices=*/SmallVector<Value>{},
        /*offsets=*/SmallVector<Value>{leftChunkIdx},
        /*sizes=*/SmallVector<Value>{one},
        /*partition_indices=*/SmallVector<Value>{},
        /*partition_offsets=*/SmallVector<Value>{},
        /*partition_sizes=*/SmallVector<Value>{},
        /*bounds_valid=*/leftValid,
        /*element_offsets=*/buildHaloOffsets(leftElemOffset),
        /*element_sizes=*/buildHaloSizes(haloLeftVal));

    addHaloAcquireToEdt(acquire, leftHalo, builder);
    /// Track left halo block arg index (it's the last one added)
    auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
    if (edt)
      leftHaloArgIdx = edt.getBody().front().getNumArguments() - 1;
    ARTS_DEBUG("  Created left halo acquire, argIdx=" << leftHaloArgIdx);
  }

  /// Create RIGHT HALO acquire (partial: first haloRight rows of next chunk)
  unsigned rightHaloArgIdx = ~0u;
  if (haloRightVal > 0) {
    Value rightChunkIdx = builder.create<arith::AddIOp>(loc, startBlock, one);

    /// bounds_valid = (startBlock != lastChunk): valid only if not last chunk
    Value rightValid = builder.create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::ne, startBlock, lastChunkIdx);

    auto rightHalo = builder.create<DbAcquireOp>(
        loc, ArtsMode::in, newAlloc.getGuid(), newAlloc.getPtr(),
        PartitionMode::stencil, /*indices=*/SmallVector<Value>{},
        /*offsets=*/SmallVector<Value>{rightChunkIdx},
        /*sizes=*/SmallVector<Value>{one},
        /*partition_indices=*/SmallVector<Value>{},
        /*partition_offsets=*/SmallVector<Value>{},
        /*partition_sizes=*/SmallVector<Value>{},
        /*bounds_valid=*/rightValid,
        /*element_offsets=*/buildHaloOffsets(zero),
        /*element_sizes=*/buildHaloSizes(haloRightVal));

    addHaloAcquireToEdt(acquire, rightHalo, builder);
    /// Track right halo block arg index (it's the last one added)
    auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
    if (edt)
      rightHaloArgIdx = edt.getBody().front().getNumArguments() - 1;
    ARTS_DEBUG("  Created right halo acquire, argIdx=" << rightHaloArgIdx);
  }

  /// Store halo block arg indices for the 3-buffer selection approach
  auto [edt, ownedBlockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
  if (edt && (haloLeftVal > 0 || haloRightVal > 0)) {
    unsigned ownedArgIdx = ownedBlockArg.getArgNumber();

    ARTS_DEBUG("  3-buffer selection: owned=" << ownedArgIdx
                                              << ", left=" << leftHaloArgIdx
                                              << ", right=" << rightHaloArgIdx);

    /// Store halo arg indices as acquire attributes for rebaseEdtUsers
    acquire->setAttr("left_halo_arg_idx", builder.getIndexAttr(leftHaloArgIdx));
    acquire->setAttr("right_halo_arg_idx",
                     builder.getIndexAttr(rightHaloArgIdx));
  }

  /// Set acquire offsets/sizes for stencil mode
  SmallVector<Value> newOffsets, newSizes;
  newOffsets.push_back(startBlock);
  newSizes.push_back(chunkCount);
  acquire.getOffsetsMutable().assign(newOffsets);
  acquire.getSizesMutable().assign(newSizes);

  /// Rebase EDT users with 3-buffer stencil mode
  rebaseEdtUsers(acquire, builder, startBlock);
}

void DbStencilRewriter::transformDbRef(DbRefOp ref, DbAllocOp newAlloc,
                                       OpBuilder &builder) {
  Location loc = ref.getLoc();
  Type newElementType = newAlloc.getAllocatedElementType();
  Value newSource = (ref.getSource() == oldAlloc.getPtr()) ? newAlloc.getPtr()
                                                           : ref.getSource();

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

    /// STENCIL: div/mod for chunk selection (owned rows start at 0).
    /// Prefer row index from load/store when it includes row+col, or when the
    /// element type is 1D (single index is the row). Otherwise fall back to
    /// db_ref indices (row-partitioned access with column-only loads).
    Value cs = plan.getBlockSize(0) ? plan.getBlockSize(0) : zero();
    Value rowIdx;
    bool rowFromLoad = false;
    unsigned elemRank = 0;
    if (auto memrefTy = newElementType.dyn_cast<MemRefType>())
      elemRank = memrefTy.getRank();

    bool loadIncludesRow = (loadStoreIndices.size() > 1) || (elemRank == 1);

    if (loadIncludesRow && !loadStoreIndices.empty()) {
      rowIdx = loadStoreIndices.front();
      rowFromLoad = true;
    } else if (!ref.getIndices().empty()) {
      rowIdx = ref.getIndices().front();
    } else if (!loadStoreIndices.empty()) {
      rowIdx = loadStoreIndices.front();
      rowFromLoad = true;
    }

    if (rowIdx) {
      newOuter.push_back(builder.create<arith::DivUIOp>(userLoc, rowIdx, cs));

      /// Inner index = (rowIdx % blockSize)
      Value rem = builder.create<arith::RemUIOp>(userLoc, rowIdx, cs);
      newInner.push_back(rem);

      if (rowFromLoad) {
        for (unsigned i = 1; i < loadStoreIndices.size(); ++i)
          newInner.push_back(loadStoreIndices[i]);
      } else {
        for (Value idx : loadStoreIndices)
          newInner.push_back(idx);
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

bool DbStencilRewriter::rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                                       Value startBlock,
                                       bool /*isSingleChunk*/) {
  ARTS_DEBUG("DbStencilRewriter::rebaseEdtUsers (3-buffer mode)");

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

  /// Derive BASE offset for stencil localization.
  /// FIXED: Pass BASE offset (startBlock * blockSize), NOT extended offset.
  /// The indexer uses base-offset semantics where:
  ///   - localRow < 0 -> left halo
  ///   - 0 <= localRow < blockSize -> owned
  ///   - localRow >= blockSize -> right halo
  Location loc = acquire.getLoc();
  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value haloLeft =
      builder.create<arith::ConstantIndexOp>(loc, plan.stencilInfo->haloLeft);
  Value haloRight =
      builder.create<arith::ConstantIndexOp>(loc, plan.stencilInfo->haloRight);

  /// Compute BASE offset = startBlock * blockSize (NOT extended!)
  Value baseOffset;
  if (startBlock && plan.getBlockSize(0)) {
    baseOffset =
        builder.create<arith::MulIOp>(loc, startBlock, plan.getBlockSize(0));
  } else {
    baseOffset = zero;
  }

  /// Adjust base offset by stencil center shift (if any) to align localRow
  /// with the logical loop index when loops start at non-zero bounds.
  if (auto centerAttr =
          acquire->getAttrOfType<IntegerAttr>("stencil_center_offset")) {
    int64_t center = centerAttr.getInt();
    if (center != 0) {
      Value shift = builder.create<arith::ConstantIndexOp>(loc, center);
      baseOffset = builder.create<arith::AddIOp>(loc, baseOffset, shift);
    }
  }

  /// For stencil mode, get the 3 block args (owned, left halo, right halo)
  Value leftHaloArg, rightHaloArg;
  Block &edtBody = edt.getBody().front();

  /// Read halo arg indices from acquire attributes
  if (auto leftIdxAttr =
          acquire->getAttrOfType<IntegerAttr>("left_halo_arg_idx")) {
    unsigned leftIdx = leftIdxAttr.getInt();
    if (leftIdx < edtBody.getNumArguments())
      leftHaloArg = edtBody.getArgument(leftIdx);
  }
  if (auto rightIdxAttr =
          acquire->getAttrOfType<IntegerAttr>("right_halo_arg_idx")) {
    unsigned rightIdx = rightIdxAttr.getInt();
    if (rightIdx < edtBody.getNumArguments())
      rightHaloArg = edtBody.getArgument(rightIdx);
  }

  ARTS_DEBUG("  Stencil 3-buffer: owned="
             << blockArg << ", left=" << (leftHaloArg ? "valid" : "null")
             << ", right=" << (rightHaloArg ? "valid" : "null"));

  /// Build PartitionInfo with BASE offset semantics
  PartitionInfo info;
  info.mode = PartitionMode::stencil;
  info.offsets.push_back(baseOffset);
  info.sizes.push_back(plan.getBlockSize(0));

  /// Create stencil indexer with PartitionInfo (base-offset semantics)
  auto indexer = std::make_unique<DbStencilIndexer>(
      info, haloLeft, haloRight, plan.outerRank(), plan.innerRank(), blockArg,
      leftHaloArg, rightHaloArg);

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

void DbStencilRewriter::addHaloAcquireToEdt(DbAcquireOp originalAcq,
                                            DbAcquireOp haloAcq,
                                            OpBuilder &builder) {
  ARTS_DEBUG("DbStencilRewriter::addHaloAcquireToEdt");

  /// Find the EDT that uses the original acquire
  auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(originalAcq);
  if (!edt) {
    ARTS_DEBUG("  No EDT found for acquire - skipping halo addition");
    return;
  }

  /// Add the halo acquire's ptr as a new EDT dependency
  /// The appendDependency method updates the EDT operands
  edt.appendDependency(haloAcq.getPtr());

  /// Add corresponding block argument to EDT body
  Block &edtBody = edt.getBody().front();
  edtBody.addArgument(haloAcq.getPtr().getType(), haloAcq.getLoc());

  ARTS_DEBUG("  Added halo dep to EDT, now has "
             << edt.getDependencies().size() << " deps and "
             << edtBody.getNumArguments() << " block args");
}
