///==========================================================================///
/// File: HeuristicsConfig.h
///
/// This file defines the HeuristicsConfig class which centralizes all
/// compile-time heuristic decision logic for single-rank and other
/// optimizations in the CARTS compiler framework.
///==========================================================================///

#ifndef ARTS_ANALYSIS_HEURISTICSCONFIG_H
#define ARTS_ANALYSIS_HEURISTICSCONFIG_H

#include "arts/Analysis/Graphs/Db/DbAccessPattern.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "llvm/Support/JSON.h"
#include <optional>

namespace mlir {

class Operation;

namespace arts {

class IdRegistry;

/// Forward declarations to avoid circular includes
enum class RewriterMode;
enum class AccessPattern;

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

/// Source of partitioning decision (affects priority)
enum class PartitioningSource {
  UserProvided, /// User specified depend clause (omp task) - respect it
  ParallelFor,  /// parallel_for without deps - prioritize chunking
  Fallback      /// Default/unknown - apply standard H2
};

/// Proof method for why twin-diff can be disabled
enum class TwinDiffProof {
  None,             /// No proof - use safe default (twin_diff=true)
  IndexedControl,   // DbControlOps provided indexed access (proven isolation)
  PartitionSuccess, /// Partitioning proved non-overlapping chunks
  AliasAnalysis,    /// DbAliasAnalysis proved disjoint acquires
};

/// Context for twin-diff decisions (for centralized decision-making)
struct TwinDiffContext {
  TwinDiffProof proof = TwinDiffProof::None;
  bool isCoarseAllocation = false;
  Operation *op = nullptr; /// The operation this decision applies to
};

/// Mode-dependent chunking thresholds
struct ChunkingThresholds {
  int64_t maxOuterDBs;         /// Max total datablocks
  int64_t maxTotalDBsPerEDT;   /// Max DBs per EDT (grid-aware!)
  int64_t minInnerBytes;       /// Min chunk size in bytes
  int64_t minElementsPerChunk; /// Min elements per chunk
};

/// Context for partitioning mode decisions (H3 heuristic)
struct PartitioningModeContext {
  AccessPattern pattern;                      /// Dominant access pattern
  bool hasChunkHints = false;                 /// Chunk hints available
  std::optional<int64_t> totalElements;       /// Total elements in allocation
  std::optional<int64_t> chunkSize;           /// Chunk size if known
  std::optional<StencilBounds> stencilBounds; /// Stencil bounds if detected
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

  /// Get mode-dependent thresholds
  const ChunkingThresholds &getThresholds() const { return thresholds_; }

  /// Compute minimum inner bytes
  int64_t computeMinInnerBytes() const;

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
  // H3: Partitioning Mode Heuristic
  //===--------------------------------------------------------------------===//

  /// Recommend partitioning mode based on access patterns and context.
  /// Decision logic:
  /// - Stencil patterns → ElementWise (until ESD infrastructure ready)
  /// - Uniform + chunk hints + H2 passes → Chunked
  /// - Default → ElementWise (safest, works for all patterns)
  RewriterMode
  getRecommendedPartitioningMode(const PartitioningModeContext &context) const;

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

  /// Initialize thresholds based on machine config
  void initializeThresholds();
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_HEURISTICSCONFIG_H
