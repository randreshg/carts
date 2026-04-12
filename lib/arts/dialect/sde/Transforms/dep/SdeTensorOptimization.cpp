///==========================================================================///
/// File: SdeTensorOptimization.cpp
///
/// Cost-model-driven tensor optimization in SDE. The current implementation
/// focuses on executable loop nests whose tensor-backed linalg carrier proves
/// that strip-mining the outer SDE loop preserves a disjoint write set. The
/// pass rewrites the surrounding sde.su_iterate so the tiled loop nest
/// survives SDE->ARTS lowering while tensor carriers remain an SDE-only
/// concern.
///==========================================================================///

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_SDETENSOROPTIMIZATION
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

#include "arts/utils/LoopUtils.h"
#include "arts/utils/ValueAnalysis.h"
#include "arts/utils/costs/SDECostModel.h"

#include "mlir/Dialect/Linalg/Transforms/Transforms.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

#include "llvm/Support/MathExtras.h"

#include <algorithm>
#include <cmath>

using namespace mlir;
using namespace mlir::arts;

namespace {

static Value stripSimpleMemrefAlias(Value value) {
  while (auto castOp = value.getDefiningOp<memref::CastOp>())
    value = castOp.getSource();
  return value;
}

static bool isTensorOptimizationSchedule(sde::SdeScheduleKindAttr schedule) {
  if (!schedule)
    return true;
  return schedule.getValue() == sde::SdeScheduleKind::static_;
}

static Value getConstantIndex(OpBuilder &builder, Location loc, int64_t value) {
  return arith::ConstantIndexOp::create(builder, loc, value);
}

static Value buildTripCountValue(OpBuilder &builder, Location loc,
                                 sde::SdeSuIterateOp op) {
  if (std::optional<int64_t> tripCount = getStaticTripCount(op.getOperation()))
    return getConstantIndex(builder, loc, *tripCount);

  if (op.getLowerBounds().size() != 1 || op.getUpperBounds().size() != 1 ||
      op.getSteps().size() != 1)
    return Value();

  Value lowerBound = op.getLowerBounds().front();
  Value upperBound = op.getUpperBounds().front();
  Value step = op.getSteps().front();
  Value zero = getConstantIndex(builder, loc, 0);
  Value one = getConstantIndex(builder, loc, 1);

  Value span = arith::SubIOp::create(builder, loc, upperBound, lowerBound);

  int64_t constantStep = 0;
  Value safeStep = step;
  if (ValueAnalysis::getConstantIndex(step, constantStep)) {
    if (constantStep <= 0)
      return Value();
  } else {
    Value stepIsTooSmall = arith::CmpIOp::create(
        builder, loc, arith::CmpIPredicate::sle, step, zero);
    safeStep = arith::SelectOp::create(builder, loc, stepIsTooSmall, one, step);
  }

  Value spanIsNegative = arith::CmpIOp::create(
      builder, loc, arith::CmpIPredicate::slt, span, zero);
  Value nonNegativeSpan =
      arith::SelectOp::create(builder, loc, spanIsNegative, zero, span);
  return arith::CeilDivSIOp::create(builder, loc, nonNegativeSpan, safeStep);
}

static Value buildTileIterationValue(OpBuilder &builder, Location loc,
                                     sde::SdeSuIterateOp op,
                                     sde::SDECostModel &costModel) {
  Value tripCount = buildTripCountValue(builder, loc, op);
  if (!tripCount)
    return Value();

  Value one = getConstantIndex(builder, loc, 1);
  Value workerCountValue = getConstantIndex(
      builder, loc, std::max<int64_t>(1, costModel.getWorkerCount()));
  Value minIterationsValue = getConstantIndex(
      builder, loc,
      std::max<int64_t>(1, costModel.getMinIterationsPerWorker()));

  Value clampedTripCount = arith::MaxUIOp::create(builder, loc, tripCount, one);
  Value balancedTile = arith::CeilDivUIOp::create(
      builder, loc, clampedTripCount, workerCountValue);
  Value preferredTile =
      arith::MaxUIOp::create(builder, loc, balancedTile, minIterationsValue);
  return arith::MinUIOp::create(builder, loc, preferredTile, clampedTripCount);
}

static SmallVector<Value> buildPerDimTripCounts(OpBuilder &builder, Location loc,
                                                sde::SdeSuIterateOp op) {
  SmallVector<Value> tripCounts;
  unsigned numDims = op.getLowerBounds().size();

  for (unsigned d = 0; d < numDims; ++d) {
    Value lb = op.getLowerBounds()[d];
    Value ub = op.getUpperBounds()[d];
    Value step = op.getSteps()[d];
    Value zero = getConstantIndex(builder, loc, 0);
    Value one = getConstantIndex(builder, loc, 1);

    Value span = arith::SubIOp::create(builder, loc, ub, lb);

    int64_t constantStep = 0;
    Value safeStep = step;
    if (ValueAnalysis::getConstantIndex(step, constantStep)) {
      if (constantStep <= 0)
        return {};
    } else {
      Value stepIsTooSmall = arith::CmpIOp::create(
          builder, loc, arith::CmpIPredicate::sle, step, zero);
      safeStep =
          arith::SelectOp::create(builder, loc, stepIsTooSmall, one, step);
    }

    Value spanIsNegative = arith::CmpIOp::create(
        builder, loc, arith::CmpIPredicate::slt, span, zero);
    Value nonNegativeSpan =
        arith::SelectOp::create(builder, loc, spanIsNegative, zero, span);
    tripCounts.push_back(
        arith::CeilDivSIOp::create(builder, loc, nonNegativeSpan, safeStep));
  }
  return tripCounts;
}

static SmallVector<Value>
buildPerDimTileIterations(OpBuilder &builder, Location loc,
                          sde::SdeSuIterateOp op,
                          sde::SDECostModel &costModel) {
  SmallVector<Value> tripCounts = buildPerDimTripCounts(builder, loc, op);
  if (tripCounts.empty())
    return {};

  unsigned numDims = tripCounts.size();
  int workersPerDim = std::max(
      1, static_cast<int>(std::ceil(
             std::pow(costModel.getWorkerCount(), 1.0 / numDims))));
  int64_t minIter = costModel.getMinIterationsPerWorker();

  SmallVector<Value> tileIterations;
  for (unsigned d = 0; d < numDims; ++d) {
    Value one = getConstantIndex(builder, loc, 1);
    Value workersVal = getConstantIndex(builder, loc, workersPerDim);
    Value minIterVal =
        getConstantIndex(builder, loc, std::max<int64_t>(1, minIter));
    Value clampedTrip =
        arith::MaxUIOp::create(builder, loc, tripCounts[d], one);
    Value balanced =
        arith::CeilDivUIOp::create(builder, loc, clampedTrip, workersVal);
    Value preferred =
        arith::MaxUIOp::create(builder, loc, balanced, minIterVal);
    Value tile =
        arith::MinUIOp::create(builder, loc, preferred, clampedTrip);
    tileIterations.push_back(tile);
  }
  return tileIterations;
}

static bool isCarrierOp(Operation &op) {
  return isa<bufferization::ToTensorOp, tensor::EmptyOp, linalg::GenericOp>(op);
}

static bool isScalarExecutableOp(Operation &op) {
  if (isa<memref::LoadOp, memref::StoreOp>(op))
    return true;
  return op.getNumRegions() == 0 && isMemoryEffectFree(&op);
}

static bool isExecutableInnermostBody(Block &body) {
  for (Operation &op : body.without_terminator()) {
    if (isCarrierOp(op))
      continue;
    if (!isScalarExecutableOp(op))
      return false;
  }
  return true;
}

static bool hasPerfectNestedScalarLoopNest(Block &body, unsigned numLoops) {
  if (numLoops == 0)
    return false;
  if (numLoops == 1)
    return isExecutableInnermostBody(body);

  Block *current = &body;
  for (unsigned depth = 1; depth < numLoops; ++depth) {
    scf::ForOp nestedLoop;
    for (Operation &op : current->without_terminator()) {
      if (isCarrierOp(op))
        continue;
      if (!isa<scf::ForOp>(op) || nestedLoop)
        return false;
      nestedLoop = cast<scf::ForOp>(op);
    }
    if (!nestedLoop || !nestedLoop.getInitArgs().empty())
      return false;
    current = nestedLoop.getBody();
  }

  return isExecutableInnermostBody(*current);
}

static linalg::GenericOp findTensorCarrier(Block &body) {
  linalg::GenericOp tensorGeneric;
  for (Operation &op : body) {
    auto generic = dyn_cast<linalg::GenericOp>(op);
    if (!generic)
      continue;
    if (!llvm::all_of(generic.getDpsInputs(), [](Value operand) {
          return isa<TensorType>(operand.getType());
        }))
      continue;
    if (!llvm::all_of(generic.getDpsInits(), [](Value operand) {
          return isa<TensorType>(operand.getType());
        }))
      continue;
    if (tensorGeneric)
      return nullptr;
    tensorGeneric = generic;
  }
  return tensorGeneric;
}

static bool hasDisjointWriteSet(Block &body, linalg::GenericOp tensorGeneric) {
  unsigned loadCount = 0;
  unsigned storeCount = 0;
  SmallVector<Value> writtenMemrefs;

  body.walk([&](memref::LoadOp) { ++loadCount; });
  body.walk([&](memref::StoreOp storeOp) {
    ++storeCount;
    Value base = stripSimpleMemrefAlias(storeOp.getMemref());
    if (!llvm::is_contained(writtenMemrefs, base))
      writtenMemrefs.push_back(base);
  });

  if (loadCount != tensorGeneric.getNumDpsInputs() ||
      storeCount != tensorGeneric.getNumDpsInits() ||
      writtenMemrefs.size() != static_cast<size_t>(storeCount))
    return false;

  for (auto [outputIndex, output] :
       llvm::enumerate(tensorGeneric.getDpsInits())) {
    AffineMap indexingMap = tensorGeneric.getMatchingIndexingMap(
        tensorGeneric.getDpsInitOperand(outputIndex));
    if (!indexingMap.isProjectedPermutation())
      return false;
  }

  return true;
}

static bool isElementwiseTensorCandidate(Block &body,
                                         linalg::GenericOp tensorGeneric,
                                         unsigned numSuDims) {
  if (tensorGeneric.getNumLoops() == 0)
    return false;
  if (!llvm::all_of(tensorGeneric.getIteratorTypesArray(),
                    [](utils::IteratorType type) {
                      return type == utils::IteratorType::parallel;
                    }))
    return false;
  if (tensorGeneric.getNumDpsInits() == 0)
    return false;
  if (!hasDisjointWriteSet(body, tensorGeneric))
    return false;
  // The su_iterate provides numSuDims induction variables directly as block
  // args. The remaining dimensions must appear as nested scf.for loops.
  unsigned nestedLoops = tensorGeneric.getNumLoops() >= numSuDims
                             ? tensorGeneric.getNumLoops() - numSuDims + 1
                             : 1;
  return hasPerfectNestedScalarLoopNest(body, nestedLoops);
}

static bool isMatmulTensorCandidate(Block &body,
                                    linalg::GenericOp tensorGeneric) {
  if (tensorGeneric.getNumLoops() != 3 || tensorGeneric.getNumDpsInits() != 1)
    return false;

  unsigned numParallel = 0;
  unsigned numReduction = 0;
  for (utils::IteratorType type : tensorGeneric.getIteratorTypesArray()) {
    if (type == utils::IteratorType::parallel)
      ++numParallel;
    else if (type == utils::IteratorType::reduction)
      ++numReduction;
  }
  if (numParallel != 2 || numReduction != 1)
    return false;

  unsigned nonCarrierOps = 0;
  for (Operation &nested : body.without_terminator()) {
    if (isCarrierOp(nested))
      continue;
    if (!isa<scf::ForOp>(nested))
      return false;
    ++nonCarrierOps;
  }
  return nonCarrierOps == 1;
}

static bool isReductionTensorCandidate(Block &body,
                                       linalg::GenericOp tensorGeneric) {
  if (tensorGeneric.getNumLoops() == 0 || tensorGeneric.getNumDpsInits() == 0)
    return false;
  // Must have at least one parallel dim to tile.
  bool hasParallel = false;
  bool hasReduction = false;
  for (utils::IteratorType type : tensorGeneric.getIteratorTypesArray()) {
    if (type == utils::IteratorType::parallel)
      hasParallel = true;
    else if (type == utils::IteratorType::reduction)
      hasReduction = true;
  }
  if (!hasParallel || !hasReduction)
    return false;
  // Must have a single nested scf.for (the loop nest).
  unsigned nonCarrierOps = 0;
  for (Operation &nested : body.without_terminator()) {
    if (isCarrierOp(nested))
      continue;
    if (!isa<scf::ForOp>(nested))
      return false;
    ++nonCarrierOps;
  }
  return nonCarrierOps == 1;
}

/// Return a per-SDE-dim mask: true = parallel (should tile), false = reduction
/// (keep original step). The su_iterate's first dim maps to the first carrier
/// iterator type, etc.
static SmallVector<bool>
getParallelDimMask(sde::SdeSuIterateOp op,
                   linalg::GenericOp tensorGeneric) {
  unsigned numDims = op.getLowerBounds().size();
  SmallVector<bool> mask(numDims, true); // default: tile everything
  if (!tensorGeneric)
    return mask;
  auto iterTypes = tensorGeneric.getIteratorTypesArray();
  for (unsigned d = 0; d < numDims && d < iterTypes.size(); ++d) {
    mask[d] = (iterTypes[d] == utils::IteratorType::parallel);
  }
  return mask;
}

static bool isStencilCandidate(sde::SdeSuIterateOp op, Block &body) {
  if (!op.getAccessMinOffsetsAttr() || !op.getAccessMaxOffsetsAttr())
    return false;
  // Stencils always have at least one nested scf.for for the inner dimension.
  // Count the total loop depth: 1 SDE dim + inner scf.for loops.
  unsigned numSuDims = op.getLowerBounds().size();
  unsigned innerLoops = 0;
  Block *current = &body;
  while (true) {
    scf::ForOp nestedLoop;
    for (Operation &nested : current->without_terminator()) {
      if (isCarrierOp(nested))
        continue;
      if (auto forOp = dyn_cast<scf::ForOp>(nested)) {
        if (nestedLoop)
          return false; // imperfect nest
        nestedLoop = forOp;
      } else if (!isScalarExecutableOp(nested)) {
        return false;
      }
    }
    if (!nestedLoop)
      break;
    ++innerLoops;
    current = nestedLoop.getBody();
  }
  return isExecutableInnermostBody(*current) && (numSuDims + innerLoops) >= 1;
}

static SmallVector<int64_t> getStencilHaloWidths(sde::SdeSuIterateOp op) {
  SmallVector<int64_t> halos;
  ArrayAttr minArr = op.getAccessMinOffsetsAttr();
  ArrayAttr maxArr = op.getAccessMaxOffsetsAttr();
  if (!minArr || !maxArr || minArr.size() != maxArr.size())
    return {};
  for (unsigned d = 0; d < minArr.size(); ++d) {
    int64_t lo = cast<IntegerAttr>(minArr[d]).getInt();
    int64_t hi = cast<IntegerAttr>(maxArr[d]).getInt();
    halos.push_back(std::max<int64_t>(1, hi - lo + 1));
  }
  return halos;
}

static bool isTensorOptimizationCandidate(sde::SdeSuIterateOp op, Block &body,
                                          linalg::GenericOp tensorGeneric) {
  if (op.getChunkSize() || !isTensorOptimizationSchedule(op.getScheduleAttr()))
    return false;
  if (op->getParentOfType<scf::ForOp>())
    return false;
  if (op.getLowerBounds().empty())
    return false;
  if (!op.getStructuredClassificationAttr())
    return false;

  unsigned numSuDims = op.getLowerBounds().size();
  switch (*op.getStructuredClassification()) {
  case sde::SdeStructuredClassification::stencil:
    // Stencils don't have tensor carriers; only verify loop nest structure.
    return op.getReductionAccumulators().size() == 0 &&
           isStencilCandidate(op, body);
  case sde::SdeStructuredClassification::elementwise:
    if (!tensorGeneric || op.getReductionAccumulators().size() != 0)
      return false;
    return isElementwiseTensorCandidate(body, tensorGeneric, numSuDims);
  case sde::SdeStructuredClassification::matmul:
    if (!tensorGeneric || op.getReductionAccumulators().size() != 0)
      return false;
    return isMatmulTensorCandidate(body, tensorGeneric);
  case sde::SdeStructuredClassification::reduction:
    if (!tensorGeneric)
      return false;
    return isReductionTensorCandidate(body, tensorGeneric);
  default:
    return false;
  }
}

static void cloneScalarBodyIntoTileLoop(Block &srcBody, IRMapping &mapper,
                                        OpBuilder &builder) {
  for (Operation &op : srcBody.without_terminator()) {
    if (isCarrierOp(op))
      continue;
    builder.clone(op, mapper);
  }
}

struct SdeTensorOptimizationPass
    : public arts::impl::SdeTensorOptimizationBase<SdeTensorOptimizationPass> {
  explicit SdeTensorOptimizationPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    if (!costModel)
      return;

    SmallVector<std::pair<sde::SdeSuIterateOp, linalg::GenericOp>> rewrites;
    getOperation().walk([&](sde::SdeSuIterateOp op) {
      Block &body = op.getBody().front();
      linalg::GenericOp tensorGeneric = findTensorCarrier(body);
      if (!isTensorOptimizationCandidate(op, body, tensorGeneric))
        return;

      std::optional<int64_t> tripCount = getStaticTripCount(op.getOperation());
      if (tripCount && *tripCount <= 1)
        return;
      rewrites.push_back({op, tensorGeneric});
    });

    // Reduction splitting phase: stamp advisory split factor on reduction
    // candidates with large trip counts. The actual loop splitting is deferred
    // to ARTS EdtReductionLowering which handles tree/atomic/linear strategies.
    for (auto [op, tensorGeneric] : rewrites) {
      if (!tensorGeneric)
        continue;
      auto classification = op.getStructuredClassification();
      if (!classification ||
          *classification != sde::SdeStructuredClassification::reduction)
        continue;

      // Find the reduction scf.for loop inside the su_iterate body.
      Block &body = op.getBody().front();
      scf::ForOp reductionLoop;
      for (Operation &bodyOp : body.without_terminator()) {
        if (isCarrierOp(bodyOp))
          continue;
        if (auto forOp = dyn_cast<scf::ForOp>(bodyOp))
          reductionLoop = forOp;
      }
      if (!reductionLoop)
        continue;

      std::optional<int64_t> reductionTrip =
          getStaticTripCount(reductionLoop);
      if (!reductionTrip || *reductionTrip <= 1)
        continue;

      int64_t splitFactor =
          costModel->getReductionSplitFactor(*reductionTrip);
      if (splitFactor <= 1)
        continue;

      op->setAttr("sde.reduction_split_factor",
                  IntegerAttr::get(IndexType::get(op.getContext()),
                                   splitFactor));
    }

    for (auto [op, tensorGeneric] : rewrites) {
      PatternRewriter rewriter(op.getContext());
      rewriter.setInsertionPoint(op);
      Location loc = op.getLoc();
      unsigned numDims = op.getLowerBounds().size();

      // Determine which dims are parallel (should tile) vs reduction (skip).
      SmallVector<bool> parallelMask = getParallelDimMask(op, tensorGeneric);

      // Compute per-dim tile iterations.
      SmallVector<Value> perDimTileIter;
      if (numDims == 1) {
        // 1-D fast path: preserves existing static trip count optimization.
        if (!parallelMask[0]) {
          // Single reduction dim -- nothing to tile.
          continue;
        }
        Value tileIterations;
        if (std::optional<int64_t> tripCount =
                getStaticTripCount(op.getOperation())) {
          int64_t balancedTile = llvm::divideCeil(
              *tripCount, std::max<int64_t>(1, costModel->getWorkerCount()));
          int64_t tileCount = std::clamp(
              std::max<int64_t>(balancedTile,
                                costModel->getMinIterationsPerWorker()),
              int64_t{1}, *tripCount);
          if (tileCount <= 1)
            continue;
          tileIterations = getConstantIndex(rewriter, loc, tileCount);
        } else {
          tileIterations =
              buildTileIterationValue(rewriter, loc, op, *costModel);
        }
        if (!tileIterations)
          continue;
        perDimTileIter.push_back(tileIterations);
      } else {
        // N-dim path: distribute workers across dimensions.
        perDimTileIter =
            buildPerDimTileIterations(rewriter, loc, op, *costModel);
        if (perDimTileIter.empty())
          continue;
      }

      // For stencils, enforce halo-aware minimum tile size per dimension.
      if (op.getStructuredClassification() ==
          sde::SdeStructuredClassification::stencil) {
        SmallVector<int64_t> halos = getStencilHaloWidths(op);
        for (unsigned d = 0; d < numDims && d < halos.size(); ++d) {
          Value haloVal = getConstantIndex(rewriter, loc, halos[d]);
          perDimTileIter[d] =
              arith::MaxUIOp::create(rewriter, loc, perDimTileIter[d], haloVal);
        }
        // Cache-friendly tile: prefer tiles that fit in L2 cache
        int64_t elemSize = 8; // default f64
        op.getBody().walk([&](memref::StoreOp storeOp) {
          Type elemType = storeOp.getValueToStore().getType();
          if (elemType.isF32() || elemType.isInteger(32))
            elemSize = 4;
          return WalkResult::interrupt();
        });
        int64_t l2Size = costModel->getL2CacheSize();
        int64_t cacheLineTile =
            l2Size / (elemSize * std::max<unsigned>(1, numDims));
        Value cacheVal = getConstantIndex(rewriter, loc,
                                          std::max<int64_t>(1, cacheLineTile));
        for (unsigned d = 0; d < numDims; ++d) {
          perDimTileIter[d] =
              arith::MinUIOp::create(rewriter, loc, perDimTileIter[d],
                                     cacheVal);
        }
      }

      // Validate steps and compute tiled steps per dim.
      // For reduction dims, keep the original step (no tiling).
      SmallVector<Value> tiledSteps;
      bool badStep = false;
      bool anyTiled = false;
      for (unsigned d = 0; d < numDims; ++d) {
        Value originalStep = op.getSteps()[d];
        int64_t constantStep = 0;
        if (ValueAnalysis::getConstantIndex(originalStep, constantStep) &&
            constantStep <= 0) {
          badStep = true;
          break;
        }
        if (parallelMask[d]) {
          tiledSteps.push_back(arith::MulIOp::create(
              rewriter, loc, originalStep, perDimTileIter[d]));
          anyTiled = true;
        } else {
          tiledSteps.push_back(originalStep);
        }
      }
      if (badStep || !anyTiled)
        continue;

      auto newOp = sde::SdeSuIterateOp::create(
          rewriter, loc, op.getLowerBounds(), op.getUpperBounds(),
          ValueRange{tiledSteps}, op.getScheduleAttr(), op.getChunkSize(),
          op.getNowaitAttr(), op.getReductionAccumulators(),
          op.getReductionKindsAttr(), op.getReductionStrategyAttr(),
          op.getStructuredClassificationAttr(), op.getAccessMinOffsetsAttr(),
          op.getAccessMaxOffsetsAttr(), op.getOwnerDimsAttr(),
          op.getSpatialDimsAttr(), op.getWriteFootprintAttr());
      newOp->setAttrs(sde::getRewrittenAttrs(op));

      Block &newBody = sde::ensureBlock(newOp.getBody());
      for (unsigned d = newBody.getNumArguments(); d < numDims; ++d)
        newBody.addArgument(rewriter.getIndexType(), loc);

      OpBuilder::InsertionGuard guard(rewriter);
      rewriter.setInsertionPointToStart(&newBody);

      // Build nested scf.for tile loops for parallel dims only.
      // Reduction dims pass through directly (no tiling).
      IRMapping mapper;
      Block &srcBody = op.getBody().front();
      for (unsigned d = 0; d < numDims; ++d) {
        Value tileBase = newBody.getArgument(d);
        if (!parallelMask[d]) {
          // Reduction dim: map directly, no tile loop.
          mapper.map(srcBody.getArgument(d), tileBase);
          continue;
        }
        Value tileLimit =
            arith::AddIOp::create(rewriter, loc, tileBase, tiledSteps[d]);
        Value tileUpper = arith::MinUIOp::create(rewriter, loc, tileLimit,
                                                  op.getUpperBounds()[d]);
        Value originalStep = op.getSteps()[d];
        auto tileLoop = scf::ForOp::create(rewriter, loc, tileBase, tileUpper,
                                            originalStep);
        mapper.map(srcBody.getArgument(d), tileLoop.getInductionVar());
        rewriter.setInsertionPointToStart(tileLoop.getBody());
      }

      // Clone scalar body into the innermost tile loop.
      cloneScalarBodyIntoTileLoop(srcBody, mapper, rewriter);

      // Yield at the end of the su_iterate body (not inside nested loops).
      rewriter.setInsertionPointToEnd(&newBody);
      sde::SdeYieldOp::create(rewriter, loc, ValueRange{});

      rewriter.eraseOp(op);
    }

    // Phase 6E: Dimension collapsing via collapseOpIterationDims.
    // Collapse contiguous parallel inner dims in the tensor carrier when
    // the inner dim product is too small for efficient vectorization.
    getOperation().walk([&](sde::SdeSuIterateOp op) {
      auto classification = op.getStructuredClassification();
      if (!classification)
        return;
      if (*classification != sde::SdeStructuredClassification::elementwise &&
          *classification !=
              sde::SdeStructuredClassification::elementwise_pipeline)
        return;

      Block &body = op.getBody().front();
      linalg::GenericOp carrier = findTensorCarrier(body);
      if (!carrier)
        return;

      unsigned numLoops = carrier.getNumLoops();
      if (numLoops < 2)
        return;

      // Find contiguous parallel inner dims with step=1.
      unsigned numSuDims = op.getLowerBounds().size();
      SmallVector<unsigned> collapsibleDims;
      for (int d = numLoops - 1; d >= 0; --d) {
        auto iterTypes = carrier.getIteratorTypesArray();
        if (iterTypes[d] != utils::IteratorType::parallel)
          break;
        // For dims that map to su_iterate dims, check step=1.
        if (static_cast<unsigned>(d) < numSuDims) {
          int64_t step = 0;
          if (!ValueAnalysis::getConstantIndex(op.getSteps()[d], step) ||
              step != 1)
            break;
        }
        collapsibleDims.push_back(d);
      }
      if (collapsibleDims.size() < 2)
        return;
      std::reverse(collapsibleDims.begin(), collapsibleDims.end());

      // Cost gate: only collapse when inner dim product < vectorWidth * 4.
      int vectorWidth = costModel->getVectorWidth();
      int64_t innerProduct = 1;
      bool allStatic = true;
      for (unsigned d : collapsibleDims) {
        if (d < numSuDims) {
          std::optional<int64_t> tc = getStaticTripCount(op.getOperation());
          if (!tc) {
            allStatic = false;
            break;
          }
          innerProduct *= *tc;
        }
      }
      if (allStatic && innerProduct >= vectorWidth * 4)
        return;

      // Build reassociation: group collapsible dims, keep others singleton.
      SmallVector<ReassociationIndices> reassociation;
      llvm::DenseSet<unsigned> collapsibleSet(collapsibleDims.begin(),
                                              collapsibleDims.end());
      ReassociationIndices currentGroup;
      for (unsigned d = 0; d < numLoops; ++d) {
        if (collapsibleSet.contains(d)) {
          currentGroup.push_back(d);
        } else {
          if (!currentGroup.empty()) {
            reassociation.push_back(std::move(currentGroup));
            currentGroup.clear();
          }
          reassociation.push_back({static_cast<int64_t>(d)});
        }
      }
      if (!currentGroup.empty())
        reassociation.push_back(std::move(currentGroup));

      // Verify precondition: dim sequences must be preserved in all maps.
      if (!linalg::areDimSequencesPreserved(carrier.getIndexingMapsArray(),
                                            reassociation))
        return;

      // Don't collapse dims with halo offsets (stencil dims).
      if (op.getAccessMinOffsetsAttr()) {
        for (unsigned d : collapsibleDims) {
          if (d < op.getAccessMinOffsetsAttr().size()) {
            int64_t lo =
                cast<IntegerAttr>(op.getAccessMinOffsetsAttr()[d]).getInt();
            int64_t hi =
                cast<IntegerAttr>(op.getAccessMaxOffsetsAttr()[d]).getInt();
            if (lo != 0 || hi != 0)
              return;
          }
        }
      }

      IRRewriter rewriter(op.getContext());
      rewriter.setInsertionPoint(carrier);
      FailureOr<linalg::CollapseResult> result =
          linalg::collapseOpIterationDims(carrier, reassociation, rewriter);
      if (failed(result))
        return;

      // Replace carrier results and stamp attribute.
      rewriter.replaceOp(carrier, result->results);
      op->setAttr("sde.collapsed_dims",
                  IntegerAttr::get(IndexType::get(op.getContext()),
                                   collapsibleDims.size()));
    });

    // Phase 6F: Dimension expansion via tensor::ExpandShapeOp.
    // Split a single large dim into (tile_outer, tile_inner) when the
    // working set exceeds L2 cache size.
    getOperation().walk([&](sde::SdeSuIterateOp op) {
      if (op.getLowerBounds().size() != 1)
        return;

      auto classification = op.getStructuredClassification();
      if (!classification)
        return;
      if (*classification != sde::SdeStructuredClassification::elementwise &&
          *classification !=
              sde::SdeStructuredClassification::elementwise_pipeline)
        return;

      Block &body = op.getBody().front();
      linalg::GenericOp carrier = findTensorCarrier(body);
      if (!carrier || carrier.getNumDpsInits() == 0)
        return;

      std::optional<int64_t> tripCount = getStaticTripCount(op.getOperation());
      if (!tripCount)
        return;

      int64_t l2Size = costModel->getL2CacheSize();
      // Estimate element size from the first output operand type.
      int64_t elemSize = 8; // default f64
      if (auto tensorTy =
              dyn_cast<RankedTensorType>(carrier.getDpsInits()[0].getType())) {
        unsigned bitWidth = tensorTy.getElementTypeBitWidth();
        if (bitWidth > 0)
          elemSize = std::max<int64_t>(1, bitWidth / 8);
      }
      int64_t cacheTile = l2Size / elemSize;

      if (*tripCount <= cacheTile * 4)
        return;

      // Expand each tensor operand's dim 0: [N] -> [N/tile, tile].
      int64_t tileSize = std::max<int64_t>(1, cacheTile);
      IRRewriter rewriter(op.getContext());

      for (OpOperand &operand : carrier->getOpOperands()) {
        auto tensorTy = dyn_cast<RankedTensorType>(operand.get().getType());
        if (!tensorTy || tensorTy.getRank() != 1)
          continue;

        int64_t dimSize = tensorTy.getDimSize(0);
        if (ShapedType::isDynamic(dimSize))
          continue;

        int64_t outerSize = llvm::divideCeil(dimSize, tileSize);
        SmallVector<int64_t> expandedShape = {outerSize, tileSize};
        auto expandedTy =
            RankedTensorType::get(expandedShape, tensorTy.getElementType());

        rewriter.setInsertionPoint(carrier);
        SmallVector<ReassociationIndices> reassoc = {{0, 1}};
        auto expandOp = tensor::ExpandShapeOp::create(
            rewriter, carrier.getLoc(), expandedTy, operand.get(), reassoc);
        operand.set(expandOp.getResult());
      }

      op->setAttr("sde.expand_tile_size",
                  IntegerAttr::get(IndexType::get(op.getContext()), tileSize));
    });

    // Phase 6G: Data packing via linalg::pack.
    // For matmul patterns, pack operands for cache-friendly access.
    getOperation().walk([&](sde::SdeSuIterateOp op) {
      auto classification = op.getStructuredClassification();
      if (!classification ||
          *classification != sde::SdeStructuredClassification::matmul)
        return;

      Block &body = op.getBody().front();
      linalg::GenericOp carrier = findTensorCarrier(body);
      if (!carrier)
        return;

      // Compute pack tile sizes: use cache-line-aligned tiles per dim.
      int64_t cacheLineSize = costModel->getCacheLineSize();
      int64_t elemSize = 8; // default f64
      if (auto tensorTy = dyn_cast<RankedTensorType>(
              carrier.getDpsInits()[0].getType())) {
        unsigned bitWidth = tensorTy.getElementTypeBitWidth();
        if (bitWidth > 0)
          elemSize = std::max<int64_t>(1, bitWidth / 8);
      }
      int64_t tileFactor = std::max<int64_t>(1, cacheLineSize / elemSize);

      // Build packed sizes: one per iterator dim. Use tileFactor for all dims.
      unsigned numLoops = carrier.getNumLoops();
      SmallVector<OpFoldResult> packedSizes;
      for (unsigned d = 0; d < numLoops; ++d)
        packedSizes.push_back(
            IntegerAttr::get(IndexType::get(op.getContext()), tileFactor));

      IRRewriter rewriter(op.getContext());
      rewriter.setInsertionPoint(carrier);
      FailureOr<linalg::PackResult> packResult =
          linalg::pack(rewriter, carrier, packedSizes);
      if (failed(packResult))
        return;

      op->setAttr("sde.packed_tile",
                  IntegerAttr::get(IndexType::get(op.getContext()),
                                   tileFactor));
    });
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::arts::sde {

std::unique_ptr<Pass>
createSdeTensorOptimizationPass(sde::SDECostModel *costModel) {
  return std::make_unique<SdeTensorOptimizationPass>(costModel);
}

} // namespace mlir::arts::sde
