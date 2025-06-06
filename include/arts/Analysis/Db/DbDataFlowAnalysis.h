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
class DbAccessNode;

/// Environment represents the current state of data block definitions
/// at a particular point in the program.
using Environment = DenseMap<DbAccessOp, DbAccessNode *>;

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
  std::pair<Environment, bool> processRegion(Region &region, Environment &env);

  /// Process different types of operations
  std::pair<Environment, bool> processEdt(EdtOp edtOp, Environment &env);
  std::pair<Environment, bool> processFor(scf::ForOp forOp, Environment &env);
  std::pair<Environment, bool> processIf(scf::IfOp ifOp, Environment &env);
  std::pair<Environment, bool> processCall(func::CallOp callOp,
                                           Environment &env);

  /// Helper methods
  SmallVector<DbAccessNode *, 4> findDefinition(DbAccessNode &dbNode,
                                                Environment &env);
  Environment mergeEnvironments(const Environment &env1,
                                const Environment &env2);

private:
  DbAnalysis *analysis;
  std::unique_ptr<DbGraph> graph;
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