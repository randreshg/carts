///==========================================================================///
/// ElementwisePipelinePattern.cpp - Fuse sibling pointwise ARTS loops
///
/// This transform normalizes consecutive sibling `arts.for` loops inside the
/// same parallel EDT into a single loop when each stage is pointwise over the
/// same iteration space. The fused loop keeps the original per-stage order and
/// writes, but downstream passes now see one uniform kernel family instead of
/// many small parallel phases. Stages are fused only within the same pointwise
/// compute class so arithmetic-only work is not mixed with scalar-call or
/// vector-math stages that need different downstream codegen treatment.
///
/// Before:
///   edt.parallel {
///     arts.for (...) { stage0 pointwise work }
///     arts.for (...) { stage1 pointwise work }
///     arts.for (...) { stage2 pointwise work }
///   }
///
/// After:
///   edt.parallel {
///     arts.for (...) {
///       stage0 pointwise work
///       stage1 pointwise work
///       stage2 pointwise work
///     }
///   }
///==========================================================================///

#include "arts/analysis/metadata/MetadataManager.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/transforms/kernel/KernelTransform.h"
#include "arts/utils/EdtUtils.h"
#include "arts/utils/LoopUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/DenseSet.h"
#include <iterator>

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(elementwise_pipeline_pattern);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct ElementwiseStage {
  ForOp loop;
  SmallVector<Value, 4> writes;
};

struct ElementwiseEdtStage {
  EdtOp edt;
  ElementwiseStage stage;
};

static bool isSameOrZeroOffset(Value idx, Value iv) {
  if (!idx || !iv)
    return false;

  Value strippedIdx = ValueAnalysis::stripNumericCasts(idx);
  Value strippedIv = ValueAnalysis::stripNumericCasts(iv);
  if (ValueAnalysis::sameValue(strippedIdx, strippedIv))
    return true;

  int64_t offset = 0;
  Value base = ValueAnalysis::stripConstantOffset(strippedIdx, &offset);
  if (!base)
    return false;
  base = ValueAnalysis::stripNumericCasts(base);
  return offset == 0 && ValueAnalysis::sameValue(base, strippedIv);
}

static bool isInvariantIndex(Value idx, Value iv) {
  if (!idx)
    return false;
  if (!ValueAnalysis::dependsOn(idx, iv))
    return true;
  return isSameOrZeroOffset(idx, iv);
}

static bool isPureScalarCall(func::CallOp call) {
  if (!call)
    return false;
  for (Value operand : call.getOperands()) {
    if (isa<BaseMemRefType>(operand.getType()))
      return false;
  }
  for (Type resultTy : call.getResultTypes()) {
    if (isa<BaseMemRefType>(resultTy))
      return false;
  }

  auto callee = SymbolTable::lookupNearestSymbolFrom<func::FuncOp>(
      call, call.getCalleeAttr());
  if (!callee)
    return false;

  for (Type argTy : callee.getFunctionType().getInputs()) {
    if (isa<BaseMemRefType>(argTy))
      return false;
  }
  for (Type resultTy : callee.getFunctionType().getResults()) {
    if (isa<BaseMemRefType>(resultTy))
      return false;
  }
  return isPure(call.getOperation());
}

static bool isAllowedStageOp(Operation *op, Value iv) {
  if (!op)
    return false;

  if (isa<arts::YieldOp, arith::ConstantOp>(op))
    return true;

  if (auto load = dyn_cast<memref::LoadOp>(op)) {
    if (load.getIndices().empty())
      return false;
    return llvm::all_of(load.getIndices(),
                        [&](Value idx) { return isInvariantIndex(idx, iv); });
  }

  if (auto store = dyn_cast<memref::StoreOp>(op)) {
    if (store.getIndices().empty())
      return false;
    bool hasIvIndexedStore = false;
    for (Value idx : store.getIndices()) {
      if (!ValueAnalysis::dependsOn(idx, iv)) {
        if (!isInvariantIndex(idx, iv))
          return false;
        continue;
      }
      if (!isSameOrZeroOffset(idx, iv))
        return false;
      hasIvIndexedStore = true;
    }
    return hasIvIndexedStore;
  }

  if (auto call = dyn_cast<func::CallOp>(op))
    return isPureScalarCall(call);

  if (auto effects = dyn_cast<MemoryEffectOpInterface>(op)) {
    if (!effects.hasNoEffect())
      return false;
  } else if (!isa<scf::IfOp>(op)) {
    return false;
  }

  for (Region &region : op->getRegions()) {
    for (Block &block : region) {
      for (Operation &nested : block) {
        if (!isAllowedStageOp(&nested, iv))
          return false;
      }
    }
  }
  return true;
}

