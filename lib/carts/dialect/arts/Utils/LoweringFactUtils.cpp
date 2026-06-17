///==========================================================================///
/// File: LoweringFactUtils.cpp
///
/// Utilities for querying ARTS lowering facts from the current IR.
///==========================================================================///

#include "carts/dialect/arts/Utils/LoweringFactUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/StencilAttributes.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/ArrayRef.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

static SmallVector<Value, 4> materializeIndexValues(OpBuilder &builder,
                                                    Location loc,
                                                    ArrayRef<int64_t> values) {
  SmallVector<Value, 4> result;
  result.reserve(values.size());
  for (int64_t value : values)
    result.push_back(::mlir::carts::createConstantIndex(builder, loc, value));
  return result;
}

static std::optional<SmallVector<int64_t, 4>>
readConstantIndexValues(ValueRange values) {
  SmallVector<int64_t, 4> result;
  result.reserve(values.size());
  for (Value value : values) {
    int64_t constant = 0;
    if (!value || !ValueAnalysis::getConstantIndex(value, constant))
      return std::nullopt;
    result.push_back(constant);
  }
  return result;
}

static void mergeDbGridSpatialFacts(Operation *op, LoweringFactInfo &info) {
  if (!op || !info.hasDistributionFacts())
    return;

  std::optional<ArtsDbPhysicalLayout> layout;
  if (auto alloc = dyn_cast<DbAllocOp>(op))
    layout = readArtsDbPhysicalLayoutFromCommittedType(alloc);
  else
    layout = readArtsDbPhysicalLayout(op);
  if (!layout)
    return;

  if (info.spatial.ownerDims.empty())
    info.spatial.ownerDims.assign(layout->ownerDims.begin(),
                                  layout->ownerDims.end());

  if (info.spatial.staticBlockShape.empty() &&
      info.spatial.blockShape.empty()) {
    info.spatial.staticBlockShape.assign(layout->physicalBlockShape.begin(),
                                         layout->physicalBlockShape.end());
  }
}

static std::optional<EdtDistributionPattern>
deriveDistributionPatternFromKind(FactKind kind) {
  switch (kind) {
  case FactKind::Elementwise:
    return EdtDistributionPattern::uniform;
  case FactKind::Stencil:
    return EdtDistributionPattern::stencil;
  case FactKind::Matmul:
    return EdtDistributionPattern::matmul;
  case FactKind::Triangular:
    return EdtDistributionPattern::triangular;
  case FactKind::Unknown:
    return std::nullopt;
  }
  return std::nullopt;
}

} // namespace

FactKind LoweringFactInfo::getEffectiveKind() const {
  if (pattern.kind != FactKind::Unknown)
    return pattern.kind;
  if (!pattern.depPattern) {
    if (usesStencilDistribution())
      return FactKind::Stencil;
    return FactKind::Unknown;
  }
  switch (*pattern.depPattern) {
  case ArtsDepPattern::stencil:
  case ArtsDepPattern::stencil_tiling_nd:
  case ArtsDepPattern::cross_dim_stencil_3d:
  case ArtsDepPattern::higher_order_stencil:
  case ArtsDepPattern::wavefront_2d:
  case ArtsDepPattern::alternating_buffer_stencil:
    return FactKind::Stencil;
  case ArtsDepPattern::uniform:
  case ArtsDepPattern::elementwise_pipeline:
  case ArtsDepPattern::reduction:
    return FactKind::Elementwise;
  case ArtsDepPattern::matmul:
    return FactKind::Matmul;
  case ArtsDepPattern::triangular:
    return FactKind::Triangular;
  case ArtsDepPattern::unknown:
    return FactKind::Unknown;
  }
  return FactKind::Unknown;
}

bool LoweringFactInfo::usesStencilDistribution() const {
  return pattern.distributionPattern &&
         *pattern.distributionPattern == EdtDistributionPattern::stencil;
}

