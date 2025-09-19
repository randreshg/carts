///==========================================================================
/// File: ArtsAnalysisManager.h
///
/// This file defines the ArtsAnalysisManager for centralized management
/// of all ARTS analysis objects. Provides a clean interface that exposes
/// only analysis objects, while internally managing graphs through
/// the respective analysis classes.
///==========================================================================

#ifndef ARTS_ANALYSIS_ARTSANALYSISMANAGER_H
#define ARTS_ANALYSIS_ARTSANALYSISMANAGER_H

#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Edt/EdtAnalysis.h"
#include "arts/Analysis/Concurrency/ConcurrencyAnalysis.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/DenseMap.h"
#include <memory>

namespace mlir {

class Pass;

namespace arts {

class DbGraph;
class EdtGraph;
class ConcurrencyGraph;

/// Centralized manager for all ARTS analysis objects.
/// Provides a clean public interface that exposes only analysis objects.
/// Internally manages graphs through the respective analysis classes.
class ArtsAnalysisManager {
public:
  /// Module-level constructor for external ownership and caching across funcs
  ArtsAnalysisManager(ModuleOp module);

  /// Destructor
  ~ArtsAnalysisManager();

  /// Invalidate all analysis objects and graphs
  void invalidate();

  /// Get analysis objects
  DbAnalysis &getDbAnalysis();
  EdtAnalysis &getEdtAnalysis();
  ConcurrencyAnalysis &getConcurrencyAnalysis();

  /// Graph getters (lazy-built and cached per function)
  DbGraph &getDbGraph(func::FuncOp func);
  EdtGraph &getEdtGraph(func::FuncOp func);
  ConcurrencyGraph &getConcurrencyGraph(func::FuncOp func);

  /// Per-function invalidation
  bool invalidateFunction(func::FuncOp func);

  /// Get the module containing the function
  ModuleOp &getModule() { return module; }

  /// Print summary of analysis objects and their graphs
  void print(llvm::raw_ostream &os);

  /// Export analysis objects and graphs to JSON
  void exportToJson(llvm::raw_ostream &os, bool includeAnalysis = false);

private:
  ModuleOp module;
  bool built = false;

  std::unique_ptr<DbAnalysis> dbAnalysis;
  std::unique_ptr<EdtAnalysis> edtAnalysis;
  std::unique_ptr<ConcurrencyAnalysis> concurrencyAnalysis;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_ARTSANALYSISMANAGER_H
