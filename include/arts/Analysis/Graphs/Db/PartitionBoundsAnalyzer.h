///==========================================================================///
/// File: PartitionBoundsAnalyzer.h
///
/// Helper class extracted from DbAcquireNode that handles partition bound
/// computation, partition eligibility checks, and offset-dimension mapping.
///==========================================================================///

#ifndef ARTS_ANALYSIS_GRAPHS_DB_PARTITIONBOUNDSANALYZER_H
#define ARTS_ANALYSIS_GRAPHS_DB_PARTITIONBOUNDSANALYZER_H

#include "mlir/IR/Value.h"
#include <optional>

namespace mlir {
namespace arts {

class DbAcquireNode;

/// PartitionBoundsAnalyzer -- computes partition bounds, validates partition
/// eligibility, and maps partition offsets to access-chain dimensions.
class PartitionBoundsAnalyzer {
public:
  /// Compute partition bounds for the given acquire node, populating its
  /// partitionOffset, partitionSize, stencilBounds, and computedBlockInfo.
  /// Returns true on success (or when heuristic evaluation is allowed).
  static bool computePartitionBounds(DbAcquireNode *node);

  /// Check whether the acquire (and its nested children) can be partitioned.
  static bool canBePartitioned(DbAcquireNode *node);

  /// Check whether the acquire can be partitioned using the given offset.
  static bool canPartitionWithOffset(DbAcquireNode *node, Value offset);

  /// Return the access-chain dimension that depends on the partition offset.
  /// If requireLeading is true, only accept leading dynamic index.
  static std::optional<unsigned>
  getPartitionOffsetDim(DbAcquireNode *node, Value offset, bool requireLeading);

  /// Check whether this acquire has valid EDT and memory accesses for
  /// partitioning analysis.
  static bool hasValidEdtAndAccesses(DbAcquireNode *node);

  /// Check whether this acquire needs full-range access on a block alloc.
  static bool needsFullRange(DbAcquireNode *node, Value partitionOffset);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_DB_PARTITIONBOUNDSANALYZER_H
