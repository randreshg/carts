///==========================================================================///
/// ArtsKernelTransforms.cpp - Kernel-form transforms for ARTS
///
/// This pass owns semantics-preserving rewrites that change how a kernel is
/// expressed so later ARTS passes can distribute and partition it better.
///
/// This is intentionally separate from:
///   - ArtsDepTransforms: exact dependence/schedule rewrites
///   - LoopNormalization: structural cleanup only
///   - BlockLoopStripMining: late DB-aware cleanup after partitioning
///
/// Current ownership:
///   - MatmulReductionPattern
///==========================================================================///

#include "../../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Transforms/Kernel/KernelTransform.h"
#include "arts/Utils/ArtsDebug.h"

ARTS_DEBUG_SETUP(kernel_transforms);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct ArtsKernelTransformsPass
    : public arts::ArtsKernelTransformsBase<ArtsKernelTransformsPass> {
  ArtsKernelTransformsPass(ArtsAnalysisManager *AM, bool enableMatmul,
                           bool enableTiling, int64_t tileJ,
                           int64_t minTripCount)
      : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
    this->enableMatmul = enableMatmul;
    this->enableTiling = enableTiling;
    this->tileJ = tileJ;
    this->minTripCount = minTripCount;
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    ARTS_INFO_HEADER(ArtsKernelTransformsPass);

    SmallVector<std::unique_ptr<LoopPattern>> patterns;
    if (enableMatmul)
      patterns.push_back(
          createMatmulReductionPattern(enableTiling, tileJ, minTripCount));

    SmallVector<ForOp, 16> artsFors;
    module.walk([&](ForOp fo) { artsFors.push_back(fo); });

    int rewrites = 0;
    for (ForOp fo : artsFors) {
      OpBuilder builder(fo);
      for (auto &pattern : patterns) {
        if (pattern->match(fo)) {
          if (succeeded(pattern->apply(builder))) {
            rewrites++;
            break;
          }
        }
      }
    }

    ARTS_INFO("ArtsKernelTransformsPass: applied " << rewrites
                                                   << " rewrite(s)");
    (void)AM;
    ARTS_INFO_FOOTER(ArtsKernelTransformsPass);
  }

private:
  ArtsAnalysisManager *AM;
};

} // namespace

std::unique_ptr<Pass> mlir::arts::createArtsKernelTransformsPass(
    ArtsAnalysisManager *AM, bool enableMatmul, bool enableTiling,
    int64_t tileJ, int64_t minTripCount) {
  return std::make_unique<ArtsKernelTransformsPass>(
      AM, enableMatmul, enableTiling, tileJ, minTripCount);
}
