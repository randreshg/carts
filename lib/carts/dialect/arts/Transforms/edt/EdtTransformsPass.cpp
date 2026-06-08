///==========================================================================///
/// File: EdtTransformsPass.cpp
///
/// Post-DB EDT transforms pass. Hosts EDT-level transformations that run
/// after DbTransforms and before the final DbModeTightening mode tightening.
///
/// Current transforms:
///   Mixed-root rejection -- fail closed when an EDT mixes a coarse and a
///              subpartitioned acquire of the same root DB without
///              planned-block evidence; the single grain belongs upstream.
///
///   Task granularity diagnostics -- estimate task cost from EdtInfo metrics
///              and warn on trivially small tasks.
///
///   Dead dependency elimination -- remove unused dependency slots whose block
///              arguments have zero uses or are only consumed by cleanup or
///              true-only compiler control-token stores.
///
///==========================================================================///

/// LLVM ADT
#include "llvm/ADT/DenseSet.h"
/// Dialects
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
/// Arts
#include "carts/dialect/arts/Analysis/AnalysisManager.h"
#include "carts/dialect/arts/Analysis/edt/EdtInfo.h"
#include "carts/dialect/arts/Analysis/graphs/base/EdgeBase.h"
#include "carts/dialect/arts/Analysis/graphs/base/NodeBase.h"
#include "carts/dialect/arts/Analysis/graphs/edt/EdtGraph.h"
#include "carts/dialect/arts/Analysis/graphs/edt/EdtNode.h"
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/EdtUtils.h"
#include "carts/utils/RemovalUtils.h"
/// Statistics
#include "llvm/ADT/Statistic.h"
/// Debug
#include "carts/utils/Debug.h"
#include <algorithm>
#include <cmath>

using namespace mlir;
using namespace mlir::func;
using namespace mlir::carts;
using namespace mlir::carts::arts;

#define GEN_PASS_DEF_EDTTRANSFORMS
#include "carts/passes/Passes.h.inc"

ARTS_DEBUG_SETUP(edt_transforms);

static llvm::Statistic numGranularityAnnotations{
    "edt_transforms", "NumGranularityAnnotations",
    "Number of task granularity diagnostics"};
static llvm::Statistic numDeadDepsRemoved{
    "edt_transforms", "NumDeadDepsRemoved", "Number of dead deps removed"};

namespace {

/// Minimum cost threshold below which an EDT is considered trivially small.
/// Tasks below this threshold emit a diagnostic warning suggesting fusion.
static constexpr int64_t kSmallTaskThreshold = 64;

/// Weight multiplier for loop-nested operations.  Each level of loop nesting
/// multiplies the effective cost of operations inside that loop.
static constexpr int64_t kLoopDepthMultiplier = 8;

struct EdtTransformsPass : public ::impl::EdtTransformsBase<EdtTransformsPass> {
  EdtTransformsPass(mlir::carts::arts::AnalysisManager *AM) : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
  }

  void runOnOperation() override;

private:
  mlir::carts::arts::AnalysisManager *AM = nullptr;

  /// Estimate task granularity and warn on trivially small tasks.
  /// Returns the number of warned EDTs.
  unsigned estimateTaskGranularity();

  /// Walk all EDTs and remove unused dependency slots.
  /// Returns the number of eliminated dependencies.
  unsigned eliminateDeadDependencies();

  struct ModuleEdtMetrics {
    unsigned totalEdts = 0;
  };

  ModuleEdtMetrics gatherModuleEdtFacts();
  void logModuleEdtSummary(const ModuleEdtMetrics &metrics) const;
};
} // namespace

