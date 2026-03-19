///==========================================================================///
/// File: ForOpt.cpp
///
/// Applies pre-lowering optimization hints to arts.for loops.
///
/// Example:
///   Before:
///     arts.for ... { ... }
///
///   After:
///     arts.for ... { ... }  // with tuning attrs/hints for downstream lowering
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/heuristics/PartitioningHeuristics.h"
#include "arts/analysis/loop/LoopAnalysis.h"
#define GEN_PASS_DEF_FOROPT
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "arts/passes/Passes.h.inc"
#include "arts/passes/Passes.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/abstract_machine/AbstractMachine.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#include <optional>

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(for_opt);

using namespace mlir;
using namespace mlir::arts;

namespace {

static void annotateAccessPatterns(ModuleOp module,
                                   mlir::arts::AnalysisManager *AM) {
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

struct ForOptPass : public impl::ForOptBase<ForOptPass> {
  explicit ForOptPass(mlir::arts::AnalysisManager *AM) : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
  }

  void runOnOperation() override {
    module = getOperation();
    abstractMachine = &AM->getAbstractMachine();
    LoopAnalysis &loopAnalysis = AM->getLoopAnalysis();

    ARTS_INFO_HEADER(ForOpt);
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

    ARTS_INFO_FOOTER(ForOpt);
    ARTS_DEBUG_REGION(module.dump(););
  }

private:
  mlir::arts::AnalysisManager *AM = nullptr;
  AbstractMachine *abstractMachine = nullptr;
  ModuleOp module;
};

} // namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createForOptPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<ForOptPass>(AM);
}
} // namespace arts
} // namespace mlir
