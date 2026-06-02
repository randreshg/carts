///==========================================================================///
/// File: DistributionPlanning.cpp
///
/// SDE distribution planning. This pass keeps distribution intent on the SDE
/// side of the boundary by wrapping eligible `sde.su_iterate` operations in
/// `sde.su_distribute`; it uses SDE pattern/effect facts plus abstract worker
/// capacity/locality. Concrete storage ownership, task placement, routes, and
/// target memory-model choices remain boundary decisions.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_DISTRIBUTIONPLANNING
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/StructuredOpAnalysis.h"
#include "carts/dialect/sde/Utils/CuMuGraphPartitioning.h"
#include "carts/dialect/sde/Utils/IterationSizingUtils.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/dialect/sde/Utils/SdePlanUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/LoopUtils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/MathExtras.h"

#include <algorithm>
#include <limits>

using namespace mlir;
using namespace mlir::carts;

namespace {

struct PlannedDistribution {
  sde::SdeSuIterateOp op;
  sde::SdeDistributionKind kind = sde::SdeDistributionKind::blocked;
};

static int64_t saturatingMultiplyPositive(int64_t lhs, int64_t rhs);
static void
alignLateOwnerPlanToExistingStep(sde::SdeSuIterateOp op,
                                 ArrayRef<int64_t> ownerPhysicalDims,
                                 MutableArrayRef<int64_t> physicalBlockShape);
static bool applyPhysicalPlanIfRealized(sde::SdeSuIterateOp op,
                                        ArrayRef<int64_t> ownerDims,
                                        ArrayRef<int64_t> physicalBlockShape,
                                        ArrayRef<int64_t> haloShape = {});
static bool
physicalPlanMatchesRealizedLoopSteps(sde::SdeSuIterateOp op,
                                     ArrayRef<int64_t> ownerDims,
                                     ArrayRef<int64_t> physicalBlockShape);

static int64_t getInterLocalityTargetWorkers(sde::SDECostModel &costModel) {
  return saturatingMultiplyPositive(costModel.getLogicalWorkerCapacity(),
                                    costModel.getInterLocalityTaskWaves());
}

// Worker target for the stencil physical-plan stamper. Caps at logical
// worker capacity because stencil halo ownership is currently planned as a
// single-locality layout fact. Inter-locality expansion belongs after the
// dialect boundary has a concrete halo-realization path.
static int64_t getStencilWorkerTarget(sde::SDECostModel &costModel) {
  return costModel.getLogicalWorkerCapacity();
}

// Element-byte width derived from the output plan's underlying memref. Returns
// 0 for non-numeric types or shapeless roots; callers should treat 0 as
// "cannot reason about tile bytes" and skip the floor.
static int64_t outputElementBytes(Value root) {
  auto memrefTy = dyn_cast_or_null<MemRefType>(root.getType());
  if (!memrefTy)
    return 0;
  Type elt = memrefTy.getElementType();
  while (auto nested = dyn_cast<MemRefType>(elt))
    elt = nested.getElementType();
  if (!elt.isIntOrFloat())
    return 0;
  return llvm::divideCeil(elt.getIntOrFloatBitWidth(), 8);
}

struct StaticOutputStoragePlan {
  Value root;
  SmallVector<int64_t, 4> shape;
};

static std::optional<StaticOutputStoragePlan>
findSingleExternalStoreShape(sde::SdeSuIterateOp op) {
  if (!op)
    return std::nullopt;

  Block *computeBlock = sde::getSuIterateComputeBlock(op);
  if (!computeBlock)
    return std::nullopt;

  std::optional<StaticOutputStoragePlan> selected;
  bool rejected = false;
  auto visitStore = [&](memref::StoreOp storeOp) {
    Value root =
        ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storeOp.getMemref());
    if (!root || sde::isDefinedInside(op.getOperation(), root))
      return WalkResult::advance();

    auto memrefTy = dyn_cast<MemRefType>(root.getType());
    if (!memrefTy || memrefTy.getRank() == 0) {
      rejected = true;
      return WalkResult::interrupt();
    }

    SmallVector<int64_t, 4> shape;
    shape.reserve(memrefTy.getRank());
    for (int64_t dim : memrefTy.getShape()) {
      if (dim == ShapedType::kDynamic) {
        rejected = true;
        return WalkResult::interrupt();
      }
      shape.push_back(dim);
    }

    if (!selected) {
      selected = StaticOutputStoragePlan{root, std::move(shape)};
      return WalkResult::advance();
    }

    if (selected->root != root || selected->shape != shape) {
      rejected = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  };

  for (Operation &nested : computeBlock->without_terminator())
    if (nested.walk(visitStore).wasInterrupted())
      break;

  if (rejected)
    return std::nullopt;
  return selected;
}

static int64_t readStencilHaloForOwnerDim(sde::SdeSuIterateOp op,
                                          unsigned ownerDim) {
  auto minOffsets = readI64ArrayAttr(op.getAccessMinOffsetsAttr());
  auto maxOffsets = readI64ArrayAttr(op.getAccessMaxOffsetsAttr());
  auto ownerDims = readI64ArrayAttr(op.getOwnerDimsAttr());
  if (!minOffsets || !maxOffsets || !ownerDims)
    return 0;

  for (auto [idx, rawDim] : llvm::enumerate(*ownerDims)) {
    if (rawDim < 0 || static_cast<unsigned>(rawDim) != ownerDim)
      continue;
    if (idx >= minOffsets->size() || idx >= maxOffsets->size())
      return 0;
    return std::max<int64_t>(0,
                             std::max(-(*minOffsets)[idx], (*maxOffsets)[idx]));
  }
  return 0;
}

static bool isInPlaceSelfReadStencil(sde::SdeSuIterateOp op) {
  auto classification = op.getStructuredClassification();
  if (!classification ||
      *classification != sde::SdeStructuredClassification::stencil)
    return false;

  auto effects = sde::collectStructuredMemoryEffects(op.getBody());
  return !effects.hasUnknownEffects && sde::hasInPlaceSelfRead(effects);
}

static SmallVector<unsigned, 4>
chooseMappedSdeOwnerLoopDims(sde::SdeSuIterateOp op,
                             const sde::StructuredOutputLayoutPlan &plan) {
  SmallVector<unsigned, 4> mappedLoopDims;
  mappedLoopDims.reserve(plan.loopDimToPhysicalDim.size());
  unsigned loopRank = op.getLowerBounds().size();
  for (unsigned loopDim = 0, e = plan.loopDimToPhysicalDim.size(); loopDim < e;
       ++loopDim) {
    if (loopDim >= loopRank || loopDim >= plan.loopDimToPhysicalDim.size())
      break;
    int64_t physicalDim = plan.loopDimToPhysicalDim[loopDim];
    if (physicalDim < 0 ||
        static_cast<size_t>(physicalDim) >= plan.shape.size())
      continue;
    mappedLoopDims.push_back(loopDim);
  }
  if (mappedLoopDims.empty())
    return {};

  SmallVector<unsigned, 4> haloLoopDims;
  for (unsigned loopDim : mappedLoopDims)
    if (readStencilHaloForOwnerDim(op, loopDim) > 0)
      haloLoopDims.push_back(loopDim);
  if (!haloLoopDims.empty())
    return haloLoopDims;

  return mappedLoopDims;
}

static bool buildOwnerDimPlan(sde::SdeSuIterateOp op,
                              const sde::StructuredOutputLayoutPlan &outputPlan,
                              ArrayRef<unsigned> ownerLoopDims, int64_t workers,
                              SmallVectorImpl<int64_t> &ownerPhysicalDims,
                              SmallVectorImpl<int64_t> &physicalBlockShape,
                              SmallVectorImpl<int64_t> &haloShape) {
  if (ownerLoopDims.empty() || outputPlan.shape.empty())
    return false;

  physicalBlockShape.assign(outputPlan.shape.begin(), outputPlan.shape.end());

  SmallVector<int64_t, 4> ownerExtents;
  ownerPhysicalDims.clear();
  haloShape.clear();
  for (unsigned loopDim : ownerLoopDims) {
    if (loopDim >= outputPlan.loopDimToPhysicalDim.size())
      return false;
    int64_t physicalDim = outputPlan.loopDimToPhysicalDim[loopDim];
    if (physicalDim < 0 ||
        static_cast<size_t>(physicalDim) >= outputPlan.shape.size())
      return false;
    if (outputPlan.shape[physicalDim] <= 0)
      return false;

    ownerPhysicalDims.push_back(physicalDim);
    ownerExtents.push_back(outputPlan.shape[physicalDim]);
    haloShape.push_back(
        std::max<int64_t>(0, readStencilHaloForOwnerDim(op, loopDim)));
  }

  SmallVector<int64_t, 4> workerGrid = sde::factorStencilWorkersAcrossDims(
      std::max<int64_t>(1, workers), ownerExtents, haloShape);
  for (auto [idx, physicalDim] : llvm::enumerate(ownerPhysicalDims)) {
    physicalBlockShape[physicalDim] =
        sde::ceilDivPositive(outputPlan.shape[physicalDim], workerGrid[idx]);
  }

  return true;
}

static bool buildOwnerDimPlan(const sde::LoopIndexedOutputPlan &outputPlan,
                              int64_t workers,
                              SmallVectorImpl<int64_t> &ownerPhysicalDims,
                              SmallVectorImpl<int64_t> &physicalBlockShape) {
  if (outputPlan.ownerPhysicalDims.empty() || outputPlan.shape.empty())
    return false;

  physicalBlockShape.assign(outputPlan.shape.begin(), outputPlan.shape.end());
  ownerPhysicalDims.assign(outputPlan.ownerPhysicalDims.begin(),
                           outputPlan.ownerPhysicalDims.end());

  SmallVector<int64_t, 4> ownerExtents;
  ownerExtents.reserve(ownerPhysicalDims.size());
  for (int64_t physicalDim : ownerPhysicalDims) {
    if (physicalDim < 0 ||
        static_cast<size_t>(physicalDim) >= outputPlan.shape.size())
      return false;
    if (outputPlan.shape[physicalDim] <= 0)
      return false;
    ownerExtents.push_back(outputPlan.shape[physicalDim]);
  }

  SmallVector<int64_t, 4> workerGrid =
      sde::factorWorkersAcrossDims(std::max<int64_t>(1, workers), ownerExtents);
  for (auto [idx, physicalDim] : llvm::enumerate(ownerPhysicalDims)) {
    physicalBlockShape[physicalDim] =
        sde::ceilDivPositive(outputPlan.shape[physicalDim], workerGrid[idx]);
  }

  return true;
}

static int64_t getDistributedTileParallelismFloor(sde::SDECostModel &costModel,
                                                  int64_t workers) {
  return std::clamp<int64_t>(costModel.getLogicalWorkerCapacity(), int64_t{1},
                             std::max<int64_t>(1, workers));
}

static int64_t readAbstractCommVolumeBytes(sde::SdeSuIterateOp op) {
  if (auto attr = op.getCommVolumeBytesAttr())
    return std::max<int64_t>(0, attr.getInt());
  return 0;
}

static int64_t chooseNeutralCuGroupSize(int64_t cuCount, int64_t tileBytes,
                                        int64_t targetTileBytes,
                                        int64_t targetWorkers) {
  cuCount = std::max<int64_t>(1, cuCount);
  if (cuCount <= 1 || tileBytes <= 0 || targetTileBytes <= 0 ||
      tileBytes >= targetTileBytes)
    return 1;

  int64_t desired =
      sde::ceilDivPositive(targetTileBytes, std::max<int64_t>(1, tileBytes));
  int64_t targetTasks = std::clamp<int64_t>(targetWorkers, int64_t{1},
                                            std::max<int64_t>(1, cuCount));
  int64_t maxGroupForConcurrency = cuCount / std::max<int64_t>(1, targetTasks);
  desired = std::clamp<int64_t>(desired, int64_t{1},
                                std::max<int64_t>(1, maxGroupForConcurrency));

  for (int64_t group = desired; group > 1; --group)
    if (cuCount % group == 0)
      return group;
  return 1;
}

static SmallVector<sde::CuMuHyperedgePressure, 4>
collectAbstractMuHyperedges(sde::SdeSuIterateOp op) {
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return {};

  SmallVector<sde::LayoutGraphFact, 4> facts =
      sde::parseArrayLayoutFacts(layout);
  return sde::collectCuMuHyperedgePressures(facts);
}

static SmallVector<int64_t, 16>
buildOwnerBlockWorkWeights(ArrayRef<int64_t> shape,
                           ArrayRef<int64_t> ownerPhysicalDims,
                           ArrayRef<int64_t> physicalBlockShape) {
  SmallVector<int64_t, 16> weights;
  if (shape.empty() || ownerPhysicalDims.empty() ||
      shape.size() != physicalBlockShape.size())
    return weights;

  SmallVector<int64_t, 4> ownerExtents;
  SmallVector<int64_t, 4> ownerBlocks;
  int64_t totalBlocks = 1;
  for (int64_t rawDim : ownerPhysicalDims) {
    if (rawDim < 0 || static_cast<size_t>(rawDim) >= shape.size())
      return {};
    int64_t extent = shape[rawDim];
    int64_t block = physicalBlockShape[rawDim];
    if (extent <= 0 || block <= 0)
      return {};
    int64_t blocks = sde::ceilDivPositive(extent, block);
    if (totalBlocks > 4096 / blocks)
      return {};
    totalBlocks *= blocks;
    ownerExtents.push_back(extent);
    ownerBlocks.push_back(block);
  }

  weights.reserve(totalBlocks);
  for (int64_t linear = 0; linear < totalBlocks; ++linear) {
    int64_t tmp = linear;
    int64_t work = 1;
    for (auto [idx, block] : llvm::enumerate(ownerBlocks)) {
      int64_t blockCount = sde::ceilDivPositive(ownerExtents[idx], block);
      int64_t coord = tmp % blockCount;
      tmp /= blockCount;
      int64_t begin = coord * block;
      work = saturatingMultiplyPositive(
          work, std::clamp(ownerExtents[idx] - begin, int64_t{1}, block));
    }
    weights.push_back(work);
  }
  return weights;
}

static sde::CuMuComputeUnitTarget
buildCuMuComputeTarget(sde::SDECostModel &costModel, int64_t workers,
                       ArrayRef<int64_t> cuWorkWeights = {}) {
  sde::CuMuComputeUnitTarget target;
  target.requestedComputeUnits = std::max<int64_t>(1, workers);
  target.minComputeUnits =
      getDistributedTileParallelismFloor(costModel, workers);
  target.logicalWorkerCapacity =
      std::max<int64_t>(1, costModel.getLogicalWorkerCapacity());
  target.taskCreationCost = costModel.getTaskCreationCost();
  target.taskSyncCost = costModel.getTaskSyncCost();
  target.dataAccessCost = costModel.getDataAccessCost();
  target.cuWorkWeights = cuWorkWeights;
  return target;
}

static std::optional<sde::CuMuPartitionPlan> chooseCuMuTileFloorPlan(
    ArrayRef<int64_t> shape, ArrayRef<int64_t> ownerPhysicalDims,
    int64_t elemBytes, int64_t abstractCommVolumeBytes,
    sde::SDECostModel &costModel, int64_t workers,
    ArrayRef<int64_t> physicalBlockShape,
    ArrayRef<sde::CuMuHyperedgePressure> hyperedges,
    llvm::function_ref<bool(int64_t, SmallVectorImpl<int64_t> &)> rebuild) {
  int64_t minTileBytes = costModel.getMinDistributedTileBytes();
  if (minTileBytes <= 0 || elemBytes <= 0 || workers <= 1)
    return std::nullopt;

  sde::CuMuMemoryUnit memory;
  memory.shape = shape;
  memory.ownerPhysicalDims = ownerPhysicalDims;
  memory.elementBytes = elemBytes;
  memory.abstractCommVolumeBytes = abstractCommVolumeBytes;
  memory.hyperedges = hyperedges;

  sde::CuMuPartitionObjective objective;
  objective.targetTileBytes = minTileBytes;
  SmallVector<int64_t, 16> cuWorkWeights =
      buildOwnerBlockWorkWeights(shape, ownerPhysicalDims, physicalBlockShape);
  return sde::chooseCuMuGraphPartition(
      memory, buildCuMuComputeTarget(costModel, workers, cuWorkWeights),
      objective, physicalBlockShape, rebuild);
}

static void stampCuMuPartitionGraphAttrs(sde::SdeSuIterateOp op,
                                         sde::SDECostModel &costModel) {
  if (sde::hasCommittedCuMuPartitionEvidence(op.getOperation()))
    return;

  auto blockShape = readI64ArrayAttr(op.getPhysicalBlockShapeAttr());
  auto ownerDims = readI64ArrayAttr(op.getPhysicalOwnerDimsAttr());
  if (!blockShape || blockShape->empty() || !ownerDims || ownerDims->empty())
    return;
  if (!physicalPlanMatchesRealizedLoopSteps(op, *ownerDims, *blockShape))
    return;

  std::optional<sde::LoopIndexedOutputPlan> outputPlan =
      sde::findConsistentLoopIndexedOutputPlanWithOwnerDims(op);
  if (!outputPlan)
    outputPlan = sde::findLoopIndexedOutputPlan(op);
  Value outputRoot;
  SmallVector<int64_t, 4> outputShape;
  if (outputPlan) {
    outputRoot = outputPlan->root;
    outputShape.assign(outputPlan->shape.begin(), outputPlan->shape.end());
  } else if (std::optional<sde::StructuredOutputLayoutPlan> structuredPlan =
                 sde::findCompatibleOutputLayoutPlan(op)) {
    outputRoot = structuredPlan->root;
    outputShape.assign(structuredPlan->shape.begin(),
                       structuredPlan->shape.end());
  } else if (std::optional<StaticOutputStoragePlan> storePlan =
                 findSingleExternalStoreShape(op)) {
    outputRoot = storePlan->root;
    outputShape.assign(storePlan->shape.begin(), storePlan->shape.end());
  }
  if (!outputRoot || outputShape.empty() ||
      outputShape.size() != blockShape->size())
    return;

  int64_t elemBytes = outputElementBytes(outputRoot);
  if (elemBytes <= 0)
    return;

  MLIRContext *ctx = op.getContext();
  Builder builder(ctx);
  auto i64Ty = builder.getIntegerType(64);
  auto i64Attr = [&](int64_t value) { return IntegerAttr::get(i64Ty, value); };
  auto readI64Array =
      [](ArrayAttr attr) -> std::optional<SmallVector<int64_t, 4>> {
    return readI64ArrayAttr(attr);
  };

  int64_t tileBytes = sde::tilePayloadBytes(*blockShape, elemBytes);
  int64_t cuCount =
      sde::inferCuCountFromMuPartition(outputShape, *ownerDims, *blockShape);
  int64_t targetWorkers =
      std::max<int64_t>(1, costModel.getLogicalWorkerCapacity());
  int64_t exposedCuCount =
      std::min<int64_t>(std::max<int64_t>(1, cuCount), targetWorkers);
  int64_t commBytes = readAbstractCommVolumeBytes(op);
  int64_t cuGroupSize = chooseNeutralCuGroupSize(
      std::max<int64_t>(1, cuCount), tileBytes,
      costModel.getMinDistributedTileBytes(), targetWorkers);
  int64_t cuGroupCount =
      sde::ceilDivPositive(std::max<int64_t>(1, cuCount), cuGroupSize);
  int64_t outputMuBlockCount = std::max<int64_t>(1, cuCount);
  int64_t scoreMuBlockCount = outputMuBlockCount;
  if (ArrayAttr layout = op.getArrayLayoutAttr()) {
    for (Attribute attr : layout) {
      auto dict = dyn_cast<DictionaryAttr>(attr);
      if (!dict)
        continue;
      auto blocks = dyn_cast_or_null<IntegerAttr>(
          dict.get(sde::AttrNames::LayoutGraph::MuBlockCount));
      if (blocks && blocks.getInt() > 0)
        scoreMuBlockCount =
            std::max<int64_t>(scoreMuBlockCount, blocks.getInt());
    }
  }

  SmallVector<NamedAttribute, 8> scoreAttrs;
  scoreAttrs.push_back(builder.getNamedAttr(
      sde::AttrNames::PartitionScoreKeys::Objective,
      builder.getStringAttr(sde::AttrNames::PartitionGraphValues::
                                ObjectiveMaxConcurrencyCommAware)));
  scoreAttrs.push_back(builder.getNamedAttr(
      sde::AttrNames::PartitionScoreKeys::TargetLogicalWorkers,
      i64Attr(targetWorkers)));
  scoreAttrs.push_back(
      builder.getNamedAttr(sde::AttrNames::PartitionScoreKeys::ExposedCuCount,
                           i64Attr(exposedCuCount)));
  scoreAttrs.push_back(builder.getNamedAttr(
      sde::AttrNames::PartitionScoreKeys::RequestedCuCount,
      i64Attr(std::max<int64_t>(1, getInterLocalityTargetWorkers(costModel)))));
  scoreAttrs.push_back(
      builder.getNamedAttr(sde::AttrNames::PartitionScoreKeys::ChosenCuCount,
                           i64Attr(std::max<int64_t>(1, cuCount))));
  scoreAttrs.push_back(
      builder.getNamedAttr(sde::AttrNames::PartitionScoreKeys::MuBlockCount,
                           i64Attr(scoreMuBlockCount)));
  scoreAttrs.push_back(builder.getNamedAttr(
      sde::AttrNames::PartitionScoreKeys::CuGroupSize, i64Attr(cuGroupSize)));
  scoreAttrs.push_back(builder.getNamedAttr(
      sde::AttrNames::PartitionScoreKeys::CuGroupCount, i64Attr(cuGroupCount)));
  scoreAttrs.push_back(
      builder.getNamedAttr(sde::AttrNames::PartitionScoreKeys::MinTileBytes,
                           i64Attr(costModel.getMinDistributedTileBytes())));
  scoreAttrs.push_back(builder.getNamedAttr(
      sde::AttrNames::PartitionScoreKeys::ChosenTileBytes, i64Attr(tileBytes)));
  scoreAttrs.push_back(builder.getNamedAttr(
      sde::AttrNames::PartitionScoreKeys::CommVolumeBytes, i64Attr(commBytes)));
  scoreAttrs.push_back(
      builder.getNamedAttr(sde::AttrNames::PartitionScoreKeys::OwnerDims,
                           buildI64ArrayAttr(ctx, *ownerDims)));
  scoreAttrs.push_back(
      builder.getNamedAttr(sde::AttrNames::PartitionScoreKeys::BlockShape,
                           buildI64ArrayAttr(ctx, *blockShape)));
  op->setAttr(sde::AttrNames::PartitionScore,
              DictionaryAttr::get(ctx, scoreAttrs));

  SmallVector<Attribute, 4> graphEntries;
  auto appendGraphEntry = [&](int64_t muId, StringRef role,
                              StringRef layoutKind, ArrayAttr entryOwnerDims,
                              ArrayAttr entryBlockShape, int64_t muBlockCount,
                              int64_t entryCuGroupSize, int64_t edgeCommBytes,
                              StringRef edgeClass) {
    int64_t entryTileBytes = tileBytes;
    if (auto entryShape = readI64Array(entryBlockShape))
      entryTileBytes = sde::tilePayloadBytes(*entryShape, elemBytes);
    int64_t normalizedGroup = std::clamp<int64_t>(
        entryCuGroupSize, int64_t{1}, std::max<int64_t>(1, muBlockCount));
    int64_t entryCuGroupCount = sde::ceilDivPositive(
        std::max<int64_t>(1, muBlockCount), normalizedGroup);
    SmallVector<NamedAttribute, 10> attrs;
    attrs.push_back(builder.getNamedAttr(
        sde::AttrNames::PartitionGraphKeys::MuId, i64Attr(muId)));
    attrs.push_back(builder.getNamedAttr(
        sde::AttrNames::PartitionGraphKeys::Role, builder.getStringAttr(role)));
    attrs.push_back(
        builder.getNamedAttr(sde::AttrNames::PartitionGraphKeys::LayoutKind,
                             builder.getStringAttr(layoutKind)));
    attrs.push_back(builder.getNamedAttr(
        sde::AttrNames::PartitionGraphKeys::OwnerDims, entryOwnerDims));
    attrs.push_back(builder.getNamedAttr(
        sde::AttrNames::PartitionGraphKeys::BlockShape, entryBlockShape));
    attrs.push_back(builder.getNamedAttr(
        sde::AttrNames::PartitionGraphKeys::TilePayloadBytes,
        i64Attr(entryTileBytes)));
    attrs.push_back(
        builder.getNamedAttr(sde::AttrNames::PartitionGraphKeys::MuBlockCount,
                             i64Attr(std::max<int64_t>(1, muBlockCount))));
    attrs.push_back(
        builder.getNamedAttr(sde::AttrNames::PartitionGraphKeys::CuGroupSize,
                             i64Attr(normalizedGroup)));
    attrs.push_back(
        builder.getNamedAttr(sde::AttrNames::PartitionGraphKeys::CuGroupCount,
                             i64Attr(entryCuGroupCount)));
    attrs.push_back(
        builder.getNamedAttr(sde::AttrNames::PartitionGraphKeys::EdgeCommBytes,
                             i64Attr(edgeCommBytes)));
    attrs.push_back(
        builder.getNamedAttr(sde::AttrNames::PartitionGraphKeys::EdgeClass,
                             builder.getStringAttr(edgeClass)));
    graphEntries.push_back(DictionaryAttr::get(ctx, attrs));
  };

  bool addedPrimaryOwnerBlock = false;
  if (ArrayAttr layout = op.getArrayLayoutAttr()) {
    for (auto [idx, attr] : llvm::enumerate(layout)) {
      auto dict = dyn_cast<DictionaryAttr>(attr);
      if (!dict)
        continue;
      int64_t muId = static_cast<int64_t>(idx);
      if (auto arrayId = dyn_cast_or_null<IntegerAttr>(
              dict.get(sde::AttrNames::LayoutGraph::ArrayId)))
        muId = arrayId.getInt();
      auto kind = dyn_cast_or_null<StringAttr>(
          dict.get(sde::AttrNames::LayoutGraph::Kind));
      auto layoutOwnerDims = dyn_cast_or_null<ArrayAttr>(
          dict.get(sde::AttrNames::LayoutGraph::OwnerDims));
      auto layoutBlockShape = dyn_cast_or_null<ArrayAttr>(
          dict.get(sde::AttrNames::LayoutGraph::BlockShape));
      if (!layoutOwnerDims || !layoutBlockShape)
        continue;
      auto roleAttr = dyn_cast_or_null<StringAttr>(
          dict.get(sde::AttrNames::LayoutGraph::Role));
      StringRef role =
          roleAttr ? roleAttr.getValue()
                   : StringRef(sde::AttrNames::LayoutGraphValues::RoleUnknown);
      StringRef layoutKind =
          kind ? kind.getValue()
               : StringRef(sde::AttrNames::PartitionGraphValues::UnknownLayout);
      int64_t edgeCommBytes = 0;
      if (auto edgeBytes = dyn_cast_or_null<IntegerAttr>(
              dict.get(sde::AttrNames::LayoutGraph::CommVolumeBytes)))
        edgeCommBytes = std::max<int64_t>(0, edgeBytes.getInt());
      int64_t entryMuBlockCount = 1;
      if (auto blocks = dyn_cast_or_null<IntegerAttr>(
              dict.get(sde::AttrNames::LayoutGraph::MuBlockCount)))
        entryMuBlockCount = std::max<int64_t>(1, blocks.getInt());
      StringRef edgeClass =
          edgeCommBytes > 0
              ? StringRef(
                    sde::AttrNames::PartitionGraphValues::EdgeLayoutMismatch)
              : StringRef(sde::AttrNames::PartitionGraphValues::EdgeAligned);

      if (!addedPrimaryOwnerBlock &&
          role == sde::AttrNames::LayoutGraphValues::RoleWrite) {
        layoutKind = sde::AttrNames::PartitionGraphValues::OwnerBlock;
        layoutOwnerDims = buildI64ArrayAttr(ctx, *ownerDims);
        layoutBlockShape = buildI64ArrayAttr(ctx, *blockShape);
        entryMuBlockCount = std::max<int64_t>(1, cuCount);
        edgeCommBytes = commBytes;
        edgeClass =
            edgeCommBytes > 0
                ? StringRef(
                      sde::AttrNames::PartitionGraphValues::EdgeLayoutMismatch)
                : StringRef(sde::AttrNames::PartitionGraphValues::EdgeAligned);
        addedPrimaryOwnerBlock = true;
      }

      int64_t entryCuGroupSize =
          (role == sde::AttrNames::LayoutGraphValues::RoleWrite ||
           edgeCommBytes <= 0)
              ? int64_t{1}
              : cuGroupSize;
      appendGraphEntry(muId, role, layoutKind, layoutOwnerDims,
                       layoutBlockShape, entryMuBlockCount, entryCuGroupSize,
                       edgeCommBytes, edgeClass);
    }
  }

  if (!addedPrimaryOwnerBlock) {
    StringRef edgeClass =
        commBytes > 0
            ? StringRef(
                  sde::AttrNames::PartitionGraphValues::EdgeLayoutMismatch)
            : StringRef(sde::AttrNames::PartitionGraphValues::EdgeAligned);
    appendGraphEntry(
        /*muId=*/0, sde::AttrNames::LayoutGraphValues::RoleWrite,
        sde::AttrNames::PartitionGraphValues::OwnerBlock,
        buildI64ArrayAttr(ctx, *ownerDims), buildI64ArrayAttr(ctx, *blockShape),
        std::max<int64_t>(1, cuCount), /*entryCuGroupSize=*/1, commBytes,
        edgeClass);
  }

  if (!graphEntries.empty())
    op->setAttr(sde::AttrNames::PartitionGraph,
                ArrayAttr::get(ctx, graphEntries));
}

static int64_t coarsenLoopIndexedOwnerPlanToTileFloor(
    const sde::LoopIndexedOutputPlan &outputPlan, sde::SDECostModel &costModel,
    int64_t workers, int64_t abstractCommVolumeBytes,
    ArrayRef<sde::CuMuHyperedgePressure> hyperedges,
    SmallVectorImpl<int64_t> &ownerPhysicalDims,
    SmallVectorImpl<int64_t> &physicalBlockShape) {
  int64_t elemBytes = outputElementBytes(outputPlan.root);
  if (costModel.getMinDistributedTileBytes() <= 0 || elemBytes <= 0)
    return workers;

  auto rebuild = [&](int64_t candidateWorkers,
                     SmallVectorImpl<int64_t> &candidateShape) {
    SmallVector<int64_t, 4> tmpOwnerDims;
    SmallVector<int64_t, 4> tmpBlockShape;
    if (!buildOwnerDimPlan(outputPlan, candidateWorkers, tmpOwnerDims,
                           tmpBlockShape))
      return false;
    candidateShape.assign(tmpBlockShape.begin(), tmpBlockShape.end());
    return true;
  };

  std::optional<sde::CuMuPartitionPlan> selected =
      chooseCuMuTileFloorPlan(outputPlan.shape, outputPlan.ownerPhysicalDims,
                              elemBytes, abstractCommVolumeBytes, costModel,
                              workers, physicalBlockShape, hyperedges, rebuild);
  if (!selected)
    return workers;

  ownerPhysicalDims.clear();
  ownerPhysicalDims.assign(outputPlan.ownerPhysicalDims.begin(),
                           outputPlan.ownerPhysicalDims.end());
  physicalBlockShape.assign(selected->physicalBlockShape.begin(),
                            selected->physicalBlockShape.end());
  return selected->computeUnits;
}

static void
coarsenExistingLoopIndexedOwnerPlanToTileFloor(sde::SdeSuIterateOp op,
                                               sde::SDECostModel &costModel) {
  if (sde::hasCommittedCuMuPartitionPlan(op.getOperation()) ||
      op.getDistributionKindAttr() ||
      op->getParentOfType<sde::SdeSuDistributeOp>())
    return;

  if (costModel.getMinDistributedTileBytes() <= 0 ||
      !op.getPhysicalOwnerDimsAttr() || !op.getPhysicalBlockShapeAttr())
    return;

  auto classification = op.getStructuredClassification();
  if (classification &&
      *classification == sde::SdeStructuredClassification::stencil)
    return;

  std::optional<sde::LoopIndexedOutputPlan> outputPlan =
      sde::findConsistentLoopIndexedOutputPlanWithOwnerDims(op);
  if (!outputPlan)
    outputPlan = sde::findLoopIndexedOutputPlan(op);
  if (!outputPlan || outputPlan->shape.empty())
    return;

  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(op.getPhysicalOwnerDimsAttr());
  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(op.getPhysicalBlockShapeAttr());
  if (!ownerDims || ownerDims->empty() || !blockShape ||
      blockShape->size() != outputPlan->shape.size())
    return;

  sde::LoopIndexedOutputPlan plan = *outputPlan;
  plan.ownerPhysicalDims.assign(ownerDims->begin(), ownerDims->end());
  int64_t workers = sde::inferCuCountFromMuPartition(
      plan.shape, plan.ownerPhysicalDims, *blockShape);
  if (workers <= 1)
    return;

  SmallVector<int64_t, 4> coarsenedOwnerDims(ownerDims->begin(),
                                             ownerDims->end());
  SmallVector<int64_t, 4> coarsenedBlockShape(blockShape->begin(),
                                              blockShape->end());
  int64_t coarsenedWorkers = coarsenLoopIndexedOwnerPlanToTileFloor(
      plan, costModel, workers, readAbstractCommVolumeBytes(op),
      collectAbstractMuHyperedges(op), coarsenedOwnerDims, coarsenedBlockShape);
  if (coarsenedWorkers >= workers)
    return;

  alignLateOwnerPlanToExistingStep(op, coarsenedOwnerDims, coarsenedBlockShape);
  op.setPhysicalOwnerDimsAttr(
      buildI64ArrayAttr(op.getContext(), coarsenedOwnerDims));
  op.setPhysicalBlockShapeAttr(
      buildI64ArrayAttr(op.getContext(), coarsenedBlockShape));
  op.setLogicalWorkerSliceAttr(
      buildI64ArrayAttr(op.getContext(), coarsenedBlockShape));
}

static SmallVector<Value, 4>
collectAllSuLoopIndexValues(sde::SdeSuIterateOp op) {
  SmallVector<Value, 4> ownerIndexValues;
  if (!op || op.getBody().empty())
    return ownerIndexValues;
  auto ivs = op.getLoopInductionVars();
  if (!ivs)
    return ownerIndexValues;
  ownerIndexValues.append(ivs->begin(), ivs->end());
  return ownerIndexValues;
}

static bool allRootAccessesStayWithinOwnerTile(sde::SdeSuIterateOp op,
                                               Value root,
                                               ArrayRef<int64_t> ownerDims) {
  if (!op || !root || ownerDims.empty())
    return false;

  SmallVector<Value, 4> ownerIndexValues = collectAllSuLoopIndexValues(op);
  if (ownerIndexValues.empty())
    return false;

  bool sawRootAccess = false;
  auto checkIndices = [&](Value memref, OperandRange indices) {
    Value base = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(memref);
    if (base != root)
      return WalkResult::advance();

    sawRootAccess = true;
    auto memRefType = dyn_cast<MemRefType>(base.getType());
    if (!memRefType || memRefType.getRank() == 0 || indices.empty())
      return WalkResult::interrupt();
    for (int64_t rawDim : ownerDims) {
      if (rawDim < 0 || static_cast<size_t>(rawDim) >= indices.size())
        return WalkResult::interrupt();
      if (!sde::isOwnerDependentIndex(indices[rawDim], ownerIndexValues))
        return WalkResult::interrupt();
    }
    return WalkResult::advance();
  };

  WalkResult result = op.getBody().walk([&](Operation *nested) {
    if (auto loadOp = dyn_cast<memref::LoadOp>(nested)) {
      if (isa<MemRefType>(loadOp.getResult().getType()))
        return WalkResult::advance();
      return checkIndices(loadOp.getMemref(), loadOp.getIndices());
    }
    if (auto storeOp = dyn_cast<memref::StoreOp>(nested)) {
      if (isa<MemRefType>(storeOp.getValueToStore().getType()))
        return WalkResult::advance();
      return checkIndices(storeOp.getMemref(), storeOp.getIndices());
    }
    return WalkResult::advance();
  });

  return !result.wasInterrupted() && sawRootAccess;
}

static std::optional<sde::LoopIndexedOutputPlan>
findConsistentMultiOwnerOutputPlan(sde::SdeSuIterateOp op) {
  if (!op || op.getBody().empty())
    return std::nullopt;

  SmallVector<Value, 4> ownerIndexValues = collectAllSuLoopIndexValues(op);
  if (ownerIndexValues.size() < 2)
    return std::nullopt;

  Block *computeBlock = sde::getSuIterateComputeBlock(op);
  if (!computeBlock)
    return std::nullopt;

  bool rejected = false;
  std::optional<sde::LoopIndexedOutputPlan> selectedPlan;
  auto visitStore = [&](memref::StoreOp storeOp) {
    Value base =
        ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storeOp.getMemref());
    if (sde::isDefinedInside(op.getOperation(), base))
      return WalkResult::advance();

    auto memRefType = dyn_cast<MemRefType>(base.getType());
    if (!memRefType || memRefType.getRank() == 0 ||
        storeOp.getIndices().empty()) {
      rejected = true;
      return WalkResult::interrupt();
    }

    SmallVector<int64_t, 4> ownerPhysicalDims =
        sde::collectExactOwnerIndexedPhysicalDims(storeOp.getIndices(),
                                                  ownerIndexValues);
    if (ownerPhysicalDims.size() < 2) {
      rejected = true;
      return WalkResult::interrupt();
    }

    SmallVector<int64_t, 4> shape;
    shape.reserve(memRefType.getRank());
    for (int64_t dim : memRefType.getShape()) {
      if (dim == ShapedType::kDynamic) {
        rejected = true;
        return WalkResult::interrupt();
      }
      shape.push_back(dim);
    }

    sde::LoopIndexedOutputPlan candidate{base, std::move(shape),
                                         std::move(ownerPhysicalDims)};
    if (!selectedPlan) {
      selectedPlan = std::move(candidate);
      return WalkResult::advance();
    }

    if (candidate.root != selectedPlan->root ||
        candidate.shape != selectedPlan->shape ||
        candidate.ownerPhysicalDims != selectedPlan->ownerPhysicalDims) {
      rejected = true;
      return WalkResult::interrupt();
    }

    return WalkResult::advance();
  };

