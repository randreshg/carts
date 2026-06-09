///==========================================================================///
/// File: CodirToArtsHostBlockCopyMaterialization.h
///
/// Host-whole to block copy-loop materialization.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBLOCKCOPYMATERIALIZATION_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBLOCKCOPYMATERIALIZATION_H

#include "CodirToArtsBridgeAcquireUtils.h"

namespace {

static inline LogicalResult
materializeHostBlockCopyLoop(OpBuilder &builder, Location loc, Value hostView,
                             arts::DbAllocOp blockAlloc,
                             codir::CodeletOp codelet, unsigned depIndex,
                             bool copyIntoBlock, bool crossNodeGather = false,
                             const BridgePlan *bridgePlan = nullptr) {
  auto hostType = dyn_cast<MemRefType>(hostView.getType());
  if (!hostType || hostType.getRank() == 0)
    return failure();
  arts::DbAllocOp hostAlloc = findBackingDbAlloc(hostView);
  if (!hostAlloc)
    return failure();

  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty())
    return failure();
  for (unsigned ownerDim : *ownerDims)
    if (ownerDim >= static_cast<unsigned>(hostType.getRank()))
      return failure();
  if (blockAlloc.getSizes().size() != ownerDims->size() ||
      blockAlloc.getElementSizes().size() !=
          static_cast<size_t>(hostType.getRank()))
    return failure();

  SmallVector<Value> logicalSizes =
      getBridgeLogicalElementSizes(builder, loc, hostView);
  if (logicalSizes.size() != static_cast<size_t>(hostType.getRank()))
    return failure();

  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockCount = materializeProduct(builder, loc, blockAlloc.getSizes());
  std::optional<SmallVector<int64_t, 4>> plannedBlockSizes =
      getCodirTileOwnerBlockSizes(codelet, depIndex,
                                  static_cast<unsigned>(hostType.getRank()));
  SmallVector<CodirOwnerHaloWindow, 4> ownerHalos =
      getCodirBlockStorageHaloWindows(
          codelet, depIndex, static_cast<unsigned>(hostType.getRank()));
  // Keep reload/write-back offsets consistent with the DB's storage halo.
  for (CodirOwnerHaloWindow &window : ownerHalos)
    if (window.lower <= 0) {
      CodirOwnerHaloWindow allocHalo =
          blockAllocStorageHaloForDim(blockAlloc, window.ownerDim);
      if (allocHalo.lower > 0)
        window = allocHalo;
    }
  SmallVector<Value, 4> ownerParams =
      getCodirDepOwnerParamValues(codelet, depIndex);

  bool gatherAcrossNodes = crossNodeGather && !copyIntoBlock;
  BridgeWorkGroupPlan groupPlan;
  if (bridgePlan) {
    BridgeWorkloadKind workloadKind = copyIntoBlock
                                          ? BridgeWorkloadKind::host_to_block
                                          : BridgeWorkloadKind::block_to_host;
    groupPlan =
        planBridgeWorkGroups(*bridgePlan, workloadKind, gatherAcrossNodes);
  }
  int64_t blockGroupSize = std::max<int64_t>(1, groupPlan.groupSize);
  Value blockStep = createConstantIndex(builder, loc, blockGroupSize);

  OpBuilder::InsertionGuard guard(builder);
  Value gatherNodeOrdinal;
  scf::ForOp loop;
  Value blockBase;
  if (gatherAcrossNodes) {
    auto totalNodesI32 = arts::RuntimeQueryOp::create(
        builder, loc, arts::RuntimeQueryKind::totalNodes);
    Value totalNodes = arith::IndexCastOp::create(
        builder, loc, builder.getIndexType(), totalNodesI32.getResult());
    FlatNodeBlockGroupLoop flatLoop = materializeFlatNodeBlockGroupLoop(
        builder, loc, totalNodes, blockCount, blockGroupSize);
    loop = flatLoop.loop;
    gatherNodeOrdinal = flatLoop.nodeOrdinal;
    blockBase = flatLoop.blockBase;
  } else {
    loop = scf::ForOp::create(builder, loc, zero, blockCount, blockStep);
    builder.setInsertionPointToStart(loop.getBody());
    blockBase = loop.getInductionVar();
  }

  struct CopyLane {
    SmallVector<Value> blockCoords;
    SmallVector<Value> hostOffsets;
    SmallVector<Value> blockOffsets;
    SmallVector<Value> copySizes;
  };
  SmallVector<CopyLane, 8> lanes;
  lanes.reserve(static_cast<size_t>(blockGroupSize));
  for (int64_t lane = 0; lane < blockGroupSize; ++lane) {
    Value blockOrdinal = blockBase;
    if (lane != 0) {
      Value laneValue = createConstantIndex(builder, loc, lane);
      blockOrdinal = arith::AddIOp::create(builder, loc, blockBase, laneValue);
    }
    CopyLane lanePlan;
    lanePlan.blockCoords = materializeRowMajorCoordinates(
        builder, loc, blockOrdinal, blockAlloc.getSizes());
    lanePlan.hostOffsets.assign(static_cast<size_t>(hostType.getRank()), zero);
    lanePlan.blockOffsets.assign(static_cast<size_t>(hostType.getRank()), zero);
    lanePlan.copySizes.assign(logicalSizes.begin(), logicalSizes.end());
    for (auto [slot, ownerDim] : llvm::enumerate(*ownerDims)) {
      Value ownerBlockSize =
          plannedBlockSizes && slot < plannedBlockSizes->size()
              ? createConstantIndex(builder, loc, (*plannedBlockSizes)[slot])
              : blockAlloc.getElementSizes()[ownerDim];
      Value domainBase =
          slot < ownerParams.size()
              ? materializeCodirOwnerDomainBase(builder, loc, codelet,
                                                ownerParams[slot])
              : materializeCodirOwnerDomainBase(builder, loc, codelet);
      Value ownerBlockOffset = arith::MulIOp::create(
          builder, loc, lanePlan.blockCoords[slot], ownerBlockSize);
      Value ownerOffset =
          arith::AddIOp::create(builder, loc, domainBase, ownerBlockOffset);
      CodirOwnerHaloWindow ownerHalo;
      for (CodirOwnerHaloWindow candidate : ownerHalos) {
        if (candidate.ownerDim == ownerDim) {
          ownerHalo = candidate;
          break;
        }
      }
      Value ownerCopyStart =
          copyIntoBlock
              ? subtractClampZero(builder, loc, ownerOffset, ownerHalo.lower)
              : ownerOffset;
      Value storageOrigin =
          subtractStorageHalo(builder, loc, ownerOffset, ownerHalo.lower);
      Value requestedEnd =
          arith::AddIOp::create(builder, loc, ownerOffset, ownerBlockSize);
      if (copyIntoBlock && ownerHalo.upper > 0)
        requestedEnd = arith::AddIOp::create(
            builder, loc, requestedEnd,
            createConstantIndex(builder, loc, ownerHalo.upper));
      Value ownerCopyEnd = arith::MinUIOp::create(builder, loc, requestedEnd,
                                                  logicalSizes[ownerDim]);
      lanePlan.hostOffsets[ownerDim] = ownerCopyStart;
      if (ownerHalo.lower > 0) {
        Value blockCopyStart = copyIntoBlock ? ownerCopyStart : ownerOffset;
        lanePlan.blockOffsets[ownerDim] =
            arith::SubIOp::create(builder, loc, blockCopyStart, storageOrigin);
      }
      lanePlan.copySizes[ownerDim] = materializePositiveDifferenceOrZero(
          builder, loc, ownerCopyEnd, ownerCopyStart);
    }
    lanes.push_back(std::move(lanePlan));
  }

