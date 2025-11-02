///==========================================================================
/// File: EdtGraph.cpp
/// Implementation of EdtGraph for EDT analysis and optimization.
///==========================================================================

#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/Analysis/Graphs/Db/DbEdge.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Graphs/Edt/EdtEdge.h"
#include "arts/Analysis/Graphs/Edt/EdtNode.h"
#include "arts/Utils/ArtsUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;
using namespace mlir::arts;

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(edt_analysis);

EdtGraph::EdtGraph(func::FuncOp func, DbGraph *dbGraph)
    : func(func), dbGraph(dbGraph) {
  ARTS_INFO("Creating EDT graph for function: " << func.getName().str());
}

/// Build task graph nodes and DB-induced dependency edges.
void EdtGraph::build() {
  if (isBuilt && !needsRebuild)
    return;
  ARTS_INFO("Building EDT graph");
  invalidate();
  assert(dbGraph && "DbGraph is required to build EdtGraph");
  collectNodes();
  buildDependencies();
  isBuilt = true;
  needsRebuild = false;
}

void EdtGraph::buildNodesOnly() {
  if (isBuilt && !needsRebuild)
    return;
  ARTS_INFO("Building EDT graph nodes only");
  invalidate();
  collectNodes();
  isBuilt = true;
  needsRebuild = false;
}

void EdtGraph::invalidate() {
  edtNodes.clear();
  edges.clear();
  nodes.clear();
  isBuilt = false;
  needsRebuild = true;
}

NodeBase *EdtGraph::getEntryNode() const {
  if (!edtNodes.empty())
    return edtNodes.begin()->second.get();
  return nullptr;
}

NodeBase *EdtGraph::getOrCreateNode(Operation *op) {
  if (auto edtOp = dyn_cast<EdtOp>(op)) {
    auto it = edtNodes.find(edtOp);
    if (it != edtNodes.end())
      return it->second.get();
    auto newNode = std::make_unique<EdtNode>(edtOp);
    newNode->setHierId("T" + std::to_string(edtNodes.size() + 1));
    NodeBase *ptr = newNode.get();
    edtNodes[edtOp] = std::move(newNode);
    nodes.push_back(ptr);
    return ptr;
  }
  return nullptr;
}

NodeBase *EdtGraph::getNode(Operation *op) const {
  if (auto edtOp = dyn_cast<EdtOp>(op)) {
    auto it = edtNodes.find(edtOp);
    return it != edtNodes.end() ? it->second.get() : nullptr;
  }
  return nullptr;
}

EdtNode *EdtGraph::getEdtNode(EdtOp edt) const {
  auto it = edtNodes.find(edt);
  return it != edtNodes.end() ? it->second.get() : nullptr;
}

void EdtGraph::forEachNode(const std::function<void(NodeBase *)> &fn) const {
  for (auto *node : nodes)
    fn(node);
}

bool EdtGraph::addEdge(NodeBase *from, NodeBase *to, EdgeBase *edge) {
  auto key = std::make_pair(from, to);
  if (edges.count(key))
    return false;
  edges[key] = std::unique_ptr<EdgeBase>(edge);
  from->addOutEdge(edge);
  to->addInEdge(edge);
  return true;
}

bool EdtGraph::isEdtReachable(EdtOp fromOp, EdtOp toOp) {
  EdtNode *from = static_cast<EdtNode *>(getNode(fromOp.getOperation()));
  EdtNode *to = static_cast<EdtNode *>(getNode(toOp.getOperation()));
  if (!from || !to)
    return false;
  for (auto *edge : from->getOutEdges()) {
    if (edge->getTo() == to)
      return true;
  }
  return false;
}

SmallVector<NodeBase *, 4> EdtGraph::getDbDeps(EdtOp edt) const {
  SmallVector<NodeBase *, 4> result;
  EdtNode *edtNode = getEdtNode(edt);
  if (!edtNode)
    return result;

  edt.walk([&](Operation *op) {
    if (auto dbNode = dbGraph->getNode(op))
      result.push_back(dbNode);
  });
  return result;
}

void EdtGraph::print(llvm::raw_ostream &os) {
  os << "===============================================\n";
  os << "EdtGraph for function: " << func.getName().str() << "\n";
  os << "===============================================\n";
  os << "Summary:\n";
  os << "  Tasks: " << edtNodes.size() << "\n";
  os << "  Edges: " << edges.size() << "\n\n";

  os << "Task Hierarchy:\n";
  for (auto &pair : edtNodes) {
    EdtNode *task = pair.second.get();
    os << "Task [" << task->getHierId() << "]: ";
    task->print(os);
    os << "\n";

    SmallVector<NodeBase *, 4> dbDeps = getDbDeps(pair.first);
    if (!dbDeps.empty()) {
      os << "  Data Dependencies:\n";
      for (NodeBase *dbNode : dbDeps) {
        os << "    - " << dbNode->getHierId() << "\n";
      }
    }
  }

  if (!edges.empty()) {
    os << "Task Dependencies:\n";
    for (const auto &pair : edges) {
      auto *edge = pair.second.get();
      os << "  [" << edge->getFrom()->getHierId() << "] -> ["
         << edge->getTo()->getHierId() << "] (DB: " << edge->getType() << ")\n";
    }
  }
}

void EdtGraph::exportToJson(llvm::raw_ostream &os, bool includeAnalysis) const {
  using namespace llvm::json;

  Object root;
  Array nodesArr;
  forEachNode([&](NodeBase *node) {
    nodesArr.push_back(Object{{"id", sanitizeString(node->getHierId())},
                              {"group", "edt"},
                              {"nodeKind", "EdtTask"}});
  });

  Array edgesArr;
  forEachNode([&](NodeBase *from) {
    for (auto *edge : from->getOutEdges()) {
      edgesArr.push_back(
          Object{{"from", sanitizeString(edge->getFrom()->getHierId())},
                 {"to", sanitizeString(edge->getTo()->getHierId())},
                 {"edgeKind", "Dep"},
                 {"label", edge->getType().str()}});
    }
  });

  root["nodes"] = std::move(nodesArr);
  root["edges"] = std::move(edgesArr);

  /// Include analysis data if requested
  if (includeAnalysis) {
    /// Add EDT-specific analysis data here if needed in the future
    root["analysis"] = Object{{"numTasks", size()},
                              {"function", getFunction().getName().str()}};
  }

  os << llvm::json::Value(std::move(root)) << "\n";
}

/// Helper to populate and sort children cache for a node
void EdtGraph::populateChildrenCache(NodeBase *node) {
  auto &vec = childrenCache[node];
  vec.clear();

  // Collect children from the node's outgoing edges
  for (auto *edge : node->getOutEdges())
    vec.push_back(edge->getTo());

  // Sort by hierarchical ID for deterministic order
  llvm::sort(vec, [&](NodeBase *a, NodeBase *b) {
    auto hierIdA = a ? a->getHierId() : StringRef("");
    auto hierIdB = b ? b->getHierId() : StringRef("");
    return hierIdA < hierIdB;
  });
}

GraphBase::ChildIterator EdtGraph::childBegin(NodeBase *node) {
  populateChildrenCache(node);
  return childrenCache[node].begin();
}

GraphBase::ChildIterator EdtGraph::childEnd(NodeBase *node) {
  auto it = childrenCache.find(node);
  if (it == childrenCache.end()) {
    populateChildrenCache(node);
    return childrenCache[node].end();
  }
  return it->second.end();
}

/// Collect all arts.edt operations in the function and create task nodes.
void EdtGraph::collectNodes() {
  ARTS_INFO("Phase 1 - Collecting EDT nodes");

  func.walk([&](EdtOp edtOp) { getOrCreateNode(edtOp.getOperation()); });

  ARTS_INFO("Collected " << edtNodes.size() << " tasks");
}

SmallVector<EdtNode *> EdtGraph::getAllEdtNodes() {
  SmallVector<EdtNode *> result;
  for (auto &kv : edtNodes)
    result.push_back(kv.second.get());
  return result;
}

