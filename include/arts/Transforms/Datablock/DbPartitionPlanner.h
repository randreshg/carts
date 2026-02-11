///==========================================================================///
/// File: DbPartitionPlanner.h
///
/// Mode-specialized planning interface used by DbPartitioning.
///
/// DbPartitioning orchestrates analysis + decisions, while this planner owns:
///   1. allocation shape computation per partition mode
///   2. per-acquire rewrite payload construction per partition mode
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBPARTITIONPLANNER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBPARTITIONPLANNER_H

#include "arts/Transforms/Datablock/DbRewriter.h"
#include "mlir/IR/Builders.h"
#include <memory>

namespace mlir {
namespace arts {

struct PartitioningDecision;

/// Minimal per-acquire planning view passed from DbPartitioning into planners.
struct DbAcquirePartitionView {
  DbAcquireOp acquire;
  SmallVector<Value> partitionIndices;
  SmallVector<Value> partitionOffsets;
  SmallVector<Value> partitionSizes;
  bool isValid = false;
  bool hasIndirectAccess = false;
  bool needsFullRange = false;
};

class DbPartitionPlanner {
public:
  virtual ~DbPartitionPlanner() = default;

  static std::unique_ptr<DbPartitionPlanner> create(PartitionMode mode);

  virtual void computeAllocationShape(
      DbAllocOp allocOp, const PartitioningDecision &decision,
      ArrayRef<Value> blockSizes, ArrayRef<unsigned> partitionedDims,
      SmallVector<Value> &newOuterSizes,
      SmallVector<Value> &newInnerSizes) const = 0;

  virtual void buildRewriteAcquire(const DbAcquirePartitionView &input,
                                   DbAllocOp allocOp, const DbRewritePlan &plan,
                                   DbRewriteAcquire &output,
                                   OpBuilder &builder) const = 0;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBPARTITIONPLANNER_H
