///==========================================================================///
/// File: DepTransforms.cpp
///
/// Dependence-preserving schedule rewrites that expose scalable task and DB
/// shapes before DB creation. This pass runs on ARTS/scf IR after
/// OpenMP-to-ARTS conversion, and owns structural transforms whose primary
/// purpose is changing the dependence shape seen by later ARTS passes.
///
/// Current patterns:
///   - Seidel2DWavefrontPattern
///   - JacobiAlternatingBuffersPattern
///==========================================================================///

#include "arts/passes/PassDetails.h"
#include "arts/passes/Passes.h"
#include "arts/transforms/dep/DepTransform.h"
#include "arts/utils/Debug.h"
#include "mlir/Pass/Pass.h"

ARTS_DEBUG_SETUP(dep_transforms);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct DepTransformsPass : public arts::DepTransformsBase<DepTransformsPass> {
  using Base::Base;

  void runOnOperation() override {
    ModuleOp module = getOperation();
    SmallVector<std::unique_ptr<DepPatternTransform>> patterns;
    patterns.push_back(createSeidel2DWavefrontPattern());
    patterns.push_back(createJacobiAlternatingBuffersPattern());

    int rewrites = 0;
    for (auto &pattern : patterns) {
      int applied = pattern->run(module);
      rewrites += applied;
      if (applied > 0)
        ARTS_INFO("Applied " << pattern->getName() << " (" << applied
                             << " rewrite(s))");
    }

    ARTS_INFO("DepTransformsPass: applied " << rewrites << " rewrite(s)");
  }
};

} // namespace

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createDepTransformsPass() {
  return std::make_unique<DepTransformsPass>();
}

} // namespace arts
} // namespace mlir
