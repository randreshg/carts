///==========================================================================///
/// File: ForLowering.cpp
///
/// Lowers arts.for operations within EDT regions (parallel/task)
/// 1. Splits parallel regions at arts.for boundaries
/// 2. Creates EpochOp wrappers with task EDTs for loop iterations
/// 3. Creates continuation parallel EDTs for post-loop work
/// 4. Rewires DB acquires directly to DbAllocOp (not through block arguments)
///
/// Transformation:
///   BEFORE: edt.parallel { work_1; arts.for {...}; work_2 }
///   AFTER:  edt.parallel { work_1 }
///           arts.epoch { scf.for { edt.task {...} }; edt.task<result> }
///           edt.parallel { work_2 }  /// with reacquired DBs from DbAllocOp
///
/// Worker partitioning uses block distribution:
///   - blockSize = ceil(totalIterations / numWorkers)
///   - Each worker processes iterations [start, start + count)
///
/// Example:
///   one `arts.for` inside `arts.edt<parallel>` -> epoch + per-worker task EDTs
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/graphs/db/DbGraph.h"
#include "arts/analysis/graphs/db/DbNode.h"
#include "arts/analysis/heuristics/DistributionHeuristics.h"
#include "arts/analysis/heuristics/PartitioningHeuristics.h"
#include "arts/analysis/metadata/MetadataManager.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/codegen/Codegen.h"
#include "arts/passes/PassDetails.h"
#include "arts/passes/Passes.h"
#include "arts/transforms/edt/EdtParallelSplitLowering.h"
#include "arts/transforms/edt/EdtReductionLowering.h"
#include "arts/transforms/edt/EdtRewriter.h"
#include "arts/transforms/edt/EdtTaskLoopLowering.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "arts/utils/Utils.h"
#include "arts/utils/metadata/LoopMetadata.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"

#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>
#include <optional>

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(for_lowering);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

namespace {

/// Inputs required to plan one rewritten task acquire.
struct AcquireRewritePlanningInput {
  ArtsCodegen *AC = nullptr;
  Location loc;

  ForOp loopOp;
  DbAcquireOp parentAcquire;
  Value rootGuid;
  Value rootPtr;

  DistributionKind distributionKind = DistributionKind::Flat;
  std::optional<EdtDistributionPattern> distributionPattern;
  std::optional<Tiling2DWorkerGrid> tiling2DGrid;
  AcquireRewriteContract rewriteContract;

