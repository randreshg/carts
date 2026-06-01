///==========================================================================///
/// File: StoragePlanning.cpp
///
/// CODIR-owned dependency storage planning before ARTS materialization.
///==========================================================================///

#include "carts/dialect/codir/Transforms/Passes.h"

#include "carts/dialect/codir/Utils/CodeletABIUtils.h"
#include "carts/dialect/codir/Utils/CodirAccessTraceUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallPtrSet.h"

#include <limits>

namespace mlir::carts::codir {
#define GEN_PASS_DEF_STORAGEPLANNING
#include "carts/dialect/codir/Transforms/Passes.h.inc"
} // namespace mlir::carts::codir

using namespace mlir;
using namespace mlir::carts;

namespace {

static constexpr int64_t kMaxPhaseRedistributionBridgeElements =
    16LL * 1024LL * 1024LL;

static Value stripStorageViews(Value value) {
  for (;;) {
    Operation *def = value ? value.getDefiningOp() : nullptr;
    if (auto subview = dyn_cast_or_null<memref::SubViewOp>(def)) {
      value = subview.getSource();
      continue;
    }
    if (auto cast = dyn_cast_or_null<memref::CastOp>(def)) {
      value = cast.getSource();
      continue;
    }
    return value;
  }
}

static bool isStorageView(Value value) {
  return isa_and_nonnull<memref::SubViewOp>(value ? value.getDefiningOp()
                                                  : nullptr);
}

static bool hasTileOwnerSlicePlan(codir::CodeletOp codelet) {
  return codelet && codelet.getTileShapeAttr() &&
         codelet.getTileOwnerDimsAttr();
}

static std::optional<SmallVector<unsigned, 4>>
getTileOwnerDims(codir::CodeletOp codelet) {
  if (!hasTileOwnerSlicePlan(codelet))
    return std::nullopt;
  std::optional<SmallVector<int64_t, 4>> rawDims =
      readI64ArrayAttr(codelet.getTileOwnerDimsAttr());
  if (!rawDims || rawDims->empty())
    return std::nullopt;

  SmallVector<unsigned, 4> dims;
  dims.reserve(rawDims->size());
  for (int64_t dim : *rawDims) {
    if (dim < 0)
      return std::nullopt;
    dims.push_back(static_cast<unsigned>(dim));
  }
  return dims;
}

static std::optional<unsigned> getSingleTileOwnerDim(codir::CodeletOp codelet) {
  std::optional<SmallVector<unsigned, 4>> ownerDims = getTileOwnerDims(codelet);
  if (!ownerDims || ownerDims->size() != 1)
    return std::nullopt;
  return ownerDims->front();
}

static bool isStencilPattern(codir::CodirPattern pattern) {
  switch (pattern) {
  case codir::CodirPattern::stencil_tiling_nd:
  case codir::CodirPattern::cross_dim_stencil_3d:
  case codir::CodirPattern::higher_order_stencil:
  case codir::CodirPattern::wavefront_2d:
  case codir::CodirPattern::alternating_buffer_stencil:
    return true;
  default:
    return false;
  }
}

static bool isStencilCodelet(codir::CodeletOp codelet) {
  auto pattern = codelet ? codelet.getPatternAttr() : nullptr;
  return pattern && isStencilPattern(pattern.getValue());
}

static SmallVector<Value, 4> getOwnerBaseArguments(codir::CodeletOp codelet,
                                                   unsigned ownerDimCount) {
  SmallVector<Value, 4> bases;
  if (!codelet || codelet.getBody().empty() || ownerDimCount == 0 ||
      codelet.getParams().size() < ownerDimCount)
    return bases;

  Block &body = codelet.getBody().front();
  unsigned depCount = codelet.getDeps().size();
  unsigned paramCount = codelet.getParams().size();
  if (body.getNumArguments() < depCount + paramCount)
    return bases;

  bases.reserve(ownerDimCount);
  unsigned firstOwnerParam = paramCount - ownerDimCount;
  for (unsigned slot = 0; slot < ownerDimCount; ++slot)
    bases.push_back(body.getArgument(depCount + firstOwnerParam + slot));
  return bases;
}

static std::optional<SmallVector<unsigned, 4>>
inferDepOwnerAccessDims(codir::CodeletOp codelet, unsigned depIndex) {
  if (!codelet || codelet.getBody().empty() ||
      depIndex >= codelet.getDeps().size())
    return std::nullopt;

  Block &body = codelet.getBody().front();
  if (depIndex >= body.getNumArguments())
    return std::nullopt;

  Value depArg = body.getArgument(depIndex);
  auto depType = dyn_cast<MemRefType>(depArg.getType());
  if (!depType || depType.getRank() == 0)
    return std::nullopt;

  std::optional<SmallVector<unsigned, 4>> tileOwnerDims =
      getTileOwnerDims(codelet);
  if (!tileOwnerDims)
    return std::nullopt;
  SmallVector<Value, 4> ownerBases =
      getOwnerBaseArguments(codelet, tileOwnerDims->size());
  if (ownerBases.size() != tileOwnerDims->size())
    return std::nullopt;

  bool sawDepAccess = false;
  bool rejected = false;
  std::optional<SmallVector<unsigned, 4>> selectedDims;
  body.walk([&](Operation *op) {
    if (rejected)
      return WalkResult::interrupt();

    auto access = getCodirMemoryAccessInfo(op);
    if (!access)
      return WalkResult::advance();

    SmallVector<unsigned, 4> accessDims;
    bool sawRootedAccess = false;
    for (Value ownerBase : ownerBases) {
      CodirAccessOwnerDims traced = traceCodirAccessToRoot(
          access->memref, access->indices, depArg, ownerBase);
      if (traced.status == CodirAccessTraceStatus::NotRooted)
        continue;
      sawRootedAccess = true;
      if (traced.status == CodirAccessTraceStatus::Unsupported ||
          traced.ownerDims.size() > 1) {
        rejected = true;
        return WalkResult::interrupt();
      }
      if (!traced.ownerDims.empty())
        accessDims.push_back(traced.ownerDims.front());
    }
    if (!sawRootedAccess)
      return WalkResult::advance();
    sawDepAccess = true;
    if (accessDims.empty()) {
      rejected = true;
      return WalkResult::interrupt();
    }
    if (selectedDims && *selectedDims != accessDims) {
      rejected = true;
      return WalkResult::interrupt();
    }
    selectedDims = std::move(accessDims);
    return WalkResult::advance();
  });

  if (!sawDepAccess || rejected)
    return std::nullopt;
  return selectedDims;
}

static std::optional<SmallVector<unsigned, 4>>
getDepOwnerDims(codir::CodeletOp codelet, unsigned depIndex) {
  std::optional<SmallVector<unsigned, 4>> tileOwnerDims =
      getTileOwnerDims(codelet);
  if (isStencilCodelet(codelet) && tileOwnerDims && tileOwnerDims->size() > 1)
    return tileOwnerDims;
  if (std::optional<SmallVector<unsigned, 4>> inferred =
          inferDepOwnerAccessDims(codelet, depIndex))
    return inferred;
  return tileOwnerDims;
}

static bool depAccessesStayWithinSingleOwnerSlice(codir::CodeletOp codelet,
                                                  unsigned depIndex) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty() || !codelet || codelet.getBody().empty())
    return false;

  Block &body = codelet.getBody().front();
  if (depIndex >= codelet.getDeps().size() ||
      depIndex >= body.getNumArguments())
    return false;

  Value depArg = body.getArgument(depIndex);
  auto depType = dyn_cast<MemRefType>(depArg.getType());
  if (!depType || depType.getRank() == 0)
    return false;
  for (unsigned ownerDim : *ownerDims)
    if (ownerDim >= depType.getRank())
      return false;

  std::optional<SmallVector<unsigned, 4>> tileOwnerDims =
      getTileOwnerDims(codelet);
  if (!tileOwnerDims)
    return false;
  SmallVector<Value, 4> ownerBases =
      getOwnerBaseArguments(codelet, tileOwnerDims->size());
  if (ownerBases.size() != tileOwnerDims->size())
    return false;

  bool sawDepAccess = false;
  bool rejected = false;
  body.walk([&](Operation *op) {
    if (rejected)
      return WalkResult::interrupt();

    auto access = getCodirMemoryAccessInfo(op);
    if (!access)
      return WalkResult::advance();

    SmallVector<unsigned, 4> accessDims;
    bool sawRootedAccess = false;
    for (Value ownerBase : ownerBases) {
      CodirAccessOwnerDims traced = traceCodirAccessToRoot(
          access->memref, access->indices, depArg, ownerBase);
      if (traced.status == CodirAccessTraceStatus::NotRooted)
        continue;
      sawRootedAccess = true;
      if (traced.status == CodirAccessTraceStatus::Unsupported ||
          traced.ownerDims.size() > 1) {
        rejected = true;
        return WalkResult::interrupt();
      }
      if (!traced.ownerDims.empty())
        accessDims.push_back(traced.ownerDims.front());
    }
    if (!sawRootedAccess)
      return WalkResult::advance();
    sawDepAccess = true;
    if (accessDims != *ownerDims) {
      rejected = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });

  return sawDepAccess && !rejected;
}

static bool hasHostMemrefAccessOutsideCodelet(Value root) {
  if (!root)
    return false;

  SmallVector<Value, 8> worklist{root};
  llvm::SmallPtrSet<Value, 16> visited;
  while (!worklist.empty()) {
    Value current = worklist.pop_back_val();
    if (!current || !visited.insert(current).second)
      continue;

    for (Operation *user : llvm::make_early_inc_range(current.getUsers())) {
      if (!user || user->getParentOfType<codir::CodeletOp>())
        continue;
      if (isa<memref::DeallocOp, memref::DimOp>(user))
        continue;
      if (isa<memref::LoadOp, memref::StoreOp>(user))
        return true;
      if (codir::isMemrefForwardingOp(user))
        for (Value result : user->getResults())
          if (isa<MemRefType>(result.getType()))
            worklist.push_back(result);
    }
  }

  return false;
}

static std::optional<int64_t> getStaticLogicalElementCount(Value value) {
  Value root = stripStorageViews(value);
  if (!root)
    return std::nullopt;
  auto memrefType = dyn_cast<MemRefType>(root.getType());
  if (!memrefType || !memrefType.hasStaticShape())
    return std::nullopt;

  int64_t count = 1;
  for (int64_t dim : memrefType.getShape()) {
    if (dim < 0 || count > std::numeric_limits<int64_t>::max() / dim)
      return std::nullopt;
    count *= dim;
  }
  return count;
}

