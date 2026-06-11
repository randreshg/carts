///==========================================================================///
/// File: RankExpandMu.cpp
///
/// SDE structural owner-dim/grain carrier.
///
/// Replaces the attribute-only block/owner grain with readable IR structure:
/// for every `sde.mu_alloc` governed by a committed elementwise/stencil BLOCK
/// plan with any number of owner dims, this pass rank-expands the result memref
/// so
/// the block grid is part of the type, and rewrites every CU `memref.load`/
/// `memref.store` into the physical `[block, intra-block, ...]` coordinate
/// system via the `MuLayoutRewriter`/`MuAccessIndexer` library.
///
/// This is a REAL transformation, not a metadata promise:
///   * a converted MU carries its grain structurally (no owner-dim attr on the
///     mu_alloc; owner dims are `recover(structure)`); multi-owner owner-tile
///     plans expand to a `[grid..., tile...]` form, with grid dims in canonical
///     ascending owner order,
///   * matmul / reduction / in-place / dynamic plans are left in conservative
///     flat form (or fail closed) — never papered over with an op-attribute
///     promise or a compatibility attr.
///==========================================================================///

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"

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

static ArrayAttr buildCanonicalOwnerDimsAttr(MLIRContext *ctx,
                                             ArrayRef<unsigned> ownerDims) {
  SmallVector<int64_t, 4> values;
  values.reserve(ownerDims.size());
  for (unsigned dim : ownerDims)
    values.push_back(static_cast<int64_t>(dim));
  return Builder(ctx).getI64ArrayAttr(values);
}

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

      carts::sde::SdeSuIterateOp si =
          carts::sde::findCommittedBlockPlanWriter(mu);
      carts::sde::MuPhysicalLayout plan;
      if (!carts::sde::isBlockGridRealizable(si, logicalType, plan))
        continue; // out of scope -> leave flat, add NO attrs

      std::unique_ptr<carts::sde::MuAccessIndexer> indexer =
          carts::sde::makeMuAccessIndexer(*si.getStructuredClassification(),
                                          plan);
      carts::sde::MuLayoutRewriter rewriter(plan, *indexer);
      if (mlir::failed(rewriter.apply(mu))) {
        // The plan committed a distributed/block-shaped result this pass cannot
        // realize end to end. Fail closed with evidence rather than emit a
        // partial promise.
        mu.emitOpError()
            << "committed block-grid layout cannot be realized as a "
               "rank-expanded MU (unsupported use of the MU root); refusing to "
               "leave a partial owner-dim promise";
        failed = true;
        continue;
      }
      si.setPhysicalOwnerDimsAttr(
          buildCanonicalOwnerDimsAttr(module.getContext(), plan.ownerDims));
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
