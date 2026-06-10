///==========================================================================///
/// File: CodirToArtsBridgeGrouping.h
///
/// Bridge work-group legality and group-size selection.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEGROUPING_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEGROUPING_H

#include "CodirToArtsBridgePayload.h"
#include "CodirToArtsBridgePlan.h"

namespace {

static inline bool
bridgeOwnerMapUsesContiguousRowMajorDbSpace(const BridgePlan &plan,
                                            arts::DbAllocOp blockAlloc) {
  if (!blockAlloc || !plan.ownerMap.ownerMapKind ||
      *plan.ownerMap.ownerMapKind != arts::DbOwnerMapKind::owner_dim_contiguous)
    return false;
  unsigned dbRank = blockAlloc.getSizes().size();
  if (dbRank == 0 || plan.ownerMap.ownerMapDims.size() != dbRank)
    return false;
  for (auto [index, dim] : llvm::enumerate(plan.ownerMap.ownerMapDims))
    if (dim != static_cast<int64_t>(index))
      return false;
  return true;
}

static inline std::optional<int64_t>
getContiguousOwnerRouteSpan(const BridgePlan &plan, arts::DbAllocOp blockAlloc,
                            int64_t blockCount) {
  if (!blockAlloc || blockCount <= 0)
    return std::nullopt;
  if (!plan.hasInterNodeRuntime)
    return blockCount;
  if (!bridgeOwnerMapUsesContiguousRowMajorDbSpace(plan, blockAlloc))
    return std::nullopt;

  ModuleOp module = blockAlloc->getParentOfType<ModuleOp>();
  std::optional<int64_t> totalNodes = arts::getRuntimeTotalNodes(module);
  if (!totalNodes || *totalNodes <= 1)
    return blockCount;
  if (blockCount % *totalNodes != 0)
    return std::nullopt;
  return blockCount / *totalNodes;
}

static inline bool bridgeBlockOrdinalCanGroupAdjacentBlocks(
    const BridgePlan &plan, arts::DbAllocOp blockAlloc, int64_t blockCount) {
  if (!blockAlloc || blockCount <= 1)
    return false;
  if (!plan.hasInterNodeRuntime)
    return true;
  return getContiguousOwnerRouteSpan(plan, blockAlloc, blockCount).has_value();
}

static inline bool
bridgeGroupPreservesBlockOrdinalRoute(const BridgePlan &plan,
                                      arts::DbAllocOp blockAlloc,
                                      int64_t blockCount, int64_t groupSize) {
  if (groupSize <= 1)
    return true;
  if (!blockAlloc || blockCount <= 1)
    return false;
  if (!plan.hasInterNodeRuntime)
    return blockCount % groupSize == 0;
  std::optional<int64_t> routeSpan =
      getContiguousOwnerRouteSpan(plan, blockAlloc, blockCount);
  return routeSpan && *routeSpan > 0 && *routeSpan % groupSize == 0;
}

static inline BridgeWorkloadEvidence
buildBridgeWorkloadEvidence(const BridgePlan &plan,
                            BridgeWorkloadKind workloadKind,
                            bool crossNodeGather = false) {
  BridgeWorkloadEvidence evidence;
  evidence.workloadKind = workloadKind;
  evidence.routingMode =
      getBridgeWorkloadRoutingMode(plan, workloadKind, crossNodeGather);

  switch (workloadKind) {
  case BridgeWorkloadKind::host_to_block:
    evidence.readOnlySource = true;
    evidence.copyLike = true;
    break;
  case BridgeWorkloadKind::block_to_host:
    evidence.readOnlySource = true;
    evidence.copyLike = true;
    break;
  case BridgeWorkloadKind::all_gather:
    evidence.readOnlySource = true;
    evidence.copyLike = true;
    break;
  case BridgeWorkloadKind::summing_settle:
    evidence.readOnlySource = true;
    break;
  case BridgeWorkloadKind::halo:
    evidence.readOnlySource = true;
    evidence.haloExchange = true;
    break;
  }

  // Grouping is only a launch-shaping decision: each lane keeps its own block
  // DB acquire, so per-block DB grain and single-writer evidence remain intact.
  // Block-ordinal work may group only when committed owner-map facts prove
  // adjacent block ordinals stay inside one contiguous owner-route range.
  bool reductionSettleLike =
      workloadKind == BridgeWorkloadKind::summing_settle &&
      evidence.readOnlySource && evidence.preservesPerBlockDbGrain;
  arts::DbAllocOp blockAlloc = getBridgePlanSizingAlloc(plan, workloadKind);
  int64_t blockCount = blockAlloc ? getStaticFlatBlockCount(blockAlloc) : 0;
  bool blockOrdinalGroupable =
      evidence.routingMode != BridgeRoutingMode::block_ordinal ||
      bridgeBlockOrdinalCanGroupAdjacentBlocks(plan, blockAlloc, blockCount);
  evidence.mayGroupAdjacentBlocks =
      (((evidence.copyLike || evidence.haloExchange) &&
        evidence.readOnlySource && evidence.preservesPerBlockDbGrain) ||
       reductionSettleLike) &&
      blockOrdinalGroupable;
  return evidence;
}

static inline int64_t
chooseBridgeGroupSize(const BridgePlan &plan,
                      const BridgeWorkloadEvidence &evidence) {
  constexpr int64_t kTargetBridgeTaskBytes = 256LL * 1024LL;
  constexpr int64_t kMaxBridgeGroupBlocks = 8;

  if (!evidence.mayGroupAdjacentBlocks)
    return 1;

  arts::DbAllocOp blockAlloc =
      getBridgePlanSizingAlloc(plan, evidence.workloadKind);
  if (!blockAlloc)
    return 1;
  int64_t flatBlockCount = getStaticFlatBlockCount(blockAlloc);
  if (flatBlockCount <= 1)
    return 1;
  std::optional<int64_t> blockCount = flatBlockCount;
  if (evidence.workloadKind == BridgeWorkloadKind::summing_settle) {
    if (plan.tileCount <= 1 ||
        *blockCount % static_cast<int64_t>(plan.tileCount) != 0)
      return 1;
    *blockCount /= static_cast<int64_t>(plan.tileCount);
    if (*blockCount <= 1)
      return 1;
  }

  int64_t blockBytes =
      getBridgeWorkloadPayloadBytes(plan, evidence, blockAlloc);
  if (blockBytes <= 0 || blockBytes >= kTargetBridgeTaskBytes)
    return 1;

  int64_t desired = llvm::divideCeil(kTargetBridgeTaskBytes,
                                     std::max<int64_t>(1, blockBytes));
  desired = std::clamp<int64_t>(desired, 1, kMaxBridgeGroupBlocks);
  desired = std::min<int64_t>(desired, *blockCount);

  // Bridge grouping is a launch-shaping choice over already-realized per-block
  // DBs; it never changes DB/MU storage grain.

  for (int64_t group = desired; group > 1; --group)
    if (*blockCount % group == 0 &&
        (evidence.routingMode != BridgeRoutingMode::block_ordinal ||
         bridgeGroupPreservesBlockOrdinalRoute(plan, blockAlloc, *blockCount,
                                               group)))
      return group;
  return 1;
}

static inline BridgeWorkGroupPlan
planBridgeWorkGroups(const BridgePlan &plan, BridgeWorkloadKind workloadKind,
                     bool crossNodeGather) {
  BridgeWorkGroupPlan groupPlan;
  groupPlan.evidence =
      buildBridgeWorkloadEvidence(plan, workloadKind, crossNodeGather);
  if (arts::DbAllocOp blockAlloc = getBridgePlanSizingAlloc(plan, workloadKind))
    groupPlan.staticBlockCount = getStaticFlatBlockCount(blockAlloc);
  groupPlan.groupSize = chooseBridgeGroupSize(plan, groupPlan.evidence);
  return groupPlan;
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEGROUPING_H
