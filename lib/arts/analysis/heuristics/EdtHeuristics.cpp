///==========================================================================///
/// File: EdtHeuristics.cpp
///
/// EDT heuristic policy: distribution strategy and kind selection.
///==========================================================================///

#include "arts/analysis/heuristics/EdtHeuristics.h"
#include "arts/analysis/AnalysisManager.h"

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

std::optional<int64_t>
EdtHeuristics::computeCoarsenedBlockHint(ForOp forOp,
                                         const WorkerConfig &workerCfg) const {
  return DistributionHeuristics::computeCoarsenedBlockHint(
      forOp, getAnalysisManager().getLoopAnalysis(), workerCfg);
}
