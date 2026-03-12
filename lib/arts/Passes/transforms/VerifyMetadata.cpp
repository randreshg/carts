///==========================================================================///
/// File: VerifyMetadata.cpp
///
/// This pass verifies that all loops and memrefs in the program have
/// associated metadata from CollectMetadataPass. It counts entities and
/// compares against MetadataManager entries to ensure complete coverage.
///
/// Used automatically with --diagnose to ensure the diagnose output contains
/// complete information for all entities.
///
/// Example:
///   Before:
///     module has loops/memrefs without metadata entries
///
///   After:
///     pass emits diagnostics and fails if metadata coverage is incomplete
///==========================================================================///

#include "arts/passes/PassDetails.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/loop/LoopAnalysis.h"
#include "arts/analysis/metadata/MetadataManager.h"
#include "arts/ArtsDialect.h"
#include "arts/passes/Passes.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include "arts/utils/metadata/MemrefMetadata.h"

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Dominance.h"
#include "llvm/Support/raw_ostream.h"

/// Debug
#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(verify_metadata);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// VerifyMetadataPass
///===----------------------------------------------------------------------===///

struct VerifyMetadataPass : public VerifyMetadataBase<VerifyMetadataPass> {
  mlir::arts::AnalysisManager *analysisManager = nullptr;

  VerifyMetadataPass() = default;
  VerifyMetadataPass(mlir::arts::AnalysisManager *AM, bool failOnMissing)
      : VerifyMetadataBase(), analysisManager(AM) {
    this->failOnMissing = failOnMissing;
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    ARTS_INFO_HEADER(VerifyMetadataPass);

    /// Get metadata manager
    MetadataManager *mm = nullptr;
    if (analysisManager) {
      mm = &analysisManager->getMetadataManager();
    }

    if (!mm) {
      ARTS_DEBUG("No metadata manager available, skipping verification");
      return;
    }

    /// Count loops in the module
    int64_t totalLoops = 0;
    int64_t loopsWithMetadata = 0;
    SmallVector<Operation *> loopsMissingMetadata;

    module.walk([&](Operation *op) {
      if (analysisManager->getLoopAnalysis().isLoopOperation(op)) {
        totalLoops++;
        if (mm->getLoopMetadata(op)) {
          loopsWithMetadata++;
        } else {
          loopsMissingMetadata.push_back(op);
        }
      }
    });

    /// Count memrefs in the module
    int64_t totalMemrefs = 0;
    int64_t memrefsWithMetadata = 0;
    SmallVector<Operation *> memrefsMissingMetadata;

    module.walk([&](Operation *op) {
      if (isa<memref::AllocOp, memref::AllocaOp>(op)) {
        totalMemrefs++;
        if (mm->getMemrefMetadata(op)) {
          memrefsWithMetadata++;
        } else {
          memrefsMissingMetadata.push_back(op);
        }
      }
    });

    /// Calculate coverage
    double loopCoverage =
        totalLoops > 0 ? (100.0 * loopsWithMetadata / totalLoops) : 100.0;
    double memrefCoverage =
        totalMemrefs > 0 ? (100.0 * memrefsWithMetadata / totalMemrefs) : 100.0;
    double totalCoverage =
        (totalLoops + totalMemrefs) > 0
            ? (100.0 * (loopsWithMetadata + memrefsWithMetadata) /
               (totalLoops + totalMemrefs))
            : 100.0;

    /// Report coverage
    llvm::errs() << "[CARTS] Metadata verification: " << loopsWithMetadata
                 << "/" << totalLoops << " loops, " << memrefsWithMetadata
                 << "/" << totalMemrefs << " memrefs ("
                 << llvm::format("%.1f", totalCoverage) << "% coverage)\n";

    /// Emit warnings for missing metadata
    if (!loopsMissingMetadata.empty()) {
      for (auto *op : loopsMissingMetadata) {
        auto loc = op->getLoc();
        if (auto fileLoc = dyn_cast<FileLineColLoc>(loc)) {
          llvm::errs() << "[CARTS] Warning: Loop at " << fileLoc.getFilename()
                       << ":" << fileLoc.getLine() << " missing metadata\n";
        } else {
          llvm::errs() << "[CARTS] Warning: Loop missing metadata\n";
        }
      }
    }

    if (!memrefsMissingMetadata.empty()) {
      for (auto *op : memrefsMissingMetadata) {
        auto loc = op->getLoc();
        if (auto fileLoc = dyn_cast<FileLineColLoc>(loc)) {
          llvm::errs() << "[CARTS] Warning: Memref at " << fileLoc.getFilename()
                       << ":" << fileLoc.getLine() << " missing metadata\n";
        } else {
          llvm::errs() << "[CARTS] Warning: Memref missing metadata\n";
        }
      }
    }

    /// Store coverage data in analysis manager for diagnose export
    if (analysisManager) {
      analysisManager->setMetadataCoverage(loopsWithMetadata, totalLoops,
                                           memrefsWithMetadata, totalMemrefs);
    }

    /// Optionally fail if coverage is incomplete
    if (failOnMissing && (loopsWithMetadata < totalLoops ||
                          memrefsWithMetadata < totalMemrefs)) {
      emitError(module.getLoc())
          << "Incomplete metadata coverage: " << loopCoverage << "% loops, "
          << memrefCoverage << "% memrefs";
      signalPassFailure();
    }
  }
};

std::unique_ptr<Pass>
mlir::arts::createVerifyMetadataPass(mlir::arts::AnalysisManager *AM,
                                     bool failOnMissing) {
  return std::make_unique<VerifyMetadataPass>(AM, failOnMissing);
}