  for (Operation &nested : computeBlock->without_terminator()) {
    if (nested.walk(visitStore).wasInterrupted())
      break;
  }

  if (rejected)
    return std::nullopt;
  return selectedPlan;
}

static void applyPhysicalPlan(sde::SdeSuIterateOp op,
                              ArrayRef<int64_t> ownerDims,
                              ArrayRef<int64_t> physicalBlockShape,
                              ArrayRef<int64_t> haloShape = {}) {
  op.setPhysicalOwnerDimsAttr(buildI64ArrayAttr(op.getContext(), ownerDims));
  op.setPhysicalBlockShapeAttr(
      buildI64ArrayAttr(op.getContext(), physicalBlockShape));
  if (llvm::any_of(haloShape, [](int64_t halo) { return halo > 0; }))
    op.setPhysicalHaloShapeAttr(buildI64ArrayAttr(op.getContext(), haloShape));
  op.setLogicalWorkerSliceAttr(
      buildI64ArrayAttr(op.getContext(), physicalBlockShape));
  op.setIterationTopologyAttr(sde::SdeIterationTopologyAttr::get(
      op.getContext(), ownerDims.size() > 1
                           ? sde::SdeIterationTopology::owner_tile
                           : sde::SdeIterationTopology::owner_strip));
}

static bool hasPhysicalLayoutPlan(sde::SdeSuIterateOp op) {
  auto ownerDims = readI64ArrayAttr(op.getPhysicalOwnerDimsAttr());
  auto blockShape = readI64ArrayAttr(op.getPhysicalBlockShapeAttr());
  return ownerDims && !ownerDims->empty() && blockShape && !blockShape->empty();
}

static std::optional<sde::LayoutGraphFact>
selectSingleWriteLayoutFact(sde::SdeSuIterateOp op) {
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return std::nullopt;

  std::optional<sde::LayoutGraphFact> selected;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    if (fact.role != sde::LayoutGraphRole::write || fact.ownerDims.empty() ||
        fact.blockShape.empty())
      continue;
    if (selected)
      return std::nullopt;
    selected = fact;
  }
  return selected;
}

static bool allExternalStoresCoverOwnerDims(sde::SdeSuIterateOp op,
                                            ArrayRef<int64_t> ownerDims) {
  if (!op || ownerDims.empty() || op.getBody().empty())
    return true;

  auto loopIvs = op.getLoopInductionVars();
  if (!loopIvs || loopIvs->empty())
    return false;

  bool sawExternalStore = false;
  bool rejected = false;
  op.getBody().walk([&](memref::StoreOp storeOp) {
    if (rejected)
      return;
    Value root =
        ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storeOp.getMemref());
    if (!root || sde::isDefinedInside(op.getOperation(), root))
      return;
    auto memrefType = dyn_cast<MemRefType>(root.getType());
    if (!memrefType || memrefType.getRank() == 0) {
      rejected = true;
      return;
    }

    sawExternalStore = true;
    OperandRange indices = storeOp.getIndices();
    for (int64_t ownerDim : ownerDims) {
      if (ownerDim < 0 || static_cast<unsigned>(ownerDim) >= indices.size()) {
        rejected = true;
        return;
      }
      if (!sde::isExactOwnerIndex(indices[ownerDim], *loopIvs)) {
        rejected = true;
        return;
      }
    }
  });

  return sawExternalStore && !rejected;
}

