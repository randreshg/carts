///==========================================================================///
/// File: ArtsMaterializationUtils.h
///
/// CODIR-to-ARTS materialization helpers. This boundary is where generic CODIR
/// codelet/dependency plans become abstract ARTS DB, EDT, route, barrier, and
/// resource-query objects.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_ARTSMATERIALIZATIONUTILS_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_ARTSMATERIALIZATIONUTILS_H

#include "carts/dialect/arts/Utils/DbLayoutPlanUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/dialect/codir/Utils/CodirConversionUtils.h"
#include "carts/utils/Utils.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/MathExtras.h"
#include <algorithm>
#include <limits>
#include <optional>

namespace {

static inline arts::ArtsMode convertAccessMode(codir::CodirAccessMode mode) {
  switch (mode) {
  case codir::CodirAccessMode::read:
    return arts::ArtsMode::in;
  case codir::CodirAccessMode::write:
    return arts::ArtsMode::out;
  case codir::CodirAccessMode::readwrite:
    return arts::ArtsMode::inout;
  }
  return arts::ArtsMode::inout;
}

static inline bool isReductionCodelet(codir::CodeletOp codelet) {
  auto pattern = codelet.getPatternAttr();
  return pattern && pattern.getValue() == codir::CodirPattern::reduction;
}

static inline bool
isAtomicAddAddressable(Value memref, ValueRange indices,
                       const DenseMap<Value, Value> &sourceByBlockArgument) {
  auto memrefType = dyn_cast<MemRefType>(memref.getType());
  if (!memrefType)
    return false;
  if (memrefType.getRank() == 0)
    return indices.empty();
  if (memrefType.getRank() != 1 || indices.size() != 1)
    return false;
  return isKnownZeroIndex(indices.front(), sourceByBlockArgument);
}

static inline bool sameMemrefAccess(Value lhsMemref, ValueRange lhsIndices,
                                    Value rhsMemref, ValueRange rhsIndices) {
  return lhsMemref == rhsMemref &&
         ::mlir::carts::ValueAnalysis::areValueRangesIdentical(lhsIndices,
                                                               rhsIndices);
}

static inline unsigned lowerIntegerAddReductionsToAtomics(
    Region &region, const DenseMap<Value, Value> &sourceByBlockArgument) {
  SmallVector<memref::StoreOp, 8> stores;
  region.walk([&](memref::StoreOp store) { stores.push_back(store); });

  unsigned lowered = 0;
  for (memref::StoreOp store : stores) {
    auto add = store.getValue().getDefiningOp<arith::AddIOp>();
    if (!add || !add->hasOneUse())
      continue;

    memref::LoadOp load;
    Value increment;
    for (Value operand : add->getOperands()) {
      auto candidate = operand.getDefiningOp<memref::LoadOp>();
      if (!candidate)
        continue;
      if (!sameMemrefAccess(candidate.getMemref(), candidate.getIndices(),
                            store.getMemref(), store.getIndices()))
        continue;
      load = candidate;
      increment = add.getLhs() == operand ? add.getRhs() : add.getLhs();
      break;
    }
    if (!load || !load->hasOneUse() || !increment)
      continue;
    if (!isAtomicAddAddressable(store.getMemref(), store.getIndices(),
                                sourceByBlockArgument))
      continue;

    OpBuilder builder(store);
    arts::AtomicAddOp::create(builder, store.getLoc(), store.getMemref(),
                              increment);
    store.erase();
    if (add->use_empty())
      add.erase();
    if (load->use_empty())
      load.erase();
    ++lowered;
  }

  return lowered;
}

static inline bool shouldLowerReductionsToAtomics(codir::CodeletOp codelet) {
  if (!isReductionCodelet(codelet))
    return false;
  if (codelet.getPartialReductionAttr())
    return false;
  auto strategy = codelet.getReductionStrategyAttr();
  return !strategy ||
         strategy.getValue() == codir::CodirReductionStrategy::atomic;
}

// Tier-1 enums (BarrierReason, ReductionStrategy, IterationTopology) have
// identical case sets and integer codes across SDE, CODIR, and ARTS
// (audit-verified). Call sites translate via static_cast guarded by these
// invariants.
static_assert(static_cast<int>(sde::SdeBarrierReason::unknown_required) ==
              static_cast<int>(arts::ArtsBarrierReason::unknown_required));
static_assert(
    static_cast<int>(codir::CodirReductionStrategy::local_accumulate) ==
    static_cast<int>(arts::ArtsReductionStrategy::local_accumulate));
static_assert(static_cast<int>(codir::CodirIterationTopology::owner_tile_2d) ==
              static_cast<int>(arts::ArtsPlanIterationTopology::owner_tile_2d));

static inline arts::ArtsDepPattern convertPattern(codir::CodirPattern pattern) {
  switch (pattern) {
  case codir::CodirPattern::uniform:
    return arts::ArtsDepPattern::uniform;
  case codir::CodirPattern::stencil_tiling_nd:
    return arts::ArtsDepPattern::stencil_tiling_nd;
  case codir::CodirPattern::cross_dim_stencil_3d:
    return arts::ArtsDepPattern::cross_dim_stencil_3d;
  case codir::CodirPattern::higher_order_stencil:
    return arts::ArtsDepPattern::higher_order_stencil;
  case codir::CodirPattern::wavefront_2d:
    return arts::ArtsDepPattern::wavefront_2d;
  case codir::CodirPattern::alternating_buffer_stencil:
    return arts::ArtsDepPattern::alternating_buffer_stencil;
  case codir::CodirPattern::matmul:
    return arts::ArtsDepPattern::matmul;
  case codir::CodirPattern::elementwise_pipeline:
    return arts::ArtsDepPattern::elementwise_pipeline;
  case codir::CodirPattern::reduction:
    return arts::ArtsDepPattern::reduction;
  }
  return arts::ArtsDepPattern::unknown;
}

static inline arts::EdtDistributionKind
convertDistributionKind(codir::CodirDistributionKind kind) {
  switch (kind) {
  case codir::CodirDistributionKind::owner_compute:
    return arts::EdtDistributionKind::block;
  case codir::CodirDistributionKind::blocked:
    return arts::EdtDistributionKind::block;
  case codir::CodirDistributionKind::cyclic:
    return arts::EdtDistributionKind::block_cyclic;
  }
  return arts::EdtDistributionKind::block;
}

static inline arts::ArtsPlanRepetitionStructure
convertRepetitionStructure(codir::CodirRepetitionStructure structure) {
  switch (structure) {
  case codir::CodirRepetitionStructure::none:
    return arts::ArtsPlanRepetitionStructure::none;
  case codir::CodirRepetitionStructure::pair_step:
    return arts::ArtsPlanRepetitionStructure::pair_step;
  case codir::CodirRepetitionStructure::k_step:
    return arts::ArtsPlanRepetitionStructure::k_step;
  case codir::CodirRepetitionStructure::full_timestep:
    return arts::ArtsPlanRepetitionStructure::full_timestep;
  }
  return arts::ArtsPlanRepetitionStructure::none;
}

static inline arts::EdtDistributionPattern
getDistributionPattern(codir::CodirPattern pattern) {
  switch (pattern) {
  case codir::CodirPattern::uniform:
  case codir::CodirPattern::elementwise_pipeline:
    return arts::EdtDistributionPattern::uniform;
  case codir::CodirPattern::stencil_tiling_nd:
  case codir::CodirPattern::cross_dim_stencil_3d:
  case codir::CodirPattern::higher_order_stencil:
  case codir::CodirPattern::wavefront_2d:
  case codir::CodirPattern::alternating_buffer_stencil:
    return arts::EdtDistributionPattern::stencil;
  case codir::CodirPattern::matmul:
    return arts::EdtDistributionPattern::matmul;
  case codir::CodirPattern::reduction:
    return arts::EdtDistributionPattern::uniform;
  }
  return arts::EdtDistributionPattern::unknown;
}

static inline bool sameSortedI64Set(ArrayAttr lhs, ArrayAttr rhs) {
  std::optional<SmallVector<int64_t, 4>> lhsValues = readI64ArrayAttr(lhs);
  std::optional<SmallVector<int64_t, 4>> rhsValues = readI64ArrayAttr(rhs);
  if (!lhsValues || !rhsValues || lhsValues->size() != rhsValues->size())
    return false;
  llvm::sort(*lhsValues);
  llvm::sort(*rhsValues);
  return llvm::equal(*lhsValues, *rhsValues);
}

static inline ArrayAttr getCodirStencilOwnerDimsAttr(codir::CodeletOp codelet) {
  if (!codelet)
    return ArrayAttr{};
  ArrayAttr planOwnerDims = codelet.getPlanOwnerDimsAttr();
  ArrayAttr tileOwnerDims = codelet.getTileOwnerDimsAttr();
  if (!tileOwnerDims)
    return planOwnerDims;
  if (!planOwnerDims)
    return tileOwnerDims;
  if (sameSortedI64Set(planOwnerDims, tileOwnerDims))
    return tileOwnerDims;
  return planOwnerDims;
}

static inline void propagateCodirPlanToArts(codir::CodeletOp codelet,
                                            arts::EdtOp task) {
  if (!codelet || !task)
    return;
  MLIRContext *ctx = codelet.getContext();
  Operation *taskOp = task.getOperation();
  if (auto pattern = codelet.getPatternAttr()) {
    arts::ArtsDepPattern depPattern = convertPattern(pattern.getValue());
    if (depPattern != arts::ArtsDepPattern::unknown) {
      arts::setDepPattern(taskOp, depPattern);
      arts::setEdtDistributionPattern(
          taskOp, getDistributionPattern(pattern.getValue()));
      arts::setDistributionVersion(taskOp, 1);
    }
  }
  if (auto kind = codelet.getDistributionKindAttr()) {
    arts::setEdtDistributionKind(taskOp,
                                 convertDistributionKind(kind.getValue()));
  }
  if (auto topology = codelet.getIterationTopologyAttr()) {
    arts::setPlanIterationTopologyAttr(
        taskOp, arts::ArtsPlanIterationTopologyAttr::get(
                    ctx, static_cast<arts::ArtsPlanIterationTopology>(
                             topology.getValue())));
  }
  if (auto repetition = codelet.getRepetitionStructureAttr()) {
    arts::setPlanRepetitionStructureAttr(
        taskOp, arts::ArtsPlanRepetitionStructureAttr::get(
                    ctx, convertRepetitionStructure(repetition.getValue())));
  }
  if (auto strategy = codelet.getReductionStrategyAttr())
    task.setReductionStrategyAttr(arts::ArtsReductionStrategyAttr::get(
        ctx, static_cast<arts::ArtsReductionStrategy>(strategy.getValue())));
  if (codelet.getPartialReductionAttr())
    task.setPartialReductionAttr(UnitAttr::get(ctx));
  if (auto dims = codelet.getPartialReductionDimsAttr())
    task.setPartialReductionDimsAttr(dims);
  if (auto ownerDims = codelet.getPartialReductionOwnerDimsAttr())
    task.setPartialReductionOwnerDimsAttr(ownerDims);
  if (auto depMaps = codelet.getPartialReductionDepResultDimMapsAttr())
    task.setPartialReductionDepResultDimMapsAttr(depMaps);
  if (codelet.getPartialReductionSplitRequiredAttr())
    task.setPartialReductionSplitRequiredAttr(UnitAttr::get(ctx));
  if (auto splitDims = codelet.getPartialReductionSplitDimsAttr())
    task.setPartialReductionSplitDimsAttr(splitDims);
  if (auto splitFactor = codelet.getPartialReductionSplitFactorAttr())
    task.setPartialReductionSplitFactorAttr(splitFactor);
  if (auto ownerTaskCount =
          codelet.getPartialReductionSplitOwnerTaskCountAttr())
    task.setPartialReductionSplitOwnerTaskCountAttr(ownerTaskCount);
  if (auto targetWorkerCount =
          codelet.getPartialReductionSplitTargetWorkerCountAttr())
    task.setPartialReductionSplitTargetWorkerCountAttr(targetWorkerCount);
  ArrayAttr tileShape = codelet.getTileShapeAttr();
  if (tileShape) {
    if (auto tileOwnerDims = codelet.getTileOwnerDimsAttr())
      arts::setPlanOwnerDimsAttr(taskOp, tileOwnerDims);
    arts::setPlanPhysicalBlockShapeAttr(taskOp, tileShape);
  } else if (auto ownerDims = codelet.getPlanOwnerDimsAttr()) {
    arts::setPlanOwnerDimsAttr(taskOp, ownerDims);
  }
  if (auto workerSlice = codelet.getLogicalWorkerSliceAttr())
    arts::setPlanLogicalWorkerSliceAttr(taskOp, workerSlice);
  if (auto haloShape = codelet.getHaloShapeAttr())
    arts::setPlanHaloShapeAttr(taskOp, haloShape);
  if (auto minOffsets = codelet.getAccessMinOffsetsAttr())
    task->setAttr(task.getStencilMinOffsetsAttrName(), minOffsets);
  if (auto maxOffsets = codelet.getAccessMaxOffsetsAttr())
    task->setAttr(task.getStencilMaxOffsetsAttrName(), maxOffsets);
  if (auto ownerDims = getCodirStencilOwnerDimsAttr(codelet))
    task->setAttr(task.getStencilOwnerDimsAttrName(), ownerDims);
  if (auto spatialDims = codelet.getSpatialDimsAttr())
    task->setAttr(task.getStencilSpatialDimsAttrName(), spatialDims);
  if (auto writeFootprint = codelet.getWriteFootprintAttr())
    task->setAttr(task.getStencilWriteFootprintAttrName(), writeFootprint);
  if (codelet.getInPlaceSafeAttr())
    task.setInPlaceSafeAttr(UnitAttr::get(ctx));
  if (codelet.getInPlaceSharedStateAttr())
    task.setInPlaceSharedStateAttr(UnitAttr::get(ctx));
  if (auto pattern = codelet.getPatternAttr()) {
    if (pattern.getValue() == codir::CodirPattern::stencil_tiling_nd ||
        pattern.getValue() == codir::CodirPattern::cross_dim_stencil_3d ||
        pattern.getValue() == codir::CodirPattern::higher_order_stencil ||
        pattern.getValue() == codir::CodirPattern::wavefront_2d ||
        pattern.getValue() == codir::CodirPattern::alternating_buffer_stencil) {
      if (codelet.getAccessMinOffsetsAttr() &&
          codelet.getAccessMaxOffsetsAttr())
        task->setAttr(task.getStencilSupportedBlockHaloAttrName(),
                      UnitAttr::get(ctx));
    }
  }
}

static inline FailureOr<SmallVector<Value>>
buildElementSizes(OpBuilder &builder, Location loc, MemRefType memrefType,
                  ValueRange dynamicSizes) {
  SmallVector<Value> elementSizes;
  if (memrefType.getRank() == 0) {
    if (!dynamicSizes.empty())
      return failure();
    elementSizes.push_back(createOneIndex(builder, loc));
    return elementSizes;
  }

  elementSizes.reserve(memrefType.getRank());
  unsigned dynamicIdx = 0;
  for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim) {
    if (memrefType.isDynamicDim(dim)) {
      if (dynamicIdx >= dynamicSizes.size())
        return failure();
      elementSizes.push_back(dynamicSizes[dynamicIdx++]);
      continue;
    }
    elementSizes.push_back(
        createConstantIndex(builder, loc, memrefType.getDimSize(dim)));
  }
  if (dynamicIdx != dynamicSizes.size())
    return failure();
  return elementSizes;
}

static inline MemRefType getElementMemRefType(Type elementType, unsigned rank) {
  SmallVector<int64_t> elementShape(rank, ShapedType::kDynamic);
  return MemRefType::get(elementShape, elementType);
}

static inline Value materializeInnerPayload(OpBuilder &builder, Location loc,
                                            Value sourcePtr) {
  unsigned rank = 1;
  if (auto ptrType = dyn_cast<MemRefType>(sourcePtr.getType()))
    rank = std::max<unsigned>(1, ptrType.getRank());
  SmallVector<Value> indices;
  indices.reserve(rank);
  for (unsigned idx = 0; idx < rank; ++idx)
    indices.push_back(createZeroIndex(builder, loc));
  return arts::DbRefOp::create(builder, loc, sourcePtr, indices);
}

static inline bool hasCodirTileOwnerSlicePlan(codir::CodeletOp op) {
  return op && op.getTileShapeAttr() && op.getTileOwnerDimsAttr();
}

static inline std::optional<SmallVector<unsigned, 4>>
getCodirTileOwnerDims(codir::CodeletOp codelet) {
  if (!hasCodirTileOwnerSlicePlan(codelet))
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

static inline std::optional<unsigned>
getSingleCodirTileOwnerDim(codir::CodeletOp codelet) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirTileOwnerDims(codelet);
  if (!ownerDims || ownerDims->size() != 1)
    return std::nullopt;
  return ownerDims->front();
}

static inline SmallVector<Value, 4>
getCodirOwnerBaseArguments(codir::CodeletOp codelet, unsigned ownerDimCount) {
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

static inline SmallVector<Value, 4>
getCodirOwnerParamValues(codir::CodeletOp codelet, unsigned ownerDimCount) {
  SmallVector<Value, 4> params;
  if (!codelet || ownerDimCount == 0 ||
      codelet.getParams().size() < ownerDimCount)
    return params;

  params.reserve(ownerDimCount);
  unsigned firstOwnerParam = codelet.getParams().size() - ownerDimCount;
  for (unsigned slot = 0; slot < ownerDimCount; ++slot)
    params.push_back(codelet.getParams()[firstOwnerParam + slot]);
  return params;
}

static inline std::optional<SmallVector<unsigned, 4>>
getCodirDepOwnerDims(codir::CodeletOp codelet, unsigned depIndex);

static inline SmallVector<Value, 4>
getCodirDepOwnerParamValues(codir::CodeletOp codelet, unsigned depIndex) {
  SmallVector<Value, 4> params;
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  std::optional<SmallVector<unsigned, 4>> tileOwnerDims =
      getCodirTileOwnerDims(codelet);
  if (!ownerDims || !tileOwnerDims)
    return params;

  if (codelet && !codelet.getBody().empty() &&
      depIndex < codelet.getDeps().size()) {
    Block &body = codelet.getBody().front();
    unsigned depCount = codelet.getDeps().size();
    unsigned paramCount = codelet.getParams().size();
    if (depIndex < body.getNumArguments() &&
        body.getNumArguments() >= depCount + paramCount) {
      Value depArg = body.getArgument(depIndex);
      SmallVector<std::optional<unsigned>, 4> paramSlots(ownerDims->size());
      bool rejected = false;
      body.walk([&](Operation *op) {
        if (rejected)
          return WalkResult::interrupt();

        auto access = getCodirMemoryAccessInfo(op);
        if (!access || access->memref != depArg)
          return WalkResult::advance();

        for (auto [ownerSlot, ownerDim] : llvm::enumerate(*ownerDims)) {
          if (ownerDim >= access->indices.size())
            continue;
          std::optional<unsigned> selectedParam;
          for (unsigned paramSlot = 0; paramSlot < paramCount; ++paramSlot) {
            Value bodyParam = body.getArgument(depCount + paramSlot);
            if (!indexSelectsOwnerSlice(access->indices[ownerDim], bodyParam))
              continue;
            if (selectedParam && *selectedParam != paramSlot) {
              rejected = true;
              return WalkResult::interrupt();
            }
            selectedParam = paramSlot;
          }
          if (!selectedParam)
            continue;
          if (paramSlots[ownerSlot] &&
              *paramSlots[ownerSlot] != *selectedParam) {
            rejected = true;
            return WalkResult::interrupt();
          }
          paramSlots[ownerSlot] = *selectedParam;
        }
        return WalkResult::advance();
      });

      if (!rejected &&
          llvm::all_of(paramSlots, [](const std::optional<unsigned> &slot) {
            return slot.has_value();
          })) {
        params.reserve(ownerDims->size());
        for (std::optional<unsigned> slot : paramSlots)
          params.push_back(codelet.getParams()[*slot]);
        return params;
      }
    }
  }

  SmallVector<Value, 4> tileParams =
      getCodirOwnerParamValues(codelet, tileOwnerDims->size());
  if (tileParams.empty())
    return params;
  if (tileParams.size() == ownerDims->size())
    return tileParams;
  if (tileParams.size() != tileOwnerDims->size())
    return params;

  params.reserve(ownerDims->size());
  for (unsigned ownerDim : *ownerDims) {
    auto it = llvm::find(*tileOwnerDims, ownerDim);
    if (it == tileOwnerDims->end())
      return {};
    params.push_back(tileParams[std::distance(tileOwnerDims->begin(), it)]);
  }
  return params;
}

static inline std::optional<SmallVector<unsigned, 4>>
getPlannedCodirDepOwnerDims(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr depOwnerDims =
      codelet ? codelet.getDepOwnerDimsAttr() : ArrayAttr{};
  if (!depOwnerDims || depIndex >= depOwnerDims.size())
    return std::nullopt;

  auto dims = dyn_cast<ArrayAttr>(depOwnerDims[depIndex]);
  if (!dims)
    return std::nullopt;
  std::optional<SmallVector<int64_t, 4>> values = readI64ArrayAttr(dims);
  if (!values || values->empty())
    return std::nullopt;
  SmallVector<unsigned, 4> ownerDims;
  ownerDims.reserve(values->size());
  for (int64_t dim : *values) {
    if (dim < 0)
      return std::nullopt;
    ownerDims.push_back(static_cast<unsigned>(dim));
  }
  return ownerDims;
}

static inline std::optional<SmallVector<unsigned, 4>>
getCodirDepOwnerDims(codir::CodeletOp codelet, unsigned depIndex) {
  return getPlannedCodirDepOwnerDims(codelet, depIndex);
}

static inline ArrayAttr getCodirDepOwnerDimsAttr(codir::CodeletOp codelet,
                                                 unsigned depIndex) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims)
    return ArrayAttr{};
  SmallVector<int64_t, 4> values;
  values.reserve(ownerDims->size());
  for (unsigned dim : *ownerDims)
    values.push_back(dim);
  return buildI64ArrayAttr(codelet.getContext(), values);
}

