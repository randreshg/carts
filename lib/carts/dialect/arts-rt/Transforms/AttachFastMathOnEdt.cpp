///==========================================================================///
/// File: AttachFastMathOnEdt.cpp
///
/// Walks every `llvm.func` whose symbol starts with `__arts_edt_` and stamps
/// `fastmath<reassoc,contract>` on contained `llvm.fmul`, `llvm.fadd`,
/// `llvm.fsub`, `llvm.fdiv`, `llvm.fneg`, `llvm.frem` ops that do not already
/// carry a non-`none` `fastmathFlags` attribute. `reassoc` lets the LLVM
/// vectorizer reorder reductions; `contract` lets it fuse mul+add into FMA.
///
/// We deliberately omit `nnan`, `ninf`, `nsz`, `arcp`, `afn`: those can break
/// generated initialization and reduction code that still observes strict FP
/// semantics. The OMP baseline only sees `-ffast-math` at link time, after
/// cgeist already emitted strict scalar IR, so symmetry with OMP does not
/// require the full `fast` set.
///==========================================================================///

#include "carts/dialect/arts-rt/Transforms/Passes.h"

namespace mlir::carts::arts_rt {
#define GEN_PASS_DEF_ATTACHFASTMATHONEDT
#include "carts/dialect/arts-rt/Transforms/Passes.h.inc"
} // namespace mlir::carts::arts_rt

#include "carts/utils/Debug.h"
#include "mlir/Dialect/LLVMIR/LLVMAttrs.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "llvm/ADT/TypeSwitch.h"

ARTS_DEBUG_SETUP(attach_fastmath_on_edt);

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts_rt;

namespace {

struct AttachFastMathOnEdtPass
    : public arts_rt::impl::AttachFastMathOnEdtBase<AttachFastMathOnEdtPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();
    ARTS_INFO_HEADER(AttachFastMathOnEdtPass);

    auto fastAttr = LLVM::FastmathFlagsAttr::get(
        ctx, LLVM::FastmathFlags::reassoc | LLVM::FastmathFlags::contract);

    int totalStamped = 0;
    int totalSkippedExisting = 0;

    module.walk([&](LLVM::LLVMFuncOp funcOp) {
      if (funcOp.isExternal())
        return;
      if (!funcOp.getName().starts_with("__arts_edt_"))
        return;

      int stampedInFunc = 0;
      int skippedInFunc = 0;

      funcOp.walk([&](Operation *op) {
        auto existing =
            llvm::TypeSwitch<Operation *, LLVM::FastmathFlagsAttr>(op)
                .Case<LLVM::FMulOp, LLVM::FAddOp, LLVM::FSubOp, LLVM::FDivOp,
                      LLVM::FNegOp, LLVM::FRemOp>(
                    [](auto fpOp) { return fpOp.getFastmathFlagsAttr(); })
                .Default([](Operation *) { return LLVM::FastmathFlagsAttr{}; });

        if (!existing && !isa<LLVM::FMulOp, LLVM::FAddOp, LLVM::FSubOp,
                              LLVM::FDivOp, LLVM::FNegOp, LLVM::FRemOp>(op))
          return;

        if (existing && existing.getValue() != LLVM::FastmathFlags::none) {
          skippedInFunc++;
          return;
        }

        llvm::TypeSwitch<Operation *>(op)
            .Case<LLVM::FMulOp, LLVM::FAddOp, LLVM::FSubOp, LLVM::FDivOp,
                  LLVM::FNegOp, LLVM::FRemOp>(
                [&](auto fpOp) { fpOp.setFastmathFlagsAttr(fastAttr); });
        stampedInFunc++;
      });

      if (stampedInFunc > 0 || skippedInFunc > 0) {
        ARTS_INFO("Stamped fastmath<reassoc,contract> on "
                  << stampedInFunc << " FP op(s) in " << funcOp.getName()
                  << " (skipped " << skippedInFunc
                  << " op(s) already carrying fastmath)");
        totalStamped += stampedInFunc;
        totalSkippedExisting += skippedInFunc;
      }
    });

    ARTS_INFO("Total: stamped fastmath<reassoc,contract> on "
              << totalStamped << " FP op(s); skipped " << totalSkippedExisting
              << " op(s) already carrying fastmath");
    ARTS_INFO_FOOTER(AttachFastMathOnEdtPass);

    markAllAnalysesPreserved();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::arts_rt::createAttachFastMathOnEdtPass() {
  return std::make_unique<AttachFastMathOnEdtPass>();
}
