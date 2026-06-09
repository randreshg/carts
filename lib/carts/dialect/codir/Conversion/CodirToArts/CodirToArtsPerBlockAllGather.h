///==========================================================================///
/// File: CodirToArtsPerBlockAllGather.h
///
/// Per-block single-writer all-gather materialization.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKALLGATHER_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKALLGATHER_H

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

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKALLGATHER_H
