//===----------------------------------------------------------------------===//
// Edt/EdtGraph.cpp - Implementation of EdtGraph
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtEdge.h"
#include "arts/Analysis/Graphs/Edt/EdtNode.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>

using namespace mlir;
using namespace mlir::arts;

#define dbgs() llvm::dbgs()
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

std::string EdtGraph::sanitizeForDot(StringRef s) const {
  std::string id = s.str();
  std::replace(id.begin(), id.end(), '.', '_');
  std::replace(id.begin(), id.end(), '-', '_');
  return id;
}

EdtGraph::EdtGraph(func::FuncOp func, DbGraph *dbGraph)
    : func(func), dbGraph(dbGraph) {
  LLVM_DEBUG(DBGS() << "Creating EDT graph for function: "
                    << func.getName().str() << "\n");
}

void EdtGraph::build() {
  if (isBuilt && !needsRebuild)
    return;
  LLVM_DEBUG(DBGS() << "Building EDT graph\n");
  invalidate();
  collectNodes();
  buildDependencies();
  isBuilt = true;
  needsRebuild = false;
}

void EdtGraph::invalidate() {
  taskNodes.clear();
  edges.clear();
  nodes.clear();
  isBuilt = false;
  needsRebuild = true;
}

NodeBase *EdtGraph::getEntryNode() const {
  if (!taskNodes.empty())
    return taskNodes.begin()->second.get();
  return nullptr;
}

NodeBase *EdtGraph::getOrCreateNode(Operation *op) {
  if (auto edtOp = dyn_cast<EdtOp>(op)) {
    auto it = taskNodes.find(edtOp);
    if (it != taskNodes.end())
      return it->second.get();
    auto newNode = std::make_unique<EdtTaskNode>(edtOp);
    newNode->setHierId("T" + std::to_string(taskNodes.size() + 1));
    NodeBase *ptr = newNode.get();
    taskNodes[edtOp] = std::move(newNode);
    nodes.push_back(ptr);
    return ptr;
  }
  return nullptr;
}

NodeBase *EdtGraph::getNode(Operation *op) const {
  if (auto edtOp = dyn_cast<EdtOp>(op)) {
    auto it = taskNodes.find(edtOp);
    return it != taskNodes.end() ? it->second.get() : nullptr;
  }
  return nullptr;
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

bool EdtGraph::isTaskReachable(EdtOp fromOp, EdtOp toOp) {
  EdtTaskNode *from =
      static_cast<EdtTaskNode *>(getNode(fromOp.getOperation()));
  EdtTaskNode *to = static_cast<EdtTaskNode *>(getNode(toOp.getOperation()));
  if (!from || !to)
    return false;
  for (auto *edge : from->getOutEdges()) {
    if (edge->getTo() == to)
      return true;
  }
  return false;
}

SmallVector<NodeBase *> EdtGraph::getDbDependencies(EdtOp task) const {
  SmallVector<NodeBase *> result;
  EdtTaskNode *node = static_cast<EdtTaskNode *>(getNode(task.getOperation()));
  if (!node)
    return result;

  task.walk([&](Operation *op) {
    if (auto dbNode = dbGraph->getNode(op)) {
      result.push_back(dbNode);
    }
  });
  return result;
}

void EdtGraph::print(llvm::raw_ostream &os) const {
  os << "===============================================\n";
  os << "EdtGraph for function: " << const_cast<func::FuncOp &>(func).getName()
     << "\n";
  os << "===============================================\n";
  os << "Summary:\n";
  os << "  Tasks: " << taskNodes.size() << "\n";
  os << "  Edges: " << edges.size() << "\n\n";

  os << "Task Hierarchy:\n";
  for (const auto &pair : taskNodes) {
    EdtTaskNode *task = pair.second.get();
    os << "Task [" << task->getHierId() << "]: ";
    task->print(os);
    os << "\n";

    SmallVector<NodeBase *> dbDeps = getDbDependencies(pair.first);
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

void EdtGraph::exportToDot(llvm::raw_ostream &os) const {
  os << "digraph EdtGraph {\n";
  os << "  node [shape=ellipse];\n";
  os << "  graph [label=\"" << const_cast<func::FuncOp &>(func).getName()
     << "\", labelloc=t, fontsize=20];\n\n";

  for (const auto &pair : taskNodes) {
    EdtTaskNode *task = pair.second.get();
    os << "  " << sanitizeForDot(task->getHierId()) << " [label=\""
       << task->getHierId() << "\"];\n";
  }

  for (const auto &pair : edges) {
    EdgeBase *edge = pair.second.get();
    os << "  " << sanitizeForDot(edge->getFrom()->getHierId()) << " -> "
       << sanitizeForDot(edge->getTo()->getHierId()) << " [label=\""
       << edge->getType() << "\"];\n";
  }

  os << "}\n";
}

void EdtGraph::collectNodes() {
  LLVM_DEBUG(DBGS() << "Phase 1 - Collecting EDT nodes\n");

  func.walk([&](EdtOp edtOp) { getOrCreateNode(edtOp.getOperation()); });

  LLVM_DEBUG(DBGS() << "Collected " << taskNodes.size() << " tasks\n");
}

void EdtGraph::buildDependencies() {
  LLVM_DEBUG(DBGS() << "Phase 2 - Building EDT dependencies\n");

  for (auto &fromPair : taskNodes) {
    EdtOp fromOp = fromPair.first;
    EdtTaskNode *fromNode = fromPair.second.get();

    fromOp.walk([&](Operation *op) {
      if (auto releaseOp = dyn_cast<DbReleaseOp>(op)) {
        NodeBase *releaseNode = dbGraph->getNode(releaseOp.getOperation());
        if (!releaseNode)
          return;

        // Find other EDTs that acquire the same alloc
        for (auto &toPair : taskNodes) {
          EdtOp toOp = toPair.first;
          if (toOp == fromOp)
            continue;
          EdtTaskNode *toNode = toPair.second.get();

          toOp.walk([&](Operation *innerOp) {
            if (auto acquireOp = dyn_cast<DbAcquireOp>(innerOp)) {
              NodeBase *acquireNode =
                  dbGraph->getNode(acquireOp.getOperation());
              if (!acquireNode)
                return;
              DbAllocOp fromAlloc = dbGraph->getParentAlloc(releaseOp);
              DbAllocOp toAlloc = dbGraph->getParentAlloc(acquireOp);
              if (fromAlloc && toAlloc && fromAlloc == toAlloc) {
                auto edge =
                    std::make_unique<EdtDepEdge>(fromNode, toNode, releaseNode);
                addEdge(fromNode, toNode, edge.release());
              }
            }
          });
        }
      }
    });
  }

  LLVM_DEBUG(DBGS() << "Phase 2 - Built " << edges.size()
                    << " EDT dependencies\n");
}