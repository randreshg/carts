///==========================================================================///
/// File: CodirToArtsHostBridgeHoist.h
///
/// Loop-hoist safety checks and host bridge anchor selection.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEHOIST_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEHOIST_H

#include "CodirToArtsHostBridgeParticipants.h"

namespace {

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

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEHOIST_H
