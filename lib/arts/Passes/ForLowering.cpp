///==========================================================================///
/// File: ForLowering.cpp
///
/// This pass lowers arts.for operations within arts.edt<parallel> by creating
/// a symbolic worker ID placeholder and chunk-based task EDT structure.
///
/// Pass Overview:
/// 1. Find arts.edt<parallel> operations containing arts.for
/// 2. For each arts.for:
///    a. Create arts.get_parallel_worker_id() placeholder
///    b. Compute chunk bounds using the worker ID placeholder
///    c. Create chunk-based db_acquire operations (chaining from parent)
///    d. Create arts.edt<task route=%worker_id> with scf.for over chunk
///    e. Clone loop body with adjusted indices
/// 3. Leave arts_single constructs untouched (handled by ParallelEdtLowering)
///
/// DB Acquire Chaining Strategy:
/// - Parent acquires (outside parallel EDT) are passed as dependencies
/// - These become block arguments (DbHandleType) inside the parallel EDT
/// - Task EDT acquires chain from parent handles via block arguments
/// - Handles encapsulate both GUID and pointer, maintaining EDT isolation
///
/// DB Partitioning Strategy (Deferred to DB Pass):
/// - ForLowering acquires the whole DB without partitioning (no indices/sizes)
/// - This prevents incorrect overwrites at the ARTS memory model level
/// - The DB pass will later add chunk-based partitioning:
///   * Analyze access patterns in task EDTs
///   * Insert indices=[chunk_offset] sizes=[chunk_size] parameters
///   * Enable fine-grained memory transfers for parallel tasks
/// - Why deferred?
///   * DB pass has global view of all access patterns
///   * Can implement partial releases for modified regions only
///
/// Reduction Pattern (IMPORTANT):
/// When arts.for has reduction accumulators, the following pattern is used.
/// See ForLowering_Reduction_Design.md for detailed design rationale.
///
/// OUTSIDE parallel EDT:
///   1. Allocate partial accumulators DB (one accumulator per worker)
///      - Uses array[num_workers] for bounded memory (not array[num_chunks])
///      - Each worker gets a private accumulator to avoid contention
///   2. Initialize all partial accumulators to identity value (e.g., 0 for sum)
///   3. Acquire partial accumulators and pass to parallel EDT as dependency
///
/// INSIDE parallel EDT:
///   └─ INSIDE epoch (wraps all worker operations):
///       ├─ Each worker (if has work):
///       │   └─ Acquire full partial array: partialAccums[0:num_workers]
///       │       └─ Spawn task EDT with chunk data + partial array
///       │           └─ Process chunk, accumulate into partialAccums[worker_id]
///       │
///       └─ Spawn ONE result EDT (EdtType::single with nowait):
///           └─ Acquire all partial accumulators (read-only)
///               └─ Combine all worker results into final accumulator
///
/// OUTSIDE parallel EDT (after completion):
///   4. Free partial accumulators DB
///
/// Reduction Challenge (OpenMP Lowering Mismatch):
/// OpenMP lowering may declare one variable in reduction() attribute but
/// use a different value (e.g., parallel EDT block argument) in the loop body.
/// We handle this by post-clone patching of memref.store operations.
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
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/FileSystem.h"
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
  Value workerFirstChunk, workerChunkCount, workerIterationCount, workerHasWork;

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
///
struct ReductionInfo {
  /// Original reduction variables from arts.for operation
  SmallVector<Value> reductionVars;

  /// Original loop metadata attribute (if present)
  Attribute loopMetadataAttr;

  /// Location of the original arts.for operation
  std::optional<Location> loopLocation;

  /// Partial accumulators: array[num_workers] for intermediate results
  SmallVector<Value> partialAccumGuids; /// GUID handles (outside EDT)
  SmallVector<Value> partialAccumPtrs;  /// Pointer handles (outside EDT)
  SmallVector<Value> partialAccumArgs; /// Block arguments (inside parallel EDT)

