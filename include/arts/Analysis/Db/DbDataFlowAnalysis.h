///==========================================================================
/// File: DbDataFlowAnalysis.h
///
/// DbDataFlowAnalysis handles the analysis of data dependencies between
/// DbNodes. It processes regions, EDTs, loops, and other control flow
/// structures to build a complete picture of data dependencies.
///==========================================================================

#ifndef ARTS_ANALYSIS_DB_DBDATAFLOWANALYSIS_H
#define ARTS_ANALYSIS_DB_DBDATAFLOWANALYSIS_H

#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Region.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace arts {

class DbAnalysis;
class DbGraph;
class DbDepNode;

/// Environment represents the current state of DB definitions
/// at a particular point in the program.
using DbEnvironment = DenseMap<DbDepOp, DbDepNode *>;

class DbDataFlowAnalysis {
public:
  DbDataFlowAnalysis(DbAnalysis *analysis, DbGraph *graph)
      : analysis(analysis), graph(graph) {
    assert(analysis && "Analysis cannot be null");
    assert(graph && "Graph cannot be null");
  }

  /// Main entry point for data flow analysis
  void analyze();

  /// Process a region and return the resulting environment
  std::pair<DbEnvironment, bool> processRegion(Region &region,
                                               DbEnvironment &env);

  /// Process different types of operations
  std::pair<DbEnvironment, bool> processEdt(EdtOp edtOp, DbEnvironment &env);
  std::pair<DbEnvironment, bool> processFor(scf::ForOp forOp,
                                            DbEnvironment &env);
  std::pair<DbEnvironment, bool> processWhile(scf::WhileOp whileOp,
                                              DbEnvironment &env);
  std::pair<DbEnvironment, bool> processIf(scf::IfOp ifOp, DbEnvironment &env);
  std::pair<DbEnvironment, bool> processCall(func::CallOp callOp,
                                             DbEnvironment &env);

  /// Helper methods
  SmallVector<DbDepNode *, 4> findDefinition(DbDepNode &dbNode,
                                                DbEnvironment &env);
  DbEnvironment mergeEnvironments(const DbEnvironment &env1,
                                  const DbEnvironment &env2);

  /// Dependency analysis.
  bool mayDepend(DbDepNode &prod, DbDepNode &cons);

  /// Static accessor for analysis depth (needed by helper functions)
  static int getAnalysisDepth() { return analysisDepth; }

private:
  DbAnalysis *analysis;
  DbGraph *graph;
  static int analysisDepth;

  /// Helper struct for managing debug indentation
  struct IndentScope {
    IndentScope() { ++analysisDepth; }
    ~IndentScope() { --analysisDepth; }
  };
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DBDATAFLOWANALYSIS_H