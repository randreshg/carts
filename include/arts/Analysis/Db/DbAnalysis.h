//===----------------------------------------------------------------------===//
// Db/DbAnalysis.h - Central hub for DB analysis
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_DB_DBANALYSIS_H
#define ARTS_ANALYSIS_DB_DBANALYSIS_H

#include "mlir/IR/Operation.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Analysis/DataFlow/Solver.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Db/DbDataFlowAnalysis.h"
#include "arts/Analysis/Db/DbValueDataFlowAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/LoopAnalysis.h"  // Assuming this exists for loop integration

namespace mlir {
namespace arts {

// DbAnalysis
// ---------------------------------------------------------------------------
// Responsibility
// - Central manager for DB-related analyses across a module
// - Owns per-function DbGraphs and exposes high-level queries
// - Hosts alias/dataflow/loop analyses when enabled
// Non-Goals
// - Does not mutate IR
// - Does not build EDT graphs (kept in EDT analysis)
// Design
// - Lazy graph creation per func; explicit invalidation API
// - Future: integrate dense/sparse dataflow and alias analyses
// Notes
// - Dimension/shape inference disabled for now to keep runtime lean
class DbAnalysis {
public:
  DbAnalysis(Operation *module);

  ~DbAnalysis();

  // Get or create DbGraph for a function, building if needed.
  DbGraph *getOrCreateGraph(func::FuncOp func);

  // Invalidate graph for a function.
  bool invalidateGraph(func::FuncOp func);

  // Print analysis for a function.
  void print(func::FuncOp func);

  // Compute dimension sizes for a node (DISABLED - shape inference removed).
  // SmallVector<int64_t> getComputedDimSizes(const NodeBase &node);

  // Get dimension pattern analysis for a node (DISABLED).
  // SmallVector<DimensionAnalysis> getDimensionAnalysis(const NodeBase &node);

  // Access alias analysis.
  DbAliasAnalysis *getAliasAnalysis() { return dbAliasAnalysis.get(); }

  // Access loop analysis.
  LoopAnalysis *getLoopAnalysis() { return loopAnalysis.get(); }

  // Access dataflow solver for analyses wiring.
  DataFlowSolver &getSolver() { return solver; }

  // ---- Future, alias-aware overlap and slice volumes (stubs for later) ----
  enum class OverlapKind { Unknown, Disjoint, Partial, Full };
  struct SliceInfo {
    SmallVector<int64_t, 4> offsets; // INT64_MIN if unknown
    SmallVector<int64_t, 4> sizes;   // INT64_MIN if unknown
    uint64_t estimatedBytes = 0;     // 0 if unknown
  };

  // Compute slice info for an acquire (best-effort; constants only for now).
  SliceInfo computeSliceInfo(arts::DbAcquireOp acquire);

  // Coarse overlap classification between two acquires (constants only for now).
  OverlapKind estimateOverlap(arts::DbAcquireOp a, arts::DbAcquireOp b);

private:
  Operation *module;
  DenseMap<func::FuncOp, std::unique_ptr<DbGraph>> functionGraphMap;
  std::unique_ptr<DbAliasAnalysis> dbAliasAnalysis;
  std::unique_ptr<LoopAnalysis> loopAnalysis;
  DataFlowSolver solver;  // Solver for data flow analyses
  // Future: node placement hints for DbAllocOps; consumed by placement tools
  // and passes to set the 'node' attribute on DbAllocOp.
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DBANALYSIS_H