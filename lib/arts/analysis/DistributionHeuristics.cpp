///==========================================================================///
/// File: DistributionHeuristics.cpp
///
/// H2 distribution heuristics: machine-aware work distribution.
///
/// Implements two distribution strategies:
///   - Flat: all workers divide iterations equally (intranode)
///   - TwoLevel: nodes get DB blocks, threads subdivide within (internode)
///
/// Example (strategy selection):
///   BEFORE: edt.parallel <intranode> ...   -> strategy = Flat
///   BEFORE: edt.parallel <internode> ...   -> strategy = TwoLevel
///
/// Example (pattern-to-kind selection):
///   pattern = uniform   -> kind = block / two_level
///   pattern = stencil   -> kind = block / two_level (stencil-aware acquire)
///   pattern = triangular-> kind = block_cyclic / two_level_block_cyclic
///   pattern = matmul    -> kind = tiling_2d
///==========================================================================///

#include "arts/analysis/DistributionHeuristics.h"
#include "arts/analysis/HeuristicUtils.h"
#include "arts/analysis/loop/LoopAnalysis.h"
#include "arts/codegen/Codegen.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/Utils.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/Builders.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Helpers
///===----------------------------------------------------------------------===///

/// Check if an operation can be safely cloned into a new region.
/// ARTS runtime query ops are safe to clone - they return the same value
/// regardless of where they are emitted.
static bool isSafeRuntimeQuery(Operation *op) {
  if (auto queryOp = dyn_cast<RuntimeQueryOp>(op)) {
    auto kind = queryOp.getKind();
    return kind == RuntimeQueryKind::totalNodes ||
           kind == RuntimeQueryKind::totalWorkers;
  }
  return false;
}

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
  if (auto summary =
          loopAnalysis.getLoopDbAccessSummary(forOp.getOperation())) {
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

  int64_t minItersPerWorker = 8;
  if (auto parentEdt = forOp->getParentOfType<EdtOp>()) {
    int64_t depCount = static_cast<int64_t>(parentEdt.getDependencies().size());
    if (depCount > 0) {
      /// ARTS pays fixed scheduling + dependency-slot setup cost per worker
      /// EDT. When a loop carries many DB dependencies, keep fewer, larger
      /// chunks so the useful work per EDT grows with dependency pressure.
      minItersPerWorker = std::max<int64_t>(minItersPerWorker, depCount * 2);
    }
  }

  if (effectiveTripCount >= workerCfg.totalWorkers * minItersPerWorker)
    return std::nullopt;

  int64_t desiredWorkers =
      std::max<int64_t>(1, effectiveTripCount / minItersPerWorker);
  desiredWorkers = std::min(desiredWorkers, workerCfg.totalWorkers);

  int64_t blockSize = (tripCount + desiredWorkers - 1) / desiredWorkers;
  blockSize = std::max<int64_t>(1, blockSize);

  /// For internode execution, keep enough chunks so all worker lanes can still
  /// participate. This preserves distribution while still allowing mild
  /// coarsening for very small trip counts.
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

int64_t DistributionHeuristics::chooseTiling2DColumnWorkers(
    int64_t totalWorkers, std::optional<int64_t> workersPerNode) {
  if (totalWorkers < 4)
    return 1;

  auto validDivisor = [&](int64_t d) -> bool {
    if (d <= 1 || totalWorkers % d != 0)
      return false;
    if (!workersPerNode || *workersPerNode <= 0)
      return true;
    return (*workersPerNode % d) == 0;
  };

  int64_t best = 1;
  double bestDist = std::numeric_limits<double>::infinity();
  double target = std::sqrt(static_cast<double>(totalWorkers));
  for (int64_t d = 2; d <= totalWorkers; ++d) {
    if (!validDivisor(d))
      continue;
    double dist = std::fabs(static_cast<double>(d) - target);
    if (dist < bestDist || (dist == bestDist && d > best)) {
      best = d;
      bestDist = dist;
    }
  }

  return best;
}

Tiling2DWorkerGrid DistributionHeuristics::getTiling2DWorkerGrid(
    ArtsCodegen *AC, Location loc, Value workerId, Value totalWorkers,
    Value workersPerNode) {
  Tiling2DWorkerGrid grid;
  Value one = AC->createIndexConstant(1, loc);
  Value zero = AC->createIndexConstant(0, loc);

  Value totalWorkersIndex = AC->castToIndex(totalWorkers, loc);
  Value workerIdIndex = AC->castToIndex(workerId, loc);

  grid.rowWorkers = totalWorkersIndex;
  grid.colWorkers = one;
  grid.rowWorkerId = workerIdIndex;
  grid.colWorkerId = zero;

  if (auto totalWorkersConst =
          ValueAnalysis::tryFoldConstantIndex(totalWorkersIndex)) {
    std::optional<int64_t> workersPerNodeConst = std::nullopt;
    if (workersPerNode)
      workersPerNodeConst = ValueAnalysis::tryFoldConstantIndex(
          AC->castToIndex(workersPerNode, loc));

    int64_t colWorkersConst =
        chooseTiling2DColumnWorkers(*totalWorkersConst, workersPerNodeConst);
    if (colWorkersConst > 1) {
      grid.colWorkers = AC->createIndexConstant(colWorkersConst, loc);
      grid.rowWorkers =
          AC->create<arith::DivUIOp>(loc, totalWorkersIndex, grid.colWorkers);
      grid.rowWorkerId =
          AC->create<arith::DivUIOp>(loc, workerIdIndex, grid.colWorkers);
      grid.colWorkerId =
          AC->create<arith::RemUIOp>(loc, workerIdIndex, grid.colWorkers);
    }
  }

  return grid;
}

///===----------------------------------------------------------------------===///
/// H2.1: Analyze Machine Topology -> Distribution Strategy
///===----------------------------------------------------------------------===///

DistributionStrategy
DistributionHeuristics::analyzeStrategy(EdtConcurrency concurrency,
                                        const AbstractMachine *machine) {
  DistributionStrategy strategy;

  if (concurrency == EdtConcurrency::intranode) {
    /// Flat: all workers divide iterations equally
    strategy.kind = DistributionKind::Flat;
    strategy.numNodes = 1;
    if (machine) {
      strategy.workersPerNode = machine->getRuntimeWorkersPerNode();
      strategy.totalWorkers = strategy.workersPerNode;
    }
    strategy.useDbAlignment = false;
    return strategy;
  }

  /// Internode -> TwoLevel
  strategy.kind = DistributionKind::TwoLevel;
  if (machine) {
    strategy.numNodes = machine->getNodeCount();
    strategy.workersPerNode = machine->getRuntimeWorkersPerNode();
    strategy.totalWorkers = machine->getRuntimeTotalWorkers();
  }
  strategy.useDbAlignment = true;
  return strategy;
}

DistributionKind
DistributionHeuristics::toDistributionKind(EdtDistributionKind kind) {
  switch (kind) {
  case EdtDistributionKind::block:
    return DistributionKind::Flat;
  case EdtDistributionKind::two_level:
    return DistributionKind::TwoLevel;
  case EdtDistributionKind::block_cyclic:
    return DistributionKind::BlockCyclic;
  case EdtDistributionKind::tiling_2d:
    return DistributionKind::Tiling2D;
  }
  return DistributionKind::Flat;
}

EdtDistributionKind DistributionHeuristics::selectDistributionKind(
    const DistributionStrategy &strategy, EdtDistributionPattern pattern) {
  if (pattern == EdtDistributionPattern::matmul) {
    /// Matmul benefits from 2D tile ownership under internode distribution.
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

///===----------------------------------------------------------------------===///
/// Balanced Distribution Helper
///===----------------------------------------------------------------------===///

std::pair<Value, Value> DistributionHeuristics::balancedDistribute(
    ArtsCodegen *AC, Location loc, Value totalChunks, Value numParticipants,
    Value participantId) {
  Value oneIndex = AC->createIndexConstant(1, loc);

  Value base = AC->create<arith::DivUIOp>(loc, totalChunks, numParticipants);
  Value rem = AC->create<arith::RemUIOp>(loc, totalChunks, numParticipants);

  Value getsExtra = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ult,
                                              participantId, rem);
  Value count = AC->create<arith::SelectOp>(
      loc, getsExtra, AC->create<arith::AddIOp>(loc, base, oneIndex), base);

  Value clipped = AC->create<arith::MinUIOp>(loc, participantId, rem);
  Value start = AC->create<arith::AddIOp>(
      loc, AC->create<arith::MulIOp>(loc, participantId, base), clipped);

  return {start, count};
}

///===----------------------------------------------------------------------===///
/// H2.2: Compute Runtime Bounds
///===----------------------------------------------------------------------===///

DistributionBounds DistributionHeuristics::computeBounds(
    ArtsCodegen *AC, Location loc, const DistributionStrategy &strategy,
    Value workerId, Value runtimeTotalWorkers, Value runtimeWorkersPerNode,
    Value totalIterations, Value totalChunks, Value blockSize) {
  DistributionBounds bounds;
  Value zeroIndex = AC->createIndexConstant(0, loc);

  if (strategy.kind == DistributionKind::BlockCyclic) {
    /// BlockCyclic: chunks are assigned round-robin by chunk id.
    /// Worker i processes chunk ids: i, i+W, i+2W, ...
    bounds.iterStart = zeroIndex;
    bounds.iterCount = zeroIndex;
    bounds.iterCountHint = zeroIndex;
    bounds.hasWork = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ult,
                                               workerId, totalChunks);

    /// Conservative acquire bounds: full loop span.
    /// ForLowering may specialize this further per strategy.
    bounds.acquireStart = zeroIndex;
    bounds.acquireSize = totalIterations;
    bounds.acquireSizeHint = totalIterations;
    return bounds;
  }

  if (strategy.kind == DistributionKind::Flat ||
      strategy.kind == DistributionKind::Tiling2D) {
    /// Flat: balanced distribution across all workers.
    /// Tiling2D: balanced row distribution across row workers
    /// (worker decomposition handled by getTiling2DWorkerGrid).
    Value distWorkerId = workerId;
    Value distWorkers = runtimeTotalWorkers;
    if (strategy.kind == DistributionKind::Tiling2D) {
      Tiling2DWorkerGrid grid = getTiling2DWorkerGrid(
          AC, loc, workerId, runtimeTotalWorkers, runtimeWorkersPerNode);
      distWorkerId = grid.rowWorkerId;
      distWorkers = grid.rowWorkers;
    }

    auto [firstChunkIdx, chunkCount] =
        balancedDistribute(AC, loc, totalChunks, distWorkers, distWorkerId);

    bounds.iterStart = AC->create<arith::MulIOp>(loc, firstChunkIdx, blockSize);
    bounds.iterCountHint =
        AC->create<arith::MulIOp>(loc, chunkCount, blockSize);

    /// Clamp: last worker's iterations may exceed totalIterations
    Value needZeroIters = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::uge, bounds.iterStart, totalIterations);
    Value remainingIters =
        AC->create<arith::SubIOp>(loc, totalIterations, bounds.iterStart);
    Value remainingItersNonNeg = AC->create<arith::SelectOp>(
        loc, needZeroIters, zeroIndex, remainingIters);
    bounds.iterCount = AC->create<arith::MinUIOp>(loc, bounds.iterCountHint,
                                                  remainingItersNonNeg);

    bounds.hasWork = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ugt,
                                               bounds.iterCount, zeroIndex);

    /// Flat: acquire bounds = iteration bounds
    bounds.acquireStart = bounds.iterStart;
    bounds.acquireSize = bounds.iterCount;
    bounds.acquireSizeHint = bounds.iterCountHint;

    return bounds;
  }

  /// TwoLevel: hierarchical node -> thread distribution
  Value numNodes = AC->create<arith::DivUIOp>(loc, runtimeTotalWorkers,
                                              runtimeWorkersPerNode);
  Value nodeId =
      AC->create<arith::DivUIOp>(loc, workerId, runtimeWorkersPerNode);
  Value localThreadId =
      AC->create<arith::RemUIOp>(loc, workerId, runtimeWorkersPerNode);

  /// Level 1: distribute chunks across nodes
  auto [nodeChunkStart, nodeChunkCount] =
      balancedDistribute(AC, loc, totalChunks, numNodes, nodeId);

  Value nodeStart = AC->create<arith::MulIOp>(loc, nodeChunkStart, blockSize);
  Value nodeMaxRows = AC->create<arith::MulIOp>(loc, nodeChunkCount, blockSize);

  Value needZeroNode = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::uge,
                                                 nodeStart, totalIterations);
  Value nodeRemaining =
      AC->create<arith::SubIOp>(loc, totalIterations, nodeStart);
  Value nodeRemainingNonNeg =
      AC->create<arith::SelectOp>(loc, needZeroNode, zeroIndex, nodeRemaining);
  Value nodeRows =
      AC->create<arith::MinUIOp>(loc, nodeMaxRows, nodeRemainingNonNeg);

  bounds.acquireStart = nodeStart;
  bounds.acquireSize = nodeRows;
  bounds.acquireSizeHint = nodeMaxRows;

  /// Level 2: subdivide node rows across local threads
  Value oneIndex = AC->createIndexConstant(1, loc);
  Value adjusted = AC->create<arith::AddIOp>(
      loc, nodeRows,
      AC->create<arith::SubIOp>(loc, runtimeWorkersPerNode, oneIndex));
  Value subBlock =
      AC->create<arith::DivUIOp>(loc, adjusted, runtimeWorkersPerNode);

  Value localStart = AC->create<arith::MulIOp>(loc, localThreadId, subBlock);

  Value needZeroLocal = AC->create<arith::CmpIOp>(
      loc, arith::CmpIPredicate::uge, localStart, nodeRows);
  Value localRemaining = AC->create<arith::SubIOp>(loc, nodeRows, localStart);
  Value localRemainingNonNeg = AC->create<arith::SelectOp>(
      loc, needZeroLocal, zeroIndex, localRemaining);
  Value localCount =
      AC->create<arith::MinUIOp>(loc, subBlock, localRemainingNonNeg);

  bounds.iterStart = AC->create<arith::AddIOp>(loc, nodeStart, localStart);
  bounds.iterCount = localCount;
  bounds.iterCountHint = subBlock;

  bounds.hasWork = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ugt,
                                             localCount, zeroIndex);

  return bounds;
}

