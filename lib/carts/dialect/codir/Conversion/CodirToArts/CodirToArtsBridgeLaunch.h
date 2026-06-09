///==========================================================================///
/// File: CodirToArtsBridgeLaunch.h
///
/// Bridge block-ordinal launch routing helpers.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGELAUNCH_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGELAUNCH_H

#include "CodirToArtsBridgeGrouping.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"

namespace {

static inline arts::ArtsLaunchPolicy resolveBridgeBlockOrdinalLaunchPolicy(
    ModuleOp module, const BridgePlan *plan, arts::DbAllocOp blockAlloc,
    Value blockOrdinal, OpBuilder &builder, Location loc) {
  arts::ArtsLaunchPolicy policy;
  if (!module || !arts::hasArtsInterNodeRuntime(module) || !blockOrdinal)
    return policy;

  if (plan && blockAlloc && plan->ownerMap.ownerMapKind &&
      !plan->ownerMap.ownerMapDims.empty()) {
    arts::DbOwnerMapPlan ownerPlan;
    ownerPlan.kind = *plan->ownerMap.ownerMapKind;
    ownerPlan.dims.assign(plan->ownerMap.ownerMapDims.begin(),
                          plan->ownerMap.ownerMapDims.end());
    ownerPlan.blockShape.assign(plan->ownerMap.physicalBlockShape.begin(),
                                plan->ownerMap.physicalBlockShape.end());
    SmallVector<Value, 4> dbSizes(blockAlloc.getSizes().begin(),
                                  blockAlloc.getSizes().end());
    Value totalNodes = arts::RuntimeQueryOp::create(
                           builder, loc, arts::RuntimeQueryKind::totalNodes)
                           .getResult();
    Value ownerRoute = arts::createDbOwnerRouteForLinearIndex(
        builder, loc, dbSizes, blockOrdinal, totalNodes, ownerPlan);
    if (ownerRoute) {
      policy.concurrency = arts::EdtConcurrency::internode;
      policy.route = ownerRoute;
      return policy;
    }
  }

  return arts::resolveArtsOrdinalLaunchPolicy(module, blockOrdinal, builder,
                                              loc);
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGELAUNCH_H
