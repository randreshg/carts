///==========================================================================///
/// File: Edt.cpp
///==========================================================================///

/// Dialects
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Support/LLVM.h"
/// Arts
#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(edt);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
// Pass Implementation
///===----------------------------------------------------------------------===///
namespace {
struct EdtPass : public arts::EdtBase<EdtPass> {
  EdtPass(ArtsAnalysisManager *AM, bool runAnalysis) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
    this->runAnalysis = runAnalysis;
  }
  void runOnOperation() override;
  bool convertParallelIntoSingle(EdtOp &op);

  /// Utility for creating EDT with merged dependencies and region transfer
  EdtOp createEdtWithMergedDepsAndRegion(OpBuilder &builder, Location loc,
                                         arts::EdtType newType, EdtOp sourceOp,
                                         ArrayRef<Value> additionalDeps = {});

  /// Remove barriers immediately before and after a single EDT
  void removeBarriersAroundSingleEdt(EdtOp parallelOp, EdtOp singleOp);

  /// Convert parallel EDT with inner acquires to sync EDT by rewiring outer
  /// acquires
  bool convertParallelWithAcquiresToSync(EdtOp parallelOp, EdtOp singleOp,
                                         ArrayRef<DbAcquireOp> innerAcquires,
                                         ArrayRef<DbReleaseOp> innerReleases);

  bool processSingleEdts();
  bool processParallelEdts();
  bool processSyncTaskEdts();
  bool removeBarriers();

  /// Graph-driven cleanups
  bool removeRedundantBarriersWithGraphs(func::FuncOp func,
                                         arts::EdtGraph &graph);

private:
  ModuleOp module;
  ArtsAnalysisManager *AM = nullptr;
  SetVector<Operation *> opsToRemove;
};
} // namespace

void EdtPass::runOnOperation() {
  module = getOperation();
  ARTS_INFO_HEADER(EdtPass);
  ARTS_DEBUG_REGION(module.dump(););

  if (runAnalysis) {
    ARTS_INFO("Running EDT pass with analysis");
    // removeBarriers();
  } else {
    ARTS_INFO("Running EDT pass without analysis");
    processSingleEdts();
    processParallelEdts();
    processSyncTaskEdts();
  }

  /// Remove ops marked for removal
  ARTS_DEBUG("Ops to remove: " << opsToRemove.size());
  RemovalUtils removalMgr;
  for (Operation *op : opsToRemove) {
    ARTS_DEBUG("  Marking for removal: " << op->getName().getStringRef());
    for (Value result : op->getResults()) {
      ARTS_DEBUG("    Result has "
                 << std::distance(result.use_begin(), result.use_end())
                 << " uses");
    }
    removalMgr.markForRemoval(op);
  }
  ARTS_DEBUG("Calling removeAllMarked...");
  removalMgr.removeAllMarked(module, /*recursive=*/true);
  ARTS_DEBUG("removeAllMarked completed");

  ARTS_INFO_FOOTER(EdtPass);
  ARTS_DEBUG_REGION(module.dump(););
}

/// Remove unconditional barriers that provide no additional ordering beyond
/// computed EDT dependencies (graph-informed pruning).
bool EdtPass::removeBarriers() {
  bool changed = false;
  module.walk([&](func::FuncOp func) {
    auto &edtGraph = AM->getEdtGraph(func);
    if (edtGraph.size() == 0)
      return;

    changed |= removeRedundantBarriersWithGraphs(func, edtGraph);
  });
  return changed;
}

bool EdtPass::processSingleEdts() {
  /// Find all single EDTs and remove barriers around them since single EDTs
  /// have implicit barrier semantics
  SmallVector<EdtOp, 8> singleOps;
  module.walk([&](EdtOp edt) {
    if (edt.getType() == arts::EdtType::single)
      singleOps.push_back(edt);
  });

  for (EdtOp singleEdt : singleOps) {
    /// Find parent parallel EDT if it exists
    EdtOp parallelOp = singleEdt->getParentOfType<EdtOp>();
    if (parallelOp && parallelOp.getType() == arts::EdtType::parallel)
      removeBarriersAroundSingleEdt(parallelOp, singleEdt);
  }

  return !singleOps.empty();
}

bool EdtPass::processParallelEdts() {
  /// Gather all parallel-EDT ops in the module.
  SmallVector<EdtOp, 8> parallelOps;
  module.walk([&](EdtOp edt) {
    if (edt.getType() == arts::EdtType::parallel)
      parallelOps.push_back(edt);
  });

  /// Try to convert parallel EDTs into single EDTs if they contain exactly one
  /// child.
  for (EdtOp op : parallelOps)
    convertParallelIntoSingle(op);
  return true;
}

/// Converts a parallel EDT region into a sync-task EDT when it contains a
/// single inner `arts.edt` and no other ops beyond terminators/barriers.
bool EdtPass::convertParallelIntoSingle(EdtOp &op) {
  uint32_t numOps = 0;
  EdtOp singleOp = nullptr;

  /// Track inner acquires and releases for rewiring
  SmallVector<DbAcquireOp, 4> innerAcquires;
  SmallVector<DbReleaseOp, 4> innerReleases;

  for (auto &block : op.getRegion()) {
    for (auto &inst : block) {
      ++numOps;
      if (auto edt = dyn_cast<arts::EdtOp>(&inst)) {
        if (edt.getType() == arts::EdtType::single) {
          if (singleOp)
            ARTS_UNREACHABLE(
                "Multiple single ops in parallel op not supported");
          singleOp = edt;
        } else
          return false;

      } else if (isa<arts::YieldOp>(&inst) || isa<arts::BarrierOp>(&inst)) {
        continue;
      } else if (auto acqOp = dyn_cast<DbAcquireOp>(&inst)) {
        innerAcquires.push_back(acqOp);
      } else if (auto relOp = dyn_cast<DbReleaseOp>(&inst)) {
        innerReleases.push_back(relOp);
      } else {
        /// Any other operation means we keep parallel structure
        return false;
      }
    }
  }

  /// Only convert to sync if it's JUST a single EDT with barriers/yield
  if (!singleOp || numOps < 3)
    return false;

  /// If we have inner acquires, use the enhanced conversion path
  if (!innerAcquires.empty()) {
    return convertParallelWithAcquiresToSync(op, singleOp, innerAcquires,
                                             innerReleases);
  }

  /// Original path for simple case (no inner acquires)
  OpBuilder builder(op);
  SmallVector<Value> parallelDeps(op.getDependencies().begin(),
                                  op.getDependencies().end());
  auto newEdt = createEdtWithMergedDepsAndRegion(
      builder, op.getLoc(), arts::EdtType::sync, singleOp, parallelDeps);

  /// Replace the parallel EDT with the new EDT and erase the single EDT
  op->replaceAllUsesWith(newEdt);
  singleOp->erase();

  /// Mark the parallel EDT for removal
  opsToRemove.insert(op);

  ARTS_INFO("Converted parallel EDT into sync EDT");
  return true;
}

/// Convert parallel EDT with inner acquires to sync EDT by rewiring outer
/// acquires to use the correct mode and feeding them directly to the sync EDT.
bool EdtPass::convertParallelWithAcquiresToSync(
    EdtOp parallelOp, EdtOp singleOp, ArrayRef<DbAcquireOp> innerAcquires,
    ArrayRef<DbReleaseOp> innerReleases) {

  Location loc = parallelOp.getLoc();
  Block &parallelBlock = parallelOp.getRegion().front();
  ValueRange parallelDeps = parallelOp.getDependencies();

  ARTS_DEBUG("Converting parallel with " << innerAcquires.size()
                                         << " inner acquires to sync");

  /// Step 1: Build mapping from inner acquire source → outer acquire
  /// Inner acquire sources from block arg, which maps to parallel's dependency
  DenseMap<DbAcquireOp, DbAcquireOp> innerToOuterAcquire;
  DenseSet<DbAcquireOp> usedOuterAcquires;

  for (DbAcquireOp innerAcq : innerAcquires) {
    /// Inner acquire's source should be a block argument
    Value srcPtr = innerAcq.getSourcePtr();
    auto blockArg = dyn_cast<BlockArgument>(srcPtr);
    if (!blockArg || blockArg.getOwner() != &parallelBlock) {
      ARTS_DEBUG("Inner acquire source is not a parallel block arg, aborting");
      return false;
    }

    /// Map block arg index to parallel's dependency (outer acquire)
    unsigned argIdx = blockArg.getArgNumber();
    if (argIdx >= parallelDeps.size()) {
      ARTS_DEBUG("Block arg index out of range, aborting");
      return false;
    }

    Value outerDep = parallelDeps[argIdx];
    auto outerAcq = outerDep.getDefiningOp<DbAcquireOp>();
    if (!outerAcq) {
      ARTS_DEBUG("Parallel dependency is not from DbAcquireOp, aborting");
      return false;
    }

    innerToOuterAcquire[innerAcq] = outerAcq;
    usedOuterAcquires.insert(outerAcq);
    ARTS_DEBUG("  Mapped inner acquire to outer acquire");
  }

  /// Step 2: For each inner acquire used by single, rewire outer acquire
  DenseMap<Value, Value> oldPtrToNewPtr;
  SmallVector<Value> syncDeps;

  for (Value singleDep : singleOp.getDependencies()) {
    /// Find which inner acquire produced this dependency
    DbAcquireOp innerAcq = singleDep.getDefiningOp<DbAcquireOp>();
    if (!innerAcq) {
      ARTS_DEBUG("Single dependency not from acquire, aborting");
      return false;
    }

    auto it = innerToOuterAcquire.find(innerAcq);
    if (it == innerToOuterAcquire.end()) {
      ARTS_DEBUG("Inner acquire not in map, aborting");
      return false;
    }

    DbAcquireOp outerAcq = it->second;

    /// Create replacement outer acquire with inner's mode
    OpBuilder builder(outerAcq);
    auto newAcq = builder.create<DbAcquireOp>(
        outerAcq.getLoc(), innerAcq.getMode(), outerAcq.getSourceGuid(),
        outerAcq.getSourcePtr(), outerAcq.getPtr().getType(),
        SmallVector<Value>(outerAcq.getIndices()),
        SmallVector<Value>(outerAcq.getOffsets()),
        SmallVector<Value>(outerAcq.getSizes()));

    /// Copy attributes from outer acquire
    if (outerAcq->hasAttr("arts.twin_diff"))
      newAcq->setAttr("arts.twin_diff", outerAcq->getAttr("arts.twin_diff"));

    /// Track the mapping and dependency
    oldPtrToNewPtr[outerAcq.getPtr()] = newAcq.getPtr();
    syncDeps.push_back(newAcq.getPtr());

    /// Replace uses of old outer acquire
    /// Note: We don't erase or mark the old outer acquire for removal.
    /// DCE will clean it up since all its uses have been replaced.
    outerAcq.getPtr().replaceAllUsesWith(newAcq.getPtr());
    outerAcq.getGuid().replaceAllUsesWith(newAcq.getGuid());

    ARTS_DEBUG("  Rewired outer acquire with mode " << (int)innerAcq.getMode());
  }

  /// Step 3: Create sync EDT with rewired dependencies
  OpBuilder builder(parallelOp);
  auto syncEdt =
      builder.create<EdtOp>(loc, EdtType::sync, singleOp.getConcurrency());

  /// Setup region and block arguments
  Region &syncRegion = syncEdt.getRegion();
  if (syncRegion.empty())
    syncRegion.push_back(new Block());
  Block &syncBody = syncRegion.front();

  /// Build mapping from single's block args to sync's block args
  IRMapping argMapper;
  Block &singleBody = singleOp.getRegion().front();

  for (auto [idx, dep] : llvm::enumerate(syncDeps)) {
    BlockArgument newArg = syncBody.addArgument(dep.getType(), loc);
    if (idx < singleBody.getNumArguments()) {
      argMapper.map(singleBody.getArgument(idx), newArg);
    }
  }

  syncEdt.setDependencies(syncDeps);

  /// Step 4: Clone single EDT body into sync EDT
  builder.setInsertionPointToEnd(&syncBody);
  for (Operation &bodyOp : singleBody.without_terminator()) {
    builder.clone(bodyOp, argMapper);
  }

  /// Add yield terminator if needed
  if (syncBody.empty() || !syncBody.back().hasTrait<OpTrait::IsTerminator>())
    builder.create<YieldOp>(loc);

  /// Step 5: Clean up old operations
  /// First erase the single EDT (its operands reference inner acquire results)
  singleOp->erase();

  /// Erase inner releases (they use block args but produce no results)
  for (DbReleaseOp rel : innerReleases)
    rel->erase();

  /// Erase inner acquires (their results are now unused after single removal)
  for (DbAcquireOp acq : innerAcquires)
    acq->erase();

  /// Debug: Check what's left in the parallel and block arg uses
  ARTS_DEBUG("Remaining ops in parallel before erase:");
  for (Operation &op : parallelOp.getRegion().front()) {
    ARTS_DEBUG("  - " << op.getName().getStringRef());
  }
  ARTS_DEBUG("Block arg uses:");
  for (BlockArgument arg : parallelOp.getRegion().front().getArguments()) {
    ARTS_DEBUG("  - arg has " << std::distance(arg.use_begin(), arg.use_end())
                              << " uses");
  }

  /// Drop all references in the parallel region and erase it
  ARTS_DEBUG("About to drop references and erase parallel...");
  parallelOp.getRegion().dropAllReferences();
  ARTS_DEBUG("References dropped, now erasing...");
  parallelOp->erase();
  ARTS_DEBUG("Parallel erased successfully");

  ARTS_INFO("Converted parallel EDT with inner acquires into sync EDT");
  return true;
}

/// Remove barriers immediately before and after a single EDT within a parallel
/// region. Since arts.edt<single> has implicit barrier semantics, explicit
/// barriers around it are redundant and cause issues with the CreateEpochs
/// pass.
void EdtPass::removeBarriersAroundSingleEdt(EdtOp parallelOp, EdtOp singleOp) {
  Block *parentBlock = singleOp->getBlock();
  if (!parentBlock)
    return;

  /// Find and remove barrier immediately before the single EDT
  Operation *prevOp = singleOp->getPrevNode();
  if (prevOp && isa<arts::BarrierOp>(prevOp))
    opsToRemove.insert(prevOp);

  /// Find and remove barrier immediately after the single EDT
  Operation *nextOp = singleOp->getNextNode();
  if (nextOp && isa<arts::BarrierOp>(nextOp))
    opsToRemove.insert(nextOp);
}

/// Utility function to create a new EDT operation with merged dependencies
/// and transfer the region from the source operation.
EdtOp EdtPass::createEdtWithMergedDepsAndRegion(
    OpBuilder &builder, Location loc, arts::EdtType newType, EdtOp sourceOp,
    ArrayRef<Value> additionalDeps) {

  /// Collect all dependencies from source operation
  SetVector<Value> allDeps;
  for (Value dep : sourceOp.getDependencies())
    allDeps.insert(dep);

  /// Add any additional dependencies
  for (Value dep : additionalDeps)
    allDeps.insert(dep);

  /// Create new EDT operation with merged dependencies and intranode
  /// concurrency. Create with empty deps first, then set them after adding
  /// block args.
  auto newEdt =
      builder.create<arts::EdtOp>(loc, newType, EdtConcurrency::intranode);

  /// Setup block and block arguments
  Region &newRegion = newEdt.getRegion();
  if (newRegion.empty())
    newRegion.push_back(new Block());
  Block &newBody = newRegion.front();

  /// Add block arguments for each dependency and build mapping from
  /// source block args to new block args
  IRMapping argMapper;
  Block &sourceBody = sourceOp.getRegion().front();
  SmallVector<Value> depVec;

  for (auto [idx, dep] : llvm::enumerate(allDeps.getArrayRef())) {
    BlockArgument newArg = newBody.addArgument(dep.getType(), loc);
    depVec.push_back(dep);

    /// Map source's block arg to new block arg if this dep came from source
    if (idx < sourceOp.getDependencies().size() &&
        idx < sourceBody.getNumArguments()) {
      argMapper.map(sourceBody.getArgument(idx), newArg);
    }
  }

  /// Set dependencies on the new EDT
  newEdt.setDependencies(depVec);

  /// Clone operations from source region to new region with argument remapping
  builder.setInsertionPointToEnd(&newBody);
  for (Operation &op : sourceBody.without_terminator()) {
    builder.clone(op, argMapper);
  }

  /// Add yield terminator if needed
  if (newBody.empty() || !newBody.back().hasTrait<OpTrait::IsTerminator>())
    builder.create<YieldOp>(loc);

  return newEdt;
}