/// Build inter-EDT edges by connecting release sites in one EDT to acquire
/// sites in another EDT for the same root DbAlloc.
void EdtGraph::buildDependencies() {
  ARTS_INFO("Phase 2 - Building EDT dependencies");

  if (!dbGraph) {
    ARTS_DEBUG("No DbGraph available, skipping dependency construction");
    return;
  }

  auto allEdts = getAllEdtNodes();
  unsigned edgeCount = 0;

  /// For each pair of EDTs, check for data dependencies
  for (size_t i = 0; i < allEdts.size(); ++i) {
    for (size_t j = i + 1; j < allEdts.size(); ++j) {
      auto *task1 = allEdts[i];
      auto *task2 = allEdts[j];

      /// Check program order
      unsigned order1 = dbGraph->getOpOrder(task1->getEdtOp().getOperation());
      unsigned order2 = dbGraph->getOpOrder(task2->getEdtOp().getOperation());

      /// Only create edges in program order direction
      EdtNode *from = (order1 < order2) ? task1 : task2;
      EdtNode *to = (order1 < order2) ? task2 : task1;

      /// Try to lift DB dependencies to EDT level
      if (liftDbDepsToEdtDeps(from, to)) {
        edgeCount++;
      }
    }
  }

  ARTS_INFO("Created " << edgeCount << " EDT dependency edges");
  ARTS_INFO("Phase 2 - Built " << edges.size() << " EDT dependencies");
}

bool EdtGraph::liftDbDepsToEdtDeps(EdtNode *fromTask, EdtNode *toTask) {
  ARTS_DEBUG("Checking dependencies: " << fromTask->getHierId() << " -> "
                                       << toTask->getHierId());

  /// Get all DB acquires for each task
  SmallVector<DbAcquireNode *> fromAcquires;
  SmallVector<DbAcquireNode *> toAcquires;

  /// Collect acquires from EDT dependencies
  for (auto dep : fromTask->getEdtOp().getDependencies()) {
    if (auto acqOp = dep.getDefiningOp<DbAcquireOp>()) {
      if (auto *node = dbGraph->getDbAcquireNode(acqOp))
        fromAcquires.push_back(node);
    }
  }

  for (auto dep : toTask->getEdtOp().getDependencies()) {
    if (auto acqOp = dep.getDefiningOp<DbAcquireOp>()) {
      if (auto *node = dbGraph->getDbAcquireNode(acqOp))
        toAcquires.push_back(node);
    }
  }

  /// Find DB-level dependencies between acquires
  SmallVector<DbDepEdge *> underlyingDeps;
  bool hasRaw = false, hasWaw = false, hasWar = false;

  for (auto *fromAcq : fromAcquires) {
    for (auto *toAcq : toAcquires) {
      /// Check if they access the same allocation
      if (fromAcq->getParent() != toAcq->getParent())
        continue;

      /// Look for edge in DbGraph
      auto *edge = dbGraph->getEdge(fromAcq, toAcq);
      if (!edge)
        continue;

      auto *depEdge = dyn_cast<DbDepEdge>(edge);
      if (!depEdge)
        continue;

      underlyingDeps.push_back(depEdge);

      /// Classify dependency type
      switch (depEdge->getDepType()) {
      case DbDepType::RAW:
        hasRaw = true;
        break;
      case DbDepType::WAW:
        hasWaw = true;
        break;
      case DbDepType::WAR:
        hasWar = true;
        break;
      case DbDepType::RAR:
        break; /// Affinity, not ordering
      }

      ARTS_DEBUG("  Found DB dependency: " << fromAcq->getHierId() << " -> "
                                           << toAcq->getHierId() << " ("
                                           << depEdge->getType() << ")");
    }
  }

  /// No dependencies found
  if (underlyingDeps.empty())
    return false;

  /// Create EDT-level edge
  EdgeBase *edtEdge = nullptr;

  /// Find the DB node that causes the dependency
  NodeBase *dbNode = nullptr;
  for (auto *dep : underlyingDeps) {
    /// Use the DB node from any dependency
    dbNode = dep->getFrom();
    break;
  }

  if (hasRaw) {
    /// Consumer reads producer's writes
    edtEdge = new EdtDepEdge(fromTask, toTask, dbNode);
    ARTS_DEBUG("  Creating EDT DEP edge (RAW)");
  } else if (hasWaw) {
    /// Both write same data
    edtEdge = new EdtDepEdge(fromTask, toTask, dbNode);
    ARTS_DEBUG("  Creating EDT DEP edge (WAW)");
  } else if (hasWar) {
    /// WAR is control dependency
    edtEdge = new EdtDepEdge(fromTask, toTask, dbNode);
    ARTS_DEBUG("  Creating EDT CONTROL edge (WAR)");
  }

  if (edtEdge && addEdge(fromTask, toTask, edtEdge)) {
    ARTS_INFO("  Created EDT edge: " << fromTask->getHierId() << " -> "
                                     << toTask->getHierId());
    return true;
  }

  delete edtEdge;
  return false;
}
