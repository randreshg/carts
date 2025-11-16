///==========================================================================///
/// File: DbAnalysis.cpp
/// Implementation of the DbAnalysis class for DB operation analysis.
///==========================================================================///

#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "mlir/Analysis/DataFlow/DeadCodeAnalysis.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "llvm/Support/Debug.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_analysis);

using namespace mlir;
using namespace mlir::arts;

DbAnalysis::DbAnalysis(ArtsAnalysisManager &AM) : ArtsAnalysis(AM) {
  ARTS_DEBUG("Initializing DbAnalysis");
  dbAliasAnalysis = std::make_unique<DbAliasAnalysis>(this);
}

DbAnalysis::~DbAnalysis() { ARTS_DEBUG("Destroying DbAnalysis"); }

DbGraph &DbAnalysis::getOrCreateGraph(func::FuncOp func) {
  ARTS_INFO("Getting or creating DbGraph for function: " << func.getName());
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end())
    return *it->second.get();

  ARTS_DEBUG(" - Creating new DbGraph for function: " << func.getName());
  auto newGraph = std::make_unique<DbGraph>(func, this);

  /// Build nodes and dependencies
  newGraph->build();

  /// Store the graph
  DbGraph *graphPtr = newGraph.get();
  functionGraphMap[func] = std::move(newGraph);
  return *graphPtr;
}

bool DbAnalysis::invalidateGraph(func::FuncOp func) {
  ARTS_INFO("Invalidating DbGraph for function: " << func.getName());
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end()) {
    functionGraphMap.erase(it);
    if (dbAliasAnalysis)
      dbAliasAnalysis->resetCache();
    return true;
  }
  return false;
}

void DbAnalysis::invalidate() {
  ARTS_INFO("Invalidating all DbGraphs");
  functionGraphMap.clear();
  if (dbAliasAnalysis)
    dbAliasAnalysis->resetCache();
}

void DbAnalysis::print(func::FuncOp func) {
  ARTS_INFO("Printing DbGraph for function: " << func.getName());
  DbGraph &graph = getOrCreateGraph(func);
  graph.print(ARTS_DBGS());
}

LoopAnalysis *DbAnalysis::getLoopAnalysis() {
  return &getAnalysisManager().getLoopAnalysis();
}
