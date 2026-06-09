///==========================================================================///
/// File: CodirToArtsHostBridgeMaterialization.h
///
/// Top-level host bridge and raw dependency materialization entrypoints.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEMATERIALIZATION_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEMATERIALIZATION_H

#include "CodirToArtsPerBlockCollectives.h"

namespace {

static inline FailureOr<Value>
materializeHostWholeToComputeBlockBridge(codir::CodeletOp codelet,
                                         unsigned depIndex, Value hostView) {
  if (!codelet || depIndex >= codelet.getDeps().size() || !hostView)
    return failure();
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex))
    return failure();
  if (!hasCodirTileOwnerSlicePlan(codelet) ||
      !codirDepCanUseBlockStorageAccess(codelet, depIndex))
    return failure();

  std::optional<codir::CodirAccessMode> depMode =
      getCodirDepAccessMode(codelet, depIndex);
  if (!depMode)
    return failure();

  auto memrefType = dyn_cast<MemRefType>(hostView.getType());
  if (!memrefType || memrefType.getRank() == 0 || isCodirViewDep(hostView))
    return failure();

  Operation *anchor = findCodirHostBridgeAnchor(codelet, depIndex, hostView);
  if (!anchor)
    return failure();

  SmallVector<HostBridgeParticipant> participants;
  SmallVector<Operation *> readObservationAnchors;
  FailureOr<HostBridgeUseCollection> collected =
      collectHostBridgeParticipants(anchor, codelet, depIndex, hostView);
  if (succeeded(collected)) {
    participants = std::move(collected->participants);
    readObservationAnchors = std::move(collected->readObservationAnchors);
  } else {
    participants.push_back({codelet, depIndex, *depMode});
  }

  bool needsCopyIn = hostBridgeNeedsInitialCopyIn(anchor, participants);
  bool needsCopyOut = llvm::any_of(participants, [](const auto &participant) {
    return codirAccessMayWrite(participant.mode);
  });

  OpBuilder builder(anchor);
  Location loc = codelet.getLoc();
  FailureOr<Value> materializedHostView =
      materializeCoarseHostDbForHostBridge(builder, loc, hostView);
  if (failed(materializedHostView))
    return failure();
  hostView = *materializedHostView;

  builder.setInsertionPoint(anchor);
  SmallVector<Value> dynamicSizes;
  SmallVector<Value> logicalSizes =
      getBridgeLogicalElementSizes(builder, loc, hostView);
  if (logicalSizes.size() != static_cast<size_t>(memrefType.getRank()))
    return failure();
  for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim) {
    if (memrefType.isDynamicDim(dim))
      dynamicSizes.push_back(logicalSizes[dim]);
  }

  Value blockView;
  if (failed(createDbBackedMemref(builder, loc, memrefType, dynamicSizes,
                                  blockView, codelet, depIndex)))
    return failure();
  arts::DbAllocOp blockAlloc = findBackingDbAlloc(blockView);
  if (!blockAlloc)
    return failure();
  blockAlloc.setStorageBridgeAttr(arts::StorageBridgeAttr::get(
      builder.getContext(), arts::StorageBridge::host_whole_to_compute_block));

  ModuleOp bridgeModule = codelet->getParentOfType<ModuleOp>();
  bool hasInterNodeRuntime =
      bridgeModule && arts::hasArtsInterNodeRuntime(bridgeModule);
  BridgePlan bridgePlan = buildBridgePlan(
      codelet, depIndex, hostView, blockAlloc, blockAlloc, participants,
      readObservationAnchors, needsCopyIn, needsCopyOut, hasInterNodeRuntime);
  codir::CodirCollectiveKind unsupportedCollective =
      codir::CodirCollectiveKind::none;
  if (bridgePlanHasUnsupportedCollective(bridgePlan, unsupportedCollective)) {
    return codelet.emitOpError()
           << "cannot materialize CODIR collective "
           << getCollectiveKindName(unsupportedCollective)
           << " for host-whole to compute-block bridge; add a generic "
              "BridgePlan materializer instead of falling back to a coarse "
              "copy";
  }

  // Dispatch the concrete bridge realization from CODIR's per-dep collective
  // carrier. The BridgePlan is intentionally generic; this first slice keeps
  // the existing emitters intact while moving workload sizing onto the plan.
  bool perBlockAllGather =
      bridgePlan.needsCopyOut && bridgePlan.hasInterNodeRuntime &&
      bridgePlanHasCollective(bridgePlan,
                              codir::CodirCollectiveKind::all_gather);

  bool crossNodeGatherCopyOut =
      bridgePlan.needsCopyOut &&
      bridgePlanHasCollective(bridgePlan,
                              codir::CodirCollectiveKind::reduce_scatter);

  // Block-native summing settle follows from CODIR's committed reduce_scatter
  // storage transition. The split factor gives the number of partial blocks to
  // sum.
  bool perBlockSummingSettle =
      hasInterNodeRuntime && bridgePlan.needsCopyOut &&
      llvm::any_of(bridgePlan.participants,
                   [](const HostBridgeParticipant &participant) {
                     return codirDepUsesBlockNativeSettle(participant.codelet,
                                                          participant.depIndex);
                   });

  // Iterative stencil halo uses a distributed per-block DB plus
  // nearest-neighbor RO reads. This follows the committed CODIR `halo`
  // participant, not the presence of a copy-out writer in the same bridge.
  bool perBlockStencilHalo = bridgePlanHasHaloStencilStorage(bridgePlan);

  // Read-only stencil bridges with zero reach along the committed owner
  // dimensions are block-local after the host-whole -> compute-block copy-in.
  // Commit the existing per-block single-writer stencil fact here so ARTS
  // realizes distributed ownership from CODIR's storage transition instead of
  // re-deriving a benchmark-specific exception.
  bool ownerLocalReadOnlyStencilBridge =
      bridgePlanHasOwnerLocalReadOnlyStencilStorage(bridgePlan);
  if (ownerLocalReadOnlyStencilBridge) {
    if (failed(preparePerBlockSingleWriterStencilDb(blockAlloc)))
      return failure();
  }

  if (needsCopyIn) {
    if (failed(materializeHostBlockCopyLoop(
            builder, loc, hostView, blockAlloc, codelet, depIndex,
            /*copyIntoBlock=*/true,
            /*crossNodeGather=*/false, &bridgePlan)))
      return failure();
  }

  if (perBlockStencilHalo) {
    if (failed(preparePerBlockSingleWriterStencilDb(blockAlloc)))
      return failure();
    if (failed(emitPerBlockStencilHaloBeforeReadPhases(
            builder, loc, blockAlloc, participants, &bridgePlan)))
      return failure();
  }

  if (needsCopyOut) {
    for (HostBridgeParticipant &participant : participants) {
      if (codirAccessMayWrite(participant.mode))
        participant.codelet.setCompletionBarrierAttr(
            UnitAttr::get(participant.codelet->getContext()));
    }
    SmallVector<Operation *> syncAnchors = filterHostBridgeReadSyncAnchors(
        anchor, participants, readObservationAnchors);
    for (Operation *observationAnchor : syncAnchors) {
      if (!observationAnchor || !isUseInsideAnchor(anchor, observationAnchor))
        continue;
      builder.setInsertionPoint(observationAnchor);
      if (failed(materializeHostBlockCopyLoop(
              builder, loc, hostView, blockAlloc, codelet, depIndex,
              /*copyIntoBlock=*/false, crossNodeGatherCopyOut, &bridgePlan)))
        return failure();
    }
    builder.setInsertionPointAfter(anchor);
    if (failed(materializeHostBlockCopyLoop(
            builder, loc, hostView, blockAlloc, codelet, depIndex,
            /*copyIntoBlock=*/false, crossNodeGatherCopyOut, &bridgePlan)))
      return failure();

    // Keep the coarse write-back for whole-array consumers and add the
    // block-native replicated DB substrate for tiled consumers.
    if (perBlockAllGather) {
      builder.setInsertionPointAfter(anchor);
      FailureOr<Value> gathered =
          emitPerBlockAllGatherWriteBack(builder, loc, hostView, bridgePlan);
      if (failed(gathered))
        return failure();
    }

    // Block-native summing settle writes one distinct result block per EDT.
    if (perBlockSummingSettle) {
      unsigned tileCount = 0;
      if (auto factor = codelet.getPartialReductionSplitFactorAttr())
        if (factor.getInt() > 0)
          tileCount = static_cast<unsigned>(factor.getInt());
      if (tileCount > 0) {
        builder.setInsertionPointAfter(anchor);
        FailureOr<Value> settled = emitPerBlockSummingSettle(
            builder, loc, blockAlloc, tileCount, codelet, depIndex, bridgePlan);
        if (failed(settled))
          return failure();
      }
    }
  }

  for (HostBridgeParticipant &participant : participants)
    participant.codelet->setOperand(participant.depIndex, blockView);
  return blockView;
}

