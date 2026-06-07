///==========================================================================///
/// File: MuAccessWindowSyncOpt.cpp
///
/// Removes `sde.su_barrier` ordering points proven redundant by raised MU
/// access windows. This is the transform counterpart of
/// `verify-sde-mu-access-window-sync`: both read the shared
/// `classifyBarrierSync` analysis, so the pass erases exactly the barriers the
/// verifier would reject as REDUNDANT, and leaves every barrier the verifier
/// would accept or diagnose.
///
/// Removing a redundant global barrier is a real SDE structural change: the
/// ordered CUs touch disjoint blocks (or only read shared blocks), so with the
/// barrier gone they become async siblings — a parallel wave available to later
/// scheduling and to CODIR dataflow derivation. The pass authors no replacement
/// structure (no token, slice, dependency graph, distribution, or stage
/// boundary); a JUSTIFIED ordering is left as the irreducible `sde.su_barrier`
/// for CODIR to refine into explicit dependency edges from the same windows, a
/// MISALIGNED edge is left for the later SDE redistribution transform, and any
/// barrier whose ordering cannot be proven is preserved for the verifier to
/// gate.
///==========================================================================///

#include "carts/dialect/sde/Analysis/AccessWindowSync.h"
#include "carts/dialect/sde/Transforms/Passes.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_MUACCESSWINDOWSYNCOPT
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallVector.h"

#include "carts/utils/Debug.h"
ARTS_DEBUG_SETUP(mu_access_window_sync_opt);

using namespace mlir;
using namespace mlir::carts;

namespace {

struct MuAccessWindowSyncOptPass
    : public sde::impl::MuAccessWindowSyncOptBase<MuAccessWindowSyncOptPass> {
  void runOnOperation() override {
    // Collect first, erase after the walk: erasing a barrier mid-walk would
    // invalidate the block iteration. A barrier is redundant independently of
    // the others, so one pass over the IR finds the full worklist.
    SmallVector<sde::SdeSuBarrierOp, 4> redundant;

    getOperation().walk([&](Block *block) {
      sde::BarrierSyncPartition partition = sde::partitionBarrierPhases(*block);
      for (auto [barrier, beforeIdx] : partition.barriers)
        if (sde::classifyBarrierSync(partition.phases[beforeIdx],
                                     partition.phases[beforeIdx + 1]) ==
            sde::BarrierSyncVerdict::Redundant)
          redundant.push_back(barrier);
    });

    for (sde::SdeSuBarrierOp barrier : redundant)
      barrier.erase();

    ARTS_INFO("MuAccessWindowSyncOpt: removed " << redundant.size()
                                                << " redundant barrier(s)");
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createMuAccessWindowSyncOptPass() {
  return std::make_unique<MuAccessWindowSyncOptPass>();
}
} // namespace mlir::carts::sde