///===----------------------------------------------------------------------===///
/// H2.3: Recompute Bounds Inside Task EDT
///===----------------------------------------------------------------------===///

DistributionBounds DistributionHeuristics::recomputeBoundsInside(
    ArtsCodegen *AC, Location loc, const DistributionStrategy &strategy,
    Value workerId, Value insideTotalWorkers, Value workersPerNode,
    Value upperBound, Value lowerBound, Value loopStep, Value blockSize,
    std::optional<int64_t> alignmentBlockSize, bool useRuntimeBlockAlignment) {

  Value workerIdIndex = AC->castToIndex(workerId, loc);
  Value totalWorkersIndex = AC->castToIndex(insideTotalWorkers, loc);

  Value oneIndex = AC->createIndexConstant(1, loc);
  Value zeroIndex = AC->createIndexConstant(0, loc);

  /// Clone loop bounds and their dependencies into the current region
  SetVector<Value> boundsToClone;
  Region *currentRegion = AC->getBuilder().getInsertionBlock()->getParent();

  std::function<void(Value)> collectWithDeps = [&](Value val) {
    if (boundsToClone.contains(val))
      return;
    if (Operation *defOp = val.getDefiningOp()) {
      if (!currentRegion->isAncestor(defOp->getParentRegion())) {
        for (Value operand : defOp->getOperands())
          collectWithDeps(operand);
        boundsToClone.insert(val);
      }
    }
  };

  collectWithDeps(upperBound);
  collectWithDeps(lowerBound);
  collectWithDeps(loopStep);
  collectWithDeps(blockSize);

  IRMapping boundsMapper;
  ValueAnalysis::cloneValuesIntoRegion(
      boundsToClone, currentRegion, boundsMapper, AC->getBuilder(),
      /*allowMemoryEffectFree=*/true, isSafeRuntimeQuery);

  Value localUpperBound = boundsMapper.lookupOrDefault(upperBound);
  Value localLowerBound = boundsMapper.lookupOrDefault(lowerBound);
  Value localLoopStep = boundsMapper.lookupOrDefault(loopStep);
  Value localBlockSize = boundsMapper.lookupOrDefault(blockSize);

  /// Align lower bound to block boundary if needed
  Value chunkLowerBound = localLowerBound;
  if (alignmentBlockSize) {
    if (auto lbConst = ValueAnalysis::tryFoldConstantIndex(localLowerBound)) {
      int64_t aligned = *lbConst - (*lbConst % *alignmentBlockSize);
      if (aligned != *lbConst) {
        chunkLowerBound = AC->createIndexConstant(aligned, loc);
      }
    }
  } else if (useRuntimeBlockAlignment) {
    Value rem =
        AC->create<arith::RemUIOp>(loc, localLowerBound, localBlockSize);
    chunkLowerBound = AC->create<arith::SubIOp>(loc, localLowerBound, rem);
  }

  Value range =
      AC->create<arith::SubIOp>(loc, localUpperBound, chunkLowerBound);
  Value adjustedRange = AC->create<arith::AddIOp>(
      loc, range, AC->create<arith::SubIOp>(loc, localLoopStep, oneIndex));
  Value localTotalIterations =
      AC->create<arith::DivUIOp>(loc, adjustedRange, localLoopStep);

  Value adjustedIterations = AC->create<arith::AddIOp>(
      loc, localTotalIterations,
      AC->create<arith::SubIOp>(loc, localBlockSize, oneIndex));
  Value localTotalChunks =
      AC->create<arith::DivUIOp>(loc, adjustedIterations, localBlockSize);

  if (strategy.kind == DistributionKind::BlockCyclic) {
    DistributionBounds bounds;
    bounds.iterStart = zeroIndex;
    bounds.iterCount = zeroIndex;
    bounds.iterCountHint = zeroIndex;
    bounds.hasWork = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ult,
                                               workerIdIndex, localTotalChunks);
    bounds.acquireStart = zeroIndex;
    bounds.acquireSize = localTotalIterations;
    bounds.acquireSizeHint = localTotalIterations;
    return bounds;
  }

  if (strategy.kind == DistributionKind::TwoLevel) {
    /// Clone workersPerNode into the current region if needed
    Value wpnIndex = workersPerNode;
    if (wpnIndex) {
      if (Operation *defOp = wpnIndex.getDefiningOp()) {
        if (!currentRegion->isAncestor(defOp->getParentRegion())) {
          collectWithDeps(wpnIndex);
          ValueAnalysis::cloneValuesIntoRegion(
              boundsToClone, currentRegion, boundsMapper, AC->getBuilder(),
              /*allowMemoryEffectFree=*/true, isSafeRuntimeQuery);
          wpnIndex = boundsMapper.lookupOrDefault(wpnIndex);
        }
      }
      wpnIndex = AC->castToIndex(wpnIndex, loc);
    }

    Value numNodes =
        AC->create<arith::DivUIOp>(loc, totalWorkersIndex, wpnIndex);
    Value nodeId = AC->create<arith::DivUIOp>(loc, workerIdIndex, wpnIndex);
    Value localThreadId =
        AC->create<arith::RemUIOp>(loc, workerIdIndex, wpnIndex);

    /// Level 1: distribute chunks across nodes
    auto [nodeChunkStart, nodeChunkCount] =
        balancedDistribute(AC, loc, localTotalChunks, numNodes, nodeId);

    Value nodeStart =
        AC->create<arith::MulIOp>(loc, nodeChunkStart, localBlockSize);
    Value nodeMaxRows =
        AC->create<arith::MulIOp>(loc, nodeChunkCount, localBlockSize);

    Value needZeroNode = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::uge, nodeStart, localTotalIterations);
    Value nodeRemaining =
        AC->create<arith::SubIOp>(loc, localTotalIterations, nodeStart);
    Value nodeRemainingNonNeg = AC->create<arith::SelectOp>(
        loc, needZeroNode, zeroIndex, nodeRemaining);
    Value nodeRows =
        AC->create<arith::MinUIOp>(loc, nodeMaxRows, nodeRemainingNonNeg);

    /// Level 2: subdivide node rows across local threads
    Value subAdjusted = AC->create<arith::AddIOp>(
        loc, nodeRows, AC->create<arith::SubIOp>(loc, wpnIndex, oneIndex));
    Value subBlock = AC->create<arith::DivUIOp>(loc, subAdjusted, wpnIndex);

    Value localStart = AC->create<arith::MulIOp>(loc, localThreadId, subBlock);

    Value needZeroLocal = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::uge, localStart, nodeRows);
    Value localRemaining = AC->create<arith::SubIOp>(loc, nodeRows, localStart);
    Value localRemainingNonNeg = AC->create<arith::SelectOp>(
        loc, needZeroLocal, zeroIndex, localRemaining);
    Value localCount =
        AC->create<arith::MinUIOp>(loc, subBlock, localRemainingNonNeg);

    DistributionBounds bounds;
    bounds.iterStart = AC->create<arith::AddIOp>(loc, nodeStart, localStart);
    bounds.iterCount = localCount;
    bounds.iterCountHint = subBlock;
    bounds.hasWork = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ugt,
                                               localCount, zeroIndex);

    bounds.acquireStart = nodeStart;
    bounds.acquireSize = nodeRows;
    bounds.acquireSizeHint = nodeMaxRows;
    return bounds;
  }

  /// Flat: mirror the same balanced floor + remainder mapping.
  /// Tiling2D: use row-worker decomposition for row ownership.
  Value distWorkerId = workerIdIndex;
  Value distWorkers = totalWorkersIndex;
  if (strategy.kind == DistributionKind::Tiling2D) {
    Tiling2DWorkerGrid grid = getTiling2DWorkerGrid(
        AC, loc, workerIdIndex, totalWorkersIndex, workersPerNode);
    distWorkerId = grid.rowWorkerId;
    distWorkers = grid.rowWorkers;
  }

  auto [firstChunkIdx, chunkCount] =
      balancedDistribute(AC, loc, localTotalChunks, distWorkers, distWorkerId);

  DistributionBounds bounds;
  bounds.iterStart =
      AC->create<arith::MulIOp>(loc, firstChunkIdx, localBlockSize);
  bounds.iterCountHint =
      AC->create<arith::MulIOp>(loc, chunkCount, localBlockSize);

  Value needZeroIters = AC->create<arith::CmpIOp>(
      loc, arith::CmpIPredicate::uge, bounds.iterStart, localTotalIterations);
  Value remainingIters =
      AC->create<arith::SubIOp>(loc, localTotalIterations, bounds.iterStart);
  Value remainingItersNonNeg = AC->create<arith::SelectOp>(
      loc, needZeroIters, zeroIndex, remainingIters);
  bounds.iterCount = AC->create<arith::MinUIOp>(loc, bounds.iterCountHint,
                                                remainingItersNonNeg);
  bounds.hasWork = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ugt,
                                             bounds.iterCount, zeroIndex);

  bounds.acquireStart = bounds.iterStart;
  bounds.acquireSize = bounds.iterCount;
  bounds.acquireSizeHint = bounds.iterCountHint;
  return bounds;
}

