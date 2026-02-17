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

#include "arts/Analysis/DistributionHeuristics.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/Builders.h"
#include <algorithm>

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Helpers
///===----------------------------------------------------------------------===///

/// Check if an operation can be safely cloned into a new region.
/// ARTS runtime query ops are safe to clone - they return the same value
/// regardless of where they are emitted.
static bool isSafeRuntimeQuery(Operation *op) {
  return isa<GetTotalNodesOp, GetTotalWorkersOp>(op);
}

static bool shouldSkipCoarsening(ForOp forOp, LoopNode *loopNode) {
  if (loopNode) {
    if (loopNode->hasInterIterationDeps && *loopNode->hasInterIterationDeps)
      return true;
    if (loopNode->parallelClassification &&
        *loopNode->parallelClassification ==
            LoopMetadata::ParallelClassification::Sequential)
      return true;
  }

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

std::optional<WorkerConfig> DistributionHeuristics::resolveWorkerConfig(
    EdtOp parallelEdt, const ArtsAbstractMachine *machine) {
  WorkerConfig cfg;
  cfg.internode = parallelEdt.getConcurrency() == EdtConcurrency::internode;

  if (auto workers = parallelEdt->getAttrOfType<workersAttr>(
          AttrNames::Operation::Workers)) {
    cfg.totalWorkers = workers.getValue();
    if (cfg.totalWorkers <= 0)
      return std::nullopt;

    if (cfg.internode) {
      if (machine && machine->hasValidNodeCount() &&
          machine->getNodeCount() > 0)
        cfg.workersPerNode = std::max<int64_t>(
            1,
            cfg.totalWorkers / static_cast<int64_t>(machine->getNodeCount()));
      if (cfg.workersPerNode <= 0)
        cfg.workersPerNode = 1;
    } else {
      cfg.workersPerNode = cfg.totalWorkers;
    }
    return cfg;
  }

  if (!machine)
    return std::nullopt;

  int threads = machine->hasValidThreads() ? machine->getThreads() : 0;
  if (threads <= 0)
    return std::nullopt;

  if (!cfg.internode) {
    cfg.totalWorkers = threads;
    cfg.workersPerNode = threads;
    return cfg;
  }

  int nodeCount = machine->hasValidNodeCount() ? machine->getNodeCount() : 0;
  if (nodeCount <= 0)
    return std::nullopt;

  int workerThreads =
      threads - machine->getOutgoingThreads() - machine->getIncomingThreads();
  if (workerThreads <= 0)
    workerThreads = 1;

  cfg.workersPerNode = workerThreads;
  cfg.totalWorkers = static_cast<int64_t>(nodeCount) * workerThreads;
  if (cfg.totalWorkers <= 0)
    return std::nullopt;

  return cfg;
}

std::optional<int64_t> DistributionHeuristics::computeCoarsenedBlockHint(
    ForOp forOp, LoopAnalysis &loopAnalysis, const WorkerConfig &workerCfg) {
  if (workerCfg.totalWorkers <= 0)
    return std::nullopt;

  LoopNode *loopNode = loopAnalysis.getOrCreateLoopNode(forOp.getOperation());
  if (shouldSkipCoarsening(forOp, loopNode))
    return std::nullopt;

  auto tripOpt = loopAnalysis.getStaticTripCount(forOp.getOperation());
  if (!tripOpt)
    return std::nullopt;

  int64_t tripCount = *tripOpt;
  if (tripCount <= 0)
    return std::nullopt;

  constexpr int64_t kMinItersPerWorker = 8;
  if (tripCount >= workerCfg.totalWorkers * kMinItersPerWorker)
    return std::nullopt;

  int64_t desiredWorkers = std::max<int64_t>(1, tripCount / kMinItersPerWorker);
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

int64_t
DistributionHeuristics::chooseTiling2DColumnWorkers(int64_t totalWorkers) {
  if (totalWorkers < 4)
    return 1;

  int64_t best = 1;
  for (int64_t d = 2; d * d <= totalWorkers; ++d) {
    if (totalWorkers % d == 0)
      best = d;
  }
  return best;
}

Tiling2DWorkerGrid DistributionHeuristics::getTiling2DWorkerGrid(
    ArtsCodegen *AC, Location loc, Value workerId, Value totalWorkers) {
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
          ValueUtils::tryFoldConstantIndex(totalWorkersIndex)) {
    int64_t colWorkersConst = chooseTiling2DColumnWorkers(*totalWorkersConst);
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
                                        const ArtsAbstractMachine *machine) {
  DistributionStrategy strategy;

  if (concurrency == EdtConcurrency::intranode) {
    /// Flat: all workers divide iterations equally
    strategy.kind = DistributionKind::Flat;
    strategy.numNodes = 1;
    if (machine) {
      strategy.workersPerNode = machine->getThreads();
      strategy.totalWorkers = machine->getThreads();
    }
    strategy.useDbAlignment = false;
    return strategy;
  }

  /// Internode -> TwoLevel
  strategy.kind = DistributionKind::TwoLevel;
  if (machine) {
    strategy.numNodes = machine->getNodeCount();
    int threads = machine->getThreads();
    int outgoing = machine->getOutgoingThreads();
    int incoming = machine->getIncomingThreads();
    strategy.workersPerNode = std::max(1, threads - outgoing - incoming);
    strategy.totalWorkers = strategy.numNodes * strategy.workersPerNode;
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
      Tiling2DWorkerGrid grid =
          getTiling2DWorkerGrid(AC, loc, workerId, runtimeTotalWorkers);
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
  ValueUtils::cloneValuesIntoRegion(
      boundsToClone, currentRegion, boundsMapper, AC->getBuilder(),
      /*allowMemoryEffectFree=*/true, isSafeRuntimeQuery);

  Value localUpperBound = boundsMapper.lookupOrDefault(upperBound);
  Value localLowerBound = boundsMapper.lookupOrDefault(lowerBound);
  Value localLoopStep = boundsMapper.lookupOrDefault(loopStep);
  Value localBlockSize = boundsMapper.lookupOrDefault(blockSize);

  /// Align lower bound to block boundary if needed
  Value chunkLowerBound = localLowerBound;
  if (alignmentBlockSize) {
    if (auto lbConst = ValueUtils::tryFoldConstantIndex(localLowerBound)) {
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
          ValueUtils::cloneValuesIntoRegion(
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
    Tiling2DWorkerGrid grid =
        getTiling2DWorkerGrid(AC, loc, workerIdIndex, totalWorkersIndex);
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
    auto allocInfo = DatablockUtils::traceToDbAlloc(dep);
    if (!allocInfo)
      continue;
    auto [rootGuid, rootPtr] = *allocInfo;
    auto allocOp = rootGuid.getDefiningOp<DbAllocOp>();
    if (!allocOp || allocOp.getElementSizes().empty())
      continue;

    Value elemSize = allocOp.getElementSizes().front();
    if (auto constElem = ValueUtils::tryFoldConstantIndex(elemSize))
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
        auto elemConst = ValueUtils::tryFoldConstantIndex(elemSize);
        auto partConst = ValueUtils::tryFoldConstantIndex(numPartitions);
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

Value DistributionHeuristics::getWorkersPerNode(ArtsCodegen *AC, Location loc,
                                                EdtOp parallelEdt) {
  Value workersPerNode;
  Value one = AC->createIndexConstant(1, loc);

  if (auto workers = parallelEdt->getAttrOfType<workersAttr>(
          AttrNames::Operation::Workers)) {
    Value totalWorkers = AC->createIndexConstant(workers.getValue(), loc);
    Value totalNodes =
        AC->castToIndex(AC->create<GetTotalNodesOp>(loc).getResult(), loc);
    workersPerNode = AC->create<arith::DivUIOp>(loc, totalWorkers, totalNodes);
  } else {
    workersPerNode =
        AC->castToIndex(AC->create<GetTotalWorkersOp>(loc).getResult(), loc);
  }

  /// Clamp to at least 1 to prevent division by zero
  Value zero = AC->createIndexConstant(0, loc);
  Value isZero = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::eq,
                                           workersPerNode, zero);
  return AC->create<arith::SelectOp>(loc, isZero, one, workersPerNode);
}

Value DistributionHeuristics::getTotalWorkers(ArtsCodegen *AC, Location loc,
                                              EdtOp parallelEdt) {
  if (auto workers = parallelEdt->getAttrOfType<workersAttr>(
          AttrNames::Operation::Workers)) {
    return AC->createIndexConstant(workers.getValue(), loc);
  }

  if (parallelEdt.getConcurrency() == EdtConcurrency::internode) {
    Value nodes =
        AC->castToIndex(AC->create<GetTotalNodesOp>(loc).getResult(), loc);
    Value threads =
        AC->castToIndex(AC->create<GetTotalWorkersOp>(loc).getResult(), loc);
    return AC->create<arith::MulIOp>(loc, nodes, threads);
  }

  return AC->castToIndex(AC->create<GetTotalWorkersOp>(loc).getResult(), loc);
}

Value DistributionHeuristics::getDispatchWorkerCount(OpBuilder &builder,
                                                     Location loc,
                                                     EdtOp parallelEdt) {
  if (auto workers = parallelEdt->getAttrOfType<workersAttr>(
          AttrNames::Operation::Workers)) {
    return builder.create<arith::ConstantIndexOp>(loc, workers.getValue());
  }

  Value workerCount;
  if (parallelEdt.getConcurrency() == EdtConcurrency::internode) {
    workerCount = builder.create<GetTotalNodesOp>(loc).getResult();
  } else {
    workerCount = builder.create<GetTotalWorkersOp>(loc).getResult();
  }
  if (workerCount.getType().isIndex())
    return workerCount;
  return builder.create<arith::IndexCastOp>(loc, builder.getIndexType(),
                                            workerCount);
}
