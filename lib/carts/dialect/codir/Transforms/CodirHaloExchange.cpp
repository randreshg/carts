///==========================================================================///
/// File: CodirHaloExchange.cpp
///
/// Inserts explicit CODIR halo exchange nodes for finalized halo deps.
///
/// Before:
///   codir.codelet deps(%src : memref<...>) attributes {
///     dep_collectives = [#codir.collective<halo>],
///     dep_storage_views = [#codir.storage_view<compute_block>],
///     halo_shape = [1, 1]
///   } { ... }
///
/// After:
///   %halo = codir.halo_exchange %src : memref<...> -> memref<...>
///   codir.codelet deps(%halo : memref<...>) attributes {
///     dep_collectives = [#codir.collective<halo>],
///     dep_storage_views = [#codir.storage_view<compute_block>],
///     halo_shape = [1, 1]
///   } { ... }
///
/// CODIR owns this graph rewrite. CODIR-to-ARTS only lowers the explicit
/// movement op; it does not infer stencil halo movement from codelet attrs.
///==========================================================================///

#include "carts/dialect/codir/Transforms/Passes.h"

#include "carts/dialect/codir/Utils/CodeletABIUtils.h"
#include "carts/utils/ArrayAttrUtils.h"

namespace mlir::carts::codir {
#define GEN_PASS_DEF_CODIRHALOEXCHANGE
#include "carts/dialect/codir/Transforms/Passes.h.inc"
} // namespace mlir::carts::codir

using namespace mlir;
using namespace mlir::carts;

namespace {

static bool accessMayRead(codir::CodirAccessMode mode) {
  return mode == codir::CodirAccessMode::read ||
         mode == codir::CodirAccessMode::readwrite;
}

static bool storageUsesComputeBlock(codir::CodirStorageViewKind view) {
  return view == codir::CodirStorageViewKind::compute_block ||
         view == codir::CodirStorageViewKind::phase_redistributed;
}

static std::optional<codir::CodirCollectiveKind>
getDepCollective(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr collectives =
      codelet ? codelet.getDepCollectivesAttr() : ArrayAttr{};
  if (!collectives || depIndex >= collectives.size())
    return std::nullopt;
  auto collective =
      dyn_cast<codir::CodirCollectiveKindAttr>(collectives[depIndex]);
  if (!collective)
    return std::nullopt;
  return collective.getValue();
}

static bool attrHasNonZeroI64(ArrayAttr attr) {
  std::optional<SmallVector<int64_t, 4>> values = readI64ArrayAttr(attr);
  return values && llvm::any_of(*values, [](int64_t value) {
           return value != 0;
         });
}

static bool hasHaloReach(codir::CodeletOp codelet) {
  return attrHasNonZeroI64(codelet.getHaloShapeAttr()) ||
         attrHasNonZeroI64(codelet.getAccessMinOffsetsAttr()) ||
         attrHasNonZeroI64(codelet.getAccessMaxOffsetsAttr());
}

static bool needsHaloExchange(codir::CodeletOp codelet, unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  std::optional<codir::CodirAccessMode> mode =
      codir::getDepAccessMode(codelet, depIndex);
  if (!mode || !accessMayRead(*mode))
    return false;
  std::optional<codir::CodirStorageViewKind> view =
      codir::getDepStorageViewKind(codelet, depIndex);
  if (!view || !storageUsesComputeBlock(*view))
    return false;
  std::optional<codir::CodirCollectiveKind> collective =
      getDepCollective(codelet, depIndex);
  return collective && *collective == codir::CodirCollectiveKind::halo &&
         hasHaloReach(codelet);
}

struct CodirHaloExchangePass
    : public codir::impl::CodirHaloExchangeBase<CodirHaloExchangePass> {
  void runOnOperation() override {
    SmallVector<codir::CodeletOp> codelets;
    getOperation().walk(
        [&](codir::CodeletOp codelet) { codelets.push_back(codelet); });

    for (codir::CodeletOp codelet : codelets) {
      for (auto [index, dep] : llvm::enumerate(codelet.getDeps())) {
        unsigned depIndex = static_cast<unsigned>(index);
        if (!needsHaloExchange(codelet, depIndex))
          continue;
        if (dep.getDefiningOp<codir::HaloExchangeOp>())
          continue;

        OpBuilder builder(codelet);
        auto exchange = codir::HaloExchangeOp::create(
            builder, codelet.getLoc(), dep.getType(), dep);
        codelet->setOperand(depIndex, exchange.getResult());
      }
    }
  }
};

} // namespace

std::unique_ptr<Pass>
mlir::carts::codir::createCodirHaloExchangePass() {
  return std::make_unique<CodirHaloExchangePass>();
}
