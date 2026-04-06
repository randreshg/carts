///==========================================================================///
/// File: EpochAnalysis.h
///
/// Pass-facing facade for epoch scheduling/structural policy queries.
///==========================================================================///

#ifndef ARTS_ANALYSIS_EDT_EPOCHANALYSIS_H
#define ARTS_ANALYSIS_EDT_EPOCHANALYSIS_H

#include "arts/analysis/Analysis.h"
#include "arts/analysis/heuristics/EpochHeuristics.h"

namespace mlir {
namespace arts {

class EpochAnalysis : public ArtsAnalysis {
public:
  explicit EpochAnalysis(AnalysisManager &manager) : ArtsAnalysis(manager) {}

  void invalidate() override {}

  EpochAccessSummary summarizeEpochAccess(EpochOp epoch) const;
  EpochFusionDecision
  evaluateEpochFusion(EpochOp first, EpochOp second,
                      bool continuationEnabled = true,
                      const EpochAccessSummary *firstSummary = nullptr,
                      const EpochAccessSummary *secondSummary = nullptr) const;
  EpochContinuationDecision
  evaluateContinuation(EpochOp epoch, EpochOp previousEpoch = nullptr,
                       bool continuationEnabled = true,
                       const EpochAccessSummary *previousSummary = nullptr,
                       const EpochAccessSummary *epochSummary = nullptr) const;
  EpochAsyncLoopDecision evaluateAsyncLoopStrategy(scf::ForOp forOp) const;
  EpochLoopDriverDecision evaluateCPSLoopDriver(scf::ForOp forOp) const;
  EpochChainDecision evaluateCPSChain(scf::ForOp forOp) const;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_EDT_EPOCHANALYSIS_H