  Value acquireOffset;
  Value acquireSize;
  Value acquireHintSize;
  Value step;
  bool stepIsUnit = true;
};

struct AcquireRewritePlan {
  AcquireRewriteInput rewriteInput;
  bool useStencilRewriter = false;
};

struct TaskAcquireWindow {
  Value elementOffset;
  Value elementSize;
  Value dbOffset;
  Value dbSize;
};

/// Resolve acquire rewrite contracts through shared DB analysis first, then
/// fall back to direct contract derivation when analysis is unavailable.
static AcquireRewriteContract
resolveAcquireRewriteContract(mlir::arts::AnalysisManager *AM,
                              DbAcquireOp acquire) {
  if (!acquire)
    return AcquireRewriteContract{};
  if (!AM)
    return deriveAcquireRewriteContract(acquire);
  return AM->getDbAnalysis().getAcquireRewriteContract(acquire);
}

static DistributionStrategy resolveDistributionStrategy(EdtOp originalParallel,
                                                        ForOp forOp) {
  /// Prefer explicit distribution_kind stamped by EdtDistributionPass. Fall
  /// back to concurrency-derived strategy when the annotation is absent.
  auto strategy = DistributionHeuristics::analyzeStrategy(
      originalParallel.getConcurrency(), /*machine=*/nullptr);
  if (auto selectedKind = getEdtDistributionKind(forOp.getOperation()))
    strategy.kind = DistributionHeuristics::toDistributionKind(*selectedKind);
  return strategy;
}

static std::optional<EdtDistributionPattern>
resolveDistributionPattern(mlir::arts::AnalysisManager *AM, ForOp forOp,
                           EdtOp originalParallel) {
  if (auto pattern = getEdtDistributionPattern(forOp.getOperation()))
    return pattern;
  if (AM) {
    if (auto pattern = AM->getDbAnalysis().getLoopDistributionPattern(forOp);
        pattern && *pattern != EdtDistributionPattern::unknown) {
      return pattern;
    }
  }
  return getEdtDistributionPattern(originalParallel.getOperation());
}

/// Plan one task-local acquire rewrite from the already-selected distribution
/// shape. This stays local to ForLowering because no other pass consumes the
/// planning contract.
static AcquireRewritePlan
planAcquireRewrite(AcquireRewritePlanningInput input) {
  auto makePlan = [&]() {
    return AcquireRewritePlan{
        AcquireRewriteInput{input.AC, input.loc, input.parentAcquire,
                            input.rootGuid, input.rootPtr, input.acquireOffset,
                            input.acquireSize, input.acquireHintSize,
                            /*extraOffsets=*/SmallVector<Value, 4>{},
                            /*extraSizes=*/SmallVector<Value, 4>{},
                            /*extraHintSizes=*/SmallVector<Value, 4>{},
                            /*dimensionExtents=*/SmallVector<Value, 4>{},
                            /*haloMinOffsets=*/SmallVector<int64_t, 4>{},
                            /*haloMaxOffsets=*/SmallVector<int64_t, 4>{},
                            input.step, input.stepIsUnit,
                            /*singleElement=*/false,
                            /*forceCoarse=*/
                            input.distributionKind ==
                                DistributionKind::BlockCyclic,
                            /*preserveParentDepRange=*/false,
                            /*stencilExtent=*/Value()},
        /*useStencilRewriter=*/false};
  };

  if (!input.AC || !input.parentAcquire)
    return makePlan();

  Value zero = input.AC->createIndexConstant(0, input.loc);
  Value one = input.AC->createIndexConstant(1, input.loc);
  AcquireRewritePlan plan{makePlan()};

  bool isSingleElement = false;
  bool forceCoarseRewrite = plan.rewriteInput.forceCoarse;
  Value stencilExtent;
  SmallVector<Value, 4> extraOffsets;
  SmallVector<Value, 4> extraSizes;
  SmallVector<Value, 4> extraHintSizes;
  SmallVector<Value, 4> dimensionExtents;
  AcquireRewriteContract rewriteContract = input.rewriteContract;

  if (Operation *rootAllocOp = DbUtils::getUnderlyingDbAlloc(input.rootPtr)) {
    if (auto dbAlloc = dyn_cast<DbAllocOp>(rootAllocOp)) {
      auto elemSizes = dbAlloc.getElementSizes();
      if (!elemSizes.empty()) {
        isSingleElement = llvm::all_of(elemSizes, [](Value value) {
          int64_t constant = 0;
          return ValueAnalysis::getConstantIndex(value, constant) &&
                 constant == 1;
        });
        unsigned primaryOwnerDim = 0;
        if (!rewriteContract.ownerDims.empty() &&
            rewriteContract.ownerDims.front() >= 0 &&
            static_cast<size_t>(rewriteContract.ownerDims.front()) <
                elemSizes.size()) {
          primaryOwnerDim =
              static_cast<unsigned>(rewriteContract.ownerDims.front());
        }

        Value primaryExtent =
            input.AC->castToIndex(elemSizes[primaryOwnerDim], input.loc);
        if (rewriteContract.applyStencilHalo)
          stencilExtent = primaryExtent;
        dimensionExtents.push_back(primaryExtent);

        if (input.distributionKind == DistributionKind::Tiling2D &&
            elemSizes.size() > 1 && !isSingleElement && input.tiling2DGrid &&
            input.tiling2DGrid->colWorkers) {
          unsigned secondaryOwnerDim = 1;
          if (rewriteContract.ownerDims.size() > 1 &&
              rewriteContract.ownerDims[1] >= 0 &&
              static_cast<size_t>(rewriteContract.ownerDims[1]) <
                  elemSizes.size()) {
            secondaryOwnerDim =
                static_cast<unsigned>(rewriteContract.ownerDims[1]);
          }

          Value totalCols =
              input.AC->castToIndex(elemSizes[secondaryOwnerDim], input.loc);
          Value colWorkers =
              input.AC->castToIndex(input.tiling2DGrid->colWorkers, input.loc);
          Value colWorkerId =
              input.AC->castToIndex(input.tiling2DGrid->colWorkerId, input.loc);

          Value colWorkersMinusOne =
              input.AC->create<arith::SubIOp>(input.loc, colWorkers, one);
          Value colAdjusted = input.AC->create<arith::AddIOp>(
              input.loc, totalCols, colWorkersMinusOne);
          Value colChunk = input.AC->create<arith::DivUIOp>(
              input.loc, colAdjusted, colWorkers);
          Value colOffset =
              input.AC->create<arith::MulIOp>(input.loc, colWorkerId, colChunk);

          Value colNeedZero = input.AC->create<arith::CmpIOp>(
              input.loc, arith::CmpIPredicate::uge, colOffset, totalCols);
          Value colRemaining =
              input.AC->create<arith::SubIOp>(input.loc, totalCols, colOffset);
          Value colRemainingNonNeg = input.AC->create<arith::SelectOp>(
              input.loc, colNeedZero, zero, colRemaining);
          Value colCount = input.AC->create<arith::MinUIOp>(input.loc, colChunk,
                                                            colRemainingNonNeg);

          extraOffsets.push_back(colOffset);
          extraSizes.push_back(colCount);
          extraHintSizes.push_back(colCount);
          dimensionExtents.push_back(totalCols);
          rewriteContract.preserveParentDepRange = false;
        }
      }
    }
  }

  plan.useStencilRewriter = rewriteContract.applyStencilHalo;
  plan.rewriteInput.singleElement = isSingleElement;
  plan.rewriteInput.forceCoarse = forceCoarseRewrite;
  plan.rewriteInput.preserveParentDepRange =
      rewriteContract.preserveParentDepRange;
  plan.rewriteInput.stencilExtent = stencilExtent;
  plan.rewriteInput.extraOffsets = std::move(extraOffsets);
  plan.rewriteInput.extraSizes = std::move(extraSizes);
  plan.rewriteInput.extraHintSizes = std::move(extraHintSizes);
  plan.rewriteInput.dimensionExtents = std::move(dimensionExtents);
  plan.rewriteInput.haloMinOffsets = std::move(rewriteContract.haloMinOffsets);
  plan.rewriteInput.haloMaxOffsets = std::move(rewriteContract.haloMaxOffsets);
  return plan;
}

static std::optional<TaskAcquireWindow> computeTaskAcquireWindowInBlockSpace(
    ArtsCodegen *AC, Location loc, DbAcquireOp parentAcquire,
    DbAcquireOp taskAcquire, Value rootGuidValue, Value rootPtrValue,
    DistributionKind distributionKind,
    std::optional<EdtDistributionPattern> distributionPattern,
    Value plannedElementOffset, Value plannedElementSize,
    Value plannedElementSizeSeed, bool usesStencilHalo,
    const AcquireRewriteContract &rewriteContract) {
  if (!AC || !parentAcquire || !taskAcquire)
    return std::nullopt;

  PartitionMode mode =
      taskAcquire.getPartitionMode().value_or(PartitionMode::coarse);
  if (mode != PartitionMode::block && mode != PartitionMode::stencil)
    return std::nullopt;

  Value one = AC->createIndexConstant(1, loc);
  Value zero = AC->createIndexConstant(0, loc);
  Value blockSpan = one;
  Value totalBlocks;
  DbAllocOp rootAlloc;
  if (auto allocFromGuid = rootGuidValue.getDefiningOp<DbAllocOp>())
    rootAlloc = allocFromGuid;
  else if (Operation *allocOp = DbUtils::getUnderlyingDbAlloc(rootPtrValue))
    rootAlloc = dyn_cast<DbAllocOp>(allocOp);
  else if (Operation *allocOp =
               DbUtils::getUnderlyingDbAlloc(parentAcquire.getPtr()))
    rootAlloc = dyn_cast<DbAllocOp>(allocOp);

  if (rootAlloc) {
    auto elementSizes = rootAlloc.getElementSizes();
    if (!elementSizes.empty() && elementSizes.front())
      blockSpan = AC->castToIndex(elementSizes.front(), loc);
    auto outerSizes = rootAlloc.getSizes();
    if (!outerSizes.empty() && outerSizes.front())
      totalBlocks = AC->castToIndex(outerSizes.front(), loc);
  }
  blockSpan = AC->create<arith::MaxUIOp>(loc, blockSpan, one);

  /// This helper intentionally reasons in leading-dimension block space only.
  /// It converts the already chosen task-local element window into the single
  /// block window used by DB slice metadata on 1-D / row-owned plans.
  Value elementOffset = plannedElementOffset;
  Value elementSize = plannedElementSizeSeed;
  const bool useRewrittenWindow = rewriteContract.usePartitionSliceAsDepWindow;
  const bool useTaskDepWindow =
      usesStencilHalo || rewriteContract.applyStencilHalo;

  if (useRewrittenWindow || useTaskDepWindow) {
    /// Contract-backed acquires already carry the authoritative task-local
    /// dependency window in one of two places:
    ///   1. partition_* for families that preserve an owner-aligned element
    ///      slice there
    ///   2. offsets/sizes for halo-expanded dependency windows
    ///
    /// Recomputing the DB-space window from the planned owner slice would drop
    /// halo reads at block boundaries and misalign rec_dep slot numbering with
    /// the outlined EDT's dep_gep base offset.
    auto offsetRange = useRewrittenWindow ? taskAcquire.getPartitionOffsets()
                                          : taskAcquire.getOffsets();
    auto sizeRange = useRewrittenWindow ? taskAcquire.getPartitionSizes()
                                        : taskAcquire.getSizes();

    if (!offsetRange.empty() && offsetRange.front())
      elementOffset = offsetRange.front();

    if (!sizeRange.empty() && sizeRange.front())
      elementSize = sizeRange.front();
  } else {
    if (!taskAcquire.getSizes().empty() && taskAcquire.getSizes().front())
      elementSize = taskAcquire.getSizes().front();

    if (distributionKind == DistributionKind::TwoLevel &&
        parentAcquire.getMode() != ArtsMode::in)
      elementSize = plannedElementSize;

    bool isStencilPattern =
        distributionPattern &&
        *distributionPattern == EdtDistributionPattern::stencil;
    if (isStencilPattern && mode == PartitionMode::stencil &&
        parentAcquire.getMode() != ArtsMode::in)
      elementSize = AC->create<arith::AddIOp>(loc, elementSize, one);
  }

  elementOffset = AC->castToIndex(elementOffset, loc);
  elementSize = AC->castToIndex(elementSize, loc);
  elementSize = AC->create<arith::MaxUIOp>(loc, elementSize, one);

  Value startBlock = AC->create<arith::DivUIOp>(loc, elementOffset, blockSpan);
  Value endElem = AC->create<arith::AddIOp>(loc, elementOffset, elementSize);
  endElem = AC->create<arith::SubIOp>(loc, endElem, one);
  Value endBlock = AC->create<arith::DivUIOp>(loc, endElem, blockSpan);

  Value startAboveMax;
  if (totalBlocks) {
    Value maxBlock = AC->create<arith::SubIOp>(loc, totalBlocks, one);
    startAboveMax = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ugt,
                                              startBlock, maxBlock);
    Value clampedEnd = AC->create<arith::MinUIOp>(loc, endBlock, maxBlock);
    endBlock =
        AC->create<arith::SelectOp>(loc, startAboveMax, endBlock, clampedEnd);
  }

  Value blockCount = AC->create<arith::SubIOp>(loc, endBlock, startBlock);
  blockCount = AC->create<arith::AddIOp>(loc, blockCount, one);
  if (startAboveMax) {
    startBlock =
        AC->create<arith::SelectOp>(loc, startAboveMax, zero, startBlock);
    blockCount =
        AC->create<arith::SelectOp>(loc, startAboveMax, zero, blockCount);
  }

  return TaskAcquireWindow{elementOffset, elementSize, startBlock, blockCount};
}

///===----------------------------------------------------------------------===///
/// LoopInfo - Information about a loop within a parallel EDT
///
/// LoopInfo encapsulates worker partitioning for arts.for inside a parallel
/// EDT. It implements a simple block distribution:
///   - blockSizeCeil = ceil(totalIterations / numWorkers)
///   - start = workerId * blockSizeCeil
///   - count = min(blockSizeCeil, max(0, totalIterations - start))
///   - hasWork = start < totalIterations
///===----------------------------------------------------------------------===///
class LoopInfo {
public:
  LoopInfo(ArtsCodegen *AC, ForOp forOp, Value numWorkers,
           Value runtimeBlockSizeHint = Value())
      : AC(AC), forOp(forOp), runtimeBlockSizeHint(runtimeBlockSizeHint) {
    lowerBound = forOp.getLowerBound()[0];
    upperBound = forOp.getUpperBound()[0];
    loopStep = forOp.getStep()[0];
    totalWorkers = numWorkers;
    initialize();
  }

  /// Attributes
  ArtsCodegen *AC;

  /// Loop information
  ForOp forOp;
  Value lowerBound, upperBound, loopStep;
  /// Base lower bound used for chunking (may be aligned to block size)
  Value chunkLowerBound;
  bool useAlignedLowerBound = false;
  bool useRuntimeBlockAlignment = false;
  std::optional<int64_t> alignmentBlockSize;
  Value runtimeBlockSizeHint;
  /// Distribution information
  Value blockSize, totalWorkers, totalIterations, totalChunks;

  /// Distribution strategy and bounds from DistributionHeuristics
  DistributionStrategy strategy;
  DistributionBounds bounds;       ///  Set by computeBounds()
  DistributionBounds insideBounds; ///  Set by recomputeBoundsInside()

private:
  void initialize();

  /// Phase 1: Determine the block size from PartitioningHint and/or
  /// the runtime DB-alignment hint. Sets blockSize, useRuntimeBlockAlignment.
  /// Returns the compile-time block size used for alignment decisions.
  int64_t computeBlockSize();

  /// Phase 2: Align chunkLowerBound to the nearest block boundary if the
  /// lower bound is misaligned. Sets chunkLowerBound, useAlignedLowerBound,
  /// alignmentBlockSize.
  void alignChunkLowerBound(int64_t computedBlockSize);

  /// Phase 3: Compute totalIterations = ceil((upper - chunkLower) / step)
  /// and totalChunks = ceil(totalIterations / blockSize).
  void computeIterationsAndChunks();
};

using ReductionInfo = ReductionLoweringInfo;
using ParallelRegionAnalysis = ParallelRegionSplitAnalysis;

/// Collect external values needed by operations in a block (including nested
/// regions).
static void collectExternalValues(Block &sourceBlock, Region *boundaryRegion,
                                  SetVector<Value> &externalValues,
                                  const DenseSet<Operation *> &opsToSkip) {
  Region *sourceRegion = sourceBlock.getParent();

  for (Operation &op : sourceBlock.without_terminator()) {
    if (opsToSkip.contains(&op))
      continue;

    op.walk([&](Operation *nestedOp) {
      for (Value operand : nestedOp->getOperands()) {
        if (auto blockArg = operand.dyn_cast<BlockArgument>()) {
          Region *ownerRegion = blockArg.getOwner()->getParent();
          if (sourceRegion->isAncestor(ownerRegion))
            continue;
          if (boundaryRegion->isAncestor(ownerRegion))
            continue;
        }
        if (Operation *defOp = operand.getDefiningOp()) {
          if (sourceRegion->isAncestor(defOp->getParentRegion()))
            continue;
          if (!boundaryRegion->isAncestor(defOp->getParentRegion()))
            externalValues.insert(operand);
        }
      }
    });
  }
}

/// Clone external stack allocations into the EDT region and remap uses.
static void cloneExternalAllocasIntoEdt(Region *taskEdtRegion, Block &taskBlock,
                                        IRMapping &mapper, OpBuilder &builder) {
  DenseMap<Operation *, SmallVector<Operation *, 4>> usesByAlloca;

  taskBlock.walk([&](Operation *op) {
    for (Value operand : op->getOperands()) {
      auto allocaOp = operand.getDefiningOp<memref::AllocaOp>();
      if (!allocaOp)
        continue;
      if (taskEdtRegion->isAncestor(allocaOp->getParentRegion()))
        continue;
      usesByAlloca[allocaOp.getOperation()].push_back(op);
    }
  });

  if (usesByAlloca.empty())
    return;

  ARTS_DEBUG("  - Cloning " << usesByAlloca.size()
                            << " external stack allocas into EDT");

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(&taskBlock);

  for (const auto &entry : usesByAlloca) {
    auto allocaOp = cast<memref::AllocaOp>(entry.first);
    Value originalMem = allocaOp.getResult();

    bool hasStoreInEdt = false;
    for (Operation *user : entry.second) {
      if (auto store = dyn_cast<memref::StoreOp>(user)) {
        if (store.getMemRef() == originalMem) {
          hasStoreInEdt = true;
          break;
        }
      }
    }

    bool hasStoreOutsideEdt = false;
    for (Operation *user : allocaOp->getUsers()) {
      if (auto store = dyn_cast<memref::StoreOp>(user)) {
        if (store.getMemRef() != originalMem)
          continue;
        if (!taskEdtRegion->isAncestor(store->getParentRegion())) {
          hasStoreOutsideEdt = true;
          break;
        }
      }
    }

    /// If the alloca is initialized outside and only read inside the EDT,
    /// keep the original alloca to preserve initialized values.
    Region *allocaRegion = allocaOp->getParentRegion();
    bool allocaVisible =
        allocaRegion && allocaRegion->isAncestor(taskEdtRegion);
    if (hasStoreOutsideEdt && !hasStoreInEdt && allocaVisible)
      continue;

    Operation *clonedOp = builder.clone(*allocaOp.getOperation(), mapper);
    auto newAlloca = cast<memref::AllocaOp>(clonedOp);
    Value clonedMem = newAlloca.getResult();

    for (Operation *user : entry.second)
      user->replaceUsesOfWith(originalMem, clonedMem);

    mapper.map(originalMem, clonedMem);

    /// Clone safe initialization stores for this alloca when available.
    SmallVector<memref::StoreOp, 4> initStores;
    bool hasUnsafeStore = false;
    for (Operation *user : allocaOp->getUsers()) {
      auto hasLoopAncestor = [&](Operation *op) {
        return op->getParentOfType<scf::ForOp>() ||
               op->getParentOfType<scf::IfOp>();
      };
      if (auto store = dyn_cast<memref::StoreOp>(user)) {
        if (store.getMemRef() != originalMem)
          continue;
        if (store->getBlock() != allocaOp->getBlock() ||
            hasLoopAncestor(store.getOperation())) {
          hasUnsafeStore = true;
          break;
        }
        initStores.push_back(store);
      }
    }

    if (!hasUnsafeStore && !initStores.empty()) {
      IRMapping storeMapper;
      storeMapper.map(allocaOp.getResult(), newAlloca.getResult());
      builder.setInsertionPointAfter(newAlloca);
      for (memref::StoreOp store : initStores)
        builder.clone(*store.getOperation(), storeMapper);
      builder.setInsertionPointToStart(&taskBlock);
    }
  }
}

/// ForLowering Pass Implementation
struct ForLoweringPass : public arts::ForLoweringBase<ForLoweringPass> {
  explicit ForLoweringPass(mlir::arts::AnalysisManager *AM = nullptr)
      : AM(AM) {}
  void runOnOperation() override;

private:
  ModuleOp module;
  mlir::arts::AnalysisManager *AM = nullptr;

  void gatherLowerableEdts(SmallVectorImpl<EdtOp> &lowerableEdts);
  void lowerCollectedEdts(ArrayRef<EdtOp> lowerableEdts);

  /// Process a parallel EDT containing arts_for operations
  void lowerParallelEdt(EdtOp parallelEdt);

  /// Clone loop body into task EDT's scf.for
  void cloneLoopBody(ArtsCodegen *AC, ForOp forOp, scf::ForOp chunkLoop,
                     Value chunkOffset, IRMapping &mapper);

  /// Reduction Support

  /// Allocate partial accumulators for reductions (one per worker)
  /// If splitMode is true, skip creating DbAcquireOps as dependencies to the
  /// parallel EDT (used when the parallel will be erased in split pattern)
  ReductionInfo
  allocatePartialAccumulators(ArtsCodegen *AC, ForOp forOp, EdtOp parallelEdt,
                              Location loc, bool splitMode = false,
                              Value workerCountOverride = Value());

  void createResultEdt(ArtsCodegen *AC, ReductionInfo &redInfo, Location loc);

  Attribute getLoopMetadataAttr(ForOp forOp);

  /// Lower an arts.for with DB acquires rewired directly to DbAllocOp.
  /// This is used when splitting the parallel region - the for body is
  /// extracted outside the parallel EDT and acquires DBs directly.
  void lowerForWithDbRewiring(ArtsCodegen &AC, ForOp forOp,
                              EdtOp originalParallel,
                              ParallelRegionAnalysis &analysis, Location loc);

  /// Create task EDT with DB dependencies rewired to DbAllocOp.
  EdtOp createTaskEdtWithRewiring(ArtsCodegen *AC, LoopInfo &loopInfo,
                                  ForOp forOp, Value workerIdPlaceholder,
                                  EdtOp originalParallel,
                                  ReductionInfo &redInfo);
};

} // namespace

/// LoopInfo computes how to distribute loop iterations across workers using
/// block distribution. This ensures balanced work with minimal overhead.

void LoopInfo::initialize() {
  int64_t compiletimeBlockSize = computeBlockSize();
  alignChunkLowerBound(compiletimeBlockSize);
  computeIterationsAndChunks();
}

