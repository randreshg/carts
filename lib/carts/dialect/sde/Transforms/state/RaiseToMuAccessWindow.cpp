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
/// It is a pure ADDITIVE raiser: it reads the committed layout shape and
/// expanded type VERBATIM (via the shared `queryAccessWindows` query — it
/// never recomputes owner dims or block shape), proves the structure against
/// the writer su_iterate iteration domain, and inserts explicit window
/// structure.
/// Out-of-scope MUs/CU accesses (dynamic, matmul/reduction, multi-owner,
/// unsupported use) are skipped conservatively — no window, no error, no
/// existing op mutated. Same-CU in-place access gets a `readwrite` window
/// unless the same MU has a committed halo read, where SDE emits separate read
/// and write windows. It introduces no `sde.mu_token`, slice, `sde.mu_dep`, or
/// ARTS concept.
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
      llvm::SmallVector<carts::sde::RaisedWindowSpec, 4> specs =
          carts::sde::queryAccessWindows(mu);
      if (specs.empty())
        continue; // out of scope -> conservative, no window

      for (const carts::sde::RaisedWindowSpec &spec : specs) {
        // Find the earliest dominance-safe insertion point and detect an
        // already-raised window for this (MU, mode) so the pass is idempotent.
        carts::sde::SdeCuRegionOp cu = spec.cu;
        Block &body = cu.getBody().front();
        bool exists = false;
        for (Operation &op : body) {
          if (auto win = dyn_cast<carts::sde::SdeMuAccessWindowOp>(op)) {
            if (win.getMu() == spec.mu && win.getMode() == spec.mode)
              exists = true;
          }
        }
        Operation *insertBefore = findWindowInsertionPoint(cu, spec.mu);
        if (exists || !insertBefore)
          continue;

        OpBuilder builder(insertBefore);
        IntegerAttr arrayIdAttr;
        if (spec.arrayId)
          arrayIdAttr = builder.getI64IntegerAttr(*spec.arrayId);
        carts::sde::SdeMuAccessWindowOp::create(
            builder, mu.getLoc(), spec.mu,
            carts::sde::SdeAccessModeAttr::get(ctx, spec.mode), arrayIdAttr);
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
