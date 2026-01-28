///==========================================================================///
/// File: DbBlockRewriter.cpp
///
/// Block rewriter: uniform blocks with div/mod localization.
///
/// Example transformation (N=100 rows, blockSize=25, 4 workers):
///   BEFORE:
///     %db = arts.db_alloc memref<100x10xf64>
///     arts.db_acquire %db [0] [100]            /// worker acquires all rows
///     %ref = arts.db_ref %db[%i]
///     memref.load %ref[%j]                     /// global row j
///
///   AFTER:
///     %db = arts.db_alloc memref<4x25x10xf64>  /// 4 blocks of 25 rows
///     arts.db_acquire %db [%block] [1]         /// worker 1: block[1]
///     %ref = arts.db_ref %db[%j / 25]          /// block = j / blockSize
///     memref.load %ref[%j % 25]                /// local = j % blockSize
///
/// Index localization: block = global / blockSize, local = global % blockSize
///==========================================================================///

#include "arts/Transforms/Datablock/Block/DbBlockRewriter.h"
#include "arts/Transforms/Datablock/Block/DbBlockIndexer.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/RemovalUtils.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/IRMapping.h"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

/// Check if a value is defined inside a given region.
static bool isDefinedInRegion(Value val, Region *region) {
  if (auto blockArg = val.dyn_cast<BlockArgument>())
    return region->isAncestor(blockArg.getOwner()->getParent());
  if (Operation *defOp = val.getDefiningOp())
    return region->isAncestor(defOp->getParentRegion());
  return false;
}

/// Check if an operation can be safely cloned (pure arithmetic ops).
static bool canCloneForStartBlock(Operation *op) {
  return isa<arith::ConstantIndexOp, arith::ConstantIntOp, arith::DivUIOp,
             arith::DivSIOp, arith::RemUIOp, arith::RemSIOp, arith::AddIOp,
             arith::SubIOp, arith::MulIOp, arith::MaxUIOp, arith::MinUIOp,
             arith::IndexCastOp>(op);
}

/// Clone a value's definition chain into the EDT body if the value is
/// defined outside the EDT. Returns the cloned value (or the original
/// if it's already available inside the EDT).
///
/// This handles the case where values like `k_block / 16` are computed
/// outside an EDT but referenced inside. When EdtLowering outlines the
/// EDT, such references become invalid. By cloning them inside, we
/// ensure all values used in the EDT body are properly available.
static Value cloneValueIntoEdtIfNeeded(Value val, EdtOp edt, OpBuilder &builder,
                                       IRMapping &mapping) {
  if (!val)
    return val;

  /// Check if already mapped
  if (mapping.contains(val))
    return mapping.lookup(val);

  /// If the value is defined inside the EDT, it's already available
  Region &edtRegion = edt.getBody();
  if (isDefinedInRegion(val, &edtRegion)) {
    mapping.map(val, val);
    return val;
  }

  /// For block arguments from outside, we can't clone them - they must be
  /// passed as EDT parameters by EdtLowering. Return the original and
  /// EdtLowering will handle it.
  if (val.isa<BlockArgument>())
    return val;

  Operation *defOp = val.getDefiningOp();
  if (!defOp)
    return val;

  /// Only clone safe arithmetic operations
  if (!canCloneForStartBlock(defOp))
    return val;

  /// Recursively clone operands first
  for (Value operand : defOp->getOperands()) {
    if (!mapping.contains(operand))
      cloneValueIntoEdtIfNeeded(operand, edt, builder, mapping);
  }

  /// Clone the operation with mapped operands
  Operation *clonedOp = builder.clone(*defOp, mapping);
  Value result = clonedOp->getResult(0);
  mapping.map(val, result);

  ARTS_DEBUG("  Cloned external startBlock value into EDT: "
             << defOp->getName().getStringRef());

  return result;
}

