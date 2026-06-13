///==========================================================================///
/// File: CuMuGraphPartitioning.cpp
///
/// SDE-owned CU/MU graph partitioning helpers.
///==========================================================================///

#include "carts/dialect/sde/Utils/CuMuGraphPartitioning.h"
#include "carts/dialect/sde/Utils/IterationSizingUtils.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallSet.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace mlir::carts::sde {

static int64_t saturatingMultiplyPositive(int64_t lhs, int64_t rhs);
static int64_t saturatingAddPositive(int64_t lhs, int64_t rhs);

SmallVector<unsigned, 8> buildContiguousCuPartAssignment(unsigned vertexCount,
                                                         unsigned partCount) {
  SmallVector<unsigned, 8> assignment;
  if (vertexCount == 0)
    return assignment;
  partCount = std::clamp<unsigned>(partCount, 1, vertexCount);
  assignment.reserve(vertexCount);
  for (unsigned vertex = 0; vertex < vertexCount; ++vertex) {
    unsigned part = static_cast<unsigned>(
        (static_cast<uint64_t>(vertex) * partCount) / vertexCount);
    assignment.push_back(std::min<unsigned>(part, partCount - 1));
  }
  return assignment;
}

int64_t computeCuMuHypergraphCutBytes(const CuMuTypedHypergraph &graph,
                                      ArrayRef<unsigned> vertexToPart) {
  if (graph.vertices.empty() || graph.nets.empty() || vertexToPart.empty())
    return 0;

  int64_t total = 0;
  for (const CuMuGraphNet &net : graph.nets) {
    if (net.pinCuIds.empty())
      continue;
    llvm::SmallSet<unsigned, 8> touchedParts;
    for (unsigned cuId : net.pinCuIds) {
      if (cuId >= vertexToPart.size())
        continue;
      touchedParts.insert(vertexToPart[cuId]);
    }
    if (touchedParts.size() <= 1)
      continue;
    int64_t lambdaMinusOne =
        static_cast<int64_t>(touchedParts.size()) - int64_t{1};
    int64_t weight = std::max<int64_t>(1, net.weightBytes);
    total = saturatingAddPositive(
        total, saturatingMultiplyPositive(weight, lambdaMinusOne));
  }
  return total;
}

static int64_t saturatingMultiplyPositive(int64_t lhs, int64_t rhs) {
  if (lhs <= 0 || rhs <= 0)
    return 0;
  if (lhs > std::numeric_limits<int64_t>::max() / rhs)
    return std::numeric_limits<int64_t>::max();
  return lhs * rhs;
}

static int64_t saturatingAddPositive(int64_t lhs, int64_t rhs) {
  lhs = std::max<int64_t>(0, lhs);
  rhs = std::max<int64_t>(0, rhs);
  if (lhs > std::numeric_limits<int64_t>::max() - rhs)
    return std::numeric_limits<int64_t>::max();
  return lhs + rhs;
}

static bool containsCandidate(ArrayRef<int64_t> candidates, int64_t value) {
  return std::find(candidates.begin(), candidates.end(), value) !=
         candidates.end();
}

static void appendCandidate(SmallVectorImpl<int64_t> &candidates, int64_t value,
                            int64_t floor, int64_t requested) {
  if (value < floor || value > requested ||
      containsCandidate(candidates, value))
    return;
  candidates.push_back(value);
}

static void appendCandidateNeighborhood(SmallVectorImpl<int64_t> &candidates,
                                        int64_t value, int64_t floor,
                                        int64_t requested) {
  appendCandidate(candidates, value - 1, floor, requested);
  appendCandidate(candidates, value, floor, requested);
  appendCandidate(candidates, value + 1, floor, requested);
}

static int64_t maxRemoteFanout(ArrayRef<CuMuHyperedgePressure> hyperedges) {
  int64_t fanout = 0;
  for (const CuMuHyperedgePressure &edge : hyperedges)
    fanout = std::max<int64_t>(fanout, edge.remoteFanout);
  return fanout;
}

