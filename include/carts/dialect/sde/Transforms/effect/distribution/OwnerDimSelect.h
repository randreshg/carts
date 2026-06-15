///==========================================================================///
/// File: OwnerDimSelect.h
///
/// Per-SU owner-dim selection and physical layout commit for the SDE
/// distribution chain (wavefront/skew realization, loop-step replicated layout,
/// and the ordered per-pattern physical committers). Carved verbatim from the
/// correctness-base @782988ad1 DistributionPlanning pass. The single entry
/// point is shared by the transitional DistributionPlanning orchestrator and
/// the standalone OwnerDimSelect pass.
///==========================================================================///

#ifndef ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_OWNER_DIM_SELECT_H
#define ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_OWNER_DIM_SELECT_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"

namespace mlir::carts::sde::distribution {

// Realize wavefront/skew, loop-step replicated layout, and the ordered
// per-pattern physical layout committers for one SU (budget-reconciled grain,
// co-iterated writer inherit, stencil, matmul, uniform, reduction, in-place).
// Returns true when the SU was wavefront-skewed (replaced) and needs no further
// distribution handling; false otherwise. SUs that already carry committed
// CU/MU partition facts are left untouched by the per-pattern committers.
bool applyOwnerDimSelect(sde::SdeSuIterateOp op, sde::SDECostModel &costModel);

} // namespace mlir::carts::sde::distribution

#endif // ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_OWNER_DIM_SELECT_H
