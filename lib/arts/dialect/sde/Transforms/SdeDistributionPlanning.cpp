///==========================================================================///
/// File: SdeDistributionPlanning.cpp
///
/// Cost-model-backed SDE distribution planning. This pass keeps distribution
/// ownership on the SDE side of the boundary by wrapping eligible
/// `arts_sde.su_iterate` operations in `arts_sde.su_distribute`.
///==========================================================================///

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_SDEDISTRIBUTIONPLANNING
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

#include "arts/utils/LoopUtils.h"
#include "arts/utils/costs/SDECostModel.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

struct PlannedDistribution {
  sde::SdeSuIterateOp op;
  sde::SdeDistributionKind kind = sde::SdeDistributionKind::blocked;
};

static std::optional<sde::SdeConcurrencyScope>
getEnclosingParallelScope(sde::SdeSuIterateOp op) {
  for (Operation *current = op->getParentOp(); current;
       current = current->getParentOp()) {
    auto cuRegion = dyn_cast<sde::SdeCuRegionOp>(current);
    if (!cuRegion || cuRegion.getKind() != sde::SdeCuKind::parallel)
      continue;
    if (auto scope = cuRegion.getConcurrencyScope())
      return *scope;
    return sde::SdeConcurrencyScope::local;
  }
  return std::nullopt;
}

static bool hasEnoughDistributedWork(sde::SdeSuIterateOp op,
                                     sde::SDECostModel &costModel) {
  std::optional<int64_t> tripCount = getStaticTripCount(op.getOperation());
  if (!tripCount)
    return true;

  int64_t threshold =
      std::max<int64_t>(1, costModel.getMinIterationsPerWorker()) *
      std::max<int64_t>(1, costModel.getNodeCount());
  return *tripCount >= threshold;
}

static std::optional<sde::SdeDistributionKind>
chooseDistributionKind(sde::SdeSuIterateOp op, sde::SDECostModel &costModel) {
  if (op->getParentOfType<sde::SdeSuDistributeOp>())
    return std::nullopt;
  if (costModel.getWorkerCount() <= 1)
    return std::nullopt;

  auto classificationAttr = op.getStructuredClassificationAttr();
  if (!classificationAttr)
    return std::nullopt;

  auto scope = getEnclosingParallelScope(op);
  if (!scope)
    return std::nullopt;

  switch (classificationAttr.getValue()) {
  case sde::SdeStructuredClassification::elementwise:
  case sde::SdeStructuredClassification::elementwise_pipeline:
    if (*scope == sde::SdeConcurrencyScope::local)
      return sde::SdeDistributionKind::blocked;
    return std::nullopt;
  case sde::SdeStructuredClassification::stencil:
    if (*scope == sde::SdeConcurrencyScope::distributed &&
        hasEnoughDistributedWork(op, costModel))
      return sde::SdeDistributionKind::owner_compute;
    return std::nullopt;
  case sde::SdeStructuredClassification::matmul:
  case sde::SdeStructuredClassification::reduction:
    return std::nullopt;
  }
  return std::nullopt;
}

struct SdeDistributionPlanningPass
    : public arts::impl::SdeDistributionPlanningBase<
          SdeDistributionPlanningPass> {
  explicit SdeDistributionPlanningPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    if (!costModel)
      return;

    SmallVector<PlannedDistribution> rewrites;
    getOperation().walk([&](sde::SdeSuIterateOp op) {
      if (auto kind = chooseDistributionKind(op, *costModel))
        rewrites.push_back({op, *kind});
    });

    for (PlannedDistribution rewrite : rewrites) {
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

namespace mlir::arts::sde {

std::unique_ptr<Pass>
createSdeDistributionPlanningPass(sde::SDECostModel *costModel) {
  return std::make_unique<SdeDistributionPlanningPass>(costModel);
}

} // namespace mlir::arts::sde
