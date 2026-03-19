///==========================================================================///
/// File: LoopAnalysis.cpp
/// Implementation of LoopAnalysis with LoopNode integration
///==========================================================================///

#include "arts/analysis/loop/LoopAnalysis.h"
#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/graphs/db/DbNode.h"
#include "arts/analysis/metadata/MetadataManager.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"
#include <algorithm>

ARTS_DEBUG_SETUP(loop_analysis);

using namespace mlir;
using namespace mlir::arts;

namespace {

static std::optional<int64_t> getTripCountFromMetadata(Operation *loopOp,
                                                       LoopNode *loopNode) {
  if (loopNode && loopNode->tripCount && *loopNode->tripCount > 0)
    return *loopNode->tripCount;

  if (auto artsFor = dyn_cast<arts::ForOp>(loopOp)) {
    if (auto loopAttr = artsFor->getAttrOfType<LoopMetadataAttr>(
            AttrNames::LoopMetadata::Name)) {
      if (auto tripAttr = loopAttr.getTripCount()) {
        int64_t tc = tripAttr.getInt();
        if (tc > 0)
          return tc;
      }
    }
  }

  return std::nullopt;
}

static std::optional<int64_t>
getTripCountFromConstantBounds(Operation *loopOp) {
  if (auto affineFor = dyn_cast<affine::AffineForOp>(loopOp)) {
    if (affineFor.hasConstantBounds()) {
      int64_t lb = affineFor.getConstantLowerBound();
      int64_t ub = affineFor.getConstantUpperBound();
      int64_t step = affineFor.getStepAsInt();
      if (step > 0) {
        int64_t span = ub - lb;
        if (span <= 0)
          return 0;
        return (span + step - 1) / step;
      }
    }
    return std::nullopt;
  }

  if (auto artsFor = dyn_cast<arts::ForOp>(loopOp)) {
    if (artsFor.getLowerBound().empty() || artsFor.getUpperBound().empty() ||
        artsFor.getStep().empty())
      return std::nullopt;

    int64_t lb = 0, ub = 0, step = 0;
    if (ValueAnalysis::getConstantIndex(artsFor.getLowerBound()[0], lb) &&
        ValueAnalysis::getConstantIndex(artsFor.getUpperBound()[0], ub) &&
        ValueAnalysis::getConstantIndex(artsFor.getStep()[0], step) &&
        step > 0) {
      int64_t span = ub - lb;
      if (span <= 0)
        return 0;
      return (span + step - 1) / step;
    }
  }

  if (auto forOp = dyn_cast<scf::ForOp>(loopOp)) {
    int64_t lb = 0, ub = 0, step = 0;
    if (ValueAnalysis::getConstantIndex(forOp.getLowerBound(), lb) &&
        ValueAnalysis::getConstantIndex(forOp.getUpperBound(), ub) &&
        ValueAnalysis::getConstantIndex(forOp.getStep(), step) && step > 0) {
      int64_t span = ub - lb;
      if (span <= 0)
        return 0;
      return (span + step - 1) / step;
    }
  }

  return std::nullopt;
}

} // namespace

LoopAnalysis::LoopAnalysis(AnalysisManager &analysisManager)
    : ArtsAnalysis(analysisManager), module(analysisManager.getModule()) {}

void LoopAnalysis::invalidate() {
  loopNodes.clear();
  built = false;
}

void LoopAnalysis::ensureAnalyzed() {
  if (!built)
    run();
}

void LoopAnalysis::run() {
  ARTS_DEBUG_HEADER(LoopAnalysis);

  built = true;
  loopNodes.clear();
  module.walk([&](Operation *op) {
    if (!isLoopOperation(op))
      return;
    if (auto whileOp = dyn_cast<scf::WhileOp>(op))
      ARTS_DEBUG("Discovered scf.while at " << whileOp.getLoc());
    loopNodes[op] = std::make_unique<LoopNode>(op, this);
  });

  if (loopNodes.empty())
    ARTS_WARN("No loops found in module");
  else
    ARTS_INFO("Discovered " << loopNodes.size() << " loops");

  ARTS_DEBUG_FOOTER(LoopAnalysis);
}

///===----------------------------------------------------------------------===///
/// LoopNode Access (NEW - Metadata-integrated API)
///===----------------------------------------------------------------------===///

bool LoopAnalysis::isLoopOperation(Operation *op) const {
  return isa<scf::ForOp, affine::AffineForOp, scf::ParallelOp, scf::WhileOp,
             arts::ForOp>(op);
}