static bool isPerDependencyRedistribution(codir::CodeletOp codelet,
                                          unsigned depIndex) {
  std::optional<SmallVector<unsigned, 4>> depOwnerDims =
      inferDepOwnerAccessDims(codelet, depIndex);
  std::optional<SmallVector<unsigned, 4>> codeletOwnerDims =
      getTileOwnerDims(codelet);
  return depOwnerDims && codeletOwnerDims && *depOwnerDims != *codeletOwnerDims;
}

static bool isMatmulCodelet(codir::CodeletOp codelet) {
  auto pattern = codelet ? codelet.getPatternAttr() : nullptr;
  return pattern && pattern.getValue() == codir::CodirPattern::matmul;
}

static std::optional<unsigned>
getCodeletDepOperandIndex(codir::CodeletOp codelet, OpOperand &use) {
  if (!codelet)
    return std::nullopt;
  unsigned operandIndex = use.getOperandNumber();
  if (operandIndex >= codelet.getDeps().size())
    return std::nullopt;
  return operandIndex;
}

// `getDepStorageViewKind` is provided by the shared codir Utils
// (CodeletABIUtils.h) and reached unqualified here.

static bool storageViewUsesComputeBlock(codir::CodirStorageViewKind view) {
  return view == codir::CodirStorageViewKind::compute_block ||
         view == codir::CodirStorageViewKind::phase_redistributed;
}

static bool hasSameBlockStoragePlan(codir::CodeletOp lhs, unsigned lhsDepIndex,
                                    codir::CodeletOp rhs,
                                    unsigned rhsDepIndex) {
  if (!lhs || !rhs)
    return false;
  std::optional<SmallVector<unsigned, 4>> lhsOwnerDims =
      getDepOwnerDims(lhs, lhsDepIndex);
  std::optional<SmallVector<unsigned, 4>> rhsOwnerDims =
      getDepOwnerDims(rhs, rhsDepIndex);
  return lhsOwnerDims && rhsOwnerDims && *lhsOwnerDims == *rhsOwnerDims &&
         lhs.getTileShapeAttr() == rhs.getTileShapeAttr() &&
         lhs.getLogicalWorkerSliceAttr() == rhs.getLogicalWorkerSliceAttr();
}

