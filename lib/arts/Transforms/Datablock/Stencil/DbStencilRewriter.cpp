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
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Transforms/Datablock/Stencil/DbStencilIndexer.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/RemovalUtils.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

#define DEBUG_TYPE "db_transforms"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool isDefinedInRegion(Value val, Region *region) {
  if (auto blockArg = val.dyn_cast<BlockArgument>())
    return region->isAncestor(blockArg.getOwner()->getParent());
  if (Operation *defOp = val.getDefiningOp())
    return region->isAncestor(defOp->getParentRegion());
  return false;
}

static bool isStartBlockArithmeticOp(Operation *op) {
  return isa<arith::ConstantIndexOp, arith::ConstantIntOp, arith::DivUIOp,
             arith::DivSIOp, arith::RemUIOp, arith::RemSIOp, arith::AddIOp,
             arith::SubIOp, arith::MulIOp, arith::MaxUIOp, arith::MinUIOp,
             arith::SelectOp,
             arith::IndexCastOp>(op);
}

} // namespace

void DbStencilRewriter::transformAcquire(const DbRewriteAcquire &info,
                                         DbAllocOp newAlloc,
                                         OpBuilder &builder) {
  DbAcquireOp acquire = info.acquire;
  /// Stencil rewriter needs both:
  ///   - element offset (for local row rebasing),
  ///   - chunk index (for owned/halo DB selection).
  /// Prefer explicit offsets for element base. If present, indices carry a
  /// chunk-index hint from ForLowering (non-uniform worker chunks).
  Value elemOffset = info.getOffsets().empty() ? info.getElemOffset()
                                               : info.getOffsets().front();
  Value chunkIndexHint =
      info.getIndices().empty() ? Value() : info.getIndices().front();

  ARTS_DEBUG("DbStencilRewriter::transformAcquire");
  ARTS_DEBUG("  acquire mode="
             << static_cast<int>(acquire.getMode())
             << ", partition mode=" << static_cast<int>(info.partitionInfo.mode)
             << ", offsets=" << info.partitionInfo.offsets.size()
             << ", sizes=" << info.partitionInfo.sizes.size()
             << ", indices=" << info.partitionInfo.indices.size());

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(acquire);
  Location loc = acquire.getLoc();

  /// Update source to new allocation only for host-side acquires.
  /// Acquires inside EDT regions must keep their in-scope source chain
  /// (block-arg/parent-acquire) to satisfy EDT dependency verification.
  bool inEdtRegion = static_cast<bool>(acquire->getParentOfType<EdtOp>());
  if (!inEdtRegion) {
    acquire.getSourceGuidMutable().assign(newAlloc.getGuid());
    acquire.getSourcePtrMutable().assign(newAlloc.getPtr());
  }

  /// Stencil mode uses offsets/sizes only; clear fine-grained indices and
  /// partition hints from the original acquire.
  acquire.getIndicesMutable().clear();
  acquire.clearPartitionHints();
  setPartitionMode(acquire.getOperation(), PartitionMode::stencil);

  /// Update acquire's ptr result type to match new source
  MemRefType newPtrType = acquire.getSourcePtr().getType().cast<MemRefType>();
  Type oldAcqPtrType = acquire.getPtr().getType();
  if (oldAcqPtrType != newPtrType) {
    acquire.getPtr().setType(newPtrType);

    /// Also update EDT block argument type if this acquire feeds an EDT
    auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
    if (blockArg && blockArg.getType() != newPtrType)
      blockArg.setType(newPtrType);
  }

  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);

  /// Base-offset semantics: elemOffset is the owned chunk base.
  Value baseOffset = elemOffset ? elemOffset : zero;
  if (baseOffset && !baseOffset.getType().isIndex())
    baseOffset = builder.create<arith::IndexCastOp>(loc, builder.getIndexType(),
                                                    baseOffset);
  /// Keep a center-adjusted base for chunk alignment/block-index math.
  /// Stencil loops often start at 1 (or other shifted center), while block
  /// boundaries are computed in unshifted element space.
  Value blockBaseOffset = baseOffset;
  if (auto center = getStencilCenterOffset(acquire.getOperation())) {
    if (*center != 0) {
      Value centerIdx = builder.create<arith::ConstantIndexOp>(loc, *center);
      blockBaseOffset =
          builder.create<arith::SubIOp>(loc, blockBaseOffset, centerIdx);
    }
  }
  if (chunkIndexHint && !chunkIndexHint.getType().isIndex())
    chunkIndexHint = builder.create<arith::IndexCastOp>(
        loc, builder.getIndexType(), chunkIndexHint);

  /// Uniform allocation block size — all blocks are allocated with this size.
  /// Use this instead of computeChunkRows to avoid non-uniform chunk sizes
  /// that disagree with the allocation and cause stencil indexing bugs.
  Value planBlockSize = plan.getBlockSize(0) ? plan.getBlockSize(0) : one;
  planBlockSize = builder.create<arith::MaxUIOp>(loc, planBlockSize, one);
  ARTS_DEBUG("  baseOffset=" << baseOffset << ", blockBaseOffset="
                             << blockBaseOffset << ", elemOffset=" << elemOffset
                             << ", planBlockSize=" << planBlockSize);

  /// Mixed stencil mode: only read-only stencil acquires should use ESD
  /// 3-buffer lowering. Non-stencil or write-capable acquires are lowered via
  /// regular block semantics over the same block-partitioned allocation.
  bool useStencilEsd = info.partitionInfo.mode == PartitionMode::stencil &&
                       acquire.getMode() == ArtsMode::in && plan.stencilInfo &&
                       plan.stencilInfo->hasHalo();
  int64_t haloLeftVal = plan.stencilInfo ? plan.stencilInfo->haloLeft : 0;
  int64_t haloRightVal = plan.stencilInfo ? plan.stencilInfo->haloRight : 0;
  ARTS_DEBUG("  initial ESD eligibility="
             << useStencilEsd << " (hasHalo="
             << (plan.stencilInfo ? plan.stencilInfo->hasHalo() : false)
             << ")");

  if (useStencilEsd) {
    /// ESD 3-buffer lowering currently supports one owned DB block per task.
    /// Derive an owned-span estimate by removing halo width from the hinted
    /// acquire span and require it to be bounded by one plan block.
    Value elemSize = one;
    if (!info.partitionInfo.sizes.empty() && info.partitionInfo.sizes.front())
      elemSize = info.partitionInfo.sizes.front();

    auto isBoundedByBlock = [&](Value sz, Value bsVal) -> bool {
      if (!sz || !bsVal)
        return false;
      Value s = ValueUtils::stripClampOne(sz);
      Value bs = ValueUtils::stripClampOne(bsVal);
      if (ValueUtils::areValuesEquivalent(s, bs))
        return true;
      if (auto minOp = s.getDefiningOp<arith::MinUIOp>()) {
        Value lhs = ValueUtils::stripClampOne(minOp.getLhs());
        Value rhs = ValueUtils::stripClampOne(minOp.getRhs());
        if (ValueUtils::areValuesEquivalent(lhs, bs) ||
            ValueUtils::areValuesEquivalent(rhs, bs))
          return true;
      }
      return false;
    };

    Value ownedSpan = elemSize;
    int64_t totalHalo = haloLeftVal + haloRightVal;
    if (totalHalo > 0) {
      Value haloConst = builder.create<arith::ConstantIndexOp>(loc, totalHalo);
      ownedSpan = builder.create<arith::SubIOp>(loc, ownedSpan, haloConst);
    }

    bool singleBlock = false;
    auto constOwnedSpan = arts::extractBlockSizeFromHint(ownedSpan);
    auto constPlanBs = arts::extractBlockSizeFromHint(planBlockSize);
    if (constOwnedSpan && constPlanBs && *constOwnedSpan <= *constPlanBs)
      singleBlock = true;
    if (!singleBlock && isBoundedByBlock(ownedSpan, planBlockSize))
      singleBlock = true;

    ARTS_DEBUG("  ESD single-block check: singleBlock="
               << singleBlock << ", ownedSpan=" << ownedSpan);
    useStencilEsd = singleBlock;
  }

  ARTS_DEBUG("  final ESD decision=" << useStencilEsd);
  if (!useStencilEsd) {
    transformAcquireAsBlock(info, acquire, newAlloc, builder);
    return;
  }

  /// STENCIL MODE with ESD: 3-acquire pattern
  /// Each worker acquires:
  ///   1. Owned chunk (full chunk)
  ///   2. Left halo (partial slice from previous chunk)
  ///   3. Right halo (partial slice from next chunk)
  Value chunkCount = one;

  /// Compute the block index from the element-space base offset that includes
  /// halo expansion. Normalize to owned-block coordinates first:
  ///   ownedBase = haloBase + haloLeft - centerShift
  /// then map to block space.
  Value blockIdxBase = blockBaseOffset;
  if (haloLeftVal > 0) {
    Value haloLeftConst =
        builder.create<arith::ConstantIndexOp>(loc, haloLeftVal);
    blockIdxBase =
        builder.create<arith::AddIOp>(loc, blockIdxBase, haloLeftConst);
  }
  Value blockIdx =
      builder.create<arith::DivUIOp>(loc, blockIdxBase, planBlockSize);

  ARTS_DEBUG("  Stencil ESD: blockIdx=" << blockIdx
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
    Value leftChunkIdx = builder.create<arith::SubIOp>(loc, blockIdx, one);

    /// element_offsets: start at (planBlockSize - haloLeft) within prev chunk
    Value leftChunkRows = planBlockSize;
    Value haloLeftConst =
        builder.create<arith::ConstantIndexOp>(loc, haloLeftVal);
    Value leftElemOffset =
        builder.create<arith::SubIOp>(loc, leftChunkRows, haloLeftConst);

    /// bounds_valid = (blockIdx != 0): valid only if not first block
    Value leftValid = builder.create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::ne, blockIdx, zero);

    auto leftHalo = builder.create<DbAcquireOp>(
        loc, ArtsMode::in, acquire.getSourceGuid(), acquire.getSourcePtr(),
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
    Value rightChunkIdx = builder.create<arith::AddIOp>(loc, blockIdx, one);

    /// bounds_valid = (blockIdx != lastBlock): valid only if not last block
    Value rightValid = builder.create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::ne, blockIdx, lastChunkIdx);

    auto rightHalo = builder.create<DbAcquireOp>(
        loc, ArtsMode::in, acquire.getSourceGuid(), acquire.getSourcePtr(),
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
    setLeftHaloArgIndex(acquire.getOperation(), leftHaloArgIdx);
    setRightHaloArgIndex(acquire.getOperation(), rightHaloArgIdx);
  }

  /// Set acquire offsets/sizes for stencil mode (use blockIdx, not worker
  /// index)
  SmallVector<Value> newOffsets, newSizes;
  newOffsets.push_back(blockIdx);
  newSizes.push_back(chunkCount);
  acquire.getOffsetsMutable().assign(newOffsets);
  acquire.getSizesMutable().assign(newSizes);

  /// Use the ForLowering's actual baseOffset as the indexer's base.
  /// After alignment, baseOffset == blockIdx * planBlockSize. Using the same
  /// SSA Value that ForLowering embedded in globalRow = add(baseOffset,
  /// localIter) ensures the stencil indexer's tryVersionRowLoop correctly
  /// identifies the coordinate system (includesBaseOffset = true) and splits
  /// loops correctly.
  rebaseEdtUsers(acquire, builder, baseOffset);
}

