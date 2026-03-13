///==========================================================================///
/// File: DbPartitionPlanner.cpp
///
/// Mode-specialized planning implementations for DbPartitioning.
///==========================================================================///

#include "arts/transforms/db/DbPartitionPlanner.h"
#include "arts/analysis/heuristics/PartitioningHeuristics.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

static Value makeIndexConst(OpBuilder &builder, Location loc, int64_t v) {
  return arts::createConstantIndex(builder, loc, v);
}

static Value firstTotalSize(DbAllocOp allocOp, Value zero) {
  if (allocOp.getElementSizes().empty())
    return zero;
  return allocOp.getElementSizes().front();
}

///===----------------------------------------------------------------------===///
/// Allocation shape computation per mode
///===----------------------------------------------------------------------===///

static void computeCoarseShape(DbAllocOp allocOp,
                               SmallVector<Value> &newOuterSizes,
                               SmallVector<Value> &newInnerSizes) {
  OpBuilder builder(allocOp);
  Location loc = allocOp.getLoc();
  newOuterSizes.push_back(makeIndexConst(builder, loc, 1));
  for (Value dim : allocOp.getElementSizes())
    newInnerSizes.push_back(dim);
  if (newInnerSizes.empty())
    newInnerSizes.push_back(makeIndexConst(builder, loc, 1));
}

static void computeElementWiseShape(DbAllocOp allocOp,
                                    const PartitioningDecision &decision,
                                    SmallVector<Value> &newOuterSizes,
                                    SmallVector<Value> &newInnerSizes) {
  OpBuilder builder(allocOp);
  Location loc = allocOp.getLoc();
  ValueRange elemSizes = allocOp.getElementSizes();

  unsigned outerRank = decision.outerRank;
  for (unsigned i = 0; i < outerRank && i < elemSizes.size(); ++i)
    newOuterSizes.push_back(elemSizes[i]);
  for (unsigned i = outerRank; i < elemSizes.size(); ++i)
    newInnerSizes.push_back(elemSizes[i]);

  if (newInnerSizes.empty())
    newInnerSizes.push_back(makeIndexConst(builder, loc, 1));
}

static void computeBlockShape(DbAllocOp allocOp, ArrayRef<Value> blockSizes,
                              ArrayRef<unsigned> partitionedDims,
                              SmallVector<Value> &newOuterSizes,
                              SmallVector<Value> &newInnerSizes) {
  OpBuilder builder(allocOp);
  Location loc = allocOp.getLoc();
  ValueRange elemSizes = allocOp.getElementSizes();

  if (blockSizes.empty()) {
    newOuterSizes.push_back(makeIndexConst(builder, loc, 1));
    for (Value dim : elemSizes)
      newInnerSizes.push_back(dim);
    if (newInnerSizes.empty())
      newInnerSizes.push_back(makeIndexConst(builder, loc, 1));
    return;
  }

  Type i64Type = builder.getI64Type();
  Value oneI64 = builder.create<arith::ConstantIntOp>(loc, 1, 64);
  unsigned nPartDims = blockSizes.size();

  if (!partitionedDims.empty()) {
    SmallVector<int64_t> dimToPart(elemSizes.size(), -1);
    for (unsigned p = 0; p < partitionedDims.size(); ++p) {
      if (partitionedDims[p] < elemSizes.size())
        dimToPart[partitionedDims[p]] = static_cast<int64_t>(p);
    }

    for (unsigned p = 0; p < nPartDims; ++p) {
      unsigned dim = p < partitionedDims.size() ? partitionedDims[p] : p;
      if (dim >= elemSizes.size())
        continue;
      Value bs = blockSizes[p];
      if (!bs ||
          ValueAnalysis::isZeroConstant(ValueAnalysis::stripNumericCasts(bs)))
        continue;

      Value dimSize = elemSizes[dim];
      Value dimI64 = builder.create<arith::IndexCastOp>(loc, i64Type, dimSize);
      Value bsI64 = builder.create<arith::IndexCastOp>(loc, i64Type, bs);
      Value sum = builder.create<arith::AddIOp>(loc, dimI64, bsI64);
      Value sumMinusOne = builder.create<arith::SubIOp>(loc, sum, oneI64);
      Value numBlocksI64 =
          builder.create<arith::DivUIOp>(loc, sumMinusOne, bsI64);
      Value numBlocks = builder.create<arith::IndexCastOp>(
          loc, builder.getIndexType(), numBlocksI64);
      newOuterSizes.push_back(numBlocks);
    }

    for (unsigned d = 0; d < elemSizes.size(); ++d) {
      int64_t partIdx = dimToPart[d];
      if (partIdx >= 0 && static_cast<unsigned>(partIdx) < blockSizes.size())
        newInnerSizes.push_back(blockSizes[partIdx]);
      else
        newInnerSizes.push_back(elemSizes[d]);
    }
  } else {
    for (unsigned d = 0; d < nPartDims && d < elemSizes.size(); ++d) {
      Value bs = blockSizes[d];
      if (!bs ||
          ValueAnalysis::isZeroConstant(ValueAnalysis::stripNumericCasts(bs)))
        continue;

      Value dim = elemSizes[d];
      Value dimI64 = builder.create<arith::IndexCastOp>(loc, i64Type, dim);
      Value bsI64 = builder.create<arith::IndexCastOp>(loc, i64Type, bs);
      Value sum = builder.create<arith::AddIOp>(loc, dimI64, bsI64);
      Value sumMinusOne = builder.create<arith::SubIOp>(loc, sum, oneI64);
      Value numBlocksI64 =
          builder.create<arith::DivUIOp>(loc, sumMinusOne, bsI64);
      Value numBlocks = builder.create<arith::IndexCastOp>(
          loc, builder.getIndexType(), numBlocksI64);
      newOuterSizes.push_back(numBlocks);
      newInnerSizes.push_back(bs);
    }

    for (unsigned d = nPartDims; d < elemSizes.size(); ++d)
      newInnerSizes.push_back(elemSizes[d]);
  }

  if (newOuterSizes.empty())
    newOuterSizes.push_back(makeIndexConst(builder, loc, 1));
  if (newInnerSizes.empty())
    newInnerSizes.push_back(makeIndexConst(builder, loc, 1));
}

///===----------------------------------------------------------------------===///
/// Acquire rewrite payload construction per mode
///===----------------------------------------------------------------------===///

static void buildCoarseRewriteAcquire(DbAllocOp allocOp,
                                      DbRewriteAcquire &output,
                                      OpBuilder &builder) {
  Location loc = allocOp.getLoc();
  Value zero = makeIndexConst(builder, loc, 0);
  Value totalSize = firstTotalSize(allocOp, zero);

  output.partitionInfo.offsets.clear();
  output.partitionInfo.sizes.clear();
  output.partitionInfo.mode = PartitionMode::coarse;
  output.partitionInfo.offsets.push_back(zero);
  output.partitionInfo.sizes.push_back(totalSize);
}

static void buildElementWiseRewriteAcquire(const DbAcquirePartitionView &input,
                                           DbAllocOp allocOp,
                                           DbRewriteAcquire &output,
                                           OpBuilder &builder) {
  Location loc = allocOp.getLoc();
  Value zero = makeIndexConst(builder, loc, 0);
  Value totalSize = firstTotalSize(allocOp, zero);

  output.partitionInfo.indices.clear();
  output.partitionInfo.offsets.clear();
  output.partitionInfo.sizes.clear();
  output.partitionInfo.mode = PartitionMode::fine_grained;

  if (!input.isValid) {
    output.partitionInfo.indices.push_back(zero);
    output.partitionInfo.sizes.push_back(totalSize);
    return;
  }

  output.partitionInfo.indices.assign(input.partitionIndices.begin(),
                                      input.partitionIndices.end());

  bool hasRange =
      !input.partitionSizes.empty() &&
      !ValueAnalysis::isOneConstant(
          ValueAnalysis::stripNumericCasts(input.partitionSizes.front()));

  if (output.partitionInfo.indices.empty() && hasRange) {
    output.partitionInfo.offsets.assign(input.partitionOffsets.begin(),
                                        input.partitionOffsets.end());
    output.partitionInfo.sizes.assign(input.partitionSizes.begin(),
                                      input.partitionSizes.end());
  }

  if (output.partitionInfo.indices.empty() &&
      output.partitionInfo.offsets.empty() && !input.partitionOffsets.empty()) {
    output.partitionInfo.indices.assign(input.partitionOffsets.begin(),
                                        input.partitionOffsets.end());
  }

  /// Fine-grained + indirect access fallback: coarse-like single range.
  if (input.hasIndirectAccess) {
    output.partitionInfo.indices.clear();
    output.partitionInfo.offsets.clear();
    output.partitionInfo.sizes.clear();
    output.partitionInfo.indices.push_back(zero);
    output.partitionInfo.sizes.push_back(totalSize);
  }
}

static void buildBlockRewriteAcquire(const DbAcquirePartitionView &input,
                                     DbAllocOp allocOp,
                                     const DbRewritePlan &plan,
                                     DbRewriteAcquire &output,
                                     OpBuilder &builder) {
  Location loc = allocOp.getLoc();
  Value zero = makeIndexConst(builder, loc, 0);
  Value one = makeIndexConst(builder, loc, 1);
  Value totalSize = firstTotalSize(allocOp, zero);

  output.partitionInfo.indices.clear();
  output.partitionInfo.offsets.clear();
  output.partitionInfo.sizes.clear();
  output.partitionInfo.mode = PartitionMode::block;

  if (input.needsFullRange) {
    output.isFullRange = true;
    if (!input.partitionOffsets.empty() && !input.partitionSizes.empty()) {
      output.partitionInfo.offsets.assign(input.partitionOffsets.begin(),
                                          input.partitionOffsets.end());
      output.partitionInfo.sizes.assign(input.partitionSizes.begin(),
                                        input.partitionSizes.end());
    }
    if (!plan.partitionedDims.empty()) {
      output.partitionInfo.partitionedDims.assign(plan.partitionedDims.begin(),
                                                  plan.partitionedDims.end());
    }
    return;
  }

  if (input.isValid) {
    if (input.partitionOffsets.empty() && input.partitionSizes.empty() &&
        !input.partitionIndices.empty()) {
      for (Value idx : input.partitionIndices) {
        output.partitionInfo.offsets.push_back(idx);
        output.partitionInfo.sizes.push_back(one);
      }
    } else if (plan.numPartitionedDims() > 1) {
      for (Value idx : input.partitionIndices) {
        output.partitionInfo.offsets.push_back(idx);
        unsigned dimIdx = output.partitionInfo.offsets.size() - 1;
        Value blockSz = plan.getBlockSize(dimIdx);
        output.partitionInfo.sizes.push_back(blockSz ? blockSz : one);
      }
      if (output.partitionInfo.offsets.empty()) {
        output.partitionInfo.offsets.assign(input.partitionOffsets.begin(),
                                            input.partitionOffsets.end());
        output.partitionInfo.sizes.assign(input.partitionSizes.begin(),
                                          input.partitionSizes.end());
      }
    } else {
      output.partitionInfo.offsets.assign(input.partitionOffsets.begin(),
                                          input.partitionOffsets.end());
      output.partitionInfo.sizes.assign(input.partitionSizes.begin(),
                                        input.partitionSizes.end());
    }
  } else {
    output.partitionInfo.offsets.push_back(zero);
    output.partitionInfo.sizes.push_back(totalSize);
  }

  if (!plan.partitionedDims.empty()) {
    output.partitionInfo.partitionedDims.assign(plan.partitionedDims.begin(),
                                                plan.partitionedDims.end());
  }
}

static void buildStencilRewriteAcquire(const DbAcquirePartitionView &input,
                                       DbAllocOp allocOp,
                                       const DbRewritePlan &plan,
                                       DbRewriteAcquire &output,
                                       OpBuilder &builder) {
  DbAcquireOp acquire = input.acquire;
  bool allocIsStencil = false;
  if (auto pattern = getDbAccessPattern(allocOp.getOperation()))
    allocIsStencil = *pattern == DbAccessPattern::stencil;
  ArtsDepPattern depPattern = getEffectiveDepPattern(acquire.getOperation())
                                  .value_or(ArtsDepPattern::unknown);
  bool shouldUseStencil = acquire && acquire.getMode() == ArtsMode::in &&
                          isStencilHaloDepPattern(depPattern);
  if (!shouldUseStencil) {
    shouldUseStencil =
        acquire && acquire.getMode() == ArtsMode::in &&
        (input.accessPattern == AccessPattern::Stencil || allocIsStencil);
  }

  buildBlockRewriteAcquire(input, allocOp, plan, output, builder);
  if (!shouldUseStencil || !input.isValid || input.needsFullRange) {
    output.partitionInfo.mode = PartitionMode::block;
    return;
  }

  output.partitionInfo.mode = PartitionMode::stencil;

  if (!input.partitionIndices.empty()) {
    output.partitionInfo.indices.assign(input.partitionIndices.begin(),
                                        input.partitionIndices.end());
  }
}

} // namespace

void mlir::arts::computeAllocationShape(PartitionMode mode, DbAllocOp allocOp,
                                        const PartitioningDecision &decision,
                                        ArrayRef<Value> blockSizes,
                                        ArrayRef<unsigned> partitionedDims,
                                        SmallVector<Value> &newOuterSizes,
                                        SmallVector<Value> &newInnerSizes) {
  switch (mode) {
  case PartitionMode::coarse:
    computeCoarseShape(allocOp, newOuterSizes, newInnerSizes);
    return;
  case PartitionMode::fine_grained:
    computeElementWiseShape(allocOp, decision, newOuterSizes, newInnerSizes);
    return;
  case PartitionMode::block:
  case PartitionMode::stencil:
    computeBlockShape(allocOp, blockSizes, partitionedDims, newOuterSizes,
                      newInnerSizes);
    return;
  }
}

void mlir::arts::buildRewriteAcquire(
    PartitionMode mode, const DbAcquirePartitionView &input, DbAllocOp allocOp,
    const DbRewritePlan &plan, DbRewriteAcquire &output, OpBuilder &builder) {
  switch (mode) {
  case PartitionMode::coarse:
    buildCoarseRewriteAcquire(allocOp, output, builder);
    return;
  case PartitionMode::fine_grained:
    buildElementWiseRewriteAcquire(input, allocOp, output, builder);
    return;
  case PartitionMode::block:
    buildBlockRewriteAcquire(input, allocOp, plan, output, builder);
    return;
  case PartitionMode::stencil:
    buildStencilRewriteAcquire(input, allocOp, plan, output, builder);
    return;
  }
}
