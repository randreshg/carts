///==========================================================================///
/// File: IterationSizingUtils.cpp
///
/// SDE-owned iteration sizing helpers for source-level scheduling intent.
///==========================================================================///

#include "carts/dialect/sde/Utils/IterationSizingUtils.h"

#include "carts/utils/LoopUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/MathExtras.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace mlir;
using namespace mlir::carts;

namespace mlir::carts::sde {

static int64_t saturatingMultiplyPositive(int64_t lhs, int64_t rhs) {
  if (lhs <= 0 || rhs <= 0)
    return 0;
  if (lhs > std::numeric_limits<int64_t>::max() / rhs)
    return std::numeric_limits<int64_t>::max();
  return lhs * rhs;
}

int64_t ceilDivPositive(int64_t value, int64_t divisor) {
  return llvm::divideCeil(std::max<int64_t>(1, value),
                          std::max<int64_t>(1, divisor));
}

SmallVector<int64_t, 4> factorWorkersAcrossDims(int64_t workers,
                                                ArrayRef<int64_t> extents) {
  SmallVector<int64_t, 4> grid(extents.size(), 1);
  if (workers <= 1 || extents.empty())
    return grid;

  SmallVector<int64_t, 8> factors;
  int64_t remaining = workers;
  for (int64_t factor = 2; factor * factor <= remaining; ++factor) {
    while (remaining % factor == 0) {
      factors.push_back(factor);
      remaining /= factor;
    }
  }
  if (remaining > 1)
    factors.push_back(remaining);
  llvm::sort(factors, std::greater<int64_t>());

  for (int64_t factor : factors) {
    unsigned bestDim = 0;
    int64_t bestSpan = -1;
    for (auto [idx, extent] : llvm::enumerate(extents)) {
      int64_t span = ceilDivPositive(extent, grid[idx]);
      if (span > bestSpan) {
        bestSpan = span;
        bestDim = static_cast<unsigned>(idx);
      }
    }
    grid[bestDim] *= factor;
  }

  return grid;
}

long double estimateStencilExpandedTileRatio(ArrayRef<int64_t> extents,
                                             ArrayRef<int64_t> haloRadii,
                                             ArrayRef<int64_t> grid) {
  long double ownedVolume = 1.0L;
  long double expandedVolume = 1.0L;
  for (auto [idx, extent] : llvm::enumerate(extents)) {
    int64_t tile = ceilDivPositive(extent, grid[idx]);
    int64_t halo =
        idx < haloRadii.size() ? std::max<int64_t>(0, haloRadii[idx]) : 0;
    ownedVolume *= static_cast<long double>(tile);
    expandedVolume *= static_cast<long double>(tile + 2 * halo);
  }
  if (ownedVolume <= 0.0L)
    return std::numeric_limits<long double>::infinity();
  return expandedVolume / ownedVolume;
}

SmallVector<int64_t, 4>
factorStencilWorkersAcrossDims(int64_t workers, ArrayRef<int64_t> extents,
                               ArrayRef<int64_t> haloRadii) {
  SmallVector<int64_t, 4> grid(extents.size(), 1);
  if (workers <= 1 || extents.empty())
    return grid;
  if (haloRadii.empty() ||
      llvm::all_of(haloRadii, [](int64_t halo) { return halo <= 0; }))
    return factorWorkersAcrossDims(workers, extents);

  SmallVector<int64_t, 8> factors;
  int64_t remaining = workers;
  for (int64_t factor = 2; factor * factor <= remaining; ++factor) {
    while (remaining % factor == 0) {
      factors.push_back(factor);
      remaining /= factor;
    }
  }
  if (remaining > 1)
    factors.push_back(remaining);
  llvm::sort(factors, std::greater<int64_t>());

  constexpr long double epsilon = 1.0e-12L;
  for (int64_t factor : factors) {
    unsigned bestDim = 0;
    long double bestRatio = std::numeric_limits<long double>::infinity();
    int64_t bestSpan = -1;

    for (auto [idx, extent] : llvm::enumerate(extents)) {
      SmallVector<int64_t, 4> candidate(grid.begin(), grid.end());
      candidate[idx] *= factor;
      long double ratio =
          estimateStencilExpandedTileRatio(extents, haloRadii, candidate);
      int64_t span = ceilDivPositive(extent, candidate[idx]);
      if (ratio + epsilon < bestRatio ||
          (std::abs(ratio - bestRatio) <= epsilon && span > bestSpan)) {
        bestRatio = ratio;
        bestSpan = span;
        bestDim = static_cast<unsigned>(idx);
      }
    }

    grid[bestDim] *= factor;
  }

  return grid;
}

bool enforceOwnerBlockConcurrencyFloor(
    ArrayRef<int64_t> shape, ArrayRef<int64_t> ownerPhysicalDims,
    int64_t targetComputeUnits, SmallVectorImpl<int64_t> &physicalBlockShape) {
  if (shape.empty() || ownerPhysicalDims.empty() ||
      shape.size() != physicalBlockShape.size())
    return false;

  SmallVector<int64_t, 4> ownerExtents;
  ownerExtents.reserve(ownerPhysicalDims.size());
  for (int64_t physicalDim : ownerPhysicalDims) {
    if (physicalDim < 0 || static_cast<size_t>(physicalDim) >= shape.size())
      return false;
    if (shape[physicalDim] <= 0 || physicalBlockShape[physicalDim] <= 0)
      return false;
    ownerExtents.push_back(shape[physicalDim]);
  }

  SmallVector<int64_t, 4> workerGrid = factorWorkersAcrossDims(
      std::max<int64_t>(1, targetComputeUnits), ownerExtents);
  if (workerGrid.size() != ownerPhysicalDims.size())
    return false;

  for (auto [idx, physicalDim] : llvm::enumerate(ownerPhysicalDims)) {
    int64_t balancedBlock =
        ceilDivPositive(shape[physicalDim], workerGrid[idx]);
    physicalBlockShape[physicalDim] =
        std::min<int64_t>(physicalBlockShape[physicalDim], balancedBlock);
  }
  return true;
}

int64_t haloExpandedTileBytes(ArrayRef<int64_t> extents,
                              ArrayRef<int64_t> haloRadii,
                              ArrayRef<int64_t> physicalBlockShape,
                              int64_t elemBytes) {
  int64_t ownedBytes = tilePayloadBytes(physicalBlockShape, elemBytes);
  if (ownedBytes <= 0)
    return 0;
  SmallVector<int64_t, 4> grid;
  grid.reserve(extents.size());
  for (auto [idx, extent] : llvm::enumerate(extents)) {
    if (extent <= 0 || idx >= physicalBlockShape.size() ||
        physicalBlockShape[idx] <= 0)
      return 0;
    grid.push_back(ceilDivPositive(extent, physicalBlockShape[idx]));
  }
  long double ratio =
      estimateStencilExpandedTileRatio(extents, haloRadii, grid);
  if (!std::isfinite(static_cast<double>(ratio)) || ratio <= 0.0L)
    return ownedBytes;
  long double expanded = static_cast<long double>(ownedBytes) * ratio;
  if (expanded > static_cast<long double>(std::numeric_limits<int64_t>::max()))
    return std::numeric_limits<int64_t>::max();
  return static_cast<int64_t>(expanded);
}

int64_t tilePayloadBytes(ArrayRef<int64_t> physicalBlockShape,
                         int64_t elemBytes) {
  if (elemBytes <= 0)
    return 0;
  int64_t bytes = elemBytes;
  for (int64_t dim : physicalBlockShape) {
    if (dim <= 0)
      return 0;
    bytes = saturatingMultiplyPositive(bytes, dim);
  }
  return bytes;
}

int64_t coarsenWorkersToTileByteFloor(
    int64_t workers, ArrayRef<int64_t> physicalBlockShape, int64_t elemBytes,
    int64_t minTileBytes, int64_t minWorkers,
    llvm::function_ref<bool(int64_t, SmallVectorImpl<int64_t> &)> rebuild) {
  if (minTileBytes <= 0 || workers <= 1 || elemBytes <= 0)
    return workers;

  int64_t currentWorkers = std::max<int64_t>(1, workers);
  int64_t workerFloor =
      std::clamp<int64_t>(minWorkers, int64_t{1}, currentWorkers);
  SmallVector<int64_t, 4> currentShape(physicalBlockShape.begin(),
                                       physicalBlockShape.end());
  while (currentWorkers > workerFloor &&
         tilePayloadBytes(currentShape, elemBytes) < minTileBytes) {
    int64_t nextWorkers = std::max<int64_t>(workerFloor, currentWorkers / 2);
    if (nextWorkers == currentWorkers)
      break;
    SmallVector<int64_t, 4> candidateShape;
    if (!rebuild(nextWorkers, candidateShape))
      break;
    currentWorkers = nextWorkers;
    currentShape = std::move(candidateShape);
  }
  return currentWorkers;
}

int64_t coarsenStencilWorkersToFloor(
    int64_t workers, ArrayRef<int64_t> extents, ArrayRef<int64_t> haloRadii,
    ArrayRef<int64_t> physicalBlockShape, int64_t elemBytes,
    int64_t minTileBytes,
    llvm::function_ref<bool(int64_t, SmallVectorImpl<int64_t> &)> rebuild) {
  if (minTileBytes <= 0 || workers <= 1 || elemBytes <= 0)
    return workers;
  SmallVector<int64_t, 4> currentShape(physicalBlockShape.begin(),
                                       physicalBlockShape.end());
  int64_t currentWorkers = workers;
  while (currentWorkers > 1 &&
         haloExpandedTileBytes(extents, haloRadii, currentShape, elemBytes) <
             minTileBytes) {
    int64_t nextWorkers = std::max<int64_t>(1, currentWorkers / 2);
    if (nextWorkers == currentWorkers)
      break;
    SmallVector<int64_t, 4> candidateShape;
    if (!rebuild(nextWorkers, candidateShape))
      break;
    currentWorkers = nextWorkers;
    currentShape = std::move(candidateShape);
  }
  return currentWorkers;
}

Value buildLogicalWorkerCapacityValue(OpBuilder &builder, Location loc) {
  Value logicalWorkers = SdeResourceQueryOp::create(
                             builder, loc, SdeResourceQueryKind::logicalWorkers)
                             .getResult();
  return arith::MaxUIOp::create(builder, loc, logicalWorkers,
                                createConstantIndex(builder, loc, 1));
}

Value buildTripCountValue(OpBuilder &builder, Location loc, SdeSuIterateOp op) {
  if (std::optional<int64_t> tripCount = getStaticTripCount(op.getOperation()))
    return createConstantIndex(builder, loc, *tripCount);

  if (op.getLowerBounds().size() != 1 || op.getUpperBounds().size() != 1 ||
      op.getSteps().size() != 1)
    return Value();

  Value lowerBound = op.getLowerBounds().front();
  Value upperBound = op.getUpperBounds().front();
  Value step = op.getSteps().front();
  Value zero = createConstantIndex(builder, loc, 0);
  Value one = createConstantIndex(builder, loc, 1);

  Value span = arith::SubIOp::create(builder, loc, upperBound, lowerBound);

  int64_t constantStep = 0;
  Value safeStep = step;
  if (ValueAnalysis::getConstantIndex(step, constantStep)) {
    if (constantStep <= 0)
      return Value();
  } else {
    Value stepIsTooSmall = arith::CmpIOp::create(
        builder, loc, arith::CmpIPredicate::sle, step, zero);
    safeStep = arith::SelectOp::create(builder, loc, stepIsTooSmall, one, step);
  }

  Value spanIsNegative = arith::CmpIOp::create(
      builder, loc, arith::CmpIPredicate::slt, span, zero);
  Value nonNegativeSpan =
      arith::SelectOp::create(builder, loc, spanIsNegative, zero, span);
  return arith::CeilDivSIOp::create(builder, loc, nonNegativeSpan, safeStep);
}

} // namespace mlir::carts::sde
