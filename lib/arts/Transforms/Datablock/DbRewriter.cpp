///==========================================================================///
/// File: DbRewriter.cpp
///
/// Abstract base class for datablock allocation transformation.
/// Provides factory pattern for creating mode-specific rewriters.
///==========================================================================///

#include "arts/Transforms/Datablock/DbRewriter.h"
#include "arts/Analysis/HeuristicsConfig.h"
#include "arts/Transforms/Datablock/Chunked/DbChunkedIndexer.h"
#include "arts/Transforms/Datablock/Chunked/DbChunkedRewriter.h"
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

///===----------------------------------------------------------------------===///
/// DbRewritePlan constructor from PartitioningDecision
///===----------------------------------------------------------------------===///
DbRewritePlan::DbRewritePlan(const PartitioningDecision &decision)
    : mode(decision.mode), outerRank(decision.outerRank),
      innerRank(decision.innerRank) {}

///===----------------------------------------------------------------------===///
/// DbRewriter - Abstract Base Class
///===----------------------------------------------------------------------===///

DbRewriter::DbRewriter(DbAllocOp oldAlloc, ValueRange newOuterSizes,
                       ValueRange newInnerSizes,
                       ArrayRef<DbRewriteAcquire> acquires,
                       const DbRewritePlan &plan)
    : oldAlloc(oldAlloc), newOuterSizes(newOuterSizes),
      newInnerSizes(newInnerSizes), acquires(acquires), plan(plan) {}

///===----------------------------------------------------------------------===///
/// Factory Method - Creates appropriate subclass based on mode
///===----------------------------------------------------------------------===///

std::unique_ptr<DbRewriter> DbRewriter::create(
    DbAllocOp oldAlloc, ValueRange newOuterSizes, ValueRange newInnerSizes,
    ArrayRef<DbRewriteAcquire> acquires, const DbRewritePlan &plan) {
  ARTS_DEBUG("DbRewriter::create mode=" << getRewriterModeName(plan.mode));

  switch (plan.mode) {
  case RewriterMode::ElementWise:
    return std::make_unique<DbElementWiseRewriter>(
        oldAlloc, newOuterSizes, newInnerSizes, acquires, plan);

  case RewriterMode::Chunked:
    return std::make_unique<DbChunkedRewriter>(oldAlloc, newOuterSizes,
                                               newInnerSizes, acquires, plan);

  case RewriterMode::Stencil:
    return std::make_unique<DbStencilRewriter>(oldAlloc, newOuterSizes,
                                               newInnerSizes, acquires, plan);
  }

  ARTS_UNREACHABLE("Unknown RewriterMode");
}

///===----------------------------------------------------------------------===///
/// Template Method - Shared workflow for all modes
///===----------------------------------------------------------------------===///

FailureOr<DbAllocOp> DbRewriter::apply(OpBuilder &builder) {
  ARTS_DEBUG("DbRewriter::apply - mode=" << getRewriterModeName(plan.mode)
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
  transferAttributes(oldAlloc, newAlloc, {"arts.partition"});

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

///===----------------------------------------------------------------------===///
/// Indexer Factory Methods - Used by subclasses for index localization
///===----------------------------------------------------------------------===///
std::unique_ptr<DbIndexerBase>
DbRewriter::createElementWiseIndexer(Value elemOffset, Value elemSize,
                                     unsigned outerRank, unsigned innerRank,
                                     ValueRange oldElementSizes) {
  ARTS_DEBUG("  ElementWise indexer: outerRank=" << outerRank << ", innerRank="
                                                 << innerRank);
  return std::make_unique<DbElementWiseIndexer>(elemOffset, elemSize, outerRank,
                                                innerRank, oldElementSizes);
}

std::unique_ptr<DbIndexerBase>
DbRewriter::createChunkedIndexer(Value chunkSize, Value startChunk,
                                 Value elemOffset, unsigned outerRank,
                                 unsigned innerRank) {
  ARTS_DEBUG("  Chunked indexer: outerRank=" << outerRank
                                             << ", innerRank=" << innerRank);
  return std::make_unique<DbChunkedIndexer>(chunkSize, startChunk, elemOffset,
                                            outerRank, innerRank);
}

std::unique_ptr<DbIndexerBase> DbRewriter::createStencilIndexer(
    const StencilInfo &info, Value chunkSize, Value elemOffset,
    unsigned outerRank, unsigned innerRank, Value ownedArg, Value leftHaloArg,
    Value rightHaloArg, OpBuilder &builder, Location loc) {
  /// Create halo values from StencilInfo
  Value haloLeft = builder.create<arith::ConstantIndexOp>(loc, info.haloLeft);
  Value haloRight = builder.create<arith::ConstantIndexOp>(loc, info.haloRight);

  ARTS_DEBUG("  Stencil indexer: haloLeft=" << info.haloLeft << ", haloRight="
                                            << info.haloRight);

  return std::make_unique<DbStencilIndexer>(
      haloLeft, haloRight, chunkSize, outerRank, innerRank, elemOffset,
      ownedArg, leftHaloArg, rightHaloArg);
}

std::unique_ptr<DbIndexerBase>
DbRewriter::createIndexer(const DbRewritePlan &plan, Value startChunk,
                          Value elemOffset, Value elemSize, unsigned outerRank,
                          unsigned innerRank, ValueRange oldElementSizes,
                          OpBuilder &builder, Location loc, Value ownedArg,
                          Value leftHaloArg, Value rightHaloArg) {

  ARTS_DEBUG(
      "DbRewriter::createIndexer mode=" << getRewriterModeName(plan.mode));

  switch (plan.mode) {
  case RewriterMode::ElementWise:
    return createElementWiseIndexer(elemOffset, elemSize, outerRank, innerRank,
                                    oldElementSizes);

  case RewriterMode::Chunked:
    return createChunkedIndexer(plan.chunkSize, startChunk, elemOffset,
                                outerRank, innerRank);

  case RewriterMode::Stencil:
    assert(plan.stencilInfo && "Stencil mode requires stencil info");
    return createStencilIndexer(*plan.stencilInfo, plan.chunkSize, elemOffset,
                                outerRank, innerRank, ownedArg, leftHaloArg,
                                rightHaloArg, builder, loc);
  }

  ARTS_UNREACHABLE("Unknown RewriterMode");
}
