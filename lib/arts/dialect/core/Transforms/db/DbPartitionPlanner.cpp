///==========================================================================///
/// File: DbPartitionPlanner.cpp
///
/// Mode-specialized planning implementations for DbPartitioning.
///
/// Before:
///   %db = arts.db_alloc(%n, %m) : memref<?x?xf64>
///        {partitioning = #arts.partitioning<stencil>}
///
/// After:
///   %db = arts.db_alloc(%outer_x, %outer_y, %tile_x, %tile_y)
///        : memref<?x?x?x?xf64>
///        {partitioning = #arts.partitioning<stencil>}
///==========================================================================///

#include "arts/transforms/db/DbPartitionPlanner.h"
#include "arts/analysis/heuristics/PartitioningHeuristics.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

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
  newOuterSizes.push_back(arts::createConstantIndex(builder, loc, 1));
  for (Value dim : allocOp.getElementSizes())
    newInnerSizes.push_back(dim);
  if (newInnerSizes.empty())
    newInnerSizes.push_back(arts::createConstantIndex(builder, loc, 1));
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
    newInnerSizes.push_back(arts::createConstantIndex(builder, loc, 1));
}

static void computeBlockShape(DbAllocOp allocOp, ArrayRef<Value> blockSizes,
                              ArrayRef<unsigned> partitionedDims,
                              SmallVector<Value> &newOuterSizes,
                              SmallVector<Value> &newInnerSizes) {
  OpBuilder builder(allocOp);
  Location loc = allocOp.getLoc();
  ValueRange elemSizes = allocOp.getElementSizes();

  if (blockSizes.empty()) {
    newOuterSizes.push_back(arts::createConstantIndex(builder, loc, 1));
    for (Value dim : elemSizes)
      newInnerSizes.push_back(dim);
    if (newInnerSizes.empty())
      newInnerSizes.push_back(arts::createConstantIndex(builder, loc, 1));
    return;
  }

  Type i64Type = builder.getI64Type();
  Value oneI64 = arith::ConstantIntOp::create(builder, loc, 1, 64);
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
      Value dimI64 = arith::IndexCastOp::create(builder, loc, i64Type, dimSize);
      Value bsI64 = arith::IndexCastOp::create(builder, loc, i64Type, bs);
      Value sum = arith::AddIOp::create(builder, loc, dimI64, bsI64);
      Value sumMinusOne = arith::SubIOp::create(builder, loc, sum, oneI64);
      Value numBlocksI64 =
          arith::DivUIOp::create(builder, loc, sumMinusOne, bsI64);
      Value numBlocks = arith::IndexCastOp::create(builder,
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
      Value dimI64 = arith::IndexCastOp::create(builder, loc, i64Type, dim);
      Value bsI64 = arith::IndexCastOp::create(builder, loc, i64Type, bs);
      Value sum = arith::AddIOp::create(builder, loc, dimI64, bsI64);
      Value sumMinusOne = arith::SubIOp::create(builder, loc, sum, oneI64);
      Value numBlocksI64 =
          arith::DivUIOp::create(builder, loc, sumMinusOne, bsI64);
      Value numBlocks = arith::IndexCastOp::create(builder,
          loc, builder.getIndexType(), numBlocksI64);
      newOuterSizes.push_back(numBlocks);
      newInnerSizes.push_back(bs);
    }

    for (unsigned d = nPartDims; d < elemSizes.size(); ++d)
      newInnerSizes.push_back(elemSizes[d]);
  }

  if (newOuterSizes.empty())
    newOuterSizes.push_back(arts::createConstantIndex(builder, loc, 1));
  if (newInnerSizes.empty())
    newInnerSizes.push_back(arts::createConstantIndex(builder, loc, 1));
}

///===----------------------------------------------------------------------===///
/// Acquire rewrite payload construction per mode
///===----------------------------------------------------------------------===///

