///==========================================================================///
/// File: CodirToArtsHaloWindows.h
///
/// CODIR owner-halo window readers and block-local halo base helpers.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HALOWINDOWS_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HALOWINDOWS_H

#include "CodirToArtsOwnerDomain.h"

namespace {

struct CodirOwnerHaloWindow {
  unsigned ownerDim = 0;
  int64_t lower = 0;
  int64_t upper = 0;

  bool empty() const { return lower <= 0 && upper <= 0; }
  int64_t width() const { return lower + upper; }
};

static inline std::optional<unsigned>
getCodirOwnerDimSlot(codir::CodeletOp codelet, unsigned ownerDim) {
  if (auto ownerDims = readI64ArrayAttr(codelet.getTileOwnerDimsAttr())) {
    for (auto [slot, rawDim] : llvm::enumerate(*ownerDims))
      if (rawDim >= 0 && static_cast<unsigned>(rawDim) == ownerDim)
        return static_cast<unsigned>(slot);
  }
  return std::nullopt;
}

static inline std::optional<int64_t>
getCodirOwnerDimValue(ArrayAttr attr, unsigned ownerDim,
                      std::optional<unsigned> ownerSlot, unsigned memrefRank) {
  std::optional<SmallVector<int64_t, 4>> values = readI64ArrayAttr(attr);
  if (!values || values->empty())
    return std::nullopt;
  if (values->size() == memrefRank && ownerDim < values->size())
    return (*values)[ownerDim];
  if (ownerSlot && *ownerSlot < values->size())
    return (*values)[*ownerSlot];
  if (values->size() == 1)
    return values->front();
  return std::nullopt;
}

static inline std::optional<unsigned>
getSingleCodirDepOwnerDim(codir::CodeletOp codelet, unsigned depIndex) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->size() != 1)
    return std::nullopt;
  return ownerDims->front();
}

static inline CodirOwnerHaloWindow
getCodirOwnerHaloWindowForDim(codir::CodeletOp codelet, unsigned depIndex,
                              unsigned ownerDim, unsigned memrefRank,
                              bool requireReadOnly = true) {
  CodirOwnerHaloWindow window;
  window.ownerDim = ownerDim;

  std::optional<codir::CodirAccessMode> mode =
      getCodirDepAccessMode(codelet, depIndex);
  if (requireReadOnly &&
      (!mode || !codirAccessMayRead(*mode) || codirAccessMayWrite(*mode)))
    return window;
  if (ownerDim >= memrefRank)
    return window;

  std::optional<unsigned> ownerSlot = getCodirOwnerDimSlot(codelet, ownerDim);

  if (auto minOffset = getCodirOwnerDimValue(codelet.getAccessMinOffsetsAttr(),
                                             ownerDim, ownerSlot, memrefRank))
    window.lower = std::max<int64_t>(0, -*minOffset);
  if (auto maxOffset = getCodirOwnerDimValue(codelet.getAccessMaxOffsetsAttr(),
                                             ownerDim, ownerSlot, memrefRank))
    window.upper = std::max<int64_t>(0, *maxOffset);

  if (!window.empty())
    return window;

  if (auto halo = getCodirOwnerDimValue(codelet.getHaloShapeAttr(), ownerDim,
                                        ownerSlot, memrefRank)) {
    int64_t radius = std::max<int64_t>(0, *halo);
    window.lower = radius;
    window.upper = radius;
  }

  return window;
}

static inline SmallVector<CodirOwnerHaloWindow, 4>
getCodirOwnerHaloWindows(codir::CodeletOp codelet, unsigned depIndex,
                         unsigned memrefRank, bool requireReadOnly = true) {
  SmallVector<CodirOwnerHaloWindow, 4> windows;
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims)
    return windows;

  windows.reserve(ownerDims->size());
  for (unsigned ownerDim : *ownerDims)
    windows.push_back(getCodirOwnerHaloWindowForDim(
        codelet, depIndex, ownerDim, memrefRank, requireReadOnly));
  return windows;
}

static inline CodirOwnerHaloWindow
getCodirOwnerHaloWindow(codir::CodeletOp codelet, unsigned depIndex,
                        unsigned memrefRank) {
  std::optional<unsigned> ownerDim =
      getSingleCodirDepOwnerDim(codelet, depIndex);
  if (!ownerDim)
    return {};
  return getCodirOwnerHaloWindowForDim(codelet, depIndex, *ownerDim,
                                       memrefRank);
}

static inline Value
materializeCodirBlockLocalBase(OpBuilder &builder, Location loc,
                               codir::CodeletOp codelet, unsigned depIndex,
                               Value ownerBase, unsigned memrefRank) {
  CodirOwnerHaloWindow halo =
      getCodirOwnerHaloWindow(codelet, depIndex, memrefRank);
  if (halo.empty())
    return ownerBase;
  return subtractClampZero(builder, loc, ownerBase, halo.lower);
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HALOWINDOWS_H
