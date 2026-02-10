///==========================================================================///
/// File: EdtAnalysis.h
///
/// This pass performs a comprehensive analysis on arts.edt operations.
/// It also provides helpers to analyze regions and find dependencies,
/// parameters, and constants.
///==========================================================================///

#ifndef CARTS_ANALYSIS_EDTANALYSIS_H
#define CARTS_ANALYSIS_EDTANALYSIS_H

#include "arts/Analysis/ArtsAnalysis.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLFunctionalExtras.h"

#include "arts/Analysis/Edt/EdtInfo.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"

/// Forward declarations
namespace mlir {
namespace arts {
class ArtsAnalysisManager;
class DbGraph;

///==========================================================================///
/// EdtAnalysis: per-EDT summaries and pairwise affinity metrics
///==========================================================================///

class EdtAnalysis : public ArtsAnalysis {
public:
  EdtAnalysis(ArtsAnalysisManager &AM);

  /// Build summaries for all EDTs in the module.
  void analyze();

  /// Debug printing
  void print(func::FuncOp func, llvm::raw_ostream &os);
  void toJson(func::FuncOp func, llvm::raw_ostream &os);

  /// Get EDT-level summary distribution pattern.
  std::optional<EdtDistributionPattern> getEdtDistributionPattern(EdtOp edt);

  /// Get DB access pattern inferred from EDT-level loop access analysis.
  /// Returns std::nullopt when the allocation was not observed in analyzed
  /// loops.
  std::optional<DbAccessPattern> getAllocAccessPattern(Operation *allocOp);

  /// Iterate all analyzed DB allocation access patterns.
  void forEachAllocAccessPattern(
      llvm::function_ref<void(Operation *, DbAccessPattern)> fn);

  /// Graph accessor
  EdtGraph &getOrCreateEdtGraph(func::FuncOp func);

  /// Invalidate EDT graph for a specific function
  bool invalidateGraph(func::FuncOp func);

  /// Invalidate internal caches
  void invalidate() override;

  using ArtsAnalysis::getAnalysisManager;

private:
  /// Order index per EDT in a function
  DenseMap<EdtOp, unsigned> edtOrderIndex;
  DenseMap<Operation *, EdtDistributionPattern> edtPatternByOp;
  DenseMap<Operation *, DbAccessPattern> allocPatternByOp;
  bool analyzed = false;

  /// Per-function graph caches
  llvm::DenseMap<func::FuncOp, std::unique_ptr<EdtGraph>> edtGraphs;

  void analyzeFunc(func::FuncOp func);
};

} // namespace arts
} // namespace mlir

#endif // CARTS_ANALYSIS_EDTANALYSIS_H