bool LoweringFactInfo::hasExplicitStencilFacts() const {
  /// A dep pattern alone is not enough to treat the IR as carrying
  /// authoritative stencil facts. Consumers may only rely on explicit stencil
  /// semantics once the pattern pipeline or post-DB refinement has attached
  /// concrete ownership / halo / block-shape information.
  if (!isStencilFamily())
    return false;
  return hasOwnerDims() || !spatial.stencilIndependentDims.empty() ||
         !spatial.blockShape.empty() || !spatial.minOffsets.empty() ||
         !spatial.maxOffsets.empty() || !spatial.staticBlockShape.empty() ||
         !spatial.staticMinOffsets.empty() || !spatial.staticMaxOffsets.empty();
}

bool LoweringFactInfo::supportsBlockHalo() const {
  return spatial.supportedBlockHalo && hasExplicitStencilFacts();
}

std::optional<EdtDistributionPattern>
LoweringFactInfo::getEffectiveDistributionPattern() const {
  if (pattern.distributionPattern &&
      *pattern.distributionPattern != EdtDistributionPattern::unknown)
    return pattern.distributionPattern;
  if (pattern.depPattern)
    if (auto derived = getDistributionPatternForDepPattern(*pattern.depPattern))
      return derived;
  return deriveDistributionPatternFromKind(getEffectiveKind());
}

bool LoweringFactInfo::isWavefrontFamily() const {
  return pattern.depPattern &&
         *pattern.depPattern == ArtsDepPattern::wavefront_2d;
}

bool LoweringFactInfo::prefersSemanticOwnerLayoutPreservation() const {
  return hasOwnerDims() && supportsBlockHalo();
}

bool LoweringFactInfo::isWavefrontStencilFacts() const {
  return isWavefrontFamily() && hasExplicitStencilFacts() &&
         supportsBlockHalo();
}

bool LoweringFactInfo::prefersNDBlock(unsigned requiredRank) const {
  if (!supportsBlockHalo())
    return false;
  auto staticShape = getStaticBlockShape();
  if (spatial.ownerDims.size() < requiredRank ||
      ((!staticShape || staticShape->size() < requiredRank) &&
       spatial.blockShape.size() < requiredRank))
    return false;
  return true;
}

std::optional<SmallVector<int64_t, 4>>
LoweringFactInfo::getStaticBlockShape() const {
  if (auto dynamic = readConstantIndexValues(spatial.blockShape))
    return dynamic;
  if (!spatial.staticBlockShape.empty())
    return spatial.staticBlockShape;
  return std::nullopt;
}

std::optional<SmallVector<int64_t, 4>>
LoweringFactInfo::getStaticMinOffsets() const {
  if (auto dynamic = readConstantIndexValues(spatial.minOffsets))
    return dynamic;
  if (!spatial.staticMinOffsets.empty())
    return spatial.staticMinOffsets;
  return std::nullopt;
}

std::optional<SmallVector<int64_t, 4>>
LoweringFactInfo::getStaticMaxOffsets() const {
  if (auto dynamic = readConstantIndexValues(spatial.maxOffsets))
    return dynamic;
  if (!spatial.staticMaxOffsets.empty())
    return spatial.staticMaxOffsets;
  return std::nullopt;
}

std::optional<LoweringFactInfo>
mlir::carts::arts::getLoweringFacts(Value target) {
  LoweringFactInfo info;
  bool sawInfo = false;

  auto mergeIfPresent = [&](std::optional<LoweringFactInfo> maybeInfo) {
    if (!maybeInfo)
      return;
    mergeLoweringFactInfo(info, *maybeInfo);
    sawInfo = true;
  };

  if (auto *defOp = target.getDefiningOp()) {
    mergeIfPresent(getSemanticFacts(defOp));
    if (auto acquire = dyn_cast<DbAcquireOp>(defOp))
      mergeIfPresent(getLoweringFacts(acquire.getSourcePtr()));
  }

  if (Operation *allocOp = DbUtils::getUnderlyingDbAlloc(target)) {
    if (auto alloc = dyn_cast<DbAllocOp>(allocOp)) {
      mergeIfPresent(getSemanticFacts(alloc.getOperation()));
    }
  }

  if (!sawInfo)
    return std::nullopt;
  return info;
}

