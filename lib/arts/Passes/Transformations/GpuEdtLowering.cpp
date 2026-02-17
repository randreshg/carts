///==========================================================================///
/// File: GpuEdtLowering.cpp
///
/// Lower arts.gpu_edt to task EDTs annotated with GPU launch configuration.
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/OperationAttributes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(gpu_edt_lowering);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct GpuEdtLoweringPass
    : public arts::GpuEdtLoweringBase<GpuEdtLoweringPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *context = module.getContext();

    SmallVector<GpuEdtOp, 8> gpuEdts;
    module.walk([&](GpuEdtOp op) { gpuEdts.push_back(op); });

    for (GpuEdtOp gpuEdt : gpuEdts) {
      Location loc = gpuEdt.getLoc();
      OpBuilder builder(gpuEdt);

      Value route = gpuEdt.getRoute();
      if (!route)
        route = builder.create<arith::ConstantIntOp>(loc, 0, 32);

      SmallVector<Value> deps(gpuEdt.getDependencies().begin(),
                              gpuEdt.getDependencies().end());
      auto edt = builder.create<EdtOp>(loc, EdtType::task,
                                       EdtConcurrency::intranode, route, deps);

      if (auto launchConfig = gpuEdt.getLaunchConfigAttr())
        edt->setAttr(AttrNames::Operation::GpuLaunchConfig, launchConfig);

      if (auto targetAttr =
              gpuEdt->getAttrOfType<GpuTargetAttr>(AttrNames::Operation::GpuTarget))
        edt->setAttr(AttrNames::Operation::GpuTarget, targetAttr);

      Region &srcRegion = gpuEdt.getBody();
      Region &dstRegion = edt.getBody();
      dstRegion.takeBody(srcRegion);

      if (dstRegion.empty())
        dstRegion.push_back(new Block());

      Block &dstBlock = dstRegion.front();
      if (dstBlock.getNumArguments() == 0) {
        for (Value dep : deps)
          dstBlock.addArgument(dep.getType(), loc);
      }

      for (auto [dep, arg] : llvm::zip(deps, dstBlock.getArguments())) {
        dep.replaceUsesWithIf(arg, [&](OpOperand &use) {
          return dstRegion.isAncestor(use.getOwner()->getParentRegion());
        });
      }

      if (gpuEdt->getNumResults() > 0 && !gpuEdt->use_empty()) {
        auto nullGuid = builder.create<arith::ConstantIntOp>(loc, 0, 64);
        gpuEdt.getResult().replaceAllUsesWith(nullGuid);
      }

      gpuEdt.erase();
    }
  }
};

} // namespace

std::unique_ptr<Pass> mlir::arts::createGpuEdtLoweringPass() {
  return std::make_unique<GpuEdtLoweringPass>();
}
