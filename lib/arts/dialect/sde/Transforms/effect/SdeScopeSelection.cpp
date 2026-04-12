///==========================================================================///
/// File: SdeScopeSelection.cpp
///
/// Cost-model-backed SDE concurrency scope selection. The current
/// implementation preserves explicit scope, lets nested parallel SDE regions
/// inherit the nearest enclosing parallel scope, and otherwise fills missing
/// scope from the active machine topology.
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

static void assignParallelScopes(Operation *operation,
                                 sde::SdeConcurrencyScopeAttr inheritedScope,
                                 sde::SdeConcurrencyScopeAttr defaultScope) {
  auto currentScope = inheritedScope;
  if (auto cuRegion = dyn_cast<sde::SdeCuRegionOp>(operation);
      cuRegion && cuRegion.getKind() == sde::SdeCuKind::parallel) {
    currentScope = cuRegion.getConcurrencyScopeAttr();
    if (!currentScope) {
      currentScope = inheritedScope ? inheritedScope : defaultScope;
      cuRegion.setConcurrencyScopeAttr(currentScope);
    }
  }

  for (Region &region : operation->getRegions())
    for (Block &block : region)
      for (Operation &nestedOp : block)
        assignParallelScopes(&nestedOp, currentScope, defaultScope);
}

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

    assignParallelScopes(getOperation(), {}, desiredScope);
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
