///==========================================================================///
/// File: MovementTagging.h
///
/// Distribution-kind selection for the SDE distribution chain (blocked vs
/// owner_compute) and the sde.su_distribute wrapping it drives. Carved verbatim
/// from the correctness-base @782988ad1 DistributionPlanning pass. The
/// distribution-kind chooser is shared by the transitional DistributionPlanning
/// orchestrator and the standalone MovementTagging pass.
///==========================================================================///

#ifndef ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_MOVEMENT_TAGGING_H
#define ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_MOVEMENT_TAGGING_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"

#include <optional>

namespace mlir::carts::sde::distribution {

// Choose the SDE distribution kind (blocked / owner_compute) for an eligible
// SU, or std::nullopt when the SU should not be wrapped in sde.su_distribute.
std::optional<sde::SdeDistributionKind>
chooseDistributionKind(sde::SdeSuIterateOp op, sde::SDECostModel &costModel);

} // namespace mlir::carts::sde::distribution

#endif // ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_MOVEMENT_TAGGING_H
