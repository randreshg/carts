///==========================================================================///
/// File: CodirToArtsBlockStoragePredicates.h
///
/// Block-storage materialization predicates over finalized CODIR dependency facts.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKSTORAGEPREDICATES_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKSTORAGEPREDICATES_H

#include "CodirToArtsHaloWindows.h"

namespace {

static inline bool codirDepRequiresPlannedOwnerDims(codir::CodeletOp codelet,
                                                    unsigned depIndex) {
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  if (view && codirStorageViewUsesComputeBlock(*view))
    return true;

  std::optional<codir::CodirCollectiveKind> collective =
      getCodirDepCollectiveKind(codelet, depIndex);
  if (!collective)
    return false;
  switch (*collective) {
  case codir::CodirCollectiveKind::all_gather:
  case codir::CodirCollectiveKind::reduce_scatter:
  case codir::CodirCollectiveKind::halo:
    return true;
  case codir::CodirCollectiveKind::none:
  case codir::CodirCollectiveKind::all_to_all:
  case codir::CodirCollectiveKind::allreduce:
  case codir::CodirCollectiveKind::broadcast:
    return false;
  }
  return false;
}

static inline LogicalResult
requireFinalizedCodirDepOwnerDimsForMaterialization(codir::CodeletOp codelet,
                                                    unsigned depIndex) {
  if (!codirDepRequiresPlannedOwnerDims(codelet, depIndex))
    return success();
  if (getCodirDepOwnerDims(codelet, depIndex))
    return success();
  return codelet.emitOpError()
         << "dependency #" << depIndex
         << " requires finalized non-empty dep_owner_dims for "
            "block/stencil/compute materialization";
}

static inline bool codirDepHasHaloWindow(codir::CodeletOp codelet,
                                         unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  auto memrefType = dyn_cast<MemRefType>(codelet.getDeps()[depIndex].getType());
  if (!memrefType || memrefType.getRank() == 0)
    return false;
  SmallVector<CodirOwnerHaloWindow, 4> windows = getCodirOwnerHaloWindows(
      codelet, depIndex, static_cast<unsigned>(memrefType.getRank()),
      /*requireReadOnly=*/false);
  return llvm::any_of(windows, [](const CodirOwnerHaloWindow &window) {
    return !window.empty();
  });
}

// Halo-block storage is decided from the committed structure alone: a
// compute-block storage view, a committed `halo` collective, and an
// access-window halo. The committed halo collective is what marks the edge as
// reading neighbor tiles, so no source pattern is consulted.
static inline bool codirDepUsesHaloStencilStorage(codir::CodeletOp codelet,
                                                  unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex))
    return false;
  return getFinalizedCodirDepCollectiveKind(codelet, depIndex) ==
             codir::CodirCollectiveKind::halo &&
         codirDepHasHaloWindow(codelet, depIndex);
}

static inline bool codirDepUsesBlockNativeSettle(codir::CodeletOp codelet,
                                                 unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  if (getFinalizedCodirDepCollectiveKind(codelet, depIndex) !=
      codir::CodirCollectiveKind::reduce_scatter)
    return false;
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  if (!view || *view != codir::CodirStorageViewKind::phase_redistributed)
    return false;
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex))
    return false;
  auto factor = codelet.getPartialReductionSplitFactorAttr();
  return factor && factor.getInt() > 0;
}

static inline bool
codirDepHasNoStencilReachAlongOwnerDims(codir::CodeletOp codelet,
                                        unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  auto memrefType = dyn_cast<MemRefType>(codelet.getDeps()[depIndex].getType());
  if (!memrefType || memrefType.getRank() == 0)
    return false;
  if (!codelet.getAccessMinOffsetsAttr() || !codelet.getAccessMaxOffsetsAttr())
    return false;

  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty())
    return false;

  unsigned memrefRank = static_cast<unsigned>(memrefType.getRank());
  for (unsigned ownerDim : *ownerDims) {
    std::optional<unsigned> ownerSlot = getCodirOwnerDimSlot(codelet, ownerDim);
    std::optional<int64_t> minOffset = getCodirOwnerDimValue(
        codelet.getAccessMinOffsetsAttr(), ownerDim, ownerSlot, memrefRank);
    std::optional<int64_t> maxOffset = getCodirOwnerDimValue(
        codelet.getAccessMaxOffsetsAttr(), ownerDim, ownerSlot, memrefRank);
    if (!minOffset || !maxOffset)
      return false;
    if (*minOffset != 0 || *maxOffset != 0)
      return false;
  }
  return true;
}

static inline bool
codirDepUsesOwnerLocalStencilStorage(codir::CodeletOp codelet,
                                     unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  // Owner-local stencil storage is proven structurally: a compute-block view, a
  // read-only access mode, no committed collective, and an access window that
  // never reaches outside the owner slice. The committed access-window offsets
  // (required by codirDepHasNoStencilReachAlongOwnerDims) are the stencil
  // evidence, not the source pattern.
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex))
    return false;
  std::optional<codir::CodirAccessMode> mode =
      getCodirDepAccessMode(codelet, depIndex);
  if (!mode || !codirAccessMayRead(*mode) || codirAccessMayWrite(*mode))
    return false;
  if (getFinalizedCodirDepCollectiveKind(codelet, depIndex) !=
      codir::CodirCollectiveKind::none)
    return false;
  return codirDepHasNoStencilReachAlongOwnerDims(codelet, depIndex);
}

static inline bool
codirRootHasHaloStencilStorageParticipant(codir::CodeletOp codelet,
                                          unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
      codelet.getDeps()[depIndex]);
  Operation *scope = codelet->getParentOfType<ModuleOp>();
  if (!root || !scope)
    return false;

  bool found = false;
  scope->walk([&](codir::CodeletOp candidate) {
    if (found)
      return;
    for (auto [idx, dep] : llvm::enumerate(candidate.getDeps())) {
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) != root)
        continue;
      if (codirDepUsesHaloStencilStorage(candidate,
                                         static_cast<unsigned>(idx))) {
        found = true;
        return;
      }
    }
  });
  return found;
}

static inline bool codirDepCanUseBlockStorageAccess(codir::CodeletOp codelet,
                                                    unsigned depIndex) {
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex) ||
      !hasCodirTileOwnerSlicePlan(codelet) ||
      !getCodirDepOwnerDims(codelet, depIndex))
    return false;

  // CODIR's storage plan is the committed layout fact. Do not silently
  // coarse-fallback here because the local access tracer is conservative; the
  // block-local index rewrite below is the fail-closed verifier for the actual
  // transformed body.
  return true;
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKSTORAGEPREDICATES_H
