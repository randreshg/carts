///==========================================================================///
/// File: TwinDiffHeuristics.h
///
/// H4.x twin-diff heuristics for the CARTS compiler.
///
/// These heuristics determine when twin-diff can be safely disabled based on
/// provable non-overlap of datablock accesses.
///==========================================================================///

#ifndef ARTS_ANALYSIS_HEURISTICS_TWINDIFFHEURISTICS_H
#define ARTS_ANALYSIS_HEURISTICS_TWINDIFFHEURISTICS_H

#include "arts/Analysis/Heuristics/HeuristicBase.h"

namespace mlir {
class Operation;
} // namespace mlir

namespace mlir::arts {

//===----------------------------------------------------------------------===//
// H4: Twin-Diff Context and Proof Structures
//===----------------------------------------------------------------------===//

/// Proof method for why twin-diff can be disabled.
///
/// Twin-diff is a runtime mechanism that safely merges partial updates when
/// multiple workers write to overlapping regions. Disabling it is safe only
/// when we can PROVE non-overlap at compile time.
enum class TwinDiffProof {
  None,
  IndexedControl,
  PartitionSuccess,
  AliasAnalysis
};

/// Context for twin-diff decisions (H4 heuristic).
///
/// Encapsulates all information needed to decide whether twin-diff
/// can be safely disabled for a particular acquire operation.
struct TwinDiffContext {
  TwinDiffProof proof = TwinDiffProof::None;
  bool isCoarseAllocation = false;
  Operation *op = nullptr;
};

} // namespace mlir::arts

#endif // ARTS_ANALYSIS_HEURISTICS_TWINDIFFHEURISTICS_H
