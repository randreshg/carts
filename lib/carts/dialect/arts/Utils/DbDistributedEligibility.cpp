///==========================================================================///
/// File: DbDistributedEligibility.cpp
///
/// Eligibility analysis for distributed DB ownership marking.
///==========================================================================///

#include "carts/dialect/arts/Utils/DbDistributedEligibility.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/EdtUtils.h"
#include "carts/dialect/arts/Utils/LoweringFactUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

static bool hasMultipleAllocationBlocks(DbAllocOp alloc) {
  auto sizes = alloc.getSizes();
  if (sizes.empty())
    return false;
  if (sizes.size() > 1)
    return true;

  int64_t blockCount = 0;
  if (ValueAnalysis::getConstantIndex(sizes.front(), blockCount))
    return blockCount > 1;
  return true;
}

static bool hasSupportedAllocationShape(DbAllocOp alloc) {
  /// Distributed ownership currently targets ranked memref allocations.
  /// Keep scalars local to avoid over-marking small or temporary DBs. Rank-1
  /// vectors are eligible only when SDE authored an explicit block plan;
  /// otherwise a large vector still looks like an undifferentiated aggregate.
  if (alloc.getElementSizes().empty())
    return false;
  if (alloc.getElementSizes().size() == 1 &&
      (!getPlanOwnerDimsAttr(alloc.getOperation()) ||
       !getPlanPhysicalBlockShapeAttr(alloc.getOperation())))
    return false;

  auto isPositiveOrDynamic = [](Value value) -> bool {
    int64_t constant = 0;
    if (!ValueAnalysis::getConstantIndex(
            ValueAnalysis::stripNumericCasts(value), constant))
      return true;
    return constant > 0;
  };

  for (Value size : alloc.getSizes())
    if (!isPositiveOrDynamic(size))
      return false;
  for (Value size : alloc.getElementSizes())
    if (!isPositiveOrDynamic(size))
      return false;
  return true;
}

static bool hasOwnerMapSeedPlan(DbAllocOp alloc) {
  return hasArtsDbPhysicalLayoutPlan(alloc.getOperation());
}

static bool hasSupportedOwnerMapSeedPlan(DbAllocOp alloc) {
  if (!hasArtsDbPhysicalLayoutPlan(alloc.getOperation()))
    return false;

  auto ownerBlockShape = getDbOwnerBlockShapeFromPlan(alloc);
  if (!ownerBlockShape || ownerBlockShape->empty())
    return false;

  if (auto kind = getEdtDistributionKind(alloc.getOperation());
      kind && *kind == EdtDistributionKind::block_cyclic)
    return !alloc.getSizes().empty();

  auto dbOwnerDims = getDbOwnerMapDimsFromPlan(alloc);
  return dbOwnerDims &&
         ownerDimsAddressDbRank(*dbOwnerDims, alloc.getSizes().size());
}

static bool hasOnlyAllowedHandleUsers(Value rootHandle) {
  if (!rootHandle)
    return false;

  SmallVector<Value, 8> worklist{rootHandle};
  llvm::DenseSet<Value> visitedValues;

  auto enqueue = [&](Value value) {
    if (value)
      worklist.push_back(value);
  };

  auto isMemoryUserInsideEdt = [](Operation *user) {
    return user && static_cast<bool>(user->getParentOfType<EdtOp>());
  };

  while (!worklist.empty()) {
    Value value = worklist.pop_back_val();
    if (!visitedValues.insert(value).second)
      continue;

    for (Operation *user : value.getUsers()) {
      if (isa<DbAcquireOp, DbFreeOp, DbReleaseOp, EdtOp>(user))
        continue;

      if (auto castOp = dyn_cast<memref::CastOp>(user)) {
        if (castOp.getSource() == value)
          enqueue(castOp.getResult());
        continue;
      }

      if (auto subview = dyn_cast<memref::SubViewOp>(user)) {
        if (subview.getSource() == value)
          enqueue(subview.getResult());
        continue;
      }

      if (auto reinterpret = dyn_cast<memref::ReinterpretCastOp>(user)) {
        if (reinterpret.getSource() == value)
          enqueue(reinterpret.getResult());
        continue;
      }

      if (auto view = dyn_cast<memref::ViewOp>(user)) {
        if (view.getSource() == value)
          enqueue(view.getResult());
        continue;
      }

      if (auto dbRefOp = dyn_cast<DbRefOp>(user)) {
        if (dbRefOp.getSource() == value)
          enqueue(dbRefOp.getResult());
        continue;
      }

      if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(user)) {
        if (llvm::is_contained(unrealized.getInputs(), value))
          for (Value result : unrealized->getResults())
            enqueue(result);
        continue;
      }

      if (auto access = DbUtils::getMemoryAccessInfo(user)) {
        if (access->memref == value && isMemoryUserInsideEdt(user))
          continue;
        return false;
      }

      if (isa<memref::DimOp, memref::DeallocOp>(user))
        continue;

      if (isMemoryEffectFree(user)) {
        for (Value result : user->getResults())
          enqueue(result);
        continue;
      }

      return false;
    }
  }

  return true;
}

