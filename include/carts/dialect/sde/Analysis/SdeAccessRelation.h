///==========================================================================///
/// File: SdeAccessRelation.h
///
/// Phase B substrate: access relations on affine-form SDE IR.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_ANALYSIS_SDE_ACCESS_RELATION_H
#define CARTS_DIALECT_SDE_ANALYSIS_SDE_ACCESS_RELATION_H

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/AnalysisManager.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir::carts::sde {

/// Module-scoped access relations recovered from planning-stage SU loop analysis.
struct SdeAccessRelation {
  explicit SdeAccessRelation(Operation *op);

  void compute();

  bool isInvalidated(const AnalysisManager::PreservedAnalyses &pa);

  ArrayRef<AccessRelation> getRelationsForRoot(Value root) const;
  bool empty() const { return relationsByRoot.empty(); }

private:
  Operation *operation;
  llvm::DenseMap<Value, SmallVector<AccessRelation, 4>> relationsByRoot;
};

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_ANALYSIS_SDE_ACCESS_RELATION_H
