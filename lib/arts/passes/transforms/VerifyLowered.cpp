///==========================================================================///
/// File: VerifyLowered.cpp
///
/// Verification pass that ensures no high-level ARTS dialect operations
/// survive past the arts-to-llvm conversion. Any remaining EdtOp, ForOp,
/// DbAllocOp, DbAcquireOp, DbReleaseOp, or EpochOp indicates a lowering
/// bug.
///==========================================================================///

#include "arts/Dialect.h"
#define GEN_PASS_DEF_VERIFYLOWERED
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "arts/passes/Passes.h.inc"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;

namespace {
struct VerifyLoweredPass : public arts::impl::VerifyLoweredBase<VerifyLoweredPass> {
  void runOnOperation() override {
    bool found = false;
    getOperation().walk([&](Operation *op) {
      if (isArtsOp(op)) {
        op->emitError("high-level ARTS operation survived past arts-to-llvm");
        found = true;
      }
    });
    if (found)
      signalPassFailure();
  }
};
} // namespace

std::unique_ptr<Pass> mlir::arts::createVerifyLoweredPass() {
  return std::make_unique<VerifyLoweredPass>();
}
