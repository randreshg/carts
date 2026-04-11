///==========================================================================///
/// File: SdeTensorOptimization.cpp
///
/// Cost-model-driven tensor optimization in SDE. The current implementation
/// focuses on the first executable subset: one-dimensional elementwise loops
/// whose tensor-backed linalg carrier proves that the loop body is safe to
/// strip-mine. The pass rewrites the surrounding sde.su_iterate so the tiled
/// loop nest survives SDE->ARTS lowering while tensor carriers remain an
/// SDE-only concern.
///==========================================================================///

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_SDETENSOROPTIMIZATION
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

#include "arts/utils/ValueAnalysis.h"
#include "arts/utils/costs/SDECostModel.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/PatternMatch.h"

#include <algorithm>

using namespace mlir;
using namespace mlir::arts;

namespace {

static NamedAttrList getRewrittenAttrs(sde::SdeSuIterateOp op) {
  NamedAttrList attrs(op->getAttrs());
  attrs.erase(op.getOperandSegmentSizesAttrName().getValue());
  return attrs;
}

static Block &ensureBlock(Region &region) {
  if (region.empty())
    region.push_back(new Block());
  return region.front();
}

static bool isTensorOptimizationSchedule(sde::SdeScheduleKindAttr schedule) {
  if (!schedule)
    return true;
  return schedule.getValue() == sde::SdeScheduleKind::static_;
}

static std::optional<int64_t> getConstantTripCount(sde::SdeSuIterateOp op) {
  if (op.getLowerBounds().size() != 1 || op.getUpperBounds().size() != 1 ||
      op.getSteps().size() != 1)
    return std::nullopt;

  int64_t lb = 0, ub = 0, step = 0;
  if (!ValueAnalysis::getConstantIndex(op.getLowerBounds().front(), lb) ||
      !ValueAnalysis::getConstantIndex(op.getUpperBounds().front(), ub) ||
      !ValueAnalysis::getConstantIndex(op.getSteps().front(), step) ||
      step <= 0)
    return std::nullopt;

  if (ub <= lb)
    return int64_t{0};

  int64_t span = ub - lb;
  return (span + step - 1) / step;
}

static int64_t ceilDiv(int64_t lhs, int64_t rhs) {
  return (lhs + rhs - 1) / rhs;
}

static Value getConstantIndex(OpBuilder &builder, Location loc, int64_t value) {
  return arith::ConstantIndexOp::create(builder, loc, value);
}

static Value buildTripCountValue(OpBuilder &builder, Location loc,
                                 sde::SdeSuIterateOp op) {
  if (std::optional<int64_t> tripCount = getConstantTripCount(op))
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
  Value nonNegativeSpan = arith::MaxUIOp::create(builder, loc, span, zero);

  int64_t constantStep = 0;
  Value safeStep = step;
  if (ValueAnalysis::getConstantIndex(step, constantStep)) {
    if (constantStep <= 0)
      return Value();
  } else {
    safeStep = arith::MaxUIOp::create(builder, loc, step, one);
  }

  return arith::CeilDivUIOp::create(builder, loc, nonNegativeSpan, safeStep);
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
  Value minIterationsValue =
      getConstantIndex(builder, loc,
                       std::max<int64_t>(1, costModel.getMinIterationsPerWorker()));

  Value clampedTripCount = arith::MaxUIOp::create(builder, loc, tripCount, one);
  Value balancedTile =
      arith::CeilDivUIOp::create(builder, loc, clampedTripCount,
                                 workerCountValue);
  Value preferredTile = arith::MaxUIOp::create(builder, loc, balancedTile,
                                               minIterationsValue);
  return arith::MinUIOp::create(builder, loc, preferredTile, clampedTripCount);
}

static bool isCarrierOp(Operation &op) {
  return isa<bufferization::ToTensorOp, tensor::EmptyOp, linalg::GenericOp>(op);
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

static bool isTensorOptimizationCandidate(sde::SdeSuIterateOp op,
                                          Block &body,
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
  if (!op.getLinalgClassificationAttr() ||
      op.getLinalgClassification() != sde::SdeLinalgClassification::elementwise)
    return false;
  if (tensorGeneric.getNumLoops() != 1)
    return false;
  if (!llvm::all_of(tensorGeneric.getIteratorTypesArray(),
                    [](utils::IteratorType type) {
                      return type == utils::IteratorType::parallel;
                    }))
    return false;
  if (tensorGeneric.getNumDpsInits() == 0)
    return false;

  unsigned numLoads = 0;
  unsigned numStores = 0;
  for (Operation &nested : body.without_terminator()) {
    if (nested.getNumRegions() != 0 && !isa<linalg::GenericOp>(nested))
      return false;
    if (isa<memref::LoadOp>(nested))
      ++numLoads;
    if (isa<memref::StoreOp>(nested))
      ++numStores;
  }
  return numLoads == tensorGeneric.getNumDpsInputs() &&
         numStores == tensorGeneric.getNumDpsInits();
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

      std::optional<int64_t> tripCount = getConstantTripCount(op);
      if (tripCount && *tripCount <= 1)
        return;
      rewrites.push_back(op);
    });

    for (sde::SdeSuIterateOp op : rewrites) {
      PatternRewriter rewriter(op.getContext());
      rewriter.setInsertionPoint(op);
      Location loc = op.getLoc();

      Value tileIterations;
      if (std::optional<int64_t> tripCount = getConstantTripCount(op)) {
        int64_t balancedTile =
            ceilDiv(*tripCount, std::max<int64_t>(1, costModel->getWorkerCount()));
        int64_t tileCount = std::clamp(
            std::max<int64_t>(balancedTile, costModel->getMinIterationsPerWorker()),
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
          op.getLinalgClassificationAttr());
      newOp->setAttrs(getRewrittenAttrs(op));

      Block &newBody = ensureBlock(newOp.getBody());
      if (newBody.getNumArguments() == 0)
        newBody.addArgument(rewriter.getIndexType(), loc);
      Value tileBase = newBody.getArgument(0);

      OpBuilder::InsertionGuard guard(rewriter);
      rewriter.setInsertionPointToStart(&newBody);
      Value tileLimit = arith::AddIOp::create(rewriter, loc, tileBase, tiledStep);
      Value tileUpper =
          arith::MinUIOp::create(rewriter, loc, tileLimit,
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
