///==========================================================================///
/// File: DistributionPlanning.cpp
///
/// SDE distribution transform. This pass keeps distribution intent on the SDE
/// side of the boundary by wrapping eligible `sde.su_iterate` operations in
/// `sde.su_distribute`; it uses SDE pattern/effect facts plus abstract worker
/// capacity/locality. Concrete storage ownership, task placement, routes, and
/// target memory-model choices remain ARTS/ARTS-RT decisions.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_DISTRIBUTIONPLANNING
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Utils/CuMuGraphPartitioning.h"
#include "carts/dialect/sde/Utils/IterationSizingUtils.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/LoopUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <limits>

using namespace mlir;
using namespace mlir::carts;

namespace {

struct DistributionRewrite {
  sde::SdeSuIterateOp op;
  sde::SdeDistributionKind kind = sde::SdeDistributionKind::blocked;
};

static int64_t saturatingMultiplyPositive(int64_t lhs, int64_t rhs);
static void
alignLateOwnerShapeToExistingStep(sde::SdeSuIterateOp op,
                                  ArrayRef<int64_t> ownerPhysicalDims,
                                  MutableArrayRef<int64_t> physicalBlockShape);
static bool applyPhysicalLayoutIfRealized(
    sde::SdeSuIterateOp op, ArrayRef<int64_t> ownerDims,
    ArrayRef<int64_t> physicalBlockShape, ArrayRef<int64_t> haloShape = {},
    ArrayRef<int64_t> logicalWorkerSlice = {});
static SmallVector<int64_t, 4> buildLogicalWorkerSliceOrPhysical(
    sde::SdeSuIterateOp op, ArrayRef<int64_t> shape,
    ArrayRef<int64_t> ownerDims, ArrayRef<int64_t> physicalBlockShape,
    int64_t targetComputeUnits, ArrayRef<int64_t> haloShape = {});
static bool
physicalLayoutMatchesRealizedLoopSteps(sde::SdeSuIterateOp op,
                                       ArrayRef<int64_t> ownerDims,
                                       ArrayRef<int64_t> physicalBlockShape);
static std::optional<int64_t> getPositiveConstantIndex(Value value);

static int64_t getInterLocalityTargetWorkers(sde::SDECostModel &costModel) {
  return saturatingMultiplyPositive(costModel.getLogicalWorkerCapacity(),
                                    costModel.getInterLocalityTaskWaves());
}

// Worker target for the stencil physical-layout committer. Caps at logical
// worker capacity because stencil halo ownership is currently a single-locality
// layout fact. Inter-locality expansion belongs after the dialect boundary has
// a concrete halo path.
static int64_t getStencilWorkerTarget(sde::SDECostModel &costModel) {
  return costModel.getLogicalWorkerCapacity();
}

// Element-byte width derived from the output facts' underlying memref. Returns
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

struct StaticOutputStorageFacts {
  Value root;
  SmallVector<int64_t, 4> shape;
};

static std::optional<StaticOutputStorageFacts>
findSingleExternalStoreShape(sde::SdeSuIterateOp op) {
  if (!op)
    return std::nullopt;

  Block *computeBlock = sde::getSuIterateComputeBlock(op);
  if (!computeBlock)
    return std::nullopt;

  std::optional<StaticOutputStorageFacts> selected;
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
      selected = StaticOutputStorageFacts{root, std::move(shape)};
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

static bool isOneIndex(Value value) {
  return ::mlir::carts::ValueAnalysis::isOneConstant(
             ::mlir::carts::ValueAnalysis::stripNumericCasts(value)) ||
         ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(value).value_or(
             0) == 1;
}

struct RectangularWavefrontLoopNest {
  SmallVector<Value, 4> lowerBounds;
  SmallVector<Value, 4> upperBounds;
  SmallVector<Value, 4> steps;
  SmallVector<Value, 4> sourceIvs;
  SmallVector<scf::ForOp, 4> innerForChain;
  Block *sourceComputeBlock = nullptr;
};

static bool valueDependsOnAny(Value value, ArrayRef<Value> candidates) {
  for (Value candidate : candidates)
    if (::mlir::carts::ValueAnalysis::dependsOn(value, candidate))
      return true;
  return false;
}

static Block *unwrapSingleCuRegionBody(Block *block) {
  if (!block)
    return nullptr;
  Operation *onlyOp = nullptr;
  for (Operation &op : block->without_terminator()) {
    if (onlyOp)
      return block;
    onlyOp = &op;
  }
  auto cuRegion = dyn_cast_or_null<sde::SdeCuRegionOp>(onlyOp);
  if (cuRegion && !cuRegion.getBody().empty())
    return &cuRegion.getBody().front();
  return block;
}

static scf::ForOp findOnlyNestedForWithClosedPrefix(Block *block) {
  if (!block)
    return {};

  scf::ForOp nestedFor;
  SmallVector<Operation *, 4> prefixOps;
  for (Operation &bodyOp : block->without_terminator()) {
    auto forOp = dyn_cast<scf::ForOp>(&bodyOp);
    if (forOp) {
      if (nestedFor)
        return {};
      nestedFor = forOp;
      continue;
    }
    if (nestedFor)
      return {};
    if (bodyOp.getNumRegions() != 0 || !isMemoryEffectFree(&bodyOp))
      return {};
    prefixOps.push_back(&bodyOp);
  }
  if (!nestedFor)
    return {};

  llvm::SmallPtrSet<Operation *, 8> allowedUsers;
  allowedUsers.insert(nestedFor.getOperation());
  for (Operation *prefix : prefixOps)
    allowedUsers.insert(prefix);
  for (Operation *prefix : prefixOps) {
    for (Value result : prefix->getResults()) {
      for (Operation *user : result.getUsers()) {
        if (!allowedUsers.contains(user) && !nestedFor->isAncestor(user))
          return {};
      }
    }
  }
  return nestedFor;
}

static bool collectNestedRectangularLoops(sde::SdeSuIterateOp op,
                                          Block *startBlock,
                                          RectangularWavefrontLoopNest &shape) {
  Block *current = startBlock;
  while (current) {
    current = unwrapSingleCuRegionBody(current);
    scf::ForOp nestedFor = findOnlyNestedForWithClosedPrefix(current);
    if (!nestedFor)
      break;
    if (!isOneIndex(nestedFor.getStep()) || !nestedFor.getInitArgs().empty() ||
        nestedFor.getNumResults() != 0)
      return false;

    for (Value bound : {nestedFor.getLowerBound(), nestedFor.getUpperBound(),
                        nestedFor.getStep()}) {
      if (!bound || sde::isDefinedInside(op.getOperation(), bound) ||
          valueDependsOnAny(bound, shape.sourceIvs))
        return false;
    }

    shape.lowerBounds.push_back(nestedFor.getLowerBound());
    shape.upperBounds.push_back(nestedFor.getUpperBound());
    shape.steps.push_back(nestedFor.getStep());
    shape.sourceIvs.push_back(nestedFor.getInductionVar());
    shape.innerForChain.push_back(nestedFor);
    current = nestedFor.getBody();
  }
  return true;
}

static bool
collectRectangularWavefrontLoopNest(sde::SdeSuIterateOp op,
                                    RectangularWavefrontLoopNest &shape) {
  if (!op || op.getLowerBounds().empty() ||
      op.getLowerBounds().size() != op.getUpperBounds().size() ||
      op.getLowerBounds().size() != op.getSteps().size())
    return false;
  if (op.getChunkSize() || op.getNumResults() != 0 ||
      !op.getReductionAccumulators().empty() || op.getReductionKindsAttr())
    return false;

  Block *computeBlock = sde::getSuIterateComputeBlock(op);
  if (!computeBlock || op.getBody().empty())
    return false;

  unsigned rootRank = op.getLowerBounds().size();
  if (op.getBody().front().getNumArguments() < rootRank)
    return false;

  bool rootIsElementDomain =
      llvm::all_of(op.getSteps(), [](Value step) { return isOneIndex(step); });
  if (rootIsElementDomain) {
    shape.sourceComputeBlock = computeBlock;
    for (unsigned dim = 0; dim < rootRank; ++dim) {
      shape.lowerBounds.push_back(op.getLowerBounds()[dim]);
      shape.upperBounds.push_back(op.getUpperBounds()[dim]);
      shape.steps.push_back(op.getSteps()[dim]);
      shape.sourceIvs.push_back(op.getBody().front().getArgument(dim));
    }
    if (!collectNestedRectangularLoops(op, computeBlock, shape))
      return false;
  } else {
    if (rootRank != 1)
      return false;
    scf::ForOp elementLoop = findOnlyNestedForWithClosedPrefix(computeBlock);
    if (!elementLoop || !isOneIndex(elementLoop.getStep()) ||
        !elementLoop.getInitArgs().empty() || elementLoop.getNumResults() != 0)
      return false;
    shape.lowerBounds.push_back(op.getLowerBounds().front());
    shape.upperBounds.push_back(op.getUpperBounds().front());
    shape.steps.push_back(elementLoop.getStep());
    shape.sourceIvs.push_back(elementLoop.getInductionVar());
    shape.sourceComputeBlock = unwrapSingleCuRegionBody(elementLoop.getBody());
    if (!collectNestedRectangularLoops(op, shape.sourceComputeBlock, shape))
      return false;
  }

  return shape.lowerBounds.size() >= 2;
}

static std::optional<sde::SuNeighborhoodAccessInfo>
getWavefrontNeighborhood(sde::SdeSuIterateOp op, unsigned rank) {
  sde::SuNeighborhoodAccessInfo info;

  auto minOffsets = readI64ArrayAttr(op.getAccessMinOffsetsAttr());
  auto maxOffsets = readI64ArrayAttr(op.getAccessMaxOffsetsAttr());
  auto writeFootprint = readI64ArrayAttr(op.getWriteFootprintAttr());
  if (!minOffsets || !maxOffsets || minOffsets->size() != rank ||
      maxOffsets->size() != rank || !writeFootprint ||
      writeFootprint->size() != rank) {
    std::optional<sde::SuLoopAccessSummary> summary =
        sde::analyzeSuLoopAccesses(op);
    if (!summary)
      return std::nullopt;
    std::optional<sde::SuNeighborhoodAccessInfo> extracted =
        sde::extractNeighborhoodAccessInfo(*summary);
    if (!extracted || extracted->minOffsets.size() != rank ||
        extracted->maxOffsets.size() != rank ||
        extracted->writeFootprint.size() != rank)
      return std::nullopt;
    return extracted;
  }

  info.minOffsets.assign(minOffsets->begin(), minOffsets->end());
  info.maxOffsets.assign(maxOffsets->begin(), maxOffsets->end());
  info.writeFootprint.assign(writeFootprint->begin(), writeFootprint->end());

  auto ownerDims = readI64ArrayAttr(op.getOwnerDimsAttr());
  if (ownerDims && ownerDims->size() == rank)
    info.ownerDims.assign(ownerDims->begin(), ownerDims->end());
  else
    for (unsigned dim = 0; dim < rank; ++dim)
      info.ownerDims.push_back(dim);

  auto spatialDims = readI64ArrayAttr(op.getSpatialDimsAttr());
  if (spatialDims && spatialDims->size() == rank)
    info.spatialDims.assign(spatialDims->begin(), spatialDims->end());
  else
    for (unsigned dim = 0; dim < rank; ++dim)
      info.spatialDims.push_back(dim);

  return info;
}

static std::optional<SmallVector<int64_t, 4>>
deriveLexicographicWaveCoefficients(ArrayRef<int64_t> minOffsets,
                                    ArrayRef<int64_t> maxOffsets) {
  if (minOffsets.empty() || minOffsets.size() != maxOffsets.size())
    return std::nullopt;

  unsigned rank = minOffsets.size();
  SmallVector<int64_t, 4> maxAbs(rank, 0);
  bool sawCarriedOffset = false;
  for (unsigned dim = 0; dim < rank; ++dim) {
    if (minOffsets[dim] > maxOffsets[dim])
      return std::nullopt;
    maxAbs[dim] =
        std::max(std::abs(minOffsets[dim]), std::abs(maxOffsets[dim]));
    sawCarriedOffset |= maxAbs[dim] != 0;
  }
  if (!sawCarriedOffset)
    return std::nullopt;

  SmallVector<int64_t, 4> coefficients(rank, 1);
  for (int64_t dim = static_cast<int64_t>(rank) - 2; dim >= 0; --dim) {
    int64_t guard = 1;
    for (unsigned tail = dim + 1; tail < rank; ++tail) {
      if (maxAbs[tail] == 0)
        continue;
      if (coefficients[tail] >
          (std::numeric_limits<int64_t>::max() - guard) / maxAbs[tail])
        return std::nullopt;
      guard += coefficients[tail] * maxAbs[tail];
    }
    coefficients[dim] = guard;
  }
  return coefficients;
}

struct WavefrontSkewPlan {
  RectangularWavefrontLoopNest shape;
  sde::SuNeighborhoodAccessInfo neighborhood;
  StaticOutputStorageFacts outputStorage;
  SmallVector<int64_t, 4> waveCoefficients;
  int64_t targetComputeUnits = 1;
};

struct WavefrontOwnerStoragePlan {
  SmallVector<int64_t, 4> ownerPhysicalDims;
  SmallVector<int64_t, 4> physicalBlockShape;
  SmallVector<int64_t, 4> logicalWorkerSlice;
  SmallVector<int64_t, 4> haloShape;
};

static std::optional<WavefrontOwnerStoragePlan>
buildWavefrontOwnerStoragePlan(const WavefrontSkewPlan &plan) {
  unsigned rank = plan.shape.lowerBounds.size();
  if (rank < 2 || plan.outputStorage.shape.size() != rank ||
      plan.neighborhood.ownerDims.size() != rank ||
      plan.neighborhood.minOffsets.size() != rank ||
      plan.neighborhood.maxOffsets.size() != rank ||
      plan.shape.steps.size() != rank)
    return std::nullopt;

  SmallVector<char, 4> seen(rank, false);
  for (int64_t dim : plan.neighborhood.ownerDims) {
    if (dim < 0 || static_cast<unsigned>(dim) >= rank || seen[dim])
      return std::nullopt;
    seen[dim] = true;
  }

  WavefrontOwnerStoragePlan storage;
  storage.physicalBlockShape.assign(plan.outputStorage.shape.begin(),
                                    plan.outputStorage.shape.end());
  storage.ownerPhysicalDims.assign(plan.neighborhood.ownerDims.begin(),
                                   plan.neighborhood.ownerDims.end() - 1);
  if (storage.ownerPhysicalDims.empty())
    return std::nullopt;

  for (auto [slot, ownerDim] : llvm::enumerate(storage.ownerPhysicalDims)) {
    std::optional<int64_t> step =
        getPositiveConstantIndex(plan.shape.steps[slot]);
    if (!step || *step <= 0)
      return std::nullopt;
    if (ownerDim < 0 ||
        static_cast<size_t>(ownerDim) >= storage.physicalBlockShape.size() ||
        *step > storage.physicalBlockShape[ownerDim])
      return std::nullopt;
    storage.physicalBlockShape[ownerDim] = *step;

    int64_t halo =
        std::max<int64_t>(0, std::max(-plan.neighborhood.minOffsets[slot],
                                      plan.neighborhood.maxOffsets[slot]));
    storage.haloShape.push_back(halo);
  }

  storage.logicalWorkerSlice.assign(storage.physicalBlockShape.begin(),
                                    storage.physicalBlockShape.end());
  bool hasHalo =
      llvm::any_of(storage.haloShape, [](int64_t halo) { return halo > 0; });
  if (!hasHalo && storage.ownerPhysicalDims.size() == 1)
    (void)sde::buildBlockAlignedLogicalWorkerSlice(
        plan.outputStorage.shape, storage.ownerPhysicalDims,
        storage.physicalBlockShape,
        std::max<int64_t>(1, plan.targetComputeUnits),
        storage.logicalWorkerSlice);
  return storage;
}

static Value buildWeightedIndexSum(OpBuilder &builder, Location loc,
                                   ArrayRef<Value> values,
                                   ArrayRef<int64_t> coefficients) {
  assert(values.size() == coefficients.size() && "rank mismatch");
  Value sum;
  for (auto [value, coefficient] : llvm::zip(values, coefficients)) {
    Value term = value;
    if (coefficient != 1)
      term = arith::MulIOp::create(
          builder, loc, term, createConstantIndex(builder, loc, coefficient));
    sum = sum ? arith::AddIOp::create(builder, loc, sum, term) : term;
  }
  return sum ? sum : createZeroIndex(builder, loc);
}

static Value buildWavefrontLastCoordinate(OpBuilder &builder, Location loc,
                                          Value waveIv,
                                          ArrayRef<Value> leadingIvs,
                                          ArrayRef<int64_t> coefficients) {
  assert(!coefficients.empty() && coefficients.back() == 1 &&
         "last wave coefficient must be one");
  SmallVector<int64_t, 4> leadingCoefficients(coefficients.drop_back());
  Value leadingSum =
      buildWeightedIndexSum(builder, loc, leadingIvs, leadingCoefficients);
  return arith::SubIOp::create(builder, loc, waveIv, leadingSum);
}

static void cloneWavefrontBody(OpBuilder &builder, Block *sourceBlock,
                               ArrayRef<scf::ForOp> innerForChain,
                               unsigned depth, IRMapping &mapping) {
  scf::ForOp nestedFor =
      depth < innerForChain.size() ? innerForChain[depth] : scf::ForOp();
  for (Operation &bodyOp : sourceBlock->without_terminator()) {
    if (nestedFor && &bodyOp == nestedFor.getOperation()) {
      cloneWavefrontBody(builder, nestedFor.getBody(), innerForChain, depth + 1,
                         mapping);
      return;
    }
    builder.clone(bodyOp, mapping);
  }
}

static std::optional<WavefrontSkewPlan>
buildWavefrontSkewPlan(sde::SdeSuIterateOp op, sde::SDECostModel &costModel) {
  if (costModel.getLogicalWorkerCapacity() <= 1)
    return std::nullopt;
  if (!op || !op.getInPlaceSharedStateAttr() || !isInPlaceSelfReadStencil(op))
    return std::nullopt;

  RectangularWavefrontLoopNest shape;
  if (!collectRectangularWavefrontLoopNest(op, shape))
    return std::nullopt;

  std::optional<StaticOutputStorageFacts> storePlan =
      findSingleExternalStoreShape(op);
  if (!storePlan || storePlan->shape.size() < shape.lowerBounds.size())
    return std::nullopt;

  std::optional<sde::SuNeighborhoodAccessInfo> neighborhood =
      getWavefrontNeighborhood(op, shape.lowerBounds.size());
  if (!neighborhood)
    return std::nullopt;

  std::optional<SmallVector<int64_t, 4>> waveCoefficients =
      deriveLexicographicWaveCoefficients(neighborhood->minOffsets,
                                          neighborhood->maxOffsets);
  if (!waveCoefficients)
    return std::nullopt;

  WavefrontSkewPlan plan;
  plan.shape = std::move(shape);
  plan.neighborhood = std::move(*neighborhood);
  plan.outputStorage = std::move(*storePlan);
  plan.waveCoefficients = std::move(*waveCoefficients);
  plan.targetComputeUnits =
      std::max<int64_t>(1, costModel.getLogicalWorkerCapacity());
  return plan;
}

static sde::SdeSuIterateOp realizeWavefrontSkew(sde::SdeSuIterateOp op,
                                                WavefrontSkewPlan &plan) {
  OpBuilder builder(op);
  Location loc = op.getLoc();
  MLIRContext *ctx = op.getContext();

  Value one = createOneIndex(builder, loc);
  SmallVector<Value, 4> lastValues;
  lastValues.reserve(plan.shape.upperBounds.size());
  for (Value upper : plan.shape.upperBounds)
    lastValues.push_back(arith::SubIOp::create(builder, loc, upper, one));
  Value waveFirst = buildWeightedIndexSum(builder, loc, plan.shape.lowerBounds,
                                          plan.waveCoefficients);
  Value waveLast =
      buildWeightedIndexSum(builder, loc, lastValues, plan.waveCoefficients);
  Value waveUb = arith::AddIOp::create(builder, loc, waveLast, one);

  auto waveLoop = scf::ForOp::create(builder, loc, waveFirst, waveUb, one);
  OpBuilder::InsertionGuard waveGuard(builder);
  builder.setInsertionPointToStart(waveLoop.getBody());
  Value waveIv = waveLoop.getInductionVar();

  auto distribution = sde::SdeSuDistributeOp::create(
      builder, loc,
      sde::SdeDistributionKindAttr::get(
          ctx, sde::SdeDistributionKind::owner_compute));
  Block &distributionBody = sde::ensureBlock(distribution.getBody());
  builder.setInsertionPointToStart(&distributionBody);

  SmallVector<Value, 4> leadingLowerBounds(plan.shape.lowerBounds.begin(),
                                           plan.shape.lowerBounds.end() - 1);
  SmallVector<Value, 4> leadingUpperBounds(plan.shape.upperBounds.begin(),
                                           plan.shape.upperBounds.end() - 1);
  SmallVector<Value, 4> leadingSteps(plan.shape.steps.begin(),
                                     plan.shape.steps.end() - 1);
  ArrayAttr ownerDimsAttr = buildI64ArrayAttr(ctx, plan.neighborhood.ownerDims);
  ArrayAttr spatialDimsAttr =
      buildI64ArrayAttr(ctx, plan.neighborhood.spatialDims);
  ArrayAttr writeFootprintAttr =
      buildI64ArrayAttr(ctx, plan.neighborhood.writeFootprint);
  std::optional<WavefrontOwnerStoragePlan> storagePlan =
      buildWavefrontOwnerStoragePlan(plan);
  ArrayAttr physicalOwnerDimsAttr;
  ArrayAttr physicalBlockShapeAttr;
  ArrayAttr logicalWorkerSliceAttr;
  ArrayAttr physicalHaloShapeAttr;
  sde::SdeIterationTopologyAttr iterationTopologyAttr;
  if (storagePlan) {
    physicalOwnerDimsAttr =
        buildI64ArrayAttr(ctx, storagePlan->ownerPhysicalDims);
    physicalBlockShapeAttr =
        buildI64ArrayAttr(ctx, storagePlan->physicalBlockShape);
    logicalWorkerSliceAttr =
        buildI64ArrayAttr(ctx, storagePlan->logicalWorkerSlice);
    if (llvm::any_of(storagePlan->haloShape,
                     [](int64_t halo) { return halo > 0; }))
      physicalHaloShapeAttr = buildI64ArrayAttr(ctx, storagePlan->haloShape);
    iterationTopologyAttr = sde::SdeIterationTopologyAttr::get(
        ctx, storagePlan->ownerPhysicalDims.size() > 1
                 ? sde::SdeIterationTopology::owner_tile
                 : sde::SdeIterationTopology::owner_strip);
  }
  auto newOp = sde::SdeSuIterateOp::create(
      builder, loc, /*resultTypes=*/TypeRange{}, ValueRange(leadingLowerBounds),
      ValueRange(leadingUpperBounds), ValueRange(leadingSteps),
      op.getScheduleAttr(), op.getChunkSize(), op.getNowaitAttr(),
      op.getReductionAccumulators(), op.getReductionKindsAttr(),
      op.getReductionStrategyAttr(), op.getPartialReductionAttr(),
      op.getPartialReductionDimsAttr(), op.getPartialReductionOwnerDimsAttr(),
      op.getStructuredClassificationAttr(),
      sde::SdePatternAttr::get(ctx, sde::SdePattern::stencil_tiling_nd),
      buildI64ArrayAttr(ctx, plan.neighborhood.minOffsets),
      buildI64ArrayAttr(ctx, plan.neighborhood.maxOffsets), ownerDimsAttr,
      spatialDimsAttr, writeFootprintAttr, physicalOwnerDimsAttr,
      physicalBlockShapeAttr, logicalWorkerSliceAttr, physicalHaloShapeAttr,
      iterationTopologyAttr, op.getRepetitionStructureAttr(),
      op.getAsyncStrategyAttr(), /*distributionKind=*/nullptr,
      /*inPlaceSafe=*/nullptr, /*inPlaceSharedState=*/nullptr,
      /*arrayLayout=*/nullptr, /*layoutsDisagree=*/nullptr,
      /*commVolumeBytes=*/nullptr);

  Block &newBody = sde::ensureBlock(newOp.getBody());
  while (newBody.getNumArguments() < leadingLowerBounds.size())
    newBody.addArgument(builder.getIndexType(), loc);

  builder.setInsertionPointToStart(&newBody);
  auto newCuRegion = sde::SdeCuRegionOp::create(
      builder, loc, /*resultTypes=*/TypeRange{},
      sde::SdeCuKindAttr::get(ctx, sde::SdeCuKind::parallel),
      /*nowait=*/nullptr, /*iterArgs=*/ValueRange{});
  Block &newComputeBlock = sde::ensureBlock(newCuRegion.getBody());
  builder.setInsertionPointToStart(&newComputeBlock);

  SmallVector<Value, 4> candidateIvs;
  for (unsigned dim = 0; dim < leadingLowerBounds.size(); ++dim)
    candidateIvs.push_back(newBody.getArgument(dim));
  Value lastIv = buildWavefrontLastCoordinate(
      builder, loc, waveIv, candidateIvs, plan.waveCoefficients);
  candidateIvs.push_back(lastIv);

  Value geLower = arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::sge,
                                        lastIv, plan.shape.lowerBounds.back());
  Value ltUpper = arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::slt,
                                        lastIv, plan.shape.upperBounds.back());
  Value valid = arith::AndIOp::create(builder, loc, geLower, ltUpper);

