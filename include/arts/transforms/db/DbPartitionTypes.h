///==========================================================================///
/// File: DbPartitionTypes.h
///
/// Small controller-side DB partitioning types shared across the controller
/// and block-plan resolution. These are normalized snapshots, not canonical
/// graph facts and not final rewrite payloads.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBPARTITIONTYPES_H
#define ARTS_TRANSFORMS_DATABLOCK_DBPARTITIONTYPES_H

#include "arts/Dialect.h"
#include "arts/analysis/graphs/db/DbAccessPattern.h"
#include <optional>

namespace mlir {
namespace arts {

/// Pass-local per-acquire partition summary used by DbPartitioning.
///
/// Ownership split:
///   - DbGraph / DbAnalysis own canonical facts
///   - DbPartitioning builds this normalized snapshot
///   - DbBlockPlanResolver consumes it to resolve concrete block geometry
struct AcquirePartitionInfo {
  DbAcquireOp acquire;
  PartitionMode mode = PartitionMode::coarse;
  SmallVector<Value> partitionOffsets;
  SmallVector<Value> partitionSizes;
  SmallVector<unsigned> partitionDims;
  AccessPattern accessPattern = AccessPattern::Unknown;
  bool isValid = false;
  bool hasIndirectAccess = false;
  bool hasDistributionContract = false;
  bool preservesDepMode = false;
  bool needsFullRange = false;
  /// Graph-backed stencil bounds are rewrite-only facts. They let the later
  /// block fallback recover the exact halo shift even when the acquire result
  /// contract only carries EDT-local fields such as critical_path_distance.
  std::optional<StencilBounds> graphStencilBounds;
  std::optional<unsigned> graphStencilOwnerDim;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBPARTITIONTYPES_H
