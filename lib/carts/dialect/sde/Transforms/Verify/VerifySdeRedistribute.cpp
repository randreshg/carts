///==========================================================================///
/// File: VerifySdeRedistribute.cpp
///
/// Fail-closed gate for SDE redistribution realization.
///
/// Using the same committed-edge analysis `sde-redistribute` emits from
/// (`collectRedistributionEdges`), this verifier keeps a committed
/// redistribution edge from being dropped or invented:
///
///   * COMPLETENESS — every committed `layoutsDisagree` edge is either
///     represented by a matching `sde.redist` op or is an unrepresentable edge
///     left coarse; both are errors (a movement edge must not silently vanish).
///   * GROUNDING — every `sde.redist` op matches some committed edge, so no
///     endpoint or family is invented.
///
/// It reads committed facts verbatim and never recomputes owner dims, block
/// shape, or family.
///==========================================================================///

#include "carts/dialect/sde/Analysis/RedistributionEdges.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYSDEREDISTRIBUTE
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

struct VerifySdeRedistributePass
    : public sde::impl::VerifySdeRedistributeBase<VerifySdeRedistributePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    sde::RedistributionEdges committed =
        sde::collectRedistributionEdges(module);
    bool failed = false;

    // Completeness: an unrepresentable edge is a coarse fallback the gate must
    // surface; a representable edge must carry a matching sde.redist.
    for (const sde::RedistributionEdgeFailure &f : committed.failures) {
      sde::SdeSuIterateOp consumer = f.consumer;
      consumer.emitOpError()
          << "verify-sde-redistribute: redistribution edge for array "
          << f.arrayId << " is not representable and was left coarse: "
          << f.reason;
      failed = true;
    }
    for (const sde::RedistributionEdge &edge : committed.edges) {
      bool represented = false;
      for (Operation *user : edge.root.getUsers())
        if (auto redist = dyn_cast<sde::SdeRedistOp>(user))
          if (sde::redistMatchesEdge(redist, edge)) {
            represented = true;
            break;
          }
      if (!represented) {
        sde::SdeSuIterateOp consumer = edge.consumer;
        consumer.emitOpError()
            << "verify-sde-redistribute: committed redistribution edge for "
               "array "
            << edge.arrayId << " is not represented as sde.redist";
        failed = true;
      }
    }

    // Grounding: every sde.redist must match a committed edge.
    module.walk([&](sde::SdeRedistOp redist) {
      for (const sde::RedistributionEdge &edge : committed.edges)
        if (sde::redistMatchesEdge(redist, edge))
          return;
      redist.emitOpError()
          << "verify-sde-redistribute: sde.redist is not grounded in a "
             "committed layout-disagreement edge";
      failed = true;
    });

    if (failed)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createVerifySdeRedistributePass() {
  return std::make_unique<VerifySdeRedistributePass>();
}
} // namespace mlir::carts::sde
