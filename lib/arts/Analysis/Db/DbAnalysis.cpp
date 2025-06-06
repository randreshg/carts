///==========================================================================
/// File: DbAnalysis.cpp
///
/// Implementation of the DbAnalysis class, serving as the central hub for
/// data block analysis across an MLIR module. It manages DbGraph instances
/// for each function and integrates with alias and loop analyses.
///==========================================================================

#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Db/Graph/DbGraph.h"
#include "arts/Analysis/Db/Graph/DbNode.h"
#include "arts/Analysis/LoopAnalysis.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "db-analysis"
#define dbgs() llvm::dbgs()
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;

DbAnalysis::DbAnalysis(Operation *module) : module(module) {
  LLVM_DEBUG(DBGS() << "Initializing DbAnalysis for module\n");
  loopAnalysis = std::make_unique<LoopAnalysis>(module);
  dbAliasAnalysis = std::make_unique<DbAliasAnalysis>(this);
}

DbAnalysis::~DbAnalysis() { LLVM_DEBUG(DBGS() << "Destroying DbAnalysis\n"); }

DbGraph *DbAnalysis::getOrCreateGraph(func::FuncOp func) {
  LLVM_DEBUG(dbgs() << "Getting or creating DbGraph for function: "
                    << func.getName() << "\n");
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end()) {
    LLVM_DEBUG(dbgs() << "Found existing DbGraph.\n");
    return it->second.get();
  }

  LLVM_DEBUG(dbgs() << "Creating new DbGraph for function: " << func.getName()
                    << "\n");
  auto newGraph = std::make_unique<DbGraph>(func, this);
  newGraph->build();

  DbGraph *graphPtr = newGraph.get();
  functionGraphMap[func] = std::move(newGraph);
  return graphPtr;
}

bool DbAnalysis::invalidateGraph(func::FuncOp func) {
  LLVM_DEBUG(dbgs() << "Invalidating DbGraph for function: " << func.getName()
                    << "\n");
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end()) {
    functionGraphMap.erase(it);
    LLVM_DEBUG(dbgs() << "DbGraph invalidated successfully.\n");
    return true;
  }
  LLVM_DEBUG(dbgs() << "DbGraph not found, could not invalidate.\n");
  return false;
}

void DbAnalysis::print(func::FuncOp func) {
  LLVM_DEBUG(dbgs() << "Printing DbGraph for function: " << func.getName()
                    << "\n");
  DbGraph *graph = getOrCreateGraph(func);
  if (graph) {
    LLVM_DEBUG(dbgs() << "Graph created successfully for " << func.getName()
                      << "\n");
  } else {
    LLVM_DEBUG(dbgs() << "Error: Could not get or create DbGraph for printing "
                      << "function " << func.getName() << "\n");
  }
}