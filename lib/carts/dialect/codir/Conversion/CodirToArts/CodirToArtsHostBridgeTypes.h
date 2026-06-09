///==========================================================================///
/// File: CodirToArtsHostBridgeTypes.h
///
/// Host bridge participant and bridge-plan records plus cross-helper forwards.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGETYPES_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGETYPES_H

#include "CodirToArtsCommonMaterialization.h"

namespace {

struct HostBridgeParticipant {
  codir::CodeletOp codelet;
  unsigned depIndex = 0;
  codir::CodirAccessMode mode = codir::CodirAccessMode::readwrite;
  // OpOperand to repoint at the block DB. When the codelet consumes the MU
  // through a view op (memref.subview etc.), this is the view's source operand
  // so the block DB lands UNDER the view (mirroring the normal lowerMuAlloc
  // replaceAllUsesWith path). When null, the codelet dep operand itself is
  // repointed.
  OpOperand *repointOperand = nullptr;
};

enum class BridgeWorkloadKind {
  host_to_block,
  block_to_host,
  all_gather,
  summing_settle,
  halo,
};

enum class BridgeReplicationPolicy {
  single_home,
  replicated_local,
};

enum class BridgeRoutingMode {
  current_node,
  block_ordinal,
  node_ordinal,
};

struct BridgeOwnerMap {
  SmallVector<unsigned, 4> ownerDims;
  SmallVector<int64_t, 4> ownerMapDims;
  SmallVector<int64_t, 4> physicalBlockShape;
  std::optional<arts::DbOwnerMapKind> ownerMapKind;
  int64_t flatBlockCount = 0;
};

struct BridgeWorkloadEvidence {
  BridgeWorkloadKind workloadKind = BridgeWorkloadKind::block_to_host;
  BridgeRoutingMode routingMode = BridgeRoutingMode::current_node;
  bool readOnlySource = false;
  bool copyLike = false;
  bool haloExchange = false;
  bool preservesPerBlockDbGrain = true;
  bool mayGroupAdjacentBlocks = false;
};

struct BridgeWorkGroupPlan {
  BridgeWorkloadEvidence evidence;
  int64_t staticBlockCount = 0;
  int64_t groupSize = 1;

  bool groupsBlockRanges() const { return groupSize > 1; }
};

struct BridgePlan {
  codir::CodeletOp seedCodelet;
  unsigned seedDepIndex = 0;
  codir::CodirCollectiveKind collectiveKind = codir::CodirCollectiveKind::none;
  SmallVector<HostBridgeParticipant> participants;
  SmallVector<Operation *> readObservationAnchors;
  BridgeOwnerMap ownerMap;
  Value hostView;
  arts::DbAllocOp sourceAlloc;
  arts::DbAllocOp destinationAlloc;
  bool needsCopyIn = false;
  bool needsCopyOut = false;
  bool hasInterNodeRuntime = false;
  BridgeReplicationPolicy replicationPolicy =
      BridgeReplicationPolicy::single_home;
  BridgeRoutingMode routingMode = BridgeRoutingMode::current_node;
  unsigned tileCount = 0;
  int64_t groupSize = 1;
};

struct HostBridgeUseCollection {
  SmallVector<HostBridgeParticipant> participants;
  SmallVector<Operation *> readObservationAnchors;
};

static inline BridgeWorkGroupPlan
planBridgeWorkGroups(const BridgePlan &plan, BridgeWorkloadKind workloadKind,
                     bool crossNodeGather = false);

static inline arts::ArtsLaunchPolicy resolveBridgeBlockOrdinalLaunchPolicy(
    ModuleOp module, const BridgePlan *plan, arts::DbAllocOp blockAlloc,
    Value blockOrdinal, OpBuilder &builder, Location loc);

static inline std::optional<unsigned>
getCodeletDepOperandIndex(codir::CodeletOp codelet, OpOperand &use);

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGETYPES_H
