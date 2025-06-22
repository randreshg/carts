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

/// Helper function to format debug indentation
static std::string getIndent() {
  return std::string(DbDataFlowAnalysis::getAnalysisDepth() * 2, ' ');
}

/// Helper function to get a safe node ID for debug output
static std::string getNodeId(DbDepNode *node) {
  if (!node)
    return "NULL";
  std::string id = node->getHierId();
  return id.empty()
             ? ("Node@" + std::to_string(reinterpret_cast<uintptr_t>(node)))
             : id;
}

/// Helper function to get operation location string
static std::string getOpLocationStr(Operation *op) {
  if (!op)
    return "unknown";
  std::string locStr;
  llvm::raw_string_ostream stream(locStr);
  op->getLoc().print(stream);
  return locStr;
}

/// Helper function to count operations in a region
static size_t countOpsInRegion(Region &region) {
  size_t count = 0;
  for (auto &_ : region.getOps())
    ++count;
  return count;
}

void DbDataFlowAnalysis::analyze() {
  LLVM_DEBUG(dbgs() << "----------------------------------------\n";
             DBGS() << "Starting Dataflow analysis for function: '"
                    << graph->getFunction().getName() << "'\n";);

  /// Initialize environment and process the function body
  DbEnvironment env;
  processRegion(graph->getFunction().getBody(), env);

  LLVM_DEBUG(DBGS() << "Finished Dataflow analysis for function: '"
                    << graph->getFunction().getName() << "'\n");
}

