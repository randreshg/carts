///==========================================================================///
/// File: CodirToArtsPerBlockCollectives.h
///
/// Per-block all-gather, reduction settle, and stencil halo emitters.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKCOLLECTIVES_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKCOLLECTIVES_H

#include "CodirToArtsBridgeWorkGroups.h"

namespace {

/// Per-block single-writer all-gather substrate. Each gathered block is a
/// distinct replicated DB written once, while the bridge EDT may group adjacent
/// block copies to amortize launch overhead.
static inline FailureOr<Value>
emitPerBlockAllGatherWriteBack(OpBuilder &builder, Location loc, Value hostView,
                               BridgePlan plan) {
  arts::DbAllocOp producerBlockAlloc = plan.sourceAlloc;
  auto hostType = dyn_cast<MemRefType>(hostView.getType());
  if (!hostType || hostType.getRank() == 0)
    return failure();
  ModuleOp module = producerBlockAlloc->getParentOfType<ModuleOp>();
  if (!module || !arts::hasArtsInterNodeRuntime(module))
    return failure();
  if (producerBlockAlloc.getSizes().empty() ||
      producerBlockAlloc.getElementSizes().size() !=
          static_cast<size_t>(hostType.getRank()))
    return failure();

  // Mirror the producer's block layout for the gathered replica, but mark it
  // REPLICATED (local_only, not distributed) so every node materializes all N
  // blocks locally. Each block keeps its own GUID (createMultiDbs), so the
  // single-writer property is per block.
  OpBuilder::InsertionGuard topGuard(builder);
  Value route = arts::createCurrentNodeRoute(builder, loc);
  SmallVector<Value> outerSizes(producerBlockAlloc.getSizes().begin(),
                                producerBlockAlloc.getSizes().end());
  SmallVector<Value> innerSizes(producerBlockAlloc.getElementSizes().begin(),
                                producerBlockAlloc.getElementSizes().end());
  auto replicaAlloc = arts::DbAllocOp::create(
      builder, loc, arts::ArtsMode::inout, route, arts::DbAllocType::heap,
      arts::DbMode::write, hostType.getElementType(), std::move(outerSizes),
      std::move(innerSizes), arts::PartitionMode::block);
  if (auto ownerDims =
          arts::getPlanOwnerDimsAttr(producerBlockAlloc.getOperation()))
    arts::setPlanOwnerDimsAttr(replicaAlloc.getOperation(), ownerDims);
  if (auto blockShape = arts::getPlanPhysicalBlockShapeAttr(
          producerBlockAlloc.getOperation()))
    arts::setPlanPhysicalBlockShapeAttr(replicaAlloc.getOperation(),
                                        blockShape);
  // Replicated, not distributed: every block is local on every node. The
  // perBlockReplicated marker keeps the distributed-ownership pass from
  // block-scattering the gathered blocks (which would defeat the all-gather);
  // the single-writer property holds per block-GUID either way.
  replicaAlloc.setLocalOnlyAttr(UnitAttr::get(replicaAlloc.getContext()));
  replicaAlloc.setPerBlockReplicatedAttr(
      UnitAttr::get(replicaAlloc.getContext()));

  Value one = createOneIndex(builder, loc);
  Value blockCount =
      materializeProduct(builder, loc, producerBlockAlloc.getSizes());
  SmallVector<Value> unitBlockSizes(producerBlockAlloc.getSizes().size(), one);

  SmallVector<Value> blockElementSizes(
      producerBlockAlloc.getElementSizes().begin(),
      producerBlockAlloc.getElementSizes().end());
  BridgeWorkGroupPlan groupPlan =
      planBridgeWorkGroups(plan, BridgeWorkloadKind::all_gather);
  plan.groupSize = groupPlan.groupSize;
  int64_t blockGroupSize = plan.groupSize;

  // One flattened launch loop covers every (node, block-group) pair. Routing
  // each block copy to the derived node ordinal keeps the gathered write
  // owner-local on each node's replica while every lane still acquires a
  // distinct per-block source/destination DB.
  auto totalNodesI32 = arts::RuntimeQueryOp::create(
      builder, loc, arts::RuntimeQueryKind::totalNodes);
  Value totalNodes = arith::IndexCastOp::create(
      builder, loc, builder.getIndexType(), totalNodesI32.getResult());
  FlatNodeBlockGroupLoop flatLoop = materializeFlatNodeBlockGroupLoop(
      builder, loc, totalNodes, blockCount, blockGroupSize);
  Value nodeOrdinal = flatLoop.nodeOrdinal;
  Value blockBase = flatLoop.blockBase;

  SmallVector<Value> deps;
  deps.reserve(static_cast<size_t>(blockGroupSize) * 2);
  for (int64_t lane = 0; lane < blockGroupSize; ++lane) {
    Value blockIndex = blockBase;
    if (lane != 0) {
      Value laneValue = createConstantIndex(builder, loc, lane);
      blockIndex = arith::AddIOp::create(builder, loc, blockBase, laneValue);
    }
    SmallVector<Value> blockCoords = materializeRowMajorCoordinates(
        builder, loc, blockIndex, producerBlockAlloc.getSizes());
    // Source: producer block, read-only (cross-node RO acquire;
    // PREFER_DUPLICATE is applied downstream because the producer DB is
    // distributed/read).
    auto srcAcquire = materializeBridgeAcquire(
        builder, loc, producerBlockAlloc, arts::ArtsMode::in,
        arts::PartitionMode::block, blockCoords, unitBlockSizes);
    // Destination: this gathered block, output-only. Distinct DB per block ⇒
    // single writer ⇒ no shared EW frontier.
    auto dstAcquire = materializeBridgeAcquire(
        builder, loc, replicaAlloc, arts::ArtsMode::out,
        arts::PartitionMode::block, blockCoords, unitBlockSizes);
    deps.push_back(srcAcquire.getPtr());
    deps.push_back(dstAcquire.getPtr());
  }
  SmallVector<Value> params(blockElementSizes.begin(), blockElementSizes.end());

  arts::ArtsLaunchPolicy launch =
      arts::resolveArtsOrdinalLaunchPolicy(module, nodeOrdinal, builder, loc);
  Value taskRoute =
      launch.route ? launch.route : arts::createCurrentNodeRoute(builder, loc);
  auto copyTask =
      arts::EdtOp::create(builder, loc, arts::EdtType::task, launch.concurrency,
                          taskRoute, deps, params);
  copyTask.setStorageBridgeCopyAttr(UnitAttr::get(copyTask.getContext()));
  copyTask.setPerBlockAllGatherAttr(UnitAttr::get(copyTask.getContext()));
  Block &body = copyTask.getBody().front();
  for (Value dep : deps)
    body.addArgument(dep.getType(), loc);
  for (Value param : params)
    body.addArgument(param.getType(), loc);
  {
    OpBuilder::InsertionGuard bodyGuard(builder);
    builder.setInsertionPointToStart(&body);
    unsigned paramBase = deps.size();
    SmallVector<Value> bodyCopySizes;
    bodyCopySizes.reserve(blockElementSizes.size());
    for (size_t i = 0; i < blockElementSizes.size(); ++i)
      bodyCopySizes.push_back(body.getArgument(paramBase + i));
    for (int64_t lane = 0; lane < blockGroupSize; ++lane) {
      unsigned srcArg = static_cast<unsigned>(lane) * 2;
      unsigned dstArg = srcArg + 1;
      Value srcPayload =
          materializeInnerPayload(builder, loc, body.getArgument(srcArg));
      Value dstPayload =
          materializeInnerPayload(builder, loc, body.getArgument(dstArg));
      SmallVector<Value> indices;
      materializePerBlockCopyNest(builder, loc, srcPayload, dstPayload,
                                  bodyCopySizes, indices);
    }
    arts::YieldOp::create(builder, loc);
  }

  builder.setInsertionPointAfter(flatLoop.loop.getOperation());
  auto reason = arts::ArtsBarrierReasonAttr::get(
      builder.getContext(), arts::ArtsBarrierReason::required_memory);
  arts::BarrierOp::create(builder, loc, reason);

  return materializeInnerPayload(builder, loc, replicaAlloc.getPtr());
}

/// Per-block single-writer summing settle. Each output block is written once
/// from the P per-(block,tile) partial blocks, all delivered as EDT deps.
static inline FailureOr<Value>
emitPerBlockSummingSettle(OpBuilder &builder, Location loc,
                          arts::DbAllocOp partialBlockAlloc, unsigned tileCount,
                          codir::CodeletOp codelet, unsigned depIndex,
                          const BridgePlan &bridgePlan) {
  if (tileCount == 0)
    return failure();
  ModuleOp module = partialBlockAlloc->getParentOfType<ModuleOp>();
  if (!module || !arts::hasArtsInterNodeRuntime(module))
    return failure();
  // The partials DB is a single-axis block DB whose element block carries the
  // settled block's footprint; its outer extent counts blockCount * tileCount
  // distinct per-(block,tile) GUIDs.
  if (partialBlockAlloc.getSizes().size() != 1 ||
      partialBlockAlloc.getElementSizes().empty())
    return failure();

  // Mirror the partials' element-block layout for the settle replica, but mark
  // it REPLICATED (local_only, not distributed) so every node materializes all
  // settled blocks locally. Each block keeps its own GUID (createMultiDbs), so
  // the single-writer property is per settled block.
  OpBuilder::InsertionGuard topGuard(builder);
  Value route = arts::createCurrentNodeRoute(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value tileCountVal = createConstantIndex(builder, loc, tileCount);

  // blockCount = partialCount / tileCount (the partials carry tileCount entries
  // per settled block, contiguous on the flat outer axis).
  Value partialCount = partialBlockAlloc.getSizes().front();
  Value blockCount =
      arith::DivUIOp::create(builder, loc, partialCount, tileCountVal);

  SmallVector<Value> blockElementSizes(
      partialBlockAlloc.getElementSizes().begin(),
      partialBlockAlloc.getElementSizes().end());
  // The DbAllocOp `elementType` is the SCALAR element; the ptr result nests one
  // memref level per (block axis + element rank). Mirror the partials' scalar
  // element type so the settle replica's payload has the same rank as a partial
  // block (and the addf body stores scalars, not nested memrefs).
  Type elementType = partialBlockAlloc.getElementType();

  SmallVector<Value> outerSizes{blockCount};
  SmallVector<Value> innerSizes(blockElementSizes.begin(),
                                blockElementSizes.end());
  auto settleAlloc = arts::DbAllocOp::create(
      builder, loc, arts::ArtsMode::inout, route, arts::DbAllocType::heap,
      arts::DbMode::write, elementType, std::move(outerSizes),
      std::move(innerSizes), arts::PartitionMode::block);
  if (auto ownerDims =
          arts::getPlanOwnerDimsAttr(partialBlockAlloc.getOperation()))
    arts::setPlanOwnerDimsAttr(settleAlloc.getOperation(), ownerDims);
  if (auto blockShape =
          arts::getPlanPhysicalBlockShapeAttr(partialBlockAlloc.getOperation()))
    arts::setPlanPhysicalBlockShapeAttr(settleAlloc.getOperation(), blockShape);
  // Replicated, not distributed: every settled block is local on every node, so
  // the distributed-ownership pass must not block-scatter it (that would defeat
  // the allreduce). The single-writer property holds per block-GUID either way.
  settleAlloc.setLocalOnlyAttr(UnitAttr::get(settleAlloc.getContext()));
  settleAlloc.setPerBlockReplicatedAttr(
      UnitAttr::get(settleAlloc.getContext()));

  // One flattened launch loop covers every (node, block-group) pair. Routing
  // each settle to the derived node ordinal keeps the settled write owner-local
  // on each node's replica while every lane still has its own per-block partial
  // deps and one distinct output block dep.
  auto totalNodesI32 = arts::RuntimeQueryOp::create(
      builder, loc, arts::RuntimeQueryKind::totalNodes);
  Value totalNodes = arith::IndexCastOp::create(
      builder, loc, builder.getIndexType(), totalNodesI32.getResult());

  BridgeWorkGroupPlan groupPlan =
      planBridgeWorkGroups(bridgePlan, BridgeWorkloadKind::summing_settle);
  int64_t blockGroupSize = groupPlan.groupSize;
  FlatNodeBlockGroupLoop flatLoop = materializeFlatNodeBlockGroupLoop(
      builder, loc, totalNodes, blockCount, blockGroupSize);
  Value nodeOrdinal = flatLoop.nodeOrdinal;
  Value blockBase = flatLoop.blockBase;

  // Acquire the P per-(block,tile) partials read-only OUTSIDE the EDT (the
  // EdtLowering ABI forbids GEPing an outer DB alloc from the EDT body). Each
  // grouped lane keeps its own partial deps and distinct output block dep, so
  // DB grain remains per block while launch overhead is amortized.
  SmallVector<Value> deps;
  deps.reserve(static_cast<size_t>(blockGroupSize) *
               static_cast<size_t>(tileCount + 1));
  for (int64_t lane = 0; lane < blockGroupSize; ++lane) {
    Value blockIndex = blockBase;
    if (lane != 0) {
      Value laneValue = createConstantIndex(builder, loc, lane);
      blockIndex = arith::AddIOp::create(builder, loc, blockBase, laneValue);
    }
    Value partialBase =
        arith::MulIOp::create(builder, loc, blockIndex, tileCountVal);
    for (unsigned tile = 0; tile < tileCount; ++tile) {
      Value tileVal = createConstantIndex(builder, loc, tile);
      Value partialIndex =
          arith::AddIOp::create(builder, loc, partialBase, tileVal);
      auto partialAcquire = materializeBridgeAcquire(
          builder, loc, partialBlockAlloc, arts::ArtsMode::in,
          arts::PartitionMode::block, partialIndex, one);
      deps.push_back(partialAcquire.getPtr());
    }
    // Destination: this settled block, output-only. Distinct DB per block
    // means each grouped lane still has exactly one writer.
    auto dstAcquire =
        materializeBridgeAcquire(builder, loc, settleAlloc, arts::ArtsMode::out,
                                 arts::PartitionMode::block, blockIndex, one);
    deps.push_back(dstAcquire.getPtr());
  }

  SmallVector<Value> params(blockElementSizes.begin(), blockElementSizes.end());

  arts::ArtsLaunchPolicy launch =
      arts::resolveArtsOrdinalLaunchPolicy(module, nodeOrdinal, builder, loc);
  Value taskRoute =
      launch.route ? launch.route : arts::createCurrentNodeRoute(builder, loc);
  auto settleTask =
      arts::EdtOp::create(builder, loc, arts::EdtType::task, launch.concurrency,
                          taskRoute, deps, params);
  settleTask.setStorageBridgeCopyAttr(UnitAttr::get(settleTask.getContext()));
  settleTask.setPerBlockSummingSettleAttr(
      UnitAttr::get(settleTask.getContext()));
  Block &body = settleTask.getBody().front();
  for (Value dep : deps)
    body.addArgument(dep.getType(), loc);
  for (Value param : params)
    body.addArgument(param.getType(), loc);
  {
    OpBuilder::InsertionGuard bodyGuard(builder);
    builder.setInsertionPointToStart(&body);
    SmallVector<Value> bodyCopySizes;
    bodyCopySizes.reserve(blockElementSizes.size());
    for (size_t i = 0; i < blockElementSizes.size(); ++i)
      bodyCopySizes.push_back(body.getArgument(deps.size() + i));
    for (int64_t lane = 0; lane < blockGroupSize; ++lane) {
      unsigned laneDepBase =
          static_cast<unsigned>(lane) * static_cast<unsigned>(tileCount + 1);
      SmallVector<Value> partialPayloads;
      partialPayloads.reserve(tileCount);
      for (unsigned tile = 0; tile < tileCount; ++tile)
        partialPayloads.push_back(materializeInnerPayload(
            builder, loc, body.getArgument(laneDepBase + tile)));
      Value dstPayload = materializeInnerPayload(
          builder, loc, body.getArgument(laneDepBase + tileCount));
      SmallVector<Value> indices;
      materializePerBlockSumNest(builder, loc, partialPayloads, dstPayload,
                                 bodyCopySizes, indices);
    }
    arts::YieldOp::create(builder, loc);
  }

  builder.setInsertionPointAfter(flatLoop.loop.getOperation());
  auto reason = arts::ArtsBarrierReasonAttr::get(
      builder.getContext(), arts::ArtsBarrierReason::required_memory);
  arts::BarrierOp::create(builder, loc, reason);

  return materializeInnerPayload(builder, loc, settleAlloc.getPtr());
}

static inline LogicalResult
preparePerBlockSingleWriterStencilDb(arts::DbAllocOp blockAlloc) {
  if (!blockAlloc)
    return failure();
  if (blockAlloc.getSizes().empty() || blockAlloc.getElementSizes().empty())
    return failure();
  blockAlloc.removeLocalOnlyAttr();
  blockAlloc.setPerBlockSingleWriterStencilAttr(
      UnitAttr::get(blockAlloc.getContext()));
  return success();
}

static inline FailureOr<Value> emitPerBlockSingleWriterStencilDb(
    OpBuilder &builder, Location loc, arts::DbAllocOp blockAlloc,
    codir::CodeletOp codelet, unsigned depIndex, ValueRange phaseTokens = {},
    const BridgePlan *bridgePlan = nullptr) {
  if (failed(preparePerBlockSingleWriterStencilDb(blockAlloc)))
    return failure();
  ModuleOp module = blockAlloc->getParentOfType<ModuleOp>();

  OpBuilder::InsertionGuard topGuard(builder);
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockCount = materializeProduct(builder, loc, blockAlloc.getSizes());

  SmallVector<Value> blockElementSizes(blockAlloc.getElementSizes().begin(),
                                       blockAlloc.getElementSizes().end());
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  std::optional<SmallVector<int64_t, 4>> ownerBlockSizes =
      getCodirTileOwnerBlockSizes(
          codelet, depIndex, static_cast<unsigned>(blockElementSizes.size()));
  if (!ownerDims || !ownerBlockSizes ||
      ownerDims->size() != blockAlloc.getSizes().size() ||
      ownerDims->size() != ownerBlockSizes->size())
    return failure();

  SmallVector<CodirOwnerHaloWindow, 4> ownerHalos =
      getCodirBlockStorageHaloWindows(
          codelet, depIndex, static_cast<unsigned>(blockElementSizes.size()));
  if (ownerHalos.size() != ownerDims->size())
    return failure();

  BridgeWorkGroupPlan groupPlan;
  if (bridgePlan)
    groupPlan = planBridgeWorkGroups(*bridgePlan, BridgeWorkloadKind::halo);
  int64_t blockGroupSize = std::max<int64_t>(1, groupPlan.groupSize);
  Value blockStep = createConstantIndex(builder, loc, blockGroupSize);

  auto blockLoop =
      scf::ForOp::create(builder, loc, zero, blockCount, blockStep);
  builder.setInsertionPointToStart(blockLoop.getBody());
  Value blockBase = blockLoop.getInductionVar();
  struct HaloCopyAction {
    unsigned dstArg = 0;
    unsigned depArg = 0;
    unsigned ownerSlot = 0;
    bool lower = true;
    int64_t width = 0;
    unsigned conditionParam = 0;
    bool compactSource = false;
  };
  struct HaloLanePlan {
    SmallVector<Value, 4> blockCoords;
    SmallVector<Value, 4> blockWindowSizes;
    unsigned dstArg = 0;
  };
  struct HaloSlicePlan {
    SmallVector<Value, 4> elementOffsets;
    SmallVector<Value, 4> elementSizes;
    bool compact = false;
  };
  SmallVector<HaloCopyAction, 8> copyActions;
  SmallVector<Value, 8> actionConditions;
  SmallVector<Value> deps;
  deps.reserve(static_cast<size_t>(blockGroupSize) *
               (1 + ownerHalos.size() * 2));
  SmallVector<HaloLanePlan, 8> lanes;
  lanes.reserve(static_cast<size_t>(blockGroupSize));
  SmallVector<int64_t, 4> blockStrides =
      getStaticRowMajorBlockStrides(blockAlloc);
  for (int64_t lane = 0; lane < blockGroupSize; ++lane) {
    Value blockOrdinal = blockBase;
    if (lane != 0) {
      Value laneValue = createConstantIndex(builder, loc, lane);
      blockOrdinal = arith::AddIOp::create(builder, loc, blockBase, laneValue);
    }
    SmallVector<Value> blockCoords = materializeRowMajorCoordinates(
        builder, loc, blockOrdinal, blockAlloc.getSizes());
    SmallVector<Value> blockWindowSizes(blockCoords.size(), one);

    auto dstAcquire = materializeBridgeAcquire(
        builder, loc, blockAlloc, arts::ArtsMode::out,
        arts::PartitionMode::block, blockCoords, blockWindowSizes);
    dstAcquire.setPreserveAccessMode();
    unsigned dstArg = static_cast<unsigned>(deps.size());
    deps.push_back(dstAcquire.getPtr());

    lanes.push_back(
        {std::move(blockCoords), std::move(blockWindowSizes), dstArg});
  }

  auto getInGroupSourceArg = [&](int64_t lane, unsigned slot,
                                 bool lower) -> std::optional<unsigned> {
    if (blockGroupSize <= 1 || slot >= blockStrides.size())
      return std::nullopt;
    int64_t stride = blockStrides[slot];
    if (stride <= 0)
      return std::nullopt;
    int64_t sourceLane = lower ? lane - stride : lane + stride;
    if (sourceLane < 0 || sourceLane >= static_cast<int64_t>(lanes.size()))
      return std::nullopt;
    return lanes[static_cast<size_t>(sourceLane)].dstArg;
  };
  std::optional<SmallVector<int64_t, 4>> staticElementSizes =
      getStaticElementSizes(blockAlloc);
  auto buildSourceSlicePlan = [&](unsigned actionSlot, bool lower,
                                  int64_t width) {
    HaloSlicePlan slice;
    if (!staticElementSizes)
      return slice;
    SmallVector<int64_t, 4> staticOffsets(staticElementSizes->size(), 0);
    SmallVector<int64_t, 4> staticSizes(staticElementSizes->begin(),
                                        staticElementSizes->end());
    for (auto [slot, halo] : llvm::enumerate(ownerHalos)) {
      unsigned ownerDim = (*ownerDims)[slot];
      if (ownerDim >= staticSizes.size())
        return HaloSlicePlan{};
      int64_t blockSize = (*ownerBlockSizes)[slot];
      if (slot == actionSlot) {
        staticSizes[ownerDim] = width;
        staticOffsets[ownerDim] =
            lower ? halo.lower + blockSize - width : halo.lower;
      } else {
        staticSizes[ownerDim] = blockSize;
        staticOffsets[ownerDim] = halo.lower;
      }
    }
    if (!isStaticContiguousElementSlice(staticOffsets, staticSizes,
                                        *staticElementSizes))
      return slice;
    slice.compact = true;
    slice.elementOffsets.reserve(staticOffsets.size());
    slice.elementSizes.reserve(staticSizes.size());
    for (int64_t offset : staticOffsets)
      slice.elementOffsets.push_back(createConstantIndex(builder, loc, offset));
    for (int64_t size : staticSizes)
      slice.elementSizes.push_back(createConstantIndex(builder, loc, size));
    return slice;
  };

  for (auto [lane, lanePlan] : llvm::enumerate(lanes)) {
    for (auto [slot, coord] : llvm::enumerate(lanePlan.blockCoords)) {
      CodirOwnerHaloWindow halo = ownerHalos[slot];
      Value lastCoord =
          arith::SubIOp::create(builder, loc, blockAlloc.getSizes()[slot], one);
      Value lowerRaw = arith::SubIOp::create(builder, loc, coord, one);
      Value hasLower = arith::CmpIOp::create(
          builder, loc, arith::CmpIPredicate::ugt, coord, zero);
      Value lowerCoord =
          arith::SelectOp::create(builder, loc, hasLower, lowerRaw, zero);
      Value upperRaw = arith::AddIOp::create(builder, loc, coord, one);
      Value hasUpper = arith::CmpIOp::create(
          builder, loc, arith::CmpIPredicate::ult, coord, lastCoord);
      Value upperCoord =
          arith::MinUIOp::create(builder, loc, upperRaw, lastCoord);

      SmallVector<Value> lowerCoords(lanePlan.blockCoords.begin(),
                                     lanePlan.blockCoords.end());
      SmallVector<Value> upperCoords(lanePlan.blockCoords.begin(),
                                     lanePlan.blockCoords.end());
      lowerCoords[slot] = lowerCoord;
      upperCoords[slot] = upperCoord;
      if (halo.lower > 0) {
        std::optional<unsigned> sourceArg = getInGroupSourceArg(
            static_cast<int64_t>(lane), static_cast<unsigned>(slot), true);
        bool compactSource = false;
        if (!sourceArg) {
          HaloSlicePlan sourceSlice = buildSourceSlicePlan(
              static_cast<unsigned>(slot), true, halo.lower);
          auto lowerAcquire = materializeBridgeAcquire(
              builder, loc, blockAlloc, arts::ArtsMode::in,
              arts::PartitionMode::block, lowerCoords,
              lanePlan.blockWindowSizes, hasLower, sourceSlice.elementOffsets,
              sourceSlice.elementSizes);
          compactSource = sourceSlice.compact;
          sourceArg = static_cast<unsigned>(deps.size());
          deps.push_back(lowerAcquire.getPtr());
        }
        unsigned conditionParam = actionConditions.size();
        actionConditions.push_back(hasLower);
        copyActions.push_back({lanePlan.dstArg, *sourceArg,
                               static_cast<unsigned>(slot), true, halo.lower,
                               conditionParam, compactSource});
      }
      if (halo.upper > 0) {
        std::optional<unsigned> sourceArg = getInGroupSourceArg(
            static_cast<int64_t>(lane), static_cast<unsigned>(slot), false);
        bool compactSource = false;
        if (!sourceArg) {
          HaloSlicePlan sourceSlice = buildSourceSlicePlan(
              static_cast<unsigned>(slot), false, halo.upper);
          auto upperAcquire = materializeBridgeAcquire(
              builder, loc, blockAlloc, arts::ArtsMode::in,
              arts::PartitionMode::block, upperCoords,
              lanePlan.blockWindowSizes, hasUpper, sourceSlice.elementOffsets,
              sourceSlice.elementSizes);
          compactSource = sourceSlice.compact;
          sourceArg = static_cast<unsigned>(deps.size());
          deps.push_back(upperAcquire.getPtr());
        }
        unsigned conditionParam = actionConditions.size();
        actionConditions.push_back(hasUpper);
        copyActions.push_back({lanePlan.dstArg, *sourceArg,
                               static_cast<unsigned>(slot), false, halo.upper,
                               conditionParam, compactSource});
      }
    }
  }
  SmallVector<Value> params(actionConditions.begin(), actionConditions.end());
  params.append(blockElementSizes.begin(), blockElementSizes.end());
  params.append(phaseTokens.begin(), phaseTokens.end());

  arts::ArtsLaunchPolicy launch = resolveBridgeBlockOrdinalLaunchPolicy(
      module, bridgePlan, blockAlloc, blockBase, builder, loc);
  Value taskRoute =
      launch.route ? launch.route : arts::createCurrentNodeRoute(builder, loc);
  auto haloTask =
      arts::EdtOp::create(builder, loc, arts::EdtType::task, launch.concurrency,
                          taskRoute, deps, params);
  haloTask.setStorageBridgeCopyAttr(UnitAttr::get(haloTask.getContext()));
  haloTask.setPerBlockHaloExchangeAttr(UnitAttr::get(haloTask.getContext()));
  Block &body = haloTask.getBody().front();
  for (Value dep : deps)
    body.addArgument(dep.getType(), loc);
  for (Value param : params)
    body.addArgument(param.getType(), loc);
  {
    OpBuilder::InsertionGuard bodyGuard(builder);
    builder.setInsertionPointToStart(&body);
    SmallVector<Value> bodyCopySizes;
    bodyCopySizes.reserve(blockElementSizes.size());
    unsigned sizeParamBase =
        static_cast<unsigned>(deps.size() + actionConditions.size());
    for (size_t i = 0; i < blockElementSizes.size(); ++i)
      bodyCopySizes.push_back(body.getArgument(sizeParamBase + i));
    for (const HaloCopyAction &action : copyActions) {
      Value condition = body.getArgument(deps.size() + action.conditionParam);
      auto copyIf = scf::IfOp::create(builder, loc, TypeRange{}, condition,
                                      /*withElseRegion=*/false);
      OpBuilder::InsertionGuard ifGuard(builder);
      builder.setInsertionPointToStart(&copyIf.getThenRegion().front());
      Value dstPayload = materializeInnerPayload(
          builder, loc, body.getArgument(action.dstArg));
      Value srcPayload = materializeInnerPayload(
          builder, loc, body.getArgument(action.depArg));
      SmallVector<Value> copySizes(bodyCopySizes.begin(), bodyCopySizes.end());
      SmallVector<Value> srcOffsets(blockElementSizes.size(),
                                    createZeroIndex(builder, loc));
      SmallVector<Value> dstOffsets(blockElementSizes.size(),
                                    createZeroIndex(builder, loc));
      for (auto [slot, halo] : llvm::enumerate(ownerHalos)) {
        unsigned ownerDim = (*ownerDims)[slot];
        if (ownerDim >= copySizes.size())
          continue;
        int64_t blockSize = (*ownerBlockSizes)[slot];
        Value blockSizeValue = createConstantIndex(builder, loc, blockSize);
        if (slot == action.ownerSlot) {
          copySizes[ownerDim] = createConstantIndex(builder, loc, action.width);
          int64_t srcStart =
              action.lower ? halo.lower + blockSize - action.width : halo.lower;
          int64_t dstStart = action.lower ? 0 : halo.lower + blockSize;
          if (!action.compactSource)
            srcOffsets[ownerDim] = createConstantIndex(builder, loc, srcStart);
          dstOffsets[ownerDim] = createConstantIndex(builder, loc, dstStart);
        } else {
          copySizes[ownerDim] = blockSizeValue;
          Value haloLower = createConstantIndex(builder, loc, halo.lower);
          if (!action.compactSource)
            srcOffsets[ownerDim] = haloLower;
          dstOffsets[ownerDim] = haloLower;
        }
      }
      SmallVector<Value> indices;
      materializePerBlockOffsetCopyNest(builder, loc, srcPayload, dstPayload,
                                        copySizes, srcOffsets, dstOffsets,
                                        indices);
    }
    arts::YieldOp::create(builder, loc);
  }

  builder.setInsertionPointAfter(blockLoop);
  auto reason = arts::ArtsBarrierReasonAttr::get(
      builder.getContext(), arts::ArtsBarrierReason::required_memory);
  arts::BarrierOp::create(builder, loc, reason);

  return materializeInnerPayload(builder, loc, blockAlloc.getPtr());
}

static inline Operation *findCodirOwnerDispatchAnchor(codir::CodeletOp codelet,
                                                      unsigned depIndex) {
  SmallVector<Value, 4> ownerParams =
      getCodirDepOwnerParamValues(codelet, depIndex);
  Operation *anchor = nullptr;
  bool matched = false;
  for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (!loop) {
      if (matched)
        break;
      continue;
    }
    if (containsValue(ownerParams, loop.getInductionVar())) {
      anchor = parent;
      matched = true;
      continue;
    }
    if (matched)
      break;
  }
  return anchor ? anchor : findCodirDispatchBridgeAnchor(codelet);
}

static inline SmallVector<Value, 4>
collectEnclosingControlTokens(Operation *op) {
  SmallVector<Value, 4> tokens;
  for (Operation *parent = op ? op->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    if (auto loop = dyn_cast<scf::ForOp>(parent)) {
      tokens.push_back(loop.getInductionVar());
      continue;
    }
    if (auto ifOp = dyn_cast<scf::IfOp>(parent))
      tokens.push_back(ifOp.getCondition());
  }
  return tokens;
}

static inline bool
isHaloReadParticipant(const HostBridgeParticipant &participant) {
  return codirAccessMayRead(participant.mode) &&
         codirDepUsesHaloStencilStorage(participant.codelet,
                                        participant.depIndex);
}

static inline LogicalResult emitPerBlockStencilHaloBeforeReadPhases(
    OpBuilder &builder, Location loc, arts::DbAllocOp blockAlloc,
    ArrayRef<HostBridgeParticipant> participants,
    const BridgePlan *bridgePlan = nullptr) {
  SmallVector<Operation *, 4> emittedAnchors;
  for (const HostBridgeParticipant &participant : participants) {
    if (!isHaloReadParticipant(participant))
      continue;
    Operation *dispatchAnchor =
        findCodirOwnerDispatchAnchor(participant.codelet, participant.depIndex);
    if (!dispatchAnchor)
      return failure();
    if (llvm::is_contained(emittedAnchors, dispatchAnchor))
      continue;
    emittedAnchors.push_back(dispatchAnchor);

    builder.setInsertionPoint(dispatchAnchor);
    SmallVector<Value, 4> phaseTokens =
        collectEnclosingControlTokens(dispatchAnchor);
    if (failed(emitPerBlockSingleWriterStencilDb(
            builder, loc, blockAlloc, participant.codelet, participant.depIndex,
            phaseTokens, bridgePlan)))
      return failure();
  }
  return success();
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKCOLLECTIVES_H
