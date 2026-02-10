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
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
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
      if (classAttr.getInt() ==
          static_cast<int64_t>(
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

  int64_t desiredWorkers = std::max<int64_t>(1, tripCount / kMinItersPerWorker);
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

/// Trace a db_ref source (inside an EDT body) back to the DbAllocOp it
/// originates from.  The chain is:
///   EDT block arg → EDT operand → db_acquire ptr result → db_alloc
static DbAllocOp traceDbRefToAlloc(Value dbRefSource) {
   /// Inside the EDT, the source is a block argument
  auto blockArg = dyn_cast<BlockArgument>(dbRefSource);
  if (!blockArg)
    return nullptr;

  auto parentOp = blockArg.getOwner()->getParentOp();
  auto edt = dyn_cast_or_null<EdtOp>(parentOp);
  if (!edt)
    return nullptr;

   /// Map block arg index to EDT operand (dependencies list)
  unsigned argIdx = blockArg.getArgNumber();
  auto deps = edt.getDependencies();
  if (argIdx >= deps.size())
    return nullptr;

  Value edtOperand = deps[argIdx];

   /// The operand should be a db_acquire ptr result
  if (auto acq = edtOperand.getDefiningOp<DbAcquireOp>()) {
     /// Trace through source_guid (input guid) or source_ptr (input ptr)
    if (Value srcGuid = acq.getSourceGuid())
      if (auto alloc = srcGuid.getDefiningOp<DbAllocOp>())
        return alloc;
    if (Value srcPtr = acq.getSourcePtr())
      return srcPtr.getDefiningOp<DbAllocOp>();
  }

  return nullptr;
}

/// Detect stencil access patterns by analyzing memory accesses inside
/// arts.for loops within parallel EDTs.  At the ArtsForOptimization stage,
/// acquires are coarse and live outside the parallel EDT — the partition-
/// bounds machinery cannot detect stencil patterns yet because partition
/// offsets/sizes haven't been assigned.  Instead, we directly inspect
/// load/store indices relative to the arts.for IV.
static void annotateAccessPatterns(ModuleOp module, ArtsAnalysisManager *AM) {
   /// Collect which DbAllocOps have stencil access patterns
  DenseSet<Operation *> stencilAllocs;

  module.walk([&](EdtOp parallelEdt) {
    if (parallelEdt.getType() != EdtType::parallel)
      return;

     /// Find arts.for loops directly inside this parallel EDT
    SmallVector<ForOp> forOps;
    parallelEdt.walk([&](ForOp forOp) {
      if (forOp->getParentOfType<EdtOp>() == parallelEdt)
        forOps.push_back(forOp);
    });
    if (forOps.empty())
      return;

    for (ForOp forOp : forOps) {
       /// Get the loop IV (first block argument of the for body)
      Block &body = forOp.getRegion().front();
      if (body.getNumArguments() == 0)
        continue;
      Value loopIV = body.getArgument(0);

       /// Walk all memory accesses inside this for loop
      forOp.walk([&](Operation *memOp) {
        if (!isa<memref::LoadOp, memref::StoreOp>(memOp))
          return;

         /// Get the memref being accessed
        Value memref =
            isa<memref::LoadOp>(memOp)
                ? cast<memref::LoadOp>(memOp).getMemRef()
                : cast<memref::StoreOp>(memOp).getMemRef();

         /// Trace memref back to a db_ref op
        auto dbRef = memref.getDefiningOp<DbRefOp>();
        if (!dbRef)
          return;

         /// Trace the db_ref source to the originating DbAllocOp
        DbAllocOp allocOp = traceDbRefToAlloc(dbRef.getSource());
        if (!allocOp)
          return;

         /// Check memory access indices for stencil pattern:
         /// If any index = loopIV + nonzero_constant, it's stencil
        SmallVector<Value> indices =
            DatablockUtils::collectFullIndexChain(dbRef, memOp);
        for (Value idx : indices) {
          auto offset =
              ValueUtils::extractConstantOffset(idx, loopIV, loopIV);
          if (offset && *offset != 0) {
            stencilAllocs.insert(allocOp.getOperation());
            return;
          }
        }
      });
    }
  });

   /// Now annotate all DbAllocOps: stencil if detected, uniform otherwise
  module.walk([&](DbAllocOp allocOp) {
    if (allocOp.getElementSizes().empty())
      return;
    DbAccessPattern pattern = stencilAllocs.contains(allocOp.getOperation())
                                  ? DbAccessPattern::stencil
                                  : DbAccessPattern::uniform;
    setDbAccessPattern(allocOp.getOperation(), pattern);
    ARTS_DEBUG("Annotated alloc "
               << allocOp << " with access_pattern="
               << (pattern == DbAccessPattern::stencil ? "stencil"
                                                       : "uniform"));
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

        LoopNode *loopNode =
            loopAnalysis.getOrCreateLoopNode(forOp.getOperation());
        auto coarsened = computeCoarsenedBlockSize(forOp, loopNode,
                                                   loopAnalysis, *workerCfg);
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

}  /// namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createArtsForOptimizationPass(ArtsAnalysisManager *AM) {
  return std::make_unique<ArtsForOptimizationPass>(AM);
}
}  /// namespace arts
}  /// namespace mlir
