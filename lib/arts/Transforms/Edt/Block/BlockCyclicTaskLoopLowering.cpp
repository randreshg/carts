///==========================================================================///
/// File: BlockCyclicTaskLoopLowering.cpp
///
/// Block-cyclic task loop lowering.
///
/// Example transformation:
///   Before (contiguous worker chunk):
///     scf.for %i = %chunkStart to %chunkEnd step %c1 { ... }
///
///   After (cyclic chunk ownership):
///     scf.for %chunk = %workerId to %totalChunks step %totalWorkers {
///       %chunkStart = arith.muli %chunk, %blockSize
///       %chunkEnd = min(%chunkStart + %blockSize, %totalIterations)
///       scf.for %i = %chunkStart to %chunkEnd step %c1 { ... }
///     }
///==========================================================================///

#include "arts/Transforms/Edt/EdtTaskLoopLowering.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

class BlockCyclicTaskLoopLowering final : public EdtTaskLoopLowering {
public:
  TaskLoopLoweringResult
  lower(TaskLoopLoweringInput &input,
        const TaskLoopLoweringMappedValues &mapped) const override {
    TaskLoopLoweringResult result;
    Value one = input.AC->createIndexConstant(1, input.loc);
    Value zero = input.AC->createIndexConstant(0, input.loc);
    result.innerStripeLane = zero;
    result.innerStripeCount = one;

    result.iterStart = zero;
    result.globalBase = mapped.lowerBound;

    Value chunkWorkerStart = input.taskWorkerId;
    if (!chunkWorkerStart.getType().isIndex()) {
      chunkWorkerStart = input.AC->create<arith::IndexCastOp>(
          input.loc, input.AC->getBuilder().getIndexType(), chunkWorkerStart);
    }

    Value chunkStep = input.totalWorkers;
    if (!chunkStep.getType().isIndex()) {
      chunkStep = input.AC->create<arith::IndexCastOp>(
          input.loc, input.AC->getBuilder().getIndexType(), chunkStep);
    }

    scf::ForOp chunkLoop = input.AC->create<scf::ForOp>(
        input.loc, chunkWorkerStart, mapped.totalChunks, chunkStep);
    input.AC->setInsertionPointToStart(chunkLoop.getBody());

    Value chunkId = chunkLoop.getInductionVar();
    Value chunkStart =
        input.AC->create<arith::MulIOp>(input.loc, chunkId, mapped.blockSize);
    Value chunkEndHint = input.AC->create<arith::AddIOp>(input.loc, chunkStart,
                                                         mapped.blockSize);
    Value chunkEnd = input.AC->create<arith::MinUIOp>(
        input.loc, chunkEndHint, mapped.totalIterations);

    result.iterLoop = input.AC->create<scf::ForOp>(input.loc, chunkStart,
                                                   chunkEnd, one);
    return result;
  }

  void collectExtraExternalValues(const TaskLoopLoweringInput &input,
                                  llvm::SetVector<Value> &values) const override {
    values.insert(input.blockSize);
    values.insert(input.totalIterations);
    values.insert(input.totalChunks);
  }
};

} // namespace

std::unique_ptr<EdtTaskLoopLowering>
mlir::arts::detail::createBlockCyclicTaskLoopLowering() {
  return std::make_unique<BlockCyclicTaskLoopLowering>();
}
