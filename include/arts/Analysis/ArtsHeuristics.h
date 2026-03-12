///==========================================================================///
/// File: ArtsHeuristics.h
///
/// Compile-time heuristics infrastructure for the CARTS compiler.
///
/// This header provides the base infrastructure (HeuristicsConfig, decision
/// recording, diagnostics) and re-exports PartitioningHeuristics for backward
/// compatibility.
///
/// Heuristic Families:
///   H1: Partitioning - see PartitioningHeuristics.h
///   H2: Distribution - see DistributionHeuristics.h
///==========================================================================///

#ifndef ARTS_ANALYSIS_ARTSHEURISTICS_H
#define ARTS_ANALYSIS_ARTSHEURISTICS_H

#include "arts/Analysis/DistributionHeuristics.h"
#include "arts/Analysis/HeuristicUtils.h"
#include "arts/Analysis/PartitioningHeuristics.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/JSON.h"
#include <string>

namespace mlir {

class Operation;

namespace arts {

class IdRegistry;
struct DbAcquirePartitionFacts;

///===----------------------------------------------------------------------===///
/// HeuristicDecision (for ArtsMate diagnostics)
///===----------------------------------------------------------------------===///

/// Single heuristic decision for diagnostics export
struct HeuristicDecision {
  std::string heuristic;
  bool applied;
  std::string rationale;
  int64_t affectedArtsId;
  int64_t affectedAllocId;
  llvm::SmallVector<int64_t> affectedDbIds;
  std::string sourceLocation;
  llvm::StringMap<int64_t> costModelInputs;
};

///===----------------------------------------------------------------------===///
/// HeuristicsConfig Class
///===----------------------------------------------------------------------===///

/// Centralized heuristics configuration
class HeuristicsConfig {
public:
  HeuristicsConfig(const mlir::arts::ArtsAbstractMachine &machine,
                   IdRegistry &idRegistry,
                   PartitionFallback fallback = PartitionFallback::Coarse);

  /// Machine configuration queries
  bool isSingleNode() const;
  bool isValid() const;

  /// Cost model thresholds for block partitioning
  static constexpr int64_t kMaxOuterDBs = 1024;
  static constexpr int64_t kMaxDepsPerEDT = 8;
  static constexpr int64_t kMinInnerBytes = 64;

  /// H1: Partitioning Mode Evaluation
  PartitioningDecision getPartitioningMode(const PartitioningContext &ctx);

  /// H1.7: Per-acquire partitioning decisions based on canPartitionWithOffset.
  SmallVector<AcquireDecision>
  getAcquireDecisions(ArrayRef<const DbAcquirePartitionFacts *> acquireFacts);

  /// Decision recording for diagnostics
  void recordDecision(llvm::StringRef heuristic, bool applied,
                      llvm::StringRef rationale, Operation *op,
                      const llvm::StringMap<int64_t> &inputs = {});
  llvm::ArrayRef<HeuristicDecision> getDecisions() const;
  void exportDecisionsToJson(llvm::json::OStream &J) const;

  /// Clear accumulated decisions (called on invalidation)
  void clearDecisions() { decisions.clear(); }

private:
  const mlir::arts::ArtsAbstractMachine &machine;
  IdRegistry &idRegistry;
  llvm::SmallVector<HeuristicDecision> decisions;
  PartitionFallback partitionFallback;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_ARTSHEURISTICS_H
