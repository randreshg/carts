///==========================================================================///
/// File: EdtGraph.cpp
/// Implementation of EdtGraph for EDT analysis and optimization.
///==========================================================================///

#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Edt/EdtDataFlowAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Graphs/Edt/EdtEdge.h"
#include "arts/Analysis/Graphs/Edt/EdtNode.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/Loop/LoopNode.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/LocationMetadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;
using namespace mlir::arts;

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(edt_analysis);

namespace {
StringRef depTypeToString(DbDepType type) {
  switch (type) {
  case DbDepType::RAW:
    return "RAW";
  case DbDepType::WAR:
    return "WAR";
  case DbDepType::WAW:
    return "WAW";
  case DbDepType::RAR:
    return "RAR";
  }
  return "Dep";
}
} // namespace

EdtGraph::EdtGraph(func::FuncOp func, DbGraph *dbGraph, ArtsAnalysisManager *AM)
    : func(func), dbGraph(dbGraph), analysisManager(AM) {
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
  linkEdtsToLoops();
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
  childrenCache.clear();
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
    auto newNode = std::make_unique<EdtNode>(edtOp, analysisManager);
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

    SmallVector<DbEdge, 4> dbDeps;
    for (auto *edge : task->getOutEdges())
      if (auto *depEdge = dyn_cast<EdtDepEdge>(edge)) {
        auto edges = depEdge->getEdges();
        dbDeps.append(edges.begin(), edges.end());
      }
    if (!dbDeps.empty()) {
      os << "  Data Dependencies:\n";
      for (const auto &edge : dbDeps) {
        std::string label = "db";
        if (auto *prod = edge.producer)
          if (auto *alloc = prod->getRootAlloc())
            label = alloc->getHierId().str();
        os << "    - " << label << " (" << depTypeToString(edge.depType)
           << ")\n";
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

  if (!includeAnalysis) {
    /// Original graph visualization format
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
    os << llvm::json::Value(std::move(root)) << "\n";
    return;
  }

  /// ArtsMate-compatible EDT entities export
  Array edts;

  forEachNode([&](NodeBase *node) {
    auto *edtNode = cast<EdtNode>(node);
    const EdtInfo &info = edtNode->getInfo();
    EdtOp edtOp = edtNode->getEdtOp();

    Object edt;

    /// ARTS ID
    int64_t artsId = analysisManager->getMetadataManager().getIdRegistry().get(
        edtNode->getOp());
    if (artsId != 0)
      edt["arts_id"] = artsId;

    /// EDT type (task for now)
    edt["type"] = "task";

    /// Concurrency scope
    edt["concurrency"] = "intranode";

    /// Dependencies from EdtInfo's dbAllocsRead/Written
    Array deps;
    auto &idRegistry = analysisManager->getMetadataManager().getIdRegistry();

    /// Collect all accessed DBs with their modes
    DenseMap<DbAllocOp, std::string> dbModes;
    for (DbAllocOp db : info.dbAllocsRead)
      dbModes[db] = "in";
    for (DbAllocOp db : info.dbAllocsWritten) {
      if (dbModes.count(db))
        dbModes[db] = "inout"; /// Both read and written
      else
        dbModes[db] = "out";
    }

    for (auto &[dbOp, mode] : dbModes) {
      int64_t dbId = idRegistry.get(dbOp.getOperation());
      if (dbId != 0)
        deps.push_back(Object{{"db", dbId}, {"mode", mode}});
    }
    edt["deps"] = std::move(deps);

    /// Loop metadata
    bool inLoop = edtNode->hasParallelLoopMetadata();
    edt["in_loop"] = inLoop;

    if (inLoop && !edtNode->getAssociatedLoops().empty()) {
      LoopNode *loop = edtNode->getAssociatedLoops()[0];
      auto lb = loop->getLowerBoundConstant();
      auto ub = loop->getUpperBoundConstant();
      Array bounds;
      bounds.push_back(lb ? llvm::json::Value(*lb)
                          : llvm::json::Value(nullptr));
      bounds.push_back(ub ? llvm::json::Value(*ub)
                          : llvm::json::Value(nullptr));
      edt["loop_bounds"] = std::move(bounds);
    }

    /// Source location
    auto loc = LocationMetadata::fromLocation(edtOp->getLoc());
    if (loc.isValid())
      edt["loc"] = loc.file + ":" + std::to_string(loc.line);
    else
      edt["loc"] = "unknown";

    edts.push_back(std::move(edt));
  });

  os << llvm::json::Value(std::move(edts)) << "\n";
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

/// Link EDTs to their enclosing loops (if any)
void EdtGraph::linkEdtsToLoops() {
  ARTS_INFO("Phase 1.5 - Linking EDTs to enclosing loops");

  if (!dbGraph) {
    ARTS_WARN("  No DbGraph available, skipping loop linking");
    return;
  }

  /// Get LoopAnalysis through DbGraph -> DbAnalysis
  auto *dbAnalysis = dbGraph->getAnalysis();
  if (!dbAnalysis) {
    ARTS_WARN("  No DbAnalysis available, skipping loop linking");
    return;
  }

  auto *loopAnalysis = dbAnalysis->getLoopAnalysis();
  if (!loopAnalysis) {
    ARTS_WARN("  No LoopAnalysis available, skipping loop linking");
    return;
  }

  size_t linkedCount = 0;
  for (auto &[edtOp, edtNode] : edtNodes) {
    Operation *op = edtOp.getOperation();

    SmallVector<LoopNode *, 4> enclosingLoops;
    loopAnalysis->collectEnclosingLoops(op, enclosingLoops);
    if (!enclosingLoops.empty()) {
      edtNode->setAssociatedLoops(enclosingLoops);
      linkedCount += enclosingLoops.size();
      ARTS_DEBUG("  Linked " << edtNode->getHierId() << " to "
                             << enclosingLoops.size() << " enclosing loops");
    }
  }

  ARTS_INFO("Linked " << linkedCount << " loop references to EDTs");
}

/// Build inter-EDT edges by connecting release sites in one EDT to acquire
/// sites in another EDT for the same root DbAlloc.
void EdtGraph::buildDependencies() {
  ARTS_INFO("Phase 2 - Building EDT dependencies");
  assert(dbGraph && "DbGraph is required to build EDT dependencies");
  assert(analysisManager &&
         "ArtsAnalysisManager is required to build EDT dependencies");
  EdtDataFlowAnalysis dataflow(dbGraph, analysisManager);
  auto deps = dataflow.run(func);

  unsigned created = 0;
  for (auto &dep : deps) {
    if (!dep.from || !dep.to || dep.edges.empty())
      continue;
    EdtNode *fromNode = getEdtNode(dep.from);
    EdtNode *toNode = getEdtNode(dep.to);
    if (!fromNode || !toNode)
      continue;

    auto iter = dep.edges.begin();
    auto *edge = new EdtDepEdge(fromNode, toNode, *iter);
    ++iter;
    for (; iter != dep.edges.end(); ++iter)
      edge->appendEdge(*iter);

    if (addEdge(fromNode, toNode, edge)) {
      ++created;
    } else {
      delete edge;
    }
  }

  ARTS_INFO("Created " << created << " EDT dependency edges");
}