static bool rebuildCandidateToFixpoint(
    const CuMuMemoryUnit &memory, int64_t floor, int64_t requested,
    int64_t candidateUnits, ArrayRef<int64_t> initialPhysicalBlockShape,
    llvm::function_ref<bool(int64_t, SmallVectorImpl<int64_t> &)> rebuild,
    SmallVectorImpl<int64_t> &candidateShape) {
  candidateShape.clear();
  if (candidateUnits == requested) {
    candidateShape.assign(initialPhysicalBlockShape.begin(),
                          initialPhysicalBlockShape.end());
  } else if (!rebuild(candidateUnits, candidateShape)) {
    return false;
  }

  int64_t stableUnits = candidateUnits;
  for (unsigned iteration = 0; iteration < 8; ++iteration) {
    int64_t actualUnits = inferCuCountFromMuPartition(
        memory.shape, memory.ownerPhysicalDims, candidateShape);
    if (actualUnits <= 0)
      return false;
    if (actualUnits < floor || actualUnits > requested)
      return false;
    if (actualUnits == stableUnits)
      return true;

    SmallVector<int64_t, 4> rebuiltShape;
    if (!rebuild(actualUnits, rebuiltShape))
      return true;
    stableUnits = actualUnits;
    candidateShape.assign(rebuiltShape.begin(), rebuiltShape.end());
  }

  int64_t actualUnits = inferCuCountFromMuPartition(
      memory.shape, memory.ownerPhysicalDims, candidateShape);
  return actualUnits >= floor && actualUnits <= requested;
}

static SmallVector<int64_t, 16>
enumerateCandidateComputeUnits(int64_t requested, int64_t floor,
                               int64_t logicalWorkers,
                               ArrayRef<CuMuHyperedgePressure> hyperedges) {
  SmallVector<int64_t, 16> candidates;
  appendCandidate(candidates, requested, floor, requested);
  appendCandidate(candidates, floor, floor, requested);
  appendCandidateNeighborhood(candidates, logicalWorkers, floor, requested);

  for (int64_t units = requested; units > floor;) {
    int64_t next = std::max<int64_t>(floor, units / 2);
    appendCandidate(candidates, next, floor, requested);
    if (next == units)
      break;
    units = next;
  }

  // Bounded divisor samples catch common graph partitions that a pure halving
  // sweep misses without making candidate generation proportional to the full
  // iteration extent.
  int64_t divisorLimit = std::min<int64_t>(requested, 4096);
  for (int64_t divisor = 1; divisor <= divisorLimit; ++divisor) {
    if (requested % divisor != 0)
      continue;
    appendCandidate(candidates, divisor, floor, requested);
    appendCandidate(candidates, requested / divisor, floor, requested);
  }

  int64_t fanout = maxRemoteFanout(hyperedges);
  if (fanout > 0) {
    appendCandidateNeighborhood(candidates, fanout + 1, floor, requested);
    appendCandidateNeighborhood(candidates, logicalWorkers + fanout, floor,
                                requested);
    appendCandidateNeighborhood(
        candidates, ceilDivPositive(requested, fanout + 1), floor, requested);
  }

  std::sort(candidates.begin(), candidates.end(), std::greater<int64_t>());
  return candidates;
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
    computeUnits = saturatingMultiplyPositive(computeUnits,
                                              ceilDivPositive(extent, block));
  }
  return computeUnits;
}

struct WeightedCuEstimate {
  int64_t totalWeight = 1;
  int64_t maxPartitionWeight = 1;
  double imbalance = 0.0;
};

static SmallVector<int64_t, 16>
buildOwnerBlockWorkWeights(ArrayRef<int64_t> shape,
                           ArrayRef<int64_t> ownerPhysicalDims,
                           ArrayRef<int64_t> physicalBlockShape) {
  SmallVector<int64_t, 16> weights;
  if (shape.empty() || ownerPhysicalDims.empty() ||
      shape.size() != physicalBlockShape.size())
    return weights;

  SmallVector<int64_t, 4> ownerExtents;
  SmallVector<int64_t, 4> ownerBlocks;
  int64_t totalBlocks = 1;
  for (int64_t rawDim : ownerPhysicalDims) {
    if (rawDim < 0 || static_cast<size_t>(rawDim) >= shape.size())
      return {};
    int64_t extent = shape[rawDim];
    int64_t block = physicalBlockShape[rawDim];
    if (extent <= 0 || block <= 0)
      return {};
    int64_t blocks = ceilDivPositive(extent, block);
    if (totalBlocks > 4096 / blocks)
      return {};
    totalBlocks *= blocks;
    ownerExtents.push_back(extent);
    ownerBlocks.push_back(block);
  }

  weights.reserve(totalBlocks);
  for (int64_t linear = 0; linear < totalBlocks; ++linear) {
    int64_t tmp = linear;
    int64_t work = 1;
    for (auto [idx, block] : llvm::enumerate(ownerBlocks)) {
      int64_t blockCount = ceilDivPositive(ownerExtents[idx], block);
      int64_t coord = tmp % blockCount;
      tmp /= blockCount;
      int64_t begin = coord * block;
      work = saturatingMultiplyPositive(
          work, std::clamp(ownerExtents[idx] - begin, int64_t{1}, block));
    }
    weights.push_back(work);
  }
  return weights;
}