std::pair<DbEnvironment, bool>
DbDataFlowAnalysis::processRegion(Region &region, DbEnvironment &env) {
  LLVM_DEBUG(dbgs() << getIndent() << "┌─ Processing region with "
                    << countOpsInRegion(region) << " operations\n");

  DbEnvironment newEnv = env;
  bool changed = false;
  size_t opCount = 0;

  for (Operation &op : region.getOps()) {
    IndentScope scope;
    std::pair<DbEnvironment, bool> result;

    LLVM_DEBUG(dbgs() << getIndent() << "├─ Op #" << ++opCount << ": "
                      << op.getName() << "\n");

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

  LLVM_DEBUG(dbgs() << getIndent() << "└─ Region processing complete. Changed: "
                    << (changed ? "YES" : "NO") << "\n");
  return {newEnv, changed};
}

std::pair<DbEnvironment, bool>
DbDataFlowAnalysis::processEdt(EdtOp edtOp, DbEnvironment &env) {
  static int edtCount = 0;
  LLVM_DEBUG(dbgs() << getIndent() << "┌─ Processing EDT #" << edtCount++
                    << " (deps: " << edtOp.getDependencies().size() << ")\n");

  auto edtDeps = edtOp.getDependencies();
  bool changed = false;
  DbEnvironment newEnv = env;

  /// Handle input dependencies
  if (!edtDeps.empty()) {
    LLVM_DEBUG({
      dbgs() << getIndent() << "│  Initial environment:\n";
      if (newEnv.empty()) {
        dbgs() << getIndent() << "│    (empty)\n";
      } else {
        for (auto entry : newEnv) {
          dbgs() << getIndent() << "│    " << getNodeId(entry.second) << " <- "
                 << entry.first << "\n";
        }
      }
    });

    /// Reserve dbIns and dbOuts for input and output datablocks
    SmallVector<DbDepNode *, 4> dbIns, dbOuts;
    dbIns.reserve(edtDeps.size());
    dbOuts.reserve(edtDeps.size());

    /// Iterate over the dependencies to fill dbIns and dbOuts
    for (size_t i = 0; i < edtDeps.size(); ++i) {
      auto dep = edtDeps[i];
      LLVM_DEBUG(dbgs() << getIndent() << "│  Processing dependency #" << i
                        << ": " << dep << "\n");

      mlir::Operation *defOp = dep.getDefiningOp();
      if (!defOp) {
        LLVM_DEBUG(
            dbgs() << getIndent()
                   << "│    ERROR: Dependency has no defining operation\n");
        llvm_unreachable("Dependency has no defining operation");
      }

      /// All dependencies should now be DbDepOp operations
      auto db = dyn_cast<DbDepOp>(defOp);
      if (!db) {
        LLVM_DEBUG(dbgs() << getIndent()
                          << "│    ERROR: Expected DbDepOp but got '"
                          << defOp->getName() << "' at "
                          << getOpLocationStr(defOp) << "\n");
        llvm_unreachable("Expected DbDepOp but got something else");
      }

      /// Try to retrieve the db node, and if not found, try to create it
      auto *dbDepNode = graph->getDepNode(db);
      if (!dbDepNode) {
        LLVM_DEBUG(
            dbgs()
            << getIndent() << "│    ERROR: DbDepNode not found in graph for "
            << db
            << " - this should not happen after proper graph construction\n");
        continue;
      }

      LLVM_DEBUG(dbgs() << getIndent() << "│    Found DbDepNode: "
                        << getNodeId(dbDepNode) << " (reader: "
                        << (dbDepNode->isReader() ? "YES" : "NO")
                        << ", writer: "
                        << (dbDepNode->isWriter() ? "YES" : "NO") << ")\n");

      if (dbDepNode->isWriter())
        dbOuts.push_back(dbDepNode);
      if (dbDepNode->isReader())
        dbIns.push_back(dbDepNode);
    }

    /// Process input datablocks
    LLVM_DEBUG(dbgs() << getIndent() << "│  Processing " << dbIns.size()
                      << " input datablocks\n");
    for (size_t i = 0; i < dbIns.size(); ++i) {
      auto *dbIn = dbIns[i];
      LLVM_DEBUG(dbgs() << getIndent() << "│    Input #" << i << ": "
                        << getNodeId(dbIn) << "\n");

      auto prodDefs = findDefinition(*dbIn, newEnv);
      if (prodDefs.empty()) {
        LLVM_DEBUG(dbgs() << getIndent()
                          << "│      No previous definitions found\n");
        continue;
      }

      LLVM_DEBUG(dbgs() << getIndent() << "│      Found " << prodDefs.size()
                        << " definition(s)\n");

      /// Analyze dependencies from producer definitions
      for (size_t j = 0; j < prodDefs.size(); ++j) {
        auto &prodDef = prodDefs[j];
        if (mayDepend(*prodDef, *dbIn)) {
          LLVM_DEBUG(dbgs() << getIndent()
                            << "│        Adding edge: " << getNodeId(prodDef)
                            << " -> " << getNodeId(dbIn) << "\n");
          bool res = graph->addEdge(dyn_cast<DbDepOp>(prodDef->getOp()),
                                    dyn_cast<DbDepOp>(dbIn->getOp()),
                                    DbDepType::ReadWrite);
          LLVM_DEBUG(dbgs() << getIndent() << "│          "
                            << (res ? "Added" : "Already exists") << "\n");
        } else {
          LLVM_DEBUG(dbgs() << getIndent()
                            << "│        No dependency: " << getNodeId(prodDef)
                            << " -/-> " << getNodeId(dbIn) << "\n");
        }
      }
    }

    /// Process output datablocks
    LLVM_DEBUG(dbgs() << getIndent() << "│  Processing " << dbOuts.size()
                      << " output datablocks\n");
    for (size_t i = 0; i < dbOuts.size(); ++i) {
      auto *dbOut = dbOuts[i];
      LLVM_DEBUG(dbgs() << getIndent() << "│    Output #" << i << ": "
                        << getNodeId(dbOut) << "\n");

      auto prodDefs = findDefinition(*dbOut, newEnv);
      if (prodDefs.empty()) {
        LLVM_DEBUG(
            dbgs() << getIndent()
                   << "│      No previous definitions, updating environment\n");
        newEnv[dyn_cast<DbDepOp>(dbOut->getOp())] = dbOut;
        changed |= true;
        continue;
      }

      for (auto &prodDef : prodDefs) {
        if (mayDepend(*prodDef, *dbOut))
          continue;
        LLVM_DEBUG(dbgs() << getIndent()
                          << "│      Updating environment: " << getNodeId(dbOut)
                          << " replaces " << getNodeId(prodDef) << "\n");
        newEnv[dyn_cast<DbDepOp>(prodDef->getOp())] = dbOut;
        changed |= true;
      }
    }
  }

  /// Process the EDT body region
  LLVM_DEBUG(dbgs() << getIndent() << "│  Processing EDT body region\n");
  {
    IndentScope bodyScope;
    DbEnvironment newEdtEnv;
    processRegion(edtOp.getBody(), newEdtEnv);
    /// TODO: Integrate parent/child environment relationships if needed
  }

  LLVM_DEBUG(dbgs() << getIndent() << "└─ EDT processing complete. Changed: "
                    << (changed ? "YES" : "NO") << "\n");
  return {newEnv, changed};
}

std::pair<DbEnvironment, bool>
DbDataFlowAnalysis::processFor(scf::ForOp forOp, DbEnvironment &env) {
  LLVM_DEBUG(dbgs() << getIndent() << "┌─ Processing ForOp loop\n");
  DbEnvironment loopEnv = env;
  bool overallChanged = false;
  size_t prevEdgeCount = graph->getNumEdges();
  unsigned iterationCount = 0;
  constexpr unsigned maxIterations = 5;

  /// Iterate until a fixed-point is reached or maximum iterations are hit
  while (true) {
    ++iterationCount;
    LLVM_DEBUG(dbgs() << getIndent() << "│  Iteration #" << iterationCount
                      << "\n");

    /// Process the loop body region
    auto bodyResult = processRegion(forOp->getRegion(0), loopEnv);
    DbEnvironment newLoopEnv = mergeEnvironments(loopEnv, bodyResult.first);
    overallChanged |= bodyResult.second;
    size_t currEdgeCount = graph->getNumEdges();

    LLVM_DEBUG(dbgs() << getIndent() << "│    Edges: " << prevEdgeCount
                      << " -> " << currEdgeCount << " (changed: "
                      << (bodyResult.second ? "YES" : "NO") << ")\n");

    /// Break if no changes were made and the edge count is stable
    if (!bodyResult.second && (currEdgeCount == prevEdgeCount))
      break;

    /// If maximum iterations reached without new edges, exit loop
    if (iterationCount >= maxIterations && (currEdgeCount == prevEdgeCount))
      break;

    /// Prepare for next iteration
    loopEnv = std::move(newLoopEnv);
    prevEdgeCount = currEdgeCount;
  }

  LLVM_DEBUG(dbgs() << getIndent() << "└─ ForOp complete (" << iterationCount
                    << " iterations)\n");
  return {loopEnv, overallChanged};
}

std::pair<DbEnvironment, bool>
DbDataFlowAnalysis::processWhile(scf::WhileOp whileOp, DbEnvironment &env) {
  LLVM_DEBUG(dbgs() << getIndent() << "┌─ Processing WhileOp loop\n");
  DbEnvironment loopEnv = env;
  bool overallChanged = false;
  size_t prevEdgeCount = graph->getNumEdges();
  unsigned iterationCount = 0;
  constexpr unsigned maxIterations = 5;

  /// Iterate until a fixed-point is reached or maximum iterations are hit
  while (true) {
    ++iterationCount;
    LLVM_DEBUG(dbgs() << getIndent() << "│  Iteration #" << iterationCount
                      << "\n");

    auto beforeResult = processRegion(whileOp.getBefore(), loopEnv);
    auto afterResult = processRegion(whileOp.getAfter(), beforeResult.first);

    DbEnvironment newLoopEnv = mergeEnvironments(loopEnv, afterResult.first);
    bool changedInIteration = beforeResult.second || afterResult.second;
    overallChanged |= changedInIteration;
    size_t currEdgeCount = graph->getNumEdges();

    LLVM_DEBUG(dbgs() << getIndent() << "│    Edges: " << prevEdgeCount
                      << " -> " << currEdgeCount << " (changed: "
                      << (changedInIteration ? "YES" : "NO") << ")\n");

    /// Break if no changes were made and the edge count is stable
    if (!changedInIteration && (currEdgeCount == prevEdgeCount))
      break;

    /// If maximum iterations reached without new edges, exit loop
    if (iterationCount >= maxIterations && (currEdgeCount == prevEdgeCount))
      break;

    /// Prepare for next iteration
    loopEnv = std::move(newLoopEnv);
    prevEdgeCount = currEdgeCount;
  }

  LLVM_DEBUG(dbgs() << getIndent() << "└─ WhileOp complete (" << iterationCount
                    << " iterations)\n");
  return {loopEnv, overallChanged};
}

std::pair<DbEnvironment, bool>
DbDataFlowAnalysis::processIf(scf::IfOp ifOp, DbEnvironment &env) {
  LLVM_DEBUG(dbgs() << getIndent() << "┌─ Processing IfOp\n");
  auto thenRes = processRegion(ifOp.getThenRegion(), env);
  auto elseRes = processRegion(ifOp.getElseRegion(), env);
  DbEnvironment merged = mergeEnvironments(thenRes.first, elseRes.first);
  bool changed = thenRes.second || elseRes.second;
  LLVM_DEBUG(dbgs() << getIndent() << "└─ IfOp complete. Changed: "
                    << (changed ? "YES" : "NO") << "\n");
  return {merged, changed};
}

std::pair<DbEnvironment, bool>
DbDataFlowAnalysis::processCall(func::CallOp callOp, DbEnvironment &env) {
  LLVM_DEBUG(dbgs() << getIndent() << "├─ Processing CallOp '"
                    << callOp.getCallee()
                    << "' (inter-procedural analysis not implemented)\n");
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
  LLVM_DEBUG(dbgs() << getIndent() << "│    Searching definitions for "
                    << getNodeId(&dbNode) << "\n");

  SmallVector<DbDepNode *, 4> defs;
  for (auto pair : env) {
    if (!analysis->dbAliasAnalysis->mayAlias(*pair.second, dbNode,
                                             getIndent() + "│      "))
      continue;
    LLVM_DEBUG(dbgs() << getIndent() << "│      Found potential definition: "
                      << getNodeId(pair.second) << "\n");
    /// Pessimistically assume alias implies potential dependency
    defs.push_back(pair.second);
  }

  LLVM_DEBUG(dbgs() << getIndent()
                    << "│    Total definitions found: " << defs.size() << "\n");
  return defs;
}

DbEnvironment DbDataFlowAnalysis::mergeEnvironments(const DbEnvironment &env1,
                                                    const DbEnvironment &env2) {
  LLVM_DEBUG(dbgs() << getIndent() << "│  Merging environments (" << env1.size()
                    << " + " << env2.size() << " entries)\n");

  DbEnvironment mergedEnv = env1;
  size_t addedCount = 0, updatedCount = 0;

  for (auto &pair : env2) {
    auto dbOp = pair.first;
    auto dbNode = pair.second;

    if (mergedEnv.count(dbOp)) {
      auto *node = graph->getDepNode(dbOp);
      /// If they belong to the same parent
      if (node && (dbNode->getEdtParent() == node->getEdtParent())) {
        LLVM_DEBUG(dbgs() << getIndent() << "│    Updated: "
                          << getNodeId(dbNode) << " (same parent)\n");
        mergedEnv[dbOp] = dbNode;
        ++updatedCount;
      }
    } else {
      /// Add new definition into the merged environment
      LLVM_DEBUG(dbgs() << getIndent() << "│    Added: " << getNodeId(dbNode)
                        << "\n");
      mergedEnv[dbOp] = dbNode;
      ++addedCount;
    }
  }

  LLVM_DEBUG(dbgs() << getIndent() << "│  Merge result: " << mergedEnv.size()
                    << " entries "
                    << "(+" << addedCount << " added, " << updatedCount
                    << " updated)\n");
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
  return analysis->dbAliasAnalysis->mayAlias(prod, cons, getIndent());
}
