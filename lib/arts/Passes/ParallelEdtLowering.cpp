///==========================================================================
/// File: ParallelEdtLowering.cpp
/// Complete implementation of parallel EDT lowering pass that transforms
/// arts.edt<parallel> operations into task graphs before datablock lowering.
///
/// This pass implements parallel EDT lowering that must execute before
/// datablock operations are lowered to ensure proper dependency management.
///==========================================================================

#include "ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Block.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/Matchers.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Transforms/RegionUtils.h"

#include "llvm/ADT/STLExtras.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "arts/Utils/ArtsDebug.h"
#include "polygeist/Ops.h"
ARTS_DEBUG_SETUP(parallel_edt_lowering);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

namespace {

//===----------------------------------------------------------------------===//
// ParallelEdtLowering - Lowers parallel EDTs into explicit task EDTs.
//
// This class implements OpenMP static scheduling semantics for ARTS.
// When an arts.edt<parallel> contains an arts.for, it transforms it into:
// - For static without chunk_size: Block distribution of contiguous iteration
//   ranges
// - For static with chunk_size: Round-robin (cyclic) distribution of chunks
// - Inter-node: Round-robin across nodes via route for cyclic, direct route for
//   block distribution
// - Intra-node: All tasks with route=0 for free worker assignment
// - Inner arts.edt<task> operations that execute chunks or blocks of work
//===----------------------------------------------------------------------===//
class ParallelEdtLowering {
public:
  ArtsCodegen *AC;
  Location loc;

  //===--------------------------------------------------------------------===//
  // Data Structures
  //===--------------------------------------------------------------------===//

  /// Information about the parallel EDT and distribution strategy
  struct ParallelInfo {
    Value lowerBound;          // Lower bound of parallel workers (usually 0)
    Value upperBound;          // Upper bound (number of workers)
    bool isInterNode;          // Inter-node vs intra-node parallelism
    bool isCyclicDistribution; // Cyclic vs block distribution
  };

  /// Information about the loop being parallelized
  struct LoopInfo {
    Value lowerBound;      // Loop lower bound (e.g., 0)
    Value upperBound;      // Loop upper bound (e.g., N)
    Value loopStep;        // Loop step (usually 1)
    Value chunkSize;       // Chunk size for cyclic distribution
    Value totalWorkers;    // Number of workers
    Value totalIterations; // Total loop iterations: (upper - lower) / step
    Value totalChunks;     // Total chunks: ceil(totalIterations / chunkSize)
  };

  /// Worker-specific iteration bounds for a single worker
  /// Used to calculate what iterations each worker should execute
  struct WorkerIterationInfo {
    Value lowerBound;           // Original loop lower bound
    Value upperBound;           // Original loop upper bound
    Value loopStep;             // Original loop step
    Value chunkSize;            // Chunk size (1 for block distribution)
    Value totalWorkers;         // Total number of workers
    Value totalIterations;      // Total iterations in the loop
    Value totalChunks;          // Total chunks
    Value workerFirstChunk;     // First chunk assigned to this worker
    Value workerChunkCount;     // Number of chunks for this worker
    Value workerIterationCount; // Total iterations for this worker
  };

  /// Result of creating dependencies for a worker
  struct WorkerDepsResult {
    SmallVector<Value> deps;                // Dependency values
    SmallVector<DbAcquireOp, 4> acquireOps; // DbAcquire operations created
  };

  /// Information about a reduction variable
  /// Stores both the original accumulator and the preallocated partial buffers
  struct ReductionInfo {
    Value originalAcc;            // Original reduction accumulator
    MemRefType originalType;      // Type of original accumulator
    MemRefType elementBufferType; // Type for element buffer
    Type scalarType;              // Scalar type being reduced
    DbAllocOp partialAlloc;       // Preallocated partial result buffer
    DbAllocOp originalAlloc;      // Original accumulator allocation
  };

  /// Helper structure for accessing reduction buffer elements
  struct ReductionAccess {
    Value buffer;               // Buffer to access
    SmallVector<Value> indices; // Indices for access
  };

  /// Information about a single construct within a parallel EDT
  /// Single constructs execute on only one worker using atomic synchronization
  struct SingleConstructInfo {
    EdtOp singleEdt;         // The single EDT operation
    Value atomicFlagGuid;    // GUID for the atomic flag datablock
    Value atomicFlagPtr;     // Pointer to the atomic flag datablock
    Operation *insertBefore; // Where to insert single code in worker EDT
  };

  ParallelEdtLowering(ArtsCodegen *AC, Location loc) : AC(AC), loc(loc) {}

  //===----------------------------------------------------------------------===//
  /// Main Entry Point: Lower a parallel EDT
  ///
  /// This is the main transformation that converts a parallel EDT into a task
  /// graph. The process involves:
  ///
  /// 1. Detecting the distribution strategy (block vs cyclic)
  /// 2. Computing loop bounds and worker assignments
  /// 3. Preallocating reduction buffers (if needed)
  /// 4. Creating worker EDTs with proper dependencies
  /// 5. Creating reduction aggregator EDT (if needed)
  ///
  /// Example Input:
  ///   arts.edt<parallel> {
  ///     arts.for %i = 0 to 100 step 1 chunk_size=4 {
  ///       c[%i] = a[%i] + b[%i]
  ///     }
  ///   }
  ///
  /// Example Output (cyclic distribution, 8 workers):
  ///   arts.epoch {
  ///     scf.for %worker = 0 to 8 step 1 {
  ///       arts.edt<task> {
  ///         // Worker handles chunks: worker, worker+8, worker+16, ...
  ///         scf.for %localChunk = ... {
  ///           scf.for %i = ... { c[%i] = a[%i] + b[%i] }
  ///         }
  ///       }
  ///     }
  ///   }
  //===----------------------------------------------------------------------===//
  LogicalResult lowerParallelEdt(EdtOp op) {
    if (!op.getRegion().hasOneBlock()) {
      ARTS_ERROR("Parallel EDT must have exactly one block");
      assert(false);
      return failure();
    }

    OpBuilder::InsertionGuard IG(AC->getBuilder());
    AC->setInsertionPoint(op);

    //===------------------------------------------------------------------===//
    // Step 1: Create epoch wrapper for synchronization
    //===------------------------------------------------------------------===//
    EpochOp epochOp = createEpochWrapper();
    Block &epochBody = epochOp.getRegion().front();
    AC->setInsertionPointToEnd(&epochBody);

    //===------------------------------------------------------------------===//
    // Step 2: Find contained for loop (if any)
    //===------------------------------------------------------------------===//
    arts::ForOp containedForOp = findContainedForOp(op);
    if (containedForOp) {
      ARTS_INFO("Lowering parallel EDT with contained arts.for");

      //===----------------------------------------------------------------===//
      // Step 3: Determine parallelism strategy and loop information
      //===----------------------------------------------------------------===//
      auto [lb, ub, isInterNode] = determineParallelismStrategy(op);

      // Reset reduction state
      activeReductions.clear();
      activeLoopCount = Value();
      activeLoopOp = nullptr;

      // Detect distribution strategy based on chunk_size attribute
      // - Block distribution: schedule(static) or no schedule attribute
      // - Cyclic distribution: schedule(static, N) with explicit chunk_size
      bool isCyclicDistribution = false;
      if (auto chunkAttr =
              containedForOp->getAttrOfType<IntegerAttr>("chunk_size")) {
        isCyclicDistribution = true;
      }

      // Build loop information structure
      LoopInfo loopInfo;
      loopInfo.lowerBound = containedForOp.getLowerBound()[0];
      loopInfo.upperBound = containedForOp.getUpperBound()[0];
      loopInfo.loopStep = containedForOp.getStep()[0];
      loopInfo.totalWorkers = ub;
      computeTotalIterationsAndChunkSize(containedForOp, loopInfo);

      //===----------------------------------------------------------------===//
      // Step 3.5: Detect and prepare single constructs
      //
      // Single constructs within the parallel EDT need atomic flags for
      // synchronization. We allocate these flags HERE in the epoch scope
      // so they're accessible to all worker EDTs.
      //===----------------------------------------------------------------===//
      SmallVector<EdtOp> singleEdts = findSingleConstructs(op);
      SmallVector<SingleConstructInfo> singleConstructs;

      if (!singleEdts.empty()) {
        ARTS_INFO("Found " << singleEdts.size()
                           << " single construct(s) in parallel EDT");

        for (EdtOp singleEdt : singleEdts) {
          // Allocate atomic flag for this single construct
          // Each worker will acquire it individually
          auto [flagGuid, flagPtr] = allocateSingleFlag();

          SingleConstructInfo info;
          info.singleEdt = singleEdt;
          info.atomicFlagGuid = flagGuid;
          info.atomicFlagPtr = flagPtr;
          info.insertBefore = nullptr;
          singleConstructs.push_back(info);

          ARTS_DEBUG("Prepared single construct with flag guid=" << flagGuid);
        }
      }

      //===----------------------------------------------------------------===//
      // Step 4: Preallocate reduction buffers BEFORE creating worker EDTs
      //
      // For reductions, we preallocate partial result buffers with one slot per
      // worker. Each worker accumulates its partial result in its dedicated
      // slot, which is later combined to produce the final reduction value.
      //===----------------------------------------------------------------===//
      Value slotCount = ub;
      if (isCyclicDistribution)
        ARTS_DEBUG(
            "Using cyclic distribution with chunk_size=" << loopInfo.chunkSize);
      else
        ARTS_DEBUG("Using block distribution (contiguous ranges per worker)");
      prepareReductionBuffers(containedForOp, slotCount);
      activeLoopCount = slotCount;

      //===----------------------------------------------------------------===//
      // Step 5: Lower to worker EDTs
      //===----------------------------------------------------------------===//
      if (isCyclicDistribution) {
        lowerStaticCyclic(op, containedForOp, lb, ub, isInterNode,
                          singleConstructs);
      } else {
        lowerStaticBlock(op, containedForOp, lb, ub, isInterNode,
                         singleConstructs);
      }
      for (auto &singleInfo : singleConstructs) {
        if (!singleInfo.singleEdt)
          continue;
        ARTS_DEBUG("Erasing original single EDT after lowering");
        singleInfo.singleEdt.erase();
        singleInfo.singleEdt = nullptr;
      }

      //===----------------------------------------------------------------===//
      // Step 6: Create reduction aggregator EDT (if reductions exist)
      //
      // After all worker EDTs complete, aggregate partial results back
      // into the original reduction variable.
      //===----------------------------------------------------------------===//
      if (!activeReductions.empty()) {
        if (!activeLoopOp || failed(createReductionAggregator(
                                 activeLoopCount, activeLoopOp, op)))
          return failure();
        activeReductions.clear();
        activeLoopOp = nullptr;
        activeLoopCount = Value();
      }
    } else {
      //===------------------------------------------------------------------===//
      // Simple parallel EDT without loop - replicate body across workers
      //===------------------------------------------------------------------===//
      ARTS_INFO("Lowering simple parallel EDT");
      lowerSimpleParallelEdt(op);
    }

    //===--------------------------------------------------------------------===//
    // Step 7: Cleanup - yield from epoch and erase original operations
    //===--------------------------------------------------------------------===//
    AC->create<YieldOp>(loc);

    // Store dependencies before erasing
    SmallVector<Value> originalDeps = op.getDependencies();

    // Erase the original parallel EDT
    op.erase();

    // Erase any unused DbAcquire operations
    SmallVector<Operation *> toErase;
    if (containedForOp) {
      for (Value dep : originalDeps) {
        if (auto acq = dep.getDefiningOp<DbAcquireOp>()) {
          if (llvm::all_of(acq->getResults(),
                           [](Value v) { return v.use_empty(); }))
            toErase.push_back(acq);
        }
      }
    }
    for (auto *e : toErase)
      e->erase();

    return success();
  }

private:
  void prepareReductionBuffers(ForOp forOp, Value slotCount);
  void appendReductionDeps(Value offset, SmallVector<Value> &deps);
  LogicalResult initializePrivateAccumulators(ArrayRef<Value> privateArgs,
                                              Block &body);
  Value createZeroValue(Type type);
  Value combineValues(Type type, Value lhs, Value rhs);
  Value remapDependencyValue(Value value, ValueRange sourceArgs,
                             ValueRange mappedValues);
  void remapDependencyList(ValueRange deps, ValueRange sourceArgs,
                           ValueRange mappedValues,
                           SmallVectorImpl<Value> &out);
  ReductionAccess computeReductionAccess(const ReductionInfo &info,
                                         Value buffer, Value linearIndex,
                                         Value zeroIndex, Value strideOne);
  LogicalResult createReductionAggregator(Value loopCount,
                                          Operation *insertAfter,
                                          EdtOp sourceOp);

  SmallVector<ReductionInfo> activeReductions;
  Value activeLoopCount;
  Operation *activeLoopOp = nullptr;

  /// Compute total iterations of an arts.for operation
  Value computeTotalIterations(ForOp forOp) {
    Value oneIndex = AC->createIndexConstant(1, loc);
    Value lower = forOp.getLowerBound()[0];
    Value upper = forOp.getUpperBound()[0];
    Value step = forOp.getStep()[0];
    Value range = AC->create<arith::SubIOp>(loc, upper, lower);
    Value adjustedRange = AC->create<arith::AddIOp>(
        loc, range, AC->create<arith::SubIOp>(loc, step, oneIndex));
    return AC->create<arith::DivUIOp>(loc, adjustedRange, step);
  }

  EpochOp createEpochWrapper() {
    auto epochOp = AC->create<EpochOp>(loc);
    auto &region = epochOp.getRegion();
    if (region.empty())
      region.push_back(new Block());
    return epochOp;
  }

  //===----------------------------------------------------------------------===//
  /// Create dependencies for a worker (block distribution)
  ///
  /// For each datablock dependency in the parallel EDT, creates a DbAcquire
  /// that acquires only the portion needed by this worker.
  //===----------------------------------------------------------------------===//
  WorkerDepsResult createBlockDeps(EdtOp op, ForOp containedForOp,
                                   Value workerId, Value totalWorkers,
                                   const WorkerIterationInfo &workerInfo) {
    WorkerDepsResult result;
    ValueRange srcArgs = op.getRegion().front().getArguments();
    ValueRange srcDeps = op.getDependencies();

    Value chunkOffset = workerInfo.workerFirstChunk;
    Value chunkSizeElems = workerInfo.workerIterationCount;

    for (Value dep : srcDeps) {
      if (auto acq = dep.getDefiningOp<DbAcquireOp>()) {
        bool isReductionDep = llvm::any_of(activeReductions, [&](auto &info) {
          return info.originalAlloc &&
                 acq.getSourcePtr() == info.originalAlloc.getPtr();
        });
        if (isReductionDep)
          continue;

        Value sourceGuid =
            remapDependencyValue(acq.getSourceGuid(), srcArgs, srcDeps);
        Value sourcePtr =
            remapDependencyValue(acq.getSourcePtr(), srcArgs, srcDeps);
        SmallVector<Value> offsets{chunkOffset};
        SmallVector<Value> sizes{chunkSizeElems};
        auto newAcquire =
            AC->create<DbAcquireOp>(loc, acq.getMode(), sourceGuid, sourcePtr,
                                    SmallVector<Value>{}, offsets, sizes);
        ARTS_DEBUG("  Created block DbAcquire " << newAcquire);
        result.acquireOps.push_back(newAcquire);
        result.deps.push_back(newAcquire.getPtr());
        continue;
      }
      result.deps.push_back(remapDependencyValue(dep, srcArgs, srcDeps));
    }
    return result;
  }

  //===----------------------------------------------------------------------===//
  /// Compute worker iteration bounds (block distribution)
  ///
  /// Calculates what iterations a specific worker should execute in block
  /// distribution, where each worker gets a contiguous range of iterations.
  ///
  /// Block distribution example with 4 workers, loop 0 to 100:
  /// - Total iterations = 100, chunk_size = 1
  /// - chunksPerWorker = 100 / 4 = 25
  /// - remainderChunks = 100 % 4 = 0
  ///
  /// - Worker 0: firstChunk = 0, count = 25 (iterations 0-24)
  /// - Worker 1: firstChunk = 25, count = 25 (iterations 25-49)
  /// - Worker 2: firstChunk = 50, count = 25 (iterations 50-74)
  /// - Worker 3: firstChunk = 75, count = 25 (iterations 75-99)
  ///
  /// Uneven example with 101 iterations:
  /// - Total iterations = 101, workers = 4
  /// - chunksPerWorker = 25, remainder = 1
  ///   - Worker 0: firstChunk = 0, count = 26 (gets extra iteration)
  ///   - Worker 1: firstChunk = 26, count = 25
  ///   - Worker 2: firstChunk = 51, count = 25
  ///   - Worker 3: firstChunk = 76, count = 25
  //===----------------------------------------------------------------------===//
  WorkerIterationInfo computeWorkerIterations(ForOp containedForOp,
                                              Value workerId,
                                              Value totalWorkers) {
    WorkerIterationInfo info;

    info.lowerBound = containedForOp.getLowerBound()[0];
    info.upperBound = containedForOp.getUpperBound()[0];
    info.loopStep = containedForOp.getStep()[0];

    /// Calculate total iterations: (upper - lower) / step
    Value oneIndex = AC->createIndexConstant(1, loc);
    Value range =
        AC->create<arith::SubIOp>(loc, info.upperBound, info.lowerBound);
    Value adjustedRange = AC->create<arith::AddIOp>(
        loc, range, AC->create<arith::SubIOp>(loc, info.loopStep, oneIndex));
    info.totalIterations =
        AC->create<arith::DivUIOp>(loc, adjustedRange, info.loopStep);

    /// For block distribution, chunk_size = 1
    info.chunkSize = oneIndex;

    /// Each iteration is a "chunk"
    info.totalChunks = info.totalIterations;

    /// Divide total chunks evenly across workers
    /// chunksPerEDT = totalChunks / totalWorkers
    Value chunksPerEDT =
        AC->create<arith::DivUIOp>(loc, info.totalChunks, totalWorkers);

    /// Remainder chunks
    /// remainderChunks = totalChunks % totalWorkers
    Value remainderChunks =
        AC->create<arith::RemUIOp>(loc, info.totalChunks, totalWorkers);

    /// First few workers get an extra chunk if remainder > 0
    /// getsExtra = workerId < remainderChunks
    Value getsExtra = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ult,
                                                workerId, remainderChunks);
    /// extraChunk = getsExtra ? 1 : 0
    Value extraChunk = AC->castToIndex(getsExtra, loc);

    /// Total chunks this worker should handle
    /// workerChunkCount = chunksPerEDT + extraChunk
    info.workerChunkCount =
        AC->create<arith::AddIOp>(loc, chunksPerEDT, extraChunk);

    /// Total number of workers
    info.totalWorkers = totalWorkers;

    /// Calculate which chunk this worker starts with:
    ///  workerFirstChunk = workerId * chunksPerWorker + min(workerId,remainder)
    /// Base start
    Value baseStart = AC->create<arith::MulIOp>(loc, workerId, chunksPerEDT);
    /// Extra chunks this worker gets
    Value prefixExtra =
        AC->create<arith::MinUIOp>(loc, workerId, remainderChunks);
    /// First chunk this worker handles
    info.workerFirstChunk =
        AC->create<arith::AddIOp>(loc, baseStart, prefixExtra);

    /// Calculate actual iteration count
    /// rawIterCount = workerChunkCount * chunkSize
    Value rawIterCount =
        AC->create<arith::MulIOp>(loc, info.workerChunkCount, info.chunkSize);

    /// First element this worker handles
    /// startElems = workerFirstChunk * chunkSize
    Value startElems =
        AC->create<arith::MulIOp>(loc, info.workerFirstChunk, info.chunkSize);

    /// Handle edge case: if worker starts past total iterations
    Value zeroIndex = AC->createIndexConstant(0, loc);
    Value needZero = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::ult, info.totalIterations, startElems);

    /// Calculate how many iterations are actually available to this worker
    Value remaining =
        AC->create<arith::SubIOp>(loc, info.totalIterations, startElems);

    /// Clamp to zero if worker starts beyond end (shouldn't happen in practice)
    Value remainingNonNeg =
        AC->create<arith::SelectOp>(loc, needZero, zeroIndex, remaining);

    /// Final iteration count = min(theoretical count, available count)
    info.workerIterationCount =
        AC->create<arith::MinUIOp>(loc, rawIterCount, remainingNonNeg);

    return info;
  }

  //===----------------------------------------------------------------------===//
  /// Block distribution with worker-based approach.
  ///
  /// Creates one EDT per worker, where each worker handles a contiguous
  /// block of iterations. Distributes iterations evenly, with remainder
  /// distributed to the first few workers.
  ///
  /// Example with 8 workers, 100 iterations:
  ///  - Worker 0: iterations 0-12 (13 iterations)
  ///  - Worker 1: iterations 13-25 (13 iterations)
  ///  - Worker 2: iterations 26-38 (13 iterations)
  ///  - Worker 3: iterations 39-50 (12 iterations)
  ///  - Worker 4: iterations 51-62 (12 iterations)
  ///  - Worker 5: iterations 63-74 (12 iterations)
  ///  - Worker 6: iterations 75-86 (12 iterations)
  ///  - Worker 7: iterations 87-99 (13 iterations)
  ///
  /// Formula:
  ///   iterationsPerWorker = totalIterations / numWorkers
  ///   remainder = totalIterations % numWorkers
  ///   first 'remainder' workers get (iterationsPerWorker + 1) iterations
  ///   remaining workers get iterationsPerWorker iterations
  //===----------------------------------------------------------------------===//
  void lowerStaticBlock(
      EdtOp op, ForOp containedForOp, Value lb, Value ub, bool isInterNode,
      const SmallVector<SingleConstructInfo> &singleConstructs = {}) {

    Value zero = AC->createIndexConstant(0, loc);
    Value one = AC->createIndexConstant(1, loc);

    /// Create scf.for loop over workers
    scf::ForOp workerLoop = AC->create<scf::ForOp>(loc, lb, ub, one);

    OpBuilder::InsertionGuard IG(AC->getBuilder());
    AC->setInsertionPointToStart(workerLoop.getBody());

    Value workerId = workerLoop.getInductionVar();

    ARTS_DEBUG("Processing worker " << workerId << " (block distribution)");

    /// Compute iteration bounds for this worker
    WorkerIterationInfo workerInfo =
        computeWorkerIterations(containedForOp, workerId, ub);

    ARTS_DEBUG("Block worker info: id="
               << workerId << " firstChunk=" << workerInfo.workerFirstChunk
               << " chunkCount=" << workerInfo.workerChunkCount
               << " iterations=" << workerInfo.workerIterationCount);

    /// Skip workers with no work
    Value hasWork = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::ugt, workerInfo.workerIterationCount, zero);
    auto ifHasWork =
        AC->create<scf::IfOp>(loc, hasWork, /*withElseRegion=*/false);

    AC->setInsertionPointToStart(&ifHasWork.getThenRegion().front());

    ARTS_DEBUG("  Worker has " << workerInfo.workerIterationCount
                               << " iterations");

    /// Set route based on inter/intra-node execution
    Value route;
    if (isInterNode) {
      route = AC->castToInt(AC->Int32, workerId, loc);
    } else {
      route = AC->createIntConstant(0, AC->Int32, loc);
    }

    /// Create dependencies for this worker
    WorkerDepsResult depsResult =
        createBlockDeps(op, containedForOp, workerId, ub, workerInfo);

    ValueRange srcArgs = op.getRegion().front().getArguments();
    SmallVector<Value> workerDeps = depsResult.deps;

    /// Add single construct atomic flags as dependencies
    /// Acquire the flags for this worker EDT
    SmallVector<size_t> singleFlagIndices;
    SmallVector<DbAcquireOp> singleFlagAcquires;
    for (const auto &singleInfo : singleConstructs) {
      // Acquire the atomic flag for this worker EDT
      auto flagAcquire = AC->create<DbAcquireOp>(
          loc, ArtsMode::inout, singleInfo.atomicFlagGuid,
          singleInfo.atomicFlagPtr, SmallVector<Value>{},
          SmallVector<Value>{zero}, SmallVector<Value>{one});

      singleFlagIndices.push_back(workerDeps.size());
      singleFlagAcquires.push_back(flagAcquire);
      workerDeps.push_back(flagAcquire.getPtr());
      ARTS_DEBUG("  Added single flag as dependency at index "
                 << singleFlagIndices.back());
    }

    /// Append reduction dependencies (one slot per worker)
    appendReductionDeps(workerId, workerDeps);

    /// Create arts.edt<task> for this worker
    auto workerEdt = AC->create<EdtOp>(
        loc, EdtType::task, EdtConcurrency::intranode, route, workerDeps);
    setupEdtRegion(workerEdt, op);

    Block &workerBody = workerEdt.getRegion().front();
    AC->setInsertionPointToStart(&workerBody);

    /// Handle reduction accumulators
    SmallVector<Value> privateAccArgs;
    if (!activeReductions.empty()) {
      auto workerArgs = workerBody.getArguments();
      size_t reductionArgStart = workerArgs.size() - activeReductions.size();
      for (unsigned i = 0; i < activeReductions.size(); ++i)
        privateAccArgs.push_back(workerArgs[reductionArgStart + i]);
      if (failed(initializePrivateAccumulators(privateAccArgs, workerBody)))
        return;
    }

    /// Map parallel EDT arguments to worker EDT arguments
    IRMapping mapper;
    auto workerArgs = workerBody.getArguments();
    for (auto [srcArg, workerArg] : zip(srcArgs, workerArgs))
      mapper.map(srcArg, workerArg);

    //===------------------------------------------------------------------===//
    // Insert single construct execution (if any) BEFORE for loop
    //===------------------------------------------------------------------===//
    for (size_t i = 0; i < singleConstructs.size(); ++i) {
      insertSingleIntoWorkerEdt(singleConstructs[i].singleEdt, workerBody,
                                singleFlagIndices[i], op, mapper);
    }

    /// Calculate iteration bounds for this worker
    /// startIteration = workerFirstChunk (since chunk_size=1 for block)
    Value startIteration = workerInfo.workerFirstChunk;

    /// Create loop over this worker's iterations
    /// scf.for %i = 0 to workerIterationCount step 1
    auto iterLoop =
        AC->create<scf::ForOp>(loc, zero, workerInfo.workerIterationCount, one);

    Block &iterBody = iterLoop.getRegion().front();
    AC->setInsertionPointToStart(&iterBody);

    Value localIter = iterLoop.getInductionVar();

    /// Calculate global iteration index
    /// globalIter = startIteration + localIter
    Value globalIter =
        AC->create<arith::AddIOp>(loc, startIteration, localIter);

    /// Convert to actual loop index
    /// actualLoopIndex = lowerBound + globalIter * step
    Block &loopBody = containedForOp.getRegion().front();
    Value loopInductionVar = loopBody.getArgument(0);
    Value lowerBound = containedForOp.getLowerBound()[0];
    Value loopStep = containedForOp.getStep()[0];

    IRMapping iterationMapper = mapper;
    Value actualLoopIndex = AC->create<arith::AddIOp>(
        loc, lowerBound, AC->create<arith::MulIOp>(loc, globalIter, loopStep));

    iterationMapper.map(loopInductionVar, actualLoopIndex);

    /// Map reduction accumulators
    if (!activeReductions.empty()) {
      for (auto [idx, info] : llvm::enumerate(activeReductions))
        iterationMapper.map(info.originalAcc, privateAccArgs[idx]);
    }
    mapReductionIdentityValues(iterationMapper, containedForOp);

    /// Clone loop body operations
    for (Operation &inner : loopBody.without_terminator()) {
      Operation *cloned = AC->clone(inner, iterationMapper);
      for (auto [oldRes, newRes] :
           llvm::zip(inner.getResults(), cloned->getResults()))
        iterationMapper.map(oldRes, newRes);
    }
    replaceUndefWithZero(iterBody);

    SmallVector<DbRefOp> dbRefs;
    SmallVector<memref::LoadOp> memLoads;
    SmallVector<memref::StoreOp> memStores;
    for (Operation &inner : iterBody) {
      if (auto dbRef = dyn_cast<DbRefOp>(&inner)) {
        if (auto barg = dbRef.getSource().dyn_cast<BlockArgument>())
          if (barg.getOwner() == &workerBody)
            dbRefs.push_back(dbRef);
      } else if (auto load = dyn_cast<memref::LoadOp>(&inner)) {
        if (auto dbRef = load.getMemRef().getDefiningOp<DbRefOp>())
          if (auto barg = dbRef.getSource().dyn_cast<BlockArgument>())
            if (barg.getOwner() == &workerBody)
              memLoads.push_back(load);
      } else if (auto store = dyn_cast<memref::StoreOp>(&inner)) {
        if (auto dbRef = store.getMemRef().getDefiningOp<DbRefOp>())
          if (auto barg = dbRef.getSource().dyn_cast<BlockArgument>())
            if (barg.getOwner() == &workerBody)
              memStores.push_back(store);
      }
    }

    for (DbRefOp dbRef : dbRefs) {
      AC->setInsertionPoint(dbRef);
      Value globalIndex = iterationMapper.lookup(loopInductionVar);
      Value localIndexForDb =
          AC->create<arith::SubIOp>(loc, globalIndex, startIteration);
      dbRef.getIndicesMutable().assign(ValueRange{localIndexForDb});
    }

    for (memref::LoadOp load : memLoads) {
      AC->setInsertionPoint(load);
      Value globalIndex = load.getIndices().front();
      Value localIndex =
          AC->create<arith::SubIOp>(loc, globalIndex, startIteration);
      load.getIndicesMutable().assign(ValueRange{localIndex});
    }

    for (memref::StoreOp store : memStores) {
      AC->setInsertionPoint(store);
      Value globalIndex = store.getIndices().front();
      Value localIndex =
          AC->create<arith::SubIOp>(loc, globalIndex, startIteration);
      store.getIndicesMutable().assign(ValueRange{localIndex});
    }

    // Adjusted indices to local for sliced DbAcquire. This ensures correct
    // accesses within each worker's block.

    /// End of iteration loop and worker EDT
    AC->setInsertionPointToEnd(&workerBody);

    /// Release acquired datablocks
    for (size_t i = 0; i < depsResult.acquireOps.size(); ++i) {
      if (i < workerArgs.size())
        AC->create<DbReleaseOp>(loc, workerArgs[i]);
    }

    /// Release single construct atomic flags
    for (auto flagAcquire : singleFlagAcquires) {
      // Find the corresponding block argument
      auto it = llvm::find(workerDeps, flagAcquire.getPtr());
      if (it != workerDeps.end()) {
        size_t idx = it - workerDeps.begin();
        if (idx < workerArgs.size())
          AC->create<DbReleaseOp>(loc, workerArgs[idx]);
      }
    }

    /// Release reduction accumulators
    if (!privateAccArgs.empty()) {
      for (Value accArg : privateAccArgs)
        AC->create<DbReleaseOp>(loc, accArg);
    }

    /// Ensure worker EDT has yield terminator
    if (workerBody.empty() || !isa<YieldOp>(workerBody.back()))
      AC->create<YieldOp>(loc);

    /// Track reduction aggregation
    if (!activeReductions.empty())
      activeLoopOp = workerLoop.getOperation();
  }

  //===----------------------------------------------------------------------===//
  /// Cyclic distribution with worker-based approach.
  ///
  /// Creates one EDT per worker, where each worker handles multiple chunks
  /// in a round-robin fashion. This approach allows supporting additional
  /// operations in the parallel EDT beyond just the for loop.
  ///
  /// Example with 4 workers, 20 chunks (100 iterations, chunk_size=5):
  ///  - Worker 0: chunks 0,4,8,12,16  (5 chunks total)
  ///  - Worker 1: chunks 1,5,9,13,17  (5 chunks total)
  ///  - Worker 2: chunks 2,6,10,14,18 (5 chunks total)
  ///  - Worker 3: chunks 3,7,11,15,19 (5 chunks total)
  ///
  /// Output IR:
  ///   scf.for %worker = 0 to 4 step 1 {
  ///     /// Calculate how many chunks this worker handles
  ///     %workerChunkCount = ...
  ///     arts.edt<task route=0> deps[...] {
  ///       /// Loop over this worker's chunks
  ///       scf.for %localChunk = 0 to %workerChunkCount step 1 {
  ///         %chunkId = %worker + %localChunk * numWorkers
  ///         /// Execute iterations for this chunk
  ///         scf.for %i = 0 to chunkSize step 1 { ... }
  ///       }
  ///     }
  ///   }
  //===----------------------------------------------------------------------===//
  void lowerStaticCyclic(
      EdtOp op, ForOp containedForOp, Value lb, Value ub, bool isInterNode,
      const SmallVector<SingleConstructInfo> &singleConstructs = {}) {

    LoopInfo info;
    info.lowerBound = containedForOp.getLowerBound()[0];
    info.upperBound = containedForOp.getUpperBound()[0];
    info.loopStep = containedForOp.getStep()[0];
    info.totalWorkers = ub;

    computeTotalIterationsAndChunkSize(containedForOp, info);

    Value zero = AC->createIndexConstant(0, loc);
    Value one = AC->createIndexConstant(1, loc);

    /// Create scf.for loop over workers (not chunks)
    scf::ForOp workerLoop = AC->create<scf::ForOp>(loc, lb, ub, one);

    OpBuilder::InsertionGuard IG(AC->getBuilder());
    AC->setInsertionPointToStart(workerLoop.getBody());

    Value workerId = workerLoop.getInductionVar();

    ARTS_DEBUG("Processing worker " << workerId << " of " << ub);

    /// Calculate how many chunks this worker gets in cyclic distribution
    /// Worker i gets chunks: i, i+numWorkers, i+2*numWorkers, ...
    /// Number of chunks = ceil((totalChunks - workerId) / numWorkers)
    /// Example: 20 chunks, 8 workers
    ///   - Worker 0: chunks 0,8,16 -> (20-0+8-1)/8 = 3 chunks
    ///   - Worker 4: chunks 4,12 -> (20-4+8-1)/8 = 2 chunks
    Value chunksAfterWorker =
        AC->create<arith::SubIOp>(loc, info.totalChunks, workerId);
    Value moreThanOne = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::ugt, info.totalWorkers, zero);
    Value workersMinusOne = AC->create<arith::SelectOp>(
        loc, moreThanOne,
        AC->create<arith::SubIOp>(loc, info.totalWorkers, one), zero);
    Value numerator =
        AC->create<arith::AddIOp>(loc, chunksAfterWorker, workersMinusOne);
    Value workerChunkCount =
        AC->create<arith::DivUIOp>(loc, numerator, info.totalWorkers);

    /// Skip workers with no work
    /// If workerChunkCount == 0, don't create EDT for this worker
    Value hasWork = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ugt,
                                              workerChunkCount, zero);
    auto ifHasWork =
        AC->create<scf::IfOp>(loc, hasWork, /*withElseRegion=*/false);

    AC->setInsertionPointToStart(&ifHasWork.getThenRegion().front());

    ARTS_DEBUG("  Worker has " << workerChunkCount << " chunks");

    /// Set route based on inter/intra-node execution
    Value route;
    if (isInterNode) {
      route = AC->castToInt(AC->Int32, workerId, loc);
    } else {
      route = AC->createIntConstant(0, AC->Int32, loc);
    }

    /// Create dependencies for this worker
    SmallVector<Value> workerDeps;
    ValueRange srcArgs = op.getRegion().front().getArguments();
    ValueRange srcDeps = op.getDependencies();

    SmallVector<DbAcquireOp> workerAcquires;
    for (Value dep : srcDeps) {
      if (auto acq = dep.getDefiningOp<DbAcquireOp>()) {
        /// Skip reduction dependencies (handled separately)
        bool isReductionDep = llvm::any_of(activeReductions, [&](auto &info) {
          return info.originalAlloc &&
                 acq.getSourcePtr() == info.originalAlloc.getPtr();
        });
        if (isReductionDep)
          continue;
        workerAcquires.push_back(acq);
      }

      /// Remap dependency values to worker EDT arguments
      workerDeps.push_back(remapDependencyValue(dep, srcArgs, srcDeps));
    }

    /// Add single construct atomic flags as dependencies
    /// Acquire the flags for this worker EDT
    SmallVector<size_t> singleFlagIndices;
    SmallVector<DbAcquireOp> singleFlagAcquires;
    for (const auto &singleInfo : singleConstructs) {
      // Acquire the atomic flag for this worker EDT
      auto flagAcquire = AC->create<DbAcquireOp>(
          loc, ArtsMode::inout, singleInfo.atomicFlagGuid,
          singleInfo.atomicFlagPtr, SmallVector<Value>{},
          SmallVector<Value>{zero}, SmallVector<Value>{one});

      singleFlagIndices.push_back(workerDeps.size());
      singleFlagAcquires.push_back(flagAcquire);
      workerDeps.push_back(flagAcquire.getPtr());
      ARTS_DEBUG("  Added single flag as dependency at index "
                 << singleFlagIndices.back());
    }

    /// Append reduction dependencies (one slot per worker)
    appendReductionDeps(workerId, workerDeps);

    /// Create arts.edt<task> for this worker
    auto workerEdt = AC->create<EdtOp>(
        loc, EdtType::task, EdtConcurrency::intranode, route, workerDeps);
    setupEdtRegion(workerEdt, op);

    Block &workerBody = workerEdt.getRegion().front();
    AC->setInsertionPointToStart(&workerBody);

    /// Handle reduction accumulators
    SmallVector<Value> privateAccArgs;
    if (!activeReductions.empty()) {
      auto workerArgs = workerBody.getArguments();
      size_t reductionArgStart = workerArgs.size() - activeReductions.size();
      for (unsigned i = 0; i < activeReductions.size(); ++i)
        privateAccArgs.push_back(workerArgs[reductionArgStart + i]);
      if (failed(initializePrivateAccumulators(privateAccArgs, workerBody)))
        return;
    }

    /// Map parallel EDT arguments to worker EDT arguments
    IRMapping mapper;
    auto workerArgs = workerBody.getArguments();
    for (auto [srcArg, workerArg] : zip(srcArgs, workerArgs))
      mapper.map(srcArg, workerArg);

    //===------------------------------------------------------------------===//
    // Insert single construct execution (if any) BEFORE for loop
    //===------------------------------------------------------------------===//
    for (size_t i = 0; i < singleConstructs.size(); ++i) {
      insertSingleIntoWorkerEdt(singleConstructs[i].singleEdt, workerBody,
                                singleFlagIndices[i], op, mapper);
    }

    /// Create loop over this worker's chunks
    /// scf.for %localChunk = 0 to workerChunkCount step 1
    auto localChunkLoop =
        AC->create<scf::ForOp>(loc, zero, workerChunkCount, one);

    Block &chunkLoopBody = localChunkLoop.getRegion().front();
    AC->setInsertionPointToStart(&chunkLoopBody);

    Value localChunkIdx = localChunkLoop.getInductionVar();

    /// Calculate global chunk ID for this local chunk
    /// globalChunkId = workerId + localChunkIdx * numWorkers
    /// Example: worker=1, numWorkers=4, localChunk=0 -> chunk=1
    ///          worker=1, numWorkers=4, localChunk=1 -> chunk=5
    Value chunkIdOffset =
        AC->create<arith::MulIOp>(loc, localChunkIdx, info.totalWorkers);
    Value globalChunkId =
        AC->create<arith::AddIOp>(loc, workerId, chunkIdOffset);

    ARTS_DEBUG("    Local chunk " << localChunkIdx << " -> global chunk "
                                  << globalChunkId);

    /// Calculate iteration range for this chunk
    /// chunkStartIter = globalChunkId * chunkSize
    Value chunkStartIter =
        AC->create<arith::MulIOp>(loc, globalChunkId, info.chunkSize);

    /// Handle last chunk: may have fewer iterations
    Value remaining =
        AC->create<arith::SubIOp>(loc, info.totalIterations, chunkStartIter);
    Value actualChunkSize =
        AC->create<arith::MinUIOp>(loc, info.chunkSize, remaining);

    /// Create inner loop over iterations within this chunk
    /// scf.for %localIter = 0 to actualChunkSize step 1
    auto iterLoop = AC->create<scf::ForOp>(loc, zero, actualChunkSize, one);

    Block &iterBody = iterLoop.getRegion().front();
    AC->setInsertionPointToStart(&iterBody);

    Value localIter = iterLoop.getInductionVar();

    /// Calculate global iteration number
    /// globalIter = chunkStartIter + localIter
    Value globalIterNumber =
        AC->create<arith::AddIOp>(loc, chunkStartIter, localIter);

    /// Convert to actual loop index
    /// actualLoopIndex = lowerBound + globalIterNumber * step
    Block &loopBody = containedForOp.getRegion().front();
    Value loopInductionVar = loopBody.getArgument(0);

    IRMapping iterationMapper = mapper;
    Value actualLoopIndex = AC->create<arith::AddIOp>(
        loc, info.lowerBound,
        AC->create<arith::MulIOp>(loc, globalIterNumber, info.loopStep));

    iterationMapper.map(loopInductionVar, actualLoopIndex);

    /// Map reduction accumulators
    if (!activeReductions.empty()) {
      for (auto [idx, info] : llvm::enumerate(activeReductions))
        iterationMapper.map(info.originalAcc, privateAccArgs[idx]);
    }
    mapReductionIdentityValues(iterationMapper, containedForOp);

    /// Clone loop body operations
    for (Operation &inner : loopBody.without_terminator()) {
      Operation *cloned = AC->clone(inner, iterationMapper);
      for (auto [oldRes, newRes] :
           llvm::zip(inner.getResults(), cloned->getResults()))
        iterationMapper.map(oldRes, newRes);
    }
    replaceUndefWithZero(iterBody);

    SmallVector<DbRefOp> dbRefs;
    SmallVector<memref::LoadOp> memLoads;
    SmallVector<memref::StoreOp> memStores;
    for (Operation &inner : iterBody) {
      if (auto dbRef = dyn_cast<DbRefOp>(&inner)) {
        if (auto barg = dbRef.getSource().dyn_cast<BlockArgument>())
          if (barg.getOwner() == &workerBody)
            dbRefs.push_back(dbRef);
      } else if (auto load = dyn_cast<memref::LoadOp>(&inner)) {
        if (auto dbRef = load.getMemRef().getDefiningOp<DbRefOp>())
          if (auto barg = dbRef.getSource().dyn_cast<BlockArgument>())
            if (barg.getOwner() == &workerBody)
              memLoads.push_back(load);
      } else if (auto store = dyn_cast<memref::StoreOp>(&inner)) {
        if (auto dbRef = store.getMemRef().getDefiningOp<DbRefOp>())
          if (auto barg = dbRef.getSource().dyn_cast<BlockArgument>())
            if (barg.getOwner() == &workerBody)
              memStores.push_back(store);
      }
    }

    for (DbRefOp dbRef : dbRefs) {
      AC->setInsertionPoint(dbRef);
      Value globalIndex = iterationMapper.lookup(loopInductionVar);
      Value localIndexForDb =
          AC->create<arith::SubIOp>(loc, globalIndex, chunkStartIter);
      dbRef.getIndicesMutable().assign(ValueRange{localIndexForDb});
    }

    for (memref::LoadOp load : memLoads) {
      AC->setInsertionPoint(load);
      Value globalIndex = load.getIndices().front();
      Value localIndex =
          AC->create<arith::SubIOp>(loc, globalIndex, chunkStartIter);
      load.getIndicesMutable().assign(ValueRange{localIndex});
    }

    for (memref::StoreOp store : memStores) {
      AC->setInsertionPoint(store);
      Value globalIndex = store.getIndices().front();
      Value localIndex =
          AC->create<arith::SubIOp>(loc, globalIndex, chunkStartIter);
      store.getIndicesMutable().assign(ValueRange{localIndex});
    }

    // Adjusted indices to local for sliced DbAcquire. This ensures correct
    // accesses within each worker's block.

    /// End of iteration loop, chunk loop, and worker EDT
    AC->setInsertionPointToEnd(&workerBody);

    /// Release acquired datablocks
    /// Need to use block arguments, not the DbAcquire results directly
    for (size_t i = 0; i < workerAcquires.size(); ++i) {
      /// The block arguments correspond to the dependencies in order
      if (i < workerArgs.size())
        AC->create<DbReleaseOp>(loc, workerArgs[i]);
    }

    /// Release single construct atomic flags
    for (auto flagAcquire : singleFlagAcquires) {
      // Find the corresponding block argument
      auto it = llvm::find(workerDeps, flagAcquire.getPtr());
      if (it != workerDeps.end()) {
        size_t idx = it - workerDeps.begin();
        if (idx < workerArgs.size())
          AC->create<DbReleaseOp>(loc, workerArgs[idx]);
      }
    }

    /// Release reduction accumulators
    if (!privateAccArgs.empty()) {
      for (Value accArg : privateAccArgs)
        AC->create<DbReleaseOp>(loc, accArg);
    }

    /// Ensure worker EDT has yield terminator
    if (workerBody.empty() || !isa<YieldOp>(workerBody.back()))
      AC->create<YieldOp>(loc);

    /// Track reduction aggregation
    if (!activeReductions.empty())
      activeLoopOp = workerLoop.getOperation();
  }

  //===----------------------------------------------------------------------===//
  /// Simple parallel EDT with 4 workers (threads=4):
  ///
  /// Creates identical task EDTs for embarrassingly parallel workloads
  /// - Input IR (no loop, just computation):
  ///   arts.edt<parallel> {
  ///     /// some computation work
  ///   }
  ///
  /// - Output IR:
  ///   scf.for %worker = 0 to 4 step 1 {
  ///     arts.edt<task route=0> deps[...] {
  ///       /// cloned computation work
  ///     }
  ///   }
  //===----------------------------------------------------------------------===//
  void lowerSimpleParallelEdt(EdtOp op) {
    auto [lb, ub, isInterNode] = determineParallelismStrategy(op);
    ARTS_INFO("Lowering simple parallel EDT; bounds: " << lb << ".." << ub);

    Value one = AC->createIndexConstant(1, loc);
    /// Create worker loop:
    /// for(workerId = 0; workerId < totalWorkers; workerId++)
    scf::ForOp workerLoop = AC->create<scf::ForOp>(loc, lb, ub, one);
    OpBuilder::InsertionGuard IG(AC->getBuilder());
    AC->setInsertionPointToStart(workerLoop.getBody());

    ValueRange srcArgs = op.getRegion().front().getArguments();
    ValueRange srcDeps = op.getDependencies();
    SmallVector<Value> deps;
    remapDependencyList(srcDeps, srcArgs, srcDeps, deps);
    Value workerId = workerLoop.getInductionVar();
    Value route = createRouteValue(workerId, isInterNode);

    /// Create arts.edt with intranode concurrency and deps for each worker
    auto newEdt = AC->create<EdtOp>(loc, EdtType::task,
                                    EdtConcurrency::intranode, route, deps);
    ARTS_INFO("Created task EDT with route " << route);
    setupEdtRegion(newEdt, op);

    /// Clone the original EDT's body into each worker's task EDT
    moveOperationsToNewEdt(op, newEdt, nullptr, Value());
  }

  //===----------------------------------------------------------------------===//
  /// Setup EDT region structure
  ///
  /// Initializes the region and block structure for a new EDT operation.
  /// Creates block arguments for each dependency and prepares the EDT
  /// for receiving cloned operations from the source EDT.
  //===----------------------------------------------------------------------===//
  void setupEdtRegion(EdtOp edt, EdtOp sourceEdt) {
    Region &dst = edt.getRegion();
    if (dst.empty())
      dst.push_back(new Block());

    Block &dstBody = dst.front();
    dstBody.clear();

    auto deps = edt.getDependencies();
    for (Value dep : deps)
      dstBody.addArgument(dep.getType(), loc);
  }

  //===----------------------------------------------------------------------===//
  /// Determine parallelism strategy for EDT
  ///
  /// Analyzes EDT attributes to determine the number of workers and whether
  /// the parallelism is intra-node or inter-node. Uses concurrency pass
  /// attributes or falls back to runtime worker count. Returns: (lower_bound=0,
  /// upper_bound=worker_count, is_inter_node)
  //===----------------------------------------------------------------------===//
  std::tuple<Value, Value, bool> determineParallelismStrategy(EdtOp edtOp) {
    Value lb = AC->createIndexConstant(0, loc);

    bool isParallel = edtOp.getType() == EdtType::parallel;
    bool isInterNode = (edtOp.getConcurrency() == EdtConcurrency::internode);

    /// Read parallelism strategy from EDT attributes set by concurrency pass
    if (auto workersAttr = edtOp.getWorkers()) {
      int numWorkers = workersAttr->getValue();
      Value ub = AC->createIndexConstant(numWorkers, loc);

      ARTS_INFO("Using EDT attributes: workers="
                << numWorkers << ", concurrency="
                << (isInterNode ? "internode" : "intranode"));
      return {lb, ub, isInterNode};
    }

    if (!isParallel) {
      /// Non-parallel EDTs without explicit workers execute on a single worker.
      Value ub = AC->createIndexConstant(1, loc);
      ARTS_INFO("Non-parallel EDT without workers attribute; defaulting to a"
                " single worker");
      return {lb, ub, isInterNode};
    }

    /// Fallback: no attributes set, query runtime for the appropriate count.
    Value runtimeCount =
        isInterNode ? AC->getTotalNodes(loc) : AC->getTotalWorkers(loc);
    Value ub = AC->castToIndex(runtimeCount, loc);

    if (isInterNode)
      ARTS_WARN("No workers attribute set on parallel EDT - using runtime "
                "total nodes fallback");
    else
      ARTS_WARN("No workers attribute set on parallel EDT - using runtime "
                "total workers fallback");
    return {lb, ub, isInterNode};
  }

  //===----------------------------------------------------------------------===//
  /// Find a contained for loop within the parallel EDT
  //===----------------------------------------------------------------------===//
  ForOp findContainedForOp(EdtOp op) {
    ForOp containedForOp = nullptr;
    op.walk([&](ForOp forOp) {
      if (!containedForOp)
        containedForOp = forOp;
      return WalkResult::advance();
    });
    return containedForOp;
  }

  //===----------------------------------------------------------------------===//
  /// Find all single constructs within the parallel EDT
  ///
  /// Single constructs (arts.edt<single>) within a parallel EDT need special
  /// handling - they execute on only one worker thread using atomic flags.
  //===----------------------------------------------------------------------===//
  SmallVector<EdtOp> findSingleConstructs(EdtOp parallelEdt) {
    SmallVector<EdtOp> singleEdts;

    // Walk only the immediate children, don't recurse into nested EDTs
    for (Operation &op : parallelEdt.getRegion().front().without_terminator()) {
      if (auto nestedEdt = dyn_cast<EdtOp>(&op)) {
        if (nestedEdt.getType() == EdtType::single) {
          singleEdts.push_back(nestedEdt);
          ARTS_DEBUG("Found single construct in parallel EDT");
        }
      }
    }

    return singleEdts;
  }

  //===----------------------------------------------------------------------===//
  /// Allocate atomic flag for a single construct
  ///
  /// Creates a datablock containing a single i32 value initialized to 0.
  /// This flag will be used for atomic synchronization - only the first
  /// worker to atomically swap it to 1 will execute the single body.
  ///
  /// Returns: (flagGuid, flagPtr) - the GUID and pointer to the flag datablock
  //===----------------------------------------------------------------------===//
  std::pair<Value, Value> allocateSingleFlag() {
    Value zero = AC->createIntConstant(0, AC->Int32, loc);
    Value one = AC->createIndexConstant(1, loc);

    // Allocate a datablock with one i32 element, pre-initialized to 0
    SmallVector<Value> sizes{one};        // size = 1 element
    SmallVector<Value> elementSizes{one}; // element size = 1
    auto flagAlloc = AC->create<DbAllocOp>(loc, ArtsMode::inout, zero,
                                           DbAllocType::heap, DbMode::pinned,
                                           AC->Int32, // element type = i32
                                           sizes, elementSizes);

    Value flagGuid = flagAlloc.getGuid();
    Value flagPtr = flagAlloc.getPtr();

    // Note: The datablock is automatically zero-initialized by the ARTS runtime
    // We don't need to explicitly initialize it here

    ARTS_DEBUG("Allocated atomic flag for single construct: guid=" << flagGuid);

    return {flagGuid, flagPtr};
  }

  //===----------------------------------------------------------------------===//
  /// Insert single construct execution into worker EDT body
  ///
  /// This adds code to a worker EDT that:
  /// 1. Acquires the atomic flag (passed as block argument)
  /// 2. Atomically swaps the flag from 0 to 1
  /// 3. Only executes single body if swap returned 0 (first worker)
  /// 4. Provides implicit barrier (all workers participate)
  ///
  /// The single body operations are cloned into an scf.if that only
  /// executes for the first worker to reach this point.
  ///
  /// Parameters:
  ///   singleEdt: The original single EDT from the parallel EDT
  ///   workerBody: The worker EDT body to insert code into
  ///   flagArgIdx: Index of the flag in worker EDT block arguments
  ///   parallelEdt: Original parallel EDT (for dependency mapping)
  //===----------------------------------------------------------------------===//
  void insertSingleIntoWorkerEdt(EdtOp singleEdt, Block &workerBody,
                                 size_t flagArgIdx, EdtOp parallelEdt,
                                 IRMapping &baseMapper) {

    OpBuilder::InsertionGuard IG(AC->getBuilder());

    // Insert at the beginning of worker body (before loop iterations)
    AC->setInsertionPointToStart(&workerBody);

    Value zero = AC->createIndexConstant(0, loc);
    Value zeroI32 = AC->createIntConstant(0, AC->Int32, loc);
    Value oneI32 = AC->createIntConstant(1, AC->Int32, loc);

    //===------------------------------------------------------------------===//
    // Step 1: Get atomic flag from worker block arguments
    //===------------------------------------------------------------------===//
    BlockArgument flagArg = workerBody.getArgument(flagArgIdx);
    Value flagMemref = AC->create<DbRefOp>(loc, flagArg, zero);

    ARTS_DEBUG("Inserting single construct with atomic flag at arg index "
               << flagArgIdx);

    //===------------------------------------------------------------------===//
    // Step 2: Atomic swap - only first worker gets 0 back
    //===------------------------------------------------------------------===//
    auto atomicSwapCall = AC->createRuntimeCall(
        RuntimeFunction::ARTSRTL_artsAtomicSwap, {flagMemref, oneI32}, loc);
    Value oldValue = atomicSwapCall.getResult(0);

    // Check if we were the first worker (got 0 back)
    Value shouldExecute = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::eq, oldValue, zeroI32);

    //===------------------------------------------------------------------===//
    // Step 3: Conditionally execute single body
    //===------------------------------------------------------------------===//
    auto ifOp =
        AC->create<scf::IfOp>(loc, shouldExecute, /*withElseRegion=*/false);
    Block &thenBlock = ifOp.getThenRegion().front();
    AC->setInsertionPointToStart(&thenBlock);

    // Build mapper starting from the base mapping computed for the worker EDT
    IRMapping mapper = baseMapper;
    Block &singleBody = singleEdt.getRegion().front();
    ValueRange workerArgs = workerBody.getArguments();
    ValueRange singleArgs = singleBody.getArguments();

    for (auto [idx, singleArg] : llvm::enumerate(singleArgs)) {
      if (idx < workerArgs.size())
        mapper.map(singleArg, workerArgs[idx]);
    }

    for (Operation &op : singleBody.without_terminator()) {
      // Rematerialize constants if needed
      for (Value operand : op.getOperands()) {
        if (!mapper.contains(operand)) {
          if (auto defOp = operand.getDefiningOp()) {
            if (defOp->hasTrait<OpTrait::ConstantLike>()) {
              Operation *clonedConst = AC->clone(*defOp);
              mapper.map(operand, clonedConst->getResult(0));
            }
          }
        }
      }

      // Clone the operation
      Operation *clonedOp = AC->clone(op, mapper);

      // Update mapper with cloned results
      for (auto [oldRes, newRes] :
           llvm::zip(op.getResults(), clonedOp->getResults())) {
        mapper.map(oldRes, newRes);
      }
    }

    ARTS_DEBUG("Successfully inserted single construct into worker EDT");
  }

  //===----------------------------------------------------------------------===//
  /// Create dependencies for a single chunk (cyclic distribution)
  ///
  /// For cyclic distribution, each chunk iteration creates separate
  /// dependencies to acquire only the slice of datablocks needed for that
  /// chunk.
  //===----------------------------------------------------------------------===//
  WorkerDepsResult createCyclicDeps(EdtOp op, ForOp containedForOp,
                                    Value chunkId, Value chunkSize,
                                    Value totalIterations) {
    WorkerDepsResult result;
    ValueRange srcArgs = op.getRegion().front().getArguments();
    ValueRange srcDeps = op.getDependencies();

    Value chunkOffset = AC->create<arith::MulIOp>(loc, chunkId, chunkSize);
    Value remaining =
        AC->create<arith::SubIOp>(loc, totalIterations, chunkOffset);
    Value chunkSizeElems =
        AC->create<arith::MinUIOp>(loc, chunkSize, remaining);

    for (Value dep : srcDeps) {
      if (auto acq = dep.getDefiningOp<DbAcquireOp>()) {
        bool isReductionDep = llvm::any_of(activeReductions, [&](auto &info) {
          return info.originalAlloc &&
                 acq.getSourcePtr() == info.originalAlloc.getPtr();
        });
        if (isReductionDep)
          continue;
        Value sourceGuid =
            remapDependencyValue(acq.getSourceGuid(), srcArgs, srcDeps);
        Value sourcePtr =
            remapDependencyValue(acq.getSourcePtr(), srcArgs, srcDeps);
        SmallVector<Value> offsets{chunkOffset};
        SmallVector<Value> sizes{chunkSizeElems};
        auto newAcquire =
            AC->create<DbAcquireOp>(loc, acq.getMode(), sourceGuid, sourcePtr,
                                    SmallVector<Value>{}, offsets, sizes);
        ARTS_DEBUG("  Created chunk DbAcquire "
                   << newAcquire << " block=" << newAcquire->getBlock());
        result.acquireOps.push_back(newAcquire);
        result.deps.push_back(newAcquire.getPtr());
        continue;
      }

      result.deps.push_back(remapDependencyValue(dep, srcArgs, srcDeps));
    }

    return result;
  }

  void computeTotalIterationsAndChunkSize(ForOp containedForOp,
                                          LoopInfo &info) {
    /// Calculate total chunks: ceil(totalIterations / chunkSize)
    /// Formula: (totalIterations + chunkSize - 1) / chunkSize
    /// Example: 100 iterations, chunkSize=5 -> (100 + 5 - 1) / 5 = 104 / 5 = 20
    Value oneIndex = AC->createIndexConstant(1, loc);

    /// Calculate total iterations: (upper - lower) / step
    Value range =
        AC->create<arith::SubIOp>(loc, info.upperBound, info.lowerBound);
    Value adjustedRange = AC->create<arith::AddIOp>(
        loc, range, AC->create<arith::SubIOp>(loc, info.loopStep, oneIndex));
    info.totalIterations =
        AC->create<arith::DivUIOp>(loc, adjustedRange, info.loopStep);

    /// Get chunk_size attribute from the arts.for operation
    if (auto chunkAttr =
            containedForOp->getAttrOfType<IntegerAttr>("chunk_size")) {
      int64_t chunkInt = std::max<int64_t>(1, chunkAttr.getInt());
      info.chunkSize = AC->createIndexConstant(chunkInt, loc);
    } else {
      info.chunkSize = AC->createIndexConstant(1, loc);
    }

    /// Calculate total chunks: ceil(totalIterations / chunkSize)
    /// Formula: (totalIterations + chunkSize - 1) / chunkSize
    /// - Example: 100 iterations, chunkSize=5 ->
    ///   adjustedIterations = 100 + 5 - 1 = 104
    ///   totalChunks = 104 / 5 = 20 (ceil(100/5) = 20)
    //
    /// - Example: 101 iterations, chunkSize=5 ->
    ///   adjustedIterations = 101 + 5 - 1 = 105 t
    ///   totalChunks = 105 / 5 = 21
    ///   (ceil(101/5) = 21, since 5*20 = 100 < 101)

    /// adjustedIterations = totalIterations + chunkSize - 1
    Value adjustedIterations = AC->create<arith::AddIOp>(
        loc, info.totalIterations,
        AC->create<arith::SubIOp>(loc, info.chunkSize, oneIndex));

    /// Integer division gives ceiling
    /// totalChunks = adjustedIterations / chunkSize)
    info.totalChunks =
        AC->create<arith::DivUIOp>(loc, adjustedIterations, info.chunkSize);
  }

  //===----------------------------------------------------------------------===//
  /// Move operations from source EDT to new EDT
  ///
  /// Clones operations from the source EDT region into the target EDT region,
  /// mapping block arguments and optionally skipping certain operations.
  /// Handles the transfer of computation from one EDT to another during
  /// parallel EDT lowering transformations.
  /// Example:
  ///   Source EDT with body operations -> cloned into target EDT with mapped
  ///   arguments
  //===----------------------------------------------------------------------===//
  void moveOperationsToNewEdt(EdtOp sourceOp, EdtOp newEdt,
                              Operation *operationToSkip, Value releaseValue) {
    Block &dstBody = newEdt.getRegion().front();
    Block &srcBody = sourceOp.getRegion().front();

    IRMapping mapper;

    for (auto [srcArg, dstArg] :
         zip(srcBody.getArguments(), dstBody.getArguments())) {
      mapper.map(srcArg, dstArg);
    }

    OpBuilder bodyBuilder(newEdt);
    bodyBuilder.setInsertionPointToEnd(&dstBody);

    /// Clone operations in forward order so producers are cloned before
    /// consumers
    SmallVector<Operation *> opsToErase;
    for (Operation &op : srcBody.without_terminator()) {
      if (&op != operationToSkip) {
        opsToErase.push_back(&op);
        bodyBuilder.clone(op, mapper);
      }
    }

    /// Erase operations in reverse order - consumers before producers
    for (auto it = opsToErase.rbegin(); it != opsToErase.rend(); ++it)
      (*it)->erase();

    if (dstBody.empty() || !isa<YieldOp>(dstBody.back())) {
      bodyBuilder.setInsertionPointToEnd(&dstBody);
      bodyBuilder.create<YieldOp>(newEdt.getLoc());
    }
  }

  Value createRouteValue(Value workerId, bool isInterNode) {
    if (isInterNode)
      return AC->castToInt(AC->Int32, workerId, loc);
    else
      return AC->createIntConstant(0, AC->Int32, loc);
  }

  void mapReductionIdentityValues(IRMapping &mapper, ForOp forOp) {
    if (activeReductions.empty())
      return;

    Block &body = forOp.getRegion().front();
    for (Operation &op : body.without_terminator()) {
      for (Value operand : op.getOperands()) {
        if (auto undef = operand.getDefiningOp<LLVM::UndefOp>()) {
          if (mapper.contains(operand))
            continue;
          OpBuilder::InsertionGuard IG(AC->getBuilder());
          Value zero = createZeroValue(undef.getType());
          if (zero)
            mapper.map(operand, zero);
        }
      }
    }
  }

  void replaceUndefWithZero(Block &body) {
    SmallVector<LLVM::UndefOp, 4> undefs;
    for (Operation &op : body)
      if (auto undef = dyn_cast<LLVM::UndefOp>(&op))
        undefs.push_back(undef);

    for (LLVM::UndefOp undef : undefs) {
      OpBuilder::InsertionGuard IG(AC->getBuilder());
      AC->setInsertionPoint(undef);
      Value zero = createZeroValue(undef.getType());
      if (!zero)
        continue;
      undef.replaceAllUsesWith(zero);
      undef.erase();
    }
  }
};

