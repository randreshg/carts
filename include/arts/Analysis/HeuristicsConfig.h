///==========================================================================///
/// File: HeuristicsConfig.h
///
/// This file defines the HeuristicsConfig class which centralizes all
/// compile-time heuristic decision logic for single-rank and other
/// optimizations in the CARTS compiler framework.
///==========================================================================///

#ifndef ARTS_ANALYSIS_HEURISTICSCONFIG_H
#define ARTS_ANALYSIS_HEURISTICSCONFIG_H

#include "arts/ArtsDialect.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "llvm/Support/JSON.h"
#include <optional>

namespace mlir {
namespace arts {

/// Records a single heuristic decision for diagnostics export
struct HeuristicDecision {
  std::string heuristic;
  bool applied;
  std::string rationale;
  int64_t affectedArtsId;
  llvm::StringMap<int64_t> costModelInputs;
};

/// Source of partitioning decision (affects priority)
enum class PartitioningSource {
  UserProvided, // User specified depend clause (omp task) - respect it
  ParallelFor,  // parallel_for without deps - prioritize chunking
  Fallback      // Default/unknown - apply standard H2
};

/// Mode-dependent chunking thresholds (derived from MemoryConfig)
struct ChunkingThresholds {
  int64_t maxOuterDBs;         // Max total datablocks
  int64_t maxTotalDBsPerEDT;   // Max DBs per EDT (grid-aware!)
  int64_t minInnerBytes;       // Min chunk size in bytes
  int64_t optimalInnerBytes;   // Optimal chunk (L2 cache fit)
  int64_t minElementsPerChunk; // Min elements per chunk
};

/// Centralized heuristics configuration for compile-time optimizations.
/// Provides decision methods for single-rank and other optimizations.
class HeuristicsConfig {
public:
  explicit HeuristicsConfig(const mlir::arts::ArtsAbstractMachine &machine);

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

  /// Returns true if twin_diff should be disabled.
  /// On single-node, twin_diff is pure overhead (no remote recipient).
  bool shouldDisableTwinDiff() const;

  //===--------------------------------------------------------------------===//
  // H1: Read-only Allocation Heuristic
  //===--------------------------------------------------------------------===//

  /// Returns true if read-only memrefs should prefer coarse allocation.
  /// On single-node with read-only access, fine-graining adds overhead
  /// without relieving any write contention.
  bool shouldPreferCoarseForReadOnly(mlir::arts::ArtsMode accessMode) const;

  //===--------------------------------------------------------------------===//
  // H2: Cost Model Heuristic (Memory-Aware)
  //===--------------------------------------------------------------------===//

  /// Legacy cost model thresholds (for backward compatibility)
  static constexpr int64_t kMaxOuterDBs = 1024;
  static constexpr int64_t kMaxDepsPerEDT = 8;
  static constexpr int64_t kMinInnerBytes = 64;

  /// Get mode-dependent thresholds (computed from MemoryConfig)
  const ChunkingThresholds &getThresholds() const { return thresholds_; }

  /// Compute minimum inner bytes based on execution mode and memory tier
  int64_t computeMinInnerBytes() const;

  /// Compute optimal inner bytes for cache efficiency
  int64_t computeOptimalInnerBytes() const;

  /// Evaluates cost model for fine-grained allocation decision.
  /// Uses partial evaluation: rejects early if any known value fails,
  /// returns nullopt if some values unknown but no known failures.
  ///
  /// Return values:
  /// - true: All values known and pass -> use fine-grained (chunking)
  /// - false: A known value failed -> use coarse (element-wise)
  /// - nullopt: Known values pass but some unknown -> caller decides
  /// Evaluates cost model for fine-grained allocation decision.
  /// Parameters:
  /// - totalDBs: Total number of datablocks (grid-aware: product of outer
  /// sizes)
  /// - totalDBsPerEDT: Total DBs accessed per EDT (grid-aware)
  /// - innerBytes: Chunk size in bytes
  /// - source: Where this decision comes from (affects priority)
  std::optional<bool> shouldUseFineGrained(
      std::optional<int64_t> totalDBs, std::optional<int64_t> totalDBsPerEDT,
      std::optional<int64_t> innerBytes,
      PartitioningSource source = PartitioningSource::Fallback) const;

  //===--------------------------------------------------------------------===//
  // Decision Recording for Diagnostics
  //===--------------------------------------------------------------------===//

  /// Records a heuristic decision for diagnostics export
  void recordDecision(llvm::StringRef heuristic, bool applied,
                      llvm::StringRef rationale, int64_t artsId,
                      const llvm::StringMap<int64_t> &inputs = {});

  /// Returns all recorded heuristic decisions
  llvm::ArrayRef<HeuristicDecision> getDecisions() const;

  /// Exports recorded decisions to JSON
  void exportDecisionsToJson(llvm::json::OStream &J) const;

private:
  const mlir::arts::ArtsAbstractMachine &machine;
  llvm::SmallVector<HeuristicDecision> decisions_; ///< Recorded decisions
  ChunkingThresholds thresholds_; ///< Mode-dependent thresholds

  /// Initialize thresholds based on machine config
  void initializeThresholds();
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_HEURISTICSCONFIG_H