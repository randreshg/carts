///==========================================================================///
/// File: LoopNode.cpp
///
/// Implementation of LoopNode for loop operations with integrated metadata.
///==========================================================================///

#include "arts/Analysis/Loop/LoopNode.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

using namespace mlir;
using namespace mlir::arts;

LoopNode::LoopNode(Operation *loopOp, LoopAnalysis *loopAnalysis)
    : NodeBase(), LoopMetadata(loopOp), loopOp(loopOp) {
  bool hasMetadata = importFromOp();
  if (!hasMetadata && loopAnalysis) {
    /// Get metadata manager from ArtsAnalysisManager via LoopAnalysis
    auto *analysisManager = loopAnalysis->getAnalysisManager();
    if (analysisManager) {
      auto &metadataManager = analysisManager->getMetadataManager();
      if (metadataManager.ensureLoopMetadata(loopOp))
        importFromOp();
    }
  }
  /// Generate a simple hierarchical ID based on pointer
  hierId = "loop@" + std::to_string(reinterpret_cast<uintptr_t>(loopOp));
}

void LoopNode::print(llvm::raw_ostream &os) const {
  os << "LoopNode(" << hierId << ")";

  if (auto forOp = dyn_cast<affine::AffineForOp>(loopOp)) {
    os << " affine.for";
  } else if (auto forOp = dyn_cast<scf::ForOp>(loopOp)) {
    os << " scf.for";
  } else if (auto parOp = dyn_cast<scf::ParallelOp>(loopOp)) {
    os << " scf.parallel";
  }

  if (tripCount.has_value())
    os << " trip=" << tripCount.value();
  if (nestingLevel.has_value())
    os << " depth=" << nestingLevel.value();
  os << " parallel=" << (potentiallyParallel ? "yes" : "no");

  os << "\n";
}
