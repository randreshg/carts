//===- TwinDiffHeuristics.h - H4.x twin-diff heuristics ---------*- C++ -*-===//
//
// This file defines the H4.x twin-diff heuristics for the CARTS compiler.
// These heuristics determine when twin-diff can be safely disabled based on
// provable non-overlap of datablock accesses.
//
//===----------------------------------------------------------------------===//

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
  None,             /// No proof - use safe default (twin_diff=true)
  IndexedControl,   /// DbControlOps provided indexed access (proven isolation)
  PartitionSuccess, /// Partitioning proved non-overlapping chunks
  AliasAnalysis,    /// DbAliasAnalysis proved disjoint acquires
};

/// Context for twin-diff decisions (H4 heuristic).
///
/// Encapsulates all information needed to decide whether twin-diff
/// can be safely disabled for a particular acquire operation.
struct TwinDiffContext {
  TwinDiffProof proof = TwinDiffProof::None; /// How non-overlap was proven
  bool isCoarseAllocation = false;           /// Coarse = potentially overlapping
  Operation *op = nullptr; /// The operation this decision applies to
};

//===----------------------------------------------------------------------===//
// Future: H4.x Twin-Diff Heuristic Classes
//===----------------------------------------------------------------------===//

// TODO: Refactor shouldUseTwinDiff() from HeuristicsConfig into H4.x classes:
//
// class SingleNodeTwinDiffHeuristic : public TwinDiffHeuristic {
//   // H4.1: Single-node always disables twin-diff (no remote recipient)
// };
//
// class IndexedControlTwinDiffHeuristic : public TwinDiffHeuristic {
//   // H4.2: Indexed DbControlOps prove isolation
// };
//
// class PartitionSuccessTwinDiffHeuristic : public TwinDiffHeuristic {
//   // H4.3: Successful partitioning proves non-overlap
// };

} // namespace mlir::arts

#endif // ARTS_ANALYSIS_HEURISTICS_TWINDIFFHEURISTICS_H
