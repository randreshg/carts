///==========================================================================///
/// File: EdtHeuristics.h
///
/// EDT-specific heuristic policy for distribution decisions.
///==========================================================================///

#ifndef ARTS_ANALYSIS_HEURISTICS_EDTHEURISTICS_H
#define ARTS_ANALYSIS_HEURISTICS_EDTHEURISTICS_H

#include "arts/analysis/heuristics/DistributionHeuristics.h"
#include "arts/utils/abstract_machine/AbstractMachine.h"

namespace mlir {
namespace arts {

class EdtHeuristics {
public:
  explicit EdtHeuristics(const mlir::arts::AbstractMachine &machine)
      : machine(machine) {}

  DistributionStrategy chooseStrategy(EdtConcurrency concurrency) const;
  EdtDistributionKind chooseKind(const DistributionStrategy &strategy,
                                 EdtDistributionPattern pattern) const;

private:
  const mlir::arts::AbstractMachine &machine;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_HEURISTICS_EDTHEURISTICS_H