static WeightedCuEstimate estimateWeightedCuWork(ArrayRef<int64_t> weights,
                                                 int64_t partitions) {
  WeightedCuEstimate estimate;
  if (weights.empty() || partitions <= 0)
    return estimate;

  int64_t totalWeight = 0;
  SmallVector<int64_t, 16> positiveWeights;
  positiveWeights.reserve(weights.size());
  for (int64_t rawWeight : weights) {
    int64_t weight = std::max<int64_t>(1, rawWeight);
    positiveWeights.push_back(weight);
    totalWeight = saturatingAddPositive(totalWeight, weight);
  }
  if (totalWeight <= 0)
    return estimate;

  partitions =
      std::clamp<int64_t>(partitions, int64_t{1}, positiveWeights.size());
  int64_t groupsLeft = partitions;
  int64_t remainingWeight = totalWeight;
  int64_t currentWeight = 0;
  int64_t maxPartitionWeight = 0;

  auto closeGroup = [&]() {
    maxPartitionWeight = std::max(maxPartitionWeight, currentWeight);
    remainingWeight = std::max<int64_t>(0, remainingWeight - currentWeight);
    --groupsLeft;
    currentWeight = 0;
  };

  for (int64_t weight : positiveWeights) {
    if (currentWeight > 0 && groupsLeft > 1) {
      int64_t target = ceilDivPositive(remainingWeight, groupsLeft);
      if (currentWeight + weight > target)
        closeGroup();
    }
    currentWeight = saturatingAddPositive(currentWeight, weight);
  }
  if (currentWeight > 0)
    closeGroup();

  double ideal =
      static_cast<double>(totalWeight) / static_cast<double>(partitions);
  estimate.totalWeight = totalWeight;
  estimate.maxPartitionWeight = std::max<int64_t>(1, maxPartitionWeight);
  estimate.imbalance =
      ideal > 0.0 ? std::max(0.0, estimate.maxPartitionWeight / ideal - 1.0)
                  : 0.0;
  return estimate;
}

static unsigned inferPartCount(ArrayRef<unsigned> vertexToPart) {
  unsigned partCount = 0;
  for (unsigned part : vertexToPart)
    partCount = std::max(partCount, part + 1);
  return partCount;
}

static int64_t getTypedVertexWeight(const CuMuTypedHypergraph &graph,
                                    unsigned vertex) {
  if (vertex >= graph.vertices.size())
    return 1;
  return std::max<int64_t>(1, graph.vertices[vertex].workWeight);
}

static int64_t computePartWeights(const CuMuTypedHypergraph &graph,
                                  ArrayRef<unsigned> vertexToPart,
                                  SmallVectorImpl<int64_t> &partWeights,
                                  SmallVectorImpl<unsigned> &partCounts) {
  unsigned partCount = inferPartCount(vertexToPart);
  partWeights.assign(partCount, 0);
  partCounts.assign(partCount, 0);

  int64_t totalWeight = 0;
  for (auto [vertex, part] : llvm::enumerate(vertexToPart)) {
    if (part >= partCount)
      continue;
    int64_t weight = getTypedVertexWeight(graph, vertex);
    partWeights[part] = saturatingAddPositive(partWeights[part], weight);
    ++partCounts[part];
    totalWeight = saturatingAddPositive(totalWeight, weight);
  }
  return totalWeight;
}