///===----------------------------------------------------------------------===///
/// DB Alignment Block Size
///===----------------------------------------------------------------------===///

Value DistributionHeuristics::computeDbAlignmentBlockSize(EdtOp parallelEdt,
                                                          Value numPartitions,
                                                          ArtsCodegen *AC,
                                                          Location loc) {
  if (!parallelEdt || !numPartitions)
    return Value();

  Value one = AC->createIndexConstant(1, loc);
  Value hintBlockSize;

  bool isTwoLevel = (parallelEdt.getConcurrency() == EdtConcurrency::internode);

  for (Value dep : parallelEdt.getDependencies()) {
    auto allocInfo = DbUtils::traceToDbAlloc(dep);
    if (!allocInfo)
      continue;
    auto [rootGuid, rootPtr] = *allocInfo;
    auto allocOp = rootGuid.getDefiningOp<DbAllocOp>();
    if (!allocOp || allocOp.getElementSizes().empty())
      continue;

    Value elemSize = allocOp.getElementSizes().front();
    if (auto constElem = ValueAnalysis::tryFoldConstantIndex(elemSize))
      if (*constElem <= 1)
        continue;

    /// TwoLevel (internode): align ALL arrays so all workers on the same node
    /// acquire the same DB block.
    ///
    /// Flat (intranode):
    ///   - Always align stencil arrays.
    ///   - Align write-capable arrays only when element count is not evenly
    ///     divisible by worker count. This is the mismatch case where
    ///     floor+remainder iteration slices can straddle DB block boundaries
    ///     and create unnecessary cross-chunk dependencies.
    if (!isTwoLevel) {
      auto pattern = getDbAccessPattern(allocOp.getOperation());
      bool isStencil = pattern && *pattern == DbAccessPattern::stencil;

      bool needsWriteAlignment = false;
      if (allocOp.getMode() != ArtsMode::in) {
        auto elemConst = ValueAnalysis::tryFoldConstantIndex(elemSize);
        auto partConst = ValueAnalysis::tryFoldConstantIndex(numPartitions);
        if (elemConst && partConst && *partConst > 0) {
          needsWriteAlignment = (*elemConst % *partConst) != 0;
        } else {
          /// Be conservative for dynamic sizes/partition counts.
          needsWriteAlignment = true;
        }
      }

      if (!isStencil && !needsWriteAlignment)
        continue;
    }

    elemSize = AC->castToIndex(elemSize, loc);

    /// ceil(elemSize / numPartitions) to match DbPartitioning block size
    Value adjusted = AC->create<arith::AddIOp>(
        loc, elemSize, AC->create<arith::SubIOp>(loc, numPartitions, one));
    Value candidate = AC->create<arith::DivUIOp>(loc, adjusted, numPartitions);
    candidate = AC->create<arith::MaxUIOp>(loc, candidate, one);

    if (!hintBlockSize)
      hintBlockSize = candidate;
    else
      hintBlockSize = AC->create<arith::MinUIOp>(loc, hintBlockSize, candidate);
  }

  return hintBlockSize;
}

