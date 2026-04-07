///==========================================================================///
/// File: PersistentRegionCostModel.h
///
/// Cost model for gating persistent structured region lowering.
///
/// Evaluates whether a proven regular kernel benefits from persistent
/// region execution (long-lived owner-strip or owner-tile) vs the default
/// epoch-per-step model. The gate decision is conservative: persistent
/// regions are only selected when the cost model indicates clear benefit.
///
/// Input signals:
///   - plan cost signals (launchOverhead, depPressure, amortization)
///   - ownership proof (must be fully proven)
///   - repetition count (from plan or enclosing loop)
///   - worker count and machine topology
///
/// Output:
///   - PersistentRegionGate struct with the binary gate decision and
///     contributing factors for debugging.
///==========================================================================///

#ifndef ARTS_ANALYSIS_HEURISTICS_PERSISTENTREGIONCOSTMODEL_H
#define ARTS_ANALYSIS_HEURISTICS_PERSISTENTREGIONCOSTMODEL_H

#include "arts/analysis/heuristics/StructuredKernelPlanAnalysis.h"
#include "arts/analysis/db/OwnershipProof.h"

namespace mlir {
namespace arts {

/// Result of the persistent region gate evaluation.
struct PersistentRegionGate {
  bool shouldPersist = false;

  /// Contributing factors (for debug output).
  double launchOverheadRatio = 0.0;
  double repetitionBenefit = 0.0;
  double sliceStability = 0.0;
  bool proofSufficient = false;
  bool costModelPassed = false;
};

/// Evaluate whether a kernel plan + proof combination should use persistent
/// structured region lowering.
///
/// The gate is conservative: it requires:
///   1. Fully proven ownership (all 5 dimensions)
///   2. Repetition structure (timestep or neighborhood iteration)
///   3. Cost model benefit: launch overhead dominates and state is stable
PersistentRegionGate evaluatePersistentRegionGate(
    const StructuredKernelPlan &plan,
    const OwnershipProof &proof,
    unsigned workerCount);

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_HEURISTICS_PERSISTENTREGIONCOSTMODEL_H
