///==========================================================================///
/// File: RealizeEdtDistributionPlan.cpp
///
/// Authors the ARTS EDT distribution plan from committed facts. The CODIR
/// boundary restates the committed dep pattern and access-window facts onto
/// each arts.edt; this pass derives the distribution realization plan from
/// them: the distribution family, the distribution version, and the block-halo
/// capability marker. It consumes already-committed structure and never
/// classifies movement or selects a distribution family from source patterns.
///==========================================================================///

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#define GEN_PASS_DEF_REALIZEEDTDISTRIBUTIONPLAN
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

struct RealizeEdtDistributionPlanPass
    : public impl::RealizeEdtDistributionPlanBase<
          RealizeEdtDistributionPlanPass> {
  void runOnOperation() override {
    getOperation().walk([](arts::EdtOp edt) {
      std::optional<arts::ArtsDepPattern> pattern =
          arts::getDepPattern(edt.getOperation());
      if (!pattern || *pattern == arts::ArtsDepPattern::unknown)
        return;

      Operation *op = edt.getOperation();
      if (std::optional<arts::EdtDistributionPattern> family =
              arts::getDistributionPatternForDepPattern(*pattern)) {
        arts::setEdtDistributionPattern(op, *family);
        arts::setDistributionVersion(op, 1);
      }

      // A stencil-family dep with a committed access window reads neighbor
      // tiles out of a per-block DB, so it advertises block-halo capability.
      if (arts::isStencilFamilyDepPattern(*pattern) &&
          edt->hasAttr(edt.getStencilMinOffsetsAttrName()) &&
          edt->hasAttr(edt.getStencilMaxOffsetsAttrName()))
        edt->setAttr(edt.getStencilSupportedBlockHaloAttrName(),
                     UnitAttr::get(edt.getContext()));
    });
  }
};

} // namespace

std::unique_ptr<Pass>
mlir::carts::arts::createRealizeEdtDistributionPlanPass() {
  return std::make_unique<RealizeEdtDistributionPlanPass>();
}
