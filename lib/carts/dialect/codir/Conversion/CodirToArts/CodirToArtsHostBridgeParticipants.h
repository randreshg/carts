///==========================================================================///
/// File: CodirToArtsHostBridgeParticipants.h
///
/// Host bridge participant compatibility and collection.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEPARTICIPANTS_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEPARTICIPANTS_H

#include "CodirToArtsHostBridgeAnchors.h"

namespace {

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

// The committed partition-graph block shape for a dep's array, taken from the
// codelet's `partition_graph` entry whose `muId` matches the dep's array id.
// This is the per-array storage grain (owner_block tile vs block_parallel whole
// array) that drives convert-codir-to-arts' block-local access rewrite, which a
// single canonical block DB must satisfy.
static inline ArrayAttr
codirDepPartitionBlockShape(codir::CodeletOp codelet, unsigned depIndex) {
  if (!codelet)
    return ArrayAttr{};
  std::optional<int64_t> depArrayId = codir::getDepArrayId(codelet, depIndex);
  if (!depArrayId)
    return ArrayAttr{};
  auto graph = dyn_cast_or_null<ArrayAttr>(
      codelet->getAttr(codir::AttrNames::PartitionGraph));
  if (!graph)
    return ArrayAttr{};
  for (Attribute attr : graph) {
    auto entry = dyn_cast<DictionaryAttr>(attr);
    if (!entry)
      continue;
    auto muId = dyn_cast_or_null<IntegerAttr>(
        entry.get(codir::AttrNames::PartitionGraphKeys::MuId));
    if (!muId || muId.getInt() != *depArrayId)
      continue;
    return dyn_cast_or_null<ArrayAttr>(
        entry.get(codir::AttrNames::PartitionGraphKeys::BlockShape));
  }
  return ArrayAttr{};
}

// Two compute_block deps share a canonical block DB only when their committed
// partition-graph block shapes agree. A host-read array consumed both as an
// owner_block tile (seed) and as a block_parallel whole array (sibling anchor)
// has incompatible grains; one block DB cannot serve both.
static inline bool hasSameHostBridgeBlockGrain(codir::CodeletOp seed,
                                               unsigned seedDepIndex,
                                               codir::CodeletOp codelet,
                                               unsigned depIndex) {
  ArrayAttr seedShape = codirDepPartitionBlockShape(seed, seedDepIndex);
  ArrayAttr depShape = codirDepPartitionBlockShape(codelet, depIndex);
  // When the grain evidence is absent on either side, defer to the existing
  // physical-block-shape compatibility rather than fabricate a mismatch.
  if (!seedShape || !depShape)
    return true;
  return seedShape == depShape;
}

static inline bool isCompatibleHostBridgeParticipant(codir::CodeletOp seed,
                                                     unsigned seedDepIndex,
                                                     codir::CodeletOp codelet,
                                                     unsigned depIndex) {
  if (!seed || !codelet ||
      !hasSameHostBridgePlan(seed, seedDepIndex, codelet, depIndex))
    return false;
  if (!hasSameHostBridgeBlockGrain(seed, seedDepIndex, codelet, depIndex))
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

struct HostBridgeCodeletUse {
  codir::CodeletOp codelet;
  unsigned depIndex = 0;
  OpOperand *viewSourceOperand = nullptr;
};

static inline std::optional<HostBridgeCodeletUse>
resolveHostBridgeCodeletUse(OpOperand &use) {
  Operation *owner = use.getOwner();
  if (!owner)
    return std::nullopt;
  if (auto codelet = dyn_cast<codir::CodeletOp>(owner)) {
    std::optional<unsigned> depIndex = getCodeletDepOperandIndex(codelet, use);
    if (!depIndex)
      return std::nullopt;
    return HostBridgeCodeletUse{codelet, *depIndex,
                                /*viewSourceOperand=*/nullptr};
  }
  if (owner->getNumResults() == 0 ||
      (!isCodirViewDep(owner->getResult(0)) && !isMemrefForwardingOp(owner)))
    return std::nullopt;
  for (Value result : owner->getResults()) {
    if (!isa<MemRefType>(result.getType()))
      continue;
    for (OpOperand &resultUse : result.getUses()) {
      if (std::optional<HostBridgeCodeletUse> downstream =
              resolveHostBridgeCodeletUse(resultUse)) {
        return HostBridgeCodeletUse{downstream->codelet, downstream->depIndex,
                                    &use};
      }
    }
  }
  return std::nullopt;
}

static inline FailureOr<HostBridgeUseCollection>
collectHostBridgeParticipants(Operation *anchor, codir::CodeletOp seed,
                              unsigned seedDepIndex, Value hostView) {
  if (!anchor || !seed || !hostView)
    return failure();

  HostBridgeUseCollection collection;
  for (OpOperand &use : hostView.getUses()) {
    Operation *owner = use.getOwner();
    std::optional<HostBridgeCodeletUse> codeletUse =
        resolveHostBridgeCodeletUse(use);
    Operation *containmentOp =
        codeletUse ? codeletUse->codelet.getOperation() : owner;
    if (!isUseInsideAnchor(anchor, containmentOp))
      continue;
    if (!codeletUse ||
        !isCompatibleHostBridgeParticipant(seed, seedDepIndex,
                                           codeletUse->codelet,
                                           codeletUse->depIndex)) {
      if (!hostBridgeUseMayWrite(anchor, use)) {
        appendUniqueHostBridgeReadObservation(
            findHostBridgeReadObservationAnchor(use),
            collection.readObservationAnchors);
        continue;
      }
      return failure();
    }
    std::optional<codir::CodirAccessMode> mode =
        getCodirDepAccessMode(codeletUse->codelet, codeletUse->depIndex);
    if (!mode)
      return failure();
    collection.participants.push_back({codeletUse->codelet,
                                       codeletUse->depIndex, *mode,
                                       codeletUse->viewSourceOperand});
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

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEPARTICIPANTS_H
