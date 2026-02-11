///==========================================================================///
/// File: ParallelSplitLowering.cpp
///
/// Split-analysis and continuation EDT lowering helpers used by ForLowering.
///==========================================================================///

#include "arts/Transforms/Edt/ParallelSplitLowering.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/OperationAttributes.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(parallel_split_lowering);

using namespace mlir;
using namespace mlir::arts;

ParallelRegionSplitAnalysis
ParallelRegionSplitAnalysis::analyze(EdtOp parallelEdt) {
  ParallelRegionSplitAnalysis analysis;
  Block &body = parallelEdt.getBody().front();

  bool seenFor = false;

  for (Operation &op : body.without_terminator()) {
    if (auto forOp = dyn_cast<ForOp>(&op)) {
      analysis.forOps.push_back(forOp);
      seenFor = true;
    } else if (isa<DbReleaseOp>(&op)) {
      /// Skip db_release ops - these are cleanup operations, not real work.
      continue;
    } else {
      if (!seenFor)
        analysis.opsBeforeFor.push_back(&op);
      else
        analysis.opsAfterFor.push_back(&op);
    }
  }

  return analysis;
}

void ParallelRegionSplitAnalysis::analyzeDependenciesForSplit(
    EdtOp parallelEdt) {
  Block &parallelBlock = parallelEdt.getBody().front();
  ValueRange deps = parallelEdt.getDependencies();

  for (auto [idx, dep] : llvm::enumerate(deps))
    argIndexToDep[idx] = dep;

  auto collectUsedBlockArgs = [&](Operation *op,
                                  SetVector<BlockArgument> &result) {
    op->walk([&](Operation *nested) {
      for (Value operand : nested->getOperands()) {
        if (auto blockArg = operand.dyn_cast<BlockArgument>()) {
          if (blockArg.getOwner() == &parallelBlock)
            result.insert(blockArg);
        }
      }
    });
  };

  for (ForOp forOp : forOps)
    collectUsedBlockArgs(forOp.getOperation(), depsUsedByFor);
  for (Operation *op : opsAfterFor)
    collectUsedBlockArgs(op, depsUsedAfterFor);
}

EdtOp ParallelSplitLowering::createContinuationParallel(
    ArtsCodegen &AC, EdtOp originalParallel,
    ParallelRegionSplitAnalysis &analysis, Location loc) {
  ARTS_INFO(" - Creating continuation parallel EDT for post-for work");

  AC.setInsertionPointAfter(originalParallel);

  ValueRange originalDeps = originalParallel.getDependencies();
  Block &originalBlock = originalParallel.getBody().front();

  SmallVector<Value> newDeps;
  IRMapping argMapper;
  SmallVector<unsigned> origArgIndices;

  for (BlockArgument origArg : analysis.depsUsedAfterFor) {
    unsigned idx = origArg.getArgNumber();
    if (idx >= originalDeps.size()) {
      ARTS_ERROR("Block argument index out of range: " << idx);
      continue;
    }

    Value dep = originalDeps[idx];

    auto allocInfo = DatablockUtils::traceToDbAlloc(dep);
    if (!allocInfo) {
      ARTS_ERROR("Could not trace dependency to DbAllocOp for arg " << idx);
      continue;
    }
    auto [rootGuid, rootPtr] = *allocInfo;

    auto origAcq = dep.getDefiningOp<DbAcquireOp>();
    ArtsMode mode = origAcq ? origAcq.getMode() : ArtsMode::inout;
    Type ptrType = origAcq ? origAcq.getPtr().getType() : rootPtr.getType();

    auto newAcq = AC.create<DbAcquireOp>(
        loc, mode, rootGuid, rootPtr, ptrType,
        origAcq ? origAcq.getPartitionMode() : std::nullopt,
        origAcq ? SmallVector<Value>(origAcq.getIndices())
                : SmallVector<Value>{},
        origAcq ? SmallVector<Value>(origAcq.getOffsets())
                : SmallVector<Value>{},
        origAcq ? SmallVector<Value>(origAcq.getSizes()) : SmallVector<Value>{},
        origAcq ? SmallVector<Value>(origAcq.getPartitionIndices())
                : SmallVector<Value>{},
        origAcq ? SmallVector<Value>(origAcq.getPartitionOffsets())
                : SmallVector<Value>{},
        origAcq ? SmallVector<Value>(origAcq.getPartitionSizes())
                : SmallVector<Value>{});
    if (origAcq)
      newAcq.copyPartitionSegmentsFrom(origAcq);

    newDeps.push_back(newAcq.getPtr());
    origArgIndices.push_back(idx);

    ARTS_DEBUG("  - Reacquired datablock for arg " << idx);
  }

  Value routeZero = AC.createIntConstant(0, AC.Int32, loc);
  auto contParallel =
      AC.create<EdtOp>(loc, EdtType::parallel,
                       originalParallel.getConcurrency(), routeZero, newDeps);

  if (auto workers = originalParallel.getWorkersAttr())
    contParallel->setAttr(AttrNames::Operation::Workers, workers);

  Block &contBlock = contParallel.getBody().front();
  for (auto [i, dep] : llvm::enumerate(newDeps)) {
    BlockArgument newArg = contBlock.addArgument(dep.getType(), loc);
    unsigned origIdx = origArgIndices[i];
    argMapper.map(originalBlock.getArgument(origIdx), newArg);
  }

  AC.setInsertionPointToStart(&contBlock);
  for (Operation *op : analysis.opsAfterFor)
    AC.clone(*op, argMapper);

  AC.setInsertionPointToEnd(&contBlock);
  if (contBlock.empty() || !contBlock.back().hasTrait<OpTrait::IsTerminator>())
    AC.create<YieldOp>(loc);

  ARTS_INFO(" - Continuation parallel EDT created with "
            << newDeps.size() << " reacquired dependencies");

  return contParallel;
}

void ParallelSplitLowering::cleanupOriginalParallel(
    EdtOp originalParallel, ParallelRegionSplitAnalysis &analysis,
    bool hasPreFor) {
  for (Operation *op : llvm::reverse(analysis.opsAfterFor))
    op->erase();
  for (ForOp forOp : analysis.forOps)
    forOp.erase();

  if (hasPreFor) {
    ARTS_INFO(" - Keeping pre-for work in original parallel EDT");
    return;
  }

  ARTS_INFO(" - Erasing empty original parallel EDT and its original acquires");

  SmallVector<DbAcquireOp> acquiresToErase;
  for (Value dep : originalParallel.getDependencies()) {
    if (auto acqOp = dep.getDefiningOp<DbAcquireOp>())
      acquiresToErase.push_back(acqOp);
  }

  originalParallel.erase();

  for (DbAcquireOp acqOp : acquiresToErase) {
    if (acqOp.getPtr().use_empty()) {
      ARTS_DEBUG("  - Erasing orphaned acquire for original parallel");
      acqOp.erase();
    }
  }
}
