///==========================================================================
/// File: ParallelEdtLowering.cpp
/// Complete implementation of parallel EDT lowering pass that transforms
/// arts.edt<parallel> operations into task graphs before datablock lowering.
///
/// This pass implements parallel EDT lowering that must execute before
/// datablock operations are lowered to ensure proper dependency management.
///
/// Two-Pass Design (ForLowering → ParallelEdtLowering):
/// 1. ForLowering (pre-pass):
///    - Converts arts.for → task EDT with scf.for
///    - Creates arts.get_parallel_worker_id() placeholders
///    - Computes chunk distribution math symbolically
///    - Creates chained db_acquire operations (whole DB, no partitioning)
///
/// 2. ParallelEdtLowering (this pass):
///    - Creates epoch wrapper for synchronization
///    - Creates scf.for loop over workers (0 to num_workers)
///    - Clones parallel EDT body into worker loop
///    - Replaces arts.get_parallel_worker_id() with actual worker IV
///
/// Result: Each worker's task EDT executes with its assigned worker ID,
/// enabling proper chunk distribution and parallel execution.
///
/// DB Partitioning:
/// - ForLowering acquires whole DB (deferred partitioning strategy)
/// - DB pass will later add chunk-based partitioning parameters
/// - See ForLowering.cpp header for detailed partitioning strategy
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

class ParallelInfo;

