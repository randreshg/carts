///==========================================================================///
/// File: DbPartitionDecisionArbiter.cpp
///
/// Lightweight arbitration helpers for DbPartitioning decisions.
///==========================================================================///

#include "arts/Transforms/Db/DbPartitionDecisionArbiter.h"
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

struct DistributionPatternHintSummary {
  bool hasStencil = false;
  bool hasOther = false;
  bool hasReadOnlyAcquire = false;
  bool hasStencilAccessPattern = false;
  bool hasExplicitControlDeps = false;
};

static DistributionPatternHintSummary summarizeAcquireDistributionPatterns(
    ArrayRef<AcquirePartitionInfo> acquireInfos) {
  DistributionPatternHintSummary summary;
  for (const auto &info : acquireInfos) {
    DbAcquireOp acquire = info.acquire;
    if (!acquire)
      continue;

    if (acquire.getMode() == ArtsMode::in)
      summary.hasReadOnlyAcquire = true;
    if (info.preservesDependencyMode)
      summary.hasExplicitControlDeps = true;
    if (info.accessPattern == AccessPattern::Stencil)
      summary.hasStencilAccessPattern = true;

    auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
    (void)blockArg;
    if (!edt)
      continue;

    auto pattern = getEdtDistributionPattern(edt.getOperation());
    if (!pattern)
      continue;

    if (*pattern == EdtDistributionPattern::stencil)
      summary.hasStencil = true;
    else
      summary.hasOther = true;

    if (summary.hasStencil && summary.hasOther)
      break;
  }
  return summary;
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
  const bool isDistributedExecution = ctx.machine &&
                                      ctx.machine->hasValidNodeCount() &&
                                      ctx.machine->isDistributed();

  /// Stencil distribution fallback path:
  /// Some stencil kernels distribute work across a non-leading dimension, so
  /// acquire-side stencil inference degrades to uniform/full-range and the
  /// coarse fallback fires even though block partitioning is still safe. This
  /// is a multinode-only recovery path; on single-node stencil kernels such as
  /// seidel, forcing block partitioning can over-partition a sequential
  /// dependence chain that should remain coarse.
  if (isDistributedExecution && ctx.canBlock &&
      result.decision.mode == PartitionMode::coarse && ctx.allBlockFullRange) {
    DistributionPatternHintSummary distPatterns =
        summarizeAcquireDistributionPatterns(acquireInfos);
    bool allocHasStencilPattern = false;
    if (auto allocPattern = getDbAccessPattern(allocOp.getOperation()))
      allocHasStencilPattern = *allocPattern == DbAccessPattern::stencil;

    if (distPatterns.hasStencil && !distPatterns.hasOther &&
        distPatterns.hasReadOnlyAcquire &&
        !distPatterns.hasExplicitControlDeps &&
        (distPatterns.hasStencilAccessPattern || allocHasStencilPattern)) {
      result.decision = PartitioningDecision::block(
          ctx, "stencil EDT distribution fallback to block partitioning");
      heuristics.recordDecision(
          "Partition-StencilDistributionFallback", true,
          "preserving block partitioning from stencil EDT distribution hints",
          allocOp.getOperation(), {});
      ARTS_DEBUG("  stencil EDT hints + full-range fallback -> forcing block "
                 "partitioning");
    }
  }

  /// Stencil hint path:
  /// If all distribution-pattern hints for this allocation are stencil EDTs,
  /// preserve ESD-capable stencil mode even when access-pattern inference
  /// degrades to uniform.
  if (ctx.canBlock && result.decision.mode == PartitionMode::block) {
    DistributionPatternHintSummary distPatterns =
        summarizeAcquireDistributionPatterns(acquireInfos);
    bool allocHasStencilPattern = false;
    if (auto allocPattern = getDbAccessPattern(allocOp.getOperation()))
      allocHasStencilPattern = *allocPattern == DbAccessPattern::stencil;

    if (distPatterns.hasStencil && distPatterns.hasReadOnlyAcquire &&
        !distPatterns.hasExplicitControlDeps &&
        (distPatterns.hasStencilAccessPattern || allocHasStencilPattern)) {
      result.decision = PartitioningDecision::stencil(
          ctx, "stencil EDT distribution pattern hint");
      heuristics.recordDecision(
          "Partition-StencilDistributionHint", true,
          "forcing stencil partition mode from EDT distribution pattern hints",
          allocOp.getOperation(), {});
      ARTS_DEBUG("  stencil EDT hints -> forcing stencil partitioning");
    } else if (distPatterns.hasExplicitControlDeps && distPatterns.hasStencil) {
      ARTS_DEBUG("  explicit control deps present -> skipping stencil hint "
                 "override");
    }
  }

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
