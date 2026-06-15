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
#include "carts/dialect/sde/Transforms/effect/distribution/DistributionLayoutUtils.h"
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
#include <numeric>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::sde::distribution;

namespace {

struct DistributionRewrite {
  sde::SdeSuIterateOp op;
  sde::SdeDistributionKind kind = sde::SdeDistributionKind::blocked;
};

static void
alignLateOwnerShapeToExistingStep(sde::SdeSuIterateOp op,
                                  ArrayRef<int64_t> ownerPhysicalDims,
                                  MutableArrayRef<int64_t> physicalBlockShape);

// Worker target for the stencil physical-layout committer. Caps at logical
// worker capacity because stencil halo ownership is currently a single-locality
// layout fact. Inter-locality expansion belongs after the dialect boundary has
// a concrete halo path.
static int64_t getStencilWorkerTarget(sde::SDECostModel &costModel) {
  return costModel.getLogicalWorkerCapacity();
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
  std::optional<sde::SuNeighborhoodAccessInfo> neighborhood =
      sde::queryNeighborhoodAccessInfo(op);
  if (!neighborhood)
    return 0;

  for (auto [idx, rawDim] : llvm::enumerate(neighborhood->ownerDims)) {
    if (rawDim < 0 || static_cast<unsigned>(rawDim) != ownerDim)
      continue;
    if (idx >= neighborhood->minOffsets.size() ||
        idx >= neighborhood->maxOffsets.size())
      return 0;
    return std::max<int64_t>(0, std::max(-neighborhood->minOffsets[idx],
                                         neighborhood->maxOffsets[idx]));
  }
  return 0;
}

static bool isInPlaceSelfReadStencil(sde::SdeSuIterateOp op) {
  auto classification = sde::queryStructuredClassification(op);
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
  if (op.getNumResults() != 0 || !op.getReductionAccumulators().empty() ||
      op.getReductionKindsAttr())
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
  std::optional<sde::SuNeighborhoodAccessInfo> info =
      sde::queryNeighborhoodAccessInfo(op);
  if (!info || info->minOffsets.size() != rank ||
      info->maxOffsets.size() != rank || info->writeFootprint.size() != rank)
    return std::nullopt;
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
        ValueAnalysis::getPositiveConstantIndex(plan.shape.steps[slot]);
    if (!step || *step <= 0)
      return std::nullopt;
    if (ownerDim < 0 ||
        static_cast<size_t>(ownerDim) >= storage.physicalBlockShape.size() ||
        *step > storage.physicalBlockShape[ownerDim])
      return std::nullopt;
    storage.physicalBlockShape[ownerDim] = *step;
  }

