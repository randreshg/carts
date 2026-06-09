///==========================================================================///
/// File: CodirToArts.cpp
///
/// Materializes CODIR codelets as abstract ARTS DB/EDT objects.
///==========================================================================///
#include "CodirToArtsHostBridgeMaterialization.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/codir/Conversion/Passes.h"
#include <numeric>
namespace mlir::carts::codir {
#define GEN_PASS_DEF_CONVERTCODIRTOARTS
#include "carts/dialect/codir/Conversion/Passes.h.inc"
} // namespace mlir::carts::codir
namespace {

// CODIR carries committed dependency and schedule facts on the codelet. This
// conversion only restates them onto the ARTS task in ARTS form; deriving the
// EDT distribution plan (family, version, block-halo capability) from those
// committed facts is the ARTS realize-edt-distribution-plan pass.
static arts::ArtsDepPattern convertPattern(codir::CodirPattern pattern) {
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

static arts::EdtDistributionKind
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

static arts::ArtsPlanRepetitionStructure
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

// Mechanically restate the committed codelet plan facts onto the ARTS task.
// This forwards already-committed CODIR/SDE facts in ARTS form; it does not
// classify movement and does not derive the distribution family/version/halo
// capability (the ARTS realize-edt-distribution-plan pass owns that).
static void forwardCommittedEdtPlan(codir::CodeletOp codelet,
                                    arts::EdtOp task) {
  if (!codelet || !task)
    return;
  MLIRContext *ctx = codelet.getContext();
  Operation *taskOp = task.getOperation();
  if (auto pattern = codelet.getPatternAttr()) {
    arts::ArtsDepPattern depPattern = convertPattern(pattern.getValue());
    if (depPattern != arts::ArtsDepPattern::unknown)
      arts::setDepPattern(taskOp, depPattern);
  }
  if (auto kind = codelet.getDistributionKindAttr())
    arts::setEdtDistributionKind(taskOp,
                                 convertDistributionKind(kind.getValue()));
  if (auto topology = codelet.getIterationTopologyAttr())
    arts::setPlanIterationTopologyAttr(
        taskOp, arts::ArtsPlanIterationTopologyAttr::get(
                    ctx, static_cast<arts::ArtsPlanIterationTopology>(
                             topology.getValue())));
  if (auto repetition = codelet.getRepetitionStructureAttr())
    arts::setPlanRepetitionStructureAttr(
        taskOp, arts::ArtsPlanRepetitionStructureAttr::get(
                    ctx, convertRepetitionStructure(repetition.getValue())));
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
}

static LogicalResult rejectResidualSdeOps(ModuleOp module) {
  bool found = false;
  module.walk([&](Operation *op) {
    if (!op->getDialect() || op->getDialect()->getNamespace() != "sde")
      return;
    op->emitError() << "SDE operation reached CODIR-to-ARTS; run "
                       "`materialize-sde-boundary-to-arts` before "
                       "`convert-codir-to-arts`";
    found = true;
  });
  return failure(found);
}

static void translateCodirAtomicsToArts(Region &region) {
  SmallVector<codir::AtomicAddOp, 4> atomics;
  region.walk([&](codir::AtomicAddOp atomic) { atomics.push_back(atomic); });
  for (codir::AtomicAddOp atomic : atomics) {
    OpBuilder builder(atomic);
    arts::AtomicAddOp::create(builder, atomic.getLoc(), atomic.getAddr(),
                              atomic.getValue());
    atomic.erase();
  }
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

static CodirDepSlice getCodirDepSlice(Value dep, OpBuilder &builder,
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

struct PlannedBlockDepAccessPlan {
  SmallVector<unsigned, 4> ownerDims;
  SmallVector<int64_t, 4> blockSizes;
  SmallVector<Value, 4> ownerParams;
  SmallVector<Value, 4> ownerDomainBases;
  SmallVector<Value, 4> acquiredElementBases;
  SmallVector<int64_t, 4> groupBlockCounts;
  SmallVector<int64_t, 4> ownerWindowExtents;
  SmallVector<int64_t, 4> lowerHaloBlockCounts;
  SmallVector<int64_t, 4> upperHaloBlockCounts;
  SmallVector<bool, 4> allowFullWindowAccesses;
  SmallVector<bool, 4> requireOwnerWindowProofs;
  bool grouped = false;

  bool empty() const { return ownerDims.empty(); }
};

static std::optional<SmallVector<int64_t, 4>>
getBackingAllocOwnerBlockSizes(ArrayRef<unsigned> ownerDims,
                               arts::DbAllocOp alloc);

static std::optional<SmallVector<int64_t, 4>>
getCodirLogicalOwnerBlockCounts(codir::CodeletOp codelet, unsigned depIndex,
                                unsigned memrefRank,
                                ArrayRef<int64_t> blockSizes) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty() ||
      ownerDims->size() != blockSizes.size())
    return std::nullopt;

  std::optional<SmallVector<int64_t, 4>> logicalSlice =
      readI64ArrayAttr(codelet.getLogicalWorkerSliceAttr());
  SmallVector<int64_t, 4> groupBlocks;
  groupBlocks.reserve(ownerDims->size());
  for (auto [slot, ownerDim] : llvm::enumerate(*ownerDims)) {
    int64_t blockSize = blockSizes[slot];
    if (blockSize <= 0)
      return std::nullopt;
    int64_t logicalExtent = blockSize;
    if (logicalSlice && logicalSlice->size() == memrefRank) {
      if (ownerDim >= logicalSlice->size())
        return std::nullopt;
      logicalExtent = (*logicalSlice)[ownerDim];
    } else if (logicalSlice && ownerDim < logicalSlice->size()) {
      logicalExtent = (*logicalSlice)[ownerDim];
    } else if (logicalSlice && logicalSlice->size() == ownerDims->size()) {
      logicalExtent = (*logicalSlice)[slot];
    } else if (logicalSlice && logicalSlice->size() == 1 &&
               ownerDims->size() == 1) {
      logicalExtent = logicalSlice->front();
    }
    if (logicalExtent <= 0)
      return std::nullopt;
    logicalExtent = std::max<int64_t>(logicalExtent, blockSize);
    groupBlocks.push_back(llvm::divideCeil(logicalExtent, blockSize));
  }
  return groupBlocks;
}

static bool
planReplicatedReadFullBlockAccess(codir::CodeletOp codelet, unsigned depIndex,
                                  arts::DbAllocOp alloc, OpBuilder &builder,
                                  Location loc,
                                  PlannedBlockDepAccessPlan &plannedAccess) {
  if (!canUseCodirOwnerSliceForAlloc(codelet, depIndex, alloc))
    return false;

  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  std::optional<SmallVector<int64_t, 4>> blockSizes =
      getCodirTileOwnerBlockSizes(
          codelet, depIndex,
          static_cast<unsigned>(alloc.getElementSizes().size()));
  if (!ownerDims || !blockSizes || ownerDims->empty() ||
      ownerDims->size() != blockSizes->size() ||
      alloc.getSizes().size() != ownerDims->size())
    return false;

  SmallVector<int64_t, 4> blockCounts;
  blockCounts.reserve(alloc.getSizes().size());
  for (Value blockCountValue : alloc.getSizes()) {
    std::optional<int64_t> blockCount =
        ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(blockCountValue);
    if (!blockCount || *blockCount <= 0)
      return false;
    blockCounts.push_back(*blockCount);
  }

  plannedAccess.ownerDims.assign(ownerDims->begin(), ownerDims->end());
  plannedAccess.blockSizes.assign(blockSizes->begin(), blockSizes->end());
  plannedAccess.ownerParams.assign(ownerDims->size(), Value{});
  plannedAccess.ownerDomainBases.assign(ownerDims->size(),
                                        createZeroIndex(builder, loc));
  plannedAccess.acquiredElementBases.assign(ownerDims->size(),
                                            createZeroIndex(builder, loc));
  plannedAccess.groupBlockCounts.assign(blockCounts.begin(), blockCounts.end());
  plannedAccess.ownerWindowExtents.reserve(blockCounts.size());
  for (auto [slot, blockCount] : llvm::enumerate(blockCounts)) {
    if (blockCount > std::numeric_limits<int64_t>::max() / (*blockSizes)[slot])
      return false;
    plannedAccess.ownerWindowExtents.push_back(blockCount *
                                               (*blockSizes)[slot]);
  }
  plannedAccess.lowerHaloBlockCounts.assign(ownerDims->size(), 0);
  plannedAccess.upperHaloBlockCounts.assign(ownerDims->size(), 0);
  plannedAccess.allowFullWindowAccesses.assign(ownerDims->size(), true);
  plannedAccess.requireOwnerWindowProofs.assign(ownerDims->size(), false);
  plannedAccess.grouped = true;
  return true;
}

static std::optional<SmallVector<int64_t, 4>>
getBackingAllocOwnerBlockSizes(codir::CodeletOp codelet, unsigned depIndex,
                               arts::DbAllocOp alloc) {
  if (!alloc)
    return std::nullopt;
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty())
    return std::nullopt;
  return getBackingAllocOwnerBlockSizes(*ownerDims, alloc);
}

static std::optional<SmallVector<unsigned, 4>>
getBackingAllocOwnerDims(arts::DbAllocOp alloc) {
  ArrayAttr ownerDimsAttr =
      alloc ? arts::getPlanOwnerDimsAttr(alloc.getOperation()) : ArrayAttr{};
  std::optional<SmallVector<int64_t, 4>> rawOwnerDims =
      readI64ArrayAttr(ownerDimsAttr);
  if (!rawOwnerDims || rawOwnerDims->empty())
    return std::nullopt;

  SmallVector<unsigned, 4> ownerDims;
  ownerDims.reserve(rawOwnerDims->size());
  for (int64_t ownerDim : *rawOwnerDims) {
    if (ownerDim < 0)
      return std::nullopt;
    ownerDims.push_back(static_cast<unsigned>(ownerDim));
  }
  return ownerDims;
}

static std::optional<SmallVector<int64_t, 4>>
getBackingAllocOwnerBlockSizes(ArrayRef<unsigned> ownerDims,
                               arts::DbAllocOp alloc) {
  if (!alloc || ownerDims.empty())
    return std::nullopt;

  std::optional<SmallVector<int64_t, 4>> allocBlockShape = readI64ArrayAttr(
      arts::getPlanPhysicalBlockShapeAttr(alloc.getOperation()));
  if (!allocBlockShape || allocBlockShape->empty())
    return std::nullopt;

  SmallVector<int64_t, 4> blockSizes;
  blockSizes.reserve(ownerDims.size());
  unsigned memrefRank = static_cast<unsigned>(alloc.getElementSizes().size());
  for (auto [slot, ownerDim] : llvm::enumerate(ownerDims)) {
    std::optional<int64_t> blockSize;
    if (allocBlockShape->size() == ownerDims.size()) {
      blockSize = (*allocBlockShape)[slot];
    } else if (allocBlockShape->size() == memrefRank) {
      if (ownerDim >= allocBlockShape->size())
        return std::nullopt;
      blockSize = (*allocBlockShape)[ownerDim];
    } else if (allocBlockShape->size() == 1 && ownerDims.size() == 1) {
      blockSize = allocBlockShape->front();
    }
    if (!blockSize || *blockSize <= 0)
      return std::nullopt;
    blockSizes.push_back(*blockSize);
  }
  return blockSizes;
}

static std::optional<int64_t>
getLogicalWorkerExtentForOwnerDim(codir::CodeletOp codelet, unsigned ownerDim,
                                  unsigned ownerSlot, unsigned memrefRank) {
  std::optional<SmallVector<int64_t, 4>> logicalSlice =
      readI64ArrayAttr(codelet.getLogicalWorkerSliceAttr());
  if (!logicalSlice)
    return std::nullopt;

  if (logicalSlice->size() == memrefRank) {
    if (ownerDim >= logicalSlice->size())
      return std::nullopt;
    return (*logicalSlice)[ownerDim];
  }

  std::optional<unsigned> tileSlot = getCodirOwnerDimSlot(codelet, ownerDim);
  if (tileSlot && *tileSlot < logicalSlice->size())
    return (*logicalSlice)[*tileSlot];

  if (ownerSlot < logicalSlice->size())
    return (*logicalSlice)[ownerSlot];

  return std::nullopt;
}

static std::optional<unsigned> findOwnerDimSlot(ArrayRef<unsigned> ownerDims,
                                                unsigned ownerDim) {
  auto it = llvm::find(ownerDims, ownerDim);
  if (it == ownerDims.end())
    return std::nullopt;
  return static_cast<unsigned>(std::distance(ownerDims.begin(), it));
}

static bool isKnownMultipleOfBlock(Value value, int64_t blockSize,
                                   unsigned depth = 0) {
  if (!value || blockSize <= 0 || depth > 6)
    return false;
  if (blockSize == 1)
    return true;
  value = ::mlir::carts::ValueAnalysis::stripNumericCasts(value);
  if (std::optional<int64_t> constant =
          ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(value))
    return *constant % blockSize == 0;

  if (auto blockArg = dyn_cast<BlockArgument>(value)) {
    auto loop =
        dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
    if (!loop || loop.getInductionVar() != blockArg)
      return false;
    std::optional<int64_t> step =
        ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(loop.getStep());
    return step && *step % blockSize == 0 &&
           isKnownMultipleOfBlock(loop.getLowerBound(), blockSize, depth + 1);
  }

  if (auto add = value.getDefiningOp<arith::AddIOp>())
    return isKnownMultipleOfBlock(add.getLhs(), blockSize, depth + 1) &&
           isKnownMultipleOfBlock(add.getRhs(), blockSize, depth + 1);
  if (auto sub = value.getDefiningOp<arith::SubIOp>())
    return isKnownMultipleOfBlock(sub.getLhs(), blockSize, depth + 1) &&
           isKnownMultipleOfBlock(sub.getRhs(), blockSize, depth + 1);
  if (auto mul = value.getDefiningOp<arith::MulIOp>())
    return isKnownMultipleOfBlock(mul.getLhs(), blockSize, depth + 1) ||
           isKnownMultipleOfBlock(mul.getRhs(), blockSize, depth + 1);
  return false;
}

static int64_t gcdPositive(int64_t lhs, int64_t rhs) {
  lhs = std::abs(lhs);
  rhs = std::abs(rhs);
  if (lhs == 0)
    return rhs;
  if (rhs == 0)
    return lhs;
  return std::gcd(lhs, rhs);
}

static int64_t getKnownAlignmentWithinBlock(Value value, int64_t blockSize,
                                            unsigned depth = 0) {
  if (!value || blockSize <= 1 || depth > 6)
    return 1;
  value = ::mlir::carts::ValueAnalysis::stripNumericCasts(value);
  if (std::optional<int64_t> constant =
          ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(value))
    return *constant == 0 ? blockSize : gcdPositive(*constant, blockSize);

  if (auto blockArg = dyn_cast<BlockArgument>(value)) {
    auto loop =
        dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
    if (!loop || loop.getInductionVar() != blockArg)
      return 1;
    int64_t lowerAlignment = getKnownAlignmentWithinBlock(loop.getLowerBound(),
                                                          blockSize, depth + 1);
    std::optional<int64_t> step =
        ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(loop.getStep());
    if (!step)
      return lowerAlignment;
    return gcdPositive(gcdPositive(lowerAlignment, *step), blockSize);
  }

  if (auto add = value.getDefiningOp<arith::AddIOp>())
    return gcdPositive(
        getKnownAlignmentWithinBlock(add.getLhs(), blockSize, depth + 1),
        getKnownAlignmentWithinBlock(add.getRhs(), blockSize, depth + 1));
  if (auto sub = value.getDefiningOp<arith::SubIOp>())
    return gcdPositive(
        getKnownAlignmentWithinBlock(sub.getLhs(), blockSize, depth + 1),
        getKnownAlignmentWithinBlock(sub.getRhs(), blockSize, depth + 1));
  if (auto mul = value.getDefiningOp<arith::MulIOp>()) {
    auto scaledAlignment = [&](Value scaled, Value factor) -> int64_t {
      int64_t alignment =
          getKnownAlignmentWithinBlock(scaled, blockSize, depth + 1);
      std::optional<int64_t> factorConstant =
          ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(factor);
      if (!factorConstant)
        return alignment;
      if (*factorConstant == 0)
        return blockSize;
      if (alignment >
          std::numeric_limits<int64_t>::max() / std::abs(*factorConstant))
        return blockSize;
      return gcdPositive(alignment * std::abs(*factorConstant), blockSize);
    };
    return std::max(scaledAlignment(mul.getLhs(), mul.getRhs()),
                    scaledAlignment(mul.getRhs(), mul.getLhs()));
  }
  return 1;
}

static std::optional<int64_t>
getGuaranteedOwnerWindowExtent(int64_t blockSize, int64_t groupBlockCount,
                               int64_t baseAlignment) {
  if (blockSize <= 0 || groupBlockCount <= 0 || baseAlignment <= 0 ||
      baseAlignment > blockSize)
    return std::nullopt;
  if (groupBlockCount > std::numeric_limits<int64_t>::max() / blockSize)
    return std::nullopt;
  int64_t windowExtent = groupBlockCount * blockSize;
  int64_t worstCasePrefix = blockSize - baseAlignment;
  if (windowExtent <= worstCasePrefix)
    return std::nullopt;
  return windowExtent - worstCasePrefix;
}

static bool canUseBackingAllocBlockWindowForDep(codir::CodeletOp codelet,
                                                unsigned depIndex,
                                                arts::DbAllocOp alloc) {
  if (canUseCodirOwnerSliceForAlloc(codelet, depIndex, alloc))
    return true;

  std::optional<codir::CodirAccessMode> mode =
      getCodirDepAccessMode(codelet, depIndex);
  if (!mode || *mode != codir::CodirAccessMode::read)
    return false;
  if (!codirDepAllowsComputeBlockStorage(codelet, depIndex) ||
      !hasCodirTileOwnerSlicePlan(codelet) || !alloc)
    return false;

  std::optional<arts::PartitionMode> partitionMode =
      arts::getPartitionMode(alloc.getOperation());
  if (!partitionMode || (*partitionMode != arts::PartitionMode::block &&
                         *partitionMode != arts::PartitionMode::stencil))
    return false;
  if (*partitionMode == arts::PartitionMode::stencil)
    return false;

  std::optional<SmallVector<unsigned, 4>> depOwnerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  std::optional<SmallVector<unsigned, 4>> allocOwnerDims =
      getBackingAllocOwnerDims(alloc);
  if (!depOwnerDims || depOwnerDims->empty() || !allocOwnerDims ||
      allocOwnerDims->empty() ||
      alloc.getSizes().size() != allocOwnerDims->size())
    return false;

  for (unsigned depOwnerDim : *depOwnerDims)
    if (!findOwnerDimSlot(*allocOwnerDims, depOwnerDim))
      return false;

  return static_cast<bool>(
      getBackingAllocOwnerBlockSizes(*allocOwnerDims, alloc));
}

static MemRefType getCodeletDepSourceType(codir::CodeletOp codelet,
                                          unsigned depIndex) {
  if (!codelet.getBody().empty() &&
      depIndex < codelet.getBody().front().getNumArguments())
    if (auto depType = dyn_cast<MemRefType>(
            codelet.getBody().front().getArgument(depIndex).getType()))
      return depType;
  if (depIndex < codelet.getDeps().size())
    return dyn_cast<MemRefType>(codelet.getDeps()[depIndex].getType());
  return {};
}

static bool
planReadOnlyHostWholeBlockAccess(codir::CodeletOp codelet, unsigned depIndex,
                                 arts::DbAllocOp alloc, OpBuilder &builder,
                                 Location loc,
                                 PlannedBlockDepAccessPlan &plannedAccess) {
  std::optional<codir::CodirAccessMode> mode =
      getCodirDepAccessMode(codelet, depIndex);
  if (!mode || *mode != codir::CodirAccessMode::read)
    return false;
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  if (!view || *view != codir::CodirStorageViewKind::host_whole)
    return false;
  if (!hasCodirTileOwnerSlicePlan(codelet) || !alloc)
    return false;
  std::optional<arts::PartitionMode> partitionMode =
      arts::getPartitionMode(alloc.getOperation());
  if (!partitionMode || (*partitionMode != arts::PartitionMode::block &&
                         *partitionMode != arts::PartitionMode::stencil))
    return false;

  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty() ||
      alloc.getSizes().size() != ownerDims->size())
    return false;
  ArrayAttr depOwnerDims = getCodirDepOwnerDimsAttr(codelet, depIndex);
  if (!depOwnerDims ||
      arts::getPlanOwnerDimsAttr(alloc.getOperation()) != depOwnerDims)
    return false;
  std::optional<SmallVector<int64_t, 4>> blockSizes =
      getBackingAllocOwnerBlockSizes(codelet, depIndex, alloc);
  if (!blockSizes)
    return false;

