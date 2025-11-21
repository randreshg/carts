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
    : NodeBase(), edtOp(op), opPtr(op.getOperation()), analysisManager(AM) {}

void EdtNode::print(llvm::raw_ostream &os) const {
  os << "EdtNode (" << getHierId() << ")\n";
}

bool EdtNode::hasParallelLoopMetadata() const {
  if (!edtOp || !analysisManager)
    return false;

  LoopAnalysis &loopAnalysis = analysisManager->getLoopAnalysis();
  ArtsMetadataManager &metadataManager = analysisManager->getMetadataManager();

  bool foundParallelLoop = false;
  edtOp->walk([&](Operation *op) {
    if (auto forOp = dyn_cast<scf::ForOp>(op)) {
      metadataManager.ensureLoopMetadata(forOp);

      if (LoopNode *loopNode = loopAnalysis.getOrCreateLoopNode(forOp)) {
        bool noDeps = !loopNode->hasInterIterationDeps.has_value() ||
                      !loopNode->hasInterIterationDeps.value();
        if (loopNode->potentiallyParallel && noDeps) {
          ARTS_DEBUG(
              "  Loop is parallel-friendly: potentiallyParallel=true, noDeps="
              << noDeps);
          foundParallelLoop = true;
          return WalkResult::interrupt();
        } else {
          ARTS_DEBUG(
              "  Loop is NOT parallel-friendly: potentiallyParallel="
              << loopNode->potentiallyParallel << ", hasInterIterationDeps="
              << (loopNode->hasInterIterationDeps.has_value()
                      ? (loopNode->hasInterIterationDeps.value() ? "true"
                                                                 : "false")
                      : "unknown"));
        }
      }
    }
    return WalkResult::advance();
  });

  return foundParallelLoop;
}
