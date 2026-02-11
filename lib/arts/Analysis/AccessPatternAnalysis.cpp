///==========================================================================///
/// File: AccessPatternAnalysis.cpp
///
/// Shared loop-relative memory access bounds analysis.
///==========================================================================///

#include "arts/Analysis/AccessPatternAnalysis.h"
#include "arts/Utils/ValueUtils.h"
#include <algorithm>
#include <limits>

using namespace mlir;
using namespace mlir::arts;

void arts::normalizeAccessBounds(AccessBoundsResult &bounds) {
  if (!bounds.valid || !bounds.isStencil)
    return;

  /// Already centered around zero.
  if (bounds.minOffset < 0 && bounds.maxOffset > 0)
    return;

  int64_t sum = bounds.minOffset + bounds.maxOffset;
  if (sum % 2 != 0)
    return;

  int64_t center = sum / 2;
  if (center == 0)
    return;

  bounds.centerOffset = center;
  bounds.minOffset -= center;
  bounds.maxOffset -= center;
}

AccessBoundsResult
arts::analyzeAccessBoundsFromIndices(ArrayRef<AccessIndexInfo> accesses,
                                     Value loopIV, Value blockBase,
                                     std::optional<unsigned> partitionDim) {
  AccessBoundsResult bounds;
  bounds.minOffset = std::numeric_limits<int64_t>::max();
  bounds.maxOffset = std::numeric_limits<int64_t>::min();

  if (!loopIV || !blockBase)
    return bounds;

  if (accesses.empty()) {
    bounds.minOffset = 0;
    bounds.maxOffset = 0;
    bounds.valid = true;
    return bounds;
  }

  bool foundAny = false;
  for (const AccessIndexInfo &access : accesses) {
    if (access.indexChain.empty())
      continue;

    Value idxForBounds;
    if (partitionDim) {
      unsigned chainIdx = access.dbRefPrefix + *partitionDim;
      if (chainIdx < access.indexChain.size())
        idxForBounds = access.indexChain[chainIdx];
    }

    if (!idxForBounds) {
      for (Value idx : access.indexChain) {
        int64_t constVal = 0;
        if (!ValueUtils::getConstantIndex(idx, constVal)) {
          idxForBounds = idx;
          break;
        }
      }
    }

    if (!idxForBounds)
      continue;

    auto constOffset =
        ValueUtils::extractConstantOffset(idxForBounds, loopIV, blockBase);
    if (constOffset) {
      foundAny = true;
      bounds.minOffset = std::min(bounds.minOffset, *constOffset);
      bounds.maxOffset = std::max(bounds.maxOffset, *constOffset);
      continue;
    }

    if (ValueUtils::dependsOn(idxForBounds, loopIV) &&
        ValueUtils::dependsOn(idxForBounds, blockBase))
      bounds.hasVariableOffset = true;
    return bounds;
  }

  if (foundAny) {
    bounds.isStencil = (bounds.minOffset != bounds.maxOffset);
    bounds.valid = true;
    normalizeAccessBounds(bounds);
  }

  return bounds;
}
