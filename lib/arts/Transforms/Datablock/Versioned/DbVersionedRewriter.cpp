///==========================================================================///
/// File: DbVersionedRewriter.cpp
///
/// Versioned partitioning rewriter for DbCopy/DbSync.
/// Disabled currently; kept for future enablement.
///==========================================================================///

#include "arts/Transforms/Datablock/Versioned/DbVersionedRewriter.h"
#include "arts/Transforms/Datablock/ElementWise/DbElementWiseIndexer.h"
#include "arts/Utils/EdtUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "llvm/ADT/SetVector.h"

using namespace mlir;
using namespace mlir::arts;

static bool rebaseAcquireToElementWiseCopy(DbAcquireOp acquire,
                                           Value elemOffset, Value elemSize,
                                           ValueRange oldElementSizes,
                                           OpBuilder &builder) {
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

  unsigned outerRank = 0;
  unsigned innerRank = 0;
  if (auto outer = blockArg.getType().dyn_cast<MemRefType>()) {
    outerRank = outer.getRank();
    if (auto inner = outer.getElementType().dyn_cast<MemRefType>())
      innerRank = inner.getRank();
  }

  DbElementWiseIndexer indexer(elemOffset, elemSize, outerRank, innerRank,
                               oldElementSizes);

  SmallVector<Operation *> users(blockArg.getUsers().begin(),
                                 blockArg.getUsers().end());
  llvm::SetVector<Operation *> opsToRemove;

  for (Operation *user : users) {
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      indexer.transformDbRefUsers(ref, blockArg, targetType, builder,
                                  opsToRemove);
    }
  }

  for (Operation *op : opsToRemove)
    if (op->use_empty())
      op->erase();

  return true;
}

void DbVersionedRewriter::apply(
    DbAllocOp allocOp, ArrayRef<VersionedAcquireInfo> versionedAcquires,
    ArrayRef<Value> oldElementSizes) {
  if (!allocOp || versionedAcquires.empty())
    return;

  /// Only handle rank-1 copies for now.
  if (allocOp.getElementSizes().size() != 1 || allocOp.getSizes().size() != 1)
    return;

  OpBuilder builder(allocOp);
  builder.setInsertionPointAfter(allocOp);

  auto copyOp = builder.create<DbCopyOp>(
      allocOp.getLoc(), allocOp.getGuid().getType(), allocOp.getPtr().getType(),
      allocOp.getPtr(), PartitionMode::fine_grained);

  Value zero = builder.create<arith::ConstantIndexOp>(allocOp.getLoc(), 0);
  SmallVector<Value> fullOffsets;
  SmallVector<Value> fullSizes;
  if (!oldElementSizes.empty()) {
    fullOffsets.push_back(zero);
    fullSizes.push_back(oldElementSizes.front());
  } else if (!allocOp.getSizes().empty() &&
             !allocOp.getElementSizes().empty()) {
    Value fullSize;
    if (allocOp.getSizes().size() == 1 &&
        allocOp.getElementSizes().size() == 1) {
      fullSize = builder.create<arith::MulIOp>(
          allocOp.getLoc(), allocOp.getSizes().front(),
          allocOp.getElementSizes().front());
    } else {
      fullSize = allocOp.getElementSizes().front();
    }
    fullOffsets.push_back(zero);
    fullSizes.push_back(fullSize);
  }

  for (const auto &info : versionedAcquires) {
    DbAcquireOp acqOp = info.acquire;
    if (!acqOp)
      continue;

    builder.setInsertionPoint(acqOp);

    /// Use chunk hints from VersionedAcquireInfo or fall back to offsets/sizes
    SmallVector<Value> offsets, sizes;
    if (info.chunkIndex)
      offsets.push_back(info.chunkIndex);
    else
      offsets.assign(acqOp.getOffsets().begin(), acqOp.getOffsets().end());

    if (info.chunkSize)
      sizes.push_back(info.chunkSize);
    else
      sizes.assign(acqOp.getSizes().begin(), acqOp.getSizes().end());

    SmallVector<Value> syncOffsets =
        fullOffsets.empty() ? offsets : fullOffsets;
    SmallVector<Value> syncSizes = fullSizes.empty() ? sizes : fullSizes;
    bool fullSync = !fullOffsets.empty();
    if (fullOffsets.empty())
      fullSync = syncOffsets.empty();
    builder.create<DbSyncOp>(acqOp.getLoc(), copyOp.getPtr(), allocOp.getPtr(),
                             ValueRange(syncOffsets), ValueRange(syncSizes),
                             fullSync);

    acqOp.getSourceGuidMutable().assign(copyOp.getGuid());
    acqOp.getSourcePtrMutable().assign(copyOp.getPtr());

    if (!fullOffsets.empty())
      acqOp.getOffsetsMutable().assign(fullOffsets);
    if (!fullSizes.empty())
      acqOp.getSizesMutable().assign(fullSizes);

    /// Clear chunk hints since we're using versioned copy
    acqOp.getChunkIndexMutable().clear();
    acqOp.getChunkSizeMutable().clear();

    MemRefType newPtrType = copyOp.getPtr().getType().cast<MemRefType>();
    Type oldAcqPtrType = acqOp.getPtr().getType();
    if (oldAcqPtrType != newPtrType) {
      acqOp.getPtr().setType(newPtrType);

      auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acqOp);
      if (blockArg && blockArg.getType() != newPtrType)
        blockArg.setType(newPtrType);
    }

    if (!fullOffsets.empty() && !fullSizes.empty())
      rebaseAcquireToElementWiseCopy(acqOp, fullOffsets.front(),
                                     fullSizes.front(), oldElementSizes,
                                     builder);
  }
}
