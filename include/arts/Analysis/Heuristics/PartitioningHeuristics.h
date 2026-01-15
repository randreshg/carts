///==========================================================================///
/// File: PartitioningHeuristics.h
///
/// This file defines all H1.x partitioning heuristics for the CARTS compiler.
// These heuristics determine the optimal datablock partitioning strategy
///==========================================================================///

#ifndef ARTS_ANALYSIS_HEURISTICS_PARTITIONINGHEURISTICS_H
#define ARTS_ANALYSIS_HEURISTICS_PARTITIONINGHEURISTICS_H

#include "arts/Analysis/Graphs/Db/DbAccessPattern.h"
#include "arts/Analysis/Heuristics/HeuristicBase.h"
#include "arts/ArtsDialect.h"
#include "arts/Transforms/Datablock/DbRewriter.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>
#include <optional>
#include <string>

namespace mlir::arts {

class ArtsAbstractMachine;

//===----------------------------------------------------------------------===//
// Context and Decision Structures
//===----------------------------------------------------------------------===//

/// Per-acquire info for heuristic voting.
struct AcquireInfo {
  ArtsMode accessMode = ArtsMode::uninitialized;
  bool canElementWise = false;
  bool canChunked = false;
  unsigned pinnedDimCount = 0;
};

/// Context for partitioning decisions - all inputs needed by H1.x heuristics.
struct PartitioningContext {
  /// Machine configuration (node count, execution mode, etc.)
  const ArtsAbstractMachine *machine = nullptr;

  ///  DB partitioning capabilities
  bool canElementWise = false, canChunked = false;
  unsigned pinnedDimCount = 0;
  ArtsMode accessMode = ArtsMode::uninitialized;
  std::optional<int64_t> totalElements, chunkSize, depsPerEDT;
  bool isUniformAccess = false; /// All EDTs access same pattern
  AcquirePatternSummary accessPatterns;
  ///  Memref info
  unsigned memrefRank = 0; /// Total rank of the memref being partitioned

  ///  Per-acquire info for voting (populated from DbAcquireNodes)
  SmallVector<AcquireInfo> acquires;

  ///  Aggregation helpers for per-acquire voting

  /// Returns true if any acquire has write access (out or inout).
  bool hasWriteAccess() const {
    return llvm::any_of(acquires, [](const AcquireInfo &a) {
      return a.accessMode == ArtsMode::out || a.accessMode == ArtsMode::inout;
    });
  }

  /// Returns true if ALL acquires are read-only (in mode).
  bool allReadOnly() const {
    return !acquires.empty() &&
           llvm::all_of(acquires, [](const AcquireInfo &a) {
             return a.accessMode == ArtsMode::in;
           });
  }

  /// Returns true if any acquire can use element-wise partitioning.
  bool anyCanElementWise() const {
    return llvm::any_of(acquires,
                        [](const AcquireInfo &a) { return a.canElementWise; });
  }

  /// Returns true if any acquire can use chunked partitioning.
  bool anyCanChunked() const {
    return llvm::any_of(acquires,
                        [](const AcquireInfo &a) { return a.canChunked; });
  }

  /// Returns the maximum pinnedDimCount across all acquires.
  unsigned maxPinnedDimCount() const {
    unsigned maxVal = 0;
    for (const auto &a : acquires)
      maxVal = std::max(maxVal, a.pinnedDimCount);
    return maxVal;
  }
};

/// Result of a partitioning decision.
///
/// Encapsulates the recommended partitioning strategy including the rewriter
/// mode, grid dimensions, and a human-readable rationale for diagnostics.
struct PartitioningDecision {
  RewriterMode mode = RewriterMode::ElementWise; /// Chunked or ElementWise
  unsigned outerRank = 0, innerRank = 0;
  std::string rationale;

  static PartitioningDecision coarse(const PartitioningContext &ctx,
                                     llvm::StringRef reason) {
    PartitioningDecision decision;
    decision.mode = RewriterMode::ElementWise;
    decision.outerRank = 0;
    decision.innerRank = ctx.memrefRank; /// All dims are inner for coarse
    decision.rationale = reason.str();
    return decision;
  }

