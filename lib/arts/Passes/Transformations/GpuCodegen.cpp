///==========================================================================///
/// File: GpuCodegen.cpp
///
/// Final GPU codegen preparation checks.
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(gpu_codegen);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct GpuCodegenPass : public arts::GpuCodegenBase<GpuCodegenPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool hasGpuEdt = false;
    module.walk([&](GpuEdtOp op) {
      hasGpuEdt = true;
      op.emitError("GpuEdtOp must be lowered before gpu-codegen");
    });
    if (hasGpuEdt)
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::arts::createGpuCodegenPass() {
  return std::make_unique<GpuCodegenPass>();
}
