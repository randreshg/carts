///==========================================================================///
/// File: DbRewriter.cpp
///
/// Abstract base class for datablock allocation transformation.
/// Provides factory pattern for creating mode-specific rewriters.
///==========================================================================///

#include "arts/Transforms/Datablock/DbRewriter.h"
#include "arts/Analysis/ArtsHeuristics.h"
#include "arts/Transforms/Datablock/Block/DbBlockIndexer.h"
#include "arts/Transforms/Datablock/Block/DbBlockRewriter.h"
#include "arts/Transforms/Datablock/ElementWise/DbElementWiseIndexer.h"
#include "arts/Transforms/Datablock/ElementWise/DbElementWiseRewriter.h"
#include "arts/Transforms/Datablock/Stencil/DbStencilIndexer.h"
#include "arts/Transforms/Datablock/Stencil/DbStencilRewriter.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/RemovalUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <functional>

#define DEBUG_TYPE "db_transforms"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

/// DbRewritePlan constructor from PartitioningDecision
DbRewritePlan::DbRewritePlan(const PartitioningDecision &decision)
    : mode(decision.mode), outerRank(decision.outerRank),
      innerRank(decision.innerRank) {}

/// DbRewriter - Abstract Base Class

DbRewriter::DbRewriter(DbAllocOp oldAlloc, ValueRange newOuterSizes,
                       ValueRange newInnerSizes,
                       ArrayRef<DbRewriteAcquire> acquires,
                       const DbRewritePlan &plan)
    : oldAlloc(oldAlloc), newOuterSizes(newOuterSizes),
      newInnerSizes(newInnerSizes), acquires(acquires), plan(plan) {}

/// Factory Method - Creates appropriate subclass based on mode

std::unique_ptr<DbRewriter> DbRewriter::create(
    DbAllocOp oldAlloc, ValueRange newOuterSizes, ValueRange newInnerSizes,
    ArrayRef<DbRewriteAcquire> acquires, const DbRewritePlan &plan) {
  ARTS_DEBUG("DbRewriter::create mode=" << getPartitionModeName(plan.mode));

  switch (plan.mode) {
  case PartitionMode::fine_grained:
  case PartitionMode::coarse:
    return std::make_unique<DbElementWiseRewriter>(
        oldAlloc, newOuterSizes, newInnerSizes, acquires, plan);

  case PartitionMode::block:
    return std::make_unique<DbBlockRewriter>(oldAlloc, newOuterSizes,
                                             newInnerSizes, acquires, plan);

  case PartitionMode::stencil:
    return std::make_unique<DbStencilRewriter>(oldAlloc, newOuterSizes,
                                               newInnerSizes, acquires, plan);
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

  /// 1. Create new allocation with the given sizes
  PartitionMode partitionMode =
      getPartitionMode(oldAlloc.getOperation()).value_or(PartitionMode::coarse);
  auto newAlloc = builder.create<DbAllocOp>(
      loc, oldAlloc.getMode(), oldAlloc.getRoute(), oldAlloc.getAllocType(),
      oldAlloc.getDbMode(), oldAlloc.getElementType(), oldAlloc.getAddress(),
      newOuterSizes, newInnerSizes, partitionMode);

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
  return std::make_unique<DbElementWiseIndexer>(elemOffsets, outerRank,
                                                innerRank, oldElementSizes);
}

std::unique_ptr<DbIndexerBase>
DbRewriter::createBlockIndexer(ArrayRef<Value> blockSizes,
                               ArrayRef<Value> startBlocks, unsigned outerRank,
                               unsigned innerRank) {
  ARTS_DEBUG("  Block indexer: outerRank="
             << outerRank << ", innerRank=" << innerRank
             << ", nPartDims=" << blockSizes.size());
  return std::make_unique<DbBlockIndexer>(blockSizes, startBlocks, outerRank,
                                          innerRank);
}

std::unique_ptr<DbIndexerBase> DbRewriter::createStencilIndexer(
    const StencilInfo &info, Value blockSize, Value elemOffset,
    unsigned outerRank, unsigned innerRank, Value ownedArg, Value leftHaloArg,
    Value rightHaloArg, OpBuilder &builder, Location loc) {
  /// Create halo values from StencilInfo
  Value haloLeft = builder.create<arith::ConstantIndexOp>(loc, info.haloLeft);
  Value haloRight = builder.create<arith::ConstantIndexOp>(loc, info.haloRight);

  ARTS_DEBUG("  Stencil indexer: haloLeft=" << info.haloLeft << ", haloRight="
                                            << info.haloRight);

  return std::make_unique<DbStencilIndexer>(
      haloLeft, haloRight, blockSize, outerRank, innerRank, elemOffset,
      ownedArg, leftHaloArg, rightHaloArg);
}

std::unique_ptr<DbIndexerBase>
DbRewriter::createIndexer(const DbRewritePlan &plan, Value startBlock,
                          Value elemOffset, Value elemSize, unsigned outerRank,
                          unsigned innerRank, ValueRange oldElementSizes,
                          OpBuilder &builder, Location loc, Value ownedArg,
                          Value leftHaloArg, Value rightHaloArg) {

  ARTS_DEBUG(
      "DbRewriter::createIndexer mode=" << getPartitionModeName(plan.mode));

  switch (plan.mode) {
  case PartitionMode::fine_grained:
  case PartitionMode::coarse: {
    /// For backward compatibility, wrap single elemOffset in array
    SmallVector<Value> elemOffsets;
    if (elemOffset)
      elemOffsets.push_back(elemOffset);
    return createElementWiseIndexer(elemOffsets, outerRank, innerRank,
                                    oldElementSizes);
  }

  case PartitionMode::block: {
    /// Build blockSizes and startBlocks vectors
    /// For N-D partitioning, use plan.blockSizes; for 1D, wrap single value
    SmallVector<Value> blockSizes = plan.blockSizes;
    if (blockSizes.empty() && plan.blockSize)
      blockSizes.push_back(plan.blockSize);

    /// Compute startBlocks from elemOffset and blockSizes
    SmallVector<Value> startBlocks;
    if (startBlock)
      startBlocks.push_back(startBlock);
    else if (!blockSizes.empty()) {
      /// Create zero start block if no explicit startBlock provided
      startBlocks.resize(blockSizes.size());
    }

    return createBlockIndexer(blockSizes, startBlocks, outerRank, innerRank);
  }

  case PartitionMode::stencil:
    assert(plan.stencilInfo && "Stencil mode requires stencil info");
    return createStencilIndexer(*plan.stencilInfo, plan.blockSize, elemOffset,
                                outerRank, innerRank, ownedArg, leftHaloArg,
                                rightHaloArg, builder, loc);
  }

  ARTS_UNREACHABLE("Unknown PartitionMode");
}
