///==========================================================================
/// File: DbAnalysis.h
///
/// This file defines the central analysis for DB operations.
///==========================================================================

#ifndef ARTS_ANALYSIS_DB_DBANALYSIS_H
#define ARTS_ANALYSIS_DB_DBANALYSIS_H

#include "mlir/Analysis/DataFlowFramework.h"
#include "mlir/IR/Operation.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

// Forward declarations
namespace mlir {
namespace arts {
class DbGraph;
class DbAliasAnalysis;
class LoopAnalysis;
class DbDataFlowAnalysis;
class DbAcquireOp;
} // namespace arts
} // namespace mlir

namespace mlir {
namespace arts {

/// Central manager for DB-related analyses across a module.
/// Owns per-function DbGraphs and exposes high-level queries.
/// Hosts alias/dataflow/loop analyses when enabled.
class DbAnalysis {
public:
  DbAnalysis(Operation *module);

  ~DbAnalysis();

  /// Get or create DbGraph for a function, building if needed.
  DbGraph *getOrCreateGraph(func::FuncOp func);

  /// Invalidate graph for a function.
  bool invalidateGraph(func::FuncOp func);

  /// Print analysis for a function.
  void print(func::FuncOp func);

  /// Access alias analysis.
  DbAliasAnalysis *getAliasAnalysis() { return dbAliasAnalysis.get(); }

  /// Access loop analysis.
  LoopAnalysis *getLoopAnalysis() { return loopAnalysis.get(); }

  /// Access dataflow analysis.
  DbDataFlowAnalysis *getDataFlowAnalysis() { return dbDataFlowAnalysis.get(); }

  /// Access dataflow solver for analyses wiring.
  mlir::DataFlowSolver &getSolver() { return solver; }

  /// Overlap classification between DB operations
  enum class OverlapKind { Unknown, Disjoint, Partial, Full };
  
  /// Slice information for DB operations
  struct SliceInfo {
    llvm::SmallVector<int64_t, 4> offsets; ///< INT64_MIN if unknown
    llvm::SmallVector<int64_t, 4> sizes;   ///< INT64_MIN if unknown
    uint64_t estimatedBytes = 0;     ///< 0 if unknown
  };

  /// Compute slice info for an acquire operation.
  SliceInfo computeSliceInfo(arts::DbAcquireOp acquire);

  /// Coarse overlap classification between two acquire operations.
  OverlapKind estimateOverlap(arts::DbAcquireOp a, arts::DbAcquireOp b);

private:
  Operation *module;
  llvm::DenseMap<func::FuncOp, std::unique_ptr<DbGraph>> functionGraphMap;
  std::unique_ptr<DbAliasAnalysis> dbAliasAnalysis;
  std::unique_ptr<LoopAnalysis> loopAnalysis;
  std::unique_ptr<DbDataFlowAnalysis> dbDataFlowAnalysis; ///< Dataflow analysis for DB operations
  mlir::DataFlowSolver solver;  ///< Solver for data flow analyses
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DBANALYSIS_H