void DbBlockRewriter::transformAcquire(const DbRewriteAcquire &info,
                                       DbAllocOp newAlloc, OpBuilder &builder) {
  DbAcquireOp acquire = info.acquire;

  ARTS_DEBUG("DbBlockRewriter::transformAcquire");

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

  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  unsigned nPartDims = plan.numPartitionedDims();
  if (nPartDims == 0)
    nPartDims = 1;

  /// Mixed mode: full-range coarse acquire on a block allocation.
  if (info.isFullRange) {
    SmallVector<Value> newOffsets, newSizes;
    for (unsigned d = 0; d < nPartDims; ++d) {
      newOffsets.push_back(zero);
      Value blockCount =
          (d < plan.numBlocksPerDim.size() && plan.numBlocksPerDim[d])
              ? plan.numBlocksPerDim[d]
              : (d < newOuterSizes.size() ? newOuterSizes[d] : one);
      newSizes.push_back(blockCount);
    }

    acquire.getOffsetsMutable().assign(newOffsets);
    acquire.getSizesMutable().assign(newSizes);

    ARTS_DEBUG("  Mixed mode: coarse acquire gets full range [0, ...)\n");
    if (!info.skipRebase)
      rebaseEdtUsers(acquire, builder, /*startBlock=*/zero);
    return;
  }

  /// N-D BLOCK MODE: div/mod model for each partitioned dimension
  SmallVector<Value> newOffsets, newSizes;
  bool isSingleBlock = true;

  for (unsigned d = 0; d < nPartDims; ++d) {
    Value bs = plan.getBlockSize(d);
    if (!bs)
      bs = one;
    bs = builder.create<arith::MaxUIOp>(loc, bs, one);

    /// Get element offset and size for this dimension from partitionInfo.
    /// For block mode: offsets are range starts, sizes are range sizes.
    const auto &offsets = info.partitionInfo.offsets;
    const auto &sizes = info.partitionInfo.sizes;
    Value elemOff = (d < offsets.size() && offsets[d]) ? offsets[d] : zero;
    Value elemSz = (d < sizes.size() && sizes[d]) ? sizes[d] : one;

    Value startBlock = builder.create<arith::DivUIOp>(loc, elemOff, bs);
    Value endPos = builder.create<arith::AddIOp>(loc, elemOff, elemSz);
    endPos = builder.create<arith::SubIOp>(loc, endPos, one);
    Value endBlock = builder.create<arith::DivUIOp>(loc, endPos, bs);

    /// Clamp endBlock to valid range
    if (d < newOuterSizes.size()) {
      Value maxBlock =
          builder.create<arith::SubIOp>(loc, newOuterSizes[d], one);
      endBlock = builder.create<arith::MinUIOp>(loc, endBlock, maxBlock);
    }

    Value blockCount = builder.create<arith::SubIOp>(loc, endBlock, startBlock);
    blockCount = builder.create<arith::AddIOp>(loc, blockCount, one);

    /// Check if single-block for this dimension
    auto constElemSz = ValueUtils::getConstantValue(elemSz);
    auto constBs = arts::extractBlockSizeFromHint(bs);
    bool isSingleDim = false;
    if (constElemSz && constBs) {
      isSingleDim = (*constElemSz <= *constBs);
    } else {
      isSingleBlock = false; // Unknown sizes, assume not single block
    }
    if (isSingleDim)
      blockCount = one;

    newOffsets.push_back(startBlock);
    newSizes.push_back(blockCount);

    if (!isSingleDim)
      isSingleBlock = false;
  }

  acquire.getOffsetsMutable().assign(newOffsets);
  acquire.getSizesMutable().assign(newSizes);

  /// Use mode-aware rebase for block mode EDT users
  if (!info.skipRebase)
    rebaseEdtUsers(acquire, builder, newOffsets.empty() ? zero : newOffsets[0],
                   isSingleBlock);
}

void DbBlockRewriter::transformDbRef(DbRefOp ref, DbAllocOp newAlloc,
                                     OpBuilder &builder) {
  Location loc = ref.getLoc();
  Type newElementType = newAlloc.getAllocatedElementType();
  Value newSource = (ref.getSource() == oldAlloc.getPtr()) ? newAlloc.getPtr()
                                                           : ref.getSource();
  /// Detect allocation-level single-block (same condition as verifier)
  bool allocSingleBlock = !newOuterSizes.empty() &&
      llvm::all_of(newOuterSizes, [](Value v) {
        int64_t val;
        return ValueUtils::getConstantIndex(v, val) && val == 1;
      });

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
    Value one = builder.create<arith::ConstantIndexOp>(userLoc, 1);

    /// N-D block partitioning: div/mod for each partitioned dimension
    unsigned nPartDims = plan.numPartitionedDims();
    if (nPartDims == 0)
      nPartDims = 1; // Default to 1D if not specified

    for (unsigned d = 0; d < loadStoreIndices.size(); ++d) {
      Value globalIdx = loadStoreIndices[d];
      if (d < nPartDims) {
        /// Partitioned dimension: compute block index and local index
        if (allocSingleBlock) {
          newOuter.push_back(
              builder.create<arith::ConstantIndexOp>(userLoc, 0));
        } else {
          Value bs = plan.getBlockSize(d);
          if (!bs)
            bs = one;
          bs = builder.create<arith::MaxUIOp>(userLoc, bs, one);
          newOuter.push_back(
              builder.create<arith::DivUIOp>(userLoc, globalIdx, bs));
        }

        /// Always compute rem for memref index
        Value bs = plan.getBlockSize(d);
        if (!bs)
          bs = one;
        bs = builder.create<arith::MaxUIOp>(userLoc, bs, one);
        newInner.push_back(
            builder.create<arith::RemUIOp>(userLoc, globalIdx, bs));
      } else {
        /// Non-partitioned dimension: pass through to inner
        newInner.push_back(globalIdx);
      }
    }

    if (newOuter.empty() || newInner.empty()) {
      Value zero = builder.create<arith::ConstantIndexOp>(userLoc, 0);
      if (newOuter.empty())
        newOuter.push_back(zero);
      if (newInner.empty())
        newInner.push_back(zero);
    }

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

  /// For db_refs without load/store users, compute block indices for N-D
  if (!hasLoadStoreUsers && !ref.getResult().use_empty()) {
    OpBuilder::InsertionGuard ig(builder);
    builder.setInsertionPoint(ref);

    unsigned nPartDims = plan.numPartitionedDims();
    if (nPartDims == 0)
      nPartDims = 1;

    SmallVector<Value> blockIndices;
    Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
    ValueRange refIndices = ref.getIndices();

    for (unsigned d = 0; d < nPartDims; ++d) {
      if (allocSingleBlock) {
        blockIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
      } else {
        Value bs = plan.getBlockSize(d);
        if (!bs)
          bs = one;
        bs = builder.create<arith::MaxUIOp>(loc, bs, one);

        if (d < refIndices.size()) {
          blockIndices.push_back(
              builder.create<arith::DivUIOp>(loc, refIndices[d], bs));
        } else {
          blockIndices.push_back(
              builder.create<arith::ConstantIndexOp>(loc, 0));
        }
      }
    }

    auto newRef =
        builder.create<DbRefOp>(loc, newElementType, newSource, blockIndices);
    ref.replaceAllUsesWith(newRef.getResult());
  }

  /// Remove old load/store operations
  removal.removeAllMarked();
}

bool DbBlockRewriter::rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                                     Value startBlock, bool isSingleBlock) {
  ARTS_DEBUG("DbBlockRewriter::rebaseEdtUsers (isSingleBlock=" << isSingleBlock
                                                               << ")");

  auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
  if (!blockArg)
    return false;

  /// Determine element type from DbRefOp users or block argument
  Type targetType;
  for (Operation *user : blockArg.getUsers())
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      targetType = ref.getResult().getType();
      break;
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

  /// Get start blocks directly from acquire's DB-space offsets
  /// The acquire offsets are already in block coordinates (not element
  /// coordinates)
  SmallVector<Value> dbSpaceOffsets(acquire.getOffsets().begin(),
                                    acquire.getOffsets().end());

  OpBuilder::InsertionGuard ig(builder);
  builder.setInsertionPointToStart(&edt.getBody().front());
  Location loc = acquire.getLoc();

  SmallVector<Value> effectiveBlockSizes;
  SmallVector<Value> effectiveStartBlocks;
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);

  /// IRMapping for cloning external values into the EDT body
  IRMapping cloneMapping;

  unsigned nPartDims = plan.numPartitionedDims();
  if (nPartDims == 0)
    nPartDims = 1; /// Default to 1D if not specified
  for (unsigned d = 0; d < nPartDims; ++d) {
    Value bs = plan.getBlockSize(d);
    if (!bs)
      bs = one;
    bs = builder.create<arith::MaxUIOp>(loc, bs, one);
    effectiveBlockSizes.push_back(bs);

    /// Always use DB-space offset as startBlock (even for single-block mode)
    /// The relative index formula: relBlock = physBlock - startBlock
    /// For single-block, physBlock == startBlock, so relBlock = 0 automatically
    if (d < dbSpaceOffsets.size() && dbSpaceOffsets[d]) {
      /// Clone the startBlock value into the EDT if it's defined outside.
      /// This prevents invalid SSA references when EdtLowering outlines the
      /// EDT.
      Value startBlockValue = cloneValueIntoEdtIfNeeded(dbSpaceOffsets[d], edt,
                                                        builder, cloneMapping);
      effectiveStartBlocks.push_back(startBlockValue);
    } else {
      effectiveStartBlocks.push_back(zero);
    }
  }

  /// Build PartitionInfo for block mode
  PartitionInfo info;
  info.mode = PartitionMode::block;
  info.sizes.assign(effectiveBlockSizes.begin(), effectiveBlockSizes.end());
  /// Note: offsets are stored but startBlocks are passed separately
  /// because the division (offsets / sizes) requires a builder.

  /// Detect allocation-level single-block: all outer sizes are constant 1.
  /// This matches the verifier's coarse check exactly.
  bool allocSingleBlock = !newOuterSizes.empty() &&
      llvm::all_of(newOuterSizes, [](Value v) {
        int64_t val;
        return ValueUtils::getConstantIndex(v, val) && val == 1;
      });

  /// Create N-D block indexer with PartitionInfo
  auto indexer = std::make_unique<DbBlockIndexer>(
      info, effectiveStartBlocks, newOuterSizes.size(), newInnerSizes.size(),
      allocSingleBlock);

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
