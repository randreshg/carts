///==========================================================================
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
///==========================================================================

#include "ArtsPassDetails.h"
#include "arts/Analysis/Metadata/AccessAnalyzer.h"
#include "arts/Analysis/Metadata/DependenceAnalyzer.h"
#include "arts/Analysis/Metadata/LoopAnalyzer.h"
#include "arts/Analysis/Metadata/MemrefAnalyzer.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/ArtsMetadataManager.h"
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
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include <limits>
#include <memory>

/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(collect_metadata);

using namespace mlir;
using namespace mlir::arts;

// AccessAnalyzer is now defined in arts/Analysis/Metadata/AccessAnalyzer.h
// DependenceAnalyzer is now defined in
// arts/Analysis/Metadata/DependenceAnalyzer.h MemrefAnalyzer is now defined in
// arts/Analysis/Metadata/MemrefAnalyzer.h LoopAnalyzer is now defined in
// arts/Analysis/Metadata/LoopAnalyzer.h

//===----------------------------------------------------------------------===//
// CollectMetadataPass
//===----------------------------------------------------------------------===//

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

    // Initialize metadata manager and analyzers
    auto manager = std::make_unique<ArtsMetadataManager>(context);
    auto accessAnalyzer = std::make_unique<AccessAnalyzer>(context);
    auto depAnalyzer =
        std::make_unique<DependenceAnalyzer>(context, *accessAnalyzer);
    auto reuseAnalyzer = std::make_unique<ReuseAnalyzer>(*accessAnalyzer);
    auto memrefAnalyzer = std::make_unique<MemrefAnalyzer>(
        context, *accessAnalyzer, *reuseAnalyzer);
    auto loopAnalyzer =
        std::make_unique<LoopAnalyzer>(context, *accessAnalyzer, *depAnalyzer);

    // Collect loop metadata
    ARTS_DEBUG("Collecting loop metadata...");
    collectLoopMetadata(module, *manager, *loopAnalyzer);

    // Collect memref metadata
    ARTS_DEBUG("Collecting memref metadata...");
    collectMemrefMetadata(module, *manager, *memrefAnalyzer);

    // Export all metadata to operations
    ARTS_DEBUG("Exporting metadata to operations...");
    manager->exportToOperations();

    // Export to JSON if requested
    if (exportMetadata && !metadataFile.empty()) {
      ARTS_DEBUG("Exporting metadata to JSON file: " << metadataFile);
      if (!manager->exportToJsonFile(metadataFile))
        ARTS_ERROR("Failed to export metadata to JSON");
    }

    // Print statistics
    ARTS_DEBUG("CollectMetadata: Collected metadata for " << manager->size()
                                                          << " operations");
    manager->printStatistics(llvm::errs());
  }

private:
  /// Collect metadata for all loops in the module
  void collectLoopMetadata(ModuleOp module, ArtsMetadataManager &manager,
                           LoopAnalyzer &analyzer) {
    // Collect affine.for loops
    module.walk([&](affine::AffineForOp forOp) {
      auto *metadata = manager.addLoopMetadata(forOp);
      analyzer.analyzeAffineLoop(forOp, metadata);
      ARTS_DEBUG("  Analyzed affine.for at "
                 << metadata->locationMetadata.toString());
    });

    // Collect scf.for loops
    module.walk([&](scf::ForOp forOp) {
      auto *metadata = manager.addLoopMetadata(forOp);
      analyzer.analyzeSCFLoop(forOp, metadata);
      ARTS_DEBUG("  Analyzed scf.for at "
                 << metadata->locationMetadata.toString());
    });

    // Collect scf.parallel loops
    module.walk([&](scf::ParallelOp parOp) {
      auto *metadata = manager.addLoopMetadata(parOp);
      analyzer.analyzeSCFLoop(parOp, metadata);
      ARTS_DEBUG("  Analyzed scf.parallel at "
                 << metadata->locationMetadata.toString());
    });
  }

  /// Collect metadata for all memref allocations
  void collectMemrefMetadata(ModuleOp module, ArtsMetadataManager &manager,
                             MemrefAnalyzer &analyzer) {
    // Collect memref.alloc
    module.walk([&](memref::AllocOp allocOp) {
      auto *metadata = manager.addMemrefMetadata(allocOp);
      analyzer.analyzeAllocation(allocOp, metadata, module);
      ARTS_DEBUG("  Analyzed memref.alloc " << metadata->allocationId);
    });

    // Collect memref.alloca
    module.walk([&](memref::AllocaOp allocaOp) {
      auto *metadata = manager.addMemrefMetadata(allocaOp);
      analyzer.analyzeAllocation(allocaOp, metadata, module);
      ARTS_DEBUG("  Analyzed memref.alloca " << metadata->allocationId);
    });
  }
};

//===----------------------------------------------------------------------===//
// Pass Registration
//===----------------------------------------------------------------------===//

std::unique_ptr<Pass> mlir::arts::createCollectMetadataPass() {
  return std::make_unique<CollectMetadataPass>();
}

std::unique_ptr<Pass>
mlir::arts::createCollectMetadataPass(bool exportMetadata,
                                      llvm::StringRef metadataFile) {
  return std::make_unique<CollectMetadataPass>(exportMetadata, metadataFile);
}
