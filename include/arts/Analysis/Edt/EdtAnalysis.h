///==========================================================================
/// File: EdtAnalysis.h
///
/// This pass performs a comprehensive analysis on arts.edt operations.
/// It also provides helpers to analyze regions and find dependencies,
/// parameters, and constants.
///==========================================================================

#ifndef CARTS_ANALYSIS_EDTANALYSIS_H
#define CARTS_ANALYSIS_EDTANALYSIS_H

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"

#include "arts/Analysis/Edt/EdtInfo.h"

namespace mlir {
namespace arts {

class EdtOp;

///==========================================================================
/// EdtAnalysis Class Declaration
///==========================================================================
// class EdtAnalysis {
// public:
//   explicit EdtAnalysis(Operation *module);

// private:
// };

} // namespace arts
} // namespace mlir

namespace mlir {
namespace arts {

class DbGraph;
class EdtGraph;

//==========================================================================
// EdtAnalysis: per-EDT summaries and pairwise affinity metrics
//==========================================================================

class EdtAnalysis {
public:
  explicit EdtAnalysis(Operation *module, DbGraph *db = nullptr,
                       EdtGraph *edt = nullptr);

  /// Build summaries for all EDTs in the module.
  void analyze();

  /// Compute affinity metrics for two EDTs using their summaries.
  EdtPairAffinity affinity(EdtOp from, EdtOp to) const;

  /// Debug printing
  void print(func::FuncOp func, llvm::raw_ostream &os) const;
  void toJson(func::FuncOp func, llvm::raw_ostream &os) const;

private:
  ModuleOp module;
  DbGraph *dbGraph;
  EdtGraph *edtGraph;

  /// Order index per EDT in a function
  DenseMap<EdtOp, unsigned> edtOrderIndex;

  void analyzeFunc(func::FuncOp func);

  /// Helpers
  static uint64_t estimateMaxLoopDepth(Region &region);
};

} // namespace arts
} // namespace mlir

#endif // CARTS_ANALYSIS_EDTANALYSIS_H