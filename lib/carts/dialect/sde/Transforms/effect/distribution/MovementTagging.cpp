///==========================================================================///
/// File: MovementTagging.cpp
///
/// SDE distribution-kind selection and sde.su_distribute wrapping. Decides
/// blocked vs owner_compute distribution for eligible structured SUs and wraps
/// them in sde.su_distribute. Logic carved verbatim from the correctness-base
/// @782988ad1 DistributionPlanning pass. Fail-closed gating is owned by
/// DistributionFailClosed, which runs earlier in the pipeline; this pass only
/// tags distributable SUs.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_MOVEMENTTAGGING
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Transforms/effect/distribution/DistributionLayoutUtils.h"
#include "carts/dialect/sde/Transforms/effect/distribution/MovementTagging.h"
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

} // namespace

namespace mlir::carts::sde::distribution {

std::optional<sde::SdeDistributionKind>
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

} // namespace mlir::carts::sde::distribution

namespace {

struct MovementTaggingPass
    : public sde::impl::MovementTaggingBase<MovementTaggingPass> {
  explicit MovementTaggingPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    if (!costModel)
      return;

    SmallVector<DistributionRewrite> rewrites;
    getOperation().walk([&](sde::SdeSuIterateOp op) {
      if (auto kind = chooseDistributionKind(op, *costModel))
        rewrites.push_back({op, *kind});
    });

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

std::unique_ptr<Pass> createMovementTaggingPass(sde::SDECostModel *costModel) {
  return std::make_unique<MovementTaggingPass>(costModel);
}

} // namespace mlir::carts::sde
