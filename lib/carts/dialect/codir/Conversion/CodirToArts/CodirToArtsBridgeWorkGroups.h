///==========================================================================///
/// File: CodirToArtsBridgeWorkGroups.h
///
/// Bridge workload sizing, grouping, owner-map, and route helpers.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEWORKGROUPS_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEWORKGROUPS_H

#include "CodirToArtsHostBlockCopy.h"

namespace {

static inline int64_t getScalarElementBytes(Type elementType) {
  if (!elementType || !elementType.isIntOrFloat())
    return 0;
  return llvm::divideCeil(elementType.getIntOrFloatBitWidth(), 8);
}

static inline int64_t saturatingMul(int64_t lhs, int64_t rhs) {
  if (lhs <= 0 || rhs <= 0)
    return 0;
  if (lhs > std::numeric_limits<int64_t>::max() / rhs)
    return std::numeric_limits<int64_t>::max();
  return lhs * rhs;
}

static inline std::optional<int64_t> getPositiveStaticIndex(Value value) {
  std::optional<int64_t> folded =
      ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(value);
  if (folded && *folded > 0)
    return folded;
  return std::nullopt;
}

static inline int64_t getStaticBlockPayloadBytes(arts::DbAllocOp blockAlloc) {
  int64_t bytes = getScalarElementBytes(blockAlloc.getElementType());
  if (bytes <= 0)
    return 0;
  for (Value size : blockAlloc.getElementSizes()) {
    std::optional<int64_t> constant = getPositiveStaticIndex(size);
    if (!constant)
      return 0;
    bytes = saturatingMul(bytes, *constant);
  }
  return bytes;
}

static inline int64_t getStaticHaloPayloadBytes(const BridgePlan &plan,
                                                arts::DbAllocOp blockAlloc) {
  if (!plan.seedCodelet || !blockAlloc)
    return 0;
  int64_t bytes = getScalarElementBytes(blockAlloc.getElementType());
  if (bytes <= 0)
    return 0;

  unsigned rank = static_cast<unsigned>(blockAlloc.getElementSizes().size());
  if (rank == 0)
    return 0;
  SmallVector<CodirOwnerHaloWindow, 4> ownerHalos =
      getCodirBlockStorageHaloWindows(plan.seedCodelet, plan.seedDepIndex,
                                      rank);
  std::optional<SmallVector<int64_t, 4>> ownerBlockSizes =
      getCodirTileOwnerBlockSizes(plan.seedCodelet, plan.seedDepIndex, rank);
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(plan.seedCodelet, plan.seedDepIndex);
  if (!ownerBlockSizes || !ownerDims ||
      ownerBlockSizes->size() != ownerHalos.size() ||
      ownerDims->size() != ownerHalos.size())
    return 0;

  int64_t haloBytes = 0;
  for (auto [slot, halo] : llvm::enumerate(ownerHalos)) {
    int64_t width = halo.lower + halo.upper;
    if (width <= 0)
      continue;
    int64_t faceElements = width;
    for (auto [otherSlot, otherHalo] : llvm::enumerate(ownerHalos)) {
      (void)otherHalo;
      if (otherSlot == slot)
        continue;
      if ((*ownerBlockSizes)[otherSlot] <= 0)
        return 0;
      faceElements = saturatingMul(faceElements, (*ownerBlockSizes)[otherSlot]);
    }
    int64_t faceBytes = saturatingMul(faceElements, bytes);
    if (haloBytes > std::numeric_limits<int64_t>::max() - faceBytes)
      haloBytes = std::numeric_limits<int64_t>::max();
    else
      haloBytes += faceBytes;
  }
  return haloBytes;
}

static inline int64_t
getBridgeWorkloadPayloadBytes(const BridgePlan &plan,
                              const BridgeWorkloadEvidence &evidence,
                              arts::DbAllocOp blockAlloc) {
  if (evidence.workloadKind == BridgeWorkloadKind::halo) {
    int64_t haloBytes = getStaticHaloPayloadBytes(plan, blockAlloc);
    if (haloBytes > 0)
      return haloBytes;
  }
  return getStaticBlockPayloadBytes(blockAlloc);
}

static inline std::optional<int64_t> readPositiveI64(DictionaryAttr dict,
                                                     StringRef key) {
  if (!dict)
    return std::nullopt;
  auto attr = dyn_cast_or_null<IntegerAttr>(dict.get(key));
  if (!attr || attr.getInt() <= 0)
    return std::nullopt;
  return attr.getInt();
}

static inline int64_t
readPartitionScoreConcurrencyFloor(codir::CodeletOp codelet) {
  if (!codelet)
    return 0;
  auto score = dyn_cast_or_null<DictionaryAttr>(
      codelet->getAttr(codir::AttrNames::PartitionScore));
  if (!score)
    return 0;

  std::optional<int64_t> targetWorkers = readPositiveI64(
      score, codir::AttrNames::PartitionScoreKeys::TargetLogicalWorkers);
  std::optional<int64_t> exposedCuCount = readPositiveI64(
      score, codir::AttrNames::PartitionScoreKeys::ExposedCuCount);
  if (targetWorkers && exposedCuCount)
    return std::min(*targetWorkers, *exposedCuCount);
  if (targetWorkers)
    return *targetWorkers;
  if (exposedCuCount)
    return *exposedCuCount;
  return 0;
}

struct BridgePartitionGraphEvidence {
  int64_t muBlockCount = 0;
  int64_t cuGroupSize = 0;
};

static inline bool
bridgePartitionGraphRoleMatches(DictionaryAttr entry,
                                codir::CodirAccessMode mode) {
  auto role = dyn_cast_or_null<StringAttr>(
      entry ? entry.get(codir::AttrNames::PartitionGraphKeys::Role)
            : Attribute{});
  if (!role)
    return true;
  if (codirAccessMayWrite(mode) &&
      role.getValue() == codir::AttrNames::LayoutGraphValues::RoleWrite)
    return true;
  if (codirAccessMayRead(mode) &&
      role.getValue() == codir::AttrNames::LayoutGraphValues::RoleRead)
    return true;
  return false;
}

static inline BridgePartitionGraphEvidence
readBridgePartitionGraphEvidence(const BridgePlan &plan) {
  BridgePartitionGraphEvidence evidence;
  codir::CodeletOp codelet = plan.seedCodelet;
  if (!codelet)
    return evidence;

  std::optional<int64_t> depArrayId =
      codir::getDepArrayId(codelet, plan.seedDepIndex);
  if (!depArrayId)
    return evidence;

  auto graph = dyn_cast_or_null<ArrayAttr>(
      codelet->getAttr(codir::AttrNames::PartitionGraph));
  if (!graph)
    return evidence;

  codir::CodirAccessMode mode =
      codir::getDepAccessMode(codelet, plan.seedDepIndex)
          .value_or(codir::CodirAccessMode::readwrite);
  for (Attribute attr : graph) {
    auto entry = dyn_cast<DictionaryAttr>(attr);
    if (!entry)
      continue;
    auto edgeClass = dyn_cast_or_null<StringAttr>(
        entry.get(codir::AttrNames::PartitionGraphKeys::EdgeClass));
    if (!edgeClass ||
        edgeClass.getValue() !=
            codir::AttrNames::PartitionGraphValues::EdgeLayoutMismatch)
      continue;
    if (auto layoutKind = dyn_cast_or_null<StringAttr>(
            entry.get(codir::AttrNames::PartitionGraphKeys::LayoutKind)))
      if (layoutKind.getValue() ==
          codir::AttrNames::PartitionGraphValues::OwnerBlock)
        continue;
    if (!bridgePartitionGraphRoleMatches(entry, mode))
      continue;
    auto muId = dyn_cast_or_null<IntegerAttr>(
        entry.get(codir::AttrNames::PartitionGraphKeys::MuId));
    if (!muId || muId.getInt() != *depArrayId)
      continue;

    if (std::optional<int64_t> blocks = readPositiveI64(
            entry, codir::AttrNames::PartitionGraphKeys::MuBlockCount))
      evidence.muBlockCount = std::max(evidence.muBlockCount, *blocks);
    if (std::optional<int64_t> group = readPositiveI64(
            entry, codir::AttrNames::PartitionGraphKeys::CuGroupSize))
      evidence.cuGroupSize = std::max(evidence.cuGroupSize, *group);
  }
  return evidence;
}

static inline int64_t getStaticFlatBlockCount(arts::DbAllocOp blockAlloc) {
  if (!blockAlloc)
    return 0;
  int64_t count = 1;
  for (Value size : blockAlloc.getSizes()) {
    std::optional<int64_t> constant = getPositiveStaticIndex(size);
    if (!constant)
      return 0;
    count = saturatingMul(count, *constant);
  }
  return count;
}

static inline SmallVector<int64_t, 4>
getStaticRowMajorBlockStrides(arts::DbAllocOp blockAlloc) {
  SmallVector<int64_t, 4> strides;
  if (!blockAlloc)
    return strides;
  strides.assign(blockAlloc.getSizes().size(), 0);
  int64_t stride = 1;
  for (int64_t dim = static_cast<int64_t>(blockAlloc.getSizes().size()) - 1;
       dim >= 0; --dim) {
    strides[dim] = stride;
    std::optional<int64_t> size =
        getPositiveStaticIndex(blockAlloc.getSizes()[dim]);
    if (!size) {
      strides.clear();
      return strides;
    }
    stride = saturatingMul(stride, *size);
  }
  return strides;
}

static inline bool
isStaticContiguousElementSlice(ArrayRef<int64_t> offsets,
                               ArrayRef<int64_t> sizes,
                               ArrayRef<int64_t> elementSizes) {
  if (offsets.size() != sizes.size() || offsets.size() != elementSizes.size() ||
      offsets.empty())
    return false;

  bool narrowerThanBlock = false;
  for (auto [offset, size, extent] :
       llvm::zip_equal(offsets, sizes, elementSizes)) {
    if (extent <= 0 || size <= 0 || offset < 0 || offset + size > extent)
      return false;
    narrowerThanBlock |= offset != 0 || size != extent;
  }
  if (!narrowerThanBlock)
    return false;

  for (size_t pivot = 0; pivot < sizes.size(); ++pivot) {
    bool contiguous = true;
    for (size_t dim = 0; dim < sizes.size(); ++dim) {
      if (dim < pivot) {
        contiguous &= sizes[dim] == 1;
        continue;
      }
      if (dim > pivot)
        contiguous &= offsets[dim] == 0 && sizes[dim] == elementSizes[dim];
    }
    if (contiguous)
      return true;
  }
  return false;
}

static inline std::optional<SmallVector<int64_t, 4>>
getStaticElementSizes(arts::DbAllocOp blockAlloc) {
  if (!blockAlloc)
    return std::nullopt;
  SmallVector<int64_t, 4> elementSizes;
  elementSizes.reserve(blockAlloc.getElementSizes().size());
  for (Value size : blockAlloc.getElementSizes()) {
    std::optional<int64_t> constant = getPositiveStaticIndex(size);
    if (!constant)
      return std::nullopt;
    elementSizes.push_back(*constant);
  }
  return elementSizes;
}

static inline BridgeOwnerMap buildBridgeOwnerMap(codir::CodeletOp codelet,
                                                 unsigned depIndex,
                                                 arts::DbAllocOp blockAlloc) {
  BridgeOwnerMap ownerMap;
  if (std::optional<SmallVector<unsigned, 4>> ownerDims =
          getCodirDepOwnerDims(codelet, depIndex))
    ownerMap.ownerDims.assign(ownerDims->begin(), ownerDims->end());
  if (blockAlloc) {
    if (std::optional<arts::DbOwnerMapPlan> plan =
            arts::getDbOwnerMapPlan(blockAlloc)) {
      ownerMap.ownerMapKind = plan->kind;
      ownerMap.ownerMapDims.assign(plan->dims.begin(), plan->dims.end());
    } else {
      ownerMap.ownerMapKind = arts::chooseDbOwnerMapKind(blockAlloc);
      if (std::optional<SmallVector<int64_t, 4>> dims =
              arts::getDbOwnerMapDimsFromPlan(blockAlloc))
        ownerMap.ownerMapDims.assign(dims->begin(), dims->end());
    }
    if (auto blockShape = readI64ArrayAttr(
            arts::getPlanPhysicalBlockShapeAttr(blockAlloc.getOperation())))
      ownerMap.physicalBlockShape.assign(blockShape->begin(),
                                         blockShape->end());
    ownerMap.flatBlockCount = getStaticFlatBlockCount(blockAlloc);
  }
  return ownerMap;
}

static inline bool
bridgePlanHasCollective(const BridgePlan &plan,
                        codir::CodirCollectiveKind collectiveKind) {
  return llvm::any_of(
      plan.participants, [collectiveKind](const HostBridgeParticipant &p) {
        return getFinalizedCodirDepCollectiveKind(p.codelet, p.depIndex) ==
               collectiveKind;
      });
}

static inline bool bridgePlanHasHaloStencilStorage(const BridgePlan &plan) {
  return llvm::any_of(plan.participants, [](const HostBridgeParticipant &p) {
    return codirDepUsesHaloStencilStorage(p.codelet, p.depIndex);
  });
}

static inline bool
bridgePlanHasOwnerLocalReadOnlyStencilStorage(const BridgePlan &plan) {
  bool foundOwnerLocalStencilRead = false;
  for (const HostBridgeParticipant &participant : plan.participants) {
    codir::CodeletOp participantCodelet = participant.codelet;
    if (!participantCodelet ||
        participant.depIndex >= participantCodelet.getDeps().size())
      return false;

    if (!codirAccessMayRead(participant.mode)) {
      if (codirAccessMayWrite(participant.mode))
        return false;
      continue;
    }
    if (codirAccessMayWrite(participant.mode))
      return false;

    // Only compute-block reads that carry a committed access window are
    // owner-local-stencil candidates; the access-window offsets are the
    // structural stencil evidence the owner-local check below proves against.
    if (!codirDepRequiresComputeBlockStorage(participantCodelet,
                                             participant.depIndex) ||
        !participantCodelet.getAccessMinOffsetsAttr() ||
        !participantCodelet.getAccessMaxOffsetsAttr())
      continue;

    if (codirDepUsesHaloStencilStorage(participantCodelet,
                                       participant.depIndex))
      return false;

    if (!codirDepUsesOwnerLocalStencilStorage(participantCodelet,
                                              participant.depIndex))
      return false;
    foundOwnerLocalStencilRead = true;
  }
  return foundOwnerLocalStencilRead;
}

static inline bool
bridgePlanHasUnsupportedCollective(const BridgePlan &plan,
                                   codir::CodirCollectiveKind &collectiveKind) {
  for (const HostBridgeParticipant &participant : plan.participants) {
    codir::CodirCollectiveKind kind = getFinalizedCodirDepCollectiveKind(
        participant.codelet, participant.depIndex);
    switch (kind) {
    case codir::CodirCollectiveKind::none:
    case codir::CodirCollectiveKind::all_gather:
    case codir::CodirCollectiveKind::reduce_scatter:
    case codir::CodirCollectiveKind::halo:
      break;
    case codir::CodirCollectiveKind::all_to_all:
    case codir::CodirCollectiveKind::allreduce:
    case codir::CodirCollectiveKind::broadcast:
      collectiveKind = kind;
      return true;
    }
  }
  return false;
}

static inline StringRef
getCollectiveKindName(codir::CodirCollectiveKind collectiveKind) {
  switch (collectiveKind) {
  case codir::CodirCollectiveKind::none:
    return "none";
  case codir::CodirCollectiveKind::all_gather:
    return "all_gather";
  case codir::CodirCollectiveKind::all_to_all:
    return "all_to_all";
  case codir::CodirCollectiveKind::reduce_scatter:
    return "reduce_scatter";
  case codir::CodirCollectiveKind::allreduce:
    return "allreduce";
  case codir::CodirCollectiveKind::broadcast:
    return "broadcast";
  case codir::CodirCollectiveKind::halo:
    return "halo";
  }
  return "unknown";
}

static inline BridgePlan
buildBridgePlan(codir::CodeletOp seedCodelet, unsigned seedDepIndex,
                Value hostView, arts::DbAllocOp sourceAlloc,
                arts::DbAllocOp destinationAlloc,
                ArrayRef<HostBridgeParticipant> participants,
                ArrayRef<Operation *> readObservationAnchors, bool needsCopyIn,
                bool needsCopyOut, bool hasInterNodeRuntime) {
  BridgePlan plan;
  plan.seedCodelet = seedCodelet;
  plan.seedDepIndex = seedDepIndex;
  plan.collectiveKind =
      getFinalizedCodirDepCollectiveKind(seedCodelet, seedDepIndex);
  plan.participants.assign(participants.begin(), participants.end());
  plan.readObservationAnchors.assign(readObservationAnchors.begin(),
                                     readObservationAnchors.end());
  plan.hostView = hostView;
  plan.sourceAlloc = sourceAlloc;
  plan.destinationAlloc = destinationAlloc;
  plan.needsCopyIn = needsCopyIn;
  plan.needsCopyOut = needsCopyOut;
  plan.hasInterNodeRuntime = hasInterNodeRuntime;
  plan.ownerMap = buildBridgeOwnerMap(
      seedCodelet, seedDepIndex, sourceAlloc ? sourceAlloc : destinationAlloc);

  if (plan.collectiveKind == codir::CodirCollectiveKind::all_gather ||
      (plan.collectiveKind == codir::CodirCollectiveKind::reduce_scatter &&
       codirDepUsesBlockNativeSettle(seedCodelet, seedDepIndex)))
    plan.replicationPolicy = BridgeReplicationPolicy::replicated_local;

  if (plan.collectiveKind == codir::CodirCollectiveKind::all_gather ||
      plan.collectiveKind == codir::CodirCollectiveKind::reduce_scatter)
    plan.routingMode = BridgeRoutingMode::node_ordinal;
  else if (plan.destinationAlloc)
    plan.routingMode = BridgeRoutingMode::block_ordinal;

  if (seedCodelet) {
    if (auto factor = seedCodelet.getPartialReductionSplitFactorAttr())
      if (factor.getInt() > 0)
        plan.tileCount = static_cast<unsigned>(factor.getInt());
  }
  return plan;
}

static inline arts::DbAllocOp
getBridgePlanSizingAlloc(const BridgePlan &plan,
                         BridgeWorkloadKind workloadKind) {
  switch (workloadKind) {
  case BridgeWorkloadKind::host_to_block:
    return plan.destinationAlloc ? plan.destinationAlloc : plan.sourceAlloc;
  case BridgeWorkloadKind::block_to_host:
  case BridgeWorkloadKind::all_gather:
  case BridgeWorkloadKind::summing_settle:
  case BridgeWorkloadKind::halo:
    return plan.sourceAlloc ? plan.sourceAlloc : plan.destinationAlloc;
  }
  return plan.sourceAlloc ? plan.sourceAlloc : plan.destinationAlloc;
}

static inline BridgeRoutingMode
getBridgeWorkloadRoutingMode(const BridgePlan &plan,
                             BridgeWorkloadKind workloadKind,
                             bool crossNodeGather = false) {
  switch (workloadKind) {
  case BridgeWorkloadKind::host_to_block:
  case BridgeWorkloadKind::halo:
    return BridgeRoutingMode::block_ordinal;
  case BridgeWorkloadKind::block_to_host:
    return crossNodeGather ? BridgeRoutingMode::node_ordinal
                           : BridgeRoutingMode::current_node;
  case BridgeWorkloadKind::all_gather:
  case BridgeWorkloadKind::summing_settle:
    return BridgeRoutingMode::node_ordinal;
  }
  return plan.routingMode;
}

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

  codir::CodeletOp codelet = plan.seedCodelet;
  BridgePartitionGraphEvidence graphEvidence =
      readBridgePartitionGraphEvidence(plan);
  int64_t muBlocks = graphEvidence.muBlockCount;
  if (muBlocks > 0)
    muBlocks = std::min<int64_t>(*blockCount, muBlocks);

  // Keep MU block granularity independent from copy EDT granularity: the DBs
  // stay per block, while bridge EDTs may cover block ranges when enough CU
  // parallelism remains exposed.
  int64_t concurrencyFloor = readPartitionScoreConcurrencyFloor(codelet);
  if (concurrencyFloor <= 0 && muBlocks > 0)
    concurrencyFloor = muBlocks;
  if (concurrencyFloor > 0) {
    int64_t desiredTasks = std::min<int64_t>(*blockCount, concurrencyFloor);
    int64_t maxGroupForConcurrency =
        *blockCount / std::max<int64_t>(1, desiredTasks);
    desired = std::min(desired, std::max<int64_t>(1, maxGroupForConcurrency));
  }

  int64_t authoredGroupSize = graphEvidence.cuGroupSize;
  if (authoredGroupSize > 0)
    desired = std::min<int64_t>(desired, authoredGroupSize);

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

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEWORKGROUPS_H