static inline LogicalResult
materializeExistingDbHostBridgeIfNeeded(codir::CodeletOp codelet,
                                        unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return failure();

  Value dep = codelet.getDeps()[depIndex];
  arts::DbAllocOp hostAlloc = findBackingDbAlloc(dep);
  if (!hostAlloc)
    return success();
  // Phase-redistributed deps AND iterative-stencil halo deps both need the
  // host-whole -> compute-block bridge: the bridge is where the per-block
  // single-writer stencil DB + halo exchange are realized
  // (perBlockStencilHalo). A committed compute_block+halo stencil with a coarse
  // backing host DB would otherwise never reach the bridge and stay coarse
  // local_only (not distributed).
  bool needsBridgeForRootHaloParticipant =
      codirDepRequiresComputeBlockStorage(codelet, depIndex) &&
      codirRootHasHaloStencilStorageParticipant(codelet, depIndex);
  bool needsBridgeForCommittedComputeBlock =
      codirDepRequiresComputeBlockStorage(codelet, depIndex) &&
      !canUseCodirOwnerSliceForAlloc(codelet, depIndex, hostAlloc);
  if (!codirDepRequiresPhaseRedistributionBridge(codelet, depIndex) &&
      !codirDepUsesHaloStencilStorage(codelet, depIndex) &&
      !needsBridgeForRootHaloParticipant &&
      !needsBridgeForCommittedComputeBlock)
    return success();
  if (failed(requireFinalizedCodirDepOwnerDimsForMaterialization(codelet,
                                                                 depIndex)))
    return failure();
  if (!hasCodirTileOwnerSlicePlan(codelet) || isCodirViewDep(dep))
    return success();
  if (canUseCodirOwnerSliceForAlloc(codelet, depIndex, hostAlloc))
    return success();

  std::optional<arts::PartitionMode> hostMode =
      arts::getPartitionMode(hostAlloc.getOperation());
  if (!hostMode || *hostMode != arts::PartitionMode::coarse)
    return success();
  FailureOr<Value> replacement =
      materializeHostWholeToComputeBlockBridge(codelet, depIndex, dep);
  return failed(replacement) ? failure() : success();
}

