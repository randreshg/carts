///==========================================================================///
/// File: CodirToArtsHostBlockCopy.h
///
/// Host-whole to block copy-loop materialization helpers.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBLOCKCOPY_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBLOCKCOPY_H

#include "CodirToArtsHostBridgePlanning.h"

namespace {

static inline void materializeHostBlockElementCopyNest(
    OpBuilder &builder, Location loc, Value hostView, Value blockPayload,
    ArrayRef<Value> copySizes, ArrayRef<Value> hostOffsets,
    ArrayRef<Value> blockOffsets, bool copyIntoBlock,
    SmallVectorImpl<Value> &indices) {
  unsigned dim = indices.size();
  if (dim == copySizes.size()) {
    SmallVector<Value> hostIndices;
    SmallVector<Value> blockIndices;
    hostIndices.reserve(indices.size());
    blockIndices.reserve(indices.size());
    for (auto [idx, value] : llvm::enumerate(indices)) {
      if (idx < blockOffsets.size() && blockOffsets[idx]) {
        blockIndices.push_back(
            arith::AddIOp::create(builder, loc, blockOffsets[idx], value));
      } else {
        blockIndices.push_back(value);
      }
      if (idx < hostOffsets.size() && hostOffsets[idx]) {
        hostIndices.push_back(
            arith::AddIOp::create(builder, loc, hostOffsets[idx], value));
        continue;
      }
      hostIndices.push_back(value);
    }

    if (copyIntoBlock) {
      Value loaded =
          memref::LoadOp::create(builder, loc, hostView, hostIndices);
      memref::StoreOp::create(builder, loc, loaded, blockPayload, blockIndices);
      return;
    }
    Value loaded =
        memref::LoadOp::create(builder, loc, blockPayload, blockIndices);
    memref::StoreOp::create(builder, loc, loaded, hostView, hostIndices);
    return;
  }

  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  auto loop = scf::ForOp::create(builder, loc, zero, copySizes[dim], one);

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(loop.getBody());
  indices.push_back(loop.getInductionVar());
  materializeHostBlockElementCopyNest(builder, loc, hostView, blockPayload,
                                      copySizes, hostOffsets, blockOffsets,
                                      copyIntoBlock, indices);
  indices.pop_back();
}

static inline FailureOr<Value>
materializeCoarseHostDbForBlockArgument(OpBuilder &builder, Location loc,
                                        BlockArgument blockArg) {
  Value root = blockArg;
  auto memrefType = dyn_cast<MemRefType>(root.getType());
  if (!memrefType)
    return failure();

  Block *owner = blockArg.getOwner();
  if (!owner)
    return failure();

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(owner);

  SmallVector<Value> elementSizes;
  if (memrefType.getRank() == 0) {
    elementSizes.push_back(createOneIndex(builder, loc));
  } else {
    elementSizes.reserve(memrefType.getRank());
    for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim) {
      if (memrefType.isDynamicDim(dim)) {
        elementSizes.push_back(memref::DimOp::create(builder, loc, root, dim));
        continue;
      }
      elementSizes.push_back(
          createConstantIndex(builder, loc, memrefType.getDimSize(dim)));
    }
  }

  SmallVector<Operation *> protectedOps;
  for (Value size : elementSizes)
    if (Operation *op = size.getDefiningOp())
      protectedOps.push_back(op);

  Value route = arts::createCurrentNodeRoute(builder, loc);
  auto dbAlloc = arts::DbAllocOp::create(
      builder, loc, arts::ArtsMode::inout, route, arts::DbAllocType::unknown,
      arts::DbMode::write, memrefType.getElementType(), root,
      SmallVector<Value>{createOneIndex(builder, loc)}, std::move(elementSizes),
      arts::PartitionMode::coarse);
  protectedOps.push_back(dbAlloc.getOperation());

  Value replacement = materializeInnerPayload(builder, loc, dbAlloc.getPtr());
  root.replaceUsesWithIf(replacement, [&](OpOperand &use) {
    return !llvm::is_contained(protectedOps, use.getOwner());
  });
  return replacement;
}

