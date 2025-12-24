///==========================================================================///
/// File: ForLowering.cpp
///
/// Lowers arts.for operations within arts.edt<parallel>
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
///   - chunkSize = ceil(totalIterations / numWorkers)
///   - Each worker processes iterations [start, start + count)
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"

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
// LoopInfo - Information about a loop within a parallel EDT
///===----------------------------------------------------------------------===///
/// LoopInfo encapsulates worker partitioning for arts.for inside a parallel
/// EDT. It implements a simple block distribution:
///   - chunkSizeCeil = ceil(totalIterations / numWorkers)
///   - start = workerId * chunkSizeCeil
///   - count = min(chunkSizeCeil, max(0, totalIterations - start))
///   - hasWork = start < totalIterations
class LoopInfo {
public:
  LoopInfo(ArtsCodegen *AC, ForOp forOp, Value numWorkers)
      : AC(AC), forOp(forOp) {
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
  /// Distribution information
  Value chunkSize, totalWorkers, totalIterations, totalChunks;
  /// Worker information
  Value workerFirstChunk, workerIterationCount, workerHasWork;

  /// Compute worker-specific iteration bounds for block distribution
  void computeWorkerIterationsBlock(Value workerId);

private:
  void initialize();
};

///===----------------------------------------------------------------------===///
// ReductionInfo - Information about reductions within a parallel for
///===----------------------------------------------------------------------===///
/// Tracks DB handles for reduction pattern
///
/// Two sets of DBs per reduction variable:
/// 1. Partial accumulators: array[num_workers] for intermediate worker results
/// 2. Final result: scalar for combined output
struct ReductionInfo {
  /// Original reduction variables from arts.for operation
  SmallVector<Value> reductionVars;

  /// Original loop metadata attribute (if present)
  Attribute loopMetadataAttr;

  /// Location of the original arts.for operation
  std::optional<Location> loopLocation;

  /// Partial accumulators: array[num_workers] for intermediate results
  SmallVector<Value> partialAccumGuids, partialAccumPtrs, partialAccumArgs;

  /// Final result accumulators: scalar for each reduction variable
  SmallVector<Value> finalResultGuids, finalResultPtrs, finalResultArgs;

  /// Optional stack-allocated sinks to mirror final reduction value
  SmallVector<Value> hostResultPtrs;

  /// Number of workers (from workers attribute - provides bounded memory)
  Value numWorkers;
};

///===----------------------------------------------------------------------===///
// ParallelRegionAnalysis - Analyze parallel EDT contents for splitting
/// Categorizes operations in a parallel EDT relative to arts.for operations.
/// Used to split the parallel region into pre-for and post-for sections.
///===----------------------------------------------------------------------===///
struct ParallelRegionAnalysis {
  SmallVector<Operation *, 8> opsBeforeFor;
  SmallVector<ForOp, 4> forOps;
  SmallVector<Operation *, 8> opsAfterFor;

  /// Track which block arguments (DBs) are used by post-for operations.
  /// Used for datablock reacquisition when creating continuation parallel.
  SetVector<BlockArgument> depsUsedByFor, depsUsedAfterFor;

  /// Map block argument index to the original dependency for reacquisition
  DenseMap<unsigned, Value> argIndexToDep;

  bool hasWorkBefore() const { return !opsBeforeFor.empty(); }
  bool hasWorkAfter() const { return !opsAfterFor.empty(); }
  bool needsSplit() const { return !forOps.empty(); }

  /// Analyze a parallel EDT and populate the analysis
  static ParallelRegionAnalysis analyze(EdtOp parallelEdt) {
    ParallelRegionAnalysis analysis;
    Block &body = parallelEdt.getBody().front();

    bool seenFor = false;

    for (Operation &op : body.without_terminator()) {
      if (auto forOp = dyn_cast<ForOp>(&op)) {
        analysis.forOps.push_back(forOp);
        seenFor = true;
      } else if (isa<DbReleaseOp>(&op)) {
        /// Skip db_release ops - these are cleanup operations, not real work.
        /// They'll be handled when the original parallel is cleaned up or
        /// when the EDTs containing them are erased.
        continue;
      } else {
        if (!seenFor)
          analysis.opsBeforeFor.push_back(&op);
        else
          analysis.opsAfterFor.push_back(&op);
      }
    }

    return analysis;
  }

  /// Analyze which block arguments (DBs) are used by for and post-for
  /// operations. Must be called after analyze() and before creating
  /// continuation parallel.
  void analyzeDependenciesForSplit(EdtOp parallelEdt) {
    Block &parallelBlock = parallelEdt.getBody().front();
    ValueRange deps = parallelEdt.getDependencies();

    /// Build arg index to dependency mapping
    for (auto [idx, dep] : llvm::enumerate(deps))
      argIndexToDep[idx] = dep;

    /// Helper to collect block arguments used by an operation
    auto collectUsedBlockArgs = [&](Operation *op,
                                    SetVector<BlockArgument> &result) {
      op->walk([&](Operation *nested) {
        for (Value operand : nested->getOperands()) {
          if (auto blockArg = operand.dyn_cast<BlockArgument>()) {
            if (blockArg.getOwner() == &parallelBlock)
              result.insert(blockArg);
          }
        }
      });
    };

    /// Collect deps used by arts.for operations
    for (ForOp forOp : forOps)
      collectUsedBlockArgs(forOp.getOperation(), depsUsedByFor);

    /// Collect deps used by post-for operations
    for (Operation *op : opsAfterFor)
      collectUsedBlockArgs(op, depsUsedAfterFor);
  }
};

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

///===----------------------------------------------------------------------===///
// ForLowering Pass Implementation
///===----------------------------------------------------------------------===///
struct ForLoweringPass : public arts::ForLoweringBase<ForLoweringPass> {
  void runOnOperation() override;

private:
  ModuleOp module;

  /// Process a parallel EDT containing arts_for operations
  void lowerParallelEdt(EdtOp parallelEdt);

  /// Clone loop body into task EDT's scf.for
  void cloneLoopBody(ArtsCodegen *AC, ForOp forOp, scf::ForOp chunkLoop,
                     Value chunkOffset, IRMapping &mapper);

  ///===--------------------------------------------------------------------===///
  /// Reduction Support
  ///===--------------------------------------------------------------------===///

  /// Allocate partial accumulators for reductions (one per worker)
  /// If splitMode is true, skip creating DbAcquireOps as dependencies to the
  /// parallel EDT (used when the parallel will be erased in split pattern)
  ReductionInfo allocatePartialAccumulators(ArtsCodegen *AC, ForOp forOp,
                                            EdtOp parallelEdt, Location loc,
                                            bool splitMode = false);

  /// Create result EDT that combines partial accumulators
  /// The barrierEpochGuid parameter specifies which epoch the result EDT
  /// should be spawned into.
  void createResultEdt(ArtsCodegen *AC, EdtOp parallelEdt,
                       ReductionInfo &redInfo, Location loc);

  Attribute getLoopMetadataAttr(ForOp forOp);

  /// Lower an arts.for with DB acquires rewired directly to DbAllocOp.
  /// This is used when splitting the parallel region - the for body is
  /// extracted outside the parallel EDT and acquires DBs directly.
  void lowerForWithDbRewiring(ArtsCodegen &AC, ForOp forOp,
                              EdtOp originalParallel,
                              ParallelRegionAnalysis &analysis, Location loc);

