///==========================================================================///
/// File: DistributionHeuristics.cpp
///
/// H2 distribution policy heuristics: machine-aware strategy, pattern, and
/// worker-topology selection.
///==========================================================================///

#include "arts/analysis/heuristics/DistributionHeuristics.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/heuristics/HeuristicUtils.h"
#include "arts/analysis/loop/LoopAnalysis.h"
#include "arts/utils/LoopUtils.h"
#include "arts/utils/OperationAttributes.h"
#include <algorithm>
#include <cmath>

using namespace mlir;
using namespace mlir::arts;

namespace {

static std::optional<int64_t> getExplicitWorkerCount(EdtOp edt) {
  if (!edt)
    return std::nullopt;
  return arts::getWorkers(edt.getOperation());
}

static std::optional<int64_t> getExplicitWorkersPerNodeCount(EdtOp edt) {
  if (!edt)
    return std::nullopt;
  return arts::getWorkersPerNode(edt.getOperation());
}

static DistributionKind toDistributionKind(EdtDistributionKind kind) {
  switch (kind) {
  case EdtDistributionKind::block:
    return DistributionKind::Flat;
  case EdtDistributionKind::two_level:
    return DistributionKind::TwoLevel;
  case EdtDistributionKind::block_cyclic:
    return DistributionKind::BlockCyclic;
  case EdtDistributionKind::tiling_2d:
    return DistributionKind::Tiling2D;
  case EdtDistributionKind::replicated:
    return DistributionKind::Replicated;
  }
  return DistributionKind::Flat;
}

static int64_t ceilDivPositive(int64_t num, int64_t denom) {
  if (num <= 0 || denom <= 0)
    return 1;
  return 1 + (num - 1) / denom;
}

static int64_t clampPositive(int64_t value, int64_t minValue,
                             int64_t maxValue) {
  return std::clamp(std::max<int64_t>(1, value), std::max<int64_t>(1, minValue),
                    std::max<int64_t>(1, maxValue));
}

static int64_t estimateNestedLoopWork(LoopAnalysis &loopAnalysis, ForOp forOp) {
  if (!forOp)
    return 1;

  if (auto perfectWork =
          loopAnalysis.estimateStaticPerfectNestedWork(forOp.getOperation(), 8);
      perfectWork && *perfectWork > 1) {
    return *perfectWork;
  }

  int64_t maxNestedTrip = 1;
  forOp.walk([&](scf::ForOp nestedFor) {
    if (nestedFor == forOp)
      return;
    if (auto trip = loopAnalysis.getStaticTripCount(nestedFor.getOperation()))
      maxNestedTrip = std::max(maxNestedTrip, *trip);
  });
  return maxNestedTrip;
}

static int64_t estimateStencilIterationWork(LoopAnalysis &loopAnalysis,
                                            ForOp forOp, int64_t cap) {
  if (!forOp)
    return 1;

  int64_t boundedCap = std::max<int64_t>(1, cap);
  if (auto perfectWork = loopAnalysis.estimateStaticPerfectNestedWork(
          forOp.getOperation(), boundedCap);
      perfectWork && *perfectWork > 1) {
    return *perfectWork;
  }

  int64_t maxNestedTrip = 1;
  forOp.walk([&](scf::ForOp nestedFor) {
    if (nestedFor == forOp)
      return;
    if (auto trip = loopAnalysis.getStaticTripCount(nestedFor.getOperation()))
      maxNestedTrip = std::max(maxNestedTrip, *trip);
  });
  return std::min(maxNestedTrip, boundedCap);
}

} // namespace

ParallelismDecision DistributionHeuristics::resolveParallelismFromMachine(
    const AbstractMachine *machine) {
  ParallelismDecision decision;
  if (!machine)
    return decision;

  int64_t nodeCount = machine->getNodeCount();
  int64_t workerThreads = machine->getRuntimeWorkersPerNode();

  if (nodeCount > 1) {
    decision.concurrency = EdtConcurrency::internode;
    decision.workersPerNode = workerThreads > 0 ? workerThreads : 1;
    decision.totalWorkers = machine->getRuntimeTotalWorkers();
    if (decision.totalWorkers <= 0)
      decision.totalWorkers = nodeCount * decision.workersPerNode;
    return decision;
  }

  decision.concurrency = EdtConcurrency::intranode;
  if (workerThreads > 0) {
    decision.totalWorkers = workerThreads;
    decision.workersPerNode = workerThreads;
    return decision;
  }

  if (nodeCount == 1) {
    decision.totalWorkers = 1;
    decision.workersPerNode = 1;
  }

  return decision;
}

