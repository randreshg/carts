///==========================================================================///
/// File: CuMuGraphPartitioning.cpp
///
/// SDE-owned CU/MU graph partitioning helpers.
///==========================================================================///

#include "carts/dialect/sde/Utils/CuMuGraphPartitioning.h"
#include "carts/dialect/sde/Utils/IterationSizingUtils.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace mlir::carts::sde {

static int64_t saturatingMultiplyPositive(int64_t lhs, int64_t rhs) {
  if (lhs <= 0 || rhs <= 0)
    return 0;
  if (lhs > std::numeric_limits<int64_t>::max() / rhs)
    return std::numeric_limits<int64_t>::max();
  return lhs * rhs;
}

int64_t inferCuCountFromMuPartition(ArrayRef<int64_t> shape,
                                    ArrayRef<int64_t> ownerPhysicalDims,
                                    ArrayRef<int64_t> physicalBlockShape) {
  if (shape.empty() || ownerPhysicalDims.empty() ||
      shape.size() != physicalBlockShape.size())
    return 0;

  int64_t computeUnits = 1;
  for (int64_t rawDim : ownerPhysicalDims) {
    if (rawDim < 0 || static_cast<size_t>(rawDim) >= shape.size())
      return 0;
    int64_t extent = shape[rawDim];
    int64_t block = physicalBlockShape[rawDim];
    if (extent <= 0 || block <= 0)
      return 0;
    computeUnits =
        saturatingMultiplyPositive(computeUnits, ceilDivPositive(extent, block));
  }
  return computeUnits;
}

static double scoreCuMuPartition(const CuMuMemoryUnit &memory,
                                 const CuMuComputeUnitTarget &compute,
                                 const CuMuPartitionObjective &objective,
                                 const CuMuPartitionPlan &plan) {
  double taskCost = std::max(0.0, compute.taskCreationCost);
  double syncCost = std::max(0.0, compute.taskSyncCost);
  double dataCost = std::max(1.0, compute.dataAccessCost);
  double logicalWorkers =
      static_cast<double>(std::max<int64_t>(1, compute.logicalWorkerCapacity));
  double exposed = static_cast<double>(std::max<int64_t>(
      1, std::min(plan.computeUnits, compute.logicalWorkerCapacity)));

  double score = static_cast<double>(plan.computeUnits) * taskCost;

  // Concurrency is the first-order objective. A plan that cannot expose enough
  // independent CUs to fill the platform is dominated unless no candidate can.
  double missingParallelism = std::max(0.0, logicalWorkers - exposed);
  score += missingParallelism * (taskCost + syncCost + dataCost) * 64.0;

  // Communication is abstract at SDE. Use layout-disagreement volume only as a
  // pressure signal: many tiny MUs inflate remote acquire/copy/control traffic,
  // while larger MUs amortize it. This names no collective or runtime object.
  bool hasCommunication = memory.abstractCommVolumeBytes > 0;
  if (hasCommunication) {
    score += static_cast<double>(plan.computeUnits) * syncCost;

    int64_t tileBytes = std::max<int64_t>(1, plan.tilePayloadBytes);
    double packets =
        static_cast<double>(memory.abstractCommVolumeBytes) /
        static_cast<double>(tileBytes);
    score += packets * dataCost;
  }

  if (objective.targetTileBytes > 0 &&
      plan.tilePayloadBytes < objective.targetTileBytes) {
    double shortfall =
        static_cast<double>(objective.targetTileBytes - plan.tilePayloadBytes) /
        static_cast<double>(objective.targetTileBytes);
    double pressure = hasCommunication ? 16.0 : 4.0;
    score += shortfall * pressure *
             static_cast<double>(plan.computeUnits) *
             (taskCost + syncCost + dataCost);
  }

  return score;
}

std::optional<CuMuPartitionPlan> chooseCuMuGraphPartition(
    const CuMuMemoryUnit &memory, const CuMuComputeUnitTarget &compute,
    const CuMuPartitionObjective &objective,
    ArrayRef<int64_t> initialPhysicalBlockShape,
    llvm::function_ref<bool(int64_t, SmallVectorImpl<int64_t> &)> rebuild) {
  int64_t requested =
      std::max<int64_t>(1, compute.requestedComputeUnits);
  int64_t floor =
      std::clamp<int64_t>(compute.minComputeUnits, int64_t{1}, requested);

  if (memory.shape.empty() || memory.ownerPhysicalDims.empty() ||
      memory.elementBytes <= 0 || initialPhysicalBlockShape.empty())
    return std::nullopt;

  std::optional<CuMuPartitionPlan> best;
  int64_t candidateUnits = requested;
  SmallVector<int64_t, 4> candidateShape(initialPhysicalBlockShape.begin(),
                                         initialPhysicalBlockShape.end());

  while (true) {
    int64_t actualUnits = inferCuCountFromMuPartition(
        memory.shape, memory.ownerPhysicalDims, candidateShape);
    if (actualUnits > 0) {
      CuMuPartitionPlan plan;
      plan.computeUnits = actualUnits;
      plan.exposedParallelism =
          std::min<int64_t>(actualUnits,
                            std::max<int64_t>(1, compute.logicalWorkerCapacity));
      plan.tilePayloadBytes =
          tilePayloadBytes(candidateShape, memory.elementBytes);
      plan.physicalBlockShape.assign(candidateShape.begin(),
                                     candidateShape.end());
      plan.score = scoreCuMuPartition(memory, compute, objective, plan);
      if (!best || plan.score < best->score)
        best = std::move(plan);
    }

    if (candidateUnits <= floor)
      break;

    int64_t nextUnits = std::max<int64_t>(floor, candidateUnits / 2);
    if (nextUnits == candidateUnits)
      break;

    SmallVector<int64_t, 4> rebuiltShape;
    if (!rebuild(nextUnits, rebuiltShape))
      break;
    candidateUnits = nextUnits;
    candidateShape = std::move(rebuiltShape);
  }

  return best;
}

} // namespace mlir::carts::sde
