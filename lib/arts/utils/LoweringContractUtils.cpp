///==========================================================================///
/// File: LoweringContractUtils.cpp
///
/// Utilities for first-class lowering contracts attached to DB pointer values.
/// These helpers let pattern detection passes emit one contract and let
/// lowering/partitioning passes consume it without rediscovering benchmark-
/// specific semantics from scattered attrs.
///==========================================================================///

#include "arts/utils/LoweringContractUtils.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/Operation.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

static SmallVector<Value, 4> materializeIndexValues(OpBuilder &builder,
                                                    Location loc,
                                                    ArrayRef<int64_t> values) {
  SmallVector<Value, 4> result;
  result.reserve(values.size());
  for (int64_t value : values)
    result.push_back(arts::createConstantIndex(builder, loc, value));
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

static bool valueDominatesInsertion(Value value, OpBuilder &builder) {
  if (!value)
    return false;

  Block *insertBlock = builder.getInsertionBlock();
  if (!insertBlock)
    return false;

  auto insertPt = builder.getInsertionPoint();
  if (auto blockArg = dyn_cast<BlockArgument>(value)) {
    if (blockArg.getOwner() == insertBlock)
      return true;
  } else if (Operation *defOp = value.getDefiningOp()) {
    if (defOp->getBlock() == insertBlock) {
      if (insertPt == insertBlock->end())
        return true;
      return defOp->isBeforeInBlock(&*insertPt);
    }
  }

  Operation *insertOp = insertPt != insertBlock->end()
                            ? &*insertPt
                            : insertBlock->getTerminator();
  if (!insertOp)
    return false;

  auto func = insertOp->getParentOfType<func::FuncOp>();
  if (!func)
    return false;

  DominanceInfo domInfo(func);
  return domInfo.dominates(value, insertOp);
}

static SmallVector<Value, 4>
materializeDominatingIndexValues(OpBuilder &builder, Location loc,
                                 ArrayRef<Value> values,
                                 ArrayRef<int64_t> staticValues) {
  SmallVector<Value, 4> result;
  result.reserve(values.size());

  for (Value value : values) {
    if (!value)
      return {};
    if (valueDominatesInsertion(value, builder)) {
      result.push_back(value);
      continue;
    }

    int64_t constant = 0;
    if (!ValueAnalysis::getConstantIndex(value, constant))
      return {};
    result.push_back(arts::createConstantIndex(builder, loc, constant));
  }

  if (!result.empty())
    return result;

  if (!staticValues.empty())
    return materializeIndexValues(builder, loc, staticValues);

  return {};
}

} // namespace

ContractKind LoweringContractInfo::getEffectiveKind() const {
  if (kind != ContractKind::Unknown)
    return kind;
  if (!depPattern)
    return ContractKind::Unknown;
  switch (*depPattern) {
  case ArtsDepPattern::stencil:
  case ArtsDepPattern::stencil_tiling_nd:
  case ArtsDepPattern::cross_dim_stencil_3d:
  case ArtsDepPattern::higher_order_stencil:
  case ArtsDepPattern::wavefront_2d:
  case ArtsDepPattern::jacobi_alternating_buffers:
    return ContractKind::Stencil;
  case ArtsDepPattern::uniform:
  case ArtsDepPattern::elementwise_pipeline:
    return ContractKind::Elementwise;
  case ArtsDepPattern::matmul:
    return ContractKind::Matmul;
  case ArtsDepPattern::triangular:
    return ContractKind::Triangular;
  case ArtsDepPattern::unknown:
    return ContractKind::Unknown;
  }
  return ContractKind::Unknown;
}

bool LoweringContractInfo::usesStencilDistribution() const {
  return distributionPattern &&
         *distributionPattern == EdtDistributionPattern::stencil;
}

bool LoweringContractInfo::hasExplicitStencilContract() const {
  /// A dep pattern alone is not enough to treat the IR as carrying an
  /// authoritative stencil contract. Consumers may only rely on explicit
  /// stencil semantics once the pattern pipeline or post-DB materialization
  /// has attached concrete ownership / halo / block-shape information.
  if (!isStencilFamily())
    return false;
  return hasOwnerDims() || !spatialDims.empty() ||
         !stencilIndependentDims.empty() || !blockShape.empty() ||
         !minOffsets.empty() || !maxOffsets.empty() ||
         !writeFootprint.empty() || !staticBlockShape.empty() ||
         !staticMinOffsets.empty() || !staticMaxOffsets.empty() ||
         !staticWriteFootprint.empty();
}

bool LoweringContractInfo::supportsBlockHalo() const {
  return supportedBlockHalo && hasExplicitStencilContract();
}

std::optional<SmallVector<int64_t, 4>>
LoweringContractInfo::getStaticBlockShape() const {
  if (auto dynamic = readConstantIndexValues(blockShape))
    return dynamic;
  if (!staticBlockShape.empty())
    return staticBlockShape;
  return std::nullopt;
}

std::optional<SmallVector<int64_t, 4>>
LoweringContractInfo::getStaticMinOffsets() const {
  if (auto dynamic = readConstantIndexValues(minOffsets))
    return dynamic;
  if (!staticMinOffsets.empty())
    return staticMinOffsets;
  return std::nullopt;
}

std::optional<SmallVector<int64_t, 4>>
LoweringContractInfo::getStaticMaxOffsets() const {
  if (auto dynamic = readConstantIndexValues(maxOffsets))
    return dynamic;
  if (!staticMaxOffsets.empty())
    return staticMaxOffsets;
  return std::nullopt;
}

static SmallVector<LoweringContractOp>
collectLoweringContractOps(Value target) {
  SmallVector<LoweringContractOp> result;
  if (!target)
    return result;
  for (auto *user : target.getUsers()) {
    if (auto contract = dyn_cast<LoweringContractOp>(user)) {
      if (contract.getTarget() == target)
        result.push_back(contract);
    }
  }
  return result;
}

LoweringContractOp mlir::arts::getLoweringContractOp(Value target) {
  auto contracts = collectLoweringContractOps(target);
  return contracts.empty() ? nullptr : contracts.front();
}