  bool isCoarse() const { return outerRank == 0; }
  bool isFineGrained() const { return outerRank > 0; }
  bool isChunked() const {
    return mode == RewriterMode::Chunked || mode == RewriterMode::Stencil;
  }
};

struct ChunkingThresholds {
  int64_t maxOuterDBs, maxTotalDBsPerEDT, minInnerBytes, minElementsPerChunk;
};

//===----------------------------------------------------------------------===//
// Abstract Base for Partitioning Heuristics
//===----------------------------------------------------------------------===//

/// Abstract base class for all H1.x partitioning heuristics.
class PartitioningHeuristic : public HeuristicBase {
public:
  /// Evaluate this heuristic given the context.
  virtual std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx) const = 0;
};

//===----------------------------------------------------------------------===//
/// H1.1: Read-Only Single-Node Heuristic
/// Read-only allocations on single-node prefer coarse allocation.
///  Rationale: On a single node with read-only access, there is no write
///  contention to relieve via fine-graining. Coarse allocation reduces
///  datablock count and per-EDT dependency overhead.
//===----------------------------------------------------------------------===//
class ReadOnlySingleNodeHeuristic : public PartitioningHeuristic {
public:
  llvm::StringRef getName() const override { return "H1.1"; }

  llvm::StringRef getDescription() const override {
    return "Read-only single-node prefers coarse allocation";
  }

  int getPriority() const override { return 100; }

  std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx) const override;
};

//===----------------------------------------------------------------------===//
// H1.2: Cost Model Heuristic
/// Cost model evaluation for fine-grained strategies.
/// Evaluates both chunked and element-wise allocation costs and returns
/// the better option. Uses threshold-based rejection for infeasible configs
/// and scoring for tie-breaking.
//===----------------------------------------------------------------------===//
class CostModelHeuristic : public PartitioningHeuristic {
public:
  llvm::StringRef getName() const override { return "H1.2"; }

  llvm::StringRef getDescription() const override {
    return "Cost model evaluation for fine-grained allocation";
  }

  int getPriority() const override { return 50; }

  std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx) const override;

private:
  std::optional<int64_t>
  evaluateChunkedCost(const PartitioningContext &ctx) const;

  /// Evaluate cost for element-wise allocation strategy.
  std::optional<int64_t>
  evaluateElementWiseCost(const PartitioningContext &ctx) const;
};

//===----------------------------------------------------------------------===//
// H1.3: Access Uniformity Heuristic
/// Non-uniform access patterns prefer coarse allocation.
/// When access patterns are non-uniform and no fine-grained option is
/// available, default to coarse allocation to avoid complexity.
//===----------------------------------------------------------------------===//

class AccessUniformityHeuristic : public PartitioningHeuristic {
public:
  llvm::StringRef getName() const override { return "H1.3"; }

  llvm::StringRef getDescription() const override {
    return "Non-uniform access prefers coarse allocation";
  }

  int getPriority() const override { return 80; }

  std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx) const override;
};

//===----------------------------------------------------------------------===//
// H1.4: Multi-Node Heuristic
/// Multi-node deployments prefer fine-grained for network efficiency.
/// On multi-node systems, fine-grained partitioning reduces data transfer
/// by allowing nodes to acquire only the data they need.
//===----------------------------------------------------------------------===//
class MultiNodeHeuristic : public PartitioningHeuristic {
public:
  llvm::StringRef getName() const override { return "H1.4"; }

  llvm::StringRef getDescription() const override {
    return "Multi-node prefers fine-grained for network efficiency";
  }

  int getPriority() const override { return 90; }

  std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx) const override;
};

//===----------------------------------------------------------------------===//
// H1.5: Stencil Pattern Heuristic
/// Stencil and mixed pattern handling.
/// Handles access patterns that require special partitioning:
/// - Mixed patterns (uniform+stencil, etc.) → Element-wise for fine granularity
/// - Pure stencil patterns → Stencil mode (ESD) for halo-aware partitioning
//===----------------------------------------------------------------------===//
class StencilPatternHeuristic : public PartitioningHeuristic {
public:
  llvm::StringRef getName() const override { return "H1.5"; }

  llvm::StringRef getDescription() const override {
    return "Stencil and mixed pattern handling";
  }

  int getPriority() const override { return 95; }

  std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx) const override;
};

//===----------------------------------------------------------------------===//
// Factory Function
//===----------------------------------------------------------------------===//

/// Create all default partitioning heuristics.
llvm::SmallVector<std::unique_ptr<PartitioningHeuristic>>
createDefaultPartitioningHeuristics();

} // namespace mlir::arts

#endif ///  ARTS_ANALYSIS_HEURISTICS_PARTITIONINGHEURISTICS_H
