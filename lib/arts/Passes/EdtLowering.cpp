///==========================================================================
/// File: EdtLowering.cpp
/// Complete implementation of EDT lowering pass that transforms arts.edt
/// operations into runtime-compatible function calls.
///
/// This pass implements a 6-step EDT lowering process:
/// 1. Analyze EDT region for free variables and deps
/// 2. Outline EDT region to function with ARTS runtime signature
/// 3. Insert parameter packing before EDT (edt_param_pack) - It should include
///    all parameters from the EDT + unique datablock sizes and indices for all
///    deps
/// 4. Insert parameter/dependency unpacking in outlined function
/// 5. Replace EDT with edt_create call returning GUID
// 6. Add dependency management (record_in_dep, increment_out_latch)
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
#include "mlir/Dialect/SCF/IR/SCF.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/Matchers.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Transforms/RegionUtils.h"

#include "llvm/ADT/DenseMap.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "arts/Utils/ArtsDebug.h"
#include "polygeist/Ops.h"
ARTS_DEBUG_SETUP(edt_lowering);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

namespace {

//===----------------------------------------------------------------------===//
// EdtEnvManager
// Manages the environment analysis for EDT regions by collecting parameters,
// constants, and dependencies used in the region.
//===----------------------------------------------------------------------===//
class EdtEnvManager {
public:
  EdtEnvManager(EdtOp edtOp) : edtOp(edtOp) { analyze(); }

  /// Analyze the region and collect parameters/constants
  void analyze() {
    getUsedValuesDefinedAbove(edtOp.getRegion(), capturedValues);

    /// Checks if the value is a constant, if so, ignore it
    auto isConstant = [&](Value val) {
      auto defOp = val.getDefiningOp();
      if (!defOp)
        return false;

      auto constantOp = dyn_cast<arith::ConstantOp>(defOp);
      if (!constantOp)
        return false;
      constants.insert(val);
      return true;
    };

    /// Classify variables into parameters, constants, and captured values
    for (Value val : capturedValues) {
      if (isConstant(val))
        continue;

      /// Only treat integers, indices, or floats as parameters
      if (val.getType().isIntOrIndexOrFloat())
        parameters.insert(val);

      /// Ignore other types, they might be dependencies
    }

    /// Only treat explicit EDT operands as deps
    for (Value operand : edtOp.getDependencies())
      deps.insert(operand);
  }

  ArrayRef<Value> getParameters() const { return parameters.getArrayRef(); }
  ArrayRef<Value> getConstants() const { return constants.getArrayRef(); }
  ArrayRef<Value> getCapturedValues() const {
    return capturedValues.getArrayRef();
  }
  ArrayRef<Value> getDependencies() const { return deps.getArrayRef(); }
  const DenseMap<Value, unsigned> &getValueToPackIndex() const {
    return valueToPackIndex;
  }
  DenseMap<Value, unsigned> &getValueToPackIndex() { return valueToPackIndex; }

private:
  EdtOp edtOp;
  SetVector<Value> capturedValues, parameters, constants, deps;
  DenseMap<Value, unsigned> valueToPackIndex;
};

//===----------------------------------------------------------------------===//
// ParallelEdtLowering - Lowers parallel EDTs into explicit task EDTs.
//
// This class implements OpenMP static scheduling semantics for ARTS.
// When an arts.edt<parallel> contains an arts.for, it transforms it into:
// - For static without chunk_size: Block distribution of contiguous iteration
// ranges
// - For static with chunk_size: Round-robin (cyclic) distribution of chunks
// - Inter-node: Round-robin across nodes via route for cyclic, direct route for
// block
// - Intra-node: All tasks with route=0 for free worker assignment
// - Inner arts.edt<task> operations that execute chunks or blocks of work
//
// The transformation ensures that work is distributed efficiently while
// maintaining the correct execution order and data dependencies.
//===----------------------------------------------------------------------===//
class ParallelEdtLowering {
public:
  ArtsCodegen *AC;
  Location loc;

  struct ChunkedForInfo {
    Value lowerBound;
    Value upperBound;
    Value loopStep;
    Value chunkSize;
    Value totalWorkers;
    Value totalIterations;
    Value totalChunks;
    Value workerFirstChunk;
    Value workerChunkCount;
    Value workerIterationCount;
  };

  struct LoopInfo {
    Value lowerBound;
    Value upperBound;
    Value loopStep;
    Value chunkSize;
    Value totalWorkers;
    Value totalIterations;
    Value totalChunks;
  };

  struct ChunkedDepsResult {
    SmallVector<Value> deps;
    SmallVector<DbAcquireOp, 4> chunkedAcquires;
    SmallVector<Value, 4> chunkSizes;
  };

  ParallelEdtLowering(ArtsCodegen *AC, Location loc) : AC(AC), loc(loc) {}

  LogicalResult lowerParallelEdt(EdtOp op) {
    if (!op.getRegion().hasOneBlock()) {
      ARTS_ERROR("Parallel EDT must have exactly one block");
      assert(false);
      return failure();
    }

    EpochOp epochOp = createEpochWrapper();
    Block &epochBody = epochOp.getRegion().front();
    AC->getBuilder().setInsertionPointToEnd(&epochBody);

    ForOp containedForOp = findContainedForOp(op);
    if (containedForOp) {
      ARTS_INFO("Lowering parallel EDT with contained arts.for");
      auto [lb, ub, isInterNode] = determineParallelismStrategy(op);
      if (containedForOp->hasAttr("chunk_size"))
        lowerStaticCyclic(op, containedForOp, lb, ub, isInterNode);
      else
        lowerStaticBlock(op, containedForOp, lb, ub, isInterNode);
    } else {
      ARTS_INFO("Lowering simple parallel EDT");
      lowerSimpleParallelEdt(op);
    }

    AC->getBuilder().setInsertionPointToEnd(&epochBody);
    AC->create<YieldOp>(loc);

    SmallVector<Operation *> toErase;
    if (containedForOp) {
      for (Value dep : op.getDependencies()) {
        if (auto acq = dep.getDefiningOp<DbAcquireOp>()) {
          toErase.push_back(acq);
        }
      }
    }
    op.erase();
    for (auto *e : toErase)
      e->erase();
    return success();
  }

private:
  EpochOp createEpochWrapper() {
    auto epochOp = AC->create<EpochOp>(loc);
    auto &region = epochOp.getRegion();
    if (region.empty())
      region.push_back(new Block());
    return epochOp;
  }

