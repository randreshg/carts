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
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/dialect/codir/Utils/CodirConversionUtils.h"
#include "carts/utils/OperationAttributes.h"
#include "carts/utils/Utils.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/STLExtras.h"
#include <algorithm>

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
  case codir::CodirPattern::jacobi_alternating_buffers:
    return arts::ArtsDepPattern::jacobi_alternating_buffers;
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

static inline arts::ArtsPlanAsyncStrategy
convertAsyncStrategy(codir::CodirAsyncStrategy strategy) {
  switch (strategy) {
  case codir::CodirAsyncStrategy::blocking:
    return arts::ArtsPlanAsyncStrategy::blocking;
  case codir::CodirAsyncStrategy::advance_stage:
  case codir::CodirAsyncStrategy::cps_chain:
    return arts::ArtsPlanAsyncStrategy::advance_edt;
  }
  return arts::ArtsPlanAsyncStrategy::blocking;
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
  case codir::CodirPattern::jacobi_alternating_buffers:
    return arts::EdtDistributionPattern::stencil;
  case codir::CodirPattern::matmul:
    return arts::EdtDistributionPattern::matmul;
  case codir::CodirPattern::reduction:
    return arts::EdtDistributionPattern::uniform;
  }
  return arts::EdtDistributionPattern::unknown;
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
      arts::setPatternRevision(taskOp, 1);
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
  if (auto async = codelet.getAsyncStrategyAttr()) {
    arts::setPlanAsyncStrategyAttr(
        taskOp, arts::ArtsPlanAsyncStrategyAttr::get(
                    ctx, convertAsyncStrategy(async.getValue())));
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
  if (auto ownerDims = codelet.getPlanOwnerDimsAttr())
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
        pattern.getValue() == codir::CodirPattern::jacobi_alternating_buffers) {
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
  Value zero = createZeroIndex(builder, loc);
  return arts::DbRefOp::create(builder, loc, sourcePtr,
                               SmallVector<Value>{zero});
}

static inline bool hasCodirTileOwnerSlicePlan(codir::CodeletOp op) {
  return op && op.getTileShapeAttr() && op.getTileOwnerDimsAttr();
}

static inline std::optional<unsigned>
getSingleCodirTileOwnerDim(codir::CodeletOp codelet) {
  if (!hasCodirTileOwnerSlicePlan(codelet))
    return std::nullopt;
  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(codelet.getTileOwnerDimsAttr());
  if (!ownerDims || ownerDims->size() != 1 || ownerDims->front() < 0)
    return std::nullopt;
  return static_cast<unsigned>(ownerDims->front());
}

static inline Value getCodirOwnerBaseArgument(codir::CodeletOp codelet) {
  if (!codelet || codelet.getBody().empty() || codelet.getParams().empty())
    return {};

  Block &body = codelet.getBody().front();
  unsigned depCount = codelet.getDeps().size();
  unsigned paramCount = codelet.getParams().size();
  if (body.getNumArguments() < depCount + paramCount)
    return {};
  return body.getArgument(depCount + paramCount - 1);
}

static inline std::optional<unsigned>
inferCodirDepOwnerAccessDim(codir::CodeletOp codelet, unsigned depIndex) {
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

  Value ownerBase = getCodirOwnerBaseArgument(codelet);
  if (!ownerBase)
    return std::nullopt;

  bool sawDirectRootAccess = false;
  bool rejected = false;
  std::optional<unsigned> selectedDim;
  body.walk([&](Operation *op) {
    if (rejected)
      return WalkResult::interrupt();

    auto access = getCodirMemoryAccessInfo(op);
    if (!access || access->memref != depArg)
      return WalkResult::advance();

    sawDirectRootAccess = true;
    std::optional<unsigned> accessDim;
    for (auto [dim, index] : llvm::enumerate(access->indices)) {
      if (!indexSelectsOwnerSlice(index, ownerBase))
        continue;
      if (accessDim && *accessDim != dim) {
        rejected = true;
        return WalkResult::interrupt();
      }
      accessDim = static_cast<unsigned>(dim);
    }
    if (!accessDim || *accessDim >= depType.getRank()) {
      rejected = true;
      return WalkResult::interrupt();
    }
    if (selectedDim && *selectedDim != *accessDim) {
      rejected = true;
      return WalkResult::interrupt();
    }
    selectedDim = *accessDim;
    return WalkResult::advance();
  });

  if (!sawDirectRootAccess || rejected)
    return std::nullopt;
  return selectedDim;
}

static inline std::optional<unsigned>
getPlannedCodirDepOwnerDim(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr depOwnerDims =
      codelet ? codelet.getDepOwnerDimsAttr() : ArrayAttr{};
  if (!depOwnerDims || depIndex >= depOwnerDims.size())
    return std::nullopt;

  auto dims = dyn_cast<ArrayAttr>(depOwnerDims[depIndex]);
  if (!dims)
    return std::nullopt;
  std::optional<SmallVector<int64_t, 4>> values = readI64ArrayAttr(dims);
  if (!values || values->size() != 1 || values->front() < 0)
    return std::nullopt;
  return static_cast<unsigned>(values->front());
}

static inline std::optional<unsigned>
getCodirDepOwnerDim(codir::CodeletOp codelet, unsigned depIndex) {
  if (std::optional<unsigned> planned =
          getPlannedCodirDepOwnerDim(codelet, depIndex))
    return planned;
  if (std::optional<unsigned> inferred =
          inferCodirDepOwnerAccessDim(codelet, depIndex))
    return inferred;
  return getSingleCodirTileOwnerDim(codelet);
}

static inline ArrayAttr getCodirDepOwnerDimsAttr(codir::CodeletOp codelet,
                                                 unsigned depIndex) {
  std::optional<unsigned> ownerDim = getCodirDepOwnerDim(codelet, depIndex);
  if (!ownerDim)
    return ArrayAttr{};
  return buildI64ArrayAttr(codelet.getContext(),
                           SmallVector<int64_t, 1>{*ownerDim});
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

static inline std::optional<int64_t>
getSingleCodirTileOwnerBlockSize(codir::CodeletOp codelet, unsigned depIndex,
                                 unsigned memrefRank) {
  std::optional<unsigned> ownerDim = getCodirDepOwnerDim(codelet, depIndex);
  if (!ownerDim)
    return std::nullopt;

  std::optional<SmallVector<int64_t, 4>> tileShape =
      readI64ArrayAttr(codelet.getTileShapeAttr());
  if (!tileShape || tileShape->empty())
    return std::nullopt;

  std::optional<int64_t> blockSize;
  if (tileShape->size() == memrefRank) {
    if (*ownerDim >= tileShape->size())
      return std::nullopt;
    blockSize = (*tileShape)[*ownerDim];
  } else {
    // Compact owner-slot-shaped tile metadata stores the single owner block
    // size at slot zero.
    blockSize = tileShape->front();
  }

  if (!blockSize || *blockSize <= 0)
    return std::nullopt;
  return blockSize;
}

static inline scf::ForOp findCodirOwnerDispatchLoop(codir::CodeletOp codelet) {
  for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (loop && containsValue(codelet.getParams(), loop.getInductionVar()))
      return loop;
  }
  return {};
}

static inline Value getCodirOwnerDomainLower(codir::CodeletOp codelet) {
  if (auto loop = findCodirOwnerDispatchLoop(codelet))
    return loop.getLowerBound();
  return {};
}

static inline Value getCodirOwnerDomainUpper(codir::CodeletOp codelet) {
  if (auto loop = findCodirOwnerDispatchLoop(codelet))
    return loop.getUpperBound();
  return {};
}

struct CodirOwnerHaloWindow {
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

static inline CodirOwnerHaloWindow
getCodirOwnerHaloWindow(codir::CodeletOp codelet, unsigned depIndex,
                        unsigned memrefRank) {
  std::optional<codir::CodirAccessMode> mode =
      getCodirDepAccessMode(codelet, depIndex);
  if (!mode || !codirAccessMayRead(*mode) || codirAccessMayWrite(*mode))
    return {};

  std::optional<unsigned> ownerDim = getCodirDepOwnerDim(codelet, depIndex);
  if (!ownerDim || *ownerDim >= memrefRank)
    return {};

  std::optional<unsigned> ownerSlot = getCodirOwnerDimSlot(codelet, *ownerDim);
  CodirOwnerHaloWindow window;

  if (auto minOffset = getCodirOwnerDimValue(codelet.getAccessMinOffsetsAttr(),
                                             *ownerDim, ownerSlot, memrefRank))
    window.lower = std::max<int64_t>(0, -*minOffset);
  if (auto maxOffset = getCodirOwnerDimValue(codelet.getAccessMaxOffsetsAttr(),
                                             *ownerDim, ownerSlot, memrefRank))
    window.upper = std::max<int64_t>(0, *maxOffset);

  if (!window.empty())
    return window;

  if (auto halo = getCodirOwnerDimValue(codelet.getHaloShapeAttr(), *ownerDim,
                                        ownerSlot, memrefRank)) {
    int64_t radius = std::max<int64_t>(0, *halo);
    window.lower = radius;
    window.upper = radius;
  }

  return window;
}

// Defined below (after findBackingDbAlloc); forward-declared so
// createDbBackedMemref can use the unioned per-buffer halo window.
static inline CodirOwnerHaloWindow
codirBackingBufferHaloWindow(Value rootMemref, unsigned memrefRank);

// Predicates defined later in this header; forward-declared so the union helper
// can replicate the exact block-vs-coarse materialization decision per read
// dep.
static inline bool
codirDepRequiresPhaseRedistributionBridge(codir::CodeletOp codelet,
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
  std::optional<unsigned> ownerDim = getCodirDepOwnerDim(codelet, depIndex);
  if (!ownerDim || !codelet || codelet.getBody().empty())
    return false;

  Block &body = codelet.getBody().front();
  if (depIndex >= codelet.getDeps().size() ||
      depIndex >= body.getNumArguments())
    return false;

  Value depArg = body.getArgument(depIndex);
  auto depType = dyn_cast<MemRefType>(depArg.getType());
  if (!depType || depType.getRank() == 0 || *ownerDim >= depType.getRank())
    return false;

  Value ownerBase = getCodirOwnerBaseArgument(codelet);
  if (!ownerBase)
    return false;

  bool sawDirectRootAccess = false;
  bool rejected = false;
  body.walk([&](Operation *op) {
    if (rejected)
      return WalkResult::interrupt();

    auto access = getCodirMemoryAccessInfo(op);
    if (!access)
      return WalkResult::advance();
    if (access->memref != depArg)
      return WalkResult::advance();

    sawDirectRootAccess = true;
    if (access->indices.size() <= *ownerDim ||
        !indexSelectsOwnerSlice(access->indices[*ownerDim], ownerBase)) {
      rejected = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });

  return sawDirectRootAccess && !rejected;
}

struct PlannedBlockLocalAccessRewrite {
  Value localMemref;
  unsigned ownerDim = 0;
  Value ownerBase;
  int64_t lowerHalo = 0;
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

static inline LogicalResult rewritePlannedBlockLocalAccesses(
    arts::EdtOp task, ArrayRef<PlannedBlockLocalAccessRewrite> rewrites) {
  if (rewrites.empty())
    return success();

  auto rewriteIndices = [&](Operation *op, Value memref,
                            MutableOperandRange indices) -> WalkResult {
    for (const PlannedBlockLocalAccessRewrite &rewrite : rewrites) {
      if (memref != rewrite.localMemref)
        continue;
      if (indices.size() <= rewrite.ownerDim) {
        op->emitError("planned block-local access is missing the owner "
                      "dimension index");
        return WalkResult::interrupt();
      }

      OpBuilder builder(op);
      FailureOr<Value> localIndex = materializeBlockLocalIndex(
          builder, op->getLoc(), indices[rewrite.ownerDim].get(),
          rewrite.ownerBase, rewrite.lowerHalo);
      if (failed(localIndex)) {
        op->emitError("planned block-local access does not stay within the "
                      "owner slice");
        return WalkResult::interrupt();
      }
      indices[rewrite.ownerDim].set(*localIndex);
      return WalkResult::advance();
    }
    return WalkResult::advance();
  };

  Block &body = task.getBody().front();
  WalkResult result = body.walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return rewriteIndices(op, load.getMemref(), load.getIndicesMutable());
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return rewriteIndices(op, store.getMemref(), store.getIndicesMutable());
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

  CodirOwnerHaloWindow ownerHalo;
  if (depIndex) {
    // Pad for the union of every codelet that reads this backing buffer as a
    // stencil halo input, not just the dependency that first triggered
    // materialization. A double-buffered stencil array is read by one half-step
    // and written by the other; if the write half-step materializes it first
    // the single-dep window is empty and the buffer is left unpadded, breaking
    // the symmetric block-halo read in the read half-step. The union folds in
    // the read half-step's window. For single-pass stencils (write-only output
    // re-read only by a metadata-free storageBridgeCopy) the union equals the
    // single-dep window, so their layout is unchanged.
    Value backingRoot = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
        planSource.getDeps()[*depIndex]);
    ownerHalo = codirBackingBufferHaloWindow(
        backingRoot, static_cast<unsigned>(memrefType.getRank()));
    std::optional<unsigned> ownerDim =
        getCodirDepOwnerDim(planSource, *depIndex);
    if (!ownerHalo.empty() && ownerDim &&
        *ownerDim < physicalPlan->innerSizes.size()) {
      Value haloWidth = createConstantIndex(builder, loc, ownerHalo.width());
      physicalPlan->innerSizes[*ownerDim] = arith::AddIOp::create(
          builder, loc, physicalPlan->innerSizes[*ownerDim], haloWidth);
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
  if (!ownerHalo.empty())
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
static inline bool codirAccessMayRead(codir::CodirAccessMode mode);

// The first-class collective gate predicates
// (coarseBridgeTargetHasReplicatedReadConsumer / codeletIsCrossOwnerTranspose
// Reduce / coarseBridgeTargetHasCrossOwnerReduceConsumer) and the name-free
// chooseCollective selector now live in the shared codir Utils
// (CodeletABIUtils.h). StoragePlanning stamps the result onto the per-dep
// `dep_collectives` carrier; this file READS that carrier via
// getCodirDepCollectiveKind (ADR-0003 §7b). Keeping the bodies in one place
// guarantees the refactor is byte-identical with the historical gates.

// Compute the halo padding a single backing buffer needs by taking the union of
// the owner halo windows of every codelet dependency that reads the SAME
// backing buffer as a stencil halo input. A double-buffered stencil array (e.g.
// jacobi2d's A/B) is read by one half-step and written by the other, but each
// backing memref lowers to a single block db_alloc. Deciding halo padding from
// only the dependency that first triggered materialization underpads a buffer
// whose first appearance is a write (getCodirOwnerHaloWindow returns an empty
// window for may-write deps), so the symmetric stencil EDT body then reads it
// with the block-halo column shift against an unpadded block. Unioning over all
// read deps backed by `rootMemref` makes the padding (and the
// stencil_supported_block_halo attribute) match the access formula regardless
// of which half-step materialized the buffer first.
//
// Write-only outputs (e.g. conv-2d/conv-3d's result buffer, only re-read by a
// storageBridgeCopy codelet that carries no owner/halo metadata) contribute an
// empty window, so this union reduces to the single-dep window for single-pass
// stencils and leaves them unchanged.
static inline CodirOwnerHaloWindow
codirBackingBufferHaloWindow(Value rootMemref, unsigned memrefRank) {
  CodirOwnerHaloWindow unionWindow;
  if (!rootMemref)
    return unionWindow;

  Operation *defining = rootMemref.getDefiningOp();
  Operation *scope =
      defining ? defining : rootMemref.getParentBlock()->getParentOp();
  ModuleOp module = scope ? scope->getParentOfType<ModuleOp>() : ModuleOp{};
  if (!module)
    return unionWindow;

  std::optional<unsigned> unionOwnerDim;
  module.walk([&](codir::CodeletOp codelet) {
    for (auto [idx, dep] : llvm::enumerate(codelet.getDeps())) {
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) != rootMemref)
        continue;
      unsigned depIdx = static_cast<unsigned>(idx);
      std::optional<codir::CodirAccessMode> mode =
          getCodirDepAccessMode(codelet, depIdx);
      if (!mode || !codirAccessMayRead(*mode))
        continue;
      CodirOwnerHaloWindow window =
          getCodirOwnerHaloWindow(codelet, depIdx, memrefRank);
      if (window.empty())
        continue;
      // Only a read that is itself materialized as a block owner-slice view
      // applies the block-halo column shift in its EDT body; such a read needs
      // the buffer padded. Reads that fall back to a coarse host-whole view
      // (e.g. tiles below the distribution threshold, as in jacobi2d small)
      // index the buffer globally with no halo shift, so they must not pad it.
      // This mirrors the usePlan decision in materializeRawCodirDependency so
      // the padding and the body access formula stay in agreement.
      if (!canMaterializeRawCodirDependencyWithPlan(rootMemref, codelet))
        continue;
      if (rawCodirDependencyNeedsHostBridge(rootMemref) &&
          !codirDepRequiresPhaseRedistributionBridge(codelet, depIdx))
        continue;
      std::optional<unsigned> ownerDim = getCodirDepOwnerDim(codelet, depIdx);
      // A non-empty window always carries an owner dim; require all unioned
      // read deps to agree on it so the padded dimension is unambiguous.
      if (!ownerDim)
        continue;
      if (unionOwnerDim && *unionOwnerDim != *ownerDim)
        continue;
      unionOwnerDim = ownerDim;
      unionWindow.lower = std::max(unionWindow.lower, window.lower);
      unionWindow.upper = std::max(unionWindow.upper, window.upper);
    }
  });

  return unionWindow;
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

/// First-class collective family for |codelet|'s |depIndex|, read from the
/// `dep_collectives` carrier StoragePlanning stamped via `chooseCollective`
/// (ADR-0003 §7b). Defaults to `none` when the carrier is absent (e.g. IR not
/// produced through StoragePlanning), which keeps lowering unchanged.
static inline codir::CodirCollectiveKind
getCodirDepCollectiveKind(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr collectives = codelet ? codelet.getDepCollectivesAttr() : ArrayAttr{};
  if (!collectives || depIndex >= collectives.size())
    return codir::CodirCollectiveKind::none;
  auto kind = dyn_cast<codir::CodirCollectiveKindAttr>(collectives[depIndex]);
  if (!kind)
    return codir::CodirCollectiveKind::none;
  return kind.getValue();
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

static inline bool codirDepAllowsComputeBlockStorage(codir::CodeletOp codelet,
                                                     unsigned depIndex) {
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  if (!view)
    return true;
  return codirStorageViewUsesComputeBlock(*view);
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
  for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (loop && containsValue(codelet.getParams(), loop.getInductionVar()))
      anchor = parent;
  }
  return anchor;
}

struct HostBridgeParticipant {
  codir::CodeletOp codelet;
  unsigned depIndex = 0;
  codir::CodirAccessMode mode = codir::CodirAccessMode::readwrite;
};

struct HostBridgeUseCollection {
  SmallVector<HostBridgeParticipant> participants;
  SmallVector<Operation *> readObservationAnchors;
};

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
  if (!codirDepRequiresPhaseRedistributionBridge(codelet, depIndex))
    return false;
  if (!hasCodirTileOwnerSlicePlan(codelet) ||
      !codirDepAccessesStayWithinSingleOwnerSlice(codelet, depIndex))
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
      !codirDepAccessesStayWithinSingleOwnerSlice(codelet, depIndex))
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
    ArrayRef<Value> copySizes, Value ownerOffset, unsigned ownerDim,
    bool copyIntoBlock, SmallVectorImpl<Value> &indices) {
  unsigned dim = indices.size();
  if (dim == copySizes.size()) {
    SmallVector<Value> hostIndices;
    SmallVector<Value> blockIndices;
    hostIndices.reserve(indices.size());
    blockIndices.reserve(indices.size());
    for (auto [idx, value] : llvm::enumerate(indices)) {
      blockIndices.push_back(value);
      if (idx == ownerDim) {
        hostIndices.push_back(
            arith::AddIOp::create(builder, loc, ownerOffset, value));
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
                                      copySizes, ownerOffset, ownerDim,
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

static inline arts::DbAcquireOp
materializeBridgeAcquire(OpBuilder &builder, Location loc,
                         arts::DbAllocOp alloc, arts::ArtsMode mode,
                         arts::PartitionMode partitionMode, Value offset,
                         Value size) {
  return arts::DbAcquireOp::create(builder, loc, mode, alloc.getGuid(),
                                   alloc.getPtr(), partitionMode,
                                   /*indices=*/SmallVector<Value>{},
                                   /*offsets=*/SmallVector<Value>{offset},
                                   /*sizes=*/SmallVector<Value>{size},
                                   /*partitionIndices=*/SmallVector<Value>{},
                                   /*partitionOffsets=*/SmallVector<Value>{},
                                   /*partitionSizes=*/SmallVector<Value>{},
                                   /*boundsValid=*/Value{},
                                   /*elementOffsets=*/SmallVector<Value>{},
                                   /*elementSizes=*/SmallVector<Value>{});
}

static inline LogicalResult
materializeHostBlockCopyLoop(OpBuilder &builder, Location loc, Value hostView,
                             arts::DbAllocOp blockAlloc,
                             codir::CodeletOp codelet, unsigned depIndex,
                             bool copyIntoBlock, bool crossNodeGather = false) {
  auto hostType = dyn_cast<MemRefType>(hostView.getType());
  if (!hostType || hostType.getRank() == 0)
    return failure();
  arts::DbAllocOp hostAlloc = findBackingDbAlloc(hostView);
  if (!hostAlloc)
    return failure();

  std::optional<unsigned> ownerDim = getCodirDepOwnerDim(codelet, depIndex);
  if (!ownerDim || *ownerDim >= static_cast<unsigned>(hostType.getRank()))
    return failure();
  if (blockAlloc.getSizes().size() != 1 ||
      blockAlloc.getElementSizes().size() !=
          static_cast<size_t>(hostType.getRank()))
    return failure();

  SmallVector<Value> logicalSizes =
      getBridgeLogicalElementSizes(builder, loc, hostView);
  if (logicalSizes.size() != static_cast<size_t>(hostType.getRank()))
    return failure();

  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockCount = blockAlloc.getSizes().front();
  std::optional<int64_t> plannedBlockSize = getSingleCodirTileOwnerBlockSize(
      codelet, depIndex, static_cast<unsigned>(hostType.getRank()));
  Value ownerBlockSize =
      plannedBlockSize ? createConstantIndex(builder, loc, *plannedBlockSize)
                       : blockAlloc.getElementSizes()[*ownerDim];
  CodirOwnerHaloWindow ownerHalo =
      copyIntoBlock
          ? getCodirOwnerHaloWindow(codelet, depIndex,
                                    static_cast<unsigned>(hostType.getRank()))
          : CodirOwnerHaloWindow{};

  // Cross-node gather (copy-out only): wrap the per-block copy in an outer
  // per-node loop so every node assembles the COMPLETE coarse buffer from all
  // producer blocks, pulling the blocks it did not produce through the existing
  // cross-node read-only block acquire. This is the gather half of the
  // transpose-matvec reduce (atax/bicg step2): once the coarse intermediate is
  // complete on every node, the cross-owner reduction reads correct values.
  // The default intranode copy-out (crossNodeGather == false) is byte-for-byte
  // unchanged, so every other kernel's lowering is preserved.
  bool gatherAcrossNodes = crossNodeGather && !copyIntoBlock;
  OpBuilder::InsertionGuard guard(builder);
  Value gatherNodeOrdinal;
  scf::ForOp nodeLoop;
  if (gatherAcrossNodes) {
    auto totalNodesI32 = arts::RuntimeQueryOp::create(
        builder, loc, arts::RuntimeQueryKind::totalNodes);
    Value totalNodes = arith::IndexCastOp::create(
        builder, loc, builder.getIndexType(), totalNodesI32.getResult());
    nodeLoop = scf::ForOp::create(builder, loc, zero, totalNodes, one);
    builder.setInsertionPointToStart(nodeLoop.getBody());
    gatherNodeOrdinal = nodeLoop.getInductionVar();
  }

  auto loop = scf::ForOp::create(builder, loc, zero, blockCount, one);
  builder.setInsertionPointToStart(loop.getBody());
  Value blockIndex = loop.getInductionVar();
  Value ownerDomainBase =
      materializeCodirOwnerDomainBase(builder, loc, codelet);
  Value ownerBlockOffset =
      arith::MulIOp::create(builder, loc, blockIndex, ownerBlockSize);
  Value ownerOffset =
      arith::AddIOp::create(builder, loc, ownerDomainBase, ownerBlockOffset);
  Value ownerCopyStart =
      subtractClampZero(builder, loc, ownerOffset, ownerHalo.lower);
  Value requestedEnd =
      arith::AddIOp::create(builder, loc, ownerOffset, ownerBlockSize);
  if (ownerHalo.upper > 0)
    requestedEnd = arith::AddIOp::create(
        builder, loc, requestedEnd,
        createConstantIndex(builder, loc, ownerHalo.upper));
  Value ownerCopyEnd = arith::MinUIOp::create(builder, loc, requestedEnd,
                                              logicalSizes[*ownerDim]);
  Value ownerCopySize = materializePositiveDifferenceOrZero(
      builder, loc, ownerCopyEnd, ownerCopyStart);

  SmallVector<Value> copySizes;
  copySizes.reserve(hostType.getRank());
  for (int64_t dim = 0, rank = hostType.getRank(); dim < rank; ++dim) {
    if (static_cast<unsigned>(dim) == *ownerDim) {
      copySizes.push_back(ownerCopySize);
    } else {
      copySizes.push_back(logicalSizes[dim]);
    }
  }

  arts::ArtsMode hostMode =
      copyIntoBlock ? arts::ArtsMode::in : arts::ArtsMode::inout;
  arts::ArtsMode blockMode =
      copyIntoBlock ? arts::ArtsMode::out : arts::ArtsMode::in;
  auto hostAcquire =
      materializeBridgeAcquire(builder, loc, hostAlloc, hostMode,
                               arts::PartitionMode::coarse, zero, one);
  auto blockAcquire =
      materializeBridgeAcquire(builder, loc, blockAlloc, blockMode,
                               arts::PartitionMode::block, blockIndex, one);

  SmallVector<Value> deps{hostAcquire.getPtr(), blockAcquire.getPtr()};
  SmallVector<Value> params;
  params.reserve(copySizes.size() + 1);
  params.push_back(ownerCopyStart);
  params.append(copySizes.begin(), copySizes.end());

  arts::ArtsLaunchPolicy launch;
  if (copyIntoBlock)
    launch = arts::resolveArtsOrdinalLaunchPolicy(
        blockAlloc->getParentOfType<ModuleOp>(), blockIndex, builder, loc);
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
    Value bodyZero = createZeroIndex(builder, loc);
    Value hostPayload = arts::DbRefOp::create(builder, loc, body.getArgument(0),
                                              SmallVector<Value>{bodyZero});
    Value blockPayload = arts::DbRefOp::create(
        builder, loc, body.getArgument(1), SmallVector<Value>{bodyZero});
    Value bodyOwnerOffset = body.getArgument(2);
    SmallVector<Value> bodyCopySizes;
    bodyCopySizes.reserve(copySizes.size());
    for (size_t i = 0; i < copySizes.size(); ++i)
      bodyCopySizes.push_back(body.getArgument(3 + i));
    SmallVector<Value> indices;
    materializeHostBlockElementCopyNest(builder, loc, hostPayload, blockPayload,
                                        bodyCopySizes, bodyOwnerOffset,
                                        *ownerDim, copyIntoBlock, indices);
    arts::YieldOp::create(builder, loc);
  }

  if (!copyIntoBlock) {
    builder.setInsertionPointAfter(gatherAcrossNodes ? nodeLoop.getOperation()
                                                     : loop.getOperation());
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

/// Build the per-element summing nest that settles one output block by reducing
/// the P per-tile partial payloads with `+=` (arith.addf) and writing the result
/// ONCE. This is the addf dual of materializePerBlockCopyNest: instead of a
/// single source copy, the leaf loads tile 0, accumulates tiles 1..P-1 with
/// arith.addf, and stores once into the settled block. All payloads are block
/// payloads indexed identically (no coarse host offset).
static inline void
materializePerBlockSumNest(OpBuilder &builder, Location loc,
                           ArrayRef<Value> partialPayloads, Value dstPayload,
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

/// WF-2 keystone: the per-block single-writer all-gather substrate.
///
/// The coarse write-back (materializeHostBlockCopyLoop with allGather) made
/// each node assemble a SINGLE coarse <inout> replica DB; the disjoint
/// per-block strip writes then contend on that one DB's exclusive-write (EW)
/// frontier and serialize (ADR-0001 phase 2; un-ordering it instead races).
/// That coarse replica is the thing ADR-0003 declares cannot scale.
///
/// This emission realizes the architecture's central insight (§2c): every
/// gathered output block is its OWN distinct-GUID DB written ONCE by exactly
/// one EDT. The gathered replica is a `block`-mode DB (N per-block DBs, each
/// reserved with its own GUID by createMultiDbs) created REPLICATED on every
/// node (no distributed ownership: each node holds all blocks locally, exactly
/// like an MPI rank's full recv buffer). Each copy EDT acquires:
///   - the producer block  RO  (<in>, PREFER_DUPLICATE via the read path) —
///     local fast, remote through the existing cross-node RO db acquire,
///   - its OWN gathered block  output-only (<out>) — block partition by the
///     block index, so it is one distinct DB, one writer.
/// Because each gathered block is a distinct DB, the EW frontier degenerates to
/// a single uncontended writer: race-free by construction, and the N block
/// writes run concurrently (no shared frontier). This is the phase-2
/// serialization removed at the root, not relaxed.
///
/// Returns the gathered replicated block DB's inner payload (a memref view) so
/// the caller can decide whether a consumer can read it block-native. The
/// existing coarse consumer (3mm's G, which reads the whole F on the
/// contraction dim from one EDT) cannot read N per-block DBs without
/// contraction tiling of its k-loop (WF-3); that boundary is reported, not
/// papered over by re-coarsening.
static inline FailureOr<Value>
emitPerBlockAllGatherWriteBack(OpBuilder &builder, Location loc, Value hostView,
                               arts::DbAllocOp producerBlockAlloc,
                               codir::CodeletOp codelet, unsigned depIndex) {
  auto hostType = dyn_cast<MemRefType>(hostView.getType());
  if (!hostType || hostType.getRank() == 0)
    return failure();
  ModuleOp module = producerBlockAlloc->getParentOfType<ModuleOp>();
  if (!module || !arts::hasArtsInterNodeRuntime(module))
    return failure();
  if (producerBlockAlloc.getSizes().size() != 1 ||
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

  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockCount = producerBlockAlloc.getSizes().front();

  SmallVector<Value> blockElementSizes(
      producerBlockAlloc.getElementSizes().begin(),
      producerBlockAlloc.getElementSizes().end());

  // Outer per-node loop: every node assembles its OWN full set of gathered
  // blocks. Routing each block copy to the node ordinal keeps the gathered
  // write owner-local on each node's replica while the RO producer-block
  // acquire pulls remote blocks through the existing cross-node acquire.
  auto totalNodesI32 = arts::RuntimeQueryOp::create(
      builder, loc, arts::RuntimeQueryKind::totalNodes);
  Value totalNodes = arith::IndexCastOp::create(
      builder, loc, builder.getIndexType(), totalNodesI32.getResult());
  auto nodeLoop = scf::ForOp::create(builder, loc, zero, totalNodes, one);
  builder.setInsertionPointToStart(nodeLoop.getBody());
  Value nodeOrdinal = nodeLoop.getInductionVar();

  auto blockLoop = scf::ForOp::create(builder, loc, zero, blockCount, one);
  builder.setInsertionPointToStart(blockLoop.getBody());
  Value blockIndex = blockLoop.getInductionVar();

  // Source: producer block, read-only (cross-node RO acquire; PREFER_DUPLICATE
  // is applied downstream because the producer DB is distributed/read).
  auto srcAcquire = materializeBridgeAcquire(
      builder, loc, producerBlockAlloc, arts::ArtsMode::in,
      arts::PartitionMode::block, blockIndex, one);
  // Destination: this gathered block, output-only. Distinct DB per block ⇒
  // single writer ⇒ no shared EW frontier.
  auto dstAcquire =
      materializeBridgeAcquire(builder, loc, replicaAlloc, arts::ArtsMode::out,
                               arts::PartitionMode::block, blockIndex, one);

  SmallVector<Value> deps{srcAcquire.getPtr(), dstAcquire.getPtr()};
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
    Value bodyZero = createZeroIndex(builder, loc);
    Value srcPayload = arts::DbRefOp::create(builder, loc, body.getArgument(0),
                                             SmallVector<Value>{bodyZero});
    Value dstPayload = arts::DbRefOp::create(builder, loc, body.getArgument(1),
                                             SmallVector<Value>{bodyZero});
    SmallVector<Value> bodyCopySizes;
    bodyCopySizes.reserve(blockElementSizes.size());
    for (size_t i = 0; i < blockElementSizes.size(); ++i)
      bodyCopySizes.push_back(body.getArgument(2 + i));
    SmallVector<Value> indices;
    materializePerBlockCopyNest(builder, loc, srcPayload, dstPayload,
                                bodyCopySizes, indices);
    arts::YieldOp::create(builder, loc);
  }

  builder.setInsertionPointAfter(nodeLoop);
  auto reason = arts::ArtsBarrierReasonAttr::get(
      builder.getContext(), arts::ArtsBarrierReason::required_memory);
  arts::BarrierOp::create(builder, loc, reason);

  return materializeInnerPayload(builder, loc, replicaAlloc.getPtr());
}

/// WF-3 keystone: the per-block single-writer summing settle — the `arith.addf`
/// dual of emitPerBlockAllGatherWriteBack.
///
/// A cross-owner reduction (atax/bicg's y = Aᵀ(Ax), 3mm's chained contraction,
/// any reduce-scatter / allreduce) produces, per output block, P partial
/// results — one per contraction tile / per node strip. The legacy coarse path
/// funnels those partials through a single shared <inout> replica DB whose
/// exclusive-write (EW) frontier serializes the disjoint partial writes (and
/// un-ordered, races them): the same ADR-0001 phase-2 failure the all-gather
/// removed at the root.
///
/// This emission realizes the architecture's central insight (§2c) for the
/// reduction direction: every settled output block is its OWN distinct-GUID DB
/// written ONCE by exactly one EDT. The P per-(block,tile) partials are
/// themselves per-block single-writer DBs (each its own GUID via createMultiDbs,
/// flat outer index `block * tileCount + tile`), and the settle DB is a `block`
/// mode DB created REPLICATED on every node (perBlockReplicated: each node holds
/// all settled blocks locally, exactly like an MPI rank's full recv buffer after
/// an allreduce). For each settled block, exactly one EDT:
///   - RO-acquires (with PREFER_DUPLICATE via the read path) the P partial
///     blocks it must sum — local fast, remote through the existing cross-node RO
///     db acquire. The acquires are emitted OUTSIDE the EDT and delivered as
///     block-args (the EdtLowering ABI forbids GEPing an outer DB alloc from the
///     EDT body — every DB an EDT touches must arrive as a dep), exactly as the
///     all-gather's producer side does.
///   - writes its OWN settled block output-only (<out>) ONCE, accumulating the P
///     partials with `+=` (arith.addf). NO RW accumulator, NO coarse DB.
/// Because each settled block is a distinct DB, the EW frontier degenerates to a
/// single uncontended writer: race-free by construction, and the N block settles
/// run concurrently (no shared frontier). This is the phase-2 serialization
/// removed at the root, not relaxed — the reduce-scatter/allreduce analogue of
/// MPI's disjoint per-rank send/recv buffers.
///
/// `partialBlockAlloc` is the per-(block,tile) partials DB (outer dim =
/// blockCount * tileCount, element block = one output block's footprint).
/// Returns the settled replicated block DB's inner payload (a memref view) so a
/// caller can wire a consumer to read it block-native.
static inline FailureOr<Value>
emitPerBlockSummingSettle(OpBuilder &builder, Location loc,
                          arts::DbAllocOp partialBlockAlloc, unsigned tileCount,
                          codir::CodeletOp codelet, unsigned depIndex) {
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
  Value zero = createZeroIndex(builder, loc);
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
  if (auto blockShape = arts::getPlanPhysicalBlockShapeAttr(
          partialBlockAlloc.getOperation()))
    arts::setPlanPhysicalBlockShapeAttr(settleAlloc.getOperation(), blockShape);
  // Replicated, not distributed: every settled block is local on every node, so
  // the distributed-ownership pass must not block-scatter it (that would defeat
  // the allreduce). The single-writer property holds per block-GUID either way.
  settleAlloc.setLocalOnlyAttr(UnitAttr::get(settleAlloc.getContext()));
  settleAlloc.setPerBlockReplicatedAttr(
      UnitAttr::get(settleAlloc.getContext()));

  // Outer per-node loop: every node settles its OWN full set of blocks (an
  // allreduce leaves the reduced result on every rank). Routing each settle to
  // the node ordinal keeps the settled write owner-local on each node's replica
  // while the RO partial acquires pull remote partials through the existing
  // cross-node acquire.
  auto totalNodesI32 = arts::RuntimeQueryOp::create(
      builder, loc, arts::RuntimeQueryKind::totalNodes);
  Value totalNodes = arith::IndexCastOp::create(
      builder, loc, builder.getIndexType(), totalNodesI32.getResult());
  auto nodeLoop = scf::ForOp::create(builder, loc, zero, totalNodes, one);
  builder.setInsertionPointToStart(nodeLoop.getBody());
  Value nodeOrdinal = nodeLoop.getInductionVar();

  auto blockLoop = scf::ForOp::create(builder, loc, zero, blockCount, one);
  builder.setInsertionPointToStart(blockLoop.getBody());
  Value blockIndex = blockLoop.getInductionVar();

  // Acquire the P per-(block,tile) partials read-only OUTSIDE the EDT (the
  // EdtLowering ABI forbids GEPing an outer DB alloc from the EDT body): each
  // partial arrives as a block-arg dep. Flat partial index = block*tileCount+t.
  Value blockBase =
      arith::MulIOp::create(builder, loc, blockIndex, tileCountVal);
  SmallVector<Value> deps;
  deps.reserve(tileCount + 1);
  for (unsigned tile = 0; tile < tileCount; ++tile) {
    Value tileVal = createConstantIndex(builder, loc, tile);
    Value partialIndex =
        arith::AddIOp::create(builder, loc, blockBase, tileVal);
    auto partialAcquire = materializeBridgeAcquire(
        builder, loc, partialBlockAlloc, arts::ArtsMode::in,
        arts::PartitionMode::block, partialIndex, one);
    deps.push_back(partialAcquire.getPtr());
  }
  // Destination: this settled block, output-only. Distinct DB per block ⇒
  // single writer ⇒ no shared EW frontier.
  auto dstAcquire =
      materializeBridgeAcquire(builder, loc, settleAlloc, arts::ArtsMode::out,
                               arts::PartitionMode::block, blockIndex, one);
  deps.push_back(dstAcquire.getPtr());

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
    Value bodyZero = createZeroIndex(builder, loc);
    // Block-args 0..tileCount-1 are the partial payloads; tileCount is the dst.
    SmallVector<Value> partialPayloads;
    partialPayloads.reserve(tileCount);
    for (unsigned tile = 0; tile < tileCount; ++tile)
      partialPayloads.push_back(arts::DbRefOp::create(
          builder, loc, body.getArgument(tile), SmallVector<Value>{bodyZero}));
    Value dstPayload =
        arts::DbRefOp::create(builder, loc, body.getArgument(tileCount),
                              SmallVector<Value>{bodyZero});
    SmallVector<Value> bodyCopySizes;
    bodyCopySizes.reserve(blockElementSizes.size());
    for (size_t i = 0; i < blockElementSizes.size(); ++i)
      bodyCopySizes.push_back(body.getArgument(tileCount + 1 + i));
    SmallVector<Value> indices;
    materializePerBlockSumNest(builder, loc, partialPayloads, dstPayload,
                               bodyCopySizes, indices);
    arts::YieldOp::create(builder, loc);
  }

  builder.setInsertionPointAfter(nodeLoop);
  auto reason = arts::ArtsBarrierReasonAttr::get(
      builder.getContext(), arts::ArtsBarrierReason::required_memory);
  arts::BarrierOp::create(builder, loc, reason);

  return materializeInnerPayload(builder, loc, settleAlloc.getPtr());
}

static inline FailureOr<Value>
materializeHostWholeToComputeBlockBridge(codir::CodeletOp codelet,
                                         unsigned depIndex, Value hostView) {
  if (!codelet || depIndex >= codelet.getDeps().size() || !hostView)
    return failure();
  if (!codirDepRequiresPhaseRedistributionBridge(codelet, depIndex))
    return failure();
  if (!hasCodirTileOwnerSlicePlan(codelet) ||
      !codirDepAccessesStayWithinSingleOwnerSlice(codelet, depIndex))
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

  // First-class collective dispatch (ADR-0003 §7b). The all-gather and cross-
  // owner reduce realizations are selected by reading the `dep_collectives`
  // carrier StoragePlanning stamped via `chooseCollective` (the extracted gate
  // bodies), not by re-evaluating the ad-hoc predicates here. `chooseCollective`
  // already folds the per-dep write-mode guard, so this `any_of` over the bridge
  // participants reproduces the historical gate decision byte-identically:
  //   all_gather     -> 3mm's F (coarse intermediate read by a sibling
  //                     `replicated_read` consumer): emit the per-block single-
  //                     writer all-gather substrate. gemm/2mm/correlation
  //                     outputs read only by the host select `none`, so the gate
  //                     stays closed and their IR is unchanged.
  //   reduce_scatter -> atax/bicg cross-owner transpose-reduce coarse buffers:
  //                     fill cross-node so every node holds the complete result.
  bool perBlockAllGather =
      needsCopyOut &&
      llvm::any_of(participants, [](const HostBridgeParticipant &participant) {
        return getCodirDepCollectiveKind(participant.codelet,
                                         participant.depIndex) ==
               codir::CodirCollectiveKind::all_gather;
      });

  bool crossNodeGatherCopyOut =
      needsCopyOut &&
      llvm::any_of(participants, [](const HostBridgeParticipant &participant) {
        return getCodirDepCollectiveKind(participant.codelet,
                                         participant.depIndex) ==
               codir::CodirCollectiveKind::reduce_scatter;
      });

  // WF-3 keystone opt-in: realize a reduce_scatter/allreduce dep with the
  // block-native per-block summing settle instead of the legacy coarse gather.
  // ADDITIVE: `emit_block_native_settle` is absent on every kernel today
  // (chooseCollective never sets it), so this stays false for the 18 oracle
  // baselines and the settle is emitted only where a producer has explicitly
  // opted in. The number of per-block partials to sum is the contraction-tile /
  // node-strip count carried on `partial_reduction_split_factor`.
  bool perBlockSummingSettle =
      needsCopyOut &&
      llvm::any_of(participants, [](const HostBridgeParticipant &participant) {
        codir::CodeletOp participantCodelet = participant.codelet;
        return participantCodelet &&
               participantCodelet.getEmitBlockNativeSettleAttr() &&
               getCodirDepCollectiveKind(participantCodelet,
                                         participant.depIndex) ==
                   codir::CodirCollectiveKind::reduce_scatter;
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

  if (needsCopyIn) {
    if (failed(materializeHostBlockCopyLoop(builder, loc, hostView, blockAlloc,
                                            codelet, depIndex,
                                            /*copyIntoBlock=*/true)))
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
              /*copyIntoBlock=*/false, crossNodeGatherCopyOut)))
        return failure();
    }
    builder.setInsertionPointAfter(anchor);
    if (failed(materializeHostBlockCopyLoop(
            builder, loc, hostView, blockAlloc, codelet, depIndex,
            /*copyIntoBlock=*/false, crossNodeGatherCopyOut)))
      return failure();

    // Emit the per-block single-writer all-gather substrate for the gated
    // replicated-read consumer pattern. This assembles, on every node, N
    // per-block replicated DBs (each its own GUID, written once, output-only),
    // RO-acquiring the producer's blocks — the race-free, concurrent collective
    // the coarse <inout> replica above could never be (ADR-0003 §2c). The
    // coarse write-back is kept so the existing whole-array consumer (3mm's G)
    // stays correct: rewiring that consumer to read the per-block DBs needs
    // contraction tiling of its k-loop (WF-3), the precise boundary of D2.
    if (perBlockAllGather) {
      builder.setInsertionPointAfter(anchor);
      FailureOr<Value> gathered = emitPerBlockAllGatherWriteBack(
          builder, loc, hostView, blockAlloc, codelet, depIndex);
      if (failed(gathered))
        return failure();
    }

    // Emit the per-block single-writer summing settle (the arith.addf dual) for
    // a reduce_scatter/allreduce dep that opted into the block-native path. Each
    // settled block is its OWN distinct-GUID DB written once with `+=` over the
    // P per-tile partials — race-free and concurrent, never the coarse <inout>
    // replica whose EW frontier serializes the partial writes (ADR-0001 phase 2;
    // ADR-0003 §2c). Until per-tile partial producers are materialized for
    // atax/bicg/3mm, this gate is opt-in only (emit_block_native_settle), so the
    // coarse gather above stays the default and the oracle is byte-identical.
    if (perBlockSummingSettle) {
      unsigned tileCount = 0;
      if (auto factor = codelet.getPartialReductionSplitFactorAttr())
        if (factor.getInt() > 0)
          tileCount = static_cast<unsigned>(factor.getInt());
      if (tileCount > 0) {
        builder.setInsertionPointAfter(anchor);
        FailureOr<Value> settled = emitPerBlockSummingSettle(
            builder, loc, blockAlloc, tileCount, codelet, depIndex);
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
  if (!codirDepRequiresPhaseRedistributionBridge(codelet, depIndex))
    return success();
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
      !codirDepAccessesStayWithinSingleOwnerSlice(codelet, depIndex))
    return success();

  Value dep = codelet.getDeps()[depIndex];
  Value hostView = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep);
  arts::DbAllocOp sourceAlloc = findBackingDbAlloc(hostView);
  if (!sourceAlloc)
    return success();
  if (canUseCodirOwnerSliceForAlloc(codelet, depIndex, sourceAlloc))
    return success();

  std::optional<arts::PartitionMode> sourceMode =
      arts::getPartitionMode(sourceAlloc.getOperation());
  if (!sourceMode || *sourceMode != arts::PartitionMode::coarse)
    return success();

  auto memrefType = dyn_cast<MemRefType>(hostView.getType());
  if (!memrefType || memrefType.getRank() == 0)
    return success();

  FailureOr<SmallVector<HostBridgeParticipant>> participants =
      collectComputeBlockParticipants(codelet, depIndex, hostView, sourceAlloc);
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
  return codirDepAccessesStayWithinSingleOwnerSlice(planSource, *depIndex);
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

  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep);
  auto memrefType = dyn_cast<MemRefType>(root.getType());
  if (!memrefType)
    return failure();

  Operation *def = root.getDefiningOp();
  OpBuilder builder(root.getContext());
  Value replacement;
  bool usePlan = canMaterializeRawCodirDependencyWithPlan(root, planSource);
  bool needsHostBridge = rawCodirDependencyNeedsHostBridge(root);
  if (usePlan && needsHostBridge &&
      !codirDepRequiresPhaseRedistributionBridge(planSource, depIndex))
    usePlan = false;
  if (usePlan &&
      codirDepRequiresPhaseRedistributionBridge(planSource, depIndex) &&
      needsHostBridge) {
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
