///==========================================================================///
/// File: DistributionPlanning.cpp
///
/// SDE distribution transform orchestrator (transitional). Drives the per-SU
/// owner-dim selection + physical layout commit (OwnerDimSelect), the A1
/// same-owner grain unification (BlockGrainPlan), the A7 fail-closed gate
/// (DistributionFailClosed), and the distribution-kind selection +
/// su_distribute wrapping (MovementTagging, still inlined here until the Step 5
/// pipeline swap). Behavior is byte-identical to the correctness base
/// @782988ad1; the owning logic now lives in the split passes that this
/// orchestrator calls.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_DISTRIBUTIONPLANNING
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Transforms/effect/distribution/BlockGrainPlan.h"
#include "carts/dialect/sde/Transforms/effect/distribution/DistributionFailClosed.h"
#include "carts/dialect/sde/Transforms/effect/distribution/DistributionLayoutUtils.h"
#include "carts/dialect/sde/Transforms/effect/distribution/MovementTagging.h"
#include "carts/dialect/sde/Transforms/effect/distribution/OwnerDimSelect.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "llvm/ADT/SmallVector.h"

#include <optional>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::sde::distribution;

namespace {

struct DistributionRewrite {
  sde::SdeSuIterateOp op;
  sde::SdeDistributionKind kind = sde::SdeDistributionKind::blocked;
};

struct DistributionPlanningPass
    : public sde::impl::DistributionPlanningBase<DistributionPlanningPass> {
  explicit DistributionPlanningPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    if (!costModel)
      return;

    SmallVector<DistributionRewrite> rewrites;
    bool failed = false;
    auto chooseDistributionOrFailClosed = [&](sde::SdeSuIterateOp op) {
      if (auto kind = chooseDistributionKind(op, *costModel)) {
        rewrites.push_back({op, *kind});
        return;
      }
      if (requiresUnimplementedStencilWavefront(op, *costModel)) {
        emitStencilWavefrontFailClosed(op);
        failed = true;
      }
    };
    SmallVector<sde::SdeSuIterateOp, 16> iterates;
    getOperation().walk(
        [&](sde::SdeSuIterateOp op) { iterates.push_back(op); });

    for (sde::SdeSuIterateOp original : iterates) {
      if (!original || !original->getBlock())
        continue;
      sde::SdeSuIterateOp op = original;
      if (requiresInPlaceSelfRawWavefrontFailClosed(op, *costModel)) {
        emitStencilWavefrontFailClosed(op);
        failed = true;
        continue;
      }
      if (applyOwnerDimSelect(op, *costModel))
        continue;
      chooseDistributionOrFailClosed(op);
    }

    if (failed) {
      signalPassFailure();
      return;
    }

    // Unify producer/consumer block grain per array so a same-owner re-tile is
    // never left for RedistributionEdges to reject (A1). Runs after all per-SU
    // facts are committed; downstream passes consume the unified facts.
    reconcileSameOwnerArrayGrain(getOperation());

    for (DistributionRewrite rewrite : rewrites) {
      if (rewrite.op.getNumResults() > 0)
        continue;

      IRRewriter rewriter(rewrite.op.getContext());
      rewriter.setInsertionPoint(rewrite.op);

      auto distributeOp = sde::SdeSuDistributeOp::create(
          rewriter, rewrite.op.getLoc(),
          sde::SdeDistributionKindAttr::get(&getContext(), rewrite.kind));
      Block &body = sde::ensureBlock(distributeOp.getBody());
      rewrite.op->moveBefore(&body, body.end());
    }
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass>
createDistributionPlanningPass(sde::SDECostModel *costModel) {
  return std::make_unique<DistributionPlanningPass>(costModel);
}

} // namespace mlir::carts::sde