void DbStencilRewriter::transformAcquireAsBlock(const DbRewriteAcquire &info,
                                                DbAcquireOp acquire,
                                                DbAllocOp newAlloc,
                                                OpBuilder &builder) {
  Location loc = acquire.getLoc();
  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);

  acquire.clearPartitionHints();
  setPartitionMode(acquire.getOperation(), PartitionMode::block);

  SmallVector<Value> newOffsets;
  SmallVector<Value> newSizes;
  unsigned nPartDims = plan.numPartitionedDims();
  if (nPartDims == 0)
    nPartDims = 1;

  bool hasPartitionSlices =
      !info.partitionInfo.offsets.empty() || !info.partitionInfo.sizes.empty();
  bool forceFullRangeStencilWriter =
      acquire.getMode() != ArtsMode::in && !hasPartitionSlices;

  if (info.isFullRange || forceFullRangeStencilWriter) {
    for (unsigned d = 0; d < nPartDims; ++d) {
      newOffsets.push_back(zero);
      Value blockCount = (d < plan.outerSizes.size() && plan.outerSizes[d])
                             ? plan.outerSizes[d]
                             : one;
      newSizes.push_back(blockCount);
    }
  } else {
    /// For block-mode fallback acquires on stencil allocations, convert
    /// element-space partition hints into DB-space block slices.
    const auto &offsets = info.partitionInfo.offsets;
    const auto &sizes = info.partitionInfo.sizes;
    for (unsigned d = 0; d < nPartDims; ++d) {
      Value blockSize = plan.getBlockSize(d);
      if (!blockSize)
        blockSize = one;
      blockSize = builder.create<arith::MaxUIOp>(loc, blockSize, one);

      Value elemOff = (d < offsets.size() && offsets[d]) ? offsets[d] : zero;
      Value elemSz = (d < sizes.size() && sizes[d]) ? sizes[d] : one;
      elemSz = builder.create<arith::MaxUIOp>(loc, elemSz, one);

      Value startBlock =
          builder.create<arith::DivUIOp>(loc, elemOff, blockSize);
      Value endElem = builder.create<arith::AddIOp>(loc, elemOff, elemSz);
      endElem = builder.create<arith::SubIOp>(loc, endElem, one);
      Value endBlock = builder.create<arith::DivUIOp>(loc, endElem, blockSize);

      Value startAboveMax;
      if (d < plan.outerSizes.size()) {
        Value maxBlock =
            builder.create<arith::SubIOp>(loc, plan.outerSizes[d], one);
        startAboveMax = builder.create<arith::CmpIOp>(
            loc, arith::CmpIPredicate::ugt, startBlock, maxBlock);
        Value clampedEnd =
            builder.create<arith::MinUIOp>(loc, endBlock, maxBlock);
        endBlock = builder.create<arith::SelectOp>(loc, startAboveMax, endBlock,
                                                   clampedEnd);
      }

      Value blockCount =
          builder.create<arith::SubIOp>(loc, endBlock, startBlock);
      blockCount = builder.create<arith::AddIOp>(loc, blockCount, one);

      if (startAboveMax) {
        startBlock = builder.create<arith::SelectOp>(loc, startAboveMax, zero,
                                                     startBlock);
        blockCount = builder.create<arith::SelectOp>(loc, startAboveMax, zero,
                                                     blockCount);
      }

      newOffsets.push_back(startBlock);
      newSizes.push_back(blockCount);
    }
  }

  if (newOffsets.empty())
    newOffsets.push_back(zero);
  if (newSizes.empty()) {
    Value fallbackSize = (!plan.outerSizes.empty() && plan.outerSizes.front())
                             ? plan.outerSizes.front()
                             : one;
    newSizes.push_back(fallbackSize);
  }

  acquire.getOffsetsMutable().assign(newOffsets);
  acquire.getSizesMutable().assign(newSizes);

  /// Block fallback must always rebase EDT-side db_ref users into local
  /// block coordinates. Stencil-origin acquires can still carry global-row
  /// indexing; skipping rebasing leaves invalid local block accesses.
  bool rebaseAsBlock = !info.skipRebase;
  if (rebaseAsBlock)
    rebaseEdtUsersAsBlock(acquire, builder);
}