static void buildCoarseRewriteAcquire(DbAllocOp allocOp,
                                      DbRewriteAcquire &output,
                                      OpBuilder &builder) {
  Location loc = allocOp.getLoc();
  Value zero = arts::createConstantIndex(builder, loc, 0);
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
  Value zero = arts::createConstantIndex(builder, loc, 0);
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
  Value zero = arts::createConstantIndex(builder, loc, 0);
  Value one = arts::createConstantIndex(builder, loc, 1);
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
      if (plan.numPartitionedDims() > 1) {
        SmallVector<unsigned> planDims;
        planDims.reserve(plan.numPartitionedDims());
        for (unsigned d = 0; d < plan.numPartitionedDims(); ++d)
          planDims.push_back(
              d < plan.partitionedDims.size() ? plan.partitionedDims[d] : d);

        output.partitionInfo.offsets.assign(plan.numPartitionedDims(), zero);
        output.partitionInfo.sizes.reserve(plan.numPartitionedDims());
        for (unsigned d = 0; d < plan.numPartitionedDims(); ++d) {
          unsigned physDim = planDims[d];
          Value fullExtent = physDim < allocOp.getElementSizes().size()
                                 ? allocOp.getElementSizes()[physDim]
                                 : one;
          output.partitionInfo.sizes.push_back(fullExtent ? fullExtent : one);
        }

        for (unsigned i = 0; i < input.partitionIndices.size(); ++i) {
          if (i >= input.partitionDims.size())
            continue;
          unsigned physDim = input.partitionDims[i];
          auto it = llvm::find(planDims, physDim);
          if (it == planDims.end())
            continue;
          unsigned slot =
              static_cast<unsigned>(std::distance(planDims.begin(), it));
          output.partitionInfo.offsets[slot] = input.partitionIndices[i];
          Value blockSz = plan.getBlockSize(slot);
          output.partitionInfo.sizes[slot] = blockSz ? blockSz : one;
        }
      } else {
        for (Value idx : input.partitionIndices) {
          output.partitionInfo.offsets.push_back(idx);
          output.partitionInfo.sizes.push_back(one);
        }
      }
    } else if (plan.numPartitionedDims() > 1) {
      SmallVector<unsigned> planDims;
      planDims.reserve(plan.numPartitionedDims());
      for (unsigned d = 0; d < plan.numPartitionedDims(); ++d)
        planDims.push_back(
            d < plan.partitionedDims.size() ? plan.partitionedDims[d] : d);

      output.partitionInfo.offsets.assign(plan.numPartitionedDims(), zero);
      output.partitionInfo.sizes.reserve(plan.numPartitionedDims());
      for (unsigned d = 0; d < plan.numPartitionedDims(); ++d) {
        unsigned physDim = planDims[d];
        Value fullExtent = physDim < allocOp.getElementSizes().size()
                               ? allocOp.getElementSizes()[physDim]
                               : one;
        output.partitionInfo.sizes.push_back(fullExtent ? fullExtent : one);
      }

      if (!input.partitionIndices.empty()) {
        for (unsigned i = 0; i < input.partitionIndices.size(); ++i) {
          if (i >= input.partitionDims.size())
            continue;
          unsigned physDim = input.partitionDims[i];
          auto it = llvm::find(planDims, physDim);
          if (it == planDims.end())
            continue;
          unsigned slot =
              static_cast<unsigned>(std::distance(planDims.begin(), it));
          output.partitionInfo.offsets[slot] = input.partitionIndices[i];
          Value blockSz = plan.getBlockSize(slot);
          output.partitionInfo.sizes[slot] = blockSz ? blockSz : one;
        }
      }

      unsigned rangeCount =
          std::min(input.partitionOffsets.size(), input.partitionSizes.size());
      for (unsigned i = 0; i < rangeCount; ++i) {
        if (i >= input.partitionDims.size())
          continue;
        unsigned physDim = input.partitionDims[i];
        auto it = llvm::find(planDims, physDim);
        if (it == planDims.end())
          continue;
        unsigned slot =
            static_cast<unsigned>(std::distance(planDims.begin(), it));
        output.partitionInfo.offsets[slot] = input.partitionOffsets[i];
        output.partitionInfo.sizes[slot] = input.partitionSizes[i];
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
  ArtsDepPattern depPattern = ArtsDepPattern::unknown;
  if (auto contract = getLoweringContract(acquire.getPtr());
      contract && contract->pattern.depPattern) {
    depPattern = *contract->pattern.depPattern;
  } else if (auto semantic = getSemanticContract(acquire.getOperation());
             semantic && semantic->pattern.depPattern) {
    depPattern = *semantic->pattern.depPattern;
  } else {
    depPattern = getEffectiveDepPattern(acquire.getOperation())
                     .value_or(ArtsDepPattern::unknown);
  }
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

  auto elementOffsets = acquire.getElementOffsets();
  auto elementSizes = acquire.getElementSizes();
  if (!elementOffsets.empty() && elementOffsets.size() == elementSizes.size() &&
      plan.numPartitionedDims() > 0) {
    output.partitionInfo.offsets.clear();
    output.partitionInfo.sizes.clear();

    for (unsigned d = 0; d < plan.numPartitionedDims(); ++d) {
      unsigned elemDim =
          d < plan.partitionedDims.size() ? plan.partitionedDims[d] : d;
      if (elemDim >= elementOffsets.size())
        break;
      output.partitionInfo.offsets.push_back(elementOffsets[elemDim]);
      output.partitionInfo.sizes.push_back(elementSizes[elemDim]);
    }
  }
}

} // namespace

void mlir::arts::computeAllocationShape(PartitionMode mode, DbAllocOp allocOp,
                                        const PartitioningDecision &decision,
                                        ArrayRef<Value> blockSizes,
                                        ArrayRef<unsigned> partitionedDims,
                                        SmallVector<Value> &newOuterSizes,
                                        SmallVector<Value> &newInnerSizes) {
  if (!requiresWorkerBoundsPlanning(mode)) {
    computeCoarseShape(allocOp, newOuterSizes, newInnerSizes);
    return;
  }
  if (usesElementLayout(mode)) {
    computeElementWiseShape(allocOp, decision, newOuterSizes, newInnerSizes);
    return;
  }
  computeBlockShape(allocOp, blockSizes, partitionedDims, newOuterSizes,
                    newInnerSizes);
}

void mlir::arts::buildRewriteAcquire(
    PartitionMode mode, const DbAcquirePartitionView &input, DbAllocOp allocOp,
    const DbRewritePlan &plan, DbRewriteAcquire &output, OpBuilder &builder) {
  if (!requiresWorkerBoundsPlanning(mode)) {
    buildCoarseRewriteAcquire(allocOp, output, builder);
    return;
  }
  if (usesElementLayout(mode)) {
    buildElementWiseRewriteAcquire(input, allocOp, output, builder);
    return;
  }
  if (supportsHaloExtension(mode)) {
    buildStencilRewriteAcquire(input, allocOp, plan, output, builder);
    return;
  }
  buildBlockRewriteAcquire(input, allocOp, plan, output, builder);
}
