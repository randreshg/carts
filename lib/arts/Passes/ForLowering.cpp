///==========================================================================///
/// File: ForLowering.cpp
///
/// This pass lowers arts.for operations within arts.edt<parallel> by creating
/// a symbolic worker ID placeholder and chunk-based task EDT structure.
///==========================================================================///

#include "ArtsPassDetails.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/LoopMetadata.h"

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
// This class encapsulates worker iteration distribution logic
///===----------------------------------------------------------------------===///
/// LoopInfo: encapsulates worker partitioning for arts.for inside a parallel
/// EDT. It implements a simple block distribution:
///   - chunkSizeCeil = ceil(totalIterations / numWorkers)
///   - start = workerId * chunkSizeCeil
///   - count = min(chunkSizeCeil, max(0, totalIterations - start))
///   - hasWork = start < totalIterations
/// This avoids overlapping slices and keeps the math easy to audit.
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
/// Tracks DB handles for reduction pattern (see file header for full details).
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

  /// Create task EDT that processes a chunk using worker_id placeholder
  EdtOp createTaskEdt(ArtsCodegen *AC, LoopInfo &loopInfo, ForOp forOp,
                      Value workerIdPlaceholder, EdtOp parallelEdt,
                      ReductionInfo &redInfo);

  /// Clone loop body into task EDT's scf.for
  void cloneLoopBody(ArtsCodegen *AC, ForOp forOp, scf::ForOp chunkLoop,
                     Value chunkOffset, IRMapping &mapper);

  ///===--------------------------------------------------------------------===///
  /// Reduction Support
  ///===--------------------------------------------------------------------===///

  /// Allocate partial accumulators for reductions (one per worker)
  ReductionInfo allocatePartialAccumulators(ArtsCodegen *AC, ForOp forOp,
                                            EdtOp parallelEdt, Location loc);

  /// Create result EDT that combines partial accumulators
  void createResultEdt(ArtsCodegen *AC, EdtOp parallelEdt,
                       ReductionInfo &redInfo, Location loc);

  void lowerSingleFor(ArtsCodegen &AC, EdtOp parallelEdt, ForOp forOp);

  Attribute getLoopMetadataAttr(ForOp forOp);
};

} // namespace

///===----------------------------------------------------------------------===///
// LoopInfo Implementation - Work Partitioning Logic
///===----------------------------------------------------------------------===///
/// LoopInfo computes how to distribute loop iterations across workers using
/// block distribution. This ensures balanced work with minimal overhead.
///===----------------------------------------------------------------------===///

