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
/// Follows PGO pattern: compute compile-time → runtime mapping for correlation.
struct HeuristicDecision {
  std::string heuristic;
  bool applied;
  std::string rationale;
  int64_t affectedArtsId;  /// Target operation arts_id (e.g., DbAcquireOp)
  int64_t affectedAllocId; /// Parent allocation arts_id (e.g., DbAllocOp)
  llvm::SmallVector<int64_t>
      affectedDbIds;          /// Runtime DB IDs this decision affects
  std::string sourceLocation; /// Source location for ArtsMate correlation
  llvm::StringMap<int64_t> costModelInputs;
};

/// Centralized heuristics configuration for compile-time optimizations.
/// Provides decision methods for single-rank and other optimizations.
class HeuristicsConfig {
public:
  HeuristicsConfig(const mlir::arts::ArtsAbstractMachine &machine,
                   IdRegistry &idRegistry);

  //===--------------------------------------------------------------------===//
  // Machine Queries
  //===--------------------------------------------------------------------===//

  /// Returns true if running on a single node (nodeCount == 1)
  bool isSingleNode() const;

  /// Returns true if machine config is valid
  bool isValid() const;

  //===--------------------------------------------------------------------===//
  // H4: Twin-diff Heuristic
  //===--------------------------------------------------------------------===//

  /// Consolidated twin-diff decision based on context and proof method.
  /// Returns true if twin_diff should be ENABLED (safe default).
  /// Returns false if twin_diff can be safely DISABLED (proven non-overlap).
  ///
  /// Decision priority:
  /// 1. Single-node (H4): Always disable (no remote recipient)
  /// 2. Proof-based: Disable if proof of non-overlap exists
  /// 3. Default: Enable (safe, handles potential overlap at runtime)
  bool shouldUseTwinDiff(const TwinDiffContext &context);

  //===--------------------------------------------------------------------===//
  // Cost Model Thresholds (used by H1.2)
  //===--------------------------------------------------------------------===//

  /// Cost model thresholds for backward compatibility
  static constexpr int64_t kMaxOuterDBs = 1024;
  static constexpr int64_t kMaxDepsPerEDT = 8;
  static constexpr int64_t kMinInnerBytes = 64;

  /// Get mode-dependent thresholds
  const ChunkingThresholds &getThresholds() const { return thresholds_; }

  //===--------------------------------------------------------------------===//
  // H1: Unified Partitioning Heuristics Registry
  //===--------------------------------------------------------------------===//

  /// Register a partitioning heuristic.
  /// Heuristics are evaluated in priority order (highest first).
  void registerHeuristic(std::unique_ptr<PartitioningHeuristic> h);

  /// Get partitioning decision by evaluating all registered heuristics.
  /// Heuristics are evaluated in priority order (highest first).
  /// The first heuristic that returns a decision wins.
  ///
  /// @param ctx The partitioning context containing all decision inputs
  /// @return The partitioning decision (mode, ranks, rationale)
  PartitioningDecision getPartitioningMode(const PartitioningContext &ctx);

  //===--------------------------------------------------------------------===//
  // Decision Recording for Diagnostics
  //===--------------------------------------------------------------------===//

  /// Records a heuristic decision for diagnostics export.
  /// Preferred interface: pass the affected Operation* and let the config
  /// resolve the artsId automatically via IdRegistry.
  void recordDecision(llvm::StringRef heuristic, bool applied,
                      llvm::StringRef rationale, Operation *op,
                      const llvm::StringMap<int64_t> &inputs = {});

  /// Returns all recorded heuristic decisions
  llvm::ArrayRef<HeuristicDecision> getDecisions() const;

  /// Exports recorded decisions to JSON
  void exportDecisionsToJson(llvm::json::OStream &J) const;

private:
  const mlir::arts::ArtsAbstractMachine &machine;
  IdRegistry &idRegistry_;
  llvm::SmallVector<HeuristicDecision> decisions_;
  ChunkingThresholds thresholds_;

  /// Registered partitioning heuristics (sorted by priority, highest first)
  llvm::SmallVector<std::unique_ptr<PartitioningHeuristic>>
      partitioningHeuristics_;

  /// Initialize thresholds based on machine config
  void initializeThresholds();

  /// Initialize and register default heuristics
  void initializeDefaultHeuristics();
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_HEURISTICSCONFIG_H
