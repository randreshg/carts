///==========================================================================///
/// File: LoopAnalysis.cpp
/// Implementation of LoopAnalysis with LoopNode integration
///==========================================================================///

#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/Utils/ArtsDebug.h"

ARTS_DEBUG_SETUP(loop_analysis);

using namespace mlir;
using namespace mlir::arts;

LoopAnalysis::LoopAnalysis(Operation *module,
                           ArtsAnalysisManager *analysisManager)
    : module(cast<ModuleOp>(module)), analysisManager(analysisManager) {
  run();
}

LoopAnalysis::~LoopAnalysis() {
  // Clean up legacy LoopInfo objects
  for (auto &loop : loopInfoMap) {
    delete loop.second;
  }
}

void LoopAnalysis::run() {
  ARTS_DEBUG_HEADER(LoopAnalysis);

  /// Walk module and create LoopNodes + legacy LoopInfo for all loops
  module.walk([&](Operation *op) {
    if (auto fop = dyn_cast<scf::ForOp>(op)) {
      loops.push_back(fop);
      // Create LoopNode (auto-imports metadata via LoopAnalysis)
      loopNodes[fop] = std::make_unique<LoopNode>(fop, this);
      // Legacy LoopInfo
      loopInfoMap[fop] = new LoopInfo(false, {}, {}, fop, fop.getInductionVar());
    } else if (auto aop = dyn_cast<affine::AffineForOp>(op)) {
      loops.push_back(aop);
      // Create LoopNode (auto-imports metadata via LoopAnalysis)
      loopNodes[aop] = std::make_unique<LoopNode>(aop, this);
      // Legacy LoopInfo
      loopInfoMap[aop] = new LoopInfo(true, aop, {}, {}, aop.getInductionVar());
    } else if (auto pop = dyn_cast<scf::ParallelOp>(op)) {
      loops.push_back(pop);
      // Create LoopNode (auto-imports metadata via LoopAnalysis)
      loopNodes[pop] = std::make_unique<LoopNode>(pop, this);
      // Legacy LoopInfo (parallel loops have multiple IVs, use first)
      Value firstIV = pop.getInductionVars().empty() ? Value() : pop.getInductionVars()[0];
      loopInfoMap[pop] = new LoopInfo(false, {}, pop, {}, firstIV);
    }
  });

  ARTS_DEBUG_REGION({
    if (loops.empty()) {
      ARTS_WARN("No loops found in module");
    } else {
      ARTS_INFO("Discovered " << loops.size() << " loops ("
                              << loopNodes.size() << " LoopNodes created)");
    }
  });

  /// Analyze induction variables
  analyzeLoopIV();
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

  // Create new LoopNode (pass this for LoopAnalysis)
  auto newNode = std::make_unique<LoopNode>(loopOp, this);
  LoopNode *ptr = newNode.get();
  loopNodes[loopOp] = std::move(newNode);

  // Also add to legacy structures
  loops.push_back(loopOp);
  if (auto fop = dyn_cast<scf::ForOp>(loopOp)) {
    loopInfoMap[loopOp] = new LoopInfo(false, {}, {}, fop, fop.getInductionVar());
  } else if (auto aop = dyn_cast<affine::AffineForOp>(loopOp)) {
    loopInfoMap[loopOp] = new LoopInfo(true, aop, {}, {}, aop.getInductionVar());
  } else if (auto pop = dyn_cast<scf::ParallelOp>(loopOp)) {
    Value firstIV = pop.getInductionVars().empty() ? Value() : pop.getInductionVars()[0];
    loopInfoMap[loopOp] = new LoopInfo(false, {}, pop, {}, firstIV);
  }

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

void LoopAnalysis::collectAffectingLoops(
    Operation *op, SmallVectorImpl<LoopNode *> &affectingLoops) {
  DenseSet<Operation *> uniq;
  for (Value operand : op->getOperands()) {
    SmallVector<Operation *, 4> loopsForVal;
    if (isDependentOnLoop(operand, loopsForVal)) {
      for (Operation *l : loopsForVal) {
        if (uniq.insert(l).second) {
          if (LoopNode *node = getLoopNode(l)) {
            affectingLoops.push_back(node);
          }
        }
      }
    }
  }
}

///===----------------------------------------------------------------------===///
// Legacy API (for backward compatibility)
///===----------------------------------------------------------------------===///

LoopInfo *LoopAnalysis::getLoopInfo(Operation *op) {
  auto it = loopInfoMap.find(op);
  return it != loopInfoMap.end() ? it->second : nullptr;
}

bool LoopAnalysis::isDependentOnLoop(Value val,
                                     SmallVectorImpl<Operation *> &loops) {
  auto it = loopValsMap.find(val);
  if (it != loopValsMap.end()) {
    loops.assign(it->second.begin(), it->second.end());
    return true;
  }
  return false;
}

void LoopAnalysis::collectAffectingLoops(
    Value val, SmallVectorImpl<Operation *> &affectingLoops) {
  SmallVector<Operation *, 4> loopsForVal;
  if (isDependentOnLoop(val, loopsForVal))
    affectingLoops.append(loopsForVal.begin(), loopsForVal.end());
}

///===----------------------------------------------------------------------===///
// Private helpers
///===----------------------------------------------------------------------===///

void LoopAnalysis::analyzeLoopIV() {
  auto loopBfs = [&](Operation *loopOp, Value iv) {
    if (!iv)
      return;  // Skip if no induction variable

    DenseSet<Value> visited;
    SmallVector<Value, 8> queue;
    visited.insert(iv);
    queue.push_back(iv);

    while (!queue.empty()) {
      Value cur = queue.pop_back_val();
      for (auto &use : cur.getUses()) {
        Operation *owner = use.getOwner();
        for (Value res : owner->getResults()) {
          if (visited.insert(res).second)
            queue.push_back(res);
        }
      }
    }

    for (auto &val : visited)
      loopValsMap[val].push_back(loopOp);
  };

  // Analyze induction variables for all loops
  for (auto &loopPair : loopInfoMap) {
    loopBfs(loopPair.first, loopPair.second->inductionVar);
  }
}
