///==========================================================================///
/// File: VerifySdeMuAccessWindowSync.cpp
///
/// Verifies that the surviving SU ordering between compute units is justified
/// by query-derived MU access windows. A `sde.su_barrier` is the last-resort
/// global ordering; with rank-expanded MUs and in-CU memory ops present,
/// whether a barrier is needed is a structural fact, not a guess.
///
/// Distinct from effect-based barrier elimination: that decides removal from
/// structured memory effects and rewrites the barrier; this proves
/// justification from raised block-grid window facts and changes nothing. The
/// removal counterpart of this verdict is `sde-mu-access-window-sync-opt`,
/// which shares the same classifier and erases the barriers proven REDUNDANT.
///==========================================================================///

#include "carts/dialect/sde/Analysis/AccessWindowSync.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYSDEMUACCESSWINDOWSYNC
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

struct VerifySdeMuAccessWindowSyncPass
    : public sde::impl::VerifySdeMuAccessWindowSyncBase<
          VerifySdeMuAccessWindowSyncPass> {
  void runOnOperation() override {
    bool failed = false;

    getOperation().walk([&](Block *block) {
      sde::BarrierSyncPartition partition = sde::partitionBarrierPhases(*block);
      for (auto [barrier, beforeIdx] : partition.barriers) {
        switch (sde::classifyBarrierSync(partition.phases[beforeIdx],
                                         partition.phases[beforeIdx + 1])) {
        case sde::BarrierSyncVerdict::OutOfScope:
        case sde::BarrierSyncVerdict::Justified:
          // No window dependency to reason about, or a real ordering: accept.
          break;
        case sde::BarrierSyncVerdict::Malformed:
          barrier.emitOpError(
              "a malformed access window prevents proving the ordering");
          failed = true;
          break;
        case sde::BarrierSyncVerdict::Misaligned:
          barrier.emitOpError(
              "a compute unit reads blocks of a shared memory unit that the "
              "ordered producer does not write; this needs a redistribution, "
              "not an ordering");
          failed = true;
          break;
        case sde::BarrierSyncVerdict::RankMismatch:
          barrier.emitOpError(
              "access windows on a shared memory unit disagree on owner-dim "
              "rank; the ordering cannot be proven");
          failed = true;
          break;
        case sde::BarrierSyncVerdict::Redundant:
          barrier.emitOpError("the access windows of the ordered compute units "
                              "prove they touch "
                              "disjoint blocks (or only read shared blocks), "
                              "so this ordering is "
                              "redundant");
          failed = true;
          break;
        case sde::BarrierSyncVerdict::Unprovable:
          barrier.emitOpError("an ordered compute unit has a memory access no "
                              "window describes; "
                              "the ordering cannot be proven");
          failed = true;
          break;
        }
      }
    });

    if (failed)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createVerifySdeMuAccessWindowSyncPass() {
  return std::make_unique<VerifySdeMuAccessWindowSyncPass>();
}
} // namespace mlir::carts::sde
