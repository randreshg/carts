///==========================================================================///
/// File: HeuristicUtils.h
///
/// Shared heuristic building blocks used by both DistributionHeuristics (H2)
/// and PartitioningHeuristics (H1).
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_ANALYSIS_HEURISTICS_HEURISTICUTILS_H
#define ARTS_DIALECT_CORE_ANALYSIS_HEURISTICS_HEURISTICUTILS_H

#include "arts/Dialect.h"
#include <cstdint>

namespace mlir {

class Operation;

namespace arts {

class LoopNode;

///===----------------------------------------------------------------------===///
/// Loop Classification Helpers
///===----------------------------------------------------------------------===///

/// Check whether a loop should be treated as sequential based on its metadata.
/// Examines both the LoopNode (if available) and the LoopMetadataAttr on
/// the operation for inter-iteration dependencies and the Sequential
/// parallel classification.
bool isSequentialLoop(ForOp forOp, LoopNode *loopNode = nullptr);

/// Shared arithmetic helpers for bounded heuristic cost models.
int64_t ceilDivPositive(int64_t num, int64_t denom);
int64_t clampPositive(int64_t value, int64_t minValue, int64_t maxValue);
int64_t saturatingMulPositive(int64_t lhs, int64_t rhs);
int64_t floorLog2Positive(int64_t value);

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_ANALYSIS_HEURISTICS_HEURISTICUTILS_H
