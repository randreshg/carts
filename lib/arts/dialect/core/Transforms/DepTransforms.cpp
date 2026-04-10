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

#define GEN_PASS_DEF_DEPTRANSFORMS
#include "arts/Dialect.h"
#include "arts/dialect/core/Analysis/AnalysisDependencies.h"
#include "arts/dialect/core/Analysis/AnalysisManager.h"
#include "arts/dialect/core/Transforms/dep/DepTransform.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/utils/Debug.h"
#include "mlir/Pass/Pass.h"

ARTS_DEBUG_SETUP(dep_transforms);

using namespace mlir;
using namespace mlir::arts;

static const AnalysisKind kDepTransforms_reads[] = {
    AnalysisKind::MetadataManager};
[[maybe_unused]] static const AnalysisDependencyInfo kDepTransforms_deps = {
    kDepTransforms_reads, {}};

namespace {

struct DepTransformsPass : public impl::DepTransformsBase<DepTransformsPass> {
  explicit DepTransformsPass(mlir::arts::AnalysisManager *AM) : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    auto &metadataManager = AM->getMetadataManager();
    SmallVector<std::unique_ptr<DepPatternTransform>> patterns;
    patterns.push_back(createSeidel2DWavefrontPattern(metadataManager));
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

private:
  mlir::arts::AnalysisManager *AM;
};

} // namespace

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createDepTransformsPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<DepTransformsPass>(AM);
}

} // namespace arts
} // namespace mlir
