///==========================================================================
/// File: ArtsGraphManager.cpp
/// Implementation of ArtsGraphManager for managing analysis graphs.
///==========================================================================

#include "arts/Analysis/Graphs/ArtsGraphManager.h"
#include "arts/Analysis/Edt/EdtAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Graphs/Edt/EdtConcurrencyGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtNode.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"

using namespace mlir;
using namespace mlir::arts;

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(arts_graph_manager);

ArtsGraphManager::ArtsGraphManager(std::unique_ptr<DbGraph> dbGraph,
                                   std::unique_ptr<EdtGraph> edtGraph)
    : dbGraph(std::move(dbGraph)), edtGraph(std::move(edtGraph)) {
  ARTS_INFO("Creating ArtsGraphManager");
}

void ArtsGraphManager::build() {
  ARTS_INFO("Building graphs via ArtsGraphManager");
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

NodeBase *ArtsGraphManager::getNode(Operation *op) const {
  if (NodeBase *node = dbGraph->getNode(op))
    return node;
  return edtGraph->getNode(op);
}

bool ArtsGraphManager::isEdtDependentOnDb(EdtOp edt, DbAllocOp alloc) {
  SmallVector<NodeBase *, 4> dbDeps = getDbNodesForEdt(edt);
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

SmallVector<NodeBase *, 4> ArtsGraphManager::getDbNodesForEdt(EdtOp edt) const {
  return edtGraph->getDbDeps(edt);
}