  auto ifOp = scf::IfOp::create(builder, loc, TypeRange{}, valid,
                                /*withElseRegion=*/false);
  Block &thenBlock = ifOp.getThenRegion().front();
  builder.setInsertionPointToStart(&thenBlock);
  IRMapping mapper;
  for (auto [sourceIv, candidateIv] :
       llvm::zip(plan.shape.sourceIvs, candidateIvs))
    mapper.map(sourceIv, candidateIv);
  cloneWavefrontBody(builder, plan.shape.sourceComputeBlock,
                     plan.shape.innerForChain, 0, mapper);

  builder.setInsertionPointAfter(ifOp);
  sde::SdeYieldOp::create(builder, loc, ValueRange{});
  builder.setInsertionPointAfter(newCuRegion);
  sde::SdeYieldOp::create(builder, loc, ValueRange{});

  op.erase();
  return newOp;
}

static std::optional<sde::SdeSuIterateOp>
tryRealizeWavefrontSkew(sde::SdeSuIterateOp op, sde::SDECostModel &costModel) {
  if (!op || op->getParentOfType<sde::SdeSuDistributeOp>())
    return std::nullopt;
  std::optional<WavefrontSkewPlan> plan = buildWavefrontSkewPlan(op, costModel);
  if (!plan)
    return std::nullopt;
  return realizeWavefrontSkew(op, *plan);
}