  /// BLOCK DISTRIBUTION EXAMPLE with 4 workers (threads=4):
  ///
  /// INPUT IR (no chunk_size):
  ///   arts.edt<parallel> {
  ///     arts.for %i = 0 to 100 step 1 {
  ///       /// loop body computing 100 iterations
  ///     }
  ///   }
  ///
  /// OUTPUT IR:
  ///   Total iterations: 100, Workers: 4
  ///   Worker 0: iterations 0-24   (25 iterations)
  ///   Worker 1: iterations 25-49  (25 iterations)
  ///   Worker 2: iterations 50-74  (25 iterations)
  ///   Worker 3: iterations 75-99  (25 iterations)
  ///
  ///   scf.for %worker = 0 to 4 step 1 {
  ///     arts.edt<task route=0> deps[...] {
  ///       scf.for %local_i = 0 to worker_chunk_count step 1 {
  ///         %global_i = worker_first_chunk * 1 + %local_i
  ///         /// cloned loop body with %global_i
  ///       }
  ///     }
  ///   }
  ///
  /// For uneven division (101 iterations):
  /// Worker 0: 26 iterations, Worker 1: 26 iterations
  /// Worker 2: 25 iterations, Worker 3: 24 iterations
  void lowerStaticBlock(EdtOp op, ForOp containedForOp, Value lb, Value ub,
                        bool isInterNode) {

    ARTS_INFO(
        "Lowering parallel EDT with arts.for using block distribution; bounds: "
        << lb << ".." << ub);

    Value one = AC->createIndexConstant(1, loc);
    /// Create loop: for(workerId = 0; workerId < totalWorkers; workerId++)
    scf::ForOp workerLoop = AC->create<scf::ForOp>(loc, lb, ub, one);

    OpBuilder::InsertionGuard IG(AC->getBuilder());
    AC->setInsertionPointToStart(workerLoop.getBody());

    DenseMap<Value, Value> depArgMap = makeDependencyMap(op);
    /// workerId ∈ [0, totalWorkers)
    Value workerId = workerLoop.getInductionVar();
    /// route = isInterNode ? (int32)workerId : 0
    Value route = createRouteValue(workerId, isInterNode);

    DenseMap<Value, Value> loopValueCache;
    /// Compute how many iterations this worker should execute
    ChunkedForInfo chunkInfo = computeChunkBounds(containedForOp, workerId, ub,
                                                  loopValueCache, depArgMap);
    ARTS_INFO("Worker chunk info computed");

    /// Create deps, potentially chunking datablocks for this worker's portion
    auto chunkResult =
        createBlockDeps(op, containedForOp, workerId, ub, depArgMap, chunkInfo);
    ARTS_INFO("EDT deps: " << chunkResult.deps.size());

    /// Create edt with chunked dependencies and intranode concurrency
    auto newEdt = AC->create<EdtOp>(
        loc, EdtType::task, EdtConcurrency::intranode, route, chunkResult.deps);
    ARTS_INFO("Created task EDT with route " << route);
    setupEdtRegion(newEdt, op);

    AC->getBuilder().setInsertionPointToEnd(&newEdt.getRegion().front());

    IRMapping mapper;
    auto srcArgs = op.getRegion().front().getArguments();
    auto dstArgs = newEdt.getRegion().front().getArguments();
    for (auto [srcArg, dstArg] : zip(srcArgs, dstArgs))
      mapper.map(srcArg, dstArg);

    /// Create the inner loop that executes this worker's assigned iterations
    Block &loopBody = containedForOp.getRegion().front();
    /// Original %i from arts.for loop
    Value loopInductionVar = loopBody.getArgument(0);

    Value zeroIndex = AC->createIndexConstant(0, loc);
    Value oneIndex = AC->createIndexConstant(1, loc);

    /// Create: scf.for %localIndex = 0 to workerIterationCount step 1
    /// Example: Worker 0 gets 25 iterations, so %localIndex ∈ [0, 24]
    auto assignedLoop = AC->getBuilder().create<scf::ForOp>(
        loc, zeroIndex, chunkInfo.workerIterationCount, oneIndex);

    Block &assignedBody = assignedLoop.getRegion().front();
    Value localIndex = assignedLoop.getInductionVar();
    AC->getBuilder().setInsertionPointToStart(&assignedBody);

    /// Convert local index to global iteration number:
    /// global_iter = workerFirstChunk + localIndex
    /// Example: Worker 0, chunk_size=1: global_iter = 0 + localIndex ∈ [0,24]
    /// Example: Worker 1, chunk_size=1: global_iter = 25 + localIndex ∈ [25,49]
    Value baseIter = AC->create<arith::MulIOp>(loc, chunkInfo.workerFirstChunk,
                                               chunkInfo.chunkSize);
    Value iterationNumber =
        AC->create<arith::AddIOp>(loc, baseIter, localIndex);

    /// Convert iteration number to actual loop index:
    /// actual_index = lowerBound + iterationNumber * step
    /// Example: for i=0 to 100 step 1: actual_index = 0 + iterationNumber * 1
    IRMapping iterationMapper = mapper;
    Value actualLoopIndex = AC->create<arith::AddIOp>(
        loc, chunkInfo.lowerBound,
        AC->create<arith::MulIOp>(loc, iterationNumber, chunkInfo.loopStep));
    iterationMapper.map(loopInductionVar, actualLoopIndex);

    for (Operation &inner : loopBody.without_terminator()) {
      Operation *cloned = AC->getBuilder().clone(inner, iterationMapper);
      for (auto [oldRes, newRes] :
           llvm::zip(inner.getResults(), cloned->getResults()))
        iterationMapper.map(oldRes, newRes);
    }

    AC->getBuilder().setInsertionPointToEnd(&newEdt.getRegion().front());
    for (auto acq : chunkResult.chunkedAcquires)
      AC->create<DbReleaseOp>(loc, acq.getPtr());

    AC->getBuilder().setInsertionPointToEnd(&newEdt.getRegion().front());
    if (!isa<YieldOp>(newEdt.getRegion().front().back()))
      AC->create<YieldOp>(loc);
  }

  ChunkedDepsResult createBlockDeps(EdtOp op, ForOp containedForOp,
                                    Value workerId, Value totalWorkers,
                                    DenseMap<Value, Value> &depArgMap,
                                    ChunkedForInfo &chunkInfo) {

    DenseMap<Value, Value> valueCache;
    ChunkedDepsResult result;
    for (Value dep : op.getDependencies()) {
      if (auto acq = dep.getDefiningOp<DbAcquireOp>()) {
        Value sourceGuid =
            materializeValue(acq.getSourceGuid(), valueCache, &depArgMap);
        Value sourcePtr =
            materializeValue(acq.getSourcePtr(), valueCache, &depArgMap);

        /// Example: Worker 0: offset=0*1=0, size=min(25,100)=25 (elements 0-24)
        /// Worker 1: offset=25*1=25, size=min(25,75)=25 (elements 25-49)
        /// Worker 3: offset=75*1=75, size=min(25,25)=25 (elements 75-99)

        /// Element index where this worker starts
        /// chunkOffset = workerFirstChunk * chunkSize
        Value chunkOffset = AC->create<arith::MulIOp>(
            loc, chunkInfo.workerFirstChunk, chunkInfo.chunkSize);

        /// Total elements this worker should get
        /// chunkSizeElems = workerChunkCount * chunkSize
        Value chunkSizeElems = AC->create<arith::MulIOp>(
            loc, chunkInfo.workerChunkCount, chunkInfo.chunkSize);

        Value zeroIndex = AC->createIndexConstant(0, loc);
        Value totalI = chunkInfo.totalIterations;
        /// Need to clamp if worker starts past array end
        /// needClamp = (totalIterations < chunkOffset) ? true
        Value needClamp = AC->create<arith::CmpIOp>(
            loc, arith::CmpIPredicate::ult, totalI, chunkOffset);

        /// Elements available from start position
        /// diff = totalIterations - chunkOffset
        Value diff = AC->create<arith::SubIOp>(loc, totalI, chunkOffset);

        /// Clamp to zero if starting past array end
        /// remaining = needClamp ? 0 : diff
        Value remaining =
            AC->create<arith::SelectOp>(loc, needClamp, zeroIndex, diff);

        chunkSizeElems =
            AC->create<arith::MinUIOp>(loc, chunkSizeElems, remaining);
        /// Don't exceed available elements
        /// chunkSizeElems = min(chunkSizeElems, remaining)

        /// Create new DbAcquireOp with the chunked offsets and sizes
        SmallVector<Value> offsets{chunkOffset};
        SmallVector<Value> sizes{chunkSizeElems};
        auto newAcquire =
            AC->create<DbAcquireOp>(loc, acq.getMode(), sourceGuid, sourcePtr,
                                    SmallVector<Value, 0>{}, offsets, sizes);
        result.deps.push_back(newAcquire.getPtr());
        result.chunkedAcquires.push_back(newAcquire);
        continue;
      }

      result.deps.push_back(materializeValue(dep, valueCache, &depArgMap));
    }

    return result;
  }

