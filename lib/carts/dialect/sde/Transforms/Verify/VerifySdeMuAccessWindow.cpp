///==========================================================================///
/// File: VerifySdeMuAccessWindow.cpp
///
/// Verifies that per-CU MU access windows cover and describe rank-expanded
/// block-grid MUs.
///
///   R1 — coverage: every in-scope MU (a converted single-owner block-grid
///        MU read or written by exactly one elementwise/stencil CU, not in
///        place — exactly the `planAccessWindow` predicate the raiser uses) has
///        EXACTLY ONE `sde.mu_access_window` in its enclosing `sde.cu_region`.
///        Zero => the raiser did not run; more than one => the idempotency
///        guard is broken.
///
///   R2 — grain consistency: every `sde.mu_access_window`'s `blockHi` is the
///        `ceilDiv` of a REAL iteration extent on the writer `su_iterate` (an
///        INDEPENDENT fact, never re-derived from the window), so the window
///        cannot silently encode a recomputed grain. (Block/valid-range bounds
///        are already enforced by the op's own ODS verifier.)
///
/// Conservative (out-of-scope) MUs are skipped, never rejected.
///==========================================================================///

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/utils/ArrayAttrUtils.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYSDEMUACCESSWINDOW
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

struct VerifySdeMuAccessWindowPass
    : public sde::impl::VerifySdeMuAccessWindowBase<
          VerifySdeMuAccessWindowPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool failed = false;

    // R1 — coverage: each in-scope MU has exactly one window in its CU.
    module.walk([&](sde::SdeMuAllocOp mu) {
      std::optional<sde::RaisedWindowPlan> plan = sde::planAccessWindow(mu);
      if (!plan)
        return; // out of scope -> no window required
      unsigned countMu = 0, countModeMatch = 0;
      for (Operation &op : plan->cu.getBody().front())
        if (auto win = dyn_cast<sde::SdeMuAccessWindowOp>(op))
          if (win.getMu() == plan->mu) {
            ++countMu;
            if (win.getMode() == plan->mode)
              ++countModeMatch;
          }
      if (countMu == 0) {
        mu.emitOpError()
            << "converted block-grid MU is in scope but has no "
               "sde.mu_access_window in its enclosing cu_region; run "
               "raise-to-mu-access-window first";
        failed = true;
      } else if (countMu > 1) {
        mu.emitOpError()
            << "duplicate sde.mu_access_window for this MU in its cu_region "
               "(raise-to-mu-access-window idempotency broken)";
        failed = true;
      } else if (countModeMatch == 0) {
        // Exactly one window, but its mode disagrees with how the CU actually
        // accesses the MU (a window must faithfully describe the access).
        mu.emitOpError() << "sde.mu_access_window has the wrong access mode; "
                            "the CU accesses "
                            "this MU as "
                         << sde::stringifySdeAccessMode(plan->mode);
        failed = true;
      }
    });

    // R2 — non-tautological grain: blockHi == ceilDiv(real iteration extent,
    // committed block) on the writer su_iterate.
    module.walk([&](sde::SdeMuAccessWindowOp win) {
      auto muAlloc = win.getMu().getDefiningOp<sde::SdeMuAllocOp>();
      if (!muAlloc)
        return; // the op's ODS verifier already requires an sde.mu_alloc root
      sde::SdeSuIterateOp si = sde::findCommittedBlockPlanWriter(muAlloc);
      if (!si)
        return; // no committed writer to cross-check against -> conservative
      std::optional<SmallVector<int64_t, 4>> ownerVals =
          readI64ArrayAttr(si.getPhysicalOwnerDimsAttr());
      std::optional<SmallVector<int64_t, 4>> blockVals =
          readI64ArrayAttr(si.getPhysicalBlockShapeAttr());
      std::optional<SmallVector<int64_t, 4>> blockHi =
          readI64ArrayAttr(win.getBlockHi());
      if (!ownerVals || !blockVals || !blockHi || ownerVals->size() != 1 ||
          blockHi->size() != 1)
        return;
      int64_t ownerDim = (*ownerVals)[0];
      if (ownerDim < 0 || static_cast<size_t>(ownerDim) >= blockVals->size())
        return;
      int64_t blockExtent = (*blockVals)[ownerDim];
      if (!sde::findOwnerIterationExtent(si, blockExtent, (*blockHi)[0])) {
        win.emitOpError()
            << "blockHi=" << (*blockHi)[0]
            << " is not ceilDiv(iterationExtent, " << blockExtent
            << ") of any committed iteration extent on the writer su_iterate; "
               "access-window verification must not recompute the grain";
        failed = true;
      }
    });

    if (failed)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createVerifySdeMuAccessWindowPass() {
  return std::make_unique<VerifySdeMuAccessWindowPass>();
}
} // namespace mlir::carts::sde
