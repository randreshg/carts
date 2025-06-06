///==========================================================================
/// File: DbDataFlowAnalysis.cpp
///
/// Implementation of data flow analysis for DbGraph.
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

#define DEBUG_TYPE "dataflow-analysis"
#define dbgs() llvm::dbgs()
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

namespace llvm {
template <> struct DenseMapInfo<ValueOrInt> {
  static ValueOrInt getEmptyKey() {
    return ValueOrInt(llvm::DenseMapInfo<int64_t>::getEmptyKey());
  }
  static ValueOrInt getTombstoneKey() {
    return ValueOrInt(llvm::DenseMapInfo<int64_t>::getTombstoneKey());
  }
  static unsigned getHashValue(const ValueOrInt &vai) {
    if (vai.isValue) {
      return llvm::DenseMapInfo<mlir::Value>::getHashValue(vai.v_val);
    } else {
      return llvm::hash_value(vai.i_val);
    }
  }
  static bool isEqual(const ValueOrInt &L, const ValueOrInt &R) {
    if (L.isValue != R.isValue)
      return false;
    if (L.isValue)
      return llvm::DenseMapInfo<mlir::Value>::isEqual(L.v_val, R.v_val);
    return L.i_val == R.i_val;
  }
};
} // namespace llvm

using namespace mlir;
using namespace mlir::arts;

// Initialize static member
int DbDataFlowAnalysis::analysisDepth = 0;

void DbDataFlowAnalysis::analyze() {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Starting data flow analysis for function: "
                    << graph->getFunction().getName() << "\n");

  // Initialize environment and process the function body
  Environment env;
  processRegion(graph->getFunction().getBody(), env);

  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "Finished data flow analysis for function: "
                    << graph->getFunction().getName() << "\n");
}

std::pair<Environment, bool>
DbDataFlowAnalysis::processRegion(Region &region, Environment &env) {
  Environment newEnv = env;
  bool changed = false;
  for (Operation &op : region.getOps()) {
    IndentScope scope;
    std::pair<Environment, bool> result;
    if (auto edtOp = dyn_cast<EdtOp>(&op))
      result = processEdt(edtOp, newEnv);
    else if (auto ifOp = dyn_cast<scf::IfOp>(&op))
      result = processIf(ifOp, newEnv);
    else if (auto forOp = dyn_cast<scf::ForOp>(&op))
      result = processFor(forOp, newEnv);
    else if (auto callOp = dyn_cast<func::CallOp>(&op))
      result = processCall(callOp, newEnv);
    else
      continue;

    // Merge the new environment with the current one
    newEnv = mergeEnvironments(newEnv, result.first);
    changed = changed || result.second;
  }
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << " - Finished processing region. Environment changed: "
                    << (changed ? "true" : "false") << "\n");
  return {newEnv, changed};
}

