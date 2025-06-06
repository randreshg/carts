///==========================================================================
/// File: DbGraph.cpp
///
/// Simplified implementation of db dependency graph for analyzing
/// memory access patterns and dependencies.
///==========================================================================

#include "arts/Analysis/Db/Graph/DbGraph.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Db/DbInfo.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>

#define DEBUG_TYPE "db-graph"
#define dbgs() llvm::dbgs()
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;

/// Helper to generate letter-based IDs (A, B, C, ..., Z, AA, AB, ...)
static std::string generateAllocId(unsigned id) {
  std::string s;
  if (id == 0)
    return "A";
  unsigned n = id;
  while (n > 0) {
    unsigned rem = (n - 1) % 26;
    s = char('A' + rem) + s;
    n = (n - 1) / 26;
  }
  return s;
}

///===----------------------------------------------------------------------===///
/// DbGraph Implementation
///===----------------------------------------------------------------------===///

DbGraph::DbGraph(func::FuncOp func, DbAnalysis *analysis)
    : func(func), analysis(analysis), isBuilt(false), needsRebuild(true),
      nextAllocId(1) {
  assert(func && "Function cannot be null");
  dataFlowAnalysis = std::make_unique<DbDataFlowAnalysis>(analysis, this);
  LLVM_DEBUG(DBGS() << "Creating db graph for function: "
                    << func.getName().str() << "\n");
}

bool DbGraph::isAllocReachable(DbCreateOp from, DbCreateOp to) {
  if (!from || !to)
    return false;

  DbAllocNode *fromNode = getAllocNode(from);
  DbAllocNode *toNode = getAllocNode(to);
  if (!fromNode || !toNode)
    return false;

  for (auto *edge : getOutAllocEdges(fromNode)) {
    if (edge->getTo() == toNode)
      return true;
  }
  return false;
}

bool DbGraph::isAccessReachable(DbAccessOp from, DbAccessOp to) {
  DbAccessNode *fromNode = getAccessNode(from);
  DbAccessNode *toNode = getAccessNode(to);
  if (!fromNode || !toNode)
    return false;

  for (auto *edge : getOutDepEdges(fromNode)) {
    if (edge->getTo() == toNode)
      return true;
  }
  return false;
}

bool DbGraph::hasDataDep(DbAccessOp from, DbAccessOp to) {
  DbAccessNode *fromNode = getAccessNode(from);
  DbAccessNode *toNode = getAccessNode(to);
  if (!fromNode || !toNode)
    return false;

  for (auto *edge : getOutDepEdges(fromNode)) {
    if (edge->getTo() == toNode)
      return true;
  }
  return false;
}

DbCreateOp DbGraph::getParentAlloc(DbAccessOp access) {
  DbAccessNode *accessNode = getAccessNode(access);
  if (!accessNode)
    return nullptr;
  return dyn_cast<DbCreateOp>(accessNode->getParentAlloc()->getOp());
}

bool DbGraph::mayAlias(DbCreateOp alloc1, DbCreateOp alloc2) {
  DbAllocNode *node1 = getAllocNode(alloc1);
  DbAllocNode *node2 = getAllocNode(alloc2);
  if (!node1 || !node2)
    return false;
  return analysis->dbAliasAnalysis->mayAlias(*node1, *node2);
}

bool DbGraph::mayAlias(DbAccessOp access1, DbAccessOp access2) {
  DbAccessNode *node1 = getAccessNode(access1);
  DbAccessNode *node2 = getAccessNode(access2);
  if (!node1 || !node2)
    return false;
  return analysis->dbAliasAnalysis->mayAlias(*node1, *node2);
}

///===----------------------------------------------------------------------===///
/// Node Management
///===----------------------------------------------------------------------===///
DbAllocNode *DbGraph::getOrCreateAllocNode(DbCreateOp allocOp) {
  auto it = allocNodes.find(allocOp);
  if (it != allocNodes.end())
    return it->second.get();

  auto newNode = std::make_unique<DbAllocNode>(allocOp, analysis);
  DbAllocNode *nodePtr = newNode.get();

  /// Set hierarchical ID for allocation node (A, B, C, etc.)
  nodePtr->setHierId(generateAllocId(nextAllocId++));

  allocNodes[allocOp] = std::move(newNode);
  return nodePtr;
}

DbAllocNode *DbGraph::getAllocNode(DbCreateOp allocOp) {
  auto it = allocNodes.find(allocOp);
  return it != allocNodes.end() ? it->second.get() : nullptr;
}

DbAccessNode *DbGraph::getOrCreateAccessNode(DbCreateOp allocOp,
                                             DbAccessOp accessOp) {
  DbAllocNode *allocNode = getOrCreateAllocNode(allocOp);
  auto *accessNode = allocNode->getOrCreateAccessNode(accessOp);
  accessNodeMap[accessOp] = accessNode;
  return accessNode;
}

