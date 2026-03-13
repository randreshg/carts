///==========================================================================///
/// File: EdtReductionLowering.cpp
///
/// Shared reduction-lowering helpers used by ForLowering.
///
/// Transformation:
///   BEFORE:
///     arts.for ... reductions(%acc = ...)
///
///   AFTER (conceptual):
///     1) Allocate partial accumulator array [numWorkers]
///     2) Each task EDT writes partial[workerId]
///     3) Result EDT combines partials -> final accumulator DB
///     4) Final result is exposed to host/result users
///
/// The reduction flow preserves the same dependence ordering via EDT creation
/// and dependency recording in downstream lowering.
///==========================================================================///

#include "arts/transforms/edt/EdtReductionLowering.h"
#include "arts/analysis/heuristics/DistributionHeuristics.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/APFloat.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(reduction_lowering);

using namespace mlir;
using namespace mlir::arts;

namespace {

static Value createZeroValue(ArtsCodegen *AC, Type elemType, Location loc) {
  if (!elemType)
    return Value();

  if (isa<IndexType>(elemType))
    return AC->createIndexConstant(0, loc);

  if (auto intTy = dyn_cast<IntegerType>(elemType))
    return AC->create<arith::ConstantIntOp>(loc, 0, intTy.getWidth());

  if (auto floatTy = dyn_cast<FloatType>(elemType)) {
    llvm::APFloat zero = llvm::APFloat::getZero(floatTy.getFloatSemantics());
    return AC->create<arith::ConstantFloatOp>(loc, zero, floatTy);
  }

  return Value();
}

} // namespace

DenseSet<Value> mlir::arts::detectReductionBlockArgs(ForOp forOp) {
  DenseSet<Value> result;
  forOp.walk([&](memref::StoreOp store) {
    if (auto dbRef = store.getMemRef().getDefiningOp<DbRefOp>()) {
      if (auto blockArg = dyn_cast<BlockArgument>(dbRef.getSource())) {
        result.insert(blockArg);
        ARTS_DEBUG(
            "  - Detected old accumulator block arg in store: " << blockArg);
      }
    }
  });
  return result;
}

bool mlir::arts::shouldSkipReductionArg(
    BlockArgument parallelArg, const ReductionLoweringInfo &redInfo,
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
    ARTS_DEBUG("  - Skipping old accumulator (replaced by worker-based "
               "partials)");
    return true;
  }

  return false;
}

