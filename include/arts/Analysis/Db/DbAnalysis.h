///==========================================================================
/// File: DbAnalysis.h
///
/// This pass performs a comprehensive analysis on arts.db operations.
///==========================================================================

#ifndef CARTS_ANALYSIS_DBANALYSIS_H
#define CARTS_ANALYSIS_DBANALYSIS_H

#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Db/DbDataFlowAnalysis.h"
#include "arts/Analysis/LoopAnalysis.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/AnalysisManager.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/PassManager.h"

namespace mlir {
namespace arts {

//===----------------------------------------------------------------------===//
// DbAnalysis
// This is a pass that performs a comprehensive analysis on arts.db
// operations.It builds a dependency graph for each function in the module
// and provides methods to query the graph.
//===----------------------------------------------------------------------===//
class DbAnalysis {
  friend class DbGraph;
  friend class DbAliasAnalysis;
  friend class DbDataFlowAnalysis;

public:
  explicit DbAnalysis(Operation *module);
  ~DbAnalysis();

  /// Get the graph for a given function, or create it if it does not exist.
  DbGraph *getOrCreateGraph(func::FuncOp func);
  bool invalidateGraph(func::FuncOp func);

  bool isInvalidated(const AnalysisManager::PreservedAnalyses &pa) {
    return !pa.isPreserved<DbAnalysis>();
  }

private:
  ModuleOp module;

  /// Map from function to its dependency graph.
  DenseMap<func::FuncOp, std::unique_ptr<DbGraph>> functionGraphMap;

  /// Other
  void print(func::FuncOp func);

  /// Child Analyses
  std::unique_ptr<DbAliasAnalysis> dbAliasAnalysis;
  std::unique_ptr<LoopAnalysis> loopAnalysis;
};
} // namespace arts
} // namespace mlir

#endif // CARTS_ANALYSIS_DBANALYSIS_H