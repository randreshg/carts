///==========================================================================///
/// File: DistributionHeuristics.cpp
///
/// H2 distribution heuristics: machine-aware work distribution.
///
/// Implements two distribution strategies:
///   - Flat: all workers divide iterations equally (intranode)
///   - TwoLevel: nodes get DB blocks, threads subdivide within (internode)
///==========================================================================///

#include "arts/Analysis/DistributionHeuristics.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/Builders.h"

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Helpers
///===----------------------------------------------------------------------===///

static Value castToIndex(ArtsCodegen *AC, Value v, Location loc) {
  if (!v)
    return v;
  if (v.getType().isIndex())
    return v;
  auto indexTy = AC->getBuilder().getIndexType();
  return AC->create<arith::IndexCastOp>(loc, indexTy, v);
}

/// Check if an operation can be safely cloned into a new region.
/// ARTS runtime query ops are safe to clone - they return the same value
/// regardless of where they are emitted.
static bool isSafeRuntimeQuery(Operation *op) {
  return isa<GetTotalNodesOp, GetTotalWorkersOp>(op);
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

///===----------------------------------------------------------------------===///
/// Balanced Distribution Helper
///===----------------------------------------------------------------------===///

std::pair<Value, Value>
DistributionHeuristics::balancedDistribute(ArtsCodegen *AC, Location loc,
                                           Value totalChunks,
                                           Value numParticipants,
                                           Value participantId) {
  Value oneIndex = AC->createIndexConstant(1, loc);

  /// base = totalChunks / numParticipants
  Value base = AC->create<arith::DivUIOp>(loc, totalChunks, numParticipants);
  /// rem = totalChunks % numParticipants
  Value rem = AC->create<arith::RemUIOp>(loc, totalChunks, numParticipants);

  /// count = base + (participantId < rem ? 1 : 0)
  Value getsExtra = AC->create<arith::CmpIOp>(
      loc, arith::CmpIPredicate::ult, participantId, rem);
  Value count = AC->create<arith::SelectOp>(
      loc, getsExtra,
      AC->create<arith::AddIOp>(loc, base, oneIndex), base);

  /// start = participantId * base + min(participantId, rem)
  Value clipped = AC->create<arith::MinUIOp>(loc, participantId, rem);
  Value start = AC->create<arith::AddIOp>(
      loc, AC->create<arith::MulIOp>(loc, participantId, base), clipped);

  return {start, count};
}

///===----------------------------------------------------------------------===///
/// H2.2: Compute Runtime Bounds
///===----------------------------------------------------------------------===///

DistributionBounds
DistributionHeuristics::computeBounds(ArtsCodegen *AC, Location loc,
                                      const DistributionStrategy &strategy,
                                      Value workerId,
                                      Value runtimeTotalWorkers,
                                      Value runtimeWorkersPerNode,
                                      Value totalIterations,
                                      Value totalChunks,
                                      Value blockSize) {
  DistributionBounds bounds;
  Value zeroIndex = AC->createIndexConstant(0, loc);

  if (strategy.kind == DistributionKind::Flat) {
    /// Flat: balanced distribution across all workers
    auto [firstChunkIdx, chunkCount] =
        balancedDistribute(AC, loc, totalChunks, runtimeTotalWorkers,
                           workerId);

    /// Convert chunk indices to iteration indices
    bounds.iterStart =
        AC->create<arith::MulIOp>(loc, firstChunkIdx, blockSize);
    bounds.iterCountHint =
        AC->create<arith::MulIOp>(loc, chunkCount, blockSize);

    /// Handle edge case: last worker's iterations may exceed totalIterations
    Value needZeroIters = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::uge, bounds.iterStart, totalIterations);
    Value remainingIters =
        AC->create<arith::SubIOp>(loc, totalIterations, bounds.iterStart);
    Value remainingItersNonNeg = AC->create<arith::SelectOp>(
        loc, needZeroIters, zeroIndex, remainingIters);
    bounds.iterCount = AC->create<arith::MinUIOp>(loc, bounds.iterCountHint,
                                                  remainingItersNonNeg);

    /// hasWork = iterCount > 0
    bounds.hasWork = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::ugt, bounds.iterCount, zeroIndex);

    /// For Flat, acquire bounds = iteration bounds
    bounds.acquireStart = bounds.iterStart;
    bounds.acquireSize = bounds.iterCount;
    bounds.acquireSizeHint = bounds.iterCountHint;

    return bounds;
  }

  /// TwoLevel: two-level hierarchical distribution
  /// Derive numNodes from runtime values: numNodes = totalWorkers / workersPerNode
  Value numNodes = AC->create<arith::DivUIOp>(loc, runtimeTotalWorkers,
                                              runtimeWorkersPerNode);

  /// Derive nodeId and localThreadId from global workerId
  Value nodeId = AC->create<arith::DivUIOp>(loc, workerId,
                                            runtimeWorkersPerNode);
  Value localThreadId =
      AC->create<arith::RemUIOp>(loc, workerId, runtimeWorkersPerNode);

  /// Level 1: chunks -> nodes (balanced)
  auto [nodeChunkStart, nodeChunkCount] =
      balancedDistribute(AC, loc, totalChunks, numNodes, nodeId);

  Value nodeStart =
      AC->create<arith::MulIOp>(loc, nodeChunkStart, blockSize);
  Value nodeMaxRows =
      AC->create<arith::MulIOp>(loc, nodeChunkCount, blockSize);

  /// Clamp to remaining iterations
  Value needZeroNode = AC->create<arith::CmpIOp>(
      loc, arith::CmpIPredicate::uge, nodeStart, totalIterations);
  Value nodeRemaining =
      AC->create<arith::SubIOp>(loc, totalIterations, nodeStart);
  Value nodeRemainingNonNeg = AC->create<arith::SelectOp>(
      loc, needZeroNode, zeroIndex, nodeRemaining);
  Value nodeRows = AC->create<arith::MinUIOp>(loc, nodeMaxRows,
                                              nodeRemainingNonNeg);

  /// Acquire bounds = node-level
  bounds.acquireStart = nodeStart;
  bounds.acquireSize = nodeRows;
  bounds.acquireSizeHint = nodeMaxRows;

  /// Level 2: node rows -> local threads (sub-block)
  Value oneIndex = AC->createIndexConstant(1, loc);
  /// subBlock = ceil(nodeRows / workersPerNode)
  Value adjusted = AC->create<arith::AddIOp>(
      loc, nodeRows,
      AC->create<arith::SubIOp>(loc, runtimeWorkersPerNode, oneIndex));
  Value subBlock =
      AC->create<arith::DivUIOp>(loc, adjusted, runtimeWorkersPerNode);

  /// localStart = localThreadId * subBlock
  Value localStart =
      AC->create<arith::MulIOp>(loc, localThreadId, subBlock);

  /// localCount = min(subBlock, max(0, nodeRows - localStart))
  Value needZeroLocal = AC->create<arith::CmpIOp>(
      loc, arith::CmpIPredicate::uge, localStart, nodeRows);
  Value localRemaining =
      AC->create<arith::SubIOp>(loc, nodeRows, localStart);
  Value localRemainingNonNeg = AC->create<arith::SelectOp>(
      loc, needZeroLocal, zeroIndex, localRemaining);
  Value localCount = AC->create<arith::MinUIOp>(loc, subBlock,
                                                localRemainingNonNeg);

  /// iterStart = nodeStart + localStart (global offset)
  bounds.iterStart =
      AC->create<arith::AddIOp>(loc, nodeStart, localStart);
  bounds.iterCount = localCount;
  bounds.iterCountHint = subBlock;

  /// hasWork = localCount > 0
  bounds.hasWork = AC->create<arith::CmpIOp>(
      loc, arith::CmpIPredicate::ugt, localCount, zeroIndex);

  return bounds;
}

///===----------------------------------------------------------------------===///
/// H2.3: Recompute Bounds Inside Task EDT
///===----------------------------------------------------------------------===///

DistributionBounds DistributionHeuristics::recomputeBoundsInside(
    ArtsCodegen *AC, Location loc,
    const DistributionStrategy &strategy,
    Value workerId, Value insideTotalWorkers,
    Value workersPerNode,
    Value upperBound, Value lowerBound, Value loopStep,
    Value blockSize,
    std::optional<int64_t> alignmentBlockSize,
    bool useRuntimeBlockAlignment) {

  /// Cast worker values to index type
  Type indexType = AC->getBuilder().getIndexType();
  Value workerIdIndex =
      workerId.getType().isIndex()
          ? workerId
          : AC->create<arith::IndexCastOp>(loc, indexType, workerId);
  Value totalWorkersIndex =
      insideTotalWorkers.getType().isIndex()
          ? insideTotalWorkers
          : AC->create<arith::IndexCastOp>(loc, indexType, insideTotalWorkers);

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
  ValueUtils::cloneValuesIntoRegion(boundsToClone, currentRegion, boundsMapper,
                                    AC->getBuilder(),
                                    /*allowMemoryEffectFree=*/true,
                                    isSafeRuntimeQuery);

  Value localUpperBound = boundsMapper.lookupOrDefault(upperBound);
  Value localLowerBound = boundsMapper.lookupOrDefault(lowerBound);
  Value localLoopStep = boundsMapper.lookupOrDefault(loopStep);
  Value localBlockSize = boundsMapper.lookupOrDefault(blockSize);

  /// Compute aligned chunk lower bound
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

  /// totalIterations = ceil((upper - lower) / step)
  Value range =
      AC->create<arith::SubIOp>(loc, localUpperBound, chunkLowerBound);
  Value adjustedRange = AC->create<arith::AddIOp>(
      loc, range, AC->create<arith::SubIOp>(loc, localLoopStep, oneIndex));
  Value localTotalIterations =
      AC->create<arith::DivUIOp>(loc, adjustedRange, localLoopStep);

  /// totalChunks = ceil(totalIterations / blockSize)
  Value adjustedIterations = AC->create<arith::AddIOp>(
      loc, localTotalIterations,
      AC->create<arith::SubIOp>(loc, localBlockSize, oneIndex));
  Value localTotalChunks =
      AC->create<arith::DivUIOp>(loc, adjustedIterations, localBlockSize);

  if (strategy.kind == DistributionKind::TwoLevel) {
    /// TwoLevel: replicate the two-level logic using runtime values
    /// Clone workersPerNode (and its dependencies) into the current region
    Value wpnIndex = workersPerNode;
    if (wpnIndex) {
      if (Operation *defOp = wpnIndex.getDefiningOp()) {
        if (!currentRegion->isAncestor(defOp->getParentRegion())) {
          collectWithDeps(wpnIndex);
          ValueUtils::cloneValuesIntoRegion(boundsToClone, currentRegion,
                                            boundsMapper, AC->getBuilder(),
                                            /*allowMemoryEffectFree=*/true,
                                            isSafeRuntimeQuery);
          wpnIndex = boundsMapper.lookupOrDefault(wpnIndex);
        }
      }
      if (!wpnIndex.getType().isIndex())
        wpnIndex = AC->create<arith::IndexCastOp>(loc, indexType, wpnIndex);
    }

    /// numNodes = totalWorkers / workersPerNode
    Value numNodes =
        AC->create<arith::DivUIOp>(loc, totalWorkersIndex, wpnIndex);

    Value nodeId =
        AC->create<arith::DivUIOp>(loc, workerIdIndex, wpnIndex);
    Value localThreadId =
        AC->create<arith::RemUIOp>(loc, workerIdIndex, wpnIndex);

    /// Level 1: chunks -> nodes
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

    /// Level 2: local threads
    Value subAdjusted = AC->create<arith::AddIOp>(
        loc, nodeRows,
        AC->create<arith::SubIOp>(loc, wpnIndex, oneIndex));
    Value subBlock =
        AC->create<arith::DivUIOp>(loc, subAdjusted, wpnIndex);

    Value localStart =
        AC->create<arith::MulIOp>(loc, localThreadId, subBlock);

    Value needZeroLocal = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::uge, localStart, nodeRows);
    Value localRemaining =
        AC->create<arith::SubIOp>(loc, nodeRows, localStart);
    Value localRemainingNonNeg = AC->create<arith::SelectOp>(
        loc, needZeroLocal, zeroIndex, localRemaining);
    Value localCount =
        AC->create<arith::MinUIOp>(loc, subBlock, localRemainingNonNeg);

    DistributionBounds bounds;
    bounds.iterStart =
        AC->create<arith::AddIOp>(loc, nodeStart, localStart);
    bounds.iterCount = localCount;
    bounds.iterCountHint = subBlock;
    bounds.hasWork = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::ugt, localCount, zeroIndex);

    bounds.acquireStart = nodeStart;
    bounds.acquireSize = nodeRows;
    bounds.acquireSizeHint = nodeMaxRows;
    return bounds;
  }

  /// Flat: mirror the same balanced floor + remainder mapping
  auto [firstChunkIdx, chunkCount] =
      balancedDistribute(AC, loc, localTotalChunks, totalWorkersIndex,
                         workerIdIndex);

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
  bounds.hasWork = AC->create<arith::CmpIOp>(
      loc, arith::CmpIPredicate::ugt, bounds.iterCount, zeroIndex);

  bounds.acquireStart = bounds.iterStart;
  bounds.acquireSize = bounds.iterCount;
  bounds.acquireSizeHint = bounds.iterCountHint;
  return bounds;
}

