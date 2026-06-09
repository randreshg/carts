///==========================================================================///
/// File: CodirToArtsPerBlockStencilDb.h
///
/// Per-block single-writer stencil DB and halo-exchange materialization.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKSTENCILDB_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKSTENCILDB_H

#include "CodirToArtsPerBlockSummingSettle.h"

namespace {

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
                                  int64_t width) -> FailureOr<HaloSlicePlan> {
    HaloSlicePlan slice;
    if (!staticElementSizes)
      return codelet.emitOpError()
             << "cannot materialize dynamic " << (lower ? "lower" : "upper")
             << " halo destination slice";
    SmallVector<int64_t, 4> staticOffsets(staticElementSizes->size(), 0);
    SmallVector<int64_t, 4> staticSizes(staticElementSizes->begin(),
                                        staticElementSizes->end());
    for (auto [slot, halo] : llvm::enumerate(ownerHalos)) {
      unsigned ownerDim = (*ownerDims)[slot];
      if (ownerDim >= staticSizes.size())
        return codelet.emitOpError()
               << "cannot materialize non-contiguous "
               << (lower ? "lower" : "upper") << " halo destination slice";
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
      return codelet.emitOpError()
             << "cannot materialize non-contiguous "
             << (lower ? "lower" : "upper") << " halo destination slice";
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
          FailureOr<HaloSlicePlan> sourceSlice = buildSourceSlicePlan(
              static_cast<unsigned>(slot), true, halo.lower);
          if (failed(sourceSlice))
            return failure();
          auto lowerAcquire = materializeBridgeAcquire(
              builder, loc, blockAlloc, arts::ArtsMode::in,
              arts::PartitionMode::block, lowerCoords,
              lanePlan.blockWindowSizes, hasLower, sourceSlice->elementOffsets,
              sourceSlice->elementSizes);
          compactSource = sourceSlice->compact;
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
          FailureOr<HaloSlicePlan> sourceSlice = buildSourceSlicePlan(
              static_cast<unsigned>(slot), false, halo.upper);
          if (failed(sourceSlice))
            return failure();
          auto upperAcquire = materializeBridgeAcquire(
              builder, loc, blockAlloc, arts::ArtsMode::in,
              arts::PartitionMode::block, upperCoords,
              lanePlan.blockWindowSizes, hasUpper, sourceSlice->elementOffsets,
              sourceSlice->elementSizes);
          compactSource = sourceSlice->compact;
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

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKSTENCILDB_H
