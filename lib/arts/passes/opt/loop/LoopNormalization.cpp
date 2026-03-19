///==========================================================================///
/// LoopNormalization.cpp - Pattern-based loop normalization pass
///
/// Applies loop normalization patterns to enable downstream DbPartitioning.
/// Each pattern lives in its own file under lib/arts/transforms/Loop/ and is
/// registered here in priority order (first match wins per loop).
///
/// Current patterns:
///   - SymmetricTriangularPattern (triangular → rectangular)
///   - PerfectNestLinearizationPattern (collapse profitable perfect nests)
///
/// Heavier semantic rewrites such as matmul kernel-form lowering and cache-
/// oriented loop reordering live in KernelTransforms / LoopReordering,
/// not here.
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

#include "arts/analysis/AnalysisManager.h"
#define GEN_PASS_DEF_LOOPNORMALIZATION
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "arts/passes/Passes.h.inc"
#include "arts/passes/Passes.h"
#include "arts/transforms/loop/LoopNormalizer.h"
#include "arts/utils/Debug.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

ARTS_DEBUG_SETUP(loop_normalization);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct LoopNormalizationPass
    : public arts::impl::LoopNormalizationBase<LoopNormalizationPass> {
  LoopNormalizationPass(mlir::arts::AnalysisManager *AM) : AM(AM) {}

  void runOnOperation() override {
    ModuleOp module = getOperation();

    ARTS_INFO_HEADER(LoopNormalizationPass);

    /// Register structural normalization patterns (order = priority)
    SmallVector<std::unique_ptr<LoopPattern>> patterns;
    patterns.push_back(createSymmetricTriangularPattern());
    patterns.push_back(createPerfectNestLinearizationPattern());

    int rewrites = 0;
    bool changed = true;
    while (changed) {
      changed = false;

      /// Collect arts.for ops upfront for this round (avoid iterator
      /// invalidation while still allowing follow-on rewrites on newly created
      /// arts.for ops in the next round).
      SmallVector<ForOp> artsFors;
      module.walk([&](ForOp fo) { artsFors.push_back(fo); });

      for (ForOp fo : artsFors) {
        if (!fo || !fo->getBlock())
          continue;

        OpBuilder builder(fo);
        for (auto &pattern : patterns) {
          if (pattern->match(fo)) {
            ARTS_DEBUG("Matched " << pattern->getName()
                                  << " pattern on arts.for");
            if (succeeded(pattern->apply(builder))) {
              ARTS_INFO("Applied " << pattern->getName() << " pattern");
              rewrites++;
              changed = true;
              break; /// first match wins per loop per round
            }
          }
        }
      }
    }

    ARTS_INFO_FOOTER("LoopNormalizationPass: " << rewrites << " rewrites");
    (void)rewrites;
  }

private:
  [[maybe_unused]] mlir::arts::AnalysisManager *AM;
};

} // namespace

std::unique_ptr<Pass>
mlir::arts::createLoopNormalizationPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<LoopNormalizationPass>(AM);
}