void LoopInfo::initialize() {
  Location loc = forOp.getLoc();

  /// Extract or default the chunk size from loop attributes
  if (auto chunkAttr = forOp->getAttrOfType<IntegerAttr>("chunk_size"))
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
  ///===--------------------------------------------------------------------===///
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
  /// Block Distribution Strategy 
  ///===--------------------------------------------------------------------===///
  /// This function computes the iteration bounds for a specific worker using
  /// block distribution to ensure balanced load across all workers.
  ///
  /// Algorithm:
  ///   1. chunkSizeCeil = ceil(totalIterations / totalWorkers)
  ///      - Each worker gets approximately equal work
  ///      - Last worker may get fewer iterations
  ///   2. offset = workerId * chunkSizeCeil
  ///      - Starting iteration for this worker
  ///      - Used as offset_hint in db_acquire
  ///   3. chunk = min(chunkSizeCeil, max(0, totalIterations - offset))
  ///      - Actual iterations for this worker
  ///      - Handles case where totalIterations < numWorkers
  ///      - Used as size_hint in db_acquire
  ///   4. hasWork = chunk > 0
  ///      - Determines if worker should create task EDT
  ///      - Workers beyond totalIterations skip EDT creation
  ///
  /// Example: N=10, workers=8
  ///   - chunkSizeCeil = ceil(10/8) = 2
  ///   - worker 0: offset=0, chunk=2  (iterations 0,1)
  ///   - worker 1: offset=2, chunk=2  (iterations 2,3)
  ///   - worker 4: offset=8, chunk=2  (iterations 8,9)
  ///   - worker 5: offset=10, chunk=0 (no work)
  ///===--------------------------------------------------------------------===///

  /// Step 1: Compute ceiling chunk size per worker
  /// chunkSizeCeil = ceil(totalIterations / totalWorkers)
  ///              = (totalIterations + totalWorkers - 1) / totalWorkers
  Value oneIndex = AC->createIndexConstant(1, loc);
  Value workersMinusOne =
      AC->create<arith::SubIOp>(loc, totalWorkers, oneIndex);
  Value adjustedTotal =
      AC->create<arith::AddIOp>(loc, totalIterations, workersMinusOne);
  Value chunkSizeCeil =
      AC->create<arith::DivUIOp>(loc, adjustedTotal, totalWorkers);

  /// Step 2: Compute worker's starting offset
  /// offset = workerId * chunkSizeCeil
  workerFirstChunk = AC->create<arith::MulIOp>(loc, workerId, chunkSizeCeil);

  /// Step 3: Compute actual chunk size for this worker
  /// Handle edge case: if offset >= totalIterations, worker has no work
  Value needZero = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::uge,
                                             workerFirstChunk, totalIterations);
  Value remaining =
      AC->create<arith::SubIOp>(loc, totalIterations, workerFirstChunk);
  Value remainingNonNeg = AC->create<arith::SelectOp>(
      loc, needZero, AC->createIndexConstant(0, loc), remaining);

  /// chunk = min(chunkSizeCeil, remaining)
  /// Ensures last worker doesn't exceed totalIterations
  workerIterationCount =
      AC->create<arith::MinUIOp>(loc, chunkSizeCeil, remainingNonNeg);

  /// Step 4: Determine if worker has any work
  /// hasWork = (chunk > 0)
  workerHasWork = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ugt,
                                            workerIterationCount,
                                            AC->createIndexConstant(0, loc));
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
  ARTS_INFO("Lowering parallel EDT");

  /// Find all arts.for operations within this parallel EDT
  SmallVector<ForOp, 4> forOps;
  parallelEdt.walk([&](ForOp forOp) { forOps.push_back(forOp); });
  if (forOps.empty()) {
    ARTS_DEBUG(" - No arts.for operations found, skipping");
    return;
  }

  ARTS_INFO(" - Found " << forOps.size() << " arts.for operation(s)");
  /// Construct codegen with runtime debug printing disabled by default.
  ArtsCodegen AC(module, /*debug=*/false);
  for (ForOp forOp : forOps)
    lowerSingleFor(AC, parallelEdt, forOp);
}

