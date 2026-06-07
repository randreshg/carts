///==========================================================================///
/// File: VerifySdeMuAccessWindowSync.cpp
///
/// Verifies that the surviving SU ordering between compute units is justified
/// by their raised MU access windows. A `sde.su_barrier` is the last-resort
/// global ordering; with canonical per-CU windows present, whether a barrier is
/// needed is a structural fact, not a guess. For each barrier this pass reads
/// the windows of the immediately-before and immediately-after CU phases
/// through the shared `classifyBarrierSync` analysis and rejects the verdicts
/// that prove a barrier should not have survived:
///
///   JUSTIFIED  — a shared MU is written on one side and accessed on the other
///                with overlapping blocks, and any read side stays within the
///                written blocks (aligned RAW/WAW/WAR). The barrier orders a
///                real dependency: accept.
///   REDUNDANT  — every shared MU is touched on disjoint blocks or read-only on
///                both sides (and the phases are fully described by windows),
///                so the ordered CUs are independent: the barrier is avoidable.
///   MISALIGNED — a consumer reads blocks of a shared MU that the producer does
///                not write (overlap, but the read is not within the write).
///                The data must move; that redistribution is owned by a later
///                SDE distribution transform, not by this ordering. Diagnose.
///
/// Fail-closed (windows present but the ordering cannot be proven): a shared MU
/// carries inconsistent owner-dim rank, or — when concluding REDUNDANT — an
/// adjacent CU has a memory access no window describes. Out of scope (no
/// windows on either side of the barrier) is skipped, never rejected; window
/// coverage itself is `verify-sde-mu-access-window`'s job. This pass mutates no
/// IR and introduces no token, slice, dependency-graph, or distribution
/// structure.
///
/// Distinct from effect-based barrier elimination: that decides removal from
/// structured memory effects and rewrites/stamps the barrier; this proves
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