static bool isCompatibleBlockStorageParticipant(codir::CodeletOp seed,
                                                unsigned seedDepIndex,
                                                codir::CodeletOp candidate,
                                                unsigned candidateDepIndex) {
  if (!seed || !candidate ||
      !hasSameBlockStoragePlan(seed, seedDepIndex, candidate,
                               candidateDepIndex))
    return false;
  std::optional<codir::CodirStorageViewKind> view =
      getDepStorageViewKind(candidate, candidateDepIndex);
  if (view && !storageViewUsesComputeBlock(*view))
    return false;
  if (!hasTileOwnerSlicePlan(candidate) ||
      !depAccessesStayWithinSingleOwnerSlice(candidate, candidateDepIndex))
    return false;
  return true;
}

static bool hasIncompatibleSharedStorageUse(codir::CodeletOp seed,
                                            unsigned seedDepIndex, Value root) {
  if (!seed || !root)
    return false;

  SmallVector<Value, 8> worklist{root};
  llvm::SmallPtrSet<Value, 16> visited;
  while (!worklist.empty()) {
    Value current = worklist.pop_back_val();
    if (!current || !visited.insert(current).second)
      continue;

    for (OpOperand &use : llvm::make_early_inc_range(current.getUses())) {
      Operation *owner = use.getOwner();
      if (!owner)
        continue;
      if (owner->getParentOfType<codir::CodeletOp>() &&
          !isa<codir::CodeletOp>(owner))
        continue;
      if (isa<memref::DeallocOp, memref::DimOp>(owner))
        continue;

      if (auto codelet = dyn_cast<codir::CodeletOp>(owner)) {
        std::optional<unsigned> depIndex =
            getCodeletDepOperandIndex(codelet, use);
        if (!depIndex)
          return true;
        if (codelet == seed && *depIndex == seedDepIndex)
          continue;
        if (!isCompatibleBlockStorageParticipant(seed, seedDepIndex, codelet,
                                                 *depIndex))
          return true;
        continue;
      }

      if (codir::isMemrefForwardingOp(owner)) {
        for (Value result : owner->getResults())
          if (isa<MemRefType>(result.getType()))
            worklist.push_back(result);
        continue;
      }

      if (isa<memref::LoadOp, memref::StoreOp>(owner))
        return true;
    }
  }

  return false;
}

static bool needsSharedRootRedistribution(codir::CodeletOp codelet,
                                          unsigned depIndex, Value dep) {
  Value root = stripStorageViews(dep);
  if (!root)
    return false;
  return hasIncompatibleSharedStorageUse(codelet, depIndex, root);
}

static bool shouldDeferPhaseRedistribution(codir::CodeletOp codelet,
                                           unsigned depIndex, Value dep) {
  if (isMatmulCodelet(codelet))
    return false;
  if (!isPerDependencyRedistribution(codelet, depIndex))
    return false;

  std::optional<int64_t> elements = getStaticLogicalElementCount(dep);
  return elements && *elements > kMaxPhaseRedistributionBridgeElements;
}

static bool shouldUseHostWholeReadOnlyDep(codir::CodeletOp codelet,
                                          unsigned depIndex, Value dep) {
  std::optional<codir::CodirAccessMode> mode =
      getDepAccessMode(codelet, depIndex);
  if (!mode || *mode != codir::CodirAccessMode::read)
    return false;

  constexpr int64_t kMaxSmallReadOnlyHostWholeElements = 64LL * 1024LL;
  std::optional<int64_t> elements = getStaticLogicalElementCount(dep);
  return elements && *elements > 0 &&
         *elements <= kMaxSmallReadOnlyHostWholeElements;
}

/// True when any access offset on |codelet| is nonzero. For stencil codelets
/// this implies the halo crosses tile boundaries even when every leaf access
/// indexes via the owner dim, which is the case `depAccessesStayWithinSingle
/// OwnerSlice` cannot detect (it tracks dim alignment, not byte offsets).
static bool stencilCrossesTileBoundary(codir::CodeletOp codelet) {
  if (!codelet)
    return false;
  auto anyNonzero = [](ArrayAttr attr) {
    if (!attr)
      return false;
    for (Attribute element : attr) {
      auto intAttr = dyn_cast<IntegerAttr>(element);
      if (intAttr && intAttr.getInt() != 0)
        return true;
    }
    return false;
  };
  return anyNonzero(codelet.getAccessMinOffsetsAttr()) ||
         anyNonzero(codelet.getAccessMaxOffsetsAttr());
}