///===----------------------------------------------------------------------===///
/// Workers Per Node
///===----------------------------------------------------------------------===///

/// Cast a Value to index type if needed. Returns null Value for null input.
static Value castToIndexType(OpBuilder &builder, Location loc, Value v) {
  if (!v)
    return Value();
  if (v.getType().isIndex())
    return v;
  return builder.create<arith::IndexCastOp>(loc, builder.getIndexType(), v);
}

Value DistributionHeuristics::getWorkersPerNode(OpBuilder &builder,
                                                Location loc,
                                                EdtOp parallelEdt) {
  Value one = arts::createOneIndex(builder, loc);
  Value workersPerNode;

  if (auto wpnConst = getExplicitWorkersPerNodeCount(parallelEdt)) {
    int64_t clamped = std::max<int64_t>(1, *wpnConst);
    return arts::createConstantIndex(builder, loc, clamped);
  }

  if (auto workers = getExplicitWorkerCount(parallelEdt)) {
    Value totalWorkers = arts::createConstantIndex(builder, loc, *workers);
    Value totalNodes = castToIndexType(
        builder, loc,
        builder.create<RuntimeQueryOp>(loc, RuntimeQueryKind::totalNodes)
            .getResult());
    workersPerNode =
        builder.create<arith::DivUIOp>(loc, totalWorkers, totalNodes);
  } else {
    workersPerNode = castToIndexType(
        builder, loc,
        builder.create<RuntimeQueryOp>(loc, RuntimeQueryKind::totalWorkers)
            .getResult());
  }

  /// Clamp to at least 1 to prevent division by zero.
  Value zero = arts::createZeroIndex(builder, loc);
  Value isZero = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::eq,
                                               workersPerNode, zero);
  return builder.create<arith::SelectOp>(loc, isZero, one, workersPerNode);
}