  SmallVector<int64_t, 4> blockCounts;
  blockCounts.reserve(alloc.getSizes().size());
  MemRefType depType = getCodeletDepSourceType(codelet, depIndex);
  for (auto [slot, blockCountValue] : llvm::enumerate(alloc.getSizes())) {
    std::optional<int64_t> blockCount =
        ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(blockCountValue);
    if (!blockCount && depType &&
        (*ownerDims)[slot] < static_cast<unsigned>(depType.getRank())) {
      int64_t extent = depType.getDimSize((*ownerDims)[slot]);
      if (extent >= 0)
        blockCount = llvm::divideCeil(extent, (*blockSizes)[slot]);
    }
    if (!blockCount || *blockCount <= 0)
      return false;
    blockCounts.push_back(*blockCount);
  }

  plannedAccess.ownerDims.assign(ownerDims->begin(), ownerDims->end());
  plannedAccess.blockSizes.assign(blockSizes->begin(), blockSizes->end());
  plannedAccess.ownerParams.assign(ownerDims->size(), Value{});
  plannedAccess.ownerDomainBases.assign(ownerDims->size(),
                                        createZeroIndex(builder, loc));
  plannedAccess.groupBlockCounts.assign(blockCounts.begin(), blockCounts.end());
  plannedAccess.ownerWindowExtents.reserve(blockCounts.size());
  for (auto [slot, blockCount] : llvm::enumerate(blockCounts)) {
    if (blockCount > std::numeric_limits<int64_t>::max() / (*blockSizes)[slot])
      return false;
    plannedAccess.ownerWindowExtents.push_back(blockCount *
                                               (*blockSizes)[slot]);
  }
  plannedAccess.allowFullWindowAccesses.assign(ownerDims->size(), true);
  plannedAccess.requireOwnerWindowProofs.assign(ownerDims->size(), false);
  plannedAccess.grouped = true;
  return true;
}

static bool planReadOnlyBlockStorageAccessFromBackingAlloc(
    codir::CodeletOp codelet, unsigned depIndex, arts::DbAllocOp alloc,
    OpBuilder &builder, Location loc, SmallVectorImpl<Value> &dbOffsets,
    SmallVectorImpl<Value> &dbSizes, PlannedBlockDepAccessPlan &plannedAccess) {
  std::optional<codir::CodirAccessMode> mode =
      getCodirDepAccessMode(codelet, depIndex);
  if (!mode || *mode != codir::CodirAccessMode::read)
    return false;
  std::optional<codir::CodirStorageViewKind> view =
      getCodirDepStorageViewKind(codelet, depIndex);
  if (!view || !codirStorageViewUsesComputeBlock(*view))
    return false;
  if (!hasCodirTileOwnerSlicePlan(codelet) || !alloc)
    return false;
  std::optional<arts::PartitionMode> partitionMode =
      arts::getPartitionMode(alloc.getOperation());
  if (!partitionMode || (*partitionMode != arts::PartitionMode::block &&
                         *partitionMode != arts::PartitionMode::stencil))
    return false;

  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty())
    return false;

  std::optional<SmallVector<unsigned, 4>> allocOwnerDims =
      getBackingAllocOwnerDims(alloc);
  if (!allocOwnerDims || allocOwnerDims->empty() ||
      alloc.getSizes().size() != allocOwnerDims->size())
    return false;

  std::optional<SmallVector<int64_t, 4>> blockSizes =
      getBackingAllocOwnerBlockSizes(*allocOwnerDims, alloc);
  if (!blockSizes || blockSizes->size() != allocOwnerDims->size())
    return false;

  SmallVector<Value, 4> ownerParams =
      getCodirDepOwnerParamValues(codelet, depIndex);
  if (ownerParams.size() != ownerDims->size())
    return false;

  SmallVector<std::optional<unsigned>, 4> depSlotByAllocSlot;
  depSlotByAllocSlot.reserve(allocOwnerDims->size());
  for (unsigned allocOwnerDim : *allocOwnerDims) {
    std::optional<unsigned> depSlot =
        findOwnerDimSlot(*ownerDims, allocOwnerDim);
    depSlotByAllocSlot.push_back(depSlot);
  }
  for (unsigned depOwnerDim : *ownerDims)
    if (!findOwnerDimSlot(*allocOwnerDims, depOwnerDim))
      return false;

  bool hasHaloWindow = false;
  for (unsigned ownerDim : *allocOwnerDims) {
    CodirOwnerHaloWindow halo = getCodirBlockStorageHaloWindowForDim(
        codelet, depIndex, ownerDim,
        static_cast<unsigned>(alloc.getElementSizes().size()));
    hasHaloWindow |= !halo.empty();
  }

  dbOffsets.clear();
  dbSizes.clear();
  bool grouped = false;
  for (auto [slot, ownerDim] : llvm::enumerate(*allocOwnerDims)) {
    std::optional<unsigned> depSlot = depSlotByAllocSlot[slot];
    Value blockIndex = createZeroIndex(builder, loc);
    int64_t groupBlockCount = 1;
    int64_t ownerWindowExtent = 1;
    bool allowFullWindowAccess = false;
    Value ownerParam;
    Value domainBase = createZeroIndex(builder, loc);

    if (depSlot) {
      int64_t blockSize = (*blockSizes)[slot];
      std::optional<int64_t> logicalExtent = getLogicalWorkerExtentForOwnerDim(
          codelet, ownerDim, static_cast<unsigned>(slot),
          static_cast<unsigned>(alloc.getElementSizes().size()));
      if (!logicalExtent)
        logicalExtent = blockSize;
      if (*logicalExtent <= 0)
        return false;
      int64_t exactLogicalExtent = *logicalExtent;

      Value blockSizeValue = createConstantIndex(builder, loc, blockSize);
      ownerParam = ownerParams[*depSlot];
      domainBase =
          materializeCodirOwnerDomainBase(builder, loc, codelet, ownerParam);
      Value relativeBase =
          ::mlir::carts::ValueAnalysis::sameValue(ownerParam, domainBase)
              ? createZeroIndex(builder, loc)
              : arith::SubIOp::create(builder, loc, ownerParam, domainBase)
                    .getResult();
      int64_t baseAlignment =
          getKnownAlignmentWithinBlock(relativeBase, blockSize);
      int64_t maxCoveredExtent =
          exactLogicalExtent + (blockSize - baseAlignment);
      groupBlockCount =
          std::max<int64_t>(1, llvm::divideCeil(maxCoveredExtent, blockSize));
      std::optional<int64_t> computedOwnerWindowExtent =
          getGuaranteedOwnerWindowExtent(blockSize, groupBlockCount,
                                         baseAlignment);
      if (!computedOwnerWindowExtent)
        return false;
      ownerWindowExtent = exactLogicalExtent;
      blockIndex =
          arith::DivUIOp::create(builder, loc, relativeBase, blockSizeValue);
      dbOffsets.push_back(blockIndex);

      if (groupBlockCount > 1) {
        Value blockOffset =
            arith::MulIOp::create(builder, loc, blockIndex, blockSizeValue);
        Value intraBlockOffset =
            arith::SubIOp::create(builder, loc, relativeBase, blockOffset);
        Value logicalExtentValue =
            createConstantIndex(builder, loc, exactLogicalExtent);
        Value coveredExtent = arith::AddIOp::create(
            builder, loc, intraBlockOffset, logicalExtentValue);
        Value ceilNumerator = arith::AddIOp::create(
            builder, loc, coveredExtent,
            createConstantIndex(builder, loc, blockSize - 1));
        Value dynamicGroupBlockCount =
            arith::DivUIOp::create(builder, loc, ceilNumerator, blockSizeValue);
        Value maxGroupBlockCount =
            createConstantIndex(builder, loc, groupBlockCount);
        Value requestedBlockCount = arith::MinUIOp::create(
            builder, loc, dynamicGroupBlockCount, maxGroupBlockCount);
        Value remainingBlocks = arith::SubIOp::create(
            builder, loc, alloc.getSizes()[slot], blockIndex);
        dbSizes.push_back(arith::MinUIOp::create(builder, loc, remainingBlocks,
                                                 requestedBlockCount));
        grouped = true;
        plannedAccess.ownerDims.push_back(ownerDim);
        plannedAccess.blockSizes.push_back((*blockSizes)[slot]);
        plannedAccess.ownerParams.push_back(ownerParam);
        plannedAccess.ownerDomainBases.push_back(domainBase);
        plannedAccess.groupBlockCounts.push_back(groupBlockCount);
        plannedAccess.ownerWindowExtents.push_back(ownerWindowExtent);
        plannedAccess.allowFullWindowAccesses.push_back(false);
        plannedAccess.requireOwnerWindowProofs.push_back(true);
        continue;
      }
    } else {
      std::optional<int64_t> blockCount =
          ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(
              alloc.getSizes()[slot]);
      if (!blockCount || *blockCount <= 0)
        return false;
      groupBlockCount = *blockCount;
      if (groupBlockCount >
          std::numeric_limits<int64_t>::max() / (*blockSizes)[slot])
        return false;
      ownerWindowExtent = groupBlockCount * (*blockSizes)[slot];
      allowFullWindowAccess = true;
      dbOffsets.push_back(blockIndex);
    }

    if (groupBlockCount <= 1) {
      dbSizes.push_back(createOneIndex(builder, loc));
    } else {
      Value requestedBlocks =
          createConstantIndex(builder, loc, groupBlockCount);
      Value remainingBlocks = arith::SubIOp::create(
          builder, loc, alloc.getSizes()[slot], blockIndex);
      dbSizes.push_back(arith::MinUIOp::create(builder, loc, remainingBlocks,
                                               requestedBlocks));
    }
    grouped |= groupBlockCount > 1;
    plannedAccess.ownerDims.push_back(ownerDim);
    plannedAccess.blockSizes.push_back((*blockSizes)[slot]);
    plannedAccess.ownerParams.push_back(ownerParam);
    plannedAccess.ownerDomainBases.push_back(domainBase);
    plannedAccess.groupBlockCounts.push_back(groupBlockCount);
    plannedAccess.ownerWindowExtents.push_back(ownerWindowExtent);
    plannedAccess.allowFullWindowAccesses.push_back(allowFullWindowAccess);
    plannedAccess.requireOwnerWindowProofs.push_back(false);
  }
  if (grouped && hasHaloWindow &&
      !codirDepAccessesStayWithinSingleOwnerSlice(codelet, depIndex))
    return false;
  plannedAccess.grouped = grouped;
  return true;
}

