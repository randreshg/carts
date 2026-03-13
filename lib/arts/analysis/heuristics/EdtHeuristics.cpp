///==========================================================================///
/// File: EdtHeuristics.cpp
///
/// EDT heuristic policy: distribution strategy and kind selection.
///==========================================================================///

#include "arts/analysis/heuristics/EdtHeuristics.h"

using namespace mlir;
using namespace mlir::arts;

DistributionStrategy
EdtHeuristics::chooseStrategy(EdtConcurrency concurrency) const {
  return DistributionHeuristics::analyzeStrategy(concurrency, &machine);
}

EdtDistributionKind
EdtHeuristics::chooseKind(const DistributionStrategy &strategy,
                          EdtDistributionPattern pattern) const {
  return DistributionHeuristics::selectDistributionKind(strategy, pattern);
}
