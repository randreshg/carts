///==========================================================================///
/// File: CodirToArtsDepFacts.h
///
/// CODIR dependency fact readers and access-mode predicates.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_DEPFACTS_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_DEPFACTS_H

#include "CodirToArtsBlockLocalRewrite.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/codir/Utils/CodirConversionUtils.h"
#include "carts/utils/Utils.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/ADT/STLExtras.h"

namespace {

static inline arts::ArtsMode convertAccessMode(codir::CodirAccessMode mode) {
  switch (mode) {
  case codir::CodirAccessMode::read:
    return arts::ArtsMode::in;
  case codir::CodirAccessMode::write:
    return arts::ArtsMode::out;
  case codir::CodirAccessMode::readwrite:
    return arts::ArtsMode::inout;
  }
  return arts::ArtsMode::inout;
}

// Tier-1 enums (BarrierReason, ReductionStrategy, IterationTopology) have
// identical case sets and integer codes across SDE, CODIR, and ARTS
// (audit-verified). Call sites translate via static_cast guarded by these
// invariants.
static_assert(static_cast<int>(sde::SdeBarrierReason::unknown_required) ==
              static_cast<int>(arts::ArtsBarrierReason::unknown_required));
static_assert(
    static_cast<int>(codir::CodirReductionStrategy::local_accumulate) ==
    static_cast<int>(arts::ArtsReductionStrategy::local_accumulate));
static_assert(static_cast<int>(codir::CodirIterationTopology::owner_tile_2d) ==
              static_cast<int>(arts::ArtsPlanIterationTopology::owner_tile_2d));

static inline std::optional<codir::CodirStorageViewKind>
getCodirDepStorageViewKind(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr views = codelet ? codelet.getDepStorageViewsAttr() : ArrayAttr{};
  if (!views || depIndex >= views.size())
    return std::nullopt;
  auto view = dyn_cast<codir::CodirStorageViewKindAttr>(views[depIndex]);
  if (!view)
    return std::nullopt;
  return view.getValue();
}

/// First-class collective family for |codelet|'s |depIndex|. Absence means the
/// CODIR storage/collective facts have not been finalized.
static inline std::optional<codir::CodirCollectiveKind>
getCodirDepCollectiveKind(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr collectives =
      codelet ? codelet.getDepCollectivesAttr() : ArrayAttr{};
  if (!collectives || depIndex >= collectives.size())
    return std::nullopt;
  auto kind = dyn_cast<codir::CodirCollectiveKindAttr>(collectives[depIndex]);
  if (!kind)
    return std::nullopt;
  return kind.getValue();
}

static inline codir::CodirCollectiveKind
getFinalizedCodirDepCollectiveKind(codir::CodeletOp codelet,
                                   unsigned depIndex) {
  return getCodirDepCollectiveKind(codelet, depIndex)
      .value_or(codir::CodirCollectiveKind::none);
}

static inline std::optional<codir::CodirAccessMode>
getCodirDepAccessMode(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr modes = codelet ? codelet.getDepModesAttr() : ArrayAttr{};
  if (!modes || depIndex >= modes.size())
    return std::nullopt;
  auto mode = dyn_cast<codir::CodirAccessModeAttr>(modes[depIndex]);
  if (!mode)
    return std::nullopt;
  return mode.getValue();
}

static inline bool codirAccessMayWrite(codir::CodirAccessMode mode) {
  return mode == codir::CodirAccessMode::write ||
         mode == codir::CodirAccessMode::readwrite;
}

static inline bool codirAccessMayRead(codir::CodirAccessMode mode) {
  return mode == codir::CodirAccessMode::read ||
         mode == codir::CodirAccessMode::readwrite;
}

static inline bool
codirStorageViewUsesComputeBlock(codir::CodirStorageViewKind view) {
  return view == codir::CodirStorageViewKind::compute_block ||
         view == codir::CodirStorageViewKind::phase_redistributed;
}

static inline bool codirDepAllowsComputeBlockStorage(codir::CodeletOp codelet,
                                                     unsigned depIndex) {
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  return view && codirStorageViewUsesComputeBlock(*view);
}

static inline bool codirDepRequiresComputeBlockStorage(codir::CodeletOp codelet,
                                                       unsigned depIndex) {
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  return view && codirStorageViewUsesComputeBlock(*view);
}

static inline bool
codirDepRequiresPhaseRedistributionBridge(codir::CodeletOp codelet,
                                          unsigned depIndex) {
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  return view && *view == codir::CodirStorageViewKind::phase_redistributed;
}
} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_DEPFACTS_H
