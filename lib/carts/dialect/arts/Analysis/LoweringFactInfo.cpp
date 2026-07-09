///==========================================================================///
/// File: LoweringFactInfo.cpp
///
/// Phase A AnalysisManager stub delegating to Utils/LoweringFactUtils.
///==========================================================================///

#include "carts/dialect/arts/Analysis/LoweringFactInfo.h"

using namespace mlir;
using namespace mlir::carts::arts;

LoweringFactAnalysis::LoweringFactAnalysis(Operation *root) : root(root) {}

std::optional<LoweringFactInfo>
LoweringFactAnalysis::getFacts(Operation *target) const {
  (void)root;
  return getSemanticFacts(target);
}

std::optional<LoweringFactInfo>
LoweringFactAnalysis::getFacts(Value target) const {
  (void)root;
  return getLoweringFacts(target);
}

bool LoweringFactAnalysis::isInvalidated(
    const AnalysisManager::PreservedAnalyses &pa) {
  return !pa.isPreserved<LoweringFactAnalysis>();
}
