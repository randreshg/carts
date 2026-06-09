///==========================================================================///
/// File: CodirToArtsHostWholeToBlockBridge.h
///
/// Host-whole to compute-block bridge materialization.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTWHOLETOBLOCKBRIDGE_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTWHOLETOBLOCKBRIDGE_H

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

  // The canonical MU root. After the coarse swap below only repoints the host
  // accesses, the root's remaining uses are the array's compute_block subviews,
  // which may live in dispatch anchors other than the seed anchor (e.g. the
  // separate init/timestep dispatch loops of a double-buffered stencil). They
  // must all land on the single canonical block DB, not just the seed anchor's.
  Value muRoot = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(hostView);

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
    auto reason = arts::ArtsBarrierReasonAttr::get(
        builder.getContext(), arts::ArtsBarrierReason::required_memory);
    arts::BarrierOp::create(builder, loc, reason);
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

  Value blockStaticView = blockView;
  if (blockStaticView.getType() != memrefType) {
    builder.setInsertionPointAfterValue(blockView);
    blockStaticView =
        memref::CastOp::create(builder, loc, memrefType, blockView);
  }
  Operation *castDef = blockStaticView.getDefiningOp();
  auto repointViewSource = [&](OpOperand &viewSource) {
    Operation *viewOp = viewSource.getOwner();
    if (castDef && viewOp && viewOp->getBlock() == castDef->getBlock() &&
        viewOp->isBeforeInBlock(castDef))
      viewOp->moveAfter(castDef);
    viewSource.set(blockStaticView);
  };
  for (HostBridgeParticipant &participant : participants) {
    if (participant.repointOperand)
      repointViewSource(*participant.repointOperand);
    else
      participant.codelet->setOperand(participant.depIndex, blockStaticView);
  }

  // An sde.mu_alloc host-bridge root MUST be fully drained: convert-codir-to-arts
  // rejects any surviving sde op. The coarse swap already moved the genuine host
  // accesses onto the coarse DB and the seed loop repointed the seed anchor, so
  // any MU use left here is a compute_block view living in a sibling dispatch
  // anchor (e.g. the separate init/timestep dispatch loops of a double-buffered
  // stencil). Repoint each onto the single canonical block DB when it commits
  // the SAME block grain as the seed; fail closed with evidence otherwise,
  // rather than leaving the MU to surface as an opaque "SDE operation reached
  // CODIR-to-ARTS" or repointing an incompatible grain that aborts the
  // block-local access rewrite downstream. memref.alloc roots are not subject to
  // this rule: their residual per-dep uses are materialized later, so leave them
  // for the existing materializeRawCodirDependency path.
  if (muRoot && muRoot != blockView &&
      isa_and_nonnull<sde::SdeMuAllocOp>(muRoot.getDefiningOp())) {
    for (OpOperand &use : llvm::make_early_inc_range(muRoot.getUses())) {
      Operation *owner = use.getOwner();
      if (isa_and_nonnull<memref::DeallocOp, memref::DimOp>(owner))
        continue;
      std::optional<HostBridgeCodeletUse> codeletUse =
          resolveHostBridgeCodeletUse(use);
      if (!codeletUse)
        return codelet.emitOpError()
               << "host-read array has a residual non-compute use that the "
                  "host-whole to compute-block bridge cannot drain";
      if (!isCompatibleHostBridgeParticipant(codelet, depIndex,
                                             codeletUse->codelet,
                                             codeletUse->depIndex))
        return codeletUse->codelet.emitOpError()
               << "host-read array is consumed at an incompatible block grain "
                  "across dispatch anchors; a single canonical block DB cannot "
                  "serve both this access and the seed compute_block grain "
                  "(cross-anchor storage-grain reconciliation required)";
      if (codeletUse->viewSourceOperand)
        repointViewSource(*codeletUse->viewSourceOperand);
      else
        codeletUse->codelet->setOperand(codeletUse->depIndex, blockStaticView);
    }
  }
  return blockView;
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTWHOLETOBLOCKBRIDGE_H
