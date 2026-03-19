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

#include "arts/transforms/db/block/DbBlockRewriter.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/transforms/db/block/DbBlockIndexer.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/Debug.h"
#include "arts/utils/EdtUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/RemovalUtils.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/IRMapping.h"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

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
  if (arts::isDefinedInRegion(val, &edtRegion)) {
    mapping.map(val, val);
    return val;
  }

  /// For block arguments from outside, we can't clone them - they must be
  /// passed as EDT parameters by EdtLowering. Return the original and
  /// EdtLowering will handle it.
  if (isa<BlockArgument>(val))
    return val;

  Operation *defOp = val.getDefiningOp();
  if (!defOp)
    return val;

  /// Only clone whitelisted arithmetic operations.
  if (!ValueAnalysis::canCloneOperation(defOp, /*allowMemoryEffectFree=*/false,
                                        arts::isStartBlockArithmeticOp))
    return val;

  /// Recursively clone operands first.
  for (Value operand : defOp->getOperands()) {
    if (!mapping.contains(operand))
      cloneValueIntoEdtIfNeeded(operand, edt, builder, mapping);
  }

  /// Clone the operation with mapped operands.
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

  /// Update source to new allocation only for host-side acquires.
  /// Acquires inside EDT regions must keep their in-scope source chain
  /// (block-arg/parent-acquire) to satisfy EDT dependency verification.
  bool inEdtRegion = static_cast<bool>(acquire->getParentOfType<EdtOp>());
  if (!inEdtRegion) {
    acquire.getSourceGuidMutable().assign(newAlloc.getGuid());
    acquire.getSourcePtrMutable().assign(newAlloc.getPtr());
  }

  /// Update acquire's ptr result type to match new source
  MemRefType newPtrType = cast<MemRefType>(acquire.getSourcePtr().getType());
  Type oldAcqPtrType = acquire.getPtr().getType();
  if (oldAcqPtrType != newPtrType) {
    acquire.getPtr().setType(newPtrType);

    /// Also update EDT block argument type if this acquire feeds an EDT
    auto [edt, blockArg] = getEdtBlockArgumentForAcquire(acquire);
    if (blockArg && blockArg.getType() != newPtrType)
      blockArg.setType(newPtrType);
  }

  Value one = arts::createOneIndex(builder, loc);
  Value zero = arts::createZeroIndex(builder, loc);
  unsigned nPartDims = plan.numPartitionedDims();
  if (nPartDims == 0)
    nPartDims = 1;

  /// Mixed mode: full-range coarse acquire on a block allocation.
  if (info.isFullRange) {
    SmallVector<Value> newOffsets, newSizes;
    for (unsigned d = 0; d < nPartDims; ++d) {
      newOffsets.push_back(zero);
      Value blockCount = (d < plan.outerSizes.size() && plan.outerSizes[d])
                             ? plan.outerSizes[d]
                             : one;
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

  /// Helper lambda to check dynamic alignment to block size
  auto isAlignedToBlockDynamic = [&](Value offset, Value bsVal) -> bool {
    if (!offset || !bsVal)
      return false;
    Value off = ValueAnalysis::stripNumericCasts(offset);
    Value bs = ValueAnalysis::stripClampOne(bsVal);
    if (auto mul = off.getDefiningOp<arith::MulIOp>()) {
      Value lhs = ValueAnalysis::stripClampOne(mul.getLhs());
      Value rhs = ValueAnalysis::stripClampOne(mul.getRhs());
      if (ValueAnalysis::areValuesEquivalent(lhs, bs) ||
          ValueAnalysis::areValuesEquivalent(rhs, bs))
        return true;
    }
    return false;
  };
  /// Helper lambda to check if size is bounded by block size
  auto isBoundedByBlock = [&](Value sz, Value bsVal) -> bool {
    if (!sz || !bsVal)
      return false;
    Value s = ValueAnalysis::stripClampOne(sz);
    Value bs = ValueAnalysis::stripClampOne(bsVal);
    if (ValueAnalysis::areValuesEquivalent(s, bs))
      return true;
    if (auto minOp = s.getDefiningOp<arith::MinUIOp>()) {
      Value lhs = ValueAnalysis::stripClampOne(minOp.getLhs());
      Value rhs = ValueAnalysis::stripClampOne(minOp.getRhs());
      if (ValueAnalysis::areValuesEquivalent(lhs, bs) ||
          ValueAnalysis::areValuesEquivalent(rhs, bs))
        return true;
    }
    return false;
  };
  /// Helper lambda to check static alignment to constant block size
  auto isAlignedToBlock = [&](Value offset,
                              const std::optional<int64_t> &bsConst) -> bool {
    if (!offset || !bsConst || *bsConst <= 0)
      return false;
    Value stripped = ValueAnalysis::stripNumericCasts(offset);
    if (auto c = ValueAnalysis::getConstantValue(stripped))
      return (*c % *bsConst) == 0;
    if (auto mul = stripped.getDefiningOp<arith::MulIOp>()) {
      Value lhs = ValueAnalysis::stripNumericCasts(mul.getLhs());
      Value rhs = ValueAnalysis::stripNumericCasts(mul.getRhs());
      if (auto lhsC = ValueAnalysis::getConstantValue(lhs))
        if ((*lhsC % *bsConst) == 0)
          return true;
      if (auto rhsC = ValueAnalysis::getConstantValue(rhs))
        if ((*rhsC % *bsConst) == 0)
          return true;
    }
    return false;
  };

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

    /// Clamp only when the slice starts in-range.
    /// For out-of-range starts (e.g., i-1 at the first row), emit an empty
    /// dependency slice (size=0) with a safe offset to avoid invalid DB
    /// indices.
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

    Value blockCount = builder.create<arith::SubIOp>(loc, endBlock, startBlock);
    blockCount = builder.create<arith::AddIOp>(loc, blockCount, one);

    if (startAboveMax) {
      startBlock =
          builder.create<arith::SelectOp>(loc, startAboveMax, zero, startBlock);
      blockCount =
          builder.create<arith::SelectOp>(loc, startAboveMax, zero, blockCount);
    }

    /// Check if single-block for this dimension
    auto constElemSz = ValueAnalysis::getConstantValue(elemSz);
    auto constElemSzUpper =
        constElemSz ? constElemSz : arts::extractBlockSizeFromHint(elemSz);
    auto constBs = arts::extractBlockSizeFromHint(bs);
    bool isAligned = isAlignedToBlock(elemOff, constBs);
    bool isSingleDim = false;
    if (auto constBlocks = ValueAnalysis::tryFoldConstantIndex(blockCount)) {
      if (*constBlocks == 1)
        isSingleDim = true;
    }
    if (!isSingleDim && constElemSzUpper && constBs) {
      if (*constElemSzUpper <= *constBs && isAligned)
        isSingleDim = true;
    }
    if (!isSingleDim) {
      bool dynAligned = isAlignedToBlockDynamic(elemOff, bs) ||
                        isAlignedToBlockDynamic(elemOff, elemSz);
      if (isBoundedByBlock(elemSz, bs) && (isAligned || dynAligned))
        isSingleDim = true;
    }
    /// Unknown/ambiguous sizes, assume not single block
    if (!isSingleDim)
      isSingleBlock = false;
    if (isSingleDim)
      blockCount = one;

    newOffsets.push_back(startBlock);
    newSizes.push_back(blockCount);
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
  bool allocSingleBlock =
      !plan.outerSizes.empty() && llvm::all_of(plan.outerSizes, [](Value v) {
        auto val = ValueAnalysis::tryFoldConstantIndex(v);
        return val && *val == 1;
      });

  /// Collect load/store users of this db_ref
  SmallVector<Operation *> refUsers(ref.getResult().getUsers().begin(),
                                    ref.getResult().getUsers().end());
  RemovalUtils removal;

  /// Build a block indexer with zero start blocks (global indexing).
  unsigned nPartDims = plan.numPartitionedDims();
  if (nPartDims == 0)
    nPartDims = 1;
  SmallVector<Value> startBlocks;
  startBlocks.reserve(nPartDims);
  Value zero = arts::createZeroIndex(builder, loc);
  for (unsigned d = 0; d < nPartDims; ++d)
    startBlocks.push_back(zero);

  PartitionInfo info;
  info.mode = PartitionMode::block;
  info.sizes.assign(plan.blockSizes.begin(), plan.blockSizes.end());
  if (!plan.partitionedDims.empty())
    info.partitionedDims.assign(plan.partitionedDims.begin(),
                                plan.partitionedDims.end());

  DbBlockIndexer indexer(info, startBlocks, plan.outerRank(), plan.innerRank(),
                         allocSingleBlock);

  bool hasLoadStoreUsers = false;
  for (Operation *user : refUsers) {
    auto access = DbUtils::getMemoryAccessInfo(user);
    if (!access)
      continue;
    hasLoadStoreUsers = true;

    OpBuilder::InsertionGuard ig(builder);
    builder.setInsertionPoint(user);
    Location userLoc = user->getLoc();

    ValueRange loadStoreIndices = access->indices;

    LocalizedIndices localized;
    if (!loadStoreIndices.empty()) {
      SmallVector<Value> indices(loadStoreIndices.begin(),
                                 loadStoreIndices.end());
      localized = indexer.localize(indices, builder, userLoc);
    } else {
      localized.dbRefIndices.push_back(zero);
      localized.memrefIndices.push_back(zero);
    }

    auto newRef = builder.create<DbRefOp>(userLoc, newElementType, newSource,
                                          localized.dbRefIndices);

    if (access->isRead()) {
      auto load = cast<memref::LoadOp>(user);
      auto newLoad = builder.create<memref::LoadOp>(userLoc, newRef.getResult(),
                                                    localized.memrefIndices);
      load.replaceAllUsesWith(newLoad.getResult());
    } else if (auto store = dyn_cast<memref::StoreOp>(user)) {
      builder.create<memref::StoreOp>(userLoc, store.getValue(),
                                      newRef.getResult(),
                                      localized.memrefIndices);
    }
    removal.markForRemoval(user);
  }

  /// For db_refs without load/store users, rewrite the db_ref indices only.
  if (!hasLoadStoreUsers && !ref.getResult().use_empty()) {
    OpBuilder::InsertionGuard ig(builder);
    builder.setInsertionPoint(ref);

    SmallVector<Value> refIndices(ref.getIndices().begin(),
                                  ref.getIndices().end());
    LocalizedIndices localized =
        refIndices.empty() ? LocalizedIndices()
                           : indexer.localize(refIndices, builder, loc);
    if (localized.dbRefIndices.empty())
      localized.dbRefIndices.push_back(zero);

    auto newRef = builder.create<DbRefOp>(loc, newElementType, newSource,
                                          localized.dbRefIndices);
    ref.replaceAllUsesWith(newRef.getResult());
  }

  /// Remove old load/store operations
  removal.removeAllMarked();
}

bool DbBlockRewriter::rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                                     Value startBlock, bool isSingleBlock) {
  ARTS_DEBUG("DbBlockRewriter::rebaseEdtUsers (isSingleBlock=" << isSingleBlock
                                                               << ")");

  auto [edt, blockArg] = getEdtBlockArgumentForAcquire(acquire);
  Value localView = blockArg ? Value(blockArg) : acquire.getPtr();
  if (!edt)
    edt = acquire->getParentOfType<EdtOp>();
  if (!localView || !edt)
    return false;

  /// Determine element type from DbRefOp users or block argument
  Type targetType;
  for (Operation *user : localView.getUsers())
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      targetType = ref.getResult().getType();
      break;
    }

  if (!targetType) {
    targetType = localView.getType();
    if (auto outer = dyn_cast<MemRefType>(targetType))
      if (auto inner = dyn_cast<MemRefType>(outer.getElementType()))
        targetType = inner;
  }
  if (!targetType || !isa<MemRefType>(targetType))
    return false;

  Type derivedType = targetType;
  if (auto dbAlloc =
          dyn_cast_or_null<DbAllocOp>(DbUtils::getUnderlyingDbAlloc(localView)))
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
  Value one = arts::createOneIndex(builder, loc);
  Value zero = arts::createZeroIndex(builder, loc);

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
  if (!plan.partitionedDims.empty())
    info.partitionedDims.assign(plan.partitionedDims.begin(),
                                plan.partitionedDims.end());
  /// Note: offsets are stored but startBlocks are passed separately
  /// because the division (offsets / sizes) requires a builder.

  /// Detect allocation-level single-block: all outer sizes are constant 1.
  /// This matches the verifier's coarse check exactly.
  bool allocSingleBlock =
      !plan.outerSizes.empty() && llvm::all_of(plan.outerSizes, [](Value v) {
        auto val = ValueAnalysis::tryFoldConstantIndex(v);
        return val && *val == 1;
      });

  /// Create N-D block indexer with PartitionInfo
  auto indexer = std::make_unique<DbBlockIndexer>(
      info, effectiveStartBlocks, plan.outerRank(), plan.innerRank(),
      allocSingleBlock, isSingleBlock, zero);

  SmallVector<Operation *> users(localView.getUsers().begin(),
                                 localView.getUsers().end());
  llvm::SetVector<Operation *> opsToRemove;
  bool rewritten = false;

  for (Operation *user : users) {
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      indexer->transformDbRefUsers(ref, localView, derivedType, builder,
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
