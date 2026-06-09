///==========================================================================///
/// File: CodirToArtsPerBlockSummingSettle.h
///
/// Per-block single-writer summing-settle materialization.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKSUMMINGSETTLE_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKSUMMINGSETTLE_H

#include "CodirToArtsPerBlockAllGather.h"

namespace {

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

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKSUMMINGSETTLE_H
