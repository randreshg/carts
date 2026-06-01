#include "carts/dialect/codir/Utils/CodeletABIUtils.h"

#include "carts/dialect/codir/Utils/CodirAttrNames.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"

namespace mlir::carts::codir {

namespace {
inline bool codirAccessMayRead(CodirAccessMode mode) {
  return mode == CodirAccessMode::read || mode == CodirAccessMode::readwrite;
}
inline bool codirAccessMayWrite(CodirAccessMode mode) {
  return mode == CodirAccessMode::write || mode == CodirAccessMode::readwrite;
}

static bool isPositiveI64(DictionaryAttr dict, StringRef key) {
  auto value =
      dyn_cast_or_null<IntegerAttr>(dict ? dict.get(key) : Attribute{});
  return value && value.getInt() > 0;
}

static bool hasStringValue(DictionaryAttr dict, StringRef key,
                           StringRef expected) {
  auto value = dyn_cast_or_null<StringAttr>(dict ? dict.get(key) : Attribute{});
  return value && value.getValue() == expected;
}

static bool isRoleCompatible(DictionaryAttr dict, CodirAccessMode mode) {
  auto role = dyn_cast_or_null<StringAttr>(
      dict ? dict.get(AttrNames::LayoutGraphKeys::Role) : Attribute{});
  if (!role)
    role = dyn_cast_or_null<StringAttr>(
        dict ? dict.get(AttrNames::PartitionGraphKeys::Role) : Attribute{});
  if (!role)
    return true;
  if (codirAccessMayWrite(mode) &&
      role.getValue() == AttrNames::LayoutGraphValues::RoleWrite)
    return true;
  if (codirAccessMayRead(mode) &&
      role.getValue() == AttrNames::LayoutGraphValues::RoleRead)
    return true;
  return false;
}

static bool arrayAttrContainsI64(ArrayAttr attr, int64_t value) {
  if (!attr)
    return false;
  for (Attribute element : attr)
    if (auto intAttr = dyn_cast<IntegerAttr>(element))
      if (intAttr.getInt() == value)
        return true;
  return false;
}

static bool arrayLayoutEntryHasMismatch(CodeletOp codelet,
                                        DictionaryAttr entry) {
  if (!entry)
    return false;
  if (isPositiveI64(entry, AttrNames::LayoutGraphKeys::CommVolumeBytes))
    return true;
  auto arrayId = dyn_cast_or_null<IntegerAttr>(
      entry.get(AttrNames::LayoutGraphKeys::ArrayId));
  return arrayId && arrayAttrContainsI64(codelet.getLayoutsDisagreeAttr(),
                                         arrayId.getInt());
}

static bool depArrayLayoutHasMismatch(CodeletOp codelet, unsigned depIndex,
                                      CodirAccessMode mode) {
  ArrayAttr layout = codelet ? codelet.getArrayLayoutAttr() : ArrayAttr{};
  if (!layout || depIndex >= layout.size())
    return false;
  auto entry = dyn_cast<DictionaryAttr>(layout[depIndex]);
  return entry && isRoleCompatible(entry, mode) &&
         arrayLayoutEntryHasMismatch(codelet, entry);
}

static bool partitionGraphHasMismatch(CodeletOp codelet, CodirAccessMode mode) {
  ArrayAttr graph = codelet ? dyn_cast_or_null<ArrayAttr>(
                                  codelet->getAttr(AttrNames::PartitionGraph))
                            : ArrayAttr{};
  if (!graph)
    return false;
  for (Attribute attr : graph) {
    auto entry = dyn_cast<DictionaryAttr>(attr);
    if (!entry)
      continue;
    if (!hasStringValue(entry, AttrNames::PartitionGraphKeys::EdgeClass,
                        AttrNames::PartitionGraphValues::EdgeLayoutMismatch))
      continue;
    if (!isRoleCompatible(entry, mode))
      continue;
    // Owner-block entries describe the compute DB home shape. They may carry
    // aggregate communication pressure for the codelet, but they are not by
    // themselves a per-dependency redistribution edge.
    if (hasStringValue(entry, AttrNames::PartitionGraphKeys::LayoutKind,
                       AttrNames::PartitionGraphValues::OwnerBlock))
      continue;
    return true;
  }
  return false;
}

static bool depHasLayoutMismatchEvidence(CodeletOp codelet, unsigned depIndex,
                                         CodirAccessMode mode) {
  return depArrayLayoutHasMismatch(codelet, depIndex, mode) ||
         partitionGraphHasMismatch(codelet, mode);
}

static bool isReductionLike(CodeletOp codelet) {
  if (!codelet)
    return false;
  if (codelet.getPartialReductionAttr() ||
      codeletIsCrossOwnerTransposeReduce(codelet))
    return true;
  auto pattern = codelet.getPatternAttr();
  return pattern && pattern.getValue() == CodirPattern::reduction;
}

static bool isStencilCollectiveLike(CodeletOp codelet) {
  if (!codelet)
    return false;
  auto pattern = codelet.getPatternAttr();
  if (!pattern)
    return false;
  switch (pattern.getValue()) {
  case CodirPattern::stencil_tiling_nd:
  case CodirPattern::cross_dim_stencil_3d:
  case CodirPattern::higher_order_stencil:
  case CodirPattern::wavefront_2d:
  case CodirPattern::alternating_buffer_stencil:
    break;
  default:
    return false;
  }
  if (codelet.getEmitBlockNativeStencilAttr())
    return true;
  auto repetition = codelet.getRepetitionStructureAttr();
  return repetition &&
         repetition.getValue() == CodirRepetitionStructure::full_timestep;
}
} // namespace

bool isCodirDependencyType(Type type) {
  return isa<MemRefType, UnrankedMemRefType>(type);
}

bool isCodirScalarParamType(Type type) { return type.isIntOrIndexOrFloat(); }

bool isMemrefForwardingOp(Operation *op) {
  if (!op || op->getNumRegions() != 0)
    return false;
  return llvm::any_of(op->getResults(), [](Value result) {
    return isa<MemRefType>(result.getType());
  });
}

std::optional<CodirAccessMode> getDepAccessMode(CodeletOp codelet,
                                                unsigned depIndex) {
  ArrayAttr modes = codelet ? codelet.getDepModesAttr() : ArrayAttr{};
  if (!modes || depIndex >= modes.size())
    return std::nullopt;
  auto mode = dyn_cast<CodirAccessModeAttr>(modes[depIndex]);
  if (!mode)
    return std::nullopt;
  return mode.getValue();
}

std::optional<CodirStorageViewKind> getDepStorageViewKind(CodeletOp codelet,
                                                          unsigned depIndex) {
  ArrayAttr views = codelet ? codelet.getDepStorageViewsAttr() : ArrayAttr{};
  if (!views || depIndex >= views.size())
    return std::nullopt;
  auto view = dyn_cast<CodirStorageViewKindAttr>(views[depIndex]);
  if (!view)
    return std::nullopt;
  return view.getValue();
}

bool coarseBridgeTargetHasReplicatedReadConsumer(CodeletOp producer,
                                                 unsigned depIndex) {
  if (!producer || depIndex >= producer.getDeps().size())
    return false;
  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
      producer.getDeps()[depIndex]);
  if (!root)
    return false;
  Operation *scope = producer->getParentOfType<ModuleOp>();
  if (!scope)
    return false;

