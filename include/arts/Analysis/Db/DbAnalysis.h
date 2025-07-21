//===----------------------------------------------------------------------===//
// Db/DbAnalysis.h - Central hub for DB analysis
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_DB_DBANALYSIS_H
#define ARTS_ANALYSIS_DB_DBANALYSIS_H

#include "mlir/IR/Operation.h"
#include "mlir/IR/Module.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Db/DbDataFlowAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/LoopAnalysis.h"  // Assuming this exists for loop integration

namespace mlir {
namespace arts {

// DbAnalysis: Central manager for DB-related analyses across a module.
// Manages per-function DbGraphs, alias analysis, data flow, and loop analysis.
// Enhancements:
// - Integrates DbDataFlowAnalysis to populate graph edges during build.
// - Provides dimension and size queries, enhanced with shape inference.
// - Supports new DB ops (alloc, acquire, release) without DbDep.
// Potential problems: Module-wide analysis may be memory-heavy; per-function graphs mitigate.
// Improvements: Lazy graph creation; cache invalidation on IR changes.
// TODO: Add inter-procedural analysis by propagating across func calls.
// TODO: Integrate with MLIR's pass manager for automatic invalidation.
// TODO: Enhance dimension analysis with symbolic affine maps for precision.
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

  // Compute dimension sizes for a node (uses shape inference).
  SmallVector<int64_t> getComputedDimSizes(const NodeBase &node);

  // Get dimension pattern analysis for a node.
  SmallVector<DimensionAnalysis> getDimensionAnalysis(const NodeBase &node);

  // Access alias analysis.
  DbAliasAnalysis *getAliasAnalysis() { return dbAliasAnalysis.get(); }

  // Access loop analysis.
  LoopAnalysis *getLoopAnalysis() { return loopAnalysis.get(); }

private:
  Operation *module;
  DenseMap<func::FuncOp, std::unique_ptr<DbGraph>> functionGraphMap;
  std::unique_ptr<DbAliasAnalysis> dbAliasAnalysis;
  std::unique_ptr<LoopAnalysis> loopAnalysis;
  DataFlowSolver solver;  // Solver for data flow analyses
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DBANALYSIS_H