//===----------------------------------------------------------------------===//
/// Prepare reduction buffers for the given arts.for loop.
/// Clears the active reductions list and creates partial result datablocks
/// for each reduction accumulator found in the for loop. Each partial buffer
/// is allocated with the specified slot count to store intermediate results
/// from parallel worker EDTs before final aggregation.
//===----------------------------------------------------------------------===//
void ParallelEdtLowering::prepareReductionBuffers(ForOp forOp,
                                                  Value slotCount) {
  assert(forOp && "ForOp cannot be null when preparing reduction buffers");
  assert(slotCount &&
         "SlotCount cannot be null when preparing reduction buffers");

  activeReductions.clear();

  ValueRange reductionAccs = forOp.getReductionAccumulators();
  if (reductionAccs.empty())
    return;

  Value zeroRoute = AC->createIntConstant(0, AC->Int32, loc);
  for (Value acc : reductionAccs) {
    auto memType = acc.getType().dyn_cast<MemRefType>();
    if (!memType) {
      emitError(loc) << "Unsupported reduction accumulator type: "
                     << acc.getType();
      continue;
    }

    Operation *underlying = arts::getUnderlyingDbAlloc(acc);
    auto dbAllocOp = dyn_cast_or_null<DbAllocOp>(underlying);
    Type scalarType;
    if (dbAllocOp)
      scalarType = dbAllocOp.getElementType();
    else
      scalarType = memType.getElementType();
    while (auto nested = scalarType.dyn_cast<MemRefType>())
      scalarType = nested.getElementType();

    if (!scalarType.isIntOrIndexOrFloat()) {
      emitError(loc) << "Only integer, index, or floating-point reductions are "
                        "supported but found type "
                     << scalarType;
      continue;
    }

    SmallVector<Value> elementSizes;
    if (dbAllocOp) {
      elementSizes.assign(dbAllocOp.getElementSizes().begin(),
                          dbAllocOp.getElementSizes().end());
    } else {
      elementSizes.reserve(memType.getRank());
      for (int64_t dim = 0, e = memType.getRank(); dim < e; ++dim) {
        if (memType.isDynamicDim(dim))
          elementSizes.push_back(
              AC->create<memref::DimOp>(loc, acc, dim).getResult());
        else
          elementSizes.push_back(
              AC->createIndexConstant(memType.getDimSize(dim), loc));
      }
    }

    ARTS_DEBUG("Preparing reduction accumulator of type "
               << memType << (dbAllocOp ? " (db)" : " (memref)"));

    SmallVector<int64_t> elementShape;
    if (!elementSizes.empty())
      elementShape.assign(elementSizes.size(), ShapedType::kDynamic);
    MemRefType elementBufferType = MemRefType::get(elementShape, scalarType);

    SmallVector<Value> partialSizes{slotCount};
    auto partialAlloc = AC->create<DbAllocOp>(
        loc, ArtsMode::inout, zeroRoute, DbAllocType::heap, DbMode::write,
        scalarType, partialSizes, elementSizes);
    if (!partialAlloc) {
      emitError(loc) << "Failed to allocate partial reduction datablock";
      continue;
    }

    ReductionInfo info;
    info.originalAcc = acc;
    info.originalType = memType;
    info.elementBufferType = elementBufferType;
    info.scalarType = scalarType;
    info.partialAlloc = partialAlloc;
    info.originalAlloc = dbAllocOp;
    activeReductions.push_back(info);
  }
}

