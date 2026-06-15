///==========================================================================///
/// File: DistributionFailClosed.h
///
/// Fail-closed predicates and diagnostic for the SDE distribution passes (A7
/// in-place self-read wavefront/skew gate). Carved verbatim from the
/// correctness-base @782988ad1 DistributionPlanning pass, now owned by the
/// standalone DistributionFailClosed pass.
///==========================================================================///

#ifndef ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_FAIL_CLOSED_H
#define ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_FAIL_CLOSED_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"

namespace mlir::carts::sde::distribution {

// A7: a multi-worker in-place self-read neighborhood stencil (Gauss-Seidel
// family) with loop-carried neighbor offsets cannot be distributed without an
// unimplemented SDE wavefront/skew transform.
bool requiresInPlaceSelfRawWavefrontFailClosed(sde::SdeSuIterateOp op,
                                               sde::SDECostModel &costModel);

// Cite committed SDE facts so downstream layers cannot reinterpret this case.
void emitStencilWavefrontFailClosed(sde::SdeSuIterateOp op);

} // namespace mlir::carts::sde::distribution

#endif // ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_FAIL_CLOSED_H