std::pair<Environment, bool> DbDataFlowAnalysis::processEdt(EdtOp edtOp,
                                                            Environment &env) {
  static int edtCount = 0;
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing EDT #" << edtCount++ << "\n");

  // Process the inputs and outputs of the EDT
  auto edtDeps = edtOp.getDependencies();
  bool changed = false;
  Environment newEnv = env;
  DbGraph *graph =
      analysis->getOrCreateGraph(edtOp->getParentOfType<func::FuncOp>());

  // Handle input dependencies
  if (!edtDeps.empty()) {
    LLVM_DEBUG({
      dbgs() << std::string(analysisDepth * 2, ' ')
             << " Initial environment: {";
      for (auto entry : newEnv) {
        if (auto *dbInfo = graph->getNode(entry.first)) {
          dbgs() << "  #" << dbInfo->getHierId() << " -> #"
                 << entry.second->getHierId() << ",";
        }
      }
      dbgs() << "}\n";
    });

    // Reserve dbIns and dbOuts for input and output datablocks
    SmallVector<DbAccessNode *, 4> dbIns, dbOuts;
    dbIns.reserve(edtDeps.size());
    dbOuts.reserve(edtDeps.size());

    // Iterate over the dependencies to fill dbIns and dbOuts
    for (auto dep : edtDeps) {
      auto db = dyn_cast<DbControlOp>(dep.getDefiningOp());
      assert(db && "Dependency must be a db");
      // Retrieve the db node and categorize based on mode
      auto *dbInfo = graph->getNode(db);
      if (auto *dbAccessNode = dyn_cast<DbAccessNode>(dbInfo)) {
        if (dbAccessNode->isWriter())
          dbOuts.push_back(dbAccessNode);
        if (dbAccessNode->isReader())
          dbIns.push_back(dbAccessNode);
      }
    }

    // Process input datablocks
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
        // if (auto *entryNode = graph->getEntryNode()) {
        //   graph->addEdge(*entryNode, *dbIn);
        // }
        continue;
      }
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ') << "Found "
                        << prodDefs.size() << " definitions for DB #"
                        << dbIn->getHierId() << "\n");
      // Analyze dependencies from producer definitions
      for (auto &prodDef : prodDefs) {
        bool isDirect = false;
        if (!analysis->mayDepend(*prodDef, *dbIn, isDirect))
          continue;
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "Adding edge from DB #" << prodDef->getHierId()
                          << " to DB #" << dbIn->getHierId() << "\n");
        bool res = graph->addEdge(dyn_cast<DbAccessOp>(prodDef->getOp()),
                                  dyn_cast<DbAccessOp>(dbIn->getOp()),
                                  DbDepType::ReadWrite);
        if (res) {
          LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                            << "Edge added successfully\n");
        } else {
          LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                            << "Edge already exists, skipping\n");
        }
      }
    }

    // Process output datablocks
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
        newEnv[dyn_cast<DbAccessOp>(dbOut->getOp())] = dbOut;
        changed |= true;
        continue;
      }
      for (auto &prodDef : prodDefs) {
        bool isDirect = false;
        if (!analysis->mayDepend(*prodDef, *dbOut, isDirect))
          continue;
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "Updating environment: DB #" << dbOut->getHierId()
                          << " now defined from DB #" << prodDef->getHierId()
                          << "\n");
        newEnv[dyn_cast<DbAccessOp>(prodDef->getOp())] = dbOut;
        changed |= true;
      }
    }
  }

  // Process the EDT body region
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  Processing EDT body region\n");
  {
    IndentScope bodyScope;
    Environment newEdtEnv;
    processRegion(edtOp.getBody(), newEdtEnv);
    // TODO: Integrate parent/child environment relationships if needed
  }

  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "Finished processing EDT. Environment changed: "
                    << (changed ? "true" : "false") << "\n");
  return {newEnv, changed};
}

std::pair<Environment, bool> DbDataFlowAnalysis::processFor(scf::ForOp forOp,
                                                            Environment &env) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing ForOp loop\n");
  Environment loopEnv = env;
  bool overallChanged = false;
  size_t prevEdgeCount = graph->getNumEdges();
  unsigned iterationCount = 0;
  constexpr unsigned maxIterations = 5;

  // Iterate until a fixed-point is reached or maximum iterations are hit
  while (true) {
    ++iterationCount;
    // Process the loop body region
    auto bodyResult = processRegion(forOp->getRegion(0), loopEnv);
    Environment newLoopEnv = mergeEnvironments(loopEnv, bodyResult.first);
    overallChanged |= bodyResult.second;
    size_t currEdgeCount = graph->getNumEdges();

    // Log the current iteration and edge count
    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ') << "  - Iteration "
                      << iterationCount << " completed: edges=" << currEdgeCount
                      << "\n");

    // Break if no changes were made and the edge count is stable
    if (!bodyResult.second && (currEdgeCount == prevEdgeCount))
      break;

    // If maximum iterations reached without new edges, exit loop
    if (iterationCount >= maxIterations && (currEdgeCount == prevEdgeCount))
      break;

    // Prepare for next iteration
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

std::pair<Environment, bool> DbDataFlowAnalysis::processIf(scf::IfOp ifOp,
                                                           Environment &env) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing IfOp with then and else regions\n");
  auto thenRes = processRegion(ifOp.getThenRegion(), env);
  auto elseRes = processRegion(ifOp.getElseRegion(), env);
  Environment merged = mergeEnvironments(thenRes.first, elseRes.first);
  bool changed = thenRes.second || elseRes.second;
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  - IfOp regions merged. Environment changed: "
                    << (changed ? "true" : "false") << "\n");
  return {merged, changed};
}

std::pair<Environment, bool>
DbDataFlowAnalysis::processCall(func::CallOp callOp, Environment &env) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing CallOp (ignoring for now)\n");
  // TODO: Expand call handling logic for inter-procedural analysis
  return {env, false};
}

