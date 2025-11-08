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
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Transforms/DbRewriter.h"
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
  DbPass(ArtsAnalysisManager *AM, bool exportJson, bool processParallel)
      : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
    this->exportJson = exportJson;
    this->processParallel = processParallel;
  }

  void runOnOperation() override;

  /// Optimizations
  bool adjustDbModes();
  bool pinAndSplitAcquires();
  bool promotePinnedDimsToElementDims();
  bool tightenAcquireSlices();
  bool convertToParameters();
  bool refineTaskAcquires();
  bool refineParallelAcquires();

  /// Graph rebuild
  void invalidateAndRebuildGraph();

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

  if (processParallel) {
    /// Phase 2: Focus on parallel EDT worker acquires
    /// After ArtsForLowering has created worker EDTs with chunk-based acquires,
    /// this phase refines the partitioning strategy
    ARTS_INFO(" - Running Phase 2 optimizations (parallel EDT refinement)");
    // changed |= refineParallelAcquires();
    // changed |= adjustDbModes();
  } else {
    /// Phase 1: Focus on non-parallel EDTs (tasks)
    /// This runs before ArtsForLowering, analyzing task dependencies
    ARTS_INFO(" - Running Phase 1 optimizations (non-parallel analysis)");
    // changed |= deadDbElimination();  // DISABLED
    // changed |= adjustDbModes();
    // changed |= pinAndSplitAcquires();  // DISABLED - conflicts with
    // fine-grained indexing changed |= promotePinnedDimsToElementDims();  //
    // DISABLED - conflicts with fine-grained indexing

    /// Metadata-driven chunk-based partitioning
    /// This detects when metadata suggests chunk-based access patterns
    /// NOTE: Currently only detects eligible allocations, transformation
    /// incomplete
    // changed |= refineTaskAcquires();
  }

  if (changed) {
    /// If the module has changed, adjust the db modes again
    changed |= adjustDbModes();
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

      /// Use metadata directly from allocNode (inherited from MemrefMetadata)
      if (allocNode->readWriteRatio) {
        double ratio = *allocNode->readWriteRatio;
        ArtsMode metadataMode = ArtsMode::inout;
        if (ratio > 0.9)
          metadataMode = ArtsMode::in;
        else if (ratio < 0.1)
          metadataMode = ArtsMode::out;
        maxMode = combineAccessModes(maxMode, metadataMode);
        ARTS_DEBUG("  Using metadata read/write ratio: " << ratio
                   << " -> " << metadataMode);
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


///===----------------------------------------------------------------------===///
// Metadata-Driven Optimization Helpers
///===----------------------------------------------------------------------===///

/// Infer the appropriate access mode using imported metadata
/// Uses read/write ratio from sequential analysis to make better decisions
ArtsMode DbPass::inferModeFromMetadata(Operation *allocOp) {
  MemrefMetadata memrefMeta(allocOp);
  memrefMeta.importFromOp();

  // Use read/write ratio if available (from sequential compilation)
  if (auto ratio = memrefMeta.readWriteRatio) {
    ARTS_DEBUG(
        "  Metadata-driven mode inference: read/write ratio = " << *ratio);

    // Read-heavy: use read-only mode for better caching
    if (*ratio > 0.9) {
      ARTS_DEBUG("    -> Selecting 'in' mode (read-heavy pattern)");
      return ArtsMode::in;
    }
    // Write-heavy: use write-only mode
    else if (*ratio < 0.1) {
      ARTS_DEBUG("    -> Selecting 'out' mode (write-heavy pattern)");
      return ArtsMode::out;
    }
    // Mixed access: use read-write mode
    else {
      ARTS_DEBUG("    -> Selecting 'inout' mode (mixed access pattern)");
      return ArtsMode::inout;
    }
  }

  // Fallback: if no metadata, default to inout
  ARTS_DEBUG("  No metadata available, using default 'inout' mode");
  return ArtsMode::inout;
}

/// Suggest partitioning strategy based on access pattern metadata
/// Returns "block", "cyclic", "block-cyclic", or nullopt if no suggestion
/// Suggest chunk size based on memory footprint and cache characteristics
/// Returns suggested chunk size in elements, or nullopt if no suggestion
std::optional<int64_t> DbPass::suggestChunkSize(Operation *loopOp,
                                                Operation *allocOp) {
  MemrefMetadata memrefMeta(allocOp);
  memrefMeta.importFromOp();

  // Use memory footprint from polyhedral analysis (sequential compilation)
  if (auto footprint = memrefMeta.memoryFootprint) {
    if (*footprint <= 0)
      return std::nullopt;

    int64_t footprintBytes = *footprint;

    // Typical L2 cache size: 256KB - 1MB, use 256KB as conservative estimate
    constexpr int64_t L2_CACHE_SIZE = 256 * 1024;

    ARTS_DEBUG("  Metadata-driven chunk sizing: footprint = " << footprintBytes
                                                              << " bytes");

    // If the entire array fits in L2 cache, use larger chunks
    if (footprintBytes < L2_CACHE_SIZE) {
      int64_t chunkSize =
          footprintBytes / 64; // Divide by element size estimate
      ARTS_DEBUG("    -> Array fits in L2, suggest chunk size = " << chunkSize);
      return chunkSize;
    }
    // For larger arrays, chunk to fit in L2 cache
    else {
      int64_t chunkSize = L2_CACHE_SIZE / 64; // Cache-sized chunks
      ARTS_DEBUG(
          "    -> Large array, suggest cache-sized chunks = " << chunkSize);
      return chunkSize;
    }
  }

  return std::nullopt;
}

////===----------------------------------------------------------------------===////
/// Pass creation
////===----------------------------------------------------------------------===////
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbPass(ArtsAnalysisManager *AM, bool exportJson,
                                   bool processParallel) {
  return std::make_unique<DbPass>(AM, exportJson, processParallel);
}
} // namespace arts
} // namespace mlir
