///==========================================================================///
/// File: SdeSubstrateAnalyses.h
///
/// Phase B substrate analyses registered with MLIR AnalysisManager.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_ANALYSIS_SDE_SUBSTRATE_ANALYSES_H
#define CARTS_DIALECT_SDE_ANALYSIS_SDE_SUBSTRATE_ANALYSES_H

#include "carts/dialect/sde/Analysis/SdeAccessRelation.h"
#include "carts/dialect/sde/Analysis/SdeCommVolumeCost.h"
#include "carts/dialect/sde/Analysis/SdeDependence.h"
#include "carts/dialect/sde/Analysis/SdeMemoryLegality.h"
#include "mlir/Pass/AnalysisManager.h"

namespace mlir::carts::sde {
/// All Phase B substrate analyses expose `(Operation *)` construction and
/// `isInvalidated(...)`. Passes consume them through these typed accessors.

SdeAccessRelation &getSdeAccessRelation(mlir::AnalysisManager &am);
SdeCommVolumeCost &getSdeCommVolumeCost(mlir::AnalysisManager &am);
SdeDependence &getSdeDependence(mlir::AnalysisManager &am);
SdeMemoryLegality &getSdeMemoryLegality(mlir::AnalysisManager &am);

/// Mark every substrate analysis preserved across a transform pass.
void preserveSdeSubstrateAnalyses(AnalysisManager::PreservedAnalyses &pa);

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_ANALYSIS_SDE_SUBSTRATE_ANALYSES_H