LoopNode *LoopAnalysis::getOrCreateLoopNode(Operation *loopOp) {
  ensureAnalyzed();
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

LoopNode *LoopAnalysis::getLoopNode(Operation *loopOp) {
  ensureAnalyzed();
  auto it = loopNodes.find(loopOp);
  return it != loopNodes.end() ? it->second.get() : nullptr;
}

void LoopAnalysis::collectEnclosingLoops(
    Operation *op, SmallVectorImpl<LoopNode *> &enclosingLoops) {
  ensureAnalyzed();
  for (Operation *p = op->getParentOp(); p; p = p->getParentOp()) {
    if (isLoopOperation(p)) {
      if (LoopNode *node = getLoopNode(p)) {
        enclosingLoops.push_back(node);
      }
    }
  }
  /// Reverse to get outermost-to-innermost order
  std::reverse(enclosingLoops.begin(), enclosingLoops.end());
}

void LoopAnalysis::collectLoopsInOperation(Operation *op,
                                           SmallVectorImpl<LoopNode *> &loops) {
  ensureAnalyzed();
  op->walk([&](Operation *childOp) {
    if (isLoopOperation(childOp)) {
      if (LoopNode *node = getOrCreateLoopNode(childOp))
        loops.push_back(node);
    }
  });
}

void LoopAnalysis::collectForLoopsInOperation(
    Operation *op, SmallVectorImpl<LoopNode *> &loops) {
  ensureAnalyzed();
  op->walk([&](Operation *childOp) {
    if (isa<scf::ForOp>(childOp)) {
      if (LoopNode *node = getOrCreateLoopNode(childOp))
        loops.push_back(node);
    }
  });
}

std::optional<int64_t> LoopAnalysis::getStaticTripCount(Operation *loopOp) {
  ensureAnalyzed();
  if (!loopOp || !isLoopOperation(loopOp))
    return std::nullopt;

  LoopNode *loopNode = getLoopNode(loopOp);
  if (auto metadataTripCount = getTripCountFromMetadata(loopOp, loopNode))
    return metadataTripCount;

  return getTripCountFromConstantBounds(loopOp);
}

std::optional<int64_t>
LoopAnalysis::estimateStaticPerfectNestedWork(Operation *loopOp, int64_t cap) {
  ensureAnalyzed();
  if (!loopOp || !isLoopOperation(loopOp))
    return std::nullopt;
  if (LoopNode *loopNode = getOrCreateLoopNode(loopOp))
    return loopNode->estimateStaticPerfectNestedWork(cap);
  return std::nullopt;
}

std::optional<DbAnalysis::LoopDbAccessSummary>
LoopAnalysis::getLoopDbAccessSummary(Operation *loopOp) {
  ensureAnalyzed();
  auto forOp = dyn_cast_or_null<arts::ForOp>(loopOp);
  if (!forOp)
    return std::nullopt;
  return getAnalysisManager().getDbAnalysis().getLoopDbAccessSummary(forOp);
}

bool LoopAnalysis::operationHasDistributedDbContract(Operation *op) {
  ensureAnalyzed();
  return getAnalysisManager().getDbAnalysis().operationHasDistributedDbContract(
      op);
}

bool LoopAnalysis::operationHasPeerInferredPartitionDims(Operation *op) {
  ensureAnalyzed();
  return getAnalysisManager()
      .getDbAnalysis()
      .operationHasPeerInferredPartitionDims(op);
}

template <typename LoopOpType>
void LoopAnalysis::collectLoopsInOperation(Operation *op,
                                           SmallVectorImpl<LoopNode *> &loops) {
  ensureAnalyzed();
  op->walk([&](Operation *childOp) {
    if (isa<LoopOpType>(childOp)) {
      if (LoopNode *node = getOrCreateLoopNode(childOp))
        loops.push_back(node);
    }
  });
}

template void LoopAnalysis::collectLoopsInOperation<scf::ForOp>(
    Operation *op, SmallVectorImpl<LoopNode *> &loops);
template void LoopAnalysis::collectLoopsInOperation<scf::WhileOp>(
    Operation *op, SmallVectorImpl<LoopNode *> &loops);
template void LoopAnalysis::collectLoopsInOperation<arts::ForOp>(
    Operation *op, SmallVectorImpl<LoopNode *> &loops);

LoopNode *LoopAnalysis::findEnclosingLoopDrivenBy(Operation *op, Value idx) {
  ensureAnalyzed();
  for (Operation *parent = op->getParentOp(); parent;
       parent = parent->getParentOp()) {
    if (!isLoopOperation(parent))
      continue;
    if (LoopNode *node = getLoopNode(parent)) {
      if (node->dependsOnInductionVar(idx))
        return node;
    }
  }
  return nullptr;
}
