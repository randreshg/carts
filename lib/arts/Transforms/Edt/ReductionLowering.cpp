///==========================================================================///
/// File: ReductionLowering.cpp
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

#include "arts/Transforms/Edt/ReductionLowering.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/APFloat.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(reduction_lowering);

using namespace mlir;
using namespace mlir::arts;

namespace {

static Value castToIndex(ArtsCodegen *AC, Value v, Location loc) {
  if (!v)
    return v;
  if (v.getType().isIndex())
    return v;
  auto indexTy = AC->getBuilder().getIndexType();
  return AC->create<arith::IndexCastOp>(loc, indexTy, v);
}

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

} // namespace

DenseSet<Value> mlir::arts::detectReductionBlockArgs(ForOp forOp) {
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

ReductionLoweringInfo mlir::arts::allocatePartialAccumulators(
    ArtsCodegen *AC, ForOp forOp, EdtOp parallelEdt, Location loc,
    Attribute loopMetadataAttr, bool splitMode) {
  ReductionLoweringInfo redInfo;
  redInfo.loopMetadataAttr = loopMetadataAttr;
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
  else if (parallelEdt.getConcurrency() == EdtConcurrency::internode) {
    Value nodes =
        castToIndex(AC, AC->create<GetTotalNodesOp>(loc).getResult(), loc);
    Value threads =
        castToIndex(AC, AC->create<GetTotalWorkersOp>(loc).getResult(), loc);
    numWorkers = AC->create<arith::MulIOp>(loc, nodes, threads);
  } else {
    numWorkers = AC->create<GetTotalWorkersOp>(loc).getResult();
  }
  numWorkers = castToIndex(AC, numWorkers, loc);

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

  for (auto [idx, redVar] : llvm::enumerate(reductionAccums)) {
    redInfo.reductionVars.push_back(redVar);

    auto redMemRef = redVar.getType().dyn_cast<MemRefType>();
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
        auto allocInfo = DatablockUtils::traceToDbAlloc(existingPtr);
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

    /// Step 2: Allocate and initialize partial accumulators DB.
    auto allocOp = AC->create<DbAllocOp>(
        loc, ArtsMode::inout, routeZero, DbAllocType::heap, DbMode::write,
        elementType, Value(), ValueRange{numWorkers}, ValueRange{sizeOne});
    Value partialGuid = allocOp.getGuid();
    Value partialPtr = allocOp.getPtr();

    Value one = AC->createIndexConstant(1, loc);
    Location loopLoc = redInfo.loopLocation.value_or(loc);
    auto initLoop = AC->create<scf::ForOp>(loopLoc, zeroIndex, numWorkers, one);
    if (loopMetadataAttr)
      initLoop->setAttr(AttrNames::LoopMetadata::Name, loopMetadataAttr);
    AC->setInsertionPointToStart(initLoop.getBody());
    Value workerIdx = initLoop.getInductionVar();

    SmallVector<Value> outerIndices{workerIdx};
    Value partialMemRef = AC->create<DbRefOp>(loc, partialPtr, outerIndices);
    AC->create<memref::StoreOp>(loc, identity, partialMemRef, innerIndices);

    AC->setInsertionPointAfter(initLoop);

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

  ARTS_INFO(" - Creating single result EDT to combine partial accumulators");

  Value numWorkers = redInfo.numWorkers;
  if (!numWorkers) {
    ARTS_ERROR("Missing numWorkers in ReductionInfo");
    return;
  }

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

    auto acqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::in, partialGuid, partialPtr, PartitionMode::coarse,
        /*indices=*/SmallVector<Value>{}, /*offsets=*/partialOffsets,
        /*sizes=*/partialSizes,
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

    Value zeroIndex = AC->createIndexConstant(0, loc);
    Value sizeOne = AC->createIndexConstant(1, loc);
    SmallVector<Value> finalOffsets{zeroIndex};
    SmallVector<Value> finalSizes{sizeOne};

    auto acqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::out, finalGuid, finalPtr, PartitionMode::coarse,
        /*indices=*/SmallVector<Value>{}, /*offsets=*/finalOffsets,
        /*sizes=*/finalSizes,
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
      loc, EdtType::task, EdtConcurrency::intranode, routeZero, edtDeps);

  Block &resultBlock = resultEdt.getBody().front();
  AC->setInsertionPointToStart(&resultBlock);

  SmallVector<BlockArgument> depBlockArgs;
  depBlockArgs.reserve(edtDeps.size());
  for (Value dep : edtDeps)
    depBlockArgs.push_back(resultBlock.addArgument(dep.getType(), loc));

  for (uint64_t i = 0; i < reductionCount; i++) {
    BlockArgument partialArg = depBlockArgs[i];
    BlockArgument finalArg = depBlockArgs[partialAcqPtrs.size() + i];

    Value zeroIndex = AC->createIndexConstant(0, loc);
    SmallVector<Value> indices{zeroIndex};
    Value finalMemRef = AC->create<DbRefOp>(loc, finalArg, indices);

    auto memType = finalMemRef.getType().cast<MemRefType>();
    Type elemType = memType.getElementType();
    Value identity = createZeroValue(AC, elemType, loc);
    if (!identity) {
      ARTS_ERROR("Unsupported reduction element type");
      return;
    }

    Value zeroIdx = AC->createIndexConstant(0, loc);
    Value oneIdx = AC->createIndexConstant(1, loc);
    Location loopLoc = redInfo.loopLocation.value_or(loc);
    auto combineLoop = AC->create<scf::ForOp>(loopLoc, zeroIdx, numWorkers,
                                              oneIdx, ValueRange{identity});
    if (redInfo.loopMetadataAttr)
      combineLoop->setAttr(AttrNames::LoopMetadata::Name,
                           redInfo.loopMetadataAttr);

    AC->setInsertionPointToStart(combineLoop.getBody());
    Value workerIdx = combineLoop.getInductionVar();
    Value accumulator = combineLoop.getRegionIterArg(0);

    Value workerSlot =
        AC->create<DbRefOp>(loc, partialArg, SmallVector<Value>{workerIdx});
    SmallVector<Value> workerElemIdx{zeroIdx};
    Value partialValue =
        AC->create<memref::LoadOp>(loc, workerSlot, workerElemIdx);

    if (AC->isDebug()) {
      Value workerIdxI64 = AC->castToInt(AC->Int64, workerIdx, loc);
      Value partialI64 = AC->castToInt(AC->Int64, partialValue, loc);
      AC->createPrintfCall(loc, "agg partial w=%lu val=%lu\\n",
                           ValueRange{workerIdxI64, partialI64});
    }

    Value combined = AC->create<arith::AddIOp>(loc, accumulator, partialValue);
    AC->create<scf::YieldOp>(loc, combined);

    AC->setInsertionPointAfter(combineLoop);
    SmallVector<Value> finalIndices{zeroIdx};
    AC->create<memref::StoreOp>(loc, combineLoop.getResult(0), finalMemRef,
                                finalIndices);

    if (AC->isDebug()) {
      Value finalValI64 =
          AC->castToInt(AC->Int64, combineLoop.getResult(0), loc);
      AC->createPrintfCall(loc, "agg final=%lu\\n", ValueRange{finalValI64});
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
    auto redMemRefTy = redVar.getType().dyn_cast<MemRefType>();
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

  for (uint64_t i = 0; i < redInfo.partialAccumGuids.size(); i++) {
    AC->create<DbFreeOp>(loc, redInfo.partialAccumGuids[i]);
    AC->create<DbFreeOp>(loc, redInfo.partialAccumPtrs[i]);
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
