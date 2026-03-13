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

#include "arts/transforms/edt/EdtRewriter.h"
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

using namespace mlir;
using namespace mlir::arts;

DbAcquireOp mlir::arts::rewriteAcquire(AcquireRewriteInput &in,
                                       bool applyStencilHalo) {
  Value zero = in.AC->createIndexConstant(0, in.loc);
  Value one = in.AC->createIndexConstant(1, in.loc);

  if (in.singleElement || in.forceCoarse) {
    /// Intentionally drop any worker-local block hints here. For single-node
    /// Seidel-like inout updates with cross-element self-reads, keeping those
    /// hints lets DbPartitioning recover an unsafe block/stencil layout later.
    auto coarseAcquire = in.AC->create<DbAcquireOp>(
        in.loc, in.parentAcquire.getMode(), in.rootGuid, in.rootPtr,
        in.parentAcquire.getPtr().getType(), PartitionMode::coarse,
        /*indices=*/SmallVector<Value>{},
        /*offsets=*/SmallVector<Value>{zero},
        /*sizes=*/SmallVector<Value>{one},
        /*partition_indices=*/SmallVector<Value>{},
        /*partition_offsets=*/SmallVector<Value>{},
        /*partition_sizes=*/SmallVector<Value>{});
    copySemanticContractAttrs(in.parentAcquire.getOperation(),
                              coarseAcquire.getOperation());
    copyLoweringContract(in.parentAcquire.getPtr(), coarseAcquire.getPtr(),
                         in.AC->getBuilder(), in.loc);
    if (in.parentAcquire.getPreserveAccessMode())
      coarseAcquire.setPreserveAccessMode();
    if (in.parentAcquire.getPreserveDepEdge())
      coarseAcquire.setPreserveDepEdge();
    return coarseAcquire;
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
  for (unsigned i = 0; i < in.extraOffsets.size(); ++i) {
    Value extraOffset = in.extraOffsets[i];
    Value extraSize = i < in.extraSizes.size() ? in.extraSizes[i] : one;
    Value extraHint =
        i < in.extraHintSizes.size() ? in.extraHintSizes[i] : extraSize;
    workerOffsets.push_back(extraOffset ? extraOffset : zero);
    workerSizes.push_back(extraSize ? extraSize : one);
    workerHintSizes.push_back(extraHint ? extraHint : extraSize);
  }

  auto applyHaloToDim = [&](unsigned dim, int64_t minOffset, int64_t maxOffset,
                            Value extent) {
    if (dim >= workerOffsets.size() || !extent)
      return;

    Value start = workerOffsets[dim];
    Value size = workerSizes[dim];
    Value end = in.AC->create<arith::AddIOp>(in.loc, start, size);

    Value haloStart = start;
    if (minOffset < 0) {
      Value leftHalo = in.AC->createIndexConstant(-minOffset, in.loc);
      Value canShiftLeft = in.AC->create<arith::CmpIOp>(
          in.loc, arith::CmpIPredicate::uge, start, leftHalo);
      Value shiftedStart =
          in.AC->create<arith::SubIOp>(in.loc, start, leftHalo);
      haloStart = in.AC->create<arith::SelectOp>(in.loc, canShiftLeft,
                                                 shiftedStart, zero);
    } else if (minOffset > 0) {
      Value shift = in.AC->createIndexConstant(minOffset, in.loc);
      haloStart = in.AC->create<arith::AddIOp>(in.loc, start, shift);
    }

    Value haloEnd = end;
    if (maxOffset > 0) {
      Value rightHalo = in.AC->createIndexConstant(maxOffset, in.loc);
      Value endPlusHalo = in.AC->create<arith::AddIOp>(in.loc, end, rightHalo);
      haloEnd = in.AC->create<arith::MinUIOp>(in.loc, endPlusHalo, extent);
    } else if (maxOffset < 0) {
      Value shrink = in.AC->createIndexConstant(-maxOffset, in.loc);
      Value canShrink = in.AC->create<arith::CmpIOp>(
          in.loc, arith::CmpIPredicate::uge, end, shrink);
      Value shrunkenEnd = in.AC->create<arith::SubIOp>(in.loc, end, shrink);
      haloEnd =
          in.AC->create<arith::SelectOp>(in.loc, canShrink, shrunkenEnd, zero);
    }

    Value haloEndAboveStart = in.AC->create<arith::CmpIOp>(
        in.loc, arith::CmpIPredicate::uge, haloEnd, haloStart);
    Value rawHaloSize =
        in.AC->create<arith::SubIOp>(in.loc, haloEnd, haloStart);
    Value haloSize = in.AC->create<arith::SelectOp>(in.loc, haloEndAboveStart,
                                                    rawHaloSize, zero);
    workerOffsets[dim] = haloStart;
    workerSizes[dim] = haloSize;
    workerHintSizes[dim] = haloSize;
  };

  if (applyStencilHalo) {
    if (!in.dimensionExtents.empty() && !in.haloMinOffsets.empty() &&
        !in.haloMaxOffsets.empty()) {
      unsigned haloRank = std::min<unsigned>(
          workerOffsets.size(),
          std::min<unsigned>(in.dimensionExtents.size(),
                             std::min<unsigned>(in.haloMinOffsets.size(),
                                                in.haloMaxOffsets.size())));
      for (unsigned dim = 0; dim < haloRank; ++dim)
        applyHaloToDim(dim, in.haloMinOffsets[dim], in.haloMaxOffsets[dim],
                       in.dimensionExtents[dim]);
    } else if (in.stencilExtent) {
      applyHaloToDim(/*dim=*/0, /*minOffset=*/-1, /*maxOffset=*/1,
                     in.stencilExtent);
    }
  }

  SmallVector<Value> partitionOffsets(workerOffsets.begin(),
                                      workerOffsets.end());
  SmallVector<Value> partitionHintSizes(workerHintSizes.begin(),
                                        workerHintSizes.end());

  /// Keep dependency slices in element space at this stage, but only along the
  /// already-materialized DB-space rank. Extra N-D ownership stays in
  /// partition_* hints until DbPartitioning rewrites the allocation shape.
  SmallVector<Value> dependencyOffsets = {workerOffsets.front()};
  SmallVector<Value> dependencySizes = {workerHintSizes.front()};

  /// Generic read-only acquires may still preserve the parent dependency
  /// range. Pattern-backed block/stencil contracts must keep the worker-local
  /// dependency slice authoritative so later lowering does not widen them back
  /// to full-range.
  bool useParentDependencyRange = in.preserveParentDependencyRange &&
                                  !applyStencilHalo &&
                                  !in.parentAcquire.getOffsets().empty() &&
                                  !in.parentAcquire.getSizes().empty();
  if (useParentDependencyRange) {
    dependencyOffsets.assign(in.parentAcquire.getOffsets().begin(),
                             in.parentAcquire.getOffsets().end());
    dependencySizes.assign(in.parentAcquire.getSizes().begin(),
                           in.parentAcquire.getSizes().end());
  }

  auto blockAcquire = in.AC->create<DbAcquireOp>(
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
  copySemanticContractAttrs(in.parentAcquire.getOperation(),
                            blockAcquire.getOperation());
  copyLoweringContract(in.parentAcquire.getPtr(), blockAcquire.getPtr(),
                       in.AC->getBuilder(), in.loc);
  if (in.parentAcquire.getPreserveAccessMode())
    blockAcquire.setPreserveAccessMode();
  if (in.parentAcquire.getPreserveDepEdge())
    blockAcquire.setPreserveDepEdge();
  return blockAcquire;
}
