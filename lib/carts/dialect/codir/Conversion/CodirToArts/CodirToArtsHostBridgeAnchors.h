///==========================================================================///
/// File: CodirToArtsHostBridgeAnchors.h
///
/// Host bridge anchor, read-observation, and local write-use analysis.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEANCHORS_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEANCHORS_H

#include "CodirToArtsHostBridgeTypes.h"

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

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEANCHORS_H
