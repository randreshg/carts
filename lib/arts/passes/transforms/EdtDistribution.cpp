///==========================================================================///
/// File: EdtDistribution.cpp
///
/// Annotates arts.for loops with distribution strategy decisions while keeping
/// loop semantics unchanged.
///
/// Transformation (annotation only):
///   BEFORE:
///     arts.for (...) {
///       ...
///     }
///
///   AFTER:
///     arts.for (...) {
///       ...
///     } {distribution_kind = ..., distribution_pattern = ...}
///
/// This pass does not create/remove dependencies; it only writes typed attrs
///
/// Example:
///   matmul loop in internode EDT -> annotation picks
///   `distribution_kind=tiling_2d`
/// consumed later by lowering.
///==========================================================================///

#define GEN_PASS_DEF_EDTDISTRIBUTION
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "arts/passes/Passes.h.inc"
#include "arts/analysis/AnalysisManager.h"
#include "arts/passes/Passes.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include <cassert>

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(edt_distribution);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct EdtDistributionPass
    : public impl::EdtDistributionBase<EdtDistributionPass> {
  explicit EdtDistributionPass(mlir::arts::AnalysisManager *AM) : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    auto *machine = &AM->getAbstractMachine();
    if (!machine->hasConfigFile() || !machine->hasValidNodeCount() ||
        !machine->hasValidThreads()) {
      module.emitError(
          "invalid ARTS machine configuration for distribution planning");
      signalPassFailure();
      return;
    }

    /// Query distribution patterns from DB analysis built from the current IR.
    AM->getDbAnalysis().invalidate();

    module.walk([&](EdtOp edt) {
      if (edt.getType() != EdtType::parallel && edt.getType() != EdtType::task)
        return;

      auto &heuristics = AM->getEdtHeuristics();
      DistributionStrategy strategy =
          heuristics.chooseStrategy(edt.getConcurrency());

      edt.walk([&](ForOp forOp) {
        if (forOp->getParentOfType<EdtOp>() != edt)
          return;

        EdtDistributionPattern pattern = EdtDistributionPattern::unknown;
        if (auto analyzedPattern =
                heuristics.resolveDistributionPattern(forOp, edt))
          pattern = *analyzedPattern;
        EdtDistributionKind kind = heuristics.chooseKind(strategy, pattern);
        /// Keep distribution_kind focused on execution decomposition.
        /// 2-D stencil owner semantics already flow through the stencil/lowering
        /// contract attrs (owner dims, halo, block shape). Overloading
        /// `tiling_2d` here forces stencil loops such as Seidel through the
        /// matmul-oriented Tile2DTaskLoopLowering path even when they only need
        /// block-style task distribution with N-D ownership preserved.
        setEdtDistributionKind(forOp.getOperation(), kind);
        setEdtDistributionPattern(forOp.getOperation(), pattern);
        forOp->setAttr(
            AttrNames::Operation::DistributionVersion,
            IntegerAttr::get(IntegerType::get(forOp.getContext(), 32), 1));

        ARTS_DEBUG("Annotated arts.for with kind="
                   << static_cast<int>(kind)
                   << " pattern=" << static_cast<int>(pattern));
      });
    });
  }

private:
  mlir::arts::AnalysisManager *AM = nullptr;
};

} // namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass>
createEdtDistributionPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<EdtDistributionPass>(AM);
}
} // namespace arts
} // namespace mlir
