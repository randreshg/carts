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
  if (depEdges.count(key))
    return false;

  auto edge = std::make_unique<DbDepEdge>(fromNode, toNode, type);
  fromNode->addOutDepEdge(edge.get());
  toNode->addInDepEdge(edge.get());
  depEdges[key] = std::move(edge);
  return true;
}

bool DbGraph::insertAllocEdge(DbCreateOp from, DbCreateOp to) {
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

DbDepEdge *DbGraph::getDependenceEdge(DbAccessOp from, DbAccessOp to) {
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

const DenseSet<DbDepEdge *> &DbGraph::getInDepEdges(DbAccessNode *node) {
  return node->getInDepEdges();
}

const DenseSet<DbDepEdge *> &DbGraph::getOutDepEdges(DbAccessNode *node) {
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

  collectNodes();
  buildDependencies();

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
  os << "===============================================\n";
  os << "DbGraph for function: " << func.getName() << "\n";
  os << "===============================================\n";
  os << "Summary:\n";
  os << "  Allocations: " << allocNodes.size() << "\n";
  os << "  Access nodes: " << accessNodeMap.size() << "\n";
  os << "  Dependence edges: " << depEdges.size() << "\n";
  os << "  Allocation edges: " << allocEdges.size() << "\n";
  os << "\n";

  // Print allocation hierarchy with their access nodes and dependencies
  os << "Allocation Hierarchy:\n";
  os << "=====================\n";
  forEachAllocNode([&](DbAllocNode *allocNode) {
    os << "Allocation [" << allocNode->getHierId() << "]: ";
    allocNode->print(os);
    os << "\n";

    // Count and list access nodes for this allocation
    std::vector<DbAccessNode *> accessNodes;
    allocNode->forEachAccessNode(
        [&](DbAccessNode *accessNode) { accessNodes.push_back(accessNode); });

    if (accessNodes.empty()) {
      os << "   └── (no access nodes)\n";
    } else {
      os << "   └── Access nodes (" << accessNodes.size() << "):\n";

      for (size_t i = 0; i < accessNodes.size(); ++i) {
        DbAccessNode *accessNode = accessNodes[i];
        bool isLast = (i == accessNodes.size() - 1);
        os << "       " << (isLast ? "└──" : "├──") << " ["
           << accessNode->getHierId() << "]: ";
        accessNode->print(os);
        os << "\n";

        // Show outgoing dependencies for this access node
        auto outEdges = getOutDepEdges(accessNode);
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

    bool hasAccessNodes = false;
    allocNode->forEachAccessNode([&](DbAccessNode *accessNode) {
      hasAccessNodes = true;
      os << "    " << sanitizeForDot(accessNode->getHierId()) << " [label=\""
         << accessNode->getHierId() << "\"];\n";
    });

    if (!hasAccessNodes) {
      os << "    " << sanitizeForDot(allocNode->getHierId()) << " [label=\""
         << allocNode->getHierId() << "\", style=invisible];\n";
    }

    os << "  }\n\n";
  });

  /// Define dependency edges
  os << "  // Dependency Edges\n";
  forEachAccessNode([&](DbAccessNode *accessNode) {
    for (auto *edge : accessNode->getOutDepEdges()) {
      DbAccessNode *toNode = edge->getTo();
      os << "  " << sanitizeForDot(accessNode->getHierId()) << " -> "
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
      bool fromHasAccess = false;
      allocNode->forEachAccessNode([&](DbAccessNode *n) {
        if (!fromHasAccess) {
          fromRep = sanitizeForDot(n->getHierId());
          fromHasAccess = true;
        }
      });
      if (!fromHasAccess) {
        fromRep = sanitizeForDot(allocNode->getHierId());
      }

      /// Find representative node for 'to'
      std::string toRep;
      bool toHasAccess = false;
      toNode->forEachAccessNode([&](DbAccessNode *n) {
        if (!toHasAccess) {
          toRep = sanitizeForDot(n->getHierId());
          toHasAccess = true;
        }
      });
      if (!toHasAccess) {
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

  /// Collect allocations and their accesses in one pass
  func.walk([&](DbCreateOp allocOp) {
    auto *allocNode = getOrCreateAllocNode(allocOp);

    /// Find all subviews and casts that use this allocation
    for (auto *user : allocOp.getPtr().getUsers()) {
      if (isa<memref::SubViewOp>(user) || isa<memref::CastOp>(user)) {
        auto accessOp = getDbAccessOp(user);
        auto *accessNode = allocNode->getOrCreateAccessNode(accessOp);
        accessNodeMap[accessOp] = accessNode;
      }
    }
  });

  LLVM_DEBUG(DBGS() << "Phase 1 - Collected " << allocNodes.size()
                    << " allocations and " << accessNodeMap.size()
                    << " accesses\n");
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

  if (auto allocOp = dyn_cast<DbCreateOp>(op))
    return getAllocNode(allocOp);
  if (isa<memref::SubViewOp>(op) || isa<memref::CastOp>(op)) {
    auto accessOp = getDbAccessOp(op);
    return getAccessNode(accessOp);
  }

  return nullptr;
}

DbInfo *DbGraph::getNode(DbControlOp dbOp) {
  if (!dbOp)
    return nullptr;
  return getNode(dbOp.getOperation());
}

DbInfo *DbGraph::getEntryNode() {
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
