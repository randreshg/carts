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
///        committed single-owner BLOCK facts, the owner dim recovered purely
///        from the expanded memref type must equal the committed
///        `physicalOwnerDims`.
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
      sde::SdeSuIterateOp si = sde::findCommittedBlockLayoutWriter(mu);
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

      // R2: ownerDims == recover(structure) for converted MUs (ND).
      if (!expanded)
        return;
      const unsigned numOwner = expanded->ownerDims.size();
      ArrayRef<int64_t> eshape = muType.getShape();
      // Expanded layout: K grid dims (owner order, ascending) then L logical
      // tile dims.
      ArrayRef<int64_t> tiles = eshape.drop_front(numOwner);
      // Per-owner tile extent == committed block extent.
      for (unsigned i = 0; i < numOwner; ++i) {
        if (tiles[expanded->ownerDims[i]] != expanded->blockExtents[i]) {
          mu.emitOpError()
              << "expanded MU tile extent does not match committed block shape "
                 "on owner dim "
              << expanded->ownerDims[i];
          failed = true;
          return;
        }
      }
      // Validate the grid counts against the writer's iteration domain — an
      // INDEPENDENT fact — so recover() is given the real owner extents rather
      // than ones reconstructed from the expanded type (which would make the
      // proof tautological).
      std::optional<SmallVector<int64_t, 4>> ownerExtents =
          sde::findOwnerIterationExtents(si, expanded->blockExtents,
                                         expanded->gridCounts);
      if (!ownerExtents) {
        mu.emitOpError()
            << "rank-expanded block grid counts are not ceilDiv(extent, block) "
               "of distinct committed iteration extents; structure does not "
               "encode the committed grain";
        failed = true;
        return;
      }
      SmallVector<int64_t, 4> logicalShape(tiles.begin(), tiles.end());
      for (unsigned i = 0; i < numOwner; ++i)
        logicalShape[expanded->ownerDims[i]] = (*ownerExtents)[i];

      std::optional<SmallVector<unsigned, 2>> recovered =
          sde::recoverOwnerDims(muType, logicalShape);
      if (!recovered || ArrayRef<unsigned>(*recovered) !=
                            ArrayRef<unsigned>(expanded->ownerDims)) {
        mu.emitOpError()
            << "rank-expanded structure does not recover the committed owner "
               "dims (ownerDims != recover(structure))";
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