/// Phase 1: Determine the block size.
///
/// Reads the PartitioningHint attached to the for-op and merges it with the
/// runtime DB-alignment block-size hint. The final blockSize Value is the
/// maximum of the two so that worker chunks never straddle DB block
/// boundaries.
///
/// Returns the compile-time block size (from the hint) for use in alignment
/// decisions in subsequent phases.
int64_t LoopInfo::computeBlockSize() {
  Location loc = forOp.getLoc();
  Value one = AC->createIndexConstant(1, loc);

  if (getEffectiveDepPattern(forOp.getOperation()) ==
      ArtsDepPattern::wavefront_2d) {
    /// Wavefront-transformed tile loops already encode their task granularity
    /// in the loop step. Reusing DB-alignment hints here collapses small wave
    /// frontiers down to a single chunk, which serializes the schedule.
    blockSize = one;
    useRuntimeBlockAlignment = false;
    return 1;
  }

  int64_t computedBlockSize = 1;

  if (auto hint = getPartitioningHint(forOp.getOperation())) {
    if (hint->mode == PartitionMode::block && hint->blockSize &&
        *hint->blockSize > 0) {
      computedBlockSize = *hint->blockSize;
      ARTS_DEBUG(
          "Using explicit PartitioningHint blockSize=" << computedBlockSize);
    }
  }

  blockSize = AC->createIndexConstant(computedBlockSize, loc);
  useRuntimeBlockAlignment = false;

  if (runtimeBlockSizeHint) {
    Value runtimeBlockSize = AC->castToIndex(runtimeBlockSizeHint, loc);
    runtimeBlockSize = AC->create<arith::MaxUIOp>(loc, runtimeBlockSize, one);

    /// Respect explicit coarsening hints, but never allow them to go below
    /// runtime DB-alignment guidance. Smaller chunk sizes can force write
    /// tasks to span multiple DB blocks and trigger block-level clobbering in
    /// distributed mode.
    bool shouldUseRuntimeAlignment = (computedBlockSize == 1);
    if (!shouldUseRuntimeAlignment) {
      if (auto runtimeConst =
              ValueAnalysis::tryFoldConstantIndex(runtimeBlockSize))
        shouldUseRuntimeAlignment = (*runtimeConst > computedBlockSize);
      else
        shouldUseRuntimeAlignment = true;
    }

    if (shouldUseRuntimeAlignment) {
      blockSize = AC->create<arith::MaxUIOp>(loc, blockSize, runtimeBlockSize);
      /// When runtime alignment contributes to block size selection, align
      /// chunk partitioning at runtime to keep DB-space slices block-safe.
      useRuntimeBlockAlignment = true;
    }
  }

  return computedBlockSize;
}

/// Phase 2: Align the chunk lower bound to the nearest block boundary.
///
/// When blockSize > 1 or runtime alignment is active, the lower bound used
/// for chunking may need to be rounded down so that chunk boundaries align
/// with DB block boundaries. This avoids overlapping block partitions
/// (e.g., stencil loops where lowerBound is not a multiple of blockSize).
void LoopInfo::alignChunkLowerBound(int64_t computedBlockSize) {
  Location loc = forOp.getLoc();

  chunkLowerBound = lowerBound;
  useAlignedLowerBound = false;
  alignmentBlockSize = std::nullopt;

  int64_t alignSize = computedBlockSize;
  if (alignSize > 1)
    alignmentBlockSize = alignSize;

  if (alignmentBlockSize) {
    if (auto lbConst = ValueAnalysis::tryFoldConstantIndex(lowerBound)) {
      int64_t lbVal = *lbConst;
      int64_t aligned = lbVal - (lbVal % *alignmentBlockSize);
      if (aligned != lbVal) {
        useAlignedLowerBound = true;
        chunkLowerBound = AC->createIndexConstant(aligned, loc);
        ARTS_DEBUG("Aligning chunk lower bound from " << lbVal << " to "
                                                      << aligned);
      }
    }
  }

  if (useRuntimeBlockAlignment) {
    Value rem = AC->create<arith::RemUIOp>(loc, lowerBound, blockSize);
    chunkLowerBound = AC->create<arith::SubIOp>(loc, lowerBound, rem);
    useAlignedLowerBound = true;
  }
}

/// Phase 3: Compute total iterations and total chunks.
///
///   totalIterations = ceil((upperBound - chunkLowerBound) / loopStep)
///   totalChunks     = ceil(totalIterations / blockSize)
void LoopInfo::computeIterationsAndChunks() {
  Location loc = forOp.getLoc();

  Value range = AC->create<arith::SubIOp>(loc, upperBound, chunkLowerBound);
  Value adjustedRange = AC->create<arith::AddIOp>(
      loc, range,
      AC->create<arith::SubIOp>(loc, loopStep,
                                AC->createIndexConstant(1, loc)));
  totalIterations = AC->create<arith::DivUIOp>(loc, adjustedRange, loopStep);

  Value adjustedIterations = AC->create<arith::AddIOp>(
      loc, totalIterations,
      AC->create<arith::SubIOp>(loc, blockSize,
                                AC->createIndexConstant(1, loc)));
  totalChunks = AC->create<arith::DivUIOp>(loc, adjustedIterations, blockSize);
}

///===----------------------------------------------------------------------===///
/// Pass Entry Point
///===----------------------------------------------------------------------===///

void ForLoweringPass::runOnOperation() {
  module = getOperation();

  /// Phase 1: Gather candidate EDTs carrying arts.for operations.
  SmallVector<EdtOp, 4> lowerableEdts;
  gatherLowerableEdts(lowerableEdts);

  /// Skip pass bookkeeping/logging entirely when no arts.for is present.
  if (lowerableEdts.empty()) {
    ARTS_DEBUG("No arts.for operations found, skipping ForLowering");
    return;
  }

  ARTS_INFO_HEADER(ForLowering);
  ARTS_DEBUG_REGION(module.dump(););

  /// Phase 2-4: Evaluate lowering policy, build rewrite plans, and apply.
  lowerCollectedEdts(lowerableEdts);

  ARTS_INFO_FOOTER(ForLowering);
  ARTS_DEBUG_REGION(module.dump(););
}

void ForLoweringPass::gatherLowerableEdts(
    SmallVectorImpl<EdtOp> &lowerableEdts) {
  module.walk([&](EdtOp edt) {
    if (edt.getType() != EdtType::parallel && edt.getType() != EdtType::task)
      return;

    bool hasForOps = false;
    edt.getBody().walk([&](ForOp) { hasForOps = true; });
    if (hasForOps)
      lowerableEdts.push_back(edt);
  });
}

void ForLoweringPass::lowerCollectedEdts(ArrayRef<EdtOp> lowerableEdts) {
  for (EdtOp parallelEdt : lowerableEdts)
    lowerParallelEdt(parallelEdt);
}

void ForLoweringPass::lowerParallelEdt(EdtOp parallelEdt) {
  ARTS_INFO("Lowering parallel EDT with ALWAYS-SPLIT pattern");

  /// Analyze the parallel EDT structure to find arts.for operations
  /// and categorize operations as before/after the for loop
  ParallelRegionAnalysis analysis =
      ParallelRegionAnalysis::analyze(parallelEdt);

  if (!analysis.needsSplit()) {
    ARTS_DEBUG(" - No arts.for operations found, skipping");
    return;
  }

  ARTS_INFO(" - Found " << analysis.forOps.size() << " arts.for operation(s)");

  /// Analyze which DBs are used by for and post-for operations
  analysis.analyzeDependenciesForSplit(parallelEdt);

  bool hasPreFor = analysis.hasWorkBefore();
  bool hasPostFor = analysis.hasWorkAfter();

  ARTS_INFO(" - Pre-for work: " << (hasPreFor ? "yes" : "no")
                                << ", Post-for work: "
                                << (hasPostFor ? "yes" : "no"));
  ARTS_DEBUG(" - Deps used by for: " << analysis.depsUsedByFor.size());
  ARTS_DEBUG(" - Deps used after for: " << analysis.depsUsedAfterFor.size());

  ArtsCodegen AC(module);
  if (AM)
    AC.setAbstractMachine(&AM->getAbstractMachine());
  Location loc = parallelEdt.getLoc();

  /// Extract for-body outside the parallel EDT
  /// The transformation is:
  /// - Pre-for work stays in original parallel (if any)
  /// - For-body becomes: EpochOp { scf.for { task EDTs } + result EDT }
  /// - Post-for work goes into a new continuation parallel (if any)

  /// Step 1: Set insertion point after original parallel EDT
  AC.setInsertionPointAfter(parallelEdt);

  /// Step 2: Lower each arts.for with DB rewiring (create epoch + tasks)
  /// This creates the lowered for structure AFTER the parallel EDT
  for (ForOp forOp : analysis.forOps)
    lowerForWithDbRewiring(AC, forOp, parallelEdt, analysis, loc);

  /// Step 3: Create continuation parallel for post-for work (if any)
  /// Skip if opsAfterFor only contains DB release cleanup operations.
  /// Keep barrier-only continuations intact because barrier semantics may be
  /// required by timestep-style loops (e.g., stencil updates) between epochs.
  if (hasPostFor) {
    bool onlyCleanup = llvm::all_of(analysis.opsAfterFor, [](Operation *op) {
      return isa<DbReleaseOp>(op);
    });
    if (!onlyCleanup)
      createContinuationParallel(AC, parallelEdt, analysis, loc);
  }

  /// Step 4: Clean up original parallel EDT + dependencies.
  cleanupOriginalParallel(parallelEdt, analysis, hasPreFor);

  ARTS_INFO(" - Parallel EDT lowering complete");
}

Attribute ForLoweringPass::getLoopMetadataAttr(ForOp forOp) {
  Attribute attr = forOp->getAttr(AttrNames::LoopMetadata::Name);
  if (attr)
    return attr;
  MetadataManager manager(forOp.getContext());
  manager.ensureLoopMetadata(forOp);
  return forOp->getAttr(AttrNames::LoopMetadata::Name);
}

