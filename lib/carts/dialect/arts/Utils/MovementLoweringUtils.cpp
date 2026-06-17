///==========================================================================///
/// File: MovementLoweringUtils.cpp
///
/// Shared SDE movement → ARTS lowering helpers for the SDE-to-ARTS boundary.
///==========================================================================///

#include "carts/dialect/arts/Utils/MovementLoweringUtils.h"

#include "carts/dialect/arts/Utils/DbBackedMemrefUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/DenseMap.h"
#include <algorithm>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

FailureOr<ReduceScatterRedistFacts>
arts::buildReduceScatterRedistFacts(sde::SdeSuReduceScatterOp reduce) {
  if (!reduce.getArrayIdAttr())
    return reduce.emitOpError()
           << "commits reduce-scatter movement without array_id";
  auto ownerDims = readI64ArrayAttr(reduce.getOwnerDims());
  auto blockShape = readI64ArrayAttr(reduce.getBlockShape());
  if (!ownerDims || !blockShape || ownerDims->empty() || blockShape->empty())
    return reduce.emitOpError()
           << "commits reduce-scatter movement without concrete owner/block "
              "geometry";
  return ReduceScatterRedistFacts{
      reduce.getArrayIdAttr(), reduce.getOwnerDims(), reduce.getBlockShape(),
      reduce.getOwnerDims(), reduce.getBlockShape()};
}

FailureOr<AllToAllRedistFacts>
arts::buildAllToAllRedistFacts(sde::SdeSuAllToAllOp allToAll) {
  if (!allToAll.getArrayIdAttr())
    return allToAll.emitOpError()
           << "commits all-to-all movement without array_id";
  auto sourceOwner = readI64ArrayAttr(allToAll.getSourceOwnerDims());
  auto sourceBlock = readI64ArrayAttr(allToAll.getSourceBlockShape());
  auto targetOwner = readI64ArrayAttr(allToAll.getTargetOwnerDims());
  auto targetBlock = readI64ArrayAttr(allToAll.getTargetBlockShape());
  if (!sourceOwner || !sourceBlock || !targetOwner || !targetBlock ||
      sourceOwner->empty() || sourceBlock->empty() || targetOwner->empty() ||
      targetBlock->empty())
    return allToAll.emitOpError()
           << "commits all-to-all movement without concrete source/target "
              "owner/block geometry";
  return AllToAllRedistFacts{
      allToAll.getArrayIdAttr(), allToAll.getSourceOwnerDims(),
      allToAll.getSourceBlockShape(), allToAll.getTargetOwnerDims(),
      allToAll.getTargetBlockShape()};
}

bool arts::isOrderPreservingAllToAllOwnerDims(ArrayRef<int64_t> sourceOwner,
                                              ArrayRef<int64_t> targetOwner) {
  if (targetOwner.size() > sourceOwner.size())
    return false;
  for (size_t idx = 0; idx < targetOwner.size(); ++idx) {
    if (targetOwner[idx] != sourceOwner[idx])
      return false;
  }
  return true;
}

bool arts::isPermutedAllToAllOwnerDims(ArrayRef<int64_t> sourceOwner,
                                       ArrayRef<int64_t> targetOwner) {
  if (sourceOwner.size() != targetOwner.size())
    return false;
  SmallVector<int64_t, 4> sortedSource(sourceOwner.begin(), sourceOwner.end());
  SmallVector<int64_t, 4> sortedTarget(targetOwner.begin(), targetOwner.end());
  llvm::sort(sortedSource);
  llvm::sort(sortedTarget);
  return sortedSource == sortedTarget && sourceOwner != targetOwner;
}

