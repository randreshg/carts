///==========================================================================///
/// File: EpochOpt.cpp
///
/// Epoch optimization pass driver. Realizes the committed repeated-timestep
/// epoch shape by amortizing the repeat loop. Transform implementation lives in
/// the neighboring EpochOptStructural compilation unit.
///==========================================================================///

#define GEN_PASS_DEF_EPOCHOPT

#include "EpochOptInternal.h"
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "carts/utils/Debug.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/Statistic.h"

ARTS_DEBUG_SETUP(epoch_opt);

using namespace mlir::carts;
using namespace mlir::carts::arts;

using namespace mlir;
using namespace mlir::carts::arts::epoch_opt;

namespace {

struct EpochOptPass : public impl::EpochOptBase<EpochOptPass> {
  EpochOptPass() = default;
  EpochOptPass(const EpochOptPass &other)
      : impl::EpochOptBase<EpochOptPass>(other) {}

  explicit EpochOptPass(bool amortization) : EpochOptPass() {
    enableAmortization = amortization;
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    ARTS_INFO_HEADER(EpochOptPass);

    if (!enableAmortization) {
      markAllAnalysesPreserved();
      ARTS_INFO_FOOTER(EpochOptPass);
      return;
    }

    SmallVector<EpochOp> amortEpochOps;
    module.walk([&](EpochOp epochOp) { amortEpochOps.push_back(epochOp); });
    ARTS_INFO("Found " << amortEpochOps.size()
                       << " epoch operations to analyze for amortization");

    unsigned amortized = 0;
    for (EpochOp epochOp : amortEpochOps) {
      if (tryAmortizeRepeatedEpochLoop(epochOp))
        ++amortized;
    }

    if (amortized > 0) {
      numRepeatedEpochLoopsAmortized += amortized;
      ARTS_INFO("Amortized " << amortized << " repeated epoch loop(s)");
    } else {
      markAllAnalysesPreserved();
    }

    ARTS_INFO_FOOTER(EpochOptPass);
  }

private:
  Statistic numRepeatedEpochLoopsAmortized{
      this, "num-repeated-epoch-loops-amortized",
      "Number of repeated epoch loops amortized"};
};

} // namespace

namespace mlir {
namespace carts::arts {

std::unique_ptr<Pass> createEpochOptPass() {
  return std::make_unique<EpochOptPass>();
}

std::unique_ptr<Pass> createEpochOptPass(bool amortization) {
  return std::make_unique<EpochOptPass>(amortization);
}

} // namespace carts::arts
} // namespace mlir