  /// Final result accumulators: scalar for each reduction variable
  SmallVector<Value> finalResultGuids; /// GUID handles (outside EDT)
  SmallVector<Value> finalResultPtrs;  /// Pointer handles (outside EDT)
  SmallVector<Value> finalResultArgs;  /// Block arguments (inside parallel EDT)

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

  /// Create chunk-based db_acquire operations using worker_id
  SmallVector<Value> createChunkAcquires(ArtsCodegen *AC, Location loc,
                                         Value chunkOffset, Value chunkSize,
                                         ValueRange parentDeps,
                                         Block &parallelBlock, EdtOp taskEdt,
                                         IRMapping &mapper);

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

  /// Ensure sequential metadata has been imported into the module so the
  /// original arts.for operations carry loop metadata before lowering.
  void ensureMetadataAvailable();
  Attribute getLoopMetadataAttr(ForOp forOp);
};

} // namespace

///===----------------------------------------------------------------------===///
// LoopInfo Implementation
///===----------------------------------------------------------------------===///

void LoopInfo::initialize() {
  Location loc = forOp.getLoc();

  /// Distribution strategy - Block distribution
  if (auto chunkAttr = forOp->getAttrOfType<IntegerAttr>("chunk_size"))
    chunkSize =
        AC->createIndexConstant(std::max<int64_t>(1, chunkAttr.getInt()), loc);
  else
    chunkSize = AC->createIndexConstant(1, loc);

  /// Compute total iterations for the `for` loop:
  /// ((upper - lower) + (step - 1)) / step
  Value range = AC->create<arith::SubIOp>(loc, upperBound, lowerBound);
  Value adjustedRange = AC->create<arith::AddIOp>(
      loc, range,
      AC->create<arith::SubIOp>(loc, loopStep,
                                AC->createIndexConstant(1, loc)));
  totalIterations = AC->create<arith::DivUIOp>(loc, adjustedRange, loopStep);

  /// Compute total chunks for the `for` loop:
  /// (totalIterations + chunkSize - 1) / chunkSize
  Value adjustedIterations = AC->create<arith::AddIOp>(
      loc, totalIterations,
      AC->create<arith::SubIOp>(loc, chunkSize,
                                AC->createIndexConstant(1, loc)));
  totalChunks = AC->create<arith::DivUIOp>(loc, adjustedIterations, chunkSize);
}

void LoopInfo::computeWorkerIterationsBlock(Value workerId) {
  auto loc = forOp.getLoc();

  /// Divide total chunks evenly across workers
  /// chunksPerEdt = totalChunks / totalWorkers
  Value chunksPerEdt =
      AC->create<arith::DivUIOp>(loc, totalChunks, totalWorkers);

  /// Remainder chunks
  /// remainderChunks = totalChunks % totalWorkers
  Value remainderChunks =
      AC->create<arith::RemUIOp>(loc, totalChunks, totalWorkers);

  /// First few workers get an extra chunk if remainder > 0
  /// getsExtra = workerId < remainderChunks
  Value getsExtra = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ult,
                                              workerId, remainderChunks);
  /// extraChunk = getsExtra ? 1 : 0
  Value extraChunk = AC->castToIndex(getsExtra, loc);

  /// Total chunks this worker should handle
  /// workerChunkCount = chunksPerEdt + extraChunk
  workerChunkCount = AC->create<arith::AddIOp>(loc, chunksPerEdt, extraChunk);

  /// Calculate which chunk this worker starts with:
  ///  workerFirstChunk =
  ///    (workerId * chunksPerEdt) + min(workerId, remainder)
  Value baseStart = AC->create<arith::MulIOp>(loc, workerId, chunksPerEdt);
  Value prefixExtra =
      AC->create<arith::MinUIOp>(loc, workerId, remainderChunks);
  workerFirstChunk = AC->create<arith::AddIOp>(loc, baseStart, prefixExtra);

  /// Convert chunk-based values to iteration-based values
  /// workerFirstIter = workerFirstChunk * chunkSize
  Value workerFirstIter =
      AC->create<arith::MulIOp>(loc, workerFirstChunk, chunkSize);

  /// workerIterationCount = workerChunkCount * chunkSize
  Value theoreticalIterations =
      AC->create<arith::MulIOp>(loc, workerChunkCount, chunkSize);

  /// Handle edge case: if worker starts past total iterations
  Value zeroIndex = AC->createIndexConstant(0, loc);
  Value needZero = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ult,
                                             totalIterations, workerFirstIter);

  /// Calculate how many iterations are actually available to this worker
  Value remaining =
      AC->create<arith::SubIOp>(loc, totalIterations, workerFirstIter);

  /// Clamp to zero if worker starts beyond end (shouldn't happen in practice)
  Value remainingNonNeg =
      AC->create<arith::SelectOp>(loc, needZero, zeroIndex, remaining);

  /// Final iteration count = min(theoretical count, available count)
  workerIterationCount =
      AC->create<arith::MinUIOp>(loc, theoreticalIterations, remainingNonNeg);

  /// Determine if this worker has work to do
  /// workerHasWork = workerIterationCount > 0
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

  ensureMetadataAvailable();

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
  ArtsCodegen AC(module);
  for (ForOp forOp : forOps)
    lowerSingleFor(AC, parallelEdt, forOp);
}

