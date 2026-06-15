///==========================================================================///
/// File: EdtSplitForMixedDeps.cpp
///
/// Handle EDTs whose dependency set mixes a local-only distributed dependency
/// with a distributed writer. Carved from the former
/// DistributedLaunchConsistency pass (T031): this half rejects the unsplittable
/// mixed-dep join and localizes internode EDTs that only carry rejected
/// local-only distributed deps; the owner-route derivation/promotion half lives
/// in WriterOwnerRoute.
///==========================================================================///

#define GEN_PASS_DEF_EDTSPLITFORMIXEDDEPS
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#include "carts/utils/Debug.h"

ARTS_DEBUG_SETUP(edt_split_for_mixed_deps);

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

static bool hasDistributedWriterDependency(EdtOp edt) {
  for (Value dep : edt.getDependencies()) {
    Operation *underlying = DbUtils::getUnderlyingDb(dep);
    auto acquire = dyn_cast_or_null<DbAcquireOp>(underlying);
    if (!acquire || !DbUtils::isWriterMode(acquire.getMode()))
      continue;
    auto alloc =
        dyn_cast_or_null<DbAllocOp>(DbUtils::getUnderlyingDbAlloc(dep));
    if (alloc && hasDistributedDbAllocation(alloc.getOperation()))
      return true;
  }
  return false;
}

struct EdtSplitForMixedDepsPass
    : public impl::EdtSplitForMixedDepsBase<EdtSplitForMixedDepsPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool requiresInterNodeRouting = requiresArtsInterNodeOwnerRouting(module);
    unsigned localized = 0;
    bool failed = false;

    module.walk([&](EdtOp edt) {
      if (failed)
        return;
      if (DbUtils::hasLocalOnlyDistributedLaunchDependency(edt) &&
          hasDistributedWriterDependency(edt) && requiresInterNodeRouting) {
        edt.emitError()
            << "mixes a local-only distributed dependency with a distributed "
               "writer; preserving the original EDT join would require an "
               "explicit ordering dependency before ARTS can split the codelet";
        failed = true;
        return;
      }
      if (edt.getConcurrency() != EdtConcurrency::internode)
        return;
      if (!DbUtils::hasLocalOnlyDistributedLaunchDependency(edt))
        return;

      OpBuilder builder(edt);
      Value localRoute = createCurrentNodeRoute(builder, edt.getLoc());
      edt.setConcurrency(EdtConcurrency::intranode);
      edt.getRouteMutable().set(localRoute);
      ++localized;
      ARTS_DEBUG(
          "Localized internode EDT with rejected distributed DB dep: " << edt);
    });

    if (failed) {
      signalPassFailure();
      return;
    }

    ARTS_INFO("EDT split for mixed deps localized " << localized << " EDTs");
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::arts::createEdtSplitForMixedDepsPass() {
  return std::make_unique<EdtSplitForMixedDepsPass>();
}
