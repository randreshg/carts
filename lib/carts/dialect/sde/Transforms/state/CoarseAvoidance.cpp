///==========================================================================///
/// File: CoarseAvoidance.cpp
///
/// SDE coarse-allocation avoidance (best-effort realize).
///
/// Makes coarse-producing MU shape a diagnosed last resort. For every
/// `sde.mu_alloc`:
///
///   * if it is already rank-expanded (the block grid is in the memref type),
///     the maximum legal independent MU blocks are already exposed — skip;
///   * else if it carries a committed, single-contiguous-owner, fully-static
///     elementwise/stencil BLOCK plan that proves a real grid, realize that
///     finest grain as structure (the same gate + rewriter rank expansion uses,
///     consuming the committed plan verbatim);
///   * else leave it flat. Whether a flat MU is an avoidable bug or a legitimate
///     last resort (dynamic / in-place / reduction / matmul / multi-owner /
///     aliasing) is diagnosed by `verify-sde-coarse-avoidance`, which fails
///     closed with a reason rather than letting coarse pass silently.
///
/// The only hard failure here is a committed plan that cannot be realized
/// end to end: rather than leave a partial owner-dim promise, the pass fails
/// closed with evidence.
///
/// It changes only MU storage shape. The load/store rewrites move no op across a
/// CU/SU boundary, so CU legality established by
/// `sde-cu-normalization`/`verify-sde` is preserved by construction.
///==========================================================================///

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_SDECOARSEAVOIDANCE
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallVector.h"

#include <memory>

using namespace mlir;
using namespace mlir::carts;

namespace {

struct SdeCoarseAvoidancePass
    : public carts::sde::impl::SdeCoarseAvoidanceBase<SdeCoarseAvoidancePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    llvm::SmallVector<carts::sde::SdeMuAllocOp> worklist;
    module.walk([&](carts::sde::SdeMuAllocOp mu) { worklist.push_back(mu); });

    bool failed = false;
    for (carts::sde::SdeMuAllocOp mu : worklist) {
      auto logicalType = dyn_cast<MemRefType>(mu.getMemref().getType());
      if (!logicalType)
        continue;

      carts::sde::SdeSuIterateOp si =
          carts::sde::findCommittedBlockPlanWriter(mu);

      // Already block-partitioned: the grain is in the type, max legal blocks
      // are exposed.
      if (si && carts::sde::recognizeExpandedBlockGridMu(si, logicalType))
        continue;

      // Best effort: realize the committed finest grain as structure. Out-of-scope
      // (coarse) MUs are left flat for verify-sde-coarse-avoidance to diagnose.
      carts::sde::MuPhysicalLayout plan;
      if (!carts::sde::isSingleOwnerBlockGridRealizable(si, logicalType, plan))
        continue;

      std::unique_ptr<carts::sde::MuAccessIndexer> indexer =
          carts::sde::makeMuAccessIndexer(*si.getStructuredClassification(),
                                          plan);
      carts::sde::MuLayoutRewriter rewriter(plan, *indexer);
      if (mlir::failed(rewriter.apply(mu))) {
        // The committed plan cannot be realized end to end. Fail closed with
        // evidence rather than emit a partial owner-dim promise.
        mu.emitOpError()
            << "committed block-grid layout cannot be realized as a "
               "rank-expanded MU (unsupported use of the MU root); refusing to "
               "leave a partial owner-dim promise";
        failed = true;
      }
    }

    if (failed)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createSdeCoarseAvoidancePass() {
  return std::make_unique<SdeCoarseAvoidancePass>();
}
} // namespace mlir::carts::sde