  /// Create a continuation parallel EDT for post-for work.
  /// Reacquires all DBs from the original parallel EDT.
  EdtOp createContinuationParallel(ArtsCodegen &AC, EdtOp originalParallel,
                                   ParallelRegionAnalysis &analysis,
                                   Location loc);

  /// Create task EDT with DB dependencies rewired to DbAllocOp.
  EdtOp createTaskEdtWithRewiring(ArtsCodegen *AC, LoopInfo &loopInfo,
                                  ForOp forOp, Value workerIdPlaceholder,
                                  EdtOp originalParallel,
                                  ReductionInfo &redInfo);
};

} // namespace

///===----------------------------------------------------------------------===///
// LoopInfo Implementation - Work Partitioning Logic
/// LoopInfo computes how to distribute loop iterations across workers using
/// block distribution. This ensures balanced work with minimal overhead.
///===----------------------------------------------------------------------===///

void LoopInfo::initialize() {
  Location loc = forOp.getLoc();

  /// Extract or default the chunk size from loop attributes
  if (auto chunkAttr =
          forOp->getAttrOfType<IntegerAttr>(AttrNames::Operation::ChunkSize))
    chunkSize =
        AC->createIndexConstant(std::max<int64_t>(1, chunkAttr.getInt()), loc);
  else
    chunkSize = AC->createIndexConstant(1, loc);

  /// Compute total iterations: ceil((upper - lower) / step)
  Value range = AC->create<arith::SubIOp>(loc, upperBound, lowerBound);
  Value adjustedRange = AC->create<arith::AddIOp>(
      loc, range,
      AC->create<arith::SubIOp>(loc, loopStep,
                                AC->createIndexConstant(1, loc)));
  totalIterations = AC->create<arith::DivUIOp>(loc, adjustedRange, loopStep);

  ///===--------------------------------------------------------------------===///
  /// Compute total chunks: ceil(totalIterations / chunkSize)
  /// Formula: totalChunks = (totalIterations + chunkSize - 1) / chunkSize
  Value adjustedIterations = AC->create<arith::AddIOp>(
      loc, totalIterations,
      AC->create<arith::SubIOp>(loc, chunkSize,
                                AC->createIndexConstant(1, loc)));
  totalChunks = AC->create<arith::DivUIOp>(loc, adjustedIterations, chunkSize);
}

void LoopInfo::computeWorkerIterationsBlock(Value workerId) {
  auto loc = forOp.getLoc();

  ///===--------------------------------------------------------------------===///
  /// Block Distribution Strategy with Chunk Size Support
  /// This function computes the iteration bounds for a specific worker using
  /// block distribution. When a chunk size is specified (e.g., schedule(static,
  /// 4)), we distribute chunks to workers rather than individual iterations.
  ///
  /// Algorithm:
  ///   1. totalChunks = ceil(totalIterations / chunkSize)
  ///      - Already computed in initialize()
  ///   2. chunksPerWorkerCeil = ceil(totalChunks / totalWorkers)
  ///      - Each worker gets approximately equal number of chunks
  ///   3. workerFirstChunk = workerId * chunksPerWorkerCeil
  ///      - First chunk index for this worker
  ///   4. workerChunkCount = min(chunksPerWorkerCeil,
  ///                             max(0, totalChunks - workerFirstChunk))
  ///      - Number of chunks this worker processes
  ///   5. Convert chunk indices to iteration indices:
  ///      - workerFirstIteration = workerFirstChunk * chunkSize
  ///      - workerIterationCount = min(workerChunkCount * chunkSize,
  ///      totalIterations - workerFirstIteration)
  ///
  /// Example: N=16, chunkSize=4, workers=8
  ///   - totalChunks = ceil(16/4) = 4
  ///   - chunksPerWorkerCeil = ceil(4/8) = 1
  ///   - worker 0: firstChunk=0, chunkCount=1, iterations 0-3
  ///   - worker 1: firstChunk=1, chunkCount=1, iterations 4-7
  ///   - worker 2: firstChunk=2, chunkCount=1, iterations 8-11
  ///   - worker 3: firstChunk=3, chunkCount=1, iterations 12-15
  ///   - worker 4+: firstChunk>=4, chunkCount=0, no work
  ///===--------------------------------------------------------------------===///

  Value oneIndex = AC->createIndexConstant(1, loc);
  Value zeroIndex = AC->createIndexConstant(0, loc);

  /// Step 1: Compute ceiling chunks per worker
  /// chunksPerWorkerCeil = ceil(totalChunks / totalWorkers)
  ///                     = (totalChunks + totalWorkers - 1) / totalWorkers
  Value workersMinusOne =
      AC->create<arith::SubIOp>(loc, totalWorkers, oneIndex);
  Value adjustedChunks =
      AC->create<arith::AddIOp>(loc, totalChunks, workersMinusOne);
  Value chunksPerWorkerCeil =
      AC->create<arith::DivUIOp>(loc, adjustedChunks, totalWorkers);

  /// Step 2: Compute worker's first chunk index
  /// workerFirstChunkIdx = workerId * chunksPerWorkerCeil
  Value workerFirstChunkIdx =
      AC->create<arith::MulIOp>(loc, workerId, chunksPerWorkerCeil);

  /// Step 3: Compute number of chunks for this worker
  /// Handle edge case: if firstChunkIdx >= totalChunks, worker has no work
  Value needZeroChunks = AC->create<arith::CmpIOp>(
      loc, arith::CmpIPredicate::uge, workerFirstChunkIdx, totalChunks);
  Value remainingChunks =
      AC->create<arith::SubIOp>(loc, totalChunks, workerFirstChunkIdx);
  Value remainingChunksNonNeg = AC->create<arith::SelectOp>(
      loc, needZeroChunks, zeroIndex, remainingChunks);
  Value workerChunkCount = AC->create<arith::MinUIOp>(loc, chunksPerWorkerCeil,
                                                      remainingChunksNonNeg);

  /// Step 4: Convert chunk indices to iteration indices
  /// workerFirstIteration = workerFirstChunkIdx * chunkSize
  workerFirstChunk =
      AC->create<arith::MulIOp>(loc, workerFirstChunkIdx, chunkSize);

  /// workerMaxIterations = workerChunkCount * chunkSize
  Value workerMaxIterations =
      AC->create<arith::MulIOp>(loc, workerChunkCount, chunkSize);

  /// Handle edge case: last worker's iterations may exceed totalIterations
  /// remaining = max(0, totalIterations - workerFirstIteration)
  Value needZeroIters = AC->create<arith::CmpIOp>(
      loc, arith::CmpIPredicate::uge, workerFirstChunk, totalIterations);
  Value remainingIters =
      AC->create<arith::SubIOp>(loc, totalIterations, workerFirstChunk);
  Value remainingItersNonNeg = AC->create<arith::SelectOp>(
      loc, needZeroIters, zeroIndex, remainingIters);

  /// workerIterationCount = min(workerMaxIterations, remainingIters)
  workerIterationCount = AC->create<arith::MinUIOp>(loc, workerMaxIterations,
                                                    remainingItersNonNeg);

  /// Step 5: Determine if worker has any work
  /// hasWork = (iterationCount > 0)
  workerHasWork = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ugt,
                                            workerIterationCount, zeroIndex);
}

///===----------------------------------------------------------------------===///
// Pass Entry Point
///===----------------------------------------------------------------------===///

void ForLoweringPass::runOnOperation() {
  module = getOperation();

  ARTS_INFO_HEADER(ForLowering);
  ARTS_DEBUG_REGION(module.dump(););

  /// Walk all parallel EDTs and lower arts.for operations
  SmallVector<EdtOp, 4> parallelEdts;
  module.walk([&](EdtOp edt) {
    if (edt.getType() == EdtType::parallel)
      parallelEdts.push_back(edt);
  });

  for (EdtOp parallelEdt : parallelEdts)
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

  /// Construct codegen with runtime debug printing disabled by default.
  ArtsCodegen AC(module, /*debug=*/false);
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
  if (hasPostFor)
    createContinuationParallel(AC, parallelEdt, analysis, loc);

  /// Step 4: Clean up original parallel EDT
  /// First, erase the operations that have been moved/lowered externally
  /// This must happen BEFORE erasing the parallel EDT to avoid use-after-free

  /// Erase post-for operations from original parallel (now in continuation)
  for (Operation *op : llvm::reverse(analysis.opsAfterFor))
    op->erase();

  /// Erase arts.for operations (now lowered externally as epoch + task EDTs)
  for (ForOp forOp : analysis.forOps)
    forOp.erase();

  if (hasPreFor) {
    ARTS_INFO(" - Keeping pre-for work in original parallel EDT");
    /// Pre-for operations remain in the original parallel
  } else {
    /// No pre-for work - erase the now-empty original parallel entirely
    /// Also erase the original acquires that were dependencies for it
    ARTS_INFO(
        " - Erasing empty original parallel EDT and its original acquires");

    /// Collect original acquire ops to erase (dependencies of original
    /// parallel)
    SmallVector<DbAcquireOp> acquiresToErase;
    for (Value dep : parallelEdt.getDependencies()) {
      if (auto acqOp = dep.getDefiningOp<DbAcquireOp>())
        acquiresToErase.push_back(acqOp);
    }

    /// Erase the parallel first
    parallelEdt.erase();

    /// Then erase the orphaned acquires.
    /// An acquire is orphaned if its ptr result is unused. The guid may still
    /// be referenced but without a corresponding ptr use, the acquire serves
    /// no purpose.
    for (DbAcquireOp acqOp : acquiresToErase) {
      if (acqOp.getPtr().use_empty()) {
        ARTS_DEBUG("  - Erasing orphaned acquire for original parallel");
        acqOp.erase();
      }
    }
  }

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

///===----------------------------------------------------------------------===///
// Helper Functions for Reduction Handling
///===----------------------------------------------------------------------===///

/// Detect block arguments used for reduction stores in the loop body.
/// OpenMP lowering may use different variables than declared in reduction().
static DenseSet<Value> detectReductionBlockArgs(ForOp forOp) {
  DenseSet<Value> result;
  forOp.walk([&](memref::StoreOp store) {
    if (auto dbRef = store.getMemRef().getDefiningOp<DbRefOp>()) {
      if (auto blockArg = dbRef.getSource().dyn_cast<BlockArgument>()) {
        result.insert(blockArg);
        ARTS_DEBUG(
            "  - Detected old accumulator block arg in store: " << blockArg);
      }
    }
  });
  return result;
}

/// Check if a parallel block argument should be skipped. Returns true if the
/// argument is reduction-related.
static bool shouldSkipReductionArg(BlockArgument parallelArg,
                                   const ReductionInfo &redInfo,
                                   const DenseSet<Value> &reductionBlockArgs) {
  /// Final result accumulator - only needed by result EDT
  if (llvm::is_contained(redInfo.finalResultArgs, parallelArg)) {
    ARTS_DEBUG("  - Skipping final result accumulator (only for result EDT)");
    return true;
  }

  /// Partial accumulator - handled separately with full array acquisition
  if (llvm::is_contained(redInfo.partialAccumArgs, parallelArg)) {
    ARTS_DEBUG("  - Skipping partial accumulator (acquired separately)");
    return true;
  }

  /// Old accumulator from OpenMP lowering - replaced by partial accumulators
  if (reductionBlockArgs.contains(parallelArg)) {
    ARTS_DEBUG(
        "  - Skipping old accumulator (replaced by worker-based partials)");
    return true;
  }

  return false;
}

/// Collect db_ref operations that use old accumulator block args.
/// These operations will be skipped during cloning, and their results mapped to
/// myAccumulator. This fixes dominance violations where cloned operations would
/// reference parent scope values.
static void
collectOldAccumulatorDbRefs(ForOp forOp, Block &parallelBlock,
                            const DenseSet<Value> &reductionBlockArgs,
                            DenseSet<Operation *> &opsToSkip, IRMapping &mapper,
                            Value myAccumulator) {
  forOp.walk([&](DbRefOp dbRef) {
    if (auto blockArg = dbRef.getSource().dyn_cast<BlockArgument>()) {
      if (blockArg.getOwner() == &parallelBlock &&
          reductionBlockArgs.contains(blockArg)) {
        mapper.map(dbRef.getResult(), myAccumulator);
        opsToSkip.insert(dbRef.getOperation());
        ARTS_DEBUG(
            "  - Will skip cloning old accumulator db_ref, map to worker slot");
      }
    }
  });
}

void ForLoweringPass::cloneLoopBody(ArtsCodegen *AC, ForOp forOp,
                                    scf::ForOp chunkLoop, Value chunkOffset,
                                    IRMapping &mapper) {
  Location loc = forOp.getLoc();
  OpBuilder builder(AC->getContext());
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

///===----------------------------------------------------------------------===///
/// Reduction Support Implementation
///===----------------------------------------------------------------------===///
/// Allocate reduction DBs outside parallel EDT:
/// For each reduction variable:
/// 1. Reuse the existing reduction result DB if available (fallback allocates
///    one to preserve legacy behaviour).
/// 2. Allocate partial accumulators DB (array[numWorkers]) - intermediate
///    worker results.
///
/// Partial accumulators are initialized to identity and passed to the parallel
/// EDT. See file header for full reduction pattern details.
ReductionInfo ForLoweringPass::allocatePartialAccumulators(ArtsCodegen *AC,
                                                           ForOp forOp,
                                                           EdtOp parallelEdt,
                                                           Location loc,
                                                           bool splitMode) {
  ReductionInfo redInfo;
  Attribute loopAttr = getLoopMetadataAttr(forOp);
  redInfo.loopMetadataAttr = loopAttr;
  redInfo.loopLocation = forOp.getLoc();
  ValueRange reductionAccums = forOp.getReductionAccumulators();
  if (reductionAccums.empty())
    return redInfo;

  /// Try to find a stack-allocated DB sink in the parent block to mirror the
  /// final reduction result (used by host prints).
  if (!forOp->getBlock()->empty()) {
    for (Operation &op : forOp->getBlock()->getOperations()) {
      if (auto dbAlloc = dyn_cast<DbAllocOp>(&op)) {
        if (dbAlloc.getAllocType() == DbAllocType::stack &&
            dbAlloc.getDbMode() == DbMode::write) {
          redInfo.hostResultPtrs.push_back(dbAlloc.getPtr());
          break;
        }
      }
      if (&op == forOp.getOperation())
        break;
    }
  }

  ARTS_INFO(" - Allocating partial accumulators for " << reductionAccums.size()
                                                      << " reduction(s)");

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(parallelEdt);

  /// Use numWorkers from parallel EDT workers attribute
  Value numWorkers;
  if (auto workers = parallelEdt->getAttrOfType<workersAttr>(
          AttrNames::Operation::Workers))
    numWorkers = AC->createIndexConstant(workers.getValue(), loc);
  else if (parallelEdt.getConcurrency() == EdtConcurrency::internode)
    numWorkers = AC->create<GetTotalNodesOp>(loc).getResult();
  else
    numWorkers = AC->create<GetTotalWorkersOp>(loc).getResult();

  Block &parallelBlock = parallelEdt.getBody().front();
  ValueRange parentDeps = parallelEdt.getDependencies();

  /// Detect existing reduction handles inside the parallel EDT. The reduction
  /// pattern should have already allocated the final result datablock, so we
  /// try to reuse that handle instead of creating a new one.
  DenseSet<Value> reductionBlockArgs = detectReductionBlockArgs(forOp);
  SmallVector<BlockArgument> existingResultArgs;
  SmallVector<Value> existingResultPtrs;

  for (auto [idx, dep] : llvm::enumerate(parentDeps)) {
    BlockArgument arg = parallelBlock.getArgument(idx);
    if (reductionBlockArgs.contains(arg)) {
      existingResultArgs.push_back(arg);
      existingResultPtrs.push_back(dep);
    }
  }

  Value routeZero = AC->create<arith::ConstantIntOp>(loc, 0, 32);
  Value sizeOne = AC->createIndexConstant(1, loc);
  Value zeroIndex = AC->createIndexConstant(0, loc);

  for (auto [idx, redVar] : llvm::enumerate(reductionAccums)) {
    redInfo.reductionVars.push_back(redVar);

    /// Get the reduction variable type
    /// Reduction variables are typically memref<?xi32> or similar
    auto redMemRef = redVar.getType().dyn_cast<MemRefType>();
    /// Support scalar reductions by using the scalar type directly.
    Type elementType =
        redMemRef ? redMemRef.getElementType() : redVar.getType();
    Value identity = createZeroValue(AC, elementType, loc);
    if (!identity) {
      ARTS_ERROR("Unsupported reduction element type for initialization");
      continue;
    }

    /// Step 1: Reuse existing final result DB if available, otherwise fall back
    /// to a private allocation (legacy path).
    if (idx < existingResultArgs.size()) {
      redInfo.finalResultArgs.push_back(existingResultArgs[idx]);

      /// For split mode, we need to trace back to the original DbAllocOp
      /// rather than using the acquire ptr directly
      if (idx < existingResultPtrs.size()) {
        Value existingPtr = existingResultPtrs[idx];
        auto allocInfo = DatablockUtils::traceToDbAlloc(existingPtr);
        if (allocInfo) {
          auto [rootGuid, rootPtr] = *allocInfo;
          redInfo.finalResultGuids.push_back(rootGuid);
          redInfo.finalResultPtrs.push_back(rootPtr);
          ARTS_DEBUG("  - Traced reduction result to DbAllocOp from parent");
        } else {
          /// Fallback: use the acquire ptr directly (for non-split mode)
          redInfo.finalResultPtrs.push_back(existingPtr);
          ARTS_DEBUG(
              "  - Reusing pre-allocated reduction result datablock from "
              "parent (acquire ptr)");
        }
      }
      ARTS_DEBUG(
          "  - Reusing pre-allocated reduction result datablock from parent");
    } else {
      /// Allocate final result DB: sizes[1], elementSizes[1]
      auto finalAllocOp = AC->create<DbAllocOp>(
          loc, ArtsMode::inout, routeZero, DbAllocType::heap, DbMode::write,
          elementType, Value(), ValueRange{sizeOne}, ValueRange{sizeOne});
      Value finalGuid = finalAllocOp.getResult(0);
      Value finalPtr = finalAllocOp.getResult(1);

      /// Initialize directly using db_alloc result
      SmallVector<Value> finalInitIndices{zeroIndex};
      Value finalMemRef = AC->create<DbRefOp>(loc, finalPtr, finalInitIndices);
      SmallVector<Value> innerIndices{zeroIndex};
      AC->create<memref::StoreOp>(loc, identity, finalMemRef, innerIndices);

      /// Store handles for cleanup (db_free) after parallel EDT completes
      redInfo.finalResultGuids.push_back(finalGuid);
      redInfo.finalResultPtrs.push_back(finalPtr);

      /// In non-split mode, acquire and pass to parallel EDT as dependency
      /// In split mode, skip this - acquires happen directly in result/task
      /// EDTs
      if (!splitMode) {
        auto finalDepAcqOp = AC->create<DbAcquireOp>(
            loc, ArtsMode::inout, finalGuid, finalPtr, finalPtr.getType(),
            ValueRange{}, ValueRange{}, ValueRange{});
        Value finalDepPtr = finalDepAcqOp.getPtr();
        parallelEdt.appendDependency(finalDepPtr);
        BlockArgument finalPtrArg =
            parallelBlock.addArgument(finalDepPtr.getType(), loc);
        redInfo.finalResultArgs.push_back(finalPtrArg);
      }

      ARTS_DEBUG("  - Allocated fallback final result DB for reduction "
                 "variable");
    }

    SmallVector<Value> innerIndices{zeroIndex};

    /// Step 2: Allocate and initialize partial accumulators DB
    /// Allocate array[numWorkers] for partial accumulators
    /// Each worker gets exactly one accumulator slot
    auto allocOp = AC->create<DbAllocOp>(
        loc, ArtsMode::inout, routeZero, DbAllocType::heap, DbMode::write,
        elementType, Value(), ValueRange{numWorkers}, ValueRange{sizeOne});
    Value partialGuid = allocOp.getGuid();
    Value partialPtr = allocOp.getPtr();

    /// Initialize all partial accumulators to identity value (0 for addition)
    Value one = AC->createIndexConstant(1, loc);

    /// Loop to initialize each worker's partial accumulator
    /// for (i = 0; i < numWorkers; i++) { partialAccums[i] = identity; }
    Location loopLoc = redInfo.loopLocation.value_or(loc);
    auto initLoop = AC->create<scf::ForOp>(loopLoc, zeroIndex, numWorkers, one);
    if (loopAttr)
      initLoop->setAttr(AttrNames::LoopMetadata::Name, loopAttr);
    AC->setInsertionPointToStart(initLoop.getBody());
    Value workerIdx = initLoop.getInductionVar();

    /// Get the memref for this worker's partial accumulator
    /// Use partialPtr directly from db_alloc result
    SmallVector<Value> outerIndices{workerIdx};
    Value partialMemRef = AC->create<DbRefOp>(loc, partialPtr, outerIndices);

    /// Store identity value: partialAccums[workerIdx] = identity
    AC->create<memref::StoreOp>(loc, identity, partialMemRef, innerIndices);

    /// Return to parent scope after initialization
    AC->setInsertionPointAfter(initLoop);

    /// Store handles for cleanup (db_free) after parallel EDT completes
    redInfo.partialAccumGuids.push_back(partialGuid);
    redInfo.partialAccumPtrs.push_back(partialPtr);

    /// In non-split mode, acquire and pass to parallel EDT as dependency
    /// In split mode, skip this - acquires happen directly in result/task EDTs
    if (!splitMode) {
      SmallVector<Value> partialOffsets{zeroIndex};
      SmallVector<Value> partialSizes{numWorkers};
      auto depAcqOp = AC->create<DbAcquireOp>(
          loc, ArtsMode::inout, partialGuid, partialPtr, partialPtr.getType(),
          ValueRange{}, partialOffsets, partialSizes);
      Value depPtr = depAcqOp.getPtr();
      parallelEdt.appendDependency(depPtr);
      BlockArgument partialPtrArg =
          parallelBlock.addArgument(depPtr.getType(), loc);
      redInfo.partialAccumArgs.push_back(partialPtrArg);
    }

    ARTS_DEBUG("  - Allocated and initialized partial accumulator DB for "
               "reduction variable");
  }

  /// Store numWorkers for use by result EDT
  redInfo.numWorkers = numWorkers;

  return redInfo;
}

///===----------------------------------------------------------------------===///
/// Create result EDT to combine partial accumulators into final result.
///===----------------------------------------------------------------------===///
void ForLoweringPass::createResultEdt(ArtsCodegen *AC, EdtOp parallelEdt,
                                      ReductionInfo &redInfo, Location loc) {
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  if (redInfo.reductionVars.empty())
    return;

  ARTS_INFO(" - Creating single result EDT to combine partial accumulators");

  /// Use numWorkers from ReductionInfo (already computed during allocation)
  Value numWorkers = redInfo.numWorkers;
  if (!numWorkers) {
    ARTS_ERROR("Missing numWorkers in ReductionInfo");
    return;
  }

  /// Step 1: Acquire all partial accumulator DBs (READ-ONLY)
  /// Each partial accumulator DB contains one slot per worker.
  /// The result EDT reads ALL worker slots to combine them into the final
  /// result.
  SmallVector<Value> partialAcqPtrs;
  partialAcqPtrs.reserve(redInfo.partialAccumPtrs.size());
  for (uint64_t i = 0; i < redInfo.partialAccumPtrs.size(); i++) {
    Value partialPtr = redInfo.partialAccumPtrs[i];
    Value partialGuid = i < redInfo.partialAccumGuids.size()
                            ? redInfo.partialAccumGuids[i]
                            : Value();
    if (!partialPtr) {
      ARTS_ERROR("Missing partial accumulator ptr for reduction " << i);
      continue;
    }

    Value zeroIndex = AC->createIndexConstant(0, loc);
    SmallVector<Value> partialOffsets{zeroIndex};
    SmallVector<Value> partialSizes{numWorkers};

    /// Acquire entire partial accumulator array with <in> mode (read-only)
    /// Source directly from DbAllocOp (partialGuid, partialPtr)
    auto acqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::in, partialGuid, partialPtr, partialPtr.getType(),
        SmallVector<Value>{}, partialOffsets, partialSizes);

    partialAcqPtrs.push_back(acqOp.getResult(1));
  }

  /// Step 2: Acquire final result DBs (WRITE-ONLY)
  /// The final result DBs hold the combined reduction result.
  /// Each final result is a scalar (size=1).
  SmallVector<Value> finalResultAcqPtrs;
  finalResultAcqPtrs.reserve(redInfo.finalResultPtrs.size());
  for (uint64_t i = 0; i < redInfo.finalResultPtrs.size(); i++) {
    Value finalPtr = redInfo.finalResultPtrs[i];
    Value finalGuid = i < redInfo.finalResultGuids.size()
                          ? redInfo.finalResultGuids[i]
                          : Value();
    if (!finalPtr) {
      ARTS_ERROR("Missing final result ptr for reduction " << i);
      continue;
    }

    Value zeroIndex = AC->createIndexConstant(0, loc);
    Value sizeOne = AC->createIndexConstant(1, loc);
    SmallVector<Value> finalOffsets{zeroIndex};
    SmallVector<Value> finalSizes{sizeOne};

    /// Acquire final result with <out> mode (write-only)
    /// The result EDT will store the combined value, no need to read it first
    /// Source directly from DbAllocOp (finalGuid, finalPtr)
    auto acqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::out, finalGuid, finalPtr, finalPtr.getType(),
        SmallVector<Value>{}, finalOffsets, finalSizes);

    finalResultAcqPtrs.push_back(acqOp.getResult(1));
  }

  uint64_t reductionCount = std::min<uint64_t>(
      redInfo.reductionVars.size(),
      std::min(partialAcqPtrs.size(), finalResultAcqPtrs.size()));
  if (reductionCount != redInfo.reductionVars.size()) {
    ARTS_ERROR("Reduction/result mismatch: reductions="
               << redInfo.reductionVars.size()
               << " partials=" << partialAcqPtrs.size()
               << " finals=" << finalResultAcqPtrs.size());
  }
  if (reductionCount == 0) {
    ARTS_ERROR("No reduction handles available for result EDT");
    return;
  }

  /// Step 3: Create result EDT with dependencies
  /// The result EDT receives two sets of dependencies:
  ///   1. Partial accumulators (read) - input data to combine
  ///   2. Final results (write) - output location for combined result
  ///
  /// Dependency order matters for block argument indexing:
  ///   - First N arguments: partial accumulators
  ///   - Next M arguments: final results
  SmallVector<Value> edtDeps;
  edtDeps.reserve(partialAcqPtrs.size() + finalResultAcqPtrs.size());

  /// Add partial accumulators first (indices 0..N-1)
  for (Value dep : partialAcqPtrs)
    edtDeps.push_back(dep);

  /// Add final results second (indices N..N+M-1)
  for (Value dep : finalResultAcqPtrs)
    edtDeps.push_back(dep);

  Value routeZero = AC->create<arith::ConstantIntOp>(loc, 0, 32);
  auto resultEdt = AC->create<EdtOp>(
      loc, EdtType::task, EdtConcurrency::intranode, routeZero, edtDeps);

  Block &resultBlock = resultEdt.getBody().front();
  AC->setInsertionPointToStart(&resultBlock);

  /// Create block arguments for all EDT dependencies
  SmallVector<BlockArgument> depBlockArgs;
  depBlockArgs.reserve(edtDeps.size());
  for (Value dep : edtDeps)
    depBlockArgs.push_back(resultBlock.addArgument(dep.getType(), loc));

  if (depBlockArgs.size() < edtDeps.size()) {
    ARTS_ERROR("Result EDT block arguments do not cover all dependencies");
  }

  /// Step 5: Combine partial accumulators into final result
  /// For each reduction variable:
  ///   1. Get partial accumulator array (block arg at index i)
  ///   2. Get final result scalar (block arg at index N+i)
  ///   3. Loop over all workers: accumulate += partialAccums[worker_id]
  ///   4. Store final accumulated value to final result DB
  for (uint64_t i = 0; i < reductionCount; i++) {
    uint64_t partialIdx = i;
    uint64_t finalIdx = partialAcqPtrs.size() + i;

    /// Get block arguments for this reduction variable
    /// partialArg: handle to partial accumulator array[numWorkers]
    /// finalArg: handle to final result scalar
    BlockArgument partialArg = depBlockArgs[partialIdx];
    BlockArgument finalArg = depBlockArgs[finalIdx];

    Value zeroIndex = AC->createIndexConstant(0, loc);
    SmallVector<Value> indices{zeroIndex};

    /// Extract memref from DB handle using db_ref
    Value finalMemRef = AC->create<DbRefOp>(loc, finalArg, indices);

    /// Initialize accumulator to identity value (0 for addition)
    /// The loop will accumulate: result = identity + partial[0] + ... +
    /// partial[numWorkers-1]
    /// TODO: Support different reduction operations (min, max, multiply, etc.)
    auto memType = finalMemRef.getType().cast<MemRefType>();
    Type elemType = memType.getElementType();
    Value identity = createZeroValue(AC, elemType, loc);
    if (!identity) {
      ARTS_ERROR("Unsupported reduction element type");
      return;
    }

    /// Combine loop: for (w = 0; w < numWorkers; w++) { acc +=
    /// partialAccums[w]; }
    /// Note: Some workers may not have processed any work (if insufficient
    /// chunks) Their accumulator contains identity value, which doesn't affect
    /// sum
    Value zeroIdx = AC->createIndexConstant(0, loc);
    Value oneIdx = AC->createIndexConstant(1, loc);
    Location loopLoc = redInfo.loopLocation.value_or(loc);
    auto combineLoop = AC->create<scf::ForOp>(loopLoc, zeroIdx, numWorkers,
                                              oneIdx, ValueRange{identity});
    if (redInfo.loopMetadataAttr) {
      combineLoop->setAttr(AttrNames::LoopMetadata::Name,
                           redInfo.loopMetadataAttr);
      ARTS_DEBUG("  Set loop metadata on result EDT combine loop");

      /// Validate that metadata was properly set
      if (auto loopAttr = combineLoop->getAttr(AttrNames::LoopMetadata::Name)) {
        if (auto loopMetadata = dyn_cast<LoopMetadataAttr>(loopAttr)) {
          ARTS_DEBUG("    - Metadata validated: potentiallyParallel="
                     << loopMetadata.getPotentiallyParallel().getValue());
        }
      } else {
        ARTS_DEBUG(
            "    - WARNING: Loop metadata attribute not found after setting");
      }
    } else {
      ARTS_DEBUG(
          "  WARNING: No loop metadata available for result EDT combine loop");
    }

    AC->setInsertionPointToStart(combineLoop.getBody());
    Value workerIdx = combineLoop.getInductionVar();
    Value accumulator =
        combineLoop.getRegionIterArg(0); /// Current accumulated value

    /// Load this worker's partial result: partialAccums[workerIdx][0]
    Value workerSlot =
        AC->create<DbRefOp>(loc, partialArg, SmallVector<Value>{workerIdx});
    SmallVector<Value> workerElemIdx{zeroIdx};
    Value partialValue =
        AC->create<memref::LoadOp>(loc, workerSlot, workerElemIdx);

    /// Debug: print worker idx and its partial value
    if (AC->isDebug()) {
      Value workerIdxI64 = AC->castToInt(AC->Int64, workerIdx, loc);
      Value partialI64 = AC->castToInt(AC->Int64, partialValue, loc);
      AC->createPrintfCall(loc, "agg partial w=%lu val=%lu\n",
                           ValueRange{workerIdxI64, partialI64});
    }

    /// Combine with current accumulator: acc = acc + partialValue
    /// TODO: Support other reduction operations
    Value combined = AC->create<arith::AddIOp>(loc, accumulator, partialValue);

    /// Yield combined value for next iteration
    AC->create<scf::YieldOp>(loc, combined);

    /// Store final accumulated result to final result DB
    AC->setInsertionPointAfter(combineLoop);
    SmallVector<Value> finalIndices{zeroIdx};
    AC->create<memref::StoreOp>(loc, combineLoop.getResult(0), finalMemRef,
                                finalIndices);

    /// Debug: print final combined value
    if (AC->isDebug()) {
      Value finalValI64 =
          AC->castToInt(AC->Int64, combineLoop.getResult(0), loc);
      AC->createPrintfCall(loc, "agg final=%lu\n", ValueRange{finalValI64});
    }
  }

  /// Step 4: Release all DB handles and cleanup
  /// All dependencies passed to the result EDT are DB handles that were
  /// acquired from the parent EDT's block arguments. We must release them
  /// before the EDT ends.
  AC->setInsertionPointToEnd(&resultBlock);
  for (BlockArgument blockArg : depBlockArgs)
    AC->create<DbReleaseOp>(loc, blockArg);

  /// Add terminator to close the EDT region
  AC->create<YieldOp>(loc);

  /// NOTE: db_free operations are handled after epoch completion
  /// They must be placed OUTSIDE the parallel EDT region, after it completes

  ARTS_INFO(" - Result EDT created successfully");
}