static bool matchElementwiseStage(ForOp loop, ElementwiseStage &stage) {
  if (!loop || loop.getRegion().empty())
    return false;
  if (!loop.getReductionAccumulators().empty())
    return false;

  Block &body = loop.getRegion().front();
  if (body.getNumArguments() != 1)
    return false;
  Value iv = body.getArgument(0);

  DenseSet<Value> writes;
  bool sawStore = false;
  for (Operation &op : body.getOperations()) {
    if (op.hasTrait<OpTrait::IsTerminator>())
      continue;
    if (!isAllowedStageOp(&op, iv))
      return false;
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      sawStore = true;
      Value target = ValueAnalysis::stripMemrefViewOps(store.getMemRef());
      if (!writes.insert(target).second)
        return false;
      stage.writes.push_back(target);
    }
  }

  if (!sawStore)
    return false;

  stage.loop = loop;
  return true;
}

static ForOp fuseStages(SmallVectorImpl<ElementwiseStage> &stages,
                        MetadataManager &metadataManager) {
  assert(stages.size() >= 2 && "expected at least two stages");

  ForOp first = stages.front().loop;
  OpBuilder builder(first);
  builder.setInsertionPoint(first);
  auto fused = cast<ForOp>(builder.clone(*first.getOperation()));

  copyPatternAttrs(first.getOperation(), fused.getOperation());
  metadataManager.rewriteMetadata(first.getOperation(), fused.getOperation());
  setDepPattern(fused.getOperation(), ArtsDepPattern::elementwise_pipeline);
  setEdtDistributionPattern(fused.getOperation(),
                            EdtDistributionPattern::uniform);

  Block &dst = fused.getRegion().front();
  Operation *terminator = dst.getTerminator();
  OpBuilder bodyBuilder(terminator);
  IRMapping mapping;
  mapping.map(stages.front().loop.getRegion().front().getArgument(0),
              dst.getArgument(0));

  for (size_t stageIdx = 1; stageIdx < stages.size(); ++stageIdx) {
    ElementwiseStage &stage = stages[stageIdx];
    Value stageIv = stage.loop.getRegion().front().getArgument(0);
    mapping.map(stageIv, dst.getArgument(0));
    for (Operation &op : stage.loop.getRegion().front().getOperations()) {
      if (op.hasTrait<OpTrait::IsTerminator>())
        continue;
      bodyBuilder.clone(op, mapping);
    }
  }

  metadataManager.refreshMetadata(fused.getOperation());
  return fused;
}

static bool matchSingleLoopElementwiseEdt(EdtOp edt,
                                          ElementwiseEdtStage &stage) {
  if (!edt || edt.getType() != EdtType::parallel)
    return false;

  ForOp onlyLoop = getSingleTopLevelFor(edt);
  if (!onlyLoop)
    return false;

  ElementwiseStage loopStage;
  if (!matchElementwiseStage(onlyLoop, loopStage))
    return false;

  stage.edt = edt;
  stage.stage = std::move(loopStage);
  return true;
}

static EdtOp fuseEdtStages(SmallVectorImpl<ElementwiseEdtStage> &stages,
                           MetadataManager &metadataManager) {
  assert(stages.size() >= 2 && "expected at least two EDT stages");

  EdtOp first = stages.front().edt;
  OpBuilder builder(first);
  builder.setInsertionPoint(first);
  auto fused = cast<EdtOp>(builder.clone(*first.getOperation()));
  metadataManager.rewriteMetadata(first.getOperation(), fused.getOperation());
  copyWorkerTopologyAttrs(first.getOperation(), fused.getOperation());
  setDepPattern(fused.getOperation(), ArtsDepPattern::elementwise_pipeline);
  setEdtDistributionPattern(fused.getOperation(),
                            EdtDistributionPattern::uniform);
  if (ForOp fusedLoop = getSingleTopLevelFor(fused))
    metadataManager.rewriteMetadata(stages.front().stage.loop.getOperation(),
                                    fusedLoop.getOperation());

  Block &dstBlock = fused.getBody().front();
  Operation *terminator = dstBlock.getTerminator();
  OpBuilder bodyBuilder(terminator);
  IRMapping mapping;
  for (size_t stageIdx = 1; stageIdx < stages.size(); ++stageIdx) {
    for (Operation &op :
         stages[stageIdx].edt.getBody().front().without_terminator()) {
      if (Operation *cloned = bodyBuilder.clone(op, mapping))
        metadataManager.rewriteMetadata(&op, cloned);
    }
  }
  metadataManager.refreshMetadata(fused.getOperation());
  return fused;
}

} // namespace