Value DistributionHeuristics::getWorkersPerNode(ArtsCodegen *AC, Location loc,
                                                EdtOp parallelEdt) {
  return getWorkersPerNode(AC->getBuilder(), loc, parallelEdt);
}

Value DistributionHeuristics::getTotalWorkers(OpBuilder &builder, Location loc,
                                              EdtOp parallelEdt) {
  if (auto workers = getExplicitWorkerCount(parallelEdt))
    return arts::createConstantIndex(builder, loc, *workers);

  if (parallelEdt.getConcurrency() == EdtConcurrency::internode) {
    Value nodes = castToIndexType(
        builder, loc,
        builder.create<RuntimeQueryOp>(loc, RuntimeQueryKind::totalNodes)
            .getResult());
    Value threads = castToIndexType(
        builder, loc,
        builder.create<RuntimeQueryOp>(loc, RuntimeQueryKind::totalWorkers)
            .getResult());
    return builder.create<arith::MulIOp>(loc, nodes, threads);
  }

  return castToIndexType(
      builder, loc,
      builder.create<RuntimeQueryOp>(loc, RuntimeQueryKind::totalWorkers)
          .getResult());
}

Value DistributionHeuristics::getTotalWorkers(ArtsCodegen *AC, Location loc,
                                              EdtOp parallelEdt) {
  return getTotalWorkers(AC->getBuilder(), loc, parallelEdt);
}

