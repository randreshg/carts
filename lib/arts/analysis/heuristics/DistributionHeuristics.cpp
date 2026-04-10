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
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/PatternSemantics.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace mlir;
using namespace mlir::arts;

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

  if (auto workers = parallelEdt ? arts::getWorkers(parallelEdt.getOperation())
                                 : std::nullopt) {
    cfg.totalWorkers = *workers;
    if (cfg.totalWorkers <= 0)
      return std::nullopt;

    if (cfg.internode) {
      if (auto workersPerNode =
              parallelEdt ? arts::getWorkersPerNode(parallelEdt.getOperation())
                          : std::nullopt) {
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
    /// Weighted wavefronts only scale with the row frontier. When the kernel
    /// has ample work, drive that frontier toward the available worker count
    /// instead of only half-saturating the machine.
    int64_t workerSaturationFloor =
        clampPositive(effectiveWorkers, minRowTiles, maxRowsByTileArea);
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

std::optional<StencilStripCostModelResult>
DistributionHeuristics::evaluateStencilStripCostModel(
    const StencilStripCostModelInput &input) {
  if (input.tripCount <= 0 || input.stencilIterationWork <= 0 ||
      input.totalWorkers <= 0)
    return std::nullopt;

  constexpr int64_t kMinOwnedOuterItersFloor = 8;
  constexpr int64_t kAmortizationPressureStride = 8;
  constexpr int64_t kOwnedSpanStridePenalty = 2;
  constexpr int64_t kMaxAmortizationMultiplier = 4;

  int64_t baselineOwnedOuterIters =
      std::max<int64_t>(kMinOwnedOuterItersFloor,
                        ceilDivPositive(input.tripCount, input.totalWorkers));
  int64_t baselineOwnedCells = saturatingMulPositive(
      baselineOwnedOuterIters, input.stencilIterationWork);

  int64_t amortizationScore =
      floorLog2Positive(std::max<int64_t>(1, input.repeatedTripProduct)) +
      floorLog2Positive(std::max<int64_t>(1, input.stencilIterationWork));
  int64_t ownedSpanScore =
      floorLog2Positive(std::max<int64_t>(1, baselineOwnedOuterIters));
  int64_t stencilPressureScore =
      std::max<int64_t>(0, amortizationScore - ownedSpanScore);
  /// As each worker already owns a wider outer strip, require proportionally
  /// more amortization pressure before reducing worker count again. This keeps
  /// the policy monotone without a hard large-strip cutoff.
  int64_t amortizationStride =
      kAmortizationPressureStride + ownedSpanScore * kOwnedSpanStridePenalty;
  int64_t amortizationMultiplier = 1;
  if (stencilPressureScore >= amortizationStride) {
    /// Only coarsen once the pressure clears a full stride. Using ceil() here
    /// makes any positive excess immediately widen the owned strip, which can
    /// strand a large fraction of the machine on long-running stencil loops.
    amortizationMultiplier += stencilPressureScore / amortizationStride;
  }
  amortizationMultiplier =
      clampPositive(amortizationMultiplier, 1, kMaxAmortizationMultiplier);

  int64_t targetOwnedCells =
      saturatingMulPositive(baselineOwnedCells, amortizationMultiplier);
  int64_t minOwnedOuterIters = std::max<int64_t>(
      baselineOwnedOuterIters,
      ceilDivPositive(targetOwnedCells, input.stencilIterationWork));
  int64_t maxWorkersByOwnedSpan =
      std::max<int64_t>(1, input.tripCount / minOwnedOuterIters);

  StencilStripCostModelResult result;
  result.baselineOwnedOuterIters = baselineOwnedOuterIters;
  result.minOwnedOuterIters = minOwnedOuterIters;
  result.targetOwnedCells = targetOwnedCells;
  result.amortizationMultiplier = amortizationMultiplier;
  result.desiredWorkers =
      std::min(input.totalWorkers, std::max<int64_t>(1, maxWorkersByOwnedSpan));
  return result;
}

LoopCoarseningDecision DistributionHeuristics::computeLoopCoarseningDecision(
    ForOp forOp, LoopAnalysis &loopAnalysis, const WorkerConfig &workerCfg) {
  auto estimateNestedLoopWork = [&](ForOp loop, int64_t cap) {
    if (!loop)
      return int64_t{1};

    int64_t boundedCap = std::max<int64_t>(1, cap);
    if (auto perfectWork = loopAnalysis.estimateStaticPerfectNestedWork(
            loop.getOperation(), boundedCap);
        perfectWork && *perfectWork > 1) {
      return *perfectWork;
    }

    int64_t maxNestedTrip = 1;
    loop.walk([&](scf::ForOp nestedFor) {
      if (nestedFor == loop)
        return;
      if (auto trip = loopAnalysis.getStaticTripCount(nestedFor.getOperation()))
        maxNestedTrip = std::max(maxNestedTrip, *trip);
    });
    return std::min(maxNestedTrip, boundedCap);
  };
  auto estimateStencilIterationWork = [&](ForOp loop, int64_t cap) {
    if (!loop)
      return int64_t{1};

    int64_t boundedCap = std::max<int64_t>(1, cap);
    if (auto perfectWork = loopAnalysis.estimateStaticPerfectNestedWork(
            loop.getOperation(), boundedCap);
        perfectWork && *perfectWork > 1) {
      return *perfectWork;
    }

    int64_t maxNestedTrip = 1;
    loop.walk([&](scf::ForOp nestedFor) {
      if (nestedFor == loop)
        return;
      if (auto trip = loopAnalysis.getStaticTripCount(nestedFor.getOperation()))
        maxNestedTrip = std::max(maxNestedTrip, *trip);
    });
    return std::min(maxNestedTrip, boundedCap);
  };

  LoopCoarseningDecision decision;
  decision.desiredWorkers = std::max<int64_t>(1, workerCfg.totalWorkers);

  if (workerCfg.totalWorkers <= 0)
    return decision;

  LoopNode *loopNode = loopAnalysis.getOrCreateLoopNode(forOp.getOperation());
  if (isSequentialLoop(forOp, loopNode))
    return decision;

  auto tripOpt = loopAnalysis.getStaticTripCount(forOp.getOperation());
  if (!tripOpt)
    return decision;

  int64_t tripCount = *tripOpt;
  if (tripCount <= 0)
    return decision;

  int64_t effectiveTripCount = tripCount;
  bool allowNestedWorkBoost = false;
  bool isStencilPattern = false;
  bool isWavefrontPattern = false;
  bool canUseStencilStripCoarsening = false;
  bool hasTriangularBound = false;
  int64_t repeatedTripProduct = 1;
  int64_t nestedLoopWork = 1;
  int64_t stencilIterationWork = 1;
  std::optional<StencilStripCostModelResult> stencilStripPlan;
  std::optional<EdtDistributionPattern> semanticDistributionPattern;
  if (auto depPattern = getEffectiveDepPattern(forOp.getOperation())) {
    isWavefrontPattern = PatternSemantics::isWavefrontFamily(*depPattern);
    /// The loop access summary is not always populated yet when ForOpt runs.
    /// Fall back to the semantic dep-pattern contract so stencil-family loops
    /// still take the stencil coarsening path before lowering.
    isStencilPattern = PatternSemantics::isStencilFamily(*depPattern);
    if (auto directPattern = getDistributionPatternForDepPattern(*depPattern))
      semanticDistributionPattern = *directPattern;
  }
  if (isStencilPattern) {
    LoweringContractInfo loopContract =
        resolveLoopDistributionContract(forOp.getOperation());
    /// The owned-strip stencil policy is only profitable once lowering carries
    /// an explicit halo/ownership contract. Pure dep-pattern classification is
    /// too coarse and can under-distribute neighborhood kernels like
    /// convolution that do not need stencil halo amortization.
    canUseStencilStripCoarsening = loopContract.hasExplicitStencilContract();
  }
  if (auto summary =
          loopAnalysis.getLoopDbAccessSummary(forOp.getOperation())) {
    hasTriangularBound = summary->hasTriangularBound;
    if (summary->distributionPattern != EdtDistributionPattern::unknown)
      semanticDistributionPattern = summary->distributionPattern;
    isStencilPattern = isStencilPattern || semanticDistributionPattern ==
                                               EdtDistributionPattern::stencil;
    /// Uniform kernels should account for the work hidden in perfectly nested
    /// inner loops even when the body contains neighbor accesses. The DB
    /// summary marks those accesses with `hasStencilOffset`, but that does not
    /// mean the outer worksharing loop needs stencil-family coarsening. Kernels
    /// such as specfem velocity still scale by splitting the outer dimension
    /// directly across workers.
  }
  allowNestedWorkBoost =
      !isStencilPattern &&
      semanticDistributionPattern == EdtDistributionPattern::uniform &&
      !hasTriangularBound;
  if (isStencilPattern) {
    /// Keep the generic nested-work signal aggressively capped, but estimate
    /// stencil inner-loop work with a larger budget so repeated N-D stencils
    /// can coarsen their owned strips from real per-iteration work.
    constexpr int64_t kStencilIterationWorkEstimateCap = 1 << 21;
    stencilIterationWork =
        estimateStencilIterationWork(forOp, kStencilIterationWorkEstimateCap);
  }
  int64_t minItersPerWorker = 8;
  if (auto parentEdt = forOp->getParentOfType<EdtOp>()) {
    int64_t depCount = static_cast<int64_t>(parentEdt.getDependencies().size());
    if (depCount > 0)
      minItersPerWorker = std::max<int64_t>(minItersPerWorker, depCount * 2);
  }

  if (canUseStencilStripCoarsening) {
    repeatedTripProduct =
        arts::getRepeatedParentTripProduct(forOp.getOperation());
    StencilStripCostModelInput input;
    input.tripCount = tripCount;
    input.stencilIterationWork = std::max<int64_t>(1, stencilIterationWork);
    input.repeatedTripProduct = std::max<int64_t>(1, repeatedTripProduct);
    input.totalWorkers = workerCfg.totalWorkers;
    stencilStripPlan = evaluateStencilStripCostModel(input);

    if (stencilStripPlan && !isWavefrontPattern && !workerCfg.internode)
      minItersPerWorker = std::max<int64_t>(
          minItersPerWorker, stencilStripPlan->minOwnedOuterIters);
  }
  decision.minItersPerWorker = minItersPerWorker;

  /// Size the nested-work proof to the amount of work needed to keep the
  /// current worker topology busy. A fixed tiny cap can incorrectly collapse
  /// naturally scalable uniform kernels whose useful work mostly lives in
  /// inner loops, such as specfem velocity.
  int64_t targetEffectiveTripCount =
      saturatingMulPositive(workerCfg.totalWorkers, minItersPerWorker);
  int64_t nestedWorkSaturationTarget = ceilDivPositive(
      std::max<int64_t>(1, targetEffectiveTripCount), tripCount);
  int64_t nestedLoopWorkCap = std::max<int64_t>(8, nestedWorkSaturationTarget);
  nestedLoopWork = estimateNestedLoopWork(forOp, nestedLoopWorkCap);

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

  /// Prefer the stencil strip cost model for single-node stencils, including
  /// halo-exchange families. Larger owned strips can materially reduce EDT and
  /// dependence overhead on repeated stencil kernels once the worker-local
  /// halo fast path is available.
  if (canUseStencilStripCoarsening && !isWavefrontPattern &&
      !workerCfg.internode) {
    decision.stencilStripPlan = stencilStripPlan;
    if (decision.stencilStripPlan) {
      decision.desiredWorkers = decision.stencilStripPlan->desiredWorkers;
      if (workerCfg.totalWorkers > decision.desiredWorkers) {
        int64_t blockSize = ceilDivPositive(tripCount, decision.desiredWorkers);
        if (blockSize > 1) {
          decision.blockSize = blockSize;
          return decision;
        }
      }
    }
  }

  if (effectiveTripCount >= workerCfg.totalWorkers * minItersPerWorker)
    return decision;

  decision.desiredWorkers =
      std::max<int64_t>(1, effectiveTripCount / minItersPerWorker);
  decision.desiredWorkers =
      std::min(decision.desiredWorkers, workerCfg.totalWorkers);

  int64_t blockSize = ceilDivPositive(tripCount, decision.desiredWorkers);
  blockSize = std::max<int64_t>(1, blockSize);

  if (workerCfg.internode) {
    int64_t maxBlockForAllWorkers = tripCount / workerCfg.totalWorkers;
    if (maxBlockForAllWorkers <= 1)
      return decision;
    blockSize = std::min(blockSize, maxBlockForAllWorkers);
  }

  if (blockSize <= 1)
    return decision;

  decision.blockSize = blockSize;
  return decision;
}

std::optional<int64_t> DistributionHeuristics::computeCoarsenedBlockHint(
    ForOp forOp, LoopAnalysis &loopAnalysis, const WorkerConfig &workerCfg) {
  return computeLoopCoarseningDecision(forOp, loopAnalysis, workerCfg)
      .blockSize;
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
  if (auto selectedKind = getEdtDistributionKind(forOp.getOperation())) {
    switch (*selectedKind) {
    case EdtDistributionKind::block:
      strategy.kind = DistributionKind::Flat;
      break;
    case EdtDistributionKind::two_level:
      strategy.kind = DistributionKind::TwoLevel;
      break;
    case EdtDistributionKind::block_cyclic:
      strategy.kind = DistributionKind::BlockCyclic;
      break;
    case EdtDistributionKind::tiling_2d:
      strategy.kind = DistributionKind::Tiling2D;
      break;
    case EdtDistributionKind::replicated:
      strategy.kind = DistributionKind::Replicated;
      break;
    }
  }
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
  /// First, check if the distribution pattern has a pattern-specific override.
  /// This handles matmul -> tiling_2d for TwoLevel.
  /// PatternSemantics::getPreferredDistributionKind currently takes
  /// ArtsDepPattern, but we don't have access to that here. For now, inline the
  /// matmul override.
  if (pattern == EdtDistributionPattern::matmul) {
    if (strategy.kind == DistributionKind::TwoLevel)
      return EdtDistributionKind::tiling_2d;
    return EdtDistributionKind::block;
  }

  /// Strategy-based dispatch (general case).
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

  /// Fallback to pattern-specific defaults.
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