  arts::ArtsMode hostMode =
      copyIntoBlock ? arts::ArtsMode::in : arts::ArtsMode::inout;
  arts::ArtsMode blockMode =
      copyIntoBlock ? arts::ArtsMode::out : arts::ArtsMode::in;
  auto hostAcquire =
      materializeBridgeAcquire(builder, loc, hostAlloc, hostMode,
                               arts::PartitionMode::coarse, zero, one);
  SmallVector<Value> deps{hostAcquire.getPtr()};
  deps.reserve(1 + lanes.size());
  for (const CopyLane &lane : lanes) {
    SmallVector<Value> blockWindowSizes(lane.blockCoords.size(), one);
    auto blockAcquire = materializeBridgeAcquire(
        builder, loc, blockAlloc, blockMode, arts::PartitionMode::block,
        lane.blockCoords, blockWindowSizes);
    deps.push_back(blockAcquire.getPtr());
  }

  SmallVector<Value> params;
  unsigned rank = static_cast<unsigned>(hostType.getRank());
  params.reserve(lanes.size() * rank * 3);
  for (const CopyLane &lane : lanes) {
    params.append(lane.hostOffsets.begin(), lane.hostOffsets.end());
    params.append(lane.blockOffsets.begin(), lane.blockOffsets.end());
    params.append(lane.copySizes.begin(), lane.copySizes.end());
  }

  arts::ArtsLaunchPolicy launch;
  if (copyIntoBlock)
    launch = resolveBridgeBlockOrdinalLaunchPolicy(
        blockAlloc->getParentOfType<ModuleOp>(), bridgePlan, blockAlloc,
        blockBase, builder, loc);
  else if (gatherAcrossNodes)
    launch = arts::resolveArtsOrdinalLaunchPolicy(
        blockAlloc->getParentOfType<ModuleOp>(), gatherNodeOrdinal, builder,
        loc);
  Value route =
      launch.route ? launch.route : arts::createCurrentNodeRoute(builder, loc);
  auto copyTask = arts::EdtOp::create(builder, loc, arts::EdtType::task,
                                      launch.concurrency, route, deps, params);
  copyTask.setStorageBridgeCopyAttr(UnitAttr::get(copyTask.getContext()));
  Block &body = copyTask.getBody().front();
  for (Value dep : deps)
    body.addArgument(dep.getType(), loc);
  for (Value param : params)
    body.addArgument(param.getType(), loc);

  {
    OpBuilder::InsertionGuard bodyGuard(builder);
    builder.setInsertionPointToStart(&body);
    Value hostPayload =
        materializeInnerPayload(builder, loc, body.getArgument(0));
    unsigned paramBase = deps.size();
    for (int64_t lane = 0; lane < blockGroupSize; ++lane) {
      Value blockPayload =
          materializeInnerPayload(builder, loc, body.getArgument(1 + lane));
      unsigned laneParamBase =
          paramBase + static_cast<unsigned>(lane) * rank * 3;
      SmallVector<Value> bodyHostOffsets;
      bodyHostOffsets.reserve(rank);
      for (unsigned i = 0; i < rank; ++i)
        bodyHostOffsets.push_back(body.getArgument(laneParamBase + i));
      SmallVector<Value> bodyBlockOffsets;
      bodyBlockOffsets.reserve(rank);
      for (unsigned i = 0; i < rank; ++i)
        bodyBlockOffsets.push_back(body.getArgument(laneParamBase + rank + i));
      SmallVector<Value> bodyCopySizes;
      bodyCopySizes.reserve(rank);
      for (unsigned i = 0; i < rank; ++i)
        bodyCopySizes.push_back(body.getArgument(laneParamBase + rank * 2 + i));
      SmallVector<Value> indices;
      materializeHostBlockElementCopyNest(
          builder, loc, hostPayload, blockPayload, bodyCopySizes,
          bodyHostOffsets, bodyBlockOffsets, copyIntoBlock, indices);
    }
    arts::YieldOp::create(builder, loc);
  }

  if (!copyIntoBlock) {
    builder.setInsertionPointAfter(loop.getOperation());
    auto reason = arts::ArtsBarrierReasonAttr::get(
        builder.getContext(), arts::ArtsBarrierReason::required_memory);
    arts::BarrierOp::create(builder, loc, reason);
  }
  return success();
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBLOCKCOPYMATERIALIZATION_H
