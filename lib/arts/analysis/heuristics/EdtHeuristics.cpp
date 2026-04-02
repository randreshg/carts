///==========================================================================///
/// File: EdtHeuristics.cpp
///
/// EDT heuristic policy: distribution strategy and kind selection.
///==========================================================================///

#include "arts/analysis/heuristics/EdtHeuristics.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/edt/EdtAnalysis.h"

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

  const AbstractMachine &machine = getMachine();
  int64_t workerCount = std::max<int64_t>(
      1, resolveWorkerConfig(first).value_or(WorkerConfig{}).totalWorkers);
  if (workerCount <= 1) {
    auto fallback = DistributionHeuristics::resolveParallelismFromMachine(
        machine.isValid() ? &machine : nullptr);
    workerCount = std::max<int64_t>(1, fallback.totalWorkers);
  }

  EdtAnalysis &edtAnalysis = getAnalysisManager().getEdtAnalysis();
  EdtAnalysis::ParallelEdtWorkSummary firstLoops =
      edtAnalysis.summarizeParallelEdtWork(first, workerCount);
  EdtAnalysis::ParallelEdtWorkSummary secondLoops =
      edtAnalysis.summarizeParallelEdtWork(second, workerCount);
  bool sharedReadonlyInputs =
      edtAnalysis.hasSharedReadonlyInputs(first, second);
  bool heavyReductionPipeline = workerCount > 1 && sharedReadonlyInputs &&
                                firstLoops.hasReductionLoop &&
                                secondLoops.hasReductionLoop &&
                                firstLoops.peakWorkPerWorker >= workerCount &&
                                secondLoops.peakWorkPerWorker >= workerCount;
  if (heavyReductionPipeline) {
    decision.rationale = "sibling reductions already amortize launch overhead";
    return decision;
  }

  decision.shouldFuse = true;
  decision.rationale = "eligible";
  return decision;
}