/// Remap a dependency value from source arguments to mapped values
/// If the value is a BlockArgument, returns the corresponding mapped value
/// If the value is a constant, clones it
/// Otherwise returns the value unchanged
static Value remapDepValue(ArtsCodegen *AC, Value value, ValueRange sourceArgs,
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

/// Result of creating dependencies for a worker
struct WorkerEdtDepsResult {
  SmallVector<Value> deps;                // Dependency values
  SmallVector<DbAcquireOp, 4> acquireOps; // DbAcquire operations created
};

//===----------------------------------------------------------------------===//
// ReductionInfo - Information about a reduction within an arts.for
//===----------------------------------------------------------------------===//
class ReductionInfo {
public:
  ReductionInfo(ArtsCodegen *AC, Value acc) : AC(AC), acc(acc) { initialize(); }

  /// Attributes
  ArtsCodegen *AC;
  Value acc;

  /// Reduction information
  Value originalAcc;            // Original reduction accumulator
  MemRefType originalType;      // Type of original accumulator
  MemRefType elementBufferType; // Type for element buffer
  Type scalarType;              // Scalar type being reduced
  DbAllocOp partialAlloc;       // Preallocated partial result buffer
  DbAllocOp originalAlloc;      // Original accumulator allocation

  /// Helper structure for accessing reduction buffer elements
  struct ReductionAccess {
    Value buffer;               // Buffer to access
    SmallVector<Value> indices; // Indices for access
  };

private:
  void initialize() {
    auto loc = acc.getLoc();
    auto memType = acc.getType().dyn_cast<MemRefType>();
    assert(memType && "Unsupported reduction accumulator type");
    Type scalarType = memType.getElementType();
    assert(scalarType.isIntOrIndexOrFloat() &&
           "Only integer, index, or floating-point reductions are supported");

    DbAllocOp dbAllocOp = acc.getDefiningOp<DbAllocOp>();
    SmallVector<Value> elementSizes;
    if (dbAllocOp) {
      elementSizes = dbAllocOp.getElementSizes();
    } else {
      /// For local memref, create a single-element DbAlloc for the
      /// accumulator
      Value zeroRoute = AC->createIntConstant(0, AC->Int32, loc);
      Value one = AC->createIndexConstant(1, loc);
      dbAllocOp = AC->create<DbAllocOp>(
          loc, ArtsMode::inout, zeroRoute, DbAllocType::stack, DbMode::write,
          scalarType, SmallVector<Value>{one}, SmallVector<Value>{one});
    }

    ARTS_DEBUG("Preparing reduction accumulator of type "
               << memType << (dbAllocOp ? " (db)" : " (memref)"));

    SmallVector<int64_t> elementShape;
    if (!elementSizes.empty())
      elementShape.assign(elementSizes.size(), ShapedType::kDynamic);
    MemRefType elementBufferType = MemRefType::get(elementShape, scalarType);

    // SmallVector<Value> partialSizes{slotCount};
    // auto partialAlloc = AC->create<DbAllocOp>(
    //     loc, ArtsMode::inout, zeroRoute, DbAllocType::heap, DbMode::write,
    //     scalarType, partialSizes, elementSizes);
    // if (!partialAlloc) {
    //   ARTS_ERROR("Failed to allocate partial reduction datablock");
    //   assert(false);
    // }

    // ReductionInfo info;
    // info.originalAcc = acc;
    // info.originalType = memType;
    // info.elementBufferType = elementBufferType;
    // info.scalarType = scalarType;
    // info.partialAlloc = partialAlloc;
    // info.originalAlloc = dbAllocOp;
    // reductions.push_back(info);
  }

  Value computeReductionAccess(const ReductionInfo &info, Value buffer,
                               Value linearIndex, Value zeroIndex,
                               Value strideOne) {
    assert(buffer && "Buffer cannot be null in computeReductionAccess");
    assert(info.originalType &&
           "Original type cannot be null in computeReductionAccess");

    // ReductionAccess access;
    // unsigned rank = info.originalType.getRank();
    // SmallVector<Value> outerIdx(rank, zeroIndex);
    // if (rank > 0)
    //   outerIdx.front() = linearIndex;
    // SmallVector<Value> strides(rank, strideOne);
    // Value elementPtr = AC->create<DbGepOp>(loc,
    // AC->getLLVMPointerType(buffer),
    //                                        buffer, outerIdx, strides);
    // access.buffer =
    //     AC->create<LLVM::LoadOp>(loc, info.elementBufferType, elementPtr)
    //         .getResult();
    // unsigned innerRank = info.elementBufferType.getRank();
    // access.indices.assign(innerRank, zeroIndex);
    // return access;
    return nullptr;
  }
};

//===----------------------------------------------------------------------===//
// LoopInfo - Information about a loop within a parallel EDT
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
  Value chunkSize;           // Chunk size for block distribution
  Value totalWorkers;        // Number of workers
  Value totalIterations;     // Total loop iterations: (upper - lower) / step
  Value totalChunks;         // Total chunks: ceil(totalIterations / chunkSize)
  bool isCyclicDistribution; // Cyclic vs block distribution (currently always
                             // false)

  /// Worker information
  Value workerFirstChunk;     // First chunk assigned to this worker
  Value workerChunkCount;     // Number of chunks for this worker
  Value workerIterationCount; // Total iterations for this worker
  Value workerHasWork;        // Whether this worker has work to do

  /// Reduction information
  SmallVector<ReductionInfo, 4> reductions;

  /// Maps an external dependency to the new dependency created for the worker
  DenseMap<Value, SmallVector<DbAcquireOp, 4>> dependencyMap;

private:
  void initialize() {
    Location loc = forOp.getLoc();

    /// Distribution strategy: Always use block distribution
    /// - If chunk_size attribute is provided: block distribution with that
    /// chunk_size
    /// - If no chunk_size attribute: block distribution with chunk_size = 1
    /// Note: chunk_size controls the size of chunks in block distribution,
    /// not the distribution strategy itself
    if (auto chunkAttr = forOp->getAttrOfType<IntegerAttr>("chunk_size")) {
      int64_t chunkInt = std::max<int64_t>(1, chunkAttr.getInt());
      chunkSize = AC->createIndexConstant(chunkInt, loc);
    } else {
      chunkSize = AC->createIndexConstant(1, loc);
    }
    /// Always use block distribution
    isCyclicDistribution = false;

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
    totalChunks =
        AC->create<arith::DivUIOp>(loc, adjustedIterations, chunkSize);

    /// Check if the loop has reductions
    /// TODO: Implement reduction information for the loop
    // setReductionInfo(totalChunks);

    /// Compute worker iterations (always block distribution)
    /// Note: workerId needs to be provided when calling
    /// computeWorkerIterationsBlock This is called per-worker in the worker
    /// loop
  }

  //===----------------------------------------------------------------------===//
  /// Set reduction information for the given for loop.
  /// Creates partial result datablocks for each reduction accumulator found in
  /// the for loop. Each partial buffer is allocated with the specified slot
  /// count to store intermediate results from parallel worker EDTs before final
  /// aggregation.

  //===----------------------------------------------------------------------===//
#if 0  // OLD CODE - Reduction handling moved to ForLowering
  void setReductionInfo(Value slotCount) {
    Location loc = forOp.getLoc();
    ValueRange reductionAccs = forOp.getReductionAccumulators();
    assert(reductionAccs.size() == 0 &&
           "Reduction accumulators are not supported");
    if (reductionAccs.empty())
      return;

    Value zeroRoute = AC->createIntConstant(0, AC->Int32, loc);
    for (Value acc : reductionAccs) {
    }
  }
#endif // OLD CODE

  //===----------------------------------------------------------------------===//
  /// Compute worker iteration bounds for block distribution
  ///
  /// Calculates what iterations a specific worker should execute in block
  /// distribution, where each worker gets a contiguous range of iterations.
  ///
  /// The algorithm works in two phases:
  /// 1. Distribute chunks across workers (chunks may contain multiple
  ///    iterations)
  /// 2. Convert chunk assignments to iteration assignments
  ///
  /// Block distribution example with 4 workers, loop 0 to 100, chunk_size = 1:
  /// - Total iterations = 100, totalChunks = 100
  /// - chunksPerWorker = 100 / 4 = 25
  /// - remainderChunks = 100 % 4 = 0
  ///
  /// - Worker 0: firstChunk = 0, chunkCount = 25, iterations = 0-24 (25)
  /// - Worker 1: firstChunk = 25, chunkCount = 25, iterations = 25-49 (25)
  /// - Worker 2: firstChunk = 50, chunkCount = 25, iterations = 50-74 (25)
  /// - Worker 3: firstChunk = 75, chunkCount = 25, iterations = 75-99 (25)
  ///
  /// Uneven example with 101 iterations, chunk_size = 1:
  /// - Total iterations = 101, totalChunks = 101
  /// - chunksPerWorker = 25, remainder = 1
  ///   - Worker 0: firstChunk = 0, chunkCount = 26, iterations = 0-25 (26)
  ///   - Worker 1: firstChunk = 26, chunkCount = 25, iterations = 26-50 (25)
  ///   - Worker 2: firstChunk = 51, chunkCount = 25, iterations = 51-75 (25)
  ///   - Worker 3: firstChunk = 76, chunkCount = 25, iterations = 76-100 (25)
  //===----------------------------------------------------------------------===//
  void computeWorkerIterationsBlock(Value workerId) {
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
    Value needZero = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::ult, totalIterations, workerFirstIter);

    /// Calculate how many iterations are actually available to this worker
    Value remaining =
        AC->create<arith::SubIOp>(loc, totalIterations, workerFirstIter);

    /// Clamp to zero if worker starts beyond end (shouldn't happen in
    /// practice)
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
  /// Compute worker iteration bounds (cyclic distribution)
  ///
  /// Calculates what chunks a specific worker should execute in cyclic
  /// distribution, where each worker gets chunks in a round-robin fashion.
  ///
  /// The algorithm calculates how many chunks each worker gets:
  /// - Worker i gets chunks: i, i+numWorkers, i+2*numWorkers, ...
  /// - Number of chunks = ceil((totalChunks - workerId) / numWorkers)
  ///
  /// Formula: (totalChunks - workerId + numWorkers - 1) / numWorkers
  /// This implements ceiling division: ceil((totalChunks - workerId) /
  /// numWorkers)
  ///
  /// Example: 20 chunks, 8 workers
  ///   - Worker 0: chunks 0,8,16 -> (20-0+8-1)/8 = 27/8 = 3 chunks
  ///   - Worker 4: chunks 4,12 -> (20-4+8-1)/8 = 23/8 = 2 chunks
  ///   - Worker 7: chunks 7,15 -> (20-7+8-1)/8 = 20/8 = 2 chunks
  //===----------------------------------------------------------------------===//
  void computeWorkerIterationsCyclic(Value workerId) {
    auto loc = forOp.getLoc();
    Value zero = AC->createIndexConstant(0, loc);
    Value one = AC->createIndexConstant(1, loc);

    /// Calculate how many chunks this worker gets in cyclic distribution
    /// chunksAfterWorker = totalChunks - workerId
    Value chunksAfterWorker =
        AC->create<arith::SubIOp>(loc, totalChunks, workerId);

    /// Handle edge case: avoid division by zero if numWorkers == 0
    /// If numWorkers > 1, use (numWorkers - 1) for ceiling division
    /// If numWorkers == 1, use 0 (no adjustment needed)
    Value moreThanOne = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::ugt, totalWorkers, zero);
    Value workersMinusOne = AC->create<arith::SelectOp>(
        loc, moreThanOne, AC->create<arith::SubIOp>(loc, totalWorkers, one),
        zero);

    /// Ceiling division: (a + b - 1) / b
    /// Formula: (totalChunks - workerId + numWorkers - 1) / numWorkers
    Value numerator =
        AC->create<arith::AddIOp>(loc, chunksAfterWorker, workersMinusOne);
    workerChunkCount = AC->create<arith::DivUIOp>(loc, numerator, totalWorkers);

    /// Determine if this worker has work to do
    /// workerHasWork = workerChunkCount > 0
    workerHasWork = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ugt,
                                              workerChunkCount, zero);
  }

  //===----------------------------------------------------------------------===//
  /// Create dependencies for a worker/chunk
  ///
  /// Generalized function that creates DbAcquire operations for datablock
  /// dependencies. Uses block distribution with the provided chunk_size.
  ///
  /// Parameters:
  /// - chunkId: Unused (kept for API compatibility, uses workerFirstChunk
  /// internally)
  ///
  /// Uses member variables:
  /// - chunkSize: Number of iterations per chunk (from chunk_size attribute or
  /// 1)
  /// - totalIterations: Total iterations (for bounds checking)
  /// - workerFirstChunk: First chunk assigned to this worker
  ///
  /// Computes chunkOffset internally:
  ///   chunkOffset = workerFirstChunk * chunkSize
  //===----------------------------------------------------------------------===//
  WorkerEdtDepsResult createWorkerDeps(EdtOp op, Value chunkId) {
    auto loc = op.getLoc();
    WorkerEdtDepsResult result;
    ValueRange srcArgs = op.getRegion().front().getArguments();
    ValueRange srcDeps = op.getDependencies();

    /// Use workerFirstChunk for block distribution
    /// chunkOffset = workerFirstChunk * chunkSize
    Value chunkOffset =
        AC->create<arith::MulIOp>(loc, workerFirstChunk, chunkSize);

    /// Handle last chunk: may have fewer iterations if chunk extends beyond
    /// totalIterations
    Value remaining =
        AC->create<arith::SubIOp>(loc, totalIterations, chunkOffset);
    Value actualChunkSize =
        AC->create<arith::MinUIOp>(loc, chunkSize, remaining);

    for (Value dep : srcDeps) {
      if (auto acq = dep.getDefiningOp<DbAcquireOp>()) {
        // bool isReductionDep = llvm::any_of(activeReductions, [&](auto &info)
        // {
        //   return info.originalAlloc &&
        //          acq.getSourcePtr() == info.originalAlloc.getPtr();
        // });
        // if (isReductionDep)
        //   continue;

        Value sourceGuid =
            remapDepValue(AC, acq.getSourceGuid(), srcArgs, srcDeps);
        Value sourcePtr =
            remapDepValue(AC, acq.getSourcePtr(), srcArgs, srcDeps);

        /// Acquire the slice of datablocks needed for this chunk
        /// indices=[chunkOffset] acquires datablocks starting at chunkOffset
        /// sizes=[actualChunkSize] acquires actualChunkSize elements
        SmallVector<Value> indices{chunkOffset};
        SmallVector<Value> offsets{};
        SmallVector<Value> sizes{actualChunkSize};

        auto newAcquire = AC->create<DbAcquireOp>(
            loc, acq.getMode(), sourceGuid, sourcePtr, indices, offsets, sizes);
        ARTS_DEBUG("  Created DbAcquire " << newAcquire << " with indices=["
                                          << chunkOffset << "] sizes=["
                                          << actualChunkSize << "]");
        result.acquireOps.push_back(newAcquire);
        result.deps.push_back(newAcquire.getPtr());
        continue;
      }

      result.deps.push_back(remapDepValue(AC, dep, srcArgs, srcDeps));
    }
    return result;
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
#if 0 // OLD CODE - Replaced by new ForLowering + simplified ParallelEdtLowering
  void lowerStaticBlock(EdtOp op, ForOp containedForOp, Value lb, Value ub,
                        bool isInterNode,
                        const SmallVector<SingleEdtInfo> &singleConstructs = {}) {

    Value zero = AC->createIndexConstant(0, loc);
    Value one = AC->createIndexConstant(1, loc);

    /// Create scf.for loop over workers
    scf::ForOp workerLoop = AC->create<scf::ForOp>(loc, lb, ub, one);

    OpBuilder::InsertionGuard IG(AC->getBuilder());
    AC->setInsertionPointToStart(workerLoop.getBody());

    Value workerId = workerLoop.getInductionVar();

    ARTS_DEBUG("Processing worker " << workerId << " (block distribution)");

    /// Create LoopInfo and compute worker iteration bounds
    LoopInfo info(AC, containedForOp, ub);
    info.computeWorkerIterationsBlock(workerId);

    ARTS_DEBUG("Block worker info: id="
               << workerId << " firstChunk=" << info.workerFirstChunk
               << " chunkCount=" << info.workerChunkCount
               << " iterations=" << info.workerIterationCount);

    /// Skip workers with no work
    auto ifHasWork =
        AC->create<scf::IfOp>(loc, info.workerHasWork, /*withElseRegion=*/false);

    AC->setInsertionPointToStart(&ifHasWork.getThenRegion().front());

    ARTS_DEBUG("  Worker has " << info.workerIterationCount << " iterations");

    /// Set route based on inter/intra-node execution
    Value route;
    if (isInterNode) {
      route = AC->castToInt(AC->Int32, workerId, loc);
    } else {
      route = AC->createIntConstant(0, AC->Int32, loc);
    }

    /// Create dependencies for this worker
    /// For block distribution, chunkId parameter is unused (uses
    /// workerFirstChunk)
    WorkerEdtDepsResult depsResult = info.createWorkerDeps(op, workerId);

    ValueRange srcArgs = op.getRegion().front().getArguments();
    SmallVector<Value> workerDeps = depsResult.deps;

    /// Add single construct atomic flags as dependencies
    /// Acquire the flags for this worker EDT
    SmallVector<size_t> singleFlagIndices;
    SmallVector<DbAcquireOp> singleFlagAcquires;
    for (const auto &singleInfo : singleConstructs) {
      // Acquire the atomic flag for this worker EDT
      auto flagAcquire = AC->create<DbAcquireOp>(
          loc, ArtsMode::inout, singleInfo.flagGuid, singleInfo.flagPtr,
          SmallVector<Value>{}, SmallVector<Value>{zero},
          SmallVector<Value>{one});

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

    // After refactoring: sizes=[1,N] → memref<?x?xi32>
    // db_acquire with indices=[start] sizes=[count] → memref<?xi32>
    // No db_ref needed - we can access the memref directly with local indices

    ARTS_DEBUG("Fixing " << dbRefs.size() << " DbRef and "
                         << (memLoads.size() + memStores.size())
                         << " load/store operations");

    // Replace db_ref operations with direct memref access
    for (DbRefOp dbRef : dbRefs) {
      // The db_ref returns the datablock argument directly (no indexing
      // needed) Replace uses of db_ref result with the block argument
      Value blockArg = dbRef.getSource();
      dbRef.getResult().replaceAllUsesWith(blockArg);
      dbRef.erase();
      ARTS_DEBUG("  Removed db_ref, using block argument directly");
    }

    // Adjust memref.load/store to use local 0-based indices
    for (memref::LoadOp load : memLoads) {
      AC->setInsertionPoint(load);
      Value globalIndex = load.getIndices().front();
      Value localIndex =
          AC->create<arith::SubIOp>(loc, globalIndex, startIteration);
      load.getIndicesMutable().assign(ValueRange{localIndex});
      ARTS_DEBUG("  Adjusted load to local index");
    }

    for (memref::StoreOp store : memStores) {
      AC->setInsertionPoint(store);
      Value globalIndex = store.getIndices().front();
      Value localIndex =
          AC->create<arith::SubIOp>(loc, globalIndex, startIteration);
      store.getIndicesMutable().assign(ValueRange{localIndex});
      ARTS_DEBUG("  Adjusted store to local index");
    }

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
#endif // OLD CODE
};

//===----------------------------------------------------------------------===//
// SingleEdtInfo - Information about a single construct within a parallel EDT
//===----------------------------------------------------------------------===//
class SingleEdtInfo {
public:
  /// Constructor
  SingleEdtInfo(ArtsCodegen *AC, EdtOp singleEdt)
      : AC(AC), singleEdt(singleEdt) {
    initialize();
  }

  /// Attributes
  ArtsCodegen *AC;
  EdtOp singleEdt;
  Value flagGuid;
  Value flagPtr;

private:
  //===----------------------------------------------------------------------===//
  /// Initialize the single edt info
  ///
  /// This initializes the single edt info with the single edt operation and
  /// allocates a datablock containing a single i32 value initialized to 0.
  /// This flag will be used for atomic synchronization - only the first
  /// worker to atomically swap it to 1 will execute the single body.
  //===----------------------------------------------------------------------===//
  void initialize() {
    Location loc = singleEdt.getLoc();
    Value zero = AC->createIntConstant(0, AC->Int32, loc);
    Value one = AC->createIndexConstant(1, loc);

    /// Allocate a datablock with one i32 element, pre-initialized to 0
    SmallVector<Value> sizes{one};
    SmallVector<Value> elementSizes{one};
    auto flagAlloc =
        AC->create<DbAllocOp>(loc, ArtsMode::inout, zero, DbAllocType::heap,
                              DbMode::pinned, AC->Int32, sizes, elementSizes);
    flagGuid = flagAlloc.getGuid();
    flagPtr = flagAlloc.getPtr();

    /// Initialize the atomic flag to 0
    AC->create<memref::StoreOp>(loc, zero, flagPtr, ValueRange{zero});
    ARTS_DEBUG("Allocated atomic flag for single construct: guid=" << flagGuid);
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
    Location loc = singleEdt.getLoc();
    OpBuilder::InsertionGuard IG(AC->getBuilder());

    // Insert at the beginning of worker body (before loop iterations)
    AC->setInsertionPointToStart(&workerBody);

    Value zero = AC->createIndexConstant(0, loc);
    Value zeroI32 = AC->createIntConstant(0, AC->Int32, loc);
    Value oneI32 = AC->createIntConstant(1, AC->Int32, loc);

    /// Step 1: Get atomic flag from worker block arguments
    BlockArgument flagArg = workerBody.getArgument(flagArgIdx);
    Value flagMemref = AC->create<DbRefOp>(loc, flagArg, zero);

    ARTS_DEBUG("Inserting single construct with atomic flag at arg index "
               << flagArgIdx);

    /// Step 2: Atomic swap - only first worker gets 0 back
    auto atomicSwapCall = AC->createRuntimeCall(
        RuntimeFunction::ARTSRTL_artsAtomicSwap, {flagMemref, oneI32}, loc);
    Value oldValue = atomicSwapCall.getResult(0);

    /// Check if we were the first worker (got 0 back)
    Value shouldExecute = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::eq, oldValue, zeroI32);

    /// Step 3: Conditionally execute single body
    auto ifOp =
        AC->create<scf::IfOp>(loc, shouldExecute, /*withElseRegion=*/false);
    Block &thenBlock = ifOp.getThenRegion().front();
    AC->setInsertionPointToStart(&thenBlock);

    /// Build mapper starting from the base mapping computed for the worker EDT
    IRMapping mapper = baseMapper;

    /// Map parallel arguments to worker arguments to handle outer scope
    /// captures
    ValueRange parallelArgs = parallelEdt.getRegion().front().getArguments();
    ValueRange workerArgs = workerBody.getArguments();
    mapper.map(parallelArgs, workerArgs.take_front(parallelArgs.size()));

    /// Then map single's own arguments to worker arguments (as before)
    Block &singleBody = singleEdt.getRegion().front();
    ValueRange singleArgs = singleBody.getArguments();

    for (auto [idx, singleArg] : llvm::enumerate(singleArgs)) {
      if (idx < workerArgs.size())
        mapper.map(singleArg, workerArgs[idx]);
    }

    /// Now clone operations with the complete mapper
    for (Operation &op : singleBody.without_terminator()) {
      /// Rematerialize constants if needed
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

      /// Clone the operation
      Operation *clonedOp = AC->clone(op, mapper);

      /// Update mapper with cloned results
      for (auto [oldRes, newRes] :
           llvm::zip(op.getResults(), clonedOp->getResults())) {
        mapper.map(oldRes, newRes);
      }
    }

    ARTS_DEBUG("Successfully inserted single construct into worker EDT");
  }
};

//===----------------------------------------------------------------------===//
// ParallelEdtLowering - Lowers parallel EDTs into explicit task EDTs.
//
// This class implements OpenMP static scheduling semantics for ARTS.
// When an arts.edt<parallel> contains an arts.for, it transforms it into:
// - For static without chunk_size: Block distribution of contiguous iteration
//   ranges
// - For static with chunk_size: Round-robin (cyclic) distribution of chunks
// - Inter-node: Round-robin across nodes via route for cyclic, direct route
// for
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
  /// Information about the parallel EDT
  struct ParallelInfo {
    EdtOp parallelEdtOp;
    Value lowerBound;
    Value upperBound;
    bool isInterNode;

    /// Single constructs (if any)
    SmallVector<SingleEdtInfo, 4> singles;
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
    OpBuilder::InsertionGuard IG(AC->getBuilder());
    AC->setInsertionPoint(op);

    /// Step 1: Create epoch wrapper for synchronization
    EpochOp epochOp = createEpochWrapper();
    Block &epochBody = epochOp.getRegion().front();
    AC->setInsertionPointToEnd(&epochBody);

    /// Step 2: Set up parallel information
    ParallelInfo parallelInfo;
    setParallelInfo(op, parallelInfo);

    /// Step 3: Create scf.for loop over workers
    Value one = AC->createIndexConstant(1, loc);
    scf::ForOp workerLoop = AC->create<scf::ForOp>(
        loc, parallelInfo.lowerBound, parallelInfo.upperBound, one);

    /// Step 4: Clone parallel EDT body into worker loop and replace
    /// placeholders
    createWorkerEdt(workerLoop, parallelInfo);

    /// Step 5: Cleanup - yield from epoch and erase original parallel EDT
    AC->create<YieldOp>(loc);
    op.erase();

    return success();
  }

private:
  void createWorkerEdt(scf::ForOp workerLoop, ParallelInfo &parallelInfo) {
    OpBuilder::InsertionGuard LIG(AC->getBuilder());
    AC->setInsertionPointToStart(workerLoop.getBody());
    Value workerId = workerLoop.getInductionVar();
    EdtOp parallelEdt = parallelInfo.parallelEdtOp;

    ARTS_DEBUG("Processing worker " << workerId << " of "
                                    << parallelInfo.upperBound);

    /// Clone the parallel EDT body directly into the worker loop
    /// This transfers all task EDTs created by ForLowering
    Block &srcBody = parallelEdt.getRegion().front();
    Block &dstBody = *workerLoop.getBody();

    IRMapping mapper;

    /// Map parallel EDT block arguments to the EDT dependencies
    /// (these are already available in the outer scope)
    ValueRange srcArgs = srcBody.getArguments();
    ValueRange srcDeps = parallelEdt.getDependencies();
    for (auto [srcArg, srcDep] : llvm::zip(srcArgs, srcDeps)) {
      mapper.map(srcArg, srcDep);
    }

    /// Clone all operations from parallel EDT body
    OpBuilder bodyBuilder(&dstBody, dstBody.begin());
    for (Operation &op : srcBody.without_terminator()) {
      bodyBuilder.clone(op, mapper);
    }

    /// Now walk the cloned operations and replace all uses of
    /// arts.get_parallel_worker_id() with the worker loop IV
    replaceWorkerIdPlaceholders(dstBody, workerId);

    /// Handle single constructs if present
    if (!parallelInfo.singles.empty()) {
      handleSingleConstructs(dstBody, parallelInfo, workerId);
    }

    /// Add yield to worker loop body if needed
    if (dstBody.empty() || !isa<scf::YieldOp>(dstBody.back())) {
      bodyBuilder.setInsertionPointToEnd(&dstBody);
      bodyBuilder.create<scf::YieldOp>(loc);
    }
  }

  //===----------------------------------------------------------------------===//
  /// Replace arts.get_parallel_worker_id() placeholders with actual worker IV
  ///
  /// This method walks through all operations in the block and replaces any
  /// arts.get_parallel_worker_id() operations with the provided worker ID
  /// value. This is the critical step that binds the symbolic worker ID
  /// placeholder (created by ForLowering) to the actual scf.for induction
  /// variable.
  //===----------------------------------------------------------------------===//
  void replaceWorkerIdPlaceholders(Block &block, Value workerId) {
    SmallVector<GetParallelWorkerIdOp, 4> placeholders;

    /// Collect all placeholder operations
    block.walk([&](GetParallelWorkerIdOp op) { placeholders.push_back(op); });

    /// Replace each placeholder with the worker ID
    for (GetParallelWorkerIdOp placeholder : placeholders) {
      ARTS_DEBUG("Replacing get_parallel_worker_id with worker loop IV");
      placeholder.getResult().replaceAllUsesWith(workerId);
      placeholder.erase();
    }
  }

  //===----------------------------------------------------------------------===//
  /// Handle single constructs within the parallel EDT
  ///
  /// Single constructs (arts.edt<single>) are executed by only one worker.
  /// This is implemented using atomic flags that ensure only the first
  /// worker to reach the single construct executes it.
  ///
  /// TODO: Implement proper single construct handling for the new design.
  /// The challenge is that single constructs need atomic flags passed as
  /// dependencies, but in the new design we're cloning directly into the
  /// worker loop rather than creating worker EDTs with dependencies.
  /// This requires rethinking how atomic flags are passed to the single
  /// constructs.
  //===----------------------------------------------------------------------===//
  void handleSingleConstructs(Block &block, ParallelInfo &parallelInfo,
                              Value workerId) {
    /// Find all single EDTs in the cloned block
    SmallVector<EdtOp, 4> singleEdts;
    block.walk([&](EdtOp edt) {
      if (edt.getType() == EdtType::single)
        singleEdts.push_back(edt);
    });

    if (!singleEdts.empty()) {
      ARTS_ERROR("Single constructs are not yet supported in the new "
                 "ParallelEdtLowering design");
      ARTS_ERROR("Found " << singleEdts.size()
                          << " single construct(s) - skipping");
    }
  }

  void remapDepList(ValueRange deps, ValueRange sourceArgs,
                    ValueRange mappedValues, SmallVectorImpl<Value> &out);

  EpochOp createEpochWrapper() {
    auto epochOp = AC->create<EpochOp>(loc);
    auto &region = epochOp.getRegion();
    if (region.empty())
      region.push_back(new Block());
    return epochOp;
  }

  //===----------------------------------------------------------------------===//
  /// Refactor DbAllocOp from coarse to fine-grained for parallel access
  ///
  /// Changes: sizes=[1] elementSizes=[N] -> sizes=[N] elementSizes=[]
  /// This allows workers to acquire individual elements using indices
  //===----------------------------------------------------------------------===//
  void refactorDbAllocForParallel(DbAllocOp allocOp) {
    ARTS_DEBUG("Refactoring DbAlloc " << allocOp << " for parallel access");

    // Get current sizes and elementSizes
    auto sizes = allocOp.getSizes();
    auto elementSizes = allocOp.getElementSizes();

    // Replace sizes with elementSizes for fine-grained allocation
    // Original: sizes=[1] elementSizes=[N] → memref<?xmemref<memref<?xi32>>>
    // After:    sizes=[N] elementSizes=[] → memref<?xi32>
    // This allows workers to acquire slices using indices
    if (!elementSizes.empty() && sizes.size() == 1) {
      SmallVector<Value> newSizes(elementSizes.begin(), elementSizes.end());
      SmallVector<Value> newElementSizes{};

      allocOp.getSizesMutable().assign(newSizes);
      allocOp.getElementSizesMutable().assign(newElementSizes);

      ARTS_DEBUG("  Refactored from sizes=["
                 << sizes[0] << "] elementSizes=[" << elementSizes[0]
                 << "] to sizes=[" << newSizes[0] << "] elementSizes=[]");
    }
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
  /// Set up parallel information for EDT
  ///
  /// Analyzes EDT attributes to determine the number of workers and whether
  /// the parallelism is intra-node or inter-node. Uses concurrency pass
  /// attributes or falls back to runtime worker count.
  //===----------------------------------------------------------------------===//
  void setParallelInfo(EdtOp edtOp, ParallelInfo &parallelInfo) {
    /// Set the parallel EDT operation
    parallelInfo.parallelEdtOp = edtOp;

    /// Set the lower bound to 0
    parallelInfo.lowerBound = AC->createIndexConstant(0, loc);

    /// Set the upper bound to the number of workers
    bool isParallel = edtOp.getType() == EdtType::parallel;
    parallelInfo.isInterNode =
        (edtOp.getConcurrency() == EdtConcurrency::internode);

    /// Read parallelism strategy from EDT attributes set by concurrency pass
    if (auto workersAttr = edtOp.getWorkers()) {
      int numWorkers = workersAttr->getValue();
      parallelInfo.upperBound = AC->createIndexConstant(numWorkers, loc);

      ARTS_INFO("Using EDT attributes: workers="
                << numWorkers << ", concurrency="
                << (parallelInfo.isInterNode ? "internode" : "intranode"));
      return;
    }

    if (!isParallel) {
      /// Non-parallel EDTs without explicit workers execute on a single
      /// worker.
      parallelInfo.upperBound = AC->createIndexConstant(1, loc);
      ARTS_INFO("Non-parallel EDT without workers attribute; defaulting to a"
                " single worker");
      return;
    }

    /// Fallback: no attributes set, query runtime for the appropriate count.
    Value runtimeCount = parallelInfo.isInterNode ? AC->getTotalNodes(loc)
                                                  : AC->getTotalWorkers(loc);
    parallelInfo.upperBound = AC->castToIndex(runtimeCount, loc);

    if (parallelInfo.isInterNode)
      ARTS_WARN("No workers attribute set on parallel EDT - using runtime "
                "total nodes fallback");
    else
      ARTS_WARN("No workers attribute set on parallel EDT - using runtime "
                "total workers fallback");
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
#if 0 // OLD CODE - Replaced by new ForLowering + simplified ParallelEdtLowering
  void lowerStaticCyclic(EdtOp op, ForOp containedForOp, Value lb, Value ub,
                         bool isInterNode,
                         const SmallVector<SingleEdtInfo> &singleConstructs = {}) {

    LoopInfo info;
    setLoopInfo(containedForOp, parallelInfo, info);

    Value zero = AC->createIndexConstant(0, loc);
    Value one = AC->createIndexConstant(1, loc);

    /// Create scf.for loop over workers
    scf::ForOp workerLoop = AC->create<scf::ForOp>(loc, lb, ub, one);

    OpBuilder::InsertionGuard IG(AC->getBuilder());
    AC->setInsertionPointToStart(workerLoop.getBody());

    Value workerId = workerLoop.getInductionVar();

    ARTS_DEBUG("Processing worker " << workerId << " of " << ub);

    /// Compute chunk count for this worker in cyclic distribution
    info.computeWorkerIterationsCyclic(workerId);

    /// Skip workers with no work
    /// If workerChunkCount == 0, don't create EDT for this worker
    auto ifHasWork =
        AC->create<scf::IfOp>(loc, info.workerHasWork, /*withElseRegion=*/false);

    AC->setInsertionPointToStart(&ifHasWork.getThenRegion().front());

    ARTS_DEBUG("  Worker has " << info.workerChunkCount << " chunks");

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
      workerDeps.push_back(remapDepValue(AC, dep, srcArgs, srcDeps));
    }

    /// Add single construct atomic flags as dependencies
    /// Acquire the flags for this worker EDT
    SmallVector<size_t> singleFlagIndices;
    SmallVector<DbAcquireOp> singleFlagAcquires;
    for (const auto &singleInfo : singleConstructs) {
      // Acquire the atomic flag for this worker EDT
      auto flagAcquire = AC->create<DbAcquireOp>(
          loc, ArtsMode::inout, singleInfo.flagGuid, singleInfo.flagPtr,
          SmallVector<Value>{}, SmallVector<Value>{zero},
          SmallVector<Value>{one});

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
        AC->create<scf::ForOp>(loc, zero, info.workerChunkCount, one);

    Block &chunkLoopBody = localChunkLoop.getRegion().front();
    AC->setInsertionPointToStart(&chunkLoopBody);

    Value localChunkIdx = localChunkLoop.getInductionVar();

    /// Calculate global chunk ID for this local chunk
    /// globalChunkId = workerId + localChunkIdx * numWorkers
    ///
    /// Cyclic distribution pattern:
    /// - Worker 0: chunks 0, numWorkers, 2*numWorkers, ...
    /// - Worker 1: chunks 1, numWorkers+1, 2*numWorkers+1, ...
    ///
    /// Example: worker=1, numWorkers=4
    ///   - localChunk=0 -> globalChunkId = 1 + 0*4 = 1
    ///   - localChunk=1 -> globalChunkId = 1 + 1*4 = 5
    ///   - localChunk=2 -> globalChunkId = 1 + 2*4 = 9
    Value chunkIdOffset =
        AC->create<arith::MulIOp>(loc, localChunkIdx, info.totalWorkers);
    Value globalChunkId =
        AC->create<arith::AddIOp>(loc, workerId, chunkIdOffset);

    ARTS_DEBUG("    Local chunk " << localChunkIdx << " -> global chunk "
                                  << globalChunkId);

    /// Convert chunk ID to iteration index
    /// chunkStartIter = globalChunkId * chunkSize
    /// This gives the starting iteration index for this chunk
    Value chunkStartIter =
        AC->create<arith::MulIOp>(loc, globalChunkId, info.chunkSize);

    /// Handle last chunk: may have fewer iterations if chunk extends beyond
    /// totalIterations. This ensures we don't process iterations beyond the
    /// loop bounds.
    /// remaining = totalIterations - chunkStartIter (iterations remaining)
    /// actualChunkSize = min(chunkSize, remaining) (clamp to available)
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

    /// Calculate global iteration number (0-based iteration index)
    /// globalIterNumber = chunkStartIter + localIter
    /// This is the iteration index within the total iteration space
    Value globalIterNumber =
        AC->create<arith::AddIOp>(loc, chunkStartIter, localIter);

    /// Convert global iteration number to actual loop index value
    /// The loop may have a non-zero lower bound and non-unit step
    /// actualLoopIndex = lowerBound + globalIterNumber * loopStep
    ///
    /// Example: loop from 10 to 100 step 3
    ///   - globalIterNumber=0 -> actualLoopIndex = 10 + 0*3 = 10
    ///   - globalIterNumber=1 -> actualLoopIndex = 10 + 1*3 = 13
    ///   - globalIterNumber=2 -> actualLoopIndex = 10 + 2*3 = 16
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

    // After refactoring: sizes=[1,N] → memref<?x?xi32>
    // db_acquire with indices=[start] sizes=[count] → memref<?xi32>
    // No db_ref needed - we can access the memref directly with local indices

    ARTS_DEBUG("Fixing " << dbRefs.size() << " DbRef and "
                         << (memLoads.size() + memStores.size())
                         << " load/store operations");

    // Replace db_ref operations with direct memref access
    for (DbRefOp dbRef : dbRefs) {
      // The db_ref returns the datablock argument directly (no indexing
      // needed) Replace uses of db_ref result with the block argument
      Value blockArg = dbRef.getSource();
      dbRef.getResult().replaceAllUsesWith(blockArg);
      dbRef.erase();
      ARTS_DEBUG("  Removed db_ref, using block argument directly");
    }

    // Adjust memref.load/store to use local 0-based indices
    for (memref::LoadOp load : memLoads) {
      AC->setInsertionPoint(load);
      Value globalIndex = load.getIndices().front();
      Value localIndex =
          AC->create<arith::SubIOp>(loc, globalIndex, chunkStartIter);
      load.getIndicesMutable().assign(ValueRange{localIndex});
      ARTS_DEBUG("  Adjusted load to local index");
    }

    for (memref::StoreOp store : memStores) {
      AC->setInsertionPoint(store);
      Value globalIndex = store.getIndices().front();
      Value localIndex =
          AC->create<arith::SubIOp>(loc, globalIndex, chunkStartIter);
      store.getIndicesMutable().assign(ValueRange{localIndex});
      ARTS_DEBUG("  Adjusted store to local index");
    }

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
  /// Set loop information
  ///
  /// This function calculates the total number of iterations and chunks for a
  /// loop based on the loop bounds, step, and chunk size.
  //===----------------------------------------------------------------------===//

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
#endif // OLD CODE
};

#if 0  // OLD CODE - Reduction handling moved to ForLowering
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
#endif // OLD CODE

/// Collect the dependencies of the parallel EDT and remap them to the block
/// arguments of the parallel EDT
void ParallelEdtLowering::remapDepList(ValueRange deps, ValueRange sourceArgs,
                                       ValueRange mappedValues,
                                       SmallVectorImpl<Value> &out) {

  out.clear();
  out.reserve(deps.size());
  for (Value dep : deps)
    out.push_back(remapDepValue(AC, dep, sourceArgs, mappedValues));
}

#if 0  // OLD CODE - Reduction handling moved to ForLowering
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
  if (activeReductions.empty())
    return success();

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPointAfter(insertAfter);

  SmallVector<DbAcquireOp> partialAcquires;
  SmallVector<DbAcquireOp> origAcquires(activeReductions.size(), nullptr);
  Value zeroIndex = AC->createIndexConstant(0, loc);
  Value oneIndex = AC->createIndexConstant(1, loc);
  ARTS_DEBUG("Creating reduction aggregator with " << activeReductions.size()
                                                   << " accumulators");

  /// Create arts.db_acquire for partial results and original accumulators if
  /// db
  for (size_t i = 0; i < activeReductions.size(); ++i) {
    auto &info = activeReductions[i];
    /// Acquire partial result datablock (read-only)
    SmallVector<Value> offsets{zeroIndex};
    SmallVector<Value> sizes{loopCount};
    DbAllocOp partialAlloc = info.partialAlloc;
    auto partialAcquire = AC->create<DbAcquireOp>(
        loc, ArtsMode::in, partialAlloc.getGuid(), partialAlloc.getPtr(),
        SmallVector<Value>{}, offsets, sizes);
    partialAcquires.push_back(partialAcquire);

    /// Acquire original accumulator (read-write) if db-backed
    if (info.originalAlloc) {
      SmallVector<Value> origOffsets{zeroIndex};
      SmallVector<Value> origSizes{oneIndex};
      auto origAcquire = AC->create<DbAcquireOp>(
          loc, ArtsMode::inout, info.originalAlloc.getGuid(),
          info.originalAlloc.getPtr(), SmallVector<Value>{}, origOffsets,
          origSizes);
      origAcquires[i] = origAcquire;
    }
  }

  /// Implement aggregation logic for each reduction variable
  for (size_t idx = 0; idx < activeReductions.size(); ++idx) {
    auto &info = activeReductions[idx];
    Value partialPtr = partialAcquires[idx].getPtr();
    Value origPtr =
        origAcquires[idx] ? origAcquires[idx].getPtr() : info.originalAcc;
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
        computeReductionAccess(info, partialPtr, iv, zeroIndex, oneIndex);
    Value partialVal =
        AC->create<memref::LoadOp>(loc, access.buffer, access.indices);
    Value combined = combineValues(info.scalarType, running, partialVal);
    AC->create<scf::YieldOp>(loc, ValueRange{combined});

    /// Store final result to original accumulator
    AC->setInsertionPointAfter(sumLoop);
    Value finalVal = sumLoop.getResult(0);
    ReductionAccess origAccess =
        computeReductionAccess(info, origPtr, zeroIndex, zeroIndex, oneIndex);
    AC->create<memref::StoreOp>(loc, finalVal, origAccess.buffer,
                                origAccess.indices);
  }

  /// Release all acquires
  for (auto acq : partialAcquires) {
    AC->create<DbReleaseOp>(loc, acq.getPtr());
  }
  for (auto acq : origAcquires) {
    if (acq)
      AC->create<DbReleaseOp>(loc, acq.getPtr());
  }

  return success();
}
#endif // OLD CODE

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
