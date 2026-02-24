///==========================================================================///
/// File: Concurrency.cpp
/// Defines concurrency-driven optimizations using ARTS analysis managers.
/// The Concurrency pass make high-level decisions about task (EDT) placement
/// and data (DB) affinity - it advices on where to execute EDTs or
/// nodes—essentially, deciding locality (e.g., which worker thread, node, or
/// memory tier) based on access patterns, dependencies, and the abstract
/// machine model.
///
/// Example:
///   Before:
///     arts.edt <parallel> { arts.for ... }
///
///   After:
///     arts.edt <parallel> { arts.for ... }
///     // with updated workers/concurrency/route attrs from machine + analysis
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/DistributionHeuristics.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "arts/Utils/OperationAttributes.h"
#include <algorithm>

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(concurrency)

using namespace mlir;
using namespace mlir::arts;

///==========================================================================///
/// ConcurrencyPass
///==========================================================================///
struct ConcurrencyPass : public ConcurrencyBase<ConcurrencyPass> {
  explicit ConcurrencyPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }
  void runOnOperation() override;

private:
  void applyEdtParallelismStrategy(EdtOp edtOp);
  void adjustDbModes();
  void updateRuntimeQueryOperations();

  ArtsAbstractMachine *abstractMachine = nullptr;
  ArtsAnalysisManager *AM = nullptr;
  ModuleOp module;
};

void ConcurrencyPass::runOnOperation() {
  module = getOperation();

  /// Get abstractMachine from ArtsAnalysisManager
  abstractMachine = &AM->getAbstractMachine();
  /// Rebuild DB graph from current IR so concurrency decisions consume
  /// up-to-date DB read/write users.
  AM->getDbAnalysis().invalidate();

  ARTS_INFO_HEADER(Concurrency);
  ARTS_DEBUG_REGION(module.dump(););
  ARTS_INFO("Machine valid=" << (abstractMachine->isValid() ? 1 : 0)
                             << ", nodes=" << abstractMachine->getNodeCount()
                             << ", threads=" << abstractMachine->getThreads());

  /// Apply EDT parallelism strategy to all EDT operations
  module.walk([&](EdtOp edtOp) { applyEdtParallelismStrategy(edtOp); });

  /// Adjust datablock modes based on EDT concurrency
  adjustDbModes();

  /// Update runtime query operations based on EDT concurrency
  updateRuntimeQueryOperations();

  ARTS_INFO_FOOTER(Concurrency);
  ARTS_DEBUG_REGION(module.dump(););
}

void ConcurrencyPass::updateRuntimeQueryOperations() {
  ARTS_DEBUG("Updating runtime query operations");

  /// Collect operations to replace - avoid modifying IR during walk
  /// which can cause iterator invalidation and dominance issues.
  SmallVector<GetCurrentWorkerOp, 4> workersToNodes;
  SmallVector<GetTotalWorkersOp, 4> totalWorkersToNodes;
  SmallVector<GetCurrentNodeOp, 4> nodesToWorkers;
  SmallVector<GetTotalNodesOp, 4> totalNodesToWorkers;

  /// Phase 1: Collect operations to replace
  module.walk([&](EdtOp edtOp) {
    bool isInterNode = (edtOp.getConcurrency() == EdtConcurrency::internode);

    edtOp.walk([&](Operation *op) {
      if (auto getWorkerOp = dyn_cast<GetCurrentWorkerOp>(op)) {
        if (isInterNode)
          workersToNodes.push_back(getWorkerOp);
      } else if (auto getTotalWorkersOp = dyn_cast<GetTotalWorkersOp>(op)) {
        if (isInterNode)
          totalWorkersToNodes.push_back(getTotalWorkersOp);
      } else if (auto getNodeOp = dyn_cast<GetCurrentNodeOp>(op)) {
        if (!isInterNode)
          nodesToWorkers.push_back(getNodeOp);
      } else if (auto getTotalNodesOp = dyn_cast<GetTotalNodesOp>(op)) {
        if (!isInterNode)
          totalNodesToWorkers.push_back(getTotalNodesOp);
      }
    });
  });

  /// Phase 2: Replace collected operations (safe - no iteration happening)
  for (auto op : workersToNodes) {
    ARTS_INFO("Replacing arts.get_current_worker with "
              "arts.get_current_node in internode EDT");
    OpBuilder builder(op);
    auto newOp = builder.create<GetCurrentNodeOp>(op.getLoc());
    op.replaceAllUsesWith(newOp.getResult());
    op.erase();
  }

  for (auto op : totalWorkersToNodes) {
    ARTS_INFO("Replacing arts.get_total_workers with "
              "arts.get_total_nodes in internode EDT");
    OpBuilder builder(op);
    auto newOp = builder.create<GetTotalNodesOp>(op.getLoc());
    op.replaceAllUsesWith(newOp.getResult());
    op.erase();
  }

  for (auto op : nodesToWorkers) {
    ARTS_INFO("Replacing arts.get_current_node with "
              "arts.get_current_worker in intranode EDT");
    OpBuilder builder(op);
    auto newOp = builder.create<GetCurrentWorkerOp>(op.getLoc());
    op.replaceAllUsesWith(newOp.getResult());
    op.erase();
  }

  for (auto op : totalNodesToWorkers) {
    ARTS_INFO("Replacing arts.get_total_nodes with "
              "arts.get_total_workers in intranode EDT");
    OpBuilder builder(op);
    auto newOp = builder.create<GetTotalWorkersOp>(op.getLoc());
    op.replaceAllUsesWith(newOp.getResult());
    op.erase();
  }
}

void ConcurrencyPass::adjustDbModes() {
  /// For each EDT that has parallelism strategy set, adjust datablock modes
  module.walk([&](EdtOp edtOp) {
    /// No parallelism strategy set
    if (!edtOp.getWorkers().has_value())
      return;

    /// Set Datablock mode based on access mode
    std::function<void(Operation *)> adjustDbMode = [&](Operation *op) {
      if (auto dbAlloc = dyn_cast<DbAllocOp>(op)) {
        ArtsMode accessMode = dbAlloc.getMode();
        DbMode newDbMode =
            (accessMode == ArtsMode::in) ? DbMode::read : DbMode::write;

        /// Update the dbMode attribute
        OpBuilder builder(dbAlloc.getContext());
        dbAlloc.setDbModeAttr(DbModeAttr::get(builder.getContext(), newDbMode));
        ARTS_INFO("Adjusted datablock mode to "
                  << (newDbMode == DbMode::read ? "read" : "write"));
      }

      /// Recursively check operands
      for (Value operand : op->getOperands()) {
        if (Operation *defOp = operand.getDefiningOp())
          adjustDbMode(defOp);
      }
    };

    /// Start traversal from EDT dependencies
    for (Value dep : edtOp.getDependencies()) {
      if (Operation *defOp = dep.getDefiningOp())
        adjustDbMode(defOp);
    }
  });
}

