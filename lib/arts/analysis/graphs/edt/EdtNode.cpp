///==========================================================================///
/// File: EdtNode.cpp
/// Implementation of EDT nodes for graph analysis.
///==========================================================================///

#include "arts/analysis/graphs/edt/EdtNode.h"
#include "arts/analysis/edt/EdtAnalysis.h"
#include "arts/analysis/loop/LoopAnalysis.h"
#include "arts/analysis/loop/LoopNode.h"
#include "arts/analysis/metadata/MetadataManager.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

using namespace mlir;
using namespace mlir::arts;

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(edt_node);

EdtNode::EdtNode(EdtOp op, EdtAnalysis *EA)
    : NodeBase(), edtOp(op), edtAnalysis(EA) {
  assert(edtOp.getOperation() && "Operation must always be available");
  if (EA)
    EA->getMetadataManager().getIdRegistry().getOrCreate(edtOp);
}

void EdtNode::print(llvm::raw_ostream &os) const {
  os << "EdtNode (" << getHierId() << ")\n";
}

bool EdtNode::hasParallelLoopMetadata() const {
  if (!edtOp || !edtAnalysis)
    return false;

  LoopAnalysis &loopAnalysis = edtAnalysis->getLoopAnalysis();
  MetadataManager &metadataManager = edtAnalysis->getMetadataManager();

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