void ForLoweringPass::cloneLoopBody(ArtsCodegen *AC, ForOp forOp,
                                    scf::ForOp chunkLoop, Value chunkOffset,
                                    IRMapping &mapper) {
  Location loc = forOp.getLoc();
  AC->setInsertionPointToStart(chunkLoop.getBody());

  /// Compute global index from local iteration variable
  ///  - global_iter = chunkOffset + local_iter
  ///  - global_idx = lowerBound + global_iter * step
  Value localIter = chunkLoop.getInductionVar();
  Value globalIter = AC->create<arith::AddIOp>(loc, chunkOffset, localIter);
  Value globalIterScaled =
      AC->create<arith::MulIOp>(loc, globalIter, forOp.getStep()[0]);
  Value globalIdx = AC->create<arith::AddIOp>(loc, forOp.getLowerBound()[0],
                                              globalIterScaled);

  /// Map arts.for induction variable to computed global index
  Block &forBody = forOp.getRegion().front();
  if (forBody.getNumArguments() > 0) {
    BlockArgument forIV = forBody.getArgument(0);
    mapper.map(forIV, globalIdx);
  }

  /// Collect constants used in the loop body that are defined outside
  /// and clone them inside the EDT region
  SetVector<Value> constantsToClone;
  Region *taskEdtRegion = chunkLoop->getParentRegion();
  for (Operation &op : forBody.without_terminator()) {
    for (Value operand : op.getOperands()) {
      if (Operation *defOp = operand.getDefiningOp()) {
        if (defOp->hasTrait<OpTrait::ConstantLike>()) {
          /// Check if this constant is defined outside the task EDT region
          Region *defRegion = defOp->getParentRegion();
          if (!taskEdtRegion->isAncestor(defRegion)) {
            constantsToClone.insert(operand);
          }
        }
      }
    }
  }

  /// Clone constants inside the task EDT region before cloning operations
  /// Use the builder with the correct insertion point
  for (Value constant : constantsToClone) {
    if (Operation *defOp = constant.getDefiningOp()) {
      Operation *clonedConst = AC->getBuilder().clone(*defOp);
      mapper.map(constant, clonedConst->getResult(0));
    }
  }

  /// Clone all operations from arts.for body into chunk loop
  for (Operation &op : forBody.without_terminator())
    AC->clone(op, mapper);

  ARTS_DEBUG(
      "    Cloned " << std::distance(forBody.without_terminator().begin(),
                                     forBody.without_terminator().end())
                    << " operations into chunk loop");
}

ReductionInfo ForLoweringPass::allocatePartialAccumulators(
    ArtsCodegen *AC, ForOp forOp, EdtOp parallelEdt, Location loc,
    bool splitMode, Value workerCountOverride) {
  return mlir::arts::allocatePartialAccumulators(
      AC, forOp, parallelEdt, loc, getLoopMetadataAttr(forOp), splitMode,
      workerCountOverride);
}

void ForLoweringPass::createResultEdt(ArtsCodegen *AC, ReductionInfo &redInfo,
                                      Location loc) {
  arts::createResultEdt(AC, redInfo, loc);
}

///===----------------------------------------------------------------------===///
/// Parallel Region Splitting Implementation
///===----------------------------------------------------------------------===///
void ForLoweringPass::lowerForWithDbRewiring(ArtsCodegen &AC, ForOp forOp,
                                             EdtOp originalParallel,
                                             ParallelRegionAnalysis &analysis,
                                             Location loc) {
  ARTS_INFO(" - Lowering arts.for with DB rewiring (split pattern)");

  /// Read distribution strategy selected by EdtDistributionPass.
  DistributionStrategy strategy = resolveDistributionStrategy(originalParallel, forOp);

  Value zero;
  Value one;
  std::optional<LoopInfo> loopInfoStorage;
  Value dispatchWorkers;
  ReductionInfo redInfo;

  {
    OpBuilder::InsertionGuard guard(AC.getBuilder());
    /// Values consumed by split-mode reduction allocation must dominate the
    /// allocs inserted before the original parallel EDT.
    AC.setInsertionPoint(originalParallel);

    /// Get numWorkers from explicit attrs or runtime queries.
    Value numWorkers =
        DistributionHeuristics::getTotalWorkers(&AC, loc, originalParallel);

    /// numDbPartitions: the number of DB partition blocks (= numNodes for
    /// internode). Used to align worker chunk boundaries to DB block
    /// boundaries.
    Value numDbPartitions;
    if (originalParallel.getConcurrency() == EdtConcurrency::internode) {
      numDbPartitions = AC.castToIndex(
          AC.create<RuntimeQueryOp>(loc, RuntimeQueryKind::totalNodes)
              .getResult(),
          loc);
    } else {
      numDbPartitions = numWorkers;
    }

    zero = AC.createIndexConstant(0, loc);
    one = AC.createIndexConstant(1, loc);

    /// Determine loop chunking before building the epoch so we can size the
    /// dispatch loop and any reduction temporaries to the worker lanes that can
    /// actually receive work.
    Value runtimeBlockSize =
        DistributionHeuristics::computeDbAlignmentBlockSize(
            originalParallel, numDbPartitions, &AC, loc);
    loopInfoStorage.emplace(&AC, forOp, numWorkers, runtimeBlockSize);
    LoopInfo &loopInfo = *loopInfoStorage;
    loopInfo.strategy = strategy;
    dispatchWorkers = DistributionHeuristics::getForDispatchWorkerCount(
        &AC, loc, originalParallel, loopInfo.strategy, loopInfo.totalChunks);

    /// Allocate reduction accumulators (same as before, but place BEFORE epoch)
    /// Use splitMode=true since we're splitting the parallel EDT and will
    /// create acquires directly in result/task EDTs.
    if (!forOp.getReductionAccumulators().empty()) {
      ARTS_INFO(" - Detected reduction(s), allocating partial accumulators");
      redInfo =
          allocatePartialAccumulators(&AC, forOp, originalParallel, loc,
                                      /*splitMode=*/true, dispatchWorkers);
    }
  }

  auto isWavefrontPattern = [](Operation *op) {
    return op && getEffectiveDepPattern(op) == ArtsDepPattern::wavefront_2d;
  };
  const bool reuseEnclosingEpoch =
      forOp.getReductionAccumulators().empty() &&
      originalParallel->getParentOfType<EpochOp>() &&
      (isWavefrontPattern(forOp.getOperation()) ||
       isWavefrontPattern(originalParallel.getOperation()) ||
       isWavefrontPattern(originalParallel->getParentOp()));

  std::optional<EpochOp> forEpoch;
  Block *epochBlock = nullptr;
  if (reuseEnclosingEpoch) {
    epochBlock = originalParallel->getBlock();
    AC.setInsertionPoint(originalParallel);
  } else {
    /// Create EpochOp wrapper for the for-body at the caller-managed insertion
    /// point so multiple arts.for regions preserve source order.
    forEpoch = AC.create<EpochOp>(loc);
    Region &epochRegion = forEpoch->getRegion();
    if (epochRegion.empty())
      epochRegion.push_back(new Block());
    epochBlock = &epochRegion.front();
    AC.setInsertionPointToStart(epochBlock);
  }

  /// Worker dispatch loop - emit only the worker lanes that can receive at
  /// least one chunk for this loop/distribution strategy.
  auto workerLoop = AC.create<scf::ForOp>(loc, zero, dispatchWorkers, one);
  AC.setInsertionPointToStart(workerLoop.getBody());
  Value workerIV = workerLoop.getInductionVar();

  /// Create task EDT with DB rewiring
  EdtOp taskEdt = createTaskEdtWithRewiring(
      &AC, *loopInfoStorage, forOp, workerIV, originalParallel, redInfo);
  transferOperationContract(forOp.getOperation(), taskEdt.getOperation());
  if (forEpoch)
    transferOperationContract(forOp.getOperation(), forEpoch->getOperation());

  /// After worker loop, create result EDT (if reductions)
  AC.setInsertionPointAfter(workerLoop);
  if (!redInfo.reductionVars.empty()) {
    createResultEdt(&AC, redInfo, loc);
  }

  if (forEpoch) {
    /// Close epoch with yield
    AC.setInsertionPointToEnd(epochBlock);
    AC.create<YieldOp>(loc);
    /// Set insertion point after epoch so subsequent for loops insert
    /// correctly.
    AC.setInsertionPointAfter(*forEpoch);
  } else {
    AC.setInsertionPointAfter(workerLoop);
  }
  if (!redInfo.reductionVars.empty())
    finalizeReductionAfterEpoch(&AC, redInfo, loc);

  ARTS_INFO(" - arts.for lowering with DB rewiring complete");
}

EdtOp ForLoweringPass::createTaskEdtWithRewiring(
    ArtsCodegen *AC, LoopInfo &loopInfo, ForOp forOp, Value workerIdPlaceholder,
    EdtOp originalParallel, ReductionInfo &redInfo) {
  Location loc = forOp.getLoc();

  ARTS_DEBUG("  Creating task EDT with DB rewiring");

  /// Recreate total workers using shared distribution helper.
  loopInfo.totalWorkers =
      DistributionHeuristics::getTotalWorkers(AC, loc, originalParallel);

  /// Compute workers-per-node for internode routing and TwoLevel distribution.
  /// Flat may still run in internode mode during debugging/experiments.
  Value workersPerNode;
  if (loopInfo.strategy.kind == DistributionKind::TwoLevel ||
      originalParallel.getConcurrency() == EdtConcurrency::internode) {
    workersPerNode =
        DistributionHeuristics::getWorkersPerNode(AC, loc, originalParallel);
  }

  /// Compute worker iteration bounds using DistributionHeuristics
  loopInfo.bounds = DistributionHeuristics::computeBounds(
      AC, loc, loopInfo.strategy, workerIdPlaceholder, loopInfo.totalWorkers,
      workersPerNode, loopInfo.totalIterations, loopInfo.totalChunks,
      loopInfo.blockSize);

  Value chunkOffset = loopInfo.bounds.iterStart;
  ValueRange parentDeps = originalParallel.getDependencies();
  Block &parallelBlock = originalParallel.getRegion().front();

  IRMapping mapper;

  /// Create scf.if to conditionally create task EDT only if worker has work
  auto ifOp = AC->create<scf::IfOp>(loc, loopInfo.bounds.hasWork,
                                    /*withElseRegion=*/false);
  AC->setInsertionPointToStart(&ifOp.getThenRegion().front());

  /// Detect reduction block arguments
  DenseSet<Value> reductionBlockArgs = redInfo.reductionVars.empty()
                                           ? DenseSet<Value>{}
                                           : detectReductionBlockArgs(forOp);

  Value one = AC->createIndexConstant(1, loc);
  Value taskWorkerId = workerIdPlaceholder;
  TaskLoopLoweringInput taskLoopInput{AC,
                                      loc,
                                      loopInfo.strategy,
                                      loopInfo.bounds,
                                      taskWorkerId,
                                      loopInfo.totalWorkers,
                                      workersPerNode,
                                      loopInfo.upperBound,
                                      loopInfo.lowerBound,
                                      loopInfo.chunkLowerBound
                                          ? loopInfo.chunkLowerBound
                                          : forOp.getLowerBound()[0],
                                      loopInfo.loopStep,
                                      loopInfo.blockSize,
                                      loopInfo.totalIterations,
                                      loopInfo.totalChunks,
                                      loopInfo.alignmentBlockSize,
                                      loopInfo.useRuntimeBlockAlignment,
                                      loopInfo.useAlignedLowerBound};

  std::unique_ptr<EdtTaskLoopLowering> taskLoopLowering =
      EdtTaskLoopLowering::create(loopInfo.strategy.kind);
  TaskAcquirePlanningResult acquirePlanning =
      taskLoopLowering->planAcquireRewrite(taskLoopInput, chunkOffset);
  Value workerOffsetVal = acquirePlanning.workerBaseOffset;
  Value acquireOffsetVal = acquirePlanning.acquireOffset;
  Value acquireSizeVal = acquirePlanning.acquireSize;
  Value acquireHintSizeVal = acquirePlanning.acquireHintSize;
  Value stepVal = acquirePlanning.step;
  bool stepIsUnit = acquirePlanning.stepIsUnit;
  std::optional<Tiling2DWorkerGrid> tiling2DGrid = acquirePlanning.tiling2DGrid;

  /// Acquire worker-local partial accumulator slot for reductions
  SmallVector<Value> reductionTaskDeps;
  DenseMap<Value, uint64_t> reductionVarIndex;

  for (uint64_t i = 0; i < redInfo.reductionVars.size(); i++) {
    /// Use partialAccumPtrs (from DbAllocOp) instead of partialAccumArgs
    /// (BlockArguments inside parallel EDT) to avoid use-after-free when
    /// erasing the parallel EDT in split mode
    Value partialPtr = i < redInfo.partialAccumPtrs.size()
                           ? redInfo.partialAccumPtrs[i]
                           : Value();
    Value partialGuid = i < redInfo.partialAccumGuids.size()
                            ? redInfo.partialAccumGuids[i]
                            : Value();
    if (!partialPtr) {
      ARTS_ERROR("Missing partial accumulator ptr for reduction " << i);
      continue;
    }

    auto partialPtrType = partialPtr.getType().cast<MemRefType>();
    auto innerMemrefType = partialPtrType.getElementType().cast<MemRefType>();

    /// Acquire the worker's slice of the partial accumulator array
    /// Source directly from DbAllocOp (partialGuid, partialPtr)
    /// Use chunked partition mode since we're acquiring by worker offset
    /// offsets=[workerId] and sizes=[1] to get this worker's slot
    auto partialAcqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::inout, partialGuid, partialPtr, innerMemrefType,
        PartitionMode::block, /*indices=*/SmallVector<Value>{},
        /*offsets=*/SmallVector<Value>{workerIdPlaceholder},
        /*sizes=*/SmallVector<Value>{one},
        /*partition_indices=*/SmallVector<Value>{},
        /*partition_offsets=*/SmallVector<Value>{},
        /*partition_sizes=*/SmallVector<Value>{});
    /// Worker-local partial reduction acquires already have the exact slot
    /// access mode and must not be re-inferred later.
    partialAcqOp.setPreserveAccessMode();

    reductionTaskDeps.push_back(partialAcqOp.getResult(1));
    reductionVarIndex[redInfo.reductionVars[i]] = i;

    ARTS_DEBUG("  - Acquired worker-local partial accumulator slot");
  }

  SmallVector<Value> taskDeps;
  SmallVector<std::pair<BlockArgument, Value>> parallelArgToAcquire;
  bool forceOwnerRoute = false;
  ARTS_DEBUG("  - Processing " << parentDeps.size()
                               << " parent dependencies with DB rewiring");

  for (auto [idx, parentDep] : llvm::enumerate(parentDeps)) {
    auto parentAcqOp = parentDep.getDefiningOp<DbAcquireOp>();
    if (!parentAcqOp) {
      ARTS_DEBUG("    - Dep " << idx << ": Not a DbAcquireOp, skipping");
      continue;
    }

    BlockArgument parallelArg = parallelBlock.getArgument(idx);
    if (shouldSkipReductionArg(parallelArg, redInfo, reductionBlockArgs))
      continue;

    /// Rewiring: Trace to DbAllocOp and acquire from there
    auto allocInfo = DbUtils::traceToDbAlloc(parentDep);
    if (!allocInfo) {
      ARTS_ERROR("Could not trace dependency " << idx << " to DbAllocOp");
      continue;
    }
    auto [rootGuid, rootPtr] = *allocInfo;
    Value rootGuidValue = rootGuid;
    Value rootPtrValue = rootPtr;

    /// Check if parent acquire already has partition hints (from CreateDbs via
    /// DbControlOp). If hints exist, the user provided explicit partitioning,
    /// so ForLowering RESPECTS the user's intent and does NOT override.
    bool parentHasPartitionInfo = parentAcqOp.hasExplicitPartitionHints();

    DbAcquireOp chunkAcqOp;
    bool chunkUsesStencilHalo = false;

    std::optional<EdtDistributionPattern> distributionPattern =
        resolveDistributionPattern(AM, forOp, originalParallel);

    AcquireRewritePlanningInput planningInput{
        AC,
        loc,
        forOp,
        parentAcqOp,
        rootGuidValue,
        rootPtrValue,
        loopInfo.strategy.kind,
        distributionPattern,
        tiling2DGrid,
        resolveAcquireRewriteContract(AM, parentAcqOp),
        acquireOffsetVal,
        acquireSizeVal,
        acquireHintSizeVal,
        stepVal,
        stepIsUnit};

    auto createPlannedAcquire = [&]() -> std::pair<DbAcquireOp, bool> {
      AcquireRewritePlan rewritePlan = planAcquireRewrite(planningInput);
      DbAcquireOp rewritten = rewriteAcquire(rewritePlan.rewriteInput,
                                             rewritePlan.useStencilRewriter);
      return {rewritten, rewritePlan.useStencilRewriter};
    };

    auto setDbSpaceSliceFromAcquirePlan = [&](DbAcquireOp acquireOp,
                                              bool usesStencilHalo) {
      AcquireRewriteContract rewriteContract = planningInput.rewriteContract;
      bool shouldUseStencilWindow =
          usesStencilHalo || rewriteContract.usePartitionSliceAsDepWindow;

      /// Read-only acquires already get their task-local window from
      /// rewriteAcquire(). This helper only updates the DB-space slice metadata
      /// for write-capable acquires, plus stencil-contract read acquires that
      /// must carry a concrete block-space dependency window to avoid
      /// widening back to full-range during later lowering.
      if (!acquireOp)
        return;
      if (parentAcqOp.getMode() == ArtsMode::in && !shouldUseStencilWindow)
        return;
      if (parentAcqOp.getMode() != ArtsMode::in &&
          (acquireOp.getOffsets().size() > 1 ||
           acquireOp.getPartitionOffsets().size() > 1)) {
        ARTS_DEBUG("    - Preserving N-D task-local slice metadata");
        return;
      }

      OpBuilder::InsertionGuard guard(AC->getBuilder());
      AC->setInsertionPoint(acquireOp);

      auto window = computeTaskAcquireWindowInBlockSpace(
          AC, loc, parentAcqOp, acquireOp, rootGuidValue, rootPtrValue,
          loopInfo.strategy.kind, distributionPattern, acquireOffsetVal,
          loopInfo.bounds.iterCount, acquireHintSizeVal,
          shouldUseStencilWindow, planningInput.rewriteContract);
      if (!window)
        return;

      /// Keep partition hint ranges worker-local for two-level write acquires.
      /// DbPartitioning consults partition_* hints; if these remain node-wide
      /// while offsets/sizes are worker-local, later rewrites may widen the
      /// write slice back to node scope.
      if (loopInfo.strategy.kind == DistributionKind::TwoLevel &&
          parentAcqOp.getMode() != ArtsMode::in) {
        if (!acquireOp.getPartitionOffsets().empty())
          acquireOp.getPartitionOffsetsMutable().assign(
              SmallVector<Value>{window->elementOffset});
        if (!acquireOp.getPartitionSizes().empty())
          acquireOp.getPartitionSizesMutable().assign(
              SmallVector<Value>{window->elementSize});
      }
      acquireOp.getOffsetsMutable().assign(
          SmallVector<Value>{window->dbOffset});
      acquireOp.getSizesMutable().assign(SmallVector<Value>{window->dbSize});
    };

    if (parentHasPartitionInfo) {
      /// USER HINT EXISTS - ForLowering RESPECTS it!
      /// Use parent's partition mode and operands (from DbControlOp via
      /// CreateDbs)
      ARTS_DEBUG("    - Respecting existing DbControlOp hint on allocation");

      /// Reuse the parent acquire's partitioning clause which came from
      /// CreateDbs
      auto parentPartMode = parentAcqOp.getPartitionMode();
      SmallVector<Value> parentIndices(parentAcqOp.getIndices().begin(),
                                       parentAcqOp.getIndices().end());
      SmallVector<Value> parentOffsets(parentAcqOp.getOffsets().begin(),
                                       parentAcqOp.getOffsets().end());
      SmallVector<Value> parentSizes(parentAcqOp.getSizes().begin(),
                                     parentAcqOp.getSizes().end());

      SmallVector<Value> parentPartIndices(
          parentAcqOp.getPartitionIndices().begin(),
          parentAcqOp.getPartitionIndices().end());
      SmallVector<Value> parentPartOffsets(
          parentAcqOp.getPartitionOffsets().begin(),
          parentAcqOp.getPartitionOffsets().end());
      SmallVector<Value> parentPartSizes(
          parentAcqOp.getPartitionSizes().begin(),
          parentAcqOp.getPartitionSizes().end());
      PartitionMode mode = parentPartMode.value_or(PartitionMode::coarse);
      bool hasExplicitRangeHints =
          !parentOffsets.empty() && !parentSizes.empty();

      /// Hinted ranges are usually element-space and still need DB-space slice
      /// planning for rec_dep/depCount correctness.
      if (mode == PartitionMode::block || mode == PartitionMode::stencil ||
          hasExplicitRangeHints) {
        auto [plannedAcquire, useStencilHalo] = createPlannedAcquire();
        chunkAcqOp = plannedAcquire;
        chunkUsesStencilHalo = useStencilHalo;
        if (parentPartMode)
          setPartitionMode(chunkAcqOp.getOperation(), mode);
        chunkAcqOp.getIndicesMutable().assign(parentIndices);
        chunkAcqOp.getPartitionIndicesMutable().assign(parentPartIndices);
        chunkAcqOp.getPartitionOffsetsMutable().assign(parentPartOffsets);
        chunkAcqOp.getPartitionSizesMutable().assign(parentPartSizes);
        chunkAcqOp.copyPartitionSegmentsFrom(parentAcqOp);
      } else {
        chunkAcqOp = AC->create<DbAcquireOp>(
            loc, parentAcqOp.getMode(), rootGuidValue, rootPtrValue,
            parentAcqOp.getPtr().getType(), mode, parentIndices, parentOffsets,
            parentSizes, parentPartIndices, parentPartOffsets, parentPartSizes);
        transferContract(parentAcqOp.getOperation(), chunkAcqOp.getOperation(),
                         parentAcqOp.getPtr(), chunkAcqOp.getPtr(),
                         AC->getBuilder(), loc);
        chunkAcqOp.copyPartitionSegmentsFrom(parentAcqOp);
      }

    } else {
      /// NO USER HINT - plan strategy-specific acquire rewriting externally.
      auto [plannedAcquire, useStencilHalo] = createPlannedAcquire();
      chunkAcqOp = plannedAcquire;
      chunkUsesStencilHalo = useStencilHalo;
    }

    auto chunkMode = chunkAcqOp.getPartitionMode();
    AcquireRewriteContract chunkRewriteContract = planningInput.rewriteContract;
    bool shouldMarkStencilCenter =
        chunkRewriteContract.usePartitionSliceAsDepWindow ||
        chunkRewriteContract.applyStencilHalo ||
        (chunkMode && *chunkMode == PartitionMode::stencil);
    if (shouldMarkStencilCenter &&
        !getStencilCenterOffset(chunkAcqOp.getOperation())) {
      int64_t centerOffset = 1;
      if (!chunkRewriteContract.haloMinOffsets.empty())
        centerOffset =
            std::max<int64_t>(0, -chunkRewriteContract.haloMinOffsets.front());
      setStencilCenterOffset(chunkAcqOp.getOperation(), centerOffset);
    }

    setDbSpaceSliceFromAcquirePlan(chunkAcqOp, chunkUsesStencilHalo);

    if (originalParallel.getConcurrency() == EdtConcurrency::internode &&
        chunkAcqOp.getMode() != ArtsMode::in &&
        DbAnalysis::isCoarse(chunkAcqOp)) {
      auto allocOp = dyn_cast_or_null<DbAllocOp>(
          DbUtils::getUnderlyingDbAlloc(chunkAcqOp.getSourcePtr()));
      if (allocOp && !hasDistributedDbAllocation(allocOp.getOperation())) {
        forceOwnerRoute = true;
        ARTS_DEBUG("    - Coarse write dependency requires owner-local route");
      }
    }

    Value acquirePtr = chunkAcqOp.getResult(1);

    taskDeps.push_back(acquirePtr);
    parallelArgToAcquire.push_back({parallelArg, acquirePtr});
    ARTS_DEBUG("    - Created rewired acquire for dep " << idx);
  }

  /// Create task EDT
  /// Inherit concurrency from parent parallel EDT - if internode, route to
  /// worker node
  EdtConcurrency taskConcurrency = originalParallel.getConcurrency();
  Value routeValue;
  if (taskConcurrency == EdtConcurrency::internode) {
    if (forceOwnerRoute) {
      routeValue = AC->createIntConstant(0, AC->Int32, loc);
      ARTS_DEBUG("  - Forcing owner-local routing for coarse write task");
    } else {
      /// Route to destination node from the global worker id:
      ///   nodeId = globalWorkerId / workersPerNode
      /// workersPerNode is always materialized for internode routing.
      if (!workersPerNode)
        workersPerNode = DistributionHeuristics::getWorkersPerNode(
            AC, loc, originalParallel);
      Value nodeId =
          AC->create<arith::DivUIOp>(loc, workerIdPlaceholder, workersPerNode);
      routeValue = AC->castToInt(AC->Int32, nodeId, loc);
      ARTS_DEBUG("  - Using internode routing by workers-per-node");
    }
  } else {
    routeValue = AC->createIntConstant(0, AC->Int32, loc);
  }
  auto taskEdt = AC->create<EdtOp>(loc, EdtType::task, taskConcurrency,
                                   routeValue, ValueRange{});

  Block &taskBlock = taskEdt.getBody().front();
  AC->setInsertionPointToStart(&taskBlock);

  /// Track where reduction dependencies start
  uint64_t reductionArgStart = taskDeps.size();

  /// Combine regular and reduction dependencies
  taskDeps.append(reductionTaskDeps.begin(), reductionTaskDeps.end());

  /// Add block arguments to task EDT
  for (uint64_t i = 0; i < taskDeps.size(); i++) {
    Value dep = taskDeps[i];
    if (!dep) {
      ARTS_ERROR("Null dependency at index " << i);
      continue;
    }
    BlockArgument taskArg = taskBlock.addArgument(dep.getType(), loc);
    if (i < reductionArgStart)
      mapper.map(dep, taskArg);
  }

  taskEdt.setDependencies(taskDeps);

  /// Map parallelArg → taskArg for cloning pre-for operations
  for (auto [parallelArg, acquirePtr] : parallelArgToAcquire) {
    if (Value taskArg = mapper.lookupOrNull(acquirePtr))
      mapper.map(parallelArg, taskArg);
  }

  Value insideTotalWorkers =
      DistributionHeuristics::getTotalWorkers(AC, loc, originalParallel);
  taskLoopInput.totalWorkers = insideTotalWorkers;

  /// Map reduction variables to worker's accumulator slot
  IRMapping redMapper = mapper;
  DenseSet<Operation *> opsToSkip;

  for (auto [redVar, idx] : reductionVarIndex) {
    uint64_t argIndex = reductionArgStart + idx;
    if (argIndex >= taskBlock.getNumArguments())
      continue;

    BlockArgument partialArg = taskBlock.getArgument(argIndex);
    auto zeroIndex = AC->createIndexConstant(0, loc);
    Value myAccumulator =
        AC->create<DbRefOp>(loc, partialArg, SmallVector<Value>{zeroIndex});
    redMapper.map(redVar, myAccumulator);

    collectOldAccumulatorDbRefs(forOp, parallelBlock, reductionBlockArgs,
                                opsToSkip, redMapper, myAccumulator);
  }

  Block &forBody = forOp.getRegion().front();
  Region *taskEdtRegion = taskBlock.getParent();

  /// Collect external values needed by the loop body and induction variable
  SetVector<Value> externalValues;
  collectExternalValues(forBody, taskEdtRegion, externalValues, opsToSkip);

  Value origStep = forOp.getStep()[0];
  Value origLowerBound = forOp.getLowerBound()[0];
  Value origUpperBound = forOp.getUpperBound()[0];
  Value chunkLowerBoundVal =
      loopInfo.chunkLowerBound ? loopInfo.chunkLowerBound : origLowerBound;

  Region *forBodyRegion = forBody.getParent();
  std::function<void(Value)> collectWithDeps = [&](Value val) {
    if (externalValues.contains(val))
      return;
    if (Operation *defOp = val.getDefiningOp()) {
      if (forBodyRegion->isAncestor(defOp->getParentRegion()))
        return;
      if (!taskEdtRegion->isAncestor(defOp->getParentRegion())) {
        for (Value operand : defOp->getOperands())
          collectWithDeps(operand);
        externalValues.insert(val);
      }
    }
  };

  collectWithDeps(origStep);
  collectWithDeps(origLowerBound);
  if (loopInfo.useAlignedLowerBound)
    collectWithDeps(origUpperBound);
  collectWithDeps(chunkLowerBoundVal);
  collectWithDeps(workerOffsetVal);

  taskLoopLowering->collectExtraExternalValues(taskLoopInput, externalValues);

  /// Collect transitive dependencies
  SmallVector<Value> valuesToProcess(externalValues.begin(),
                                     externalValues.end());
  for (Value val : valuesToProcess) {
    if (Operation *defOp = val.getDefiningOp()) {
      for (Value operand : defOp->getOperands())
        collectWithDeps(operand);
    }
  }

  auto extraCloneableOps = [](Operation *op) {
    return isa<arts::DbRefOp, memref::LoadOp>(op);
  };
  if (!ValueAnalysis::cloneValuesIntoRegion(
          externalValues, taskEdtRegion, redMapper, AC->getBuilder(),
          /*allowMemoryEffectFree=*/true, extraCloneableOps)) {
    for (Value external : externalValues) {
      if (redMapper.contains(external))
        continue;
      if (Operation *defOp = external.getDefiningOp())
        ARTS_DEBUG("  - Uncloned external value op: " << defOp->getName());
      else
        ARTS_DEBUG("  - Uncloned external value (no defining op)");
    }
    ARTS_WARN("Some external values could not be cloned - they may need to be "
              "passed as EDT dependencies");
  }

  Value stepValMapped = redMapper.lookupOrDefault(origStep);
  Value origLowerBoundVal = redMapper.lookupOrDefault(origLowerBound);
  Value origUpperBoundVal = redMapper.lookupOrDefault(origUpperBound);
  TaskLoopLoweringMappedValues mappedLoopValues;
  mappedLoopValues.step = stepValMapped;
  mappedLoopValues.lowerBound = origLowerBoundVal;
  mappedLoopValues.upperBound = origUpperBoundVal;
  mappedLoopValues.workerBaseOffset =
      redMapper.lookupOrDefault(workerOffsetVal);
  mappedLoopValues.blockSize = redMapper.lookupOrDefault(loopInfo.blockSize);
  mappedLoopValues.totalIterations =
      redMapper.lookupOrDefault(loopInfo.totalIterations);
  mappedLoopValues.totalChunks =
      redMapper.lookupOrDefault(loopInfo.totalChunks);

  TaskLoopLoweringResult loweredLoop =
      taskLoopLowering->lower(taskLoopInput, mappedLoopValues);
  loopInfo.insideBounds = loweredLoop.insideBounds;
  chunkOffset = loweredLoop.iterStart;
  scf::ForOp iterLoop = loweredLoop.iterLoop;

  /// Set loop metadata, marking as parallel if only reduction deps existed
  if (Attribute loopAttr = getLoopMetadataAttr(forOp)) {
    if (!reductionVarIndex.empty()) {
      if (auto origMeta = dyn_cast<LoopMetadataAttr>(loopAttr)) {
        int64_t memrefDeps = 0;
        if (auto attr = origMeta.getMemrefsWithLoopCarriedDeps())
          memrefDeps = attr.getInt();

        if (memrefDeps == 0) {
          auto parallelMeta = LoopMetadata::createParallelizedMetadata(
              forOp.getContext(), origMeta);
          iterLoop->setAttr(AttrNames::LoopMetadata::Name, parallelMeta);
          ARTS_DEBUG("  Updated loop metadata: potentiallyParallel=true "
                     "(reduction-only deps, no stencil patterns)");
        } else {
          iterLoop->setAttr(AttrNames::LoopMetadata::Name, loopAttr);
          ARTS_DEBUG("  Keeping original metadata: memrefsWithLoopCarriedDeps="
                     << memrefDeps << " (stencil patterns detected)");
        }
      } else {
        iterLoop->setAttr(AttrNames::LoopMetadata::Name, loopAttr);
      }
    } else {
      iterLoop->setAttr(AttrNames::LoopMetadata::Name, loopAttr);
    }
  }

  AC->setInsertionPointToStart(iterLoop.getBody());

  /// Map induction variable
  Value localIter = iterLoop.getInductionVar();
  Value globalIterScaled =
      AC->create<arith::MulIOp>(loc, localIter, stepValMapped);
  Value globalIdx =
      AC->create<arith::AddIOp>(loc, loweredLoop.globalBase, globalIterScaled);

  /// Map undef to identity value
  for (Operation &op : forBody.without_terminator()) {
    for (Value operand : op.getOperands()) {
      if (auto undef = operand.getDefiningOp<LLVM::UndefOp>()) {
        Value undefVal = undef.getResult();
        Type elemType = undefVal.getType();
        Value identity = AC->createZeroValue(elemType, loc);
        if (identity)
          redMapper.map(undefVal, identity);
      }
    }
  }

  if (forBody.getNumArguments() > 0)
    redMapper.map(forBody.getArgument(0), globalIdx);

  /// No per-iteration bounds check needed: loop bounds already clamped.

  /// Clone stack allocations first so subsequent ops can map to them.
  for (Operation &op : forBody.without_terminator()) {
    if (opsToSkip.contains(&op))
      continue;
    if (isa<memref::AllocaOp>(op))
      AC->clone(op, redMapper);
  }

  /// Clone remaining loop body operations.
  for (Operation &op : forBody.without_terminator()) {
    if (opsToSkip.contains(&op))
      continue;
    if (isa<memref::AllocaOp>(op))
      continue;
    AC->clone(op, redMapper);
  }

  TaskLoopPostCloneInput postCloneInput{AC,
                                        loc,
                                        iterLoop,
                                        globalIdx,
                                        loweredLoop.innerStripeLane,
                                        loweredLoop.innerStripeCount};
  taskLoopLowering->postCloneAdjust(postCloneInput);

  /// Ensure stack allocations used inside the EDT are cloned locally.
  cloneExternalAllocasIntoEdt(taskEdtRegion, taskBlock, redMapper,
                              AC->getBuilder());

  /// Add yield terminator
  AC->setInsertionPointToEnd(&taskBlock);
  if (taskBlock.empty() || !taskBlock.back().hasTrait<OpTrait::IsTerminator>())
    AC->create<YieldOp>(loc);

  /// Release all DB dependencies before terminator
  AC->setInsertionPoint(taskBlock.getTerminator());
  for (uint64_t i = 0; i < taskDeps.size(); i++) {
    BlockArgument dbPtrArg = taskBlock.getArgument(i);
    AC->create<DbReleaseOp>(loc, dbPtrArg);
  }

  AC->setInsertionPointAfter(ifOp);

  return taskEdt;
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createForLoweringPass() {
  return std::make_unique<ForLoweringPass>();
}

std::unique_ptr<Pass> createForLoweringPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<ForLoweringPass>(AM);
}
} // namespace arts
} // namespace mlir
