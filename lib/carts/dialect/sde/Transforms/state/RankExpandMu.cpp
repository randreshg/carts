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

      std::optional<carts::sde::SdeStructuredClassification> classification =
          carts::sde::queryStructuredClassification(committed->writer);
      if (!classification)
        classification = committed->writer.getStructuredClassification();
      if (!classification)
        continue;

      // Commit classification durably before the rewrite makes the body div/rem
      // (un-re-derivable). 1n-inert: only runs for committed block layouts.
      if (!committed->writer.getStructuredClassificationAttr())
        committed->writer.setStructuredClassificationAttr(
            carts::sde::SdeStructuredClassificationAttr::get(
                committed->writer.getContext(), *classification));

      std::unique_ptr<carts::sde::MuAccessIndexer> indexer =
          carts::sde::makeMuAccessIndexer(*classification, committed->layout);
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
      carts::sde::rewriteWriterArrayLayoutToPhysicalShape(
          committed->writer, ownerDims, blockShape);
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
