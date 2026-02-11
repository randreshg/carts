///==========================================================================///
/// File: Tiling2DTaskLoopLowering.cpp
///
/// Matmul-oriented task loop lowering.
/// This path keeps ownership-safe row partitioning while applying a dedicated
/// column-striping rewrite inside each task for matmul-like loop bodies.
///
/// Example transformation (post-clone striping):
///   Before:
///     scf.for %j = %c0 to %NJ step %c1 {
///       memref.store %v, %C[%globalI, %j] : memref<?x?xf32>
///     }
///
///   After:
///     %jLowerLane = %c0 + (%colWorkerId * %c1)
///     %jStepLane = %c1 * %colWorkers
///     scf.for %j = %jLowerLane to %NJ step %jStepLane {
///       memref.store %v, %C[%globalI, %j] : memref<?x?xf32>
///     }
///==========================================================================///

#include "arts/Transforms/Edt/EdtTaskLoopLowering.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/ADT/STLExtras.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool usesAsColumnOfRow(Value idx0, Value idx1, Value rowIdx, Value iv) {
  return rowIdx && iv && ValueUtils::dependsOn(idx0, rowIdx) &&
         ValueUtils::dependsOn(idx1, iv);
}

static bool isColumnLoopForGlobalIter(scf::ForOp loop, Value globalIter) {
  if (!loop || !globalIter)
    return false;
  if (!loop.getInitArgs().empty() || loop.getNumResults() != 0)
    return false;

  Value iv = loop.getInductionVar();
  bool matches = false;
  loop.walk([&](Operation *op) {
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      ValueRange idx = store.getIndices();
      if (idx.size() >= 2 &&
          usesAsColumnOfRow(idx[0], idx[1], globalIter, iv)) {
        matches = true;
        return WalkResult::interrupt();
      }
    }
    return WalkResult::advance();
  });
  return matches;
}

static void stripeLoopByColumns(TaskLoopPostCloneInput &input,
                                scf::ForOp loop) {
  ArtsCodegen *AC = input.AC;
  if (!AC || !loop || !input.innerStripeLane || !input.innerStripeCount)
    return;

  if (!loop.getInitArgs().empty() || loop.getNumResults() != 0)
    return;

  OpBuilder &builder = AC->getBuilder();
  OpBuilder::InsertionGuard guard(builder);
  Location loc = loop.getLoc();
  builder.setInsertionPoint(loop);
  Value lane = input.innerStripeLane;
  Value step = loop.getStep();
  Value laneOffset = builder.create<arith::MulIOp>(loc, lane, step);
  Value stripedLower =
      builder.create<arith::AddIOp>(loc, loop.getLowerBound(), laneOffset);
  Value stripedStep =
      builder.create<arith::MulIOp>(loc, step, input.innerStripeCount);

  scf::ForOp striped = builder.create<scf::ForOp>(
      loc, stripedLower, loop.getUpperBound(), stripedStep);
  striped->setAttrs(loop->getAttrs());

  IRMapping map;
  map.map(loop.getInductionVar(), striped.getInductionVar());
  builder.setInsertionPointToStart(striped.getBody());
  for (Operation &op : loop.getBody()->without_terminator())
    builder.clone(op, map);

  loop.erase();
}

class Tiling2DTaskLoopLowering final : public EdtTaskLoopLowering {
public:
  TaskAcquirePlanningResult
  planAcquireRewrite(const TaskLoopLoweringInput &input,
                     Value chunkOffset) const override {
    TaskAcquirePlanningResult result =
        EdtTaskLoopLowering::planAcquireRewrite(input, chunkOffset);
    result.tiling2DGrid = DistributionHeuristics::getTiling2DWorkerGrid(
        input.AC, input.loc, input.taskWorkerId, input.totalWorkers);
    return result;
  }

  TaskLoopLoweringResult
  lower(TaskLoopLoweringInput &input,
        const TaskLoopLoweringMappedValues &mapped) const override {
    TaskLoopLoweringResult result;

    Value zero = input.AC->createIndexConstant(0, input.loc);
    Value one = input.AC->createIndexConstant(1, input.loc);
    Tiling2DWorkerGrid grid = DistributionHeuristics::getTiling2DWorkerGrid(
        input.AC, input.loc, input.taskWorkerId, input.totalWorkers);
    result.innerStripeLane = grid.colWorkerId;
    result.innerStripeCount = grid.colWorkers;

    result.insideBounds = DistributionHeuristics::recomputeBoundsInside(
        input.AC, input.loc, input.strategy, input.taskWorkerId,
        input.totalWorkers, input.workersPerNode, input.upperBound,
        input.lowerBound, input.loopStep, input.blockSize,
        input.alignmentBlockSize, input.useRuntimeBlockAlignment);
    result.iterStart = result.insideBounds.iterStart;
    result.globalBase = mapped.workerBaseOffset;

    Value loopLower = zero;
    Value loopUpper = result.insideBounds.iterCount;
    if (input.useAlignedLowerBound) {
      Value stepClamped =
          input.AC->create<arith::MaxUIOp>(input.loc, mapped.step, one);

      auto clampNonNeg = [&](Value v) -> Value {
        Value isNeg = input.AC->create<arith::CmpIOp>(
            input.loc, arith::CmpIPredicate::slt, v, zero);
        return input.AC->create<arith::SelectOp>(input.loc, isNeg, zero, v);
      };
      auto ceilDiv = [&](Value num, Value denom) -> Value {
        Value denomMinusOne =
            input.AC->create<arith::SubIOp>(input.loc, denom, one);
        Value adjusted =
            input.AC->create<arith::AddIOp>(input.loc, num, denomMinusOne);
        return input.AC->create<arith::DivUIOp>(input.loc, adjusted, denom);
      };

      Value diffLower = input.AC->create<arith::SubIOp>(
          input.loc, mapped.lowerBound, mapped.workerBaseOffset);
      Value diffUpper = input.AC->create<arith::SubIOp>(
          input.loc, mapped.upperBound, mapped.workerBaseOffset);
      Value diffLowerPos = clampNonNeg(diffLower);
      Value diffUpperPos = clampNonNeg(diffUpper);

      loopLower = ceilDiv(diffLowerPos, stepClamped);
      loopUpper = ceilDiv(diffUpperPos, stepClamped);
      loopUpper = input.AC->create<arith::MinUIOp>(
          input.loc, loopUpper, result.insideBounds.iterCount);
    }

    result.iterLoop =
        input.AC->create<scf::ForOp>(input.loc, loopLower, loopUpper, one);
    return result;
  }

  void postCloneAdjust(TaskLoopPostCloneInput &input) const override {
    if (!input.AC || !input.iterLoop || !input.globalIter ||
        !input.innerStripeLane || !input.innerStripeCount)
      return;

    auto stripeConst = ValueUtils::tryFoldConstantIndex(input.innerStripeCount);
    if (!stripeConst || *stripeConst <= 1)
      return;

    SmallVector<scf::ForOp, 8> columnLoops;
    input.iterLoop.walk([&](scf::ForOp nested) {
      if (nested == input.iterLoop)
        return;
      if (isColumnLoopForGlobalIter(nested, input.globalIter))
        columnLoops.push_back(nested);
    });

    for (scf::ForOp loop : llvm::reverse(columnLoops)) {
      if (!loop || !loop->getBlock())
        continue;
      stripeLoopByColumns(input, loop);
    }
  }
};

} // namespace

std::unique_ptr<EdtTaskLoopLowering>
mlir::arts::detail::createTiling2DTaskLoopLowering() {
  return std::make_unique<Tiling2DTaskLoopLowering>();
}