void EdtTransformsPass::runOnOperation() {
  ARTS_INFO_HEADER(EdtTransformsPass);
  ModuleEdtMetrics metrics = gatherModuleEdtFacts();

  ///===--------------------------------------------------------------------===///
  /// Mixed-root dependency rejection
  ///
  /// An EDT must not mix a coarse and a subpartitioned acquire of the same
  /// root DB without planned-block evidence. ARTS realizes committed grain; it
  /// does not collapse a mixed grain into a coarse one. Fail closed so the
  /// single grain is committed upstream (SDE/CODIR).
  ///===--------------------------------------------------------------------===///
  if (failed(EdtUtils::verifyNoMixedRootDependencies(getOperation()))) {
    signalPassFailure();
    return;
  }

  ///===--------------------------------------------------------------------===///
  /// Task granularity diagnostics.
  ///
  /// Walk all EDTs, compute a cost estimate from EdtInfo metrics, and emit a
  /// diagnostic warning for trivially small tasks (< threshold).
  ///===--------------------------------------------------------------------===///
  unsigned et1Count = estimateTaskGranularity();
  numGranularityAnnotations += et1Count;
  if (et1Count > 0)
    ARTS_INFO("warned on " << et1Count << " trivially small EDTs");

  ///===--------------------------------------------------------------------===///
  /// Dead dependency elimination.
  ///
  /// Walk all EDTs and remove dependency slots whose block arguments are
  /// unused or only consumed by DbReleaseOps. This tightens the task graph
  /// and can unlock further DbScratchElimination.
  ///===--------------------------------------------------------------------===///
  unsigned extEdt2Count = eliminateDeadDependencies();
  numDeadDepsRemoved += extEdt2Count;
  if (extEdt2Count > 0)
    ARTS_INFO("eliminated " << extEdt2Count << " dead dependencies");

  logModuleEdtSummary(metrics);

  ARTS_INFO_FOOTER(EdtTransformsPass);
}

EdtTransformsPass::ModuleEdtMetrics EdtTransformsPass::gatherModuleEdtFacts() {
  ModuleOp module = getOperation();
  ModuleEdtMetrics metrics;

  /// Walk all functions and gather EDT metrics while the graph is already hot.
  module.walk([&](func::FuncOp func) {
    auto &edtGraph = AM->getEdtAnalysis().getOrCreateEdtGraph(func);
    if (edtGraph.size() == 0)
      return;

    ARTS_DEBUG("Processing function: " << func.getName());

    func.walk([&](EdtOp edt) {
      EdtNode *node = edtGraph.getEdtNode(edt);
      if (!node)
        return;

      ++metrics.totalEdts;

      ARTS_DEBUG("  EDT [" << node->getHierId() << "]");
    });
  });

  return metrics;
}

void EdtTransformsPass::logModuleEdtSummary(
    const ModuleEdtMetrics &metrics) const {
  if (metrics.totalEdts > 0) {
    ARTS_INFO("EDT transforms summary: edts=" << metrics.totalEdts);
    return;
  }
  ARTS_INFO("No EDTs found in module");
}

///===----------------------------------------------------------------------===///
/// Task granularity diagnostics.
///===----------------------------------------------------------------------===///
unsigned EdtTransformsPass::estimateTaskGranularity() {
  ModuleOp module = getOperation();
  unsigned count = 0;

  module.walk([&](func::FuncOp func) {
    auto &edtGraph = AM->getEdtAnalysis().getOrCreateEdtGraph(func);
    if (edtGraph.size() == 0)
      return;

    func.walk([&](EdtOp edt) {
      EdtNode *node = edtGraph.getEdtNode(edt);
      if (!node)
        return;

      const EdtInfo &info = node->getInfo();

      /// Cost model: use loop depth as proxy for task weight.
      /// TODO: populate EdtInfo with actual op counts for better estimates.
      int64_t cost = 1;
      if (info.maxLoopDepth > 0) {
        int64_t depthScale = 1;
        for (uint64_t d = 0; d < info.maxLoopDepth; ++d) {
          depthScale *= kLoopDepthMultiplier;
          if (depthScale > 1000000) {
            depthScale = 1000000;
            break;
          }
        }
        cost = depthScale;
      }

      ARTS_DEBUG("EDT [" << node->getHierId() << "]"
                         << " maxLoopDepth=" << info.maxLoopDepth
                         << " => estimatedTaskCost=" << cost);

      /// Warn about trivially small tasks that may benefit from fusion.
      if (cost < kSmallTaskThreshold) {
        edt.emitWarning("trivially small task (estimated cost ")
            << cost << " < " << kSmallTaskThreshold
            << "); consider fusing with a neighbour EDT";
        ++count;
      }
    });
  });

  return count;
}

