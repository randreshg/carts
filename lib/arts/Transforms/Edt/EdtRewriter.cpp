///==========================================================================///
/// File: EdtRewriter.cpp
///
/// Implements acquire rewriting helpers used by ForLowering.
///
/// Shared rewrite behavior:
///   1. Single-element or forced-coarse path:
///      db_acquire -> partitioning(<coarse>), offsets[0], sizes[1]
///   2. Block path:
///      db_acquire -> partitioning(<block>), worker offsets/sizes
///   3. Stencil extension (when enabled):
///      expand worker offsets/sizes with halo-aware [start-1, end+1] clamping
///   4. 2D owner hints:
///      append extra partition dimensions to partition_offsets/sizes only
///==========================================================================///

#include "arts/Transforms/Edt/EdtRewriter.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

using namespace mlir;
using namespace mlir::arts;

DbAcquireOp mlir::arts::detail::rewriteAcquireAsBlock(
    AcquireRewriteInput &in, bool applyStencilHalo) {
  if (in.singleElement || in.forceCoarse) {
    return in.AC->create<DbAcquireOp>(
        in.loc, in.parentAcquire.getMode(), in.rootGuid, in.rootPtr,
        in.parentAcquire.getPtr().getType(), PartitionMode::coarse,
        /*indices=*/SmallVector<Value>{},
        /*offsets=*/SmallVector<Value>{in.zero},
        /*sizes=*/SmallVector<Value>{in.one},
        /*partition_indices=*/SmallVector<Value>{},
        /*partition_offsets=*/SmallVector<Value>{},
        /*partition_sizes=*/SmallVector<Value>{});
  }

  Value workerSize = in.acquireSize;
  Value workerHintSize = in.acquireHintSize;
  if (!in.stepIsUnit) {
    workerSize = in.AC->create<arith::MulIOp>(in.loc, workerSize, in.step);
    workerHintSize =
        in.AC->create<arith::MulIOp>(in.loc, workerHintSize, in.step);
  }

  SmallVector<Value> workerOffsets = {in.acquireOffset};
  SmallVector<Value> workerSizes = {workerSize};
  SmallVector<Value> workerHintSizes = {workerHintSize};

  if (applyStencilHalo && in.stencilExtent && !workerOffsets.empty() &&
      !workerSizes.empty()) {
    Value start = workerOffsets.front();
    Value size = workerSizes.front();
    Value end = in.AC->create<arith::AddIOp>(in.loc, start, size);

    Value canShiftLeft =
        in.AC->create<arith::CmpIOp>(in.loc, arith::CmpIPredicate::uge, start,
                                     in.one);
    Value startMinusOne = in.AC->create<arith::SubIOp>(in.loc, start, in.one);
    Value haloStart = in.AC->create<arith::SelectOp>(in.loc, canShiftLeft,
                                                     startMinusOne, in.zero);

    Value endPlusOne = in.AC->create<arith::AddIOp>(in.loc, end, in.one);
    Value haloEnd =
        in.AC->create<arith::MinUIOp>(in.loc, endPlusOne, in.stencilExtent);
    Value haloSize = in.AC->create<arith::SubIOp>(in.loc, haloEnd, haloStart);

    workerOffsets.front() = haloStart;
    workerSizes.front() = haloSize;
    workerHintSizes.front() = haloSize;
  }

  SmallVector<Value> partitionOffsets(workerOffsets.begin(),
                                      workerOffsets.end());
  SmallVector<Value> partitionHintSizes(workerHintSizes.begin(),
                                        workerHintSizes.end());
  for (unsigned i = 0; i < in.extraOffsets.size(); ++i) {
    Value extraOffset = in.extraOffsets[i];
    Value extraHint = i < in.extraHintSizes.size() ? in.extraHintSizes[i]
                                                   : Value();
    Value extraSize = i < in.extraSizes.size() ? in.extraSizes[i] : in.one;
    partitionOffsets.push_back(extraOffset ? extraOffset : in.zero);
    partitionHintSizes.push_back(extraHint ? extraHint : extraSize);
  }

  return in.AC->create<DbAcquireOp>(
      in.loc, in.parentAcquire.getMode(), in.rootGuid, in.rootPtr,
      in.parentAcquire.getPtr().getType(), PartitionMode::block,
      /*indices=*/
      SmallVector<Value>(in.parentAcquire.getIndices().begin(),
                         in.parentAcquire.getIndices().end()),
      /*offsets=*/workerOffsets,
      /*sizes=*/workerSizes,
      /*partition_indices=*/SmallVector<Value>{},
      /*partition_offsets=*/partitionOffsets,
      /*partition_sizes=*/partitionHintSizes);
}

std::unique_ptr<EdtRewriter> EdtRewriter::create(bool stencilHalo) {
  if (stencilHalo)
    return detail::createStencilEdtRewriter();
  return detail::createBlockEdtRewriter();
}