std::optional<LoweringFactInfo>
mlir::carts::arts::getSemanticFacts(Operation *op) {
  if (!op)
    return std::nullopt;

  LoweringFactInfo info;
  info.pattern.depPattern = getDepPattern(op);
  info.pattern.distributionKind = getEdtDistributionKind(op);
  info.pattern.distributionPattern = getEdtDistributionPattern(op);
  if (auto version = getDistributionVersionAttr(op))
    info.pattern.distributionVersion = version.getInt();
  if (auto ownerDims = getStencilOwnerDims(op))
    info.spatial.ownerDims.assign(ownerDims->begin(), ownerDims->end());
  if (auto minOffsets = getStencilMinOffsets(op))
    info.spatial.staticMinOffsets.assign(minOffsets->begin(),
                                         minOffsets->end());
  if (auto maxOffsets = getStencilMaxOffsets(op))
    info.spatial.staticMaxOffsets.assign(maxOffsets->begin(),
                                         maxOffsets->end());
  if (auto spatialDims = getStencilSpatialDims(op))
    info.spatial.spatialDims.assign(spatialDims->begin(), spatialDims->end());
  info.spatial.centerOffset = getStencilCenterOffset(op);
  // Only propagate supportedBlockHalo when the depPattern is stencil-family;
  // otherwise an op that carries mixed attrs (e.g. uniform depPattern +
  // residual stencil spatial attrs from a sibling loop) would produce an
  // inconsistent lowering fact that fails verification.
  info.spatial.supportedBlockHalo =
      hasSupportedBlockHalo(op) && info.pattern.depPattern &&
      isStencilFamilyDepPattern(*info.pattern.depPattern);
  info.analysis.narrowableDep =
      op->hasAttr(::mlir::carts::arts::AttrNames::Semantic::NarrowableDep);
  mergeDbGridSpatialFacts(op, info);
  if (info.empty())
    return std::nullopt;
  return info;
}

std::optional<LoweringFactInfo>
mlir::carts::arts::getLoweringFacts(Operation *op, OpBuilder &builder,
                                    Location loc) {
  if (!op)
    return std::nullopt;

  LoweringFactInfo info = getSemanticFacts(op).value_or(LoweringFactInfo{});
  if (auto minOffsets = getStencilMinOffsets(op))
    info.spatial.minOffsets = materializeIndexValues(builder, loc, *minOffsets);
  if (auto maxOffsets = getStencilMaxOffsets(op))
    info.spatial.maxOffsets = materializeIndexValues(builder, loc, *maxOffsets);
  if (auto writeFootprint = getStencilWriteFootprint(op))
    info.spatial.writeFootprint =
        materializeIndexValues(builder, loc, *writeFootprint);

  if (info.empty())
    return std::nullopt;
  return info;
}