DbAccessNode *DbGraph::getAccessNode(DbAccessOp accessOp) {
  auto it = accessNodeMap.find(accessOp);
  if (it != accessNodeMap.end())
    return it->second;

  for (auto &pair : allocNodes) {
    if (auto *accessNode = pair.second->findAccessNode(accessOp)) {
      accessNodeMap[accessOp] = accessNode;
      return accessNode;
    }
  }
  return nullptr;
}

///===----------------------------------------------------------------------===///
/// Edge Management
///===----------------------------------------------------------------------===///

bool DbGraph::insertDepEdge(DbAccessOp from, DbAccessOp to, DbDepType type) {
  DbAccessNode *fromNode = getAccessNode(from);
  DbAccessNode *toNode = getAccessNode(to);
  if (!fromNode || !toNode)
    return false;

  auto key = std::make_pair(fromNode, toNode);
  if (depEdges.find(key) != depEdges.end())
    return false;

  auto edge = std::make_unique<DbDepEdge>(fromNode, toNode, type);
  depEdges[key] = std::move(edge);
  return true;
}

bool DbGraph::insertAllocEdge(DbCreateOp from, DbCreateOp to) {
  DbAllocNode *fromNode = getAllocNode(from);
  DbAllocNode *toNode = getAllocNode(to);
  if (!fromNode || !toNode)
    return false;

  auto key = std::make_pair(fromNode, toNode);
  if (allocEdges.find(key) != allocEdges.end())
    return false;

  auto edge = std::make_unique<DbAllocEdge>(fromNode, toNode);
  allocEdges[key] = std::move(edge);
  return true;
}

DbDepEdge *DbGraph::getDependenceEdge(memref::SubViewOp from,
                                      memref::SubViewOp to) {
  DbAccessNode *fromNode = getAccessNode(from);
  DbAccessNode *toNode = getAccessNode(to);
  if (!fromNode || !toNode)
    return nullptr;

  auto key = std::make_pair(fromNode, toNode);
  auto it = depEdges.find(key);
  return it != depEdges.end() ? it->second.get() : nullptr;
}

DbAllocEdge *DbGraph::getAllocEdge(DbCreateOp from, DbCreateOp to) {
  DbAllocNode *fromNode = getAllocNode(from);
  DbAllocNode *toNode = getAllocNode(to);
  if (!fromNode || !toNode)
    return nullptr;

  auto key = std::make_pair(fromNode, toNode);
  auto it = allocEdges.find(key);
  return it != allocEdges.end() ? it->second.get() : nullptr;
}

///===----------------------------------------------------------------------===///
/// Edge Iteration
///===----------------------------------------------------------------------===///

DenseSet<DbDepEdge *> DbGraph::getInDepEdges(DbAccessNode *node) {
  DenseSet<DbDepEdge *> result;
  for (auto &pair : depEdges) {
    if (pair.second->getTo() == node)
      result.insert(pair.second.get());
  }
  return result;
}

DenseSet<DbDepEdge *> DbGraph::getOutDepEdges(DbAccessNode *node) {
  DenseSet<DbDepEdge *> result;
  for (auto &pair : depEdges) {
    if (pair.second->getFrom() == node)
      result.insert(pair.second.get());
  }
  return result;
}

DenseSet<DbAllocEdge *> DbGraph::getInAllocEdges(DbAllocNode *node) {
  DenseSet<DbAllocEdge *> result;
  for (auto &pair : allocEdges) {
    if (pair.second->getTo() == node)
      result.insert(pair.second.get());
  }
  return result;
}

DenseSet<DbAllocEdge *> DbGraph::getOutAllocEdges(DbAllocNode *node) {
  DenseSet<DbAllocEdge *> result;
  for (auto &pair : allocEdges) {
    if (pair.second->getFrom() == node)
      result.insert(pair.second.get());
  }
  return result;
}

///===----------------------------------------------------------------------===///
/// Node Iteration
///===----------------------------------------------------------------------===///

void DbGraph::forEachAllocNode(const std::function<void(DbAllocNode *)> &fn) {
  for (auto &pair : allocNodes)
    fn(pair.second.get());
}

void DbGraph::forEachAccessNode(const std::function<void(DbAccessNode *)> &fn) {
  for (auto &pair : accessNodeMap)
    fn(pair.second);
}

///===----------------------------------------------------------------------===///
/// Graph Construction
///===----------------------------------------------------------------------===///