std::optional<LoweringContractInfo>
mlir::arts::getLoweringContract(Value target) {
  Value originalTarget = target;
  auto resolveContractTarget = [&](Value value) -> Value {
    if (!value)
      return Value();
    if (auto contract = getLoweringContractOp(value))
      return contract.getTarget();

    if (auto acquire = value.getDefiningOp<DbAcquireOp>()) {
      if (auto sourcePtr = acquire.getSourcePtr()) {
        if (getLoweringContractOp(sourcePtr))
          return sourcePtr;
      }
    }

    if (Operation *allocOp = DbUtils::getUnderlyingDbAlloc(value)) {
      if (auto alloc = dyn_cast<DbAllocOp>(allocOp)) {
        if (auto allocPtr = alloc.getPtr()) {
          if (getLoweringContractOp(allocPtr))
            return allocPtr;
        }
      }
    }

    return value;
  };

  auto contract = getLoweringContractOp(resolveContractTarget(target));
  if (!contract)
    return [&]() -> std::optional<LoweringContractInfo> {
      if (auto *defOp = originalTarget.getDefiningOp()) {
        if (auto semantic = getSemanticContract(defOp))
          return *semantic;
      }
      return std::nullopt;
    }();

  LoweringContractInfo info;
  if (auto depPattern = contract.getDepPattern())
    info.depPattern = *depPattern;
  if (auto distributionKind = contract.getDistributionKind())
    info.distributionKind = *distributionKind;
  if (auto distributionPattern = contract.getDistributionPattern())
    info.distributionPattern = *distributionPattern;
  if (auto version = contract.getDistributionVersion())
    info.distributionVersion = static_cast<int64_t>(*version);
  if (auto ownerDims = contract.getOwnerDims())
    info.ownerDims = SmallVector<int64_t, 4>(*ownerDims);
  if (auto spatialDims = contract.getSpatialDims())
    info.spatialDims = SmallVector<int64_t, 4>(*spatialDims);
  info.blockShape.assign(contract.getBlockShape().begin(),
                         contract.getBlockShape().end());
  info.minOffsets.assign(contract.getMinOffsets().begin(),
                         contract.getMinOffsets().end());
  info.maxOffsets.assign(contract.getMaxOffsets().begin(),
                         contract.getMaxOffsets().end());
  info.writeFootprint.assign(contract.getWriteFootprint().begin(),
                             contract.getWriteFootprint().end());
  info.supportedBlockHalo = contract.getSupportedBlockHalo().value_or(false);
  if (auto stencilIndependentDims = contract.getStencilIndependentDims())
    info.stencilIndependentDims =
        SmallVector<int64_t, 4>(*stencilIndependentDims);
  if (auto esdByteOffset = contract.getEsdByteOffset())
    info.esdByteOffset = static_cast<int64_t>(*esdByteOffset);
  if (auto esdByteSize = contract.getEsdByteSize())
    info.esdByteSize = static_cast<int64_t>(*esdByteSize);
  if (auto cachedStartBlock = contract.getCachedStartBlock())
    info.cachedStartBlock = static_cast<int64_t>(*cachedStartBlock);
  if (auto cachedBlockCount = contract.getCachedBlockCount())
    info.cachedBlockCount = static_cast<int64_t>(*cachedBlockCount);
  info.postDbRefined = contract.getPostDbRefined().value_or(false);
  if (auto estimatedTaskCost = contract.getEstimatedTaskCost())
    info.estimatedTaskCost = static_cast<int64_t>(*estimatedTaskCost);
  if (auto criticalPathDistance = contract.getCriticalPathDistance())
    info.criticalPathDistance = static_cast<int64_t>(*criticalPathDistance);
  if (auto contractKind = contract.getContractKind())
    info.kind = static_cast<ContractKind>(*contractKind);

  if (auto *defOp = originalTarget.getDefiningOp()) {
    if (auto semantic = getSemanticContract(defOp))
      mergeLoweringContractInfo(info, *semantic);
  }
  return info;
}

std::optional<LoweringContractInfo>
mlir::arts::getSemanticContract(Operation *op) {
  if (!op)
    return std::nullopt;

  LoweringContractInfo info;
  info.depPattern = getDepPattern(op);
  info.distributionKind = getEdtDistributionKind(op);
  info.distributionPattern = getEdtDistributionPattern(op);
  if (auto version = op->getAttrOfType<IntegerAttr>(
          AttrNames::Operation::DistributionVersion))
    info.distributionVersion = version.getInt();
  if (auto ownerDims = getStencilOwnerDims(op))
    info.ownerDims.assign(ownerDims->begin(), ownerDims->end());
  if (auto spatialDims = getStencilSpatialDims(op))
    info.spatialDims.assign(spatialDims->begin(), spatialDims->end());
  if (auto blockShape = getStencilBlockShape(op))
    info.staticBlockShape.assign(blockShape->begin(), blockShape->end());
  if (auto minOffsets = getStencilMinOffsets(op))
    info.staticMinOffsets.assign(minOffsets->begin(), minOffsets->end());
  if (auto maxOffsets = getStencilMaxOffsets(op))
    info.staticMaxOffsets.assign(maxOffsets->begin(), maxOffsets->end());
  if (auto writeFootprint = getStencilWriteFootprint(op)) {
    info.staticWriteFootprint.assign(writeFootprint->begin(),
                                     writeFootprint->end());
  }
  info.supportedBlockHalo = hasSupportedBlockHalo(op);
  if (info.empty())
    return std::nullopt;
  return info;
}

