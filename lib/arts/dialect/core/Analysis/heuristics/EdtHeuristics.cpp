///==========================================================================///
/// File: EdtHeuristics.cpp
///
/// EDT heuristic policy: distribution strategy and kind selection.
///==========================================================================///

#include "arts/dialect/core/Analysis/heuristics/EdtHeuristics.h"
#include "arts/dialect/core/Analysis/AnalysisManager.h"
#include "arts/dialect/core/Analysis/db/DbAnalysis.h"
#include "arts/dialect/core/Analysis/edt/EdtAnalysis.h"

using namespace mlir;
using namespace mlir::arts;

const AbstractMachine &EdtHeuristics::getMachine() const {
  return getAnalysisManager().getAbstractMachine();
}

DistributionStrategy
EdtHeuristics::chooseStrategy(EdtConcurrency concurrency) const {
  return DistributionHeuristics::analyzeStrategy(concurrency, &getMachine());
}

EdtDistributionKind
EdtHeuristics::chooseKind(const DistributionStrategy &strategy,
                          EdtDistributionPattern pattern) const {
  return DistributionHeuristics::selectDistributionKind(strategy, pattern);
}

ParallelismDecision EdtHeuristics::resolveParallelism() const {
  return DistributionHeuristics::resolveParallelismFromMachine(&getMachine());
}

std::optional<WorkerConfig>
EdtHeuristics::resolveWorkerConfig(EdtOp parallelEdt) const {
  return DistributionHeuristics::resolveWorkerConfig(parallelEdt,
                                                     &getMachine());
}

DistributionStrategy
EdtHeuristics::resolveLoweringStrategy(EdtOp originalParallel,
                                       ForOp forOp) const {
  return DistributionHeuristics::resolveLoweringStrategy(originalParallel,
                                                         forOp);
}

std::optional<EdtDistributionPattern>
EdtHeuristics::resolveDistributionPattern(ForOp forOp,
                                          EdtOp originalParallel) const {
  return DistributionHeuristics::resolveDistributionPattern(
      &getAnalysisManager(), forOp, originalParallel);
}

LoopCoarseningDecision EdtHeuristics::computeLoopCoarseningDecision(
    ForOp forOp, const WorkerConfig &workerCfg) const {
  return DistributionHeuristics::computeLoopCoarseningDecision(
      forOp, getAnalysisManager().getLoopAnalysis(), workerCfg);
}

std::optional<int64_t>
EdtHeuristics::computeCoarsenedBlockHint(ForOp forOp,
                                         const WorkerConfig &workerCfg) const {
  return DistributionHeuristics::computeCoarsenedBlockHint(
      forOp, getAnalysisManager().getLoopAnalysis(), workerCfg);
}

ParallelEdtFusionDecision
EdtHeuristics::evaluateParallelEdtFusion(EdtOp first, EdtOp second) const {
  ParallelEdtFusionDecision decision;
  if (!first || !second) {
    decision.rationale = "missing parallel EDT";
    return decision;
  }
  if (!EdtAnalysis::isParallelEdtFusable(first) ||
      !EdtAnalysis::isParallelEdtFusable(second)) {
    decision.rationale = "parallel EDT is not structurally fusable";
    return decision;
  }
  if (DbAnalysis::hasDbConflict(first, second)) {
    decision.rationale = "parallel EDTs have conflicting memory effects";
    return decision;
  }

  decision.shouldFuse = true;
  decision.rationale = "eligible";
  return decision;
}
