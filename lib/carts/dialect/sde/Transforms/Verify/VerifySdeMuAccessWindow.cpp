///==========================================================================///
/// File: VerifySdeMuAccessWindow.cpp
///
/// Verifies that per-CU MU access windows cover and describe rank-expanded
/// block-grid MUs.
///
///   R1 — coverage: every in-scope (MU, CU, mode) access planned by
///        `planAccessWindows` has EXACTLY ONE `sde.mu_access_window` in its
///        enclosing `sde.cu_region`. Zero => the raiser did not run; more than
///        one => the idempotency guard is broken.
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

    // R1 — coverage: each in-scope per-CU MU access has exactly one matching
    // window in that CU.
    module.walk(
        [&](sde::SdeMuAllocOp mu) {
          llvm::SmallVector<sde::RaisedWindowPlan, 4> plans =
              sde::planAccessWindows(mu);
          if (plans.empty())
            return; // out of scope -> no window required
          for (const sde::RaisedWindowPlan &plan : plans) {
            unsigned countModeMatch = 0;
            sde::SdeCuRegionOp cu = plan.cu;
            for (Operation &op : cu.getBody().front())
              if (auto win = dyn_cast<sde::SdeMuAccessWindowOp>(op))
                if (win.getMu() == plan.mu && win.getMode() == plan.mode)
                  ++countModeMatch;
            if (countModeMatch == 0) {
              mu.emitOpError()
                  << "converted block-grid MU is in scope but has no "
                     "sde.mu_access_window for "
                  << sde::stringifySdeAccessMode(plan.mode)
                  << " access in its enclosing cu_region; run "
                     "raise-to-mu-access-window first";
              failed = true;
            } else if (countModeMatch > 1) {
              mu.emitOpError()
                  << "duplicate sde.mu_access_window for this MU/mode in its "
                     "cu_region (raise-to-mu-access-window idempotency broken)";
              failed = true;
            }
          }
        });

    module.walk([&](sde::SdeCuRegionOp cu) {
      for (Operation &op : cu.getBody().front()) {
        auto win = dyn_cast<sde::SdeMuAccessWindowOp>(op);
        if (!win)
          continue;
        if (auto muAlloc = win.getMu().getDefiningOp<sde::SdeMuAllocOp>()) {
          llvm::SmallVector<sde::RaisedWindowPlan, 4> expected =
              sde::planAccessWindows(muAlloc);
          bool hasExpected = false;
          for (const sde::RaisedWindowPlan &plan : expected)
            if (plan.cu == cu && plan.mu == win.getMu() &&
                plan.mode == win.getMode()) {
              hasExpected = true;
              break;
            }
          if (!hasExpected) {
            win.emitOpError()
                << "does not match any in-scope per-CU access-window plan";
            failed = true;
          }
        }
        unsigned duplicates = 0;
        for (Operation &otherOp : cu.getBody().front()) {
          auto other = dyn_cast<sde::SdeMuAccessWindowOp>(otherOp);
          if (other && other.getMu() == win.getMu() &&
              other.getMode() == win.getMode())
            ++duplicates;
        }
        if (duplicates > 1) {
          win.emitOpError()
              << "duplicate sde.mu_access_window for this MU/mode in one "
                 "cu_region";
          failed = true;
        }
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
      if (!ownerVals || !blockVals || !blockHi || ownerVals->empty() ||
          blockHi->size() != ownerVals->size())
        return;
      // The window's blockHi carries the per-owner grid counts (owner order,
      // ND); cross-check each against a DISTINCT committed iteration extent.
      SmallVector<int64_t, 4> blockExtents;
      blockExtents.reserve(ownerVals->size());
      for (int64_t ownerDim : *ownerVals) {
        if (ownerDim < 0 || static_cast<size_t>(ownerDim) >= blockVals->size())
          return;
        blockExtents.push_back((*blockVals)[ownerDim]);
      }
      if (!sde::findOwnerIterationExtents(si, blockExtents, *blockHi)) {
        win.emitOpError()
            << "blockHi grid counts are not ceilDiv(iterationExtent, block) of "
               "distinct committed iteration extents on the writer su_iterate; "
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
