///==========================================================================///
/// File: Db.cpp
/// Pass for DB optimizations and memory management.
///==========================================================================///

/// Dialects

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Visitors.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include <algorithm>
#include <cstdint>
#include <iterator>
/// File operations
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"
ARTS_DEBUG_SETUP(db);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
// Pass Implementation
// transform/optimize ARTS DB IR using DbGraph/analyses.
///===----------------------------------------------------------------------===///
namespace {
struct DbPass : public arts::DbBase<DbPass> {
  DbPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override;

  /// Optimizations
  bool adjustDbModes();
  bool refineParallelAcquires();

  /// Graph rebuild
  void invalidateAndRebuildGraph();
  bool isEligibleForSlicing(DbAcquireNode *node);
  bool edtHasParallelLoopMetadata(EdtOp edt);
  bool isMemrefParallelFriendly(DbAllocNode *allocNode);

private:
  ModuleOp module;
  ArtsAnalysisManager *AM = nullptr;

  /// Helper functions
  ArtsMode inferModeFromMetadata(Operation *allocOp);
  std::optional<int64_t> suggestChunkSize(Operation *loopOp,
                                          Operation *allocOp);
};
} // namespace

void DbPass::runOnOperation() {
  bool changed = false;
  module = getOperation();

  ARTS_INFO_HEADER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););

  assert(AM && "ArtsAnalysisManager must be provided externally");

  /// Graph construction and analysis
  invalidateAndRebuildGraph();

  changed |= refineParallelAcquires();

  /// Phase 2: Focus on parallel EDT worker acquires
  /// After ArtsForLowering has created worker EDTs with chunk-based acquires,
  /// this phase refines the partitioning strategy
  ARTS_INFO(" - Running Phase 2 optimizations (parallel EDT refinement)");
  // changed |= refineParallelAcquires();
  // changed |= adjustDbModes();

  if (changed) {
    /// If the module has changed, adjust the db modes again
    // changed |= adjustDbModes();
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
      bool hasLoads = !acqNode->getLoads().empty();
      bool hasStores = !acqNode->getStores().empty();

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

      ARTS_DEBUG("AcquireOp: " << acqOp);
      ARTS_DEBUG(" from " << acqOp.getMode() << " to " << newMode);
      acqOp.setModeAttr(ArtsModeAttr::get(acqOp.getContext(), newMode));
      changed = true;
    });

    /// Then, adjust alloc dbMode - collect modes from all acquires in hierarchy
    /// (direct children and nested acquires) and compute maximum required mode
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      ArtsMode maxMode = ArtsMode::in;

      /// Recursive helper to collect modes from all acquire levels
      std::function<void(DbAcquireNode *)> collectModes =
          [&](DbAcquireNode *acqNode) {
            ArtsMode mode = acqNode->getDbAcquireOp().getMode();
            maxMode = combineAccessModes(maxMode, mode);

            /// Recursively process child acquires
            acqNode->forEachChildNode([&](NodeBase *child) {
              if (auto *nestedAcq = dyn_cast<DbAcquireNode>(child))
                collectModes(nestedAcq);
            });
          };

      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto *acqNode = dyn_cast<DbAcquireNode>(child)) {
          collectModes(acqNode);
        }
      });

      DbAllocOp allocOp = allocNode->getDbAllocOp();

      /// Use metadata from allocNode's info (MemrefMetadata)
      if (allocNode->readWriteRatio) {
        double ratio = *allocNode->readWriteRatio;
        ArtsMode metadataMode = ArtsMode::inout;
        if (ratio > 0.9)
          metadataMode = ArtsMode::in;
        else if (ratio < 0.1)
          metadataMode = ArtsMode::out;
        maxMode = combineAccessModes(maxMode, metadataMode);
        ARTS_DEBUG("  Using metadata read/write ratio: " << ratio << " -> "
                                                         << metadataMode);
      }

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

bool DbPass::refineParallelAcquires() {
  /// Metadata-driven slicing will be reintroduced once the sequential
  /// analysis produces the required access summaries. For now, keep the
  /// legacy hook but do not attempt transformations.
  return false;
}

bool DbPass::isEligibleForSlicing(DbAcquireNode *node) {
  DbAcquireOp acq = node->getDbAcquireOp();
  auto skip = [&](StringRef reason) {
    ARTS_DEBUG("Skipping acquire " << acq << ": " << reason);
    return false;
  };
  if (!acq)
    return false;

  EdtOp edt = node->getEdtUser();
  if (!edt || edt.getType() != EdtType::task)
    return skip("not a task EDT");

  EdtOp parent = edt->getParentOfType<EdtOp>();
  if (!parent || parent.getType() != EdtType::parallel)
    return skip("not nested under parallel EDT");

  if (node->getLoads().empty() && node->getStores().empty())
    return skip("no memory accesses");

  DbAllocNode *allocNode = node->getRootAlloc();
  if (!allocNode)
    return skip("missing allocation node");

  if (allocNode->hasUniformAccess && !*allocNode->hasUniformAccess)
    return skip("non-uniform access");

  if (!isMemrefParallelFriendly(allocNode))
    return skip("memref metadata is not marked as parallel-friendly");

  if (!edtHasParallelLoopMetadata(edt))
    return skip("worker EDT does not contain parallel loop metadata");

  return true;
}

bool DbPass::isMemrefParallelFriendly(DbAllocNode *allocNode) {
  if (!allocNode)
    return false;

  if (!allocNode->accessedInParallelLoop ||
      !allocNode->accessedInParallelLoop.value_or(false))
    return false;

  return true;
}

bool DbPass::edtHasParallelLoopMetadata(EdtOp edt) {
  if (!edt)
    return false;

  bool foundParallelLoop = false;
  auto &dbAnalysis = AM->getDbAnalysis();
  auto *loopAnalysis = dbAnalysis.getLoopAnalysis();
  if (!loopAnalysis)
    return false;

  edt.walk([&](Operation *op) {
    if (auto forOp = dyn_cast<scf::ForOp>(op)) {
      if (LoopNode *loopNode = loopAnalysis->getOrCreateLoopNode(forOp)) {
        bool noDeps = !loopNode->hasInterIterationDeps.has_value() ||
                      !loopNode->hasInterIterationDeps.value();
        if (loopNode->potentiallyParallel && noDeps) {
          foundParallelLoop = true;
          return WalkResult::interrupt();
        }
      }
    }
    return WalkResult::advance();
  });

  return foundParallelLoop;
}


///===----------------------------------------------------------------------===///
/// Invalidate and rebuild the graph
///===----------------------------------------------------------------------===///
void DbPass::invalidateAndRebuildGraph() {
  module.walk([&](func::FuncOp func) {
    AM->invalidateFunction(func);
    DbGraph &graph = AM->getDbGraph(func);
    graph.build();

    /// Print analysis results for verification
    ARTS_DEBUG_REGION({
      graph.print(llvm::outs());
      llvm::outs().flush();
    });
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
