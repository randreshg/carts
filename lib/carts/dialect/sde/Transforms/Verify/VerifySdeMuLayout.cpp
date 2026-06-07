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
///   R2 — `ownerDims == recover(structure)`. For every converted MU governed by
///        a committed single-owner BLOCK plan, the owner dim recovered purely
///        from the expanded memref type must equal the committed
///        `physicalOwnerDims`.
///
/// Conservative (flat) MUs and out-of-scope plans are skipped, never rejected.
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

// Shared helpers keep shape recognition and grain checks consistent across
// rank expansion, access-window raising, and verification.

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
      sde::SdeSuIterateOp si = sde::findCommittedBlockPlanWriter(mu);
      std::optional<sde::ExpandedBlockGridMu> expanded =
          sde::recognizeExpandedBlockGridMu(si, muType);

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
        // A rank-reducing view of a CONVERTED MU reintroduces a logical-rank
        // handle and bypasses the physical coordinate system.
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

      // R2: ownerDims == recover(structure) for converted MUs.
      if (!expanded)
        return;
      ArrayRef<int64_t> eshape = muType.getShape();
      int64_t gridExtent = eshape.front();           // single grid axis
      ArrayRef<int64_t> tiles = eshape.drop_front(); // originalRank tiles
      if (tiles[expanded->ownerDim] != expanded->blockExtent) {
        mu.emitOpError() << "expanded MU tile extent does not match committed "
                            "block shape on the owner dim";
        failed = true;
        return;
      }
      // Validate the grid count against the writer's iteration domain — an
      // INDEPENDENT fact — so recover() is given the real owner extent rather
      // than one reconstructed from the expanded type (which would make the
      // proof tautological).
      std::optional<int64_t> ownerExtent =
          sde::findOwnerIterationExtent(si, expanded->blockExtent, gridExtent);
      if (!ownerExtent) {
        mu.emitOpError()
            << "rank-expanded block grid count " << gridExtent
            << " is not ceilDiv(extent, " << expanded->blockExtent
            << ") of any committed iteration extent; structure does not encode "
               "the committed grain";
        failed = true;
        return;
      }
      SmallVector<int64_t, 4> logicalShape(tiles.begin(), tiles.end());
      logicalShape[expanded->ownerDim] = *ownerExtent;

      std::optional<SmallVector<unsigned, 2>> recovered =
          sde::recoverOwnerDims(muType, logicalShape);
      SmallVector<unsigned, 2> committed{expanded->ownerDim};
      if (!recovered || *recovered != committed) {
        mu.emitOpError()
            << "rank-expanded structure does not recover the committed owner "
               "dim (ownerDims != recover(structure))";
        failed = true;
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
