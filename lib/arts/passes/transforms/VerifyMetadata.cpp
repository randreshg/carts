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

#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/loop/LoopAnalysis.h"
#include "arts/analysis/metadata/MetadataManager.h"
#include "arts/utils/LoopUtils.h"
#define GEN_PASS_DEF_VERIFYMETADATA
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/utils/metadata/LoopMetadata.h"
#include "arts/utils/metadata/MemrefMetadata.h"
#include "mlir/Pass/Pass.h"

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Dominance.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

/// Debug
#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(verify_metadata);

using namespace mlir;
using namespace mlir::arts;

namespace {
struct LoopMetadataMismatch {
  Operation *op = nullptr;
  int64_t staticTripCount = 0;
  int64_t metadataTripCount = 0;
};
} // namespace

///===----------------------------------------------------------------------===///
/// VerifyMetadataPass
///===----------------------------------------------------------------------===///

struct VerifyMetadataPass
    : public impl::VerifyMetadataBase<VerifyMetadataPass> {
  VerifyMetadataPass() = default;
  VerifyMetadataPass(const VerifyMetadataPass &other)
      : impl::VerifyMetadataBase<VerifyMetadataPass>(other),
        analysisManager(other.analysisManager) {}
  VerifyMetadataPass(mlir::arts::AnalysisManager *AM, bool failOnMissing)
      : VerifyMetadataBase(), analysisManager(AM) {
    this->failOnMissing = failOnMissing;
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    mlir::arts::AnalysisManager &AM = getAnalysisManager(module);

    ARTS_INFO_HEADER(VerifyMetadataPass);

    MetadataManager &mm = AM.getMetadataManager();

    /// Count loops in the module
    int64_t totalLoops = 0;
    int64_t loopsWithMetadata = 0;
    SmallVector<Operation *> loopsMissingMetadata;
    SmallVector<LoopMetadataMismatch> loopsWithMismatchedTripCount;

    module.walk([&](Operation *op) {
      if (AM.getLoopAnalysis().isLoopOperation(op)) {
        totalLoops++;
        if (auto *loopMetadata = mm.getLoopMetadata(op)) {
          loopsWithMetadata++;
          if (loopMetadata->tripCount) {
            if (std::optional<int64_t> staticTripCount = getStaticTripCount(op);
                staticTripCount &&
                *staticTripCount != *loopMetadata->tripCount) {
              loopsWithMismatchedTripCount.push_back(
                  {op, *staticTripCount, *loopMetadata->tripCount});
            }
          }
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
        if (mm.getMemrefMetadata(op)) {
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
    ARTS_INFO("Metadata verification: " << loopsWithMetadata << "/"
                                        << totalLoops << " loops, "
                                        << memrefsWithMetadata << "/"
                                        << totalMemrefs << " memrefs ("
                                        << llvm::format("%.1f", totalCoverage)
                                        << "% coverage)");

    /// Emit warnings for missing metadata
    if (!loopsMissingMetadata.empty()) {
      for (auto *op : loopsMissingMetadata) {
        op->emitWarning("loop missing metadata");
      }
    }

    if (!loopsWithMismatchedTripCount.empty()) {
      for (const LoopMetadataMismatch &mismatch :
           loopsWithMismatchedTripCount) {
        mismatch.op->emitWarning()
            << "loop has stale metadata tripCount="
            << mismatch.metadataTripCount << " (static trip count is "
            << mismatch.staticTripCount << ")";
      }
    }

    if (!memrefsMissingMetadata.empty()) {
      for (auto *op : memrefsMissingMetadata) {
        op->emitWarning("memref missing metadata");
      }
    }

    /// Store coverage data in analysis manager for diagnose export
    AM.setMetadataCoverage(loopsWithMetadata, totalLoops, memrefsWithMetadata,
                           totalMemrefs);

    /// Optionally fail if coverage is incomplete
    if (failOnMissing && (loopsWithMetadata < totalLoops ||
                          memrefsWithMetadata < totalMemrefs ||
                          !loopsWithMismatchedTripCount.empty())) {
      emitError(module.getLoc())
          << "Metadata verification failed: " << loopCoverage << "% loops, "
          << memrefCoverage << "% memrefs, "
          << loopsWithMismatchedTripCount.size()
          << " loop trip-count mismatch(es)";
      signalPassFailure();
    }
  }

private:
  mlir::arts::AnalysisManager &getAnalysisManager(ModuleOp module) {
    if (!analysisManager) {
      ownedAnalysisManager =
          std::make_unique<mlir::arts::AnalysisManager>(module);
      analysisManager = ownedAnalysisManager.get();
    }
    return *analysisManager;
  }

  mlir::arts::AnalysisManager *analysisManager = nullptr;
  std::unique_ptr<mlir::arts::AnalysisManager> ownedAnalysisManager;
};

std::unique_ptr<Pass>
mlir::arts::createVerifyMetadataPass(mlir::arts::AnalysisManager *AM,
                                     bool failOnMissing) {
  return std::make_unique<VerifyMetadataPass>(AM, failOnMissing);
}
