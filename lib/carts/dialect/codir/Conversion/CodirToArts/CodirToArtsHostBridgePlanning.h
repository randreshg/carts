///==========================================================================///
/// File: CodirToArtsHostBridgePlanning.h
///
/// Host bridge participant collection, anchor selection, and bridge plan records.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEPLANNING_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEPLANNING_H

#include "CodirToArtsCommonMaterialization.h"

namespace {

static inline Operation *
findCodirDispatchBridgeAnchor(codir::CodeletOp codelet) {
  Operation *anchor = codelet ? codelet.getOperation() : nullptr;
  Operation *nearestLoop = nullptr;
  for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (loop && !nearestLoop)
      nearestLoop = parent;
    if (loop && containsValue(codelet.getParams(), loop.getInductionVar()))
      anchor = parent;
  }
  if (codelet && anchor == codelet.getOperation() &&
      hasCodirTileOwnerSlicePlan(codelet) && nearestLoop)
    return nearestLoop;
  return anchor;
}

struct HostBridgeParticipant {
  codir::CodeletOp codelet;
  unsigned depIndex = 0;
  codir::CodirAccessMode mode = codir::CodirAccessMode::readwrite;
};

enum class BridgeWorkloadKind {
  host_to_block,
  block_to_host,
  all_gather,
  summing_settle,
  halo,
};

enum class BridgeReplicationPolicy {
  single_home,
  replicated_local,
};

enum class BridgeRoutingMode {
  current_node,
  block_ordinal,
  node_ordinal,
};

struct BridgeOwnerMap {
  SmallVector<unsigned, 4> ownerDims;
  SmallVector<int64_t, 4> ownerMapDims;
  SmallVector<int64_t, 4> physicalBlockShape;
  std::optional<arts::DbOwnerMapKind> ownerMapKind;
  int64_t flatBlockCount = 0;
};

struct BridgeWorkloadEvidence {
  BridgeWorkloadKind workloadKind = BridgeWorkloadKind::block_to_host;
  BridgeRoutingMode routingMode = BridgeRoutingMode::current_node;
  bool readOnlySource = false;
  bool copyLike = false;
  bool haloExchange = false;
  bool preservesPerBlockDbGrain = true;
  bool mayGroupAdjacentBlocks = false;
};

struct BridgeWorkGroupPlan {
  BridgeWorkloadEvidence evidence;
  int64_t staticBlockCount = 0;
  int64_t groupSize = 1;

  bool groupsBlockRanges() const { return groupSize > 1; }
};

struct BridgePlan {
  codir::CodeletOp seedCodelet;
  unsigned seedDepIndex = 0;
  codir::CodirCollectiveKind collectiveKind = codir::CodirCollectiveKind::none;
  SmallVector<HostBridgeParticipant> participants;
  SmallVector<Operation *> readObservationAnchors;
  BridgeOwnerMap ownerMap;
  Value hostView;
  arts::DbAllocOp sourceAlloc;
  arts::DbAllocOp destinationAlloc;
  bool needsCopyIn = false;
  bool needsCopyOut = false;
  bool hasInterNodeRuntime = false;
  BridgeReplicationPolicy replicationPolicy =
      BridgeReplicationPolicy::single_home;
  BridgeRoutingMode routingMode = BridgeRoutingMode::current_node;
  unsigned tileCount = 0;
  int64_t groupSize = 1;
};

struct HostBridgeUseCollection {
  SmallVector<HostBridgeParticipant> participants;
  SmallVector<Operation *> readObservationAnchors;
};

static inline BridgeWorkGroupPlan
planBridgeWorkGroups(const BridgePlan &plan, BridgeWorkloadKind workloadKind,
                     bool crossNodeGather = false);

static inline arts::ArtsLaunchPolicy resolveBridgeBlockOrdinalLaunchPolicy(
    ModuleOp module, const BridgePlan *plan, arts::DbAllocOp blockAlloc,
    Value blockOrdinal, OpBuilder &builder, Location loc);

