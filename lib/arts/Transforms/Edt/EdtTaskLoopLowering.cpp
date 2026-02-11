///==========================================================================///
/// File: EdtTaskLoopLowering.cpp
///
/// Factory for strategy-specific task loop lowerers.
///==========================================================================///

#include "arts/Transforms/Edt/EdtTaskLoopLowering.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

using namespace mlir;
using namespace mlir::arts;

TaskAcquirePlanningResult
EdtTaskLoopLowering::planAcquireRewrite(const TaskLoopLoweringInput &input,
                                        Value chunkOffset) const {
  TaskAcquirePlanningResult result;
  if (!input.AC)
    return result;

  Value one = input.AC->createIndexConstant(1, input.loc);
  Value stepVal = input.loopStep ? input.loopStep : one;
  int64_t stepConst = 0;
  bool stepIsUnit =
      !stepVal ||
      (ValueUtils::getConstantIndex(stepVal, stepConst) && stepConst == 1);

  Value lowerBoundVal =
      input.chunkLowerBound ? input.chunkLowerBound : input.lowerBound;
  Value workerOffsetVal = chunkOffset;
  if (!stepIsUnit)
    workerOffsetVal =
        input.AC->create<arith::MulIOp>(input.loc, workerOffsetVal, stepVal);
  workerOffsetVal = input.AC->create<arith::AddIOp>(input.loc, lowerBoundVal,
                                                    workerOffsetVal);

  result.workerBaseOffset = workerOffsetVal;
  result.acquireOffset = workerOffsetVal;
  result.acquireSize = input.bounds.iterCount;
  result.acquireHintSize = input.bounds.iterCountHint
                               ? input.bounds.iterCountHint
                               : result.acquireSize;
  result.step = stepVal;
  result.stepIsUnit = stepIsUnit;
  return result;
}

std::unique_ptr<EdtTaskLoopLowering>
EdtTaskLoopLowering::create(DistributionKind kind) {
  switch (kind) {
  case DistributionKind::Flat:
  case DistributionKind::TwoLevel:
    return detail::createBlockTaskLoopLowering();
  case DistributionKind::BlockCyclic:
    return detail::createBlockCyclicTaskLoopLowering();
  case DistributionKind::Tiling2D:
    return detail::createTiling2DTaskLoopLowering();
  }
  return detail::createBlockTaskLoopLowering();
}