void ForLoweringPass::lowerSingleFor(ArtsCodegen &AC, EdtOp parallelEdt,
                                     ForOp forOp) {
  Location loc = forOp.getLoc();

  OpBuilder::InsertionGuard topGuard(AC.getBuilder());
  AC.setInsertionPoint(forOp);

  /// Create epoch wrapper to synchronize all spawned EDTs
  auto epochOp = AC.create<EpochOp>(loc);
  Region &epochRegion = epochOp.getRegion();
  if (epochRegion.empty())
    epochRegion.push_back(new Block());
  Block &epochBlock = epochRegion.front();
  AC.setInsertionPointToStart(&epochBlock);

  /// Allocate and initialize partial accumulators for reductions
  /// If the loop has reduction variables, allocate:
  /// - Partial accumulators: array[numWorkers], one per worker
  /// - Final result DBs: scalar per reduction variable
  /// All are initialized to identity values (e.g., 0 for sum).
  ReductionInfo redInfo;
  if (!forOp.getReductionAccumulators().empty()) {
    ARTS_INFO(" - Detected reduction(s), allocating partial accumulators");
    redInfo = allocatePartialAccumulators(&AC, forOp, parallelEdt, loc);
  }

  /// Create orchestrator pattern
  /// Only worker 0 (orchestrator) launches all worker EDTs and the reduction
  /// EDT. Other workers skip this block via the scf.if guard.

  /// Create arts.get_parallel_worker_id() placeholder
  /// ParallelEdtLowering will replace this with the outer worker loop IV
  Value currentWorkerId = AC.create<GetParallelWorkerIdOp>(loc).getResult();

  /// Check if current worker is the orchestrator (worker 0)
  Value zero = AC.createIndexConstant(0, loc);
  Value isOrchestrator = AC.create<arith::CmpIOp>(loc, arith::CmpIPredicate::eq,
                                                  currentWorkerId, zero);

  /// Create conditional block for orchestrator
  auto ifOrchestrator =
      AC.create<scf::IfOp>(loc, isOrchestrator, /*withElseRegion=*/false);
  Block &orchestratorBlock = ifOrchestrator.getThenRegion().front();
  Operation *orchestratorTerminator = orchestratorBlock.getTerminator();

  /// Compute work partitioning parameters
  AC.setInsertionPointToStart(&orchestratorBlock);
  Value numWorkers;
  if (auto workers = parallelEdt->getAttrOfType<workersAttr>("workers"))
    numWorkers = AC.createIndexConstant(workers.getValue(), loc);
  else
    numWorkers = AC.create<GetTotalWorkersOp>(loc).getResult();
  Value one = AC.createIndexConstant(1, loc);
  LoopInfo loopInfo(&AC, forOp, numWorkers);

  /// Launch all worker EDTs
  /// For each worker w in [0, numWorkers):
  ///   - Compute chunk bounds: offset, size
  ///   - Check if worker has work (hasWork)
  ///   - If hasWork, acquire input DBs with chunk hints
  ///   - Create worker task EDT to process the chunk
  auto workerLoop = AC.create<scf::ForOp>(loc, zero, numWorkers, one);
  AC.setInsertionPointToStart(workerLoop.getBody());
  Value workerIV = workerLoop.getInductionVar();
  createTaskEdt(&AC, loopInfo, forOp, workerIV, parallelEdt, redInfo);

  /// Create reduction EDT (if reductions exist)
  /// After launching all worker EDTs, create a single reduction EDT to combine
  /// partial accumulators into the final result.
  AC.setInsertionPoint(orchestratorTerminator);
  if (!redInfo.reductionVars.empty()) {
    /// Barrier ensures all worker EDTs complete before reduction EDT spawns
    AC.create<BarrierOp>(loc);
    createResultEdt(&AC, parallelEdt, redInfo, loc);
  }

  /// Finalize epoch and cleanup
  AC.setInsertionPointToEnd(&epochBlock);
  AC.create<YieldOp>(loc);
  AC.setInsertionPointAfter(epochOp);
  forOp.erase();

  /// After the parallel EDT finishes, copy final reduction results back into
  /// the original reduction variables using the existing acquire handles.
  if (!redInfo.reductionVars.empty()) {
    OpBuilder::InsertionGuard guard(AC.getBuilder());
    AC.setInsertionPointAfter(parallelEdt);
    Value zero = AC.createIndexConstant(0, loc);
    SmallVector<Value> zeroIdx{zero};
    for (auto [idx, redVar] : llvm::enumerate(redInfo.reductionVars)) {
      if (idx >= redInfo.finalResultPtrs.size())
        continue;
      auto redMemRefTy = redVar.getType().dyn_cast<MemRefType>();
      if (!redMemRefTy)
        continue;

      Value finalHandle = redInfo.finalResultPtrs[idx];
      Value finalRef = AC.create<DbRefOp>(loc, finalHandle, zeroIdx);
      Value loaded = AC.create<memref::LoadOp>(loc, finalRef, zeroIdx);
      Value casted =
          AC.castParameter(redMemRefTy.getElementType(), loaded, loc);
      AC.create<memref::StoreOp>(loc, casted, redVar, zeroIdx);

      /// If a host-visible stack DB was detected, mirror the value there too so
      /// host-side checks read the reduced result.
      if (idx < redInfo.hostResultPtrs.size()) {
        Value hostPtr = redInfo.hostResultPtrs[idx];
        Value hostRef = AC.create<DbRefOp>(loc, hostPtr, zeroIdx);
        AC.create<memref::StoreOp>(loc, casted, hostRef, zeroIdx);
      }
    }

    /// Free partial accumulator DBs (array[numWorkers] per reduction variable)
    for (uint64_t i = 0; i < redInfo.partialAccumGuids.size(); i++) {
      AC.create<DbFreeOp>(loc, redInfo.partialAccumGuids[i]);
      AC.create<DbFreeOp>(loc, redInfo.partialAccumPtrs[i]);
    }

    /// Free final result DBs (scalar per reduction variable)
    for (uint64_t i = 0; i < redInfo.finalResultGuids.size(); i++) {
      AC.create<DbFreeOp>(loc, redInfo.finalResultGuids[i]);
      AC.create<DbFreeOp>(loc, redInfo.finalResultPtrs[i]);
    }
  }
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

EdtOp ForLoweringPass::createTaskEdt(ArtsCodegen *AC, LoopInfo &loopInfo,
                                     ForOp forOp, Value workerIdPlaceholder,
                                     EdtOp parallelEdt,
                                     ReductionInfo &redInfo) {
  Location loc = forOp.getLoc();

  ARTS_DEBUG("  Creating task EDT using worker_id placeholder");

  /// Recreate numWorkers constant inside the parallel EDT to avoid external
  if (auto workers = parallelEdt->getAttrOfType<workersAttr>("workers"))
    loopInfo.totalWorkers = AC->createIndexConstant(workers.getValue(), loc);
  else
    loopInfo.totalWorkers = AC->create<GetTotalWorkersOp>(loc).getResult();

  /// Compute worker iteration bounds using the placeholder
  /// This generates runtime computation code that will be executed per-worker
  loopInfo.computeWorkerIterationsBlock(workerIdPlaceholder);

  /// Calculate chunk offset; workerFirstChunk is already in iteration space.
  Value chunkOffset = loopInfo.workerFirstChunk;

  /// Get parent EDT's dependencies (db_acquire results from outside parallel
  /// EDT) These become block arguments inside the parallel EDT
  ValueRange parentDeps = parallelEdt.getDependencies();
  Block &parallelBlock = parallelEdt.getRegion().front();

  /// Create mapping for cloning loop body
  IRMapping mapper;

  /// Create scf.if to conditionally create task EDT only if worker has work
  auto ifOp = AC->create<scf::IfOp>(loc, loopInfo.workerHasWork,
                                    /*withElseRegion=*/false);
  AC->setInsertionPointToStart(&ifOp.getThenRegion().front());

  /// Detect block arguments used for reduction stores (old accumulators)
  DenseSet<Value> reductionBlockArgs = redInfo.reductionVars.empty()
                                           ? DenseSet<Value>{}
                                           : detectReductionBlockArgs(forOp);

  Value zero = AC->createIndexConstant(0, loc);
  Value one = AC->createIndexConstant(1, loc);

  /// Acquire worker-local partial accumulator slot for reductions
  /// Each worker will write only to partialAccums[worker_id] to avoid
  /// contention
  SmallVector<Value> reductionTaskDeps;
  DenseMap<Value, uint64_t> reductionVarIndex;

  for (uint64_t i = 0; i < redInfo.reductionVars.size(); i++) {
    Value partialHandle = i < redInfo.partialAccumArgs.size()
                              ? redInfo.partialAccumArgs[i]
                              : Value();
    if (!partialHandle) {
      ARTS_ERROR("Missing partial accumulator handle for reduction " << i);
      continue;
    }

    /// Acquire only this worker's partial accumulator slot so the task EDT
    /// depends on a single element
    auto partialHandleType = partialHandle.getType().cast<MemRefType>();
    auto innerMemrefType =
        partialHandleType.getElementType().cast<MemRefType>();

    /// Acquire the worker's slice of the partial accumulator array
    auto partialAcqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::inout, Value(), partialHandle, innerMemrefType,
        SmallVector<Value>{}, SmallVector<Value>{workerIdPlaceholder},
        SmallVector<Value>{one},
        /*offset_hints=*/SmallVector<Value>{workerIdPlaceholder},
        /*size_hints=*/SmallVector<Value>{one});

    reductionTaskDeps.push_back(partialAcqOp.getResult(1));
    reductionVarIndex[redInfo.reductionVars[i]] = i;

    ARTS_DEBUG("  - Acquired worker-local partial accumulator slot for "
               "reduction "
               << i);
  }

  /// Chain db_acquire operations from parent EDT
  SmallVector<Value> taskDeps;
  for (auto [idx, parentDep] : llvm::enumerate(parentDeps)) {
    auto parentAcqOp = parentDep.getDefiningOp<DbAcquireOp>();
    if (!parentAcqOp)
      continue;

    BlockArgument parallelArg = parallelBlock.getArgument(idx);
    if (shouldSkipReductionArg(parallelArg, redInfo, reductionBlockArgs))
      continue;

    /// Acquire using parallel EDT's block arg (coarse-grained acquire) and
    /// attach chunk hints so DbPass can reuse the known partitioning.
    SmallVector<Value> chunkOffsets = getOffsetsFromDb(parentDep);
    SmallVector<Value> chunkSizes = getSizesFromDb(parentDep);
    if (chunkOffsets.empty())
      chunkOffsets.push_back(AC->createIndexConstant(0, loc));
    if (chunkSizes.empty())
      chunkSizes.push_back(AC->createIndexConstant(1, loc));

    auto chunkAcqOp = AC->create<DbAcquireOp>(
        loc, parentAcqOp.getMode(), Value(), parallelArg, parallelArg.getType(),
        /*indices=*/
        SmallVector<Value>(parentAcqOp.getIndices().begin(),
                           parentAcqOp.getIndices().end()),
        /*offsets=*/chunkOffsets,
        /*sizes=*/chunkSizes,
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
  }

  /// Create task EDT with route = workerIdPlaceholder
  /// For intranode tasks the route should remain local (0).
  Value routeValue = AC->createIntConstant(0, AC->Int32, loc);
  auto taskEdt = AC->create<EdtOp>(
      loc, EdtType::task, EdtConcurrency::intranode, routeValue, ValueRange{});

  Block &taskBlock = taskEdt.getBody().front();
  AC->setInsertionPointToStart(&taskBlock);

  /// Track where reduction dependencies start
  uint64_t reductionArgStart = taskDeps.size();

  /// Combine regular and reduction dependencies
  taskDeps.append(reductionTaskDeps.begin(), reductionTaskDeps.end());

  /// Add block arguments to task EDT for the acquire results
  for (uint64_t i = 0; i < taskDeps.size(); i++) {
    Value dep = taskDeps[i];
    BlockArgument taskArg = taskBlock.addArgument(dep.getType(), loc);

    /// For regular dependencies, map the acquire result to task block arg
    /// For reduction dependencies, we'll handle mapping separately below
    if (i < reductionArgStart)
      mapper.map(dep, taskArg);
  }

  /// Update task EDT operands
  SmallVector<Value> edtOperands;
  if (Value route = taskEdt.getRoute())
    edtOperands.push_back(route);
  for (Value dep : taskDeps)
    edtOperands.push_back(dep);
  taskEdt->setOperands(edtOperands);

  /// Create simple loop over worker's local iterations
  /// Each worker accumulates ALL its work into partialAccums[worker_id]
  scf::ForOp iterLoop =
      AC->create<scf::ForOp>(loc, zero, loopInfo.workerIterationCount, one);

  if (Attribute loopAttr = getLoopMetadataAttr(forOp))
    iterLoop->setAttr(AttrNames::LoopMetadata::Name, loopAttr);

  AC->setInsertionPointToStart(iterLoop.getBody());

  /// Map reduction variables to this worker's accumulator slot
  /// Each worker accumulates into partialAccums[worker_id_placeholder]
  IRMapping redMapper = mapper;
  DenseSet<Operation *> opsToSkip;

  for (auto [redVar, idx] : reductionVarIndex) {
    uint64_t argIndex = reductionArgStart + idx;
    if (argIndex >= taskBlock.getNumArguments()) {
      ARTS_ERROR("Reduction block argument index " << argIndex
                                                   << " out of range");
      continue;
    }

    BlockArgument partialArg = taskBlock.getArgument(argIndex);

    /// The accumulator is the worker-local inner memref.
    auto zeroIndex = AC->createIndexConstant(0, loc);
    Value myAccumulator =
        AC->create<DbRefOp>(loc, partialArg, SmallVector<Value>{zeroIndex});

    /// Map reduction variable to worker's accumulator
    redMapper.map(redVar, myAccumulator);
    ARTS_DEBUG("  - Mapped reduction variable to worker accumulator slot");

    /// Collect old accumulator db_refs to skip during cloning
    collectOldAccumulatorDbRefs(forOp, parallelBlock, reductionBlockArgs,
                                opsToSkip, redMapper, myAccumulator);
  }

  /// Map induction variable: local -> global index
  Value localIter = iterLoop.getInductionVar();
  Value globalIter = AC->create<arith::AddIOp>(loc, chunkOffset, localIter);
  Value globalIterScaled =
      AC->create<arith::MulIOp>(loc, globalIter, forOp.getStep()[0]);
  Value globalIdx = AC->create<arith::AddIOp>(loc, forOp.getLowerBound()[0],
                                              globalIterScaled);

  /// Debug print for per-worker iteration mapping.
  if (AC->isDebug()) {
    Value workerIdI64 = AC->castToInt(AC->Int64, workerIdPlaceholder, loc);
    Value chunkOffI64 = AC->castToInt(AC->Int64, chunkOffset, loc);
    Value localIterI64 = AC->castToInt(AC->Int64, localIter, loc);
    Value globalIdxI64 = AC->castToInt(AC->Int64, globalIdx, loc);
    AC->createPrintfCall(
        loc, "wrk=%lu chunkOff=%lu local=%lu global=%lu\n",
        ValueRange{workerIdI64, chunkOffI64, localIterI64, globalIdxI64});
  }

  Block &forBody = forOp.getRegion().front();

  /// Map LLVM undef constants used in the loop body to the reduction identity
  /// to avoid propagating uninitialized values into the accumulator.
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

  /// Clone external constants into task EDT region
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

  /// Clone loop body (skip old accumulator db_refs)
  for (Operation &op : forBody.without_terminator()) {
    if (!opsToSkip.contains(&op))
      AC->clone(op, redMapper);
  }

  /// Add yield terminator to task EDT if not already present
  AC->setInsertionPointToEnd(&taskBlock);
  if (taskBlock.empty() ||
      !taskBlock.back().hasTrait<OpTrait::IsTerminator>()) {
    AC->create<YieldOp>(loc);
  }

  /// Insert db_release operations before task EDT terminator
  AC->setInsertionPoint(taskBlock.getTerminator());
  for (uint64_t i = 0; i < taskDeps.size(); i++) {
    BlockArgument dbPtrArg = taskBlock.getArgument(i);
    AC->create<DbReleaseOp>(loc, dbPtrArg);
  }

  /// Exit the scf.if block - set insertion point after the conditional
  /// This ensures subsequent operations are placed correctly
  AC->setInsertionPointAfter(ifOp);

  return taskEdt;
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
                                                           Location loc) {
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
  if (auto workers = parallelEdt->getAttrOfType<workersAttr>("workers"))
    numWorkers = AC->createIndexConstant(workers.getValue(), loc);
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
      if (idx < existingResultPtrs.size())
        redInfo.finalResultPtrs.push_back(existingResultPtrs[idx]);
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

      /// Acquire for passing to parallel EDT as dependency
      auto finalDepAcqOp = AC->create<DbAcquireOp>(
          loc, ArtsMode::inout, finalGuid, finalPtr, finalPtr.getType(),
          ValueRange{}, ValueRange{}, ValueRange{});

      /// Add as dependency and create block argument for parallel EDT
      Value finalDepPtr = finalDepAcqOp.getPtr();
      parallelEdt.getDependenciesMutable().append(finalDepPtr);
      BlockArgument finalPtrArg =
          parallelBlock.addArgument(finalDepPtr.getType(), loc);
      redInfo.finalResultArgs.push_back(finalPtrArg);

      /// Store handles for cleanup (db_free) after parallel EDT completes
      redInfo.finalResultGuids.push_back(finalGuid);
      redInfo.finalResultPtrs.push_back(finalPtr);

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

    /// Acquire for passing to parallel EDT as dependency
    /// Worker EDTs will acquire their individual slots with <inout> mode
    /// Result EDT will acquire all slots with <in> mode (read-only)
    SmallVector<Value> partialOffsets{zeroIndex};
    SmallVector<Value> partialSizes{numWorkers};
    auto depAcqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::inout, partialGuid, partialPtr, partialPtr.getType(),
        ValueRange{}, partialOffsets, partialSizes);

    /// Add as dependency and create block argument for parallel EDT
    Value depPtr = depAcqOp.getPtr();
    parallelEdt.getDependenciesMutable().append(depPtr);
    BlockArgument partialPtrArg =
        parallelBlock.addArgument(depPtr.getType(), loc);

    redInfo.partialAccumArgs.push_back(partialPtrArg);

    /// Store handles for cleanup (db_free) after parallel EDT completes
    redInfo.partialAccumGuids.push_back(partialGuid);
    redInfo.partialAccumPtrs.push_back(partialPtr);

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
  partialAcqPtrs.reserve(redInfo.partialAccumArgs.size());
  for (uint64_t i = 0; i < redInfo.partialAccumArgs.size(); i++) {
    Value partialHandle = redInfo.partialAccumArgs[i];
    if (!partialHandle) {
      ARTS_ERROR("Missing partial accumulator block argument for reduction "
                 << i);
      continue;
    }

    Value zeroIndex = AC->createIndexConstant(0, loc);
    SmallVector<Value> partialOffsets{zeroIndex};
    SmallVector<Value> partialSizes{numWorkers};

    /// Acquire entire partial accumulator array with <in> mode (read-only)
    auto acqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::in, Value(), partialHandle, partialHandle.getType(),
        SmallVector<Value>{}, partialOffsets, partialSizes);

    partialAcqPtrs.push_back(acqOp.getResult(1));
  }

  /// Step 2: Acquire final result DBs (WRITE-ONLY)
  /// The final result DBs hold the combined reduction result.
  /// Each final result is a scalar (size=1).
  SmallVector<Value> finalResultAcqPtrs;
  finalResultAcqPtrs.reserve(redInfo.finalResultArgs.size());
  for (uint64_t i = 0; i < redInfo.finalResultArgs.size(); i++) {
    Value finalHandle = redInfo.finalResultArgs[i];
    if (!finalHandle) {
      ARTS_ERROR("Missing final result block argument for reduction " << i);
      continue;
    }

    Value zeroIndex = AC->createIndexConstant(0, loc);
    Value sizeOne = AC->createIndexConstant(1, loc);
    SmallVector<Value> finalOffsets{zeroIndex};
    SmallVector<Value> finalSizes{sizeOne};

    /// Acquire final result with <out> mode (write-only)
    /// The result EDT will store the combined value, no need to read it first
    auto acqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::out, Value(), finalHandle, finalHandle.getType(),
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

  /// Create the result EDT as a task EDT
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

  /// NOTE: db_free operations are handled by the caller (lowerSingleFor)
  /// They must be placed OUTSIDE the parallel EDT region, after it completes

  ARTS_INFO(" - Result EDT created successfully");
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