FailureOr<sde::SdeSuIterateOp>
arts::findAllToAllConsumerIterate(sde::SdeSuAllToAllOp allToAll) {
  bool sawBarrier = false;
  for (Operation *next = allToAll->getNextNode(); next;
       next = next->getNextNode()) {
    if (isa<sde::SdeSuBarrierOp>(next)) {
      sawBarrier = true;
      continue;
    }
    if (auto iterate = dyn_cast<sde::SdeSuIterateOp>(next)) {
      if (!sawBarrier)
        return allToAll.emitOpError()
               << "requires a committed sde.su_barrier between the all-to-all "
                  "movement and the consumer iterate";
      return iterate;
    }
    if (isa<sde::SdeSuHaloOp, sde::SdeSuReduceScatterOp>(next))
      continue;
    break;
  }
  return allToAll.emitOpError()
         << "has no consumer su_iterate after the all-to-all barrier";
}

FailureOr<Value> arts::findAllToAllTargetMemref(sde::SdeSuAllToAllOp allToAll,
                                                sde::SdeSuIterateOp consumer) {
  Value sourceMu = allToAll.getMu();
  Value targetMu;
  bool foundError = false;
  auto considerWrite = [&](Value mu) {
    if (!mu || mu == sourceMu)
      return;
    if (targetMu && targetMu != mu) {
      foundError = true;
      return;
    }
    targetMu = mu;
  };

  consumer.walk(
      [&](memref::StoreOp store) { considerWrite(store.getMemRef()); });
  consumer.walk([&](sde::SdeArrayLayoutRootOp root) {
    if (root.getMode() != sde::SdeAccessMode::write)
      return;
    considerWrite(root.getRoot());
  });
  if (foundError)
    return consumer.emitOpError()
           << "commits multiple distinct write MUs for one all-to-all target";
  if (!targetMu)
    return allToAll.emitOpError()
           << "has no distinct consumer write MU for all-to-all target DB "
              "realization";
  return targetMu;
}

LogicalResult arts::emitAllToAllBlockCopy(
    OpBuilder &builder, Location loc, ArrayRef<Value> sourcePayloads,
    Value targetPayload, ArrayRef<int64_t> sourceBlockShape,
    ArrayRef<int64_t> targetBlockShape, ArrayRef<int64_t> targetBlockCoords,
    int64_t coveringSourceBlockBase, Type elementType) {
  if (sourcePayloads.empty())
    return failure();
  std::optional<int64_t> targetTileElems = staticProduct(targetBlockShape);
  if (!targetTileElems)
    return failure();

  std::optional<SmallVector<int64_t, 4>> targetStrides =
      staticRowMajorStrides(targetBlockShape);
  if (!targetStrides)
    return failure();

  auto linearLoop =
      scf::ForOp::create(builder, loc, createZeroIndex(builder, loc),
                         createConstantIndex(builder, loc, *targetTileElems),
                         createOneIndex(builder, loc));
  OpBuilder loopBuilder = OpBuilder::atBlockBegin(linearLoop.getBody());
  Value linear = linearLoop.getInductionVar();

  SmallVector<Value, 4> targetLocalCoords;
  targetLocalCoords.reserve(targetBlockShape.size());
  for (unsigned dim = 0; dim < targetBlockShape.size(); ++dim) {
    int64_t stride = (*targetStrides)[dim];
    Value raw = loopBuilder.createOrFold<arith::DivUIOp>(
        loc, linear, createConstantIndex(loopBuilder, loc, stride));
    Value coord = raw;
    if (dim + 1 < targetBlockShape.size())
      coord = loopBuilder.createOrFold<arith::RemUIOp>(
          loc, raw,
          createConstantIndex(loopBuilder, loc, targetBlockShape[dim]));
    targetLocalCoords.push_back(coord);
  }

  if (targetBlockShape.size() != 2 || sourceBlockShape.size() != 2 ||
      targetBlockCoords.empty())
    return failure();

  Value blockRowBase = createConstantIndex(
      loopBuilder, loc, targetBlockCoords.front() * targetBlockShape[0]);
  Value globalRow = loopBuilder.createOrFold<arith::AddIOp>(
      loc, blockRowBase, targetLocalCoords[0]);
  Value srcBlockIdx = loopBuilder.createOrFold<arith::DivUIOp>(
      loc, globalRow,
      createConstantIndex(loopBuilder, loc, sourceBlockShape[0]));
  Value srcLocalRow = loopBuilder.createOrFold<arith::RemUIOp>(
      loc, globalRow,
      createConstantIndex(loopBuilder, loc, sourceBlockShape[0]));
  Value relBlock = loopBuilder.createOrFold<arith::SubIOp>(
      loc, srcBlockIdx,
      createConstantIndex(loopBuilder, loc, coveringSourceBlockBase));

  auto loadFromPayload = [&](OpBuilder &bodyBuilder, Value payload) -> Value {
    SmallVector<Value, 4> sourceIndices;
    sourceIndices.push_back(srcLocalRow);
    sourceIndices.push_back(targetLocalCoords[1]);
    return memref::LoadOp::create(bodyBuilder, loc, payload, sourceIndices);
  };

  Value loaded;
  if (sourcePayloads.size() == 1) {
    loaded = loadFromPayload(loopBuilder, sourcePayloads.front());
  } else if (sourcePayloads.size() == 2) {
    auto ifOp = scf::IfOp::create(
        loopBuilder, loc, elementType,
        arith::CmpIOp::create(loopBuilder, loc, arith::CmpIPredicate::eq,
                              relBlock, createZeroIndex(loopBuilder, loc)),
        /*withElseRegion=*/true);
    {
      OpBuilder thenBuilder =
          OpBuilder::atBlockBegin(&ifOp.getThenRegion().front());
      scf::YieldOp::create(thenBuilder, loc,
                           loadFromPayload(thenBuilder, sourcePayloads[0]));
    }
    {
      OpBuilder elseBuilder =
          OpBuilder::atBlockBegin(&ifOp.getElseRegion().front());
      scf::YieldOp::create(elseBuilder, loc,
                           loadFromPayload(elseBuilder, sourcePayloads[1]));
    }
    loaded = ifOp.getResult(0);
  } else {
    return failure();
  }

  memref::StoreOp::create(loopBuilder, loc, loaded, targetPayload,
                          targetLocalCoords);
  builder.setInsertionPointAfter(linearLoop.getOperation());
  return success();
}

