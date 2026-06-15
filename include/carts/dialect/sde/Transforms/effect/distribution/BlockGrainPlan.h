///==========================================================================///
/// File: BlockGrainPlan.h
///
/// Budget-reconciled block-grain commit + A1 same-owner grain unification for
/// the SDE distribution chain. Carved verbatim from the correctness-base
/// @782988ad1 DistributionPlanning pass. These entry points are shared by the
/// OwnerDimSelect per-SU commit pass (commitBudgetReconciledLayout) and the
/// standalone BlockGrainPlan pass (reconcileSameOwnerArrayGrain).
///==========================================================================///

#ifndef ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_BLOCK_GRAIN_PLAN_H
#define ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_BLOCK_GRAIN_PLAN_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"

#include "mlir/IR/Operation.h"

namespace mlir::carts::sde::distribution {

// Commit the one node-agnostic budget grain for a multi-owner data-parallel
// writer SU when the current SU step already realizes the selected block grain.
bool commitBudgetReconciledLayout(sde::SdeSuIterateOp op,
                                  sde::SDECostModel &costModel);

// A1 grain unification: commit one block grain (per-dim GCD / budget grain) on
// every block_parallel fact of an array so producer/consumer home==reader and
// no same-owner re-tile is left for RedistributionEdges to reject. Runs after
// all per-SU layout commits.
void reconcileSameOwnerArrayGrain(Operation *moduleOp);

} // namespace mlir::carts::sde::distribution

#endif // ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_BLOCK_GRAIN_PLAN_H
