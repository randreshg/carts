///==========================================================================///
/// File: PersistentRegionCostModel.cpp
///
/// Implementation of the persistent region gate cost model.
///==========================================================================///

#include "arts/analysis/heuristics/PersistentRegionCostModel.h"

using namespace mlir;
using namespace mlir::arts;

PersistentRegionGate mlir::arts::evaluatePersistentRegionGate(
    const StructuredKernelPlan &plan,
    const OwnershipProof &proof,
    unsigned workerCount) {
  PersistentRegionGate gate;

  // 1. Proof must be fully proven
  gate.proofSufficient = proof.isFullyProven();
  if (!gate.proofSufficient)
    return gate;

  // 2. Must have repetition structure (timestep or neighborhood)
  if (plan.repetition == RepetitionStructure::None)
    return gate;

  // 3. Must be a structured family (stencil, wavefront, or timestep chain)
  bool isStructuredFamily =
      plan.family == KernelFamily::Stencil ||
      plan.family == KernelFamily::Wavefront ||
      plan.family == KernelFamily::TimestepChain;
  if (!isStructuredFamily)
    return gate;

  // 4. Cost model: launch overhead ratio
  // Higher values mean more launch overhead relative to work → more benefit
  // from persistent regions.
  double overhead = plan.costSignals.schedulerOverhead;
  double pressure = plan.costSignals.sliceWideningPressure;
  double amortization = plan.costSignals.relaunchAmortization;

  // Launch overhead ratio: how much of the total cost is launch overhead
  gate.launchOverheadRatio = overhead;

  // Repetition benefit: more repetitions = more benefit from persistence
  gate.repetitionBenefit = amortization;

  // Slice stability: lower dep pressure = more stable slices
  gate.sliceStability = 1.0 - pressure;

  // Combined gate: persistent region is beneficial when:
  //   - launch overhead is significant (> 0.3)
  //   - repetition benefit is meaningful (> 0.2)
  //   - slices are reasonably stable (stability > 0.4)
  gate.costModelPassed =
      gate.launchOverheadRatio > 0.3 &&
      gate.repetitionBenefit > 0.2 &&
      gate.sliceStability > 0.4;

  gate.shouldPersist = gate.proofSufficient && gate.costModelPassed;

  return gate;
}