FactMergeChange
mlir::carts::arts::mergeLoweringFactInfo(LoweringFactInfo &dest,
                                         const LoweringFactInfo &src) {
  bool changed = false;

  if (dest.pattern.kind == FactKind::Unknown &&
      src.pattern.kind != FactKind::Unknown) {
    dest.pattern.kind = src.pattern.kind;
    changed = true;
  }
  if (!dest.pattern.depPattern && src.pattern.depPattern) {
    dest.pattern.depPattern = src.pattern.depPattern;
    changed = true;
  }
  if (!dest.pattern.distributionKind && src.pattern.distributionKind) {
    dest.pattern.distributionKind = src.pattern.distributionKind;
    changed = true;
  }
  if (!dest.pattern.distributionPattern && src.pattern.distributionPattern) {
    dest.pattern.distributionPattern = src.pattern.distributionPattern;
    changed = true;
  }
  if (!dest.pattern.distributionVersion && src.pattern.distributionVersion) {
    dest.pattern.distributionVersion = src.pattern.distributionVersion;
    changed = true;
  }
  if (!dest.analysis.narrowableDep && src.analysis.narrowableDep) {
    dest.analysis.narrowableDep = true;
    changed = true;
  }

  auto shouldTakeHigherRank = [](size_t current, size_t incoming) -> bool {
    if (incoming == 0)
      return false;
    return current == 0 || incoming > current;
  };

  if (shouldTakeHigherRank(dest.spatial.ownerDims.size(),
                           src.spatial.ownerDims.size())) {
    dest.spatial.ownerDims.assign(src.spatial.ownerDims.begin(),
                                  src.spatial.ownerDims.end());
    changed = true;
  }
  if (!dest.spatial.centerOffset && src.spatial.centerOffset) {
    dest.spatial.centerOffset = src.spatial.centerOffset;
    changed = true;
  }

  if (shouldTakeHigherRank(dest.spatial.blockShape.size(),
                           src.spatial.blockShape.size())) {
    dest.spatial.blockShape.assign(src.spatial.blockShape.begin(),
                                   src.spatial.blockShape.end());
    changed = true;
  }
  if (shouldTakeHigherRank(dest.spatial.minOffsets.size(),
                           src.spatial.minOffsets.size())) {
    dest.spatial.minOffsets.assign(src.spatial.minOffsets.begin(),
                                   src.spatial.minOffsets.end());
    changed = true;
  }
  if (shouldTakeHigherRank(dest.spatial.maxOffsets.size(),
                           src.spatial.maxOffsets.size())) {
    dest.spatial.maxOffsets.assign(src.spatial.maxOffsets.begin(),
                                   src.spatial.maxOffsets.end());
    changed = true;
  }
  if (shouldTakeHigherRank(dest.spatial.writeFootprint.size(),
                           src.spatial.writeFootprint.size())) {
    dest.spatial.writeFootprint.assign(src.spatial.writeFootprint.begin(),
                                       src.spatial.writeFootprint.end());
    changed = true;
  }

  if (shouldTakeHigherRank(dest.spatial.staticBlockShape.size(),
                           src.spatial.staticBlockShape.size())) {
    dest.spatial.staticBlockShape.assign(src.spatial.staticBlockShape.begin(),
                                         src.spatial.staticBlockShape.end());
    changed = true;
  }
  if (shouldTakeHigherRank(dest.spatial.staticMinOffsets.size(),
                           src.spatial.staticMinOffsets.size())) {
    dest.spatial.staticMinOffsets.assign(src.spatial.staticMinOffsets.begin(),
                                         src.spatial.staticMinOffsets.end());
    changed = true;
  }
  if (shouldTakeHigherRank(dest.spatial.staticMaxOffsets.size(),
                           src.spatial.staticMaxOffsets.size())) {
    dest.spatial.staticMaxOffsets.assign(src.spatial.staticMaxOffsets.begin(),
                                         src.spatial.staticMaxOffsets.end());
    changed = true;
  }

  // Only merge supportedBlockHalo when the effective depPattern is
  // stencil-family (either already set in dest or coming from src).
  auto effectiveDepPattern = dest.pattern.depPattern ? dest.pattern.depPattern
                                                     : src.pattern.depPattern;
  if (!dest.spatial.supportedBlockHalo && src.spatial.supportedBlockHalo &&
      effectiveDepPattern && isStencilFamilyDepPattern(*effectiveDepPattern)) {
    dest.spatial.supportedBlockHalo = true;
    changed = true;
  }

  if (dest.spatial.spatialDims.empty() && !src.spatial.spatialDims.empty()) {
    dest.spatial.spatialDims.assign(src.spatial.spatialDims.begin(),
                                    src.spatial.spatialDims.end());
    changed = true;
  }

  if (dest.spatial.stencilIndependentDims.empty() &&
      !src.spatial.stencilIndependentDims.empty()) {
    dest.spatial.stencilIndependentDims.assign(
        src.spatial.stencilIndependentDims.begin(),
        src.spatial.stencilIndependentDims.end());
    changed = true;
  }

  if (!dest.analysis.postDbRefined && src.analysis.postDbRefined) {
    dest.analysis.postDbRefined = true;
    changed = true;
  }
  normalizeLoweringFactInfo(dest);
  return changed ? FactMergeChange::Changed : FactMergeChange::Unchanged;
}

