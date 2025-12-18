///==========================================================================///
/// File: CollectMetadata.cpp
///
/// This pass collects comprehensive metadata from sequential code before
/// OpenMP transformation. Uses ArtsMetadataManager with specialized analyzer
/// classes for clean separation of concerns.
///
/// Architecture:
/// - LoopAnalyzer: Analyzes loop operations (defined in Analysis/Metadata/)
/// - MemrefAnalyzer: Analyzes memref allocations (defined in
/// Analysis/Metadata/)
/// - ReuseAnalyzer: Stack distance algorithm for reuse distance
/// - AccessAnalyzer: Analyzes individual access operations (inline helper)
/// - DependenceAnalyzer: Analyzes dependencies and parallelism (inline helper)
/// - CollectMetadataPass: Orchestrates using ArtsMetadataManager
///
/// Assumptions:
/// - Memrefs have clear multi-dimensional types (no flattened arrays)
/// - Sequential code before OpenMP pragmas
///
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Analysis/Metadata/AccessAnalyzer.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/Analysis/Metadata/DependenceAnalyzer.h"
#include "arts/Analysis/Metadata/LoopAnalyzer.h"
#include "arts/Analysis/Metadata/MemrefAnalyzer.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/LocationMetadata.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"

#include "mlir/Dialect/Affine/Analysis/AffineAnalysis.h"
#include "mlir/Dialect/Affine/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Affine/Analysis/Utils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Dominance.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include <limits>
#include <memory>
#include <utility>

/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(collect_metadata);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
// CollectMetadataPass
///===----------------------------------------------------------------------===///

struct CollectMetadataPass : public CollectMetadataBase<CollectMetadataPass> {
  CollectMetadataPass() = default;
  CollectMetadataPass(bool exportMetadata, llvm::StringRef metadataFile)
      : CollectMetadataBase() {
    this->exportMetadata = exportMetadata;
    this->metadataFile = metadataFile.str();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *context = &getContext();

    ARTS_INFO_HEADER(CollectMetadataPass);

    ///  Initialize metadata manager and analyzers
    auto manager = std::make_unique<ArtsMetadataManager>(context);

    /// Initialize deterministic sequential ID assignment
    manager->getIdRegistry().reset();
    manager->getIdRegistry().initializeFromModule(module);
    auto accessAnalyzer = std::make_unique<AccessAnalyzer>(context);
    auto depAnalyzer =
        std::make_unique<DependenceAnalyzer>(context, *accessAnalyzer);
    auto reuseAnalyzer = std::make_unique<ReuseAnalyzer>(*accessAnalyzer);
    auto memrefAnalyzer = std::make_unique<MemrefAnalyzer>(
        *accessAnalyzer, *reuseAnalyzer, *manager, *depAnalyzer);
    auto loopAnalyzer =
        std::make_unique<LoopAnalyzer>(context, *accessAnalyzer, *depAnalyzer);

    ///  Collect loop metadata
    ARTS_DEBUG("Collecting loop metadata...");
    collectLoopMetadata(module, *manager, *loopAnalyzer);

    ///  Collect memref metadata
    ARTS_DEBUG("Collecting memref metadata...");
    collectMemrefMetadata(module, *manager, *memrefAnalyzer);

    ///  Refine loop metadata using memref insights
    refineLoopMemrefMetadata(module, *manager, *accessAnalyzer, *depAnalyzer);

    ///  Export all metadata to operations
    ARTS_DEBUG("Exporting metadata to operations...");
    manager->exportToOperations();

    ///  Export to JSON if requested
    if (exportMetadata && !metadataFile.empty()) {
      ARTS_DEBUG("Exporting metadata to JSON file: " << metadataFile);
      if (!manager->exportToJsonFile(metadataFile))
        ARTS_ERROR("Failed to export metadata to JSON");
    }

    ///  Print statistics
    ARTS_DEBUG("CollectMetadata: Collected metadata for " << manager->size()
                                                          << " operations");
    manager->printStatistics(ARTS_DBGS());
  }

private:
  /// / Collect metadata for all loops in the module
  void collectLoopMetadata(ModuleOp module, ArtsMetadataManager &manager,
                           LoopAnalyzer &analyzer) {
    ///  Collect affine.for loops
    module.walk([&](affine::AffineForOp forOp) {
      auto *metadata = manager.addLoopMetadata(forOp);
      analyzer.analyzeAffineLoop(forOp, metadata);
      /// Analyze loop reordering opportunities for perfect nests
      analyzer.analyzeLoopReordering(forOp, metadata, manager);
      /// Analyze per-dimension dependencies for stencil optimization
      /// This enables outer loop parallelization even when inner loops have
      /// deps
      analyzer.analyzeLoopNestDependences(forOp, metadata);
      ARTS_DEBUG("  Analyzed affine.for at "
                 << metadata->locationMetadata.getKey());
    });

    ///  Collect scf.for loops
    module.walk([&](scf::ForOp forOp) {
      auto *metadata = manager.addLoopMetadata(forOp);
      analyzer.analyzeSCFLoop(forOp, metadata);
      ARTS_DEBUG("  Analyzed scf.for at "
                 << metadata->locationMetadata.getKey());
    });

    ///  Collect scf.parallel loops
    module.walk([&](scf::ParallelOp parOp) {
      auto *metadata = manager.addLoopMetadata(parOp);
      analyzer.analyzeSCFLoop(parOp, metadata);
      ARTS_DEBUG("  Analyzed scf.parallel at "
                 << metadata->locationMetadata.getKey());
    });

    ///  Collect scf.while loops
    module.walk([&](scf::WhileOp whileOp) {
      auto *metadata = manager.addLoopMetadata(whileOp);
      analyzer.analyzeSCFLoop(whileOp, metadata);
      ARTS_DEBUG("  Analyzed scf.while at "
                 << metadata->locationMetadata.getKey());
    });
  }