/// Check if an EDT contains nested task EDTs.
/// Parallel EDTs with nested tasks should remain intranode to ensure correct
/// work distribution - task parallelism is designed for local execution.
static bool containsNestedTaskEdts(EdtOp edtOp) {
  bool hasNestedTasks = false;
  edtOp.walk([&](EdtOp nestedEdt) {
    /// Skip the EDT itself
    if (nestedEdt == edtOp)
      return WalkResult::advance();
    if (nestedEdt.getType() == EdtType::task) {
      hasNestedTasks = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return hasNestedTasks;
}

void ConcurrencyPass::applyEdtParallelismStrategy(EdtOp edtOp) {
  /// Only apply to parallel EDTs that don't already have workers attribute set
  if (edtOp.getType() != EdtType::parallel)
    return;

  /// Already has parallelism strategy set
  if (edtOp.getWorkers().has_value())
    return;

  /// If no config file is present, avoid forcing a compile-time worker count.
  /// Let the runtime query decide (arts.get_total_workers).
  if (!abstractMachine->hasConfigFile()) {
    ARTS_WARN("No arts.cfg; using runtime worker count for parallel EDT");
    edtOp.setConcurrency(EdtConcurrency::intranode);
    return;
  }

  /// Check for nested task EDTs - these must remain intranode.
  /// Task parallelism is designed for local execution within a single node.
  /// Distributing a parallel EDT containing tasks across nodes would cause
  /// incorrect work distribution (each node would only process its portion
  /// while tasks expect to see the full iteration space).
  bool hasNestedTasks = containsNestedTaskEdts(edtOp);
  ParallelismDecision machineDecision =
      DistributionHeuristics::resolveParallelismFromMachine(abstractMachine);
  if (hasNestedTasks) {
    int64_t workers = std::max<int64_t>(1, machineDecision.workersPerNode);
    ARTS_INFO("Parallel EDT contains nested tasks - keeping intranode with "
              << workers << " workers");
    edtOp.setConcurrency(EdtConcurrency::intranode);
    arts::setWorkers(edtOp.getOperation(), workers);
    arts::setWorkersPerNode(edtOp.getOperation(), 0);
    return;
  }

  if (AM->getDbAnalysis().hasNonInternodeConsumerForWrittenDb(edtOp)) {
    int64_t workers = std::max<int64_t>(1, machineDecision.workersPerNode);
    ARTS_INFO("Parallel EDT writes DB consumed outside internode EDT flow - "
              "keeping intranode with "
              << workers << " workers");
    edtOp.setConcurrency(EdtConcurrency::intranode);
    arts::setWorkers(edtOp.getOperation(), workers);
    arts::setWorkersPerNode(edtOp.getOperation(), 0);
    return;
  }

  /// Determine parallelism strategy based on abstract machine
  int numWorkers = 0;
  EdtConcurrency concurrencyType = EdtConcurrency::intranode;

  if (!abstractMachine->isValid()) {
    ARTS_WARN(
        "Abstract machine validation failed; using available configuration "
        "data when setting parallelism");
  }

  int nodeCount = abstractMachine->hasValidNodeCount()
                      ? abstractMachine->getNodeCount()
                      : 0;
  int workerThreads = static_cast<int>(machineDecision.workersPerNode);
  numWorkers = static_cast<int>(machineDecision.totalWorkers);
  concurrencyType = machineDecision.concurrency;

  if (concurrencyType == EdtConcurrency::internode) {
    ARTS_INFO("Setting EDT parallelism: inter-node across "
              << nodeCount << " nodes x " << workerThreads << " workers");
  } else if (numWorkers > 1) {
    ARTS_INFO("Setting EDT parallelism: intra-node across " << numWorkers
                                                            << " workers");
  } else if (numWorkers == 1 && nodeCount == 1) {
    ARTS_INFO("Setting EDT parallelism: single-node execution with 1 worker");
  } else {
    /// Unknown configuration - default to intranode with runtime-determined
    /// workers The EDT will query worker count at runtime via GetTotalWorkersOp
    numWorkers = 0;
    concurrencyType = EdtConcurrency::intranode;
    ARTS_INFO("Unknown configuration; defaulting to intranode with runtime "
              "worker count");
  }

  /// Set the attributes on the EDT operation
  edtOp.setConcurrency(concurrencyType);
  arts::setWorkers(edtOp.getOperation(), numWorkers);

  if (concurrencyType == EdtConcurrency::internode && workerThreads > 0) {
    arts::setWorkersPerNode(edtOp.getOperation(),
                            static_cast<int64_t>(workerThreads));
  } else {
    arts::setWorkersPerNode(edtOp.getOperation(), 0);
  }
  ARTS_INFO("Set EDT concurrency="
            << (concurrencyType == EdtConcurrency::internode ? "internode"
                                                             : "intranode")
            << ", workers=" << numWorkers);
}

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createConcurrencyPass(ArtsAnalysisManager *AM) {
  return std::make_unique<ConcurrencyPass>(AM);
}

} // namespace arts
} // namespace mlir