///===----------------------------------------------------------------------===///
// Parallel Region Splitting Implementation
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

  /// Get numWorkers from original parallel EDT
  Value numWorkers;
  if (auto workers = originalParallel->getAttrOfType<workersAttr>(
          AttrNames::Operation::Workers))
    numWorkers = AC.createIndexConstant(workers.getValue(), loc);
  else if (originalParallel.getConcurrency() == EdtConcurrency::internode)
    numWorkers = AC.create<GetTotalNodesOp>(loc).getResult();
  else
    numWorkers = AC.create<GetTotalWorkersOp>(loc).getResult();

  Value zero = AC.createIndexConstant(0, loc);
  Value one = AC.createIndexConstant(1, loc);

  /// Create EpochOp wrapper for the for-body
  ARTS_DEBUG(" - Creating epoch wrapper for worker dispatch");
  auto forEpoch = AC.create<EpochOp>(loc);
  Region &epochRegion = forEpoch.getRegion();
  if (epochRegion.empty())
    epochRegion.push_back(new Block());
  Block &epochBlock = epochRegion.front();
  AC.setInsertionPointToStart(&epochBlock);

  /// Worker dispatch loop - creates one task EDT per worker
  ARTS_DEBUG(" - Creating worker dispatch loop");
  auto workerLoop = AC.create<scf::ForOp>(loc, zero, numWorkers, one);
  AC.setInsertionPointToStart(workerLoop.getBody());
  Value workerIV = workerLoop.getInductionVar();

  /// Create task EDT with DB rewiring
  LoopInfo loopInfo(&AC, forOp, numWorkers);
  createTaskEdtWithRewiring(&AC, loopInfo, forOp, workerIV, originalParallel,
                            redInfo);

  /// After worker loop, create result EDT (if reductions)
  AC.setInsertionPointAfter(workerLoop);
  if (!redInfo.reductionVars.empty()) {
    ARTS_DEBUG(" - Creating result EDT");
    createResultEdt(&AC, originalParallel, redInfo, loc);
  }

  /// Close epoch with yield
  AC.setInsertionPointToEnd(&epochBlock);
  AC.create<YieldOp>(loc);

  /// Reduction cleanup happens AFTER the epoch
  if (!redInfo.reductionVars.empty()) {
    AC.setInsertionPointAfter(forEpoch);
    Value zeroIdx = AC.createIndexConstant(0, loc);
    SmallVector<Value> zeroIndices{zeroIdx};

    /// Copy final results back to original reduction variables
    for (auto [idx, redVar] : llvm::enumerate(redInfo.reductionVars)) {
      if (idx >= redInfo.finalResultPtrs.size())
        continue;
      auto redMemRefTy = redVar.getType().dyn_cast<MemRefType>();
      if (!redMemRefTy)
        continue;

      Value finalHandle = redInfo.finalResultPtrs[idx];
      Value finalRef = AC.create<DbRefOp>(loc, finalHandle, zeroIndices);
      Value loaded = AC.create<memref::LoadOp>(loc, finalRef, zeroIndices);
      Value casted =
          AC.castParameter(redMemRefTy.getElementType(), loaded, loc);
      AC.create<memref::StoreOp>(loc, casted, redVar, zeroIndices);

      /// Mirror to host-visible stack DB if present
      if (idx < redInfo.hostResultPtrs.size()) {
        Value hostPtr = redInfo.hostResultPtrs[idx];
        Value hostRef = AC.create<DbRefOp>(loc, hostPtr, zeroIndices);
        AC.create<memref::StoreOp>(loc, casted, hostRef, zeroIndices);
      }
    }

    /// Free partial accumulator DBs
    for (uint64_t i = 0; i < redInfo.partialAccumGuids.size(); i++) {
      AC.create<DbFreeOp>(loc, redInfo.partialAccumGuids[i]);
      AC.create<DbFreeOp>(loc, redInfo.partialAccumPtrs[i]);
    }

    /// Free final result DBs
    for (uint64_t i = 0; i < redInfo.finalResultGuids.size(); i++) {
      AC.create<DbFreeOp>(loc, redInfo.finalResultGuids[i]);
      AC.create<DbFreeOp>(loc, redInfo.finalResultPtrs[i]);
    }
  } else {
    /// No reductions - still need to set insertion point after epoch
    /// so the next for loop (if any) inserts AFTER this epoch, not inside it
    AC.setInsertionPointAfter(forEpoch);
  }

  ARTS_INFO(" - arts.for lowering with DB rewiring complete");
}

