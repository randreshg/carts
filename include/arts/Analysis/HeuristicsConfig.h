///==========================================================================///
/// File: HeuristicsConfig.h
///
/// This file defines the HeuristicsConfig class which centralizes all
/// compile-time heuristic decision logic for single-rank and other
/// optimizations in the CARTS compiler framework.
///==========================================================================///

#ifndef ARTS_ANALYSIS_HEURISTICSCONFIG_H
#define ARTS_ANALYSIS_HEURISTICSCONFIG_H

#include "arts/Analysis/Heuristics/PartitioningHeuristics.h"
#include "arts/Analysis/Heuristics/TwinDiffHeuristics.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "llvm/Support/JSON.h"
#include <memory>
#include <optional>

namespace mlir {

class Operation;

namespace arts {

class IdRegistry;

/// Records a single heuristic decision for diagnostics export.
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

/// Centralized heuristics configuration for compile-time optimizations.
class HeuristicsConfig {
public:
  HeuristicsConfig(const mlir::arts::ArtsAbstractMachine &machine,
                   IdRegistry &idRegistry);

  /// Machine Queries
  bool isSingleNode() const;
  bool isValid() const;

  /// H4: Twin-diff Heuristic
  bool shouldUseTwinDiff(const TwinDiffContext &context);

  /// Cost model thresholds
  static constexpr int64_t kMaxOuterDBs = 1024;
  static constexpr int64_t kMaxDepsPerEDT = 8;
  static constexpr int64_t kMinInnerBytes = 64;
  const ChunkingThresholds &getThresholds() const { return thresholds_; }

  /// H1: Unified Partitioning Heuristics Registry
  void registerHeuristic(std::unique_ptr<PartitioningHeuristic> h);
  PartitioningDecision getPartitioningMode(const PartitioningContext &ctx);

  /// Decision Recording for Diagnostics
  void recordDecision(llvm::StringRef heuristic, bool applied,
                      llvm::StringRef rationale, Operation *op,
                      const llvm::StringMap<int64_t> &inputs = {});
  llvm::ArrayRef<HeuristicDecision> getDecisions() const;
  void exportDecisionsToJson(llvm::json::OStream &J) const;

private:
  const mlir::arts::ArtsAbstractMachine &machine;
  IdRegistry &idRegistry_;
  llvm::SmallVector<HeuristicDecision> decisions_;
  ChunkingThresholds thresholds_;

  /// Registered partitioning heuristics (sorted by priority, highest first)
  llvm::SmallVector<std::unique_ptr<PartitioningHeuristic>>
      partitioningHeuristics_;

  void initializeThresholds();
  void initializeDefaultHeuristics();
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_HEURISTICSCONFIG_H
