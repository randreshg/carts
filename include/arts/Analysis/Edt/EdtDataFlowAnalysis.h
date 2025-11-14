///==========================================================================///
/// File: EdtDataFlowAnalysis.h
/// Dataflow-based dependency analysis that produces EDT-level edges.
///==========================================================================///

#ifndef ARTS_ANALYSIS_EDT_EDTDATAFLOWANALYSIS_H
#define ARTS_ANALYSIS_EDT_EDTDATAFLOWANALYSIS_H

#include "arts/Analysis/Edt/EdtInfo.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Graphs/Edt/EdtEdge.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace arts {

class DbAliasAnalysis;
class DbAnalysis;

struct EdtDependency {
  EdtOp from;
  EdtOp to;
  SmallVector<DbEdgeSlice, 2> slices;
};

/// Performs a lightweight dataflow analysis over EDT regions to determine
/// dependencies between tasks based on their datablock acquires.
class EdtDataFlowAnalysis {
public:
  EdtDataFlowAnalysis(DbGraph *dbGraph, DbAnalysis *analysis);

  SmallVector<EdtDependency, 16> run(func::FuncOp func);

private:
  struct Environment {
    llvm::DenseMap<DbAllocNode *, llvm::SetVector<DbAcquireNode *>> writers;
    llvm::DenseMap<DbAllocNode *, llvm::SetVector<DbAcquireNode *>> readers;
  };

  std::pair<Environment, bool> processRegion(Region &region,
                                             Environment &env);
  std::pair<Environment, bool> processEdt(EdtOp edtOp, Environment &env);
  std::pair<Environment, bool> processEpoch(EpochOp epochOp, Environment &env);
  std::pair<Environment, bool> processIf(scf::IfOp ifOp, Environment &env);
  std::pair<Environment, bool> processFor(scf::ForOp forOp, Environment &env);

  SmallVector<DbAcquireNode *> findDefinitions(DbAcquireNode *acquire,
                                               Environment &env);
  bool unionInto(Environment &target, const Environment &source);
  bool mayDepend(DbAcquireNode *producer, DbAcquireNode *consumer);

  void recordDependency(DbAcquireNode *producer, DbAcquireNode *consumer,
                        DbDepType depType);
  void recordDependency(DbAcquireNode *reader, DbAcquireNode *writer);

  DbAcquireUsage buildUsage(DbAcquireNode *node);

  DbGraph *dbGraph = nullptr;
  DbAnalysis *analysis = nullptr;
  DbAliasAnalysis *aliasAA = nullptr;

  llvm::DenseMap<std::pair<Operation *, Operation *>,
                 SmallVector<DbEdgeSlice, 2>>
      dependencyMap;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_EDT_EDTDATAFLOWANALYSIS_H