std::optional<LoweringContractInfo>
mlir::arts::getLoweringContract(Operation *op, OpBuilder &builder,
                                Location loc) {
  if (!op)
    return std::nullopt;

  LoweringContractInfo info =
      getSemanticContract(op).value_or(LoweringContractInfo{});
  if (auto blockShape = getStencilBlockShape(op))
    info.blockShape = materializeIndexValues(builder, loc, *blockShape);
  if (auto minOffsets = getStencilMinOffsets(op))
    info.minOffsets = materializeIndexValues(builder, loc, *minOffsets);
  if (auto maxOffsets = getStencilMaxOffsets(op))
    info.maxOffsets = materializeIndexValues(builder, loc, *maxOffsets);
  if (auto footprint = getStencilWriteFootprint(op))
    info.writeFootprint = materializeIndexValues(builder, loc, *footprint);

  if (info.empty())
    return std::nullopt;
  return info;
}

void mlir::arts::mergeLoweringContractInfo(LoweringContractInfo &dest,
                                           const LoweringContractInfo &src) {
  if (dest.kind == ContractKind::Unknown && src.kind != ContractKind::Unknown)
    dest.kind = src.kind;
  if (!dest.depPattern && src.depPattern)
    dest.depPattern = src.depPattern;
  if (!dest.distributionKind && src.distributionKind)
    dest.distributionKind = src.distributionKind;
  if (!dest.distributionPattern && src.distributionPattern)
    dest.distributionPattern = src.distributionPattern;
  if (!dest.distributionVersion && src.distributionVersion)
    dest.distributionVersion = src.distributionVersion;

  auto shouldTakeHigherRank = [](size_t current, size_t incoming) -> bool {
    if (incoming == 0)
      return false;
    return current == 0 || incoming > current;
  };

  if (shouldTakeHigherRank(dest.ownerDims.size(), src.ownerDims.size()))
    dest.ownerDims.assign(src.ownerDims.begin(), src.ownerDims.end());
  if (shouldTakeHigherRank(dest.spatialDims.size(), src.spatialDims.size()))
    dest.spatialDims.assign(src.spatialDims.begin(), src.spatialDims.end());

  if (shouldTakeHigherRank(dest.blockShape.size(), src.blockShape.size()))
    dest.blockShape.assign(src.blockShape.begin(), src.blockShape.end());
  if (shouldTakeHigherRank(dest.minOffsets.size(), src.minOffsets.size()))
    dest.minOffsets.assign(src.minOffsets.begin(), src.minOffsets.end());
  if (shouldTakeHigherRank(dest.maxOffsets.size(), src.maxOffsets.size()))
    dest.maxOffsets.assign(src.maxOffsets.begin(), src.maxOffsets.end());
  if (shouldTakeHigherRank(dest.writeFootprint.size(),
                           src.writeFootprint.size()))
    dest.writeFootprint.assign(src.writeFootprint.begin(),
                               src.writeFootprint.end());

  if (shouldTakeHigherRank(dest.staticBlockShape.size(),
                           src.staticBlockShape.size()))
    dest.staticBlockShape.assign(src.staticBlockShape.begin(),
                                 src.staticBlockShape.end());
  if (shouldTakeHigherRank(dest.staticMinOffsets.size(),
                           src.staticMinOffsets.size()))
    dest.staticMinOffsets.assign(src.staticMinOffsets.begin(),
                                 src.staticMinOffsets.end());
  if (shouldTakeHigherRank(dest.staticMaxOffsets.size(),
                           src.staticMaxOffsets.size()))
    dest.staticMaxOffsets.assign(src.staticMaxOffsets.begin(),
                                 src.staticMaxOffsets.end());
  if (shouldTakeHigherRank(dest.staticWriteFootprint.size(),
                           src.staticWriteFootprint.size()))
    dest.staticWriteFootprint.assign(src.staticWriteFootprint.begin(),
                                     src.staticWriteFootprint.end());

  dest.supportedBlockHalo = dest.supportedBlockHalo || src.supportedBlockHalo;

  if (dest.stencilIndependentDims.empty() &&
      !src.stencilIndependentDims.empty()) {
    dest.stencilIndependentDims.assign(src.stencilIndependentDims.begin(),
                                       src.stencilIndependentDims.end());
  }

  if (!dest.esdByteOffset && src.esdByteOffset)
    dest.esdByteOffset = src.esdByteOffset;
  if (!dest.esdByteSize && src.esdByteSize)
    dest.esdByteSize = src.esdByteSize;
  if (!dest.cachedStartBlock && src.cachedStartBlock)
    dest.cachedStartBlock = src.cachedStartBlock;
  if (!dest.cachedBlockCount && src.cachedBlockCount)
    dest.cachedBlockCount = src.cachedBlockCount;
  dest.postDbRefined = dest.postDbRefined || src.postDbRefined;
  if (!dest.estimatedTaskCost && src.estimatedTaskCost)
    dest.estimatedTaskCost = src.estimatedTaskCost;
  if (!dest.criticalPathDistance && src.criticalPathDistance)
    dest.criticalPathDistance = src.criticalPathDistance;

  normalizeLoweringContractInfo(dest);
}

void mlir::arts::normalizeLoweringContractInfo(LoweringContractInfo &info) {
  const size_t expectedRank = info.ownerDims.size();
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

  clearMismatchedDynamic(info.blockShape);
  clearMismatchedDynamic(info.minOffsets);
  clearMismatchedDynamic(info.maxOffsets);
  clearMismatchedDynamic(info.writeFootprint);

  clearMismatchedStatic(info.staticBlockShape);
  clearMismatchedStatic(info.staticMinOffsets);
  clearMismatchedStatic(info.staticMaxOffsets);
  clearMismatchedStatic(info.staticWriteFootprint);
}

AcquireRewriteContract
mlir::arts::deriveAcquireRewriteContract(DbAcquireOp acquire) {
  AcquireRewriteContract contract;
  auto info = getLoweringContract(acquire.getPtr());
  if (!info)
    return contract;

  contract.ownerDims.assign(info->ownerDims.begin(), info->ownerDims.end());

  if (auto minOffsets = info->getStaticMinOffsets())
    contract.haloMinOffsets = *minOffsets;
  if (auto maxOffsets = info->getStaticMaxOffsets())
    contract.haloMaxOffsets = *maxOffsets;

  contract.applyStencilHalo =
      acquire.getMode() == ArtsMode::in && info->supportsBlockHalo();
  /// Halo-aware stencil reads keep the authoritative dependency window in the
  /// rewritten acquire's offsets/sizes. partition_* remains the owner slice and
  /// would drop the left/right halo if used as the block-dependency window.
  const bool hasExplicitStencilContract = info->hasExplicitStencilContract();
  contract.usePartitionSliceAsDepWindow =
      (acquire.getMode() == ArtsMode::in && hasExplicitStencilContract &&
       !contract.applyStencilHalo &&
       acquire.getPartitionMode().value_or(PartitionMode::coarse) ==
           PartitionMode::stencil) ||
      (acquire.getMode() == ArtsMode::inout && info->depPattern &&
       *info->depPattern == ArtsDepPattern::wavefront_2d &&
       info->supportsBlockHalo());
  contract.preserveParentDepRange = acquire.getMode() == ArtsMode::in &&
                                    !contract.applyStencilHalo &&
                                    !hasExplicitStencilContract;
  return contract;
}

SmallVector<unsigned, 4>
mlir::arts::resolveContractOwnerDims(const LoweringContractInfo &info,
                                     unsigned rank) {
  SmallVector<unsigned, 4> dims;
  dims.reserve(rank);
  for (int64_t dim : info.ownerDims) {
    if (dim < 0)
      continue;
    unsigned converted = static_cast<unsigned>(dim);
    if (converted >= rank)
      continue;
    dims.push_back(converted);
    if (dims.size() == rank)
      break;
  }
  if (dims.empty()) {
    for (unsigned dim = 0; dim < rank; ++dim)
      dims.push_back(dim);
  }
  return dims;
}