  storage.haloShape.reserve(rank);
  for (unsigned dim = 0; dim < rank; ++dim) {
    int64_t halo =
        std::max<int64_t>(0, std::max(-plan.neighborhood.minOffsets[dim],
                                      plan.neighborhood.maxOffsets[dim]));
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
  if (!op || !sde::queryInPlaceSharedState(op) || !isInPlaceSelfReadStencil(op))
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
  sde::SuIterateAttrs suAttrs = sde::SuIterateAttrs::fromOp(op);
  suAttrs.pattern =
      sde::SdePatternAttr::get(ctx, sde::SdePattern::stencil_tiling_nd);
  suAttrs.accessMinOffsets =
      buildI64ArrayAttr(ctx, plan.neighborhood.minOffsets);
  suAttrs.accessMaxOffsets =
      buildI64ArrayAttr(ctx, plan.neighborhood.maxOffsets);
  suAttrs.ownerDims = ownerDimsAttr;
  suAttrs.spatialDims = spatialDimsAttr;
  suAttrs.writeFootprint = writeFootprintAttr;
  // Distribution/in-place predicates are intentionally re-derived downstream.
  suAttrs.inPlaceSafe = nullptr;
  suAttrs.inPlaceSharedState = nullptr;
  auto newOp = sde::buildSuIterate(builder, loc, ValueRange(leadingLowerBounds),
                                   ValueRange(leadingUpperBounds),
                                   ValueRange(leadingSteps), suAttrs,
                                   op.getReductionAccumulators());

  Block &newBody = sde::ensureBlock(newOp.getBody());
  while (newBody.getNumArguments() < leadingLowerBounds.size())
    newBody.addArgument(builder.getIndexType(), loc);

  builder.setInsertionPointToStart(&newBody);
  IRMapping rootMapper;
  for (sde::SdeArrayLayoutRootOp root :
       op.getBody().front().getOps<sde::SdeArrayLayoutRootOp>()) {
    sde::SdeAccessModeAttr mode = root.getModeAttr();
    if (root.getMode() == sde::SdeAccessMode::write)
      mode = sde::SdeAccessModeAttr::get(ctx, sde::SdeAccessMode::read);
    sde::SdeArrayLayoutRootOp::create(
        builder, root.getLoc(), rootMapper.lookupOrDefault(root.getRoot()),
        mode, root.getArrayIdAttr());
  }

  auto newCuRegion = sde::buildCuRegion(
      builder, loc, sde::SdeCuKindAttr::get(ctx, sde::SdeCuKind::parallel));
  if (storagePlan)
    (void)sde::commitWriterPhysicalLayoutFacts(
        newOp, storagePlan->ownerPhysicalDims, storagePlan->physicalBlockShape,
        storagePlan->logicalWorkerSlice);
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

static std::optional<sde::LayoutGraphFact>
layoutFactFromCommittedPhysicalLayout(sde::SdeSuIterateOp op) {
  std::optional<sde::CommittedSuPhysicalLayout> layout =
      sde::recoverCommittedPhysicalLayout(op);
  if (!layout || layout->ownerDims.empty() || layout->blockShape.empty())
    return std::nullopt;
  sde::LayoutGraphFact fact;
  fact.role = sde::LayoutGraphRole::write;
  fact.layoutKind = sde::ArrayLayoutKind::blockParallel;
  fact.ownerDims = layout->ownerDims;
  fact.blockShape = layout->blockShape;
  return fact;
}

static std::optional<sde::LayoutGraphFact>
selectSingleWriteLayoutFact(sde::SdeSuIterateOp op) {
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return layoutFactFromCommittedPhysicalLayout(op);

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
  if (!selected)
    return layoutFactFromCommittedPhysicalLayout(op);
  return selected;
}

static ArrayRef<int64_t>
assignedPhysicalBlockShape(const sde::LayoutGraphFact &fact) {
  return fact.budgetBlockShape.empty()
             ? ArrayRef<int64_t>(fact.blockShape)
             : ArrayRef<int64_t>(fact.budgetBlockShape);
}

static std::optional<sde::LayoutGraphFact>
selectSingleAssignedWriteLayoutFact(sde::SdeSuIterateOp op) {
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return std::nullopt;

  std::optional<sde::LayoutGraphFact> selected;
  llvm::SmallDenseSet<int64_t, 4> writtenIds;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    if (fact.role != sde::LayoutGraphRole::write || fact.blockShape.empty())
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

static std::optional<sde::LayoutGraphFact> selectCompatibleCoiteratedReadLayout(
    sde::SdeSuIterateOp op, const sde::LayoutGraphFact &writeLayout,
    const sde::SuOutputLayoutFacts &outputPlan) {
  ArrayRef<int64_t> writeBlock = assignedPhysicalBlockShape(writeLayout);
  if (writeBlock.size() != outputPlan.shape.size())
    return std::nullopt;

  std::optional<sde::LayoutGraphFact> selected;
  for (const sde::LayoutGraphFact &fact :
       sde::parseArrayLayoutFacts(op.getArrayLayoutAttr())) {
    if (fact.role != sde::LayoutGraphRole::read || fact.ownerDims.empty())
      continue;
    if (fact.layoutKind != sde::ArrayLayoutKind::blockParallel)
      continue;
    if (fact.ownerDims.size() != op.getLowerBounds().size())
      continue;
    if (assignedPhysicalBlockShape(fact) != writeBlock)
      continue;
    std::optional<SmallVector<int64_t, 4>> orderedOwnerDims =
        orderPhysicalOwnerDimsByLoop(outputPlan, fact.ownerDims,
                                     op.getLowerBounds().size());
    if (!orderedOwnerDims)
      continue;
    if (!allExternalStoresCoverOwnerDims(op, *orderedOwnerDims,
                                         outputPlan.physicalDimToLoopDim))
      continue;
    if (!selected) {
      selected = fact;
      continue;
    }
    if (selected->ownerDims != fact.ownerDims ||
        assignedPhysicalBlockShape(*selected) !=
            assignedPhysicalBlockShape(fact))
      return std::nullopt;
  }
  return selected;
}

static bool commitWriterLayoutFromCoiteratedRead(sde::SdeSuIterateOp op) {
  if (!op || hasCommittedPhysicalLayout(op))
    return false;

  auto classification = sde::queryStructuredClassification(op);
  if (!classification ||
      (*classification != sde::SdeStructuredClassification::stencil &&
       *classification != sde::SdeStructuredClassification::elementwise &&
       *classification !=
           sde::SdeStructuredClassification::elementwise_pipeline))
    return false;

  std::optional<sde::LayoutGraphFact> writeLayout =
      selectSingleAssignedWriteLayoutFact(op);
  if (!writeLayout || !writeLayout->ownerDims.empty())
    return false;

  std::optional<sde::SuOutputLayoutFacts> outputPlan =
      sde::findCompatibleSuOutputLayoutFacts(op);
  if (!outputPlan || outputPlan->shape.empty() ||
      outputPlan->loopDimToPhysicalDim.size() < op.getLowerBounds().size())
    return false;

  std::optional<sde::LayoutGraphFact> readLayout =
      selectCompatibleCoiteratedReadLayout(op, *writeLayout, *outputPlan);
  if (!readLayout)
    return false;
  std::optional<SmallVector<int64_t, 4>> orderedOwnerDims =
      orderPhysicalOwnerDimsByLoop(*outputPlan, readLayout->ownerDims,
                                   op.getLowerBounds().size());
  if (!orderedOwnerDims)
    return false;

  ArrayRef<int64_t> assignedBlock = assignedPhysicalBlockShape(*writeLayout);
  SmallVector<int64_t, 4> physicalBlockShape(assignedBlock.begin(),
                                             assignedBlock.end());
  return applyPhysicalLayoutIfRealized(op, *orderedOwnerDims,
                                       physicalBlockShape);
}

static std::optional<sde::LayoutGraphFact>
findSingleLoopStepWriteLayoutFact(sde::SdeSuIterateOp op) {
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout || op.getSteps().empty())
    return std::nullopt;

  std::optional<sde::LayoutGraphFact> selected;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    if (fact.role != sde::LayoutGraphRole::write || fact.blockShape.empty())
      continue;
    if (fact.layoutKind != sde::ArrayLayoutKind::replicated &&
        fact.layoutKind != sde::ArrayLayoutKind::blockContraction)
      continue;
    if (fact.id < 0)
      return std::nullopt;
    if (!selected) {
      selected = fact;
      continue;
    }
    if (selected->id != fact.id || selected->layoutKind != fact.layoutKind ||
        selected->blockShape != fact.blockShape)
      return std::nullopt;
  }
  return selected;
}

static void commitLoopStepRealizedReplicatedLayout(sde::SdeSuIterateOp op) {
  if (!op || op.getSteps().size() != 1 ||
      sde::hasCommittedWriterBlockLayout(op))
    return;
  std::optional<sde::LayoutGraphFact> writeLayout =
      findSingleLoopStepWriteLayoutFact(op);
  if (!writeLayout || !writeLayout->ownerDims.empty())
    return;
  Value root =
      findArrayLayoutRoot(op, writeLayout->id, sde::SdeAccessMode::write);
  auto muType = root ? dyn_cast<MemRefType>(root.getType()) : MemRefType();
  if (!muType || !muType.hasStaticShape() || muType.getShape().empty())
    return;
  std::optional<int64_t> step =
      ValueAnalysis::getPositiveConstantIndex(op.getSteps()[0]);
  if (!step || *step <= 1 || muType.getShape()[0] <= *step)
    return;

  SmallVector<int64_t, 4> physicalBlockShape(muType.getShape().begin(),
                                             muType.getShape().end());
  physicalBlockShape[0] = *step;
  SmallVector<int64_t, 4> ownerDims{0};
  (void)sde::commitWriterPhysicalLayoutFacts(op, ownerDims, physicalBlockShape,
                                             physicalBlockShape);
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

static bool
commitPhysicalLayoutFromAssignedLayout(sde::SdeSuIterateOp op,
                                       sde::SDECostModel &costModel) {
  if (!op || hasCommittedPhysicalLayout(op))
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
  SmallVector<int64_t, 4> physicalDimToLoopDim;
  if (std::optional<sde::SuOutputLayoutFacts> structuredPlan =
          sde::findCompatibleSuOutputLayoutFacts(op)) {
    std::optional<SmallVector<int64_t, 4>> orderedOwnerDims =
        orderPhysicalOwnerDimsByLoop(*structuredPlan, writeLayout->ownerDims,
                                     op.getLowerBounds().size());
    if (!orderedOwnerDims)
      return false;
    plan.ownerPhysicalDims.assign(orderedOwnerDims->begin(),
                                  orderedOwnerDims->end());
    physicalDimToLoopDim.assign(structuredPlan->physicalDimToLoopDim.begin(),
                                structuredPlan->physicalDimToLoopDim.end());
  } else {
    plan.ownerPhysicalDims.assign(writeLayout->ownerDims.begin(),
                                  writeLayout->ownerDims.end());
  }
  if (!allExternalStoresCoverOwnerDims(op, plan.ownerPhysicalDims,
                                       physicalDimToLoopDim))
    return false;

  SmallVector<int64_t, 4> physicalBlockShape;
  ArrayRef<int64_t> assignedBlock =
      writeLayout->budgetBlockShape.empty()
          ? ArrayRef<int64_t>(writeLayout->blockShape)
          : ArrayRef<int64_t>(writeLayout->budgetBlockShape);
  if (assignedBlock.size() != plan.shape.size())
    return false;
  physicalBlockShape.assign(assignedBlock.begin(), assignedBlock.end());
  for (int64_t ownerDim : plan.ownerPhysicalDims) {
    if (ownerDim < 0 || static_cast<size_t>(ownerDim) >= plan.shape.size())
      return false;
    if (physicalBlockShape[ownerDim] <= 0 ||
        physicalBlockShape[ownerDim] > plan.shape[ownerDim])
      return false;
  }

  SmallVector<int64_t, 4> ownerDims(plan.ownerPhysicalDims.begin(),
                                    plan.ownerPhysicalDims.end());
  int64_t logicalTarget =
      std::max<int64_t>(1, getInterLocalityTargetWorkers(costModel));
  SmallVector<int64_t, 4> logicalWorkerSlice =
      buildLogicalWorkerSliceOrPhysical(op, plan.shape, ownerDims,
                                        physicalBlockShape, logicalTarget);
  return applyPhysicalLayoutIfRealized(op, ownerDims, physicalBlockShape,
                                       /*haloShape=*/{}, logicalWorkerSlice);
}

struct BlockGrainPlan {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> physicalBlockShape;
  SmallVector<int64_t, 4> logicalWorkerSlice;
  SmallVector<int64_t, 4> haloShape;
};

// Choose the one committed node-agnostic budget grain for every SU that writes
// a multi-owner-distributed data-parallel array. The caller only commits it
// when the current SU step already realizes the selected block grain.
static std::optional<BlockGrainPlan>
buildBudgetReconciledBlockGrainPlan(sde::SdeSuIterateOp op,
                                    sde::SDECostModel &costModel) {
  if (!op || hasCommittedPhysicalLayout(op))
    return std::nullopt;
  // Matmul/contraction keeps its dedicated contraction-tiling plan: its CU-task
  // grain is the reduction-aware worker grain, not the data-parallel block
  // grain reconciled here. This is the one genuinely layout-irreducible family.
  if (auto cls = sde::queryStructuredClassification(op);
      cls && *cls == sde::SdeStructuredClassification::matmul)
    return std::nullopt;
  if (auto cls = sde::queryStructuredClassification(op);
      cls && *cls == sde::SdeStructuredClassification::stencil)
    return std::nullopt;
  if (auto cls = sde::queryStructuredClassification(op);
      cls &&
      (*cls == sde::SdeStructuredClassification::elementwise ||
       *cls == sde::SdeStructuredClassification::elementwise_pipeline) &&
      sde::queryInPlaceSafe(op))
    return std::nullopt;
  if (auto pat = sde::querySuPattern(op);
      pat && *pat == sde::SdePattern::matmul)
    return std::nullopt;
  std::optional<sde::LayoutGraphFact> writeLayout =
      selectSingleWriteLayoutFact(op);
  if (!writeLayout || writeLayout->ownerDims.size() < 2 ||
      writeLayout->budgetBlockShape.empty())
    return std::nullopt;
  // owner_tile needs one realized SDE loop dimension per owner dim. A 1-D loop
  // (e.g. a residual/reduction loop over the same array) that did not get
  // promoted cannot carry a multi-owner tile; leave it to the pattern
  // committers rather than committing unverifiable owner_tile facts.
  if (op.getLowerBounds().size() < writeLayout->ownerDims.size())
    return std::nullopt;
  std::optional<sde::SuOutputLayoutFacts> outputPlan =
      sde::findCompatibleSuOutputLayoutFacts(op);
  if (!outputPlan)
    return std::nullopt;
  std::optional<SmallVector<int64_t, 4>> orderedOwnerDims =
      orderPhysicalOwnerDimsByLoop(*outputPlan, writeLayout->ownerDims,
                                   op.getLowerBounds().size());
  if (!orderedOwnerDims)
    return std::nullopt;
  BlockGrainPlan plan;
  plan.ownerDims.assign(orderedOwnerDims->begin(), orderedOwnerDims->end());
  if (!allExternalStoresCoverOwnerDims(op, plan.ownerDims,
                                       outputPlan->physicalDimToLoopDim))
    return std::nullopt;
  plan.physicalBlockShape.assign(writeLayout->budgetBlockShape.begin(),
                                 writeLayout->budgetBlockShape.end());
  if (!sde::enforceOwnerBlockConcurrencyFloor(
          outputPlan->shape, plan.ownerDims,
          getInterLocalityTargetWorkers(costModel), plan.physicalBlockShape))
    return std::nullopt;
  // Per-owner-dim halo from the op's stencil access offsets (0 for
  // non-stencils). Not compared by hasSameHostBridgePlan, but needed for
  // correct halo exchange.
  bool anyHalo = false;
  for (int64_t od : plan.ownerDims) {
    int64_t h =
        od >= 0 ? readStencilHaloForOwnerDim(op, static_cast<unsigned>(od)) : 0;
    plan.haloShape.push_back(std::max<int64_t>(0, h));
    anyHalo |= h > 0;
  }
  plan.logicalWorkerSlice = buildLogicalWorkerSliceOrPhysical(
      op, outputPlan->shape, plan.ownerDims, plan.physicalBlockShape,
      costModel.getLogicalWorkerCapacity(),
      anyHalo ? ArrayRef<int64_t>(plan.haloShape) : ArrayRef<int64_t>{});
  if (!anyHalo)
    plan.haloShape.clear();
  return plan;
}

static bool commitBudgetReconciledLayout(sde::SdeSuIterateOp op,
                                         sde::SDECostModel &costModel) {
  std::optional<BlockGrainPlan> plan =
      buildBudgetReconciledBlockGrainPlan(op, costModel);
  if (!plan)
    return false;
  return applyPhysicalLayoutIfRealized(
      op, plan->ownerDims, plan->physicalBlockShape, plan->haloShape,
      plan->logicalWorkerSlice);
}

static void
alignLateOwnerShapeToExistingStep(sde::SdeSuIterateOp op,
                                  ArrayRef<int64_t> ownerDims,
                                  MutableArrayRef<int64_t> physicalBlockShape) {
  if (ownerDims.empty() || op.getSteps().empty() ||
      ownerDims.size() > op.getSteps().size())
    return;

  std::optional<SmallVector<int64_t, 4>> physicalDimToLoopDim =
      derivePhysicalDimToSuLoopDimFromExternalStores(op, ownerDims);
  if (!physicalDimToLoopDim)
    return;
  for (auto [ownerSlot, ownerPhysicalDim] : llvm::enumerate(ownerDims)) {
    if (ownerPhysicalDim < 0 ||
        static_cast<size_t>(ownerPhysicalDim) >= physicalBlockShape.size())
      return;

    (void)ownerSlot;
    if (static_cast<size_t>(ownerPhysicalDim) >= physicalDimToLoopDim->size())
      return;
    int64_t mapped = (*physicalDimToLoopDim)[ownerPhysicalDim];
    if (mapped < 0)
      return;
    unsigned stepSlot = static_cast<unsigned>(mapped);
    if (stepSlot >= op.getSteps().size())
      return;

    std::optional<int64_t> ownerStep =
        ValueAnalysis::getPositiveConstantIndex(op.getSteps()[stepSlot]);
    if (!ownerStep || *ownerStep <= 1)
      continue;

    physicalBlockShape[ownerPhysicalDim] = *ownerStep;
  }
}

static void commitStencilPhysicalLayout(sde::SdeSuIterateOp op,
                                        sde::SDECostModel &costModel) {
  if (hasCommittedPhysicalLayout(op) ||
      costModel.getLogicalWorkerCapacity() <= 1)
    return;
  auto classification = sde::queryStructuredClassification(op);
  bool isStencil = classification &&
                   *classification == sde::SdeStructuredClassification::stencil;
  if (sde::queryInPlaceSafe(op) && !isStencil)
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

      if (isInPlaceSelfReadStencil(op) && !sde::queryInPlaceSafe(op)) {
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
  if (hasCommittedPhysicalLayout(op) ||
      costModel.getLogicalWorkerCapacity() <= 1)
    return;
  auto classification = sde::queryStructuredClassification(op);
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
            int64_t logicalTarget = workers;
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
  alignLateOwnerShapeToExistingStep(op, outputPlan->ownerPhysicalDims,
                                    physicalBlockShape);
  SmallVector<int64_t, 4> logicalWorkerSlice =
      buildLogicalWorkerSliceOrPhysical(op, outputPlan->shape,
                                        outputPlan->ownerPhysicalDims,
                                        physicalBlockShape, workers);

  if (!classification)
    return;
  (void)applyPhysicalLayoutIfRealized(op, outputPlan->ownerPhysicalDims,
                                      physicalBlockShape,
                                      /*haloShape=*/{}, logicalWorkerSlice);
}

static void commitMatmulPhysicalLayout(sde::SdeSuIterateOp op,
                                       sde::SDECostModel &costModel) {
  if (hasCommittedPhysicalLayout(op) ||
      costModel.getLogicalWorkerCapacity() <= 1)
    return;
  auto classification = sde::queryStructuredClassification(op);
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

  SmallVector<int64_t, 2> ownerDims{0, 1};
  SmallVector<int64_t, 4> logicalWorkerSlice =
      buildLogicalWorkerSliceOrPhysical(op, outputPlan->shape, ownerDims,
                                        physicalBlockShape, workers);

  sde::commitWriterPhysicalLayoutFacts(op, ownerDims, physicalBlockShape,
                                       logicalWorkerSlice);
}

static void commitDirectRowMatmulPhysicalLayout(sde::SdeSuIterateOp op,
                                                sde::SDECostModel &costModel) {
  if (hasCommittedPhysicalLayout(op) ||
      costModel.getLogicalWorkerCapacity() <= 1)
    return;
  if (op.getLowerBounds().size() != 1 || op.getSteps().size() != 1)
    return;
  auto classification = sde::queryStructuredClassification(op);
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
  int64_t logicalTarget = workers;
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
  if (sde::SdeCuRegionOp cu = sde::findSuComputeCuRegion(op);
      cu && cu.getGroupBlockCountAttr() && hasCommittedPhysicalLayout(op))
    return;
  auto classification = sde::queryStructuredClassification(op);
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
      for (int64_t rawDim : outputPlan->ownerPhysicalDims) {
        if (rawDim < 0 ||
            static_cast<size_t>(rawDim) >= physicalBlockShape.size())
          return;
        physicalBlockShape[rawDim] = slice;
      }
      for (int64_t rawDim : outputPlan->ownerPhysicalDims)
        if (rawDim < 0 ||
            static_cast<size_t>(rawDim) >= physicalBlockShape.size())
          return;
      SmallVector<int64_t, 4> ownerDims(outputPlan->ownerPhysicalDims.begin(),
                                        outputPlan->ownerPhysicalDims.end());
      int64_t logicalTarget = targetTasks;
      SmallVector<int64_t, 4> logicalWorkerSlice =
          buildLogicalWorkerSliceOrPhysical(op, outputPlan->shape, ownerDims,
                                            physicalBlockShape, logicalTarget);
      sde::commitWriterPhysicalLayoutFacts(op, ownerDims, physicalBlockShape,
                                           logicalWorkerSlice);
      return;
    }
  }
}

static void
commitInPlaceSharedStencilSerialSlice(sde::SdeSuIterateOp op,
                                      sde::SDECostModel &costModel) {
  if (sde::SdeCuRegionOp cu = sde::findSuComputeCuRegion(op);
      cu && cu.getGroupBlockCountAttr())
    return;
  if (costModel.getLogicalWorkerCapacity() <= 1)
    return;
  auto classification = sde::queryStructuredClassification(op);
  if (!classification ||
      *classification != sde::SdeStructuredClassification::stencil)
    return;
  if (!sde::queryInPlaceSharedState(op))
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

  sde::commitWriterPhysicalLayoutFacts(op, SmallVector<int64_t, 1>{0},
                                       SmallVector<int64_t, 1>{slice},
                                       SmallVector<int64_t, 1>{slice});
}

static bool hasLoopCarriedNeighborOffsets(sde::SdeSuIterateOp op) {
  std::optional<sde::SuNeighborhoodAccessInfo> neighborhood =
      sde::queryNeighborhoodAccessInfo(op);
  if (neighborhood) {
    for (auto [minOffset, maxOffset] :
         llvm::zip_equal(neighborhood->minOffsets, neighborhood->maxOffsets))
      if (minOffset < 0 || maxOffset > 0)
        return true;
    return false;
  }

  std::optional<SmallVector<int64_t, 4>> mins =
      readI64ArrayAttr(op.getAccessMinOffsetsAttr());
  std::optional<SmallVector<int64_t, 4>> maxs =
      readI64ArrayAttr(op.getAccessMaxOffsetsAttr());
  if (!mins || !maxs || mins->size() != maxs->size())
    return false;
  for (auto [minOffset, maxOffset] : llvm::zip_equal(*mins, *maxs))
    if (minOffset < 0 || maxOffset > 0)
      return true;
  return false;
}

static bool
requiresInPlaceSelfRawWavefrontFailClosed(sde::SdeSuIterateOp op,
                                          sde::SDECostModel &costModel) {
  if (costModel.getLogicalWorkerCapacity() <= 1)
    return false;
  if (sde::queryInPlaceSafe(op))
    return false;
  auto classification = sde::queryStructuredClassification(op);
  if (!classification ||
      *classification != sde::SdeStructuredClassification::stencil)
    return false;
  if (!sde::queryInPlaceSharedState(op))
    return false;
  if (!hasLoopCarriedNeighborOffsets(op))
    return false;
  return true;
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

  auto classificationAttr = sde::queryStructuredClassification(op);
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

  if (op.getNumResults() > 0 &&
      *classificationAttr != sde::SdeStructuredClassification::reduction)
    return std::nullopt;

  switch (*classificationAttr) {
  case sde::SdeStructuredClassification::elementwise:
  case sde::SdeStructuredClassification::elementwise_pipeline:
    return sde::SdeDistributionKind::blocked;
  case sde::SdeStructuredClassification::stencil:
    if (sde::requiresNestedStencilOwnerPromotion(op) &&
        !sde::hasRealizableOwnerStrip(op))
      return std::nullopt;
    if (isInPlaceSelfReadStencil(op) && !sde::queryInPlaceSafe(op))
      return std::nullopt;
    if (hasEnoughWorkForDistribution(op, costModel))
      return sde::SdeDistributionKind::owner_compute;
    return std::nullopt;
  case sde::SdeStructuredClassification::matmul:
    return sde::SdeDistributionKind::blocked;
  case sde::SdeStructuredClassification::reduction:
    if (!op.getReductionAccumulators().empty())
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
  return requiresInPlaceSelfRawWavefrontFailClosed(op, costModel);
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

// A1 grain unification. Producer and consumer SUs of the same distributed array
// can commit different block grains for purely scheduling reasons (e.g. a
// matmul writer at [512,*] and its consumer at budget [256,*]).
// RedistributionEdges then reads a same-owner re-tile and fails closed
// ("refusing to emit degenerate all_to_all") because no movement op realizes a
// within-owner re-block. The grain is a planning artifact, not a data-location
// difference: commit ONE grain (the per-dim GCD, finest both already realize)
// on every block_parallel fact of the array so home==reader and no
// redistribution edge exists. Node-agnostic; runs after per-SU commit so
// RankExpandMu/RedistributionEdges consume unified facts.
static void reconcileSameOwnerArrayGrain(Operation *moduleOp) {
  auto committedGrain = [](const sde::LayoutGraphFact &f) -> ArrayRef<int64_t> {
    return f.budgetBlockShape.empty() ? ArrayRef<int64_t>(f.blockShape)
                                      : ArrayRef<int64_t>(f.budgetBlockShape);
  };
  struct GrainInfo {
    SmallVector<int64_t, 4> ownerDims;
    SmallVector<int64_t, 4> unified;
    SmallVector<int64_t, 4> rootShape;
    SmallVector<sde::SdeSuIterateOp, 2> writerOps;
    bool eligible = true;
    bool seen = false;
    bool differs = false;
    // Per-dim GCD of the genuine node-agnostic *budget* grains. A writer that
    // committed only an abstract coarse blockShape (no budgetBlockShape) must
    // be reconciled DOWN to this budget rather than dragging the unified grain
    // to a coprime GCD with it: gcd(abstractBlock, budget) can be 1, which is a
    // grain NEITHER side realizes (one DB block per element -> millions of
    // EDTs). The budget is the one grain both sides can realize.
    SmallVector<int64_t, 4> budgetUnified;
    bool budgetSeen = false;
    bool budgetSizeMismatch = false;
    // Set when some contributing grain is *incompatible* with the budget grain
    // on a dim (neither divides the other), so the cross-fact GCD collapses to
    // a grain the incompatible side cannot realize. A grain that merely divides
    // the budget (a legitimate finer matmul/elementwise tile) is compatible and
    // is preserved.
    bool incompatibleWithBudget = false;
    // Every per-dim grain that contributed to the cross-fact GCD, for the
    // post-walk budget-compatibility check.
    SmallVector<SmallVector<int64_t, 4>, 4> contributingGrains;
  };
  llvm::DenseMap<int64_t, GrainInfo> byId;

  moduleOp->walk([&](sde::SdeSuIterateOp op) {
    ArrayAttr layout = op.getArrayLayoutAttr();
    if (!layout)
      return;
    for (const sde::LayoutGraphFact &f : sde::parseArrayLayoutFacts(layout)) {
      if (f.id < 0)
        continue;
      GrainInfo &info = byId[f.id];
      if (f.layoutKind != sde::ArrayLayoutKind::blockParallel ||
          f.ownerDims.empty()) {
        info.eligible = false;
        continue;
      }
      ArrayRef<int64_t> grain = committedGrain(f);
      if (grain.empty()) {
        info.eligible = false;
        continue;
      }
      info.contributingGrains.push_back(
          SmallVector<int64_t, 4>(grain.begin(), grain.end()));
      if (!info.seen) {
        info.seen = true;
        info.ownerDims.assign(f.ownerDims.begin(), f.ownerDims.end());
        info.unified.assign(grain.begin(), grain.end());
      } else if (ArrayRef<int64_t>(info.ownerDims) !=
                     ArrayRef<int64_t>(f.ownerDims) ||
                 info.unified.size() != grain.size()) {
        info.eligible = false;
      } else {
        for (size_t i = 0; i < grain.size(); ++i) {
          if (grain[i] != info.unified[i])
            info.differs = true;
          int64_t g = std::gcd(info.unified[i], grain[i]);
          info.unified[i] = g > 0 ? g : info.unified[i];
        }
      }
      // Track the GCD of declared budget grains separately. Only facts that
      // actually carry a budgetBlockShape contribute, so a coarse abstract
      // writer blockShape never collapses this.
      if (!f.budgetBlockShape.empty()) {
        ArrayRef<int64_t> budget(f.budgetBlockShape);
        if (!info.budgetSeen) {
          info.budgetSeen = true;
          info.budgetUnified.assign(budget.begin(), budget.end());
        } else if (info.budgetUnified.size() != budget.size()) {
          info.budgetSizeMismatch = true;
        } else {
          for (size_t i = 0; i < budget.size(); ++i) {
            int64_t g = std::gcd(info.budgetUnified[i], budget[i]);
            info.budgetUnified[i] = g > 0 ? g : info.budgetUnified[i];
          }
        }
      }
      if (f.role == sde::LayoutGraphRole::write) {
        info.writerOps.push_back(op);
        if (info.rootShape.empty())
          if (auto rs = sde::findWriteArrayRootShape(op, f.id))
            info.rootShape.assign(rs->begin(), rs->end());
      }
    }
  });

  // When a declared budget grain exists, it is the authoritative node-agnostic
  // grain both producers and consumers can realize. The raw cross-fact GCD is
  // only safe when every contributing grain is *compatible* with the budget
  // (one divides the other on every dim). If some contributing grain is
  // incompatible with the budget (neither divides the other, e.g. a writer's
  // coarse abstract blockShape that was never physically committed at budget),
  // the GCD collapses to a grain that incompatible side cannot realize — for a
  // 1-D elementwise array this is one DB block per element (millions of EDTs).
  // In that case adopt the budget grain and re-commit the writers at it.
  for (auto &kv : byId) {
    GrainInfo &info = kv.second;
    if (!info.eligible || !info.seen || !info.budgetSeen ||
        info.budgetSizeMismatch)
      continue;
    if (info.unified.size() != info.budgetUnified.size())
      continue;
    for (const SmallVector<int64_t, 4> &grain : info.contributingGrains) {
      if (grain.size() != info.budgetUnified.size())
        continue;
      for (size_t i = 0; i < grain.size(); ++i) {
        int64_t g = grain[i], b = info.budgetUnified[i];
        if (g <= 0 || b <= 0)
          continue;
        int64_t common = std::gcd(g, b);
        if (common != std::min(g, b))
          info.incompatibleWithBudget = true;
      }
    }
    if (!info.incompatibleWithBudget)
      continue;
    // Adopt the budget grain as the unified target and force the differing
    // writers to be re-committed at it.
    info.unified.assign(info.budgetUnified.begin(), info.budgetUnified.end());
    info.differs = true;
  }

  llvm::DenseMap<int64_t, SmallVector<int64_t, 4>> targetById;
  for (auto &kv : byId) {
    GrainInfo &info = kv.second;
    if (info.eligible && info.seen && info.differs && !info.ownerDims.empty())
      targetById[kv.first] = info.unified;
  }
  if (targetById.empty())
    return;

  // Re-commit each writer's physical layout at the unified grain. The physical
  // CU-group block counts (not the arrayLayout fact) are what RankExpandMu
  // reads to shape the MU and re-author the write fact, so unifying only the
  // fact is not enough — the producer must realize the finer grain.
  for (auto &kv : targetById) {
    GrainInfo &info = byId[kv.first];
    for (sde::SdeSuIterateOp writer : info.writerOps)
      if (writer)
        (void)sde::commitWriterPhysicalLayoutFacts(writer, info.ownerDims,
                                                   kv.second);
  }

  MLIRContext *ctx = moduleOp->getContext();
  Builder builder(ctx);
  StringAttr blockShapeName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::BlockShape);
  StringAttr budgetName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::BudgetBlockShape);
  StringAttr muCountName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::MuBlockCount);

