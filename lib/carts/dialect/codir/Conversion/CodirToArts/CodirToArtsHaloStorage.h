///==========================================================================///
/// File: CodirToArtsHaloStorage.h
///
/// Unioned per-buffer halo storage windows and backing-DB halo padding readers.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HALOSTORAGE_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HALOSTORAGE_H

#include "CodirToArtsBlockStoragePredicates.h"
#include "carts/dialect/arts/Utils/DbUtils.h"

namespace {

static inline bool
canMaterializeRawCodirDependencyWithPlan(Value root,
                                         codir::CodeletOp planSource);
static inline bool rawCodirDependencyNeedsHostBridge(Value root);

static inline void
mergeCodirOwnerHaloWindow(SmallVectorImpl<CodirOwnerHaloWindow> &windows,
                          CodirOwnerHaloWindow incoming) {
  if (incoming.empty())
    return;
  for (CodirOwnerHaloWindow &window : windows) {
    if (window.ownerDim != incoming.ownerDim)
      continue;
    window.lower = std::max(window.lower, incoming.lower);
    window.upper = std::max(window.upper, incoming.upper);
    return;
  }
  windows.push_back(incoming);
}

static inline SmallVector<CodirOwnerHaloWindow, 4>
codirBackingBufferHaloWindows(Value rootMemref, unsigned memrefRank) {
  SmallVector<CodirOwnerHaloWindow, 4> unionWindows;
  if (!rootMemref)
    return unionWindows;

  Operation *defining = rootMemref.getDefiningOp();
  Operation *scope =
      defining ? defining : rootMemref.getParentBlock()->getParentOp();
  ModuleOp module = scope ? scope->getParentOfType<ModuleOp>() : ModuleOp{};
  if (!module)
    return unionWindows;

  module.walk([&](codir::CodeletOp codelet) {
    for (auto [idx, dep] : llvm::enumerate(codelet.getDeps())) {
      Value depRoot = getCodirHaloStorageRoot(dep);
      if (depRoot != rootMemref)
        continue;
      unsigned depIdx = static_cast<unsigned>(idx);
      if (!codirDepUsesHaloStencilStorage(codelet, depIdx))
        continue;
      if (!canMaterializeRawCodirDependencyWithPlan(rootMemref, codelet))
        continue;
      if (rawCodirDependencyNeedsHostBridge(rootMemref) &&
          !codirDepRequiresComputeBlockStorage(codelet, depIdx))
        continue;
      for (CodirOwnerHaloWindow window :
           getCodirOwnerHaloWindows(codelet, depIdx, memrefRank,
                                    /*requireReadOnly=*/false))
        mergeCodirOwnerHaloWindow(unionWindows, window);
    }
  });

  return unionWindows;
}

static inline SmallVector<CodirOwnerHaloWindow, 4>
getCodirBlockStorageHaloWindows(codir::CodeletOp codelet, unsigned depIndex,
                                unsigned memrefRank) {
  SmallVector<CodirOwnerHaloWindow, 4> windows;
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!codelet || depIndex >= codelet.getDeps().size() || !ownerDims)
    return windows;

  SmallVector<CodirOwnerHaloWindow, 4> storageWindows =
      codirBackingBufferHaloWindows(
          getCodirHaloStorageRoot(codelet.getDeps()[depIndex]), memrefRank);
  windows.reserve(ownerDims->size());
  for (unsigned ownerDim : *ownerDims) {
    CodirOwnerHaloWindow resolved;
    resolved.ownerDim = ownerDim;
    for (const CodirOwnerHaloWindow &candidate : storageWindows) {
      if (candidate.ownerDim == ownerDim) {
        resolved = candidate;
        break;
      }
    }
    windows.push_back(resolved);
  }
  return windows;
}

static inline std::optional<unsigned>
getCodirRankExpandedHaloStorageDim(codir::CodeletOp codelet, unsigned depIndex,
                                   unsigned ownerSlot, unsigned memrefRank) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(codir::getDepPhysicalBlockShapeAttr(codelet, depIndex));
  if (!ownerDims || ownerDims->empty() || ownerSlot >= ownerDims->size() ||
      !blockShape || blockShape->size() != memrefRank)
    return std::nullopt;

  unsigned ownerRank = static_cast<unsigned>(ownerDims->size());
  if (ownerRank + ownerSlot >= memrefRank)
    return std::nullopt;

  for (auto [slot, ownerDim] : llvm::enumerate(*ownerDims)) {
    if (ownerDim != slot || ownerDim >= blockShape->size())
      return std::nullopt;
    if ((*blockShape)[ownerDim] != 1)
      return std::nullopt;
  }

  unsigned storageDim = ownerRank + ownerSlot;
  if (storageDim >= blockShape->size() || (*blockShape)[storageDim] <= 1)
    return std::nullopt;
  return storageDim;
}

static inline unsigned getCodirHaloStorageDim(codir::CodeletOp codelet,
                                              unsigned depIndex,
                                              unsigned ownerSlot,
                                              unsigned ownerDim,
                                              unsigned memrefRank) {
  if (std::optional<unsigned> storageDim = getCodirRankExpandedHaloStorageDim(
          codelet, depIndex, ownerSlot, memrefRank))
    return *storageDim;
  return ownerDim;
}