  bool found = false;
  scope->walk([&](CodeletOp consumer) {
    if (found || consumer == producer)
      return;
    for (auto [idx, dep] : llvm::enumerate(consumer.getDeps())) {
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) != root)
        continue;
      unsigned consumerDep = static_cast<unsigned>(idx);
      std::optional<CodirAccessMode> mode =
          getDepAccessMode(consumer, consumerDep);
      if (!mode || !codirAccessMayRead(*mode))
        continue;
      std::optional<CodirStorageViewKind> view =
          getDepStorageViewKind(consumer, consumerDep);
      if (view && *view == CodirStorageViewKind::replicated_read) {
        found = true;
        return;
      }
    }
  });
  return found;
}

bool codeletIsCrossOwnerTransposeReduce(CodeletOp consumer) {
  if (!consumer || !consumer.getPartialReductionAttr())
    return false;
  ArrayAttr ownerDims = consumer.getPartialReductionOwnerDimsAttr();
  if (!ownerDims || ownerDims.empty())
    return false;
  auto isResultOwnerDim = [&](int64_t dim) {
    for (Attribute owner : ownerDims)
      if (auto intAttr = dyn_cast<IntegerAttr>(owner))
        if (intAttr.getInt() == dim)
          return true;
    return false;
  };
  ArrayAttr depMaps = consumer.getPartialReductionDepResultDimMapsAttr();
  if (!depMaps)
    return false;
  for (Attribute mapAttr : depMaps) {
    auto depMap = dyn_cast<ArrayAttr>(mapAttr);
    if (!depMap || depMap.size() != 2)
      continue;
    auto leading = dyn_cast<IntegerAttr>(depMap[0]);
    auto trailing = dyn_cast<IntegerAttr>(depMap[1]);
    if (!leading || !trailing)
      continue;
    if (leading.getInt() < 0 && isResultOwnerDim(trailing.getInt()))
      return true;
  }
  return false;
}

