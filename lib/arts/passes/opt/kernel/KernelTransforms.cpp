///==========================================================================///
/// KernelTransforms.cpp - Kernel-form transforms for ARTS
///
/// This pass owns semantics-preserving rewrites that change how a kernel is
/// expressed so later ARTS passes can distribute and partition it better.
///
/// This is intentionally separate from:
///   - DepTransforms: exact dependence/schedule rewrites
///   - LoopNormalization: structural cleanup only
///   - BlockLoopStripMining: late DB-aware cleanup after partitioning
///
/// Current ownership:
///   - elementwise_pipeline
///   - StencilTilingNDPattern
///   - MatmulReductionPattern
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/passes/Passes.h"
#include "arts/transforms/kernel/KernelTransform.h"
#include "arts/utils/Debug.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace mlir::arts;

#define GEN_PASS_DEF_KERNELTRANSFORMS
#include "arts/passes/Passes.h.inc"

ARTS_DEBUG_SETUP(kernel_transforms);

namespace {

struct KernelTransformsPass
    : public ::impl::KernelTransformsBase<KernelTransformsPass> {
  KernelTransformsPass(mlir::arts::AnalysisManager *AM,
                       bool enableElementwisePipeline, bool enableMatmul,
                       bool enableTiling, int64_t tileJ, int64_t minTripCount)
      : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
    this->enableElementwisePipeline = enableElementwisePipeline;
    this->enableMatmul = enableMatmul;
    this->enableTiling = enableTiling;
    this->tileJ = tileJ;
    this->minTripCount = minTripCount;
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    auto &metadataManager = AM->getMetadataManager();
    ARTS_INFO_HEADER(KernelTransformsPass);

    int rewrites = 0;
    if (enableElementwisePipeline)
      rewrites += applyElementwisePipelineTransform(module, metadataManager);

    SmallVector<std::unique_ptr<KernelPatternTransform>> patterns;
    patterns.push_back(createStencilTilingNDPattern());
    if (enableMatmul) {
      patterns.push_back(createMatmulReductionPattern(
          enableTiling, tileJ, minTripCount, metadataManager));
    }

    SmallVector<ForOp, 16> artsFors;
    module.walk([&](ForOp fo) { artsFors.push_back(fo); });

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

    ARTS_INFO("KernelTransformsPass: applied " << rewrites << " rewrite(s)");
    (void)AM;
    ARTS_INFO_FOOTER(KernelTransformsPass);
  }

private:
  mlir::arts::AnalysisManager *AM;
};

} // namespace

std::unique_ptr<Pass> mlir::arts::createKernelTransformsPass(
    mlir::arts::AnalysisManager *AM, bool enableElementwisePipeline,
    bool enableMatmul, bool enableTiling, int64_t tileJ, int64_t minTripCount) {
  return std::make_unique<KernelTransformsPass>(AM, enableElementwisePipeline,
                                                enableMatmul, enableTiling,
                                                tileJ, minTripCount);
}
