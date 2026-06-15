///==========================================================================///
/// File: NeighborhoodAnalysis.cpp
///
/// Stencil neighborhood-offset extraction for SDE scheduling-unit loops:
/// derives per-loop min/max access offsets from the read access entries.
/// Backs the public extractNeighborhoodAccessInfo(summary) wrapper.
///==========================================================================///

#include "SuLoopAccessAnalysisDetail.h"

#include "carts/dialect/sde/Analysis/AffineIndexUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"

using namespace mlir;
using namespace mlir::carts;

namespace mlir::carts::sde::detail {

std::optional<SuNeighborhoodAccessInfo>
extractNeighborhoodAccessInfo(ArrayRef<MemrefAccessEntry> reads,
                              unsigned numLoops) {
  if (numLoops == 0)
    return std::nullopt;

  SuNeighborhoodAccessInfo info;
  info.minOffsets.assign(numLoops, 0);
  info.maxOffsets.assign(numLoops, 0);
  info.writeFootprint.assign(numLoops, 1);
  for (unsigned dim = 0; dim < numLoops; ++dim) {
    info.ownerDims.push_back(dim);
    info.spatialDims.push_back(dim);
  }

  bool sawNeighborhoodOffset = false;
  for (const MemrefAccessEntry &entry : reads) {
    for (AffineExpr result : entry.indexingMap.getResults()) {
      auto dimOffset = extractDimOffset(result);
      if (!dimOffset || !dimOffset->dim)
        continue;

      unsigned dim = *dimOffset->dim;
      if (dim >= numLoops)
        continue;

      info.minOffsets[dim] = std::min(info.minOffsets[dim], dimOffset->offset);
      info.maxOffsets[dim] = std::max(info.maxOffsets[dim], dimOffset->offset);
      sawNeighborhoodOffset |= dimOffset->offset != 0;
    }
  }

  if (!sawNeighborhoodOffset)
    return std::nullopt;
  return info;
}

} // namespace mlir::carts::sde::detail