void mlir::carts::arts::normalizeLoweringFactInfo(LoweringFactInfo &info) {
  const size_t expectedRank = info.spatial.ownerDims.size();
  if (expectedRank == 0)
    return;

  auto clearMismatchedDynamic = [&](SmallVector<Value, 4> &values) {
    if (!values.empty() && values.size() != expectedRank)
      values.clear();
  };
  auto clearMismatchedStatic = [&](SmallVector<int64_t, 4> &values) {
    if (!values.empty() && values.size() != expectedRank)
      values.clear();
  };

  clearMismatchedDynamic(info.spatial.blockShape);
  clearMismatchedDynamic(info.spatial.minOffsets);
  clearMismatchedDynamic(info.spatial.maxOffsets);
  clearMismatchedDynamic(info.spatial.writeFootprint);

  clearMismatchedStatic(info.spatial.staticBlockShape);
  clearMismatchedStatic(info.spatial.staticMinOffsets);
  clearMismatchedStatic(info.spatial.staticMaxOffsets);
}

SmallVector<unsigned, 4>
mlir::carts::arts::resolveFactOwnerDims(const LoweringFactInfo &info,
                                        unsigned rank) {
  SmallVector<unsigned, 4> dims;
  dims.reserve(rank);
  for (int64_t dim : info.spatial.ownerDims) {
    if (dim < 0)
      continue;
    /// `rank` is the number of owner dimensions the caller wants to resolve,
    /// not an upper bound on the physical memref dimension number. Preserving
    /// the recorded memref dims is critical for N-D block partitions such as
    /// k-owned 3-D/4-D arrays, where ownerDims may legitimately be [2] or [3].
    dims.push_back(static_cast<unsigned>(dim));
    if (dims.size() == rank)
      break;
  }
  if (dims.empty()) {
    for (unsigned dim = 0; dim < rank; ++dim)
      dims.push_back(dim);
  }
  return dims;
}

std::optional<std::pair<SmallVector<int64_t, 4>, SmallVector<int64_t, 4>>>
mlir::carts::arts::projectHaloWindow(const LoweringFactInfo &facts) {
  auto minOffsets = facts.getStaticMinOffsets();
  auto maxOffsets = facts.getStaticMaxOffsets();
  if (!minOffsets || !maxOffsets)
    return std::nullopt;
  return std::make_pair(*minOffsets, *maxOffsets);
}

std::optional<LoweringFactInfo>
mlir::carts::arts::resolveAcquireFacts(DbAcquireOp acquire) {
  if (!acquire)
    return std::nullopt;

  auto info = getLoweringFacts(acquire.getPtr());
  if (auto alloc = dyn_cast_or_null<DbAllocOp>(
          DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr())))
    if (auto allocInfo = getLoweringFacts(alloc.getPtr())) {
      if (!info)
        info = *allocInfo;
      else
        mergeLoweringFactInfo(*info, *allocInfo);
    }
  return info;
}

bool mlir::carts::arts::shouldApplyStencilHalo(const LoweringFactInfo &facts,
                                               ArtsMode effectiveMode) {
  if (effectiveMode != ArtsMode::in)
    return false;
  return facts.supportsBlockHalo();
}

bool mlir::carts::arts::shouldApplyStencilHalo(const LoweringFactInfo &facts,
                                               DbAcquireOp acquire) {
  if (!acquire)
    return false;
  return shouldApplyStencilHalo(facts, acquire.getMode());
}