bool coarseBridgeTargetHasCrossOwnerReduceConsumer(CodeletOp producer,
                                                   unsigned depIndex) {
  if (!producer || depIndex >= producer.getDeps().size())
    return false;
  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
      producer.getDeps()[depIndex]);
  if (!root)
    return false;
  Operation *scope = producer->getParentOfType<ModuleOp>();
  if (!scope)
    return false;

  bool found = false;
  scope->walk([&](CodeletOp consumer) {
    if (found || consumer == producer)
      return;
    if (!codeletIsCrossOwnerTransposeReduce(consumer))
      return;
    for (auto [idx, dep] : llvm::enumerate(consumer.getDeps())) {
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) != root)
        continue;
      std::optional<CodirAccessMode> mode =
          getDepAccessMode(consumer, static_cast<unsigned>(idx));
      if (mode && codirAccessMayRead(*mode)) {
        found = true;
        return;
      }
    }
  });
  return found;
}

CodirCollectiveKind chooseCollective(CodeletOp codelet, unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return CodirCollectiveKind::none;
  // The all-gather and cross-owner reduce gates only fire for a dep the codelet
  // WRITES (the coarse copy-out producer); the materializer evaluates them on
  // write-mode participants only. Mirror that here so a read dep that happens
  // to share a root with a written one is never mislabeled.
  std::optional<CodirAccessMode> mode = getDepAccessMode(codelet, depIndex);
  if (!mode || !codirAccessMayWrite(*mode))
    return CodirCollectiveKind::none;
  if (depHasLayoutMismatchEvidence(codelet, depIndex, *mode)) {
    if (isStencilCollectiveLike(codelet))
      return CodirCollectiveKind::halo;
    if (isReductionLike(codelet))
      return CodirCollectiveKind::reduce_scatter;
    // An all-gather presupposes the dep is block-distributed (owner-tiled) so
    // there are per-owner blocks to gather. An in-place shared-state codelet
    // deliberately keeps its backing store as a single coarse host_whole
    // readwrite buffer with no owner-tiled blocks, so all-gather does not
    // apply: leave it a plain coarse readwrite dep that lowers to
    // inPlaceSharedState. Stamping all_gather here would pair a distribution
    // collective with the empty dep_owner_dims an in-place dep has (and must
    // keep), which the ARTS materializer fail-closes on.
    if (codelet.getInPlaceSharedStateAttr())
      return CodirCollectiveKind::none;
    return CodirCollectiveKind::all_gather;
  }
  if (coarseBridgeTargetHasReplicatedReadConsumer(codelet, depIndex))
    return CodirCollectiveKind::all_gather;
  if (codeletIsCrossOwnerTransposeReduce(codelet) ||
      coarseBridgeTargetHasCrossOwnerReduceConsumer(codelet, depIndex))
    return CodirCollectiveKind::reduce_scatter;
  // Iterative stencil writes select nearest-neighbor halo exchange even without
  // a layout mismatch, so the buffer stays a per-block single-writer distributed
  // DB across timesteps. This covers BOTH in-place (stencil_tiling_nd) and
  // double-buffered (alternating_buffer_stencil) iterative stencils -- the same
  // set isStencilCollectiveLike already recognizes in the layout-mismatch branch
  // above. Without this, a same-layout double-buffer jacobi stencil falls to
  // `none` and is realized via a per-timestep host_whole<->block bridge copy.
  if (isStencilCollectiveLike(codelet))
    return CodirCollectiveKind::halo;
  return CodirCollectiveKind::none;
}

} // namespace mlir::carts::codir