SmallVector<DbAccessNode *, 4>
DbDataFlowAnalysis::findDefinition(DbAccessNode &dbNode, Environment &env) {
  IndentScope scope;
  DbGraph *graph = analysis->getOrCreateGraph(
      dbNode.getOp()->getParentOfType<func::FuncOp>());
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  Searching for definitions for DB #"
                    << dbNode.getHierId() << "\n");
  SmallVector<DbAccessNode *, 4> defs;
  for (auto pair : env) {
    if (!analysis->dbAliasAnalysis->mayAlias(*pair.second, dbNode))
      continue;
    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                      << "  - Potential definition found: DB #"
                      << pair.second->getHierId() << "\n");
    // Pessimistically assume alias implies potential dependency
    defs.push_back(pair.second);
  }
  return defs;
}

Environment DbDataFlowAnalysis::mergeEnvironments(const Environment &env1,
                                                  const Environment &env2) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  Merging environments\n");
  Environment mergedEnv = env1;
  DbGraph *graph = nullptr;
  if (!env1.empty())
    graph = analysis->getOrCreateGraph(
        env1.begin()->first->getParentOfType<func::FuncOp>());
  else if (!env2.empty())
    graph = analysis->getOrCreateGraph(
        env2.begin()->first->getParentOfType<func::FuncOp>());

  for (auto &pair : env2) {
    auto dbOp = pair.first;
    auto dbNode = pair.second;
    if (mergedEnv.count(dbOp)) {
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                        << "  - DB already exists in merged environment: "
                        << dbNode->getHierId() << "\n");
      auto *node =
          dyn_cast<DbAccessNode>(graph ? graph->getNode(dbOp) : nullptr);
      // If they belong to the same parent
      if (node && (dbNode->getParent() == node->getParent())) {
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "    - Same parent, "
                          << "updating definition\n");
        mergedEnv[dbOp] = dbNode;
      }
    } else {
      // Add new definition into the merged environment
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                        << "  - Adding DB #" << dbNode->getHierId()
                        << " to merged environment\n");
      mergedEnv[dbOp] = dbNode;
    }
  }
  // Debug final merged environment
  LLVM_DEBUG({
    dbgs() << std::string(analysisDepth * 2, ' ')
           << "  - Final merged environment: {";
    for (auto &pair : mergedEnv) {
      if (graph) {
        if (auto *dbInfo = graph->getNode(pair.first)) {
          dbgs() << " #" << dbInfo->getHierId() << " -> #"
                 << pair.second->getHierId() << ",";
        }
      }
    }
    dbgs() << "}\n";
  });
  return mergedEnv;
}


/// Dependency analysis.
bool DbDataFlowAnalysis::mayDepend(DatablockNode &prod, DatablockNode &cons,
                                  bool &isDirect) {
  /// Verify if they belong to the same EDT parent.
  if (prod.edtParent != cons.edtParent)
    return false;

  /// Only consider writer producer and reader consumer.
  if (!prod.isWriter() || !cons.isReader())
    return false;

  /// Check if nodes are different
  auto compResult = compare(prod, cons);
  if (compResult == DatablockNodeComp::Different)
    return false;

  /// There is a direct dependency if
  isDirect = true ? (compResult == DatablockNodeComp::Equal) : false;

  /// TODO: We can make a more complex analysis here, such as checking an
  /// affine iteration space, and verifying if the producer and consumer are
  /// in the same iteration space.
  return true;
}

DatablockNodeComp DatablockAnalysis::compare(DatablockNode &A,
                                             DatablockNode &B) {
  /// If it is already known that the nodes are equivalent, return true.
  if (A.duplicates.contains(B.id) || B.duplicates.contains(A.id))
    return DatablockNodeComp::Equal;

  /// Compute it, otherwise.
  if (!ptrMayAlias(A, B))
    return DatablockNodeComp::Different;

  /// Compare indices
  if (A.indices.size() != B.indices.size())
    return DatablockNodeComp::BaseAlias;

  if (!std::equal(A.indices.begin(), A.indices.end(), B.indices.begin()))
    return DatablockNodeComp::BaseAlias;

  /// Cache the result.
  A.duplicates.insert(B.id);
  B.duplicates.insert(A.id);
  return DatablockNodeComp::Equal;
}
