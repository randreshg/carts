///==========================================================================///
/// File: SdeCommVolumeCost.h
///
/// Phase B substrate: abstract communication volume cost per owner-dim choice.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_ANALYSIS_SDE_COMM_VOLUME_COST_H
#define CARTS_DIALECT_SDE_ANALYSIS_SDE_COMM_VOLUME_COST_H

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/AnalysisManager.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

#include <cstdint>
#include <optional>

namespace mlir::carts::sde {

struct SdeCommittedLayoutCostRecord {
  int64_t arrayId = -1;
  LayoutGraphRole role = LayoutGraphRole::unknown;
  int64_t bytes = 0;
};

struct SdeCommVolumeCost {
  explicit SdeCommVolumeCost(Operation *op);

  void compute();

  bool isInvalidated(const AnalysisManager::PreservedAnalyses &pa);

  int64_t estimateCandidateBytes(const ArrayAccessProfile &profile,
                                 const ArrayLayoutCandidate &candidate) const;

  std::optional<int64_t> getCommittedLayoutBytes(SdeSuIterateOp op,
                                                 int64_t arrayId,
                                                 LayoutGraphRole role) const;

private:
  Operation *operation;
  llvm::DenseMap<Operation *, SmallVector<SdeCommittedLayoutCostRecord, 4>>
      committedBytesBySuAndArray;
};

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_ANALYSIS_SDE_COMM_VOLUME_COST_H