static inline FailureOr<Value>
materializeCoarseHostDbForHostBridge(OpBuilder &builder, Location loc,
                                     Value hostView) {
  if (!hostView)
    return failure();
  if (findBackingDbAlloc(hostView))
    return hostView;

  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(hostView);
  if (root != hostView)
    return failure();

  auto memrefType = dyn_cast<MemRefType>(root.getType());
  if (!memrefType)
    return failure();

  if (auto blockArg = dyn_cast<BlockArgument>(root))
    return materializeCoarseHostDbForBlockArgument(builder, loc, blockArg);

  Operation *def = root.getDefiningOp();
  if (!def)
    return failure();

  SmallVector<Value> dynamicSizes;
  OpBuilder::InsertionGuard guard(builder);
  if (auto alloc = dyn_cast<memref::AllocOp>(def)) {
    dynamicSizes.assign(alloc.getDynamicSizes().begin(),
                        alloc.getDynamicSizes().end());
    builder.setInsertionPointAfter(alloc);
  } else if (auto alloca = dyn_cast<memref::AllocaOp>(def)) {
    dynamicSizes.assign(alloca.getDynamicSizes().begin(),
                        alloca.getDynamicSizes().end());
    builder.setInsertionPointAfter(alloca);
  } else if (auto muAlloc = dyn_cast<sde::SdeMuAllocOp>(def)) {
    dynamicSizes.assign(muAlloc.getDynamicSizes().begin(),
                        muAlloc.getDynamicSizes().end());
    builder.setInsertionPointAfter(muAlloc);
  } else {
    return failure();
  }

  SmallVector<memref::DeallocOp> deallocs;
  for (Operation *user : llvm::make_early_inc_range(root.getUsers())) {
    auto dealloc = dyn_cast<memref::DeallocOp>(user);
    if (dealloc && dealloc.getMemref() == root)
      deallocs.push_back(dealloc);
  }

  Value replacement;
  if (failed(createDbBackedMemref(builder, root.getLoc(), memrefType,
                                  dynamicSizes, replacement)))
    return failure();

  root.replaceAllUsesWith(replacement);
  for (memref::DeallocOp dealloc : deallocs)
    dealloc.erase();
  if (def->use_empty())
    def->erase();
  return replacement;
}

static inline arts::DbAcquireOp materializeBridgeAcquire(
    OpBuilder &builder, Location loc, arts::DbAllocOp alloc,
    arts::ArtsMode mode, arts::PartitionMode partitionMode,
    ArrayRef<Value> offsets, ArrayRef<Value> sizes, Value boundsValid = Value{},
    ArrayRef<Value> elementOffsets = {}, ArrayRef<Value> elementSizes = {}) {
  return arts::DbAcquireOp::create(
      builder, loc, mode, alloc.getGuid(), alloc.getPtr(), partitionMode,
      /*indices=*/SmallVector<Value>{},
      SmallVector<Value>(offsets.begin(), offsets.end()),
      SmallVector<Value>(sizes.begin(), sizes.end()),
      /*partitionIndices=*/SmallVector<Value>{},
      /*partitionOffsets=*/SmallVector<Value>{},
      /*partitionSizes=*/SmallVector<Value>{}, boundsValid,
      /*elementOffsets=*/
      SmallVector<Value>(elementOffsets.begin(), elementOffsets.end()),
      /*elementSizes=*/
      SmallVector<Value>(elementSizes.begin(), elementSizes.end()));
}

static inline arts::DbAcquireOp materializeBridgeAcquire(
    OpBuilder &builder, Location loc, arts::DbAllocOp alloc,
    arts::ArtsMode mode, arts::PartitionMode partitionMode, Value offset,
    Value size, Value boundsValid = Value{},
    ArrayRef<Value> elementOffsets = {}, ArrayRef<Value> elementSizes = {}) {
  SmallVector<Value, 1> offsets{offset};
  SmallVector<Value, 1> sizes{size};
  return materializeBridgeAcquire(builder, loc, alloc, mode, partitionMode,
                                  offsets, sizes, boundsValid, elementOffsets,
                                  elementSizes);
}