static inline LogicalResult
materializeExistingDbComputeBlockIfNeeded(codir::CodeletOp codelet,
                                          unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return failure();
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex) ||
      codirDepRequiresPhaseRedistributionBridge(codelet, depIndex) ||
      codirDepUsesHaloStencilStorage(codelet, depIndex))
    return success();
  if (!hasCodirTileOwnerSlicePlan(codelet) ||
      !codirDepCanUseBlockStorageAccess(codelet, depIndex))
    return success();
  if (failed(requireFinalizedCodirDepOwnerDimsForMaterialization(codelet,
                                                                 depIndex)))
    return failure();

  Value dep = codelet.getDeps()[depIndex];
  Value hostRoot = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep);
  arts::DbAllocOp sourceAlloc = findBackingDbAlloc(hostRoot);
  if (!sourceAlloc)
    return success();
  if (canUseCodirOwnerSliceForAlloc(codelet, depIndex, sourceAlloc))
    return success();

  std::optional<arts::PartitionMode> sourceMode =
      arts::getPartitionMode(sourceAlloc.getOperation());
  if (!sourceMode || *sourceMode != arts::PartitionMode::coarse)
    return success();

  auto memrefType = dyn_cast<MemRefType>(dep.getType());
  if (!memrefType || memrefType.getRank() == 0)
    return success();

  FailureOr<SmallVector<HostBridgeParticipant>> participants =
      collectComputeBlockParticipants(codelet, depIndex, dep, sourceAlloc);
  if (failed(participants))
    return success();

  SmallVector<Value> dynamicSizes;
  for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim)
    if (memrefType.isDynamicDim(dim)) {
      if (static_cast<size_t>(dim) >= sourceAlloc.getElementSizes().size())
        return failure();
      dynamicSizes.push_back(sourceAlloc.getElementSizes()[dim]);
    }

  OpBuilder builder(sourceAlloc);
  builder.setInsertionPointAfter(sourceAlloc);
  Value blockView;
  if (failed(createDbBackedMemref(builder, codelet.getLoc(), memrefType,
                                  dynamicSizes, blockView, codelet, depIndex)))
    return failure();

  for (HostBridgeParticipant &participant : *participants)
    participant.codelet->setOperand(participant.depIndex, blockView);
  return success();
}