void mlir::arts::collectOldAccumulatorDbRefs(
    ForOp forOp, Block &parallelBlock,
    const DenseSet<Value> &reductionBlockArgs, DenseSet<Operation *> &opsToSkip,
    IRMapping &mapper, Value myAccumulator) {
  forOp.walk([&](DbRefOp dbRef) {
    if (auto blockArg = dyn_cast<BlockArgument>(dbRef.getSource())) {
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

ReductionLoweringInfo mlir::arts::allocatePartialAccumulators(
    ArtsCodegen *AC, ForOp forOp, EdtOp parallelEdt, Location loc,
    Attribute loopMetadataAttr, bool splitMode, Value workerCountOverride) {
  ReductionLoweringInfo redInfo;
  redInfo.loopMetadataAttr = loopMetadataAttr;
  redInfo.loopLocation = forOp.getLoc();
  redInfo.parentConcurrency = parallelEdt.getConcurrency();
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

  /// Resolve worker count consistently with ForLowering distribution.
  /// Prefer explicit EDT worker annotations to avoid context-sensitive
  /// runtime-query cloning when outlining result EDT bodies.
  Value numWorkers;
  if (workerCountOverride) {
    numWorkers = workerCountOverride;
  } else if (auto explicitWorkers =
                 arts::getWorkers(parallelEdt.getOperation());
             explicitWorkers && *explicitWorkers > 0) {
    numWorkers = AC->createIndexConstant(*explicitWorkers, loc);
  } else {
    numWorkers = DistributionHeuristics::getTotalWorkers(AC, loc, parallelEdt);
  }

  Block &parallelBlock = parallelEdt.getBody().front();
  ValueRange parentDeps = parallelEdt.getDependencies();

  /// Detect existing reduction handles inside the parallel EDT.
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
  auto numWorkersConst = ValueAnalysis::tryFoldConstantIndex(numWorkers);
  bool isSingleWorker = numWorkersConst && *numWorkersConst == 1;
  bool canCombineDirectly =
      splitMode && isSingleWorker &&
      llvm::all_of(existingResultPtrs, [](Value ptr) {
        return static_cast<bool>(DbUtils::traceToDbAlloc(ptr));
      });
  redInfo.combineDirectlyInTask = canCombineDirectly;

  for (auto [idx, redVar] : llvm::enumerate(reductionAccums)) {
    redInfo.reductionVars.push_back(redVar);

    auto redMemRef = dyn_cast<MemRefType>(redVar.getType());
    Type elementType =
        redMemRef ? redMemRef.getElementType() : redVar.getType();
    Value identity = createZeroValue(AC, elementType, loc);
    if (!identity) {
      ARTS_ERROR("Unsupported reduction element type for initialization");
      continue;
    }

    /// Step 1: Reuse existing final result DB if available, otherwise fallback.
    if (idx < existingResultArgs.size()) {
      redInfo.finalResultArgs.push_back(existingResultArgs[idx]);

      if (idx < existingResultPtrs.size()) {
        Value existingPtr = existingResultPtrs[idx];
        auto allocInfo = DbUtils::traceToDbAlloc(existingPtr);
        if (allocInfo) {
          auto [rootGuid, rootPtr] = *allocInfo;
          redInfo.finalResultGuids.push_back(rootGuid);
          redInfo.finalResultPtrs.push_back(rootPtr);
          redInfo.finalResultIsExternal.push_back(true);
          ARTS_DEBUG("  - Traced reduction result to DbAllocOp from parent");
        } else {
          redInfo.finalResultPtrs.push_back(existingPtr);
          redInfo.finalResultIsExternal.push_back(true);
          ARTS_DEBUG(
              "  - Reusing pre-allocated reduction result datablock from "
              "parent (acquire ptr)");
        }
      }
      ARTS_DEBUG(
          "  - Reusing pre-allocated reduction result datablock from parent");
    } else {
      auto finalAllocOp = AC->create<DbAllocOp>(
          loc, ArtsMode::inout, routeZero, DbAllocType::heap, DbMode::write,
          elementType, Value(), ValueRange{sizeOne}, ValueRange{sizeOne});
      Value finalGuid = finalAllocOp.getResult(0);
      Value finalPtr = finalAllocOp.getResult(1);

      SmallVector<Value> finalInitIndices{zeroIndex};
      Value finalMemRef = AC->create<DbRefOp>(loc, finalPtr, finalInitIndices);
      SmallVector<Value> innerIndices{zeroIndex};
      AC->create<memref::StoreOp>(loc, identity, finalMemRef, innerIndices);

      redInfo.finalResultGuids.push_back(finalGuid);
      redInfo.finalResultPtrs.push_back(finalPtr);
      redInfo.finalResultIsExternal.push_back(false);

      if (!splitMode) {
        auto finalDepAcqOp = AC->create<DbAcquireOp>(
            loc, ArtsMode::inout, finalGuid, finalPtr, PartitionMode::coarse,
            /*indices=*/SmallVector<Value>{},
            /*offsets=*/SmallVector<Value>{},
            /*sizes=*/SmallVector<Value>{},
            /*partition_indices=*/SmallVector<Value>{},
            /*partition_offsets=*/SmallVector<Value>{},
            /*partition_sizes=*/SmallVector<Value>{});
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

    if (canCombineDirectly) {
      Value finalGuid = redInfo.finalResultGuids[idx];
      Value finalPtr = redInfo.finalResultPtrs[idx];
      Value finalMemRef = AC->create<DbRefOp>(loc, finalPtr, innerIndices);
      AC->create<memref::StoreOp>(loc, identity, finalMemRef, innerIndices);

      redInfo.partialAccumGuids.push_back(finalGuid);
      redInfo.partialAccumPtrs.push_back(finalPtr);

      ARTS_DEBUG("  - Reusing final result DB as the worker-local accumulator");
      continue;
    }

    /// Step 2: Allocate and initialize partial accumulators DB.
    auto allocOp = AC->create<DbAllocOp>(
        loc, ArtsMode::inout, routeZero, DbAllocType::heap, DbMode::write,
        elementType, Value(), ValueRange{numWorkers}, ValueRange{sizeOne});
    Value partialGuid = allocOp.getGuid();
    Value partialPtr = allocOp.getPtr();

    Value one = AC->createIndexConstant(1, loc);
    Location loopLoc = redInfo.loopLocation.value_or(loc);

    /// When numWorkers is a compile-time constant 1, emit straight-line init
    /// instead of a loop.  A loop from 0 to 1 would use its induction variable
    /// as a db_ref index, which the verifier rejects for coarse-grained DBs
    /// (sizes[1]) because the IV is not a constant zero.
    if (isSingleWorker) {
      SmallVector<Value> outerIndices{zeroIndex};
      Value partialMemRef = AC->create<DbRefOp>(loc, partialPtr, outerIndices);
      AC->create<memref::StoreOp>(loc, identity, partialMemRef, innerIndices);
    } else {
      auto initLoop =
          AC->create<scf::ForOp>(loopLoc, zeroIndex, numWorkers, one);
      if (loopMetadataAttr)
        initLoop->setAttr(AttrNames::LoopMetadata::Name, loopMetadataAttr);
      AC->setInsertionPointToStart(initLoop.getBody());
      Value workerIdx = initLoop.getInductionVar();

      SmallVector<Value> outerIndices{workerIdx};
      Value partialMemRef = AC->create<DbRefOp>(loc, partialPtr, outerIndices);
      AC->create<memref::StoreOp>(loc, identity, partialMemRef, innerIndices);

      AC->setInsertionPointAfter(initLoop);
    }

    redInfo.partialAccumGuids.push_back(partialGuid);
    redInfo.partialAccumPtrs.push_back(partialPtr);

    if (!splitMode) {
      SmallVector<Value> partialOffsets{zeroIndex};
      SmallVector<Value> partialSizes{numWorkers};
      auto depAcqOp = AC->create<DbAcquireOp>(
          loc, ArtsMode::inout, partialGuid, partialPtr, PartitionMode::coarse,
          /*indices=*/SmallVector<Value>{}, /*offsets=*/partialOffsets,
          /*sizes=*/partialSizes,
          /*partition_indices=*/SmallVector<Value>{},
          /*partition_offsets=*/SmallVector<Value>{},
          /*partition_sizes=*/SmallVector<Value>{});
      Value depPtr = depAcqOp.getPtr();
      parallelEdt.appendDependency(depPtr);
      BlockArgument partialPtrArg =
          parallelBlock.addArgument(depPtr.getType(), loc);
      redInfo.partialAccumArgs.push_back(partialPtrArg);
    }

    ARTS_DEBUG("  - Allocated and initialized partial accumulator DB for "
               "reduction variable");
  }

  redInfo.numWorkers = numWorkers;
  return redInfo;
}

void mlir::arts::createResultEdt(ArtsCodegen *AC,
                                 ReductionLoweringInfo &redInfo, Location loc) {
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  if (redInfo.reductionVars.empty())
    return;
  if (redInfo.combineDirectlyInTask)
    return;

  ARTS_INFO(" - Creating single result EDT to combine partial accumulators");

  Value numWorkers = redInfo.numWorkers;
  if (!numWorkers) {
    ARTS_ERROR("Missing numWorkers in ReductionInfo");
    return;
  }

  Value zeroIndex = AC->createIndexConstant(0, loc);
  Value sizeOne = AC->createIndexConstant(1, loc);
  auto numWorkersConst = ValueAnalysis::tryFoldConstantIndex(numWorkers);
  bool isSingleWorker = numWorkersConst && *numWorkersConst == 1;

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

    auto acqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::in, partialGuid, partialPtr, PartitionMode::coarse,
        /*indices=*/SmallVector<Value>{},
        /*offsets=*/SmallVector<Value>{zeroIndex},
        /*sizes=*/SmallVector<Value>{numWorkers},
        /*partition_indices=*/SmallVector<Value>{},
        /*partition_offsets=*/SmallVector<Value>{},
        /*partition_sizes=*/SmallVector<Value>{});

    partialAcqPtrs.push_back(acqOp.getResult(1));
  }

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

    auto acqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::out, finalGuid, finalPtr, PartitionMode::coarse,
        /*indices=*/SmallVector<Value>{},
        /*offsets=*/SmallVector<Value>{zeroIndex},
        /*sizes=*/SmallVector<Value>{sizeOne},
        /*partition_indices=*/SmallVector<Value>{},
        /*partition_offsets=*/SmallVector<Value>{},
        /*partition_sizes=*/SmallVector<Value>{});

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

  SmallVector<Value> edtDeps;
  edtDeps.reserve(partialAcqPtrs.size() + finalResultAcqPtrs.size());
  edtDeps.append(partialAcqPtrs.begin(), partialAcqPtrs.end());
  edtDeps.append(finalResultAcqPtrs.begin(), finalResultAcqPtrs.end());

  Value routeZero = AC->create<arith::ConstantIntOp>(loc, 0, 32);
  auto resultEdt = AC->create<EdtOp>(
      loc, EdtType::task, redInfo.parentConcurrency, routeZero, edtDeps);

  Block &resultBlock = resultEdt.getBody().front();
  AC->setInsertionPointToStart(&resultBlock);

  SmallVector<BlockArgument> depBlockArgs;
  depBlockArgs.reserve(edtDeps.size());
  for (Value dep : edtDeps)
    depBlockArgs.push_back(resultBlock.addArgument(dep.getType(), loc));

  Location loopLoc = redInfo.loopLocation.value_or(loc);

  for (uint64_t i = 0; i < reductionCount; i++) {
    BlockArgument partialArg = depBlockArgs[i];
    BlockArgument finalArg = depBlockArgs[partialAcqPtrs.size() + i];

    SmallVector<Value> zeroIndices{zeroIndex};
    Value finalMemRef = AC->create<DbRefOp>(loc, finalArg, zeroIndices);

    auto memType = cast<MemRefType>(finalMemRef.getType());
    Type elemType = memType.getElementType();
    Value identity = createZeroValue(AC, elemType, loc);
    if (!identity) {
      ARTS_ERROR("Unsupported reduction element type");
      return;
    }

    /// When numWorkers is a compile-time constant 1, emit straight-line
    /// combine instead of a loop.  A loop from 0 to 1 would use its IV as a
    /// db_ref index, which the verifier rejects for coarse-grained DBs.
    if (isSingleWorker) {
      Value workerSlot =
          AC->create<DbRefOp>(loc, partialArg, SmallVector<Value>{zeroIndex});
      Value partialValue =
          AC->create<memref::LoadOp>(loc, workerSlot, zeroIndices);
      AC->create<memref::StoreOp>(loc, partialValue, finalMemRef, zeroIndices);
    } else {
      auto combineLoop = AC->create<scf::ForOp>(loopLoc, zeroIndex, numWorkers,
                                                sizeOne, ValueRange{identity});
      if (redInfo.loopMetadataAttr)
        combineLoop->setAttr(AttrNames::LoopMetadata::Name,
                             redInfo.loopMetadataAttr);

      AC->setInsertionPointToStart(combineLoop.getBody());
      Value workerIdx = combineLoop.getInductionVar();
      Value accumulator = combineLoop.getRegionIterArg(0);

      Value workerSlot =
          AC->create<DbRefOp>(loc, partialArg, SmallVector<Value>{workerIdx});
      Value partialValue =
          AC->create<memref::LoadOp>(loc, workerSlot, zeroIndices);

      Value combined;
      if (isa<FloatType>(elemType))
        combined = AC->create<arith::AddFOp>(loc, accumulator, partialValue);
      else
        combined = AC->create<arith::AddIOp>(loc, accumulator, partialValue);
      AC->create<scf::YieldOp>(loc, combined);

      AC->setInsertionPointAfter(combineLoop);
      AC->create<memref::StoreOp>(loc, combineLoop.getResult(0), finalMemRef,
                                  zeroIndices);
    }
  }

  AC->setInsertionPointToEnd(&resultBlock);
  for (BlockArgument blockArg : depBlockArgs)
    AC->create<DbReleaseOp>(loc, blockArg);

  AC->create<YieldOp>(loc);
  ARTS_INFO(" - Result EDT created successfully");
}

void mlir::arts::finalizeReductionAfterEpoch(ArtsCodegen *AC,
                                             ReductionLoweringInfo &redInfo,
                                             Location loc) {
  if (redInfo.reductionVars.empty())
    return;

  Value zeroIdx = AC->createIndexConstant(0, loc);
  SmallVector<Value> zeroIndices{zeroIdx};

  /// Copy final results back to original reduction variables.
  for (auto [idx, redVar] : llvm::enumerate(redInfo.reductionVars)) {
    if (idx >= redInfo.finalResultPtrs.size())
      continue;
    auto redMemRefTy = dyn_cast<MemRefType>(redVar.getType());
    if (!redMemRefTy)
      continue;

    Value finalHandle = redInfo.finalResultPtrs[idx];
    Value finalRef = AC->create<DbRefOp>(loc, finalHandle, zeroIndices);
    Value loaded = AC->create<memref::LoadOp>(loc, finalRef, zeroIndices);
    Value casted = AC->castParameter(redMemRefTy.getElementType(), loaded, loc);
    AC->create<memref::StoreOp>(loc, casted, redVar, zeroIndices);

    if (idx < redInfo.hostResultPtrs.size()) {
      Value hostPtr = redInfo.hostResultPtrs[idx];
      Value hostRef = AC->create<DbRefOp>(loc, hostPtr, zeroIndices);
      AC->create<memref::StoreOp>(loc, casted, hostRef, zeroIndices);
    }
  }

  if (!redInfo.combineDirectlyInTask) {
    for (uint64_t i = 0; i < redInfo.partialAccumGuids.size(); i++) {
      AC->create<DbFreeOp>(loc, redInfo.partialAccumGuids[i]);
      AC->create<DbFreeOp>(loc, redInfo.partialAccumPtrs[i]);
    }
  }

  for (uint64_t i = 0; i < redInfo.finalResultGuids.size(); i++) {
    if (i < redInfo.finalResultIsExternal.size() &&
        redInfo.finalResultIsExternal[i]) {
      ARTS_DEBUG("  - Skipping db_free for external final result DB at index "
                 << i);
      continue;
    }
    AC->create<DbFreeOp>(loc, redInfo.finalResultGuids[i]);
    AC->create<DbFreeOp>(loc, redInfo.finalResultPtrs[i]);
  }
}