//===----------------------------------------------------------------------===//
/// Append reduction dependencies to the dependency list for a worker EDT.
///
/// Each worker EDT needs write access to its designated slot in the partial
/// result datablocks for each reduction variable. This function creates
/// DbAcquire operations in "out" mode for each active reduction accumulator,
/// using the provided offset as the slot index within the partial buffers.
//===----------------------------------------------------------------------===//
void ParallelEdtLowering::appendReductionDeps(Value offset,
                                              SmallVector<Value> &deps) {
  if (activeReductions.empty())
    return;

  Value oneIndex = AC->createIndexConstant(1, loc);
  for (ReductionInfo &info : activeReductions) {
    SmallVector<Value> offsets{offset};
    SmallVector<Value> sizes{oneIndex};
    DbAllocOp partialAlloc = info.partialAlloc;
    auto acquire = AC->create<DbAcquireOp>(
        loc, ArtsMode::out, partialAlloc.getGuid(), partialAlloc.getPtr(),
        SmallVector<Value>{}, offsets, sizes);
    deps.push_back(acquire.getPtr());
  }
}

//===----------------------------------------------------------------------===//
/// Initialize private accumulator variables for a worker EDT.
///
/// Each worker EDT receives private accumulator arguments that correspond to
/// its slot in the partial result datablocks. This function initializes these
/// accumulators to zero so workers can start accumulating their partial
/// results. It iterates through each private accumulator argument, creates
/// a zero value of the appropriate type, and stores it in the accumulator's
/// memory location.
//===----------------------------------------------------------------------===//
LogicalResult
ParallelEdtLowering::initializePrivateAccumulators(ArrayRef<Value> privateArgs,
                                                   Block &body) {
  if (privateArgs.empty())
    return success();

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPointToStart(&body);
  Value zeroIndex = AC->createIndexConstant(0, loc);
  Value oneIndex = AC->createIndexConstant(1, loc);
  for (auto [idx, arg] : llvm::enumerate(privateArgs)) {
    ReductionInfo &info = activeReductions[idx];
    Value zero = createZeroValue(info.scalarType);
    if (!zero)
      return emitOptionalError(loc, "Unsupported reduction element type for "
                                    "private accumulator initialization");
    auto ptrType = arg.getType().dyn_cast<MemRefType>();
    if (!ptrType)
      return emitOptionalError(
          loc, "Expected memref pointer for private reduction accumulator");

    ReductionAccess access =
        computeReductionAccess(info, arg, zeroIndex, zeroIndex, oneIndex);
    AC->create<memref::StoreOp>(loc, zero, access.buffer, access.indices);
  }
  AC->setInsertionPointToEnd(&body);
  return success();
}

