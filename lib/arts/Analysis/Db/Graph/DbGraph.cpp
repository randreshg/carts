///==========================================================================
/// File: DbGraph.cpp
///
/// Simplified implementation of db dependency graph for analyzing
/// memory dep patterns and dependencies.
///==========================================================================

#include "arts/Analysis/Db/Graph/DbGraph.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Db/DbInfo.h"
#include "arts/ArtsDialect.h"
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

/// Helper to generate sanitized DOT identifiers.
static std::string sanitizeForDot(StringRef s) {
  std::string id = s.str();
  std::replace(id.begin(), id.end(), '.', '_');
  std::replace(id.begin(), id.end(), '-', '_');
  return id;
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

bool DbGraph::isAllocReachable(DbAllocOp from, DbAllocOp to) {
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

bool DbGraph::isDepReachable(DbDepOp from, DbDepOp to) {
  DbDepNode *fromNode = getDepNode(from);
  DbDepNode *toNode = getDepNode(to);
  if (!fromNode || !toNode)
    return false;

  for (auto *edge : getOutDepEdges(fromNode)) {
    if (edge->getTo() == toNode)
      return true;
  }
  return false;
}

bool DbGraph::hasDataDep(DbDepOp from, DbDepOp to) {
  DbDepNode *fromNode = getDepNode(from);
  DbDepNode *toNode = getDepNode(to);
  if (!fromNode || !toNode)
    return false;

  for (auto *edge : getOutDepEdges(fromNode)) {
    if (edge->getTo() == toNode)
      return true;
  }
  return false;
}

DbAllocOp DbGraph::getParentAlloc(DbDepOp dep) {
  DbDepNode *depNode = getDepNode(dep);
  if (!depNode)
    return nullptr;
  return dyn_cast<DbAllocOp>(depNode->getParentAlloc()->getOp());
}

bool DbGraph::mayAlias(DbAllocOp alloc1, DbAllocOp alloc2) {
  DbAllocNode *node1 = getAllocNode(alloc1);
  DbAllocNode *node2 = getAllocNode(alloc2);
  if (!node1 || !node2)
    return false;
  return analysis->dbAliasAnalysis->mayAlias(*node1, *node2);
}

bool DbGraph::mayAlias(DbDepOp dep1, DbDepOp dep2) {
  DbDepNode *node1 = getDepNode(dep1);
  DbDepNode *node2 = getDepNode(dep2);
  if (!node1 || !node2)
    return false;
  return analysis->dbAliasAnalysis->mayAlias(*node1, *node2);
}

///===----------------------------------------------------------------------===///
/// Node Management
///===----------------------------------------------------------------------===///
DbAllocNode *DbGraph::getOrCreateAllocNode(DbAllocOp allocOp) {
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

DbAllocNode *DbGraph::getAllocNode(DbAllocOp allocOp) {
  auto it = allocNodes.find(allocOp);
  return it != allocNodes.end() ? it->second.get() : nullptr;
}

DbDepNode *DbGraph::getOrCreateDepNode(DbAllocOp allocOp,
                                             DbDepOp depOp) {
  DbAllocNode *allocNode = getOrCreateAllocNode(allocOp);
  auto *depNode = allocNode->getOrCreateDepNode(depOp);
  depNodeMap[depOp] = depNode;
  return depNode;
}

DbDepNode *DbGraph::getDepNode(DbDepOp depOp) {
  auto it = depNodeMap.find(depOp);
  if (it != depNodeMap.end())
    return it->second;

  for (auto &pair : allocNodes) {
    if (auto *depNode = pair.second->findDepNode(depOp)) {
      depNodeMap[depOp] = depNode;
      return depNode;
    }
  }
  return nullptr;
}

///===----------------------------------------------------------------------===///
/// Edge Management
///===----------------------------------------------------------------------===///

bool DbGraph::insertDepEdge(DbDepOp from, DbDepOp to, DbDepType type) {
  DbDepNode *fromNode = getDepNode(from);
  DbDepNode *toNode = getDepNode(to);
  if (!fromNode || !toNode)
    return false;

  auto key = std::make_pair(fromNode, toNode);
  if (depEdges.count(key))
    return false;

  auto edge = std::make_unique<DbDepEdge>(fromNode, toNode, type);
  fromNode->addOutDepEdge(edge.get());
  toNode->addInDepEdge(edge.get());
  depEdges[key] = std::move(edge);
  return true;
}

bool DbGraph::insertAllocEdge(DbAllocOp from, DbAllocOp to) {
  DbAllocNode *fromNode = getOrCreateAllocNode(from);
  DbAllocNode *toNode = getOrCreateAllocNode(to);
  if (!fromNode || !toNode)
    return false;

  auto key = std::make_pair(fromNode, toNode);
  if (allocEdges.count(key))
    return false;

  auto edge = std::make_unique<DbAllocEdge>(fromNode, toNode);
  fromNode->addOutAllocEdge(edge.get());
  toNode->addInAllocEdge(edge.get());
  allocEdges[key] = std::move(edge);
  return true;
}

DbDepEdge *DbGraph::getDependenceEdge(DbDepOp from, DbDepOp to) {
  DbDepNode *fromNode = getDepNode(from);
  DbDepNode *toNode = getDepNode(to);
  if (!fromNode || !toNode)
    return nullptr;

  auto key = std::make_pair(fromNode, toNode);
  auto it = depEdges.find(key);
  return it != depEdges.end() ? it->second.get() : nullptr;
}

DbAllocEdge *DbGraph::getAllocEdge(DbAllocOp from, DbAllocOp to) {
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

const DenseSet<DbDepEdge *> &DbGraph::getInDepEdges(DbDepNode *node) {
  return node->getInDepEdges();
}

const DenseSet<DbDepEdge *> &DbGraph::getOutDepEdges(DbDepNode *node) {
  return node->getOutDepEdges();
}

const DenseSet<DbAllocEdge *> &DbGraph::getInAllocEdges(DbAllocNode *node) {
  return node->getInAllocEdges();
}

const DenseSet<DbAllocEdge *> &DbGraph::getOutAllocEdges(DbAllocNode *node) {
  return node->getOutAllocEdges();
}

///===----------------------------------------------------------------------===///
/// Node Iteration
///===----------------------------------------------------------------------===///

void DbGraph::forEachAllocNode(const std::function<void(DbAllocNode *)> &fn) {
  for (auto &pair : allocNodes)
    fn(pair.second.get());
}

void DbGraph::forEachDepNode(const std::function<void(DbDepNode *)> &fn) {
  for (auto &pair : depNodeMap)
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

  collectNodes();
  buildDependencies();

  isBuilt = true;
  needsRebuild = false;
}

void DbGraph::invalidate() {
  allocNodes.clear();
  depNodeMap.clear();
  depEdges.clear();
  allocEdges.clear();
  nextAllocId = 1;
  isBuilt = false;
  needsRebuild = true;
}

void DbGraph::print(llvm::raw_ostream &os) {
  os << "===============================================\n";
  os << "DbGraph for function: " << func.getName() << "\n";
  os << "===============================================\n";
  os << "Summary:\n";
  os << "  Allocations: " << allocNodes.size() << "\n";
  os << "  Dep nodes: " << depNodeMap.size() << "\n";
  os << "  Dependence edges: " << depEdges.size() << "\n";
  os << "  Allocation edges: " << allocEdges.size() << "\n";
  os << "\n";

  // Print allocation hierarchy with their dep nodes and dependencies
  os << "Allocation Hierarchy:\n";
  os << "=====================\n";
  forEachAllocNode([&](DbAllocNode *allocNode) {
    os << "Allocation [" << allocNode->getHierId() << "]: ";
    allocNode->print(os);
    os << "\n";

    // Count and list dep nodes for this allocation
    std::vector<DbDepNode *> depNodes;
    allocNode->forEachDepNode(
        [&](DbDepNode *depNode) { depNodes.push_back(depNode); });

    if (depNodes.empty()) {
      os << "   └── (no dep nodes)\n";
    } else {
      os << "   └── Dep nodes (" << depNodes.size() << "):\n";

      for (size_t i = 0; i < depNodes.size(); ++i) {
        DbDepNode *depNode = depNodes[i];
        bool isLast = (i == depNodes.size() - 1);
        os << "       " << (isLast ? "└──" : "├──") << " ["
           << depNode->getHierId() << "]: ";
        depNode->print(os);
        os << "\n";

        // Show outgoing dependencies for this dep node
        auto outEdges = getOutDepEdges(depNode);
        if (!outEdges.empty()) {
          os << "       " << (isLast ? "   " : "│  ") << "    Dependencies:\n";
          for (auto *edge : outEdges) {
            os << "       " << (isLast ? "   " : "│  ") << "      → ["
               << edge->getTo()->getHierId() << "] (";
            switch (edge->getType()) {
            case DbDepType::Read:
              os << "Read";
              break;
            case DbDepType::Write:
              os << "Write";
              break;
            case DbDepType::ReadWrite:
              os << "ReadWrite";
              break;
            }
            os << ")\n";
          }
        }
      }
    }

    // Show outgoing allocation edges
    auto outAllocEdges = getOutAllocEdges(allocNode);
    if (!outAllocEdges.empty()) {
      os << "   └── Allocation dependencies:\n";
      for (auto *edge : outAllocEdges) {
        os << "       → Allocation [" << edge->getTo()->getHierId() << "]\n";
      }
    }
    os << "\n";
  });

  // Print dependency summary
  if (!depEdges.empty()) {
    os << "Dependency Summary:\n";
    os << "==================\n";

    // Count dependencies by type
    size_t readCount = 0, writeCount = 0, readWriteCount = 0;
    for (auto &pair : depEdges) {
      switch (pair.second->getType()) {
      case DbDepType::Read:
        readCount++;
        break;
      case DbDepType::Write:
        writeCount++;
        break;
      case DbDepType::ReadWrite:
        readWriteCount++;
        break;
      }
    }

    os << "  Read dependencies: " << readCount << "\n";
    os << "  Write dependencies: " << writeCount << "\n";
    os << "  ReadWrite dependencies: " << readWriteCount << "\n";
    os << "\n";

    // List all dependencies in order
    os << "All Dependencies:\n";
    for (auto &pair : depEdges) {
      auto *edge = pair.second.get();
      os << "  [" << edge->getFrom()->getHierId() << "] → ["
         << edge->getTo()->getHierId() << "] (";
      switch (edge->getType()) {
      case DbDepType::Read:
        os << "Read";
        break;
      case DbDepType::Write:
        os << "Write";
        break;
      case DbDepType::ReadWrite:
        os << "ReadWrite";
        break;
      }
      os << ")\n";
    }
  }

  os << "===============================================\n";
}

void DbGraph::exportToDot(llvm::raw_ostream &os) {
  os << "digraph DbGraph {\n";
  os << "  compound=true;\n";
  os << "  node [shape=box];\n";
  os << "  graph [label=\"" << func.getName()
     << "\", labelloc=t, fontsize=20];\n\n";

  /// Define subgraphs for each allocation
  forEachAllocNode([&](DbAllocNode *allocNode) {
    os << "  subgraph cluster_" << sanitizeForDot(allocNode->getHierId())
       << " {\n";
    os << "    label = \"Alloc: " << allocNode->getHierId() << "\";\n";
    os << "    style=filled;\n";
    os << "    color=lightgrey;\n\n";

    bool hasDepNodes = false;
    allocNode->forEachDepNode([&](DbDepNode *depNode) {
      hasDepNodes = true;
      os << "    " << sanitizeForDot(depNode->getHierId()) << " [label=\""
         << depNode->getHierId() << "\"];\n";
    });

    if (!hasDepNodes) {
      os << "    " << sanitizeForDot(allocNode->getHierId()) << " [label=\""
         << allocNode->getHierId() << "\", style=invisible];\n";
    }

    os << "  }\n\n";
  });

  /// Define dependency edges
  os << "  // Dependency Edges\n";
  forEachDepNode([&](DbDepNode *depNode) {
    for (auto *edge : depNode->getOutDepEdges()) {
      DbDepNode *toNode = edge->getTo();
      os << "  " << sanitizeForDot(depNode->getHierId()) << " -> "
         << sanitizeForDot(toNode->getHierId()) << ";\n";
    }
  });
  os << "\n";

  /// Define allocation edges
  os << "  // Allocation Edges\n";
  forEachAllocNode([&](DbAllocNode *allocNode) {
    for (auto *edge : allocNode->getOutAllocEdges()) {
      DbAllocNode *toNode = edge->getTo();

      /// Find representative node for 'from'
      std::string fromRep;
      bool fromHasDep = false;
      allocNode->forEachDepNode([&](DbDepNode *n) {
        if (!fromHasDep) {
          fromRep = sanitizeForDot(n->getHierId());
          fromHasDep = true;
        }
      });
      if (!fromHasDep) {
        fromRep = sanitizeForDot(allocNode->getHierId());
      }

      /// Find representative node for 'to'
      std::string toRep;
      bool toHasDep = false;
      toNode->forEachDepNode([&](DbDepNode *n) {
        if (!toHasDep) {
          toRep = sanitizeForDot(n->getHierId());
          toHasDep = true;
        }
      });
      if (!toHasDep) {
        toRep = sanitizeForDot(toNode->getHierId());
      }

      os << "  " << fromRep << " -> " << toRep << " [lhead=cluster_"
         << sanitizeForDot(toNode->getHierId()) << ", ltail=cluster_"
         << sanitizeForDot(allocNode->getHierId())
         << ", style=dashed, color=blue];\n";
    }
  });

  os << "}\n";
}

///===----------------------------------------------------------------------===///
/// Private Construction Phases
///===----------------------------------------------------------------------===///

void DbGraph::collectNodes() {
  LLVM_DEBUG(DBGS() << "Phase 1 - Collecting nodes\n");

  /// Collect allocations and their depes in one pass
  func.walk([&](DbAllocOp allocOp) {
    auto *allocNode = getOrCreateAllocNode(allocOp);

    /// Find all depes that use this allocation
    for (auto *user : allocOp.getPtr().getUsers()) {
      if (auto depOp = dyn_cast<DbDepOp>(user)) {
        auto *depNode = allocNode->getOrCreateDepNode(depOp);
        depNodeMap[depOp] = depNode;
      }
    }
  });

  LLVM_DEBUG(DBGS() << "Phase 1 - Collected " << allocNodes.size()
                    << " allocations and " << depNodeMap.size()
                    << " depes\n");
}

void DbGraph::buildDependencies() {
  LLVM_DEBUG(DBGS() << "Phase 2 - Building dependencies\n");
  dataFlowAnalysis->analyze();

  LLVM_DEBUG(DBGS() << "Phase 2 - Data flow analysis finished\n");
}

///===----------------------------------------------------------------------===///
/// Helper Methods
///===----------------------------------------------------------------------===///

DbInfo *DbGraph::getNode(Operation *op) {
  if (!op)
    return nullptr;

  if (auto allocOp = dyn_cast<DbAllocOp>(op))
    return getAllocNode(allocOp);
  if (auto depOp = dyn_cast<DbDepOp>(op))
    return getDepNode(depOp);

  return nullptr;
}

DbInfo *DbGraph::getNode(DbDepOp dbOp) {
  if (!dbOp)
    return nullptr;
  return getNode(dbOp.getOperation());
}

DbInfo *DbGraph::getEntryNode() {
  if (!allocNodes.empty())
    return allocNodes.begin()->second.get();

  return nullptr;
}

bool DbGraph::addEdge(DbDepOp from, DbDepOp to, DbDepType type) {
  return insertDepEdge(from, to, type);
}

bool DbGraph::addEdge(DbAllocOp from, DbAllocOp to) {
  return insertAllocEdge(from, to);
}

size_t DbGraph::getNumEdges() const {
  return depEdges.size() + allocEdges.size();
}
