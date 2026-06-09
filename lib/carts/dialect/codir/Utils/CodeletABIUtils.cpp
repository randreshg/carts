#include "carts/dialect/codir/Utils/CodeletABIUtils.h"

#include "carts/dialect/codir/Utils/CodirAccessTraceUtils.h"
#include "carts/dialect/codir/Utils/CodirAttrNames.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"

#include <algorithm>

namespace mlir::carts::codir {

bool stencilWriteFitsInTile(CodeletOp codelet) {
  if (!codelet || !codelet.getTileOwnerDimsAttr() ||
      !codelet.getTileShapeAttr())
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

static bool isPositiveI64Array(ArrayAttr attr) {
  std::optional<SmallVector<int64_t, 4>> values = readI64ArrayAttr(attr);
  if (!values || values->empty())
    return false;
  return llvm::all_of(*values, [](int64_t value) { return value > 0; });
}

static std::optional<CodirCollectiveKind>
getDepCollectiveKind(CodeletOp codelet, unsigned depIndex) {
  ArrayAttr collectives =
      codelet ? codelet.getDepCollectivesAttr() : ArrayAttr{};
  if (!collectives || depIndex >= collectives.size())
    return std::nullopt;
  auto kind = dyn_cast<CodirCollectiveKindAttr>(collectives[depIndex]);
  if (!kind)
    return std::nullopt;
  return kind.getValue();
}

static bool blockShapeAlignsWithTile(ArrayAttr blockShape,
                                     ArrayAttr tileShape) {
  std::optional<SmallVector<int64_t, 4>> blocks = readI64ArrayAttr(blockShape);
  std::optional<SmallVector<int64_t, 4>> tiles = readI64ArrayAttr(tileShape);
  if (!blocks || !tiles || blocks->empty() || blocks->size() != tiles->size())
    return false;
  for (auto [block, tile] : llvm::zip_equal(*blocks, *tiles))
    if (block <= 0 || tile <= 0 || block % tile != 0)
      return false;
  return true;
}

static bool isFullTimestep(CodeletOp codelet) {
  auto repetition = codelet ? codelet.getRepetitionStructureAttr() : nullptr;
  return repetition &&
         repetition.getValue() == CodirRepetitionStructure::full_timestep;
}

static bool isUniformCodelet(CodeletOp codelet) {
  auto pattern = codelet ? codelet.getPatternAttr() : nullptr;
  return pattern && pattern.getValue() == CodirPattern::uniform;
}

static bool isStencilCodelet(CodeletOp codelet) {
  auto pattern = codelet ? codelet.getPatternAttr() : nullptr;
  if (!pattern)
    return false;
  switch (pattern.getValue()) {
  case CodirPattern::stencil_tiling_nd:
  case CodirPattern::cross_dim_stencil_3d:
  case CodirPattern::higher_order_stencil:
  case CodirPattern::wavefront_2d:
  case CodirPattern::alternating_buffer_stencil:
    return true;
  default:
    return false;
  }
}

static bool isFullTimestepUniformStencilPair(CodeletOp lhs, CodeletOp rhs) {
  return isFullTimestep(lhs) && isFullTimestep(rhs) &&
         ((isUniformCodelet(lhs) && isStencilCodelet(rhs)) ||
          (isStencilCodelet(lhs) && isUniformCodelet(rhs)));
}

static ArrayAttr getComputeBlockReadShapeOr(CodeletOp codelet,
                                            unsigned depIndex,
                                            CodirAccessMode mode,
                                            ArrayAttr fallback) {
  if (!fallback || !codirAccessMayRead(mode) || codirAccessMayWrite(mode))
    return fallback;
  std::optional<CodirStorageViewKind> view =
      getDepStorageViewKind(codelet, depIndex);
  if (!view || *view != CodirStorageViewKind::compute_block)
    return fallback;
  std::optional<CodirCollectiveKind> collective =
      getDepCollectiveKind(codelet, depIndex);
  if (!collective || *collective != CodirCollectiveKind::halo)
    return fallback;
  ArrayAttr tileShape = codelet ? codelet.getTileShapeAttr() : ArrayAttr{};
  if (!isPositiveI64Array(tileShape) ||
      blockShapeAlignsWithTile(fallback, tileShape))
    return fallback;
  return tileShape;
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
  DictionaryAttr entry = getArrayLayoutEntryForDep(codelet, depIndex);
  return entry && isRoleCompatible(entry, mode) &&
         arrayLayoutEntryHasMismatch(codelet, entry);
}

static bool partitionGraphHasMismatch(CodeletOp codelet, unsigned depIndex,
                                      CodirAccessMode mode) {
  std::optional<int64_t> depArrayId = getDepArrayId(codelet, depIndex);
  if (!depArrayId)
    return false;
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
    auto muId = dyn_cast_or_null<IntegerAttr>(
        entry.get(AttrNames::PartitionGraphKeys::MuId));
    if (!muId || muId.getInt() != *depArrayId)
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
         partitionGraphHasMismatch(codelet, depIndex, mode);
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

static bool storageViewUsesComputeBlock(CodirStorageViewKind view) {
  return view == CodirStorageViewKind::compute_block ||
         view == CodirStorageViewKind::phase_redistributed;
}

static bool depHasBlockStoragePlan(CodeletOp codelet, unsigned depIndex) {
  std::optional<CodirStorageViewKind> view =
      getDepStorageViewKind(codelet, depIndex);
  if (!view || !storageViewUsesComputeBlock(*view))
    return false;
  ArrayAttr ownerDims = codelet ? codelet.getDepOwnerDimsAttr() : ArrayAttr{};
  if (!ownerDims || depIndex >= ownerDims.size())
    return false;
  auto dims = dyn_cast<ArrayAttr>(ownerDims[depIndex]);
  return dims && !dims.empty();
}

static std::optional<SmallVector<unsigned, 4>>
readUnsignedDims(ArrayAttr attr) {
  std::optional<SmallVector<int64_t, 4>> rawDims = readI64ArrayAttr(attr);
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

static std::optional<SmallVector<unsigned, 4>>
getTileOwnerDims(CodeletOp codelet) {
  return codelet ? readUnsignedDims(codelet.getTileOwnerDimsAttr())
                 : std::nullopt;
}

static std::optional<SmallVector<unsigned, 4>>
getDepOwnerDims(CodeletOp codelet, unsigned depIndex) {
  ArrayAttr ownerDims = codelet ? codelet.getDepOwnerDimsAttr() : ArrayAttr{};
  if (!ownerDims || depIndex >= ownerDims.size())
    return std::nullopt;
  auto dims = dyn_cast<ArrayAttr>(ownerDims[depIndex]);
  if (!dims)
    return std::nullopt;
  return readUnsignedDims(dims);
}

static SmallVector<Value, 4> getOwnerBaseArguments(CodeletOp codelet,
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

static bool isReadAccess(Operation *op) {
  return isa<memref::LoadOp, affine::AffineLoadOp, polygeist::DynLoadOp>(op);
}

static std::optional<int64_t>
getOwnerDimOffset(ArrayAttr attr, unsigned ownerDim, unsigned ownerSlot) {
  std::optional<SmallVector<int64_t, 4>> values = readI64ArrayAttr(attr);
  if (!values || values->empty())
    return std::nullopt;
  if (ownerDim < values->size())
    return (*values)[ownerDim];
  if (ownerSlot < values->size())
    return (*values)[ownerSlot];
  if (values->size() == 1)
    return values->front();
  return std::nullopt;
}

static bool depHasCommittedShiftedOwnerDimRead(CodeletOp codelet,
                                               unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  std::optional<CodirAccessMode> mode = getDepAccessMode(codelet, depIndex);
  if (!mode || !codirAccessMayRead(*mode))
    return false;
  if (!codirAccessMayWrite(*mode) && codelet.getDeps().size() != 1)
    return false;

  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getDepOwnerDims(codelet, depIndex);
  std::optional<SmallVector<unsigned, 4>> tileOwnerDims =
      getTileOwnerDims(codelet);
  if (!ownerDims || ownerDims->empty() || !tileOwnerDims)
    return false;

  for (unsigned ownerDim : *ownerDims) {
    auto slotIt = llvm::find(*tileOwnerDims, ownerDim);
    if (slotIt == tileOwnerDims->end())
      continue;
    unsigned ownerSlot =
        static_cast<unsigned>(std::distance(tileOwnerDims->begin(), slotIt));
    std::optional<int64_t> minOffset = getOwnerDimOffset(
        codelet.getAccessMinOffsetsAttr(), ownerDim, ownerSlot);
    std::optional<int64_t> maxOffset = getOwnerDimOffset(
        codelet.getAccessMaxOffsetsAttr(), ownerDim, ownerSlot);
    if ((minOffset && *minOffset < 0) || (maxOffset && *maxOffset > 0))
      return true;
  }
  return false;
}

static bool depHasShiftedOwnerDimRead(CodeletOp codelet, unsigned depIndex) {
  if (depHasCommittedShiftedOwnerDimRead(codelet, depIndex))
    return true;

  if (!codelet || codelet.getBody().empty() ||
      depIndex >= codelet.getDeps().size())
    return false;

  Block &body = codelet.getBody().front();
  if (depIndex >= body.getNumArguments())
    return false;
  Value depArg = body.getArgument(depIndex);
  auto depType = dyn_cast<MemRefType>(depArg.getType());
  if (!depType || depType.getRank() == 0)
    return false;

  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getDepOwnerDims(codelet, depIndex);
  std::optional<SmallVector<unsigned, 4>> tileOwnerDims =
      getTileOwnerDims(codelet);
  if (!ownerDims || ownerDims->empty() || !tileOwnerDims)
    return false;
  SmallVector<Value, 4> ownerBases =
      getOwnerBaseArguments(codelet, tileOwnerDims->size());
  if (ownerBases.size() != tileOwnerDims->size())
    return false;

  bool found = false;
  body.walk([&](Operation *op) {
    if (found)
      return WalkResult::interrupt();
    if (!isReadAccess(op))
      return WalkResult::advance();

    auto access = getCodirMemoryAccessInfo(op);
    if (!access)
      return WalkResult::advance();

    for (unsigned ownerDim : *ownerDims) {
      if (ownerDim >= access->indices.size())
        continue;
      auto slotIt = llvm::find(*tileOwnerDims, ownerDim);
      if (slotIt == tileOwnerDims->end())
        continue;
      unsigned ownerSlot =
          static_cast<unsigned>(std::distance(tileOwnerDims->begin(), slotIt));
      if (ownerSlot >= ownerBases.size())
        continue;

      Value ownerBase = ownerBases[ownerSlot];
      CodirAccessOwnerDims traced = traceCodirAccessToRoot(
          access->memref, access->indices, depArg, ownerBase);
      if (traced.status != CodirAccessTraceStatus::Rooted)
        continue;

      int64_t constantOffset = 0;
      Value stripped = ::mlir::carts::ValueAnalysis::stripConstantOffset(
          access->indices[ownerDim], &constantOffset);
      if (constantOffset == 0)
        continue;
      if (!indexSelectsOwnerSlice(stripped, ownerBase) &&
          !indexSelectsOwnerSlice(access->indices[ownerDim], ownerBase))
        continue;

      found = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return found;
}

static bool isStencilPatternKind(CodeletOp codelet) {
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
  return true;
}

static bool codeletHasHaloAccessWindow(CodeletOp codelet) {
  if (!codelet)
    return false;
  auto hasNonZeroEntry = [](ArrayAttr offsets) {
    if (!offsets)
      return false;
    for (Attribute offset : offsets) {
      auto value = dyn_cast<IntegerAttr>(offset);
      if (value && value.getInt() != 0)
        return true;
    }
    return false;
  };
  return hasNonZeroEntry(codelet.getAccessMinOffsetsAttr()) ||
         hasNonZeroEntry(codelet.getAccessMaxOffsetsAttr()) ||
         hasNonZeroEntry(codelet.getHaloShapeAttr());
}

static bool isStencilReadHaloCandidate(CodeletOp codelet) {
  return isStencilPatternKind(codelet) && codeletHasHaloAccessWindow(codelet);
}

static bool isStencilCollectiveLike(CodeletOp codelet) {
  if (!isStencilPatternKind(codelet))
    return false;
  if (codelet.getInPlaceSafeAttr() && codelet.getTileOwnerDimsAttr() &&
      codelet.getTileShapeAttr() && stencilWriteFitsInTile(codelet) &&
      codeletHasHaloAccessWindow(codelet))
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

std::optional<int64_t> getDepArrayId(CodeletOp codelet, unsigned depIndex) {
  ArrayAttr depArrayIds = codelet ? codelet.getDepArrayIdsAttr() : ArrayAttr{};
  if (!depArrayIds || depIndex >= depArrayIds.size())
    return std::nullopt;
  auto intAttr = dyn_cast<IntegerAttr>(depArrayIds[depIndex]);
  if (!intAttr || intAttr.getInt() < 0)
    return std::nullopt;
  return intAttr.getInt();
}

DictionaryAttr getArrayLayoutEntryForDep(CodeletOp codelet, unsigned depIndex) {
  std::optional<int64_t> depArrayId = getDepArrayId(codelet, depIndex);
  if (!depArrayId)
    return {};
  ArrayAttr layout = codelet ? codelet.getArrayLayoutAttr() : ArrayAttr{};
  if (!layout)
    return {};
  for (Attribute attr : layout) {
    auto entry = dyn_cast<DictionaryAttr>(attr);
    if (!entry)
      continue;
    auto arrayId = dyn_cast_or_null<IntegerAttr>(
        entry.get(AttrNames::LayoutGraphKeys::ArrayId));
    if (arrayId && arrayId.getInt() == *depArrayId)
      return entry;
  }
  return {};
}

static ArrayAttr getDepBasePhysicalBlockShapeAttr(CodeletOp codelet,
                                                  unsigned depIndex) {
  std::optional<int64_t> depArrayId = getDepArrayId(codelet, depIndex);
  std::optional<CodirAccessMode> mode = getDepAccessMode(codelet, depIndex);
  if (depArrayId && mode) {
    ArrayAttr graph = codelet ? dyn_cast_or_null<ArrayAttr>(
                                    codelet->getAttr(AttrNames::PartitionGraph))
                              : ArrayAttr{};
    ArrayAttr firstMatchingShape;
    if (graph) {
      ArrayAttr ownerBlockShape;
      ArrayAttr readBlockShape;
      for (Attribute attr : graph) {
        auto entry = dyn_cast<DictionaryAttr>(attr);
        if (!entry)
          continue;
        auto muId = dyn_cast_or_null<IntegerAttr>(
            entry.get(AttrNames::PartitionGraphKeys::MuId));
        if (!muId || muId.getInt() != *depArrayId)
          continue;
        if (!isRoleCompatible(entry, *mode))
          continue;
        auto blockShape = dyn_cast_or_null<ArrayAttr>(
            entry.get(AttrNames::PartitionGraphKeys::BlockShape));
        if (!isPositiveI64Array(blockShape))
          continue;
        if (!firstMatchingShape)
          firstMatchingShape = blockShape;
        if (hasStringValue(entry, AttrNames::PartitionGraphKeys::LayoutKind,
                           AttrNames::PartitionGraphValues::OwnerBlock)) {
          if (!ownerBlockShape)
            ownerBlockShape = blockShape;
          continue;
        }
        if (!readBlockShape)
          readBlockShape = blockShape;
      }
      if (codirAccessMayWrite(*mode) && ownerBlockShape)
        return ownerBlockShape;
      if (codirAccessMayRead(*mode) && readBlockShape)
        return getComputeBlockReadShapeOr(codelet, depIndex, *mode,
                                          readBlockShape);
    }
    if (firstMatchingShape)
      return getComputeBlockReadShapeOr(codelet, depIndex, *mode,
                                        firstMatchingShape);
  }

  DictionaryAttr layoutEntry = getArrayLayoutEntryForDep(codelet, depIndex);
  auto layoutShape = dyn_cast_or_null<ArrayAttr>(
      layoutEntry ? layoutEntry.get(AttrNames::LayoutGraphKeys::BlockShape)
                  : Attribute{});
  if (isPositiveI64Array(layoutShape)) {
    if (mode)
      return getComputeBlockReadShapeOr(codelet, depIndex, *mode, layoutShape);
    return layoutShape;
  }
  return codelet ? codelet.getTileShapeAttr() : ArrayAttr{};
}

static bool sameOwnerDims(std::optional<SmallVector<unsigned, 4>> lhs,
                          std::optional<SmallVector<unsigned, 4>> rhs) {
  return lhs && rhs && *lhs == *rhs;
}

bool areCommensurateBlockShapes(ArrayAttr lhs, ArrayAttr rhs) {
  std::optional<SmallVector<int64_t, 4>> lhsShape = readI64ArrayAttr(lhs);
  std::optional<SmallVector<int64_t, 4>> rhsShape = readI64ArrayAttr(rhs);
  if (!lhsShape || !rhsShape || lhsShape->empty() ||
      lhsShape->size() != rhsShape->size())
    return false;

  for (auto [lhsDim, rhsDim] : llvm::zip_equal(*lhsShape, *rhsShape)) {
    if (lhsDim <= 0 || rhsDim <= 0)
      return false;
    int64_t larger = std::max(lhsDim, rhsDim);
    int64_t smaller = std::min(lhsDim, rhsDim);
    if (larger % smaller != 0)
      return false;
  }
  return true;
}

bool isStrictlyFinerBlockShape(ArrayAttr candidate, ArrayAttr fallback) {
  std::optional<SmallVector<int64_t, 4>> candidateShape =
      readI64ArrayAttr(candidate);
  std::optional<SmallVector<int64_t, 4>> fallbackShape =
      readI64ArrayAttr(fallback);
  if (!candidateShape || !fallbackShape || candidateShape->empty() ||
      candidateShape->size() != fallbackShape->size())
    return false;

  bool strictlyFiner = false;
  for (auto [candidateDim, fallbackDim] :
       llvm::zip_equal(*candidateShape, *fallbackShape)) {
    if (candidateDim <= 0 || fallbackDim <= 0 || candidateDim > fallbackDim)
      return false;
    if (candidateDim < fallbackDim)
      strictlyFiner = true;
  }
  return strictlyFiner;
}

static ArrayAttr getSiblingWriterBlockShapeOr(CodeletOp codelet,
                                              unsigned depIndex,
                                              ArrayAttr fallback) {
  if (!codelet || depIndex >= codelet.getDeps().size() || !fallback)
    return fallback;

  std::optional<CodirAccessMode> mode = getDepAccessMode(codelet, depIndex);
  if (!mode || !codirAccessMayRead(*mode) || codirAccessMayWrite(*mode))
    return fallback;

  std::optional<CodirStorageViewKind> view =
      getDepStorageViewKind(codelet, depIndex);
  if (!view || !storageViewUsesComputeBlock(*view))
    return fallback;

  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
      codelet.getDeps()[depIndex]);
  Operation *scope = codelet->getParentOfType<ModuleOp>();
  if (!root || !scope)
    return fallback;

  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty())
    return fallback;

  ArrayAttr selected;
  bool ambiguous = false;
  scope->walk([&](CodeletOp candidate) {
    if (ambiguous)
      return;
    if (candidate == codelet ||
        !isFullTimestepUniformStencilPair(codelet, candidate))
      return;
    for (auto [idx, dep] : llvm::enumerate(candidate.getDeps())) {
      unsigned candidateDepIndex = static_cast<unsigned>(idx);
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) != root)
        continue;
      std::optional<CodirAccessMode> candidateMode =
          getDepAccessMode(candidate, candidateDepIndex);
      if (!candidateMode || !codirAccessMayWrite(*candidateMode))
        continue;
      std::optional<CodirStorageViewKind> candidateView =
          getDepStorageViewKind(candidate, candidateDepIndex);
      if (!candidateView || !storageViewUsesComputeBlock(*candidateView))
        continue;
      if (!sameOwnerDims(ownerDims,
                         getDepOwnerDims(candidate, candidateDepIndex)))
        continue;
      ArrayAttr candidateShape =
          getDepBasePhysicalBlockShapeAttr(candidate, candidateDepIndex);
      if (!isPositiveI64Array(candidateShape) ||
          !isStrictlyFinerBlockShape(candidateShape, fallback))
        continue;
      if (selected && selected != candidateShape) {
        ambiguous = true;
        return;
      }
      selected = candidateShape;
      return;
    }
  });

  return selected && !ambiguous ? selected : fallback;
}

ArrayAttr getDepPhysicalBlockShapeAttr(CodeletOp codelet, unsigned depIndex) {
  ArrayAttr fallback = getDepBasePhysicalBlockShapeAttr(codelet, depIndex);
  return getSiblingWriterBlockShapeOr(codelet, depIndex, fallback);
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
  std::optional<CodirAccessMode> mode = getDepAccessMode(codelet, depIndex);
  if (!mode)
    return CodirCollectiveKind::none;

  if (codirAccessMayRead(*mode) && isStencilReadHaloCandidate(codelet) &&
      depHasBlockStoragePlan(codelet, depIndex) &&
      depHasShiftedOwnerDimRead(codelet, depIndex))
    return CodirCollectiveKind::halo;

  // The all-gather and cross-owner reduce gates only fire for a dep the codelet
  // WRITES (the coarse copy-out producer); the materializer evaluates them on
  // write-mode participants only. Mirror that here so a read dep that happens
  // to share a root with a written one is never mislabeled.
  if (!codirAccessMayWrite(*mode))
    return CodirCollectiveKind::none;

  if (depHasLayoutMismatchEvidence(codelet, depIndex, *mode)) {
    if (isStencilCollectiveLike(codelet)) {
      return CodirCollectiveKind::none;
    }
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
  return CodirCollectiveKind::none;
}

} // namespace mlir::carts::codir
