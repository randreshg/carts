///==========================================================================
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
/// - These become block arguments inside the parallel EDT
/// - Task EDT acquires chain from parent results via block arguments
/// - Chaining: uses parent's result GUID and parallel EDT block argument
///
/// DB Partitioning Strategy (Deferred to DB Pass):
/// - ForLowering acquires the WHOLE DB without partitioning (no indices/sizes)
/// - This prevents incorrect overwrites at the ARTS memory model level
/// - The DB pass will later add chunk-based partitioning:
///   * Analyze access patterns in task EDTs
///   * Insert indices=[chunk_offset] sizes=[chunk_size] parameters
///   * Enable fine-grained memory transfers for parallel tasks
/// - Why deferred?
///   * DB pass has global view of all access patterns
///   * Can handle halo exchanges (overlapping regions) for stencil codes
///   * Can implement partial releases for modified regions only
///
/// Input IR:
///   %guid_0, %ptr_1 = arts.db_acquire[<out>] (%guid, %ptr)
///   arts.edt<parallel> (%ptr_1) {
///   ^bb0(%arg2: memref<?xmemref<memref<?xi32>>>):
///     arts.for %i = %lb to %ub step %step {
///       // loop body
///     }
///   }
///
/// Output IR:
///   %guid_0, %ptr_1 = arts.db_acquire[<out>] (%guid, %ptr)
///   arts.edt<parallel> (%ptr_1) {
///   ^bb0(%arg2: memref<?xmemref<memref<?xi32>>>):
///     %worker_id = arts.get_parallel_worker_id()
///     %chunk_offset = ... (computed from %worker_id)
///     %guid_1, %ptr_2 = arts.db_acquire[<out>] (%guid_0, %arg2)
///     arts.edt<task route=%worker_id> (%ptr_2) {
///       scf.for %local = 0 to %chunk_iter {
///         // modified body
///       }
///       arts.db_release(%ptr_2)
///     }
///     arts.db_release(%arg2)
///   }
///==========================================================================

#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"

#include "llvm/ADT/SmallVector.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(for_lowering);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

namespace {

//===----------------------------------------------------------------------===//
// LoopInfo - Information about a loop within a parallel EDT
// This class encapsulates worker iteration distribution logic
//===----------------------------------------------------------------------===//
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

//===----------------------------------------------------------------------===//
// ForLowering Pass Implementation
//===----------------------------------------------------------------------===//
struct ForLoweringPass : public arts::ForLoweringBase<ForLoweringPass> {
  void runOnOperation() override;

private:
  ModuleOp module;

  /// Process a parallel EDT containing arts_for operations
  void lowerParallelEdt(EdtOp parallelEdt);

  /// Create task EDT that processes a chunk using worker_id placeholder
  EdtOp createTaskEdt(ArtsCodegen *AC, LoopInfo &loopInfo, ForOp forOp,
                      Value workerIdPlaceholder, EdtOp parallelEdt);

  /// Create chunk-based db_acquire operations using worker_id
  SmallVector<Value> createChunkAcquires(ArtsCodegen *AC, OpBuilder &builder,
                                         Location loc, Value chunkOffset,
                                         Value chunkSize, ValueRange parentDeps,
                                         Block &parallelBlock, EdtOp taskEdt);

  /// Clone loop body into task EDT's scf.for
  void cloneLoopBody(ArtsCodegen *AC, ForOp forOp, scf::ForOp chunkLoop,
                     Value chunkOffset, IRMapping &mapper);
};

} // namespace

//===----------------------------------------------------------------------===//
// LoopInfo Implementation
//===----------------------------------------------------------------------===//

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

//===----------------------------------------------------------------------===//
// Pass Entry Point
//===----------------------------------------------------------------------===//

