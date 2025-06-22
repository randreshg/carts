///==========================================================================
/// File: DbDataFlowAnalysis.cpp
///
/// Implementation of dataflow analysis for DbGraph.
/// Handles the analysis of data dependencies between DbNodes.
///==========================================================================

#include "arts/Analysis/Db/DbDataFlowAnalysis.h"
#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Db/DbInfo.h"
#include "arts/Analysis/Db/Graph/DbGraph.h"
#include "arts/Analysis/Db/Graph/DbNode.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "db-dataflow-analysis"
#define dbgs() llvm::dbgs()
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;

int DbDataFlowAnalysis::analysisDepth = 0;

void DbDataFlowAnalysis::analyze() {
  LLVM_DEBUG(DBGS() << std::string(analysisDepth * 2, ' ')
                    << "- Starting Dataflow analysis for function: "
                    << graph->getFunction().getName() << "\n");

  /// Initialize environment and process the function body
  DbEnvironment env;
  processRegion(graph->getFunction().getBody(), env);

  LLVM_DEBUG(DBGS() << std::string(analysisDepth * 2, ' ')
                    << "Finished Dataflow analysis for function: "
                    << graph->getFunction().getName() << "\n");
}

std::pair<DbEnvironment, bool>
DbDataFlowAnalysis::processRegion(Region &region, DbEnvironment &env) {
  DbEnvironment newEnv = env;
  bool changed = false;
  for (Operation &op : region.getOps()) {
    IndentScope scope;
    std::pair<DbEnvironment, bool> result;
    if (auto edtOp = dyn_cast<EdtOp>(&op))
      result = processEdt(edtOp, newEnv);
    else if (auto ifOp = dyn_cast<scf::IfOp>(&op))
      result = processIf(ifOp, newEnv);
    else if (auto forOp = dyn_cast<scf::ForOp>(&op))
      result = processFor(forOp, newEnv);
    else if (auto whileOp = dyn_cast<scf::WhileOp>(&op))
      result = processWhile(whileOp, newEnv);
    else if (auto callOp = dyn_cast<func::CallOp>(&op))
      result = processCall(callOp, newEnv);
    else
      continue;

    /// Merge the new environment with the current one
    newEnv = mergeEnvironments(newEnv, result.first);
    changed = changed || result.second;
  }
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << " - Finished processing region. Environment changed: "
                    << (changed ? "true" : "false") << "\n");
  return {newEnv, changed};
}