/// Returns true when an acquire node carries dependency-scoped stencil facts.
/// The parent EDT's aggregate stencil pattern is not enough: one stencil task
/// can contain a plain read-only input, a writer, and a halo reader, and ARTS
/// must not smear the halo reader's shape over every dependency.
static bool isStencilAcquire(DbAcquireOp acquireOp) {
  if (!acquireOp)
    return false;

  if (auto facts = resolveAcquireFacts(acquireOp);
      facts && facts->hasExplicitStencilFacts())
    return true;

  if (auto depPattern = getDepPattern(acquireOp.getOperation()))
    if (isStencilFamilyDepPattern(*depPattern))
      return true;

  return false;
}

/// Summary of facts collected in a single pass over all acquire nodes
/// belonging to a given DB allocation.
struct EligibilityFacts {
  bool hasInternodeEdtUse = false;
  bool hasStencilReadInternodeUse = false;
  bool hasInternodeWriteUse = false;
  bool allAcquiresReadOnly = true;
  bool isStencilFamily = false;
  bool allHaveEdtAcquireUsers = true;
  /// True only when every internode read-only stencil acquire of |alloc|
  /// carries the `replicatedRead` storage-view marker authored by ARTS
  /// storage planning. This is the facts that says "the codelet wants
  /// the full DB replicated", not just "the access happens to be RO".
  bool allInternodeStencilReadsAreReplicated = true;
};

/// Performs a single traversal of the acquire ops for |alloc|, collecting
/// every fact needed by evaluateDistributedDbEligibility.  Returns std::nullopt
/// when the parent function cannot be resolved.
static std::optional<EligibilityFacts>
collectEligibilityFacts(DbAllocOp alloc) {
  func::FuncOp parentFunc = alloc->getParentOfType<func::FuncOp>();
  if (!parentFunc)
    return std::nullopt;

  EligibilityFacts facts;

  parentFunc.walk([&](DbAcquireOp acquire) {
    if (!acquire)
      return;
    auto root = dyn_cast_or_null<DbAllocOp>(
        DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr()));
    if (root != alloc)
      return;

    /// allHaveEdtAcquireUsers (universal)
    auto [edt, blockArg] = EdtUtils::getBlockArgumentForAcquire(acquire);
    (void)blockArg;
    if (!edt)
      facts.allHaveEdtAcquireUsers = false;

    /// allAcquiresReadOnly (universal)
    bool readOnly = acquire.getMode() == ArtsMode::in;
    if (!readOnly)
      facts.allAcquiresReadOnly = false;

    /// isStencilFamily (existential)
    bool stencil = isStencilAcquire(acquire);
    if (stencil)
      facts.isStencilFamily = true;

    /// hasInternodeEdtUse (existential)
    bool internode = edt && edt.getConcurrency() == EdtConcurrency::internode;
    if (internode)
      facts.hasInternodeEdtUse = true;
    /// Bridge-fill EDTs (carrying the `storageBridgeCopy` marker) are
    /// orchestration machinery that materializes the host->compute bridge,
    /// not user computation. Their writes must not block the halo-backed
    /// bridge eligibility path; otherwise every bridged stencil DB looks
    /// "internode-written" before the user codelet ever runs.
    bool isBridgeFill = edt && edt.getStorageBridgeCopyAttr();
    if (internode && !readOnly && !isBridgeFill)
      facts.hasInternodeWriteUse = true;

    /// hasStencilReadInternodeUse (existential)
    if (internode && readOnly && stencil) {
      facts.hasStencilReadInternodeUse = true;
      /// allInternodeStencilReadsAreReplicated (universal, over the same
      /// subset). The codelet expresses storage-view intent via the
      /// `replicatedRead` marker on its acquire; respect that facts.
      if (!acquire.getReplicatedReadAttr())
        facts.allInternodeStencilReadsAreReplicated = false;
    }
  });

  return facts;
}

static bool hasReadOnlyAfterInitAttr(DbAllocOp alloc) {
  return alloc.getReadOnlyAfterInit().value_or(false);
}

static bool isHostWholeToComputeBlockBridge(DbAllocOp alloc) {
  if (!alloc)
    return false;
  auto bridge = alloc.getStorageBridgeAttr();
  return bridge &&
         bridge.getValue() == StorageBridge::host_whole_to_compute_block;
}

static bool hasBridgeHaloFacts(DbAllocOp alloc) {
  if (!alloc)
    return false;
  if (getPlanHaloShapeAttr(alloc.getOperation()))
    return true;
  return alloc->hasAttr(alloc.getStencilSupportedBlockHaloAttrName());
}

static bool isHaloBackedHostBridge(DbAllocOp alloc) {
  if (!isHostWholeToComputeBlockBridge(alloc) || !hasBridgeHaloFacts(alloc))
    return false;
  std::optional<PartitionMode> mode = getPartitionMode(alloc.getOperation());
  return mode &&
         (*mode == PartitionMode::block || *mode == PartitionMode::stencil);
}

} // namespace

