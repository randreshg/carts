///==========================================================================///
/// File: CodirToArtsExistingDbMaterialization.h
///
/// Existing DB dependency bridge and compute-block materialization entrypoints.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_EXISTINGDBMATERIALIZATION_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_EXISTINGDBMATERIALIZATION_H

#include "CodirToArtsHostWholeToBlockBridge.h"

namespace {

static inline LogicalResult
materializeExistingDbHostBridgeIfNeeded(codir::CodeletOp codelet,
                                        unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return failure();

  Value dep = codelet.getDeps()[depIndex];
  if (auto exchange = dep.getDefiningOp<codir::HaloExchangeOp>())
    dep = exchange.getSource();
  arts::DbAllocOp hostAlloc = findBackingDbAlloc(dep);
  if (!hostAlloc)
    return success();
  // Phase-redistributed and halo deps with a coarse host DB still need the
  // host-whole -> compute-block copy. The explicit codir.halo_exchange op owns
  // the later halo movement.
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

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_EXISTINGDBMATERIALIZATION_H