static inline bool
canMaterializeRawCodirDependencyWithPlan(Value root,
                                         codir::CodeletOp planSource) {
  if (!hasCodirTileOwnerSlicePlan(planSource))
    return false;
  std::optional<unsigned> depIndex =
      findCodirDependencyIndexForRoot(planSource, root);
  if (!depIndex)
    return false;
  if (!codirDepAllowsComputeBlockStorage(planSource, *depIndex))
    return false;
  return codirDepCanUseBlockStorageAccess(planSource, *depIndex);
}

static inline bool rawCodirDependencyNeedsHostBridge(Value root) {
  if (!root)
    return false;
  if (isa<BlockArgument>(root))
    return true;
  return hasHostMemrefAccessOutsideSchedulingUnit(root);
}

static inline LogicalResult
materializeRawCodirDependency(Value dep, codir::CodeletOp planSource,
                              unsigned depIndex) {
  if (findBackingDbAlloc(dep))
    return success();
  if (failed(requireFinalizedCodirDepOwnerDimsForMaterialization(planSource,
                                                                 depIndex)))
    return failure();

  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep);
  auto memrefType = dyn_cast<MemRefType>(root.getType());
  if (!memrefType)
    return failure();

  Operation *def = root.getDefiningOp();
  OpBuilder builder(root.getContext());
  Value replacement;
  bool usePlan = canMaterializeRawCodirDependencyWithPlan(root, planSource);
  bool needsHostBridge = rawCodirDependencyNeedsHostBridge(root);
  if (usePlan && needsHostBridge) {
    if (auto blockArg = dyn_cast<BlockArgument>(root)) {
      FailureOr<Value> hostView = materializeCoarseHostDbForBlockArgument(
          builder, root.getLoc(), blockArg);
      if (failed(hostView))
        return failure();
      FailureOr<Value> bridged = materializeHostWholeToComputeBlockBridge(
          planSource, depIndex, *hostView);
      return failed(bridged) ? failure() : success();
    }

    FailureOr<Value> bridged =
        materializeHostWholeToComputeBlockBridge(planSource, depIndex, root);
    if (failed(bridged))
      return failure();
    return success();
  }
  if (!def) {
    auto blockArg = dyn_cast<BlockArgument>(root);
    if (!blockArg)
      return failure();

    Block *owner = blockArg.getOwner();
    if (!owner)
      return failure();
    builder.setInsertionPointToStart(owner);

    SmallVector<Value> elementSizes;
    SmallVector<Value> dynamicSizes;
    if (memrefType.getRank() == 0) {
      elementSizes.push_back(createOneIndex(builder, root.getLoc()));
    } else {
      elementSizes.reserve(memrefType.getRank());
      for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim) {
        if (memrefType.isDynamicDim(dim)) {
          Value size = memref::DimOp::create(builder, root.getLoc(), root, dim);
          elementSizes.push_back(size);
          dynamicSizes.push_back(size);
        } else {
          elementSizes.push_back(createConstantIndex(
              builder, root.getLoc(), memrefType.getDimSize(dim)));
        }
      }
    }

    SmallVector<Operation *> dimOps;
    for (Value size : elementSizes)
      if (Operation *op = size.getDefiningOp())
        dimOps.push_back(op);

    arts::DbAllocOp createdDbAlloc;
    if (usePlan) {
      if (failed(createDbBackedMemref(builder, root.getLoc(), memrefType,
                                      dynamicSizes, replacement, planSource,
                                      depIndex)))
        return failure();
    } else {
      Value route = arts::createCurrentNodeRoute(builder, root.getLoc());
      auto dbAlloc = arts::DbAllocOp::create(
          builder, root.getLoc(), arts::ArtsMode::inout, route,
          arts::DbAllocType::unknown, arts::DbMode::write,
          memrefType.getElementType(), root,
          SmallVector<Value>{createOneIndex(builder, root.getLoc())},
          std::move(elementSizes), arts::PartitionMode::coarse);
      createdDbAlloc = dbAlloc;
      replacement =
          materializeInnerPayload(builder, root.getLoc(), dbAlloc.getPtr());
    }

    root.replaceUsesWithIf(replacement, [&](OpOperand &use) {
      Operation *owner = use.getOwner();
      if (createdDbAlloc && owner == createdDbAlloc.getOperation())
        return false;
      return !llvm::is_contained(dimOps, owner);
    });
    return success();
  }

  builder.setInsertionPointAfter(def);
  if (auto alloc = dyn_cast<memref::AllocOp>(def)) {
    if (failed(usePlan
                   ? createDbBackedMemref(builder, alloc.getLoc(), memrefType,
                                          alloc.getDynamicSizes(), replacement,
                                          planSource, depIndex)
                   : createDbBackedMemref(builder, alloc.getLoc(), memrefType,
                                          alloc.getDynamicSizes(),
                                          replacement)))
      return failure();
  } else if (auto alloca = dyn_cast<memref::AllocaOp>(def)) {
    if (failed(usePlan
                   ? createDbBackedMemref(builder, alloca.getLoc(), memrefType,
                                          alloca.getDynamicSizes(), replacement,
                                          planSource, depIndex)
                   : createDbBackedMemref(builder, alloca.getLoc(), memrefType,
                                          alloca.getDynamicSizes(),
                                          replacement)))
      return failure();
  } else {
    return failure();
  }

  SmallVector<memref::DeallocOp> deallocs;
  for (Operation *user : llvm::make_early_inc_range(root.getUsers())) {
    auto dealloc = dyn_cast<memref::DeallocOp>(user);
    if (dealloc && dealloc.getMemref() == root)
      deallocs.push_back(dealloc);
  }
  for (memref::DeallocOp dealloc : deallocs)
    dealloc.erase();

  root.replaceAllUsesWith(replacement);
  if (def->use_empty())
    def->erase();
  return success();
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEMATERIALIZATION_H
