///==========================================================================///
/// File: SdeSubstrateAnalyses.cpp
///
/// AnalysisManager accessors and preservation helpers for Phase B substrate.
///==========================================================================///

#include "carts/dialect/sde/Analysis/SdeSubstrateAnalyses.h"

namespace mlir::carts::sde {

SdeAccessRelation &getSdeAccessRelation(AnalysisManager &am) {
  return am.getAnalysis<SdeAccessRelation>();
}

SdeCommVolumeCost &getSdeCommVolumeCost(AnalysisManager &am) {
  return am.getAnalysis<SdeCommVolumeCost>();
}

SdeDependence &getSdeDependence(AnalysisManager &am) {
  return am.getAnalysis<SdeDependence>();
}

SdeMemoryLegality &getSdeMemoryLegality(AnalysisManager &am) {
  return am.getAnalysis<SdeMemoryLegality>();
}

void preserveSdeSubstrateAnalyses(AnalysisManager::PreservedAnalyses &pa) {
  pa.preserve<SdeAccessRelation>();
  pa.preserve<SdeCommVolumeCost>();
  pa.preserve<SdeDependence>();
  pa.preserve<SdeMemoryLegality>();
}

} // namespace mlir::carts::sde