///===----------------------------------------------------------------------===///
/// Dead dependency elimination.
///===----------------------------------------------------------------------===///
unsigned EdtTransformsPass::eliminateDeadDependencies() {
  ModuleOp module = getOperation();
  unsigned totalEliminated = 0;

  module.walk([&](EdtOp edt) {
    Block &body = edt.getBody().front();
    ValueRange deps = edt.getDependencies();
    unsigned numArgs = body.getNumArguments();

    /// Dependencies and block arguments must be in sync.
    if (deps.size() != numArgs)
      return;

    /// Identify dead block arguments whose reachable use graph reduces to
    /// forwarding and cleanup only.
    SmallVector<unsigned, 4> deadIndices;
    llvm::SetVector<Operation *> cleanupOpsToErase;
    for (unsigned i = 0; i < numArgs; ++i) {
      BlockArgument arg = body.getArgument(i);
      DbAcquireOp acquire = deps[i].getDefiningOp<DbAcquireOp>();

      if (acquire && acquire.getPreserveDepEdge()) {
        ARTS_DEBUG("keeping cleanup-only dep " << i
                                               << " due to preserveDepEdge");
        continue;
      }

      llvm::SetVector<Operation *> cleanupChain;
      bool removable = DbUtils::collectCleanupOnlyUseChain(arg, cleanupChain,
                                                           &edt.getRegion());
      if (!removable && acquire && acquire.getMode() == ArtsMode::out)
        removable = DbUtils::collectTrueOnlyControlTokenUseChain(
            arg, cleanupChain, &edt.getRegion());
      if (!removable)
        continue;

      deadIndices.push_back(i);
      for (Operation *op : cleanupChain)
        cleanupOpsToErase.insert(op);
      ARTS_DEBUG("block arg " << i << " is cleanup-only and can be eliminated");
    }

    if (deadIndices.empty())
      return;

    RemovalUtils removalMgr;
    for (Operation *op : cleanupOpsToErase)
      removalMgr.markForRemoval(op);
    removalMgr.removeAllMarked(module, /*recursive=*/true);

    /// Build the new dependency list, excluding dead indices.
    /// Remove block arguments in reverse order to avoid index invalidation.
    llvm::sort(deadIndices, std::greater<>());
    deadIndices.erase(std::unique(deadIndices.begin(), deadIndices.end()),
                      deadIndices.end());

    DenseSet<unsigned> deadIndexSet(deadIndices.begin(), deadIndices.end());
    SmallVector<Value> newDeps;
    for (unsigned i = 0; i < deps.size(); ++i)
      if (!deadIndexSet.contains(i))
        newDeps.push_back(deps[i]);

    /// Collect the acquiring ops whose results may become dead.
    SmallVector<DbAcquireOp, 4> acquireCandidates;
    for (unsigned idx : deadIndices) {
      Value dep = deps[idx];
      if (auto acquire = dep.getDefiningOp<DbAcquireOp>())
        acquireCandidates.push_back(acquire);
    }

    /// Erase block arguments in reverse index order.
    for (unsigned idx : deadIndices)
      body.eraseArgument(idx);

    edt.setDependencies(newDeps);

    /// Erase now-unused DbAcquireOps (only if both results are dead).
    for (DbAcquireOp acquire : acquireCandidates) {
      if (acquire.getGuid().use_empty() && acquire.getPtr().use_empty())
        acquire.erase();
    }

    totalEliminated += deadIndices.size();
    ARTS_DEBUG("eliminated " << deadIndices.size() << " dead deps from EDT");
  });

  return totalEliminated;
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace carts::arts {
std::unique_ptr<Pass>
createEdtTransformsPass(mlir::carts::arts::AnalysisManager *AM) {
  return std::make_unique<EdtTransformsPass>(AM);
}
} // namespace carts::arts
} // namespace mlir