std::pair<DbEnvironment, bool>
DbDataFlowAnalysis::processEdt(EdtOp edtOp, DbEnvironment &env) {
  static int edtCount = 0;
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing EDT #" << edtCount++ << "\n");

  /// Process the inputs and outputs of the EDT
  auto edtDeps = edtOp.getDependencies();
  bool changed = false;
  DbEnvironment newEnv = env;

  /// Handle input dependencies
  if (!edtDeps.empty()) {
    LLVM_DEBUG({
      dbgs() << std::string(analysisDepth * 2, ' ')
             << " Initial environment: {";
      for (auto entry : newEnv) {
        if (auto *dbInfo = graph->getNode(entry.first.getOperation())) {
          dbgs() << "  #" << dbInfo->getHierId() << " -> #"
                 << entry.second->getHierId() << ",";
        }
      }
      dbgs() << "}\n";
    });

    /// Reserve dbIns and dbOuts for input and output datablocks
    SmallVector<DbDepNode *, 4> dbIns, dbOuts;
    dbIns.reserve(edtDeps.size());
    dbOuts.reserve(edtDeps.size());

    /// Iterate over the dependencies to fill dbIns and dbOuts
    for (auto dep : edtDeps) {
      mlir::Operation *defOp = dep.getDefiningOp();
      if (!defOp) {
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "Warning: Dependency has no defining operation, skipping\n");
        continue;
      }
      
      // All dependencies should now be DbDepOp operations
      auto db = dyn_cast<DbDepOp>(defOp);
      if (!db) {
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "Warning: Expected DbDepOp but got " << defOp->getName() << ", skipping\n");
        continue;
      }
      
      /// Retrieve the db node and categorize based on mode
      auto *dbDepNode = graph->getDepNode(db);
      if (!dbDepNode) {
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "Warning: Could not find DbDepNode for DbDepOp, skipping\n");
        continue;
      }
      
      if (dbDepNode->isWriter())
        dbOuts.push_back(dbDepNode);
      if (dbDepNode->isReader())
        dbIns.push_back(dbDepNode);
    }

    /// Process input datablocks
    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                      << "  Processing EDT inputs\n");
    for (auto *dbIn : dbIns) {
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                        << "  - Examining DB #" << dbIn->getHierId()
                        << " as input\n");
      auto prodDefs = findDefinition(*dbIn, newEnv);
      if (prodDefs.empty()) {
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "    - No previous definition for DB #"
                          << dbIn->getHierId()
                          << ", add edge from entry node\n");
        continue;
      }
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ') << "Found "
                        << prodDefs.size() << " definitions for DB #"
                        << dbIn->getHierId() << "\n");
      /// Analyze dependencies from producer definitions
      for (auto &prodDef : prodDefs) {
        if (mayDepend(*prodDef, *dbIn)) {
          LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                            << "Adding edge from DB #" << prodDef->getHierId()
                            << " to DB #" << dbIn->getHierId() << "\n");
          bool res = graph->addEdge(dyn_cast<DbDepOp>(prodDef->getOp()),
                                    dyn_cast<DbDepOp>(dbIn->getOp()),
                                    DbDepType::ReadWrite);
          LLVM_DEBUG({
            if (res) {
              dbgs() << std::string(analysisDepth * 2, ' ')
                     << "Edge added successfully\n";
            } else {
              dbgs() << std::string(analysisDepth * 2, ' ')
                     << "Edge already exists, skipping\n";
            }
          });
        }
      }
    }

    /// Process output datablocks
    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                      << "  Processing EDT outputs\n");
    for (auto *dbOut : dbOuts) {
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                        << "  - Examining DB #" << dbOut->getHierId()
                        << " as output\n");
      auto prodDefs = findDefinition(*dbOut, newEnv);
      if (prodDefs.empty()) {
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "    - No previous definition for DB #"
                          << dbOut->getHierId()
                          << ", updating environment with new definition\n");
        newEnv[dyn_cast<DbDepOp>(dbOut->getOp())] = dbOut;
        changed |= true;
        continue;
      }
      for (auto &prodDef : prodDefs) {
        if (mayDepend(*prodDef, *dbOut))
          continue;
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "Updating environment: DB #" << dbOut->getHierId()
                          << " now defined from DB #" << prodDef->getHierId()
                          << "\n");
        newEnv[dyn_cast<DbDepOp>(prodDef->getOp())] = dbOut;
        changed |= true;
      }
    }
  }

  // Process the EDT body region
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  Processing EDT body region\n");
  {
    IndentScope bodyScope;
    DbEnvironment newEdtEnv;
    processRegion(edtOp.getBody(), newEdtEnv);
    /// TODO: Integrate parent/child environment relationships if needed
  }

  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "Finished processing EDT. Environment changed: "
                    << (changed ? "true" : "false") << "\n");
  return {newEnv, changed};
}

std::pair<DbEnvironment, bool>
DbDataFlowAnalysis::processFor(scf::ForOp forOp, DbEnvironment &env) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing ForOp loop\n");
  DbEnvironment loopEnv = env;
  bool overallChanged = false;
  size_t prevEdgeCount = graph->getNumEdges();
  unsigned iterationCount = 0;
  constexpr unsigned maxIterations = 5;

  /// Iterate until a fixed-point is reached or maximum iterations are hit
  while (true) {
    ++iterationCount;
    /// Process the loop body region
    auto bodyResult = processRegion(forOp->getRegion(0), loopEnv);
    DbEnvironment newLoopEnv = mergeEnvironments(loopEnv, bodyResult.first);
    overallChanged |= bodyResult.second;
    size_t currEdgeCount = graph->getNumEdges();

    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ') << "  - Iteration "
                      << iterationCount << " completed: edges=" << currEdgeCount
                      << "\n");

    /// Break if no changes were made and the edge count is stable
    if (!bodyResult.second && (currEdgeCount == prevEdgeCount))
      break;

    /// If maximum iterations reached without new edges, exit loop
    if (iterationCount >= maxIterations && (currEdgeCount == prevEdgeCount))
      break;

    /// Prepare for next iteration
    loopEnv = std::move(newLoopEnv);
    prevEdgeCount = currEdgeCount;
    LLVM_DEBUG(
        dbgs() << std::string(analysisDepth * 2, ' ')
               << "  - Loop environment updated, iterating fixed-point...\n");
  }
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Finished processing ForOp loop\n");
  return {loopEnv, overallChanged};
}