bool DbStencilRewriter::rebaseEdtUsersAsBlock(DbAcquireOp acquire,
                                              OpBuilder &builder) {
  auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
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
  if (!targetType.isa_and_nonnull<MemRefType>())
    return false;

  Type derivedType = targetType;
  if (auto dbAlloc = dyn_cast_or_null<DbAllocOp>(
          DatablockUtils::getUnderlyingDbAlloc(blockArg)))
    derivedType = dbAlloc.getAllocatedElementType();

  OpBuilder::InsertionGuard ig(builder);
  builder.setInsertionPointToStart(&edt.getBody().front());
  Location loc = acquire.getLoc();
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);

  unsigned nPartDims = plan.numPartitionedDims();
  if (nPartDims == 0)
    nPartDims = 1;

  SmallVector<Value> effectiveBlockSizes;
  SmallVector<Value> effectiveStartBlocks;
  effectiveBlockSizes.reserve(nPartDims);
  effectiveStartBlocks.reserve(nPartDims);

  Region &edtRegion = edt.getBody();
  IRMapping cloneMapping;
  llvm::SetVector<Value> valuesToClone;

  for (unsigned d = 0; d < nPartDims; ++d) {
    Value bs = plan.getBlockSize(d);
    if (!bs)
      bs = one;
    bs = builder.create<arith::MaxUIOp>(loc, bs, one);
    effectiveBlockSizes.push_back(bs);

    Value startBlock =
        d < acquire.getOffsets().size() ? acquire.getOffsets()[d] : zero;
    if (startBlock && !startBlock.getType().isIndex())
      startBlock = builder.create<arith::IndexCastOp>(
          loc, builder.getIndexType(), startBlock);
    if (!startBlock)
      startBlock = zero;
    if (!isDefinedInRegion(startBlock, &edtRegion) &&
        startBlock.getDefiningOp())
      valuesToClone.insert(startBlock);
    effectiveStartBlocks.push_back(startBlock);
  }

  if (!valuesToClone.empty()) {
    if (!ValueUtils::cloneValuesIntoRegion(
            valuesToClone, &edtRegion, cloneMapping, builder,
            /*allowMemoryEffectFree=*/false, isStartBlockArithmeticOp))
      return false;
  }

  for (Value &startBlock : effectiveStartBlocks) {
    if (!startBlock)
      continue;
    if (cloneMapping.contains(startBlock))
      startBlock = cloneMapping.lookup(startBlock);
    if (!isDefinedInRegion(startBlock, &edtRegion))
      startBlock = zero;
  }

  auto indexer = createBlockIndexer(effectiveBlockSizes, effectiveStartBlocks,
                                    plan.outerRank(), plan.innerRank(),
                                    plan.partitionedDims);
  if (!indexer)
    return false;

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

    ARTS_DEBUG("  Outside-EDT remap uses uniform plan block size: " << cs);

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
                                       Value baseOffsetArg,
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
  /// The incoming value is the acquire's true element base offset.
  /// The indexer uses base-offset semantics where:
  ///   - localRow < 0 -> left halo
  ///   - 0 <= localRow < blockSize -> owned
  ///   - localRow >= blockSize -> right halo
  Location loc = acquire.getLoc();
  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
  Value haloLeft =
      builder.create<arith::ConstantIndexOp>(loc, plan.stencilInfo->haloLeft);
  Value haloRight =
      builder.create<arith::ConstantIndexOp>(loc, plan.stencilInfo->haloRight);

  Value blockSize = plan.getBlockSize(0) ? plan.getBlockSize(0) : one;
  if (!blockSize.getType().isIndex())
    blockSize = builder.create<arith::IndexCastOp>(loc, builder.getIndexType(),
                                                   blockSize);
  blockSize = builder.create<arith::MaxUIOp>(loc, blockSize, one);

  Value baseOffset = baseOffsetArg ? baseOffsetArg : zero;
  if (!baseOffset.getType().isIndex())
    baseOffset = builder.create<arith::IndexCastOp>(loc, builder.getIndexType(),
                                                    baseOffset);

  /// For stencil mode, get the 3 block args (owned, left halo, right halo)
  Value leftHaloArg, rightHaloArg;
  Block &edtBody = edt.getBody().front();

  /// Read halo arg indices from acquire attributes
  if (auto leftIdxOpt = getLeftHaloArgIndex(acquire.getOperation())) {
    unsigned leftIdx = *leftIdxOpt;
    if (leftIdx < edtBody.getNumArguments())
      leftHaloArg = edtBody.getArgument(leftIdx);
  }
  if (auto rightIdxOpt = getRightHaloArgIndex(acquire.getOperation())) {
    unsigned rightIdx = *rightIdxOpt;
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
  info.sizes.push_back(blockSize);

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
  Value haloArg = edtBody.getArgument(edtBody.getNumArguments() - 1);

  /// Ensure the halo acquire is released in the task body.
  /// ForLowering only inserts releases for original dependencies; halo
  /// dependencies are introduced later by this rewriter.
  bool hasRelease = false;
  for (Operation &op : edtBody.without_terminator()) {
    if (auto release = dyn_cast<DbReleaseOp>(&op)) {
      if (release.getSource() == haloArg) {
        hasRelease = true;
        break;
      }
    }
  }
  if (!hasRelease) {
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPoint(edtBody.getTerminator());
    builder.create<DbReleaseOp>(haloAcq.getLoc(), haloArg);
  }

  ARTS_DEBUG("  Added halo dep to EDT, now has "
             << edt.getDependencies().size() << " deps and "
             << edtBody.getNumArguments() << " block args");
}
