///==========================================================================///
/// File: LoweringFactInfo.h
///
/// Phase A stub for AnalysisManager-resident lowering-fact queries.
/// Full migration from Utils/LoweringFactUtils.h is planned in Phase E.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_ANALYSIS_LOWERINGFACTINFO_H
#define CARTS_DIALECT_ARTS_ANALYSIS_LOWERINGFACTINFO_H

#include "carts/dialect/arts/Utils/LoweringFactUtils.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/AnalysisManager.h"
#include <optional>

namespace mlir::carts::arts {

/// AnalysisManager-resident view of committed lowering facts for one IR root.
class LoweringFactAnalysis {
public:
  explicit LoweringFactAnalysis(Operation *root);

  std::optional<LoweringFactInfo> getFacts(Operation *target) const;
  std::optional<LoweringFactInfo> getFacts(Value target) const;

  bool isInvalidated(const AnalysisManager::PreservedAnalyses &pa);

private:
  Operation *root = nullptr;
};

} // namespace mlir::carts::arts

#endif // CARTS_DIALECT_ARTS_ANALYSIS_LOWERINGFACTINFO_H