//===----------------------------------------------------------------------===//
/// Create a zero value of the specified type for reduction initialization.
///
/// This utility function creates appropriate zero constants for different
/// numeric types used in reduction operations. It handles index, integer,
/// and floating-point types, returning an appropriate zero value that can
/// be used to initialize reduction accumulators or as a starting value
/// for reduction aggregation.
///
///===----------------------------------------------------------------------===//
Value ParallelEdtLowering::createZeroValue(Type type) {
  if (type.isa<IndexType>())
    return AC->createIndexConstant(0, loc);
  if (auto intTy = type.dyn_cast<IntegerType>())
    return AC->createIntConstant(0, intTy, loc);
  if (auto floatTy = type.dyn_cast<FloatType>())
    return AC->create<arith::ConstantOp>(
        loc, floatTy, AC->getBuilder().getFloatAttr(floatTy, 0.0));
  return Value();
}

//===----------------------------------------------------------------------===//
/// Combine two values using the appropriate reduction operation.
///
/// Performs the core reduction operation by combining two
/// values of the same type. For integer and index types, it uses integer
/// addition. For floating-point types, it uses floating-point addition.
//===----------------------------------------------------------------------===//
Value ParallelEdtLowering::combineValues(Type type, Value lhs, Value rhs) {
  if (type.isa<IntegerType>() || type.isa<IndexType>())
    return AC->create<arith::AddIOp>(loc, lhs, rhs).getResult();

  if (type.isa<FloatType>())
    return AC->create<arith::AddFOp>(loc, lhs, rhs).getResult();

  /// This should never happen as types are validated earlier
  llvm_unreachable("Unsupported reduction element type");
}