void DbGraph::build() {
  if (isBuilt && !needsRebuild)
    return;

  LLVM_DEBUG(DBGS() << "Building simplified db graph\n");
  invalidate();

  /// Phase 1: Find all db operations and create nodes
  collectNodes();
  /// Phase 2: Build dependence relationships
  buildDependencies();
  /// Phase 3: Analyze patterns and characterize
  analyzePatternsAndCharacterize();

  isBuilt = true;
  needsRebuild = false;
}

void DbGraph::invalidate() {
  allocNodes.clear();
  accessNodeMap.clear();
  depEdges.clear();
  allocEdges.clear();
  nextAllocId = 1;
  isBuilt = false;
  needsRebuild = true;
}

void DbGraph::print(llvm::raw_ostream &os) {
  os << "DbGraph for function: " << func.getName() << "\n";
  os << "  Allocs: " << allocNodes.size() << "\n";
  os << "  Accesses: " << accessNodeMap.size() << "\n";
  os << "  Dependence edges: " << depEdges.size() << "\n";
  os << "  Alloc edges: " << allocEdges.size() << "\n";

  os << "\nAlloc nodes:\n";
  forEachAllocNode([&](DbAllocNode *node) {
    node->print(os);
    os << "\n";
  });

  os << "\nAccess nodes:\n";
  forEachAccessNode([&](DbAccessNode *node) {
    node->print(os);
    os << "\n";
  });
}

///===----------------------------------------------------------------------===///
/// Private Construction Phases
///===----------------------------------------------------------------------===///

void DbGraph::collectNodes() {
  LLVM_DEBUG(DBGS() << "Phase 1: Collecting nodes\n");

  /// Collect allocations and their accesses in one pass
  func.walk([&](DbCreateOp allocOp) {
    auto *allocNode = getOrCreateAllocNode(allocOp);

    /// Find all subviews that use this allocation
    for (auto *user : allocOp.getPtr().getUsers()) {
      if (auto accessOp = dyn_cast<DbAccessOp>(user)) {
        auto *accessNode = allocNode->getOrCreateAccessNode(accessOp);
        accessNodeMap[accessOp] = accessNode;
      }
    }
  });

  LLVM_DEBUG(dbgs() << "Collected " << allocNodes.size() << " allocations and "
                    << accessNodeMap.size() << " accesses\n");
}

void DbGraph::buildDependencies() {
  LLVM_DEBUG(DBGS() << "Phase 2: Building dependencies\n");
  dataFlowAnalysis->analyze();

  LLVM_DEBUG(DBGS() << "Phase 2 complete: Data flow analysis finished\n");
}

void DbGraph::analyzePatternsAndCharacterize() {
  LLVM_DEBUG(DBGS() << "Phase 3: Analyzing patterns and characterizing\n");

  // Analyze access patterns for each access node
  forEachAccessNode([](DbAccessNode *node) { node->analyze(); });

  // Characterize each allocation based on its access patterns
  forEachAllocNode([](DbAllocNode *node) { node->analyze(); });
}

///===----------------------------------------------------------------------===///
/// Helper Methods
///===----------------------------------------------------------------------===///

bool DbGraph::detectCycleUtil(DbAccessNode *node,
                              DenseSet<DbAccessNode *> &visited,
                              DenseSet<DbAccessNode *> &recursionStack) {
  visited.insert(node);
  recursionStack.insert(node);

  for (auto *edge : getOutDepEdges(node)) {
    auto *nextNode = edge->getTo();

    if (recursionStack.find(nextNode) != recursionStack.end()) {
      return true; // Found cycle
    }

    if (visited.find(nextNode) == visited.end()) {
      if (detectCycleUtil(nextNode, visited, recursionStack)) {
        return true;
      }
    }
  }

  recursionStack.erase(node);
  return false;
}

DbInfo *DbGraph::getNode(Operation *op) {
  if (!op)
    return nullptr;

  if (auto allocOp = dyn_cast<DbCreateOp>(op))
    return getAllocNode(allocOp);
  if (auto accessOp = dyn_cast<DbAccessOp>(op))
    return getAccessNode(accessOp);

  return nullptr;
}

DbInfo *DbGraph::getNode(DbControlOp dbOp) {
  if (!dbOp)
    return nullptr;
  return getNode(dbOp.getOperation());
}

DbInfo *DbGraph::getEntryNode() {
  /// For simplicity, return the first allocation node as the entry node
  if (!allocNodes.empty())
    return allocNodes.begin()->second.get();

  return nullptr;
}

bool DbGraph::addEdge(DbAccessOp from, DbAccessOp to, DbDepType type) {
  return insertDepEdge(from, to, type);
}

bool DbGraph::addEdge(DbCreateOp from, DbCreateOp to) {
  return insertAllocEdge(from, to);
}

size_t DbGraph::getNumEdges() const {
  return depEdges.size() + allocEdges.size();
}
