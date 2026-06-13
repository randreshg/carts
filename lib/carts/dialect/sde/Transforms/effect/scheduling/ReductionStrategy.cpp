///==========================================================================///
/// File: ReductionStrategy.cpp
///
/// Former cost-model-backed SDE reduction strategy selection. Step 9 retires
/// the `reductionStrategy` attr: atomic lowering is realized directly by
/// `sde-atomic-reduction-realization`, tree was never realized, and partial
/// reductions carry their shape structurally.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_REDUCTIONSTRATEGY
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Utils/SDECostModel.h"

#include "mlir/IR/BuiltinOps.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

struct ReductionStrategyPass
    : public sde::impl::ReductionStrategyBase<ReductionStrategyPass> {
  explicit ReductionStrategyPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    (void)costModel;
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass>
createReductionStrategyPass(sde::SDECostModel *costModel) {
  return std::make_unique<ReductionStrategyPass>(costModel);
}

} // namespace mlir::carts::sde
