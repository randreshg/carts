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

#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsHeuristics.h"
#include "arts/Analysis/DistributionHeuristics.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Transforms/Edt/AcquireRewritePlanning.h"
#include "arts/Transforms/Edt/EdtRewriter.h"
#include "arts/Transforms/Edt/EdtTaskLoopLowering.h"
#include "arts/Transforms/Edt/ParallelSplitLowering.h"
#include "arts/Transforms/Edt/ReductionLowering.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DbUtils.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>
#include <optional>

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(for_lowering);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

namespace {

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
  DistributionBounds bounds;       ///< Set by computeBounds()
  DistributionBounds insideBounds; ///< Set by recomputeBoundsInside()

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

/// Create a zero/identity value matching the provided element type.
static Value createZeroValue(ArtsCodegen *AC, Type elemType, Location loc) {
  if (!elemType)
    return Value();

  if (elemType.isa<IndexType>())
    return AC->createIndexConstant(0, loc);

  if (auto intTy = elemType.dyn_cast<IntegerType>())
    return AC->create<arith::ConstantIntOp>(loc, 0, intTy.getWidth());

  if (auto floatTy = elemType.dyn_cast<FloatType>()) {
    llvm::APFloat zero = llvm::APFloat::getZero(floatTy.getFloatSemantics());
    return AC->create<arith::ConstantFloatOp>(loc, zero, floatTy);
  }

  return Value();
}

/// ForLowering Pass Implementation
struct ForLoweringPass : public arts::ForLoweringBase<ForLoweringPass> {
  void runOnOperation() override;

private:
  ModuleOp module;

  /// Process a parallel EDT containing arts_for operations
  void lowerParallelEdt(EdtOp parallelEdt);

  /// Clone loop body into task EDT's scf.for
  void cloneLoopBody(ArtsCodegen *AC, ForOp forOp, scf::ForOp chunkLoop,
                     Value chunkOffset, IRMapping &mapper);

  /// Reduction Support

  /// Allocate partial accumulators for reductions (one per worker)
  /// If splitMode is true, skip creating DbAcquireOps as dependencies to the
  /// parallel EDT (used when the parallel will be erased in split pattern)
  ReductionInfo allocatePartialAccumulators(ArtsCodegen *AC, ForOp forOp,
                                            EdtOp parallelEdt, Location loc,
                                            bool splitMode = false);

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
              ValueUtils::tryFoldConstantIndex(runtimeBlockSize))
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
    if (auto lbConst = ValueUtils::tryFoldConstantIndex(lowerBound)) {
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

  /// Walk EDTs that may carry arts.for and lower them.
  SmallVector<EdtOp, 4> lowerableEdts;
  module.walk([&](EdtOp edt) {
    if (edt.getType() != EdtType::parallel && edt.getType() != EdtType::task)
      return;

    bool hasForOps = false;
    edt.getBody().walk([&](ForOp) { hasForOps = true; });

    if (hasForOps)
      lowerableEdts.push_back(edt);
  });

  /// Skip pass bookkeeping/logging entirely when no arts.for is present.
  if (lowerableEdts.empty()) {
    ARTS_DEBUG("No arts.for operations found, skipping ForLowering");
    return;
  }

  ARTS_INFO_HEADER(ForLowering);
  ARTS_DEBUG_REGION(module.dump(););

  for (EdtOp parallelEdt : lowerableEdts)
    lowerParallelEdt(parallelEdt);

  ARTS_INFO_FOOTER(ForLowering);
  ARTS_DEBUG_REGION(module.dump(););
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
  ArtsMetadataManager manager(forOp.getContext());
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

ReductionInfo ForLoweringPass::allocatePartialAccumulators(ArtsCodegen *AC,
                                                           ForOp forOp,
                                                           EdtOp parallelEdt,
                                                           Location loc,
                                                           bool splitMode) {
  return mlir::arts::allocatePartialAccumulators(
      AC, forOp, parallelEdt, loc, getLoopMetadataAttr(forOp), splitMode);
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

  /// Allocate reduction accumulators (same as before, but place BEFORE epoch)
  /// Use splitMode=true since we're splitting the parallel EDT and will
  /// create acquires directly in result/task EDTs
  ReductionInfo redInfo;
  if (!forOp.getReductionAccumulators().empty()) {
    ARTS_INFO(" - Detected reduction(s), allocating partial accumulators");
    redInfo = allocatePartialAccumulators(&AC, forOp, originalParallel, loc,
                                          /*splitMode=*/true);
  }

  /// Read distribution strategy selected by EdtDistributionPass.
  /// Fallback to concurrency-driven default when annotation is missing.
  auto strategy = DistributionHeuristics::analyzeStrategy(
      originalParallel.getConcurrency(), /*machine=*/nullptr);
  std::optional<EdtDistributionKind> selectedKindAttr =
      getEdtDistributionKind(forOp.getOperation());
  if (selectedKindAttr)
    strategy.kind =
        DistributionHeuristics::toDistributionKind(*selectedKindAttr);

  /// Get numWorkers from explicit attrs or runtime queries.
  Value numWorkers =
      DistributionHeuristics::getTotalWorkers(&AC, loc, originalParallel);

  /// numDbPartitions: the number of DB partition blocks (= numNodes for
  /// internode). Used to align worker chunk boundaries to DB block boundaries.
  Value numDbPartitions;
  if (originalParallel.getConcurrency() == EdtConcurrency::internode) {
    numDbPartitions = AC.castToIndex(
        AC.create<RuntimeQueryOp>(loc, RuntimeQueryKind::totalNodes)
            .getResult(),
        loc);
  } else {
    numDbPartitions = numWorkers;
  }

  Value zero = AC.createIndexConstant(0, loc);
  Value one = AC.createIndexConstant(1, loc);

  /// Create EpochOp wrapper for the for-body
  auto forEpoch = AC.create<EpochOp>(loc);
  Region &epochRegion = forEpoch.getRegion();
  if (epochRegion.empty())
    epochRegion.push_back(new Block());
  Block &epochBlock = epochRegion.front();
  AC.setInsertionPointToStart(&epochBlock);

  /// Worker dispatch loop - creates one task EDT per worker
  auto workerLoop = AC.create<scf::ForOp>(loc, zero, numWorkers, one);
  AC.setInsertionPointToStart(workerLoop.getBody());
  Value workerIV = workerLoop.getInductionVar();

  /// Create task EDT with DB rewiring
  /// Use DistributionHeuristics for alignment block size
  Value runtimeBlockSize = DistributionHeuristics::computeDbAlignmentBlockSize(
      originalParallel, numDbPartitions, &AC, loc);
  LoopInfo loopInfo(&AC, forOp, numWorkers, runtimeBlockSize);
  loopInfo.strategy = strategy;
  EdtOp taskEdt = createTaskEdtWithRewiring(&AC, loopInfo, forOp, workerIV,
                                            originalParallel, redInfo);
  copyDistributionAttrs(forOp.getOperation(), taskEdt.getOperation());
  copyDistributionAttrs(forOp.getOperation(), forEpoch.getOperation());

  /// After worker loop, create result EDT (if reductions)
  AC.setInsertionPointAfter(workerLoop);
  if (!redInfo.reductionVars.empty()) {
    createResultEdt(&AC, redInfo, loc);
  }

  /// Close epoch with yield
  AC.setInsertionPointToEnd(&epochBlock);
  AC.create<YieldOp>(loc);

  /// Set insertion point after epoch so subsequent for loops insert correctly.
  AC.setInsertionPointAfter(forEpoch);
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
    partialAcqOp.setPreserveDepMode();

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
        getEdtDistributionPattern(forOp.getOperation());
    if (!distributionPattern)
      distributionPattern =
          getEdtDistributionPattern(originalParallel.getOperation());

    AcquireRewritePlanningInput planningInput{AC,
                                              loc,
                                              parentAcqOp,
                                              rootGuidValue,
                                              rootPtrValue,
                                              loopInfo.strategy.kind,
                                              distributionPattern,
                                              tiling2DGrid,
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
      if (!acquireOp || parentAcqOp.getMode() == ArtsMode::in)
        return;

      OpBuilder::InsertionGuard guard(AC->getBuilder());
      AC->setInsertionPoint(acquireOp);

      PartitionMode mode =
          acquireOp.getPartitionMode().value_or(PartitionMode::coarse);
      /// Convert element-space task hints to DB-space slice coordinates for
      /// partitioned acquires. This avoids out-of-range DB dependency slices
      /// when block/stencil allocations use multi-row blocks.
      if (mode != PartitionMode::stencil && mode != PartitionMode::block)
        return;

      Value one = AC->createIndexConstant(1, loc);
      Value blockSpan = one;
      Value totalBlocks;
      DbAllocOp rootAlloc;
      if (auto allocFromGuid = rootGuidValue.getDefiningOp<DbAllocOp>())
        rootAlloc = allocFromGuid;
      else if (Operation *allocOp = DbUtils::getUnderlyingDbAlloc(rootPtrValue))
        rootAlloc = dyn_cast<DbAllocOp>(allocOp);
      else if (Operation *allocOp =
                   DbUtils::getUnderlyingDbAlloc(parentAcqOp.getPtr()))
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

      Value elemOffset = acquireOffsetVal;
      Value elemHintSize = acquireHintSizeVal;
      /// Stencil slices may widen and shift by halo extent; DB-space slicing
      /// must follow the rewritten task acquire window to keep dep indexing
      /// valid inside task EDT bodies.
      const bool useRewrittenWindow =
          parentAcqOp.getMode() == ArtsMode::in &&
          (usesStencilHalo || mode == PartitionMode::stencil);
      if (useRewrittenWindow) {
        if (!acquireOp.getOffsets().empty() && acquireOp.getOffsets().front())
          elemOffset = acquireOp.getOffsets().front();
        if (!acquireOp.getSizes().empty() && acquireOp.getSizes().front())
          elemHintSize = acquireOp.getSizes().front();
      } else {
        /// Keep block-mode base aligned across dependencies for the same loop
        /// slice.
        if (!acquireOp.getSizes().empty() && acquireOp.getSizes().front())
          elemHintSize = acquireOp.getSizes().front();

        /// Two-level lowering computes node-wide acquire hints for alignment,
        /// but write-capable dependencies must commit only the worker-local
        /// iteration span to avoid over-acquiring neighboring DB blocks.
        if (loopInfo.strategy.kind == DistributionKind::TwoLevel &&
            parentAcqOp.getMode() != ArtsMode::in)
          elemHintSize = loopInfo.bounds.iterCount;

        /// Only true stencil-mode writes need the extra terminal element in
        /// DB-space. Block-mode write deps use precise worker-local ranges;
        /// widening them by one can over-acquire a neighboring block and
        /// clobber a different writer's updates during full-block write-back.
        bool isStencilPattern =
            distributionPattern &&
            *distributionPattern == EdtDistributionPattern::stencil;
        if (isStencilPattern && mode == PartitionMode::stencil &&
            parentAcqOp.getMode() != ArtsMode::in)
          elemHintSize = AC->create<arith::AddIOp>(loc, elemHintSize, one);
      }
      elemOffset = AC->castToIndex(elemOffset, loc);
      elemHintSize = AC->castToIndex(elemHintSize, loc);
      elemHintSize = AC->create<arith::MaxUIOp>(loc, elemHintSize, one);

      /// Keep partition hint ranges worker-local for two-level write acquires.
      /// DbPartitioning consults partition_* hints; if these remain node-wide
      /// while offsets/sizes are worker-local, later rewrites may widen the
      /// write slice back to node scope.
      if (loopInfo.strategy.kind == DistributionKind::TwoLevel &&
          parentAcqOp.getMode() != ArtsMode::in) {
        if (!acquireOp.getPartitionOffsets().empty())
          acquireOp.getPartitionOffsetsMutable().assign(
              SmallVector<Value>{elemOffset});
        if (!acquireOp.getPartitionSizes().empty())
          acquireOp.getPartitionSizesMutable().assign(
              SmallVector<Value>{elemHintSize});
      }

      Value startBlock = AC->create<arith::DivUIOp>(loc, elemOffset, blockSpan);
      Value endElem = AC->create<arith::AddIOp>(loc, elemOffset, elemHintSize);
      endElem = AC->create<arith::SubIOp>(loc, endElem, one);
      Value endBlock = AC->create<arith::DivUIOp>(loc, endElem, blockSpan);

      Value startAboveMax;
      if (totalBlocks) {
        Value maxBlock = AC->create<arith::SubIOp>(loc, totalBlocks, one);
        startAboveMax = AC->create<arith::CmpIOp>(
            loc, arith::CmpIPredicate::ugt, startBlock, maxBlock);
        Value clampedEnd = AC->create<arith::MinUIOp>(loc, endBlock, maxBlock);
        endBlock = AC->create<arith::SelectOp>(loc, startAboveMax, endBlock,
                                               clampedEnd);
      }

      Value blockCount = AC->create<arith::SubIOp>(loc, endBlock, startBlock);
      blockCount = AC->create<arith::AddIOp>(loc, blockCount, one);
      if (startAboveMax) {
        Value zero = AC->createIndexConstant(0, loc);
        startBlock =
            AC->create<arith::SelectOp>(loc, startAboveMax, zero, startBlock);
        blockCount =
            AC->create<arith::SelectOp>(loc, startAboveMax, zero, blockCount);
      }
      acquireOp.getOffsetsMutable().assign(SmallVector<Value>{startBlock});
      acquireOp.getSizesMutable().assign(SmallVector<Value>{blockCount});
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
        chunkAcqOp.copyPartitionSegmentsFrom(parentAcqOp);
      }

    } else {
      /// NO USER HINT - plan strategy-specific acquire rewriting externally.
      auto [plannedAcquire, useStencilHalo] = createPlannedAcquire();
      chunkAcqOp = plannedAcquire;
      chunkUsesStencilHalo = useStencilHalo;
    }

    auto chunkMode = chunkAcqOp.getPartitionMode();
    bool shouldMarkStencilCenter =
        distributionPattern &&
        *distributionPattern == EdtDistributionPattern::stencil &&
        (chunkUsesStencilHalo || chunkAcqOp.getMode() == ArtsMode::in ||
         (chunkMode && *chunkMode == PartitionMode::stencil));
    if (shouldMarkStencilCenter &&
        !getStencilCenterOffset(chunkAcqOp.getOperation())) {
      setStencilCenterOffset(chunkAcqOp.getOperation(), 1);
    }

    setDbSpaceSliceFromAcquirePlan(chunkAcqOp, chunkUsesStencilHalo);

    if (originalParallel.getConcurrency() == EdtConcurrency::internode &&
        chunkAcqOp.getMode() != ArtsMode::in && DbUtils::isCoarse(chunkAcqOp)) {
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
  if (!ValueUtils::cloneValuesIntoRegion(
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
        Value identity = createZeroValue(AC, elemType, loc);
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
} // namespace arts
} // namespace mlir
