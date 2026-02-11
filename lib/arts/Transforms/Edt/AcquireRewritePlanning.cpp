///==========================================================================///
/// File: AcquireRewritePlanning.cpp
///
/// Plans per-task DbAcquire rewrite inputs by distribution strategy.
///
/// Example (tiling_2d output owner hints):
///   Before:
///     %acq = arts.db_acquire[<inout>] ... offsets[%rowOff], sizes[%rowSize]
///
///   Planned rewrite:
///     - keep row partitioning in offsets/sizes
///     - append [colOff, colSize] to partition_offsets/partition_sizes
///       so DbPartitioning can materialize 2D owner layout for outputs.
///==========================================================================///

#include "arts/Transforms/Edt/AcquireRewritePlanning.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

static Value castToIndex(ArtsCodegen *AC, Value value, Location loc) {
  if (!value)
    return value;
  if (value.getType().isIndex())
    return value;
  return AC->create<arith::IndexCastOp>(loc, AC->getBuilder().getIndexType(),
                                        value);
}

} // namespace

AcquireRewritePlan
mlir::arts::planAcquireRewrite(AcquireRewritePlanningInput input) {
  auto makePlan = [&]() {
    return AcquireRewritePlan{
        AcquireRewriteInput{input.AC, input.loc, input.parentAcquire,
                            input.rootGuid, input.rootPtr, input.acquireOffset,
                            input.acquireSize, input.acquireHintSize,
                            /*extraOffsets=*/SmallVector<Value, 4>{},
                            /*extraSizes=*/SmallVector<Value, 4>{},
                            /*extraHintSizes=*/SmallVector<Value, 4>{},
                            input.step, input.stepIsUnit,
                            /*singleElement=*/false,
                            /*forceCoarse=*/
                            input.distributionKind ==
                                DistributionKind::BlockCyclic,
                            /*stencilExtent=*/Value()},
        /*useStencilRewriter=*/false};
  };

  if (!input.AC || !input.parentAcquire)
    return makePlan();

  Value zero = input.AC->createIndexConstant(0, input.loc);
  Value one = input.AC->createIndexConstant(1, input.loc);
  AcquireRewritePlan plan{makePlan()};

  bool isSingleElement = false;
  bool needsStencilHalo = false;
  Value stencilExtent;
  SmallVector<Value, 4> extraOffsets;
  SmallVector<Value, 4> extraSizes;
  SmallVector<Value, 4> extraHintSizes;

  if (Operation *rootAllocOp =
          DatablockUtils::getUnderlyingDbAlloc(input.rootPtr)) {
    if (auto dbAlloc = dyn_cast<DbAllocOp>(rootAllocOp)) {
      auto elemSizes = dbAlloc.getElementSizes();
      if (!elemSizes.empty()) {
        isSingleElement = llvm::all_of(elemSizes, [](Value value) {
          int64_t constant = 0;
          return ValueUtils::getConstantIndex(value, constant) && constant == 1;
        });

        auto accessPattern = getDbAccessPattern(dbAlloc.getOperation());
        needsStencilHalo = !isSingleElement && accessPattern &&
                           *accessPattern == DbAccessPattern::stencil &&
                           input.parentAcquire.getMode() == ArtsMode::in;
        if (needsStencilHalo)
          stencilExtent = castToIndex(input.AC, elemSizes.front(), input.loc);

        if (input.distributionKind == DistributionKind::Tiling2D &&
            input.parentAcquire.getMode() == ArtsMode::inout &&
            elemSizes.size() > 1 && !isSingleElement && input.tiling2DGrid &&
            input.tiling2DGrid->colWorkers) {
          Value totalCols = castToIndex(input.AC, elemSizes[1], input.loc);
          Value colWorkers =
              castToIndex(input.AC, input.tiling2DGrid->colWorkers, input.loc);
          Value colWorkerId =
              castToIndex(input.AC, input.tiling2DGrid->colWorkerId, input.loc);

          Value colWorkersMinusOne =
              input.AC->create<arith::SubIOp>(input.loc, colWorkers, one);
          Value colAdjusted = input.AC->create<arith::AddIOp>(
              input.loc, totalCols, colWorkersMinusOne);
          Value colChunk = input.AC->create<arith::DivUIOp>(
              input.loc, colAdjusted, colWorkers);
          Value colOffset =
              input.AC->create<arith::MulIOp>(input.loc, colWorkerId, colChunk);

          Value colNeedZero = input.AC->create<arith::CmpIOp>(
              input.loc, arith::CmpIPredicate::uge, colOffset, totalCols);
          Value colRemaining =
              input.AC->create<arith::SubIOp>(input.loc, totalCols, colOffset);
          Value colRemainingNonNeg = input.AC->create<arith::SelectOp>(
              input.loc, colNeedZero, zero, colRemaining);
          Value colCount = input.AC->create<arith::MinUIOp>(input.loc, colChunk,
                                                            colRemainingNonNeg);

          extraOffsets.push_back(colOffset);
          extraSizes.push_back(colCount);
          extraHintSizes.push_back(colCount);
        }
      }
    }
  }

  plan.useStencilRewriter = needsStencilHalo;
  plan.rewriteInput.singleElement = isSingleElement;
  plan.rewriteInput.stencilExtent = stencilExtent;
  plan.rewriteInput.extraOffsets = std::move(extraOffsets);
  plan.rewriteInput.extraSizes = std::move(extraSizes);
  plan.rewriteInput.extraHintSizes = std::move(extraHintSizes);
  return plan;
}