EdtOp ForLoweringPass::createContinuationParallel(
    ArtsCodegen &AC, EdtOp originalParallel, ParallelRegionAnalysis &analysis,
    Location loc) {
  ARTS_INFO(" - Creating continuation parallel EDT for post-for work");

  /// We need to insert the continuation parallel AFTER the original parallel
  /// EDT (which contains the epochs).
  AC.setInsertionPointAfter(originalParallel);

  ValueRange originalDeps = originalParallel.getDependencies();
  Block &originalBlock = originalParallel.getBody().front();

  SmallVector<Value> newDeps;
  IRMapping argMapper;

  /// Track which original arg indices map to which new dep indices
  SmallVector<unsigned> origArgIndices;

  /// Reacquire DBs used by post-for operations
  for (BlockArgument origArg : analysis.depsUsedAfterFor) {
    unsigned idx = origArg.getArgNumber();
    if (idx >= originalDeps.size()) {
      ARTS_ERROR("Block argument index out of range: " << idx);
      continue;
    }

    Value dep = originalDeps[idx];

    /// Trace back to original DbAllocOp
    auto allocInfo = DatablockUtils::traceToDbAlloc(dep);
    if (!allocInfo) {
      ARTS_ERROR("Could not trace dependency to DbAllocOp for arg " << idx);
      continue;
    }
    auto [rootGuid, rootPtr] = *allocInfo;

    /// Get original acquire parameters
    auto origAcq = dep.getDefiningOp<DbAcquireOp>();
    ArtsMode mode = origAcq ? origAcq.getMode() : ArtsMode::inout;
    Type ptrType = origAcq ? origAcq.getPtr().getType() : rootPtr.getType();

    /// Create NEW acquire directly from DbAllocOp
    auto newAcq = AC.create<DbAcquireOp>(
        loc, mode, rootGuid, rootPtr, ptrType,
        origAcq ? SmallVector<Value>(origAcq.getIndices())
                : SmallVector<Value>{},
        origAcq ? SmallVector<Value>(origAcq.getOffsets())
                : SmallVector<Value>{},
        origAcq ? SmallVector<Value>(origAcq.getSizes())
                : SmallVector<Value>{});

    newDeps.push_back(newAcq.getPtr());
    origArgIndices.push_back(idx);

    ARTS_DEBUG("  - Reacquired datablock for arg " << idx);
  }

  /// Create continuation parallel EDT
  Value routeZero = AC.createIntConstant(0, AC.Int32, loc);
  auto contParallel =
      AC.create<EdtOp>(loc, EdtType::parallel,
                       originalParallel.getConcurrency(), routeZero, newDeps);

  /// Copy workers attribute if present
  if (auto workers = originalParallel.getWorkersAttr())
    contParallel->setAttr(AttrNames::Operation::Workers, workers);

  /// Add block arguments and create mapping
  Block &contBlock = contParallel.getBody().front();
  for (auto [i, dep] : llvm::enumerate(newDeps)) {
    BlockArgument newArg = contBlock.addArgument(dep.getType(), loc);
    /// Map original block arg to new block arg
    unsigned origIdx = origArgIndices[i];
    argMapper.map(originalBlock.getArgument(origIdx), newArg);
  }

  /// Clone post-for operations into continuation parallel
  AC.setInsertionPointToStart(&contBlock);
  for (Operation *op : analysis.opsAfterFor) {
    AC.clone(*op, argMapper);
  }

  /// Add yield terminator
  AC.setInsertionPointToEnd(&contBlock);
  if (contBlock.empty() || !contBlock.back().hasTrait<OpTrait::IsTerminator>())
    AC.create<YieldOp>(loc);

  ARTS_INFO(" - Continuation parallel EDT created with "
            << newDeps.size() << " reacquired dependencies");

  return contParallel;
}

