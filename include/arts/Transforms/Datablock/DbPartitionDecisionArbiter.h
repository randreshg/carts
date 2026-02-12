///==========================================================================///
/// File: DbPartitionDecisionArbiter.h
///
/// Lightweight arbitration helpers for DbPartitioning decisions.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBPARTITIONDECISIONARBITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBPARTITIONDECISIONARBITER_H

#include "arts/Analysis/Graphs/Db/DbAccessPattern.h"
#include "arts/Analysis/PartitioningHeuristics.h"
#include "arts/ArtsDialect.h"
#include "mlir/Support/LLVM.h"

namespace mlir {
namespace arts {

class HeuristicsConfig;

/// Per-acquire partition analysis results for DbPartitioning.
struct AcquirePartitionInfo {
  DbAcquireOp acquire;
  PartitionMode mode = PartitionMode::coarse;
  /// N-D partition support: vectors for all dimensions
  SmallVector<Value> partitionOffsets;
  SmallVector<Value> partitionSizes;
  SmallVector<unsigned> partitionDims;
  AccessPattern accessPattern = AccessPattern::Unknown;
  bool isValid = false;
  bool hasIndirectAccess = false;
  /// For mixed mode: this coarse acquire needs full-range access to chunked
  /// allocation.
  bool needsFullRange = false;

  /// Returns true if both acquires can use a consistent partition strategy.
  bool isConsistentWith(const AcquirePartitionInfo &other) const;
};

/// Result of reconciling per-acquire partition modes.
struct AcquireModeReconcileResult {
  PartitionMode consistentMode = PartitionMode::coarse;
  bool modesConsistent = true;
  bool allBlockFullRange = false;
};

/// Reconcile per-acquire modes into allocation-level capabilities.
/// Mutates ctx and acquireInfos for mixed-mode/full-range handling.
AcquireModeReconcileResult
reconcileAcquireModes(PartitioningContext &ctx,
                      SmallVectorImpl<AcquirePartitionInfo> &acquireInfos,
                      bool canUseBlock);

/// Initial arbitration output before later fallback refinements.
struct PartitionDecisionArbiterResult {
  PartitioningDecision decision;
  SmallVector<Value> blockSizesForNDBlock;
};

/// Choose initial partition decision from heuristics and transfer hints.
PartitionDecisionArbiterResult arbitrateInitialPartitionDecision(
    const PartitioningContext &ctx, ArrayRef<AcquirePartitionInfo> acquireInfos,
    DbAllocOp allocOp, HeuristicsConfig &heuristics);

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBPARTITIONDECISIONARBITER_H
