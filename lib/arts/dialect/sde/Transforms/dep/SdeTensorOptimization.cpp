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

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

#include "llvm/Support/MathExtras.h"

#include <algorithm>

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
                                         linalg::GenericOp tensorGeneric) {
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
  return hasPerfectNestedScalarLoopNest(body, tensorGeneric.getNumLoops());
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

static bool isTensorOptimizationCandidate(sde::SdeSuIterateOp op, Block &body,
                                          linalg::GenericOp tensorGeneric) {
  if (!tensorGeneric)
    return false;
  if (op.getChunkSize() || !isTensorOptimizationSchedule(op.getScheduleAttr()))
    return false;
  if (op.getReductionAccumulators().size() != 0)
    return false;
  if (op->getParentOfType<scf::ForOp>())
    return false;
  if (op.getLowerBounds().size() != 1 || op.getUpperBounds().size() != 1 ||
      op.getSteps().size() != 1)
    return false;
  if (!op.getStructuredClassificationAttr())
    return false;

  switch (*op.getStructuredClassification()) {
  case sde::SdeStructuredClassification::elementwise:
    return isElementwiseTensorCandidate(body, tensorGeneric);
  case sde::SdeStructuredClassification::matmul:
    return isMatmulTensorCandidate(body, tensorGeneric);
  default:
    return false;
  }
}

static void cloneScalarBodyIntoTileLoop(Block &srcBody, scf::ForOp tileLoop,
                                        PatternRewriter &rewriter) {
  IRMapping mapper;
  mapper.map(srcBody.getArgument(0), tileLoop.getInductionVar());

  OpBuilder::InsertionGuard guard(rewriter);
  rewriter.setInsertionPoint(tileLoop.getBody()->getTerminator());
  for (Operation &op : srcBody.without_terminator()) {
    if (isCarrierOp(op))
      continue;
    rewriter.clone(op, mapper);
  }
}

struct SdeTensorOptimizationPass
    : public arts::impl::SdeTensorOptimizationBase<SdeTensorOptimizationPass> {
  explicit SdeTensorOptimizationPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    if (!costModel)
      return;

    SmallVector<sde::SdeSuIterateOp> rewrites;
    getOperation().walk([&](sde::SdeSuIterateOp op) {
      Block &body = op.getBody().front();
      linalg::GenericOp tensorGeneric = findTensorCarrier(body);
      if (!isTensorOptimizationCandidate(op, body, tensorGeneric))
        return;

      std::optional<int64_t> tripCount = getStaticTripCount(op.getOperation());
      if (tripCount && *tripCount <= 1)
        return;
      rewrites.push_back(op);
    });

    for (sde::SdeSuIterateOp op : rewrites) {
      PatternRewriter rewriter(op.getContext());
      rewriter.setInsertionPoint(op);
      Location loc = op.getLoc();

      Value tileIterations;
      if (std::optional<int64_t> tripCount = getStaticTripCount(op.getOperation())) {
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
        tileIterations = buildTileIterationValue(rewriter, loc, op, *costModel);
      }
      if (!tileIterations)
        continue;

      Value originalStep = op.getSteps().front();
      int64_t constantStep = 0;
      if (ValueAnalysis::getConstantIndex(originalStep, constantStep) &&
          constantStep <= 0)
        continue;

      Value tiledStep =
          arith::MulIOp::create(rewriter, loc, originalStep, tileIterations);

      auto newOp = sde::SdeSuIterateOp::create(
          rewriter, loc, op.getLowerBounds(), op.getUpperBounds(),
          ValueRange{tiledStep}, op.getScheduleAttr(), op.getChunkSize(),
          op.getNowaitAttr(), op.getReductionAccumulators(),
          op.getReductionKindsAttr(), op.getReductionStrategyAttr(),
          op.getStructuredClassificationAttr(), op.getAccessMinOffsetsAttr(),
          op.getAccessMaxOffsetsAttr(), op.getOwnerDimsAttr(),
          op.getSpatialDimsAttr(), op.getWriteFootprintAttr());
      newOp->setAttrs(sde::getRewrittenAttrs(op));

      Block &newBody = sde::ensureBlock(newOp.getBody());
      if (newBody.getNumArguments() == 0)
        newBody.addArgument(rewriter.getIndexType(), loc);
      Value tileBase = newBody.getArgument(0);

      OpBuilder::InsertionGuard guard(rewriter);
      rewriter.setInsertionPointToStart(&newBody);
      Value tileLimit =
          arith::AddIOp::create(rewriter, loc, tileBase, tiledStep);
      Value tileUpper = arith::MinUIOp::create(rewriter, loc, tileLimit,
                                               op.getUpperBounds().front());
      auto tileLoop =
          scf::ForOp::create(rewriter, loc, tileBase, tileUpper, originalStep);
      cloneScalarBodyIntoTileLoop(op.getBody().front(), tileLoop, rewriter);
      sde::SdeYieldOp::create(rewriter, loc, ValueRange{});

      rewriter.eraseOp(op);
    }
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