static inline Value materializeProduct(OpBuilder &builder, Location loc,
                                       ValueRange values) {
  Value product = createOneIndex(builder, loc);
  for (Value value : values)
    product = arith::MulIOp::create(builder, loc, product, value);
  return product;
}

static inline SmallVector<Value>
materializeRowMajorCoordinates(OpBuilder &builder, Location loc, Value ordinal,
                               ValueRange sizes) {
  SmallVector<Value> coords(sizes.size());
  Value remaining = ordinal;
  for (int64_t dim = static_cast<int64_t>(sizes.size()) - 1; dim >= 0; --dim) {
    Value size = sizes[dim];
    coords[dim] = arith::RemUIOp::create(builder, loc, remaining, size);
    if (dim != 0)
      remaining = arith::DivUIOp::create(builder, loc, remaining, size);
  }
  return coords;
}

struct FlatNodeBlockGroupLoop {
  scf::ForOp loop;
  Value nodeOrdinal;
  Value blockBase;
};

static inline FlatNodeBlockGroupLoop
materializeFlatNodeBlockGroupLoop(OpBuilder &builder, Location loc,
                                  Value totalNodes, Value blockCount,
                                  int64_t blockGroupSize) {
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockStep =
      createConstantIndex(builder, loc, std::max<int64_t>(1, blockGroupSize));
  Value blockGroupCount =
      materializeNonNegativeCeilDiv(builder, loc, blockCount, blockStep);
  Value workItemCount =
      arith::MulIOp::create(builder, loc, totalNodes, blockGroupCount);

  auto flatLoop = scf::ForOp::create(builder, loc, zero, workItemCount, one);
  builder.setInsertionPointToStart(flatLoop.getBody());
  Value launchOrdinal = flatLoop.getInductionVar();

  // The loop body is unreachable when blockGroupCount is zero, but keep the
  // divisor nonzero so zero-sized dynamic inputs still have well-formed IR.
  Value emptyBlocks = arith::CmpIOp::create(
      builder, loc, arith::CmpIPredicate::eq, blockGroupCount, zero);
  Value safeBlockGroupCount =
      arith::SelectOp::create(builder, loc, emptyBlocks, one, blockGroupCount);
  Value nodeOrdinal =
      arith::DivUIOp::create(builder, loc, launchOrdinal, safeBlockGroupCount);
  Value blockGroupOrdinal =
      arith::RemUIOp::create(builder, loc, launchOrdinal, safeBlockGroupCount);
  Value blockBase =
      arith::MulIOp::create(builder, loc, blockGroupOrdinal, blockStep);
  return {flatLoop, nodeOrdinal, blockBase};
}

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
      Value blockPayloadStart =
          subtractClampZero(builder, loc, ownerOffset, ownerHalo.lower);
      Value requestedEnd =
          arith::AddIOp::create(builder, loc, ownerOffset, ownerBlockSize);
      if (copyIntoBlock && ownerHalo.upper > 0)
        requestedEnd = arith::AddIOp::create(
            builder, loc, requestedEnd,
            createConstantIndex(builder, loc, ownerHalo.upper));
      Value ownerCopyEnd = arith::MinUIOp::create(builder, loc, requestedEnd,
                                                  logicalSizes[ownerDim]);
      // Write back only the owner iteration range; reload keeps halo extent.
      if (!copyIntoBlock) {
        std::optional<unsigned> ownerSlot =
            getCodirOwnerDimSlot(codelet, ownerDim);
        if (std::optional<int64_t> maxOffset = getCodirOwnerDimValue(
                codelet.getAccessMaxOffsetsAttr(), ownerDim, ownerSlot,
                static_cast<unsigned>(hostType.getRank())))
          if (*maxOffset > 0) {
            Value writeEnd = arith::SubIOp::create(
                builder, loc, logicalSizes[ownerDim],
                createConstantIndex(builder, loc, *maxOffset));
            ownerCopyEnd =
                arith::MinUIOp::create(builder, loc, ownerCopyEnd, writeEnd);
          }
      }
      lanePlan.hostOffsets[ownerDim] = ownerCopyStart;
      // The owned payload starts after the lower-halo ring.
      if (!copyIntoBlock && ownerHalo.lower > 0)
        lanePlan.blockOffsets[ownerDim] =
            arith::SubIOp::create(builder, loc, ownerOffset, blockPayloadStart);
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

/// Build the per-element copy nest that fills one gathered block of the
/// replicated DB from the corresponding producer block. Unlike the coarse
/// write-back (materializeHostBlockElementCopyNest), both source and
/// destination are block payloads indexed identically; there is no coarse host
/// offset to add, because each gathered block is a full standalone DB.
static inline void
materializePerBlockCopyNest(OpBuilder &builder, Location loc, Value srcPayload,
                            Value dstPayload, ArrayRef<Value> copySizes,
                            SmallVectorImpl<Value> &indices) {
  unsigned dim = indices.size();
  if (dim == copySizes.size()) {
    Value loaded = memref::LoadOp::create(builder, loc, srcPayload, indices);
    memref::StoreOp::create(builder, loc, loaded, dstPayload, indices);
    return;
  }
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  auto loop = scf::ForOp::create(builder, loc, zero, copySizes[dim], one);
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(loop.getBody());
  indices.push_back(loop.getInductionVar());
  materializePerBlockCopyNest(builder, loc, srcPayload, dstPayload, copySizes,
                              indices);
  indices.pop_back();
}

static inline void materializePerBlockOffsetCopyNest(
    OpBuilder &builder, Location loc, Value srcPayload, Value dstPayload,
    ArrayRef<Value> copySizes, ArrayRef<Value> srcOffsets,
    ArrayRef<Value> dstOffsets, SmallVectorImpl<Value> &indices) {
  unsigned dim = indices.size();
  if (dim == copySizes.size()) {
    SmallVector<Value> srcIndices;
    SmallVector<Value> dstIndices;
    srcIndices.reserve(indices.size());
    dstIndices.reserve(indices.size());
    for (auto [idx, induction] : llvm::enumerate(indices)) {
      srcIndices.push_back(
          arith::AddIOp::create(builder, loc, srcOffsets[idx], induction));
      dstIndices.push_back(
          arith::AddIOp::create(builder, loc, dstOffsets[idx], induction));
    }
    Value loaded = memref::LoadOp::create(builder, loc, srcPayload, srcIndices);
    memref::StoreOp::create(builder, loc, loaded, dstPayload, dstIndices);
    return;
  }
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  auto loop = scf::ForOp::create(builder, loc, zero, copySizes[dim], one);
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(loop.getBody());
  indices.push_back(loop.getInductionVar());
  materializePerBlockOffsetCopyNest(builder, loc, srcPayload, dstPayload,
                                    copySizes, srcOffsets, dstOffsets, indices);
  indices.pop_back();
}

/// Build the per-element summing nest that settles one output block by reducing
/// the P per-tile partial payloads with `+=` (arith.addf) and writing the
/// result ONCE. This is the addf dual of materializePerBlockCopyNest: instead
/// of a single source copy, the leaf loads tile 0, accumulates tiles 1..P-1
/// with arith.addf, and stores once into the settled block. All payloads are
/// block payloads indexed identically (no coarse host offset).
static inline void materializePerBlockSumNest(OpBuilder &builder, Location loc,
                                              ArrayRef<Value> partialPayloads,
                                              Value dstPayload,
                                              ArrayRef<Value> copySizes,
                                              SmallVectorImpl<Value> &indices) {
  unsigned dim = indices.size();
  if (dim == copySizes.size()) {
    Value acc =
        memref::LoadOp::create(builder, loc, partialPayloads.front(), indices);
    for (size_t tile = 1; tile < partialPayloads.size(); ++tile) {
      Value next =
          memref::LoadOp::create(builder, loc, partialPayloads[tile], indices);
      acc = arith::AddFOp::create(builder, loc, acc, next);
    }
    memref::StoreOp::create(builder, loc, acc, dstPayload, indices);
    return;
  }
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  auto loop = scf::ForOp::create(builder, loc, zero, copySizes[dim], one);
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(loop.getBody());
  indices.push_back(loop.getInductionVar());
  materializePerBlockSumNest(builder, loc, partialPayloads, dstPayload,
                             copySizes, indices);
  indices.pop_back();
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBLOCKCOPY_H