  ChunkedForInfo computeChunkBounds(ForOp containedForOp, Value workerId,
                                    Value totalWorkers,
                                    DenseMap<Value, Value> &valueCache,
                                    DenseMap<Value, Value> &depArgMap) {
    /// BLOCK DISTRIBUTION CALCULATION EXAMPLE with 4 workers, loop 0 to 100:
    /// Total iterations = (100 - 0) / 1 = 100
    /// chunksPerWorker = 100 / 4 = 25
    /// remainderChunks = 100 % 4 = 0
    //
    /// Worker 0: firstChunk = 0*25 + min(0,0) = 0, count = 25+0 = 25
    /// Worker 1: firstChunk = 1*25 + min(1,0) = 25, count = 25+0 = 25
    /// Worker 2: firstChunk = 2*25 + min(2,0) = 50, count = 25+0 = 25
    /// Worker 3: firstChunk = 3*25 + min(3,0) = 75, count = 25+0 = 25
    //
    /// UNEVEN EXAMPLE with 101 iterations:
    /// Total iterations = 101, workers = 4
    /// chunksPerWorker = 101/4 = 25, remainder = 101%4 = 1
    /// Worker 0: firstChunk = 0 + min(0,1) = 0, count = 25+1 = 26
    /// Worker 1: firstChunk = 25 + min(1,1) = 26, count = 25+1 = 26
    /// Worker 2: firstChunk = 50 + min(2,1) = 51, count = 25+0 = 25
    /// Worker 3: firstChunk = 75 + min(3,1) = 76, count = 25+0 = 25

    ChunkedForInfo info;

    info.lowerBound = containedForOp.getLowerBound()[0];
    info.upperBound = containedForOp.getUpperBound()[0];
    info.loopStep = containedForOp.getStep()[0];

    /// Calculate total iterations: (upper - lower) / step
    /// Example: (100 - 0) / 1 = 100 iterations
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

    /// BLOCK DISTRIBUTION: Divide total chunks evenly across workers
    /// Example: 100 iterations, 4 workers
    /// chunksPerEDT = 100/4 = 25
    Value chunksPerEDT =
        AC->create<arith::DivUIOp>(loc, info.totalChunks, totalWorkers);
    /// remainderChunks = 100%4 = 0
    Value remainderChunks =
        AC->create<arith::RemUIOp>(loc, info.totalChunks, totalWorkers);

    /// First few workers get an extra chunk if remainder > 0
    /// Example: remainderChunks=0, so getsExtra = (workerId < 0) = false for
    /// all workers
    /// getsExtra = workerId < remainderChunks
    Value getsExtra = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ult,
                                                workerId, remainderChunks);
    /// extraChunk = getsExtra ? 1 : 0
    Value extraChunk = AC->castToIndex(getsExtra, loc);
    /// workerChunkCount = 25 + 0 = 25 for all
    info.workerChunkCount =
        AC->create<arith::AddIOp>(loc, chunksPerEDT, extraChunk);

    info.totalWorkers = totalWorkers;

    /// Calculate which chunk this worker starts with:
    /// Formula: workerFirstChunk = workerId * chunksPerWorker + min(workerId,
    /// remainder) Example: Worker 0: 0*25 + min(0,0) = 0, Worker 1: 1*25 +
    /// min(1,0) = 25 With remainder=1: Worker 0: 0*25 + min(0,1) = 0, Worker 1:
    /// 1*25 + min(1,1) = 26

    Value baseStart = AC->create<arith::MulIOp>(loc, workerId, chunksPerEDT);
    /// baseStart = workerId * chunksPerEDT

    Value prefixExtra =
        AC->create<arith::MinUIOp>(loc, workerId, remainderChunks);
    /// prefixExtra = min(workerId, remainderChunks)

    info.workerFirstChunk =
        AC->create<arith::AddIOp>(loc, baseStart, prefixExtra);
    /// workerFirstChunk = baseStart + prefixExtra

    /// Calculate actual iteration count (may be less if we run out of
    /// iterations) Example: Worker 0: rawIterCount=25*1=25, startElems=0*1=0,
    /// remaining=100-0=100 workerIterationCount = min(25, 100) = 25 Worker 3:
    /// rawIterCount=25*1=25, startElems=75*1=75, remaining=100-75=25
    /// workerIterationCount = min(25, 25) = 25

    /// rawIterCount = workerChunkCount * chunkSize
    Value rawIterCount =
        AC->create<arith::MulIOp>(loc, info.workerChunkCount, info.chunkSize);

    /// startElems = workerFirstChunk * chunkSize  (first element this worker
    /// handles)
    Value startElems =
        AC->create<arith::MulIOp>(loc, info.workerFirstChunk, info.chunkSize);

    /// needZero = (totalIterations < startElems) ? true if worker starts past
    /// end
    Value zeroIndex = AC->createIndexConstant(0, loc);
    Value needZero = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::ult, info.totalIterations, startElems);

    /// remaining = totalIterations - startElems  (iterations available to this
    /// worker)
    Value remaining =
        AC->create<arith::SubIOp>(loc, info.totalIterations, startElems);

    /// remainingNonNeg = needZero ? 0 : remaining  (clamp negative to zero)
    Value remainingNonNeg =
        AC->create<arith::SelectOp>(loc, needZero, zeroIndex, remaining);

    /// workerIterationCount = min(rawIterCount, remainingNonNeg)
    info.workerIterationCount =
        AC->create<arith::MinUIOp>(loc, rawIterCount, remainingNonNeg);