static inline std::optional<int64_t>
getCodirHaloStorageBlockSize(codir::CodeletOp codelet, unsigned depIndex,
                             unsigned ownerSlot, unsigned memrefRank,
                             int64_t ownerBlockSize) {
  if (std::optional<unsigned> storageDim = getCodirRankExpandedHaloStorageDim(
          codelet, depIndex, ownerSlot, memrefRank)) {
    std::optional<SmallVector<int64_t, 4>> blockShape = readI64ArrayAttr(
        codir::getDepPhysicalBlockShapeAttr(codelet, depIndex));
    if (!blockShape || *storageDim >= blockShape->size() ||
        (*blockShape)[*storageDim] <= 0)
      return std::nullopt;
    return (*blockShape)[*storageDim];
  }
  return ownerBlockSize > 0 ? std::optional<int64_t>(ownerBlockSize)
                            : std::nullopt;
}

static inline bool codirAllocUsesProjectedRankExpandedHaloStorage(
    codir::CodeletOp codelet, unsigned depIndex, arts::DbAllocOp alloc,
    unsigned ownerSlot, unsigned ownerDim, unsigned memrefRank,
    int64_t tileExtent, CodirOwnerHaloWindow halo) {
  if (!alloc || tileExtent <= 0 || halo.empty())
    return false;
  std::optional<unsigned> storageDim = getCodirRankExpandedHaloStorageDim(
      codelet, depIndex, ownerSlot, memrefRank);
  if (!storageDim || ownerDim >= alloc.getElementSizes().size() ||
      *storageDim >= alloc.getElementSizes().size())
    return false;
  std::optional<int64_t> ownerElements =
      ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(
          alloc.getElementSizes()[ownerDim]);
  std::optional<int64_t> storageElements =
      ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(
          alloc.getElementSizes()[*storageDim]);
  if (!ownerElements || !storageElements || *ownerElements != 1)
    return false;
  return *storageElements >= tileExtent + halo.lower + halo.upper;
}

static inline ArrayAttr
buildSymmetricPlanHaloShapeAttr(MLIRContext *context,
                                ArrayRef<CodirOwnerHaloWindow> ownerHalos) {
  SmallVector<int64_t, 4> haloShape;
  haloShape.reserve(ownerHalos.size());
  bool hasHalo = false;
  for (const CodirOwnerHaloWindow &halo : ownerHalos) {
    if (halo.lower != halo.upper)
      return {};
    hasHalo |= halo.lower > 0;
    haloShape.push_back(std::max<int64_t>(0, halo.lower));
  }
  if (!hasHalo)
    return {};
  return buildI64ArrayAttr(context, haloShape);
}

static inline CodirOwnerHaloWindow
getCodirBlockStorageHaloWindowForDim(codir::CodeletOp codelet,
                                     unsigned depIndex, unsigned ownerDim,
                                     unsigned memrefRank) {
  for (const CodirOwnerHaloWindow &window :
       getCodirBlockStorageHaloWindows(codelet, depIndex, memrefRank)) {
    if (window.ownerDim == ownerDim)
      return window;
  }
  CodirOwnerHaloWindow empty;
  empty.ownerDim = ownerDim;
  return empty;
}

// Use the DB allocation padding as the storage-halo authority when read/write
// dependency values are split before CODIR.
static inline CodirOwnerHaloWindow
blockAllocStorageHaloForDim(arts::DbAllocOp blockAlloc, unsigned ownerDim) {
  CodirOwnerHaloWindow window;
  window.ownerDim = ownerDim;
  if (!blockAlloc)
    return window;
  Operation *op = blockAlloc.getOperation();
  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(arts::getPlanOwnerDimsAttr(op));
  std::optional<SmallVector<int64_t, 4>> haloShape =
      readI64ArrayAttr(arts::getPlanHaloShapeAttr(op));
  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(arts::getPlanPhysicalBlockShapeAttr(op));
  if (!ownerDims || !blockShape)
    return window;
  int slot = -1;
  for (auto [i, d] : llvm::enumerate(*ownerDims))
    if (d >= 0 && static_cast<unsigned>(d) == ownerDim) {
      slot = static_cast<int>(i);
      break;
    }
  if (slot < 0 || static_cast<size_t>(slot) >= blockShape->size())
    return window;
  int64_t block = (*blockShape)[slot];
  ValueRange elementSizes = blockAlloc.getElementSizes();
  if (ownerDim >= elementSizes.size())
    return window;
  std::optional<int64_t> elem =
      ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(
          elementSizes[ownerDim]);
  if (!elem)
    return window;
  int64_t halo = 0;
  if (haloShape && static_cast<size_t>(slot) < haloShape->size()) {
    if (ownerDim < haloShape->size() && haloShape->size() != ownerDims->size())
      halo = (*haloShape)[ownerDim];
    else
      halo = (*haloShape)[slot];
  } else if (op->hasAttr(blockAlloc.getStencilSupportedBlockHaloAttrName())) {
    int64_t padded = *elem - block;
    if (padded <= 0 || padded % 2 != 0)
      return window;
    halo = padded / 2;
  }
  if (halo <= 0 || *elem < block + 2 * halo)
    return window;
  window.lower = halo;
  window.upper = halo;
  return window;
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HALOSTORAGE_H