std::optional<WorkerConfig>
DistributionHeuristics::resolveWorkerConfig(EdtOp parallelEdt,
                                            const AbstractMachine *machine) {
  WorkerConfig cfg;
  cfg.internode = parallelEdt.getConcurrency() == EdtConcurrency::internode;

  if (auto workers = getExplicitWorkerCount(parallelEdt)) {
    cfg.totalWorkers = *workers;
    if (cfg.totalWorkers <= 0)
      return std::nullopt;

    if (cfg.internode) {
      if (auto workersPerNode = getExplicitWorkersPerNodeCount(parallelEdt)) {
        cfg.workersPerNode = *workersPerNode;
      } else if (machine && machine->getNodeCount() > 0) {
        int64_t nc = machine->getNodeCount();
        cfg.workersPerNode = std::max<int64_t>(1, cfg.totalWorkers / nc);
      }
      if (cfg.workersPerNode <= 0)
        cfg.workersPerNode = 1;
    } else {
      cfg.workersPerNode = cfg.totalWorkers;
    }
    return cfg;
  }

  if (auto module = parallelEdt->getParentOfType<ModuleOp>()) {
    if (auto runtimeTotalWorkers = getRuntimeTotalWorkers(module);
        runtimeTotalWorkers && *runtimeTotalWorkers > 0) {
      cfg.totalWorkers = *runtimeTotalWorkers;
      if (!cfg.internode) {
        cfg.workersPerNode = cfg.totalWorkers;
      } else if (auto runtimeTotalNodes = getRuntimeTotalNodes(module);
                 runtimeTotalNodes && *runtimeTotalNodes > 0) {
        cfg.workersPerNode =
            std::max<int64_t>(1, cfg.totalWorkers / *runtimeTotalNodes);
      } else {
        cfg.workersPerNode = cfg.totalWorkers;
      }
      return cfg;
    }
  }

  if (!machine)
    return std::nullopt;

  int64_t runtimeWorkersPerNode = machine->getRuntimeWorkersPerNode();
  if (runtimeWorkersPerNode <= 0)
    return std::nullopt;

  if (!cfg.internode) {
    cfg.workersPerNode = runtimeWorkersPerNode;
    cfg.totalWorkers = cfg.workersPerNode;
    return cfg;
  }

  if (machine->getNodeCount() <= 0)
    return std::nullopt;

  cfg.workersPerNode = runtimeWorkersPerNode;
  cfg.totalWorkers = machine->getRuntimeTotalWorkers();
  if (cfg.totalWorkers <= 0)
    return std::nullopt;

  return cfg;
}

