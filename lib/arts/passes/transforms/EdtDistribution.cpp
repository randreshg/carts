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

#include "../PassDetails.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/DistributionHeuristics.h"
#include "arts/passes/Passes.h"
#include "arts/utils/OperationAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include <cassert>

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(edt_distribution);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct EdtDistributionPass
    : public arts::EdtDistributionBase<EdtDistributionPass> {
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

      DistributionStrategy strategy = DistributionHeuristics::analyzeStrategy(
          edt.getConcurrency(), machine);

      edt.walk([&](ForOp forOp) {
        if (forOp->getParentOfType<EdtOp>() != edt)
          return;

        EdtDistributionPattern pattern = EdtDistributionPattern::unknown;
        if (auto analyzedPattern =
                AM->getLoopDistributionPattern(forOp.getOperation()))
          pattern = *analyzedPattern;
        EdtDistributionKind kind =
            DistributionHeuristics::selectDistributionKind(strategy, pattern);
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
