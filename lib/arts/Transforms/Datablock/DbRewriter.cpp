///==========================================================================///
/// File: DbRewriter.cpp
///
/// Abstract base class for datablock allocation transformation.
/// Provides factory pattern for creating mode-specific rewriters.
///==========================================================================///

#include "arts/Transforms/Datablock/DbRewriter.h"
#include "arts/Analysis/Heuristics/PartitioningHeuristics.h"
#include "arts/Transforms/Datablock/Chunked/DbChunkedIndexer.h"
#include "arts/Transforms/Datablock/Chunked/DbChunkedRewriter.h"
#include "arts/Transforms/Datablock/ElementWise/DbElementWiseIndexer.h"
#include "arts/Transforms/Datablock/ElementWise/DbElementWiseRewriter.h"
#include "arts/Transforms/Datablock/Stencil/DbStencilIndexer.h"
#include "arts/Transforms/Datablock/Stencil/DbStencilRewriter.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
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
    : oldAlloc_(oldAlloc), newOuterSizes_(newOuterSizes),
      newInnerSizes_(newInnerSizes), acquires_(acquires), plan_(plan) {}

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

  llvm_unreachable("Unknown RewriterMode");
}

///===----------------------------------------------------------------------===///
/// Template Method - Shared workflow for all modes
///===----------------------------------------------------------------------===///

FailureOr<DbAllocOp> DbRewriter::apply(OpBuilder &builder) {
  ARTS_DEBUG("DbRewriter::apply - mode=" << getRewriterModeName(plan_.mode)
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

  /// 3. Rewrite all acquires with their partition info (virtual dispatch)
  for (const auto &info : acquires_)
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
  collectDbRefs(oldAlloc_.getPtr());

  for (DbRefOp ref : dbRefUsers) {
    transformDbRef(ref, newAlloc, builder);
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

  llvm_unreachable("Unknown RewriterMode");
}
