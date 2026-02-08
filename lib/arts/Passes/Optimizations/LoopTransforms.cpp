///==========================================================================///
/// LoopTransforms.cpp - Loop transformation pass
///
/// Delegates to LoopPattern-based matmul reduction distribution.
/// The actual pattern logic lives in MatmulReductionPattern.cpp.
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Transforms/Loop/LoopNormalizer.h"
#include "arts/Utils/ArtsDebug.h"

ARTS_DEBUG_SETUP(loop_transforms);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct LoopTransformsPass
    : public arts::ArtsLoopTransformsBase<LoopTransformsPass> {
  LoopTransformsPass(ArtsAnalysisManager *AM, bool enableMatmul,
                     bool enableTiling, int64_t tileJ, int64_t minTripCount)
      : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
    this->enableMatmul = enableMatmul;
    this->enableTiling = enableTiling;
    this->tileJ = tileJ;
    this->minTripCount = minTripCount;
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    ARTS_INFO_HEADER(LoopTransformsPass);

    /// Build pattern list based on options
    SmallVector<std::unique_ptr<LoopPattern>> patterns;
    if (enableMatmul)
      patterns.push_back(
          createMatmulReductionPattern(enableTiling, tileJ, minTripCount));

    /// Collect arts.for ops first to avoid iterator invalidation during
    /// rewrites.
    SmallVector<ForOp, 16> artsFors;
    module.walk([&](ForOp fo) { artsFors.push_back(fo); });

    int rewrites = 0;
    for (ForOp fo : artsFors) {
      OpBuilder builder(fo);
      for (auto &pattern : patterns) {
        if (pattern->match(fo)) {
          if (succeeded(pattern->apply(builder))) {
            rewrites++;
            break; /// first match wins per loop
          }
        }
      }
    }

    ARTS_INFO("LoopTransformsPass: applied " << rewrites << " rewrite(s)");
    (void)AM;
    ARTS_INFO_FOOTER(LoopTransformsPass);
  }

private:
  ArtsAnalysisManager *AM;
};

} // namespace

std::unique_ptr<Pass>
mlir::arts::createLoopTransformsPass(ArtsAnalysisManager *AM, bool enableMatmul,
                                     bool enableTiling, int64_t tileJ,
                                     int64_t minTripCount) {
  return std::make_unique<LoopTransformsPass>(AM, enableMatmul, enableTiling,
                                              tileJ, minTripCount);
}