  moduleOp->walk([&](sde::SdeSuIterateOp op) {
    ArrayAttr layout = op.getArrayLayoutAttr();
    if (!layout)
      return;
    bool changed = false;
    SmallVector<Attribute, 4> rewritten;
    rewritten.reserve(layout.size());
    for (Attribute attr : layout) {
      auto dict = dyn_cast<DictionaryAttr>(attr);
      std::optional<sde::LayoutGraphFact> f =
          dict ? sde::parseArrayLayoutFact(dict) : std::nullopt;
      if (!dict || !f) {
        rewritten.push_back(attr);
        continue;
      }
      auto it = targetById.find(f->id);
      const GrainInfo &info = byId[f->id];
      if (it == targetById.end() || f->role != sde::LayoutGraphRole::read ||
          f->layoutKind != sde::ArrayLayoutKind::blockParallel ||
          ArrayRef<int64_t>(f->ownerDims) !=
              ArrayRef<int64_t>(info.ownerDims) ||
          it->second.size() != committedGrain(*f).size() ||
          committedGrain(*f) == ArrayRef<int64_t>(it->second)) {
        rewritten.push_back(attr);
        continue;
      }
      ArrayRef<int64_t> target = it->second;
      SmallVector<NamedAttribute, 8> fields;
      for (NamedAttribute named : dict)
        if (named.getName() != blockShapeName &&
            named.getName() != budgetName && named.getName() != muCountName)
          fields.push_back(named);
      fields.push_back(
          builder.getNamedAttr(blockShapeName, buildI64ArrayAttr(ctx, target)));
      fields.push_back(
          builder.getNamedAttr(budgetName, buildI64ArrayAttr(ctx, target)));
      if (!info.rootShape.empty() && info.rootShape.size() == target.size()) {
        int64_t count = sde::inferCuCountFromMuPartition(
            info.rootShape, info.ownerDims, target);
        if (count > 0)
          fields.push_back(builder.getNamedAttr(
              muCountName, builder.getI64IntegerAttr(count)));
      }
      rewritten.push_back(builder.getDictionaryAttr(fields));
      changed = true;
    }
    if (changed)
      op.setArrayLayoutAttr(ArrayAttr::get(ctx, rewritten));
  });
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
      if (requiresInPlaceSelfRawWavefrontFailClosed(op, *costModel)) {
        emitStencilWavefrontFailClosed(op);
        failed = true;
        continue;
      }
      if (std::optional<sde::SdeSuIterateOp> wavefront =
              tryRealizeWavefrontSkew(op, *costModel)) {
        (void)*wavefront;
        continue;
      }

      commitLoopStepRealizedReplicatedLayout(op);

      if (sde::hasCommittedCuMuPartitionFacts(op)) {
        chooseDistributionOrFailClosed(op);
        continue;
      }

      // Budget-reconciled layout is authored first so the per-pattern
      // committers below see an already-committed multi-owner data-parallel SU
      // and skip it; they still run for the families budget declines
      // (single-owner, matmul, reduction, in-place). The committed budget
      // layout is the authority.
      commitWriterLayoutFromCoiteratedRead(op);
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

    // Unify producer/consumer block grain per array so a same-owner re-tile is
    // never left for RedistributionEdges to reject (A1). Runs after all per-SU
    // facts are committed; downstream passes consume the unified facts.
    reconcileSameOwnerArrayGrain(getOperation());

    for (DistributionRewrite rewrite : rewrites) {
      if (rewrite.op.getNumResults() > 0)
        continue;

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