    return info;
  }

  /// CYCLIC DISTRIBUTION EXAMPLE with 4 workers (threads=4):
  ///
  /// INPUT IR (with chunk_size=5):
  ///   arts.edt<parallel> {
  ///     arts.for %i = 0 to 100 step 1 chunk_size=5 {
  ///       /// loop body
  ///     }
  ///   }
  ///
  /// OUTPUT IR:
  ///   Total iterations: 100, Chunk size: 5, Total chunks: 20
  ///   Worker 0: chunks 0,4,8,12,16  (iterations 0-4,20-24,40-44,60-64,80-84)
  ///   Worker 1: chunks 1,5,9,13,17  (iterations 5-9,25-29,45-49,65-69,85-89)
  ///   Worker 2: chunks 2,6,10,14,18 (iterations 10-14,30-34,50-54,70-74,90-94)
  ///   Worker 3: chunks 3,7,11,15,19 (iterations 15-19,35-39,55-59,75-79,95-99)
  ///
  ///   scf.for %chunk = 0 to 20 step 1 {
  ///     %worker = %chunk % 4  /// cyclic assignment
  ///     arts.edt<task route=0> deps[...] {
  ///       scf.for %local_i = 0 to 5 step 1 {
  ///         %global_i = %chunk * 5 + %local_i
  ///         /// cloned loop body
  ///       }
  ///     }
  ///   }
  void lowerStaticCyclic(EdtOp op, ForOp containedForOp, Value lb, Value ub,
                         bool isInterNode) {

    ARTS_INFO("Lowering parallel EDT with arts.for using cyclic distribution; "
              "bounds: "
              << lb << ".." << ub);

    LoopInfo info;
    info.lowerBound = containedForOp.getLowerBound()[0];
    info.upperBound = containedForOp.getUpperBound()[0];
    info.loopStep = containedForOp.getStep()[0];
    info.totalWorkers = ub; /// Total number of workers (e.g., 4)

    computeTotalIterationsAndChunkSize(containedForOp, info);
    /// info.totalIterations, info.chunkSize, info.totalChunks now computed

    Value zero = AC->createIndexConstant(0, loc);
    Value one = AC->createIndexConstant(1, loc);
    /// Create loop over all chunks: for(chunkId = 0; chunkId < totalChunks;
    /// chunkId++)
    scf::ForOp chunkLoop =
        AC->create<scf::ForOp>(loc, zero, info.totalChunks, one);

    OpBuilder::InsertionGuard IG(AC->getBuilder());
    AC->setInsertionPointToStart(chunkLoop.getBody());

    DenseMap<Value, Value> depArgMap = makeDependencyMap(op);
    /// chunkId ∈ [0, totalChunks)
    Value chunkId = chunkLoop.getInductionVar();

    /// CYCLIC ASSIGNMENT: worker = chunkId % totalWorkers
    /// Example: 20 chunks, 4 workers, chunk_size=5
    /// Chunk  0: worker = 0%4 = 0, iterations 0-4
    /// Chunk  1: worker = 1%4 = 1, iterations 5-9
    /// Chunk  2: worker = 2%4 = 2, iterations 10-14
    /// Chunk  3: worker = 3%4 = 3, iterations 15-19
    /// Chunk  4: worker = 4%4 = 0, iterations 20-24
    /// Chunk  5: worker = 5%4 = 1, iterations 25-29
    /// ...
    /// Chunk 19: worker = 19%4 = 3, iterations 95-99
    //
    /// Result: Each worker gets 5 chunks of 5 iterations = 25 iterations total

    Value route;
    if (isInterNode) {
      /// Inter-node: route = chunkId % totalWorkers (distribute across nodes)
      /// Example: chunkId=0, totalWorkers=4 -> route = 0%4 = 0
      /// Example: chunkId=5, totalWorkers=4 -> route = 5%4 = 1
      /// nodeId = chunkId % totalWorkers
      Value nodeId =
          AC->create<arith::RemUIOp>(loc, chunkId, info.totalWorkers);
      route = AC->castToInt(AC->Int32, nodeId, loc);
    } else {
      /// Intra-node: route = 0 (all tasks run on local node)
      route = AC->createIntConstant(0, AC->Int32, loc);
    }

    ChunkedDepsResult chunkResult =
        createCyclicDeps(op, containedForOp, chunkId, info.chunkSize,
                         info.totalIterations, depArgMap);
    ARTS_INFO("EDT deps: " << chunkResult.deps.size());

    auto newEdt = AC->create<EdtOp>(
        loc, EdtType::task, EdtConcurrency::intranode, route, chunkResult.deps);
    ARTS_INFO("Created task EDT with route " << route);
    setupEdtRegion(newEdt, op);

    AC->getBuilder().setInsertionPointToEnd(&newEdt.getRegion().front());

    IRMapping mapper;
    auto srcArgs = op.getRegion().front().getArguments();
    auto dstArgs = newEdt.getRegion().front().getArguments();
    for (auto [srcArg, dstArg] : zip(srcArgs, dstArgs))
      mapper.map(srcArg, dstArg);

    /// Calculate which iterations this chunk should execute
    Block &loopBody = containedForOp.getRegion().front();
    Value loopInductionVar = loopBody.getArgument(0);

    /// Calculate which iterations this chunk should execute
    /// chunkStartIter = chunkId * chunkSize
    /// Example: chunkId=0, chunkSize=5 -> chunkStartIter = 0*5 = 0 (iterations
    /// 0,1,2,3,4) Example: chunkId=4, chunkSize=5 -> chunkStartIter = 4*5 = 20
    /// (iterations 20,21,22,23,24) Example: chunkId=19, chunkSize=5 ->
    /// chunkStartIter = 19*5 = 95 (iterations 95,96,97,98,99)
    Value chunkStartIter =
        AC->create<arith::MulIOp>(loc, chunkId, info.chunkSize);
    /// chunkStartIter = chunkId * chunkSize

    /// Handle last chunk: may have fewer than chunkSize iterations
    /// Example: totalIterations=100, chunkId=19, chunkSize=5
    /// remaining = 100 - 95 = 5, so actualChunkSize = min(5, 5) = 5 ✓
    /// Example: totalIterations=102, chunkId=20, chunkSize=5 (hypothetical)
    /// remaining = 102 - 100 = 2, so actualChunkSize = min(5, 2) = 2 ✓
    Value remaining =
        AC->create<arith::SubIOp>(loc, info.totalIterations, chunkStartIter);
    /// remaining = totalIterations - chunkStartIter

    Value actualChunkSize =
        AC->create<arith::MinUIOp>(loc, info.chunkSize, remaining);
    /// actualChunkSize = min(chunkSize, remaining)

    /// Create inner loop: scf.for %localIndex = 0 to actualChunkSize step 1
    /// Example: chunkSize=5, not last chunk -> %localIndex ∈ [0,4]
    /// Example: last chunk with only 3 remaining -> %localIndex ∈ [0,2]
    auto assignedLoop =
        AC->getBuilder().create<scf::ForOp>(loc, zero, actualChunkSize, one);

    Block &assignedBody = assignedLoop.getRegion().front();
    AC->getBuilder().setInsertionPointToStart(&assignedBody);

    Value localIndex = assignedLoop.getInductionVar();

    /// iterationNumber = chunkStartIter + localIndex
    /// Example: chunkId=0, localIndex=0 -> 0+0=0, chunkId=4, localIndex=4 ->
    /// 20+4=24
    /// iterationNumber = chunkStartIter + localIndex
    Value iterationNumber =
        AC->create<arith::AddIOp>(loc, chunkStartIter, localIndex);

    /// Convert to actual loop index: actual_index = lowerBound +
    /// iterationNumber * step Example: for i=0 to 100 step 1: actual_index = 0
    /// + iterationNumber * 1
    IRMapping iterationMapper = mapper;
    /// actualLoopIndex = lowerBound + iterationNumber * step
    Value actualLoopIndex = AC->create<arith::AddIOp>(
        loc, info.lowerBound,
        AC->create<arith::MulIOp>(loc, iterationNumber, info.loopStep));

    /// Map the original loop induction variable to our computed actual index
    iterationMapper.map(loopInductionVar, actualLoopIndex);

    for (Operation &inner : loopBody.without_terminator()) {
      Operation *cloned = AC->getBuilder().clone(inner, iterationMapper);
      for (auto [oldRes, newRes] :
           llvm::zip(inner.getResults(), cloned->getResults()))
        iterationMapper.map(oldRes, newRes);
    }

    AC->getBuilder().setInsertionPointToEnd(&newEdt.getRegion().front());
    for (auto acq : chunkResult.chunkedAcquires)
      AC->create<DbReleaseOp>(loc, acq.getPtr());

    AC->getBuilder().setInsertionPointToEnd(&newEdt.getRegion().front());
    if (!isa<YieldOp>(newEdt.getRegion().front().back()))
      AC->create<YieldOp>(loc);
  }

  /// SIMPLE PARALLEL EDT EXAMPLE with 4 workers (threads=4):
  ///
  /// INPUT IR (no loop, just computation):
  ///   arts.edt<parallel> {
  ///     /// some computation work
  ///   }
  ///
  /// OUTPUT IR:
  ///   scf.for %worker = 0 to 4 step 1 {
  ///     arts.edt<task route=0> deps[...] {
  ///       /// cloned computation work
  ///     }
  ///   }
  ///
  /// This creates identical task EDTs for embarrassingly parallel workloads
  /// where each worker performs the same computation independently (e.g.,
  /// Monte Carlo simulations, parameter sweeps, etc.)
  void lowerSimpleParallelEdt(EdtOp op) {

    auto [lb, ub, isInterNode] = determineParallelismStrategy(op);
    ARTS_INFO("Lowering simple parallel EDT; bounds: " << lb << ".." << ub);

    Value one = AC->createIndexConstant(1, loc);
    /// Create worker loop:
    /// for(workerId = 0; workerId < totalWorkers; workerId++)
    scf::ForOp workerLoop = AC->create<scf::ForOp>(loc, lb, ub, one);
    OpBuilder::InsertionGuard guard(AC->getBuilder());
    AC->setInsertionPointToStart(workerLoop.getBody());

    DenseMap<Value, Value> depArgMap = makeDependencyMap(op);
    /// Create dependencies
    SmallVector<Value> deps = createSimpleDeps(op, depArgMap);
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

  std::tuple<Value, Value, bool> determineParallelismStrategy(EdtOp edtOp) {
    Value lb = AC->createIndexConstant(0, loc);
    Value ub = nullptr;

    /// Read parallelism strategy from EDT attributes set by concurrency pass
    if (auto workersAttr = edtOp.getWorkers()) {
      int numWorkers = workersAttr->getValue();
      ub = AC->createIndexConstant(numWorkers, loc);

      bool isInterNode = (edtOp.getConcurrency() == EdtConcurrency::internode);
      ARTS_INFO("Using EDT attributes: workers="
                << numWorkers << ", concurrency="
                << (isInterNode ? "internode" : "intranode"));
      return {lb, ub, isInterNode};
    }

    /// Fallback: no attributes set, use runtime workers
    ARTS_WARN("No parallelism attributes set on EDT, using runtime fallback");
    ub = AC->castToIndex(AC->getTotalWorkers(loc), loc);
    return {lb, ub, /*isInterNode=*/false};
  }

  ForOp findContainedForOp(EdtOp op) {
    ForOp containedForOp = nullptr;
    op.walk([&](ForOp forOp) {
      if (!containedForOp)
        containedForOp = forOp;
      return WalkResult::advance();
    });
    return containedForOp;
  }

  ChunkedDepsResult createCyclicDeps(EdtOp op, ForOp containedForOp,
                                     Value chunkId, Value chunkSize,
                                     Value totalIterations,
                                     DenseMap<Value, Value> &depArgMap) {

    DenseMap<Value, Value> valueCache;
    ChunkedDepsResult result;
    /// Example: Chunk 0: offset=0*5=0, size=min(5,100)=5 (elements 0-4)
    /// Chunk 1:  offset=1*5=5,   size=min(5,95)=5 (elements 5-9)
    /// Chunk 19: offset=19*5=95, size=min(5,5)=5  (elements 95-99)

    /// Element index where this chunk starts
    /// chunkOffset = chunkId * chunkSize
    Value chunkOffset = AC->create<arith::MulIOp>(loc, chunkId, chunkSize);

    /// Elements available from start position
    /// remaining = totalIterations - chunkOffset
    Value remaining =
        AC->create<arith::SubIOp>(loc, totalIterations, chunkOffset);

    /// Handle last chunk being smaller
    /// chunkSizeElems = min(chunkSize, remaining)
    Value chunkSizeElems =
        AC->create<arith::MinUIOp>(loc, chunkSize, remaining);

    for (Value dep : op.getDependencies()) {
      if (auto acq = dep.getDefiningOp<DbAcquireOp>()) {
        Value sourceGuid =
            materializeValue(acq.getSourceGuid(), valueCache, &depArgMap);
        Value sourcePtr =
            materializeValue(acq.getSourcePtr(), valueCache, &depArgMap);

        /// Create new DbAcquireOp with the chunked offsets and sizes
        SmallVector<Value> offsets{chunkOffset};
        SmallVector<Value> sizes{chunkSizeElems};
        auto newAcquire =
            AC->create<DbAcquireOp>(loc, acq.getMode(), sourceGuid, sourcePtr,
                                    SmallVector<Value, 0>{}, offsets, sizes);
        result.deps.push_back(newAcquire.getPtr());
        result.chunkedAcquires.push_back(newAcquire);
        continue;
      }

      result.deps.push_back(materializeValue(dep, valueCache, &depArgMap));
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
    auto chunkAttr = containedForOp->getAttrOfType<IntegerAttr>("chunk_size");
    int64_t chunkInt = std::max<int64_t>(1, chunkAttr.getInt());
    info.chunkSize = AC->createIndexConstant(chunkInt, loc);

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
    ///
    /// - Example: 102 iterations, chunkSize=5 ->
    ///   adjustedIterations = 102 + 5 - 1 = 106
    ///   totalChunks = 106 / 5 = 21
    ///   (still 21, since 5*21 = 105 >= 102)

    /// adjustedIterations = totalIterations + chunkSize - 1
    Value adjustedIterations = AC->create<arith::AddIOp>(
        loc, info.totalIterations,
        AC->create<arith::SubIOp>(loc, info.chunkSize, oneIndex));

    /// Integer division gives ceiling
    /// totalChunks = adjustedIterations / chunkSize)
    info.totalChunks =
        AC->create<arith::DivUIOp>(loc, adjustedIterations, info.chunkSize);
  }

  SmallVector<Value> createSimpleDeps(EdtOp op,
                                      DenseMap<Value, Value> &depArgMap) {
    SmallVector<Value> deps;
    DenseMap<Value, Value> valueCache;
    for (Value dep : op.getDependencies())
      deps.push_back(materializeValue(dep, valueCache, &depArgMap));
    return deps;
  }

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
      if (releaseValue)
        bodyBuilder.create<DbReleaseOp>(newEdt.getLoc(), releaseValue);
      bodyBuilder.create<YieldOp>(newEdt.getLoc());
    } else if (releaseValue) {
      bodyBuilder.setInsertionPoint(&dstBody.back());
      bodyBuilder.create<DbReleaseOp>(newEdt.getLoc(), releaseValue);
    }
  }

  DenseMap<Value, Value> makeDependencyMap(EdtOp op) {
    DenseMap<Value, Value> mapping;
    ValueRange deps = op.getDependencies();
    Block &srcBlock = op.getRegion().front();
    for (auto [index, arg] : enumerate(srcBlock.getArguments())) {
      if (index < deps.size())
        mapping[arg] = deps[index];
    }
    return mapping;
  }

  Value materializeValue(Value val, DenseMap<Value, Value> &cache,
                         DenseMap<Value, Value> *depArgMap) {
    if (cache.count(val))
      return cache[val];

    if (depArgMap) {
      auto it = depArgMap->find(val);
      if (it != depArgMap->end()) {
        cache[val] = it->second;
        return it->second;
      }
    }

    if (auto constOp = val.getDefiningOp<arith::ConstantOp>()) {
      Value newConst = AC->getBuilder().clone(*constOp)->getResult(0);
      cache[val] = newConst;
      return newConst;
    }

    cache[val] = val;
    return val;
  }

  Value createRouteValue(Value workerId, bool isInterNode) {
    if (isInterNode)
      return AC->castToInt(AC->Int32, workerId, loc);
    else
      return AC->createIntConstant(0, AC->Int32, loc);
  }
};