EdtOp ForLoweringPass::createTaskEdtWithRewiring(
    ArtsCodegen *AC, LoopInfo &loopInfo, ForOp forOp, Value workerIdPlaceholder,
    EdtOp originalParallel, ReductionInfo &redInfo) {
  Location loc = forOp.getLoc();

  ARTS_DEBUG("  Creating task EDT with DB rewiring");

  /// Recreate numWorkers constant
  if (auto workers = originalParallel->getAttrOfType<workersAttr>(
          AttrNames::Operation::Workers))
    loopInfo.totalWorkers = AC->createIndexConstant(workers.getValue(), loc);
  else if (originalParallel.getConcurrency() == EdtConcurrency::internode)
    loopInfo.totalWorkers = AC->create<GetTotalNodesOp>(loc).getResult();
  else
    loopInfo.totalWorkers = AC->create<GetTotalWorkersOp>(loc).getResult();

  /// Compute worker iteration bounds
  loopInfo.computeWorkerIterationsBlock(workerIdPlaceholder);

  Value chunkOffset = loopInfo.workerFirstChunk;
  ValueRange parentDeps = originalParallel.getDependencies();
  Block &parallelBlock = originalParallel.getRegion().front();

  IRMapping mapper;

  /// Create scf.if to conditionally create task EDT only if worker has work
  auto ifOp = AC->create<scf::IfOp>(loc, loopInfo.workerHasWork,
                                    /*withElseRegion=*/false);
  AC->setInsertionPointToStart(&ifOp.getThenRegion().front());

  /// Detect reduction block arguments
  DenseSet<Value> reductionBlockArgs = redInfo.reductionVars.empty()
                                           ? DenseSet<Value>{}
                                           : detectReductionBlockArgs(forOp);

  Value zero = AC->createIndexConstant(0, loc);
  Value one = AC->createIndexConstant(1, loc);

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
    auto partialAcqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::inout, partialGuid, partialPtr, innerMemrefType,
        SmallVector<Value>{}, SmallVector<Value>{workerIdPlaceholder},
        SmallVector<Value>{one},
        /*offset_hints=*/SmallVector<Value>{workerIdPlaceholder},
        /*size_hints=*/SmallVector<Value>{one});

    reductionTaskDeps.push_back(partialAcqOp.getResult(1));
    reductionVarIndex[redInfo.reductionVars[i]] = i;

    ARTS_DEBUG("  - Acquired worker-local partial accumulator slot");
  }

  /// DB Rewiring: Acquire directly from DbAllocOp instead of block arguments
  SmallVector<Value> taskDeps;
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
    auto allocInfo = DatablockUtils::traceToDbAlloc(parentDep);
    if (!allocInfo) {
      ARTS_ERROR("Could not trace dependency " << idx << " to DbAllocOp");
      continue;
    }
    auto [rootGuid, rootPtr] = *allocInfo;

    /// Get offsets/sizes from parent acquire for hints
    SmallVector<Value> chunkOffsets =
        DatablockUtils::getOffsetsFromDb(parentDep);
    SmallVector<Value> chunkSizes = DatablockUtils::getSizesFromDb(parentDep);
    if (chunkOffsets.empty())
      chunkOffsets.push_back(AC->createIndexConstant(0, loc));
    if (chunkSizes.empty())
      chunkSizes.push_back(AC->createIndexConstant(1, loc));

    /// Create acquire directly from DbAllocOp with chunk hints
    auto chunkAcqOp = AC->create<DbAcquireOp>(
        loc, parentAcqOp.getMode(),
        rootGuid, /// From DbAllocOp
        rootPtr,  /// From DbAllocOp
        parentAcqOp.getPtr().getType(),
        SmallVector<Value>(parentAcqOp.getIndices().begin(),
                           parentAcqOp.getIndices().end()),
        chunkOffsets, chunkSizes,
        /*offset_hints=*/SmallVector<Value>{chunkOffset},
        /*size_hints=*/SmallVector<Value>{loopInfo.workerIterationCount});

    Value acquirePtr = chunkAcqOp.getResult(1);

    /// Replace uses within arts.for so cloned body uses acquire result
    Region &forRegion = forOp.getRegion();
    parallelArg.replaceUsesWithIf(acquirePtr, [&](OpOperand &use) {
      Region *useRegion = use.getOwner()->getParentRegion();
      return useRegion == &forRegion || forRegion.isAncestor(useRegion);
    });

    taskDeps.push_back(acquirePtr);
    ARTS_DEBUG("    - Created rewired acquire for dep " << idx);
  }

  /// Create task EDT
  /// Inherit concurrency from parent parallel EDT - if internode, route to
  /// worker node
  EdtConcurrency taskConcurrency = originalParallel.getConcurrency();
  Value routeValue;
  if (taskConcurrency == EdtConcurrency::internode) {
    /// Route to the worker node (workerIdPlaceholder is the node ID)
    routeValue = AC->castToInt(AC->Int32, workerIdPlaceholder, loc);
    ARTS_DEBUG("  - Using internode routing: worker " << workerIdPlaceholder);
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

  /// Create iteration loop
  scf::ForOp iterLoop =
      AC->create<scf::ForOp>(loc, zero, loopInfo.workerIterationCount, one);

  /// Set loop metadata on the chunked iteration loop.
  /// When handling reductions, update metadata to reflect that the chunked loop
  /// is now parallel for input arrays (but ONLY if there are no stencil/memref
  /// loop-carried dependencies).
  if (Attribute loopAttr = getLoopMetadataAttr(forOp)) {
    if (!reductionVarIndex.empty()) {
      /// We have reductions - check if we can mark as parallel
      if (auto origMeta = dyn_cast<LoopMetadataAttr>(loopAttr)) {
        /// Check if the ONLY source of inter-iteration deps was the reduction.
        /// If there are memref-based loop-carried deps (stencil patterns like
        /// a[i] = a[i-1] + a[i+1]), we CANNOT mark as parallel even after
        /// handling the reduction via partial accumulators.
        int64_t memrefDeps = 0;
        if (auto attr = origMeta.getMemrefsWithLoopCarriedDeps())
          memrefDeps = attr.getInt();

        if (memrefDeps == 0) {
          /// Safe: only scalar reduction deps existed, now broken by partial
          /// accumulators.
          auto parallelMeta = LoopMetadata::createParallelizedMetadata(
              forOp.getContext(), origMeta);
          iterLoop->setAttr(AttrNames::LoopMetadata::Name, parallelMeta);
          ARTS_DEBUG("  Updated loop metadata: potentiallyParallel=true "
                     "(reduction-only deps, no stencil patterns)");
        } else {
          /// NOT safe: there are stencil/memref loop-carried deps that are NOT
          /// broken by handling reductions. Keep original metadata.
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

  /// Map induction variable
  Value localIter = iterLoop.getInductionVar();
  Value globalIter = AC->create<arith::AddIOp>(loc, chunkOffset, localIter);
  Value globalIterScaled =
      AC->create<arith::MulIOp>(loc, globalIter, forOp.getStep()[0]);
  Value globalIdx = AC->create<arith::AddIOp>(loc, forOp.getLowerBound()[0],
                                              globalIterScaled);

  Block &forBody = forOp.getRegion().front();

  /// Map LLVM undef to identity value
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

  /// Clone external constants
  SetVector<Value> constantsToClone;
  Region *taskEdtRegion = iterLoop->getParentRegion();
  for (Operation &op : forBody.without_terminator()) {
    if (opsToSkip.contains(&op))
      continue;
    for (Value operand : op.getOperands()) {
      if (Operation *defOp = operand.getDefiningOp()) {
        if (defOp->hasTrait<OpTrait::ConstantLike>() &&
            !taskEdtRegion->isAncestor(defOp->getParentRegion())) {
          constantsToClone.insert(operand);
        }
      }
    }
  }

  for (Value constant : constantsToClone) {
    if (Operation *defOp = constant.getDefiningOp()) {
      Operation *clonedConst = AC->getBuilder().clone(*defOp);
      redMapper.map(constant, clonedConst->getResult(0));
    }
  }

  /// Clone loop body
  for (Operation &op : forBody.without_terminator()) {
    if (!opsToSkip.contains(&op))
      AC->clone(op, redMapper);
  }

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
// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createForLoweringPass() {
  return std::make_unique<ForLoweringPass>();
}
} // namespace arts
} // namespace mlir