bool mlir::carts::arts::shouldUsePartitionSliceAsDepWindow(
    const LoweringFactInfo &facts, ArtsMode effectiveMode,
    PartitionMode partitionMode) {
  const bool hasExplicitStencilFacts = facts.hasExplicitStencilFacts();
  const bool applyStencilHalo = shouldApplyStencilHalo(facts, effectiveMode);

  if (effectiveMode == ArtsMode::in && hasExplicitStencilFacts &&
      !applyStencilHalo && partitionMode == PartitionMode::stencil)
    return true;

  if (effectiveMode == ArtsMode::inout && facts.isWavefrontStencilFacts())
    return true;

  return false;
}

bool mlir::carts::arts::shouldUsePartitionSliceAsDepWindow(
    const LoweringFactInfo &facts, DbAcquireOp acquire) {
  if (!acquire)
    return false;
  std::optional<PartitionMode> partitionMode = acquire.getPartitionMode();
  if (!partitionMode)
    return false;
  return shouldUsePartitionSliceAsDepWindow(facts, acquire.getMode(),
                                            *partitionMode);
}

bool mlir::carts::arts::shouldPreserveParentDepRange(
    const LoweringFactInfo &facts, ArtsMode effectiveMode) {
  if (effectiveMode != ArtsMode::in)
    return false;

  const bool applyStencilHalo = shouldApplyStencilHalo(facts, effectiveMode);
  const bool hasExplicitStencilFacts = facts.hasExplicitStencilFacts();
  const bool prefersWorkerLocalReadSlice = facts.analysis.narrowableDep;

  return !applyStencilHalo && !hasExplicitStencilFacts &&
         !prefersWorkerLocalReadSlice;
}

bool mlir::carts::arts::shouldPreserveParentDepRange(
    const LoweringFactInfo &facts, DbAcquireOp acquire) {
  if (!acquire)
    return false;
  return shouldPreserveParentDepRange(facts, acquire.getMode());
}

void mlir::carts::arts::transferOperationFacts(Operation *source,
                                               Operation *target) {
  if (!source || !target)
    return;
  copySemanticFactAttrs(source, target);
}

std::optional<ArtsDbPhysicalLayout>
mlir::carts::arts::readArtsDbPhysicalLayoutFromCommittedType(DbAllocOp alloc) {
  if (!alloc)
    return std::nullopt;

  auto partition = alloc.getPartitionMode();
  if (!partition || !usesBlockLayout(*partition) || alloc.getSizes().empty() ||
      alloc.getElementSizes().empty())
    return readArtsDbPhysicalLayout(alloc.getOperation());

  SmallVector<int64_t, 4> expandedShape;
  expandedShape.reserve(alloc.getSizes().size() +
                        alloc.getElementSizes().size());
  for (Value size : alloc.getSizes()) {
    std::optional<int64_t> constant = ValueAnalysis::tryFoldConstantIndex(
        ValueAnalysis::stripNumericCasts(size));
    if (!constant || *constant <= 0)
      return readArtsDbPhysicalLayout(alloc.getOperation());
    expandedShape.push_back(*constant);
  }
  for (Value size : alloc.getElementSizes()) {
    std::optional<int64_t> constant = ValueAnalysis::tryFoldConstantIndex(
        ValueAnalysis::stripNumericCasts(size));
    if (!constant || *constant <= 0)
      return readArtsDbPhysicalLayout(alloc.getOperation());
    expandedShape.push_back(*constant);
  }

  if (std::optional<sde::RecoveredMuPhysicalLayout> recovered =
          sde::recoverMuPhysicalLayoutFromExpandedShape(
              expandedShape, alloc.getElementType())) {
    ArtsDbPhysicalLayout layout;
    layout.ownerDims.reserve(recovered->ownerDims.size());
    for (unsigned dim : recovered->ownerDims)
      layout.ownerDims.push_back(static_cast<int64_t>(dim));
    layout.physicalBlockShape.assign(recovered->physicalBlockShape.begin(),
                                     recovered->physicalBlockShape.end());
    return layout;
  }

  return readArtsDbPhysicalLayout(alloc.getOperation());
}