int mlir::arts::applyElementwisePipelineTransform(
    ModuleOp module, MetadataManager &metadataManager) {
  if (!module)
    return 0;

  int rewrites = 0;
  bool changed = true;
  while (changed) {
    changed = false;
    SmallVector<EdtOp, 16> parallelEdts;
    module.walk([&](EdtOp edt) {
      if (edt.getType() == EdtType::parallel)
        parallelEdts.push_back(edt);
    });

    for (EdtOp edt : parallelEdts) {
      if (!edt || !edt->getBlock())
        continue;
      Block &block = edt.getBody().front();
      for (auto it = block.begin(), e = block.end(); it != e; ++it) {
        Operation &op = *it;
        if (op.hasTrait<OpTrait::IsTerminator>())
          break;

        auto firstLoop = dyn_cast<ForOp>(&op);
        ElementwiseStage firstStage;
        if (!firstLoop || !matchElementwiseStage(firstLoop, firstStage))
          continue;

        SmallVector<ElementwiseStage, 8> stages;
        stages.push_back(firstStage);
        PointwiseLoopComputeClass firstStageClass =
            classifyPointwiseLoopCompute(firstLoop);
        DenseSet<Value> writtenTargets;
        for (Value target : firstStage.writes)
          writtenTargets.insert(target);

        for (auto nextIt = std::next(it); nextIt != e; ++nextIt) {
          Operation &nextOp = *nextIt;
          if (nextOp.hasTrait<OpTrait::IsTerminator>())
            break;
          auto nextLoop = dyn_cast<ForOp>(&nextOp);
          ElementwiseStage nextStage;
          if (!nextLoop || !haveSameBounds(firstLoop, nextLoop) ||
              !matchElementwiseStage(nextLoop, nextStage))
            break;
          if (classifyPointwiseLoopCompute(nextLoop) != firstStageClass)
            break;

          bool disjointWrites =
              llvm::all_of(nextStage.writes, [&](Value target) {
                return !writtenTargets.contains(target);
              });
          if (!disjointWrites)
            break;

          for (Value target : nextStage.writes)
            writtenTargets.insert(target);
          stages.push_back(std::move(nextStage));
        }

        if (stages.size() < 2)
          continue;

        fuseStages(stages, metadataManager);
        for (ElementwiseStage &stage : stages) {
          metadataManager.removeMetadata(stage.loop.getOperation());
          stage.loop.erase();
        }
        rewrites++;
        changed = true;
        break;
      }
      if (changed)
        break;
    }

    if (changed)
      continue;

    SmallVector<Block *, 32> parentBlocks;
    module.walk([&](Operation *op) {
      for (Region &region : op->getRegions())
        for (Block &block : region)
          parentBlocks.push_back(&block);
    });

    for (Block *parentBlock : parentBlocks) {
      if (changed || !parentBlock)
        break;

      for (auto it = parentBlock->begin(), e = parentBlock->end(); it != e;
           ++it) {
        auto firstEdt = dyn_cast<EdtOp>(&*it);
        ElementwiseEdtStage firstStage;
        if (!firstEdt || !matchSingleLoopElementwiseEdt(firstEdt, firstStage))
          continue;

        SmallVector<ElementwiseEdtStage, 8> stages;
        stages.push_back(firstStage);
        PointwiseLoopComputeClass firstStageClass =
            classifyPointwiseLoopCompute(firstStage.stage.loop);
        DenseSet<Value> writtenTargets;
        for (Value target : firstStage.stage.writes)
          writtenTargets.insert(target);

        auto nextIt = std::next(it);
        for (; nextIt != e; ++nextIt) {
          auto nextEdt = dyn_cast<EdtOp>(&*nextIt);
          ElementwiseEdtStage nextStage;
          if (!nextEdt ||
              nextEdt.getConcurrency() != firstEdt.getConcurrency() ||
              !matchSingleLoopElementwiseEdt(nextEdt, nextStage) ||
              !haveSameBounds(firstStage.stage.loop, nextStage.stage.loop))
            break;
          if (classifyPointwiseLoopCompute(nextStage.stage.loop) !=
              firstStageClass)
            break;

          bool disjointWrites =
              llvm::all_of(nextStage.stage.writes, [&](Value target) {
                return !writtenTargets.contains(target);
              });
          if (!disjointWrites)
            break;

          for (Value target : nextStage.stage.writes)
            writtenTargets.insert(target);
          stages.push_back(std::move(nextStage));
        }

        if (stages.size() < 2)
          continue;

        fuseEdtStages(stages, metadataManager);
        for (auto &stage : stages) {
          metadataManager.removeMetadata(stage.stage.loop.getOperation());
          metadataManager.removeMetadata(stage.edt.getOperation());
          stage.edt.erase();
        }
        rewrites++;
        changed = true;
        break;
      }
    }
  }

  if (rewrites > 0)
    ARTS_INFO("Applied elementwise_pipeline transform (" << rewrites
                                                         << " rewrite(s))");
  return rewrites;
}
