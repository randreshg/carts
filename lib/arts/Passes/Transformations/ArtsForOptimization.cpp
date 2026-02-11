///==========================================================================///
/// File: ArtsForOptimization.cpp
///
/// Applies pre-lowering optimization hints to arts.for loops.
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/ArtsHeuristics.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "arts/Utils/OperationAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#include <optional>

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(arts_for_optimization);

using namespace mlir;
using namespace mlir::arts;

namespace {

static void annotateAccessPatterns(ModuleOp module, ArtsAnalysisManager *AM) {
  if (!AM)
    return;

  /// Rebuild DB analysis from current IR before querying alloc access patterns.
  auto &dbAnalysis = AM->getDbAnalysis();
  dbAnalysis.invalidate();

  module.walk([&](DbAllocOp allocOp) {
    if (allocOp.getElementSizes().empty())
      return;

    DbAccessPattern pattern = DbAccessPattern::uniform;
    if (auto analyzed = dbAnalysis.getAllocAccessPattern(allocOp))
      pattern = *analyzed;
    if (pattern == DbAccessPattern::unknown)
      pattern = DbAccessPattern::uniform;

    setDbAccessPattern(allocOp.getOperation(), pattern);
    ARTS_DEBUG("Annotated alloc " << allocOp << " with access_pattern="
                                  << stringifyDbAccessPattern(pattern));
  });
}

struct ArtsForOptimizationPass
    : public arts::ArtsForOptimizationBase<ArtsForOptimizationPass> {
  explicit ArtsForOptimizationPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override {
    module = getOperation();
    abstractMachine = &AM->getAbstractMachine();
    LoopAnalysis &loopAnalysis = AM->getLoopAnalysis();

    ARTS_INFO_HEADER(ArtsForOptimization);
    ARTS_DEBUG_REGION(module.dump(););

    annotateAccessPatterns(module, AM);

    module.walk([&](EdtOp parallelEdt) {
      if (parallelEdt.getType() != EdtType::parallel)
        return;

      auto workerCfg = DistributionHeuristics::resolveWorkerConfig(
          parallelEdt, abstractMachine);
      if (!workerCfg) {
        ARTS_DEBUG("Skipping arts.for optimization (unknown worker count)");
        return;
      }

      parallelEdt.walk([&](ForOp forOp) {
        if (forOp->getParentOfType<EdtOp>() != parallelEdt)
          return;

        /// Respect explicit user/compiler hints already present.
        if (getPartitioningHint(forOp.getOperation()))
          return;

        auto coarsened = DistributionHeuristics::computeCoarsenedBlockHint(
            forOp, loopAnalysis, *workerCfg);
        if (!coarsened)
          return;

        setPartitioningHint(forOp.getOperation(),
                            PartitioningHint::block(*coarsened));

        ARTS_INFO("Set arts.partition_hint blockSize="
                  << *coarsened << " for arts.for ("
                  << stringifyEdtConcurrency(parallelEdt.getConcurrency())
                  << ", workers=" << workerCfg->totalWorkers << ")");
      });
    });

    ARTS_INFO_FOOTER(ArtsForOptimization);
    ARTS_DEBUG_REGION(module.dump(););
  }

private:
  ArtsAnalysisManager *AM = nullptr;
  ArtsAbstractMachine *abstractMachine = nullptr;
  ModuleOp module;
};

} // namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createArtsForOptimizationPass(ArtsAnalysisManager *AM) {
  return std::make_unique<ArtsForOptimizationPass>(AM);
}
} // namespace arts
} // namespace mlir
