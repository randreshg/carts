///==========================================================================///
/// File: Db.cpp
/// Pass for DB mode adjustments based on access patterns.
/// Partitioning logic has been moved to DbPartitioning.cpp.
///==========================================================================///

/// Dialects
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
/// Arts
#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Other
#include "mlir/Pass/Pass.h"
/// Debug
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"

ARTS_DEBUG_SETUP(db);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
// Pass Implementation
// Adjust Datablock modes based on access patterns
///===----------------------------------------------------------------------===///
namespace {
struct DbPass : public arts::DbBase<DbPass> {
  DbPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override;

  /// Mode adjustment
  bool adjustDbModes();

  /// Graph rebuild
  void invalidateAndRebuildGraph();

private:
  ModuleOp module;
  ArtsAnalysisManager *AM = nullptr;
};
} // namespace

void DbPass::runOnOperation() {
  module = getOperation();

  ARTS_INFO_HEADER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););

  assert(AM && "ArtsAnalysisManager must be provided externally");

  /// Graph construction and analysis
  invalidateAndRebuildGraph();

  /// Adjust DB modes based on access patterns
  bool changed = adjustDbModes();

  if (changed) {
    ARTS_INFO(" Module has changed, invalidating analyses");
    AM->invalidate();
  }

  ARTS_INFO_FOOTER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Adjust DB modes based on accesses.
/// Based on the collected loads and stores, adjust the DB mode to in, out, or
/// inout.
///===----------------------------------------------------------------------===///
bool DbPass::adjustDbModes() {
  ARTS_DEBUG_HEADER(AdjustDBModes);
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    /// First, adjust per-acquire modes
    graph.forEachAcquireNode([&](DbAcquireNode *acqNode) {
      bool hasLoads = acqNode->hasLoads();
      bool hasStores = acqNode->hasStores();

      DbAcquireOp acqOp = acqNode->getDbAcquireOp();
      ArtsMode newMode = ArtsMode::in;
      if (hasLoads && hasStores)
        newMode = ArtsMode::inout;
      else if (hasStores)
        newMode = ArtsMode::out;
      else
        newMode = ArtsMode::in;

      /// Each DbAcquireNode's mode is derived from its own accesses only.
      /// Nested acquires will be processed separately by forEachAcquireNode.
      if (newMode == acqOp.getMode())
        return;

      ARTS_DEBUG("AcquireOp: " << acqOp << " from " << acqOp.getMode() << " to "
                               << newMode);
      acqOp.setModeAttr(ArtsModeAttr::get(acqOp.getContext(), newMode));
      changed = true;
    });

    /// Then, adjust alloc dbMode - collect modes from all acquires in hierarchy
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      ArtsMode maxMode = ArtsMode::in;

      /// Recursive helper to collect modes from all acquire levels
      std::function<void(DbAcquireNode *)> collectModes =
          [&](DbAcquireNode *acqNode) {
            ArtsMode mode = acqNode->getDbAcquireOp().getMode();
            maxMode = combineAccessModes(maxMode, mode);
            acqNode->forEachChildNode([&](NodeBase *child) {
              if (auto *nestedAcq = dyn_cast<DbAcquireNode>(child))
                collectModes(nestedAcq);
            });
          };

      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto *acqNode = dyn_cast<DbAcquireNode>(child))
          collectModes(acqNode);
      });

      /// Update the alloc mode
      DbAllocOp allocOp = allocNode->getDbAllocOp();
      ArtsMode currentDbMode = allocOp.getMode();
      if (currentDbMode == maxMode)
        return;
      ARTS_DEBUG("AllocOp: " << allocOp << " from " << currentDbMode << " to "
                             << maxMode);
      allocOp.setModeAttr(ArtsModeAttr::get(allocOp.getContext(), maxMode));

      changed = true;
    });
  });

  ARTS_DEBUG_FOOTER(AdjustDBModes);
  return changed;
}

///===----------------------------------------------------------------------===///
/// Invalidate and rebuild the graph
///===----------------------------------------------------------------------===///
void DbPass::invalidateAndRebuildGraph() {
  module.walk([&](func::FuncOp func) {
    AM->invalidateFunction(func);
    (void)AM->getDbGraph(func);
  });
}

////===----------------------------------------------------------------------===////
/// Pass creation
////===----------------------------------------------------------------------===////
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbPass(ArtsAnalysisManager *AM) {
  return std::make_unique<DbPass>(AM);
}
} // namespace arts
} // namespace mlir