Value DistributionHeuristics::getDispatchWorkerCount(OpBuilder &builder,
                                                     Location loc,
                                                     EdtOp parallelEdt) {
  if (auto workers = getExplicitWorkerCount(parallelEdt))
    return arts::createConstantIndex(builder, loc, *workers);

  RuntimeQueryKind queryKind =
      parallelEdt.getConcurrency() == EdtConcurrency::internode
          ? RuntimeQueryKind::totalNodes
          : RuntimeQueryKind::totalWorkers;
  Value workerCount =
      builder.create<RuntimeQueryOp>(loc, queryKind).getResult();
  return castToIndexType(builder, loc, workerCount);
}

Value DistributionHeuristics::getForDispatchWorkerCount(
    ArtsCodegen *AC, Location loc, EdtOp parallelEdt,
    const DistributionStrategy &strategy, Value totalChunks) {
  Value one = AC->createIndexConstant(1, loc);
  Value totalWorkers = getTotalWorkers(AC, loc, parallelEdt);
  Value clampedDispatch = totalWorkers;

  switch (strategy.kind) {
  case DistributionKind::Flat:
  case DistributionKind::BlockCyclic:
    clampedDispatch =
        AC->create<arith::MinUIOp>(loc, totalWorkers, totalChunks);
    break;
  case DistributionKind::TwoLevel: {
    Value workersPerNode = getWorkersPerNode(AC, loc, parallelEdt);
    Value numNodes =
        AC->create<arith::DivUIOp>(loc, totalWorkers, workersPerNode);
    Value activeNodes = AC->create<arith::MinUIOp>(loc, numNodes, totalChunks);
    clampedDispatch =
        AC->create<arith::MulIOp>(loc, activeNodes, workersPerNode);
    break;
  }
  case DistributionKind::Tiling2D: {
    Value totalWorkersIndex = AC->castToIndex(totalWorkers, loc);
    auto totalWorkersConst =
        ValueAnalysis::tryFoldConstantIndex(totalWorkersIndex);
    if (!totalWorkersConst)
      break;

    std::optional<int64_t> workersPerNodeConst = std::nullopt;
    if (parallelEdt.getConcurrency() == EdtConcurrency::internode) {
      Value workersPerNode = getWorkersPerNode(AC, loc, parallelEdt);
      workersPerNodeConst = ValueAnalysis::tryFoldConstantIndex(
          AC->castToIndex(workersPerNode, loc));
    }

    int64_t colWorkersConst =
        chooseTiling2DColumnWorkers(*totalWorkersConst, workersPerNodeConst);
    int64_t rowWorkersConst =
        std::max<int64_t>(1, *totalWorkersConst / colWorkersConst);

    Value rowWorkers = AC->createIndexConstant(rowWorkersConst, loc);
    Value activeRows = AC->create<arith::MinUIOp>(loc, rowWorkers, totalChunks);
    Value colWorkers = AC->createIndexConstant(colWorkersConst, loc);
    clampedDispatch = AC->create<arith::MulIOp>(loc, activeRows, colWorkers);
    break;
  }
  }

  return AC->create<arith::MaxUIOp>(loc, clampedDispatch, one);
}