struct ConvertCodirToArtsPass
    : public codir::impl::ConvertCodirToArtsBase<ConvertCodirToArtsPass> {
  llvm::SmallDenseSet<Operation *, 16> loopCompletionBarriers;

  LogicalResult requireOnePlanningEntryPerDependency(codir::CodeletOp codelet,
                                                     ArrayAttr attr,
                                                     StringRef attrName) {
    if (codelet.getDeps().size() == (attr ? attr.size() : 0))
      return success();
    return codelet.emitOpError()
           << "requires one " << attrName
           << " entry per dependency before CODIR-to-ARTS materialization";
  }

  LogicalResult requireDepModes(codir::CodeletOp codelet) {
    StringRef attrName = codelet.getDepModesAttrName();
    ArrayAttr modes = codelet.getDepModesAttr();
    if (failed(requireOnePlanningEntryPerDependency(codelet, modes, attrName)))
      return failure();
    if (!modes)
      return success();
    for (auto [index, attr] : llvm::enumerate(modes)) {
      if (isa<codir::CodirAccessModeAttr>(attr))
        continue;
      return codelet.emitOpError()
             << attrName << " entry #" << index
             << " must be a CODIR access_mode attribute, got " << attr;
    }
    return success();
  }

  LogicalResult requireDepStorageViews(codir::CodeletOp codelet) {
    StringRef attrName = codelet.getDepStorageViewsAttrName();
    ArrayAttr storageViews = codelet.getDepStorageViewsAttr();
    if (failed(requireOnePlanningEntryPerDependency(codelet, storageViews,
                                                    attrName)))
      return failure();
    if (!storageViews)
      return success();
    for (auto [index, attr] : llvm::enumerate(storageViews)) {
      if (isa<codir::CodirStorageViewKindAttr>(attr))
        continue;
      return codelet.emitOpError()
             << attrName << " entry #" << index
             << " must be a CODIR storage_view attribute, got " << attr;
    }
    return success();
  }

  LogicalResult requireDepOwnerDims(codir::CodeletOp codelet) {
    StringRef attrName = codelet.getDepOwnerDimsAttrName();
    ArrayAttr ownerDims = codelet.getDepOwnerDimsAttr();
    if (failed(
            requireOnePlanningEntryPerDependency(codelet, ownerDims, attrName)))
      return failure();
    if (!ownerDims)
      return success();
    for (auto [index, attr] : llvm::enumerate(ownerDims)) {
      auto dims = dyn_cast<ArrayAttr>(attr);
      if (!dims)
        return codelet.emitOpError()
               << attrName << " entry #" << index
               << " must be an array attribute, got " << attr;
      for (Attribute dim : dims)
        if (!isa<IntegerAttr>(dim))
          return codelet.emitOpError()
                 << attrName << " entry #" << index
                 << " must contain integer attributes, got " << dim;
    }
    return success();
  }

  LogicalResult requireDepCollectives(codir::CodeletOp codelet) {
    StringRef attrName = codelet.getDepCollectivesAttrName();
    ArrayAttr collectives = codelet.getDepCollectivesAttr();
    if (failed(requireOnePlanningEntryPerDependency(codelet, collectives,
                                                    attrName)))
      return failure();
    if (!collectives)
      return success();
    for (auto [index, attr] : llvm::enumerate(collectives)) {
      if (isa<codir::CodirCollectiveKindAttr>(attr))
        continue;
      return codelet.emitOpError()
             << attrName << " entry #" << index
             << " must be a CODIR collective attribute, got " << attr;
    }
    return success();
  }

  LogicalResult requireFinalizedPlanningFacts(codir::CodeletOp codelet) {
    if (failed(requireDepModes(codelet)))
      return failure();
    if (failed(requireDepStorageViews(codelet)))
      return failure();
    if (failed(requireDepOwnerDims(codelet)))
      return failure();
    if (failed(requireDepCollectives(codelet)))
      return failure();
    return success();
  }

  bool isRankExpandedOwnerStripReadOnlyHaloDep(codir::CodeletOp codelet,
                                               unsigned depIdx) {
    auto topology = codelet.getIterationTopology();
    if (!topology || *topology != codir::CodirIterationTopology::owner_strip)
      return false;
    if (!codirDepRequiresComputeBlockStorage(codelet, depIdx))
      return false;
    if (getFinalizedCodirDepCollectiveKind(codelet, depIdx) !=
        codir::CodirCollectiveKind::halo)
      return false;
    std::optional<codir::CodirAccessMode> mode =
        getCodirDepAccessMode(codelet, depIdx);
    if (!mode || !codirAccessMayRead(*mode) || codirAccessMayWrite(*mode))
      return false;
    std::optional<SmallVector<unsigned, 4>> depOwnerDims =
        getCodirDepOwnerDims(codelet, depIdx);
    std::optional<SmallVector<int64_t, 4>> tileOwnerDims =
        readI64ArrayAttr(codelet.getTileOwnerDimsAttr());
    if (!depOwnerDims || !tileOwnerDims || depOwnerDims->empty() ||
        depOwnerDims->size() != tileOwnerDims->size())
      return false;
    for (auto [slot, depOwnerDim] : llvm::enumerate(*depOwnerDims)) {
      int64_t tileOwnerDim = (*tileOwnerDims)[slot];
      if (tileOwnerDim < 0 ||
          static_cast<unsigned>(tileOwnerDim) != depOwnerDim)
        return true;
    }
    return false;
  }

  bool isGroupedOwnerStripReadWriteHaloDep(codir::CodeletOp codelet,
                                           unsigned depIdx) {
    auto topology = codelet.getIterationTopologyAttr();
    if (!topology ||
        topology.getValue() != codir::CodirIterationTopology::owner_strip)
      return false;
    auto distribution = codelet.getDistributionKindAttr();
    if (!distribution ||
        distribution.getValue() != codir::CodirDistributionKind::owner_compute)
      return false;
    if (!codirDepRequiresComputeBlockStorage(codelet, depIdx))
      return false;
    if (getFinalizedCodirDepCollectiveKind(codelet, depIdx) !=
        codir::CodirCollectiveKind::halo)
      return false;
    std::optional<codir::CodirAccessMode> mode =
        getCodirDepAccessMode(codelet, depIdx);
    if (!mode || *mode != codir::CodirAccessMode::readwrite)
      return false;
    auto memrefType = dyn_cast<MemRefType>(codelet.getDeps()[depIdx].getType());
    if (!memrefType || memrefType.getRank() == 0)
      return false;
    std::optional<SmallVector<int64_t, 4>> blockSizes =
        getCodirTileOwnerBlockSizes(
            codelet, depIdx, static_cast<unsigned>(memrefType.getRank()));
    if (!blockSizes)
      return false;
    std::optional<SmallVector<int64_t, 4>> groupBlockCounts =
        getCodirLogicalOwnerBlockCounts(
            codelet, depIdx, static_cast<unsigned>(memrefType.getRank()),
            *blockSizes);
    return groupBlockCounts &&
           llvm::any_of(*groupBlockCounts,
                        [](int64_t count) { return count > 1; });
  }

  // Movement that derives from committed structure must have the structure it
  // needs. A committed halo collective on a compute-block dep is realized from
  // access-window halo facts; fail closed when those facts are missing instead
  // of silently degrading to coarse storage.
  LogicalResult requireMaterializableMovement(codir::CodeletOp codelet) {
    for (unsigned depIdx = 0, e = codelet.getDeps().size(); depIdx < e;
         ++depIdx) {
      if (codirDepRequiresComputeBlockStorage(codelet, depIdx) &&
          getFinalizedCodirDepCollectiveKind(codelet, depIdx) ==
              codir::CodirCollectiveKind::halo &&
          !codirDepHasHaloWindow(codelet, depIdx))
        return codelet.emitOpError()
               << "dependency #" << depIdx
               << " commits a halo collective but has no access-window halo "
                  "facts to materialize block-native halo storage";
      if (isRankExpandedOwnerStripReadOnlyHaloDep(codelet, depIdx))
        return codelet.emitOpError()
               << "dependency #" << depIdx
               << " commits a rank-expanded owner-strip read-only halo; "
                  "CODIR/ARTS owner-strip RO halo materialization is not yet "
                  "implemented, so this path fails closed instead of "
                  "materializing a partial halo";
      if (isGroupedOwnerStripReadWriteHaloDep(codelet, depIdx))
        return codelet.emitOpError()
               << "grouped owner-compute halo dependency #" << depIdx
               << " requires explicit per-face element_offsets/"
                  "element_sizes; widened whole-block halo acquires are "
                  "forbidden";
    }
    return success();
  }

  bool hasGenericWorkerPlan(codir::CodeletOp codelet) const {
    if (!codelet)
      return false;
    return codelet.getDistributionKindAttr() ||
           codelet.getIterationTopologyAttr() ||
           codelet.getLogicalWorkerSliceAttr() || codelet.getTileShapeAttr();
  }

  bool isSmallReadOnlyCoarseDep(codir::CodeletOp codelet, unsigned depIndex,
                                arts::DbAllocOp alloc) const {
    std::optional<codir::CodirAccessMode> mode =
        getCodirDepAccessMode(codelet, depIndex);
    return mode && *mode == codir::CodirAccessMode::read &&
           arts::DbUtils::isSmallCoarseUserDataDb(alloc);
  }

  bool isReplicatedReadDep(codir::CodeletOp codelet, unsigned depIndex) const {
    std::optional<codir::CodirAccessMode> mode =
        getCodirDepAccessMode(codelet, depIndex);
    std::optional<codir::CodirStorageViewKind> view =
        getCodirDepStorageViewKind(codelet, depIndex);
    return mode && *mode == codir::CodirAccessMode::read && view &&
           *view == codir::CodirStorageViewKind::replicated_read;
  }

  bool hasDistributedLaunchStoragePlan(codir::CodeletOp codelet) const {
    if (!hasGenericWorkerPlan(codelet))
      return false;
    if (codelet.getDeps().empty())
      return true;
    if (!hasCodirTileOwnerSlicePlan(codelet))
      return false;

    for (auto [idx, dep] : llvm::enumerate(codelet.getDeps())) {
      arts::DbAllocOp alloc = findBackingDbAlloc(dep);
      if (isSmallReadOnlyCoarseDep(codelet, static_cast<unsigned>(idx), alloc))
        continue;
      if (isReplicatedReadDep(codelet, static_cast<unsigned>(idx)))
        continue;
      if (!codirDepAllowsComputeBlockStorage(codelet,
                                             static_cast<unsigned>(idx)))
        return false;
      if (!canUseBackingAllocBlockWindowForDep(
              codelet, static_cast<unsigned>(idx), alloc))
        return false;
      if (!codirDepCanUseBlockStorageAccess(codelet,
                                            static_cast<unsigned>(idx)))
        return false;
    }
    return true;
  }

  scf::ForOp findGenericWorkerDispatchLoop(codir::CodeletOp codelet) const {
    scf::ForOp nearestLoop;
    for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
         parent = parent->getParentOp()) {
      auto loop = dyn_cast<scf::ForOp>(parent);
      if (!loop)
        continue;
      if (!nearestLoop)
        nearestLoop = loop;
      if (containsValue(codelet.getParams(), loop.getInductionVar()))
        return loop;
    }
    // Flattened owner-tile dispatch rematerializes owner bases from a block
    // ordinal, so the dispatch IV is not a codelet parameter. It is still the
    // ARTS launch ordinal for the committed tile-owner plan.
    if (hasCodirTileOwnerSlicePlan(codelet))
      return nearestLoop;
    return {};
  }

  Operation *getCompletionBarrierAnchor(codir::CodeletOp codelet,
                                        arts::EdtOp task) {
    Operation *nearestLoop = nullptr;
    Operation *dispatchAnchor = nullptr;
    bool matchedDispatchLoop = false;

    for (Operation *parent = task->getParentOp(); parent;
         parent = parent->getParentOp()) {
      auto loop = dyn_cast<scf::ForOp>(parent);
      if (!loop) {
        if (matchedDispatchLoop)
          break;
        continue;
      }

      if (!nearestLoop)
        nearestLoop = parent;

      if (containsValue(codelet.getParams(), loop.getInductionVar())) {
        dispatchAnchor = parent;
        matchedDispatchLoop = true;
        continue;
      }

      if (matchedDispatchLoop)
        break;
    }

    if (dispatchAnchor)
      return dispatchAnchor;
    if (nearestLoop)
      return nearestLoop;
    return task.getOperation();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    if (failed(rejectResidualSdeOps(module))) {
      signalPassFailure();
      return;
    }

    SmallVector<codir::CodeletOp> codelets;
    module.walk([&](codir::CodeletOp op) { codelets.push_back(op); });
    for (codir::CodeletOp codelet : codelets) {
      if (failed(requireFinalizedPlanningFacts(codelet)) ||
          failed(requireMaterializableMovement(codelet))) {
        signalPassFailure();
        return;
      }
    }

    for (codir::CodeletOp codelet : codelets) {
      for (auto [depIndex, dep] : llvm::enumerate(codelet.getDeps())) {
        unsigned depIdx = static_cast<unsigned>(depIndex);
        if (findBackingDbAlloc(dep)) {
          if (failed(
                  materializeExistingDbComputeBlockIfNeeded(codelet, depIdx))) {
            codelet.emitOpError()
                << "dependency #" << depIdx
                << " requests compute-block storage but cannot be materialized "
                   "from its existing DB view";
            signalPassFailure();
            return;
          }
          if (failed(
                  materializeExistingDbHostBridgeIfNeeded(codelet, depIdx))) {
            codelet.emitOpError()
                << "dependency #" << depIdx
                << " requests compute-block storage but cannot be bridged "
                   "from its host-whole DB view";
            signalPassFailure();
            return;
          }
          continue;
        }
        if (failed(materializeRawCodirDependency(dep, codelet, depIdx))) {
          codelet.emitOpError()
              << "dependency is not backed by SDE/CODIR DB materialization "
                 "and cannot be materialized from a local memref allocation";
          signalPassFailure();
          return;
        }
      }
    }

    for (codir::CodeletOp codelet : codelets) {
      if (failed(lowerCodelet(codelet))) {
        signalPassFailure();
        return;
      }
    }
  }

  LogicalResult lowerCodelet(codir::CodeletOp codelet) {
    Location loc = codelet.getLoc();
    OpBuilder builder(codelet);

    ArrayAttr depModes = codelet.getDepModesAttr();
    if (failed(requireFinalizedPlanningFacts(codelet)))
      return failure();

    SmallVector<Value> taskDeps;
    SmallVector<Type> blockArgTypes;
    SmallVector<unsigned, 4> depTaskArgIndices;
    SmallVector<CodirDepSlice, 4> depSlices;
    SmallVector<PlannedBlockDepAccessPlan, 4> plannedBlockAccessPlans;
    SmallVector<Operation *, 4> depViewCleanup;
    taskDeps.reserve(codelet.getDeps().size());
    blockArgTypes.reserve(codelet.getDeps().size());
    depTaskArgIndices.reserve(codelet.getDeps().size());
    depSlices.reserve(codelet.getDeps().size());
    plannedBlockAccessPlans.reserve(codelet.getDeps().size());

    for (auto [idx, dep] : llvm::enumerate(codelet.getDeps())) {
      if (isCodirViewDep(dep))
        depViewCleanup.push_back(dep.getDefiningOp());

      auto modeAttr = cast<codir::CodirAccessModeAttr>(depModes[idx]);
      arts::DbAllocOp alloc = findBackingDbAlloc(dep);
      if (!alloc)
        return codelet.emitOpError()
               << "dependency #" << idx
               << " is not backed by SDE/CODIR DB materialization";
      unsigned depIdx = static_cast<unsigned>(idx);

      CodirDepSlice slice = getCodirDepSlice(dep, builder, loc);
      std::optional<arts::PartitionMode> partitionMode;
      SmallVector<Value> partitionOffsets;
      SmallVector<Value> partitionSizes;
      if (slice.sliced) {
        partitionMode = arts::PartitionMode::block;
        partitionOffsets.assign(slice.offsets.begin(), slice.offsets.end());
        partitionSizes.assign(slice.sizes.begin(), slice.sizes.end());
      }

      Value zero = createZeroIndex(builder, loc);
      SmallVector<Value> dbOffsets(alloc.getSizes().size(), zero);
      SmallVector<Value> dbSizes(alloc.getSizes().begin(),
                                 alloc.getSizes().end());
      if (dbOffsets.empty()) {
        dbOffsets.push_back(zero);
        dbSizes.push_back(createOneIndex(builder, loc));
      }
      PlannedBlockDepAccessPlan plannedAccess;
      planReadOnlyHostWholeBlockAccess(codelet, depIdx, alloc, builder, loc,
                                       plannedAccess);
      if (plannedAccess.empty())
        planReadOnlyBlockStorageAccessFromBackingAlloc(codelet, depIdx, alloc,
                                                       builder, loc, dbOffsets,
                                                       dbSizes, plannedAccess);
      if (plannedAccess.empty() &&
          codirDepAllowsComputeBlockStorage(codelet, depIdx) &&
          canUseCodirOwnerSliceForAlloc(codelet, depIdx, alloc) &&
          codirDepCanUseBlockStorageAccess(codelet, depIdx) &&
          !codelet.getParams().empty()) {
        std::optional<SmallVector<unsigned, 4>> ownerDims =
            getCodirDepOwnerDims(codelet, depIdx);
        std::optional<SmallVector<int64_t, 4>> blockSizes =
            getCodirTileOwnerBlockSizes(
                codelet, depIdx,
                static_cast<unsigned>(alloc.getElementSizes().size()));
        SmallVector<Value, 4> ownerParams =
            getCodirDepOwnerParamValues(codelet, depIdx);
        if (ownerDims && blockSizes &&
            ownerDims->size() == blockSizes->size() &&
            ownerParams.size() == ownerDims->size() &&
            alloc.getSizes().size() == ownerDims->size()) {
          std::optional<SmallVector<int64_t, 4>> groupBlockCounts =
              getCodirLogicalOwnerBlockCounts(
                  codelet, depIdx,
                  static_cast<unsigned>(alloc.getElementSizes().size()),
                  *blockSizes);
          if (!groupBlockCounts ||
              groupBlockCounts->size() != ownerDims->size())
            return codelet.emitOpError()
                   << "failed to derive logical block window for dependency #"
                   << depIdx;

          bool hasHaloWindow = false;
          for (unsigned ownerDim : *ownerDims) {
            CodirOwnerHaloWindow halo = getCodirBlockStorageHaloWindowForDim(
                codelet, depIdx, ownerDim,
                static_cast<unsigned>(alloc.getElementSizes().size()));
            hasHaloWindow |= !halo.empty();
          }
          bool grouped = llvm::any_of(*groupBlockCounts,
                                      [](int64_t count) { return count > 1; });
          if (grouped && hasHaloWindow &&
              !codirDepAccessesStayWithinSingleOwnerSlice(codelet, depIdx))
            return codelet.emitOpError()
                   << "grouped compute over halo block dependency #" << depIdx
                   << " requires lane-specific halo acquire materialization";

          dbOffsets.clear();
          dbSizes.clear();
          for (auto [slot, ownerDim] : llvm::enumerate(*ownerDims)) {
            Value blockSizeValue =
                createConstantIndex(builder, loc, (*blockSizes)[slot]);
            Value base = ownerParams[slot];
            Value domainBase =
                materializeCodirOwnerDomainBase(builder, loc, codelet, base);
            Value relativeBase =
                ::mlir::carts::ValueAnalysis::sameValue(base, domainBase)
                    ? createZeroIndex(builder, loc)
                    : arith::SubIOp::create(builder, loc, base, domainBase)
                          .getResult();
            Value blockIndex = arith::DivUIOp::create(
                builder, loc, relativeBase, blockSizeValue);
            int64_t groupBlockCount = (*groupBlockCounts)[slot];
            int64_t baseAlignment =
                getKnownAlignmentWithinBlock(relativeBase, (*blockSizes)[slot]);
            std::optional<int64_t> ownerWindowExtent =
                getGuaranteedOwnerWindowExtent((*blockSizes)[slot],
                                               groupBlockCount, baseAlignment);
            if (!ownerWindowExtent)
              return codelet.emitOpError()
                     << "failed to prove owner window coverage for dependency #"
                     << depIdx;
            Value ownedBlockCount;
            if (groupBlockCount <= 1) {
              ownedBlockCount = createOneIndex(builder, loc);
            } else {
              Value requestedBlocks =
                  createConstantIndex(builder, loc, groupBlockCount);
              Value remainingBlocks = arith::SubIOp::create(
                  builder, loc, alloc.getSizes()[slot], blockIndex);
              ownedBlockCount = arith::MinUIOp::create(
                  builder, loc, remainingBlocks, requestedBlocks);
            }
            Value acquireBlockIndex = blockIndex;
            Value acquireBlockCount = ownedBlockCount;
            dbOffsets.push_back(acquireBlockIndex);
            dbSizes.push_back(acquireBlockCount);
            plannedAccess.ownerDims.push_back(ownerDim);
            plannedAccess.blockSizes.push_back((*blockSizes)[slot]);
            plannedAccess.ownerParams.push_back(ownerParams[slot]);
            plannedAccess.ownerDomainBases.push_back(domainBase);
            plannedAccess.acquiredElementBases.push_back(base);
            plannedAccess.groupBlockCounts.push_back(groupBlockCount);
            plannedAccess.ownerWindowExtents.push_back(*ownerWindowExtent);
            plannedAccess.lowerHaloBlockCounts.push_back(0);
            plannedAccess.upperHaloBlockCounts.push_back(0);
            plannedAccess.allowFullWindowAccesses.push_back(false);
            plannedAccess.requireOwnerWindowProofs.push_back(false);
          }
          plannedAccess.grouped = grouped;
        }
      }
      if (plannedAccess.empty() && isReplicatedReadDep(codelet, depIdx))
        planReplicatedReadFullBlockAccess(codelet, depIdx, alloc, builder, loc,
                                          plannedAccess);
      unsigned primaryTaskDepIndex = static_cast<unsigned>(taskDeps.size());
      auto acquire = arts::DbAcquireOp::create(
          builder, loc, convertAccessMode(modeAttr.getValue()), alloc.getGuid(),
          alloc.getPtr(), partitionMode,
          /*indices=*/SmallVector<Value>{}, std::move(dbOffsets),
          std::move(dbSizes),
          /*partitionIndices=*/SmallVector<Value>{},
          std::move(partitionOffsets), std::move(partitionSizes),
          /*boundsValid=*/Value{},
          /*elementOffsets=*/SmallVector<Value>{},
          /*elementSizes=*/SmallVector<Value>{});
      acquire.setPreserveAccessMode();
      if (isReplicatedReadDep(codelet, depIdx))
        acquire.setReplicatedReadAttr(UnitAttr::get(codelet.getContext()));
      taskDeps.push_back(acquire.getPtr());
      blockArgTypes.push_back(acquire.getPtr().getType());
      depTaskArgIndices.push_back(primaryTaskDepIndex);
      depSlices.push_back(std::move(slice));
      plannedBlockAccessPlans.push_back(std::move(plannedAccess));
    }

    SmallVector<Value> taskParams(codelet.getParams().begin(),
                                  codelet.getParams().end());
    SmallVector<Value> codeletDeps(codelet.getDeps().begin(),
                                   codelet.getDeps().end());
    appendDynamicCodirDepSliceParams(codeletDeps, taskParams);
    for (Value dep : codelet.getDeps()) {
      arts::DbAllocOp alloc = findBackingDbAlloc(dep);
      if (!alloc)
        continue;
      for (Value elementSize : alloc.getElementSizes()) {
        if (!isCodirScalarParamType(elementSize.getType()) ||
            ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(elementSize) ||
            containsValue(taskParams, elementSize))
          continue;
        taskParams.push_back(elementSize);
      }
    }
    for (const PlannedBlockDepAccessPlan &accessPlan :
         plannedBlockAccessPlans) {
      for (Value domainBase : accessPlan.ownerDomainBases) {
        if (!domainBase || !isCodirScalarParamType(domainBase.getType()) ||
            ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(domainBase) ||
            containsValue(taskParams, domainBase))
          continue;
        taskParams.push_back(domainBase);
      }
    }
    // CODIR carries only generic worker-plan facts. The ARTS boundary is the
    // first place where runtime topology can turn that plan into inter-node
    // EDT placement and routing.
    arts::ArtsLaunchPolicy launch = arts::resolveArtsLaunchPolicy(
        codelet->getParentOfType<ModuleOp>(),
        findGenericWorkerDispatchLoop(codelet),
        hasDistributedLaunchStoragePlan(codelet), builder, loc);
    auto task =
        launch.route
            ? arts::EdtOp::create(builder, loc, arts::EdtType::task,
                                  launch.concurrency, launch.route, taskDeps,
                                  taskParams)
            : arts::EdtOp::create(builder, loc, arts::EdtType::task,
                                  launch.concurrency, taskDeps, taskParams);
    forwardCommittedEdtPlan(codelet, task);
    bool isTaskDepend = static_cast<bool>(codelet.getTaskDependAttr());
    bool requiresOrderedDependBarrier =
        static_cast<bool>(codelet.getOrderedTaskDependAttr());
    bool requiresCompletionBarrier =
        static_cast<bool>(codelet.getCompletionBarrierAttr());
    Block &taskBlock = task.getBody().front();
    for (Type type : blockArgTypes)
      taskBlock.addArgument(type, loc);
    unsigned firstParamArg = blockArgTypes.size();
    DenseMap<Value, Value> paramBlockArgs;
    DenseMap<Value, Value> sourceByBlockArgument;
    for (auto [idx, param] : llvm::enumerate(taskParams)) {
      Value arg = taskBlock.addArgument(param.getType(), loc);
      paramBlockArgs.try_emplace(param, arg);
      sourceByBlockArgument.try_emplace(arg, param);
    }

    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToStart(&taskBlock);

    IRMapping mapper;
    Block &codeletBlock = codelet.getBody().front();
    unsigned numDeps = codelet.getDeps().size();
    SmallVector<PlannedBlockLocalAccessRewrite, 4> localAccessRewrites;
    for (unsigned idx = 0; idx < numDeps; ++idx) {
      if (idx >= depTaskArgIndices.size() ||
          depTaskArgIndices[idx] >= taskBlock.getNumArguments())
        return codelet.emitOpError()
               << "failed to materialize task dependency argument for "
                  "codelet dependency #"
               << idx;
      Value payload = materializeInnerPayload(
          builder, loc, taskBlock.getArgument(depTaskArgIndices[idx]));
      const CodirDepSlice &slice = depSlices[idx];
      if (slice.sliced) {
        auto depType = cast<MemRefType>(codelet.getDeps()[idx].getType());
        if (slice.subindex) {
          Value subindex = slice.subindexIndex;
          auto it = paramBlockArgs.find(subindex);
          if (it != paramBlockArgs.end())
            subindex = it->second;
          else if (std::optional<int64_t> constant =
                       ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(
                           subindex))
            subindex = createConstantIndex(builder, loc, *constant);
          payload = polygeist::SubIndexOp::create(builder, loc, depType,
                                                  payload, subindex);
        } else {
          SmallVector<OpFoldResult> offsets = remapIndexFoldResults(
              builder, loc, slice.mixedOffsets, paramBlockArgs);
          SmallVector<OpFoldResult> sizes = remapIndexFoldResults(
              builder, loc, slice.mixedSizes, paramBlockArgs);
          SmallVector<OpFoldResult> strides = remapIndexFoldResults(
              builder, loc, slice.mixedStrides, paramBlockArgs);
          auto resultType = memref::SubViewOp::inferResultType(
              cast<MemRefType>(payload.getType()), offsets, sizes, strides);
          payload = memref::SubViewOp::create(builder, loc, resultType, payload,
                                              offsets, sizes, strides);
        }
      } else if (!plannedBlockAccessPlans[idx].empty()) {
        auto payloadType = dyn_cast<MemRefType>(payload.getType());
        if (!payloadType)
          return codelet.emitOpError()
                 << "planned block-local dependency payload is not a memref";
        const PlannedBlockDepAccessPlan &accessPlan =
            plannedBlockAccessPlans[idx];
        if (accessPlan.ownerParams.size() != accessPlan.ownerDims.size() ||
            accessPlan.blockSizes.size() != accessPlan.ownerDims.size() ||
            accessPlan.ownerDomainBases.size() != accessPlan.ownerDims.size() ||
            accessPlan.groupBlockCounts.size() != accessPlan.ownerDims.size() ||
            accessPlan.ownerWindowExtents.size() !=
                accessPlan.ownerDims.size() ||
            accessPlan.allowFullWindowAccesses.size() !=
                accessPlan.ownerDims.size() ||
            accessPlan.requireOwnerWindowProofs.size() !=
                accessPlan.ownerDims.size())
          return codelet.emitOpError()
                 << "failed to materialize owner-base parameters for planned "
                    "block-local access rewrite";
        arts::DbAllocOp blockAlloc = findBackingDbAlloc(codelet.getDeps()[idx]);
        Value groupedReadSource = taskBlock.getArgument(depTaskArgIndices[idx]);
        for (auto [slot, ownerDim] : llvm::enumerate(accessPlan.ownerDims)) {
          Value ownerBase;
          if (Value ownerParam = accessPlan.ownerParams[slot])
            ownerBase = paramBlockArgs.lookup(ownerParam);
          else
            ownerBase = createZeroIndex(builder, loc);
          if (!ownerBase)
            return codelet.emitOpError()
                   << "failed to materialize owner-base parameter for planned "
                      "block-local access rewrite";
          Value ownerDomainBase = accessPlan.ownerDomainBases[slot];
          if (std::optional<int64_t> folded =
                  ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(
                      ownerDomainBase)) {
            ownerDomainBase = createConstantIndex(builder, loc, *folded);
          } else if (Value mappedDomainBase =
                         paramBlockArgs.lookup(ownerDomainBase)) {
            ownerDomainBase = mappedDomainBase;
          } else {
            return codelet.emitOpError()
                   << "failed to materialize owner-domain base parameter for "
                      "planned block-local access rewrite";
          }
          Value sourceOwnerParam = accessPlan.ownerParams[slot];
          Value sourceDomainBase = accessPlan.ownerDomainBases[slot];
          bool alignedOwnerBase =
              sourceOwnerParam && sourceDomainBase &&
              ::mlir::carts::ValueAnalysis::isZeroConstant(sourceDomainBase) &&
              isKnownMultipleOfBlock(sourceOwnerParam,
                                     accessPlan.blockSizes[slot]);
          Value localOrigin =
              alignedOwnerBase
                  ? ownerBase
                  : materializeBlockLocalOrigin(builder, loc, ownerBase,
                                                ownerDomainBase,
                                                accessPlan.blockSizes[slot]);
          CodirOwnerHaloWindow ownerHalo = getCodirBlockStorageHaloWindowForDim(
              codelet, idx, ownerDim,
              static_cast<unsigned>(payloadType.getRank()));
          // Keep producer stores aligned with the DB's storage halo.
          if (ownerHalo.lower <= 0 || ownerHalo.upper <= 0) {
            CodirOwnerHaloWindow allocHalo =
                blockAllocStorageHaloForDim(blockAlloc, ownerDim);
            if (allocHalo.lower > 0)
              ownerHalo.lower = allocHalo.lower;
            if (ownerHalo.upper <= 0 && allocHalo.upper > 0)
              ownerHalo.upper = allocHalo.upper;
          }
          int64_t sourceDimExtent = ShapedType::kDynamic;
          if (MemRefType depType = getCodeletDepSourceType(codelet, idx))
            if (ownerDim < static_cast<unsigned>(depType.getRank()))
              sourceDimExtent = depType.getDimSize(ownerDim);
          localAccessRewrites.push_back(
              {payload, ownerDim, ownerBase, localOrigin, ownerHalo.lower,
               ownerHalo.upper, groupedReadSource, static_cast<unsigned>(slot),
               accessPlan.blockSizes[slot], accessPlan.groupBlockCounts[slot],
               accessPlan.ownerWindowExtents[slot], sourceDimExtent,
               accessPlan.grouped, accessPlan.allowFullWindowAccesses[slot],
               accessPlan.requireOwnerWindowProofs[slot]});
        }
      }
      mapper.map(codeletBlock.getArgument(idx), payload);
    }

    for (auto [idx, param] : llvm::enumerate(codelet.getParams()))
      mapper.map(codeletBlock.getArgument(numDeps + idx),
                 taskBlock.getArgument(firstParamArg + idx));
    for (Operation &nested : codeletBlock.without_terminator())
      builder.insert(nested.clone(mapper));

    translateCodirAtomicsToArts(task.getBody());

    if (failed(rewritePlannedBlockLocalAccesses(task, localAccessRewrites,
                                                &sourceByBlockArgument)))
      return codelet.emitOpError()
             << "failed to rewrite planned block dependency accesses to "
                "block-local indices";

    arts::YieldOp::create(builder, loc);

    Operation *barrierAnchor = nullptr;
    if (requiresOrderedDependBarrier) {
      OpBuilder barrierBuilder(task);
      barrierBuilder.setInsertionPointAfter(task);
      auto reason = arts::ArtsBarrierReasonAttr::get(
          codelet.getContext(), arts::ArtsBarrierReason::required_memory);
      arts::BarrierOp::create(barrierBuilder, loc, reason);
    } else if (isTaskDepend || requiresCompletionBarrier) {
      barrierAnchor = getCompletionBarrierAnchor(codelet, task);

      if (barrierAnchor == task.getOperation() ||
          loopCompletionBarriers.insert(barrierAnchor).second) {
        OpBuilder barrierBuilder(barrierAnchor);
        barrierBuilder.setInsertionPointAfter(barrierAnchor);
        auto reason = arts::ArtsBarrierReasonAttr::get(
            codelet.getContext(), arts::ArtsBarrierReason::required_memory);
        arts::BarrierOp::create(barrierBuilder, loc, reason);
      }
    }

    codelet.erase();
    for (Operation *view : depViewCleanup)
      if (view && view->use_empty())
        view->erase();
    return success();
  }
};
} // namespace
std::unique_ptr<Pass> mlir::carts::codir::createConvertCodirToArtsPass() {
  return std::make_unique<ConvertCodirToArtsPass>();
}
