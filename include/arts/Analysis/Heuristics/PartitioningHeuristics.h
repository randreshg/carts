//===- PartitioningHeuristics.h - H1.x partitioning heuristics --*- C++ -*-===//
//
// This file defines all H1.x partitioning heuristics for the CARTS compiler.
// These heuristics determine the optimal datablock partitioning strategy
// (coarse, element-wise, or chunked) based on access patterns and machine
// configuration.
//
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_HEURISTICS_PARTITIONINGHEURISTICS_H
#define ARTS_ANALYSIS_HEURISTICS_PARTITIONINGHEURISTICS_H

#include "arts/Analysis/Heuristics/HeuristicBase.h"
#include "arts/ArtsDialect.h"
#include "arts/Transforms/Datablock/DbRewriter.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>
#include <optional>
#include <string>

namespace mlir::arts {

class ArtsAbstractMachine;

//===----------------------------------------------------------------------===//
// Context and Decision Structures
//===----------------------------------------------------------------------===//

/// Context for partitioning decisions - all inputs needed by H1.x heuristics.
///
/// This struct encapsulates all the information that partitioning heuristics
/// need to make a decision: machine configuration, DB partitioning
/// capabilities, access patterns, and data characteristics.
struct PartitioningContext {
  /// Machine configuration (node count, execution mode, etc.)
  const ArtsAbstractMachine *machine = nullptr;

  // DB partitioning capabilities
  bool canElementWise = false; /// Has indexed deps (fine-grained possible)
  bool canChunked = false;     /// Has chunk deps (offset/size based)
  unsigned pinnedDimCount = 0; /// Dimensions with indexed access

  // DB info
  ArtsMode accessMode = ArtsMode::uninitialized; /// Read/Write/ReadWrite
  std::optional<int64_t> totalElements;          /// Total allocation size
  std::optional<int64_t> chunkSize;              /// Chunk size if known
  std::optional<int64_t> depsPerEDT;             /// Max deps per EDT

  // Access pattern info
  bool isUniformAccess = false;  /// All EDTs access same pattern
  bool isStencilPattern = false; /// Stencil access detected

  // Memref info
  unsigned memrefRank = 0; /// Total rank of the memref being partitioned
};

/// Result of a partitioning decision.
///
/// Encapsulates the recommended partitioning strategy including the rewriter
/// mode, grid dimensions, and a human-readable rationale for diagnostics.
struct PartitioningDecision {
  RewriterMode mode = RewriterMode::ElementWise; /// Chunked or ElementWise
  unsigned outerRank = 0; /// Outer grid rank (0 = coarse allocation)
  unsigned innerRank = 0; /// Inner element rank
  std::string rationale;  /// Human-readable explanation

  /// Returns true if this is a coarse (single-DB) allocation
  bool isCoarse() const { return outerRank == 0; }

  /// Returns true if this is a fine-grained (multi-DB) allocation
  bool isFineGrained() const { return outerRank > 0; }

  /// Returns true if using chunked rewriter mode
  bool isChunked() const { return mode == RewriterMode::Chunked; }
};

/// Mode-dependent chunking thresholds for H1.2 cost model.
///
/// These thresholds determine when fine-grained allocation is beneficial
/// based on the expected number of datablocks, dependencies per EDT, and
/// minimum chunk sizes.
struct ChunkingThresholds {
  int64_t maxOuterDBs;         /// Max total datablocks before rejecting
  int64_t maxTotalDBsPerEDT;   /// Max DBs per EDT (grid-aware!)
  int64_t minInnerBytes;       /// Min chunk size in bytes
  int64_t minElementsPerChunk; /// Min elements per chunk
};

//===----------------------------------------------------------------------===//
// Abstract Base for Partitioning Heuristics
//===----------------------------------------------------------------------===//

/// Abstract base class for all H1.x partitioning heuristics.
///
/// Each partitioning heuristic evaluates a PartitioningContext and optionally
/// returns a PartitioningDecision. Returning std::nullopt means the heuristic
/// does not apply to this context.
class PartitioningHeuristic : public HeuristicBase {
public:
  /// Evaluate this heuristic given the context.
  ///
  /// @param ctx The partitioning context containing all decision inputs
  /// @return A PartitioningDecision if this heuristic applies, std::nullopt
  /// otherwise
  virtual std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx) const = 0;
};

