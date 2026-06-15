///==========================================================================///
/// File: DistributionLayoutUtils.h
///
/// Shared layout/commit helpers for the SDE distribution passes. These were
/// extracted verbatim from the monolithic DistributionPlanning pass so the
/// split passes (OwnerDimSelect, BlockGrainPlan, MovementTagging,
/// DistributionFailClosed) can share one owner of the physical-layout commit
/// machinery instead of duplicating it. The logic is byte-identical to the
/// correctness-base @782988ad1 helpers.
///==========================================================================///

#ifndef ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_LAYOUT_UTILS_H
#define ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_LAYOUT_UTILS_H

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"

#include "mlir/IR/Value.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"

#include <cstdint>
#include <optional>

namespace mlir::carts::sde::distribution {

int64_t saturatingMultiplyPositive(int64_t lhs, int64_t rhs);

// Stencil halo radius for an owner physical dim from the SU neighborhood
// access info (0 for non-stencils / dims without an owner offset).
int64_t readStencilHaloForOwnerDim(sde::SdeSuIterateOp op, unsigned ownerDim);

// Recover a write LayoutGraphFact from an already-committed physical layout.
std::optional<sde::LayoutGraphFact>
layoutFactFromCommittedPhysicalLayout(sde::SdeSuIterateOp op);

// Select the single consistent write LayoutGraphFact for an SU (falling back to
// the committed physical layout); std::nullopt if writers disagree.
std::optional<sde::LayoutGraphFact>
selectSingleWriteLayoutFact(sde::SdeSuIterateOp op);

int64_t getInterLocalityTargetWorkers(sde::SDECostModel &costModel);

std::optional<unsigned> findDependentSuLoopSlot(Value index,
                                                ArrayRef<Value> loopIvs);

std::optional<SmallVector<int64_t, 4>>
derivePhysicalDimToSuLoopDimFromExternalStores(sde::SdeSuIterateOp op,
                                               ArrayRef<int64_t> ownerDims);

bool hasCommittedPhysicalLayout(sde::SdeSuIterateOp op);

void applyPhysicalPlan(sde::SdeSuIterateOp op, ArrayRef<int64_t> ownerDims,
                       ArrayRef<int64_t> physicalBlockShape,
                       ArrayRef<int64_t> haloShape = {},
                       ArrayRef<int64_t> logicalWorkerSlice = {});

bool allowsGroupedLogicalWorkerSlice(sde::SdeSuIterateOp op,
                                     ArrayRef<int64_t> haloShape = {});

SmallVector<int64_t, 4> buildLogicalWorkerSliceOrPhysical(
    sde::SdeSuIterateOp op, ArrayRef<int64_t> shape,
    ArrayRef<int64_t> ownerDims, ArrayRef<int64_t> physicalBlockShape,
    int64_t targetComputeUnits, ArrayRef<int64_t> haloShape = {});

bool physicalLayoutMatchesRealizedLoopSteps(
    sde::SdeSuIterateOp op, ArrayRef<int64_t> ownerDims,
    ArrayRef<int64_t> physicalBlockShape, ArrayRef<int64_t> logicalWorkerSlice);

bool applyPhysicalLayoutIfRealized(sde::SdeSuIterateOp op,
                                   ArrayRef<int64_t> ownerDims,
                                   ArrayRef<int64_t> physicalBlockShape,
                                   ArrayRef<int64_t> haloShape = {},
                                   ArrayRef<int64_t> logicalWorkerSlice = {});

std::optional<SmallVector<int64_t, 4>>
orderPhysicalOwnerDimsByLoop(const sde::SuOutputLayoutFacts &outputPlan,
                             ArrayRef<int64_t> layoutOwnerDims,
                             unsigned loopRank);

bool allExternalStoresCoverOwnerDims(
    sde::SdeSuIterateOp op, ArrayRef<int64_t> ownerDims,
    ArrayRef<int64_t> physicalDimToLoopDim = {});

} // namespace mlir::carts::sde::distribution

#endif // ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_DISTRIBUTION_LAYOUT_UTILS_H
