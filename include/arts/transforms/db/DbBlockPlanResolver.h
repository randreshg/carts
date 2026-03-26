///==========================================================================///
/// File: DbBlockPlanResolver.h
///
/// Block-plan resolution contract for DbPartitioning.
///
/// Resolves:
///   1) block sizes that dominate allocation points
///   2) partitioned dimensions for block/stencil plans
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBBLOCKPLANRESOLVER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBBLOCKPLANRESOLVER_H

#include "arts/transforms/db/DbPartitionTypes.h"
#include "mlir/IR/Builders.h"
#include "mlir/Support/LogicalResult.h"
#include <optional>

namespace mlir {
namespace arts {

struct DbBlockPlanInput {
  DbAllocOp allocOp;
  ArrayRef<AcquirePartitionInfo> acquireInfos;
  std::optional<int64_t> staticBlockSizeHint;
  Value dynamicBlockSizeHint;
  ArrayRef<Value> ndBlockSizeHints;
  OpBuilder *builder = nullptr;
  Location loc;
  bool useNodesForFallback = false;
  bool clampStencilFallbackWorkers = false;
};

struct DbBlockPlanResult {
  SmallVector<Value> blockSizes;
  SmallVector<unsigned> partitionedDims;
};

/// Resolve block sizes and preferred partition dimensions for block-like plans.
FailureOr<DbBlockPlanResult> resolveDbBlockPlan(const DbBlockPlanInput &input);

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBBLOCKPLANRESOLVER_H
