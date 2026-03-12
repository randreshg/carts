///==========================================================================///
/// LoopNormalization.cpp - Pattern-based loop normalization pass
///
/// Applies loop normalization patterns to enable downstream DbPartitioning.
/// Each pattern lives in its own file under lib/arts/Transforms/Loop/ and is
/// registered here in priority order (first match wins per loop).
///
/// Phase 1: SymmetricTriangularPattern (triangular → rectangular)
/// Phase 2: LoopInterchangePattern, MatmulReductionPattern (future migration)
///
/// Example:
///   Before:
///     scf.for %i = ... {
///       scf.for %j = %i to %N { ... }
///     }
///
///   After:
///     scf.for %i = ... {
///       scf.for %j = 0 to %N { if (%j >= %i) ... }
///     }
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Transforms/Loop/LoopNormalizer.h"
#include "arts/Utils/ArtsDebug.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

ARTS_DEBUG_SETUP(loop_normalization);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct LoopNormalizationPass
    : public arts::LoopNormalizationBase<LoopNormalizationPass> {
  LoopNormalizationPass(ArtsAnalysisManager *AM) : AM(AM) {}

  void runOnOperation() override {
    ModuleOp module = getOperation();

    ARTS_INFO_HEADER(LoopNormalizationPass);

    /// Register all available patterns (order = priority)
    SmallVector<std::unique_ptr<LoopPattern>> patterns;
    patterns.push_back(createSymmetricTriangularPattern());
    patterns.push_back(createPerfectNestLinearizationPattern());
    /// Phase 2: patterns.push_back(createLoopInterchangePattern(AM));
    /// Phase 2: patterns.push_back(createMatmulReductionPattern(AM));

    /// Collect arts.for ops upfront (avoid iterator invalidation)
    SmallVector<ForOp> artsFors;
    module.walk([&](ForOp fo) { artsFors.push_back(fo); });

    int rewrites = 0;
    for (ForOp fo : artsFors) {
      OpBuilder builder(fo);
      for (auto &pattern : patterns) {
        if (pattern->match(fo)) {
          ARTS_DEBUG("Matched " << pattern->getName()
                                << " pattern on arts.for");
          if (succeeded(pattern->apply(builder))) {
            ARTS_INFO("Applied " << pattern->getName() << " pattern");
            rewrites++;
            break; /// first match wins per loop
          }
        }
      }
    }

    ARTS_INFO_FOOTER("LoopNormalizationPass: " << rewrites << " rewrites");
    (void)rewrites;
  }

private:
  [[maybe_unused]] ArtsAnalysisManager *AM;
};

} // namespace

std::unique_ptr<Pass>
mlir::arts::createLoopNormalizationPass(ArtsAnalysisManager *AM) {
  return std::make_unique<LoopNormalizationPass>(AM);
}
