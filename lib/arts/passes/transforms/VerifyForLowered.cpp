///==========================================================================///
/// File: VerifyForLowered.cpp
///
/// Verification pass that ensures no arts.for operations survive past the
/// edt-distribution stage. This catches missing ForLowering patterns early
/// instead of failing in later LLVM lowering.
///==========================================================================///

#include "arts/Dialect.h"
#define GEN_PASS_DEF_VERIFYFORLOWERED
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "arts/passes/Passes.h.inc"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;

namespace {
struct VerifyForLoweredPass
    : public arts::impl::VerifyForLoweredBase<VerifyForLoweredPass> {
  void runOnOperation() override {
    bool found = false;
    getOperation().walk([&](arts::ForOp op) {
      op.emitError("arts.for survived past edt-distribution stage");
      found = true;
    });
    if (found)
      signalPassFailure();
  }
};
} // namespace

std::unique_ptr<Pass> mlir::arts::createVerifyForLoweredPass() {
  return std::make_unique<VerifyForLoweredPass>();
}
