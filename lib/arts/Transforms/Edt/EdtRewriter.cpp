///==========================================================================///
/// File: EdtRewriter.cpp
///
/// Implements acquire rewriting helpers used by ForLowering.
///
/// Rewrite behavior:
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
#include "mlir/Dialect/Arith/IR/Arith.h"

using namespace mlir;
using namespace mlir::arts;

DbAcquireOp mlir::arts::rewriteAcquire(AcquireRewriteInput &in,
                                       bool applyStencilHalo) {
  Value zero = in.AC->createIndexConstant(0, in.loc);
  Value one = in.AC->createIndexConstant(1, in.loc);

  if (in.singleElement || in.forceCoarse) {
    return in.AC->create<DbAcquireOp>(
        in.loc, in.parentAcquire.getMode(), in.rootGuid, in.rootPtr,
        in.parentAcquire.getPtr().getType(), PartitionMode::coarse,
        /*indices=*/SmallVector<Value>{},
        /*offsets=*/SmallVector<Value>{zero},
        /*sizes=*/SmallVector<Value>{one},
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

  if (applyStencilHalo && in.stencilExtent) {
    Value start = workerOffsets.front();
    Value size = workerSizes.front();
    Value end = in.AC->create<arith::AddIOp>(in.loc, start, size);

    Value canShiftLeft = in.AC->create<arith::CmpIOp>(
        in.loc, arith::CmpIPredicate::uge, start, one);
    Value startMinusOne = in.AC->create<arith::SubIOp>(in.loc, start, one);
    Value haloStart = in.AC->create<arith::SelectOp>(in.loc, canShiftLeft,
                                                     startMinusOne, zero);

    Value endPlusOne = in.AC->create<arith::AddIOp>(in.loc, end, one);
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
    Value extraHint =
        i < in.extraHintSizes.size() ? in.extraHintSizes[i] : Value();
    Value extraSize = i < in.extraSizes.size() ? in.extraSizes[i] : one;
    partitionOffsets.push_back(extraOffset ? extraOffset : zero);
    partitionHintSizes.push_back(extraHint ? extraHint : extraSize);
  }

  /// Keep dependency slices in element space at this stage.
  /// DbPartitioning/DbPass own the final DB-space mapping once layout decisions
  /// are materialized.
  SmallVector<Value> dependencyOffsets(workerOffsets.begin(),
                                       workerOffsets.end());
  SmallVector<Value> dependencySizes(workerHintSizes.begin(),
                                     workerHintSizes.end());

  for (unsigned i = 0; i < in.extraOffsets.size(); ++i) {
    Value extraOffset = in.extraOffsets[i];
    Value extraHint =
        i < in.extraHintSizes.size() ? in.extraHintSizes[i] : Value();
    Value extraSize = i < in.extraSizes.size() ? in.extraSizes[i] : one;
    dependencyOffsets.push_back(extraOffset ? extraOffset : zero);
    dependencySizes.push_back(extraHint ? extraHint : extraSize);
  }

  return in.AC->create<DbAcquireOp>(
      in.loc, in.parentAcquire.getMode(), in.rootGuid, in.rootPtr,
      in.parentAcquire.getPtr().getType(), PartitionMode::block,
      /*indices=*/
      SmallVector<Value>(in.parentAcquire.getIndices().begin(),
                         in.parentAcquire.getIndices().end()),
      /*offsets=*/dependencyOffsets,
      /*sizes=*/dependencySizes,
      /*partition_indices=*/SmallVector<Value>{},
      /*partition_offsets=*/partitionOffsets,
      /*partition_sizes=*/partitionHintSizes);
}
