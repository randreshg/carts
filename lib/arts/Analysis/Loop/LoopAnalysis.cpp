///==========================================================================///
/// File: LoopAnalysis.cpp
/// Implementation of LoopAnalysis with LoopNode integration
///==========================================================================///

#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/Utils/ArtsDebug.h"
#include <algorithm>

ARTS_DEBUG_SETUP(loop_analysis);

using namespace mlir;
using namespace mlir::arts;

LoopAnalysis::LoopAnalysis(ArtsAnalysisManager &analysisManager)
    : ArtsAnalysis(analysisManager), module(analysisManager.getModule()) {
  run();
}

void LoopAnalysis::invalidate() { run(); }

void LoopAnalysis::run() {
  ARTS_DEBUG_HEADER(LoopAnalysis);

  loopNodes.clear();
  module.walk([&](Operation *op) {
    if (!isLoopOperation(op))
      return;
    loopNodes[op] = std::make_unique<LoopNode>(op, this);
  });

  if (loopNodes.empty())
    ARTS_WARN("No loops found in module");
  else
    ARTS_INFO("Discovered " << loopNodes.size() << " loops");

  ARTS_DEBUG_FOOTER(LoopAnalysis);
}

///===----------------------------------------------------------------------===///
// LoopNode Access (NEW - Metadata-integrated API)
///===----------------------------------------------------------------------===///

bool LoopAnalysis::isLoopOperation(Operation *op) const {
  return isa<scf::ForOp, affine::AffineForOp, scf::ParallelOp>(op);
}

LoopNode *LoopAnalysis::getOrCreateLoopNode(Operation *loopOp) {
  if (!isLoopOperation(loopOp))
    return nullptr;

  auto it = loopNodes.find(loopOp);
  if (it != loopNodes.end())
    return it->second.get();

  auto newNode = std::make_unique<LoopNode>(loopOp, this);
  LoopNode *ptr = newNode.get();
  loopNodes[loopOp] = std::move(newNode);
  return ptr;
}

LoopNode *LoopAnalysis::getLoopNode(Operation *loopOp) const {
  auto it = loopNodes.find(loopOp);
  return it != loopNodes.end() ? it->second.get() : nullptr;
}

void LoopAnalysis::collectEnclosingLoops(
    Operation *op, SmallVectorImpl<LoopNode *> &enclosingLoops) {
  for (Operation *p = op->getParentOp(); p; p = p->getParentOp()) {
    if (isLoopOperation(p)) {
      if (LoopNode *node = getLoopNode(p)) {
        enclosingLoops.push_back(node);
      }
    }
  }
  // Reverse to get outermost-to-innermost order
  std::reverse(enclosingLoops.begin(), enclosingLoops.end());
}
