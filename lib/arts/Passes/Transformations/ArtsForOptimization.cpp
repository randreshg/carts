///==========================================================================///
/// File: ArtsForOptimization.cpp
///
/// Applies pre-lowering optimization hints to arts.for loops.
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/ArtsHeuristics.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/Loop/LoopNode.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#include <optional>

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(arts_for_optimization);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct WorkerConfig {
  int64_t totalWorkers = 0;
  bool internode = false;
};

static bool shouldSkipCoarsening(ForOp forOp, LoopNode *loopNode) {
  if (loopNode) {
    if (loopNode->hasInterIterationDeps && *loopNode->hasInterIterationDeps)
      return true;
    if (loopNode->parallelClassification &&
        *loopNode->parallelClassification ==
            LoopMetadata::ParallelClassification::Sequential)
      return true;
  }

  if (auto loopAttr = forOp->getAttrOfType<LoopMetadataAttr>(
          AttrNames::LoopMetadata::Name)) {
    if (auto depsAttr = loopAttr.getHasInterIterationDeps())
      if (depsAttr.getValue())
        return true;

    if (auto classAttr = loopAttr.getParallelClassification()) {
      if (classAttr.getInt() == static_cast<int64_t>(
                                LoopMetadata::ParallelClassification::Sequential))
        return true;
    }
  }

  return false;
}

static std::optional<WorkerConfig>
resolveWorkerConfig(EdtOp parallelEdt, ArtsAbstractMachine *machine) {
  WorkerConfig cfg;
  cfg.internode = parallelEdt.getConcurrency() == EdtConcurrency::internode;

  if (auto workers = parallelEdt->getAttrOfType<workersAttr>(
          AttrNames::Operation::Workers)) {
    cfg.totalWorkers = workers.getValue();
    if (cfg.totalWorkers > 0)
      return cfg;
  }

  if (!machine)
    return std::nullopt;

  int threads = machine->hasValidThreads() ? machine->getThreads() : 0;
  if (threads <= 0)
    return std::nullopt;

  if (cfg.internode) {
    int nodeCount = machine->hasValidNodeCount() ? machine->getNodeCount() : 0;
    if (nodeCount <= 0)
      return std::nullopt;

    int workerThreads = threads;
    if (nodeCount > 1) {
      workerThreads = threads - machine->getOutgoingThreads() -
                      machine->getIncomingThreads();
      if (workerThreads <= 0)
        workerThreads = 1;
    }

    cfg.totalWorkers = static_cast<int64_t>(nodeCount) * workerThreads;
  } else {
    cfg.totalWorkers = threads;
  }

  if (cfg.totalWorkers <= 0)
    return std::nullopt;

  return cfg;
}

static std::optional<int64_t>
computeCoarsenedBlockSize(ForOp forOp, LoopNode *loopNode,
                          LoopAnalysis &loopAnalysis,
                          const WorkerConfig &workerCfg) {
  if (workerCfg.totalWorkers <= 0)
    return std::nullopt;

  if (shouldSkipCoarsening(forOp, loopNode))
    return std::nullopt;

  auto tripOpt = loopAnalysis.getStaticTripCount(forOp.getOperation());
  if (!tripOpt)
    return std::nullopt;

  int64_t tripCount = *tripOpt;
  if (tripCount <= 0)
    return std::nullopt;

  constexpr int64_t kMinItersPerWorker = 8;
  if (tripCount >= workerCfg.totalWorkers * kMinItersPerWorker)
    return std::nullopt;

  int64_t desiredWorkers =
      std::max<int64_t>(1, tripCount / kMinItersPerWorker);
  desiredWorkers = std::min(desiredWorkers, workerCfg.totalWorkers);

  int64_t blockSize = (tripCount + desiredWorkers - 1) / desiredWorkers;
  blockSize = std::max<int64_t>(1, blockSize);

  /// For internode execution, keep enough chunks so all worker lanes can still
  /// participate. This preserves distribution while still allowing mild
  /// coarsening for very small trip counts.
  if (workerCfg.internode) {
    int64_t maxBlockForAllWorkers = tripCount / workerCfg.totalWorkers;
    if (maxBlockForAllWorkers <= 1)
      return std::nullopt;
    blockSize = std::min(blockSize, maxBlockForAllWorkers);
  }

  if (blockSize <= 1)
    return std::nullopt;

  return blockSize;
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

    module.walk([&](EdtOp parallelEdt) {
      if (parallelEdt.getType() != EdtType::parallel)
        return;

      auto workerCfg = resolveWorkerConfig(parallelEdt, abstractMachine);
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

        LoopNode *loopNode = loopAnalysis.getOrCreateLoopNode(forOp.getOperation());
        auto coarsened =
            computeCoarsenedBlockSize(forOp, loopNode, loopAnalysis, *workerCfg);
        if (!coarsened)
          return;

        setPartitioningHint(forOp.getOperation(),
                            PartitioningHint::block(*coarsened));

        ARTS_INFO("Set arts.partition_hint blockSize="
                  << *coarsened << " for arts.for ("
                  << (workerCfg->internode ? "internode" : "intranode")
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