static inline std::optional<unsigned>
getCodeletDepOperandIndex(codir::CodeletOp codelet, OpOperand &use);

static inline bool isUseInsideAnchor(Operation *anchor, Operation *owner) {
  if (!anchor || !owner)
    return false;
  if (anchor == owner)
    return true;
  for (Region &region : anchor->getRegions())
    if (region.isAncestor(owner->getParentRegion()))
      return true;
  return false;
}

static inline bool hostBridgeValueMayBeWrittenInsideAnchor(Operation *anchor,
                                                           Value value);

static inline bool hostBridgeUseMayWrite(Operation *anchor, OpOperand &use) {
  Operation *owner = use.getOwner();
  if (!anchor || !owner)
    return true;
  if (!isUseInsideAnchor(anchor, owner))
    return false;

  if (auto codelet = dyn_cast<codir::CodeletOp>(owner)) {
    std::optional<unsigned> depIndex = getCodeletDepOperandIndex(codelet, use);
    if (!depIndex)
      return true;
    std::optional<codir::CodirAccessMode> mode =
        getCodirDepAccessMode(codelet, *depIndex);
    return !mode || codirAccessMayWrite(*mode);
  }

  if (auto store = dyn_cast<memref::StoreOp>(owner))
    return store.getMemRef() == use.get();
  if (auto store = dyn_cast<polygeist::DynStoreOp>(owner))
    return store.getMemref() == use.get();
  if (auto store = dyn_cast<affine::AffineStoreOp>(owner))
    return store.getMemRef() == use.get();
  if (isa<memref::DeallocOp>(owner))
    return true;
  if (isa<memref::LoadOp, memref::DimOp>(owner))
    return false;

  if (isMemrefForwardingOp(owner)) {
    for (Value result : owner->getResults())
      if (isa<MemRefType>(result.getType()) &&
          hostBridgeValueMayBeWrittenInsideAnchor(anchor, result))
        return true;
    return false;
  }

  return true;
}

static inline bool hostBridgeValueMayBeWrittenInsideAnchor(Operation *anchor,
                                                           Value value) {
  if (!anchor || !value)
    return true;
  for (OpOperand &use : value.getUses())
    if (hostBridgeUseMayWrite(anchor, use))
      return true;
  return false;
}

static inline Operation *findHostBridgeReadObservationAnchor(OpOperand &use) {
  Operation *owner = use.getOwner();
  if (!owner)
    return nullptr;
  if (auto codelet = dyn_cast<codir::CodeletOp>(owner))
    if (Operation *anchor = findCodirDispatchBridgeAnchor(codelet))
      return anchor;
  return owner;
}

static inline void
appendUniqueHostBridgeReadObservation(Operation *anchor,
                                      SmallVectorImpl<Operation *> &anchors) {
  if (!anchor || llvm::is_contained(anchors, anchor))
    return;
  anchors.push_back(anchor);
}

static inline Operation *findHostBridgeEventUnderAnchor(Operation *anchor,
                                                        Operation *op) {
  if (!anchor || !op)
    return nullptr;
  if (anchor == op)
    return op;

  Operation *event = op;
  while (event && event->getParentOp() != anchor) {
    Operation *parent = event->getParentOp();
    if (!parent)
      return nullptr;
    event = parent;
  }
  return event;
}

static inline SmallVector<Operation *>
filterHostBridgeReadSyncAnchors(Operation *anchor,
                                ArrayRef<HostBridgeParticipant> participants,
                                ArrayRef<Operation *> readObservationAnchors) {
  if (!anchor || readObservationAnchors.empty())
    return SmallVector<Operation *>{readObservationAnchors.begin(),
                                    readObservationAnchors.end()};

  struct Event {
    Operation *eventOp = nullptr;
    Operation *syncAnchor = nullptr;
    bool write = false;
    bool readObservation = false;
    unsigned ordinal = 0;
  };

  SmallVector<Event> events;
  unsigned ordinal = 0;
  for (const HostBridgeParticipant &participant : participants) {
    if (!codirAccessMayWrite(participant.mode))
      continue;
    Operation *dispatchAnchor =
        findCodirDispatchBridgeAnchor(participant.codelet);
    Operation *event = findHostBridgeEventUnderAnchor(anchor, dispatchAnchor);
    if (!event)
      return SmallVector<Operation *>{readObservationAnchors.begin(),
                                      readObservationAnchors.end()};
    events.push_back({event, nullptr, /*write=*/true,
                      /*readObservation=*/false, ordinal++});
  }

  for (Operation *syncAnchor : readObservationAnchors) {
    Operation *event = findHostBridgeEventUnderAnchor(anchor, syncAnchor);
    if (!event)
      return SmallVector<Operation *>{readObservationAnchors.begin(),
                                      readObservationAnchors.end()};
    events.push_back({event, syncAnchor, /*write=*/false,
                      /*readObservation=*/true, ordinal++});
  }

  Block *eventBlock = nullptr;
  for (const Event &event : events) {
    if (!event.eventOp || !event.eventOp->getBlock())
      return SmallVector<Operation *>{readObservationAnchors.begin(),
                                      readObservationAnchors.end()};
    if (!eventBlock) {
      eventBlock = event.eventOp->getBlock();
      continue;
    }
    if (eventBlock != event.eventOp->getBlock())
      return SmallVector<Operation *>{readObservationAnchors.begin(),
                                      readObservationAnchors.end()};
  }

  llvm::sort(events, [](const Event &lhs, const Event &rhs) {
    if (lhs.eventOp == rhs.eventOp)
      return lhs.ordinal < rhs.ordinal;
    return lhs.eventOp->isBeforeInBlock(rhs.eventOp);
  });

  SmallVector<Operation *> filtered;
  bool blockDirtyForHost = false;
  for (const Event &event : events) {
    if (event.write)
      blockDirtyForHost = true;
    if (event.readObservation && blockDirtyForHost) {
      appendUniqueHostBridgeReadObservation(event.syncAnchor, filtered);
      blockDirtyForHost = false;
    }
  }
  return filtered;
}

static inline bool
hostBridgeNeedsInitialCopyIn(Operation *anchor,
                             ArrayRef<HostBridgeParticipant> participants) {
  if (llvm::any_of(participants, [](const HostBridgeParticipant &participant) {
        if (!codirAccessMayWrite(participant.mode))
          return false;
        return getFinalizedCodirDepCollectiveKind(participant.codelet,
                                                  participant.depIndex) ==
               codir::CodirCollectiveKind::halo;
      }))
    return true;

  bool hasReadParticipant =
      llvm::any_of(participants, [](const HostBridgeParticipant &participant) {
        return codirAccessMayRead(participant.mode);
      });
  if (!hasReadParticipant) {
    // A write participant with no committed write footprint cannot be proven to
    // define its whole owner block, so seed the block from the host view first.
    // The write-footprint carrier is the structural coverage evidence.
    bool hasUnprovenCoverageWriter =
        llvm::any_of(participants, [](HostBridgeParticipant participant) {
          return codirAccessMayWrite(participant.mode) &&
                 !participant.codelet.getWriteFootprintAttr();
        });
    if (hasUnprovenCoverageWriter)
      return true;
    return false;
  }
  if (!anchor)
    return true;

  struct Event {
    Operation *eventOp = nullptr;
    codir::CodirAccessMode mode = codir::CodirAccessMode::readwrite;
    unsigned ordinal = 0;
  };

  SmallVector<Event> events;
  unsigned ordinal = 0;
  for (const HostBridgeParticipant &participant : participants) {
    Operation *dispatchAnchor =
        findCodirDispatchBridgeAnchor(participant.codelet);
    Operation *event = findHostBridgeEventUnderAnchor(anchor, dispatchAnchor);
    if (!event)
      return true;
    events.push_back({event, participant.mode, ordinal++});
  }

  Block *eventBlock = nullptr;
  for (const Event &event : events) {
    if (!event.eventOp || !event.eventOp->getBlock())
      return true;
    if (!eventBlock) {
      eventBlock = event.eventOp->getBlock();
      continue;
    }
    if (eventBlock != event.eventOp->getBlock())
      return true;
  }

  llvm::sort(events, [](const Event &lhs, const Event &rhs) {
    if (lhs.eventOp == rhs.eventOp)
      return lhs.ordinal < rhs.ordinal;
    return lhs.eventOp->isBeforeInBlock(rhs.eventOp);
  });

  for (const Event &event : events) {
    if (codirAccessMayRead(event.mode))
      return true;
    if (codirAccessMayWrite(event.mode))
      return false;
  }
  return true;
}

static inline bool hasSameHostBridgePlan(codir::CodeletOp lhs,
                                         unsigned lhsDepIndex,
                                         codir::CodeletOp rhs,
                                         unsigned rhsDepIndex) {
  if (!lhs || !rhs)
    return false;
  // Host bridge compatibility is about the DB/MU storage grain. The logical
  // worker slice is CU grouping evidence and may differ between a grouped copy
  // CU and a stencil/compute CU that share the same physical block DBs.
  return getCodirDepOwnerDimsAttr(lhs, lhsDepIndex) ==
             getCodirDepOwnerDimsAttr(rhs, rhsDepIndex) &&
         codir::getDepPhysicalBlockShapeAttr(lhs, lhsDepIndex) ==
             codir::getDepPhysicalBlockShapeAttr(rhs, rhsDepIndex);
}

static inline std::optional<unsigned>
getCodeletDepOperandIndex(codir::CodeletOp codelet, OpOperand &use) {
  if (!codelet)
    return std::nullopt;
  unsigned operandIndex = use.getOperandNumber();
  if (operandIndex >= codelet.getDeps().size())
    return std::nullopt;
  return operandIndex;
}

static inline bool isCompatibleHostBridgeParticipant(codir::CodeletOp seed,
                                                     unsigned seedDepIndex,
                                                     codir::CodeletOp codelet,
                                                     unsigned depIndex) {
  if (!seed || !codelet ||
      !hasSameHostBridgePlan(seed, seedDepIndex, codelet, depIndex))
    return false;
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex))
    return false;
  if (!hasCodirTileOwnerSlicePlan(codelet) ||
      !codirDepCanUseBlockStorageAccess(codelet, depIndex))
    return false;
  return true;
}

static inline bool isCompatibleComputeBlockParticipant(
    codir::CodeletOp seed, unsigned seedDepIndex, codir::CodeletOp codelet,
    unsigned depIndex, arts::DbAllocOp sourceAlloc) {
  if (!seed || !codelet || !sourceAlloc ||
      !hasSameHostBridgePlan(seed, seedDepIndex, codelet, depIndex))
    return false;
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex) ||
      codirDepRequiresPhaseRedistributionBridge(codelet, depIndex))
    return false;
  if (!hasCodirTileOwnerSlicePlan(codelet) ||
      !codirDepCanUseBlockStorageAccess(codelet, depIndex))
    return false;
  return findBackingDbAlloc(codelet.getDeps()[depIndex]) == sourceAlloc;
}

static inline FailureOr<HostBridgeUseCollection>
collectHostBridgeParticipants(Operation *anchor, codir::CodeletOp seed,
                              unsigned seedDepIndex, Value hostView) {
  if (!anchor || !seed || !hostView)
    return failure();

  HostBridgeUseCollection collection;
  for (OpOperand &use : hostView.getUses()) {
    Operation *owner = use.getOwner();
    if (!isUseInsideAnchor(anchor, owner))
      continue;

    auto codelet = dyn_cast<codir::CodeletOp>(owner);
    if (!codelet) {
      if (!hostBridgeUseMayWrite(anchor, use)) {
        appendUniqueHostBridgeReadObservation(
            findHostBridgeReadObservationAnchor(use),
            collection.readObservationAnchors);
        continue;
      }
      return failure();
    }

    std::optional<unsigned> depIndex = getCodeletDepOperandIndex(codelet, use);
    if (!depIndex || !isCompatibleHostBridgeParticipant(seed, seedDepIndex,
                                                        codelet, *depIndex)) {
      if (!hostBridgeUseMayWrite(anchor, use)) {
        appendUniqueHostBridgeReadObservation(
            findHostBridgeReadObservationAnchor(use),
            collection.readObservationAnchors);
        continue;
      }
      return failure();
    }

    std::optional<codir::CodirAccessMode> mode =
        getCodirDepAccessMode(codelet, *depIndex);
    if (!mode)
      return failure();
    collection.participants.push_back({codelet, *depIndex, *mode});
  }

  if (collection.participants.empty())
    return failure();
  return collection;
}

static inline FailureOr<SmallVector<HostBridgeParticipant>>
collectComputeBlockParticipants(codir::CodeletOp seed, unsigned seedDepIndex,
                                Value hostView, arts::DbAllocOp sourceAlloc) {
  if (!seed || !hostView || !sourceAlloc)
    return failure();

  SmallVector<HostBridgeParticipant> participants;
  for (OpOperand &use : hostView.getUses()) {
    Operation *owner = use.getOwner();
    auto codelet = dyn_cast_or_null<codir::CodeletOp>(owner);
    if (!codelet) {
      if (isa_and_nonnull<memref::DimOp, memref::DeallocOp>(owner))
        continue;
      return failure();
    }

    std::optional<unsigned> depIndex = getCodeletDepOperandIndex(codelet, use);
    if (!depIndex || !isCompatibleComputeBlockParticipant(
                         seed, seedDepIndex, codelet, *depIndex, sourceAlloc))
      return failure();

    std::optional<codir::CodirAccessMode> mode =
        getCodirDepAccessMode(codelet, *depIndex);
    if (!mode)
      return failure();
    participants.push_back({codelet, *depIndex, *mode});
  }

  if (participants.empty())
    return failure();
  return participants;
}

static inline bool hasLoopCarriedHostBridgeObservationHazard(
    scf::ForOp loop, codir::CodeletOp seed, unsigned seedDepIndex,
    Value hostView) {
  if (!loop || !seed || !hostView)
    return true;

  struct Event {
    Operation *eventOp = nullptr;
    bool compatibleBlockWriter = false;
    bool coarseReadObservation = false;
    unsigned ordinal = 0;
  };

  SmallVector<Event> events;
  unsigned ordinal = 0;
  Operation *loopOp = loop.getOperation();
  for (OpOperand &use : hostView.getUses()) {
    Operation *owner = use.getOwner();
    if (!isUseInsideAnchor(loopOp, owner))
      continue;

    auto codelet = dyn_cast<codir::CodeletOp>(owner);
    std::optional<unsigned> depIndex =
        codelet ? getCodeletDepOperandIndex(codelet, use) : std::nullopt;
    Operation *anchor =
        codelet ? findCodirDispatchBridgeAnchor(codelet) : owner;
    Operation *event = findHostBridgeEventUnderAnchor(loopOp, anchor);
    if (!event)
      return true;

    if (depIndex && isCompatibleHostBridgeParticipant(seed, seedDepIndex,
                                                      codelet, *depIndex)) {
      std::optional<codir::CodirAccessMode> mode =
          getCodirDepAccessMode(codelet, *depIndex);
      if (mode && codirAccessMayWrite(*mode))
        events.push_back({event, /*compatibleBlockWriter=*/true,
                          /*coarseReadObservation=*/false, ordinal++});
      continue;
    }

    if (!hostBridgeUseMayWrite(loopOp, use))
      events.push_back({event, /*compatibleBlockWriter=*/false,
                        /*coarseReadObservation=*/true, ordinal++});
  }

  if (events.empty())
    return false;

  Block *eventBlock = nullptr;
  for (const Event &event : events) {
    if (!event.eventOp || !event.eventOp->getBlock())
      return true;
    if (!eventBlock) {
      eventBlock = event.eventOp->getBlock();
      continue;
    }
    if (eventBlock != event.eventOp->getBlock())
      return true;
  }

  llvm::sort(events, [](const Event &lhs, const Event &rhs) {
    if (lhs.eventOp == rhs.eventOp)
      return lhs.ordinal < rhs.ordinal;
    return lhs.eventOp->isBeforeInBlock(rhs.eventOp);
  });

  bool hasCompatibleWriter = llvm::any_of(
      events, [](const Event &event) { return event.compatibleBlockWriter; });
  if (!hasCompatibleWriter)
    return false;

  bool sawCompatibleWriter = false;
  for (const Event &event : events) {
    if (event.compatibleBlockWriter) {
      sawCompatibleWriter = true;
      continue;
    }
    if (event.coarseReadObservation && !sawCompatibleWriter)
      return true;
  }
  return false;
}

static inline bool canHoistHostBridgeAcrossLoop(scf::ForOp loop,
                                                codir::CodeletOp codelet,
                                                unsigned seedDepIndex,
                                                Value hostView) {
  if (!loop || !codelet || !hostView)
    return false;
  if (containsValue(codelet.getParams(), loop.getInductionVar()))
    return false;

  arts::DbAllocOp hostAlloc = findBackingDbAlloc(hostView);
  if (hostAlloc) {
    bool hasCoarseWriteToHost = false;
    loop.walk([&](arts::DbAcquireOp acquire) {
      if (hasCoarseWriteToHost)
        return WalkResult::interrupt();
      if (!arts::DbUtils::isWriterMode(acquire.getMode()))
        return WalkResult::advance();
      if (acquire.getSourcePtr() != hostAlloc.getPtr())
        return WalkResult::advance();
      if (Value sourceGuid = acquire.getSourceGuid();
          sourceGuid && sourceGuid != hostAlloc.getGuid())
        return WalkResult::advance();
      if (acquire.getPartitionModeOr() != arts::PartitionMode::coarse)
        return WalkResult::advance();
      hasCoarseWriteToHost = true;
      return WalkResult::interrupt();
    });
    if (hasCoarseWriteToHost)
      return false;
  }

  Value hostBridgeRoot =
      ::mlir::carts::ValueAnalysis::stripMemrefViewOps(hostView);
  arts::DbAllocOp hostBridgeAlloc = findBackingDbAlloc(hostView);
  bool hasExistingBridgeWriter = false;
  loop.walk([&](codir::CodeletOp nestedCodelet) {
    if (hasExistingBridgeWriter)
      return WalkResult::interrupt();
    if (nestedCodelet == codelet)
      return WalkResult::advance();
    for (auto [idx, dep] : llvm::enumerate(nestedCodelet.getDeps())) {
      std::optional<codir::CodirAccessMode> mode =
          getCodirDepAccessMode(nestedCodelet, static_cast<unsigned>(idx));
      if (!mode || !codirAccessMayWrite(*mode))
        continue;
      arts::DbAllocOp alloc = findBackingDbAlloc(dep);
      if (!alloc || !alloc.getStorageBridgeAttr())
        continue;
      // Only an already-materialized bridge writer for the SAME host data is a
      // staleness hazard. A bridge writer for a different program array (a
      // sibling array that was hoisted on an earlier seed) is independent and
      // must not block hoisting this array's bridge. Match by the host root the
      // dep still references, or by the host DB alloc backing this bridge.
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) !=
              hostBridgeRoot &&
          (!hostBridgeAlloc || findBackingDbAlloc(dep) != hostBridgeAlloc))
        continue;
      hasExistingBridgeWriter = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  if (hasExistingBridgeWriter)
    return false;

  Value hostRoot = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(hostView);
  bool hasPotentialSameRootWriter = false;
  loop.walk([&](codir::CodeletOp nestedCodelet) {
    if (hasPotentialSameRootWriter)
      return WalkResult::interrupt();
    if (nestedCodelet == codelet)
      return WalkResult::advance();
    for (auto [idx, dep] : llvm::enumerate(nestedCodelet.getDeps())) {
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) != hostRoot)
        continue;
      std::optional<codir::CodirStorageViewKind> view =
          getCodirDepStorageViewKind(nestedCodelet, static_cast<unsigned>(idx));
      if (!view || !codirStorageViewUsesComputeBlock(*view))
        continue;
      std::optional<codir::CodirAccessMode> mode =
          getCodirDepAccessMode(nestedCodelet, static_cast<unsigned>(idx));
      if (!mode || !codirAccessMayWrite(*mode))
        continue;
      // A same-root writer that is itself a compatible host-bridge participant
      // writes into the shared block tile this bridge materializes, not back
      // into the coarse host image. For an iterative stencil (the read seed and
      // the write live in the same time-loop iteration), the coarse host array
      // is therefore untouched between the pre-loop copy-in and post-loop
      // copy-out, so the bridge stays loop-invariant and is safe to hoist. Only
      // a writer that bypasses this bridge (writing the coarse host directly)
      // blocks hoisting.
      if (isCompatibleHostBridgeParticipant(
              codelet, seedDepIndex, nestedCodelet, static_cast<unsigned>(idx)))
        continue;
      hasPotentialSameRootWriter = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  if (hasPotentialSameRootWriter)
    return false;

  // A same-root compatible block writer does not touch the coarse host image in
  // the current iteration, but a host-whole read before the first such writer
  // observes the previous iteration's value. Keeping the copy-out after the
  // loop would therefore feed stale host data to the next iteration.
  if (hasLoopCarriedHostBridgeObservationHazard(loop, codelet, seedDepIndex,
                                                hostView))
    return false;

  Region &loopRegion = loop.getRegion();
  for (OpOperand &use : hostView.getUses()) {
    Operation *owner = use.getOwner();
    if (!owner || !loopRegion.isAncestor(owner->getParentRegion()))
      continue;
    auto userCodelet = dyn_cast<codir::CodeletOp>(owner);
    std::optional<unsigned> depIndex =
        userCodelet ? getCodeletDepOperandIndex(userCodelet, use)
                    : std::nullopt;
    if (depIndex && isCompatibleHostBridgeParticipant(codelet, seedDepIndex,
                                                      userCodelet, *depIndex))
      continue;
    if (!hostBridgeUseMayWrite(loop.getOperation(), use))
      continue;
    return false;
  }
  return true;
}

static inline Operation *findCodirHostBridgeAnchor(codir::CodeletOp codelet,
                                                   unsigned depIndex,
                                                   Value hostView) {
  Operation *anchor = findCodirDispatchBridgeAnchor(codelet);
  if (!anchor)
    return nullptr;

  for (Operation *parent = anchor->getParentOp(); parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (!loop)
      continue;
    if (!canHoistHostBridgeAcrossLoop(loop, codelet, depIndex, hostView))
      break;
    anchor = loop.getOperation();
  }
  return anchor;
}

static inline SmallVector<Value>
getBridgeLogicalElementSizes(OpBuilder &builder, Location loc, Value hostView) {
  if (arts::DbAllocOp hostAlloc = findBackingDbAlloc(hostView))
    return SmallVector<Value>(hostAlloc.getElementSizes().begin(),
                              hostAlloc.getElementSizes().end());

  SmallVector<Value> sizes;
  auto memrefType = dyn_cast<MemRefType>(hostView.getType());
  if (!memrefType)
    return sizes;
  sizes.reserve(memrefType.getRank());
  for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim) {
    if (memrefType.isDynamicDim(dim)) {
      sizes.push_back(memref::DimOp::create(builder, loc, hostView, dim));
      continue;
    }
    sizes.push_back(
        createConstantIndex(builder, loc, memrefType.getDimSize(dim)));
  }
  return sizes;
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEPLANNING_H
