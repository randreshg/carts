///==========================================================================///
/// File: CodirToArtsHostBridgeParticipants.h
///
/// Host bridge participant compatibility and collection.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEPARTICIPANTS_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGEPARTICIPANTS_H

#include "CodirToArtsHostBridgeAnchors.h"

namespace {

static inline bool codirBridgeIndexIsAddOf(Value candidate, Value base,
                                           int64_t offset) {
  candidate = ::mlir::carts::ValueAnalysis::stripNumericCasts(candidate);
  base = ::mlir::carts::ValueAnalysis::stripNumericCasts(base);
  if (offset == 0)
    return ::mlir::carts::ValueAnalysis::sameValue(candidate, base);

  auto add = candidate.getDefiningOp<arith::AddIOp>();
  if (!add)
    return false;
  auto rhs = ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(add.getRhs());
  if (rhs && *rhs == offset &&
      ::mlir::carts::ValueAnalysis::sameValue(add.getLhs(), base))
    return true;
  auto lhs = ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(add.getLhs());
  return lhs && *lhs == offset &&
         ::mlir::carts::ValueAnalysis::sameValue(add.getRhs(), base);
}

static inline bool codirBridgeUpperMatchesLinearExtent(Value upper, Value base,
                                                       int64_t extent) {
  upper = ::mlir::carts::ValueAnalysis::stripNumericCasts(upper);
  if (codirBridgeIndexIsAddOf(upper, base, extent))
    return true;
  if (auto min = upper.getDefiningOp<arith::MinUIOp>())
    return codirBridgeIndexIsAddOf(min.getLhs(), base, extent) ||
           codirBridgeIndexIsAddOf(min.getRhs(), base, extent);
  if (auto min = upper.getDefiningOp<arith::MinSIOp>())
    return codirBridgeIndexIsAddOf(min.getLhs(), base, extent) ||
           codirBridgeIndexIsAddOf(min.getRhs(), base, extent);
  return false;
}

static inline bool codirBridgeLoopStepEquals(scf::ForOp loop, int64_t step) {
  if (!loop || step <= 0)
    return false;
  std::optional<int64_t> folded =
      ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(loop.getStep());
  return folded && *folded == step;
}

static inline Value codirBridgeBodyParamSource(codir::CodeletOp codelet,
                                               Value bodyValue) {
  auto bodyArg = dyn_cast<BlockArgument>(
      ::mlir::carts::ValueAnalysis::stripNumericCasts(bodyValue));
  if (!codelet || !bodyArg || codelet.getBody().empty() ||
      bodyArg.getOwner() != &codelet.getBody().front())
    return {};
  unsigned depCount = codelet.getDeps().size();
  if (bodyArg.getArgNumber() < depCount)
    return {};
  unsigned paramIndex = bodyArg.getArgNumber() - depCount;
  if (paramIndex >= codelet.getParams().size())
    return {};
  return codelet.getParams()[paramIndex];
}

static inline bool codirBridgeBodyParamIsDivOf(codir::CodeletOp codelet,
                                               Value quotientBodyParam,
                                               Value dividendBodyParam,
                                               int64_t divisor) {
  if (divisor <= 0)
    return false;
  Value quotientSource = codirBridgeBodyParamSource(codelet, quotientBodyParam);
  Value dividendSource = codirBridgeBodyParamSource(codelet, dividendBodyParam);
  if (!quotientSource || !dividendSource)
    return false;
  auto div = ::mlir::carts::ValueAnalysis::stripNumericCasts(quotientSource)
                 .getDefiningOp<arith::DivUIOp>();
  if (!div ||
      !::mlir::carts::ValueAnalysis::sameValue(div.getLhs(), dividendSource))
    return false;
  std::optional<int64_t> folded =
      ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(div.getRhs());
  return folded && *folded == divisor;
}

static inline Value codirBridgeRankExpandedLinearStoreIndex(
    Value ownerIndex, Value tileIndex, Value ownerBlockBase, int64_t tileExtent,
    bool &ownerIndexIsRelative) {
  if (tileExtent <= 0)
    return {};
  ownerIndex = ::mlir::carts::ValueAnalysis::stripNumericCasts(ownerIndex);
  tileIndex = ::mlir::carts::ValueAnalysis::stripNumericCasts(tileIndex);
  ownerBlockBase =
      ::mlir::carts::ValueAnalysis::stripNumericCasts(ownerBlockBase);

  auto rem = tileIndex.getDefiningOp<arith::RemUIOp>();
  if (!rem)
    return {};
  std::optional<int64_t> remRhs =
      ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(rem.getRhs());
  if (!remRhs || *remRhs != tileExtent)
    return {};

  auto getDivLinearIndex = [&](Value candidate) -> Value {
    auto div = ::mlir::carts::ValueAnalysis::stripNumericCasts(candidate)
                   .getDefiningOp<arith::DivUIOp>();
    if (!div)
      return {};
    std::optional<int64_t> divRhs =
        ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(div.getRhs());
    if (!divRhs || *divRhs != tileExtent ||
        !::mlir::carts::ValueAnalysis::sameValue(div.getLhs(), rem.getLhs()))
      return {};
    return div.getLhs();
  };

  if (Value direct = getDivLinearIndex(ownerIndex)) {
    ownerIndexIsRelative = false;
    return direct;
  }

  auto sub = ownerIndex.getDefiningOp<arith::SubIOp>();
  if (!sub ||
      !::mlir::carts::ValueAnalysis::sameValue(sub.getRhs(), ownerBlockBase))
    return {};
  if (Value relative = getDivLinearIndex(sub.getLhs())) {
    ownerIndexIsRelative = true;
    return relative;
  }
  return {};
}

static inline bool codirBridgeStoreHasOnlyLoopAncestors(Operation *store,
                                                        Operation *bodyRoot) {
  if (!store || !bodyRoot)
    return false;
  for (Operation *parent = store->getParentOp(); parent && parent != bodyRoot;
       parent = parent->getParentOp()) {
    if (!isa<scf::ForOp>(parent))
      return false;
  }
  return true;
}

static inline bool
codirBridgeIsRankExpandedFullLinearWriter(codir::CodeletOp codelet,
                                          unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size() ||
      codelet.getBody().empty())
    return false;
  if (getFinalizedCodirDepCollectiveKind(codelet, depIndex) !=
      codir::CodirCollectiveKind::none)
    return false;