static inline bool canUseCodirOwnerSliceForAlloc(codir::CodeletOp codelet,
                                                 unsigned depIndex,
                                                 arts::DbAllocOp alloc) {
  if (!hasCodirTileOwnerSlicePlan(codelet) || !alloc)
    return false;

  std::optional<arts::PartitionMode> mode =
      arts::getPartitionMode(alloc.getOperation());
  if (!mode || (*mode != arts::PartitionMode::block &&
                *mode != arts::PartitionMode::stencil))
    return false;

  ArrayAttr ownerDims = getCodirDepOwnerDimsAttr(codelet, depIndex);
  return ownerDims &&
         arts::getPlanOwnerDimsAttr(alloc.getOperation()) == ownerDims &&
         arts::getPlanPhysicalBlockShapeAttr(alloc.getOperation()) ==
             codelet.getTileShapeAttr();
}

static inline std::optional<codir::CodirAccessMode>
getCodirDepAccessMode(codir::CodeletOp codelet, unsigned depIndex);

static inline bool codirAccessMayRead(codir::CodirAccessMode mode);

static inline bool codirAccessMayWrite(codir::CodirAccessMode mode);

static inline std::optional<SmallVector<int64_t, 4>>
getCodirTileOwnerBlockSizes(codir::CodeletOp codelet, unsigned depIndex,
                            unsigned memrefRank) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty())
    return std::nullopt;

  std::optional<SmallVector<int64_t, 4>> tileShape =
      readI64ArrayAttr(codelet.getTileShapeAttr());
  if (!tileShape || tileShape->empty())
    return std::nullopt;

  SmallVector<int64_t, 4> blockSizes;
  blockSizes.reserve(ownerDims->size());
  for (auto [slot, ownerDim] : llvm::enumerate(*ownerDims)) {
    std::optional<int64_t> blockSize;
    if (tileShape->size() == memrefRank) {
      if (ownerDim >= tileShape->size())
        return std::nullopt;
      blockSize = (*tileShape)[ownerDim];
    } else if (tileShape->size() == ownerDims->size()) {
      blockSize = (*tileShape)[slot];
    } else if (tileShape->size() == 1 && ownerDims->size() == 1) {
      blockSize = tileShape->front();
    }
    if (!blockSize || *blockSize <= 0)
      return std::nullopt;
    blockSizes.push_back(*blockSize);
  }
  return blockSizes;
}

static inline std::optional<int64_t>
getSingleCodirTileOwnerBlockSize(codir::CodeletOp codelet, unsigned depIndex,
                                 unsigned memrefRank) {
  std::optional<SmallVector<int64_t, 4>> blockSizes =
      getCodirTileOwnerBlockSizes(codelet, depIndex, memrefRank);
  if (!blockSizes || blockSizes->size() != 1)
    return std::nullopt;
  return blockSizes->front();
}

static inline scf::ForOp findCodirOwnerDispatchLoop(codir::CodeletOp codelet) {
  scf::ForOp nearestLoop;
  for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (loop && !nearestLoop)
      nearestLoop = loop;
    if (loop && containsValue(codelet.getParams(), loop.getInductionVar()))
      return loop;
  }
  if (hasCodirTileOwnerSlicePlan(codelet))
    return nearestLoop;
  return {};
}

static inline scf::ForOp findCodirOwnerDispatchLoop(codir::CodeletOp codelet,
                                                    Value ownerParam) {
  for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (loop && loop.getInductionVar() == ownerParam)
      return loop;
  }
  return {};
}

static inline Value getCodirOwnerDomainLower(codir::CodeletOp codelet) {
  if (auto loop = findCodirOwnerDispatchLoop(codelet))
    return loop.getLowerBound();
  return {};
}

static inline bool dependsOnAncestorLoop(Value value,
                                         codir::CodeletOp codelet) {
  if (!value || !codelet)
    return false;
  for (Operation *parent = codelet->getParentOp(); parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (loop &&
        ::mlir::carts::ValueAnalysis::dependsOn(value, loop.getInductionVar()))
      return true;
  }
  return false;
}

static inline Value
deriveCodirOwnerDomainLowerFromParam(codir::CodeletOp codelet,
                                     Value ownerParam) {
  Value stripped = ::mlir::carts::ValueAnalysis::stripNumericCasts(ownerParam);
  auto add = stripped ? stripped.getDefiningOp<arith::AddIOp>() : nullptr;
  if (!add)
    return {};

  Value lhs = add.getLhs();
  Value rhs = add.getRhs();
  bool lhsDependsOnDispatch = dependsOnAncestorLoop(lhs, codelet);
  bool rhsDependsOnDispatch = dependsOnAncestorLoop(rhs, codelet);
  if (lhsDependsOnDispatch == rhsDependsOnDispatch)
    return {};
  return lhsDependsOnDispatch ? rhs : lhs;
}

static inline Value getCodirOwnerDomainLower(codir::CodeletOp codelet,
                                             Value ownerParam) {
  if (auto loop = findCodirOwnerDispatchLoop(codelet, ownerParam))
    return loop.getLowerBound();
  if (Value lower = deriveCodirOwnerDomainLowerFromParam(codelet, ownerParam))
    return lower;
  return getCodirOwnerDomainLower(codelet);
}

static inline Value getCodirOwnerDomainUpper(codir::CodeletOp codelet) {
  if (auto loop = findCodirOwnerDispatchLoop(codelet))
    return loop.getUpperBound();
  return {};
}

struct CodirOwnerHaloWindow {
  unsigned ownerDim = 0;
  int64_t lower = 0;
  int64_t upper = 0;

  bool empty() const { return lower <= 0 && upper <= 0; }
  int64_t width() const { return lower + upper; }
};

static inline std::optional<unsigned>
getCodirOwnerDimSlot(codir::CodeletOp codelet, unsigned ownerDim) {
  if (auto ownerDims = readI64ArrayAttr(codelet.getTileOwnerDimsAttr())) {
    for (auto [slot, rawDim] : llvm::enumerate(*ownerDims))
      if (rawDim >= 0 && static_cast<unsigned>(rawDim) == ownerDim)
        return static_cast<unsigned>(slot);
  }
  return std::nullopt;
}

static inline std::optional<int64_t>
getCodirOwnerDimValue(ArrayAttr attr, unsigned ownerDim,
                      std::optional<unsigned> ownerSlot, unsigned memrefRank) {
  std::optional<SmallVector<int64_t, 4>> values = readI64ArrayAttr(attr);
  if (!values || values->empty())
    return std::nullopt;
  if (values->size() == memrefRank && ownerDim < values->size())
    return (*values)[ownerDim];
  if (ownerSlot && *ownerSlot < values->size())
    return (*values)[*ownerSlot];
  if (values->size() == 1)
    return values->front();
  return std::nullopt;
}

static inline std::optional<unsigned>
getSingleCodirDepOwnerDim(codir::CodeletOp codelet, unsigned depIndex) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->size() != 1)
    return std::nullopt;
  return ownerDims->front();
}

static inline CodirOwnerHaloWindow
getCodirOwnerHaloWindowForDim(codir::CodeletOp codelet, unsigned depIndex,
                              unsigned ownerDim, unsigned memrefRank,
                              bool requireReadOnly = true) {
  CodirOwnerHaloWindow window;
  window.ownerDim = ownerDim;

  std::optional<codir::CodirAccessMode> mode =
      getCodirDepAccessMode(codelet, depIndex);
  if (requireReadOnly &&
      (!mode || !codirAccessMayRead(*mode) || codirAccessMayWrite(*mode)))
    return window;
  if (ownerDim >= memrefRank)
    return window;

  std::optional<unsigned> ownerSlot = getCodirOwnerDimSlot(codelet, ownerDim);

  if (auto minOffset = getCodirOwnerDimValue(codelet.getAccessMinOffsetsAttr(),
                                             ownerDim, ownerSlot, memrefRank))
    window.lower = std::max<int64_t>(0, -*minOffset);
  if (auto maxOffset = getCodirOwnerDimValue(codelet.getAccessMaxOffsetsAttr(),
                                             ownerDim, ownerSlot, memrefRank))
    window.upper = std::max<int64_t>(0, *maxOffset);

  if (!window.empty())
    return window;

  if (auto halo = getCodirOwnerDimValue(codelet.getHaloShapeAttr(), ownerDim,
                                        ownerSlot, memrefRank)) {
    int64_t radius = std::max<int64_t>(0, *halo);
    window.lower = radius;
    window.upper = radius;
  }

  return window;
}

static inline SmallVector<CodirOwnerHaloWindow, 4>
getCodirOwnerHaloWindows(codir::CodeletOp codelet, unsigned depIndex,
                         unsigned memrefRank, bool requireReadOnly = true) {
  SmallVector<CodirOwnerHaloWindow, 4> windows;
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims)
    return windows;

  windows.reserve(ownerDims->size());
  for (unsigned ownerDim : *ownerDims)
    windows.push_back(getCodirOwnerHaloWindowForDim(
        codelet, depIndex, ownerDim, memrefRank, requireReadOnly));
  return windows;
}

static inline CodirOwnerHaloWindow
getCodirOwnerHaloWindow(codir::CodeletOp codelet, unsigned depIndex,
                        unsigned memrefRank) {
  std::optional<unsigned> ownerDim =
      getSingleCodirDepOwnerDim(codelet, depIndex);
  if (!ownerDim)
    return {};
  return getCodirOwnerHaloWindowForDim(codelet, depIndex, *ownerDim,
                                       memrefRank);
}

// Defined below (after findBackingDbAlloc); forward-declared so
// createDbBackedMemref can use the unioned per-buffer halo window.
static inline SmallVector<CodirOwnerHaloWindow, 4>
codirBackingBufferHaloWindows(Value rootMemref, unsigned memrefRank);

// Predicates defined later in this header; forward-declared so the union helper
// can replicate the exact block-vs-coarse materialization decision per read
// dep.
static inline bool
codirDepRequiresPhaseRedistributionBridge(codir::CodeletOp codelet,
                                          unsigned depIndex);
static inline bool codirDepUsesHaloStencilStorage(codir::CodeletOp codelet,
                                                  unsigned depIndex);
static inline bool
canMaterializeRawCodirDependencyWithPlan(Value root,
                                         codir::CodeletOp planSource);
static inline bool rawCodirDependencyNeedsHostBridge(Value root);

static inline Value subtractClampZero(OpBuilder &builder, Location loc,
                                      Value value, int64_t amount) {
  if (amount <= 0)
    return value;
  Value offset = createConstantIndex(builder, loc, amount);
  Value zero = createZeroIndex(builder, loc);
  Value canSubtract = arith::CmpIOp::create(
      builder, loc, arith::CmpIPredicate::uge, value, offset);
  Value shifted = arith::SubIOp::create(builder, loc, value, offset);
  return arith::SelectOp::create(builder, loc, canSubtract, shifted, zero);
}

static inline Value materializePositiveDifferenceOrZero(OpBuilder &builder,
                                                        Location loc, Value end,
                                                        Value start) {
  Value zero = createZeroIndex(builder, loc);
  Value hasPositiveExtent = arith::CmpIOp::create(
      builder, loc, arith::CmpIPredicate::ugt, end, start);
  Value difference = arith::SubIOp::create(builder, loc, end, start);
  return arith::SelectOp::create(builder, loc, hasPositiveExtent, difference,
                                 zero);
}

static inline Value materializeCodirOwnerDomainBase(OpBuilder &builder,
                                                    Location loc,
                                                    codir::CodeletOp codelet) {
  if (Value lower = getCodirOwnerDomainLower(codelet))
    return lower;
  return createZeroIndex(builder, loc);
}

static inline Value materializeCodirOwnerDomainBase(OpBuilder &builder,
                                                    Location loc,
                                                    codir::CodeletOp codelet,
                                                    Value ownerParam) {
  if (Value lower = getCodirOwnerDomainLower(codelet, ownerParam))
    return lower;
  return createZeroIndex(builder, loc);
}

static inline Value
materializeCodirBlockLocalBase(OpBuilder &builder, Location loc,
                               codir::CodeletOp codelet, unsigned depIndex,
                               Value ownerBase, unsigned memrefRank) {
  CodirOwnerHaloWindow halo =
      getCodirOwnerHaloWindow(codelet, depIndex, memrefRank);
  if (halo.empty())
    return ownerBase;
  return subtractClampZero(builder, loc, ownerBase, halo.lower);
}

static inline bool
codirDepAccessesStayWithinSingleOwnerSlice(codir::CodeletOp codelet,
                                           unsigned depIndex) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
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
      getCodirTileOwnerDims(codelet);
  if (!tileOwnerDims)
    return false;
  SmallVector<Value, 4> ownerBases =
      getCodirOwnerBaseArguments(codelet, tileOwnerDims->size());
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

struct PlannedBlockLocalAccessRewrite {
  Value localMemref;
  unsigned ownerDim = 0;
  Value ownerBase;
  int64_t lowerHalo = 0;
  Value groupedSourcePtr;
  unsigned ownerSlot = 0;
  int64_t blockSize = 1;
  int64_t groupBlockCount = 1;
  bool grouped = false;
};

static inline FailureOr<Value>
materializeBlockLocalIndex(OpBuilder &builder, Location loc, Value index,
                           Value ownerBase, int64_t lowerHalo) {
  if (!index || !ownerBase)
    return failure();
  Value localOrigin = ownerBase;
  if (lowerHalo > 0) {
    Value halo = createConstantIndex(builder, loc, lowerHalo);
    Value zero = createZeroIndex(builder, loc);
    Value canSubtract = arith::CmpIOp::create(
        builder, loc, arith::CmpIPredicate::uge, ownerBase, halo);
    Value shifted = arith::SubIOp::create(builder, loc, ownerBase, halo);
    localOrigin =
        arith::SelectOp::create(builder, loc, canSubtract, shifted, zero);
  }
  if (::mlir::carts::ValueAnalysis::sameValue(index, localOrigin))
    return createZeroIndex(builder, loc);
  if (auto sub = index.getDefiningOp<arith::SubIOp>())
    if (::mlir::carts::ValueAnalysis::sameValue(sub.getRhs(), localOrigin))
      return index;
  if (!indexSelectsOwnerSlice(index, ownerBase))
    return failure();
  return arith::SubIOp::create(builder, loc, index, localOrigin).getResult();
}

static inline FailureOr<Value> materializeGroupedBlockLocalIndex(
    OpBuilder &builder, Location loc, Value index, Value ownerBase,
    int64_t blockSize, int64_t groupBlockCount, Value &relativeBlock) {
  if (!index || !ownerBase || blockSize <= 0 || groupBlockCount <= 0)
    return failure();
  if (groupBlockCount > std::numeric_limits<int64_t>::max() / blockSize)
    return failure();
  if (!indexSelectsOwnerSlice(index, ownerBase))
    return failure();

  int64_t windowExtent = blockSize * groupBlockCount;
  auto getOwnerRelativeConstant =
      [&](Value candidate) -> std::optional<int64_t> {
    int64_t offset = 0;
    Value base =
        ::mlir::carts::ValueAnalysis::stripConstantOffset(candidate, &offset);
    if (::mlir::carts::ValueAnalysis::sameValue(base, ownerBase))
      return offset;
    return std::nullopt;
  };
  auto pointStaysInWindow = [&](Value candidate) {
    std::optional<int64_t> offset = getOwnerRelativeConstant(candidate);
    return offset && *offset >= 0 && *offset < windowExtent;
  };
  auto upperOffsetStaysInWindow = [&](Value candidate) {
    std::optional<int64_t> offset = getOwnerRelativeConstant(candidate);
    return offset && *offset >= 0 && *offset <= windowExtent;
  };
  auto upperStaysInWindow = [&](Value candidate) {
    if (upperOffsetStaysInWindow(candidate))
      return true;
    candidate = ::mlir::carts::ValueAnalysis::stripNumericCasts(candidate);
    if (auto min = candidate.getDefiningOp<arith::MinUIOp>())
      return upperOffsetStaysInWindow(min.getLhs()) ||
             upperOffsetStaysInWindow(min.getRhs());
    if (auto min = candidate.getDefiningOp<arith::MinSIOp>())
      return upperOffsetStaysInWindow(min.getLhs()) ||
             upperOffsetStaysInWindow(min.getRhs());
    return false;
  };

  bool provenInWindow = pointStaysInWindow(index);
  if (!provenInWindow) {
    if (auto blockArg = dyn_cast<BlockArgument>(index)) {
      auto loop =
          dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
      if (loop && loop.getInductionVar() == index &&
          ::mlir::carts::ValueAnalysis::isConstantAtLeastOne(loop.getStep())) {
        std::optional<int64_t> lower =
            getOwnerRelativeConstant(loop.getLowerBound());
        provenInWindow = lower && *lower >= 0 && *lower < windowExtent &&
                         upperStaysInWindow(loop.getUpperBound());
      }
    }
  }
  if (!provenInWindow)
    return failure();

  Value blockSizeValue = createConstantIndex(builder, loc, blockSize);
  Value relativeIndex =
      ::mlir::carts::ValueAnalysis::sameValue(index, ownerBase)
          ? createZeroIndex(builder, loc)
          : arith::SubIOp::create(builder, loc, index, ownerBase).getResult();
  relativeBlock =
      arith::DivUIOp::create(builder, loc, relativeIndex, blockSizeValue);
  Value blockOffset =
      arith::MulIOp::create(builder, loc, relativeBlock, blockSizeValue);
  Value blockBase = arith::AddIOp::create(builder, loc, ownerBase, blockOffset);
  if (::mlir::carts::ValueAnalysis::sameValue(index, blockBase))
    return createZeroIndex(builder, loc);
  return arith::SubIOp::create(builder, loc, index, blockBase).getResult();
}

static inline LogicalResult rewritePlannedBlockLocalAccesses(
    arts::EdtOp task, ArrayRef<PlannedBlockLocalAccessRewrite> rewrites) {
  if (rewrites.empty())
    return success();

  auto rewriteIndices = [&](Operation *op, MutableOperandRange memrefOperand,
                            MutableOperandRange indices) -> WalkResult {
    Value memref = memrefOperand[0].get();
    SmallVector<const PlannedBlockLocalAccessRewrite *, 4> matching;
    for (const PlannedBlockLocalAccessRewrite &rewrite : rewrites)
      if (memref == rewrite.localMemref)
        matching.push_back(&rewrite);
    if (matching.empty())
      return WalkResult::advance();

    bool hasGrouped = llvm::any_of(
        matching, [](const PlannedBlockLocalAccessRewrite *rewrite) {
          return rewrite->grouped;
        });
    if (hasGrouped) {
      Value sourcePtr;
      unsigned sourceRank = 0;
      for (const PlannedBlockLocalAccessRewrite *rewrite : matching) {
        if (!rewrite->grouped || rewrite->lowerHalo != 0 ||
            !rewrite->groupedSourcePtr || rewrite->blockSize <= 0 ||
            rewrite->groupBlockCount <= 0) {
          op->emitError("grouped planned block-local access requires "
                        "block-window facts for every owner dimension");
          return WalkResult::interrupt();
        }
        if (!sourcePtr)
          sourcePtr = rewrite->groupedSourcePtr;
        if (sourcePtr != rewrite->groupedSourcePtr) {
          op->emitError("grouped planned block-local access mixes dependency "
                        "sources");
          return WalkResult::interrupt();
        }
        sourceRank = std::max<unsigned>(sourceRank, rewrite->ownerSlot + 1);
      }
      if (auto sourceType = dyn_cast<MemRefType>(sourcePtr.getType()))
        sourceRank = std::max<unsigned>(sourceRank, sourceType.getRank());
      if (sourceRank == 0)
        sourceRank = 1;

      OpBuilder builder(op);
      SmallVector<Value, 4> dbRefIndices(
          sourceRank, createZeroIndex(builder, op->getLoc()));
      for (const PlannedBlockLocalAccessRewrite *rewrite : matching) {
        if (indices.size() <= rewrite->ownerDim ||
            rewrite->ownerSlot >= dbRefIndices.size()) {
          op->emitError("grouped planned block-local access has malformed "
                        "owner-dimension facts");
          return WalkResult::interrupt();
        }
        Value relativeBlock;
        FailureOr<Value> localIndex = materializeGroupedBlockLocalIndex(
            builder, op->getLoc(), indices[rewrite->ownerDim].get(),
            rewrite->ownerBase, rewrite->blockSize, rewrite->groupBlockCount,
            relativeBlock);
        if (failed(localIndex)) {
          op->emitError("grouped planned block-local access does not stay "
                        "within the block window");
          return WalkResult::interrupt();
        }
        dbRefIndices[rewrite->ownerSlot] = relativeBlock;
        indices[rewrite->ownerDim].set(*localIndex);
      }
      Value selectedPayload =
          arts::DbRefOp::create(builder, op->getLoc(), sourcePtr, dbRefIndices);
      memrefOperand.assign(ValueRange{selectedPayload});
      return WalkResult::advance();
    }

    for (const PlannedBlockLocalAccessRewrite *rewrite : matching) {
      if (indices.size() <= rewrite->ownerDim) {
        op->emitError("planned block-local access is missing the owner "
                      "dimension index");
        return WalkResult::interrupt();
      }

      OpBuilder builder(op);
      FailureOr<Value> localIndex = materializeBlockLocalIndex(
          builder, op->getLoc(), indices[rewrite->ownerDim].get(),
          rewrite->ownerBase, rewrite->lowerHalo);
      if (failed(localIndex)) {
        op->emitError("planned block-local access does not stay within the "
                      "owner slice");
        return WalkResult::interrupt();
      }
      indices[rewrite->ownerDim].set(*localIndex);
    }
    return WalkResult::advance();
  };

  Block &body = task.getBody().front();
  WalkResult result = body.walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return rewriteIndices(op, load.getMemrefMutable(),
                            load.getIndicesMutable());
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return rewriteIndices(op, store.getMemrefMutable(),
                            store.getIndicesMutable());
    return WalkResult::advance();
  });

  return result.wasInterrupted() ? failure() : success();
}

static inline LogicalResult
createDbBackedMemref(OpBuilder &builder, Location loc, MemRefType memrefType,
                     ValueRange dynamicSizes, Value &memref,
                     sde::SdeSuIterateOp planSource = {}) {
  FailureOr<SmallVector<Value>> elementSizes =
      buildElementSizes(builder, loc, memrefType, dynamicSizes);
  if (failed(elementSizes))
    return failure();

  SmallVector<Value> sizes{createOneIndex(builder, loc)};
  SmallVector<Value> dbElementSizes = std::move(*elementSizes);
  arts::PartitionMode partitionMode = arts::PartitionMode::coarse;
  if (planSource) {
    FailureOr<arts::DbPhysicalLayoutPlan> physicalPlan =
        arts::resolvePhysicalDbLayoutPlan(
            planSource.getPhysicalOwnerDimsAttr(),
            planSource.getPhysicalBlockShapeAttr(), dbElementSizes, builder,
            loc);
    if (failed(physicalPlan))
      return failure();
    sizes.assign(physicalPlan->outerSizes.begin(),
                 physicalPlan->outerSizes.end());
    dbElementSizes.assign(physicalPlan->innerSizes.begin(),
                          physicalPlan->innerSizes.end());
    partitionMode = physicalPlan->mode;
  }

  Value route = arts::createCurrentNodeRoute(builder, loc);
  arts::DbAllocOp dbAlloc;
  if (planSource) {
    dbAlloc = arts::DbAllocOp::create(
        builder, loc, arts::ArtsMode::inout, route, arts::DbAllocType::heap,
        arts::DbMode::write, memrefType.getElementType(), std::move(sizes),
        std::move(dbElementSizes), partitionMode);
    if (auto ownerDims = planSource.getPhysicalOwnerDimsAttr())
      arts::setPlanOwnerDimsAttr(dbAlloc.getOperation(), ownerDims);
    if (auto blockShape = planSource.getPhysicalBlockShapeAttr())
      arts::setPlanPhysicalBlockShapeAttr(dbAlloc.getOperation(), blockShape);
    if (auto workerSlice = planSource.getLogicalWorkerSliceAttr())
      arts::setPlanLogicalWorkerSliceAttr(dbAlloc.getOperation(), workerSlice);
    if (auto haloShape = planSource.getPhysicalHaloShapeAttr())
      arts::setPlanHaloShapeAttr(dbAlloc.getOperation(), haloShape);
  } else {
    Type pointerElementType =
        getElementMemRefType(memrefType.getElementType(), memrefType.getRank());
    Type pointerType =
        MemRefType::get({ShapedType::kDynamic}, pointerElementType);
    dbAlloc = arts::DbAllocOp::create(
        builder, loc, arts::ArtsMode::inout, route, arts::DbAllocType::heap,
        arts::DbMode::write, memrefType.getElementType(), pointerType,
        std::move(sizes), std::move(dbElementSizes), partitionMode);
  }

  memref = materializeInnerPayload(builder, loc, dbAlloc.getPtr());
  return success();
}

static inline LogicalResult
createDbBackedMemref(OpBuilder &builder, Location loc, MemRefType memrefType,
                     ValueRange dynamicSizes, Value &memref,
                     codir::CodeletOp planSource,
                     std::optional<unsigned> depIndex = std::nullopt) {
  if (!hasCodirTileOwnerSlicePlan(planSource))
    return createDbBackedMemref(builder, loc, memrefType, dynamicSizes, memref,
                                sde::SdeSuIterateOp{});

  FailureOr<SmallVector<Value>> elementSizes =
      buildElementSizes(builder, loc, memrefType, dynamicSizes);
  if (failed(elementSizes))
    return failure();

  SmallVector<Value> dbElementSizes = std::move(*elementSizes);
  ArrayAttr ownerDims = depIndex
                            ? getCodirDepOwnerDimsAttr(planSource, *depIndex)
                            : planSource.getTileOwnerDimsAttr();
  if (!ownerDims)
    return failure();
  FailureOr<arts::DbPhysicalLayoutPlan> physicalPlan =
      arts::resolvePhysicalDbLayoutPlan(ownerDims,
                                        planSource.getTileShapeAttr(),
                                        dbElementSizes, builder, loc);
  if (failed(physicalPlan))
    return failure();

  SmallVector<CodirOwnerHaloWindow, 4> ownerHalos;
  if (depIndex) {
    Value backingRoot = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
        planSource.getDeps()[*depIndex]);
    ownerHalos = codirBackingBufferHaloWindows(
        backingRoot, static_cast<unsigned>(memrefType.getRank()));
    for (const CodirOwnerHaloWindow &ownerHalo : ownerHalos) {
      if (ownerHalo.empty() ||
          ownerHalo.ownerDim >= physicalPlan->innerSizes.size())
        continue;
      Value haloWidth = createConstantIndex(builder, loc, ownerHalo.width());
      physicalPlan->innerSizes[ownerHalo.ownerDim] = arith::AddIOp::create(
          builder, loc, physicalPlan->innerSizes[ownerHalo.ownerDim],
          haloWidth);
    }
  }

  Value route = arts::createCurrentNodeRoute(builder, loc);
  auto dbAlloc = arts::DbAllocOp::create(
      builder, loc, arts::ArtsMode::inout, route, arts::DbAllocType::heap,
      arts::DbMode::write, memrefType.getElementType(),
      SmallVector<Value>(physicalPlan->outerSizes.begin(),
                         physicalPlan->outerSizes.end()),
      SmallVector<Value>(physicalPlan->innerSizes.begin(),
                         physicalPlan->innerSizes.end()),
      physicalPlan->mode);
  arts::setPlanOwnerDimsAttr(dbAlloc.getOperation(), ownerDims);
  if (auto blockShape = planSource.getTileShapeAttr())
    arts::setPlanPhysicalBlockShapeAttr(dbAlloc.getOperation(), blockShape);
  if (auto workerSlice = planSource.getLogicalWorkerSliceAttr())
    arts::setPlanLogicalWorkerSliceAttr(dbAlloc.getOperation(), workerSlice);
  if (auto haloShape = planSource.getHaloShapeAttr())
    arts::setPlanHaloShapeAttr(dbAlloc.getOperation(), haloShape);
  if (llvm::any_of(ownerHalos, [](const CodirOwnerHaloWindow &halo) {
        return !halo.empty();
      }))
    dbAlloc->setAttr(dbAlloc.getStencilSupportedBlockHaloAttrName(),
                     UnitAttr::get(dbAlloc.getContext()));

  memref = materializeInnerPayload(builder, loc, dbAlloc.getPtr());
  return success();
}

static inline LogicalResult lowerMuData(sde::SdeMuDataOp op) {
  auto memrefType = dyn_cast<MemRefType>(op.getHandle().getType());
  if (!memrefType)
    return op.emitOpError()
           << "expects a memref handle before CODIR-to-ARTS materialization";
  if (memrefType.getNumDynamicDims() != 0)
    return op.emitOpError()
           << "has dynamic dimensions but carries no dynamic size operands";

  OpBuilder builder(op);
  Value replacement;
  if (failed(createDbBackedMemref(builder, op.getLoc(), memrefType,
                                  ValueRange{}, replacement)))
    return failure();
  if (replacement.getType() != memrefType)
    replacement =
        memref::CastOp::create(builder, op.getLoc(), memrefType, replacement);

  op.getHandle().replaceAllUsesWith(replacement);
  op.erase();
  return success();
}

static inline LogicalResult lowerMuAlloc(sde::SdeMuAllocOp op) {
  auto memrefType = dyn_cast<MemRefType>(op.getMemref().getType());
  if (!memrefType)
    return op.emitOpError()
           << "expects a memref result before CODIR-to-ARTS materialization";

  OpBuilder builder(op);
  FailureOr<sde::SdeSuIterateOp> planSource = selectMuAllocWritePlan(op);
  if (failed(planSource))
    return failure();

  Value replacement;
  if (failed(createDbBackedMemref(builder, op.getLoc(), memrefType,
                                  op.getDynamicSizes(), replacement,
                                  *planSource)))
    return failure();
  if (replacement.getType() != memrefType)
    replacement =
        memref::CastOp::create(builder, op.getLoc(), memrefType, replacement);

  op.getMemref().replaceAllUsesWith(replacement);
  op.erase();
  return success();
}

static inline arts::DbAllocOp findBackingDbAlloc(Value storage) {
  return dyn_cast_or_null<arts::DbAllocOp>(arts::DbUtils::getUnderlyingDbAlloc(
      ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storage)));
}

static inline std::optional<unsigned>
findCodirDependencyIndexForRoot(codir::CodeletOp codelet, Value root) {
  for (auto [idx, dep] : llvm::enumerate(codelet.getDeps()))
    if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) == root)
      return static_cast<unsigned>(idx);
  return std::nullopt;
}

static inline std::optional<codir::CodirStorageViewKind>
getCodirDepStorageViewKind(codir::CodeletOp codelet, unsigned depIndex);
static inline std::optional<codir::CodirAccessMode>
getCodirDepAccessMode(codir::CodeletOp codelet, unsigned depIndex);
static inline std::optional<codir::CodirCollectiveKind>
getCodirDepCollectiveKind(codir::CodeletOp codelet, unsigned depIndex);
static inline codir::CodirCollectiveKind
getFinalizedCodirDepCollectiveKind(codir::CodeletOp codelet, unsigned depIndex);
static inline bool codirDepRequiresComputeBlockStorage(codir::CodeletOp codelet,
                                                       unsigned depIndex);
static inline bool codirAccessMayRead(codir::CodirAccessMode mode);
static inline bool codirAccessMayWrite(codir::CodirAccessMode mode);

// StoragePlanning stamps collective selection onto `dep_collectives`; this
// file reads that carrier instead of re-running CODIR predicates.

static inline void
mergeCodirOwnerHaloWindow(SmallVectorImpl<CodirOwnerHaloWindow> &windows,
                          CodirOwnerHaloWindow incoming) {
  if (incoming.empty())
    return;
  for (CodirOwnerHaloWindow &window : windows) {
    if (window.ownerDim != incoming.ownerDim)
      continue;
    window.lower = std::max(window.lower, incoming.lower);
    window.upper = std::max(window.upper, incoming.upper);
    return;
  }
  windows.push_back(incoming);
}

static inline SmallVector<CodirOwnerHaloWindow, 4>
codirBackingBufferHaloWindows(Value rootMemref, unsigned memrefRank) {
  SmallVector<CodirOwnerHaloWindow, 4> unionWindows;
  if (!rootMemref)
    return unionWindows;

  Operation *defining = rootMemref.getDefiningOp();
  Operation *scope =
      defining ? defining : rootMemref.getParentBlock()->getParentOp();
  ModuleOp module = scope ? scope->getParentOfType<ModuleOp>() : ModuleOp{};
  if (!module)
    return unionWindows;

  module.walk([&](codir::CodeletOp codelet) {
    for (auto [idx, dep] : llvm::enumerate(codelet.getDeps())) {
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) != rootMemref)
        continue;
      unsigned depIdx = static_cast<unsigned>(idx);
      if (!codirDepUsesHaloStencilStorage(codelet, depIdx))
        continue;
      if (!canMaterializeRawCodirDependencyWithPlan(rootMemref, codelet))
        continue;
      if (rawCodirDependencyNeedsHostBridge(rootMemref) &&
          !codirDepRequiresComputeBlockStorage(codelet, depIdx))
        continue;
      for (CodirOwnerHaloWindow window :
           getCodirOwnerHaloWindows(codelet, depIdx, memrefRank,
                                    /*requireReadOnly=*/false))
        mergeCodirOwnerHaloWindow(unionWindows, window);
    }
  });

  return unionWindows;
}

static inline SmallVector<CodirOwnerHaloWindow, 4>
getCodirBlockStorageHaloWindows(codir::CodeletOp codelet, unsigned depIndex,
                                unsigned memrefRank) {
  SmallVector<CodirOwnerHaloWindow, 4> windows;
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!codelet || depIndex >= codelet.getDeps().size() || !ownerDims)
    return windows;

  SmallVector<CodirOwnerHaloWindow, 4> storageWindows =
      codirBackingBufferHaloWindows(
          ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
              codelet.getDeps()[depIndex]),
          memrefRank);
  windows.reserve(ownerDims->size());
  for (unsigned ownerDim : *ownerDims) {
    CodirOwnerHaloWindow resolved;
    resolved.ownerDim = ownerDim;
    for (const CodirOwnerHaloWindow &candidate : storageWindows) {
      if (candidate.ownerDim == ownerDim) {
        resolved = candidate;
        break;
      }
    }
    windows.push_back(resolved);
  }
  return windows;
}

static inline CodirOwnerHaloWindow
getCodirBlockStorageHaloWindowForDim(codir::CodeletOp codelet,
                                     unsigned depIndex, unsigned ownerDim,
                                     unsigned memrefRank) {
  for (const CodirOwnerHaloWindow &window :
       getCodirBlockStorageHaloWindows(codelet, depIndex, memrefRank)) {
    if (window.ownerDim == ownerDim)
      return window;
  }
  CodirOwnerHaloWindow empty;
  empty.ownerDim = ownerDim;
  return empty;
}

static inline std::optional<codir::CodirStorageViewKind>
getCodirDepStorageViewKind(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr views = codelet ? codelet.getDepStorageViewsAttr() : ArrayAttr{};
  if (!views || depIndex >= views.size())
    return std::nullopt;
  auto view = dyn_cast<codir::CodirStorageViewKindAttr>(views[depIndex]);
  if (!view)
    return std::nullopt;
  return view.getValue();
}

/// First-class collective family for |codelet|'s |depIndex|. Absence means the
/// CODIR storage/collective planning contract has not been finalized.
static inline std::optional<codir::CodirCollectiveKind>
getCodirDepCollectiveKind(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr collectives =
      codelet ? codelet.getDepCollectivesAttr() : ArrayAttr{};
  if (!collectives || depIndex >= collectives.size())
    return std::nullopt;
  auto kind = dyn_cast<codir::CodirCollectiveKindAttr>(collectives[depIndex]);
  if (!kind)
    return std::nullopt;
  return kind.getValue();
}

static inline codir::CodirCollectiveKind
getFinalizedCodirDepCollectiveKind(codir::CodeletOp codelet,
                                   unsigned depIndex) {
  return getCodirDepCollectiveKind(codelet, depIndex)
      .value_or(codir::CodirCollectiveKind::none);
}

static inline std::optional<codir::CodirAccessMode>
getCodirDepAccessMode(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr modes = codelet ? codelet.getDepModesAttr() : ArrayAttr{};
  if (!modes || depIndex >= modes.size())
    return std::nullopt;
  auto mode = dyn_cast<codir::CodirAccessModeAttr>(modes[depIndex]);
  if (!mode)
    return std::nullopt;
  return mode.getValue();
}

static inline bool
codirStorageViewUsesComputeBlock(codir::CodirStorageViewKind view) {
  return view == codir::CodirStorageViewKind::compute_block ||
         view == codir::CodirStorageViewKind::phase_redistributed;
}

static inline bool codirDepRequiresPlannedOwnerDims(codir::CodeletOp codelet,
                                                    unsigned depIndex) {
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  if (view && codirStorageViewUsesComputeBlock(*view))
    return true;

  std::optional<codir::CodirCollectiveKind> collective =
      getCodirDepCollectiveKind(codelet, depIndex);
  if (!collective)
    return false;
  switch (*collective) {
  case codir::CodirCollectiveKind::all_gather:
  case codir::CodirCollectiveKind::reduce_scatter:
  case codir::CodirCollectiveKind::halo:
    return true;
  case codir::CodirCollectiveKind::none:
  case codir::CodirCollectiveKind::all_to_all:
  case codir::CodirCollectiveKind::allreduce:
  case codir::CodirCollectiveKind::broadcast:
    return false;
  }
  return false;
}

static inline LogicalResult
requireFinalizedCodirDepOwnerDimsForMaterialization(codir::CodeletOp codelet,
                                                    unsigned depIndex) {
  if (!codirDepRequiresPlannedOwnerDims(codelet, depIndex))
    return success();
  if (getCodirDepOwnerDims(codelet, depIndex))
    return success();
  return codelet.emitOpError()
         << "dependency #" << depIndex
         << " requires finalized non-empty dep_owner_dims for "
            "block/stencil/compute materialization";
}

static inline bool codirDepAllowsComputeBlockStorage(codir::CodeletOp codelet,
                                                     unsigned depIndex) {
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  return view && codirStorageViewUsesComputeBlock(*view);
}

// An iterative-stencil dep carrying a committed `halo` collective legitimately
// reads neighbor tiles OUTSIDE its owner slice; the per-block halo exchange
// supplies those neighbors, so it qualifies for block-native distributed
// storage even though its accesses cross the owner slice. This is the ARTS dual
// of StoragePlanning's stencil-halo compute_block demotion: without it ARTS
// re-derives single-owner-slice containment, rejects the committed
// compute_block+halo plan, and coarse-falls-back the stencil buffer to a single
// local_only block (correct-but-not-distributed).
static inline bool codirDepIsHaloStencilStorage(codir::CodeletOp codelet,
                                                unsigned depIndex) {
  return getFinalizedCodirDepCollectiveKind(codelet, depIndex) ==
         codir::CodirCollectiveKind::halo;
}

static inline bool codirDepRequiresComputeBlockStorage(codir::CodeletOp codelet,
                                                       unsigned depIndex) {
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  return view && codirStorageViewUsesComputeBlock(*view);
}

static inline bool
codirDepRequiresPhaseRedistributionBridge(codir::CodeletOp codelet,
                                          unsigned depIndex) {
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  return view && *view == codir::CodirStorageViewKind::phase_redistributed;
}

static inline bool isCodirStencilPattern(codir::CodeletOp codelet) {
  auto pattern = codelet ? codelet.getPatternAttr() : nullptr;
  if (!pattern)
    return false;
  switch (pattern.getValue()) {
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

static inline bool codirDepHasHaloWindow(codir::CodeletOp codelet,
                                         unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  auto memrefType = dyn_cast<MemRefType>(codelet.getDeps()[depIndex].getType());
  if (!memrefType || memrefType.getRank() == 0)
    return false;
  SmallVector<CodirOwnerHaloWindow, 4> windows = getCodirOwnerHaloWindows(
      codelet, depIndex, static_cast<unsigned>(memrefType.getRank()),
      /*requireReadOnly=*/false);
  return llvm::any_of(windows, [](const CodirOwnerHaloWindow &window) {
    return !window.empty();
  });
}

static inline bool codirDepUsesHaloStencilStorage(codir::CodeletOp codelet,
                                                  unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex) ||
      !isCodirStencilPattern(codelet))
    return false;
  return getFinalizedCodirDepCollectiveKind(codelet, depIndex) ==
             codir::CodirCollectiveKind::halo &&
         codirDepHasHaloWindow(codelet, depIndex);
}

static inline bool codirDepUsesBlockNativeSettle(codir::CodeletOp codelet,
                                                 unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  if (getFinalizedCodirDepCollectiveKind(codelet, depIndex) !=
      codir::CodirCollectiveKind::reduce_scatter)
    return false;
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  if (!view || *view != codir::CodirStorageViewKind::phase_redistributed)
    return false;
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex))
    return false;
  auto factor = codelet.getPartialReductionSplitFactorAttr();
  return factor && factor.getInt() > 0;
}

static inline bool
codirDepHasNoStencilReachAlongOwnerDims(codir::CodeletOp codelet,
                                        unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  auto memrefType = dyn_cast<MemRefType>(codelet.getDeps()[depIndex].getType());
  if (!memrefType || memrefType.getRank() == 0)
    return false;
  if (!codelet.getAccessMinOffsetsAttr() || !codelet.getAccessMaxOffsetsAttr())
    return false;

  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty())
    return false;

  unsigned memrefRank = static_cast<unsigned>(memrefType.getRank());
  for (unsigned ownerDim : *ownerDims) {
    std::optional<unsigned> ownerSlot = getCodirOwnerDimSlot(codelet, ownerDim);
    std::optional<int64_t> minOffset = getCodirOwnerDimValue(
        codelet.getAccessMinOffsetsAttr(), ownerDim, ownerSlot, memrefRank);
    std::optional<int64_t> maxOffset = getCodirOwnerDimValue(
        codelet.getAccessMaxOffsetsAttr(), ownerDim, ownerSlot, memrefRank);
    if (!minOffset || !maxOffset)
      return false;
    if (*minOffset != 0 || *maxOffset != 0)
      return false;
  }
  return true;
}

static inline bool
codirDepUsesOwnerLocalStencilStorage(codir::CodeletOp codelet,
                                     unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex) ||
      !isCodirStencilPattern(codelet))
    return false;
  std::optional<codir::CodirAccessMode> mode =
      getCodirDepAccessMode(codelet, depIndex);
  if (!mode || !codirAccessMayRead(*mode) || codirAccessMayWrite(*mode))
    return false;
  if (getFinalizedCodirDepCollectiveKind(codelet, depIndex) !=
      codir::CodirCollectiveKind::none)
    return false;
  return codirDepHasNoStencilReachAlongOwnerDims(codelet, depIndex);
}

static inline bool
codirRootHasHaloStencilStorageParticipant(codir::CodeletOp codelet,
                                          unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return false;
  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
      codelet.getDeps()[depIndex]);
  Operation *scope = codelet->getParentOfType<ModuleOp>();
  if (!root || !scope)
    return false;

  bool found = false;
  scope->walk([&](codir::CodeletOp candidate) {
    if (found)
      return;
    for (auto [idx, dep] : llvm::enumerate(candidate.getDeps())) {
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) != root)
        continue;
      if (codirDepUsesHaloStencilStorage(candidate,
                                         static_cast<unsigned>(idx))) {
        found = true;
        return;
      }
    }
  });
  return found;
}

static inline bool codirDepCanUseBlockStorageAccess(codir::CodeletOp codelet,
                                                    unsigned depIndex) {
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex) ||
      !hasCodirTileOwnerSlicePlan(codelet) ||
      !getCodirDepOwnerDims(codelet, depIndex))
    return false;

  // CODIR's storage plan is the committed layout fact. Do not silently
  // coarse-fallback here because the local access tracer is conservative; the
  // block-local index rewrite below is the fail-closed verifier for the actual
  // transformed body.
  return true;
}

static inline bool codirAccessMayWrite(codir::CodirAccessMode mode) {
  return mode == codir::CodirAccessMode::write ||
         mode == codir::CodirAccessMode::readwrite;
}

static inline bool codirAccessMayRead(codir::CodirAccessMode mode) {
  return mode == codir::CodirAccessMode::read ||
         mode == codir::CodirAccessMode::readwrite;
}

static inline Operation *
findCodirDispatchBridgeAnchor(codir::CodeletOp codelet) {
  Operation *anchor = codelet ? codelet.getOperation() : nullptr;
  Operation *nearestLoop = nullptr;
  for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (loop && !nearestLoop)
      nearestLoop = parent;
    if (loop && containsValue(codelet.getParams(), loop.getInductionVar()))
      anchor = parent;
  }
  if (codelet && anchor == codelet.getOperation() &&
      hasCodirTileOwnerSlicePlan(codelet) && nearestLoop)
    return nearestLoop;
  return anchor;
}

struct HostBridgeParticipant {
  codir::CodeletOp codelet;
  unsigned depIndex = 0;
  codir::CodirAccessMode mode = codir::CodirAccessMode::readwrite;
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

static inline bool isUseInsideAnchor(Operation *anchor, Operation *owner) {
  if (!anchor || !owner)
    return false;
  if (anchor == owner)
    return true;
  for (Region &region : anchor->getRegions())
    if (region.isAncestor(owner->getParentRegion()))
      return true;
  return false;
}

static inline bool hostBridgeValueMayBeWrittenInsideAnchor(Operation *anchor,
                                                           Value value);

static inline bool hostBridgeUseMayWrite(Operation *anchor, OpOperand &use) {
  Operation *owner = use.getOwner();
  if (!anchor || !owner)
    return true;
  if (!isUseInsideAnchor(anchor, owner))
    return false;

  if (auto codelet = dyn_cast<codir::CodeletOp>(owner)) {
    std::optional<unsigned> depIndex = getCodeletDepOperandIndex(codelet, use);
    if (!depIndex)
      return true;
    std::optional<codir::CodirAccessMode> mode =
        getCodirDepAccessMode(codelet, *depIndex);
    return !mode || codirAccessMayWrite(*mode);
  }

  if (auto store = dyn_cast<memref::StoreOp>(owner))
    return store.getMemRef() == use.get();
  if (auto store = dyn_cast<polygeist::DynStoreOp>(owner))
    return store.getMemref() == use.get();
  if (auto store = dyn_cast<affine::AffineStoreOp>(owner))
    return store.getMemRef() == use.get();
  if (isa<memref::DeallocOp>(owner))
    return true;
  if (isa<memref::LoadOp, memref::DimOp>(owner))
    return false;

  if (isMemrefForwardingOp(owner)) {
    for (Value result : owner->getResults())
      if (isa<MemRefType>(result.getType()) &&
          hostBridgeValueMayBeWrittenInsideAnchor(anchor, result))
        return true;
    return false;
  }

  return true;
}

static inline bool hostBridgeValueMayBeWrittenInsideAnchor(Operation *anchor,
                                                           Value value) {
  if (!anchor || !value)
    return true;
  for (OpOperand &use : value.getUses())
    if (hostBridgeUseMayWrite(anchor, use))
      return true;
  return false;
}

static inline Operation *findHostBridgeReadObservationAnchor(OpOperand &use) {
  Operation *owner = use.getOwner();
  if (!owner)
    return nullptr;
  if (auto codelet = dyn_cast<codir::CodeletOp>(owner))
    if (Operation *anchor = findCodirDispatchBridgeAnchor(codelet))
      return anchor;
  return owner;
}

static inline void
appendUniqueHostBridgeReadObservation(Operation *anchor,
                                      SmallVectorImpl<Operation *> &anchors) {
  if (!anchor || llvm::is_contained(anchors, anchor))
    return;
  anchors.push_back(anchor);
}

static inline Operation *findHostBridgeEventUnderAnchor(Operation *anchor,
                                                        Operation *op) {
  if (!anchor || !op)
    return nullptr;
  if (anchor == op)
    return op;

  Operation *event = op;
  while (event && event->getParentOp() != anchor) {
    Operation *parent = event->getParentOp();
    if (!parent)
      return nullptr;
    event = parent;
  }
  return event;
}

static inline SmallVector<Operation *>
filterHostBridgeReadSyncAnchors(Operation *anchor,
                                ArrayRef<HostBridgeParticipant> participants,
                                ArrayRef<Operation *> readObservationAnchors) {
  if (!anchor || readObservationAnchors.empty())
    return SmallVector<Operation *>{readObservationAnchors.begin(),
                                    readObservationAnchors.end()};

  struct Event {
    Operation *eventOp = nullptr;
    Operation *syncAnchor = nullptr;
    bool write = false;
    bool readObservation = false;
    unsigned ordinal = 0;
  };

  SmallVector<Event> events;
  unsigned ordinal = 0;
  for (const HostBridgeParticipant &participant : participants) {
    if (!codirAccessMayWrite(participant.mode))
      continue;
    Operation *dispatchAnchor =
        findCodirDispatchBridgeAnchor(participant.codelet);
    Operation *event = findHostBridgeEventUnderAnchor(anchor, dispatchAnchor);
    if (!event)
      return SmallVector<Operation *>{readObservationAnchors.begin(),
                                      readObservationAnchors.end()};
    events.push_back({event, nullptr, /*write=*/true,
                      /*readObservation=*/false, ordinal++});
  }

  for (Operation *syncAnchor : readObservationAnchors) {
    Operation *event = findHostBridgeEventUnderAnchor(anchor, syncAnchor);
    if (!event)
      return SmallVector<Operation *>{readObservationAnchors.begin(),
                                      readObservationAnchors.end()};
    events.push_back({event, syncAnchor, /*write=*/false,
                      /*readObservation=*/true, ordinal++});
  }

  Block *eventBlock = nullptr;
  for (const Event &event : events) {
    if (!event.eventOp || !event.eventOp->getBlock())
      return SmallVector<Operation *>{readObservationAnchors.begin(),
                                      readObservationAnchors.end()};
    if (!eventBlock) {
      eventBlock = event.eventOp->getBlock();
      continue;
    }
    if (eventBlock != event.eventOp->getBlock())
      return SmallVector<Operation *>{readObservationAnchors.begin(),
                                      readObservationAnchors.end()};
  }

  llvm::sort(events, [](const Event &lhs, const Event &rhs) {
    if (lhs.eventOp == rhs.eventOp)
      return lhs.ordinal < rhs.ordinal;
    return lhs.eventOp->isBeforeInBlock(rhs.eventOp);
  });

  SmallVector<Operation *> filtered;
  bool blockDirtyForHost = false;
  for (const Event &event : events) {
    if (event.write)
      blockDirtyForHost = true;
    if (event.readObservation && blockDirtyForHost) {
      appendUniqueHostBridgeReadObservation(event.syncAnchor, filtered);
      blockDirtyForHost = false;
    }
  }
  return filtered;
}

static inline bool
hostBridgeNeedsInitialCopyIn(Operation *anchor,
                             ArrayRef<HostBridgeParticipant> participants) {
  if (llvm::any_of(participants, [](const HostBridgeParticipant &participant) {
        if (!codirAccessMayWrite(participant.mode))
          return false;
        return getFinalizedCodirDepCollectiveKind(participant.codelet,
                                                  participant.depIndex) ==
               codir::CodirCollectiveKind::halo;
      }))
    return true;

  bool hasReadParticipant =
      llvm::any_of(participants, [](const HostBridgeParticipant &participant) {
        return codirAccessMayRead(participant.mode);
      });
  if (!hasReadParticipant) {
    bool hasUnclassifiedWriter =
        llvm::any_of(participants, [](HostBridgeParticipant participant) {
          return codirAccessMayWrite(participant.mode) &&
                 !participant.codelet.getPatternAttr();
        });
    if (hasUnclassifiedWriter)
      return true;
    return false;
  }
  if (!anchor)
    return true;

  struct Event {
    Operation *eventOp = nullptr;
    codir::CodirAccessMode mode = codir::CodirAccessMode::readwrite;
    unsigned ordinal = 0;
  };

  SmallVector<Event> events;
  unsigned ordinal = 0;
  for (const HostBridgeParticipant &participant : participants) {
    Operation *dispatchAnchor =
        findCodirDispatchBridgeAnchor(participant.codelet);
    Operation *event = findHostBridgeEventUnderAnchor(anchor, dispatchAnchor);
    if (!event)
      return true;
    events.push_back({event, participant.mode, ordinal++});
  }

  Block *eventBlock = nullptr;
  for (const Event &event : events) {
    if (!event.eventOp || !event.eventOp->getBlock())
      return true;
    if (!eventBlock) {
      eventBlock = event.eventOp->getBlock();
      continue;
    }
    if (eventBlock != event.eventOp->getBlock())
      return true;
  }

  llvm::sort(events, [](const Event &lhs, const Event &rhs) {
    if (lhs.eventOp == rhs.eventOp)
      return lhs.ordinal < rhs.ordinal;
    return lhs.eventOp->isBeforeInBlock(rhs.eventOp);
  });

  for (const Event &event : events) {
    if (codirAccessMayRead(event.mode))
      return true;
    if (codirAccessMayWrite(event.mode))
      return false;
  }
  return true;
}

static inline bool hasSameHostBridgePlan(codir::CodeletOp lhs,
                                         unsigned lhsDepIndex,
                                         codir::CodeletOp rhs,
                                         unsigned rhsDepIndex) {
  if (!lhs || !rhs)
    return false;
  return getCodirDepOwnerDimsAttr(lhs, lhsDepIndex) ==
             getCodirDepOwnerDimsAttr(rhs, rhsDepIndex) &&
         lhs.getTileShapeAttr() == rhs.getTileShapeAttr() &&
         lhs.getLogicalWorkerSliceAttr() == rhs.getLogicalWorkerSliceAttr();
}

static inline std::optional<unsigned>
getCodeletDepOperandIndex(codir::CodeletOp codelet, OpOperand &use) {
  if (!codelet)
    return std::nullopt;
  unsigned operandIndex = use.getOperandNumber();
  if (operandIndex >= codelet.getDeps().size())
    return std::nullopt;
  return operandIndex;
}

static inline bool isCompatibleHostBridgeParticipant(codir::CodeletOp seed,
                                                     unsigned seedDepIndex,
                                                     codir::CodeletOp codelet,
                                                     unsigned depIndex) {
  if (!seed || !codelet ||
      !hasSameHostBridgePlan(seed, seedDepIndex, codelet, depIndex))
    return false;
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex))
    return false;
  if (!hasCodirTileOwnerSlicePlan(codelet) ||
      !codirDepCanUseBlockStorageAccess(codelet, depIndex))
    return false;
  return true;
}

static inline bool isCompatibleComputeBlockParticipant(
    codir::CodeletOp seed, unsigned seedDepIndex, codir::CodeletOp codelet,
    unsigned depIndex, arts::DbAllocOp sourceAlloc) {
  if (!seed || !codelet || !sourceAlloc ||
      !hasSameHostBridgePlan(seed, seedDepIndex, codelet, depIndex))
    return false;
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex) ||
      codirDepRequiresPhaseRedistributionBridge(codelet, depIndex))
    return false;
  if (!hasCodirTileOwnerSlicePlan(codelet) ||
      !codirDepCanUseBlockStorageAccess(codelet, depIndex))
    return false;
  return findBackingDbAlloc(codelet.getDeps()[depIndex]) == sourceAlloc;
}

static inline FailureOr<HostBridgeUseCollection>
collectHostBridgeParticipants(Operation *anchor, codir::CodeletOp seed,
                              unsigned seedDepIndex, Value hostView) {
  if (!anchor || !seed || !hostView)
    return failure();

  HostBridgeUseCollection collection;
  for (OpOperand &use : hostView.getUses()) {
    Operation *owner = use.getOwner();
    if (!isUseInsideAnchor(anchor, owner))
      continue;

    auto codelet = dyn_cast<codir::CodeletOp>(owner);
    if (!codelet) {
      if (!hostBridgeUseMayWrite(anchor, use)) {
        appendUniqueHostBridgeReadObservation(
            findHostBridgeReadObservationAnchor(use),
            collection.readObservationAnchors);
        continue;
      }
      return failure();
    }

    std::optional<unsigned> depIndex = getCodeletDepOperandIndex(codelet, use);
    if (!depIndex || !isCompatibleHostBridgeParticipant(seed, seedDepIndex,
                                                        codelet, *depIndex)) {
      if (!hostBridgeUseMayWrite(anchor, use)) {
        appendUniqueHostBridgeReadObservation(
            findHostBridgeReadObservationAnchor(use),
            collection.readObservationAnchors);
        continue;
      }
      return failure();
    }

    std::optional<codir::CodirAccessMode> mode =
        getCodirDepAccessMode(codelet, *depIndex);
    if (!mode)
      return failure();
    collection.participants.push_back({codelet, *depIndex, *mode});
  }

  if (collection.participants.empty())
    return failure();
  return collection;
}

static inline FailureOr<SmallVector<HostBridgeParticipant>>
collectComputeBlockParticipants(codir::CodeletOp seed, unsigned seedDepIndex,
                                Value hostView, arts::DbAllocOp sourceAlloc) {
  if (!seed || !hostView || !sourceAlloc)
    return failure();

  SmallVector<HostBridgeParticipant> participants;
  for (OpOperand &use : hostView.getUses()) {
    Operation *owner = use.getOwner();
    auto codelet = dyn_cast_or_null<codir::CodeletOp>(owner);
    if (!codelet) {
      if (isa_and_nonnull<memref::DimOp, memref::DeallocOp>(owner))
        continue;
      return failure();
    }

    std::optional<unsigned> depIndex = getCodeletDepOperandIndex(codelet, use);
    if (!depIndex || !isCompatibleComputeBlockParticipant(
                         seed, seedDepIndex, codelet, *depIndex, sourceAlloc))
      return failure();

    std::optional<codir::CodirAccessMode> mode =
        getCodirDepAccessMode(codelet, *depIndex);
    if (!mode)
      return failure();
    participants.push_back({codelet, *depIndex, *mode});
  }

  if (participants.empty())
    return failure();
  return participants;
}

static inline bool hasLoopCarriedHostBridgeObservationHazard(
    scf::ForOp loop, codir::CodeletOp seed, unsigned seedDepIndex,
    Value hostView) {
  if (!loop || !seed || !hostView)
    return true;

  struct Event {
    Operation *eventOp = nullptr;
    bool compatibleBlockWriter = false;
    bool coarseReadObservation = false;
    unsigned ordinal = 0;
  };

  SmallVector<Event> events;
  unsigned ordinal = 0;
  Operation *loopOp = loop.getOperation();
  for (OpOperand &use : hostView.getUses()) {
    Operation *owner = use.getOwner();
    if (!isUseInsideAnchor(loopOp, owner))
      continue;

    auto codelet = dyn_cast<codir::CodeletOp>(owner);
    std::optional<unsigned> depIndex =
        codelet ? getCodeletDepOperandIndex(codelet, use) : std::nullopt;
    Operation *anchor =
        codelet ? findCodirDispatchBridgeAnchor(codelet) : owner;
    Operation *event = findHostBridgeEventUnderAnchor(loopOp, anchor);
    if (!event)
      return true;

    if (depIndex && isCompatibleHostBridgeParticipant(seed, seedDepIndex,
                                                      codelet, *depIndex)) {
      std::optional<codir::CodirAccessMode> mode =
          getCodirDepAccessMode(codelet, *depIndex);
      if (mode && codirAccessMayWrite(*mode))
        events.push_back({event, /*compatibleBlockWriter=*/true,
                          /*coarseReadObservation=*/false, ordinal++});
      continue;
    }

    if (!hostBridgeUseMayWrite(loopOp, use))
      events.push_back({event, /*compatibleBlockWriter=*/false,
                        /*coarseReadObservation=*/true, ordinal++});
  }

  if (events.empty())
    return false;

  Block *eventBlock = nullptr;
  for (const Event &event : events) {
    if (!event.eventOp || !event.eventOp->getBlock())
      return true;
    if (!eventBlock) {
      eventBlock = event.eventOp->getBlock();
      continue;
    }
    if (eventBlock != event.eventOp->getBlock())
      return true;
  }

  llvm::sort(events, [](const Event &lhs, const Event &rhs) {
    if (lhs.eventOp == rhs.eventOp)
      return lhs.ordinal < rhs.ordinal;
    return lhs.eventOp->isBeforeInBlock(rhs.eventOp);
  });

  bool hasCompatibleWriter = llvm::any_of(
      events, [](const Event &event) { return event.compatibleBlockWriter; });
  if (!hasCompatibleWriter)
    return false;

  bool sawCompatibleWriter = false;
  for (const Event &event : events) {
    if (event.compatibleBlockWriter) {
      sawCompatibleWriter = true;
      continue;
    }
    if (event.coarseReadObservation && !sawCompatibleWriter)
      return true;
  }
  return false;
}