void ForLoweringPass::runOnOperation() {
  module = getOperation();

  ARTS_INFO_HEADER(ForLowering);
  ARTS_DEBUG_REGION(module.dump(););

  /// Walk all parallel EDTs and lower arts.for operations
  SmallVector<EdtOp, 4> parallelEdts;
  module.walk([&](EdtOp edt) {
    if (edt.getType() == EdtType::parallel) {
      parallelEdts.push_back(edt);
    }
  });

  for (EdtOp parallelEdt : parallelEdts) {
    lowerParallelEdt(parallelEdt);
  }

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

  /// TODO: For now, we only support a single arts.for per parallel EDT
  if (forOps.size() > 1) {
    ARTS_ERROR(
        "Multiple arts.for operations in parallel EDT not yet supported");
    return;
  }

  ArtsCodegen AC(module);
  Location loc = parallelEdt.getLoc();
  ForOp forOp = forOps[0];

  /// Get number of workers from Concurrency pass attribute
  Value numWorkers;
  if (auto workersAttr = parallelEdt->getAttrOfType<IntegerAttr>("workers")) {
    numWorkers = AC.createIndexConstant(workersAttr.getInt(), loc);
  } else {
    /// Fallback: query runtime
    numWorkers = AC.create<GetTotalWorkersOp>(loc).getResult();
  }

  /// Create arts.get_parallel_worker_id() placeholder
  /// This will be replaced by ParallelEdtLowering with the actual worker IV
  AC.setInsertionPoint(forOp);
  Value workerIdPlaceholder = AC.create<GetParallelWorkerIdOp>(loc).getResult();

  ARTS_INFO(" - Created worker ID placeholder");

  /// Create LoopInfo with the placeholder worker count
  LoopInfo loopInfo(&AC, forOp, numWorkers);

  /// Create a single task EDT that uses the worker_id placeholder
  EdtOp taskEdt =
      createTaskEdt(&AC, loopInfo, forOp, workerIdPlaceholder, parallelEdt);

  ARTS_DEBUG_REGION({
    if (taskEdt)
      ARTS_INFO(" - Created task EDT with worker_id placeholder");
    else
      ARTS_ERROR(" - Failed to create task EDT");
  });

  /// Erase the original arts.for operation (now replaced by task EDT)
  forOp.erase();
}

EdtOp ForLoweringPass::createTaskEdt(ArtsCodegen *AC, LoopInfo &loopInfo,
                                     ForOp forOp, Value workerIdPlaceholder,
                                     EdtOp parallelEdt) {
  Location loc = forOp.getLoc();
  OpBuilder builder(AC->getContext());
  builder.setInsertionPoint(forOp);

  ARTS_DEBUG("  Creating task EDT using worker_id placeholder");

  /// Compute worker iteration bounds using the placeholder
  /// This generates runtime computation code that will be executed per-worker
  loopInfo.computeWorkerIterationsBlock(workerIdPlaceholder);

  /// Calculate chunk offset = workerFirstChunk * chunkSize
  Value chunkOffset = AC->create<arith::MulIOp>(loc, loopInfo.workerFirstChunk,
                                                loopInfo.chunkSize);

  /// Calculate actual chunk size (may be smaller for last chunk)
  Value remaining =
      AC->create<arith::SubIOp>(loc, loopInfo.totalIterations, chunkOffset);
  Value actualChunkSize =
      AC->create<arith::MinUIOp>(loc, loopInfo.chunkSize, remaining);

  /// Create task EDT with route = workerIdPlaceholder
  /// ParallelEdtLowering will replace this placeholder with actual worker IV
  /// Cast workerIdPlaceholder from index to i32 for route parameter
  Value routeValue = AC->castToInt(AC->Int32, workerIdPlaceholder, loc);
  auto taskEdt = builder.create<EdtOp>(
      loc, EdtType::task, EdtConcurrency::intranode, routeValue, ValueRange{});
  taskEdt.setNoVerifyAttr(NoVerifyAttr::get(AC->getContext()));

  Block &taskBlock = taskEdt.getBody().front();
  builder.setInsertionPointToStart(&taskBlock);

  /// Get parent EDT's dependencies (db_acquire results from outside parallel
  /// EDT) These become block arguments inside the parallel EDT
  ValueRange parentDeps = parallelEdt.getDependencies();
  Block &parallelBlock = parallelEdt.getRegion().front();

  /// Create db_acquire operations for this task's chunk
  /// Chains from parent acquires using parallel EDT block arguments
  SmallVector<Value> taskDeps =
      createChunkAcquires(AC, builder, loc, chunkOffset, actualChunkSize,
                          parentDeps, parallelBlock, taskEdt);

  /// Create local iteration loop: scf.for %local_iter = 0 to
  /// %workerIterationCount
  Value zero = AC->createIndexConstant(0, loc);
  Value one = AC->createIndexConstant(1, loc);
  scf::ForOp chunkLoop =
      builder.create<scf::ForOp>(loc, zero, loopInfo.workerIterationCount, one);

  builder.setInsertionPointToStart(chunkLoop.getBody());

  /// Clone loop body with adjusted indices
  IRMapping mapper;
  cloneLoopBody(AC, forOp, chunkLoop, chunkOffset, mapper);

  /// Add yield terminator to task EDT if not already present
  builder.setInsertionPointToEnd(&taskBlock);
  if (taskBlock.empty() ||
      !taskBlock.back().hasTrait<OpTrait::IsTerminator>()) {
    builder.create<YieldOp>(loc);
  }

  /// Insert db_release operations before task EDT terminator
  builder.setInsertionPoint(taskBlock.getTerminator());
  for (size_t i = 0; i < taskDeps.size(); i++) {
    BlockArgument dbArg = taskBlock.getArgument(i);
    builder.create<DbReleaseOp>(loc, dbArg);
  }

  return taskEdt;
}

