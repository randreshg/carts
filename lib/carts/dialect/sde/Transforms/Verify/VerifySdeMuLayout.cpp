///==========================================================================///
/// File: VerifySdeMuLayout.cpp
///
/// Verifies the structural block-grid carrier on `sde.mu_alloc`.
///
///   R1 — no stale logical-rank access against a converted (rank-expanded) MU.
///        Core MLIR already rejects a direct `memref.load/store` whose index
///        arity disagrees with the MU rank; this rule additionally rejects a
///        rank-reducing view (subview/collapse/cast) that would reintroduce a
///        logical-rank handle onto a converted block-grid MU.
///
/// Conservative (flat) MUs and out-of-scope cases are skipped, never rejected.
///==========================================================================///

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYSDEMULAYOUT
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

struct VerifySdeMuLayoutPass
    : public sde::impl::VerifySdeMuLayoutBase<VerifySdeMuLayoutPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool failed = false;

    module.walk([&](sde::SdeMuAllocOp mu) {
      auto muType = dyn_cast<MemRefType>(mu.getMemref().getType());
      if (!muType)
        return;
      const int64_t muRank = muType.getRank();
      std::optional<sde::ExpandedBlockGridMu> expanded =
          sde::recognizeExpandedBlockGridMu(mu);

      // R1: access soundness against the MU.
      for (Operation *user : mu.getMemref().getUsers()) {
        if (auto load = dyn_cast<memref::LoadOp>(user)) {
          if (load.getMemRef() == mu.getMemref() &&
              static_cast<int64_t>(load.getIndices().size()) != muRank) {
            load.emitOpError()
                << "stale " << load.getIndices().size()
                << "-index access against rank-" << muRank << " MU";
            failed = true;
          }
          continue;
        }
        if (auto store = dyn_cast<memref::StoreOp>(user)) {
          if (store.getMemRef() == mu.getMemref() &&
              static_cast<int64_t>(store.getIndices().size()) != muRank) {
            store.emitOpError()
                << "stale " << store.getIndices().size()
                << "-index access against rank-" << muRank << " MU";
            failed = true;
          }
          continue;
        }
        if (expanded) {
          for (Value res : user->getResults()) {
            auto resType = dyn_cast<MemRefType>(res.getType());
            if (resType && resType.getRank() < muRank) {
              user->emitOpError() << "rank-reducing view of a rank-expanded "
                                     "block-grid MU reintroduces a stale "
                                     "logical-rank access";
              failed = true;
            }
          }
        }
      }
    });

    if (failed)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createVerifySdeMuLayoutPass() {
  return std::make_unique<VerifySdeMuLayoutPass>();
}
} // namespace mlir::carts::sde