/// True when the stencil's per-iteration write footprint fits inside the
/// codelet's owner-dim tile slice. Conv-3d-style centrally-written stencils
/// have `write_footprint = [1, ...]` against tiles of [32, 64, 64]; such
/// writes are safe to lower as block-owned.
static bool stencilWriteFitsInTile(codir::CodeletOp codelet) {
  if (!codelet || !hasTileOwnerSlicePlan(codelet))
    return false;
  std::optional<SmallVector<int64_t, 4>> writeFootprint =
      readI64ArrayAttr(codelet.getWriteFootprintAttr());
  std::optional<SmallVector<int64_t, 4>> tileShape =
      readI64ArrayAttr(codelet.getTileShapeAttr());
  if (!writeFootprint || !tileShape ||
      writeFootprint->size() != tileShape->size())
    return false;
  for (size_t dim = 0, e = writeFootprint->size(); dim < e; ++dim) {
    int64_t footprint = (*writeFootprint)[dim];
    int64_t tile = (*tileShape)[dim];
    if (footprint < 0 || tile <= 0 || footprint > tile)
      return false;
  }
  return true;
}

/// True iff `root` (the allocation underlying a codelet dep) is written by
/// some other CodeletOp in the same function. Walks `root`'s users
/// transitively through memref-forwarding ops so alternating-buffer swap loops,
/// where the same outer alloc reaches both a write codelet and a read
/// codelet via casts or subviews, are classified as having a sibling
/// writer. Bare memref.store users (sequential init code outside any
/// codelet) are ignored: they are not concurrent with parallel execution
/// and do not invalidate replicated_read.
static bool isWrittenByAnotherCodelet(Value root, codir::CodeletOp self) {
  if (!root)
    return false;
  SmallVector<Value, 8> worklist{root};
  llvm::SmallPtrSet<Value, 16> visited;
  while (!worklist.empty()) {
    Value cur = worklist.pop_back_val();
    if (!cur || !visited.insert(cur).second)
      continue;
    for (Operation *user : cur.getUsers()) {
      if (!user)
        continue;
      if (auto otherCodelet = dyn_cast<codir::CodeletOp>(user)) {
        if (otherCodelet == self)
          continue;
        for (auto [idx, dep] : llvm::enumerate(otherCodelet.getDeps())) {
          if (dep != cur)
            continue;
          std::optional<codir::CodirAccessMode> mode =
              getDepAccessMode(otherCodelet, static_cast<unsigned>(idx));
          if (mode && (*mode == codir::CodirAccessMode::write ||
                       *mode == codir::CodirAccessMode::readwrite))
            return true;
        }
        continue;
      }
      if (codir::isMemrefForwardingOp(user))
        for (Value result : user->getResults())
          if (isa<MemRefType>(result.getType()))
            worklist.push_back(result);
    }
  }
  return false;
}

/// A stencil codelet's read dep is replicate-eligible iff (a) the codelet
/// pattern is a halo-style stencil whose reads may cross a tile boundary
/// (wavefront and alternating-buffer patterns are excluded by enum), and
/// (b) the dep's underlying allocation is not written by any sibling
/// codelet in the same function. Condition (b) distinguishes the
/// read-only stencil case (replicate-safe) from the alternating-buffer
/// case where replicate forces a whole-array re-broadcast every step.
///
/// cross_dim_stencil_3d is excluded because its halo-crossing read deps need
/// the compute_block path (with halo exchange) to guarantee correct boundary
/// values at 2n. The replicated-read path produces a local_only DB whose
/// boundary correctness relies on each node independently initializing the
/// full array, but the halo-exchange contract provides stronger ordering
/// guarantees at the producer-consumer boundary across partitions.
///
/// stencil_tiling_nd and higher_order_stencil are only eligible when the
/// codelet distributes along a single tile owner dim. With multiple tile owner
/// dims (2D distribution), the per-node tile is a 2D shard and the read-only
/// input cannot be correctly replicated at partition boundaries via the
/// local_only path; route through compute_block halo exchange instead.
static bool isStencilDepReplicateEligible(codir::CodeletOp codelet,
                                          unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  auto pattern = codelet.getPatternAttr();
  if (!pattern)
    return false;
  switch (pattern.getValue()) {
  case codir::CodirPattern::stencil_tiling_nd:
  case codir::CodirPattern::higher_order_stencil:
    // Only safe when exactly one tile owner dim drives the distribution.
    // Multi-owner-dim (2D/3D tile) distributions require halo exchange.
    if (!getSingleTileOwnerDim(codelet))
      return false;
    break;
  case codir::CodirPattern::cross_dim_stencil_3d:
  case codir::CodirPattern::wavefront_2d:
  case codir::CodirPattern::alternating_buffer_stencil:
    return false;
  default:
    return false;
  }
  if (!stencilCrossesTileBoundary(codelet))
    return false;
  Value root = stripStorageViews(codelet.getDeps()[depIndex]);
  return !isWrittenByAnotherCodelet(root, codelet);
}

/// Stencil writes that stay inside the owner-dim tile slice are the canonical
/// block-owned output: demote SDE's default `host_whole` to `compute_block`
/// so the resulting DB is distributed instead of host-resident.
static bool shouldDemoteStencilWriteToComputeBlock(codir::CodeletOp codelet,
                                                   unsigned depIndex) {
  if (!isStencilCodelet(codelet) || !hasTileOwnerSlicePlan(codelet))
    return false;
  std::optional<codir::CodirAccessMode> mode =
      getDepAccessMode(codelet, depIndex);
  if (!mode || *mode != codir::CodirAccessMode::write)
    return false;
  return stencilWriteFitsInTile(codelet);
}

/// Stencil reads on patterns that have writers in the same time loop
/// (alternating buffers, wavefront) must stay block-distributed. SDE may stamp
/// `host_whole` as the initial view; demote it so the resulting DB is
/// distributed and the existing halo-exchange machinery
/// (`kArtsDepFlagPreserveShape` + `arts_add_dependence_at_ex`) fires for the
/// neighbor-tile slices.
static bool shouldDemoteStencilHaloReadToComputeBlock(codir::CodeletOp codelet,
                                                      unsigned depIndex) {
  if (!isStencilCodelet(codelet) || !hasTileOwnerSlicePlan(codelet))
    return false;
  if (isStencilDepReplicateEligible(codelet, depIndex))
    return false;
  std::optional<codir::CodirAccessMode> mode =
      getDepAccessMode(codelet, depIndex);
  if (!mode || *mode != codir::CodirAccessMode::read)
    return false;
  if (!stencilCrossesTileBoundary(codelet))
    return false;
  return stencilWriteFitsInTile(codelet);
}

static bool shouldUseReplicatedReadDep(codir::CodeletOp codelet,
                                       unsigned depIndex) {
  if (!isMatmulCodelet(codelet) && !isStencilCodelet(codelet))
    return false;
  std::optional<codir::CodirAccessMode> mode =
      getDepAccessMode(codelet, depIndex);
  if (!mode || *mode != codir::CodirAccessMode::read)
    return false;
  if (!hasTileOwnerSlicePlan(codelet))
    return false;
  if (isStencilCodelet(codelet))
    return isStencilDepReplicateEligible(codelet, depIndex);
  /// Matmul keeps the dim-alignment semantic: a B/A read whose access dim
  /// disagrees with the codelet's owner dim is the replicated participant.
  return !depAccessesStayWithinSingleOwnerSlice(codelet, depIndex);
}

static ArrayAttr
buildOwnerDimsAttr(MLIRContext *ctx,
                   std::optional<SmallVector<unsigned, 4>> ownerDims) {
  if (!ownerDims)
    return ArrayAttr::get(ctx, {});
  SmallVector<int64_t, 4> values;
  values.reserve(ownerDims->size());
  for (unsigned dim : *ownerDims)
    values.push_back(dim);
  return buildI64ArrayAttr(ctx, values);
}

static bool hasFinalizedStoragePlanningFacts(codir::CodeletOp codelet) {
  if (!codelet)
    return false;
  unsigned depCount = codelet.getDeps().size();
  ArrayAttr storageViews = codelet.getDepStorageViewsAttr();
  ArrayAttr ownerDims = codelet.getDepOwnerDimsAttr();
  ArrayAttr collectives = codelet.getDepCollectivesAttr();
  if (!storageViews || !ownerDims || !collectives ||
      storageViews.size() != depCount || ownerDims.size() != depCount ||
      collectives.size() != depCount)
    return false;

  for (unsigned index = 0; index < depCount; ++index) {
    if (!isa<codir::CodirStorageViewKindAttr>(storageViews[index]) ||
        !isa<ArrayAttr>(ownerDims[index]) ||
        !isa<codir::CodirCollectiveKindAttr>(collectives[index]))
      return false;
  }
  return true;
}

