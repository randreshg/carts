//===----------------------------------------------------------------------===//
// Db/DbDataFlowAnalysis.h - Dense forward lifetime/dep builder
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_DB_DBDATAFLOWANALYSIS_H
#define ARTS_ANALYSIS_DB_DBDATAFLOWANALYSIS_H

#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/ArtsDialect.h"
#include "mlir/Analysis/DataFlow/DenseAnalysis.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

namespace mlir {
namespace arts {

struct DbState : public dataflow::AbstractDenseLattice {
  using AbstractDenseLattice::AbstractDenseLattice;
  ChangeResult join(const AbstractDenseLattice &rhs) override;
  void print(llvm::raw_ostream &os) const override;

  DenseSet<DbAllocOp> liveAllocs;
  DenseMap<DbAllocOp, DenseSet<DbAcquireOp>> activeAcquires;
  DenseMap<DbAllocOp, DenseSet<DbReleaseOp>> pendingReleases;
};

class DbAnalysis;

class DbDataFlowAnalysis
    : public dataflow::DenseForwardDataFlowAnalysis<DbState> {
public:
  DbDataFlowAnalysis(DataFlowSolver &solver, DbGraph *graph,
                     DbAnalysis *analysis);

  void visitOperation(Operation *op, const DbState &before,
                      DbState *after) override;

  // Required by DenseForwardDataFlowAnalysis interface
  void setToEntryState(DbState *lattice) override;

  void analyze();

private:
  void createLifetimeEdge(DbAcquireNode *acquire, DbReleaseNode *release);
  SmallVector<MemoryEffects::EffectInstance> getDbEffects(Operation *op);

  DbGraph *graph;
  DbAnalysis *analysis;
  DbAliasAnalysis aliasAA;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DBDATAFLOWANALYSIS_H