//===----------------------------------------------------------------------===//
// EDT Lowering Pass Implementation
//===----------------------------------------------------------------------===//
struct EdtLoweringPass : public arts::EdtLoweringBase<EdtLoweringPass> {
  explicit EdtLoweringPass() {}
  ~EdtLoweringPass() { delete AC; }
  void runOnOperation() override;

private:
  /// Core transformation methods
  LogicalResult lowerEdt(EdtOp edtOp);
  LogicalResult lowerParallelEdt(EdtOp edtOp);

  /// Function outlining with ARTS signature
  func::FuncOp createOutlinedFunction(EdtOp edtOp, EdtEnvManager &envManager);

  /// Parameter handling
  Value packParams(Location loc, EdtEnvManager &envManager,
                   SmallVector<Type> &packTypes);

  /// Region outlining
  LogicalResult outlineRegionToFunction(EdtOp edtOp, func::FuncOp targetFunc,
                                        EdtEnvManager &envManager,
                                        const SmallVector<Type> &packTypes,
                                        size_t numUserParams);

  void transformDepUses(Block *block, ArrayRef<Value> originalDeps, Value depv,
                        ArrayRef<Value> allParams, EdtEnvManager &envManager,
                        ArrayRef<Value> depIdentifiers);

  /// Dependency satisfaction
  LogicalResult insertDepManagement(Location loc, Value edtGuid,
                                    const SmallVector<Value> &deps);