LogicalResult arts::convertAllToAllMovement(sde::SdeSuAllToAllOp allToAll) {
  FailureOr<AllToAllRedistFacts> facts = buildAllToAllRedistFacts(allToAll);
  if (failed(facts))
    return failure();

  std::optional<SmallVector<int64_t, 4>> sourceOwner =
      readI64ArrayAttr(facts->sourceOwnerDims);
  std::optional<SmallVector<int64_t, 4>> targetOwner =
      readI64ArrayAttr(facts->targetOwnerDims);
  std::optional<SmallVector<int64_t, 4>> sourceBlock =
      readI64ArrayAttr(facts->sourceBlockShape);
  std::optional<SmallVector<int64_t, 4>> targetBlock =
      readI64ArrayAttr(facts->targetBlockShape);
  if (!sourceOwner || !targetOwner || !sourceBlock || !targetBlock)
    return failure();

  if (isPermutedAllToAllOwnerDims(*sourceOwner, *targetOwner))
    return allToAll.emitOpError()
           << "commits permuted all-to-all owner dimensions; extend DbAlloc "
              "owner-dims before realizing transpose repartition";
  if (!isOrderPreservingAllToAllOwnerDims(*sourceOwner, *targetOwner))
    return allToAll.emitOpError()
           << "commits non-order-preserving all-to-all owner geometry; tier-1 "
              "realizer requires a leading-prefix owner layout";

  DbAllocOp sourceAlloc = resolveBoundaryDbAlloc(allToAll.getMu());
  if (!sourceAlloc)
    return allToAll.emitOpError()
           << "does not reference an ARTS DB-backed source MU after storage "
              "realization";

  FailureOr<sde::SdeSuIterateOp> consumer =
      findAllToAllConsumerIterate(allToAll);
  if (failed(consumer))
    return failure();
  FailureOr<Value> targetMu = findAllToAllTargetMemref(allToAll, *consumer);
  if (failed(targetMu))
    return failure();
  DbAllocOp targetAlloc = resolveBoundaryDbAlloc(*targetMu);
  if (!targetAlloc)
    return allToAll.emitOpError()
           << "does not reference an ARTS DB-backed target MU after storage "
              "realization";
  if (sourceAlloc.getOperation() == targetAlloc.getOperation())
    return allToAll.emitOpError()
           << "requires distinct source and target DB allocations for "
              "all-to-all repartition";

  std::optional<SmallVector<int64_t, 4>> sourceDbSizes =
      foldStaticDbIndexValues(SmallVector<Value>(sourceAlloc.getSizes().begin(),
                                                 sourceAlloc.getSizes().end()));
  std::optional<SmallVector<int64_t, 4>> targetDbSizes =
      foldStaticDbIndexValues(SmallVector<Value>(targetAlloc.getSizes().begin(),
                                                 targetAlloc.getSizes().end()));
  if (!sourceDbSizes || !targetDbSizes)
    return allToAll.emitOpError()
           << "requires static DB block grids for all-to-all realization";

  std::optional<int64_t> sourceBlockCount = staticProduct(*sourceDbSizes);
  std::optional<int64_t> targetBlockCount = staticProduct(*targetDbSizes);
  if (!sourceBlockCount || !targetBlockCount || *sourceBlockCount <= 0 ||
      *targetBlockCount <= 0)
    return allToAll.emitOpError()
           << "has non-positive all-to-all block-grid volume";
  if (*sourceBlockCount % *targetBlockCount != 0 &&
      *targetBlockCount % *sourceBlockCount != 0)
    return allToAll.emitOpError()
           << "requires divisible source/target block counts for tier-1 "
              "all-to-all realization (got "
           << *sourceBlockCount << " vs " << *targetBlockCount << ")";
  const int64_t targetBlocks = *targetBlockCount;
  const int64_t sourceBlocks = *sourceBlockCount;
  const int64_t sourcePerTarget =
      sourceBlocks >= targetBlocks ? sourceBlocks / targetBlocks : 1;
  const int64_t targetsPerSource =
      targetBlocks >= sourceBlocks ? targetBlocks / sourceBlocks : 1;

  ModuleOp module = allToAll->getParentOfType<ModuleOp>();
  std::optional<int64_t> totalNodes = getRuntimeTotalNodes(module);
  if (!totalNodes)
    return allToAll.emitOpError()
           << "requires runtime node count for all-to-all realization";

  std::optional<DbOwnerRouteFacts> targetOwnerFacts =
      deriveDbOwnerRouteFactsFromDbGrid(targetAlloc);
  if (!targetOwnerFacts)
    return allToAll.emitOpError()
           << "cannot derive owner-route facts for the all-to-all target DB";

  Location loc = allToAll.getLoc();
  OpBuilder builder(allToAll);
  auto elementType =
      cast<MemRefType>(allToAll.getMu().getType()).getElementType();
  Value totalNodesValue = createConstantIndex(builder, loc, *totalNodes);
  EdtConcurrency concurrency =
      *totalNodes > 1 ? EdtConcurrency::internode : EdtConcurrency::intranode;

  for (int64_t targetLinear = 0; targetLinear < targetBlocks; ++targetLinear) {
    std::optional<SmallVector<int64_t, 4>> targetCoords =
        staticCoordsFromLinearIndex(targetLinear, *targetDbSizes);
    if (!targetCoords)
      return allToAll.emitOpError()
             << "cannot decode fold-constant all-to-all target block "
                "coordinates";

    SmallVector<int64_t, 4> coveringSourceLinears;
    if (sourcePerTarget > 1) {
      coveringSourceLinears.reserve(sourcePerTarget);
      for (int64_t slot = 0; slot < sourcePerTarget; ++slot)
        coveringSourceLinears.push_back(targetLinear * sourcePerTarget + slot);
    } else {
      coveringSourceLinears.push_back(targetLinear / targetsPerSource);
    }

    SmallVector<Value, 4> taskDeps;
    taskDeps.reserve(coveringSourceLinears.size() + 1);
    for (int64_t sourceLinear : coveringSourceLinears) {
      std::optional<SmallVector<int64_t, 4>> sourceCoords =
          staticCoordsFromLinearIndex(sourceLinear, *sourceDbSizes);
      if (!sourceCoords)
        return allToAll.emitOpError()
               << "cannot decode fold-constant all-to-all source block "
                  "coordinates";
      auto sourceAcquire = createUnitBlockDbAcquireAtCoords(
          builder, loc, ArtsMode::in, sourceAlloc, *sourceCoords);
      sourceAcquire.setPreserveAccessMode();
      sourceAcquire.setPreserveDepEdge();
      taskDeps.push_back(sourceAcquire.getPtr());
    }

    auto targetAcquire = createUnitBlockDbAcquireAtCoords(
        builder, loc, ArtsMode::out, targetAlloc, *targetCoords);
    targetAcquire.setPreserveAccessMode();
    targetAcquire.setPreserveDepEdge();
    taskDeps.push_back(targetAcquire.getPtr());

    SmallVector<Value, 4> targetCoordValues;
    targetCoordValues.reserve(targetCoords->size());
    for (int64_t coord : *targetCoords)
      targetCoordValues.push_back(createConstantIndex(builder, loc, coord));
    SmallVector<Value, 4> targetDbSizeValues(targetAlloc.getSizes().begin(),
                                             targetAlloc.getSizes().end());
    Value route = createDbOwnerRouteForCoords(
        builder, loc, targetDbSizeValues, targetCoordValues, totalNodesValue,
        *targetOwnerFacts);
    if (!route)
      return allToAll.emitOpError()
             << "cannot derive owner route for all-to-all target block "
             << targetLinear;

    auto edt = EdtOp::create(builder, loc, EdtType::sync, concurrency, route,
                             taskDeps, SmallVector<Value>{});
    Block &body = edt.getBody().front();
    for (Value dep : taskDeps)
      body.addArgument(dep.getType(), loc);
    OpBuilder bodyBuilder(edt.getContext());
    bodyBuilder.setInsertionPointToStart(&body);
    const unsigned sourceDepCount = taskDeps.size() - 1;
    SmallVector<Value, 4> sourcePayloads;
    sourcePayloads.reserve(sourceDepCount);
    for (unsigned idx = 0; idx < sourceDepCount; ++idx)
      sourcePayloads.push_back(
          realizeDbInnerPayload(bodyBuilder, loc, body.getArgument(idx)));
    Value targetPayload = realizeDbInnerPayload(
        bodyBuilder, loc, body.getArgument(sourceDepCount));
    int64_t coveringSourceBlockBase = 0;
    if (std::optional<SmallVector<int64_t, 4>> baseCoords =
            staticCoordsFromLinearIndex(coveringSourceLinears.front(),
                                        *sourceDbSizes))
      coveringSourceBlockBase = (*baseCoords)[0];
    if (failed(emitAllToAllBlockCopy(
            bodyBuilder, loc, sourcePayloads, targetPayload, *sourceBlock,
            *targetBlock, *targetCoords, coveringSourceBlockBase, elementType)))
      return allToAll.emitOpError()
             << "cannot emit element-grain copy for all-to-all target block "
             << targetLinear;
    bodyBuilder.setInsertionPointToEnd(&body);
    for (unsigned idx = 0; idx < taskDeps.size(); ++idx)
      DbReleaseOp::create(bodyBuilder, loc, body.getArgument(idx));
    YieldOp::create(bodyBuilder, loc);
  }

  allToAll.erase();
  return success();
}

