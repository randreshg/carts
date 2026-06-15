///==========================================================================///
/// File: RankExpandMu.cpp
///
/// SDE structural owner-dim/grain carrier.
///
/// Replaces the attribute-only block/owner grain with readable IR structure:
/// for every `sde.mu_alloc` governed by a committed elementwise/stencil BLOCK
/// layout with any number of owner dims, this pass rank-expands the result
/// memref so
/// the block grid is part of the type, and rewrites every CU `memref.load`/
/// `memref.store` into the physical `[block, intra-block, ...]` coordinate
/// system via the `MuLayoutRewriter`/`MuAccessIndexer` library.
///
/// This is a real transformation:
///   * a converted MU carries its grain structurally (no owner-dim attr on the
///     mu_alloc; owner dims are `recover(structure)`); multi-owner owner-tile
///     layouts expand to a `[grid..., tile...]` form, with grid dims in
///     canonical ascending owner order,
///   * accumulator-reduction / in-place / dynamic cases stay in flat form or
///     fail closed; this pass does not add compatibility attrs.
///==========================================================================///

#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_SDERANKEXPANDMU
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallVector.h"

#include <memory>

using namespace mlir;

namespace {

// Shared helpers keep the block-grid realize gate and index localization
// identical across rank expansion, coarse avoidance, and verification.

struct SdeRankExpandMuPass
    : public carts::sde::impl::SdeRankExpandMuBase<SdeRankExpandMuPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    llvm::SmallVector<carts::sde::SdeMuAllocOp> worklist;
    module.walk([&](carts::sde::SdeMuAllocOp mu) { worklist.push_back(mu); });

    bool failed = false;
    for (carts::sde::SdeMuAllocOp mu : worklist) {
      auto logicalType = dyn_cast<MemRefType>(mu.getMemref().getType());
      if (!logicalType || !logicalType.hasStaticShape())
        continue; // dynamic / non-memref -> conservative

      std::optional<carts::sde::CommittedMuBlockLayout> committed =
          carts::sde::findCommittedMuBlockLayout(mu);
      if (!committed ||
          !carts::sde::supportsRankExpandedAccessWindows(committed->writer))
        continue; // out of scope -> leave flat, add NO attrs

      // Resolve the array id before rewriter.apply replaces the MU root (which
      // invalidates the layout-root users the lookup walks).
      std::optional<int64_t> muArrayId =
          carts::sde::getMuArrayIdFromLayoutRoot(mu);

      std::optional<carts::sde::SdeStructuredClassification> classification =
          carts::sde::queryStructuredClassification(committed->writer);
      if (!classification)
        classification = committed->writer.getStructuredClassification();

      // Commit classification durably before the rewrite makes the body div/rem
      // (un-re-derivable). 1n-inert: only runs for committed block layouts.
      if (classification &&
          !committed->writer.getStructuredClassificationAttr())
        committed->writer.setStructuredClassificationAttr(
            carts::sde::SdeStructuredClassificationAttr::get(
                committed->writer.getContext(), *classification));

      std::unique_ptr<carts::sde::MuAccessIndexer> indexer =
          carts::sde::makeMuAccessIndexerForCommittedLayout(committed->writer,
                                                            committed->layout);
      carts::sde::MuLayoutRewriter rewriter(committed->layout, *indexer);
      if (mlir::failed(rewriter.apply(mu))) {
        mu.emitOpError()
            << "committed block-grid layout cannot be realized as a "
               "rank-expanded MU (unsupported use of the MU root); refusing to "
               "leave a partial owner-dim promise";
        failed = true;
        continue;
      }
      SmallVector<int64_t, 4> ownerDims;
      ownerDims.reserve(committed->layout.ownerDims.size());
      for (unsigned dim : committed->layout.ownerDims)
        ownerDims.push_back(static_cast<int64_t>(dim));
      SmallVector<int64_t, 4> blockShape = committed->layout.logicalShape;
      for (auto [slot, dim] : llvm::enumerate(committed->layout.ownerDims))
        blockShape[dim] = committed->layout.blockExtents[slot];
      // Owner-dim recovery for a separate init writer is per-dependency at the
      // boundary (readPhysicalLayoutFromDepWindow), not a shared SU attr here.
      // Restate only THIS MU's array for matmul/contraction AND
      // elementwise-pipeline (matvec / pipelined-reduction) witnesses: there
      // the witness SU writes a DISTINCT, lower-rank output array whose
      // committed grain must not be clobbered by a higher-rank INPUT array's
      // block shape.
      //
      // A read-only input MU's rank expansion restates the write facts of its
      // representative reader SU (findCommittedMuBlockLayout falls back to a
      // reader when the array has no committed-layout writer). For bicg's
      // `q = A*p`, that representative reader is the q SU: it reads the rank-2
      // `A` (owner [0,1]) but writes the rank-1 vector `q` (owner [0]). The
      // unrestricted restatement stamps A's [0,1] onto q's write fact, which
      // then contradicts q's 1-owner-dim access window and fails the
      // sde-to-arts owner-dim check. Restricting to `muArrayId` makes the
      // restatement inert for the read-only input (the helper only touches
      // WRITE facts), preserving q's committed [0] grain.
      //
      // STENCIL witnesses stay on the UNRESTRICTED path: there the witness's
      // own output legitimately inherits the input's multi-dim block grid (e.g.
      // convolution output [0]->[0,1] aligned with the input image), and
      // restricting would drop the needed write-fact restatement and break
      // CreateDbs. This classification gate keys on the loop family that
      // determines whether the output's grain follows the input's owner rank;
      // it is distinct from any writer/array-identity test.
      bool restrictToOwnArray =
          classification &&
          (*classification == carts::sde::SdeStructuredClassification::matmul ||
           *classification ==
               carts::sde::SdeStructuredClassification::elementwise_pipeline ||
           *classification ==
               carts::sde::SdeStructuredClassification::reduction);
      carts::sde::rewriteWriterArrayLayoutToPhysicalShape(
          committed->writer, ownerDims, blockShape,
          restrictToOwnArray ? muArrayId : std::nullopt);
    }

    if (failed)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createSdeRankExpandMuPass() {
  return std::make_unique<SdeRankExpandMuPass>();
}
} // namespace mlir::carts::sde
