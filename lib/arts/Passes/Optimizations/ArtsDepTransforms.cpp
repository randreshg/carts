///==========================================================================///
/// File: ArtsDepTransforms.cpp
///
/// Dependence-preserving schedule rewrites that expose scalable task and DB
/// shapes before DB creation. This pass owns structural transforms whose
/// primary purpose is changing the dependence shape seen by later ARTS passes.
///
/// Current patterns:
///   - JacobiAlternatingBuffersPattern
///
/// Planned patterns:
///   - Seidel2DWavefrontPattern
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Transforms/Dependence/DependenceTransform.h"
#include "arts/Utils/ArtsDebug.h"
#include "mlir/Pass/Pass.h"

ARTS_DEBUG_SETUP(arts_dep_transforms);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct ArtsDepTransformsPass
    : public arts::ArtsDepTransformsBase<ArtsDepTransformsPass> {
  using Base::Base;

  void runOnOperation() override {
    ModuleOp module = getOperation();
    SmallVector<std::unique_ptr<DependenceTransform>> patterns;
    patterns.push_back(createJacobiAlternatingBuffersPattern());

    int rewrites = 0;
    for (auto &pattern : patterns) {
      int applied = pattern->run(module);
      rewrites += applied;
      if (applied > 0)
        ARTS_INFO("Applied " << pattern->getName() << " (" << applied
                             << " rewrite(s))");
    }

    ARTS_INFO("ArtsDepTransformsPass: applied " << rewrites << " rewrite(s)");
  }
};

} // namespace

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createArtsDepTransformsPass() {
  return std::make_unique<ArtsDepTransformsPass>();
}

} // namespace arts
} // namespace mlir