SmallVector<Value>
ForLoweringPass::createChunkAcquires(ArtsCodegen *AC, OpBuilder &builder,
                                     Location loc, Value chunkOffset,
                                     Value chunkSize, ValueRange parentDeps,
                                     Block &parallelBlock, EdtOp taskEdt) {
  SmallVector<Value> taskDeps;
  Block &taskBlock = taskEdt.getBody().front();

  ARTS_DEBUG("    Creating " << parentDeps.size() << " chunk-based acquires");

  /// For each db_acquire dependency from parent, create a chained acquire
  /// NOTE: For now, we don't partition the DB - each task acquires the
  /// entire DB. The DB pass will add partitioning logic later.
  for (auto [idx, parentDep] : llvm::enumerate(parentDeps)) {
    /// Check if this dependency is from a DbAcquireOp
    auto parentAcqOp = parentDep.getDefiningOp<DbAcquireOp>();
    if (!parentAcqOp) {
      ARTS_DEBUG("    Skipping non-DbAcquireOp dependency at index " << idx);
      continue;
    }

    /// Get the corresponding block argument in the parallel EDT
    /// Parent acquire result becomes a block argument inside parallel EDT
    BlockArgument parallelArg = parallelBlock.getArgument(idx);

    /// Create acquire for whole DB, chaining from parent's result
    /// Uses parent's result guid and parallel EDT block argument as sources
    auto chunkAcqOp = builder.create<DbAcquireOp>(
        loc,
        parentAcqOp.getMode(), // Same access mode as parent
        parentAcqOp.getGuid(), // Chain from parent's result guid
        parallelArg,           // Chain from parallel EDT block argument
        ValueRange{},          // indices: empty - acquire whole DB
        ValueRange{},          // offsets: empty
        ValueRange{}           // sizes: empty - acquire whole DB
    );

    /// Add block argument to task EDT for the acquired DB
    Type acquiredType = chunkAcqOp.getPtr().getType();
    taskBlock.addArgument(acquiredType, loc);

    /// Track the dependency
    taskDeps.push_back(chunkAcqOp.getPtr());

    ARTS_DEBUG(
        "    Created chunk acquire: indices=[chunkOffset], sizes=[chunkSize]");
  }

  /// Set task EDT operands to the acquired dependencies
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
  builder.setInsertionPointToStart(chunkLoop.getBody());

  /// Compute global index from local iteration variable
  /// global_iter = chunkOffset + local_iter
  /// global_idx = lowerBound + global_iter * step
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

  /// Clone all operations from arts.for body into chunk loop
  for (Operation &op : forBody.without_terminator())
    builder.clone(op, mapper);

  ARTS_DEBUG(
      "    Cloned " << std::distance(forBody.without_terminator().begin(),
                                     forBody.without_terminator().end())
                    << " operations into chunk loop");
}

//===----------------------------------------------------------------------===//
// Pass Factory
//===----------------------------------------------------------------------===//

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createForLoweringPass() {
  return std::make_unique<ForLoweringPass>();
}
} // namespace arts
} // namespace mlir
