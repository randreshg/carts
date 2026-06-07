///==========================================================================///
/// File: DbOwnerMapRealization.cpp
///
/// Realizes ARTS owner-map and memory-placement facts on DbAlloc operations
/// that are eligible for distributed ownership. The pass is mechanical: it
/// consumes the committed SDE/CODIR owner/block plan and projects it into an
/// ARTS owner map plus a scattered home. It does not choose a distribution
/// family, recompute block shape, or coarsen grain.
///
/// Example:
///   Before:
///     %db = arts.db_alloc ...      // committed plan, no owner-map facts
///
///   After:
///     %db = arts.db_alloc ... {distributed, owner_map_kind, owner_map_dims,
///                              owner_block_shape,
///                              db_memory_placement = owner_scattered}
///==========================================================================///

#define GEN_PASS_DEF_DBOWNERMAPREALIZATION
#include "carts/dialect/arts/Analysis/AnalysisManager.h"
#include "carts/dialect/arts/Analysis/db/DbAnalysis.h"
#include "carts/dialect/arts/Analysis/db/DbDistributedEligibility.h"
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#include "carts/utils/Debug.h"
ARTS_DEBUG_SETUP(db_owner_map_realization);

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

struct DbOwnerMapRealizationPass
    : public impl::DbOwnerMapRealizationBase<DbOwnerMapRealizationPass> {
  explicit DbOwnerMapRealizationPass(mlir::carts::arts::AnalysisManager *AM)
      : AM(AM) {}

  void runOnOperation() override {
    ModuleOp module = getOperation();
    if (!AM) {
      module.emitError()
          << "db-owner-map-realization requires the staged compiler pipeline; "
             "textual --pass-pipeline use cannot provide the ARTS "
             "AnalysisManager, runtime configuration, or distributed "
             "ownership wiring";
      signalPassFailure();
      return;
    }

    auto *machine = &AM->getRuntimeConfig();
    if (!machine->hasConfigFile() || !machine->hasValidNodeCount() ||
        !machine->hasValidThreads()) {
      module.emitError(
          "invalid ARTS machine configuration for distributed DB ownership");
      signalPassFailure();
      return;
    }

    auto &dbAnalysis = AM->getDbAnalysis();
    dbAnalysis.invalidate();

    unsigned totalAllocs = 0;
    unsigned markedDistributed = 0;
    bool failed = false;
    module.walk([&](DbAllocOp alloc) {
      if (failed)
        return;
      ++totalAllocs;
      auto eligibility = evaluateDistributedDbEligibility(alloc, dbAnalysis);
      setDistributedDbAllocation(alloc.getOperation(), eligibility.eligible);
      if (eligibility.eligible) {
        ++markedDistributed;
        alloc.removeDistributedRejectReasonAttr();
        alloc.removeLocalOnlyAttr();
        /// Stamp the distribution kind when the eligibility analysis specifies
        /// one (when a non-default distribution applies).
        if (eligibility.distributionKind)
          setEdtDistributionKind(alloc.getOperation(),
                                 *eligibility.distributionKind);
        /// Fail closed: a DB judged eligible carries a committed owner/block
        /// plan, so a plan that cannot be projected into an owner map is a real
        /// un-realizable partition, not a reason to fall back to a coarse DB.
        if (!realizeDbOwnerMapFromPlan(alloc)) {
          alloc.emitOpError()
              << "is eligible for distributed ownership but its committed "
                 "owner/block plan cannot be realized into an ARTS owner map";
          failed = true;
          return;
        }
      } else {
        alloc.setDistributedRejectReason(toString(eligibility.reason));
        ARTS_DEBUG("Reject DbAlloc arts.id=" << getArtsId(alloc.getOperation())
                                             << " reason="
                                             << toString(eligibility.reason));
      }
    });

    if (failed) {
      signalPassFailure();
      return;
    }

    ARTS_INFO("DbOwnerMapRealization realized " << markedDistributed << " / "
                                                << totalAllocs
                                                << " DbAlloc operations");
  }

private:
  mlir::carts::arts::AnalysisManager *AM = nullptr;
};

} // namespace

namespace mlir {
namespace carts::arts {
std::unique_ptr<Pass>
createDbOwnerMapRealizationPass(mlir::carts::arts::AnalysisManager *AM) {
  return std::make_unique<DbOwnerMapRealizationPass>(AM);
}
} // namespace carts::arts
} // namespace mlir
