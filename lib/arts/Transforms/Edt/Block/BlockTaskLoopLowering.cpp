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

    /// Two-level routing maps workers to nodes, but task acquires must stay at
    /// per-worker granularity to avoid cross-worker overlap on write-capable
    /// buffers.
    result.acquireOffset = result.workerBaseOffset;
    result.acquireSize = input.bounds.iterCount;

    Value hintSize = input.bounds.acquireSizeHint
                         ? input.bounds.acquireSizeHint
                         : input.bounds.acquireSize;
    result.acquireHintSize = hintSize ? hintSize : result.acquireSize;
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
