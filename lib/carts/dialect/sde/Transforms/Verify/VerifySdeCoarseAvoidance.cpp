///==========================================================================///
/// File: VerifySdeCoarseAvoidance.cpp
///
/// Coarse MU shape is rejected when SDE can realize the committed block grid.
///
/// For every `sde.mu_alloc` that is not block-partitioned, this verifier fails
/// only when the MU is in the single-owner block-grid realize scope and was
/// left flat. Unsupported shapes remain outside this verifier until SDE can
/// materialize them without inventing layout. The verifier reads the committed
/// plan verbatim and shares the realize gate
/// (`isSingleOwnerBlockGridRealizable`) with rank expansion and the pass; it
/// never recomputes owner dims or block shape.
///==========================================================================///

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYSDECOARSEAVOIDANCE
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

/// True if the MU's redistribution is explicitly represented by an sde.redist
/// fact — its movement is owned by sde-redistribute, not a coarse last resort.
static bool muHasRedist(carts::sde::SdeMuAllocOp mu) {
  for (Operation *user : mu.getMemref().getUsers())
    if (isa<carts::sde::SdeRedistOp>(user))
      return true;
  return false;
}

struct VerifySdeCoarseAvoidancePass
    : public sde::impl::VerifySdeCoarseAvoidanceBase<
          VerifySdeCoarseAvoidancePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool failed = false;

    module.walk(
        [&](sde::SdeMuAllocOp mu) {
          auto muType = dyn_cast<MemRefType>(mu.getMemref().getType());
          if (!muType)
            return;
          // Redistribution explicitly represented: sde-redistribute owns this
          // MU's movement, so it is not a coarse last resort.
          // verify-sde-redistribute gates the redistribution structure itself.
          if (muHasRedist(mu))
            return;
          sde::SdeSuIterateOp si = sde::findCommittedBlockPlanWriter(mu);

          // Block-partitioned: the grid is in the type. OK.
          if (si && sde::recognizeExpandedBlockGridMu(si, muType))
            return;

          // Avoidable coarse: in the realize scope but left flat.
          sde::MuPhysicalLayout plan;
          if (sde::isSingleOwnerBlockGridRealizable(si, muType, plan)) {
            mu.emitOpError()
                << "MU is in the block-grid realize scope but left coarse; "
                   "sde-coarse-avoidance must rank-expand the committed finest "
                   "grain";
            failed = true;
            return;
          }

          // Unsupported and local flat MUs are outside this gate.
        });

    if (failed)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createVerifySdeCoarseAvoidancePass() {
  return std::make_unique<VerifySdeCoarseAvoidancePass>();
}
} // namespace mlir::carts::sde
