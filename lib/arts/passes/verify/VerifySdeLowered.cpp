///==========================================================================///
/// File: VerifySdeLowered.cpp
///
/// Verification pass that ensures no sde.* operations survive after
/// SDE-to-ARTS conversion. Any remaining SDE ops indicate a conversion
/// failure.
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/dialect/sde/IR/SdeDialect.h"
#define GEN_PASS_DEF_VERIFYSDELOWERED
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "mlir/Pass/Pass.h"

using namespace mlir;

namespace {
struct VerifySdeLoweredPass
    : public impl::VerifySdeLoweredBase<VerifySdeLoweredPass> {
  void runOnOperation() override {
    bool failed = false;
    getOperation().walk([&](Operation *op) {
      if (op->getDialect() &&
          op->getDialect()->getNamespace() == "sde") {
        op->emitError() << "sde operation '" << op->getName()
                        << "' survived past SDE-to-ARTS conversion";
        failed = true;
      }
    });
    if (failed)
      signalPassFailure();
  }
};
} // namespace

std::unique_ptr<Pass> mlir::arts::createVerifySdeLoweredPass() {
  return std::make_unique<VerifySdeLoweredPass>();
}
