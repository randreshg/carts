///==========================================================================///
/// File: RaiseToMuAccessWindow.cpp
///
/// SDE per-CU MU access-window raiser.
///
/// For every `sde.mu_alloc` that rank expansion converted into a
/// single-contiguous-owner block-grid MU (elementwise/stencil, fully static),
/// this pass raises one `sde.mu_access_window` fact near the top of each
/// enclosing `sde.cu_region`, after any same-block `sde.mu_alloc` it names, per
/// (MU root, read/write mode), in the SAME block-grid coordinate system the
/// carrier type already encodes.
///
/// It is a pure ADDITIVE raiser: it reads the committed plan + expanded type
/// VERBATIM (via the shared `planAccessWindows` query — it never recomputes
/// owner dims or block shape), proves the structure against the writer
/// su_iterate iteration domain, and inserts explicit window structure.
/// Out-of-scope MUs/CU accesses (dynamic, matmul/reduction, multi-owner,
/// same-CU in-place, unsupported use) are skipped conservatively — no window,
/// no error, no existing op mutated. It introduces no `sde.mu_token`, slice,
/// `sde.mu_dep`, or CODIR concept.
///==========================================================================///

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/MuAccessWindow.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_SDERAISETOMUACCESSWINDOW
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallVector.h"

#include <memory>

using namespace mlir;

namespace {

static Operation *findWindowInsertionPoint(carts::sde::SdeCuRegionOp cu,
                                           Value mu) {
  Block &body = cu.getBody().front();
  if (Operation *def = mu.getDefiningOp())
    if (def->getBlock() == &body)
      return def->getNextNode();

  for (Operation &op : body)
    if (!isa<carts::sde::SdeMuAccessWindowOp>(op))
      return &op;
  return nullptr;
}

struct RaiseToMuAccessWindowPass
    : public carts::sde::impl::SdeRaiseToMuAccessWindowBase<
          RaiseToMuAccessWindowPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();

    llvm::SmallVector<carts::sde::SdeMuAllocOp> worklist;
    module.walk([&](carts::sde::SdeMuAllocOp mu) { worklist.push_back(mu); });

    for (carts::sde::SdeMuAllocOp mu : worklist) {
      llvm::SmallVector<carts::sde::RaisedWindowPlan, 4> plans =
          carts::sde::planAccessWindows(mu);
      if (plans.empty())
        continue; // out of scope -> conservative, no window

      for (const carts::sde::RaisedWindowPlan &plan : plans) {
        // Find the earliest dominance-safe insertion point and detect an
        // already-raised window for this (MU, mode) so the pass is idempotent.
        carts::sde::SdeCuRegionOp cu = plan.cu;
        Block &body = cu.getBody().front();
        bool exists = false;
        for (Operation &op : body) {
          if (auto win = dyn_cast<carts::sde::SdeMuAccessWindowOp>(op)) {
            if (win.getMu() == plan.mu && win.getMode() == plan.mode)
              exists = true;
          }
        }
        Operation *insertBefore = findWindowInsertionPoint(cu, plan.mu);
        if (exists || !insertBefore)
          continue;

        OpBuilder builder(insertBefore);
        carts::sde::SdeMuAccessWindowOp::create(
            builder, mu.getLoc(), plan.mu,
            carts::sde::SdeAccessModeAttr::get(ctx, plan.mode),
            builder.getI64IntegerAttr(plan.ownerDimCount),
            builder.getI64ArrayAttr(plan.blockLo),
            builder.getI64ArrayAttr(plan.blockHi),
            builder.getI64ArrayAttr(plan.validExtents));
      }
    }
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createRaiseToMuAccessWindowPass() {
  return std::make_unique<RaiseToMuAccessWindowPass>();
}
} // namespace mlir::carts::sde