static bool assignedWriteLayoutMatchesOwnerDims(sde::SdeSuIterateOp op,
                                                ArrayRef<int64_t> ownerDims) {
  std::optional<sde::LayoutGraphFact> writeLayout =
      selectSingleWriteLayoutFact(op);
  if (!writeLayout)
    return true;
  return llvm::equal(writeLayout->ownerDims, ownerDims);
}

static bool stampPhysicalPlanFromAssignedLayout(sde::SdeSuIterateOp op,
                                                sde::SDECostModel &costModel) {
  if (!op || hasPhysicalLayoutPlan(op) ||
      sde::hasCommittedCuMuPartitionEvidence(op.getOperation()))
    return false;

  auto classification = op.getStructuredClassification();
  if (classification &&
      *classification == sde::SdeStructuredClassification::stencil)
    return false;

  std::optional<sde::LayoutGraphFact> writeLayout =
      selectSingleWriteLayoutFact(op);
  if (!writeLayout)
    return false;

  std::optional<sde::LoopIndexedOutputPlan> outputPlan =
      sde::findConsistentLoopIndexedOutputPlanWithOwnerDims(op);
  if (!outputPlan)
    outputPlan = sde::findLoopIndexedOutputPlan(op);
  if (!outputPlan || outputPlan->shape.empty())
    return false;

  sde::LoopIndexedOutputPlan plan = *outputPlan;
  plan.ownerPhysicalDims.assign(writeLayout->ownerDims.begin(),
                                writeLayout->ownerDims.end());
  if (!allExternalStoresCoverOwnerDims(op, plan.ownerPhysicalDims))
    return false;

  int64_t workers =
      std::max<int64_t>(1, getInterLocalityTargetWorkers(costModel));
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> physicalBlockShape;
  if (!buildOwnerDimPlan(plan, workers, ownerDims, physicalBlockShape))
    return false;

  coarsenLoopIndexedOwnerPlanToTileFloor(
      plan, costModel, workers, readAbstractCommVolumeBytes(op),
      collectAbstractMuHyperedges(op), ownerDims, physicalBlockShape);
  alignLateOwnerPlanToExistingStep(op, ownerDims, physicalBlockShape);
  return applyPhysicalPlanIfRealized(op, ownerDims, physicalBlockShape);
}

// Consume the one committed node-agnostic budget layout for every SU that
// writes a multi-owner-distributed data-parallel array, stamping identical
// physicalOwnerDims + physicalBlockShape (+ logicalWorkerSlice) across all
// writers of that array. That equality is what hasSameHostBridgePlan
// (ArtsMaterializationUtils.h) requires, so the per-timestep host bridge hoists
// and the iterative double-buffer stencils stop materializing a coarse
// per-timestep copy. Runs first in the stamper dispatch and is the default for
// the multi-owner data-parallel family (matmul/contraction excluded).
// Realization is not gated on the loop step: the committed layout is the
// authority and the iteration-space decomposition re-tiles to the block.
static bool stampBudgetReconciledPlan(sde::SdeSuIterateOp op,
                                      sde::SDECostModel &costModel) {
  (void)costModel;
  if (!op || hasPhysicalLayoutPlan(op) ||
      sde::hasCommittedCuMuPartitionEvidence(op.getOperation()))
    return false;
  // Matmul/contraction keeps its dedicated contraction-tiling plan: its CU-task
  // grain is the reduction-aware worker grain, not the data-parallel block
  // grain reconciled here. This is the one genuinely layout-irreducible family.
  if (auto cls = op.getStructuredClassification();
      cls && *cls == sde::SdeStructuredClassification::matmul)
    return false;
  if (auto cls = op.getStructuredClassification();
      cls && *cls == sde::SdeStructuredClassification::stencil)
    return false;
  if (auto cls = op.getStructuredClassification();
      cls &&
      (*cls == sde::SdeStructuredClassification::elementwise ||
       *cls == sde::SdeStructuredClassification::elementwise_pipeline) &&
      op.getInPlaceSafeAttr())
    return false;
  if (auto pat = op.getPattern(); pat && *pat == sde::SdePattern::matmul)
    return false;
  std::optional<sde::LayoutGraphFact> writeLayout =
      selectSingleWriteLayoutFact(op);
  if (!writeLayout || writeLayout->ownerDims.size() < 2 ||
      writeLayout->budgetBlockShape.empty())
    return false;
  // owner_tile needs one realized SDE loop dimension per owner dim. A 1-D loop
  // (e.g. a residual/reduction loop over the same array) that did not get
  // promoted cannot carry a multi-owner tile; leave it to the pattern stampers
  // rather than stamping an unverifiable owner_tile plan.
  if (op.getLowerBounds().size() < writeLayout->ownerDims.size())
    return false;
  SmallVector<int64_t, 4> ownerDims(writeLayout->ownerDims.begin(),
                                    writeLayout->ownerDims.end());
  if (!allExternalStoresCoverOwnerDims(op, ownerDims))
    return false;
  SmallVector<int64_t, 4> blockShape(writeLayout->budgetBlockShape.begin(),
                                     writeLayout->budgetBlockShape.end());
  // Per-owner-dim halo from the op's stencil access offsets (0 for
  // non-stencils). Not compared by hasSameHostBridgePlan, but needed for
  // correct halo exchange.
  SmallVector<int64_t, 4> haloShape;
  bool anyHalo = false;
  for (int64_t od : ownerDims) {
    int64_t h =
        od >= 0 ? readStencilHaloForOwnerDim(op, static_cast<unsigned>(od)) : 0;
    haloShape.push_back(std::max<int64_t>(0, h));
    anyHalo |= h > 0;
  }
  applyPhysicalPlan(op, ownerDims, blockShape,
                    anyHalo ? ArrayRef<int64_t>(haloShape)
                            : ArrayRef<int64_t>{});
  return true;
}

static bool mayRefineExistingPhysicalLayoutPlan(sde::SdeSuIterateOp op) {
  if (sde::hasCommittedCuMuPartitionPlan(op.getOperation()))
    return false;
  if (op.getDistributionKindAttr() ||
      op->getParentOfType<sde::SdeSuDistributeOp>())
    return false;

  auto classification = op.getStructuredClassification();
  if (classification &&
      *classification == sde::SdeStructuredClassification::matmul)
    return true;

  auto pattern = op.getPattern();
  return pattern && *pattern == sde::SdePattern::matmul;
}

static std::optional<int64_t> getPositiveConstantIndex(Value value) {
  int64_t constant = 0;
  if (::mlir::carts::ValueAnalysis::getConstantIndex(value, constant) &&
      constant > 0)
    return constant;
  std::optional<int64_t> folded =
      ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(value);
  if (folded && *folded > 0)
    return folded;
  return std::nullopt;
}

static void
alignLateOwnerPlanToExistingStep(sde::SdeSuIterateOp op,
                                 ArrayRef<int64_t> ownerDims,
                                 MutableArrayRef<int64_t> physicalBlockShape) {
  if (ownerDims.size() != 1 || op.getSteps().empty())
    return;

  int64_t ownerPhysicalDim = ownerDims.front();
  if (ownerPhysicalDim < 0 ||
      static_cast<size_t>(ownerPhysicalDim) >= physicalBlockShape.size())
    return;

  std::optional<int64_t> ownerStep = getPositiveConstantIndex(op.getSteps()[0]);
  if (!ownerStep || *ownerStep <= physicalBlockShape[ownerPhysicalDim])
    return;

  // DistributionPlanning runs after SDE loop tiling. When it authors a physical
  // owner plan late, the element-space block for the owner dimension must cover
  // the already-existing owner-loop step; otherwise one planned task slice can
  // index outside the dependency window described by the SDE owner plan.
  physicalBlockShape[ownerPhysicalDim] = *ownerStep;
}

static bool
physicalPlanMatchesOwnerStepOrder(sde::SdeSuIterateOp op,
                                  ArrayRef<int64_t> ownerDims,
                                  ArrayRef<int64_t> physicalBlockShape) {
  if (!op || ownerDims.empty() || physicalBlockShape.empty() ||
      ownerDims.size() > op.getSteps().size())
    return false;

  for (auto [idx, rawPhysicalDim] : llvm::enumerate(ownerDims)) {
    if (rawPhysicalDim < 0 ||
        static_cast<size_t>(rawPhysicalDim) >= physicalBlockShape.size())
      return false;
    std::optional<int64_t> realizedStep =
        getPositiveConstantIndex(op.getSteps()[idx]);
    if (!realizedStep || physicalBlockShape[rawPhysicalDim] > *realizedStep)
      return false;
  }
  return true;
}

static bool
physicalPlanMatchesRealizedLoopSteps(sde::SdeSuIterateOp op,
                                     ArrayRef<int64_t> ownerDims,
                                     ArrayRef<int64_t> physicalBlockShape) {
  if (!op || ownerDims.empty() || physicalBlockShape.empty() ||
      op.getSteps().empty())
    return false;

  // Once SDE has committed an owner-tile/strip physical plan, the SU loop step
  // operands are the realized owner-block schedule. Later structured analysis
  // may see the inner element loops introduced by tiling, so validate the
  // committed owner step order before consulting access-derived maps.
  if (physicalPlanMatchesOwnerStepOrder(op, ownerDims, physicalBlockShape))
    return true;

  SmallVector<int64_t, 4> physicalDimToLoopDim(physicalBlockShape.size(), -1);
  if (std::optional<sde::StructuredOutputLayoutPlan> layoutPlan =
          sde::findCompatibleOutputLayoutPlan(op)) {
    for (auto [physicalDim, rawLoopDim] :
         llvm::enumerate(layoutPlan->physicalDimToLoopDim)) {
      if (physicalDim >= physicalDimToLoopDim.size())
        break;
      physicalDimToLoopDim[physicalDim] = rawLoopDim;
    }
  }

  for (auto [idx, rawPhysicalDim] : llvm::enumerate(ownerDims)) {
    if (rawPhysicalDim < 0 ||
        static_cast<size_t>(rawPhysicalDim) >= physicalBlockShape.size())
      return false;
    int64_t loopDim = physicalDimToLoopDim[rawPhysicalDim];
    if (loopDim < 0 && ownerDims.size() == 1)
      loopDim = 0;
    if (loopDim < 0 && idx < op.getSteps().size())
      loopDim = static_cast<int64_t>(idx);
    if (loopDim < 0 || static_cast<size_t>(loopDim) >= op.getSteps().size())
      return false;

    std::optional<int64_t> realizedStep =
        getPositiveConstantIndex(op.getSteps()[loopDim]);
    if (!realizedStep || physicalBlockShape[rawPhysicalDim] > *realizedStep)
      return false;
  }
  return true;
}

static bool applyPhysicalPlanIfRealized(sde::SdeSuIterateOp op,
                                        ArrayRef<int64_t> ownerDims,
                                        ArrayRef<int64_t> physicalBlockShape,
                                        ArrayRef<int64_t> haloShape) {
  if (!physicalPlanMatchesRealizedLoopSteps(op, ownerDims, physicalBlockShape))
    return false;
  applyPhysicalPlan(op, ownerDims, physicalBlockShape, haloShape);
  return true;
}

static void stampStencilPhysicalPlan(sde::SdeSuIterateOp op,
                                     sde::SDECostModel &costModel) {
  if ((op.getPhysicalOwnerDimsAttr() && op.getPhysicalBlockShapeAttr()) ||
      costModel.getLogicalWorkerCapacity() <= 1)
    return;
  auto classification = op.getStructuredClassification();
  bool isStencil = classification &&
                   *classification == sde::SdeStructuredClassification::stencil;
  if (op.getInPlaceSafe() && !isStencil)
    return;
  if (classification && !isStencil)
    return;
  if (isStencil && sde::hasNestedStencilOwnerContract(op))
    return;

  /// First-dimension unclassified loops are owned by the uniform/matmul
  /// planners below. This secondary owner-dim path handles imperfect local
  /// stencil/update nests whose owner IV indexes a later physical output
  /// dimension.
  if (!isStencil && sde::findLoopIndexedOutputPlan(op))
    return;

  if (isStencil) {
    std::optional<sde::StructuredOutputLayoutPlan> outputPlan =
        sde::findCompatibleOutputLayoutPlan(op);
    if (outputPlan) {
      SmallVector<unsigned, 4> ownerLoopDims =
          chooseMappedSdeOwnerLoopDims(op, *outputPlan);
      if (ownerLoopDims.size() > op.getLowerBounds().size())
        return;
      int64_t workers = std::max<int64_t>(1, getStencilWorkerTarget(costModel));
      // One-dimensional halo stencils form a dependency pipeline between
      // neighboring owner blocks. Planning modestly more owner blocks than
      // logical worker capacity gives later scheduling enough ready tasks
      // without flooding target materialization with tiny stencil slices.
      if (ownerLoopDims.size() == 1)
        workers *= 2;
      SmallVector<int64_t, 4> ownerDims;
      SmallVector<int64_t, 4> physicalBlockShape;
      SmallVector<int64_t, 4> haloShape;
      if (!buildOwnerDimPlan(op, *outputPlan, ownerLoopDims, workers, ownerDims,
                             physicalBlockShape, haloShape))
        return;

      // Optional halo-expanded tile-bytes floor. Mirrors the matmul coarsening
      // loop but accounts for perimeter halo overhead. When the configured
      // floor is met by the initial plan (or disabled), this is a no-op.
      int64_t stencilFloor = costModel.getMinDistributedStencilTileBytes();
      if (stencilFloor > 0) {
        int64_t elemBytes = outputElementBytes(outputPlan->root);
        if (elemBytes > 0) {
          // Per-physical-dim halo radii (zeros for non-owner dims), so the
          // expansion ratio reflects the full N-d tile.
          SmallVector<int64_t, 4> haloByPhysical(outputPlan->shape.size(), 0);
          for (auto [idx, physDim] : llvm::enumerate(ownerDims)) {
            if (physDim < 0 ||
                static_cast<size_t>(physDim) >= haloByPhysical.size())
              continue;
            haloByPhysical[physDim] =
                idx < haloShape.size() ? haloShape[idx] : 0;
          }
          auto rebuild = [&](int64_t candidateWorkers,
                             SmallVectorImpl<int64_t> &candidateShape) {
            SmallVector<int64_t, 4> tmpOwnerDims;
            SmallVector<int64_t, 4> tmpHalo;
            SmallVector<int64_t, 4> tmpBlock;
            if (!buildOwnerDimPlan(op, *outputPlan, ownerLoopDims,
                                   candidateWorkers, tmpOwnerDims, tmpBlock,
                                   tmpHalo))
              return false;
            candidateShape.assign(tmpBlock.begin(), tmpBlock.end());
            return true;
          };
          int64_t coarsened = sde::coarsenStencilWorkersToFloor(
              workers, outputPlan->shape, haloByPhysical, physicalBlockShape,
              elemBytes, stencilFloor, rebuild);
          if (coarsened < workers) {
            workers = coarsened;
            ownerDims.clear();
            physicalBlockShape.clear();
            haloShape.clear();
            if (!buildOwnerDimPlan(op, *outputPlan, ownerLoopDims, workers,
                                   ownerDims, physicalBlockShape, haloShape))
              return;
          }
        }
      }

      if (isInPlaceSelfReadStencil(op) && !op.getInPlaceSafeAttr()) {
        auto effects = sde::collectStructuredMemoryEffects(op.getBody());
        if (effects.hasUnknownEffects || effects.writes.empty())
          return;
        for (Value written : effects.writes) {
          if (sde::isDefinedInside(op.getOperation(), written))
            continue;
          if (!effects.reads.contains(written))
            continue;
          if (!sde::allRootAccessesStayWithinOwnerSlice(op, written, ownerDims))
            return;
        }
      }

      (void)applyPhysicalPlanIfRealized(op, ownerDims, physicalBlockShape,
                                        haloShape);
      return;
    }
  }

  std::optional<sde::LoopIndexedOutputPlan> secondaryPlan =
      sde::findConsistentLoopIndexedOutputPlanWithOwnerDims(op);
  if (!secondaryPlan || secondaryPlan->ownerPhysicalDims.size() != 1)
    return;

  auto effects = sde::collectStructuredMemoryEffects(op.getBody());
  if (effects.hasUnknownEffects || effects.writes.empty())
    return;
  for (Value written : effects.writes) {
    if (sde::isDefinedInside(op.getOperation(), written))
      continue;
    if (!effects.reads.contains(written))
      continue;
    if (!sde::allRootAccessesStayWithinOwnerSlice(
            op, written, secondaryPlan->ownerPhysicalDims))
      return;
  }

  int64_t workers = std::max<int64_t>(1, getStencilWorkerTarget(costModel));
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> physicalBlockShape;
  if (!buildOwnerDimPlan(*secondaryPlan, workers, ownerDims,
                         physicalBlockShape))
    return;

  // Halo-expanded tile-bytes floor (secondary owner-dim path). This planner
  // arm handles imperfect local stencil/update nests; halo radii are not
  // surfaced here, so the floor compares against owned-tile bytes alone. Any
  // halo overhead beyond that becomes additional motivation to coarsen.
  int64_t stencilFloor = costModel.getMinDistributedStencilTileBytes();
  if (stencilFloor > 0) {
    int64_t elemBytes = outputElementBytes(secondaryPlan->root);
    if (elemBytes > 0) {
      SmallVector<int64_t, 4> zeroHalo(secondaryPlan->shape.size(), 0);
      auto rebuild = [&](int64_t candidateWorkers,
                         SmallVectorImpl<int64_t> &candidateShape) {
        SmallVector<int64_t, 4> tmpOwnerDims;
        SmallVector<int64_t, 4> tmpBlock;
        if (!buildOwnerDimPlan(*secondaryPlan, candidateWorkers, tmpOwnerDims,
                               tmpBlock))
          return false;
        candidateShape.assign(tmpBlock.begin(), tmpBlock.end());
        return true;
      };
      int64_t coarsened = sde::coarsenStencilWorkersToFloor(
          workers, secondaryPlan->shape, zeroHalo, physicalBlockShape,
          elemBytes, stencilFloor, rebuild);
      if (coarsened < workers) {
        workers = coarsened;
        ownerDims.clear();
        physicalBlockShape.clear();
        if (!buildOwnerDimPlan(*secondaryPlan, workers, ownerDims,
                               physicalBlockShape))
          return;
      }
    }
  }

  alignLateOwnerPlanToExistingStep(op, ownerDims, physicalBlockShape);
  (void)applyPhysicalPlanIfRealized(op, ownerDims, physicalBlockShape);
}

static void stampUniformPhysicalPlan(sde::SdeSuIterateOp op,
                                     sde::SDECostModel &costModel) {
  if ((op.getPhysicalOwnerDimsAttr() && op.getPhysicalBlockShapeAttr()) ||
      costModel.getLogicalWorkerCapacity() <= 1)
    return;
  auto classification = op.getStructuredClassification();
  if (classification) {
    if (*classification == sde::SdeStructuredClassification::stencil ||
        *classification == sde::SdeStructuredClassification::matmul)
      return;
    if (*classification == sde::SdeStructuredClassification::reduction &&
        !sde::isOwnerLocalPipelineReduction(op))
      return;
  }
  std::optional<sde::LoopIndexedOutputPlan> outputPlan =
      sde::findLoopIndexedOutputPlan(op);
  bool usedConsistentOwnerFallback = false;
  if (!outputPlan) {
    if (classification &&
        *classification != sde::SdeStructuredClassification::elementwise &&
        *classification !=
            sde::SdeStructuredClassification::elementwise_pipeline)
      return;
    outputPlan = sde::findConsistentLoopIndexedOutputPlanWithOwnerDims(op);
    if (outputPlan && outputPlan->ownerPhysicalDims.size() != 1)
      outputPlan.reset();
    usedConsistentOwnerFallback = outputPlan.has_value();
  }
  if (!outputPlan || outputPlan->shape.empty() ||
      outputPlan->ownerPhysicalDims.empty())
    return;

  if (classification &&
      (*classification == sde::SdeStructuredClassification::elementwise ||
       *classification ==
           sde::SdeStructuredClassification::elementwise_pipeline)) {
    std::optional<sde::LoopIndexedOutputPlan> multiOwnerPlan =
        findConsistentMultiOwnerOutputPlan(op);
    if (multiOwnerPlan && multiOwnerPlan->ownerPhysicalDims.size() >= 2 &&
        op.getLowerBounds().size() >=
            multiOwnerPlan->ownerPhysicalDims.size() &&
        op.getUpperBounds().size() >=
            multiOwnerPlan->ownerPhysicalDims.size() &&
        op.getSteps().size() >= multiOwnerPlan->ownerPhysicalDims.size()) {
      auto effects = sde::collectStructuredMemoryEffects(op.getBody());
      if (!effects.hasUnknownEffects) {
        bool ownerLocal = true;
        for (Value written : effects.writes) {
          if (sde::isDefinedInside(op.getOperation(), written))
            continue;
          if (!effects.reads.contains(written))
            continue;
          if (!allRootAccessesStayWithinOwnerTile(
                  op, written, multiOwnerPlan->ownerPhysicalDims)) {
            ownerLocal = false;
            break;
          }
        }
        if (ownerLocal) {
          SmallVector<int64_t, 4> ownerDims;
          SmallVector<int64_t, 4> physicalBlockShape;
          int64_t workers =
              std::max<int64_t>(1, getInterLocalityTargetWorkers(costModel));
          if (buildOwnerDimPlan(*multiOwnerPlan, workers, ownerDims,
                                physicalBlockShape)) {
            coarsenLoopIndexedOwnerPlanToTileFloor(
                *multiOwnerPlan, costModel, workers,
                readAbstractCommVolumeBytes(op),
                collectAbstractMuHyperedges(op), ownerDims, physicalBlockShape);
            if (applyPhysicalPlanIfRealized(op, ownerDims, physicalBlockShape))
              return;
          }
        }
      }
    }
  }

  if (!assignedWriteLayoutMatchesOwnerDims(op, outputPlan->ownerPhysicalDims))
    return;
  if (!allExternalStoresCoverOwnerDims(op, outputPlan->ownerPhysicalDims))
    return;

  int64_t ownerPhysicalDim = outputPlan->ownerPhysicalDims.front();
  if (ownerPhysicalDim < 0 ||
      static_cast<size_t>(ownerPhysicalDim) >= outputPlan->shape.size() ||
      outputPlan->shape[ownerPhysicalDim] <= 0)
    return;
  if (!classification || usedConsistentOwnerFallback) {
    auto effects = sde::collectStructuredMemoryEffects(op.getBody());
    bool selfRead = effects.reads.contains(outputPlan->root);
    bool ownerLocalSelfRead =
        selfRead && sde::allRootAccessesStayWithinOwnerSlice(
                        op, outputPlan->root, outputPlan->ownerPhysicalDims);
    if (effects.hasUnknownEffects || (selfRead && !ownerLocalSelfRead))
      return;
  }

  int64_t workers =
      std::max<int64_t>(1, getInterLocalityTargetWorkers(costModel));
  if (outputPlan->shape[ownerPhysicalDim] >= workers * 4LL * 1024LL * 1024LL)
    workers *= 2;
  SmallVector<int64_t, 4> physicalBlockShape(outputPlan->shape);
  bool ownerLocalPipeline =
      classification &&
      *classification ==
          sde::SdeStructuredClassification::elementwise_pipeline &&
      sde::isOwnerLocalPipelineReduction(op);
  int64_t minOwnerIterations =
      ownerLocalPipeline
          ? 1
          : std::max<int64_t>(1, costModel.getMinIterationsPerWorker());
  int64_t balancedOwnerIterations =
      sde::ceilDivPositive(outputPlan->shape[ownerPhysicalDim], workers);
  physicalBlockShape[ownerPhysicalDim] =
      std::clamp(std::max(balancedOwnerIterations, minOwnerIterations),
                 int64_t{1}, outputPlan->shape[ownerPhysicalDim]);
  int64_t elemBytes = outputElementBytes(outputPlan->root);
  if (costModel.getMinDistributedTileBytes() > 0 && elemBytes > 0) {
    auto rebuild = [&](int64_t candidateWorkers,
                       SmallVectorImpl<int64_t> &candidateShape) {
      candidateShape.assign(outputPlan->shape.begin(), outputPlan->shape.end());
      int64_t candidateIterations = sde::ceilDivPositive(
          outputPlan->shape[ownerPhysicalDim], candidateWorkers);
      candidateShape[ownerPhysicalDim] =
          std::clamp(std::max(candidateIterations, minOwnerIterations),
                     int64_t{1}, outputPlan->shape[ownerPhysicalDim]);
      return true;
    };
    std::optional<sde::CuMuPartitionPlan> selected = chooseCuMuTileFloorPlan(
        outputPlan->shape, outputPlan->ownerPhysicalDims, elemBytes,
        readAbstractCommVolumeBytes(op), costModel, workers, physicalBlockShape,
        collectAbstractMuHyperedges(op), rebuild);
    if (selected) {
      workers = selected->computeUnits;
      physicalBlockShape.assign(selected->physicalBlockShape.begin(),
                                selected->physicalBlockShape.end());
    }
  }
  alignLateOwnerPlanToExistingStep(op, outputPlan->ownerPhysicalDims,
                                   physicalBlockShape);

  if (!classification)
    op.setStructuredClassificationAttr(
        sde::SdeStructuredClassificationAttr::get(
            op.getContext(), sde::SdeStructuredClassification::elementwise));
  op.setPhysicalOwnerDimsAttr(
      buildI64ArrayAttr(op.getContext(), outputPlan->ownerPhysicalDims));
  op.setPhysicalBlockShapeAttr(
      buildI64ArrayAttr(op.getContext(), physicalBlockShape));
  op.setLogicalWorkerSliceAttr(
      buildI64ArrayAttr(op.getContext(), physicalBlockShape));
  op.setIterationTopologyAttr(sde::SdeIterationTopologyAttr::get(
      op.getContext(), sde::SdeIterationTopology::owner_strip));
}

static void stampMatmulPhysicalPlan(sde::SdeSuIterateOp op,
                                    sde::SDECostModel &costModel) {
  if ((op.getPhysicalOwnerDimsAttr() && op.getPhysicalBlockShapeAttr()) ||
      costModel.getLogicalWorkerCapacity() <= 1)
    return;
  auto classification = op.getStructuredClassification();
  if (!classification ||
      *classification != sde::SdeStructuredClassification::matmul)
    return;
  std::optional<sde::LoopIndexedOutputPlan> outputPlan =
      sde::findLoopIndexedOutputPlan(op);
  if (!outputPlan || outputPlan->shape.size() < 2 ||
      outputPlan->shape[0] <= 0 || outputPlan->shape[1] <= 0)
    return;

  auto effects = sde::collectStructuredMemoryEffects(op.getBody());
  if (effects.hasUnknownEffects || !effects.writes.contains(outputPlan->root))
    return;
  if (!sde::hasDistinctExternalMatmulInputRoots(op))
    return;

  int64_t workers =
      std::max<int64_t>(1, getInterLocalityTargetWorkers(costModel));
  SmallVector<int64_t, 4> physicalBlockShape;
  auto rebuild = [&](int64_t candidateWorkers,
                     SmallVectorImpl<int64_t> &candidateShape) {
    candidateShape.assign(outputPlan->shape.begin(), outputPlan->shape.end());
    SmallVector<int64_t, 4> ownerExtents{outputPlan->shape[0],
                                         outputPlan->shape[1]};
    SmallVector<int64_t, 4> workerGrid = sde::factorWorkersAcrossDims(
        std::max<int64_t>(1, candidateWorkers), ownerExtents);
    candidateShape[0] =
        sde::ceilDivPositive(outputPlan->shape[0], workerGrid[0]);
    candidateShape[1] =
        sde::ceilDivPositive(outputPlan->shape[1], workerGrid[1]);
    return true;
  };
  if (!rebuild(workers, physicalBlockShape))
    return;

  // Optional payload-bytes floor. The floor coarsens only excess task waves:
  // it never shrinks below the logical worker capacity, so SDE keeps blocked
  // CDAG parallelism instead of replacing a distributed layout with a few
  // oversized tiles that serialize the phase.
  if (costModel.getMinDistributedTileBytes() > 0) {
    int64_t elemBytes = outputElementBytes(outputPlan->root);
    if (elemBytes > 0) {
      SmallVector<int64_t, 2> ownerDims{0, 1};
      std::optional<sde::CuMuPartitionPlan> selected = chooseCuMuTileFloorPlan(
          outputPlan->shape, ownerDims, elemBytes,
          readAbstractCommVolumeBytes(op), costModel, workers,
          physicalBlockShape, collectAbstractMuHyperedges(op), rebuild);
      if (selected) {
        workers = selected->computeUnits;
        physicalBlockShape.assign(selected->physicalBlockShape.begin(),
                                  selected->physicalBlockShape.end());
      }
    }
  }

  op.setPhysicalOwnerDimsAttr(
      buildI64ArrayAttr(op.getContext(), SmallVector<int64_t, 2>{0, 1}));
  op.setPhysicalBlockShapeAttr(
      buildI64ArrayAttr(op.getContext(), physicalBlockShape));
  op.setLogicalWorkerSliceAttr(
      buildI64ArrayAttr(op.getContext(), physicalBlockShape));
  op.setIterationTopologyAttr(sde::SdeIterationTopologyAttr::get(
      op.getContext(), sde::SdeIterationTopology::owner_tile_2d));
}

static void stampDirectRowMatmulPhysicalPlan(sde::SdeSuIterateOp op,
                                             sde::SDECostModel &costModel) {
  if ((op.getPhysicalOwnerDimsAttr() && op.getPhysicalBlockShapeAttr()) ||
      costModel.getLogicalWorkerCapacity() <= 1)
    return;
  if (op.getLowerBounds().size() != 1 || op.getSteps().size() != 1)
    return;
  auto classification = op.getStructuredClassification();
  if (!classification ||
      *classification != sde::SdeStructuredClassification::matmul)
    return;
  std::optional<sde::LoopIndexedOutputPlan> outputPlan =
      sde::findLoopIndexedOutputPlan(op);
  if (!outputPlan || outputPlan->shape.size() < 2 ||
      outputPlan->ownerPhysicalDims.size() != 1 ||
      outputPlan->ownerPhysicalDims.front() != 0 || outputPlan->shape[0] <= 0)
    return;

  auto effects = sde::collectStructuredMemoryEffects(op.getBody());
  if (effects.hasUnknownEffects || !effects.writes.contains(outputPlan->root))
    return;
  if (!sde::hasDistinctExternalMatmulInputRoots(op))
    return;
  if (!sde::allRootAccessesStayWithinOwnerSlice(op, outputPlan->root,
                                                outputPlan->ownerPhysicalDims))
    return;

  int64_t workers =
      std::max<int64_t>(1, getInterLocalityTargetWorkers(costModel));
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> physicalBlockShape;
  if (!buildOwnerDimPlan(*outputPlan, workers, ownerDims, physicalBlockShape))
    return;
  coarsenLoopIndexedOwnerPlanToTileFloor(
      *outputPlan, costModel, workers, readAbstractCommVolumeBytes(op),
      collectAbstractMuHyperedges(op), ownerDims, physicalBlockShape);
  alignLateOwnerPlanToExistingStep(op, outputPlan->ownerPhysicalDims,
                                   physicalBlockShape);
  (void)applyPhysicalPlanIfRealized(op, ownerDims, physicalBlockShape);
}

/// Contraction tiling refinement.
///
/// PatternAnalysis detects the contraction-tiling candidate while the matmul
/// loop nest is still canonical and stamps a PROVISIONAL element-space
/// `contractionTileShape = [contractionExtent]` plus the inert declarative
/// facts. This pass, running after the matmul owner plan is stamped, refines
/// that tile size to the producer's owner-block extent so each k-tile maps to
/// exactly one producer block. Boundary planning later derives the concrete
/// split factor T = ceil(contractionExtent / tileSize) from this element-space
/// shape.
///
/// If no real tiling results (single tile / unrecoverable block), the
/// provisional intent is dropped along with its declarative facts: graceful
/// degradation to the existing coarse path, never a regression.
static void refineContractionTileShape(sde::SdeSuIterateOp op,
                                       sde::SDECostModel &costModel) {
  auto provisional = readI64ArrayAttr(op.getContractionTileShapeAttr());
  if (!provisional || provisional->size() != 1)
    return;
  int64_t contractionExtent = provisional->front();
  if (contractionExtent <= 0)
    return;

  auto dropIntent = [&]() {
    op->removeAttr(op.getContractionTileShapeAttrName());
    op->removeAttr(op.getPartialReductionDimsAttrName());
    op->removeAttr(op.getPartialReductionOwnerDimsAttrName());
    op->removeAttr(op.getReductionKindsAttrName());
  };

  if (!op.getPhysicalOwnerDimsAttr() || !op.getPhysicalBlockShapeAttr()) {
    dropIntent();
    return;
  }

  SmallVector<int64_t, 4> shape;
  if (auto blockShape = readI64ArrayAttr(op.getPhysicalBlockShapeAttr()))
    shape = *blockShape;
  int64_t tileSize = contractionExtent;
  if (auto ownerDims = readI64ArrayAttr(op.getPhysicalOwnerDimsAttr());
      ownerDims && !ownerDims->empty()) {
    int64_t ownerDim = ownerDims->front();
    if (ownerDim >= 0 && static_cast<size_t>(ownerDim) < shape.size() &&
        shape[ownerDim] > 0)
      tileSize = std::min<int64_t>(shape[ownerDim], contractionExtent);
  }

  if (tileSize <= 0 || tileSize >= contractionExtent) {
    // No real tiling possible — drop the intent, keep the coarse path.
    dropIntent();
    return;
  }

  op.setContractionTileShapeAttr(
      buildI64ArrayAttr(op.getContext(), {tileSize}));
}

static void stampReductionTaskShapePlan(sde::SdeSuIterateOp op,
                                        sde::SDECostModel &costModel) {
  if (op.getLogicalWorkerSliceAttr() &&
      (op.getPhysicalOwnerDimsAttr() || op.getPhysicalBlockShapeAttr()))
    return;
  auto classification = op.getStructuredClassification();
  if (!classification ||
      *classification != sde::SdeStructuredClassification::reduction)
    return;
  std::optional<int64_t> tripCount = getStaticTripCount(op.getOperation());
  if (!tripCount || *tripCount <= 0)
    return;

  int64_t workers = std::max<int64_t>(1, costModel.getLogicalWorkerCapacity());
  int64_t targetTasks = workers;
  if (op.getReductionAccumulators().empty())
    targetTasks = saturatingMultiplyPositive(
        workers, costModel.getOwnerLocalPipelineTargetTaskWaves());
  int64_t slice =
      std::max<int64_t>(costModel.getMinIterationsPerWorker(),
                        sde::ceilDivPositive(*tripCount, targetTasks));
  slice = std::clamp<int64_t>(slice, 1, *tripCount);

  if (op.getReductionAccumulators().empty()) {
    std::optional<sde::LoopIndexedOutputPlan> outputPlan =
        sde::findLoopIndexedOutputPlan(op);
    if (outputPlan && !outputPlan->ownerPhysicalDims.empty() &&
        !outputPlan->shape.empty()) {
      SmallVector<int64_t, 4> physicalBlockShape(outputPlan->shape);
      if (auto logicalSlice = readI64ArrayAttr(op.getLogicalWorkerSliceAttr());
          logicalSlice && logicalSlice->size() == physicalBlockShape.size()) {
        physicalBlockShape.assign(logicalSlice->begin(), logicalSlice->end());
      } else {
        for (int64_t rawDim : outputPlan->ownerPhysicalDims) {
          if (rawDim < 0 ||
              static_cast<size_t>(rawDim) >= physicalBlockShape.size())
            return;
          physicalBlockShape[rawDim] = slice;
        }
      }
      for (int64_t rawDim : outputPlan->ownerPhysicalDims)
        if (rawDim < 0 ||
            static_cast<size_t>(rawDim) >= physicalBlockShape.size())
          return;
      SmallVector<int64_t, 4> ownerDims(outputPlan->ownerPhysicalDims.begin(),
                                        outputPlan->ownerPhysicalDims.end());
      coarsenLoopIndexedOwnerPlanToTileFloor(
          *outputPlan, costModel, targetTasks, readAbstractCommVolumeBytes(op),
          collectAbstractMuHyperedges(op), ownerDims, physicalBlockShape);
      op.setPhysicalOwnerDimsAttr(
          buildI64ArrayAttr(op.getContext(), ownerDims));
      op.setPhysicalBlockShapeAttr(
          buildI64ArrayAttr(op.getContext(), physicalBlockShape));
      op.setLogicalWorkerSliceAttr(
          buildI64ArrayAttr(op.getContext(), physicalBlockShape));
      op.setIterationTopologyAttr(sde::SdeIterationTopologyAttr::get(
          op.getContext(), sde::SdeIterationTopology::owner_strip));
      return;
    }
  }

  op.setLogicalWorkerSliceAttr(
      buildI64ArrayAttr(op.getContext(), SmallVector<int64_t, 1>{slice}));
  op.setIterationTopologyAttr(sde::SdeIterationTopologyAttr::get(
      op.getContext(), sde::SdeIterationTopology::owner_strip));
}

static void stampInPlaceSharedStencilSerialSlice(sde::SdeSuIterateOp op,
                                                 sde::SDECostModel &costModel) {
  if (op.getLogicalWorkerSliceAttr() ||
      costModel.getLogicalWorkerCapacity() <= 1)
    return;
  auto classification = op.getStructuredClassification();
  if (!classification ||
      *classification != sde::SdeStructuredClassification::stencil)
    return;
  if (!op.getInPlaceSharedStateAttr())
    return;
  if (op.getLowerBounds().size() != 1 || op.getUpperBounds().size() != 1 ||
      op.getSteps().size() != 1)
    return;

  std::optional<int64_t> tripCount = getStaticTripCount(op.getOperation());
  if (!tripCount || *tripCount <= 1)
    return;

  int64_t targetTasks =
      std::max<int64_t>(1, getInterLocalityTargetWorkers(costModel));
  int64_t slice = sde::ceilDivPositive(*tripCount, targetTasks);
  if (slice <= 1)
    return;

  op.setPhysicalOwnerDimsAttr(
      buildI64ArrayAttr(op.getContext(), SmallVector<int64_t, 1>{0}));
  op.setPhysicalBlockShapeAttr(
      buildI64ArrayAttr(op.getContext(), SmallVector<int64_t, 1>{slice}));
  op.setLogicalWorkerSliceAttr(
      buildI64ArrayAttr(op.getContext(), SmallVector<int64_t, 1>{slice}));
  op.setIterationTopologyAttr(sde::SdeIterationTopologyAttr::get(
      op.getContext(), sde::SdeIterationTopology::owner_strip));
}

static int64_t saturatingMultiplyPositive(int64_t lhs, int64_t rhs) {
  lhs = std::max<int64_t>(1, lhs);
  rhs = std::max<int64_t>(1, rhs);
  if (lhs > std::numeric_limits<int64_t>::max() / rhs)
    return std::numeric_limits<int64_t>::max();
  return lhs * rhs;
}

static bool hasEnoughWorkForDistribution(sde::SdeSuIterateOp op,
                                         sde::SDECostModel &costModel) {
  std::optional<int64_t> tripCount = getStaticTripCount(op.getOperation());
  if (!tripCount)
    return true;

  int64_t threshold =
      saturatingMultiplyPositive(costModel.getLogicalWorkerCapacity(),
                                 costModel.getMinIterationsPerWorker());
  return *tripCount >= threshold;
}

static std::optional<sde::SdeDistributionKind>
chooseDistributionKind(sde::SdeSuIterateOp op, sde::SDECostModel &costModel) {
  if (op->getParentOfType<sde::SdeSuDistributeOp>())
    return std::nullopt;
  if (costModel.getLogicalWorkerCapacity() <= 1)
    return std::nullopt;

  auto classificationAttr = op.getStructuredClassificationAttr();
  if (!classificationAttr) {
    std::optional<sde::LoopIndexedOutputPlan> outputPlan =
        sde::findLoopIndexedOutputPlan(op);
    if (!outputPlan)
      return std::nullopt;
    auto effects = sde::collectStructuredMemoryEffects(op.getBody());
    if (effects.hasUnknownEffects || effects.reads.contains(outputPlan->root))
      return std::nullopt;
    if (hasEnoughWorkForDistribution(op, costModel))
      return sde::SdeDistributionKind::blocked;
    return std::nullopt;
  }

  if (op.getNumResults() > 0 && classificationAttr.getValue() !=
                                    sde::SdeStructuredClassification::reduction)
    return std::nullopt;

  switch (classificationAttr.getValue()) {
  case sde::SdeStructuredClassification::elementwise:
  case sde::SdeStructuredClassification::elementwise_pipeline:
    return sde::SdeDistributionKind::blocked;
  case sde::SdeStructuredClassification::stencil:
    if (sde::hasNestedStencilOwnerContract(op))
      return std::nullopt;
    if (isInPlaceSelfReadStencil(op) && !op.getInPlaceSafeAttr())
      return std::nullopt;
    if (hasEnoughWorkForDistribution(op, costModel))
      return sde::SdeDistributionKind::owner_compute;
    return std::nullopt;
  case sde::SdeStructuredClassification::matmul:
    return sde::SdeDistributionKind::blocked;
  case sde::SdeStructuredClassification::reduction:
    if (op.getReductionStrategyAttr() || op.getReductionAccumulators().empty())
      return sde::SdeDistributionKind::blocked;
    return std::nullopt;
  }
  return std::nullopt;
}

struct DistributionPlanningPass
    : public sde::impl::DistributionPlanningBase<DistributionPlanningPass> {
  explicit DistributionPlanningPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    if (!costModel)
      return;

    SmallVector<PlannedDistribution> rewrites;
    getOperation().walk([&](sde::SdeSuIterateOp op) {
      if (sde::hasCommittedCuMuPartitionEvidence(op.getOperation()) ||
          op.getDistributionKindAttr()) {
        stampCuMuPartitionGraphAttrs(op, *costModel);
        if (auto kind = chooseDistributionKind(op, *costModel))
          rewrites.push_back({op, *kind});
        return;
      }

      if (hasPhysicalLayoutPlan(op)) {
        if (mayRefineExistingPhysicalLayoutPlan(op)) {
          coarsenExistingLoopIndexedOwnerPlanToTileFloor(op, *costModel);
          refineContractionTileShape(op, *costModel);
        }
        stampCuMuPartitionGraphAttrs(op, *costModel);
        if (auto kind = chooseDistributionKind(op, *costModel))
          rewrites.push_back({op, *kind});
        return;
      }

      coarsenExistingLoopIndexedOwnerPlanToTileFloor(op, *costModel);
      // Budget-reconciled layout is authored first so the per-pattern stampers
      // below see an already-planned multi-owner data-parallel SU and skip it;
      // they still run for the families budget declines (single-owner, matmul,
      // reduction, in-place). The committed budget layout is the authority.
      stampBudgetReconciledPlan(op, *costModel);
      stampStencilPhysicalPlan(op, *costModel);
      stampDirectRowMatmulPhysicalPlan(op, *costModel);
      stampMatmulPhysicalPlan(op, *costModel);
      refineContractionTileShape(op, *costModel);
      stampUniformPhysicalPlan(op, *costModel);
      stampReductionTaskShapePlan(op, *costModel);
      stampInPlaceSharedStencilSerialSlice(op, *costModel);
      stampPhysicalPlanFromAssignedLayout(op, *costModel);
      stampCuMuPartitionGraphAttrs(op, *costModel);
      if (auto kind = chooseDistributionKind(op, *costModel))
        rewrites.push_back({op, *kind});
    });

    for (PlannedDistribution rewrite : rewrites) {
      if (rewrite.op.getNumResults() > 0) {
        // su_iterate with iter_arg results: can't wrap in su_distribute
        // (NoTerminator op can't forward results). Stamp distribution kind
        // directly on the su_iterate; boundary lowering carries it forward and
        // consumes the plan fact.
        rewrite.op.setDistributionKindAttr(
            sde::SdeDistributionKindAttr::get(&getContext(), rewrite.kind));
        continue;
      }

      IRRewriter rewriter(rewrite.op.getContext());
      rewriter.setInsertionPoint(rewrite.op);

      auto distributeOp = sde::SdeSuDistributeOp::create(
          rewriter, rewrite.op.getLoc(),
          sde::SdeDistributionKindAttr::get(&getContext(), rewrite.kind));
      Block &body = sde::ensureBlock(distributeOp.getBody());
      rewrite.op->moveBefore(&body, body.end());
    }
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass>
createDistributionPlanningPass(sde::SDECostModel *costModel) {
  return std::make_unique<DistributionPlanningPass>(costModel);
}

} // namespace mlir::carts::sde