static inline bool canHoistHostBridgeAcrossLoop(scf::ForOp loop,
                                                codir::CodeletOp codelet,
                                                unsigned seedDepIndex,
                                                Value hostView) {
  if (!loop || !codelet || !hostView)
    return false;
  if (containsValue(codelet.getParams(), loop.getInductionVar()))
    return false;

  arts::DbAllocOp hostAlloc = findBackingDbAlloc(hostView);
  if (hostAlloc) {
    bool hasCoarseWriteToHost = false;
    loop.walk([&](arts::DbAcquireOp acquire) {
      if (hasCoarseWriteToHost)
        return WalkResult::interrupt();
      if (!arts::DbUtils::isWriterMode(acquire.getMode()))
        return WalkResult::advance();
      if (acquire.getSourcePtr() != hostAlloc.getPtr())
        return WalkResult::advance();
      if (Value sourceGuid = acquire.getSourceGuid();
          sourceGuid && sourceGuid != hostAlloc.getGuid())
        return WalkResult::advance();
      if (acquire.getPartitionModeOr() != arts::PartitionMode::coarse)
        return WalkResult::advance();
      hasCoarseWriteToHost = true;
      return WalkResult::interrupt();
    });
    if (hasCoarseWriteToHost)
      return false;
  }

  Value hostBridgeRoot =
      ::mlir::carts::ValueAnalysis::stripMemrefViewOps(hostView);
  arts::DbAllocOp hostBridgeAlloc = findBackingDbAlloc(hostView);
  bool hasExistingBridgeWriter = false;
  loop.walk([&](codir::CodeletOp nestedCodelet) {
    if (hasExistingBridgeWriter)
      return WalkResult::interrupt();
    if (nestedCodelet == codelet)
      return WalkResult::advance();
    for (auto [idx, dep] : llvm::enumerate(nestedCodelet.getDeps())) {
      std::optional<codir::CodirAccessMode> mode =
          getCodirDepAccessMode(nestedCodelet, static_cast<unsigned>(idx));
      if (!mode || !codirAccessMayWrite(*mode))
        continue;
      arts::DbAllocOp alloc = findBackingDbAlloc(dep);
      if (!alloc || !alloc.getStorageBridgeAttr())
        continue;
      // Only an already-materialized bridge writer for the SAME host data is a
      // staleness hazard. A bridge writer for a different program array (a
      // sibling array that was hoisted on an earlier seed) is independent and
      // must not block hoisting this array's bridge. Match by the host root the
      // dep still references, or by the host DB alloc backing this bridge.
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) !=
              hostBridgeRoot &&
          (!hostBridgeAlloc || findBackingDbAlloc(dep) != hostBridgeAlloc))
        continue;
      hasExistingBridgeWriter = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  if (hasExistingBridgeWriter)
    return false;

  Value hostRoot = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(hostView);
  bool hasPotentialSameRootWriter = false;
  loop.walk([&](codir::CodeletOp nestedCodelet) {
    if (hasPotentialSameRootWriter)
      return WalkResult::interrupt();
    if (nestedCodelet == codelet)
      return WalkResult::advance();
    for (auto [idx, dep] : llvm::enumerate(nestedCodelet.getDeps())) {
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) != hostRoot)
        continue;
      std::optional<codir::CodirStorageViewKind> view =
          getCodirDepStorageViewKind(nestedCodelet, static_cast<unsigned>(idx));
      if (!view || !codirStorageViewUsesComputeBlock(*view))
        continue;
      std::optional<codir::CodirAccessMode> mode =
          getCodirDepAccessMode(nestedCodelet, static_cast<unsigned>(idx));
      if (!mode || !codirAccessMayWrite(*mode))
        continue;
      // A same-root writer that is itself a compatible host-bridge participant
      // writes into the shared block tile this bridge materializes, not back
      // into the coarse host image. For an iterative stencil (the read seed and
      // the write live in the same time-loop iteration), the coarse host array
      // is therefore untouched between the pre-loop copy-in and post-loop
      // copy-out, so the bridge stays loop-invariant and is safe to hoist. Only
      // a writer that bypasses this bridge (writing the coarse host directly)
      // blocks hoisting.
      if (isCompatibleHostBridgeParticipant(
              codelet, seedDepIndex, nestedCodelet, static_cast<unsigned>(idx)))
        continue;
      hasPotentialSameRootWriter = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  if (hasPotentialSameRootWriter)
    return false;

  // A same-root compatible block writer does not touch the coarse host image in
  // the current iteration, but a host-whole read before the first such writer
  // observes the previous iteration's value. Keeping the copy-out after the
  // loop would therefore feed stale host data to the next iteration.
  if (hasLoopCarriedHostBridgeObservationHazard(loop, codelet, seedDepIndex,
                                                hostView))
    return false;

  Region &loopRegion = loop.getRegion();
  for (OpOperand &use : hostView.getUses()) {
    Operation *owner = use.getOwner();
    if (!owner || !loopRegion.isAncestor(owner->getParentRegion()))
      continue;
    auto userCodelet = dyn_cast<codir::CodeletOp>(owner);
    std::optional<unsigned> depIndex =
        userCodelet ? getCodeletDepOperandIndex(userCodelet, use)
                    : std::nullopt;
    if (depIndex && isCompatibleHostBridgeParticipant(codelet, seedDepIndex,
                                                      userCodelet, *depIndex))
      continue;
    if (!hostBridgeUseMayWrite(loop.getOperation(), use))
      continue;
    return false;
  }
  return true;
}

static inline Operation *findCodirHostBridgeAnchor(codir::CodeletOp codelet,
                                                   unsigned depIndex,
                                                   Value hostView) {
  Operation *anchor = findCodirDispatchBridgeAnchor(codelet);
  if (!anchor)
    return nullptr;

  for (Operation *parent = anchor->getParentOp(); parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (!loop)
      continue;
    if (!canHoistHostBridgeAcrossLoop(loop, codelet, depIndex, hostView))
      break;
    anchor = loop.getOperation();
  }
  return anchor;
}

static inline SmallVector<Value>
getBridgeLogicalElementSizes(OpBuilder &builder, Location loc, Value hostView) {
  if (arts::DbAllocOp hostAlloc = findBackingDbAlloc(hostView))
    return SmallVector<Value>(hostAlloc.getElementSizes().begin(),
                              hostAlloc.getElementSizes().end());

  SmallVector<Value> sizes;
  auto memrefType = dyn_cast<MemRefType>(hostView.getType());
  if (!memrefType)
    return sizes;
  sizes.reserve(memrefType.getRank());
  for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim) {
    if (memrefType.isDynamicDim(dim)) {
      sizes.push_back(memref::DimOp::create(builder, loc, hostView, dim));
      continue;
    }
    sizes.push_back(
        createConstantIndex(builder, loc, memrefType.getDimSize(dim)));
  }
  return sizes;
}

static inline void materializeHostBlockElementCopyNest(
    OpBuilder &builder, Location loc, Value hostView, Value blockPayload,
    ArrayRef<Value> copySizes, ArrayRef<Value> hostOffsets,
    ArrayRef<Value> blockOffsets, bool copyIntoBlock,
    SmallVectorImpl<Value> &indices) {
  unsigned dim = indices.size();
  if (dim == copySizes.size()) {
    SmallVector<Value> hostIndices;
    SmallVector<Value> blockIndices;
    hostIndices.reserve(indices.size());
    blockIndices.reserve(indices.size());
    for (auto [idx, value] : llvm::enumerate(indices)) {
      if (idx < blockOffsets.size() && blockOffsets[idx]) {
        blockIndices.push_back(
            arith::AddIOp::create(builder, loc, blockOffsets[idx], value));
      } else {
        blockIndices.push_back(value);
      }
      if (idx < hostOffsets.size() && hostOffsets[idx]) {
        hostIndices.push_back(
            arith::AddIOp::create(builder, loc, hostOffsets[idx], value));
        continue;
      }
      hostIndices.push_back(value);
    }

    if (copyIntoBlock) {
      Value loaded =
          memref::LoadOp::create(builder, loc, hostView, hostIndices);
      memref::StoreOp::create(builder, loc, loaded, blockPayload, blockIndices);
      return;
    }
    Value loaded =
        memref::LoadOp::create(builder, loc, blockPayload, blockIndices);
    memref::StoreOp::create(builder, loc, loaded, hostView, hostIndices);
    return;
  }

  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  auto loop = scf::ForOp::create(builder, loc, zero, copySizes[dim], one);

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(loop.getBody());
  indices.push_back(loop.getInductionVar());
  materializeHostBlockElementCopyNest(builder, loc, hostView, blockPayload,
                                      copySizes, hostOffsets, blockOffsets,
                                      copyIntoBlock, indices);
  indices.pop_back();
}

static inline FailureOr<Value>
materializeCoarseHostDbForBlockArgument(OpBuilder &builder, Location loc,
                                        BlockArgument blockArg) {
  Value root = blockArg;
  auto memrefType = dyn_cast<MemRefType>(root.getType());
  if (!memrefType)
    return failure();

  Block *owner = blockArg.getOwner();
  if (!owner)
    return failure();

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(owner);

  SmallVector<Value> elementSizes;
  if (memrefType.getRank() == 0) {
    elementSizes.push_back(createOneIndex(builder, loc));
  } else {
    elementSizes.reserve(memrefType.getRank());
    for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim) {
      if (memrefType.isDynamicDim(dim)) {
        elementSizes.push_back(memref::DimOp::create(builder, loc, root, dim));
        continue;
      }
      elementSizes.push_back(
          createConstantIndex(builder, loc, memrefType.getDimSize(dim)));
    }
  }

  SmallVector<Operation *> protectedOps;
  for (Value size : elementSizes)
    if (Operation *op = size.getDefiningOp())
      protectedOps.push_back(op);

  Value route = arts::createCurrentNodeRoute(builder, loc);
  auto dbAlloc = arts::DbAllocOp::create(
      builder, loc, arts::ArtsMode::inout, route, arts::DbAllocType::unknown,
      arts::DbMode::write, memrefType.getElementType(), root,
      SmallVector<Value>{createOneIndex(builder, loc)}, std::move(elementSizes),
      arts::PartitionMode::coarse);
  protectedOps.push_back(dbAlloc.getOperation());

  Value replacement = materializeInnerPayload(builder, loc, dbAlloc.getPtr());
  root.replaceUsesWithIf(replacement, [&](OpOperand &use) {
    return !llvm::is_contained(protectedOps, use.getOwner());
  });
  return replacement;
}

static inline FailureOr<Value>
materializeCoarseHostDbForHostBridge(OpBuilder &builder, Location loc,
                                     Value hostView) {
  if (!hostView)
    return failure();
  if (findBackingDbAlloc(hostView))
    return hostView;

  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(hostView);
  if (root != hostView)
    return failure();

  auto memrefType = dyn_cast<MemRefType>(root.getType());
  if (!memrefType)
    return failure();

  if (auto blockArg = dyn_cast<BlockArgument>(root))
    return materializeCoarseHostDbForBlockArgument(builder, loc, blockArg);

  Operation *def = root.getDefiningOp();
  if (!def)
    return failure();

  SmallVector<Value> dynamicSizes;
  OpBuilder::InsertionGuard guard(builder);
  if (auto alloc = dyn_cast<memref::AllocOp>(def)) {
    dynamicSizes.assign(alloc.getDynamicSizes().begin(),
                        alloc.getDynamicSizes().end());
    builder.setInsertionPointAfter(alloc);
  } else if (auto alloca = dyn_cast<memref::AllocaOp>(def)) {
    dynamicSizes.assign(alloca.getDynamicSizes().begin(),
                        alloca.getDynamicSizes().end());
    builder.setInsertionPointAfter(alloca);
  } else if (auto muAlloc = dyn_cast<sde::SdeMuAllocOp>(def)) {
    dynamicSizes.assign(muAlloc.getDynamicSizes().begin(),
                        muAlloc.getDynamicSizes().end());
    builder.setInsertionPointAfter(muAlloc);
  } else {
    return failure();
  }

  SmallVector<memref::DeallocOp> deallocs;
  for (Operation *user : llvm::make_early_inc_range(root.getUsers())) {
    auto dealloc = dyn_cast<memref::DeallocOp>(user);
    if (dealloc && dealloc.getMemref() == root)
      deallocs.push_back(dealloc);
  }

  Value replacement;
  if (failed(createDbBackedMemref(builder, root.getLoc(), memrefType,
                                  dynamicSizes, replacement)))
    return failure();

  root.replaceAllUsesWith(replacement);
  for (memref::DeallocOp dealloc : deallocs)
    dealloc.erase();
  if (def->use_empty())
    def->erase();
  return replacement;
}

static inline arts::DbAcquireOp materializeBridgeAcquire(
    OpBuilder &builder, Location loc, arts::DbAllocOp alloc,
    arts::ArtsMode mode, arts::PartitionMode partitionMode,
    ArrayRef<Value> offsets, ArrayRef<Value> sizes, Value boundsValid = Value{},
    ArrayRef<Value> elementOffsets = {}, ArrayRef<Value> elementSizes = {}) {
  return arts::DbAcquireOp::create(
      builder, loc, mode, alloc.getGuid(), alloc.getPtr(), partitionMode,
      /*indices=*/SmallVector<Value>{},
      SmallVector<Value>(offsets.begin(), offsets.end()),
      SmallVector<Value>(sizes.begin(), sizes.end()),
      /*partitionIndices=*/SmallVector<Value>{},
      /*partitionOffsets=*/SmallVector<Value>{},
      /*partitionSizes=*/SmallVector<Value>{}, boundsValid,
      /*elementOffsets=*/
      SmallVector<Value>(elementOffsets.begin(), elementOffsets.end()),
      /*elementSizes=*/
      SmallVector<Value>(elementSizes.begin(), elementSizes.end()));
}

static inline arts::DbAcquireOp materializeBridgeAcquire(
    OpBuilder &builder, Location loc, arts::DbAllocOp alloc,
    arts::ArtsMode mode, arts::PartitionMode partitionMode, Value offset,
    Value size, Value boundsValid = Value{},
    ArrayRef<Value> elementOffsets = {}, ArrayRef<Value> elementSizes = {}) {
  SmallVector<Value, 1> offsets{offset};
  SmallVector<Value, 1> sizes{size};
  return materializeBridgeAcquire(builder, loc, alloc, mode, partitionMode,
                                  offsets, sizes, boundsValid, elementOffsets,
                                  elementSizes);
}

static inline Value materializeProduct(OpBuilder &builder, Location loc,
                                       ValueRange values) {
  Value product = createOneIndex(builder, loc);
  for (Value value : values)
    product = arith::MulIOp::create(builder, loc, product, value);
  return product;
}

static inline SmallVector<Value>
materializeRowMajorCoordinates(OpBuilder &builder, Location loc, Value ordinal,
                               ValueRange sizes) {
  SmallVector<Value> coords(sizes.size());
  Value remaining = ordinal;
  for (int64_t dim = static_cast<int64_t>(sizes.size()) - 1; dim >= 0; --dim) {
    Value size = sizes[dim];
    coords[dim] = arith::RemUIOp::create(builder, loc, remaining, size);
    if (dim != 0)
      remaining = arith::DivUIOp::create(builder, loc, remaining, size);
  }
  return coords;
}

struct FlatNodeBlockGroupLoop {
  scf::ForOp loop;
  Value nodeOrdinal;
  Value blockBase;
};

static inline FlatNodeBlockGroupLoop
materializeFlatNodeBlockGroupLoop(OpBuilder &builder, Location loc,
                                  Value totalNodes, Value blockCount,
                                  int64_t blockGroupSize) {
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockStep =
      createConstantIndex(builder, loc, std::max<int64_t>(1, blockGroupSize));
  Value blockGroupCount =
      materializeNonNegativeCeilDiv(builder, loc, blockCount, blockStep);
  Value workItemCount =
      arith::MulIOp::create(builder, loc, totalNodes, blockGroupCount);

  auto flatLoop = scf::ForOp::create(builder, loc, zero, workItemCount, one);
  builder.setInsertionPointToStart(flatLoop.getBody());
  Value launchOrdinal = flatLoop.getInductionVar();

  // The loop body is unreachable when blockGroupCount is zero, but keep the
  // divisor nonzero so zero-sized dynamic inputs still have well-formed IR.
  Value emptyBlocks = arith::CmpIOp::create(
      builder, loc, arith::CmpIPredicate::eq, blockGroupCount, zero);
  Value safeBlockGroupCount =
      arith::SelectOp::create(builder, loc, emptyBlocks, one, blockGroupCount);
  Value nodeOrdinal =
      arith::DivUIOp::create(builder, loc, launchOrdinal, safeBlockGroupCount);
  Value blockGroupOrdinal =
      arith::RemUIOp::create(builder, loc, launchOrdinal, safeBlockGroupCount);
  Value blockBase =
      arith::MulIOp::create(builder, loc, blockGroupOrdinal, blockStep);
  return {flatLoop, nodeOrdinal, blockBase};
}

static inline LogicalResult
materializeHostBlockCopyLoop(OpBuilder &builder, Location loc, Value hostView,
                             arts::DbAllocOp blockAlloc,
                             codir::CodeletOp codelet, unsigned depIndex,
                             bool copyIntoBlock, bool crossNodeGather = false,
                             const BridgePlan *bridgePlan = nullptr) {
  auto hostType = dyn_cast<MemRefType>(hostView.getType());
  if (!hostType || hostType.getRank() == 0)
    return failure();
  arts::DbAllocOp hostAlloc = findBackingDbAlloc(hostView);
  if (!hostAlloc)
    return failure();

  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty())
    return failure();
  for (unsigned ownerDim : *ownerDims)
    if (ownerDim >= static_cast<unsigned>(hostType.getRank()))
      return failure();
  if (blockAlloc.getSizes().size() != ownerDims->size() ||
      blockAlloc.getElementSizes().size() !=
          static_cast<size_t>(hostType.getRank()))
    return failure();

  SmallVector<Value> logicalSizes =
      getBridgeLogicalElementSizes(builder, loc, hostView);
  if (logicalSizes.size() != static_cast<size_t>(hostType.getRank()))
    return failure();

  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockCount = materializeProduct(builder, loc, blockAlloc.getSizes());
  std::optional<SmallVector<int64_t, 4>> plannedBlockSizes =
      getCodirTileOwnerBlockSizes(codelet, depIndex,
                                  static_cast<unsigned>(hostType.getRank()));
  SmallVector<CodirOwnerHaloWindow, 4> ownerHalos =
      getCodirBlockStorageHaloWindows(
          codelet, depIndex, static_cast<unsigned>(hostType.getRank()));
  SmallVector<Value, 4> ownerParams =
      getCodirDepOwnerParamValues(codelet, depIndex);

  bool gatherAcrossNodes = crossNodeGather && !copyIntoBlock;
  BridgeWorkGroupPlan groupPlan;
  if (bridgePlan) {
    BridgeWorkloadKind workloadKind = copyIntoBlock
                                          ? BridgeWorkloadKind::host_to_block
                                          : BridgeWorkloadKind::block_to_host;
    groupPlan =
        planBridgeWorkGroups(*bridgePlan, workloadKind, gatherAcrossNodes);
  }
  int64_t blockGroupSize = std::max<int64_t>(1, groupPlan.groupSize);
  Value blockStep = createConstantIndex(builder, loc, blockGroupSize);

  OpBuilder::InsertionGuard guard(builder);
  Value gatherNodeOrdinal;
  scf::ForOp loop;
  Value blockBase;
  if (gatherAcrossNodes) {
    auto totalNodesI32 = arts::RuntimeQueryOp::create(
        builder, loc, arts::RuntimeQueryKind::totalNodes);
    Value totalNodes = arith::IndexCastOp::create(
        builder, loc, builder.getIndexType(), totalNodesI32.getResult());
    FlatNodeBlockGroupLoop flatLoop = materializeFlatNodeBlockGroupLoop(
        builder, loc, totalNodes, blockCount, blockGroupSize);
    loop = flatLoop.loop;
    gatherNodeOrdinal = flatLoop.nodeOrdinal;
    blockBase = flatLoop.blockBase;
  } else {
    loop = scf::ForOp::create(builder, loc, zero, blockCount, blockStep);
    builder.setInsertionPointToStart(loop.getBody());
    blockBase = loop.getInductionVar();
  }

  struct CopyLane {
    SmallVector<Value> blockCoords;
    SmallVector<Value> hostOffsets;
    SmallVector<Value> blockOffsets;
    SmallVector<Value> copySizes;
  };
  SmallVector<CopyLane, 8> lanes;
  lanes.reserve(static_cast<size_t>(blockGroupSize));
  for (int64_t lane = 0; lane < blockGroupSize; ++lane) {
    Value blockOrdinal = blockBase;
    if (lane != 0) {
      Value laneValue = createConstantIndex(builder, loc, lane);
      blockOrdinal = arith::AddIOp::create(builder, loc, blockBase, laneValue);
    }
    CopyLane lanePlan;
    lanePlan.blockCoords = materializeRowMajorCoordinates(
        builder, loc, blockOrdinal, blockAlloc.getSizes());
    lanePlan.hostOffsets.assign(static_cast<size_t>(hostType.getRank()), zero);
    lanePlan.blockOffsets.assign(static_cast<size_t>(hostType.getRank()), zero);
    lanePlan.copySizes.assign(logicalSizes.begin(), logicalSizes.end());
    for (auto [slot, ownerDim] : llvm::enumerate(*ownerDims)) {
      Value ownerBlockSize =
          plannedBlockSizes && slot < plannedBlockSizes->size()
              ? createConstantIndex(builder, loc, (*plannedBlockSizes)[slot])
              : blockAlloc.getElementSizes()[ownerDim];
      Value domainBase =
          slot < ownerParams.size()
              ? materializeCodirOwnerDomainBase(builder, loc, codelet,
                                                ownerParams[slot])
              : materializeCodirOwnerDomainBase(builder, loc, codelet);
      Value ownerBlockOffset = arith::MulIOp::create(
          builder, loc, lanePlan.blockCoords[slot], ownerBlockSize);
      Value ownerOffset =
          arith::AddIOp::create(builder, loc, domainBase, ownerBlockOffset);
      CodirOwnerHaloWindow ownerHalo;
      for (CodirOwnerHaloWindow candidate : ownerHalos) {
        if (candidate.ownerDim == ownerDim) {
          ownerHalo = candidate;
          break;
        }
      }
      Value ownerCopyStart =
          copyIntoBlock
              ? subtractClampZero(builder, loc, ownerOffset, ownerHalo.lower)
              : ownerOffset;
      Value blockPayloadStart =
          subtractClampZero(builder, loc, ownerOffset, ownerHalo.lower);
      Value requestedEnd =
          arith::AddIOp::create(builder, loc, ownerOffset, ownerBlockSize);
      if (copyIntoBlock && ownerHalo.upper > 0)
        requestedEnd = arith::AddIOp::create(
            builder, loc, requestedEnd,
            createConstantIndex(builder, loc, ownerHalo.upper));
      Value ownerCopyEnd = arith::MinUIOp::create(builder, loc, requestedEnd,
                                                  logicalSizes[ownerDim]);
      lanePlan.hostOffsets[ownerDim] = ownerCopyStart;
      if (!copyIntoBlock && ownerHalo.lower > 0)
        lanePlan.blockOffsets[ownerDim] =
            arith::SubIOp::create(builder, loc, ownerOffset, blockPayloadStart);
      lanePlan.copySizes[ownerDim] = materializePositiveDifferenceOrZero(
          builder, loc, ownerCopyEnd, ownerCopyStart);
    }
    lanes.push_back(std::move(lanePlan));
  }

  arts::ArtsMode hostMode =
      copyIntoBlock ? arts::ArtsMode::in : arts::ArtsMode::inout;
  arts::ArtsMode blockMode =
      copyIntoBlock ? arts::ArtsMode::out : arts::ArtsMode::in;
  auto hostAcquire =
      materializeBridgeAcquire(builder, loc, hostAlloc, hostMode,
                               arts::PartitionMode::coarse, zero, one);
  SmallVector<Value> deps{hostAcquire.getPtr()};
  deps.reserve(1 + lanes.size());
  for (const CopyLane &lane : lanes) {
    SmallVector<Value> blockWindowSizes(lane.blockCoords.size(), one);
    auto blockAcquire = materializeBridgeAcquire(
        builder, loc, blockAlloc, blockMode, arts::PartitionMode::block,
        lane.blockCoords, blockWindowSizes);
    deps.push_back(blockAcquire.getPtr());
  }

  SmallVector<Value> params;
  unsigned rank = static_cast<unsigned>(hostType.getRank());
  params.reserve(lanes.size() * rank * 3);
  for (const CopyLane &lane : lanes) {
    params.append(lane.hostOffsets.begin(), lane.hostOffsets.end());
    params.append(lane.blockOffsets.begin(), lane.blockOffsets.end());
    params.append(lane.copySizes.begin(), lane.copySizes.end());
  }

  arts::ArtsLaunchPolicy launch;
  if (copyIntoBlock)
    launch = resolveBridgeBlockOrdinalLaunchPolicy(
        blockAlloc->getParentOfType<ModuleOp>(), bridgePlan, blockAlloc,
        blockBase, builder, loc);
  else if (gatherAcrossNodes)
    launch = arts::resolveArtsOrdinalLaunchPolicy(
        blockAlloc->getParentOfType<ModuleOp>(), gatherNodeOrdinal, builder,
        loc);
  Value route =
      launch.route ? launch.route : arts::createCurrentNodeRoute(builder, loc);
  auto copyTask = arts::EdtOp::create(builder, loc, arts::EdtType::task,
                                      launch.concurrency, route, deps, params);
  copyTask.setStorageBridgeCopyAttr(UnitAttr::get(copyTask.getContext()));
  Block &body = copyTask.getBody().front();
  for (Value dep : deps)
    body.addArgument(dep.getType(), loc);
  for (Value param : params)
    body.addArgument(param.getType(), loc);

  {
    OpBuilder::InsertionGuard bodyGuard(builder);
    builder.setInsertionPointToStart(&body);
    Value hostPayload =
        materializeInnerPayload(builder, loc, body.getArgument(0));
    unsigned paramBase = deps.size();
    for (int64_t lane = 0; lane < blockGroupSize; ++lane) {
      Value blockPayload =
          materializeInnerPayload(builder, loc, body.getArgument(1 + lane));
      unsigned laneParamBase =
          paramBase + static_cast<unsigned>(lane) * rank * 3;
      SmallVector<Value> bodyHostOffsets;
      bodyHostOffsets.reserve(rank);
      for (unsigned i = 0; i < rank; ++i)
        bodyHostOffsets.push_back(body.getArgument(laneParamBase + i));
      SmallVector<Value> bodyBlockOffsets;
      bodyBlockOffsets.reserve(rank);
      for (unsigned i = 0; i < rank; ++i)
        bodyBlockOffsets.push_back(body.getArgument(laneParamBase + rank + i));
      SmallVector<Value> bodyCopySizes;
      bodyCopySizes.reserve(rank);
      for (unsigned i = 0; i < rank; ++i)
        bodyCopySizes.push_back(body.getArgument(laneParamBase + rank * 2 + i));
      SmallVector<Value> indices;
      materializeHostBlockElementCopyNest(
          builder, loc, hostPayload, blockPayload, bodyCopySizes,
          bodyHostOffsets, bodyBlockOffsets, copyIntoBlock, indices);
    }
    arts::YieldOp::create(builder, loc);
  }

  if (!copyIntoBlock) {
    builder.setInsertionPointAfter(loop.getOperation());
    auto reason = arts::ArtsBarrierReasonAttr::get(
        builder.getContext(), arts::ArtsBarrierReason::required_memory);
    arts::BarrierOp::create(builder, loc, reason);
  }
  return success();
}