std::pair<DbEnvironment, bool>
DbDataFlowAnalysis::processWhile(scf::WhileOp whileOp, DbEnvironment &env) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing WhileOp loop\n");
  DbEnvironment loopEnv = env;
  bool overallChanged = false;
  size_t prevEdgeCount = graph->getNumEdges();
  unsigned iterationCount = 0;
  constexpr unsigned maxIterations = 5;

  /// Iterate until a fixed-point is reached or maximum iterations are hit
  while (true) {
    ++iterationCount;

    auto beforeResult = processRegion(whileOp.getBefore(), loopEnv);
    auto afterResult = processRegion(whileOp.getAfter(), beforeResult.first);

    DbEnvironment newLoopEnv = mergeEnvironments(loopEnv, afterResult.first);
    bool changedInIteration = beforeResult.second || afterResult.second;
    overallChanged |= changedInIteration;
    size_t currEdgeCount = graph->getNumEdges();

    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ') << "  - Iteration "
                      << iterationCount << " completed: edges=" << currEdgeCount
                      << "\n");

    /// Break if no changes were made and the edge count is stable
    if (!changedInIteration && (currEdgeCount == prevEdgeCount))
      break;

    /// If maximum iterations reached without new edges, exit loop
    if (iterationCount >= maxIterations && (currEdgeCount == prevEdgeCount))
      break;

    /// Prepare for next iteration
    loopEnv = std::move(newLoopEnv);
    prevEdgeCount = currEdgeCount;
    LLVM_DEBUG(
        dbgs() << std::string(analysisDepth * 2, ' ')
               << "  - Loop environment updated, iterating fixed-point...\n");
  }
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Finished processing WhileOp loop\n");
  return {loopEnv, overallChanged};
}

std::pair<DbEnvironment, bool>
DbDataFlowAnalysis::processIf(scf::IfOp ifOp, DbEnvironment &env) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing IfOp with then and else regions\n");
  auto thenRes = processRegion(ifOp.getThenRegion(), env);
  auto elseRes = processRegion(ifOp.getElseRegion(), env);
  DbEnvironment merged = mergeEnvironments(thenRes.first, elseRes.first);
  bool changed = thenRes.second || elseRes.second;
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  - IfOp regions merged. Environment changed: "
                    << (changed ? "true" : "false") << "\n");
  return {merged, changed};
}

std::pair<DbEnvironment, bool>
DbDataFlowAnalysis::processCall(func::CallOp callOp, DbEnvironment &env) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing CallOp (ignoring for now)\n");
  /// TODO: Implement inter-procedural analysis. This would involve:
  /// 1. Finding the callee function.
  /// 2. Mapping the DbDepOp operands of the call to the arguments of the
  ///    callee's entry block.
  /// 3. Running dataflow analysis on the callee with an initial environment
  ///    that reflects the state of the passed-in datablocks.
  /// 4. Propagating the dataflow information from the callee's return to the
  ///    caller's environment.
  return {env, false};
}

SmallVector<DbDepNode *, 4>
DbDataFlowAnalysis::findDefinition(DbDepNode &dbNode, DbEnvironment &env) {
  IndentScope scope;
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  Searching for definitions for DB #"
                    << dbNode.getHierId() << "\n");
  SmallVector<DbDepNode *, 4> defs;
  for (auto pair : env) {
    if (!analysis->dbAliasAnalysis->mayAlias(*pair.second, dbNode))
      continue;
    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                      << "  - Potential definition found: DB #"
                      << pair.second->getHierId() << "\n");
    /// Pessimistically assume alias implies potential dependency
    defs.push_back(pair.second);
  }
  return defs;
}

DbEnvironment DbDataFlowAnalysis::mergeEnvironments(const DbEnvironment &env1,
                                                    const DbEnvironment &env2) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  Merging environments\n");
  DbEnvironment mergedEnv = env1;
  for (auto &pair : env2) {
    auto dbOp = pair.first;
    auto dbNode = pair.second;
    if (mergedEnv.count(dbOp)) {
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                        << "  - DB already exists in merged environment: "
                        << dbNode->getHierId() << "\n");
      auto *node = graph->getDepNode(dbOp);
      /// If they belong to the same parent
      if (node && (dbNode->getEdtParent() == node->getEdtParent())) {
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "    - Same parent, "
                          << "updating definition\n");
        mergedEnv[dbOp] = dbNode;
      }
    } else {
      /// Add new definition into the merged environment
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                        << "  - Adding DB #" << dbNode->getHierId()
                        << " to merged environment\n");
      mergedEnv[dbOp] = dbNode;
    }
  }
  /// Debug final merged environment
  LLVM_DEBUG({
    dbgs() << std::string(analysisDepth * 2, ' ')
           << "  - Final merged environment: {";
    for (auto &pair : mergedEnv) {
      if (auto *dbInfo = graph->getDepNode(pair.first)) {
        dbgs() << " #" << dbInfo->getHierId() << " -> #"
               << pair.second->getHierId() << ",";
      }
    }
    dbgs() << "}\n";
  });
  return mergedEnv;
}

/// Dependency analysis.
bool DbDataFlowAnalysis::mayDepend(DbDepNode &prod, DbDepNode &cons) {
  /// Verify if they belong to the same EDT parent.
  if (prod.getEdtParent() != cons.getEdtParent())
    return false;

  /// Only consider writer producer and reader consumer.
  if (!prod.isWriter() || !cons.isReader())
    return false;

  /// Check if nodes may alias.
  return analysis->dbAliasAnalysis->mayAlias(prod, cons);
}
