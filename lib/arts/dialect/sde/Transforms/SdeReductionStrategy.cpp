///==========================================================================///
/// File: SdeReductionStrategy.cpp
///
/// Cost-model-backed SDE reduction strategy selection. The current
/// implementation is deliberately narrow: it annotates single-reduction
/// sde.su_iterate ops with an SDE-owned recommendation and leaves lowering
/// behavior unchanged.
///==========================================================================///

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_SDEREDUCTIONSTRATEGY
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

#include "arts/utils/costs/SDECostModel.h"

#include "mlir/IR/BuiltinOps.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool isAtomicCapable(sde::SdeReductionKind kind) {
  return kind == sde::SdeReductionKind::add;
}

static std::optional<sde::SdeReductionKind>
getSingleReductionKind(sde::SdeSuIterateOp op) {
  if (op.getReductionAccumulators().size() != 1)
    return std::nullopt;

  auto kinds = op.getReductionKindsAttr();
  if (!kinds || kinds.size() != 1)
    return std::nullopt;

  auto kindAttr = dyn_cast<sde::SdeReductionKindAttr>(kinds[0]);
  if (!kindAttr || kindAttr.getValue() == sde::SdeReductionKind::custom)
    return std::nullopt;

  return kindAttr.getValue();
}

struct SdeReductionStrategyPass
    : public arts::impl::SdeReductionStrategyBase<SdeReductionStrategyPass> {
  explicit SdeReductionStrategyPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    if (!costModel)
      return;

    int64_t workerCount = std::max<int64_t>(1, costModel->getWorkerCount());
    if (workerCount <= 1)
      return;

    getOperation().walk([&](sde::SdeSuIterateOp op) {
      if (op.getReductionStrategyAttr())
        return;

      std::optional<sde::SdeReductionKind> reductionKind =
          getSingleReductionKind(op);
      if (!reductionKind)
        return;

      auto strategy = sde::SdeReductionStrategy::tree;
      if (isAtomicCapable(*reductionKind)) {
        double atomicCost =
            static_cast<double>(workerCount) * costModel->getAtomicUpdateCost();
        double collectiveCost = costModel->getReductionCost(workerCount);
        if (atomicCost <= collectiveCost)
          strategy = sde::SdeReductionStrategy::atomic;
      }

      op.setReductionStrategyAttr(
          sde::SdeReductionStrategyAttr::get(&getContext(), strategy));
    });
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::arts::sde {

std::unique_ptr<Pass>
createSdeReductionStrategyPass(sde::SDECostModel *costModel) {
  return std::make_unique<SdeReductionStrategyPass>(costModel);
}

} // namespace mlir::arts::sde