  /// Attributes
  unsigned functionCounter = 0;
  ModuleOp module;
  ArtsCodegen *AC = nullptr;
};

} // namespace

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//

void EdtLoweringPass::runOnOperation() {
  module = getOperation();
  AC = new ArtsCodegen(module, false);

  ARTS_INFO_HEADER(EdtLoweringPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Lower all parallel EDTs
  {
    ARTS_DEBUG_HEADER(ParallelEdtLowering);
    SmallVector<EdtOp, 8> parallelEdts;
    module.walk<WalkOrder::PostOrder>([&](EdtOp edt) {
      if (edt.getType() == EdtType::parallel)
        parallelEdts.push_back(edt);
    });
    ARTS_INFO("Found " << parallelEdts.size() << " parallel EDTs to lower");
    for (EdtOp edt : parallelEdts) {
      if (failed(lowerParallelEdt(edt))) {
        edt.emitError("Failed to lower parallel EDT");
        return signalPassFailure();
      }
    }
    if (parallelEdts.size() > 0) {
      simplifyIR(module, getAnalysis<DominanceInfo>());
      ARTS_DEBUG_REGION(module.dump(););
    }

    ARTS_DEBUG_FOOTER(ParallelEdtLowering);
  }

  /// Now collect and lower all regular EDTs
  {
    ARTS_DEBUG_HEADER(TaskEdtLowering);
    SmallVector<EdtOp, 8> taskEdts;
    module.walk<WalkOrder::PostOrder>([&](EdtOp edtOp) {
      assert(edtOp.getType() == EdtType::task && "Expected task EDT");
      taskEdts.push_back(edtOp);
    });
    ARTS_INFO("Found " << taskEdts.size() << " task EDTs to lower");
    for (EdtOp edtOp : taskEdts) {
      if (failed(lowerEdt(edtOp))) {
        edtOp.emitError("Failed to lower task EDT");
        return signalPassFailure();
      }

    }
  }

  ARTS_INFO_FOOTER(EdtLoweringPass);
  ARTS_DEBUG_REGION(module.dump(););
}

LogicalResult EdtLoweringPass::lowerEdt(EdtOp edtOp) {
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(edtOp);
  Location loc = edtOp.getLoc();
  /// Analyze EDT environment (parameters, constants, deps)
  EdtEnvManager envManager(edtOp);

  /// Create local variables for parameter packing/dedup
  DenseMap<Value, unsigned> valueToPackIndex;
  SmallVector<Type> packTypes;

  /// Create edt outlined function
  func::FuncOp outlinedFunc = createOutlinedFunction(edtOp, envManager);
  if (!outlinedFunc)
    return edtOp.emitError("Failed to create outlined function");

  /// Pack parameters
  Value paramPack = packParams(loc, envManager, packTypes);

  /// Outline region to function and replace EDT
  if (failed(outlineRegionToFunction(edtOp, outlinedFunc, envManager, packTypes,
                                     envManager.getParameters().size()))) {
    return edtOp.emitError("Failed to outline region to function");
  }

  /// Calculate total dependency count (sum of elements in all deps)
  Value depCount = AC->createIntConstant(0, AC->Int32, loc);
  for (Value dep : envManager.getDependencies()) {
    SmallVector<Value> sizes = getSizesFromDb(dep);
    Value numElements = AC->create<DbNumElementsOp>(loc, sizes);
    numElements = AC->castToInt(AC->Int32, numElements, loc);
    depCount = AC->create<arith::AddIOp>(loc, depCount, numElements);
  }

  /// Create the outline operation at the same location as the EDT
  AC->setInsertionPoint(edtOp);
  Value routeVal = edtOp.getRoute();
  if (!routeVal)
    routeVal = AC->createIntConstant(0, AC->Int32, loc);

  /// Create the outline operation at the same location as the EDT
  auto outlineOp = AC->create<EdtCreateOp>(loc, paramPack, depCount, routeVal);

  outlineOp->setAttr("outlined_func",
                     AC->getBuilder().getStringAttr(outlinedFunc.getName()));

  /// Insert dependency management after the outline op
  Value edtGuid = outlineOp.getGuid();
  AC->setInsertionPointAfter(outlineOp);
  SmallVector<Value> depsVec(envManager.getDependencies().begin(),
                             envManager.getDependencies().end());
  if (failed(insertDepManagement(loc, edtGuid, depsVec)))
    return edtOp.emitError("Failed to insert dependency management");

  /// Replace all uses of EDT with the outlined function result
  if (edtOp->getNumResults() > 0) {
    SmallVector<Value> replacementValues = {outlineOp.getResult()};
    edtOp->replaceAllUsesWith(replacementValues);
  }
  /// Remove original EDT
  edtOp.erase();

  return success();
}

LogicalResult EdtLoweringPass::lowerParallelEdt(EdtOp op) {
  ArtsCodegen AC(module, false);
  AC.setInsertionPoint(op);
  ParallelEdtLowering lowerer(&AC, op.getLoc());
  return lowerer.lowerParallelEdt(op);
}

func::FuncOp
EdtLoweringPass::createOutlinedFunction(EdtOp edtOp,
                                        EdtEnvManager &envManager) {
  Location loc = edtOp.getLoc();
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(module);
  std::string funcName = "__arts_edt_" + std::to_string(++functionCounter);
  auto outlinedFunc = AC->create<func::FuncOp>(loc, funcName, AC->EdtFn);
  outlinedFunc.setPrivate();

  ARTS_INFO("Created outlined function: " << funcName);
  return outlinedFunc;
}

Value EdtLoweringPass::packParams(Location loc, EdtEnvManager &envManager,
                                  SmallVector<Type> &packTypes) {
  const auto &parameters = envManager.getParameters();
  const auto &deps = envManager.getDependencies();

  SmallVector<Value> packValues;
  auto &valueToPackIndex = envManager.getValueToPackIndex();

  /// Pack user parameters first
  for (Value v : parameters) {
    /// Skip llvm.mlir.undef values - they can be easily recreated
    if (auto defOp = v.getDefiningOp()) {
      if (defOp->getName().getStringRef() == "llvm.mlir.undef")
        continue;
    }
    valueToPackIndex.try_emplace(v, packValues.size());
    packTypes.push_back(v.getType());
    packValues.push_back(v);
  }

  /// Insert indices/offsets/sizes for deps into packValues if not already
  /// present
  for (Value dep : deps) {
    auto dbAcquireOp = dep.getDefiningOp<DbAcquireOp>();
    if (!dbAcquireOp)
      continue;

    auto appendIfMissing = [&](Value val) {
      if (!val)
        return;
      /// Skip constants; they will be recreated in outlined function
      if (val.getDefiningOp<arith::ConstantOp>())
        return;
      if (valueToPackIndex.count(val) == 0) {
        valueToPackIndex[val] = packValues.size();
        packTypes.push_back(val.getType());
        packValues.push_back(val);
      }
    };

    for (Value idx : dbAcquireOp.getIndices())
      appendIfMissing(idx);
    for (Value off : dbAcquireOp.getOffsets())
      appendIfMissing(off);
    for (Value sz : dbAcquireOp.getSizes())
      appendIfMissing(sz);
  }

  if (packValues.empty()) {
    ARTS_INFO("No parameters to pack, creating empty EdtParamPackOp");
    auto emptyType = MemRefType::get({0}, AC->Int64);
    return AC->create<EdtParamPackOp>(loc, TypeRange{emptyType}, ValueRange{});
  }

  ARTS_DEBUG_REGION({
    ARTS_INFO("Creating parameter pack for " << packValues.size() << " items");
    for (size_t i = 0; i < packValues.size(); ++i) {
      ARTS_INFO("  packValues[" << i << "]: " << packValues[i]
                                << " type: " << packValues[i].getType());
    }
  });

  auto memrefType = MemRefType::get({ShapedType::kDynamic}, AC->Int64);
  ARTS_INFO("About to create EdtParamPackOp with type: " << memrefType);
  auto packOp = AC->create<EdtParamPackOp>(loc, TypeRange{memrefType},
                                           ValueRange(packValues));
  return packOp;
}

LogicalResult EdtLoweringPass::outlineRegionToFunction(
    EdtOp edtOp, func::FuncOp targetFunc, EdtEnvManager &envManager,
    const SmallVector<Type> &packTypes, size_t numUserParams) {
  Location loc = edtOp.getLoc();
  auto &builder = AC->getBuilder();
  OpBuilder::InsertionGuard IG(builder);
  auto *entryBlock = targetFunc.addEntryBlock();
  AC->setInsertionPointToStart(entryBlock);

  /// Insert parameter and dependency unpacking
  auto args = entryBlock->getArguments();
  Value paramv = args[1];
  Value depv = args[3];

  const auto &parameters = envManager.getParameters();
  SmallVector<Value> deps(envManager.getDependencies().begin(),
                          envManager.getDependencies().end());
  ARTS_INFO("analyzeDependencies returned " << deps.size() << " dependencies");
  for (size_t i = 0; i < deps.size(); ++i) {
    ARTS_INFO(
        "  dep[" << i << "] type: " << deps[i].getType() << ", defOp: "
                 << (deps[i].getDefiningOp()
                         ? deps[i].getDefiningOp()->getName().getStringRef()
                         : "none"));
  }
  SmallVector<Value> unpackedParams, allParams;

  if (!packTypes.empty()) {
    auto paramUnpackOp = AC->create<EdtParamUnpackOp>(loc, packTypes, paramv);
    auto results = paramUnpackOp.getResults();
    allParams.assign(results.begin(), results.end());
    unpackedParams.append(allParams.begin(), allParams.begin() + numUserParams);
  }

  /// Store dependency information for direct use
  SmallVector<Value> originalDeps(envManager.getDependencies().begin(),
                                  envManager.getDependencies().end());
  Region &edtRegion = edtOp.getRegion();
  Block &edtBlock = edtRegion.front();

  /// Create compact per-dependency placeholders for later dep_gep rewrite.
  SmallVector<Value> depPlaceholders(originalDeps.size());
  for (auto it : llvm::enumerate(originalDeps))
    depPlaceholders[it.index()] =
        AC->create<UndefOp>(loc, it.value().getType());

  /// Clone constants into function
  IRMapping valueMapping;

  /// Map EDT args to placeholders so cloned ops don't reference outer values.
  for (auto [edtArg, ph] : llvm::zip(edtBlock.getArguments(), depPlaceholders))
    valueMapping.map(edtArg, ph);

  /// Also map the original dependency values to their corresponding
  /// placeholders to catch any direct uses that bypassed the block arguments.
  for (auto [originalDep, placeholder] :
       llvm::zip(originalDeps, depPlaceholders))
    valueMapping.map(originalDep, placeholder);

  for (Value constant : envManager.getConstants())
    if (Operation *defOp = constant.getDefiningOp())
      valueMapping.map(constant, builder.clone(*defOp)->getResult(0));

  /// Map parameters directly to their unpacked counterparts; clone constants
  /// and undef.
  size_t unpackedIndex = 0;
  for (Value param : parameters) {
    /// Handle llvm.mlir.undef by recreating it instead of unpacking
    if (auto defOp = param.getDefiningOp()) {
      if (defOp->getName().getStringRef() == "llvm.mlir.undef") {
        valueMapping.map(param, builder.clone(*defOp)->getResult(0));
        continue;
      }
    }
    /// Map to unpacked parameter
    if (unpackedIndex < unpackedParams.size())
      valueMapping.map(param, unpackedParams[unpackedIndex++]);
  }

  for (Value freeVar : envManager.getCapturedValues())
    if (Operation *defOp = freeVar.getDefiningOp())
      if (defOp->hasTrait<OpTrait::ConstantLike>())
        valueMapping.map(freeVar, builder.clone(*defOp)->getResult(0));

  /// Clone region operations
  builder.setInsertionPointToEnd(entryBlock);
  for (Operation &op :
       llvm::make_early_inc_range(edtBlock.without_terminator())) {
    Operation *clonedOp = builder.clone(op, valueMapping);
    for (auto [orig, clone] :
         llvm::zip(op.getResults(), clonedOp->getResults()))
      valueMapping.map(orig, clone);
  }

  /// Add return terminator
  AC->create<func::ReturnOp>(loc);

  /// Transform dependency uses inside outlined region
  transformDepUses(entryBlock, originalDeps, depv, allParams, envManager,
                   depPlaceholders);
  return success();
}

LogicalResult
EdtLoweringPass::insertDepManagement(Location loc, Value edtGuid,
                                     const SmallVector<Value> &deps) {
  if (deps.empty())
    return success();

  /// Separate deps by mode and extract GUIDs
  SmallVector<Value> inDepGuids, outDepGuids;

  for (Value dep : deps) {
    auto dbAcquireOp = dep.getDefiningOp<DbAcquireOp>();
    assert(dbAcquireOp && "Dependencies must be DbAcquireOp operations");

    /// Get the GUID from the DbAcquireOp
    Value depGuid = dbAcquireOp.getGuid();

    /// Always add to in-dependencies
    inDepGuids.push_back(depGuid);

    /// Only add to out-dependencies if mode is out or inout
    ArtsMode mode = dbAcquireOp.getMode();
    if (mode == ArtsMode::out || mode == ArtsMode::inout)
      outDepGuids.push_back(depGuid);
  }

  /// Record all input deps at once using GUIDs
  if (!inDepGuids.empty())
    AC->create<RecordInDepOp>(loc, edtGuid, inDepGuids);

  /// Increment output latches for all deps at once using GUIDs
  if (!outDepGuids.empty())
    AC->create<IncrementOutLatchOp>(loc, edtGuid, outDepGuids);

  return success();
}

void EdtLoweringPass::transformDepUses(Block *block,
                                       ArrayRef<Value> originalDeps, Value depv,
                                       ArrayRef<Value> allParams,
                                       EdtEnvManager &envManager,
                                       ArrayRef<Value> depIdentifiers) {

  const auto &paramMap = envManager.getValueToPackIndex();
  auto resolveSizes = [&](Value dep, Location loc) {
    SmallVector<Value> sizes = getSizesFromDb(dep);
    SmallVector<Value> resolved;
    for (Value sz : sizes) {
      auto it = paramMap.find(sz);
      if (it != paramMap.end() && it->second < allParams.size())
        resolved.push_back(allParams[it->second]);
      else if (auto c = sz.getDefiningOp<arith::ConstantIndexOp>())
        resolved.push_back(AC->createIndexConstant(c.value(), loc));
      else
        resolved.push_back(sz);
    }
    if (resolved.empty())
      resolved.push_back(AC->createIndexConstant(1, loc));
    return resolved;
  };

  auto resolveOffsets = [&](Value dep, Location loc) {
    SmallVector<Value> offsets = getOffsetsFromDb(dep);
    SmallVector<Value> resolved;
    for (Value off : offsets) {
      auto it = paramMap.find(off);
      if (it != paramMap.end() && it->second < allParams.size())
        resolved.push_back(allParams[it->second]);
      else if (auto c = off.getDefiningOp<arith::ConstantIndexOp>())
        resolved.push_back(AC->createIndexConstant(c.value(), loc));
      else
        resolved.push_back(off);
    }
    return resolved;
  };

  auto computeBaseOffset = [&](size_t depIndex, Location loc) {
    Value base = AC->createIndexConstant(0, loc);
    for (size_t i = 0; i < depIndex; ++i) {
      SmallVector<Value> prevResolved = resolveSizes(originalDeps[i], loc);
      Value prevElems = AC->computeTotalElements(prevResolved, loc);
      base = AC->create<arith::AddIOp>(loc, base, prevElems);
    }
    return base;
  };

  /// For each dependency placeholder, rewrite its direct uses.
  for (size_t depIndex = 0; depIndex < depIdentifiers.size(); ++depIndex) {
    Value placeholder = depIdentifiers[depIndex];
    SmallVector<Operation *, 16> users;
    for (auto &use : placeholder.getUses())
      users.push_back(use.getOwner());

    for (Operation *op : users) {
      if (auto mp = dyn_cast<polygeist::Memref2PointerOp>(op)) {
        if (mp.getSource() != placeholder)
          continue;
        AC->setInsertionPoint(op);
        Location loc = op->getLoc();
        Value baseOffset = computeBaseOffset(depIndex, loc);
        SmallVector<Value> sizes = resolveSizes(originalDeps[depIndex], loc);
        SmallVector<Value> strides = AC->computeStridesFromSizes(sizes, loc);
        SmallVector<Value> offsets =
            resolveOffsets(originalDeps[depIndex], loc);
        SmallVector<Value> offsetIndices;
        offsetIndices.reserve(strides.size());
        for (size_t i = 0; i < strides.size(); ++i) {
          if (i < offsets.size())
            offsetIndices.push_back(offsets[i]);
          else
            offsetIndices.push_back(AC->createIndexConstant(0, loc));
        }
        Value newPtr = AC->create<DepGepOp>(loc, AC->llvmPtr, depv, baseOffset,
                                            offsetIndices, strides);
        op->getResult(0).replaceAllUsesWith(newPtr);
        op->erase();
      } else if (auto dbGep = dyn_cast<arts::DbGepOp>(op)) {
        if (dbGep.getBasePtr() != placeholder)
          continue;
        AC->setInsertionPoint(op);
        Location loc = op->getLoc();
        Value baseOffset = computeBaseOffset(depIndex, loc);
        SmallVector<Value> sizes = resolveSizes(originalDeps[depIndex], loc);
        SmallVector<Value> strides = AC->computeStridesFromSizes(sizes, loc);
        SmallVector<Value> indices = dbGep.getIndices();
        SmallVector<Value> offsets =
            resolveOffsets(originalDeps[depIndex], loc);
        SmallVector<Value> adjustedIndices;
        adjustedIndices.reserve(indices.size());
        for (size_t i = 0; i < indices.size(); ++i) {
          Value idx = indices[i];
          if (i < offsets.size()) {
            Value off = offsets[i];
            if (!matchPattern(off, m_Zero()))
              idx = AC->create<arith::SubIOp>(loc, idx, off);
          }
          adjustedIndices.push_back(idx);
        }
        Value newDepGep = AC->create<DepGepOp>(
            loc, AC->llvmPtr, depv, baseOffset, adjustedIndices, strides);
        op->getResult(0).replaceAllUsesWith(newDepGep);
        op->erase();
      }
    }
  }
}

//===----------------------------------------------------------------------===//
// Pass Registration
//===----------------------------------------------------------------------===//

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createEdtLoweringPass() {
  return std::make_unique<EdtLoweringPass>();
}

} // namespace arts
} // namespace mlir
