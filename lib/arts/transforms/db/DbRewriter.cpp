///==========================================================================///
/// File: DbRewriter.cpp
///
/// Abstract base class for datablock allocation transformation.
/// Provides factory pattern for creating mode-specific rewriters.
///==========================================================================///

#include "arts/transforms/db/DbRewriter.h"
#include "arts/transforms/db/block/DbBlockIndexer.h"
#include "arts/transforms/db/block/DbBlockRewriter.h"
#include "arts/transforms/db/elementwise/DbElementWiseIndexer.h"
#include "arts/transforms/db/elementwise/DbElementWiseRewriter.h"
#include "arts/transforms/db/stencil/DbStencilIndexer.h"
#include "arts/transforms/db/stencil/DbStencilRewriter.h"
#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/RemovalUtils.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <functional>

#define DEBUG_TYPE "db_transforms"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

/// DbRewriter - Abstract Base Class

DbRewriter::DbRewriter(DbAllocOp oldAlloc, ArrayRef<DbRewriteAcquire> acquires,
                       DbRewritePlan &plan)
    : oldAlloc(oldAlloc), acquires(acquires), plan(plan) {}

/// Factory Method - Creates appropriate subclass based on mode.
/// NOTE: Coarse and fine_grained layouts share the element-wise rewriter.
///       Coarse uses outerRank=0 (single-DB layout) while fine_grained uses
///       outerRank>0 for element-wise partitioning along selected dimensions.

std::unique_ptr<DbRewriter>
DbRewriter::create(DbAllocOp oldAlloc, ArrayRef<DbRewriteAcquire> acquires,
                   DbRewritePlan &plan) {
  ARTS_DEBUG("DbRewriter::create mode=" << getPartitionModeName(plan.mode));

  switch (plan.mode) {
  case PartitionMode::fine_grained:
  case PartitionMode::coarse:
    return std::make_unique<DbElementWiseRewriter>(oldAlloc, acquires, plan);

  case PartitionMode::block:
    return std::make_unique<DbBlockRewriter>(oldAlloc, acquires, plan);

  case PartitionMode::stencil:
    return std::make_unique<DbStencilRewriter>(oldAlloc, acquires, plan);
  }

  ARTS_UNREACHABLE("Unknown PartitionMode");
}

/// Template Method - Shared workflow for all modes

FailureOr<DbAllocOp> DbRewriter::apply(OpBuilder &builder) {
  ARTS_DEBUG("DbRewriter::apply - mode=" << getPartitionModeName(plan.mode)
                                         << ", acquires=" << acquires.size());

  if (!oldAlloc)
    return failure();

  Location loc = oldAlloc.getLoc();
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(oldAlloc);

  /// 1. Create new allocation with the given sizes from plan
  PartitionMode partitionMode = plan.mode;
  auto newAlloc = builder.create<DbAllocOp>(
      loc, oldAlloc.getMode(), oldAlloc.getRoute(), oldAlloc.getAllocType(),
      oldAlloc.getDbMode(), oldAlloc.getElementType(), oldAlloc.getAddress(),
      plan.outerSizes, plan.innerSizes, partitionMode);

  /// 2. Transfer metadata/attributes from old to new allocation
  transferAttributes(oldAlloc, newAlloc, {AttrNames::Operation::PartitionMode});

  /// 3. Rewrite all acquires with their partition info (virtual dispatch)
  for (const auto &info : acquires)
    transformAcquire(info, newAlloc, builder);

  /// 4. Collect and rewrite DbRefs outside of EDTs (virtual dispatch)
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
  collectDbRefs(oldAlloc.getPtr());

  for (DbRefOp ref : dbRefUsers) {
    transformDbRef(ref, newAlloc, builder);
    opsToRemove.insert(ref.getOperation());
  }

  /// 5. Replace all uses of old allocation with new
  oldAlloc.getGuid().replaceAllUsesWith(newAlloc.getGuid());
  oldAlloc.getPtr().replaceAllUsesWith(newAlloc.getPtr());
  if (oldAlloc.use_empty())
    opsToRemove.insert(oldAlloc.getOperation());

  /// 6. Clean up removed operations
  RemovalUtils removalMgr;
  for (Operation *op : opsToRemove)
    removalMgr.markForRemoval(op);
  removalMgr.removeAllMarked();

  return newAlloc;
}

/// Indexer Factory Methods - Used by subclasses for index localization
std::unique_ptr<DbIndexerBase>
DbRewriter::createElementWiseIndexer(ArrayRef<Value> elemOffsets,
                                     unsigned outerRank, unsigned innerRank,
                                     ValueRange oldElementSizes) {
  ARTS_DEBUG("  ElementWise indexer: outerRank="
             << outerRank << ", innerRank=" << innerRank
             << ", offsetDims=" << elemOffsets.size());

  /// Build PartitionInfo from elemOffsets
  PartitionInfo info;
  info.mode = PartitionMode::fine_grained;
  info.indices.assign(elemOffsets.begin(), elemOffsets.end());

  return std::make_unique<DbElementWiseIndexer>(info, outerRank, innerRank,
                                                oldElementSizes);
}

std::unique_ptr<DbIndexerBase> DbRewriter::createBlockIndexer(
    ArrayRef<Value> blockSizes, ArrayRef<Value> startBlocks, unsigned outerRank,
    unsigned innerRank, ArrayRef<unsigned> partitionedDims) {
  ARTS_DEBUG("  Block indexer: outerRank="
             << outerRank << ", innerRank=" << innerRank
             << ", nPartDims=" << blockSizes.size());

  /// Build PartitionInfo from blockSizes
  PartitionInfo info;
  info.mode = PartitionMode::block;
  info.sizes.assign(blockSizes.begin(), blockSizes.end());
  if (!partitionedDims.empty())
    info.partitionedDims.assign(partitionedDims.begin(), partitionedDims.end());

  return std::make_unique<DbBlockIndexer>(info, startBlocks, outerRank,
                                          innerRank);
}

std::unique_ptr<DbIndexerBase> DbRewriter::createStencilIndexer(
    const StencilInfo &stencilInfo, Value blockSize, Value baseOffset,
    unsigned outerRank, unsigned innerRank, ArrayRef<unsigned> partitionedDims,
    Value ownedArg, Value leftHaloArg, Value rightHaloArg, OpBuilder &builder,
    Location loc) {
  /// Create halo values from StencilInfo
  Value haloLeft =
      arts::createConstantIndex(builder, loc, stencilInfo.haloLeft);
  Value haloRight =
      arts::createConstantIndex(builder, loc, stencilInfo.haloRight);

  ARTS_DEBUG("  Stencil indexer: haloLeft=" << stencilInfo.haloLeft
                                            << ", haloRight="
                                            << stencilInfo.haloRight);

  /// Build PartitionInfo with base offset semantics
  PartitionInfo info;
  info.mode = PartitionMode::stencil;
  if (baseOffset)
    info.offsets.push_back(baseOffset);
  if (blockSize)
    info.sizes.push_back(blockSize);
  if (!partitionedDims.empty())
    info.partitionedDims.assign(partitionedDims.begin(), partitionedDims.end());

  return std::make_unique<DbStencilIndexer>(info, haloLeft, haloRight,
                                            outerRank, innerRank, ownedArg,
                                            leftHaloArg, rightHaloArg);
}