static double computePartImbalance(ArrayRef<int64_t> partWeights,
                                   int64_t totalWeight) {
  if (partWeights.empty() || totalWeight <= 0)
    return 0.0;
  int64_t maxWeight = 0;
  for (int64_t weight : partWeights)
    maxWeight = std::max(maxWeight, weight);
  double ideal = static_cast<double>(totalWeight) /
                 static_cast<double>(partWeights.size());
  return ideal > 0.0 ? std::max(0.0, maxWeight / ideal - 1.0) : 0.0;
}

static double scoreTypedAssignment(const CuMuTypedHypergraph &graph,
                                   ArrayRef<unsigned> vertexToPart,
                                   double remoteFanoutWeight,
                                   SmallVectorImpl<int64_t> &partWeights,
                                   SmallVectorImpl<unsigned> &partCounts) {
  int64_t totalWeight =
      computePartWeights(graph, vertexToPart, partWeights, partCounts);
  int64_t cutBytes = computeCuMuHypergraphCutBytes(graph, vertexToPart);
  double imbalance = computePartImbalance(partWeights, totalWeight);
  double balancePenalty =
      imbalance * static_cast<double>(std::max<int64_t>(1, totalWeight));
  return static_cast<double>(cutBytes) * std::max(0.0, remoteFanoutWeight) +
         balancePenalty;
}

static SmallVector<unsigned, 8>
refineCuMuAssignment(const CuMuTypedHypergraph &graph,
                     ArrayRef<unsigned> initialAssignment,
                     const CuMuPartitionObjective &objective) {
  SmallVector<unsigned, 8> assignment(initialAssignment.begin(),
                                      initialAssignment.end());
  if (graph.vertices.empty() || graph.nets.empty() || assignment.size() <= 1)
    return assignment;

  unsigned partCount = inferPartCount(assignment);
  if (partCount <= 1)
    return assignment;

  double tolerance = std::max(0.0, objective.workImbalanceTolerance);
  double remoteFanoutWeight = std::max(0.0, objective.remoteFanoutWeight);

  SmallVector<int64_t, 8> partWeights;
  SmallVector<unsigned, 8> partCounts;
  double currentScore = scoreTypedAssignment(
      graph, assignment, remoteFanoutWeight, partWeights, partCounts);

  int64_t totalWeight = 0;
  int64_t heaviestVertex = 1;
  for (unsigned vertex = 0; vertex < assignment.size(); ++vertex) {
    int64_t weight = getTypedVertexWeight(graph, vertex);
    totalWeight = saturatingAddPositive(totalWeight, weight);
    heaviestVertex = std::max(heaviestVertex, weight);
  }
  int64_t idealCeil =
      ceilDivPositive(std::max<int64_t>(1, totalWeight), partCount);
  int64_t balanceCap = std::max<int64_t>(
      heaviestVertex, static_cast<int64_t>(std::ceil(
                          static_cast<double>(idealCeil) * (1.0 + tolerance))));

  unsigned maxIterations = std::min<unsigned>(
      64, static_cast<unsigned>(assignment.size() *
                                std::max<unsigned>(1, partCount)));
  for (unsigned iter = 0; iter < maxIterations; ++iter) {
    double bestGain = 0.0;
    unsigned bestVertex = assignment.size();
    unsigned bestPart = partCount;

    for (unsigned vertex = 0; vertex < assignment.size(); ++vertex) {
      unsigned fromPart = assignment[vertex];
      if (fromPart >= partCount || partCounts[fromPart] <= 1)
        continue;
      int64_t vertexWeight = getTypedVertexWeight(graph, vertex);

      for (unsigned toPart = 0; toPart < partCount; ++toPart) {
        if (toPart == fromPart)
          continue;

        int64_t newToWeight =
            saturatingAddPositive(partWeights[toPart], vertexWeight);
        if (newToWeight > balanceCap && newToWeight > partWeights[toPart])
          continue;

        SmallVector<unsigned, 8> trial(assignment.begin(), assignment.end());
        trial[vertex] = toPart;
        SmallVector<int64_t, 8> trialWeights;
        SmallVector<unsigned, 8> trialCounts;
        double trialScore = scoreTypedAssignment(
            graph, trial, remoteFanoutWeight, trialWeights, trialCounts);
        double gain = currentScore - trialScore;
        if (gain <= 1.0e-9)
          continue;
        if (gain > bestGain + 1.0e-9 ||
            (std::abs(gain - bestGain) <= 1.0e-9 &&
             (vertex < bestVertex ||
              (vertex == bestVertex && toPart < bestPart)))) {
          bestGain = gain;
          bestVertex = vertex;
          bestPart = toPart;
        }
      }
    }

    if (bestVertex >= assignment.size())
      break;

    assignment[bestVertex] = bestPart;
    currentScore = scoreTypedAssignment(graph, assignment, remoteFanoutWeight,
                                        partWeights, partCounts);
  }

  return assignment;
}