//===----------------------------------------------------------------------===//
// H1.1: Read-Only Single-Node Heuristic
//===----------------------------------------------------------------------===//

/// H1.1: Read-only allocations on single-node prefer coarse allocation.
///
/// Rationale: On a single node with read-only access, there is no write
/// contention to relieve via fine-graining. Coarse allocation reduces
/// datablock count and per-EDT dependency overhead.
class ReadOnlySingleNodeHeuristic : public PartitioningHeuristic {
public:
  llvm::StringRef getName() const override { return "H1.1"; }

  llvm::StringRef getDescription() const override {
    return "Read-only single-node prefers coarse allocation";
  }

  int getPriority() const override { return 100; } // Highest priority

  std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx) const override;
};

//===----------------------------------------------------------------------===//
// H1.2: Cost Model Heuristic
//===----------------------------------------------------------------------===//

/// H1.2: Cost model evaluation for fine-grained strategies.
///
/// Evaluates both chunked and element-wise allocation costs and returns
/// the better option. Uses threshold-based rejection for infeasible configs
/// and scoring for tie-breaking.
class CostModelHeuristic : public PartitioningHeuristic {
public:
  llvm::StringRef getName() const override { return "H1.2"; }

  llvm::StringRef getDescription() const override {
    return "Cost model evaluation for fine-grained allocation";
  }

  int getPriority() const override { return 50; } // Medium priority

  std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx) const override;

private:
  /// Evaluate cost for chunked allocation strategy.
  /// Returns a score (higher = better) or nullopt if infeasible.
  std::optional<int64_t>
  evaluateChunkedCost(const PartitioningContext &ctx) const;

  /// Evaluate cost for element-wise allocation strategy.
  /// Returns a score (higher = better) or nullopt if infeasible.
  std::optional<int64_t>
  evaluateElementWiseCost(const PartitioningContext &ctx) const;
};

//===----------------------------------------------------------------------===//
// H1.3: Access Uniformity Heuristic
//===----------------------------------------------------------------------===//

/// H1.3: Non-uniform access patterns prefer coarse allocation.
///
/// When access patterns are non-uniform and no fine-grained option is
/// available, default to coarse allocation to avoid complexity.
class AccessUniformityHeuristic : public PartitioningHeuristic {
public:
  llvm::StringRef getName() const override { return "H1.3"; }

  llvm::StringRef getDescription() const override {
    return "Non-uniform access prefers coarse allocation";
  }

  int getPriority() const override { return 80; } // High priority

  std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx) const override;
};

//===----------------------------------------------------------------------===//
// H1.4: Multi-Node Heuristic
//===----------------------------------------------------------------------===//

/// H1.4: Multi-node deployments prefer fine-grained for network efficiency.
///
/// On multi-node systems, fine-grained partitioning reduces data transfer
/// by allowing nodes to acquire only the data they need.
class MultiNodeHeuristic : public PartitioningHeuristic {
public:
  llvm::StringRef getName() const override { return "H1.4"; }

  llvm::StringRef getDescription() const override {
    return "Multi-node prefers fine-grained for network efficiency";
  }

  int getPriority() const override { return 90; } // High priority

  std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx) const override;
};

//===----------------------------------------------------------------------===//
// Factory Function
//===----------------------------------------------------------------------===//

/// Create all default partitioning heuristics.
///
/// Returns a vector of heuristics in registration order. The caller should
/// sort by priority if needed.
llvm::SmallVector<std::unique_ptr<PartitioningHeuristic>>
createDefaultPartitioningHeuristics();

} // namespace mlir::arts

#endif // ARTS_ANALYSIS_HEURISTICS_PARTITIONINGHEURISTICS_H
