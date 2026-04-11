///==========================================================================///
/// File: SdeScopeSelection.cpp
///
/// Cost-model-backed SDE concurrency scope selection. The current
/// implementation fills in missing scope on parallel sde.cu_region ops from
/// the active machine topology while preserving any explicit SDE scope that is
/// already present.
///==========================================================================///

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_SDESCOPESELECTION
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

#include "arts/utils/costs/SDECostModel.h"

#include "mlir/IR/BuiltinOps.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

struct SdeScopeSelectionPass
    : public arts::impl::SdeScopeSelectionBase<SdeScopeSelectionPass> {
  explicit SdeScopeSelectionPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    if (!costModel)
      return;

    auto desiredScope = sde::SdeConcurrencyScopeAttr::get(
        &getContext(), costModel->isDistributed()
                           ? sde::SdeConcurrencyScope::distributed
                           : sde::SdeConcurrencyScope::local);

    getOperation().walk([&](sde::SdeCuRegionOp op) {
      if (op.getKind() != sde::SdeCuKind::parallel)
        return;
      if (op.getConcurrencyScopeAttr())
        return;
      op.setConcurrencyScopeAttr(desiredScope);
    });
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::arts::sde {

std::unique_ptr<Pass>
createSdeScopeSelectionPass(sde::SDECostModel *costModel) {
  return std::make_unique<SdeScopeSelectionPass>(costModel);
}

} // namespace mlir::arts::sde
