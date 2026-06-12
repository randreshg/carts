///==========================================================================///
/// File: DbDistributedOwnershipRealization.cpp
///
/// Realizes distributed DB ownership on DbAlloc operations that are eligible
/// for distributed ownership. The pass is mechanical: it proves that owner
/// routes are derivable from the current DB block grid and marks the DB
/// distributed. It does not choose a distribution family, rewrite block shape,
/// write route attrs, or coarsen grain.
///
/// Example:
///   Before:
///     %db = arts.db_alloc ...      // block grid, no distributed DB home
///
///   After:
///     %db = arts.db_alloc ... {distributed}
///==========================================================================///

#define GEN_PASS_DEF_DBDISTRIBUTEDOWNERSHIPREALIZATION
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbDistributedEligibility.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#include "carts/utils/Debug.h"
ARTS_DEBUG_SETUP(db_owner_route_realization);

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

struct DbDistributedOwnershipRealizationPass
    : public impl::DbDistributedOwnershipRealizationBase<
          DbDistributedOwnershipRealizationPass> {
  DbDistributedOwnershipRealizationPass() = default;

  void runOnOperation() override {
    ModuleOp module = getOperation();

    auto totalNodes = arts::getRuntimeTotalNodes(module);
    auto totalWorkers = arts::getRuntimeTotalWorkers(module);
    if (!totalNodes || !totalWorkers || *totalNodes <= 0 ||
        *totalWorkers <= 0) {
      module.emitError("missing runtime worker/node configuration for "
                       "distributed DB ownership");
      signalPassFailure();
      return;
    }

    unsigned totalAllocs = 0;
    unsigned markedDistributed = 0;
    bool failed = false;
    module.walk([&](DbAllocOp alloc) {
      if (failed)
        return;
      ++totalAllocs;
      auto eligibility = evaluateDistributedDbEligibility(alloc);
      setDistributedDbAllocation(alloc.getOperation(), eligibility.eligible);
      if (eligibility.eligible) {
        ++markedDistributed;
        alloc.removeLocalOnlyAttr();
        if (eligibility.distributionKind)
          setEdtDistributionKind(alloc.getOperation(),
                                 *eligibility.distributionKind);
        if (!canDeriveDbOwnerRouteFromGrid(alloc)) {
          alloc.emitOpError()
              << "is eligible for distributed ownership but its committed "
                 "DB block grid cannot answer ARTS owner-route queries";
          failed = true;
          return;
        }
      } else {
        if (hasArtsDbPhysicalLayout(alloc.getOperation()) &&
            eligibility.reason !=
                DistributedDbEligibilityRejectReason::PerBlockReplicated &&
            eligibility.reason !=
                DistributedDbEligibilityRejectReason::NoDistributedOwnerUse) {
          alloc.emitOpError()
              << "carries a committed SDE block layout but is not eligible "
                 "for distributed DB realization ("
              << toString(eligibility.reason)
              << "); ARTS must materialize explicit graph work or fail before "
                 "distributed DB realization";
          failed = true;
          return;
        }
        ARTS_DEBUG("Reject DbAlloc arts.id=" << getArtsId(alloc.getOperation())
                                             << " reason="
                                             << toString(eligibility.reason));
      }
    });

    if (failed) {
      signalPassFailure();
      return;
    }

    ARTS_INFO("DbDistributedOwnershipRealization realized "
              << markedDistributed << " / " << totalAllocs
              << " DbAlloc operations");
  }
};

} // namespace

namespace mlir {
namespace carts::arts {
std::unique_ptr<Pass> createDbDistributedOwnershipRealizationPass() {
  return std::make_unique<DbDistributedOwnershipRealizationPass>();
}
} // namespace carts::arts
} // namespace mlir
