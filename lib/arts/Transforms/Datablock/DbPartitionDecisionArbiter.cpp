///==========================================================================///
/// File: DbPartitionDecisionArbiter.cpp
///
/// Lightweight arbitration helpers for DbPartitioning decisions.
///==========================================================================///

#include "arts/Transforms/Datablock/DbPartitionDecisionArbiter.h"
#include "arts/Analysis/ArtsHeuristics.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_partition_decision_arbiter);

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool isBlockLikeMode(PartitionMode mode) {
  return mode == PartitionMode::block || mode == PartitionMode::stencil;
}

static bool isTiling2DTaskAcquire(DbAcquireOp acquire) {
  if (!acquire)
    return false;
  auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
  (void)blockArg;
  if (!edt)
    return false;
  auto kind = getEdtDistributionKind(edt.getOperation());
  return kind && *kind == EdtDistributionKind::tiling_2d;
}

} // namespace

bool AcquirePartitionInfo::isConsistentWith(
    const AcquirePartitionInfo &other) const {
  if (mode != other.mode)
    return false;

  /// For block mode, first-dim partition sizes should agree.
  if (mode == PartitionMode::block && !partitionSizes.empty() &&
      !other.partitionSizes.empty()) {
    Value thisSize = partitionSizes.front();
    Value otherSize = other.partitionSizes.front();
    if (thisSize != otherSize) {
      int64_t lhs = 0, rhs = 0;
      bool lhsConst = ValueUtils::getConstantIndex(thisSize, lhs);
      bool rhsConst = ValueUtils::getConstantIndex(otherSize, rhs);
      if (!(lhsConst && rhsConst && lhs == rhs))
        return false;
    }
  }

  return true;
}

AcquireModeReconcileResult mlir::arts::reconcileAcquireModes(
    PartitioningContext &ctx,
    SmallVectorImpl<AcquirePartitionInfo> &acquireInfos, bool canUseBlock) {
  AcquireModeReconcileResult result;
  if (acquireInfos.empty())
    return result;

  result.consistentMode = acquireInfos.front().mode;
  for (size_t i = 1; i < acquireInfos.size(); ++i) {
    if (!acquireInfos.front().isConsistentWith(acquireInfos[i])) {
      result.modesConsistent = false;
      break;
    }
  }

  bool anyBlockCapable = false;
  bool anyBlockNotFullRange = false;
  size_t n = std::min(ctx.acquires.size(), acquireInfos.size());
  for (size_t i = 0; i < n; ++i) {
    if (!ctx.acquires[i].canBlock)
      continue;
    anyBlockCapable = true;
    if (!acquireInfos[i].needsFullRange)
      anyBlockNotFullRange = true;
  }

  result.allBlockFullRange = anyBlockCapable && !anyBlockNotFullRange;

  ctx.canElementWise = ctx.anyCanElementWise();
  ctx.canBlock = canUseBlock && ctx.anyCanBlock();
  ctx.allBlockFullRange = result.allBlockFullRange;

  if (!ctx.canBlock)
    return result;

  bool hasCoarse =
      llvm::any_of(acquireInfos, [](const AcquirePartitionInfo &info) {
        return info.mode == PartitionMode::coarse;
      });
  bool hasBlock =
      llvm::any_of(acquireInfos, [](const AcquirePartitionInfo &info) {
        return isBlockLikeMode(info.mode);
      });

  if (hasCoarse && hasBlock) {
    /// Mixed mode: chunked allocation + full-range coarse acquires.
    ctx.canElementWise = false;
    for (auto &info : acquireInfos) {
      if (info.mode == PartitionMode::coarse)
        info.needsFullRange = true;
    }
  } else if (hasCoarse) {
    /// Coarse+chunked mixes should not use element-wise allocation.
    ctx.canElementWise = false;
  }

  return result;
}

PartitionDecisionArbiterResult mlir::arts::arbitrateInitialPartitionDecision(
    const PartitioningContext &ctx, ArrayRef<AcquirePartitionInfo> acquireInfos,
    DbAllocOp allocOp, HeuristicsConfig &heuristics) {
  PartitionDecisionArbiterResult result;
  result.decision = heuristics.getPartitioningMode(ctx);

  /// tiling_2d owner hint path:
  /// when task-level distribution selected 2D and an inout acquire carries
  /// 2D partition sizes, force N-D block planning to preserve 2D ownership.
  for (const auto &info : acquireInfos) {
    DbAcquireOp acquire = info.acquire;
    if (!acquire || acquire.getMode() != ArtsMode::inout)
      continue;
    if (!isTiling2DTaskAcquire(acquire))
      continue;
    if (info.partitionSizes.size() < 2)
      continue;

    Value rowSize = info.partitionSizes[0];
    Value colSize = info.partitionSizes[1];
    if (!rowSize || !colSize)
      continue;
    if (ValueUtils::isZeroConstant(ValueUtils::stripNumericCasts(rowSize)) ||
        ValueUtils::isZeroConstant(ValueUtils::stripNumericCasts(colSize)))
      continue;

    result.blockSizesForNDBlock.push_back(rowSize);
    result.blockSizesForNDBlock.push_back(colSize);
    result.decision = PartitioningDecision::blockND(
        ctx, /*nDims=*/2, "tiling_2d owner-based N-D block partitioning");
    heuristics.recordDecision(
        "Partition-Tiling2DOwnerBlockND", true,
        "using tiling_2d owner hints for N-D block partitioning",
        allocOp.getOperation(), {{"numPartitionedDims", 2}});
    ARTS_DEBUG("  tiling_2d owner hints -> forcing N-D block partitioning");
    break;
  }

  return result;
}
