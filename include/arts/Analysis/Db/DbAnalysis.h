///==========================================================================///
/// File: DbAnalysis.h
///
/// This file defines the central analysis for DB operations.
///==========================================================================///

#ifndef ARTS_ANALYSIS_DB_DBANALYSIS_H
#define ARTS_ANALYSIS_DB_DBANALYSIS_H

#include "arts/Analysis/ArtsAnalysis.h"
#include "mlir/Analysis/DataFlowFramework.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/DenseMap.h"

// Forward declarations
namespace mlir {
namespace arts {
class DbGraph;
class DbAliasAnalysis;
class LoopAnalysis;
class DbAcquireOp;
class ArtsAnalysisManager;
} // namespace arts
} // namespace mlir

namespace mlir {
namespace arts {

/// Central manager for DB-related analyses across a module.
class ArtsAnalysisManager;

class DbAnalysis : public ArtsAnalysis {
public:
  DbAnalysis(ArtsAnalysisManager &AM);

  ~DbAnalysis();

  /// Get or create DbGraph for a function, building if needed.
  DbGraph &getOrCreateGraph(func::FuncOp func);

  /// Invalidate graph for a function.
  bool invalidateGraph(func::FuncOp func);

  /// Invalidate all graphs
  void invalidate() override;

  /// Print analysis for a function.
  void print(func::FuncOp func);

  /// Access analyses
  DbAliasAnalysis *getAliasAnalysis() { return dbAliasAnalysis.get(); }
  LoopAnalysis *getLoopAnalysis();
  using ArtsAnalysis::getAnalysisManager;

private:
  llvm::DenseMap<func::FuncOp, std::unique_ptr<DbGraph>> functionGraphMap;
  std::unique_ptr<DbAliasAnalysis> dbAliasAnalysis;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DBANALYSIS_H
