///==========================================================================///
/// File: VerifyMetadataIntegrity.cpp
///
/// Validates metadata integrity on operations that already carry ARTS
/// metadata. Coverage is handled by VerifyMetadata; this pass focuses on
/// provenance and trustworthiness of the metadata that exists.
///==========================================================================///

#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/metadata/MetadataManager.h"
#include "arts/utils/Debug.h"
#include "arts/utils/LoopUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/metadata/Metadata.h"
#define GEN_PASS_DEF_VERIFYMETADATAINTEGRITY
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>

ARTS_DEBUG_SETUP(verify_metadata_integrity);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct LoopMetadataMismatch {
  Operation *op = nullptr;
  int64_t staticTripCount = 0;
  int64_t metadataTripCount = 0;
};

static void emitIntegrityError(Operation *op, llvm::Twine message) {
  if (!op) {
    ARTS_ERROR(message);
    return;
  }

  if (auto fileLoc = dyn_cast<FileLineColLoc>(op->getLoc())) {
    ARTS_ERROR(op->getName() << " at " << fileLoc.getFilename() << ":"
                             << fileLoc.getLine() << ":" << fileLoc.getColumn()
                             << " " << message);
    return;
  }

  ARTS_ERROR(op->getName() << " " << message);
}

struct VerifyMetadataIntegrityPass
    : public impl::VerifyMetadataIntegrityBase<VerifyMetadataIntegrityPass> {
  VerifyMetadataIntegrityPass() = default;
  VerifyMetadataIntegrityPass(const VerifyMetadataIntegrityPass &other)
      : impl::VerifyMetadataIntegrityBase<VerifyMetadataIntegrityPass>(other),
        analysisManager(other.analysisManager) {}
  VerifyMetadataIntegrityPass(mlir::arts::AnalysisManager *AM,
                              bool failOnError)
      : analysisManager(AM) {
    this->failOnError = failOnError;
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    mlir::arts::AnalysisManager &AM = getAnalysisManager(module);
    MetadataManager &mm = AM.getMetadataManager();

    /// Import metadata-bearing attrs so the pass also works on handwritten
    /// contract tests and standalone IR.
    mm.collectFromModule(module);

    DenseMap<int64_t, Operation *> seenIds;
    SmallVector<Operation *> missingIds;
    SmallVector<Operation *> missingProvenance;
    SmallVector<Operation *> invalidProvenance;
    SmallVector<Operation *> transferredWithoutOrigin;
    SmallVector<LoopMetadataMismatch> tripMismatches;
    unsigned duplicateIdCount = 0;

    for (const auto &entry : mm.getRegistry().getMetadataMap()) {
      Operation *op = entry.first;
      const ArtsMetadata *metadata = entry.second.get();
      if (!op || !metadata)
        continue;

      ArtsId metadataId = metadata->getMetadataId();
      if (!metadataId.has_value() || metadataId.value() <= 0) {
        missingIds.push_back(op);
      } else {
        auto [it, inserted] = seenIds.try_emplace(metadataId.value(), op);
        if (!inserted) {
          emitIntegrityError(
              op, llvm::Twine("duplicates metadata arts.id=") +
                      llvm::Twine(metadataId.value()));
          emitIntegrityError(it->second,
                             llvm::Twine("shares metadata arts.id=") +
                                 llvm::Twine(metadataId.value()));
          ++duplicateIdCount;
        }
      }

      auto rawProvenance = op->getAttrOfType<StringAttr>(
          AttrNames::Operation::MetadataProvenance);
      if (!rawProvenance) {
        missingProvenance.push_back(op);
      } else {
        auto provenance = parseMetadataProvenance(rawProvenance.getValue());
        if (!provenance) {
          invalidProvenance.push_back(op);
        } else if (*provenance == MetadataProvenanceKind::Transferred &&
                   !getMetadataOriginId(op)) {
          transferredWithoutOrigin.push_back(op);
        }
      }

      if (metadata->getMetadataName() != AttrNames::LoopMetadata::Name)
        continue;

      auto *loopMetadata = static_cast<const LoopMetadata *>(metadata);
      if (!loopMetadata->tripCount)
        continue;

      std::optional<int64_t> staticTripCount = arts::getStaticTripCount(op);
      if (!staticTripCount)
        continue;

      if (*staticTripCount != *loopMetadata->tripCount) {
        tripMismatches.push_back({op, *staticTripCount,
                                  *loopMetadata->tripCount});
      }
    }

    for (Operation *op : missingIds)
      emitIntegrityError(op, "has metadata but no valid arts.id");
    for (Operation *op : missingProvenance)
      emitIntegrityError(op, "has metadata but no metadata provenance marker");
    for (Operation *op : invalidProvenance)
      emitIntegrityError(op, "has invalid metadata provenance marker");
    for (Operation *op : transferredWithoutOrigin)
      emitIntegrityError(op,
                         "has transferred metadata provenance but no origin id");
    for (const LoopMetadataMismatch &mismatch : tripMismatches) {
      emitIntegrityError(
          mismatch.op,
          llvm::Twine("has stale metadata tripCount=") +
              llvm::Twine(mismatch.metadataTripCount) +
              llvm::Twine(" (static trip count is ") +
              llvm::Twine(mismatch.staticTripCount) + llvm::Twine(")"));
    }

    unsigned issueCount = duplicateIdCount + missingIds.size() +
                          missingProvenance.size() + invalidProvenance.size() +
                          transferredWithoutOrigin.size() +
                          tripMismatches.size();

    ARTS_INFO("Metadata integrity verification checked "
              << mm.getRegistry().getMetadataMap().size()
              << " metadata entries and found " << issueCount
              << " integrity issue(s)");

    if (failOnError && issueCount > 0) {
      emitError(module.getLoc())
          << "Metadata integrity verification failed with " << issueCount
          << " issue(s)";
      signalPassFailure();
    }
  }

private:
  mlir::arts::AnalysisManager &getAnalysisManager(ModuleOp module) {
    if (!analysisManager) {
      ownedAnalysisManager = std::make_unique<mlir::arts::AnalysisManager>(
          module);
      analysisManager = ownedAnalysisManager.get();
    }
    return *analysisManager;
  }

  mlir::arts::AnalysisManager *analysisManager = nullptr;
  std::unique_ptr<mlir::arts::AnalysisManager> ownedAnalysisManager;
};

} // namespace

std::unique_ptr<Pass>
mlir::arts::createVerifyMetadataIntegrityPass(AnalysisManager *AM,
                                              bool failOnError) {
  return std::make_unique<VerifyMetadataIntegrityPass>(AM, failOnError);
}
