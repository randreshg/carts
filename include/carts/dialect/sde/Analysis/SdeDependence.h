///==========================================================================///
/// File: SdeDependence.h
///
/// Phase B substrate: memref dependence distance vectors for SDE planning.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_ANALYSIS_SDE_DEPENDENCE_H
#define CARTS_DIALECT_SDE_ANALYSIS_SDE_DEPENDENCE_H

#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/AnalysisManager.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir::carts::sde {

struct SdeSelfDependence {
  Value root;
  SmallVector<int64_t, 4> minDistance;
  SmallVector<int64_t, 4> maxDistance;

  bool isLoopCarried() const;
};

struct SdeDependence {
  explicit SdeDependence(Operation *op);

  void compute();

  bool isInvalidated(const AnalysisManager::PreservedAnalyses &pa);

  ArrayRef<SdeSelfDependence> getSelfDependences(SdeSuIterateOp op) const;
  bool hasLoopCarriedSelfDependence(SdeSuIterateOp op) const;

private:
  Operation *operation;
  llvm::DenseMap<Operation *, SmallVector<SdeSelfDependence, 2>>
      selfDependencesBySu;
};

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_ANALYSIS_SDE_DEPENDENCE_H
