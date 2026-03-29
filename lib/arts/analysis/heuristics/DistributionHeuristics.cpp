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
#include <limits>

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
  if (auto summary =
          loopAnalysis.getLoopDbAccessSummary(forOp.getOperation())) {
    isStencilPattern =
        summary->distributionPattern == EdtDistributionPattern::stencil;
    allowNestedWorkBoost =
        summary->distributionPattern == EdtDistributionPattern::uniform &&
        summary->allocPatterns.size() <= 4 && !summary->hasStencilOffset &&
        !summary->hasMatmulUpdate && !summary->hasTriangularBound;
  }
  if (allowNestedWorkBoost) {
    if (auto nestedWork = loopAnalysis.estimateStaticPerfectNestedWork(
            forOp.getOperation(), 8);
        nestedWork && *nestedWork > 1) {
      if (*nestedWork >=
          std::numeric_limits<int64_t>::max() / effectiveTripCount)
        effectiveTripCount = std::numeric_limits<int64_t>::max();
      else
        effectiveTripCount *= *nestedWork;
    }
  }

  if (isStencilPattern && !allowNestedWorkBoost) {
    if (auto nestedWork = loopAnalysis.estimateStaticPerfectNestedWork(
            forOp.getOperation(), 8);
        nestedWork && *nestedWork > 1) {
      int64_t cappedWork = std::min(*nestedWork, (int64_t)1024);
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
    if (auto nestedWork = loopAnalysis.estimateStaticPerfectNestedWork(
            forOp.getOperation(), 8);
        nestedWork && *nestedWork >= 4096) {
      minItersPerWorker = std::max<int64_t>(minItersPerWorker, 16);
    }

    int64_t repeatProduct =
        arts::getRepeatedParentTripProduct(forOp.getOperation());
    if (repeatProduct >= 64)
      minItersPerWorker = std::max<int64_t>(minItersPerWorker, 16);
    else if (repeatProduct >= 16)
      minItersPerWorker = std::max<int64_t>(minItersPerWorker, 12);
  }

  if (isStencilPattern && !workerCfg.internode) {
    constexpr int64_t kMinStencilOwnedOuterIters = 8;
    int64_t maxWorkersByOwnedSpan =
        std::max<int64_t>(1, tripCount / kMinStencilOwnedOuterIters);
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
  DistributionStrategy strategy = analyzeStrategy(
      originalParallel.getConcurrency(), /*machine=*/nullptr);
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
