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
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinAttributes.h"
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

} // namespace

bool LoweringContractInfo::isStencilFamily() const {
  return depPattern && isStencilFamilyDepPattern(*depPattern);
}

bool LoweringContractInfo::usesStencilDistribution() const {
  return distributionPattern &&
         *distributionPattern == EdtDistributionPattern::stencil;
}

bool LoweringContractInfo::supportsBlockHalo() const {
  return supportedBlockHalo && (isStencilFamily() || usesStencilDistribution());
}

std::optional<SmallVector<int64_t, 4>>
LoweringContractInfo::getStaticMinOffsets() const {
  return readConstantIndexValues(minOffsets);
}

std::optional<SmallVector<int64_t, 4>>
LoweringContractInfo::getStaticMaxOffsets() const {
  return readConstantIndexValues(maxOffsets);
}

SmallVector<LoweringContractOp>
mlir::arts::collectLoweringContractOps(Value target) {
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
  auto contract = getLoweringContractOp(target);
  if (!contract)
    return std::nullopt;

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
    info.stencilIndependentDims = SmallVector<int64_t, 4>(*stencilIndependentDims);
  if (auto esdByteOffset = contract.getEsdByteOffset())
    info.esdByteOffset = static_cast<int64_t>(*esdByteOffset);
  if (auto esdByteSize = contract.getEsdByteSize())
    info.esdByteSize = static_cast<int64_t>(*esdByteSize);
  if (auto cachedStartBlock = contract.getCachedStartBlock())
    info.cachedStartBlock = static_cast<int64_t>(*cachedStartBlock);
  if (auto cachedBlockCount = contract.getCachedBlockCount())
    info.cachedBlockCount = static_cast<int64_t>(*cachedBlockCount);
  info.postDbRefined = contract.getPostDbRefined().value_or(false);
  if (auto inferredDbMode = contract.getInferredDbMode())
    info.inferredDbMode = static_cast<int64_t>(*inferredDbMode);
  if (auto estimatedTaskCost = contract.getEstimatedTaskCost())
    info.estimatedTaskCost = static_cast<int64_t>(*estimatedTaskCost);
  if (auto criticalPathDistance = contract.getCriticalPathDistance())
    info.criticalPathDistance = static_cast<int64_t>(*criticalPathDistance);
  return info;
}

std::optional<LoweringContractInfo> mlir::arts::getSemanticContract(Operation *op) {
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
  contract.usePartitionSliceAsDependencyWindow =
      (acquire.getMode() == ArtsMode::in &&
       (contract.applyStencilHalo ||
        acquire.getPartitionMode().value_or(PartitionMode::coarse) ==
            PartitionMode::stencil)) ||
      (acquire.getMode() == ArtsMode::inout && info->depPattern &&
       *info->depPattern == ArtsDepPattern::wavefront_2d &&
       info->supportsBlockHalo());

  const bool hasExplicitStencilContract = info->hasOwnerDims() ||
                                          info->usesStencilDistribution() ||
                                          info->isStencilFamily();
  contract.preserveParentDependencyRange = acquire.getMode() == ArtsMode::in &&
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
  if (info.ownerDims.size() < requiredRank ||
      info.blockShape.size() < requiredRank)
    return false;
  return true;
}

// TODO(REFACTOR): upsertLoweringContract forwards 20+ positional arguments
// matching the builder. This is fragile -- any field ordering change between
// LoweringContractInfo and build() will silently produce wrong results. Use a
// struct-based builder overload.
LoweringContractOp
mlir::arts::upsertLoweringContract(OpBuilder &builder, Location loc,
                                   Value target,
                                   const LoweringContractInfo &info) {
  if (!target || info.empty())
    return nullptr;

  auto staleContracts = collectLoweringContractOps(target);

  auto contract = builder.create<LoweringContractOp>(
      loc, target, info.depPattern, info.distributionKind,
      info.distributionPattern, info.distributionVersion,
      SmallVector<int64_t>(info.ownerDims.begin(), info.ownerDims.end()),
      SmallVector<int64_t>(info.spatialDims.begin(), info.spatialDims.end()),
      SmallVector<Value>(info.blockShape.begin(), info.blockShape.end()),
      SmallVector<Value>(info.minOffsets.begin(), info.minOffsets.end()),
      SmallVector<Value>(info.maxOffsets.begin(), info.maxOffsets.end()),
      SmallVector<Value>(info.writeFootprint.begin(),
                         info.writeFootprint.end()),
      info.supportedBlockHalo,
      SmallVector<int64_t>(info.stencilIndependentDims.begin(),
                           info.stencilIndependentDims.end()),
      info.esdByteOffset, info.esdByteSize, info.cachedStartBlock,
      info.cachedBlockCount, info.postDbRefined, info.inferredDbMode,
      info.estimatedTaskCost, info.criticalPathDistance);

  for (auto stale : staleContracts)
    if (stale.getOperation() != contract.getOperation())
      stale->erase();

  return contract;
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

void mlir::arts::eraseLoweringContracts(Value target) {
  if (!target)
    return;
  for (auto contract : collectLoweringContractOps(target))
    contract->erase();
}
