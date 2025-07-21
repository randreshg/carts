//===----------------------------------------------------------------------===//
// Db/DbDataFlowAnalysis.h - Data flow analysis for DB operations
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_DB_DBDATAFLOWANALYSIS_H
#define ARTS_ANALYSIS_DB_DBDATAFLOWANALYSIS_H

#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/ArtsDialect.h"
#include "mlir/Analysis/DataFlow/DenseAnalysis.h"
#include "mlir/Analysis/DataFlow/DeadCodeAnalysis.h"
#include "mlir/Analysis/DataFlow/SparseConstantPropagation.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/raw_ostream.h"

namespace mlir {
namespace arts {

// Lattice element for DB state: tracks live allocations, active acquires, releases.
struct DbState : public dataflow::AbstractDenseLattice {
  DbState() : AbstractDenseLattice() {}
  ChangeResult join(const AbstractDenseLattice &rhs) override;
  void print(llvm::raw_ostream &os) const override;

  DenseSet<DbAllocOp> liveAllocs;
  DenseMap<DbAllocOp, DenseSet<DbAcquireOp>> activeAcquires;
  DenseMap<DbAllocOp, DenseSet<DbReleaseOp>> pendingReleases;
};

// DbDataFlowAnalysis: Dense forward data flow analysis for DB lifetime and dependencies.
// Enhancements:
// - Leverages MLIR's DenseForwardDataFlowAnalysis for state propagation.
// - Tracks live allocs, acquires, releases to detect mismatches and create graph edges.
// - Integrates DbAliasAnalysis for precise dependency detection.
// - Builds lifetime edges in DbGraph (e.g., acquire -> release if matching).
// Potential problems: Over-conservatism in aliases leads to extra edges; state explosion in large functions.
// Improvements: Combine with sparse analysis for scalability; add backward pass for def-use.
// TODO: Support inter-procedural analysis by propagating states across calls.
// TODO: Detect and diagnose errors like unmatched acquire/release or access after release.
// TODO: Incorporate shape inference to refine size-based overlaps in state tracking.
// TODO: Extend to multi-threaded models for concurrency-aware flow (e.g., per-thread states).
class DbDataFlowAnalysis : public dataflow::DenseForwardDataFlowAnalysis<DbState> {
public:
  DbDataFlowAnalysis(DataFlowSolver &solver, DbGraph *graph, DbAnalysis *analysis);

  void visitOperation(Operation *op, const DbState &before, DbState *after) override;

  // Run the analysis and populate the graph with dependencies.
  void analyze();

private:
  // Create lifetime edge if acquire and release match (via alias).
  void createLifetimeEdge(DbAcquireNode *acquire, DbReleaseNode *release);

  // Get side effects for DB ops.
  SmallVector<MemoryEffects::EffectInstance> getDbEffects(Operation *op);

  DbGraph *graph;
  DbAnalysis *analysis;
  DbAliasAnalysis aliasAA;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DBDATAFLOWANALYSIS_H