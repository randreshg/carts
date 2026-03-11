///==========================================================================///
/// File: EdtNode.cpp
/// Implementation of EDT nodes for graph analysis.
///==========================================================================///

#include "arts/Analysis/Graphs/Edt/EdtNode.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/Loop/LoopNode.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

using namespace mlir;
using namespace mlir::arts;

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(edt_node);

EdtNode::EdtNode(EdtOp op, ArtsAnalysisManager *AM)
    : NodeBase(), edtOp(op), analysisManager(AM) {
  assert(edtOp.getOperation() && "Operation must always be available");
  if (AM)
    AM->getMetadataManager().getIdRegistry().getOrCreate(edtOp);
}

void EdtNode::print(llvm::raw_ostream &os) const {
  os << "EdtNode (" << getHierId() << ")\n";
}

bool EdtNode::hasParallelLoopMetadata() const {
  if (!edtOp || !analysisManager)
    return false;

  LoopAnalysis &loopAnalysis = analysisManager->getLoopAnalysis();
  ArtsMetadataManager &metadataManager = analysisManager->getMetadataManager();

  /// EdtOp is a lightweight wrapper around Operation*; take a mutable copy
  /// so we can call non-const MLIR methods from this const member function.
  EdtOp op = edtOp;
  SmallVector<LoopNode *, 8> loops;
  loopAnalysis.collectLoopsInOperation(op.getOperation(), loops);

  for (LoopNode *loopNode : loops) {
    metadataManager.ensureLoopMetadata(loopNode->getLoopOp());
    loopNode->importFromOp();

    bool noDeps = !loopNode->hasInterIterationDeps.has_value() ||
                  !loopNode->hasInterIterationDeps.value();
    if (loopNode->potentiallyParallel && noDeps) {
      ARTS_DEBUG(
          "  Loop is parallel-friendly: potentiallyParallel=true, noDeps="
          << noDeps);
      return true;
    }
    ARTS_DEBUG(
        "  Loop is NOT parallel-friendly: potentiallyParallel="
        << loopNode->potentiallyParallel << ", hasInterIterationDeps="
        << (loopNode->hasInterIterationDeps.has_value()
                ? (loopNode->hasInterIterationDeps.value() ? "true" : "false")
                : "unknown"));
  }

  return false;
}

bool EdtNode::hasNestedEdts() const {
  if (!edtOp)
    return false;
  EdtOp op = edtOp;
  bool found = false;
  op.getRegion().walk([&](EdtOp) {
    found = true;
    return WalkResult::interrupt();
  });
  return found;
}

bool EdtNode::hasNestedTaskEdts() const {
  if (!edtOp)
    return false;
  EdtOp op = edtOp;
  bool found = false;
  op.walk([&](EdtOp nestedEdt) {
    if (nestedEdt == op)
      return WalkResult::advance();
    if (nestedEdt.getType() == EdtType::task) {
      found = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return found;
}