bool mlir::arts::prefersContractNDBlock(const LoweringContractInfo &info,
                                        unsigned requiredRank) {
  if (!info.supportsBlockHalo())
    return false;
  auto staticBlockShape = info.getStaticBlockShape();
  if (info.ownerDims.size() < requiredRank ||
      ((!staticBlockShape || staticBlockShape->size() < requiredRank) &&
       info.blockShape.size() < requiredRank))
    return false;
  return true;
}

LoweringContractOp
mlir::arts::upsertLoweringContract(OpBuilder &builder, Location loc,
                                   Value target,
                                   const LoweringContractInfo &info) {
  if (!target || info.empty())
    return nullptr;

  LoweringContractInfo normalizedInfo = info;
  normalizeLoweringContractInfo(normalizedInfo);

  auto staleContracts = collectLoweringContractOps(target);

  SmallVector<Value> blockShapeVals = materializeDominatingIndexValues(
      builder, loc, normalizedInfo.blockShape, normalizedInfo.staticBlockShape);
  SmallVector<Value> minOffsetVals = materializeDominatingIndexValues(
      builder, loc, normalizedInfo.minOffsets, normalizedInfo.staticMinOffsets);
  SmallVector<Value> maxOffsetVals = materializeDominatingIndexValues(
      builder, loc, normalizedInfo.maxOffsets, normalizedInfo.staticMaxOffsets);
  SmallVector<Value> writeFootprintVals = materializeDominatingIndexValues(
      builder, loc, normalizedInfo.writeFootprint,
      normalizedInfo.staticWriteFootprint);

  LoweringContractInfo materializedInfo = normalizedInfo;
  materializedInfo.blockShape = std::move(blockShapeVals);
  materializedInfo.minOffsets = std::move(minOffsetVals);
  materializedInfo.maxOffsets = std::move(maxOffsetVals);
  materializedInfo.writeFootprint = std::move(writeFootprintVals);

  auto contract =
      builder.create<LoweringContractOp>(loc, target, materializedInfo);

  for (auto stale : staleContracts)
    if (stale.getOperation() != contract.getOperation())
      stale->erase();

  return contract;
}

void mlir::arts::transferOperationContract(
    Operation *source, Operation *target,
    OperationContractTransferPolicy policy) {
  if (!source || !target)
    return;

  if (policy ==
      OperationContractTransferPolicy::IncludeEffectiveDepPatternFallback) {
    copyContractAttrs(source, target);
    return;
  }
  copySemanticContractAttrs(source, target);
}

void mlir::arts::transferLoweringContract(Operation *source, Value target,
                                          OpBuilder &builder, Location loc) {
  if (!source || !target)
    return;

  if (auto info = getLoweringContract(source, builder, loc))
    upsertLoweringContract(builder, loc, target, *info);
}

void mlir::arts::transferValueContract(Value source, Value target,
                                       OpBuilder &builder, Location loc) {
  copyLoweringContract(source, target, builder, loc);
}

void mlir::arts::transferContract(
    Operation *sourceOp, Operation *targetOp, Value sourceContractTarget,
    Value targetContractTarget, OpBuilder &builder, Location loc,
    OperationContractTransferPolicy policy) {
  transferOperationContract(sourceOp, targetOp, policy);
  transferValueContract(sourceContractTarget, targetContractTarget, builder,
                        loc);
}

void mlir::arts::copyLoweringContract(Value source, Value target,
                                      OpBuilder &builder, Location loc) {
  if (!source || !target || source == target)
    return;
  auto info = getLoweringContract(source);
  if (!info)
    return;
  upsertLoweringContract(builder, loc, target, *info);
}
