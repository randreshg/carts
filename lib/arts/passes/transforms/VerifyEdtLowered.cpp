///==========================================================================///
/// File: VerifyEdtLowered.cpp
///
/// Verification pass that ensures no arts.edt operations survive past EDT
/// lowering.
///==========================================================================///

#include "arts/Dialect.h"
#define GEN_PASS_DEF_VERIFYEDTLOWERED
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "mlir/Pass/Pass.h"

using namespace mlir;

namespace {
struct VerifyEdtLoweredPass
    : public impl::VerifyEdtLoweredBase<VerifyEdtLoweredPass> {
  void runOnOperation() override {
    bool found = false;
    getOperation().walk([&](arts::EdtOp op) {
      op.emitError("arts.edt survived past EDT lowering");
      found = true;
    });
    if (found)
      signalPassFailure();
  }
};
} // namespace

std::unique_ptr<Pass> mlir::arts::createVerifyEdtLoweredPass() {
  return std::make_unique<VerifyEdtLoweredPass>();
}
