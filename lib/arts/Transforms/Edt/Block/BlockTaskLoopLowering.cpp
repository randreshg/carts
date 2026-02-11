///==========================================================================///
/// File: BlockTaskLoopLowering.cpp
///
/// Block/two-level task loop lowering.
///
/// Example transformation:
///   Before (source loop intent):
///     arts.for (%i = %lb to %ub step %step) { ... use %i ... }
///
///   After (task-local lowered form):
///     scf.for %li = %loopLower to %loopUpper step %c1 {
///       %scaled = arith.muli %li, %step
///       %gi = arith.addi %workerBase, %scaled
///       ... use %gi ...
///     }
///==========================================================================///

#include "arts/Transforms/Edt/EdtTaskLoopLowering.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

class BlockTaskLoopLowering final : public EdtTaskLoopLowering {
public:
  TaskAcquirePlanningResult
  planAcquireRewrite(const TaskLoopLoweringInput &input,
                     Value chunkOffset) const override {
    TaskAcquirePlanningResult result =
        EdtTaskLoopLowering::planAcquireRewrite(input, chunkOffset);

    if (input.strategy.kind != DistributionKind::TwoLevel)
      return result;

    Value lowerBoundVal =
        input.chunkLowerBound ? input.chunkLowerBound : input.lowerBound;
    Value acquireOffsetVal = input.bounds.acquireStart;
    if (!result.stepIsUnit) {
      acquireOffsetVal = input.AC->create<arith::MulIOp>(
          input.loc, acquireOffsetVal, result.step);
    }
    acquireOffsetVal = input.AC->create<arith::AddIOp>(input.loc, lowerBoundVal,
                                                       acquireOffsetVal);

    result.acquireOffset = acquireOffsetVal;
    result.acquireSize = input.bounds.acquireSize;
    result.acquireHintSize = input.bounds.acquireSizeHint
                                 ? input.bounds.acquireSizeHint
                                 : result.acquireSize;
    if (!input.useAlignedLowerBound)
      result.acquireHintSize = result.acquireSize;
    return result;
  }

  TaskLoopLoweringResult
  lower(TaskLoopLoweringInput &input,
        const TaskLoopLoweringMappedValues &mapped) const override {
    TaskLoopLoweringResult result;

    Value zero = input.AC->createIndexConstant(0, input.loc);
    Value one = input.AC->createIndexConstant(1, input.loc);
    result.innerStripeLane = zero;
    result.innerStripeCount = one;

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
};

} // namespace

std::unique_ptr<EdtTaskLoopLowering>
mlir::arts::detail::createBlockTaskLoopLowering() {
  return std::make_unique<BlockTaskLoopLowering>();
}