static SmallVector<unsigned, 4>
chooseMappedSdeOwnerLoopDims(sde::SdeSuIterateOp op,
                             const sde::SuOutputLayoutFacts &plan) {
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
                              const sde::SuOutputLayoutFacts &outputPlan,
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

static bool buildOwnerDimPlan(const sde::LoopIndexedOutputShape &outputPlan,
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

static int64_t chooseLogicalTargetForTileFloor(
    ArrayRef<int64_t> shape, ArrayRef<int64_t> ownerPhysicalDims,
    ArrayRef<int64_t> physicalBlockShape, int64_t elemBytes,
    sde::SDECostModel &costModel, int64_t currentComputeUnits) {
  int64_t cuCount = sde::inferCuCountFromMuPartition(shape, ownerPhysicalDims,
                                                     physicalBlockShape);
  int64_t tileBytes = sde::tilePayloadBytes(physicalBlockShape, elemBytes);
  int64_t groupSize = chooseNeutralCuGroupSize(
      std::max<int64_t>(1, cuCount), tileBytes,
      costModel.getMinDistributedTileBytes(),
      std::max<int64_t>(1, costModel.getLogicalWorkerCapacity()));
  if (groupSize <= 1)
    return currentComputeUnits;
  return std::min<int64_t>(std::max<int64_t>(1, currentComputeUnits),
                           sde::ceilDivPositive(cuCount, groupSize));
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

static std::optional<sde::CuMuPartitionChoice> chooseCuMuTileFloorPlan(
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

static int64_t coarsenLoopIndexedOwnerPlanToTileFloor(
    const sde::LoopIndexedOutputShape &outputPlan, sde::SDECostModel &costModel,
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

  std::optional<sde::CuMuPartitionChoice> selected =
      chooseCuMuTileFloorPlan(outputPlan.shape, outputPlan.ownerPhysicalDims,
                              elemBytes, abstractCommVolumeBytes, costModel,
                              workers, physicalBlockShape, hyperedges, rebuild);
  if (!selected)
    return workers;

  ownerPhysicalDims.clear();
  ownerPhysicalDims.assign(outputPlan.ownerPhysicalDims.begin(),
                           outputPlan.ownerPhysicalDims.end());
  // The selected plan is a CU grouping target, not permission to inflate the
  // committed DB/MU block shape. SDE preserves the physical grain and projects
  // this lower compute-unit target into logicalWorkerSlice when committing
  // site.
  return selected->computeUnits;
}

static void
coarsenExistingLoopIndexedOwnerPlanToTileFloor(sde::SdeSuIterateOp op,
                                               sde::SDECostModel &costModel) {
  if (sde::hasCommittedCuMuPartitionFacts(op.getOperation()) ||
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

  std::optional<sde::LoopIndexedOutputShape> outputPlan =
      sde::findConsistentLoopIndexedOutputShapeWithOwnerDims(op);
  if (!outputPlan)
    outputPlan = sde::findLoopIndexedOutputShape(op);
  if (!outputPlan || outputPlan->shape.empty())
    return;

  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(op.getPhysicalOwnerDimsAttr());
  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(op.getPhysicalBlockShapeAttr());
  if (!ownerDims || ownerDims->empty() || !blockShape ||
      blockShape->size() != outputPlan->shape.size())
    return;

  sde::LoopIndexedOutputShape plan = *outputPlan;
  plan.ownerPhysicalDims.assign(ownerDims->begin(), ownerDims->end());
  int64_t workers = sde::inferCuCountFromMuPartition(
      plan.shape, plan.ownerPhysicalDims, *blockShape);
  if (workers <= 1)
    return;

  SmallVector<int64_t, 4> coarsenedOwnerDims(ownerDims->begin(),
                                             ownerDims->end());
  SmallVector<int64_t, 4> physicalBlockShape(blockShape->begin(),
                                             blockShape->end());
  int64_t coarsenedWorkers = coarsenLoopIndexedOwnerPlanToTileFloor(
      plan, costModel, workers, readAbstractCommVolumeBytes(op),
      collectAbstractMuHyperedges(op), coarsenedOwnerDims, physicalBlockShape);
  int64_t elemBytes = outputElementBytes(plan.root);
  if (elemBytes > 0)
    coarsenedWorkers = chooseLogicalTargetForTileFloor(
        plan.shape, coarsenedOwnerDims, physicalBlockShape, elemBytes,
        costModel, coarsenedWorkers);
  if (coarsenedWorkers >= workers)
    return;

  alignLateOwnerShapeToExistingStep(op, coarsenedOwnerDims, physicalBlockShape);
  SmallVector<int64_t, 4> logicalWorkerSlice =
      buildLogicalWorkerSliceOrPhysical(op, plan.shape, coarsenedOwnerDims,
                                        physicalBlockShape, coarsenedWorkers);
  op.setPhysicalOwnerDimsAttr(
      buildI64ArrayAttr(op.getContext(), coarsenedOwnerDims));
  op.setPhysicalBlockShapeAttr(
      buildI64ArrayAttr(op.getContext(), physicalBlockShape));
  op.setLogicalWorkerSliceAttr(
      buildI64ArrayAttr(op.getContext(), logicalWorkerSlice));
  sde::reconcileArrayLayoutWithCommittedPhysicalShape(op);
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
    if (ownerDims.size() > ownerIndexValues.size())
      return WalkResult::interrupt();
    for (auto [ownerSlot, rawDim] : llvm::enumerate(ownerDims)) {
      if (rawDim < 0 || static_cast<size_t>(rawDim) >= indices.size())
        return WalkResult::interrupt();
      if (!sde::isOwnerDependentIndex(indices[rawDim],
                                      ownerIndexValues[ownerSlot]))
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

static std::optional<sde::LoopIndexedOutputShape>
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
  std::optional<sde::LoopIndexedOutputShape> selectedPlan;
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

    std::optional<SmallVector<int64_t, 4>> ownerPhysicalDims =
        sde::collectOwnerIndexedPhysicalDimsByOwnerOrder(storeOp.getIndices(),
                                                         ownerIndexValues);
    if (!ownerPhysicalDims || ownerPhysicalDims->size() < 2) {
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

    sde::LoopIndexedOutputShape candidate{base, std::move(shape),
                                          std::move(*ownerPhysicalDims)};
    if (!selectedPlan) {
      selectedPlan = std::move(candidate);
      return WalkResult::advance();
    }

    auto selectedType = dyn_cast<MemRefType>(selectedPlan->root.getType());
    if (!selectedType || candidate.shape != selectedPlan->shape ||
        memRefType.getElementType() != selectedType.getElementType() ||
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
                              ArrayRef<int64_t> haloShape = {},
                              ArrayRef<int64_t> logicalWorkerSlice = {}) {
  sde::rewriteWriterArrayLayoutToPhysicalShape(op, ownerDims,
                                               physicalBlockShape);

  op.setPhysicalOwnerDimsAttr(buildI64ArrayAttr(op.getContext(), ownerDims));
  op.setPhysicalBlockShapeAttr(
      buildI64ArrayAttr(op.getContext(), physicalBlockShape));
  if (llvm::any_of(haloShape, [](int64_t halo) { return halo > 0; }))
    op.setPhysicalHaloShapeAttr(buildI64ArrayAttr(op.getContext(), haloShape));
  ArrayRef<int64_t> logicalSlice =
      logicalWorkerSlice.empty() ? physicalBlockShape : logicalWorkerSlice;
  op.setLogicalWorkerSliceAttr(
      buildI64ArrayAttr(op.getContext(), logicalSlice));
  op.setIterationTopologyAttr(sde::SdeIterationTopologyAttr::get(
      op.getContext(), ownerDims.size() > 1
                           ? sde::SdeIterationTopology::owner_tile
                           : sde::SdeIterationTopology::owner_strip));
}

static bool allowsGroupedLogicalWorkerSlice(sde::SdeSuIterateOp op,
                                            ArrayRef<int64_t> haloShape = {}) {
  if (llvm::any_of(haloShape, [](int64_t halo) { return halo > 0; }))
    return false;
  auto classification = op.getStructuredClassification();
  return !classification ||
         *classification != sde::SdeStructuredClassification::stencil;
}

static SmallVector<int64_t, 4> buildLogicalWorkerSliceOrPhysical(
    sde::SdeSuIterateOp op, ArrayRef<int64_t> shape,
    ArrayRef<int64_t> ownerDims, ArrayRef<int64_t> physicalBlockShape,
    int64_t targetComputeUnits, ArrayRef<int64_t> haloShape) {
  SmallVector<int64_t, 4> logicalWorkerSlice(physicalBlockShape.begin(),
                                             physicalBlockShape.end());
  if (!allowsGroupedLogicalWorkerSlice(op, haloShape))
    return logicalWorkerSlice;
  if (!sde::buildBlockAlignedLogicalWorkerSlice(
          shape, ownerDims, physicalBlockShape, targetComputeUnits,
          logicalWorkerSlice))
    logicalWorkerSlice.assign(physicalBlockShape.begin(),
                              physicalBlockShape.end());
  return logicalWorkerSlice;
}

static bool hasCommittedPhysicalLayout(sde::SdeSuIterateOp op) {
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
  llvm::SmallDenseSet<int64_t, 4> writtenIds;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    if (fact.role != sde::LayoutGraphRole::write || fact.ownerDims.empty() ||
        fact.blockShape.empty())
      continue;
    if (fact.id < 0 || !writtenIds.insert(fact.id).second)
      return std::nullopt;
    if (!selected) {
      selected = fact;
      continue;
    }
    if (selected->layoutKind != fact.layoutKind ||
        selected->ownerDims != fact.ownerDims ||
        selected->blockShape != fact.blockShape ||
        selected->budgetBlockShape != fact.budgetBlockShape)
      return std::nullopt;
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
    if (!memrefType) {
      rejected = true;
      return;
    }
    if (memrefType.getRank() == 0)
      return;

    sawExternalStore = true;
    OperandRange indices = storeOp.getIndices();
    if (ownerDims.size() > loopIvs->size()) {
      rejected = true;
      return;
    }
    for (auto [ownerSlot, ownerDim] : llvm::enumerate(ownerDims)) {
      if (ownerDim < 0 || static_cast<unsigned>(ownerDim) >= indices.size()) {
        rejected = true;
        return;
      }
      if (!sde::isOwnerDependentIndex(indices[ownerDim],
                                      (*loopIvs)[ownerSlot])) {
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
  if (writeLayout->ownerDims.size() != ownerDims.size())
    return false;
  SmallVector<int64_t, 4> layoutDims(writeLayout->ownerDims.begin(),
                                     writeLayout->ownerDims.end());
  SmallVector<int64_t, 4> committedDims(ownerDims.begin(), ownerDims.end());
  llvm::sort(layoutDims);
  llvm::sort(committedDims);
  return llvm::equal(layoutDims, committedDims);
}

static std::optional<SmallVector<int64_t, 4>>
orderPhysicalOwnerDimsByLoop(const sde::SuOutputLayoutFacts &outputPlan,
                             ArrayRef<int64_t> layoutOwnerDims,
                             unsigned loopRank) {
  if (layoutOwnerDims.empty() || outputPlan.loopDimToPhysicalDim.empty())
    return std::nullopt;

  SmallVector<int64_t, 4> orderedOwnerDims;
  orderedOwnerDims.reserve(layoutOwnerDims.size());
  for (unsigned loopDim = 0; loopDim < loopRank; ++loopDim) {
    if (loopDim >= outputPlan.loopDimToPhysicalDim.size())
      return std::nullopt;
    int64_t physicalDim = outputPlan.loopDimToPhysicalDim[loopDim];
    if (physicalDim < 0)
      continue;
    if (static_cast<size_t>(physicalDim) >= outputPlan.shape.size())
      return std::nullopt;
    if (llvm::is_contained(layoutOwnerDims, physicalDim))
      orderedOwnerDims.push_back(physicalDim);
  }
  if (orderedOwnerDims.size() != layoutOwnerDims.size())
    return std::nullopt;
  return orderedOwnerDims;
}

static bool
commitPhysicalLayoutFromAssignedLayout(sde::SdeSuIterateOp op,
                                       sde::SDECostModel &costModel) {
  if (!op || hasCommittedPhysicalLayout(op))
    return false;

  auto classification = op.getStructuredClassification();
  if (classification &&
      *classification == sde::SdeStructuredClassification::stencil)
    return false;

  std::optional<sde::LayoutGraphFact> writeLayout =
      selectSingleWriteLayoutFact(op);
  if (!writeLayout)
    return false;

  std::optional<sde::LoopIndexedOutputShape> outputPlan =
      sde::findConsistentLoopIndexedOutputShapeWithOwnerDims(op);
  if (!outputPlan)
    outputPlan = sde::findLoopIndexedOutputShape(op);
  if (!outputPlan || outputPlan->shape.empty())
    return false;

  sde::LoopIndexedOutputShape plan = *outputPlan;
  if (std::optional<sde::SuOutputLayoutFacts> structuredPlan =
          sde::findCompatibleSuOutputLayoutFacts(op)) {
    std::optional<SmallVector<int64_t, 4>> orderedOwnerDims =
        orderPhysicalOwnerDimsByLoop(*structuredPlan, writeLayout->ownerDims,
                                     op.getLowerBounds().size());
    if (!orderedOwnerDims)
      return false;
    plan.ownerPhysicalDims.assign(orderedOwnerDims->begin(),
                                  orderedOwnerDims->end());
  } else {
    plan.ownerPhysicalDims.assign(writeLayout->ownerDims.begin(),
                                  writeLayout->ownerDims.end());
  }
  if (!allExternalStoresCoverOwnerDims(op, plan.ownerPhysicalDims))
    return false;

  int64_t workers =
      std::max<int64_t>(1, getInterLocalityTargetWorkers(costModel));
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> physicalBlockShape;
  if (!buildOwnerDimPlan(plan, workers, ownerDims, physicalBlockShape))
    return false;

  int64_t logicalTarget = coarsenLoopIndexedOwnerPlanToTileFloor(
      plan, costModel, workers, readAbstractCommVolumeBytes(op),
      collectAbstractMuHyperedges(op), ownerDims, physicalBlockShape);
  int64_t elemBytes = outputElementBytes(plan.root);
  if (elemBytes > 0)
    logicalTarget = chooseLogicalTargetForTileFloor(
        plan.shape, ownerDims, physicalBlockShape, elemBytes, costModel,
        logicalTarget);
  alignLateOwnerShapeToExistingStep(op, ownerDims, physicalBlockShape);
  SmallVector<int64_t, 4> logicalWorkerSlice =
      buildLogicalWorkerSliceOrPhysical(op, plan.shape, ownerDims,
                                        physicalBlockShape, logicalTarget);
  return applyPhysicalLayoutIfRealized(op, ownerDims, physicalBlockShape,
                                       /*haloShape=*/{}, logicalWorkerSlice);
}

// Consume the one committed node-agnostic budget layout for every SU that
// writes a multi-owner-distributed data-parallel array, committing identical
// physicalOwnerDims + physicalBlockShape (+ logicalWorkerSlice) across all
// writers of that array. That equality lets the per-timestep host bridge hoist
// once and keeps iterative double-buffer stencils from realizing a coarse
// per-timestep copy. Runs first in the layout commit dispatch and is the
// default for the multi-owner data-parallel family (matmul/contraction
// excluded). Realization is not gated on the loop step: the committed layout is
// the authority and the iteration-space decomposition re-tiles to the block.
static bool commitBudgetReconciledLayout(sde::SdeSuIterateOp op,
                                         sde::SDECostModel &costModel) {
  if (!op || hasCommittedPhysicalLayout(op))
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
  // promoted cannot carry a multi-owner tile; leave it to the pattern
  // committers rather than committing unverifiable owner_tile facts.
  if (op.getLowerBounds().size() < writeLayout->ownerDims.size())
    return false;
  std::optional<sde::SuOutputLayoutFacts> outputPlan =
      sde::findCompatibleSuOutputLayoutFacts(op);
  if (!outputPlan)
    return false;
  std::optional<SmallVector<int64_t, 4>> orderedOwnerDims =
      orderPhysicalOwnerDimsByLoop(*outputPlan, writeLayout->ownerDims,
                                   op.getLowerBounds().size());
  if (!orderedOwnerDims)
    return false;
  SmallVector<int64_t, 4> ownerDims(orderedOwnerDims->begin(),
                                    orderedOwnerDims->end());
  if (!allExternalStoresCoverOwnerDims(op, ownerDims))
    return false;
  SmallVector<int64_t, 4> blockShape(writeLayout->budgetBlockShape.begin(),
                                     writeLayout->budgetBlockShape.end());
  if (!sde::enforceOwnerBlockConcurrencyFloor(
          outputPlan->shape, ownerDims,
          getInterLocalityTargetWorkers(costModel), blockShape))
    return false;
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
  SmallVector<int64_t, 4> logicalWorkerSlice =
      buildLogicalWorkerSliceOrPhysical(
          op, outputPlan->shape, ownerDims, blockShape,
          costModel.getLogicalWorkerCapacity(),
          anyHalo ? ArrayRef<int64_t>(haloShape) : ArrayRef<int64_t>{});
  applyPhysicalPlan(op, ownerDims, blockShape,
                    anyHalo ? ArrayRef<int64_t>(haloShape)
                            : ArrayRef<int64_t>{},
                    logicalWorkerSlice);
  return true;
}

static bool mayRefineCommittedPhysicalLayout(sde::SdeSuIterateOp op) {
  if (sde::hasCommittedCuMuPartitionFacts(op.getOperation()))
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
alignLateOwnerShapeToExistingStep(sde::SdeSuIterateOp op,
                                  ArrayRef<int64_t> ownerDims,
                                  MutableArrayRef<int64_t> physicalBlockShape) {
  if (ownerDims.empty() || op.getSteps().empty() ||
      ownerDims.size() > op.getSteps().size())
    return;

  // DistributionPlanning runs after SDE loop tiling. When it authors physical
  // owner layout late, the element-space block for each owner dimension must
  // match the already-realized owner-loop step. A coarser block would serialize
  // independent tiled loops behind one DB/MU; a finer block would describe
  // slices the loop no longer realizes.
  for (auto [ownerSlot, ownerPhysicalDim] : llvm::enumerate(ownerDims)) {
    if (ownerPhysicalDim < 0 ||
        static_cast<size_t>(ownerPhysicalDim) >= physicalBlockShape.size())
      return;

    std::optional<int64_t> ownerStep =
        getPositiveConstantIndex(op.getSteps()[ownerSlot]);
    if (!ownerStep || *ownerStep <= 1)
      continue;

    physicalBlockShape[ownerPhysicalDim] = *ownerStep;
  }
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
physicalLayoutMatchesRealizedLoopSteps(sde::SdeSuIterateOp op,
                                       ArrayRef<int64_t> ownerDims,
                                       ArrayRef<int64_t> physicalBlockShape) {
  if (!op || ownerDims.empty() || physicalBlockShape.empty() ||
      op.getSteps().empty())
    return false;

  // Once SDE has committed owner-tile/strip physical facts, the SU loop step
  // operands are the realized owner-block schedule. Later structured analysis
  // may see the inner element loops introduced by tiling, so validate the
  // committed owner step order before consulting access-derived maps.
  if (physicalPlanMatchesOwnerStepOrder(op, ownerDims, physicalBlockShape))
    return true;

  SmallVector<int64_t, 4> physicalDimToLoopDim(physicalBlockShape.size(), -1);
  if (std::optional<sde::SuOutputLayoutFacts> layoutPlan =
          sde::findCompatibleSuOutputLayoutFacts(op)) {
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

static bool applyPhysicalLayoutIfRealized(
    sde::SdeSuIterateOp op, ArrayRef<int64_t> ownerDims,
    ArrayRef<int64_t> physicalBlockShape, ArrayRef<int64_t> haloShape,
    ArrayRef<int64_t> logicalWorkerSlice) {
  if (!physicalLayoutMatchesRealizedLoopSteps(op, ownerDims,
                                              physicalBlockShape))
    return false;
  applyPhysicalPlan(op, ownerDims, physicalBlockShape, haloShape,
                    logicalWorkerSlice);
  return true;
}

static void commitStencilPhysicalLayout(sde::SdeSuIterateOp op,
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
  if (isStencil && sde::requiresNestedStencilOwnerPromotion(op) &&
      !sde::hasRealizableOwnerStrip(op))
    return;

  /// First-dimension unclassified loops are owned by the uniform/matmul
  /// planners below. This secondary owner-dim path handles imperfect local
  /// stencil/update nests whose owner IV indexes a later physical output
  /// dimension.
  if (!isStencil && sde::findLoopIndexedOutputShape(op))
    return;

  if (isStencil) {
    std::optional<sde::SuOutputLayoutFacts> outputPlan =
        sde::findCompatibleSuOutputLayoutFacts(op);
    if (outputPlan) {
      SmallVector<unsigned, 4> ownerLoopDims =
          chooseMappedSdeOwnerLoopDims(op, *outputPlan);
      if (ownerLoopDims.size() > op.getLowerBounds().size())
        return;
      int64_t workers = std::max<int64_t>(1, getStencilWorkerTarget(costModel));
      // One-dimensional halo stencils form a dependency pipeline between
      // neighboring owner blocks. Planning modestly more owner blocks than
      // logical worker capacity gives later scheduling enough ready tasks
      // without flooding target realization with tiny stencil slices.
      if (ownerLoopDims.size() == 1)
        workers *= 2;
      SmallVector<int64_t, 4> ownerDims;
      SmallVector<int64_t, 4> physicalBlockShape;
      SmallVector<int64_t, 4> haloShape;
      if (!buildOwnerDimPlan(op, *outputPlan, ownerLoopDims, workers, ownerDims,
                             physicalBlockShape, haloShape))
        return;

      // Keep halo stencil DB/MU grain fine. Grouped halo compute needs
      // lane-specific halo acquire realization in ARTS, so SDE must not
      // satisfy a tile-byte floor by inflating the physical owner block here.

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

      (void)applyPhysicalLayoutIfRealized(op, ownerDims, physicalBlockShape,
                                          haloShape);
      return;
    }
  }

  std::optional<sde::LoopIndexedOutputShape> secondaryPlan =
      sde::findConsistentLoopIndexedOutputShapeWithOwnerDims(op);
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

  // Preserve fine DB/MU grain for stencil-like updates. Coarser halo compute
  // grouping must be backed by explicit grouped halo realization in ARTS.

  alignLateOwnerShapeToExistingStep(op, ownerDims, physicalBlockShape);
  (void)applyPhysicalLayoutIfRealized(op, ownerDims, physicalBlockShape);
}

static void commitUniformPhysicalLayout(sde::SdeSuIterateOp op,
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
  std::optional<sde::LoopIndexedOutputShape> outputPlan =
      sde::findLoopIndexedOutputShape(op);
  bool usedConsistentOwnerQuery = false;
  if (!outputPlan) {
    if (classification &&
        *classification != sde::SdeStructuredClassification::elementwise &&
        *classification !=
            sde::SdeStructuredClassification::elementwise_pipeline)
      return;
    outputPlan = sde::findConsistentLoopIndexedOutputShapeWithOwnerDims(op);
    if (outputPlan && outputPlan->ownerPhysicalDims.size() != 1)
      outputPlan.reset();
    usedConsistentOwnerQuery = outputPlan.has_value();
  }
  if (!outputPlan || outputPlan->shape.empty() ||
      outputPlan->ownerPhysicalDims.empty())
    return;

  if (classification &&
      (*classification == sde::SdeStructuredClassification::elementwise ||
       *classification ==
           sde::SdeStructuredClassification::elementwise_pipeline)) {
    std::optional<sde::LoopIndexedOutputShape> multiOwnerPlan =
        findConsistentMultiOwnerOutputPlan(op);
    if (multiOwnerPlan && multiOwnerPlan->ownerPhysicalDims.size() >= 2 &&
        assignedWriteLayoutMatchesOwnerDims(
            op, multiOwnerPlan->ownerPhysicalDims) &&
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
            int64_t logicalTarget = coarsenLoopIndexedOwnerPlanToTileFloor(
                *multiOwnerPlan, costModel, workers,
                readAbstractCommVolumeBytes(op),
                collectAbstractMuHyperedges(op), ownerDims, physicalBlockShape);
            alignLateOwnerShapeToExistingStep(op, ownerDims,
                                              physicalBlockShape);
            logicalTarget = std::min<int64_t>(
                logicalTarget,
                std::max<int64_t>(1, costModel.getLogicalWorkerCapacity()));
            SmallVector<int64_t, 4> logicalWorkerSlice =
                buildLogicalWorkerSliceOrPhysical(op, multiOwnerPlan->shape,
                                                  ownerDims, physicalBlockShape,
                                                  logicalTarget);
            if (applyPhysicalLayoutIfRealized(op, ownerDims, physicalBlockShape,
                                              /*haloShape=*/{},
                                              logicalWorkerSlice))
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
  if (!classification || usedConsistentOwnerQuery) {
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
    std::optional<sde::CuMuPartitionChoice> selected = chooseCuMuTileFloorPlan(
        outputPlan->shape, outputPlan->ownerPhysicalDims, elemBytes,
        readAbstractCommVolumeBytes(op), costModel, workers, physicalBlockShape,
        collectAbstractMuHyperedges(op), rebuild);
    if (selected)
      workers = selected->computeUnits;
    workers = chooseLogicalTargetForTileFloor(
        outputPlan->shape, outputPlan->ownerPhysicalDims, physicalBlockShape,
        elemBytes, costModel, workers);
  }
  alignLateOwnerShapeToExistingStep(op, outputPlan->ownerPhysicalDims,
                                    physicalBlockShape);
  SmallVector<int64_t, 4> logicalWorkerSlice =
      buildLogicalWorkerSliceOrPhysical(op, outputPlan->shape,
                                        outputPlan->ownerPhysicalDims,
                                        physicalBlockShape, workers);

  if (!classification)
    op.setStructuredClassificationAttr(
        sde::SdeStructuredClassificationAttr::get(
            op.getContext(), sde::SdeStructuredClassification::elementwise));
  applyPhysicalPlan(op, outputPlan->ownerPhysicalDims, physicalBlockShape,
                    /*haloShape=*/{}, logicalWorkerSlice);
}

static void commitMatmulPhysicalLayout(sde::SdeSuIterateOp op,
                                       sde::SDECostModel &costModel) {
  if ((op.getPhysicalOwnerDimsAttr() && op.getPhysicalBlockShapeAttr()) ||
      costModel.getLogicalWorkerCapacity() <= 1)
    return;
  auto classification = op.getStructuredClassification();
  if (!classification ||
      *classification != sde::SdeStructuredClassification::matmul)
    return;
  std::optional<sde::LoopIndexedOutputShape> outputPlan =
      sde::findLoopIndexedOutputShape(op);
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
      std::optional<sde::CuMuPartitionChoice> selected =
          chooseCuMuTileFloorPlan(outputPlan->shape, ownerDims, elemBytes,
                                  readAbstractCommVolumeBytes(op), costModel,
                                  workers, physicalBlockShape,
                                  collectAbstractMuHyperedges(op), rebuild);
      if (selected)
        workers = selected->computeUnits;
    }
  }
  SmallVector<int64_t, 2> ownerDims{0, 1};
  SmallVector<int64_t, 4> logicalWorkerSlice =
      buildLogicalWorkerSliceOrPhysical(op, outputPlan->shape, ownerDims,
                                        physicalBlockShape, workers);

  op.setPhysicalOwnerDimsAttr(buildI64ArrayAttr(op.getContext(), ownerDims));
  op.setPhysicalBlockShapeAttr(
      buildI64ArrayAttr(op.getContext(), physicalBlockShape));
  op.setLogicalWorkerSliceAttr(
      buildI64ArrayAttr(op.getContext(), logicalWorkerSlice));
  op.setIterationTopologyAttr(sde::SdeIterationTopologyAttr::get(
      op.getContext(), sde::SdeIterationTopology::owner_tile_2d));
  sde::reconcileArrayLayoutWithCommittedPhysicalShape(op);
}

static void commitDirectRowMatmulPhysicalLayout(sde::SdeSuIterateOp op,
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
  std::optional<sde::LoopIndexedOutputShape> outputPlan =
      sde::findLoopIndexedOutputShape(op);
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
  int64_t logicalTarget = coarsenLoopIndexedOwnerPlanToTileFloor(
      *outputPlan, costModel, workers, readAbstractCommVolumeBytes(op),
      collectAbstractMuHyperedges(op), ownerDims, physicalBlockShape);
  alignLateOwnerShapeToExistingStep(op, outputPlan->ownerPhysicalDims,
                                    physicalBlockShape);
  SmallVector<int64_t, 4> logicalWorkerSlice =
      buildLogicalWorkerSliceOrPhysical(op, outputPlan->shape, ownerDims,
                                        physicalBlockShape, logicalTarget);
  (void)applyPhysicalLayoutIfRealized(op, ownerDims, physicalBlockShape,
                                      /*haloShape=*/{}, logicalWorkerSlice);
}

static void commitReductionTaskShape(sde::SdeSuIterateOp op,
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
    std::optional<sde::LoopIndexedOutputShape> outputPlan =
        sde::findLoopIndexedOutputShape(op);
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
      int64_t logicalTarget = coarsenLoopIndexedOwnerPlanToTileFloor(
          *outputPlan, costModel, targetTasks, readAbstractCommVolumeBytes(op),
          collectAbstractMuHyperedges(op), ownerDims, physicalBlockShape);
      SmallVector<int64_t, 4> logicalWorkerSlice =
          buildLogicalWorkerSliceOrPhysical(op, outputPlan->shape, ownerDims,
                                            physicalBlockShape, logicalTarget);
      op.setPhysicalOwnerDimsAttr(
          buildI64ArrayAttr(op.getContext(), ownerDims));
      op.setPhysicalBlockShapeAttr(
          buildI64ArrayAttr(op.getContext(), physicalBlockShape));
      op.setLogicalWorkerSliceAttr(
          buildI64ArrayAttr(op.getContext(), logicalWorkerSlice));
      op.setIterationTopologyAttr(sde::SdeIterationTopologyAttr::get(
          op.getContext(), sde::SdeIterationTopology::owner_strip));
      sde::reconcileArrayLayoutWithCommittedPhysicalShape(op);
      return;
    }
  }

  op.setLogicalWorkerSliceAttr(
      buildI64ArrayAttr(op.getContext(), SmallVector<int64_t, 1>{slice}));
  op.setIterationTopologyAttr(sde::SdeIterationTopologyAttr::get(
      op.getContext(), sde::SdeIterationTopology::owner_strip));
}

static void
commitInPlaceSharedStencilSerialSlice(sde::SdeSuIterateOp op,
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
  if (op->getParentOfType<sde::SdeSuDistributeOp>())
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
  sde::reconcileArrayLayoutWithCommittedPhysicalShape(op);
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
    std::optional<sde::LoopIndexedOutputShape> outputPlan =
        sde::findLoopIndexedOutputShape(op);
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
    if (sde::requiresNestedStencilOwnerPromotion(op) &&
        !sde::hasRealizableOwnerStrip(op))
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

// Multi-worker in-place neighborhood stencils need a wavefront/skew transform;
// without one, SDE may only preserve the serial single-worker lowering.
static bool
requiresUnimplementedStencilWavefront(sde::SdeSuIterateOp op,
                                      sde::SDECostModel &costModel) {
  if (costModel.getLogicalWorkerCapacity() <= 1)
    return false;
  if (op->getParentOfType<sde::SdeSuDistributeOp>())
    return false;
  return op.getInPlaceSharedStateAttr() && !op.getDistributionKindAttr();
}

static std::string formatI64Array(ArrayAttr attr) {
  std::string text;
  llvm::raw_string_ostream os(text);
  os << '[';
  if (auto values = readI64ArrayAttr(attr))
    llvm::interleaveComma(*values, os);
  os << ']';
  return os.str();
}

// Cite committed SDE facts so downstream layers cannot reinterpret this case.
static void emitStencilWavefrontFailClosed(sde::SdeSuIterateOp op) {
  op.emitOpError()
      << "in-place self-read stencil (Gauss-Seidel family) has loop-carried "
         "neighbor offsets min="
      << formatI64Array(op.getAccessMinOffsetsAttr())
      << " max=" << formatI64Array(op.getAccessMaxOffsetsAttr())
      << " on owner dims " << formatI64Array(op.getOwnerDimsAttr())
      << "; exposing legal parallelism requires an SDE wavefront/skew "
         "(loop-skewing) transform that is not implemented. Distributing in "
         "place without it would violate Gauss-Seidel ordering; preserving "
         "the order is serial, so the planner refuses multi-worker lowering. "
         "Implement the SDE wavefront/skew transform, "
         "prove the loop in-place-safe, or compile with a single logical "
         "worker.";
}

struct DistributionPlanningPass
    : public sde::impl::DistributionPlanningBase<DistributionPlanningPass> {
  explicit DistributionPlanningPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    if (!costModel)
      return;

    SmallVector<DistributionRewrite> rewrites;
    bool failed = false;
    auto chooseDistributionOrFailClosed = [&](sde::SdeSuIterateOp op) {
      if (auto kind = chooseDistributionKind(op, *costModel)) {
        rewrites.push_back({op, *kind});
        return;
      }
      if (requiresUnimplementedStencilWavefront(op, *costModel)) {
        emitStencilWavefrontFailClosed(op);
        failed = true;
      }
    };
    SmallVector<sde::SdeSuIterateOp, 16> iterates;
    getOperation().walk(
        [&](sde::SdeSuIterateOp op) { iterates.push_back(op); });

    for (sde::SdeSuIterateOp original : iterates) {
      if (!original || !original->getBlock())
        continue;
      sde::SdeSuIterateOp op = original;
      if (std::optional<sde::SdeSuIterateOp> wavefront =
              tryRealizeWavefrontSkew(op, *costModel)) {
        (void)*wavefront;
        continue;
      }

      if (op.getDistributionKindAttr()) {
        chooseDistributionOrFailClosed(op);
        continue;
      }

      if (hasCommittedPhysicalLayout(op)) {
        if (mayRefineCommittedPhysicalLayout(op)) {
          coarsenExistingLoopIndexedOwnerPlanToTileFloor(op, *costModel);
        }
        sde::reconcileArrayLayoutWithCommittedPhysicalShape(op);
        chooseDistributionOrFailClosed(op);
        continue;
      }

      coarsenExistingLoopIndexedOwnerPlanToTileFloor(op, *costModel);
      // Budget-reconciled layout is authored first so the per-pattern
      // committers below see an already-committed multi-owner data-parallel SU
      // and skip it; they still run for the families budget declines
      // (single-owner, matmul, reduction, in-place). The committed budget
      // layout is the authority.
      commitBudgetReconciledLayout(op, *costModel);
      commitStencilPhysicalLayout(op, *costModel);
      commitDirectRowMatmulPhysicalLayout(op, *costModel);
      commitMatmulPhysicalLayout(op, *costModel);
      commitUniformPhysicalLayout(op, *costModel);
      commitReductionTaskShape(op, *costModel);
      commitInPlaceSharedStencilSerialSlice(op, *costModel);
      commitPhysicalLayoutFromAssignedLayout(op, *costModel);
      chooseDistributionOrFailClosed(op);
    }

    if (failed) {
      signalPassFailure();
      return;
    }

    for (DistributionRewrite rewrite : rewrites) {
      if (rewrite.op.getNumResults() > 0) {
        // su_iterate with iter_arg results: can't wrap in su_distribute
        // (NoTerminator op can't forward results). Commit the distribution kind
        // directly on the su_iterate; boundary lowering carries that fact
        // forward.
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
