///==========================================================================///
/// File: SdeToArtsBoundary.cpp
///
/// SDE→ARTS boundary pass registration. Implementation lives in boundary/*.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryPasses.h"
#include "carts/passes/Passes.h"

namespace mlir::carts::arts {
#define GEN_PASS_DEF_SDESTORAGETOARTSDB
#define GEN_PASS_DEF_VERIFYRAWACCESSCOVERED
#define GEN_PASS_DEF_SDEACCESSESTOARTSDEPS
#define GEN_PASS_DEF_FINALIZESDETOARTS
#include "carts/passes/Passes.h.inc"

struct SdeStorageToArtsDbPass
    : public impl::SdeStorageToArtsDbBase<SdeStorageToArtsDbPass> {
  void runOnOperation() override {
    if (failed(boundary::runSdeStorageToArtsDb(getOperation())))
      signalPassFailure();
  }
};

struct VerifyRawAccessCoveredPass
    : public impl::VerifyRawAccessCoveredBase<VerifyRawAccessCoveredPass> {
  void runOnOperation() override {
    if (failed(boundary::runVerifyRawAccessCovered(getOperation())))
      signalPassFailure();
  }
};

struct SdeAccessesToArtsDepsPass
    : public impl::SdeAccessesToArtsDepsBase<SdeAccessesToArtsDepsPass> {
  void runOnOperation() override {
    if (failed(boundary::runSdeAccessesToArtsDeps(getOperation())))
      signalPassFailure();
  }
};

struct FinalizeSdeToArtsPass
    : public impl::FinalizeSdeToArtsBase<FinalizeSdeToArtsPass> {
  void runOnOperation() override {
    if (failed(boundary::runFinalizeSdeToArts(getOperation())))
      signalPassFailure();
  }
};

std::unique_ptr<Pass> createSdeStorageToArtsDbPass() {
  return std::make_unique<SdeStorageToArtsDbPass>();
}

std::unique_ptr<Pass> createVerifyRawAccessCoveredPass() {
  return std::make_unique<VerifyRawAccessCoveredPass>();
}

std::unique_ptr<Pass> createSdeAccessesToArtsDepsPass() {
  return std::make_unique<SdeAccessesToArtsDepsPass>();
}

std::unique_ptr<Pass> createFinalizeSdeToArtsPass() {
  return std::make_unique<FinalizeSdeToArtsPass>();
}

} // namespace mlir::carts::arts
