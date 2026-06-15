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
#include "carts/dialect/sde/Transforms/effect/distribution/OwnerDimSelect.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/LoopUtils.h"
#include "carts/utils/Utils.h"

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

static bool hasEnoughWorkForDistribution(sde::SdeSuIterateOp op,
                                         sde::SDECostModel &costModel) {
  std::optional<int64_t> tripCount = getStaticTripCount(op.getOperation());
  if (!tripCount)
    return true;

  int64_t threshold =
      saturatingMultiplyPositive(costModel.getLogicalWorkerCapacity(),
                                 costModel.getMinIterationsPerWorker());
  return *tripCount >= threshold;
}

static std::optional<sde::SdeDistributionKind>
chooseDistributionKind(sde::SdeSuIterateOp op, sde::SDECostModel &costModel) {
  if (op->getParentOfType<sde::SdeSuDistributeOp>())
    return std::nullopt;
  if (costModel.getLogicalWorkerCapacity() <= 1)
    return std::nullopt;

  auto classificationAttr = sde::queryStructuredClassification(op);
  if (!classificationAttr) {
    std::optional<sde::LoopIndexedOutputShape> outputPlan =
        sde::findLoopIndexedOutputShape(op);
    if (!outputPlan)
      return std::nullopt;
    auto effects = sde::collectStructuredMemoryEffects(op.getBody());
    if (effects.hasUnknownEffects || effects.reads.contains(outputPlan->root))
      return std::nullopt;
    if (hasEnoughWorkForDistribution(op, costModel))
      return sde::SdeDistributionKind::blocked;
    return std::nullopt;
  }

  if (op.getNumResults() > 0 &&
      *classificationAttr != sde::SdeStructuredClassification::reduction)
    return std::nullopt;

  switch (*classificationAttr) {
  case sde::SdeStructuredClassification::elementwise:
  case sde::SdeStructuredClassification::elementwise_pipeline:
    return sde::SdeDistributionKind::blocked;
  case sde::SdeStructuredClassification::stencil:
    if (sde::requiresNestedStencilOwnerPromotion(op) &&
        !sde::hasRealizableOwnerStrip(op))
      return std::nullopt;
    if (isInPlaceSelfReadStencil(op) && !sde::queryInPlaceSafe(op))
      return std::nullopt;
    if (hasEnoughWorkForDistribution(op, costModel))
      return sde::SdeDistributionKind::owner_compute;
    return std::nullopt;
  case sde::SdeStructuredClassification::matmul:
    return sde::SdeDistributionKind::blocked;
  case sde::SdeStructuredClassification::reduction:
    if (!op.getReductionAccumulators().empty())
      return sde::SdeDistributionKind::blocked;
    return std::nullopt;
  }
  return std::nullopt;
}

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
