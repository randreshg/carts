///==========================================================================///
/// File: VerifyDistributedDbPlacement.cpp
///
/// Verifies that distributed DB ownership has an explicit ARTS owner map before
/// ARTS-RT lowers GUID reservation and creation.
///==========================================================================///

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#define GEN_PASS_DEF_VERIFYDISTRIBUTEDDBPLACEMENT
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallPtrSet.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

/// No internode ARTS task may depend on a coarse single-block aggregate user
/// DB: distributed execution requires block DB storage (or a per-node-localized
/// host bridge / small replicated read). This runs after DbDistributedOwnership
/// and DistributedLaunchConsistency, so coarse host sources are already marked
/// local_only and their bridges localized to intranode; a coarse internode dep
/// surviving to this point is a genuine un-materialized distribution.
static void verifyDistributedDbDeps(EdtOp edt, bool &found) {
  if (!edt || edt.getConcurrency() != EdtConcurrency::internode)
    return;

  llvm::SmallPtrSet<Operation *, 4> reported;
  for (Value dep : edt.getDependencies()) {
    auto alloc =
        dyn_cast_or_null<DbAllocOp>(DbUtils::getUnderlyingDbAlloc(dep));
    if (!DbUtils::isCoarseUserDataDb(alloc))
      continue;
    if (DbUtils::isAllowedReadOnlyCoarseDep(dep, alloc))
      continue;
    if (DbUtils::isHostWholeToComputeBlockBridgeMovement(edt))
      continue;
    if (!reported.insert(alloc.getOperation()).second)
      continue;

    InFlightDiagnostic diag =
        edt.emitError()
        << "internode ARTS task depends on a coarse single-block aggregate "
           "user DB";
    diag.attachNote(alloc.getLoc())
        << "coarse DB allocation feeding the distributed task";
    diag.attachNote(edt.getLoc())
        << "SDE/CODIR must materialize block DB storage before ARTS "
           "distributed execution; CreateDbs is only a coarse raw-memref "
           "fallback";
    found = true;
  }
}

static LogicalResult verifyDistributedDbAlloc(DbAllocOp alloc) {
  if (!hasDistributedDbAllocation(alloc.getOperation()))
    return success();

  auto plan = getDbOwnerMapPlan(alloc);
  if (!plan)
    return alloc.emitOpError()
           << "is marked distributed but lacks a complete owner-map plan";

  if (!alloc.getOwnerMapVersionAttr() ||
      alloc.getOwnerMapVersionAttr().getInt() != kDbOwnerMapVersion)
    return alloc.emitOpError()
           << "has unsupported distributed owner-map version";

  if (!getPlanOwnerDimsAttr(alloc.getOperation()) ||
      !getPlanPhysicalBlockShapeAttr(alloc.getOperation()))
    return alloc.emitOpError()
           << "is distributed without preserved owner dims and physical block "
              "shape";

  if (alloc.getLocalOnly().value_or(false))
    return alloc.emitOpError() << "is both distributed and local_only";
  if (alloc.getDistributedRejectReasonAttr())
    return alloc.emitOpError()
           << "is both distributed and rejected for distributed ownership";

  if (!ownerMapPreservesPlanOwnerDims(alloc, *plan))
    return alloc.emitOpError()
           << "has owner_map_dims that do not preserve planOwnerDims";
  if (!ownerMapPreservesPlanBlockShape(alloc, *plan))
    return alloc.emitOpError()
           << "has owner_block_shape that does not preserve "
              "planPhysicalBlockShape";

  switch (plan->kind) {
  case DbOwnerMapKind::linear_mod_nodes:
  case DbOwnerMapKind::owner_dim_contiguous:
    if (!ownerDimsAddressDbRank(plan->dims, alloc.getSizes().size()))
      return alloc.emitOpError() << "has owner_map_dims outside the DB rank";
    break;
  case DbOwnerMapKind::owner_dim_grid:
  case DbOwnerMapKind::explicit_rank_table:
    return alloc.emitOpError()
           << "uses an owner-map kind that this lowering cannot realize";
  }

  return success();
}

struct VerifyDistributedDbPlacementPass
    : public impl::VerifyDistributedDbPlacementBase<
          VerifyDistributedDbPlacementPass> {
  void runOnOperation() override {
    bool failed = false;
    getOperation().walk([&](DbAllocOp alloc) {
      if (mlir::failed(verifyDistributedDbAlloc(alloc)))
        failed = true;
    });
    getOperation().walk(
        [&](EdtOp edt) { verifyDistributedDbDeps(edt, failed); });
    if (failed)
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass>
mlir::carts::arts::createVerifyDistributedDbPlacementPass() {
  return std::make_unique<VerifyDistributedDbPlacementPass>();
}