bool EdtPass::processSyncTaskEdts() {
  /// If the given single EdtOp is not nested within another EdtOp (i.e., is
  /// top-level), and is marked as sync, embed its region's contents in an
  /// arts::EpochOp. This effectively assigns the work to the master thread,
  /// avoiding unnecessary signal/sync overhead. The EdtOp itself is erased
  /// after its body is moved.
  auto convertToEpoch = [](EdtOp &op) -> bool {
    OpBuilder builder(op);
    /// If the op is not top-level, return false.
    if (op->getParentOfType<EdtOp>())
      return false;

    /// If the sync EDT has dependencies, don't convert - the EDT context is
    /// needed for proper acquire/release semantics.
    if (!op.getDependencies().empty())
      return false;

    /// Create an arts::EpochOp and its block
    auto loc = op.getLoc();
    auto epochOp = builder.create<arts::EpochOp>(loc);
    auto &epochBlock = epochOp.getRegion().emplaceBlock();
    builder.setInsertionPointToEnd(&epochBlock);
    builder.create<arts::YieldOp>(loc);

    /// Move all operations except the terminator from the EdtOp's region to the
    /// epoch block
    Block *edtBody = &op.getRegion().front();

    /// Replace block argument uses with actual dependency values before moving.
    /// This is necessary because the block arguments will be destroyed when
    /// the EDT is erased.
    ValueRange deps = op.getDependencies();
    for (auto [idx, blockArg] : llvm::enumerate(edtBody->getArguments())) {
      if (idx < deps.size()) {
        blockArg.replaceAllUsesWith(deps[idx]);
      }
    }

    /// Collect operations to move before moving them
    SmallVector<Operation *, 8> opsToMove;
    for (Operation &childOp : edtBody->without_terminator())
      opsToMove.push_back(&childOp);

    /// Move all operations to the epoch block
    for (Operation *childOp : opsToMove)
      childOp->moveBefore(epochBlock.getTerminator());

    /// Erase the now-empty EdtOp
    op.erase();

    builder.setInsertionPointAfter(epochOp);
    return true;
  };

  /// Collect all sync task EDTs
  SmallVector<EdtOp, 8> syncTaskOps;
  module.walk([&](EdtOp edt) {
    if (edt.getType() == arts::EdtType::sync)
      syncTaskOps.push_back(edt);
  });

  if (syncTaskOps.empty())
    return false;

  /// Try to convert each sync task-EDT to an EpochOp.
  for (EdtOp op : syncTaskOps) {
    op.setType(arts::EdtType::task);
    convertToEpoch(op);
  }
  return true;
}

bool EdtPass::removeRedundantBarriersWithGraphs(func::FuncOp func,
                                                arts::EdtGraph &graph) {
  bool changed = false;

  /// Collect barriers within this function and check redundancy
  SmallVector<arts::BarrierOp, 8> toErase;
  func.walk([&](arts::BarrierOp barrier) {
    Block *block = barrier->getBlock();
    /// Partition EDTs in the same block into before/after
    SmallVector<arts::EdtOp, 8> beforeTasks;
    SmallVector<arts::EdtOp, 8> afterTasks;
    bool pastBarrier = false;
    for (Operation &op : *block) {
      if (&op == barrier.getOperation()) {
        pastBarrier = true;
        continue;
      }
      if (auto edt = dyn_cast<arts::EdtOp>(&op)) {
        (pastBarrier ? afterTasks : beforeTasks).push_back(edt);
      }
    }

    if (beforeTasks.empty() || afterTasks.empty())
      return;

    bool redundant = true;
    for (arts::EdtOp a : beforeTasks) {
      for (arts::EdtOp b : afterTasks) {
        bool connected = graph.isEdtReachable(a, b);
        bool independent = graph.areEdtsIndependent(a, b);
        /// Barrier redundant if: connected (dependency already enforced) OR
        /// independent (no dependency needed)
        if (!connected && !independent) {
          redundant = false;
          break;
        }
      }
      if (!redundant)
        break;
    }

    if (redundant)
      toErase.push_back(barrier);
  });

  for (auto b : toErase) {
    ARTS_INFO("Removing redundant barrier");
    b.erase();
    changed = true;
  }
  return changed;
}

////===----------------------------------------------------------------------===////
/// Pass creation
////===----------------------------------------------------------------------===////
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createEdtPass(ArtsAnalysisManager *AM, bool runAnalysis) {
  return std::make_unique<EdtPass>(AM, runAnalysis);
}
} // namespace arts
} // namespace mlir