///===----------------------------------------------------------------------===///
/// DB Alignment Block Size
///===----------------------------------------------------------------------===///

Value DistributionHeuristics::computeDbAlignmentBlockSize(
    EdtOp parallelEdt, Value numPartitions, ArtsCodegen *AC, Location loc) {
  if (!parallelEdt || !numPartitions)
    return Value();

  Value one = AC->createIndexConstant(1, loc);
  Value hintBlockSize;

  bool isTwoLevel =
      (parallelEdt.getConcurrency() == EdtConcurrency::internode);

  for (Value dep : parallelEdt.getDependencies()) {
    /// Trace each dependency to its root DbAllocOp
    auto allocInfo = DatablockUtils::traceToDbAlloc(dep);
    if (!allocInfo)
      continue;
    auto [rootGuid, rootPtr] = *allocInfo;
    auto allocOp = rootGuid.getDefiningOp<DbAllocOp>();
    if (!allocOp || allocOp.getElementSizes().empty())
      continue;

    /// For TwoLevel (internode): align ALL arrays, not just stencil.
    /// This ensures all workers on the same node acquire the same DB block,
    /// reducing remote acquires and enabling correct stencil halo logic.
    if (!isTwoLevel) {
      /// Flat (intranode): only align stencil arrays
      auto pattern = getDbAccessPattern(allocOp.getOperation());
      if (!pattern || *pattern != DbAccessPattern::stencil)
        continue;
    }

    /// Skip scalar allocs (element size = 1)
    Value elemSize = allocOp.getElementSizes().front();
    if (auto constElem = ValueUtils::tryFoldConstantIndex(elemSize))
      if (*constElem <= 1)
        continue;

    elemSize = castToIndex(AC, elemSize, loc);

    /// Compute ceil(elemSize / numPartitions) — same formula as DbPartitioning
    Value adjusted = AC->create<arith::AddIOp>(
        loc, elemSize,
        AC->create<arith::SubIOp>(loc, numPartitions, one));
    Value candidate =
        AC->create<arith::DivUIOp>(loc, adjusted, numPartitions);
    candidate = AC->create<arith::MaxUIOp>(loc, candidate, one);

    if (!hintBlockSize)
      hintBlockSize = candidate;
    else
      hintBlockSize =
          AC->create<arith::MinUIOp>(loc, hintBlockSize, candidate);
  }

  return hintBlockSize;
}

///===----------------------------------------------------------------------===///
/// Workers Per Node
///===----------------------------------------------------------------------===///

Value DistributionHeuristics::getWorkersPerNode(ArtsCodegen *AC, Location loc,
                                                EdtOp parallelEdt) {
  Value workersPerNode;

  if (auto workers = parallelEdt->getAttrOfType<workersAttr>(
          AttrNames::Operation::Workers)) {
    Value totalWorkers = AC->createIndexConstant(workers.getValue(), loc);
    Value totalNodes =
        castToIndex(AC, AC->create<GetTotalNodesOp>(loc).getResult(), loc);
    workersPerNode =
        AC->create<arith::DivUIOp>(loc, totalWorkers, totalNodes);
  } else {
    workersPerNode =
        castToIndex(AC, AC->create<GetTotalWorkersOp>(loc).getResult(), loc);
  }

  /// Defensive clamp: ensure non-zero divisor for route computation.
  Value zero = AC->createIndexConstant(0, loc);
  Value one = AC->createIndexConstant(1, loc);
  Value isZero = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::eq,
                                           workersPerNode, zero);
  return AC->create<arith::SelectOp>(loc, isZero, one, workersPerNode);
}