  /// / Collect metadata for all memref allocations
  void collectMemrefMetadata(ModuleOp module, ArtsMetadataManager &manager,
                             MemrefAnalyzer &analyzer) {
    ///  Collect memref.alloc
    module.walk([&](memref::AllocOp allocOp) {
      auto *metadata = manager.addMemrefMetadata(allocOp);
      analyzer.analyzeAllocation(allocOp, metadata, module);
      ARTS_DEBUG("  Analyzed memref.alloc " << metadata->allocationId);
    });

    ///  Collect memref.alloca
    module.walk([&](memref::AllocaOp allocaOp) {
      auto *metadata = manager.addMemrefMetadata(allocaOp);
      analyzer.analyzeAllocation(allocaOp, metadata, module);
      ARTS_DEBUG("  Analyzed memref.alloca " << metadata->allocationId);
    });
  }

  struct LoopMemrefUsage {
    uint64_t readCount = 0, writeCount = 0;
    MemrefMetadata *memrefMetadata = nullptr;
    bool hasLoopCarriedDeps = false;
  };

  void refineLoopMemrefMetadata(ModuleOp module, ArtsMetadataManager &manager,
                                AccessAnalyzer &accessAnalyzer,
                                DependenceAnalyzer &depAnalyzer) {
    module.walk([&](Operation *loopOp) {
      auto *loopMeta = manager.getLoopMetadata(loopOp);
      if (!loopMeta)
        return;

      llvm::DenseMap<Value, LoopMemrefUsage> usage;
      loopOp->walk([&](Operation *op) {
        if (!accessAnalyzer.isMemoryAccess(op))
          return;
        Value memref = accessAnalyzer.getAccessedMemref(op);
        if (!memref)
          return;

        auto [it, inserted] = usage.try_emplace(memref);
        auto &info = it->second;
        if (inserted) {
          if (Operation *def = memref.getDefiningOp())
            info.memrefMetadata = manager.getMemrefMetadata(def);
          info.hasLoopCarriedDeps =
              depAnalyzer.hasLoopCarriedDeps(loopOp, memref);
        }
        if (accessAnalyzer.isReadAccess(op))
          info.readCount++;
        if (accessAnalyzer.isWriteAccess(op))
          info.writeCount++;
      });

      if (usage.empty())
        return;

      uint64_t readOnly = 0, writeOnly = 0, readWrite = 0;
      uint64_t memrefsWithDeps = 0;
      for (auto &entry : usage) {
        LoopMemrefUsage &info = entry.second;
        bool hasReads = info.readCount > 0;
        bool hasWrites = info.writeCount > 0;
        if (hasReads && !hasWrites)
          readOnly++;
        else if (!hasReads && hasWrites)
          writeOnly++;
        else if (hasReads && hasWrites)
          readWrite++;

        if (info.hasLoopCarriedDeps)
          memrefsWithDeps++;
      }

      loopMeta->memrefsWithLoopCarriedDeps = memrefsWithDeps;

      bool hasLoopLevelDeps =
          loopMeta->hasInterIterationDeps && *loopMeta->hasInterIterationDeps;

      // Use per-dimension analysis if available: outer loop may be parallel
      // even if inner loops have dependencies (e.g., Seidel-2D pattern)
      bool outerLoopParallelizable = false;
      if (loopMeta->outermostParallelDim.has_value() &&
          *loopMeta->outermostParallelDim == 0) {
        // Dimension 0 (this loop) doesn't carry dependencies
        outerLoopParallelizable = true;
        ARTS_DEBUG("  Per-dimension analysis: outer loop (dim 0) is parallel");
      }

      // A loop is sequential only if:
      // 1. It has loop-level deps AND per-dimension analysis didn't prove outer
      // is safe
      // 2. OR it has memrefs with loop-carried deps AND per-dimension didn't
      // prove safe
      bool sequential =
          (hasLoopLevelDeps || memrefsWithDeps > 0) && !outerLoopParallelizable;

      bool hasWrites = (writeOnly + readWrite) > 0 ||
                       loopMeta->accessStats.writeCount.value_or(0) > 0;
      LoopMetadata::ParallelClassification classification =
          LoopMetadata::ParallelClassification::Unknown;
      if (sequential) {
        classification = LoopMetadata::ParallelClassification::Sequential;
        loopMeta->potentiallyParallel = false;
      } else if (outerLoopParallelizable) {
        // Per-dimension analysis proved outer loop is safe to parallelize
        classification = LoopMetadata::ParallelClassification::Likely;
        loopMeta->potentiallyParallel = true;
        ARTS_DEBUG(
            "  Enabling parallel for outer loop via per-dimension analysis");
      } else if (!hasWrites) {
        classification = LoopMetadata::ParallelClassification::ReadOnly;
        loopMeta->potentiallyParallel = true;
      } else {
        classification = LoopMetadata::ParallelClassification::Likely;
        if (!loopMeta->potentiallyParallel)
          loopMeta->potentiallyParallel = true;
      }
      loopMeta->parallelClassification = classification;
    });
  }
};

///===----------------------------------------------------------------------===///
// Pass Registration
///===----------------------------------------------------------------------===///

std::unique_ptr<Pass> mlir::arts::createCollectMetadataPass() {
  return std::make_unique<CollectMetadataPass>();
}

std::unique_ptr<Pass>
mlir::arts::createCollectMetadataPass(bool exportMetadata,
                                      llvm::StringRef metadataFile) {
  return std::make_unique<CollectMetadataPass>(exportMetadata, metadataFile);
}