Value ParallelEdtLowering::remapDependencyValue(Value value,
                                                ValueRange sourceArgs,
                                                ValueRange mappedValues) {
  if (auto arg = value.dyn_cast<BlockArgument>()) {
    unsigned index = arg.getArgNumber();
    if (index < mappedValues.size())
      return mappedValues[index];
  }

  if (auto constOp = value.getDefiningOp<arith::ConstantOp>())
    return AC->clone(*constOp)->getResult(0);

  return value;
}

void ParallelEdtLowering::remapDependencyList(ValueRange deps,
                                              ValueRange sourceArgs,
                                              ValueRange mappedValues,
                                              SmallVectorImpl<Value> &out) {
  out.clear();
  out.reserve(deps.size());
  for (Value dep : deps)
    out.push_back(remapDependencyValue(dep, sourceArgs, mappedValues));
}

ParallelEdtLowering::ReductionAccess
ParallelEdtLowering::computeReductionAccess(const ReductionInfo &info,
                                            Value buffer, Value linearIndex,
                                            Value zeroIndex, Value strideOne) {
  assert(buffer && "Buffer cannot be null in computeReductionAccess");
  assert(info.originalType &&
         "Original type cannot be null in computeReductionAccess");

  ReductionAccess access;
  unsigned rank = info.originalType.getRank();
  SmallVector<Value> outerIdx(rank, zeroIndex);
  if (rank > 0)
    outerIdx.front() = linearIndex;
  SmallVector<Value> strides(rank, strideOne);
  Value elementPtr = AC->create<DbGepOp>(loc, AC->getLLVMPointerType(buffer),
                                         buffer, outerIdx, strides);
  access.buffer =
      AC->create<LLVM::LoadOp>(loc, info.elementBufferType, elementPtr)
          .getResult();
  unsigned innerRank = info.elementBufferType.getRank();
  access.indices.assign(innerRank, zeroIndex);
  return access;
}

//===----------------------------------------------------------------------===//
/// Create reduction aggregator EDT.
///
/// Creates an arts.edt<task> that combines partial reduction results
/// from all worker EDTs back into the original accumulators.
//===----------------------------------------------------------------------===//
LogicalResult ParallelEdtLowering::createReductionAggregator(
    Value loopCount, Operation *insertAfter, EdtOp sourceOp) {
  assert(loopCount && "LoopCount cannot be null when creating aggregator");
  assert(insertAfter && "InsertAfter cannot be null when creating aggregator");

  if (activeReductions.empty())
    return success();

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPointAfter(insertAfter);

  SmallVector<Value> partialPtrs, mappedAccs;
  SmallVector<bool> partialNeedsRelease, origNeedsRelease;
  Value zeroIndex = AC->createIndexConstant(0, loc);
  Value oneIndex = AC->createIndexConstant(1, loc);
  ARTS_DEBUG("Creating reduction aggregator with " << activeReductions.size()
                                                   << " accumulators");

  /// Create arts.db_acquire for partial results and original accumulators
  for (ReductionInfo &info : activeReductions) {
    /// Acquire partial result datablock (read-only)
    SmallVector<Value> offsets{zeroIndex};
    SmallVector<Value> sizes{loopCount};
    DbAllocOp partialAlloc = info.partialAlloc;
    auto partialAcquire = AC->create<DbAcquireOp>(
        loc, ArtsMode::in, partialAlloc.getGuid(), partialAlloc.getPtr(),
        SmallVector<Value>{}, offsets, sizes);
    partialPtrs.push_back(partialAcquire.getPtr());
    partialNeedsRelease.push_back(true);

    /// Acquire original accumulator (read-write) when datablock-backed.
    if (info.originalAlloc) {
      SmallVector<Value> origOffsets{zeroIndex};
      SmallVector<Value> origSizes{oneIndex};
      auto origAcquire = AC->create<DbAcquireOp>(
          loc, ArtsMode::inout, info.originalAlloc.getGuid(),
          info.originalAlloc.getPtr(), SmallVector<Value>{}, origOffsets,
          origSizes);
      mappedAccs.push_back(origAcquire.getPtr());
      origNeedsRelease.push_back(true);
    } else {
      mappedAccs.push_back(info.originalAcc);
      origNeedsRelease.push_back(false);
    }
  }

  /// Create arts.edt<task> aggregator with all dependencies
  Value zeroRoute = AC->createIntConstant(0, AC->Int32, loc);
  SmallVector<Value> allDeps = partialPtrs;
  allDeps.append(mappedAccs.begin(), mappedAccs.end());

  auto reducerEdt = AC->create<EdtOp>(
      loc, EdtType::task, EdtConcurrency::intranode, zeroRoute, allDeps);
  setupEdtRegion(reducerEdt, sourceOp);
  Block &aggBlock = reducerEdt.getRegion().front();
  ValueRange aggArgsRange = aggBlock.getArguments();
  ValueRange depArgs = aggArgsRange.take_front(partialPtrs.size());
  ValueRange origArgs = aggArgsRange.drop_front(partialPtrs.size());

  /// Implement aggregation logic for each reduction variable
  AC->setInsertionPointToStart(&aggBlock);
  for (auto [idx, info] : llvm::enumerate(activeReductions)) {
    Value depArg = depArgs[idx];
    Value origArg = origArgs[idx];
    Value initial = createZeroValue(info.scalarType);
    assert(initial && "Unsupported reduction element type for aggregator");

    /// Create scf.for to sum all partial results
    auto sumLoop = AC->create<scf::ForOp>(loc, zeroIndex, loopCount, oneIndex,
                                          ValueRange{initial});
    Block &loopBody = sumLoop.getRegion().front();
    AC->setInsertionPointToStart(&loopBody);
    Value iv = sumLoop.getInductionVar();
    Value running = loopBody.getArgument(1);

    /// Load and combine partial result
    ReductionAccess access =
        computeReductionAccess(info, depArg, iv, zeroIndex, oneIndex);
    Value partialVal =
        AC->create<memref::LoadOp>(loc, access.buffer, access.indices);
    Value combined = combineValues(info.scalarType, running, partialVal);
    AC->create<scf::YieldOp>(loc, ValueRange{combined});

    /// Store final result to original accumulator
    AC->setInsertionPointAfter(sumLoop);
    Value finalVal = sumLoop.getResult(0);
    if (info.originalAlloc) {
      ReductionAccess origAccess =
          computeReductionAccess(info, origArg, zeroIndex, zeroIndex, oneIndex);
      AC->create<memref::StoreOp>(loc, finalVal, origAccess.buffer,
                                  origAccess.indices);
    } else {
      SmallVector<Value> indices(info.originalType.getRank(), zeroIndex);
      if (indices.empty())
        AC->create<memref::StoreOp>(loc, finalVal, origArg, ValueRange{});
      else
        AC->create<memref::StoreOp>(loc, finalVal, origArg, indices);
    }
    AC->setInsertionPointToEnd(&aggBlock);
  }

  /// Release all acquired datablocks
  AC->setInsertionPointToEnd(&aggBlock);
  for (auto [depArg, needsRelease] :
       llvm::zip_equal(depArgs, partialNeedsRelease)) {
    if (needsRelease)
      AC->create<DbReleaseOp>(loc, depArg);
  }
  for (auto [origArg, needsRelease] :
       llvm::zip_equal(origArgs, origNeedsRelease)) {
    if (needsRelease)
      AC->create<DbReleaseOp>(loc, origArg);
  }
  if (aggBlock.empty() || !isa<YieldOp>(aggBlock.back()))
    AC->create<YieldOp>(loc);

  return success();
}

//===----------------------------------------------------------------------===//
// Parallel EDT Lowering Pass Implementation
//===----------------------------------------------------------------------===//
struct ParallelEdtLoweringPass
    : public arts::ParallelEdtLoweringBase<ParallelEdtLoweringPass> {
  explicit ParallelEdtLoweringPass() {}
  ~ParallelEdtLoweringPass() override { delete AC; }
  void runOnOperation() override;

private:
  ModuleOp module;
  ArtsCodegen *AC = nullptr;
};

} // namespace

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//

void ParallelEdtLoweringPass::runOnOperation() {
  module = getOperation();
  AC = new ArtsCodegen(module, false);

  ARTS_INFO_HEADER(ParallelEdtLoweringPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Lower all parallel EDTs
  SmallVector<EdtOp, 8> parallelEdts;
  module.walk<WalkOrder::PostOrder>([&](EdtOp edt) {
    if (edt.getType() == EdtType::parallel)
      parallelEdts.push_back(edt);
  });
  ARTS_INFO("Found " << parallelEdts.size() << " parallel EDTs to lower");
  for (EdtOp edt : parallelEdts) {
    ParallelEdtLowering lowerer(AC, edt.getLoc());
    if (failed(lowerer.lowerParallelEdt(edt))) {
      edt.emitError("Failed to lower parallel EDT");
      return signalPassFailure();
    }
  }

  ARTS_INFO_FOOTER(ParallelEdtLoweringPass);
  ARTS_DEBUG_REGION(module.dump(););
}

//===----------------------------------------------------------------------===//
// Pass Registration
//===----------------------------------------------------------------------===//

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createParallelEdtLoweringPass() {
  return std::make_unique<ParallelEdtLoweringPass>();
}

} // namespace arts
} // namespace mlir
