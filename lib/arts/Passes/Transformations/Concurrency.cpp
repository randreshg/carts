///==========================================================================///
/// File: Concurrency.cpp
/// Defines concurrency-driven optimizations using ARTS analysis managers.
/// The Concurrency pass make high-level decisions about task (EDT) placement
/// and data (DB) affinity - it advices on where to execute EDTs or
/// nodes—essentially, deciding locality (e.g., which worker thread, node, or
/// memory tier) based on access patterns, dependencies, and the abstract
/// machine model.
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/OpDefinition.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/raw_ostream.h"

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

  /// Apply EDT and DB placement decisions to all functions
  // module.walk([&](func::FuncOp func) {
  //   applyPlacementDecisions(func);
  // });

  ARTS_INFO_FOOTER(Concurrency);
  ARTS_DEBUG_REGION(module.dump(););
}

void ConcurrencyPass::updateRuntimeQueryOperations() {
  ARTS_DEBUG("Updating runtime query operations");
  /// For each EDT that has parallelism strategy set, update runtime query
  /// operations
  module.walk([&](EdtOp edtOp) {
    bool isInterNode = (edtOp.getConcurrency() == EdtConcurrency::internode);

    OpBuilder builder(edtOp.getContext());

    /// Walk through operations in the EDT body and update runtime queries
    edtOp.walk([&](Operation *op) {
      if (auto getWorkerOp = dyn_cast<GetCurrentWorkerOp>(op)) {
        if (isInterNode) {
          /// For internode parallelism, replace get_current_worker with
          /// get_current_node
          ARTS_INFO("Replacing arts.get_current_worker with "
                    "arts.get_current_node in internode EDT");
          builder.setInsertionPoint(getWorkerOp);
          auto getNodeOp =
              builder.create<GetCurrentNodeOp>(getWorkerOp.getLoc());
          getWorkerOp.replaceAllUsesWith(getNodeOp.getResult());
          getWorkerOp.erase();
        }
      } else if (auto getTotalWorkersOp = dyn_cast<GetTotalWorkersOp>(op)) {
        if (isInterNode) {
          /// For internode parallelism, replace get_total_workers with
          /// get_total_nodes
          ARTS_INFO("Replacing arts.get_total_workers with "
                    "arts.get_total_nodes in internode EDT");
          builder.setInsertionPoint(getTotalWorkersOp);
          auto getTotalNodesOp =
              builder.create<GetTotalNodesOp>(getTotalWorkersOp.getLoc());
          getTotalWorkersOp.replaceAllUsesWith(getTotalNodesOp.getResult());
          getTotalWorkersOp.erase();
        }
      } else if (auto getNodeOp = dyn_cast<GetCurrentNodeOp>(op)) {
        if (!isInterNode) {
          /// For intranode parallelism, replace get_current_node with
          /// get_current_worker
          ARTS_INFO("Replacing arts.get_current_node with "
                    "arts.get_current_worker in intranode EDT");
          builder.setInsertionPoint(getNodeOp);
          auto getWorkerOp =
              builder.create<GetCurrentWorkerOp>(getNodeOp.getLoc());
          getNodeOp.replaceAllUsesWith(getWorkerOp.getResult());
          getNodeOp.erase();
        }
      } else if (auto getTotalNodesOp = dyn_cast<GetTotalNodesOp>(op)) {
        if (!isInterNode) {
          /// For intranode parallelism, replace get_total_nodes with
          /// get_total_workers
          ARTS_INFO("Replacing arts.get_total_nodes with "
                    "arts.get_total_workers in intranode EDT");
          builder.setInsertionPoint(getTotalNodesOp);
          auto getTotalWorkersOp =
              builder.create<GetTotalWorkersOp>(getTotalNodesOp.getLoc());
          getTotalNodesOp.replaceAllUsesWith(getTotalWorkersOp.getResult());
          getTotalNodesOp.erase();
        }
      }
    });
  });
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

void ConcurrencyPass::applyEdtParallelismStrategy(EdtOp edtOp) {
  /// Only apply to parallel EDTs that don't already have workers attribute set
  if (edtOp.getType() != EdtType::parallel)
    return;

  /// Already has parallelism strategy set
  if (edtOp.getWorkers().has_value())
    return;

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
  int threads =
      abstractMachine->hasValidThreads() ? abstractMachine->getThreads() : 0;

  if (nodeCount > 1) {
    numWorkers = nodeCount;
    concurrencyType = EdtConcurrency::internode;
    ARTS_INFO("Setting EDT parallelism: inter-node across " << nodeCount
                                                            << " nodes");
  } else if (threads > 0) {
    numWorkers = threads;
    concurrencyType = EdtConcurrency::intranode;
    ARTS_INFO("Setting EDT parallelism: intra-node across " << threads
                                                            << " threads");
  } else if (nodeCount == 1) {
    numWorkers = 1;
    concurrencyType = EdtConcurrency::intranode;
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
  OpBuilder builder(edtOp.getContext());
  edtOp.setConcurrency(concurrencyType);
  if (numWorkers > 0)
    edtOp.setWorkersAttr(workersAttr::get(builder.getContext(), numWorkers));
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
