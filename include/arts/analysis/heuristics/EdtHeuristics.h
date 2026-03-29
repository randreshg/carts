///==========================================================================///
/// File: EdtHeuristics.h
///
/// EDT-specific heuristic policy and analysis façade for distribution
/// decisions.
///
/// Pass-facing policy queries should go through this interface instead of
/// calling DistributionHeuristics directly. This keeps machine/loop/DB-backed
/// distribution analysis centralized behind AnalysisManager.
///==========================================================================///

#ifndef ARTS_ANALYSIS_HEURISTICS_EDTHEURISTICS_H
#define ARTS_ANALYSIS_HEURISTICS_EDTHEURISTICS_H

#include "arts/analysis/Analysis.h"
#include "arts/analysis/heuristics/DistributionHeuristics.h"
#include <optional>

namespace mlir {
namespace arts {

class EdtHeuristics : public ArtsAnalysis {
public:
  explicit EdtHeuristics(AnalysisManager &manager) : ArtsAnalysis(manager) {}

  void invalidate() override {}

  DistributionStrategy chooseStrategy(EdtConcurrency concurrency) const;
  EdtDistributionKind chooseKind(const DistributionStrategy &strategy,
                                 EdtDistributionPattern pattern) const;
  ParallelismDecision resolveParallelism() const;
  std::optional<WorkerConfig> resolveWorkerConfig(EdtOp parallelEdt) const;
  DistributionStrategy resolveLoweringStrategy(EdtOp originalParallel,
                                               ForOp forOp) const;
  std::optional<EdtDistributionPattern>
  resolveDistributionPattern(ForOp forOp, EdtOp originalParallel) const;
  std::optional<int64_t>
  computeCoarsenedBlockHint(ForOp forOp, const WorkerConfig &workerCfg) const;

private:
  const AbstractMachine &getMachine() const;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_HEURISTICS_EDTHEURISTICS_H