std::optional<Wavefront2DTilingPlan>
DistributionHeuristics::chooseWavefront2DTilingPlan(
    int64_t rowExtent, int64_t colExtent, const WorkerConfig &workerCfg) {
  if (rowExtent <= 0 || colExtent <= 0)
    return std::nullopt;

  /// TwoLevel lowering still dispatches one task lane per global worker and
  /// routes that lane to a node from the global worker id. Keep the wavefront
  /// frontier target in global-worker units here; node-shared DB ownership is
  /// handled later by lowering, not by semantic tile selection.
  int64_t effectiveWorkers = std::max<int64_t>(1, workerCfg.totalWorkers);
  int64_t minRowTiles = (effectiveWorkers > 1 && rowExtent >= 2) ? 2 : 1;
  int64_t minColTiles = (effectiveWorkers > 1 && colExtent >= 2) ? 2 : 1;
  int64_t maxRowTiles = std::max<int64_t>(
      minRowTiles, std::min<int64_t>(rowExtent, effectiveWorkers * 2));
  int64_t maxColTiles = std::max<int64_t>(
      minColTiles, std::min<int64_t>(colExtent, effectiveWorkers * 4));

  /// A weighted 2-D wavefront with rank = 2*row + col only grows its ready
  /// frontier with the number of row tiles. Wider column grids beyond 2R-1
  /// mostly add ranks/tasks without improving the maximum ready set.
  int64_t maxFrontierRows = std::max<int64_t>(
      minRowTiles, std::min<int64_t>({maxRowTiles, effectiveWorkers,
                                      ceilDivPositive(maxColTiles, 2)}));
  if (maxFrontierRows < minRowTiles)
    return std::nullopt;

  constexpr int64_t kPreferredMinCellsPerTask = 1 << 15;
  long double totalCells =
      static_cast<long double>(rowExtent) * static_cast<long double>(colExtent);
  int64_t desiredTasksByGranularity = std::max<int64_t>(
      1,
      static_cast<int64_t>(std::ceil(
          totalCells / static_cast<long double>(kPreferredMinCellsPerTask))));
  if (desiredTasksByGranularity < 4)
    return std::nullopt;

  long double discriminant =
      1.0L + 8.0L * static_cast<long double>(desiredTasksByGranularity);
  int64_t granularityRows =
      static_cast<int64_t>(std::ceil((1.0L + std::sqrt(discriminant)) / 4.0L));
  granularityRows =
      clampPositive(granularityRows, minRowTiles, maxFrontierRows);

  /// Large weighted wavefronts can underutilize the machine even when the
  /// per-task granularity looks healthy. Allow a worker-utilization floor as
  /// long as the physical tiles remain comfortably above a hard minimum size.
  constexpr int64_t kAbsoluteMinCellsPerTask = 1 << 13;
  int64_t maxRowsByTileArea = static_cast<int64_t>(std::floor(
      std::sqrt(totalCells /
                (2.0L * static_cast<long double>(kAbsoluteMinCellsPerTask)))));
  maxRowsByTileArea =
      clampPositive(maxRowsByTileArea, minRowTiles, maxFrontierRows);

  int64_t frontierRows = granularityRows;
  if (desiredTasksByGranularity >= effectiveWorkers * 2) {
    int64_t workerSaturationFloor = clampPositive(
        (effectiveWorkers + 1) / 2, minRowTiles, maxRowsByTileArea);
    frontierRows = std::max(frontierRows, workerSaturationFloor);
  }
  frontierRows = clampPositive(frontierRows, minRowTiles, maxRowsByTileArea);

  int64_t targetColTiles =
      clampPositive(2 * frontierRows - 1, minColTiles, maxColTiles);
  /// Weighted 2-D wavefronts only expose at most `frontierRows` ready tasks at
  /// a time. Wider column grids mainly add extra anti-diagonal ranks and EDT
  /// overhead without increasing peak parallelism, so keep a soft budget on
  /// the total tiles per timestep.
  constexpr int64_t kPreferredMaxTilesPerWorker = 10;
  int64_t maxTilesPerStep = std::max<int64_t>(
      effectiveWorkers, effectiveWorkers * kPreferredMaxTilesPerWorker);
  int64_t maxColTilesByBudget =
      clampPositive(maxTilesPerStep / frontierRows, minColTiles, maxColTiles);
  targetColTiles = std::min(targetColTiles, maxColTilesByBudget);

  int64_t tileRows = ceilDivPositive(rowExtent, frontierRows);
  int64_t tileCols = ceilDivPositive(colExtent, targetColTiles);

  Wavefront2DTilingPlan plan;
  plan.tileRows = std::max<int64_t>(1, tileRows);
  plan.tileCols = std::max<int64_t>(1, tileCols);

  int64_t numRowTiles = ceilDivPositive(rowExtent, plan.tileRows);
  if (numRowTiles > effectiveWorkers) {
    int64_t chunkTiles = ceilDivPositive(numRowTiles, effectiveWorkers);
    if (chunkTiles > 1)
      plan.taskChunkHint = chunkTiles;
  }

  return plan;
}

std::optional<int64_t> DistributionHeuristics::computeCoarsenedBlockHint(
    ForOp forOp, LoopAnalysis &loopAnalysis, const WorkerConfig &workerCfg) {
  if (workerCfg.totalWorkers <= 0)
    return std::nullopt;

  LoopNode *loopNode = loopAnalysis.getOrCreateLoopNode(forOp.getOperation());
  if (isSequentialLoop(forOp, loopNode))
    return std::nullopt;

  auto tripOpt = loopAnalysis.getStaticTripCount(forOp.getOperation());
  if (!tripOpt)
    return std::nullopt;

  int64_t tripCount = *tripOpt;
  if (tripCount <= 0)
    return std::nullopt;

  int64_t effectiveTripCount = tripCount;
  bool allowNestedWorkBoost = false;
  bool isStencilPattern = false;
  bool isWavefrontPattern = false;
  int64_t repeatedTripProduct = 1;
  int64_t nestedLoopWork = estimateNestedLoopWork(loopAnalysis, forOp);
  constexpr int64_t kPreferredMinCellsPerStencilTask = 1 << 21;
  int64_t stencilIterationWork = 1;
  if (auto depPattern = getEffectiveDepPattern(forOp.getOperation())) {
    isWavefrontPattern = *depPattern == ArtsDepPattern::wavefront_2d;
    /// The loop access summary is not always populated yet when ForOpt runs.
    /// Fall back to the semantic dep-pattern contract so Jacobi/stencil loops
    /// still take the stencil coarsening path before lowering.
    isStencilPattern = isStencilFamilyDepPattern(*depPattern);
  }
  if (auto summary =
          loopAnalysis.getLoopDbAccessSummary(forOp.getOperation())) {
    isStencilPattern = isStencilPattern || summary->distributionPattern ==
                                               EdtDistributionPattern::stencil;
    allowNestedWorkBoost =
        !isStencilPattern &&
        summary->distributionPattern == EdtDistributionPattern::uniform &&
        summary->allocPatterns.size() <= 4 && !summary->hasStencilOffset &&
        !summary->hasMatmulUpdate && !summary->hasTriangularBound;
  }
  if (isStencilPattern) {
    /// Keep the generic nested-work signal aggressively capped, but estimate
    /// stencil inner-loop work with a larger budget so repeated N-D stencils
    /// can coarsen their owned strips from real per-iteration work.
    stencilIterationWork = estimateStencilIterationWork(
        loopAnalysis, forOp, kPreferredMinCellsPerStencilTask);
  }
  if (allowNestedWorkBoost) {
    if (nestedLoopWork > 1) {
      if (nestedLoopWork >=
          std::numeric_limits<int64_t>::max() / effectiveTripCount)
        effectiveTripCount = std::numeric_limits<int64_t>::max();
      else
        effectiveTripCount *= nestedLoopWork;
    }
  }

  if (isStencilPattern && !allowNestedWorkBoost) {
    if (nestedLoopWork > 1) {
      int64_t cappedWork = std::min(nestedLoopWork, (int64_t)1024);
      if (cappedWork >=
          std::numeric_limits<int64_t>::max() / effectiveTripCount)
        effectiveTripCount = std::numeric_limits<int64_t>::max();
      else
        effectiveTripCount *= cappedWork;
    }
  }

  int64_t minItersPerWorker = 8;
  if (auto parentEdt = forOp->getParentOfType<EdtOp>()) {
    int64_t depCount = static_cast<int64_t>(parentEdt.getDependencies().size());
    if (depCount > 0)
      minItersPerWorker = std::max<int64_t>(minItersPerWorker, depCount * 2);
  }

  if (isStencilPattern) {
    if (std::max(nestedLoopWork, stencilIterationWork) >= 4096) {
      minItersPerWorker = std::max<int64_t>(minItersPerWorker, 16);
    }

    repeatedTripProduct =
        arts::getRepeatedParentTripProduct(forOp.getOperation());
    if (repeatedTripProduct >= 64)
      minItersPerWorker = std::max<int64_t>(minItersPerWorker, 16);
    else if (repeatedTripProduct >= 16)
      minItersPerWorker = std::max<int64_t>(minItersPerWorker, 12);
  }

  if (isStencilPattern && !isWavefrontPattern && !workerCfg.internode) {
    int64_t minOwnedOuterIters = 8;

    /// Large stencil strips amortize better with fewer, larger owned tiles
    /// once each outer iteration already carries substantial inner-loop work.
    /// Without this floor, wide Jacobi/Poisson-style grids can still create
    /// one EDT per worker even though each strip already spans millions of
    /// cells.
    if (repeatedTripProduct >= 64 && stencilIterationWork >= 4096) {
      int64_t innerWork = std::max<int64_t>(1, stencilIterationWork);
      int64_t minOwnedByWork =
          ceilDivPositive(kPreferredMinCellsPerStencilTask, innerWork);
      minOwnedOuterIters =
          std::max<int64_t>(minOwnedOuterIters, minOwnedByWork);
    }

    int64_t maxWorkersByOwnedSpan =
        std::max<int64_t>(1, tripCount / minOwnedOuterIters);
    if (workerCfg.totalWorkers > maxWorkersByOwnedSpan) {
      int64_t desiredWorkers = maxWorkersByOwnedSpan;
      int64_t blockSize = (tripCount + desiredWorkers - 1) / desiredWorkers;
      blockSize = std::max<int64_t>(1, blockSize);
      if (blockSize > 1)
        return blockSize;
    }
  }

  if (effectiveTripCount >= workerCfg.totalWorkers * minItersPerWorker)
    return std::nullopt;

  int64_t desiredWorkers =
      std::max<int64_t>(1, effectiveTripCount / minItersPerWorker);
  desiredWorkers = std::min(desiredWorkers, workerCfg.totalWorkers);

  int64_t blockSize = (tripCount + desiredWorkers - 1) / desiredWorkers;
  blockSize = std::max<int64_t>(1, blockSize);

  if (workerCfg.internode) {
    int64_t maxBlockForAllWorkers = tripCount / workerCfg.totalWorkers;
    if (maxBlockForAllWorkers <= 1)
      return std::nullopt;
    blockSize = std::min(blockSize, maxBlockForAllWorkers);
  }

  if (blockSize <= 1)
    return std::nullopt;

  return blockSize;
}