static LogicalResult
verifyFinalizedStoragePlanningAttr(codir::CodeletOp codelet, ArrayAttr existing,
                                   ArrayAttr desired, StringRef attrName) {
  if (existing == desired)
    return success();
  return codelet.emitOpError()
         << "existing " << attrName
         << " does not match recomputed CODIR storage plan";
}

static codir::CodirStorageViewKind
chooseStorageView(codir::CodeletOp codelet, unsigned depIndex,
                  codir::CodirStorageViewKind requested) {
  bool stencilRequiresComputeBlock =
      shouldDemoteStencilWriteToComputeBlock(codelet, depIndex) ||
      shouldDemoteStencilHaloReadToComputeBlock(codelet, depIndex);
  /// Replicated-read eligibility is a semantic property of the dep (matmul
  /// inner operand, or stencil read with halo crossing). The initial view the
  /// SDE→CODIR materializer stamps (host_whole for whole-storage tokens,
  /// compute_block for sliced tokens) is irrelevant to that semantics, so the
  /// promotion fires for either starting view.
  if ((requested == codir::CodirStorageViewKind::host_whole ||
       requested == codir::CodirStorageViewKind::compute_block) &&
      shouldUseReplicatedReadDep(codelet, depIndex))
    return codir::CodirStorageViewKind::replicated_read;
  if (requested == codir::CodirStorageViewKind::host_whole &&
      stencilRequiresComputeBlock)
    requested = codir::CodirStorageViewKind::compute_block;
  if (requested != codir::CodirStorageViewKind::compute_block)
    return requested;
  if (!codelet || depIndex >= codelet.getDeps().size())
    return codir::CodirStorageViewKind::host_whole;

  Value dep = codelet.getDeps()[depIndex];
  if (isStorageView(dep))
    return codir::CodirStorageViewKind::compute_block;
  if (!hasTileOwnerSlicePlan(codelet) ||
      !depAccessesStayWithinSingleOwnerSlice(codelet, depIndex)) {
    // Stencil halo accesses cross the owner slice by construction; keep
    // compute_block when the stencil demote predicates authorize block-owned
    // storage with halo-fetched neighbors.
    if (stencilRequiresComputeBlock)
      return codir::CodirStorageViewKind::compute_block;
    return codir::CodirStorageViewKind::host_whole;
  }

  Value root = stripStorageViews(dep);
  if (stencilRequiresComputeBlock)
    return codir::CodirStorageViewKind::compute_block;

  bool needsHostBridge = isa_and_nonnull<BlockArgument>(root) ||
                         hasHostMemrefAccessOutsideCodelet(root);
  if (needsHostBridge && shouldUseHostWholeReadOnlyDep(codelet, depIndex, dep))
    return codir::CodirStorageViewKind::host_whole;

  if (needsSharedRootRedistribution(codelet, depIndex, dep)) {
    if (shouldDeferPhaseRedistribution(codelet, depIndex, dep))
      return codir::CodirStorageViewKind::host_whole;
    return codir::CodirStorageViewKind::phase_redistributed;
  }

  if (!needsHostBridge)
    return codir::CodirStorageViewKind::compute_block;

  if (shouldDeferPhaseRedistribution(codelet, depIndex, dep))
    return codir::CodirStorageViewKind::host_whole;
  return codir::CodirStorageViewKind::phase_redistributed;
}

