///==========================================================================///
/// File: EdtGraph.cpp
/// Implementation of EdtGraph for EDT analysis and optimization.
///==========================================================================///

#include "arts/analysis/graphs/edt/EdtGraph.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/edt/EdtAnalysis.h"
#include "arts/analysis/edt/EdtDataFlowAnalysis.h"
#include "arts/analysis/graphs/db/DbNode.h"
#include "arts/analysis/graphs/edt/EdtEdge.h"
#include "arts/analysis/graphs/edt/EdtNode.h"
#include "arts/analysis/loop/LoopAnalysis.h"
#include "arts/analysis/loop/LoopNode.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/Utils.h"
#include "arts/utils/metadata/LocationMetadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;
using namespace mlir::arts;

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(edt_graph);

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

EdtGraph::EdtGraph(func::FuncOp func, DbGraph *dbGraph, EdtAnalysis *EA)
    : func(func), dbGraph(dbGraph), edtAnalysis(EA) {
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
    auto newNode = std::make_unique<EdtNode>(edtOp, edtAnalysis);
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

bool EdtGraph::areEdtsIndependent(EdtOp a, EdtOp b) {
  if (!a || !b || a == b)
    return false;

  EdtNode *nodeA = getEdtNode(a);
  EdtNode *nodeB = getEdtNode(b);
  if (!nodeA || !nodeB)
    return false;

  Operation *opA = nodeA->getOp();
  Operation *opB = nodeB->getOp();
  if (!opA || !opB)
    return false;

  return !DbUtils::hasSharedWritableRootConflict(opA, opB);
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

llvm::json::Value EdtGraph::exportToJsonValue(bool includeAnalysis) const {
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
    return llvm::json::Value(std::move(root));
  }

  /// ArtsMate-compatible EDT entities export
  Array edts;

  forEachNode([&](NodeBase *node) {
    auto *edtNode = cast<EdtNode>(node);
    const EdtInfo &info = edtNode->getInfo();
    EdtOp edtOp = edtNode->getEdtOp();

    Object edt;

    /// ARTS ID
    int64_t artsId =
        edtAnalysis->getMetadataManager().getIdRegistry().get(edtNode->getOp());
    if (artsId != 0)
      edt["arts_id"] = artsId;

    /// EDT type (task for now)
    edt["type"] = "task";

    /// Concurrency scope
    edt["concurrency"] = "intranode";

    auto &idRegistry = edtAnalysis->getMetadataManager().getIdRegistry();

    /// Associated loops (EDT ↔ Loop bidirectional relationship)
    if (!edtNode->getAssociatedLoops().empty()) {
      Array loopIds;
      for (LoopNode *loop : edtNode->getAssociatedLoops()) {
        int64_t loopId = idRegistry.get(loop->getOp());
        if (loopId != 0)
          loopIds.push_back(loopId);
      }
      if (!loopIds.empty())
        edt["loop_ids"] = std::move(loopIds);
    }

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

    /// Static analysis data from EdtInfo
    Object staticAnalysis;
    staticAnalysis["max_loop_depth"] = (int64_t)info.maxLoopDepth;
    staticAnalysis["order_index"] = (int64_t)info.orderIndex;
    edt["static_analysis"] = std::move(staticAnalysis);

    edts.push_back(std::move(edt));
  });

  return llvm::json::Value(std::move(edts));
}

void EdtGraph::exportToJson(llvm::raw_ostream &os, bool includeAnalysis) const {
  os << exportToJsonValue(includeAnalysis) << "\n";
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
  assert(edtAnalysis && "EdtAnalysis is required to build EDT dependencies");
  EdtDataFlowAnalysis dataflow(dbGraph, &edtAnalysis->getAnalysisManager());
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