DistributionStrategy
DistributionHeuristics::analyzeStrategy(EdtConcurrency concurrency,
                                        const AbstractMachine *machine) {
  DistributionStrategy strategy;

  if (concurrency == EdtConcurrency::intranode) {
    strategy.kind = DistributionKind::Flat;
    strategy.numNodes = 1;
    if (machine) {
      strategy.workersPerNode = machine->getRuntimeWorkersPerNode();
      strategy.totalWorkers = strategy.workersPerNode;
    }
    strategy.useDbAlignment = false;
    return strategy;
  }

  strategy.kind = DistributionKind::TwoLevel;
  if (machine) {
    strategy.numNodes = machine->getNodeCount();
    strategy.workersPerNode = machine->getRuntimeWorkersPerNode();
    strategy.totalWorkers = machine->getRuntimeTotalWorkers();
  }
  strategy.useDbAlignment = true;
  return strategy;
}

DistributionStrategy
DistributionHeuristics::resolveLoweringStrategy(EdtOp originalParallel,
                                                ForOp forOp) {
  DistributionStrategy strategy =
      analyzeStrategy(originalParallel.getConcurrency(), /*machine=*/nullptr);
  if (auto selectedKind = getEdtDistributionKind(forOp.getOperation()))
    strategy.kind = toDistributionKind(*selectedKind);
  return strategy;
}

std::optional<EdtDistributionPattern>
DistributionHeuristics::resolveDistributionPattern(AnalysisManager *AM,
                                                   ForOp forOp,
                                                   EdtOp originalParallel) {
  if (auto pattern = getEdtDistributionPattern(forOp.getOperation()))
    return pattern;
  if (AM) {
    if (auto pattern = AM->getLoopDistributionPattern(forOp.getOperation());
        pattern && *pattern != EdtDistributionPattern::unknown) {
      return pattern;
    }
  }
  return getEdtDistributionPattern(originalParallel.getOperation());
}

EdtDistributionKind DistributionHeuristics::selectDistributionKind(
    const DistributionStrategy &strategy, EdtDistributionPattern pattern) {
  if (pattern == EdtDistributionPattern::matmul) {
    if (strategy.kind == DistributionKind::TwoLevel)
      return EdtDistributionKind::tiling_2d;
    return EdtDistributionKind::block;
  }

  switch (strategy.kind) {
  case DistributionKind::TwoLevel:
    return EdtDistributionKind::two_level;
  case DistributionKind::BlockCyclic:
    return EdtDistributionKind::block_cyclic;
  case DistributionKind::Tiling2D:
    return EdtDistributionKind::tiling_2d;
  case DistributionKind::Replicated:
    return EdtDistributionKind::replicated;
  case DistributionKind::Flat:
    break;
  }

  switch (pattern) {
  case EdtDistributionPattern::triangular:
    return EdtDistributionKind::block_cyclic;
  case EdtDistributionPattern::stencil:
  case EdtDistributionPattern::uniform:
  case EdtDistributionPattern::matmul:
  case EdtDistributionPattern::unknown:
    return EdtDistributionKind::block;
  }
  return EdtDistributionKind::block;
}
