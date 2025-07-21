//===----------------------------------------------------------------------===//
// ArtsGraphManager.cpp - Implementation of ArtsGraphManager
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Graphs/ArtsGraphManager.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Graphs/Edt/EdtNode.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"

using namespace mlir;
using namespace mlir::arts;

#define DEBUG_TYPE "arts-graph-manager"
#define dbgs() llvm::dbgs()
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

ArtsGraphManager::ArtsGraphManager(std::unique_ptr<DbGraph> dbGraph,
                                   std::unique_ptr<EdtGraph> edtGraph)
    : dbGraph(std::move(dbGraph)), edtGraph(std::move(edtGraph)) {
  LLVM_DEBUG(DBGS() << "Creating ArtsGraphManager\n");
}

void ArtsGraphManager::build() {
  LLVM_DEBUG(DBGS() << "Building graphs via ArtsGraphManager\n");
  dbGraph->build();
  edtGraph->build();
}

void ArtsGraphManager::invalidate() {
  dbGraph->invalidate();
  edtGraph->invalidate();
}

void ArtsGraphManager::print(llvm::raw_ostream &os) const {
  os << "=== ArtsGraphManager Summary ===\n";
  os << "Data Block Graph:\n";
  dbGraph->print(os);
  os << "\nEvent-Driven Task Graph:\n";
  edtGraph->print(os);
  os << "===============================\n";
}

void ArtsGraphManager::exportToDot(llvm::raw_ostream &os) const {
  os << "// DbGraph\n";
  dbGraph->exportToDot(os);
  os << "\n// EdtGraph\n";
  edtGraph->exportToDot(os);
}

NodeBase *ArtsGraphManager::getNode(Operation *op) const {
  if (NodeBase *node = dbGraph->getNode(op))
    return node;
  return edtGraph->getNode(op);
}

bool ArtsGraphManager::isTaskDependentOnDb(EdtOp task, DbAllocOp alloc) {
  SmallVector<NodeBase *> dbDeps = getDbNodesForTask(task);
  for (NodeBase *dbNode : dbDeps) {
    if (auto *allocNode = dyn_cast<DbAllocNode>(dbNode)) {
      if (allocNode->getDbAllocOp() == alloc)
        return true;
    } else if (auto *acquireNode = dyn_cast<DbAcquireNode>(dbNode)) {
      if (dbGraph->getParentAlloc(acquireNode->getOp()) == alloc)
        return true;
    } else if (auto *releaseNode = dyn_cast<DbReleaseNode>(dbNode)) {
      if (dbGraph->getParentAlloc(releaseNode->getOp()) == alloc)
        return true;
    }
  }
  return false;
}

SmallVector<NodeBase *> ArtsGraphManager::getDbNodesForTask(EdtOp task) const {
  return edtGraph->getDbDependencies(task);
}

void ArtsGraphManager::buildConcurrencyView(llvm::raw_ostream &os) const {
  os << "digraph ConcurrencyView {\n";
  os << "  node [shape=ellipse];\n";
  os << "  graph [label=\"Concurrency View\", labelloc=t, fontsize=20];\n\n";

  // For each pair of EDTs, check if they can run concurrently
  DenseMap<EdtOp, EdtTaskNode *> taskNodes;
  edtGraph->forEachNode([&](NodeBase *node) {
    if (auto *taskNode = dyn_cast<EdtTaskNode>(node)) {
      taskNodes[taskNode->getEdtOp()] = taskNode;
      os << "  " << taskNode->getHierId() << " [label=\""
         << taskNode->getHierId() << "\"];\n";
    }
  });

  for (auto &fromPair : taskNodes) {
    EdtOp fromOp = fromPair.first;
    EdtTaskNode *fromNode = fromPair.second;
    for (auto &toPair : taskNodes) {
      EdtOp toOp = toPair.first;
      if (fromOp == toOp)
        continue;
      EdtTaskNode *toNode = toPair.second;

      // Non-concurrent if direct dependency exists
      if (edtGraph->isTaskReachable(fromOp, toOp)) {
        os << "  " << fromNode->getHierId() << " -> " << toNode->getHierId()
           << " [label=\"TaskDep\"];\n";
        continue;
      }

      // Check for data conflicts via DbGraph
      SmallVector<NodeBase *> fromDeps = getDbNodesForTask(fromOp);
      SmallVector<NodeBase *> toDeps = getDbNodesForTask(toOp);
      for (NodeBase *fromDep : fromDeps) {
        for (NodeBase *toDep : toDeps) {
          if (auto *fromAlloc = dyn_cast<DbAllocNode>(fromDep)) {
            if (auto *toAlloc = dyn_cast<DbAllocNode>(toDep)) {
              if (dbGraph->mayAlias(fromAlloc->getDbAllocOp(),
                                    toAlloc->getDbAllocOp())) {
                os << "  " << fromNode->getHierId() << " -> "
                   << toNode->getHierId() << " [label=\"AliasConflict\"];\n";
                break;
              }
            }
          }
        }
      }
    }
  }

  os << "}\n";
}