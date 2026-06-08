///==========================================================================///
/// File: Redistribute.cpp
///
/// SDE redistribution realization.
///
/// `sde-layout-assignment` records, on each accessing `sde.su_iterate`, the
/// committed per-array home BLOCK layout (`arrayLayout`) plus a metadata-only
/// `layoutsDisagree` marker naming the array roots whose consumer access does
/// not align with that home. This pass emits `sde.redist` only for committed
/// disagreements that can be grounded as cross-owner reductions. Other
/// disagreements remain broad layout evidence until SDE can materialize them
/// directly. It introduces no `sde.mu_token`, slice, `sde.mu_dep`, owner map,
/// DB, route, collective, or CODIR isolation concept.
///==========================================================================///

#include "carts/dialect/sde/Analysis/RedistributionEdges.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/utils/ArrayAttrUtils.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_SDEREDISTRIBUTE
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#include <memory>

using namespace mlir;
using namespace mlir::carts;

namespace {

/// True if `root` already carries a redistribution fact equal to `edge`
/// (idempotence across re-runs and across consumers with the same target).
static bool alreadyRepresented(const carts::sde::RedistributionEdge &edge) {
  for (Operation *user : edge.root.getUsers())
    if (auto redist = dyn_cast<carts::sde::SdeRedistOp>(user))
      if (carts::sde::redistMatchesEdge(redist, edge))
        return true;
  return false;
}

struct SdeRedistributePass
    : public carts::sde::impl::SdeRedistributeBase<SdeRedistributePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();

    carts::sde::RedistributionEdges committed =
        carts::sde::collectRedistributionEdges(module);

    for (const carts::sde::RedistributionEdge &edge : committed.edges) {
      if (alreadyRepresented(edge))
        continue;
      IntegerAttr costAttr;
      if (edge.commVolumeBytes > 0)
        costAttr =
            IntegerAttr::get(IntegerType::get(ctx, 64), edge.commVolumeBytes);
      carts::sde::SdeSuIterateOp consumer = edge.consumer;
      OpBuilder builder(consumer);
      builder.create<carts::sde::SdeRedistOp>(
          consumer.getLoc(), edge.root,
          carts::sde::SdeMovementFamilyAttr::get(ctx, edge.family),
          buildI64ArrayAttr(ctx, edge.sourceOwnerDims),
          buildI64ArrayAttr(ctx, edge.sourceBlockShape),
          buildI64ArrayAttr(ctx, edge.targetOwnerDims),
          buildI64ArrayAttr(ctx, edge.targetBlockShape),
          /*haloShape=*/ArrayAttr(), /*commVolumeBytes=*/costAttr);
    }
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createSdeRedistributePass() {
  return std::make_unique<SdeRedistributePass>();
}
} // namespace mlir::carts::sde