LogicalResult arts::validateAndCollectStorageRedists(
    ModuleOp module, llvm::DenseMap<Value, HaloRedistFacts> &haloFacts,
    SmallVectorImpl<Operation *> &redists) {
  bool foundError = false;
  auto recordHalo = [&](Operation *op, Value mu, ArrayAttr ownerDims,
                        ArrayAttr blockShape, ArrayAttr haloShape) {
    if (!haloShape) {
      op->emitError() << "commits halo movement without haloShape";
      foundError = true;
      return;
    }
    std::optional<SmallVector<int64_t, 4>> halo = readI64ArrayAttr(haloShape);
    auto memrefType = dyn_cast<MemRefType>(mu.getType());
    if (!halo || !memrefType ||
        halo->size() != static_cast<size_t>(memrefType.getRank()) ||
        llvm::any_of(*halo, [](int64_t value) { return value < 0; })) {
      op->emitError()
          << "commits a halo shape that is not a non-negative rank-length "
             "array";
      foundError = true;
      return;
    }

    HaloRedistFacts facts{ownerDims, blockShape, haloShape};
    auto [it, inserted] = haloFacts.try_emplace(mu, facts);
    if (!inserted && (it->second.ownerDims != facts.ownerDims ||
                      it->second.blockShape != facts.blockShape ||
                      it->second.haloShape != facts.haloShape)) {
      op->emitError()
          << "conflicts with another committed halo movement for the same MU";
      foundError = true;
      return;
    }
    redists.push_back(op);
  };

  module.walk([&](sde::SdeSuHaloOp halo) {
    recordHalo(halo.getOperation(), halo.getMu(), halo.getOwnerDims(),
               halo.getBlockShape(), halo.getHaloShape());
  });

  module.walk(
      [&](sde::SdeSuReduceScatterOp reduce) {
        if (!reduce.getArrayIdAttr()) {
          reduce.emitOpError()
              << "commits reduce-scatter movement without array_id";
          foundError = true;
        }
      });

  module.walk([&](sde::SdeSuAllToAllOp allToAll) {
    if (!allToAll.getArrayIdAttr()) {
      allToAll.emitOpError() << "commits all-to-all movement without array_id";
      foundError = true;
      return;
    }
    auto sourceOwner = readI64ArrayAttr(allToAll.getSourceOwnerDims());
    auto sourceBlock = readI64ArrayAttr(allToAll.getSourceBlockShape());
    auto targetOwner = readI64ArrayAttr(allToAll.getTargetOwnerDims());
    auto targetBlock = readI64ArrayAttr(allToAll.getTargetBlockShape());
    if (!sourceOwner || !sourceBlock || !targetOwner || !targetBlock ||
        sourceOwner->empty() || sourceBlock->empty() || targetOwner->empty() ||
        targetBlock->empty()) {
      allToAll.emitOpError()
          << "commits all-to-all movement without concrete source/target "
             "owner/block geometry";
      foundError = true;
      return;
    }
    if (*sourceOwner == *targetOwner && *sourceBlock == *targetBlock) {
      allToAll.emitOpError()
          << "commits all-to-all movement with identical source and target "
             "geometry";
      foundError = true;
      return;
    }
    if (!dyn_cast_or_null<sde::SdeSuDistributeOp>(allToAll->getParentOp())) {
      allToAll.emitOpError()
          << "must be a direct child of sde.su_distribute before ARTS "
             "realization";
      foundError = true;
    }
  });
  return failure(foundError);
}

LogicalResult arts::realizeAllToAllMovements(ModuleOp module) {
  SmallVector<sde::SdeSuAllToAllOp> movements;
  module.walk([&](sde::SdeSuAllToAllOp op) { movements.push_back(op); });
  for (sde::SdeSuAllToAllOp op : movements)
    if (failed(convertAllToAllMovement(op)))
      return failure();
  return success();
}
