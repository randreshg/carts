///==========================================================================///
/// File: SdeChunkOptimization.cpp
///
/// Cost-model-backed SDE chunk sizing. Preserves explicit source chunk sizes
/// and synthesizes chunks only for one-dimensional dynamic/guided loops,
/// using either constant or symbolic trip-count arithmetic.
///==========================================================================///

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_SDECHUNKOPTIMIZATION
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

#include "arts/utils/ValueAnalysis.h"
#include "arts/utils/costs/SDECostModel.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"

#include <algorithm>

using namespace mlir;
using namespace mlir::arts;

namespace {

static NamedAttrList getRewrittenAttrs(Operation *op) {
  NamedAttrList attrs(op->getAttrs());
  attrs.erase("operandSegmentSizes");
  return attrs;
}

static bool isChunkOptimizableSchedule(sde::SdeScheduleKindAttr schedule) {
  if (!schedule)
    return false;
  switch (schedule.getValue()) {
  case sde::SdeScheduleKind::dynamic:
  case sde::SdeScheduleKind::guided:
    return true;
  default:
    return false;
  }
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

static Value buildSymbolicChunkValue(OpBuilder &builder, Location loc,
                                     sde::SdeSuIterateOp op,
                                     int64_t workerCount,
                                     int64_t minIterations) {
  Value tripCount = buildTripCountValue(builder, loc, op);
  if (!tripCount)
    return Value();

  Value one = getConstantIndex(builder, loc, 1);
  Value workerCountValue =
      getConstantIndex(builder, loc, std::max<int64_t>(1, workerCount));
  Value minIterationsValue =
      getConstantIndex(builder, loc, std::max<int64_t>(1, minIterations));

  Value clampedTripCount = arith::MaxUIOp::create(builder, loc, tripCount, one);
  Value balancedChunk =
      arith::CeilDivUIOp::create(builder, loc, clampedTripCount,
                                 workerCountValue);
  Value preferredChunk = arith::MaxUIOp::create(builder, loc, balancedChunk,
                                                minIterationsValue);
  return arith::MinUIOp::create(builder, loc, preferredChunk, clampedTripCount);
}

struct ChunkRewrite {
  sde::SdeSuIterateOp op;
  std::optional<int64_t> chunkSize;
};

struct SdeChunkOptimizationPass
    : public arts::impl::SdeChunkOptimizationBase<SdeChunkOptimizationPass> {
  explicit SdeChunkOptimizationPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    if (!costModel)
      return;

    SmallVector<ChunkRewrite> rewrites;
    getOperation().walk([&](sde::SdeSuIterateOp op) {
      if (op.getChunkSize() || !isChunkOptimizableSchedule(op.getScheduleAttr()))
        return;

      std::optional<int64_t> tripCount = getConstantTripCount(op);
      if (tripCount && *tripCount <= 1)
        return;

      if (tripCount) {
        int64_t workerCount = std::max<int64_t>(1, costModel->getWorkerCount());
        int64_t minIterations = std::max<int64_t>(
            1, costModel->getMinIterationsPerWorker());
        int64_t balancedChunk = ceilDiv(*tripCount, workerCount);
        int64_t chunkSize =
            std::clamp(std::max(minIterations, balancedChunk), int64_t{1},
                       *tripCount);
        rewrites.push_back({op, chunkSize});
        return;
      }

      if (op.getLowerBounds().size() != 1 || op.getUpperBounds().size() != 1 ||
          op.getSteps().size() != 1)
        return;

      rewrites.push_back({op, std::nullopt});
    });

    for (ChunkRewrite &rewrite : rewrites) {
      IRRewriter rewriter(rewrite.op.getContext());
      rewriter.setInsertionPoint(rewrite.op);

      Value chunkSize;
      if (rewrite.chunkSize) {
        chunkSize = getConstantIndex(rewriter, rewrite.op.getLoc(),
                                     *rewrite.chunkSize);
      } else {
        chunkSize = buildSymbolicChunkValue(
            rewriter, rewrite.op.getLoc(), rewrite.op,
            std::max<int64_t>(1, costModel->getWorkerCount()),
            std::max<int64_t>(1, costModel->getMinIterationsPerWorker()));
      }
      if (!chunkSize)
        continue;

      auto newOp = sde::SdeSuIterateOp::create(
          rewriter, rewrite.op.getLoc(), rewrite.op.getLowerBounds(),
          rewrite.op.getUpperBounds(), rewrite.op.getSteps(),
          rewrite.op.getScheduleAttr(), chunkSize,
          rewrite.op.getNowaitAttr(), rewrite.op.getReductionAccumulators(),
          rewrite.op.getReductionKindsAttr(),
          rewrite.op.getReductionStrategyAttr());
      newOp->setAttrs(getRewrittenAttrs(rewrite.op.getOperation()));
      newOp.getBody().takeBody(rewrite.op.getBody());
      rewriter.eraseOp(rewrite.op);
    }
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::arts::sde {

std::unique_ptr<Pass>
createSdeChunkOptimizationPass(sde::SDECostModel *costModel) {
  return std::make_unique<SdeChunkOptimizationPass>(costModel);
}

} // namespace mlir::arts::sde