void ForLoweringPass::lowerSingleFor(ArtsCodegen &AC, EdtOp parallelEdt,
                                     ForOp forOp) {
  Location loc = parallelEdt.getLoc();

  /// Replace arts.for with an epoch that encapsulates the lowered EDT graph.
  AC.setInsertionPoint(forOp);
  auto epochOp = AC.create<EpochOp>(loc);
  Region &epochRegion = epochOp.getRegion();
  if (epochRegion.empty())
    epochRegion.push_back(new Block());
  Block &epochBlock = epochRegion.front();
  AC.setInsertionPointToStart(&epochBlock);

  /// Get number of workers from Concurrency pass attribute
  Value numWorkers;
  if (auto workers = parallelEdt->getAttrOfType<workersAttr>("workers")) {
    numWorkers = AC.createIndexConstant(workers.getValue(), loc);
  } else {
    /// Fallback: query runtime
    numWorkers = AC.create<GetTotalWorkersOp>(loc).getResult();
  }

  /// Create arts.get_parallel_worker_id() placeholder
  /// This will be replaced by ParallelEdtLowering with the actual worker IV
  Value workerIdPlaceholder = AC.create<GetParallelWorkerIdOp>(loc).getResult();
  ARTS_INFO(" - Created worker ID placeholder");

  /// Create LoopInfo with the placeholder worker count
  LoopInfo loopInfo(&AC, forOp, numWorkers);

  /// Check for reductions and allocate partial accumulators
  ReductionInfo redInfo;
  if (!forOp.getReductionAccumulators().empty()) {
    ARTS_INFO(" - Detected reduction(s), allocating partial accumulators");
    redInfo = allocatePartialAccumulators(&AC, forOp, parallelEdt, loc);
  }

  /// Create a single task EDT that uses the worker_id placeholder
  EdtOp taskEdt = createTaskEdt(&AC, loopInfo, forOp, workerIdPlaceholder,
                                parallelEdt, redInfo);

  ARTS_DEBUG_REGION({
    if (taskEdt)
      ARTS_INFO(" - Created task EDT with worker_id placeholder");
    else
      ARTS_ERROR(" - Failed to create task EDT");
  });

  /// If reductions exist, create result EDT to combine partial accumulators
  if (!redInfo.reductionVars.empty()) {
    AC.setInsertionPointToEnd(&epochBlock);
    createResultEdt(&AC, parallelEdt, redInfo, loc);
  }

  /// Terminate epoch and clean up original for
  AC.setInsertionPointToEnd(&epochBlock);
  AC.create<YieldOp>(loc);
  AC.setInsertionPointAfter(epochOp);

  /// Erase the original arts.for operation (now replaced by epoch)
  forOp.erase();

  /// db_free operations are placed OUTSIDE the parallel EDT region.
  if (!redInfo.partialAccumGuids.empty()) {
    OpBuilder::InsertionGuard guard(AC.getBuilder());
    AC.setInsertionPointAfter(parallelEdt);

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

void ForLoweringPass::ensureMetadataAvailable() {
  if (module->hasAttr("arts.for_lowering.metadata_imported"))
    return;

  std::string metadataPath = ".carts-metadata.json";
  if (!llvm::sys::fs::exists(metadataPath)) {
    ARTS_DEBUG("Metadata file " << metadataPath
                                << " not found for ForLowering");
    return;
  }

  ArtsMetadataManager manager(module.getContext());
  if (manager.importFromJsonFile(module, metadataPath)) {
    ARTS_INFO("Imported metadata for ForLowering from " << metadataPath);
    module->setAttr("arts.for_lowering.metadata_imported",
                    mlir::UnitAttr::get(module.getContext()));
  } else {
    ARTS_WARN("Failed to import metadata for ForLowering from "
              << metadataPath);
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

/// Check if a parallel block argument should be skipped (not passed to task
/// EDT). Returns true if the argument is reduction-related.
static bool shouldSkipReductionArg(BlockArgument parallelArg,
                                   const ReductionInfo &redInfo,
                                   const DenseSet<Value> &reductionBlockArgs) {
  // Final result accumulator - only needed by result EDT
  if (llvm::is_contained(redInfo.finalResultArgs, parallelArg)) {
    ARTS_DEBUG("  - Skipping final result accumulator (only for result EDT)");
    return true;
  }

  // Partial accumulator - handled separately with full array acquisition
  if (llvm::is_contained(redInfo.partialAccumArgs, parallelArg)) {
    ARTS_DEBUG("  - Skipping partial accumulator (acquired separately)");
    return true;
  }

  // Old accumulator from OpenMP lowering - replaced by partial accumulators
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
  /// reference Constants are safe to duplicate, only external memrefs are
  /// problematic
  if (auto workers = parallelEdt->getAttrOfType<workersAttr>("workers"))
    loopInfo.totalWorkers = AC->createIndexConstant(workers.getValue(), loc);
  else
    loopInfo.totalWorkers = AC->create<GetTotalWorkersOp>(loc).getResult();

  /// Compute worker iteration bounds using the placeholder
  /// This generates runtime computation code that will be executed per-worker
  loopInfo.computeWorkerIterationsBlock(workerIdPlaceholder);

  /// Calculate chunk offset = workerFirstChunk * chunkSize
  Value chunkOffset = AC->create<arith::MulIOp>(loc, loopInfo.workerFirstChunk,
                                                loopInfo.chunkSize);

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

  /// Acquire full partial accumulator arrays for reductions
  /// Each worker will write to partialAccums[worker_id] to avoid contention
  SmallVector<Value> reductionTaskDeps;
  DenseMap<Value, uint64_t> reductionVarIndex;
  Value zeroOffset = AC->createIndexConstant(0, loc);

  for (uint64_t i = 0; i < redInfo.reductionVars.size(); i++) {
    Value partialHandle = i < redInfo.partialAccumArgs.size()
                              ? redInfo.partialAccumArgs[i]
                              : Value();
    if (!partialHandle) {
      ARTS_ERROR("Missing partial accumulator handle for reduction " << i);
      continue;
    }

    /// Acquire partialAccums[0:numWorkers] - each worker writes to [worker_id]
    auto partialAcqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::inout, Value(), partialHandle, partialHandle.getType(),
        SmallVector<Value>{}, SmallVector<Value>{zeroOffset},
        SmallVector<Value>{redInfo.numWorkers});

    reductionTaskDeps.push_back(partialAcqOp.getResult(1));
    reductionVarIndex[redInfo.reductionVars[i]] = i;

    ARTS_DEBUG("  - Acquired full partial accumulator array for reduction "
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

    /// Acquire using parallel EDT's block arg (coarse-grained acquire)
    /// Partitioning is done by the DbPass after the ForLowering pass
    SmallVector<Value> chunkOffsets, chunkSizes;
    chunkOffsets.push_back(chunkOffset);
    chunkSizes.push_back(loopInfo.workerIterationCount);
    auto chunkAcqOp = AC->create<DbAcquireOp>(
        loc, parentAcqOp.getMode(), Value(), parallelArg, parallelArg.getType(),
        ValueRange{}, chunkOffsets, chunkSizes);
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
  /// ParallelEdtLowering will replace this placeholder with actual worker IV
  Value routeValue = AC->castToInt(AC->Int32, workerIdPlaceholder, loc);
  auto taskEdt = AC->create<EdtOp>(
      loc, EdtType::task, EdtConcurrency::intranode, routeValue, ValueRange{});

  Block &taskBlock = taskEdt.getBody().front();
  AC->setInsertionPointToStart(&taskBlock);

  /// Track where reduction dependencies start
  uint64_t reductionArgStart = taskDeps.size();

  /// Combine regular and reduction dependencies
  taskDeps.append(reductionTaskDeps.begin(), reductionTaskDeps.end());

  /// Add block arguments to task EDT for the acquire results
  /// and update mapper to use block arguments (required by EDT verifier)
  for (uint64_t i = 0; i < taskDeps.size(); i++) {
    Value dep = taskDeps[i]; /// This is the acquire result from parallel EDT
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
  Value zero = AC->createIndexConstant(0, loc);
  Value one = AC->createIndexConstant(1, loc);

  /// Loop from 0 to workerIterationCount (local iterations for this worker)
  scf::ForOp iterLoop =
      AC->create<scf::ForOp>(loc, zero, loopInfo.workerIterationCount, one);

  if (Attribute loopAttr = getLoopMetadataAttr(forOp))
    iterLoop->setAttr(AttrNames::LoopMetadata::Name, loopAttr);

  AC->setInsertionPointToStart(iterLoop.getBody());

  /// Map reduction variables to THIS worker's accumulator slot
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

    /// Create db_ref to worker's slot: partialAccums[worker_id]
    BlockArgument partialArg = taskBlock.getArgument(argIndex);
    Value myAccumulator = AC->create<DbRefOp>(
        loc, partialArg, SmallVector<Value>{workerIdPlaceholder});

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

  Block &forBody = forOp.getRegion().front();
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

SmallVector<Value> ForLoweringPass::createChunkAcquires(
    ArtsCodegen *AC, Location loc, Value chunkOffset, Value chunkSize,
    ValueRange parentDeps, Block &parallelBlock, EdtOp taskEdt,
    IRMapping &mapper) {
  SmallVector<Value> taskDeps;
  Block &taskBlock = taskEdt.getBody().front();

  ARTS_DEBUG("    Creating " << parentDeps.size() << " chunk-based acquires");

  /// For each db_acquire dependency from parent, create a chained acquire
  /// We don't partition the DB - each task acquires the entire DB. The DB pass
  /// will add partitioning logic later.
  for (auto [idx, parentDep] : llvm::enumerate(parentDeps)) {
    /// Check if this dependency is from a DbAcquireOp (returns DbHandleType)
    auto parentAcqOp = parentDep.getDefiningOp<DbAcquireOp>();
    if (!parentAcqOp) {
      ARTS_DEBUG("    Skipping non-DbAcquireOp dependency at index " << idx);
      continue;
    }

    /// Get the corresponding block argument in the parallel EDT
    /// Parent acquire handle becomes a block argument inside parallel EDT
    BlockArgument parallelArg = parallelBlock.getArgument(idx);

    /// Add block argument to task EDT first (receives ptr from parallel EDT)
    BlockArgument taskArg = taskBlock.addArgument(parallelArg.getType(), loc);

    /// Create acquire for whole DB inside task EDT, using task's block arg
    auto chunkAcqOp = AC->create<DbAcquireOp>(
        loc, parentAcqOp.getMode(), Value(), taskArg, parallelArg.getType(),
        ValueRange{}, ValueRange{}, ValueRange{});

    /// DbAcquireOp returns (guid, ptr) tuple
    /// The ptr result is what the loop body should use (not the block arg)
    Value acquirePtr = chunkAcqOp.getResult(1);

    /// Map parallel EDT's block arg to the acquire's ptr result
    /// This way the loop body uses the locally acquired ptr, not external
    /// values
    mapper.map(parallelArg, acquirePtr);

    /// Track the parallel EDT's ptr as dependency (to pass to task EDT)
    taskDeps.push_back(parallelArg);

    ARTS_DEBUG(
        "    Created chunk acquire: indices=[chunkOffset], sizes=[chunkSize]");
  }

  /// Set task EDT operands to the acquired dependencies (handles)
  SmallVector<Value> edtOperands;
  if (Value route = taskEdt.getRoute())
    edtOperands.push_back(route);
  for (Value dep : taskDeps)
    edtOperands.push_back(dep);
  taskEdt->setOperands(edtOperands);

  return taskDeps;
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
/// Allocate reduction DBs outside parallel EDT.
///
/// For each reduction variable, allocates:
/// 1. Final result DB (scalar) - holds combined output
/// 2. Partial accumulators DB (array[numWorkers]) - intermediate worker results
///
/// Both initialized to identity value and passed to parallel EDT.
/// See file header for full reduction pattern details.
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

  ARTS_INFO(" - Allocating partial accumulators for " << reductionAccums.size()
                                                      << " reduction(s)");

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(parallelEdt);

  /// Use numWorkers from parallel EDT workers attribute
  /// This provides bounded memory regardless of problem size
  /// See ForLowering_Reduction_Design.md for detailed rationale
  Value numWorkers;
  if (auto workers = parallelEdt->getAttrOfType<workersAttr>("workers"))
    numWorkers = AC->createIndexConstant(workers.getValue(), loc);
  else
    numWorkers = AC->create<GetTotalWorkersOp>(loc).getResult();

  Block &parallelBlock = parallelEdt.getBody().front();

  for (Value redVar : reductionAccums) {
    redInfo.reductionVars.push_back(redVar);

    /// Get the reduction variable type
    /// Reduction variables are typically memref<?xi32> or similar
    auto redMemRef = redVar.getType().dyn_cast<MemRefType>();
    if (!redMemRef) {
      ARTS_ERROR("Reduction variable is not a memref type");
      continue;
    }

    /// Element type (e.g., i32 for memref<?xi32>)
    Type elementType = redMemRef.getElementType();

    /// Step 1: Allocate and initialize final result DB
    /// The final result DB is a scalar (size=1) that holds the combined result
    /// from all workers. It must be allocated OUTSIDE the parallel EDT so:
    ///   1. It can be initialized before parallel EDT runs
    ///   2. It can be accessed after parallel EDT completes
    ///   3. It can be passed as a dependency to the parallel EDT
    Value routeZero = AC->create<arith::ConstantIntOp>(loc, 0, 32);
    Value sizeOne = AC->createIndexConstant(1, loc);

    /// Allocate final result DB: sizes[1], elementSizes[1]
    auto finalAllocOp = AC->create<DbAllocOp>(
        loc, ArtsMode::inout, routeZero, DbAllocType::heap, DbMode::write,
        elementType, Value(), ValueRange{sizeOne}, ValueRange{sizeOne});
    Value finalGuid = finalAllocOp.getResult(0);
    Value finalPtr = finalAllocOp.getResult(1);

    /// Initialize final result to identity value (0 for addition)
    /// TODO: Support different reduction operations (min, max, multiply, etc.)
    Value identity = createZeroValue(AC, elementType, loc);
    if (!identity) {
      ARTS_ERROR("Unsupported reduction element type for initialization");
      continue;
    }

    /// Initialize directly using db_alloc result
    Value zeroIndex = AC->createIndexConstant(0, loc);
    SmallVector<Value> finalInitIndices{zeroIndex};
    Value finalMemRef = AC->create<DbRefOp>(loc, finalPtr, finalInitIndices);
    SmallVector<Value> innerIndices{zeroIndex};
    AC->create<memref::StoreOp>(loc, identity, finalMemRef, innerIndices);

    /// Acquire for passing to parallel EDT as dependency
    auto finalDepAcqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::inout, finalGuid, finalPtr, finalPtr.getType(),
        ValueRange{}, ValueRange{}, ValueRange{});
    Value finalDepPtr = finalDepAcqOp.getResult(1);

    /// Add as dependency and create block argument for parallel EDT
    parallelEdt.getDependenciesMutable().append(finalDepPtr);
    BlockArgument finalArg =
        parallelBlock.addArgument(finalDepPtr.getType(), loc);
    redInfo.finalResultArgs.push_back(finalArg);

    /// Store handles for cleanup (db_free) after parallel EDT completes
    redInfo.finalResultGuids.push_back(finalGuid);
    redInfo.finalResultPtrs.push_back(finalPtr);

    ARTS_DEBUG(
        "  - Allocated and initialized final result DB for reduction variable");

    /// Step 2: Allocate and initialize partial accumulators DB
    /// Allocate array[numWorkers] for partial accumulators
    /// Each worker gets exactly one accumulator slot
    auto allocOp = AC->create<DbAllocOp>(
        loc, ArtsMode::inout, routeZero, DbAllocType::heap, DbMode::write,
        elementType, Value(), ValueRange{numWorkers}, ValueRange{sizeOne});
    Value partialGuid = allocOp.getResult(0);
    Value partialPtr = allocOp.getResult(1);

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
    auto depAcqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::inout, partialGuid, partialPtr, partialPtr.getType(),
        ValueRange{}, ValueRange{}, ValueRange{});
    Value depPtr = depAcqOp.getResult(1);

    /// Add as dependency and create block argument for parallel EDT
    parallelEdt.getDependenciesMutable().append(depPtr);
    BlockArgument partialArg = parallelBlock.addArgument(depPtr.getType(), loc);
    redInfo.partialAccumArgs.push_back(partialArg);

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
///
/// Uses EdtType::single to ensure exactly ONE result EDT executes:
///   - Replaces previous scf.if (worker_id == 0) pattern
///   - Uses 'nowait' attribute (already inside epoch)
///   - Combines numWorkers partial accumulators into final result
///   - All worker partial results complete before result EDT due to epoch
///   semantics
///===----------------------------------------------------------------------===///
void ForLoweringPass::createResultEdt(ArtsCodegen *AC, EdtOp parallelEdt,
                                      ReductionInfo &redInfo, Location loc) {
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

  if (finalResultAcqPtrs.size() != redInfo.reductionVars.size()) {
    ARTS_ERROR("Final result count mismatch");
    assert(false && "Final result count mismatch");
  }

  /// Step 3: Create result EDT with dependencies
  /// The result EDT receives two sets of dependencies:
  ///   1. Partial accumulators (read) - input data to combine
  ///   2. Final results (write) - output location for combined result
  ///
  /// Dependency order matters for block argument indexing:
  ///   - First N arguments: partial accumulators
  ///   - Next M arguments: final results
  ///
  /// Use EdtType::single since only ONE result EDT should execute
  /// Use 'nowait' attribute since this EDT is already inside the epoch
  SmallVector<Value> edtDeps;
  edtDeps.reserve(partialAcqPtrs.size() + finalResultAcqPtrs.size());

  /// Add partial accumulators first (indices 0..N-1)
  for (Value dep : partialAcqPtrs)
    edtDeps.push_back(dep);

  /// Add final results second (indices N..N+M-1)
  for (Value dep : finalResultAcqPtrs)
    edtDeps.push_back(dep);

  /// Create the result EDT as a single EDT with nowait
  Value routeZero = AC->create<arith::ConstantIntOp>(loc, 0, 32);
  auto resultEdt = AC->create<EdtOp>(
      loc, EdtType::single, EdtConcurrency::intranode, routeZero, edtDeps);
  resultEdt->setAttr("nowait", UnitAttr::get(AC->getContext()));

  Block &resultBlock = resultEdt.getBody().front();
  AC->setInsertionPointToStart(&resultBlock);

  /// Create block arguments for all EDT dependencies
  /// These become the handles we use inside the EDT (EDT isolation requirement)
  SmallVector<BlockArgument> depBlockArgs;
  depBlockArgs.reserve(edtDeps.size());
  for (Value dep : edtDeps)
    depBlockArgs.push_back(resultBlock.addArgument(dep.getType(), loc));

  /// Sanity checks
  if (!redInfo.reductionVars.empty() &&
      partialAcqPtrs.size() != redInfo.reductionVars.size()) {
    ARTS_ERROR("Partial accumulator count ("
               << partialAcqPtrs.size()
               << ") does not match reduction variable count ("
               << redInfo.reductionVars.size() << ")");
  }

  if (depBlockArgs.size() < partialAcqPtrs.size() + finalResultAcqPtrs.size()) {
    ARTS_ERROR("Result EDT block arguments do not cover all dependencies");
    assert(false && "Result EDT block arguments do not cover all dependencies");
  }

  /// Step 5: Combine partial accumulators into final result
  /// For each reduction variable:
  ///   1. Get partial accumulator array (block arg at index i)
  ///   2. Get final result scalar (block arg at index N+i)
  ///   3. Loop over all workers: accumulate += partialAccums[worker_id]
  ///   4. Store final accumulated value to final result DB
  ///
  for (uint64_t i = 0; i < redInfo.reductionVars.size(); i++) {
    if (i >= partialAcqPtrs.size()) {
      ARTS_ERROR("Missing partial accumulator metadata for reduction " << i);
      assert(false && "Missing partial accumulator metadata for reduction");
      break;
    }
    uint64_t partialIdx = i;
    uint64_t finalIdx = partialAcqPtrs.size() + i;
    if (partialIdx >= depBlockArgs.size() || finalIdx >= depBlockArgs.size()) {
      ARTS_ERROR("Result EDT argument index out of range while processing "
                 "reduction "
                 << i);
      assert(
          false &&
          "Result EDT argument index out of range while processing reduction ");
      break;
    }

    /// Get block arguments for this reduction variable
    /// partialArg: handle to partial accumulator array[numWorkers]
    /// finalArg: handle to final result scalar
    BlockArgument partialArg = depBlockArgs[partialIdx];
    BlockArgument finalArg = depBlockArgs[finalIdx];

    Value zeroIndex = AC->createIndexConstant(0, loc);
    SmallVector<Value> indices{zeroIndex};

    /// Extract memrefs from DB handles using db_ref
    Value partialMemRef = AC->create<DbRefOp>(loc, partialArg, indices);
    Value finalMemRef = AC->create<DbRefOp>(loc, finalArg, indices);

    /// Initialize accumulator to identity value (0 for addition)
    /// The loop will accumulate: result = identity + partial[0] + ... +
    /// partial[numWorkers-1]
    /// TODO: Support different reduction operations (min, max, multiply, etc.)
    auto memType = partialMemRef.getType().cast<MemRefType>();
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

    /// Load this worker's partial result: partialAccums[workerIdx]
    SmallVector<Value> workerIndices{workerIdx};
    Value partialValue =
        AC->create<memref::LoadOp>(loc, partialMemRef, workerIndices);

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

  ARTS_INFO(" - Single result EDT with nowait created successfully");
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
