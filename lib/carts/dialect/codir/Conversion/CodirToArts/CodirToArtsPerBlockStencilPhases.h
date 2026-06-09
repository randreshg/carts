///==========================================================================///
/// File: CodirToArtsPerBlockStencilPhases.h
///
/// Insertion of per-block stencil halo phases before read phases.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKSTENCILPHASES_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKSTENCILPHASES_H

#include "CodirToArtsPerBlockStencilDb.h"

namespace {

static inline Operation *findCodirOwnerDispatchAnchor(codir::CodeletOp codelet,
                                                      unsigned depIndex) {
  SmallVector<Value, 4> ownerParams =
      getCodirDepOwnerParamValues(codelet, depIndex);
  Operation *anchor = nullptr;
  bool matched = false;
  for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (!loop) {
      if (matched)
        break;
      continue;
    }
    if (containsValue(ownerParams, loop.getInductionVar())) {
      anchor = parent;
      matched = true;
      continue;
    }
    if (matched)
      break;
  }
  return anchor ? anchor : findCodirDispatchBridgeAnchor(codelet);
}

static inline SmallVector<Value, 4>
collectEnclosingControlTokens(Operation *op) {
  SmallVector<Value, 4> tokens;
  for (Operation *parent = op ? op->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    if (auto loop = dyn_cast<scf::ForOp>(parent)) {
      tokens.push_back(loop.getInductionVar());
      continue;
    }
    if (auto ifOp = dyn_cast<scf::IfOp>(parent))
      tokens.push_back(ifOp.getCondition());
  }
  return tokens;
}

static inline bool
isHaloReadParticipant(const HostBridgeParticipant &participant) {
  return codirAccessMayRead(participant.mode) &&
         codirDepUsesHaloStencilStorage(participant.codelet,
                                        participant.depIndex) &&
         !codirDepUsesGroupedOwnerComputeHaloAccess(participant.codelet,
                                                    participant.depIndex);
}

static inline LogicalResult emitPerBlockStencilHaloBeforeReadPhases(
    OpBuilder &builder, Location loc, arts::DbAllocOp blockAlloc,
    ArrayRef<HostBridgeParticipant> participants,
    const BridgePlan *bridgePlan = nullptr) {
  SmallVector<Operation *, 4> emittedAnchors;
  for (const HostBridgeParticipant &participant : participants) {
    if (!isHaloReadParticipant(participant))
      continue;
    Operation *dispatchAnchor =
        findCodirOwnerDispatchAnchor(participant.codelet, participant.depIndex);
    if (!dispatchAnchor)
      return failure();
    if (llvm::is_contained(emittedAnchors, dispatchAnchor))
      continue;
    emittedAnchors.push_back(dispatchAnchor);

    builder.setInsertionPoint(dispatchAnchor);
    SmallVector<Value, 4> phaseTokens =
        collectEnclosingControlTokens(dispatchAnchor);
    if (failed(emitPerBlockSingleWriterStencilDb(
            builder, loc, blockAlloc, participant.codelet, participant.depIndex,
            phaseTokens, bridgePlan)))
      return failure();
  }
  return success();
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_PERBLOCKSTENCILPHASES_H