  std::optional<codir::CodirAccessMode> mode =
      getCodirDepAccessMode(codelet, depIndex);
  if (!mode || *mode != codir::CodirAccessMode::write)
    return false;
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex) ||
      !hasCodirTileOwnerSlicePlan(codelet))
    return false;

  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  std::optional<SmallVector<int64_t, 4>> tileShape =
      readI64ArrayAttr(codelet.getTileShapeAttr());
  std::optional<SmallVector<int64_t, 4>> logicalSlice =
      readI64ArrayAttr(codelet.getLogicalWorkerSliceAttr());
  if (!ownerDims || ownerDims->size() != 1 || ownerDims->front() != 0 ||
      !tileShape || tileShape->size() != 1 || tileShape->front() <= 0 ||
      !logicalSlice || logicalSlice->size() != 1 || logicalSlice->front() <= 0)
    return false;

  Block &body = codelet.getBody().front();
  if (depIndex >= body.getNumArguments())
    return false;
  Value depArg = body.getArgument(depIndex);
  auto depType = dyn_cast<MemRefType>(depArg.getType());
  if (!depType || depType.getRank() != 2 ||
      (depType.getDimSize(1) != ShapedType::kDynamic &&
       depType.getDimSize(1) != tileShape->front()))
    return false;

  SmallVector<Value, 4> ownerBaseCandidates;
  unsigned depCount = codelet.getDeps().size();
  unsigned paramCount = codelet.getParams().size();
  if (body.getNumArguments() < depCount + paramCount)
    return false;
  ownerBaseCandidates.reserve(paramCount);
  for (unsigned paramIndex = 0; paramIndex < paramCount; ++paramIndex)
    ownerBaseCandidates.push_back(body.getArgument(depCount + paramIndex));

  int64_t tileExtent = tileShape->front();
  int64_t logicalExtent = logicalSlice->front();

  bool sawStore = false;
  bool rejected = false;
  Value selectedOwnerBase;
  body.walk([&](Operation *op) {
    if (rejected)
      return WalkResult::interrupt();
    auto access = getCodirMemoryAccessInfo(op);
    if (!access || access->memref != depArg)
      return WalkResult::advance();

    if (isa<memref::LoadOp, polygeist::DynLoadOp, affine::AffineLoadOp>(op)) {
      rejected = true;
      return WalkResult::interrupt();
    }
    if (!isa<memref::StoreOp, polygeist::DynStoreOp, affine::AffineStoreOp>(
            op) ||
        access->indices.size() != 2 ||
        !codirBridgeStoreHasOnlyLoopAncestors(op, codelet.getOperation())) {
      rejected = true;
      return WalkResult::interrupt();
    }

    Value matchedOwnerBase;
    for (Value candidateOwnerBase : ownerBaseCandidates) {
      bool ownerIndexIsRelative = false;
      Value linearIndex = codirBridgeRankExpandedLinearStoreIndex(
          access->indices[0], access->indices[1], candidateOwnerBase,
          tileExtent, ownerIndexIsRelative);
      if (!linearIndex)
        continue;

      auto innerIv = dyn_cast<BlockArgument>(
          ::mlir::carts::ValueAnalysis::stripNumericCasts(linearIndex));
      auto innerLoop =
          innerIv
              ? dyn_cast_or_null<scf::ForOp>(innerIv.getOwner()->getParentOp())
              : scf::ForOp{};
      if (!innerLoop || innerLoop.getInductionVar() != innerIv ||
          !codirBridgeLoopStepEquals(innerLoop, 1))
        continue;

      auto outerIv = dyn_cast<BlockArgument>(
          ::mlir::carts::ValueAnalysis::stripNumericCasts(
              innerLoop.getLowerBound()));
      auto outerLoop =
          outerIv
              ? dyn_cast_or_null<scf::ForOp>(outerIv.getOwner()->getParentOp())
              : scf::ForOp{};
      if (!outerLoop || outerLoop.getInductionVar() != outerIv ||
          !codirBridgeLoopStepEquals(outerLoop, tileExtent) ||
          !codirBridgeUpperMatchesLinearExtent(innerLoop.getUpperBound(),
                                               outerIv, tileExtent))
        continue;

      bool ownerMatches = false;
      if (ownerIndexIsRelative) {
        ownerMatches = codirBridgeBodyParamIsDivOf(codelet, candidateOwnerBase,
                                                   outerLoop.getLowerBound(),
                                                   tileExtent) &&
                       codirBridgeUpperMatchesLinearExtent(
                           outerLoop.getUpperBound(), outerLoop.getLowerBound(),
                           logicalExtent);
      } else {
        ownerMatches =
            ::mlir::carts::ValueAnalysis::sameValue(outerLoop.getLowerBound(),
                                                    candidateOwnerBase) &&
            codirBridgeUpperMatchesLinearExtent(
                outerLoop.getUpperBound(), candidateOwnerBase, logicalExtent);
      }
      if (!ownerMatches)
        continue;

      if (matchedOwnerBase) {
        rejected = true;
        return WalkResult::interrupt();
      }
      matchedOwnerBase = candidateOwnerBase;
    }

    if (!matchedOwnerBase ||
        (selectedOwnerBase && matchedOwnerBase != selectedOwnerBase)) {
      rejected = true;
      return WalkResult::interrupt();
    }
    selectedOwnerBase = matchedOwnerBase;

    sawStore = true;
    return WalkResult::advance();
  });

  return sawStore && !rejected;
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
    // define its whole owner block unless its rank-expanded body already
    // exposes the contiguous owner/tile overwrite structure.
    bool hasUnprovenCoverageWriter = llvm::any_of(
        participants, [](const HostBridgeParticipant &participant) {
          if (!codirAccessMayWrite(participant.mode))
            return false;
          codir::CodeletOp codelet = participant.codelet;
          if (codelet.getWriteFootprintAttr())
            return false;
          return !codirBridgeIsRankExpandedFullLinearWriter(
              codelet, participant.depIndex);
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
        !isCompatibleHostBridgeParticipant(
            seed, seedDepIndex, codeletUse->codelet, codeletUse->depIndex)) {
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