static double scoreRemoteFanout(const CuMuMemoryUnit &memory,
                                CuMuPartitionChoice &choice,
                                const CuMuPartitionObjective &objective,
                                double syncCost, double dataCost) {
  if (!memory.typedHypergraph.vertices.empty() &&
      !memory.typedHypergraph.nets.empty()) {
    choice.vertexToPart = buildContiguousCuPartAssignment(
        memory.typedHypergraph.vertices.size(),
        static_cast<unsigned>(std::max<int64_t>(1, choice.computeUnits)));
    choice.vertexToPart = refineCuMuAssignment(memory.typedHypergraph,
                                               choice.vertexToPart, objective);
    int64_t cutBytes = computeCuMuHypergraphCutBytes(memory.typedHypergraph,
                                                     choice.vertexToPart);
    if (cutBytes <= 0)
      return 0.0;
    double packets =
        static_cast<double>(cutBytes) /
        static_cast<double>(std::max<int64_t>(1, choice.tilePayloadBytes));
    return packets *
           (syncCost + dataCost * std::max(0.0, objective.remoteFanoutWeight));
  }

  if (memory.hyperedges.empty())
    return 0.0;

  int64_t tileBytes = std::max<int64_t>(1, choice.tilePayloadBytes);
  int64_t defaultBytes = tileBytes;
  if (!memory.hyperedges.empty()) {
    int64_t totalTraffic = 0;
    for (const CuMuHyperedgePressure &edge : memory.hyperedges)
      totalTraffic += std::max<int64_t>(0, edge.trafficBytes);
    if (totalTraffic > 0)
      defaultBytes = ceilDivPositive(
          totalTraffic, static_cast<int64_t>(memory.hyperedges.size()));
  }
  double score = 0.0;
  for (const CuMuHyperedgePressure &edge : memory.hyperedges) {
    int64_t remoteFanout = std::max<int64_t>(0, edge.remoteFanout);
    if (remoteFanout == 0)
      continue;
    int64_t trafficBytes =
        edge.trafficBytes > 0 ? edge.trafficBytes : defaultBytes;
    double packets = static_cast<double>(std::max<int64_t>(1, trafficBytes)) /
                     static_cast<double>(tileBytes);
    double remoteCuts = static_cast<double>(std::min<int64_t>(
        remoteFanout, std::max<int64_t>(0, choice.computeUnits - 1)));
    score += remoteCuts * (syncCost + packets * dataCost);
  }
  return score;
}

static double scoreCuMuPartition(const CuMuMemoryUnit &memory,
                                 const CuMuComputeUnitTarget &compute,
                                 const CuMuPartitionObjective &objective,
                                 CuMuPartitionChoice &choice) {
  double taskCost = std::max(0.0, compute.taskCreationCost);
  double syncCost = std::max(0.0, compute.taskSyncCost);
  double dataCost = std::max(1.0, compute.dataAccessCost);
  double logicalWorkers =
      static_cast<double>(std::max<int64_t>(1, compute.logicalWorkerCapacity));
  double exposed = static_cast<double>(std::max<int64_t>(
      1, std::min(choice.computeUnits, compute.logicalWorkerCapacity)));

  double score = static_cast<double>(choice.computeUnits) * taskCost;

  SmallVector<int64_t, 16> candidateWorkWeights;
  ArrayRef<int64_t> workWeights = compute.cuWorkWeights;
  if (static_cast<int64_t>(workWeights.size()) != choice.computeUnits) {
    candidateWorkWeights = buildOwnerBlockWorkWeights(
        memory.shape, memory.ownerPhysicalDims, choice.physicalBlockShape);
    workWeights = candidateWorkWeights;
  }

  WeightedCuEstimate work =
      estimateWeightedCuWork(workWeights, choice.computeUnits);
  choice.maxCuWorkWeight = work.maxPartitionWeight;
  choice.workImbalance = work.imbalance;
  if (!workWeights.empty()) {
    score += work.imbalance * static_cast<double>(work.totalWeight) *
             (taskCost + dataCost);
  }

  // Concurrency is the first-order objective. A choice that cannot expose
  // enough independent CUs to fill the platform is dominated unless no
  // candidate can.
  double missingParallelism = std::max(0.0, logicalWorkers - exposed);
  score += missingParallelism * (taskCost + syncCost + dataCost) * 64.0;

  // Communication is abstract at SDE. Use layout-disagreement volume only as a
  // pressure signal: many tiny MUs inflate remote acquire/copy/control traffic,
  // while larger MUs amortize it. This names no concrete communication op or
  // target object.
  bool hasCommunication = !memory.hyperedges.empty();
  if (hasCommunication) {
    score += static_cast<double>(choice.computeUnits) * syncCost;

    int64_t tileBytes = std::max<int64_t>(1, choice.tilePayloadBytes);
    int64_t trafficBytes = tileBytes;
    for (const CuMuHyperedgePressure &edge : memory.hyperedges)
      trafficBytes += std::max<int64_t>(0, edge.trafficBytes);
    double packets = static_cast<double>(trafficBytes) /
                     static_cast<double>(tileBytes);
    score += packets * dataCost;
  }

  choice.remoteFanoutScore =
      scoreRemoteFanout(memory, choice, objective, syncCost, dataCost);
  score += choice.remoteFanoutScore;

  if (objective.targetTileBytes > 0 &&
      choice.tilePayloadBytes < objective.targetTileBytes) {
    double shortfall = static_cast<double>(objective.targetTileBytes -
                                           choice.tilePayloadBytes) /
                       static_cast<double>(objective.targetTileBytes);
    double pressure = hasCommunication ? 16.0 : 4.0;
    score += shortfall * pressure * static_cast<double>(choice.computeUnits) *
             (taskCost + syncCost + dataCost);
  }

  return score;
}

std::optional<CuMuPartitionChoice> chooseCuMuGraphPartition(
    const CuMuMemoryUnit &memory, const CuMuComputeUnitTarget &compute,
    const CuMuPartitionObjective &objective,
    ArrayRef<int64_t> initialPhysicalBlockShape,
    llvm::function_ref<bool(int64_t, SmallVectorImpl<int64_t> &)> rebuild) {
  int64_t requested = std::max<int64_t>(1, compute.requestedComputeUnits);
  int64_t floor =
      std::clamp<int64_t>(compute.minComputeUnits, int64_t{1}, requested);

  if (memory.shape.empty() || memory.ownerPhysicalDims.empty() ||
      memory.elementBytes <= 0 || initialPhysicalBlockShape.empty())
    return std::nullopt;

  std::optional<CuMuPartitionChoice> best;
  SmallVector<int64_t, 16> candidateUnits = enumerateCandidateComputeUnits(
      requested, floor, std::max<int64_t>(1, compute.logicalWorkerCapacity),
      memory.hyperedges);

  for (int64_t candidate : candidateUnits) {
    SmallVector<int64_t, 4> candidateShape;
    if (!rebuildCandidateToFixpoint(memory, floor, requested, candidate,
                                    initialPhysicalBlockShape, rebuild,
                                    candidateShape))
      continue;
    int64_t actualUnits = inferCuCountFromMuPartition(
        memory.shape, memory.ownerPhysicalDims, candidateShape);
    if (actualUnits > 0) {
      CuMuPartitionChoice choice;
      choice.computeUnits = actualUnits;
      choice.exposedParallelism = std::min<int64_t>(
          actualUnits, std::max<int64_t>(1, compute.logicalWorkerCapacity));
      choice.tilePayloadBytes =
          tilePayloadBytes(candidateShape, memory.elementBytes);
      choice.physicalBlockShape.assign(candidateShape.begin(),
                                       candidateShape.end());
      choice.score = scoreCuMuPartition(memory, compute, objective, choice);
      if (!best || choice.score < best->score)
        best = std::move(choice);
    }
  }

  return best;
}

} // namespace mlir::carts::sde
