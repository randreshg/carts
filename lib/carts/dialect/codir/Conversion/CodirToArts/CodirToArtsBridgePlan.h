///==========================================================================///
/// File: CodirToArtsBridgePlan.h
///
/// Bridge-plan predicates, collective names, routing mode, and plan assembly.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEPLAN_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEPLAN_H

#include "CodirToArtsBridgeOwnerMap.h"

namespace {

static inline bool
bridgePlanHasCollective(const BridgePlan &plan,
                        codir::CodirCollectiveKind collectiveKind) {
  return llvm::any_of(
      plan.participants, [collectiveKind](const HostBridgeParticipant &p) {
        return getFinalizedCodirDepCollectiveKind(p.codelet, p.depIndex) ==
               collectiveKind;
      });
}

static inline bool bridgePlanHasHaloStencilStorage(const BridgePlan &plan) {
  return llvm::any_of(plan.participants, [](const HostBridgeParticipant &p) {
    return codirDepUsesHaloStencilStorage(p.codelet, p.depIndex);
  });
}

static inline bool
bridgePlanHasOwnerLocalReadOnlyStencilStorage(const BridgePlan &plan) {
  bool foundOwnerLocalStencilRead = false;
  for (const HostBridgeParticipant &participant : plan.participants) {
    codir::CodeletOp participantCodelet = participant.codelet;
    if (!participantCodelet ||
        participant.depIndex >= participantCodelet.getDeps().size())
      return false;

    if (!codirAccessMayRead(participant.mode)) {
      if (codirAccessMayWrite(participant.mode))
        return false;
      continue;
    }
    if (codirAccessMayWrite(participant.mode))
      return false;

    // Only compute-block reads that carry a committed access window are
    // owner-local-stencil candidates; the access-window offsets are the
    // structural stencil evidence the owner-local check below proves against.
    if (!codirDepRequiresComputeBlockStorage(participantCodelet,
                                             participant.depIndex) ||
        !participantCodelet.getAccessMinOffsetsAttr() ||
        !participantCodelet.getAccessMaxOffsetsAttr())
      continue;

    if (codirDepUsesHaloStencilStorage(participantCodelet,
                                       participant.depIndex))
      return false;

    if (!codirDepUsesOwnerLocalStencilStorage(participantCodelet,
                                              participant.depIndex))
      return false;
    foundOwnerLocalStencilRead = true;
  }
  return foundOwnerLocalStencilRead;
}

static inline bool
bridgePlanHasUnsupportedCollective(const BridgePlan &plan,
                                   codir::CodirCollectiveKind &collectiveKind) {
  for (const HostBridgeParticipant &participant : plan.participants) {
    codir::CodirCollectiveKind kind = getFinalizedCodirDepCollectiveKind(
        participant.codelet, participant.depIndex);
    switch (kind) {
    case codir::CodirCollectiveKind::none:
    case codir::CodirCollectiveKind::all_gather:
    case codir::CodirCollectiveKind::reduce_scatter:
    case codir::CodirCollectiveKind::halo:
      break;
    case codir::CodirCollectiveKind::all_to_all:
    case codir::CodirCollectiveKind::allreduce:
    case codir::CodirCollectiveKind::broadcast:
      collectiveKind = kind;
      return true;
    }
  }
  return false;
}

static inline StringRef
getCollectiveKindName(codir::CodirCollectiveKind collectiveKind) {
  switch (collectiveKind) {
  case codir::CodirCollectiveKind::none:
    return "none";
  case codir::CodirCollectiveKind::all_gather:
    return "all_gather";
  case codir::CodirCollectiveKind::all_to_all:
    return "all_to_all";
  case codir::CodirCollectiveKind::reduce_scatter:
    return "reduce_scatter";
  case codir::CodirCollectiveKind::allreduce:
    return "allreduce";
  case codir::CodirCollectiveKind::broadcast:
    return "broadcast";
  case codir::CodirCollectiveKind::halo:
    return "halo";
  }
  return "unknown";
}

static inline BridgePlan
buildBridgePlan(codir::CodeletOp seedCodelet, unsigned seedDepIndex,
                Value hostView, arts::DbAllocOp sourceAlloc,
                arts::DbAllocOp destinationAlloc,
                ArrayRef<HostBridgeParticipant> participants,
                ArrayRef<Operation *> readObservationAnchors, bool needsCopyIn,
                bool needsCopyOut, bool hasInterNodeRuntime) {
  BridgePlan plan;
  plan.seedCodelet = seedCodelet;
  plan.seedDepIndex = seedDepIndex;
  plan.collectiveKind =
      getFinalizedCodirDepCollectiveKind(seedCodelet, seedDepIndex);
  plan.participants.assign(participants.begin(), participants.end());
  plan.readObservationAnchors.assign(readObservationAnchors.begin(),
                                     readObservationAnchors.end());
  plan.hostView = hostView;
  plan.sourceAlloc = sourceAlloc;
  plan.destinationAlloc = destinationAlloc;
  plan.needsCopyIn = needsCopyIn;
  plan.needsCopyOut = needsCopyOut;
  plan.hasInterNodeRuntime = hasInterNodeRuntime;
  plan.ownerMap = buildBridgeOwnerMap(
      seedCodelet, seedDepIndex, sourceAlloc ? sourceAlloc : destinationAlloc);

  if (plan.collectiveKind == codir::CodirCollectiveKind::all_gather ||
      (plan.collectiveKind == codir::CodirCollectiveKind::reduce_scatter &&
       codirDepUsesBlockNativeSettle(seedCodelet, seedDepIndex)))
    plan.replicationPolicy = BridgeReplicationPolicy::replicated_local;

  if (plan.collectiveKind == codir::CodirCollectiveKind::all_gather ||
      plan.collectiveKind == codir::CodirCollectiveKind::reduce_scatter)
    plan.routingMode = BridgeRoutingMode::node_ordinal;
  else if (plan.destinationAlloc)
    plan.routingMode = BridgeRoutingMode::block_ordinal;

  if (seedCodelet) {
    if (auto factor = seedCodelet.getPartialReductionSplitFactorAttr())
      if (factor.getInt() > 0)
        plan.tileCount = static_cast<unsigned>(factor.getInt());
  }
  return plan;
}

static inline arts::DbAllocOp
getBridgePlanSizingAlloc(const BridgePlan &plan,
                         BridgeWorkloadKind workloadKind) {
  switch (workloadKind) {
  case BridgeWorkloadKind::host_to_block:
    return plan.destinationAlloc ? plan.destinationAlloc : plan.sourceAlloc;
  case BridgeWorkloadKind::block_to_host:
  case BridgeWorkloadKind::all_gather:
  case BridgeWorkloadKind::summing_settle:
  case BridgeWorkloadKind::halo:
    return plan.sourceAlloc ? plan.sourceAlloc : plan.destinationAlloc;
  }
  return plan.sourceAlloc ? plan.sourceAlloc : plan.destinationAlloc;
}

static inline BridgeRoutingMode
getBridgeWorkloadRoutingMode(const BridgePlan &plan,
                             BridgeWorkloadKind workloadKind,
                             bool crossNodeGather = false) {
  switch (workloadKind) {
  case BridgeWorkloadKind::host_to_block:
  case BridgeWorkloadKind::halo:
    return BridgeRoutingMode::block_ordinal;
  case BridgeWorkloadKind::block_to_host:
    return crossNodeGather ? BridgeRoutingMode::node_ordinal
                           : BridgeRoutingMode::current_node;
  case BridgeWorkloadKind::all_gather:
  case BridgeWorkloadKind::summing_settle:
    return BridgeRoutingMode::node_ordinal;
  }
  return plan.routingMode;
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEPLAN_H