/// Build the per-element copy nest that fills one gathered block of the
/// replicated DB from the corresponding producer block. Unlike the coarse
/// write-back (materializeHostBlockElementCopyNest), both source and
/// destination are block payloads indexed identically; there is no coarse host
/// offset to add, because each gathered block is a full standalone DB.
static inline void
materializePerBlockCopyNest(OpBuilder &builder, Location loc, Value srcPayload,
                            Value dstPayload, ArrayRef<Value> copySizes,
                            SmallVectorImpl<Value> &indices) {
  unsigned dim = indices.size();
  if (dim == copySizes.size()) {
    Value loaded = memref::LoadOp::create(builder, loc, srcPayload, indices);
    memref::StoreOp::create(builder, loc, loaded, dstPayload, indices);
    return;
  }
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  auto loop = scf::ForOp::create(builder, loc, zero, copySizes[dim], one);
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(loop.getBody());
  indices.push_back(loop.getInductionVar());
  materializePerBlockCopyNest(builder, loc, srcPayload, dstPayload, copySizes,
                              indices);
  indices.pop_back();
}

static inline void materializePerBlockOffsetCopyNest(
    OpBuilder &builder, Location loc, Value srcPayload, Value dstPayload,
    ArrayRef<Value> copySizes, ArrayRef<Value> srcOffsets,
    ArrayRef<Value> dstOffsets, SmallVectorImpl<Value> &indices) {
  unsigned dim = indices.size();
  if (dim == copySizes.size()) {
    SmallVector<Value> srcIndices;
    SmallVector<Value> dstIndices;
    srcIndices.reserve(indices.size());
    dstIndices.reserve(indices.size());
    for (auto [idx, induction] : llvm::enumerate(indices)) {
      srcIndices.push_back(
          arith::AddIOp::create(builder, loc, srcOffsets[idx], induction));
      dstIndices.push_back(
          arith::AddIOp::create(builder, loc, dstOffsets[idx], induction));
    }
    Value loaded = memref::LoadOp::create(builder, loc, srcPayload, srcIndices);
    memref::StoreOp::create(builder, loc, loaded, dstPayload, dstIndices);
    return;
  }
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  auto loop = scf::ForOp::create(builder, loc, zero, copySizes[dim], one);
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(loop.getBody());
  indices.push_back(loop.getInductionVar());
  materializePerBlockOffsetCopyNest(builder, loc, srcPayload, dstPayload,
                                    copySizes, srcOffsets, dstOffsets, indices);
  indices.pop_back();
}

/// Build the per-element summing nest that settles one output block by reducing
/// the P per-tile partial payloads with `+=` (arith.addf) and writing the
/// result ONCE. This is the addf dual of materializePerBlockCopyNest: instead
/// of a single source copy, the leaf loads tile 0, accumulates tiles 1..P-1
/// with arith.addf, and stores once into the settled block. All payloads are
/// block payloads indexed identically (no coarse host offset).
static inline void materializePerBlockSumNest(OpBuilder &builder, Location loc,
                                              ArrayRef<Value> partialPayloads,
                                              Value dstPayload,
                                              ArrayRef<Value> copySizes,
                                              SmallVectorImpl<Value> &indices) {
  unsigned dim = indices.size();
  if (dim == copySizes.size()) {
    Value acc =
        memref::LoadOp::create(builder, loc, partialPayloads.front(), indices);
    for (size_t tile = 1; tile < partialPayloads.size(); ++tile) {
      Value next =
          memref::LoadOp::create(builder, loc, partialPayloads[tile], indices);
      acc = arith::AddFOp::create(builder, loc, acc, next);
    }
    memref::StoreOp::create(builder, loc, acc, dstPayload, indices);
    return;
  }
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  auto loop = scf::ForOp::create(builder, loc, zero, copySizes[dim], one);
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(loop.getBody());
  indices.push_back(loop.getInductionVar());
  materializePerBlockSumNest(builder, loc, partialPayloads, dstPayload,
                             copySizes, indices);
  indices.pop_back();
}

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

    if (!codirDepRequiresComputeBlockStorage(participantCodelet,
                                             participant.depIndex) ||
        !isCodirStencilPattern(participantCodelet))
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

/// Per-block single-writer all-gather substrate. Each gathered block is a
/// distinct replicated DB written once, while the bridge EDT may group adjacent
/// block copies to amortize launch overhead.
static inline FailureOr<Value>
emitPerBlockAllGatherWriteBack(OpBuilder &builder, Location loc, Value hostView,
                               BridgePlan plan) {
  arts::DbAllocOp producerBlockAlloc = plan.sourceAlloc;
  auto hostType = dyn_cast<MemRefType>(hostView.getType());
  if (!hostType || hostType.getRank() == 0)
    return failure();
  ModuleOp module = producerBlockAlloc->getParentOfType<ModuleOp>();
  if (!module || !arts::hasArtsInterNodeRuntime(module))
    return failure();
  if (producerBlockAlloc.getSizes().empty() ||
      producerBlockAlloc.getElementSizes().size() !=
          static_cast<size_t>(hostType.getRank()))
    return failure();

  // Mirror the producer's block layout for the gathered replica, but mark it
  // REPLICATED (local_only, not distributed) so every node materializes all N
  // blocks locally. Each block keeps its own GUID (createMultiDbs), so the
  // single-writer property is per block.
  OpBuilder::InsertionGuard topGuard(builder);
  Value route = arts::createCurrentNodeRoute(builder, loc);
  SmallVector<Value> outerSizes(producerBlockAlloc.getSizes().begin(),
                                producerBlockAlloc.getSizes().end());
  SmallVector<Value> innerSizes(producerBlockAlloc.getElementSizes().begin(),
                                producerBlockAlloc.getElementSizes().end());
  auto replicaAlloc = arts::DbAllocOp::create(
      builder, loc, arts::ArtsMode::inout, route, arts::DbAllocType::heap,
      arts::DbMode::write, hostType.getElementType(), std::move(outerSizes),
      std::move(innerSizes), arts::PartitionMode::block);
  if (auto ownerDims =
          arts::getPlanOwnerDimsAttr(producerBlockAlloc.getOperation()))
    arts::setPlanOwnerDimsAttr(replicaAlloc.getOperation(), ownerDims);
  if (auto blockShape = arts::getPlanPhysicalBlockShapeAttr(
          producerBlockAlloc.getOperation()))
    arts::setPlanPhysicalBlockShapeAttr(replicaAlloc.getOperation(),
                                        blockShape);
  // Replicated, not distributed: every block is local on every node. The
  // perBlockReplicated marker keeps the distributed-ownership pass from
  // block-scattering the gathered blocks (which would defeat the all-gather);
  // the single-writer property holds per block-GUID either way.
  replicaAlloc.setLocalOnlyAttr(UnitAttr::get(replicaAlloc.getContext()));
  replicaAlloc.setPerBlockReplicatedAttr(
      UnitAttr::get(replicaAlloc.getContext()));

  Value one = createOneIndex(builder, loc);
  Value blockCount =
      materializeProduct(builder, loc, producerBlockAlloc.getSizes());
  SmallVector<Value> unitBlockSizes(producerBlockAlloc.getSizes().size(), one);

  SmallVector<Value> blockElementSizes(
      producerBlockAlloc.getElementSizes().begin(),
      producerBlockAlloc.getElementSizes().end());
  BridgeWorkGroupPlan groupPlan =
      planBridgeWorkGroups(plan, BridgeWorkloadKind::all_gather);
  plan.groupSize = groupPlan.groupSize;
  int64_t blockGroupSize = plan.groupSize;

  // One flattened launch loop covers every (node, block-group) pair. Routing
  // each block copy to the derived node ordinal keeps the gathered write
  // owner-local on each node's replica while every lane still acquires a
  // distinct per-block source/destination DB.
  auto totalNodesI32 = arts::RuntimeQueryOp::create(
      builder, loc, arts::RuntimeQueryKind::totalNodes);
  Value totalNodes = arith::IndexCastOp::create(
      builder, loc, builder.getIndexType(), totalNodesI32.getResult());
  FlatNodeBlockGroupLoop flatLoop = materializeFlatNodeBlockGroupLoop(
      builder, loc, totalNodes, blockCount, blockGroupSize);
  Value nodeOrdinal = flatLoop.nodeOrdinal;
  Value blockBase = flatLoop.blockBase;

  SmallVector<Value> deps;
  deps.reserve(static_cast<size_t>(blockGroupSize) * 2);
  for (int64_t lane = 0; lane < blockGroupSize; ++lane) {
    Value blockIndex = blockBase;
    if (lane != 0) {
      Value laneValue = createConstantIndex(builder, loc, lane);
      blockIndex = arith::AddIOp::create(builder, loc, blockBase, laneValue);
    }
    SmallVector<Value> blockCoords = materializeRowMajorCoordinates(
        builder, loc, blockIndex, producerBlockAlloc.getSizes());
    // Source: producer block, read-only (cross-node RO acquire;
    // PREFER_DUPLICATE is applied downstream because the producer DB is
    // distributed/read).
    auto srcAcquire = materializeBridgeAcquire(
        builder, loc, producerBlockAlloc, arts::ArtsMode::in,
        arts::PartitionMode::block, blockCoords, unitBlockSizes);
    // Destination: this gathered block, output-only. Distinct DB per block ⇒
    // single writer ⇒ no shared EW frontier.
    auto dstAcquire = materializeBridgeAcquire(
        builder, loc, replicaAlloc, arts::ArtsMode::out,
        arts::PartitionMode::block, blockCoords, unitBlockSizes);
    deps.push_back(srcAcquire.getPtr());
    deps.push_back(dstAcquire.getPtr());
  }
  SmallVector<Value> params(blockElementSizes.begin(), blockElementSizes.end());

  arts::ArtsLaunchPolicy launch =
      arts::resolveArtsOrdinalLaunchPolicy(module, nodeOrdinal, builder, loc);
  Value taskRoute =
      launch.route ? launch.route : arts::createCurrentNodeRoute(builder, loc);
  auto copyTask =
      arts::EdtOp::create(builder, loc, arts::EdtType::task, launch.concurrency,
                          taskRoute, deps, params);
  copyTask.setStorageBridgeCopyAttr(UnitAttr::get(copyTask.getContext()));
  copyTask.setPerBlockAllGatherAttr(UnitAttr::get(copyTask.getContext()));
  Block &body = copyTask.getBody().front();
  for (Value dep : deps)
    body.addArgument(dep.getType(), loc);
  for (Value param : params)
    body.addArgument(param.getType(), loc);
  {
    OpBuilder::InsertionGuard bodyGuard(builder);
    builder.setInsertionPointToStart(&body);
    unsigned paramBase = deps.size();
    SmallVector<Value> bodyCopySizes;
    bodyCopySizes.reserve(blockElementSizes.size());
    for (size_t i = 0; i < blockElementSizes.size(); ++i)
      bodyCopySizes.push_back(body.getArgument(paramBase + i));
    for (int64_t lane = 0; lane < blockGroupSize; ++lane) {
      unsigned srcArg = static_cast<unsigned>(lane) * 2;
      unsigned dstArg = srcArg + 1;
      Value srcPayload =
          materializeInnerPayload(builder, loc, body.getArgument(srcArg));
      Value dstPayload =
          materializeInnerPayload(builder, loc, body.getArgument(dstArg));
      SmallVector<Value> indices;
      materializePerBlockCopyNest(builder, loc, srcPayload, dstPayload,
                                  bodyCopySizes, indices);
    }
    arts::YieldOp::create(builder, loc);
  }

  builder.setInsertionPointAfter(flatLoop.loop.getOperation());
  auto reason = arts::ArtsBarrierReasonAttr::get(
      builder.getContext(), arts::ArtsBarrierReason::required_memory);
  arts::BarrierOp::create(builder, loc, reason);

  return materializeInnerPayload(builder, loc, replicaAlloc.getPtr());
}

