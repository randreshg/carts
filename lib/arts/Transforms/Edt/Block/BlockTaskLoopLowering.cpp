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
    return lowerBlockStyle(input, mapped);
  }
};

} // namespace

std::unique_ptr<EdtTaskLoopLowering>
mlir::arts::detail::createBlockTaskLoopLowering() {
  return std::make_unique<BlockTaskLoopLowering>();
}
