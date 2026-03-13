///==========================================================================///
/// File: DbPartitionPlanner.h
///
/// Mode-specialized planning functions for DbPartitioning.
///
/// Planner responsibilities:
///   1. take a controller decision plus normalized acquire views
///   2. compute allocation layout shape
///   3. build per-acquire rewrite payloads
///
/// The planner does not rerun heuristics and does not inspect raw DB graphs.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBPARTITIONPLANNER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBPARTITIONPLANNER_H

#include "arts/analysis/graphs/db/DbAccessPattern.h"
#include "arts/transforms/db/DbRewriter.h"
#include "mlir/IR/Builders.h"

namespace mlir {
namespace arts {

struct PartitioningDecision;

/// Per-acquire planning input passed from DbPartitioning into planners.
///
/// This is the handoff boundary between controller logic and rewrite planning.
struct DbAcquirePartitionView {
  DbAcquireOp acquire;
  SmallVector<Value> partitionIndices;
  SmallVector<Value> partitionOffsets;
  SmallVector<Value> partitionSizes;
  AccessPattern accessPattern = AccessPattern::Unknown;
  bool isValid = false;
  bool hasIndirectAccess = false;
  bool needsFullRange = false;
};

/// Compute allocation outer/inner sizes for the given partition mode.
void computeAllocationShape(PartitionMode mode, DbAllocOp allocOp,
                            const PartitioningDecision &decision,
                            ArrayRef<Value> blockSizes,
                            ArrayRef<unsigned> partitionedDims,
                            SmallVector<Value> &newOuterSizes,
                            SmallVector<Value> &newInnerSizes);

/// Build per-acquire rewrite payload for the given partition mode.
void buildRewriteAcquire(PartitionMode mode,
                         const DbAcquirePartitionView &input, DbAllocOp allocOp,
                         const DbRewritePlan &plan, DbRewriteAcquire &output,
                         OpBuilder &builder);

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBPARTITIONPLANNER_H