/// Per-block single-writer summing settle. Each output block is written once
/// from the P per-(block,tile) partial blocks, all delivered as EDT deps.
static inline FailureOr<Value>
emitPerBlockSummingSettle(OpBuilder &builder, Location loc,
                          arts::DbAllocOp partialBlockAlloc, unsigned tileCount,
                          codir::CodeletOp codelet, unsigned depIndex,
                          const BridgePlan &bridgePlan) {
  if (tileCount == 0)
    return failure();
  ModuleOp module = partialBlockAlloc->getParentOfType<ModuleOp>();
  if (!module || !arts::hasArtsInterNodeRuntime(module))
    return failure();
  // The partials DB is a single-axis block DB whose element block carries the
  // settled block's footprint; its outer extent counts blockCount * tileCount
  // distinct per-(block,tile) GUIDs.
  if (partialBlockAlloc.getSizes().size() != 1 ||
      partialBlockAlloc.getElementSizes().empty())
    return failure();

  // Mirror the partials' element-block layout for the settle replica, but mark
  // it REPLICATED (local_only, not distributed) so every node materializes all
  // settled blocks locally. Each block keeps its own GUID (createMultiDbs), so
  // the single-writer property is per settled block.
  OpBuilder::InsertionGuard topGuard(builder);
  Value route = arts::createCurrentNodeRoute(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value tileCountVal = createConstantIndex(builder, loc, tileCount);

  // blockCount = partialCount / tileCount (the partials carry tileCount entries
  // per settled block, contiguous on the flat outer axis).
  Value partialCount = partialBlockAlloc.getSizes().front();
  Value blockCount =
      arith::DivUIOp::create(builder, loc, partialCount, tileCountVal);

  SmallVector<Value> blockElementSizes(
      partialBlockAlloc.getElementSizes().begin(),
      partialBlockAlloc.getElementSizes().end());
  // The DbAllocOp `elementType` is the SCALAR element; the ptr result nests one
  // memref level per (block axis + element rank). Mirror the partials' scalar
  // element type so the settle replica's payload has the same rank as a partial
  // block (and the addf body stores scalars, not nested memrefs).
  Type elementType = partialBlockAlloc.getElementType();

  SmallVector<Value> outerSizes{blockCount};
  SmallVector<Value> innerSizes(blockElementSizes.begin(),
                                blockElementSizes.end());
  auto settleAlloc = arts::DbAllocOp::create(
      builder, loc, arts::ArtsMode::inout, route, arts::DbAllocType::heap,
      arts::DbMode::write, elementType, std::move(outerSizes),
      std::move(innerSizes), arts::PartitionMode::block);
  if (auto ownerDims =
          arts::getPlanOwnerDimsAttr(partialBlockAlloc.getOperation()))
    arts::setPlanOwnerDimsAttr(settleAlloc.getOperation(), ownerDims);
  if (auto blockShape =
          arts::getPlanPhysicalBlockShapeAttr(partialBlockAlloc.getOperation()))
    arts::setPlanPhysicalBlockShapeAttr(settleAlloc.getOperation(), blockShape);
  // Replicated, not distributed: every settled block is local on every node, so
  // the distributed-ownership pass must not block-scatter it (that would defeat
  // the allreduce). The single-writer property holds per block-GUID either way.
  settleAlloc.setLocalOnlyAttr(UnitAttr::get(settleAlloc.getContext()));
  settleAlloc.setPerBlockReplicatedAttr(
      UnitAttr::get(settleAlloc.getContext()));

  // One flattened launch loop covers every (node, block-group) pair. Routing
  // each settle to the derived node ordinal keeps the settled write owner-local
  // on each node's replica while every lane still has its own per-block partial
  // deps and one distinct output block dep.
  auto totalNodesI32 = arts::RuntimeQueryOp::create(
      builder, loc, arts::RuntimeQueryKind::totalNodes);
  Value totalNodes = arith::IndexCastOp::create(
      builder, loc, builder.getIndexType(), totalNodesI32.getResult());

  BridgeWorkGroupPlan groupPlan =
      planBridgeWorkGroups(bridgePlan, BridgeWorkloadKind::summing_settle);
  int64_t blockGroupSize = groupPlan.groupSize;
  FlatNodeBlockGroupLoop flatLoop = materializeFlatNodeBlockGroupLoop(
      builder, loc, totalNodes, blockCount, blockGroupSize);
  Value nodeOrdinal = flatLoop.nodeOrdinal;
  Value blockBase = flatLoop.blockBase;

  // Acquire the P per-(block,tile) partials read-only OUTSIDE the EDT (the
  // EdtLowering ABI forbids GEPing an outer DB alloc from the EDT body). Each
  // grouped lane keeps its own partial deps and distinct output block dep, so
  // DB grain remains per block while launch overhead is amortized.
  SmallVector<Value> deps;
  deps.reserve(static_cast<size_t>(blockGroupSize) *
               static_cast<size_t>(tileCount + 1));
  for (int64_t lane = 0; lane < blockGroupSize; ++lane) {
    Value blockIndex = blockBase;
    if (lane != 0) {
      Value laneValue = createConstantIndex(builder, loc, lane);
      blockIndex = arith::AddIOp::create(builder, loc, blockBase, laneValue);
    }
    Value partialBase =
        arith::MulIOp::create(builder, loc, blockIndex, tileCountVal);
    for (unsigned tile = 0; tile < tileCount; ++tile) {
      Value tileVal = createConstantIndex(builder, loc, tile);
      Value partialIndex =
          arith::AddIOp::create(builder, loc, partialBase, tileVal);
      auto partialAcquire = materializeBridgeAcquire(
          builder, loc, partialBlockAlloc, arts::ArtsMode::in,
          arts::PartitionMode::block, partialIndex, one);
      deps.push_back(partialAcquire.getPtr());
    }
    // Destination: this settled block, output-only. Distinct DB per block
    // means each grouped lane still has exactly one writer.
    auto dstAcquire =
        materializeBridgeAcquire(builder, loc, settleAlloc, arts::ArtsMode::out,
                                 arts::PartitionMode::block, blockIndex, one);
    deps.push_back(dstAcquire.getPtr());
  }

  SmallVector<Value> params(blockElementSizes.begin(), blockElementSizes.end());

  arts::ArtsLaunchPolicy launch =
      arts::resolveArtsOrdinalLaunchPolicy(module, nodeOrdinal, builder, loc);
  Value taskRoute =
      launch.route ? launch.route : arts::createCurrentNodeRoute(builder, loc);
  auto settleTask =
      arts::EdtOp::create(builder, loc, arts::EdtType::task, launch.concurrency,
                          taskRoute, deps, params);
  settleTask.setStorageBridgeCopyAttr(UnitAttr::get(settleTask.getContext()));
  settleTask.setPerBlockSummingSettleAttr(
      UnitAttr::get(settleTask.getContext()));
  Block &body = settleTask.getBody().front();
  for (Value dep : deps)
    body.addArgument(dep.getType(), loc);
  for (Value param : params)
    body.addArgument(param.getType(), loc);
  {
    OpBuilder::InsertionGuard bodyGuard(builder);
    builder.setInsertionPointToStart(&body);
    SmallVector<Value> bodyCopySizes;
    bodyCopySizes.reserve(blockElementSizes.size());
    for (size_t i = 0; i < blockElementSizes.size(); ++i)
      bodyCopySizes.push_back(body.getArgument(deps.size() + i));
    for (int64_t lane = 0; lane < blockGroupSize; ++lane) {
      unsigned laneDepBase =
          static_cast<unsigned>(lane) * static_cast<unsigned>(tileCount + 1);
      SmallVector<Value> partialPayloads;
      partialPayloads.reserve(tileCount);
      for (unsigned tile = 0; tile < tileCount; ++tile)
        partialPayloads.push_back(materializeInnerPayload(
            builder, loc, body.getArgument(laneDepBase + tile)));
      Value dstPayload = materializeInnerPayload(
          builder, loc, body.getArgument(laneDepBase + tileCount));
      SmallVector<Value> indices;
      materializePerBlockSumNest(builder, loc, partialPayloads, dstPayload,
                                 bodyCopySizes, indices);
    }
    arts::YieldOp::create(builder, loc);
  }

  builder.setInsertionPointAfter(flatLoop.loop.getOperation());
  auto reason = arts::ArtsBarrierReasonAttr::get(
      builder.getContext(), arts::ArtsBarrierReason::required_memory);
  arts::BarrierOp::create(builder, loc, reason);

  return materializeInnerPayload(builder, loc, settleAlloc.getPtr());
}

static inline LogicalResult
preparePerBlockSingleWriterStencilDb(arts::DbAllocOp blockAlloc) {
  if (!blockAlloc)
    return failure();
  if (blockAlloc.getSizes().empty() || blockAlloc.getElementSizes().empty())
    return failure();
  blockAlloc.removeLocalOnlyAttr();
  blockAlloc.setPerBlockSingleWriterStencilAttr(
      UnitAttr::get(blockAlloc.getContext()));
  return success();
}

static inline FailureOr<Value> emitPerBlockSingleWriterStencilDb(
    OpBuilder &builder, Location loc, arts::DbAllocOp blockAlloc,
    codir::CodeletOp codelet, unsigned depIndex, ValueRange phaseTokens = {},
    const BridgePlan *bridgePlan = nullptr) {
  if (failed(preparePerBlockSingleWriterStencilDb(blockAlloc)))
    return failure();
  ModuleOp module = blockAlloc->getParentOfType<ModuleOp>();

  OpBuilder::InsertionGuard topGuard(builder);
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockCount = materializeProduct(builder, loc, blockAlloc.getSizes());

  SmallVector<Value> blockElementSizes(blockAlloc.getElementSizes().begin(),
                                       blockAlloc.getElementSizes().end());
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  std::optional<SmallVector<int64_t, 4>> ownerBlockSizes =
      getCodirTileOwnerBlockSizes(
          codelet, depIndex, static_cast<unsigned>(blockElementSizes.size()));
  if (!ownerDims || !ownerBlockSizes ||
      ownerDims->size() != blockAlloc.getSizes().size() ||
      ownerDims->size() != ownerBlockSizes->size())
    return failure();

  SmallVector<CodirOwnerHaloWindow, 4> ownerHalos =
      getCodirBlockStorageHaloWindows(
          codelet, depIndex, static_cast<unsigned>(blockElementSizes.size()));
  if (ownerHalos.size() != ownerDims->size())
    return failure();

  BridgeWorkGroupPlan groupPlan;
  if (bridgePlan)
    groupPlan = planBridgeWorkGroups(*bridgePlan, BridgeWorkloadKind::halo);
  int64_t blockGroupSize = std::max<int64_t>(1, groupPlan.groupSize);
  Value blockStep = createConstantIndex(builder, loc, blockGroupSize);

  auto blockLoop =
      scf::ForOp::create(builder, loc, zero, blockCount, blockStep);
  builder.setInsertionPointToStart(blockLoop.getBody());
  Value blockBase = blockLoop.getInductionVar();
  struct HaloCopyAction {
    unsigned dstArg = 0;
    unsigned depArg = 0;
    unsigned ownerSlot = 0;
    bool lower = true;
    int64_t width = 0;
    unsigned conditionParam = 0;
    bool compactSource = false;
  };
  struct HaloLanePlan {
    SmallVector<Value, 4> blockCoords;
    SmallVector<Value, 4> blockWindowSizes;
    unsigned dstArg = 0;
  };
  struct HaloSlicePlan {
    SmallVector<Value, 4> elementOffsets;
    SmallVector<Value, 4> elementSizes;
    bool compact = false;
  };
  SmallVector<HaloCopyAction, 8> copyActions;
  SmallVector<Value, 8> actionConditions;
  SmallVector<Value> deps;
  deps.reserve(static_cast<size_t>(blockGroupSize) *
               (1 + ownerHalos.size() * 2));
  SmallVector<HaloLanePlan, 8> lanes;
  lanes.reserve(static_cast<size_t>(blockGroupSize));
  SmallVector<int64_t, 4> blockStrides =
      getStaticRowMajorBlockStrides(blockAlloc);
  for (int64_t lane = 0; lane < blockGroupSize; ++lane) {
    Value blockOrdinal = blockBase;
    if (lane != 0) {
      Value laneValue = createConstantIndex(builder, loc, lane);
      blockOrdinal = arith::AddIOp::create(builder, loc, blockBase, laneValue);
    }
    SmallVector<Value> blockCoords = materializeRowMajorCoordinates(
        builder, loc, blockOrdinal, blockAlloc.getSizes());
    SmallVector<Value> blockWindowSizes(blockCoords.size(), one);

    auto dstAcquire = materializeBridgeAcquire(
        builder, loc, blockAlloc, arts::ArtsMode::out,
        arts::PartitionMode::block, blockCoords, blockWindowSizes);
    dstAcquire.setPreserveAccessMode();
    unsigned dstArg = static_cast<unsigned>(deps.size());
    deps.push_back(dstAcquire.getPtr());

    lanes.push_back(
        {std::move(blockCoords), std::move(blockWindowSizes), dstArg});
  }

  auto getInGroupSourceArg = [&](int64_t lane, unsigned slot,
                                 bool lower) -> std::optional<unsigned> {
    if (blockGroupSize <= 1 || slot >= blockStrides.size())
      return std::nullopt;
    int64_t stride = blockStrides[slot];
    if (stride <= 0)
      return std::nullopt;
    int64_t sourceLane = lower ? lane - stride : lane + stride;
    if (sourceLane < 0 || sourceLane >= static_cast<int64_t>(lanes.size()))
      return std::nullopt;
    return lanes[static_cast<size_t>(sourceLane)].dstArg;
  };
  std::optional<SmallVector<int64_t, 4>> staticElementSizes =
      getStaticElementSizes(blockAlloc);
  auto buildSourceSlicePlan = [&](unsigned actionSlot, bool lower,
                                  int64_t width) {
    HaloSlicePlan slice;
    if (!staticElementSizes)
      return slice;
    SmallVector<int64_t, 4> staticOffsets(staticElementSizes->size(), 0);
    SmallVector<int64_t, 4> staticSizes(staticElementSizes->begin(),
                                        staticElementSizes->end());
    for (auto [slot, halo] : llvm::enumerate(ownerHalos)) {
      unsigned ownerDim = (*ownerDims)[slot];
      if (ownerDim >= staticSizes.size())
        return HaloSlicePlan{};
      int64_t blockSize = (*ownerBlockSizes)[slot];
      if (slot == actionSlot) {
        staticSizes[ownerDim] = width;
        staticOffsets[ownerDim] =
            lower ? halo.lower + blockSize - width : halo.lower;
      } else {
        staticSizes[ownerDim] = blockSize;
        staticOffsets[ownerDim] = halo.lower;
      }
    }
    if (!isStaticContiguousElementSlice(staticOffsets, staticSizes,
                                        *staticElementSizes))
      return slice;
    slice.compact = true;
    slice.elementOffsets.reserve(staticOffsets.size());
    slice.elementSizes.reserve(staticSizes.size());
    for (int64_t offset : staticOffsets)
      slice.elementOffsets.push_back(createConstantIndex(builder, loc, offset));
    for (int64_t size : staticSizes)
      slice.elementSizes.push_back(createConstantIndex(builder, loc, size));
    return slice;
  };

  for (auto [lane, lanePlan] : llvm::enumerate(lanes)) {
    for (auto [slot, coord] : llvm::enumerate(lanePlan.blockCoords)) {
      CodirOwnerHaloWindow halo = ownerHalos[slot];
      Value lastCoord =
          arith::SubIOp::create(builder, loc, blockAlloc.getSizes()[slot], one);
      Value lowerRaw = arith::SubIOp::create(builder, loc, coord, one);
      Value hasLower = arith::CmpIOp::create(
          builder, loc, arith::CmpIPredicate::ugt, coord, zero);
      Value lowerCoord =
          arith::SelectOp::create(builder, loc, hasLower, lowerRaw, zero);
      Value upperRaw = arith::AddIOp::create(builder, loc, coord, one);
      Value hasUpper = arith::CmpIOp::create(
          builder, loc, arith::CmpIPredicate::ult, coord, lastCoord);
      Value upperCoord =
          arith::MinUIOp::create(builder, loc, upperRaw, lastCoord);

      SmallVector<Value> lowerCoords(lanePlan.blockCoords.begin(),
                                     lanePlan.blockCoords.end());
      SmallVector<Value> upperCoords(lanePlan.blockCoords.begin(),
                                     lanePlan.blockCoords.end());
      lowerCoords[slot] = lowerCoord;
      upperCoords[slot] = upperCoord;
      if (halo.lower > 0) {
        std::optional<unsigned> sourceArg = getInGroupSourceArg(
            static_cast<int64_t>(lane), static_cast<unsigned>(slot), true);
        bool compactSource = false;
        if (!sourceArg) {
          HaloSlicePlan sourceSlice = buildSourceSlicePlan(
              static_cast<unsigned>(slot), true, halo.lower);
          auto lowerAcquire = materializeBridgeAcquire(
              builder, loc, blockAlloc, arts::ArtsMode::in,
              arts::PartitionMode::block, lowerCoords,
              lanePlan.blockWindowSizes, hasLower, sourceSlice.elementOffsets,
              sourceSlice.elementSizes);
          compactSource = sourceSlice.compact;
          sourceArg = static_cast<unsigned>(deps.size());
          deps.push_back(lowerAcquire.getPtr());
        }
        unsigned conditionParam = actionConditions.size();
        actionConditions.push_back(hasLower);
        copyActions.push_back({lanePlan.dstArg, *sourceArg,
                               static_cast<unsigned>(slot), true, halo.lower,
                               conditionParam, compactSource});
      }
      if (halo.upper > 0) {
        std::optional<unsigned> sourceArg = getInGroupSourceArg(
            static_cast<int64_t>(lane), static_cast<unsigned>(slot), false);
        bool compactSource = false;
        if (!sourceArg) {
          HaloSlicePlan sourceSlice = buildSourceSlicePlan(
              static_cast<unsigned>(slot), false, halo.upper);
          auto upperAcquire = materializeBridgeAcquire(
              builder, loc, blockAlloc, arts::ArtsMode::in,
              arts::PartitionMode::block, upperCoords,
              lanePlan.blockWindowSizes, hasUpper, sourceSlice.elementOffsets,
              sourceSlice.elementSizes);
          compactSource = sourceSlice.compact;
          sourceArg = static_cast<unsigned>(deps.size());
          deps.push_back(upperAcquire.getPtr());
        }
        unsigned conditionParam = actionConditions.size();
        actionConditions.push_back(hasUpper);
        copyActions.push_back({lanePlan.dstArg, *sourceArg,
                               static_cast<unsigned>(slot), false, halo.upper,
                               conditionParam, compactSource});
      }
    }
  }
  SmallVector<Value> params(actionConditions.begin(), actionConditions.end());
  params.append(blockElementSizes.begin(), blockElementSizes.end());
  params.append(phaseTokens.begin(), phaseTokens.end());

  arts::ArtsLaunchPolicy launch = resolveBridgeBlockOrdinalLaunchPolicy(
      module, bridgePlan, blockAlloc, blockBase, builder, loc);
  Value taskRoute =
      launch.route ? launch.route : arts::createCurrentNodeRoute(builder, loc);
  auto haloTask =
      arts::EdtOp::create(builder, loc, arts::EdtType::task, launch.concurrency,
                          taskRoute, deps, params);
  haloTask.setStorageBridgeCopyAttr(UnitAttr::get(haloTask.getContext()));
  haloTask.setPerBlockHaloExchangeAttr(UnitAttr::get(haloTask.getContext()));
  Block &body = haloTask.getBody().front();
  for (Value dep : deps)
    body.addArgument(dep.getType(), loc);
  for (Value param : params)
    body.addArgument(param.getType(), loc);
  {
    OpBuilder::InsertionGuard bodyGuard(builder);
    builder.setInsertionPointToStart(&body);
    SmallVector<Value> bodyCopySizes;
    bodyCopySizes.reserve(blockElementSizes.size());
    unsigned sizeParamBase =
        static_cast<unsigned>(deps.size() + actionConditions.size());
    for (size_t i = 0; i < blockElementSizes.size(); ++i)
      bodyCopySizes.push_back(body.getArgument(sizeParamBase + i));
    for (const HaloCopyAction &action : copyActions) {
      Value condition = body.getArgument(deps.size() + action.conditionParam);
      auto copyIf = scf::IfOp::create(builder, loc, TypeRange{}, condition,
                                      /*withElseRegion=*/false);
      OpBuilder::InsertionGuard ifGuard(builder);
      builder.setInsertionPointToStart(&copyIf.getThenRegion().front());
      Value dstPayload = materializeInnerPayload(
          builder, loc, body.getArgument(action.dstArg));
      Value srcPayload = materializeInnerPayload(
          builder, loc, body.getArgument(action.depArg));
      SmallVector<Value> copySizes(bodyCopySizes.begin(), bodyCopySizes.end());
      SmallVector<Value> srcOffsets(blockElementSizes.size(),
                                    createZeroIndex(builder, loc));
      SmallVector<Value> dstOffsets(blockElementSizes.size(),
                                    createZeroIndex(builder, loc));
      for (auto [slot, halo] : llvm::enumerate(ownerHalos)) {
        unsigned ownerDim = (*ownerDims)[slot];
        if (ownerDim >= copySizes.size())
          continue;
        int64_t blockSize = (*ownerBlockSizes)[slot];
        Value blockSizeValue = createConstantIndex(builder, loc, blockSize);
        if (slot == action.ownerSlot) {
          copySizes[ownerDim] = createConstantIndex(builder, loc, action.width);
          int64_t srcStart =
              action.lower ? halo.lower + blockSize - action.width : halo.lower;
          int64_t dstStart = action.lower ? 0 : halo.lower + blockSize;
          if (!action.compactSource)
            srcOffsets[ownerDim] = createConstantIndex(builder, loc, srcStart);
          dstOffsets[ownerDim] = createConstantIndex(builder, loc, dstStart);
        } else {
          copySizes[ownerDim] = blockSizeValue;
          Value haloLower = createConstantIndex(builder, loc, halo.lower);
          if (!action.compactSource)
            srcOffsets[ownerDim] = haloLower;
          dstOffsets[ownerDim] = haloLower;
        }
      }
      SmallVector<Value> indices;
      materializePerBlockOffsetCopyNest(builder, loc, srcPayload, dstPayload,
                                        copySizes, srcOffsets, dstOffsets,
                                        indices);
    }
    arts::YieldOp::create(builder, loc);
  }

  builder.setInsertionPointAfter(blockLoop);
  auto reason = arts::ArtsBarrierReasonAttr::get(
      builder.getContext(), arts::ArtsBarrierReason::required_memory);
  arts::BarrierOp::create(builder, loc, reason);

  return materializeInnerPayload(builder, loc, blockAlloc.getPtr());
}

static inline Operation *findCodirOwnerDispatchAnchor(codir::CodeletOp codelet,
                                                      unsigned depIndex) {
  SmallVector<Value, 4> ownerParams =
      getCodirDepOwnerParamValues(codelet, depIndex);
  Operation *anchor = nullptr;
  bool matched = false;
  for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (!loop) {
      if (matched)
        break;
      continue;
    }
    if (containsValue(ownerParams, loop.getInductionVar())) {
      anchor = parent;
      matched = true;
      continue;
    }
    if (matched)
      break;
  }
  return anchor ? anchor : findCodirDispatchBridgeAnchor(codelet);
}

static inline SmallVector<Value, 4>
collectEnclosingControlTokens(Operation *op) {
  SmallVector<Value, 4> tokens;
  for (Operation *parent = op ? op->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    if (auto loop = dyn_cast<scf::ForOp>(parent)) {
      tokens.push_back(loop.getInductionVar());
      continue;
    }
    if (auto ifOp = dyn_cast<scf::IfOp>(parent))
      tokens.push_back(ifOp.getCondition());
  }
  return tokens;
}

static inline bool
isHaloReadParticipant(const HostBridgeParticipant &participant) {
  return codirAccessMayRead(participant.mode) &&
         codirDepUsesHaloStencilStorage(participant.codelet,
                                        participant.depIndex);
}

static inline LogicalResult emitPerBlockStencilHaloBeforeReadPhases(
    OpBuilder &builder, Location loc, arts::DbAllocOp blockAlloc,
    ArrayRef<HostBridgeParticipant> participants,
    const BridgePlan *bridgePlan = nullptr) {
  SmallVector<Operation *, 4> emittedAnchors;
  for (const HostBridgeParticipant &participant : participants) {
    if (!isHaloReadParticipant(participant))
      continue;
    Operation *dispatchAnchor =
        findCodirOwnerDispatchAnchor(participant.codelet, participant.depIndex);
    if (!dispatchAnchor)
      return failure();
    if (llvm::is_contained(emittedAnchors, dispatchAnchor))
      continue;
    emittedAnchors.push_back(dispatchAnchor);

    builder.setInsertionPoint(dispatchAnchor);
    SmallVector<Value, 4> phaseTokens =
        collectEnclosingControlTokens(dispatchAnchor);
    if (failed(emitPerBlockSingleWriterStencilDb(
            builder, loc, blockAlloc, participant.codelet, participant.depIndex,
            phaseTokens, bridgePlan)))
      return failure();
  }
  return success();
}

static inline FailureOr<Value>
materializeHostWholeToComputeBlockBridge(codir::CodeletOp codelet,
                                         unsigned depIndex, Value hostView) {
  if (!codelet || depIndex >= codelet.getDeps().size() || !hostView)
    return failure();
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex))
    return failure();
  if (!hasCodirTileOwnerSlicePlan(codelet) ||
      !codirDepCanUseBlockStorageAccess(codelet, depIndex))
    return failure();

  std::optional<codir::CodirAccessMode> depMode =
      getCodirDepAccessMode(codelet, depIndex);
  if (!depMode)
    return failure();

  auto memrefType = dyn_cast<MemRefType>(hostView.getType());
  if (!memrefType || memrefType.getRank() == 0 || isCodirViewDep(hostView))
    return failure();

  Operation *anchor = findCodirHostBridgeAnchor(codelet, depIndex, hostView);
  if (!anchor)
    return failure();

  SmallVector<HostBridgeParticipant> participants;
  SmallVector<Operation *> readObservationAnchors;
  FailureOr<HostBridgeUseCollection> collected =
      collectHostBridgeParticipants(anchor, codelet, depIndex, hostView);
  if (succeeded(collected)) {
    participants = std::move(collected->participants);
    readObservationAnchors = std::move(collected->readObservationAnchors);
  } else {
    participants.push_back({codelet, depIndex, *depMode});
  }

  bool needsCopyIn = hostBridgeNeedsInitialCopyIn(anchor, participants);
  bool needsCopyOut = llvm::any_of(participants, [](const auto &participant) {
    return codirAccessMayWrite(participant.mode);
  });

  OpBuilder builder(anchor);
  Location loc = codelet.getLoc();
  FailureOr<Value> materializedHostView =
      materializeCoarseHostDbForHostBridge(builder, loc, hostView);
  if (failed(materializedHostView))
    return failure();
  hostView = *materializedHostView;

  builder.setInsertionPoint(anchor);
  SmallVector<Value> dynamicSizes;
  SmallVector<Value> logicalSizes =
      getBridgeLogicalElementSizes(builder, loc, hostView);
  if (logicalSizes.size() != static_cast<size_t>(memrefType.getRank()))
    return failure();
  for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim) {
    if (memrefType.isDynamicDim(dim))
      dynamicSizes.push_back(logicalSizes[dim]);
  }

  Value blockView;
  if (failed(createDbBackedMemref(builder, loc, memrefType, dynamicSizes,
                                  blockView, codelet, depIndex)))
    return failure();
  arts::DbAllocOp blockAlloc = findBackingDbAlloc(blockView);
  if (!blockAlloc)
    return failure();
  blockAlloc.setStorageBridgeAttr(arts::StorageBridgeAttr::get(
      builder.getContext(), arts::StorageBridge::host_whole_to_compute_block));

  ModuleOp bridgeModule = codelet->getParentOfType<ModuleOp>();
  bool hasInterNodeRuntime =
      bridgeModule && arts::hasArtsInterNodeRuntime(bridgeModule);
  BridgePlan bridgePlan = buildBridgePlan(
      codelet, depIndex, hostView, blockAlloc, blockAlloc, participants,
      readObservationAnchors, needsCopyIn, needsCopyOut, hasInterNodeRuntime);
  codir::CodirCollectiveKind unsupportedCollective =
      codir::CodirCollectiveKind::none;
  if (bridgePlanHasUnsupportedCollective(bridgePlan, unsupportedCollective)) {
    return codelet.emitOpError()
           << "cannot materialize CODIR collective "
           << getCollectiveKindName(unsupportedCollective)
           << " for host-whole to compute-block bridge; add a generic "
              "BridgePlan materializer instead of falling back to a coarse "
              "copy";
  }

  // Dispatch the concrete bridge realization from CODIR's per-dep collective
  // carrier. The BridgePlan is intentionally generic; this first slice keeps
  // the existing emitters intact while moving workload sizing onto the plan.
  bool perBlockAllGather =
      bridgePlan.needsCopyOut && bridgePlan.hasInterNodeRuntime &&
      bridgePlanHasCollective(bridgePlan,
                              codir::CodirCollectiveKind::all_gather);

  bool crossNodeGatherCopyOut =
      bridgePlan.needsCopyOut &&
      bridgePlanHasCollective(bridgePlan,
                              codir::CodirCollectiveKind::reduce_scatter);

  // Block-native summing settle follows from CODIR's committed reduce_scatter
  // storage transition. The split factor gives the number of partial blocks to
  // sum.
  bool perBlockSummingSettle =
      bridgePlan.needsCopyOut &&
      llvm::any_of(bridgePlan.participants,
                   [](const HostBridgeParticipant &participant) {
                     return codirDepUsesBlockNativeSettle(participant.codelet,
                                                          participant.depIndex);
                   });

  // Iterative stencil halo uses a distributed per-block DB plus
  // nearest-neighbor RO reads. This follows the committed CODIR `halo`
  // participant, not the presence of a copy-out writer in the same bridge.
  bool perBlockStencilHalo = bridgePlanHasHaloStencilStorage(bridgePlan);

  // Read-only stencil bridges with zero reach along the committed owner
  // dimensions are block-local after the host-whole -> compute-block copy-in.
  // Commit the existing per-block single-writer stencil fact here so ARTS
  // realizes distributed ownership from CODIR's storage transition instead of
  // re-deriving a benchmark-specific exception.
  bool ownerLocalReadOnlyStencilBridge =
      bridgePlanHasOwnerLocalReadOnlyStencilStorage(bridgePlan);
  if (ownerLocalReadOnlyStencilBridge) {
    if (failed(preparePerBlockSingleWriterStencilDb(blockAlloc)))
      return failure();
  }

  if (needsCopyIn) {
    if (failed(materializeHostBlockCopyLoop(
            builder, loc, hostView, blockAlloc, codelet, depIndex,
            /*copyIntoBlock=*/true,
            /*crossNodeGather=*/false, &bridgePlan)))
      return failure();
  }

  if (perBlockStencilHalo) {
    if (failed(preparePerBlockSingleWriterStencilDb(blockAlloc)))
      return failure();
    if (failed(emitPerBlockStencilHaloBeforeReadPhases(
            builder, loc, blockAlloc, participants, &bridgePlan)))
      return failure();
  }

  if (needsCopyOut) {
    for (HostBridgeParticipant &participant : participants) {
      if (codirAccessMayWrite(participant.mode))
        participant.codelet.setCompletionBarrierAttr(
            UnitAttr::get(participant.codelet->getContext()));
    }
    SmallVector<Operation *> syncAnchors = filterHostBridgeReadSyncAnchors(
        anchor, participants, readObservationAnchors);
    for (Operation *observationAnchor : syncAnchors) {
      if (!observationAnchor || !isUseInsideAnchor(anchor, observationAnchor))
        continue;
      builder.setInsertionPoint(observationAnchor);
      if (failed(materializeHostBlockCopyLoop(
              builder, loc, hostView, blockAlloc, codelet, depIndex,
              /*copyIntoBlock=*/false, crossNodeGatherCopyOut, &bridgePlan)))
        return failure();
    }
    builder.setInsertionPointAfter(anchor);
    if (failed(materializeHostBlockCopyLoop(
            builder, loc, hostView, blockAlloc, codelet, depIndex,
            /*copyIntoBlock=*/false, crossNodeGatherCopyOut, &bridgePlan)))
      return failure();

    // Keep the coarse write-back for whole-array consumers and add the
    // block-native replicated DB substrate for tiled consumers.
    if (perBlockAllGather) {
      builder.setInsertionPointAfter(anchor);
      FailureOr<Value> gathered =
          emitPerBlockAllGatherWriteBack(builder, loc, hostView, bridgePlan);
      if (failed(gathered))
        return failure();
    }

    // Block-native summing settle writes one distinct result block per EDT.
    if (perBlockSummingSettle) {
      unsigned tileCount = 0;
      if (auto factor = codelet.getPartialReductionSplitFactorAttr())
        if (factor.getInt() > 0)
          tileCount = static_cast<unsigned>(factor.getInt());
      if (tileCount > 0) {
        builder.setInsertionPointAfter(anchor);
        FailureOr<Value> settled = emitPerBlockSummingSettle(
            builder, loc, blockAlloc, tileCount, codelet, depIndex, bridgePlan);
        if (failed(settled))
          return failure();
      }
    }
  }

  for (HostBridgeParticipant &participant : participants)
    participant.codelet->setOperand(participant.depIndex, blockView);
  return blockView;
}

static inline LogicalResult
materializeExistingDbHostBridgeIfNeeded(codir::CodeletOp codelet,
                                        unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return failure();

  Value dep = codelet.getDeps()[depIndex];
  arts::DbAllocOp hostAlloc = findBackingDbAlloc(dep);
  if (!hostAlloc)
    return success();
  // Phase-redistributed deps AND iterative-stencil halo deps both need the
  // host-whole -> compute-block bridge: the bridge is where the per-block
  // single-writer stencil DB + halo exchange are realized
  // (perBlockStencilHalo). A committed compute_block+halo stencil with a coarse
  // backing host DB would otherwise never reach the bridge and stay coarse
  // local_only (not distributed).
  bool needsBridgeForRootHaloParticipant =
      codirDepRequiresComputeBlockStorage(codelet, depIndex) &&
      codirRootHasHaloStencilStorageParticipant(codelet, depIndex);
  bool needsBridgeForCommittedComputeBlock =
      codirDepRequiresComputeBlockStorage(codelet, depIndex) &&
      !canUseCodirOwnerSliceForAlloc(codelet, depIndex, hostAlloc);
  if (!codirDepRequiresPhaseRedistributionBridge(codelet, depIndex) &&
      !codirDepUsesHaloStencilStorage(codelet, depIndex) &&
      !needsBridgeForRootHaloParticipant &&
      !needsBridgeForCommittedComputeBlock)
    return success();
  if (failed(requireFinalizedCodirDepOwnerDimsForMaterialization(codelet,
                                                                 depIndex)))
    return failure();
  if (!hasCodirTileOwnerSlicePlan(codelet) || isCodirViewDep(dep))
    return success();
  if (canUseCodirOwnerSliceForAlloc(codelet, depIndex, hostAlloc))
    return success();

  std::optional<arts::PartitionMode> hostMode =
      arts::getPartitionMode(hostAlloc.getOperation());
  if (!hostMode || *hostMode != arts::PartitionMode::coarse)
    return success();
  FailureOr<Value> replacement =
      materializeHostWholeToComputeBlockBridge(codelet, depIndex, dep);
  return failed(replacement) ? failure() : success();
}

static inline LogicalResult
materializeExistingDbComputeBlockIfNeeded(codir::CodeletOp codelet,
                                          unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return failure();
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex) ||
      codirDepRequiresPhaseRedistributionBridge(codelet, depIndex))
    return success();
  if (!hasCodirTileOwnerSlicePlan(codelet) ||
      !codirDepCanUseBlockStorageAccess(codelet, depIndex))
    return success();
  if (failed(requireFinalizedCodirDepOwnerDimsForMaterialization(codelet,
                                                                 depIndex)))
    return failure();

  Value dep = codelet.getDeps()[depIndex];
  Value hostRoot = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep);
  arts::DbAllocOp sourceAlloc = findBackingDbAlloc(hostRoot);
  if (!sourceAlloc)
    return success();
  if (canUseCodirOwnerSliceForAlloc(codelet, depIndex, sourceAlloc))
    return success();

  std::optional<arts::PartitionMode> sourceMode =
      arts::getPartitionMode(sourceAlloc.getOperation());
  if (!sourceMode || *sourceMode != arts::PartitionMode::coarse)
    return success();

  auto memrefType = dyn_cast<MemRefType>(dep.getType());
  if (!memrefType || memrefType.getRank() == 0)
    return success();

  FailureOr<SmallVector<HostBridgeParticipant>> participants =
      collectComputeBlockParticipants(codelet, depIndex, dep, sourceAlloc);
  if (failed(participants))
    return success();

  SmallVector<Value> dynamicSizes;
  for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim)
    if (memrefType.isDynamicDim(dim)) {
      if (static_cast<size_t>(dim) >= sourceAlloc.getElementSizes().size())
        return failure();
      dynamicSizes.push_back(sourceAlloc.getElementSizes()[dim]);
    }

  OpBuilder builder(sourceAlloc);
  builder.setInsertionPointAfter(sourceAlloc);
  Value blockView;
  if (failed(createDbBackedMemref(builder, codelet.getLoc(), memrefType,
                                  dynamicSizes, blockView, codelet, depIndex)))
    return failure();

  for (HostBridgeParticipant &participant : *participants)
    participant.codelet->setOperand(participant.depIndex, blockView);
  return success();
}

static inline bool
canMaterializeRawCodirDependencyWithPlan(Value root,
                                         codir::CodeletOp planSource) {
  if (!hasCodirTileOwnerSlicePlan(planSource))
    return false;
  std::optional<unsigned> depIndex =
      findCodirDependencyIndexForRoot(planSource, root);
  if (!depIndex)
    return false;
  if (!codirDepAllowsComputeBlockStorage(planSource, *depIndex))
    return false;
  return codirDepCanUseBlockStorageAccess(planSource, *depIndex);
}

static inline bool rawCodirDependencyNeedsHostBridge(Value root) {
  if (!root)
    return false;
  if (isa<BlockArgument>(root))
    return true;
  return hasHostMemrefAccessOutsideSchedulingUnit(root);
}

static inline LogicalResult
materializeRawCodirDependency(Value dep, codir::CodeletOp planSource,
                              unsigned depIndex) {
  if (findBackingDbAlloc(dep))
    return success();
  if (failed(requireFinalizedCodirDepOwnerDimsForMaterialization(planSource,
                                                                 depIndex)))
    return failure();

  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep);
  auto memrefType = dyn_cast<MemRefType>(root.getType());
  if (!memrefType)
    return failure();

  Operation *def = root.getDefiningOp();
  OpBuilder builder(root.getContext());
  Value replacement;
  bool usePlan = canMaterializeRawCodirDependencyWithPlan(root, planSource);
  bool needsHostBridge = rawCodirDependencyNeedsHostBridge(root);
  if (usePlan && needsHostBridge) {
    if (auto blockArg = dyn_cast<BlockArgument>(root)) {
      FailureOr<Value> hostView = materializeCoarseHostDbForBlockArgument(
          builder, root.getLoc(), blockArg);
      if (failed(hostView))
        return failure();
      FailureOr<Value> bridged = materializeHostWholeToComputeBlockBridge(
          planSource, depIndex, *hostView);
      return failed(bridged) ? failure() : success();
    }

    FailureOr<Value> bridged =
        materializeHostWholeToComputeBlockBridge(planSource, depIndex, root);
    if (failed(bridged))
      return failure();
    return success();
  }
  if (!def) {
    auto blockArg = dyn_cast<BlockArgument>(root);
    if (!blockArg)
      return failure();

    Block *owner = blockArg.getOwner();
    if (!owner)
      return failure();
    builder.setInsertionPointToStart(owner);

    SmallVector<Value> elementSizes;
    SmallVector<Value> dynamicSizes;
    if (memrefType.getRank() == 0) {
      elementSizes.push_back(createOneIndex(builder, root.getLoc()));
    } else {
      elementSizes.reserve(memrefType.getRank());
      for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim) {
        if (memrefType.isDynamicDim(dim)) {
          Value size = memref::DimOp::create(builder, root.getLoc(), root, dim);
          elementSizes.push_back(size);
          dynamicSizes.push_back(size);
        } else {
          elementSizes.push_back(createConstantIndex(
              builder, root.getLoc(), memrefType.getDimSize(dim)));
        }
      }
    }

    SmallVector<Operation *> dimOps;
    for (Value size : elementSizes)
      if (Operation *op = size.getDefiningOp())
        dimOps.push_back(op);

    arts::DbAllocOp createdDbAlloc;
    if (usePlan) {
      if (failed(createDbBackedMemref(builder, root.getLoc(), memrefType,
                                      dynamicSizes, replacement, planSource,
                                      depIndex)))
        return failure();
    } else {
      Value route = arts::createCurrentNodeRoute(builder, root.getLoc());
      auto dbAlloc = arts::DbAllocOp::create(
          builder, root.getLoc(), arts::ArtsMode::inout, route,
          arts::DbAllocType::unknown, arts::DbMode::write,
          memrefType.getElementType(), root,
          SmallVector<Value>{createOneIndex(builder, root.getLoc())},
          std::move(elementSizes), arts::PartitionMode::coarse);
      createdDbAlloc = dbAlloc;
      replacement =
          materializeInnerPayload(builder, root.getLoc(), dbAlloc.getPtr());
    }

    root.replaceUsesWithIf(replacement, [&](OpOperand &use) {
      Operation *owner = use.getOwner();
      if (createdDbAlloc && owner == createdDbAlloc.getOperation())
        return false;
      return !llvm::is_contained(dimOps, owner);
    });
    return success();
  }

  builder.setInsertionPointAfter(def);
  if (auto alloc = dyn_cast<memref::AllocOp>(def)) {
    if (failed(usePlan
                   ? createDbBackedMemref(builder, alloc.getLoc(), memrefType,
                                          alloc.getDynamicSizes(), replacement,
                                          planSource, depIndex)
                   : createDbBackedMemref(builder, alloc.getLoc(), memrefType,
                                          alloc.getDynamicSizes(),
                                          replacement)))
      return failure();
  } else if (auto alloca = dyn_cast<memref::AllocaOp>(def)) {
    if (failed(usePlan
                   ? createDbBackedMemref(builder, alloca.getLoc(), memrefType,
                                          alloca.getDynamicSizes(), replacement,
                                          planSource, depIndex)
                   : createDbBackedMemref(builder, alloca.getLoc(), memrefType,
                                          alloca.getDynamicSizes(),
                                          replacement)))
      return failure();
  } else {
    return failure();
  }

  SmallVector<memref::DeallocOp> deallocs;
  for (Operation *user : llvm::make_early_inc_range(root.getUsers())) {
    auto dealloc = dyn_cast<memref::DeallocOp>(user);
    if (dealloc && dealloc.getMemref() == root)
      deallocs.push_back(dealloc);
  }
  for (memref::DeallocOp dealloc : deallocs)
    dealloc.erase();

  root.replaceAllUsesWith(replacement);
  if (def->use_empty())
    def->erase();
  return success();
}

struct CodirDepSlice {
  bool sliced = false;
  bool subindex = false;
  Value subindexIndex;
  SmallVector<Value> offsets;
  SmallVector<Value> sizes;
  SmallVector<OpFoldResult> mixedOffsets;
  SmallVector<OpFoldResult> mixedSizes;
  SmallVector<OpFoldResult> mixedStrides;
};

static inline CodirDepSlice getCodirDepSlice(Value dep, OpBuilder &builder,
                                             Location loc) {
  CodirDepSlice slice;
  auto subview = dep.getDefiningOp<memref::SubViewOp>();
  if (subview) {
    slice.sliced = true;
    slice.mixedOffsets = subview.getMixedOffsets();
    slice.mixedSizes = subview.getMixedSizes();
    slice.mixedStrides = subview.getMixedStrides();
    slice.offsets =
        materializeIndexFoldResults(builder, loc, slice.mixedOffsets);
    slice.sizes = materializeIndexFoldResults(builder, loc, slice.mixedSizes);
    return slice;
  }

  auto subindex = dep.getDefiningOp<polygeist::SubIndexOp>();
  if (!subindex)
    return slice;

  auto sourceType = dyn_cast<MemRefType>(subindex.getSource().getType());
  auto resultType = dyn_cast<MemRefType>(subindex.getResult().getType());
  if (!sourceType || !resultType || sourceType.getRank() == 0 ||
      resultType.getRank() + 1 != sourceType.getRank())
    return slice;

  slice.sliced = true;
  slice.subindex = true;
  slice.subindexIndex = subindex.getIndex();
  slice.mixedOffsets.push_back(subindex.getIndex());
  slice.mixedSizes.push_back(builder.getIndexAttr(1));
  slice.mixedStrides.push_back(builder.getIndexAttr(1));

  ValueRange dynamicSizes = subindex.getSizes();
  unsigned dynamicSizeIdx = 0;
  for (int64_t dim = 0, rank = resultType.getRank(); dim < rank; ++dim) {
    slice.mixedOffsets.push_back(builder.getIndexAttr(0));
    if (resultType.isDynamicDim(dim)) {
      if (dynamicSizeIdx >= dynamicSizes.size()) {
        slice.sliced = false;
        slice.mixedOffsets.clear();
        slice.mixedSizes.clear();
        slice.mixedStrides.clear();
        return slice;
      }
      slice.mixedSizes.push_back(dynamicSizes[dynamicSizeIdx++]);
    } else {
      slice.mixedSizes.push_back(
          builder.getIndexAttr(resultType.getDimSize(dim)));
    }
    slice.mixedStrides.push_back(builder.getIndexAttr(1));
  }
  slice.offsets = materializeIndexFoldResults(builder, loc, slice.mixedOffsets);
  slice.sizes = materializeIndexFoldResults(builder, loc, slice.mixedSizes);
  return slice;
}

static inline LogicalResult lowerSdeResourceQuery(sde::SdeResourceQueryOp op) {
  OpBuilder builder(op);
  switch (op.getKind()) {
  case sde::SdeResourceQueryKind::logicalWorkers: {
    auto runtimeQuery = arts::RuntimeQueryOp::create(
        builder, op.getLoc(), arts::RuntimeQueryKind::totalWorkers);
    Value asIndex = arith::IndexCastOp::create(
        builder, op.getLoc(), builder.getIndexType(), runtimeQuery.getResult());
    op.getResult().replaceAllUsesWith(asIndex);
    op.erase();
    return success();
  }
  }
  return op.emitOpError() << "unsupported SDE resource query kind";
}

static inline LogicalResult lowerSdeControlBarrier(sde::SdeSuBarrierOp op) {
  OpBuilder builder(op);
  if (!op.getBarrierEliminatedAttr()) {
    auto reasonAttr = op.getBarrierReasonAttr();
    arts::BarrierOp::create(
        builder, op.getLoc(),
        reasonAttr ? arts::ArtsBarrierReasonAttr::get(
                         op.getContext(), static_cast<arts::ArtsBarrierReason>(
                                              reasonAttr.getValue()))
                   : arts::ArtsBarrierReasonAttr{});
  }
  op.erase();
  return success();
}

static inline LogicalResult
eraseConsumedSdeControlToken(sde::SdeControlTokenOp op) {
  if (!op.getToken().use_empty())
    return op.emitOpError()
           << "survived CODIR-to-ARTS materialization with live users; "
              "control tokens must be consumed by SDE barriers before the "
              "ARTS boundary";
  op.erase();
  return success();
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_ARTSMATERIALIZATIONUTILS_H