const char *
mlir::carts::arts::toString(DistributedDbEligibilityRejectReason reason) {
  switch (reason) {
  case DistributedDbEligibilityRejectReason::None:
    return "eligible";
  case DistributedDbEligibilityRejectReason::NestedInEdt:
    return "nested_in_edt";
  case DistributedDbEligibilityRejectReason::GlobalAllocType:
    return "global_alloc";
  case DistributedDbEligibilityRejectReason::SingleBlock:
    return "single_block";
  case DistributedDbEligibilityRejectReason::UnsupportedShape:
    return "unsupported_shape";
  case DistributedDbEligibilityRejectReason::MissingOwnerMapPlan:
    return "missing_owner_map_plan";
  case DistributedDbEligibilityRejectReason::UnsupportedOwnerMapShape:
    return "unsupported_owner_map_shape";
  case DistributedDbEligibilityRejectReason::StencilReadInternodeUse:
    return "stencil_read_internode_use";
  case DistributedDbEligibilityRejectReason::UnsupportedPtrUsers:
    return "unsupported_ptr_users";
  case DistributedDbEligibilityRejectReason::UnsupportedGuidUsers:
    return "unsupported_guid_users";
  case DistributedDbEligibilityRejectReason::NonEdtAcquireUse:
    return "non_edt_acquire_use";
  case DistributedDbEligibilityRejectReason::NoInternodeEdtUse:
    return "no_internode_edt_use";
  case DistributedDbEligibilityRejectReason::PerBlockReplicated:
    return "per_block_replicated";
  }
  return "unknown";
}

DistributedDbEligibilityResult
mlir::carts::arts::evaluateDistributedDbEligibility(DbAllocOp alloc) {
  if (!alloc)
    return {false, DistributedDbEligibilityRejectReason::UnsupportedShape};
  // Per-block all-gather replica: keep it REPLICATED (every block on every
  // node) so each node assembles its own full gathered set. Marking it
  // distributed would block-scatter the gathered blocks back across nodes,
  // defeating the all-gather. The single-writer property is preserved per
  // block-GUID regardless of distribution marking.
  if (alloc.getPerBlockReplicated().value_or(false))
    return {false, DistributedDbEligibilityRejectReason::PerBlockReplicated};
  if (alloc->getParentOfType<EdtOp>())
    return {false, DistributedDbEligibilityRejectReason::NestedInEdt};
  if (alloc.getAllocType() == DbAllocType::global)
    return {false, DistributedDbEligibilityRejectReason::GlobalAllocType};
  if (!hasMultipleAllocationBlocks(alloc))
    return {false, DistributedDbEligibilityRejectReason::SingleBlock};
  if (!hasSupportedAllocationShape(alloc))
    return {false, DistributedDbEligibilityRejectReason::UnsupportedShape};
  if (!hasOwnerMapSeedPlan(alloc))
    return {false, DistributedDbEligibilityRejectReason::MissingOwnerMapPlan};
  if (!hasSupportedOwnerMapSeedPlan(alloc))
    return {false,
            DistributedDbEligibilityRejectReason::UnsupportedOwnerMapShape};
  if (!hasOnlyAllowedHandleUsers(alloc.getPtr()))
    return {false, DistributedDbEligibilityRejectReason::UnsupportedPtrUsers};
  if (!hasOnlyAllowedHandleUsers(alloc.getGuid()))
    return {false, DistributedDbEligibilityRejectReason::UnsupportedGuidUsers};

  /// Single-pass collection of all acquire-node facts.
  auto maybeFacts = collectEligibilityFacts(alloc);
  if (!maybeFacts)
    return {false, DistributedDbEligibilityRejectReason::NonEdtAcquireUse};

  const EligibilityFacts &facts = *maybeFacts;

  if (!facts.allHaveEdtAcquireUsers)
    return {false, DistributedDbEligibilityRejectReason::NonEdtAcquireUse};
  if (!facts.hasInternodeEdtUse)
    return {false, DistributedDbEligibilityRejectReason::NoInternodeEdtUse};
  if (facts.hasStencilReadInternodeUse) {
    /// A per-block single-writer stencil DB is distributable even though it is
    /// both stencil-read and written internode: the explicit halo exchange
    /// keeps one writer frontier per block.
    if (alloc.getPerBlockSingleWriterStencil().value_or(false))
      return {true, DistributedDbEligibilityRejectReason::None};
    /// ARTS storage planning marks acquires with `replicatedRead` when the
    /// codelet wants the whole DB on every node. Without that marker, the
    /// codelet asked for a block view and ARTS must not unilaterally replicate.
    bool readOnly =
        facts.allAcquiresReadOnly || hasReadOnlyAfterInitAttr(alloc);
    if (readOnly && facts.isStencilFamily &&
        facts.allInternodeStencilReadsAreReplicated)
      return {true, DistributedDbEligibilityRejectReason::None,
              EdtDistributionKind::replicated};
    if (!facts.hasInternodeWriteUse && isHaloBackedHostBridge(alloc))
      return {true, DistributedDbEligibilityRejectReason::None};
    return {false,
            DistributedDbEligibilityRejectReason::StencilReadInternodeUse};
  }
  return {true, DistributedDbEligibilityRejectReason::None};
}
