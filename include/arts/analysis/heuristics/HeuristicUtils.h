///==========================================================================///
/// File: HeuristicUtils.h
///
/// Shared heuristic building blocks used by both DistributionHeuristics (H2)
/// and PartitioningHeuristics (H1).
///==========================================================================///

#ifndef ARTS_ANALYSIS_HEURISTICUTILS_H
#define ARTS_ANALYSIS_HEURISTICUTILS_H

#include "arts/Dialect.h"

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

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_HEURISTICUTILS_H
