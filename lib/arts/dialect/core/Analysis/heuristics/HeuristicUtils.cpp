///==========================================================================///
/// File: HeuristicUtils.cpp
///
/// Shared heuristic building blocks for H1 (Partitioning) and H2
/// (Distribution) heuristic families.
///==========================================================================///

#include "arts/dialect/core/Analysis/heuristics/HeuristicUtils.h"
#include "arts/Dialect.h"
#include "arts/dialect/core/Analysis/loop/LoopNode.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include <algorithm>
#include <limits>

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Loop Classification Helpers
///===----------------------------------------------------------------------===///

bool mlir::arts::isSequentialLoop(ForOp forOp, LoopNode *loopNode) {
  /// Check LoopNode fields first (already resolved metadata).
  if (loopNode) {
    if (loopNode->hasInterIterationDeps && *loopNode->hasInterIterationDeps)
      return true;
    if (loopNode->parallelClassification &&
        *loopNode->parallelClassification ==
            LoopMetadata::ParallelClassification::Sequential)
      return true;
  }

  /// Fall back to on-operation LoopMetadataAttr.
  if (auto loopAttr = forOp->getAttrOfType<LoopMetadataAttr>(
          AttrNames::LoopMetadata::Name)) {
    if (auto depsAttr = loopAttr.getHasInterIterationDeps())
      if (depsAttr.getValue())
        return true;

    if (auto classAttr = loopAttr.getParallelClassification()) {
      if (classAttr.getInt() ==
          static_cast<int64_t>(
              LoopMetadata::ParallelClassification::Sequential))
        return true;
    }
  }

  return false;
}

int64_t mlir::arts::ceilDivPositive(int64_t num, int64_t denom) {
  if (num <= 0 || denom <= 0)
    return 1;
  return 1 + (num - 1) / denom;
}

int64_t mlir::arts::clampPositive(int64_t value, int64_t minValue,
                                  int64_t maxValue) {
  return std::clamp(std::max<int64_t>(1, value), std::max<int64_t>(1, minValue),
                    std::max<int64_t>(1, maxValue));
}

int64_t mlir::arts::saturatingMulPositive(int64_t lhs, int64_t rhs) {
  if (lhs <= 0 || rhs <= 0)
    return 1;
  if (lhs >= std::numeric_limits<int64_t>::max() / rhs)
    return std::numeric_limits<int64_t>::max();
  return lhs * rhs;
}

int64_t mlir::arts::floorLog2Positive(int64_t value) {
  if (value <= 1)
    return 0;

  int64_t result = 0;
  while (value > 1) {
    value >>= 1;
    ++result;
  }
  return result;
}