struct StoragePlanningPass
    : public codir::impl::StoragePlanningBase<StoragePlanningPass> {
  void runOnOperation() override {
    bool hadFailure = false;
    llvm::SmallPtrSet<Operation *, 16> finalizedPlans;
    getOperation().walk([&](codir::CodeletOp codelet) {
      bool finalized = hasFinalizedStoragePlanningFacts(codelet);
      if (finalized)
        finalizedPlans.insert(codelet.getOperation());
      ArrayAttr storageViews = codelet.getDepStorageViewsAttr();

      SmallVector<Attribute> plannedViews;
      SmallVector<Attribute> plannedOwnerDims;
      plannedViews.reserve(codelet.getDeps().size());
      plannedOwnerDims.reserve(codelet.getDeps().size());
      bool changed =
          !storageViews || storageViews.size() != codelet.getDeps().size();
      for (unsigned index = 0, e = codelet.getDeps().size(); index < e;
           ++index) {
        codir::CodirStorageViewKind requested =
            hasTileOwnerSlicePlan(codelet)
                ? codir::CodirStorageViewKind::compute_block
                : codir::CodirStorageViewKind::host_whole;
        if (storageViews && index < storageViews.size()) {
          auto viewAttr =
              dyn_cast<codir::CodirStorageViewKindAttr>(storageViews[index]);
          if (!viewAttr) {
            plannedViews.push_back(storageViews[index]);
            plannedOwnerDims.push_back(buildOwnerDimsAttr(
                codelet.getContext(), getDepOwnerDims(codelet, index)));
            continue;
          }
          requested = viewAttr.getValue();
        }

        codir::CodirStorageViewKind planned =
            chooseStorageView(codelet, index, requested);
        if (planned != requested)
          changed = true;
        if (storageViews && index < storageViews.size()) {
          auto viewAttr =
              dyn_cast<codir::CodirStorageViewKindAttr>(storageViews[index]);
          if (viewAttr && planned != viewAttr.getValue())
            changed = true;
        }
        plannedViews.push_back(codir::CodirStorageViewKindAttr::get(
            codelet.getContext(), planned));
        plannedOwnerDims.push_back(buildOwnerDimsAttr(
            codelet.getContext(), getDepOwnerDims(codelet, index)));
      }

      if (storageViews && storageViews.size() > codelet.getDeps().size()) {
        for (unsigned index = codelet.getDeps().size(), e = storageViews.size();
             index < e; ++index)
          plannedViews.push_back(storageViews[index]);
      }

      ArrayAttr plannedViewsAttr =
          ArrayAttr::get(codelet.getContext(), plannedViews);
      ArrayAttr plannedOwnerDimsAttr =
          ArrayAttr::get(codelet.getContext(), plannedOwnerDims);
      if (finalized) {
        if (failed(verifyFinalizedStoragePlanningAttr(
                codelet, codelet.getDepStorageViewsAttr(), plannedViewsAttr,
                codelet.getDepStorageViewsAttrName())) ||
            failed(verifyFinalizedStoragePlanningAttr(
                codelet, codelet.getDepOwnerDimsAttr(), plannedOwnerDimsAttr,
                codelet.getDepOwnerDimsAttrName()))) {
          hadFailure = true;
          return;
        }
      }

      if (finalized)
        return;
      if (changed)
        codelet.setDepStorageViewsAttr(plannedViewsAttr);
      codelet.setDepOwnerDimsAttr(plannedOwnerDimsAttr);
    });
    if (hadFailure) {
      signalPassFailure();
      return;
    }

    // Stamp `dep_collectives` after every codelet's `dep_storage_views` is
    // planned: `chooseCollective` consults consumers' planned storage views
    // (the all-gather gate looks for a sibling `replicated_read` reader), so it
    // must run on the fully-planned module.
    getOperation().walk([&](codir::CodeletOp codelet) {
      bool finalized = finalizedPlans.contains(codelet.getOperation());
      unsigned depCount = codelet.getDeps().size();
      if (depCount == 0)
        return;
      SmallVector<Attribute> collectives;
      collectives.reserve(depCount);
      for (unsigned index = 0; index < depCount; ++index) {
        codir::CodirCollectiveKind kind =
            codir::chooseCollective(codelet, index);
        collectives.push_back(
            codir::CodirCollectiveKindAttr::get(codelet.getContext(), kind));
      }
      // Stamp one entry per dep so the carrier is uniform and self-describing.
      ArrayAttr plannedCollectives =
          ArrayAttr::get(codelet.getContext(), collectives);
      if (finalized) {
        if (failed(verifyFinalizedStoragePlanningAttr(
                codelet, codelet.getDepCollectivesAttr(), plannedCollectives,
                codelet.getDepCollectivesAttrName()))) {
          hadFailure = true;
          return;
        }
        return;
      }
      codelet.setDepCollectivesAttr(plannedCollectives);
    });
    if (hadFailure)
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::codir::createStoragePlanningPass() {
  return std::make_unique<StoragePlanningPass>();
}
