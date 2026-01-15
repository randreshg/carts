///==========================================================================///
/// File: PartitioningHeuristics.cpp
///
/// This file implements all H1.x partitioning heuristics for the CARTS
/// compiler.
///==========================================================================///

#include "arts/Analysis/Heuristics/PartitioningHeuristics.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "arts/Utils/ArtsDebug.h"

ARTS_DEBUG_SETUP(heuristics)

using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// H1.1: Read-Only Single-Node
//===----------------------------------------------------------------------===//

std::optional<PartitioningDecision>
ReadOnlySingleNodeHeuristic::evaluate(const PartitioningContext &ctx) const {
  if (!ctx.machine)
    return std::nullopt;

  ///  Only applies: single-node + ALL acquires read-only + NO fine-grained
  ///  intent If ANY acquire has write access, we need fine-grained for
  ///  concurrency. Use allReadOnly() for per-acquire voting, fall back to
  ///  accessMode if no per-acquire info available (backward compatibility).
  bool isReadOnly = !ctx.acquires.empty() ? ctx.allReadOnly()
                                          : (ctx.accessMode == ArtsMode::in);

  if (ctx.machine->getNodeCount() == 1 && isReadOnly && !ctx.canChunked &&
      !ctx.canElementWise) {
    PartitioningDecision decision;
    decision.mode = RewriterMode::ElementWise;
    decision.outerRank = 0;              ///  Coarse
    decision.innerRank = ctx.memrefRank; ///  All dims are inner for coarse
    decision.rationale =
        "H1.1: All acquires read-only on single-node prefers coarse";
    ARTS_DEBUG("H1.1 applied: " << decision.rationale);
    return decision;
  }

  return std::nullopt;
}

//===----------------------------------------------------------------------===//
// H1.2: Cost Model
//===----------------------------------------------------------------------===//

std::optional<PartitioningDecision>
CostModelHeuristic::evaluate(const PartitioningContext &ctx) const {
  if (!ctx.machine)
    return std::nullopt;

  ///  Must have at least one fine-grained option available
  if (!ctx.canElementWise && !ctx.canChunked)
    return std::nullopt;

  auto chunkedCost = evaluateChunkedCost(ctx);
  auto elementWiseCost = evaluateElementWiseCost(ctx);

  ARTS_DEBUG("H1.2 cost evaluation: chunked="
             << (chunkedCost ? std::to_string(*chunkedCost) : "N/A")
             << ", elementWise="
             << (elementWiseCost ? std::to_string(*elementWiseCost) : "N/A"));

  ///  Neither option has a computable cost
  if (!chunkedCost && !elementWiseCost) {
    ///  Optimistic path: if canChunked is true, the code structure indicates
    ///  chunked intent (e.g., loop + chunk size deps). Honor that intent even
    ///  when we can't compute exact cost (dynamic chunk size / array size).
    if (ctx.canChunked) {
      PartitioningDecision decision;
      decision.mode = RewriterMode::Chunked;
      decision.outerRank = 1;
      decision.innerRank =
          ctx.memrefRank; ///  Chunked preserves conceptual rank
      decision.rationale = "H1.2: Optimistic chunking (dynamic sizes)";
      ARTS_DEBUG("H1.2 applied (optimistic): " << decision.rationale);
      return decision;
    }
    ///  Similarly for element-wise: allow if pinnedDimCount > 0 OR if write
    ///  access exists (write benefits from fine-grained even without indexed
    ///  deps)
    if (ctx.canElementWise &&
        (ctx.pinnedDimCount > 0 || ctx.hasWriteAccess())) {
      PartitioningDecision decision;
      decision.mode = RewriterMode::ElementWise;
      ///  Use pinnedDimCount if available, otherwise default to 1 for write
      ///  access
      decision.outerRank = ctx.pinnedDimCount > 0 ? ctx.pinnedDimCount : 1;
      decision.innerRank = ctx.memrefRank > decision.outerRank
                               ? ctx.memrefRank - decision.outerRank
                               : 0;
      decision.rationale =
          ctx.pinnedDimCount > 0
              ? "H1.2: Optimistic element-wise (dynamic sizes)"
              : "H1.2: Write access benefits from element-wise partitioning";
      ARTS_DEBUG("H1.2 applied (optimistic): " << decision.rationale);
      return decision;
    }
    return std::nullopt;
  }

  ///  Prefer chunked if available and has better/equal cost
  bool preferChunked =
      chunkedCost.has_value() &&
      (!elementWiseCost.has_value() || *chunkedCost >= *elementWiseCost);

  PartitioningDecision decision;
  if (preferChunked && ctx.canChunked) {
    decision.mode = RewriterMode::Chunked;
    decision.outerRank = 1;
    decision.innerRank = ctx.memrefRank; ///  Chunked preserves conceptual rank
    decision.rationale = "H1.2: Cost model prefers chunked";
  } else if (ctx.canElementWise) {
    decision.mode = RewriterMode::ElementWise;
    decision.outerRank = ctx.pinnedDimCount;
    decision.innerRank = ctx.memrefRank - ctx.pinnedDimCount;
    decision.rationale = "H1.2: Cost model prefers element-wise";
  } else {
    return std::nullopt;
  }

  ARTS_DEBUG("H1.2 applied: " << decision.rationale);
  return decision;
}

std::optional<int64_t>
CostModelHeuristic::evaluateChunkedCost(const PartitioningContext &ctx) const {
  if (!ctx.canChunked || !ctx.chunkSize || !ctx.totalElements)
    return std::nullopt;

  int64_t chunkCount =
      (*ctx.totalElements + *ctx.chunkSize - 1) / *ctx.chunkSize;

  ///  Threshold checks (can be made configurable via HeuristicsConfig)
  constexpr int64_t kMaxChunks = 1024;
  constexpr int64_t kMinChunkBytes = 64;

  if (chunkCount > kMaxChunks) {
    ARTS_DEBUG("  H1.2 chunked rejected: chunkCount=" << chunkCount << " > "
                                                      << kMaxChunks);
    return std::nullopt;
  }

  ///  Assume f64 (8 bytes) for byte calculation
  int64_t chunkBytes = *ctx.chunkSize * 8;
  if (chunkBytes < kMinChunkBytes) {
    ARTS_DEBUG("  H1.2 chunked rejected: chunkBytes=" << chunkBytes << " < "
                                                      << kMinChunkBytes);
    return std::nullopt;
  }

  ///  Score: higher = better (fewer chunks preferred)
  int64_t score = 100 - (chunkCount / 10);
  ARTS_DEBUG("  H1.2 chunked score=" << score << " (chunkCount=" << chunkCount
                                     << ")");
  return score;
}

std::optional<int64_t> CostModelHeuristic::evaluateElementWiseCost(
    const PartitioningContext &ctx) const {
  if (!ctx.canElementWise || ctx.pinnedDimCount == 0)
    return std::nullopt;

  ///  Base score for element-wise
  int64_t score = 50;
  ARTS_DEBUG("  H1.2 element-wise score=" << score);
  return score;
}

//===----------------------------------------------------------------------===//
// H1.3: Access Uniformity
//===----------------------------------------------------------------------===//

std::optional<PartitioningDecision>
AccessUniformityHeuristic::evaluate(const PartitioningContext &ctx) const {
  ///  Non-uniform access with no fine-grained option -> coarse
  if (!ctx.isUniformAccess && !ctx.canElementWise && !ctx.canChunked) {
    PartitioningDecision decision;
    decision.mode = RewriterMode::ElementWise;
    decision.outerRank = 0;
    decision.innerRank = ctx.memrefRank; ///  All dims are inner for coarse
    decision.rationale = "H1.3: Non-uniform access prefers coarse";
    ARTS_DEBUG("H1.3 applied: " << decision.rationale);
    return decision;
  }

  return std::nullopt;
}

//===----------------------------------------------------------------------===//
// H1.4: Multi-Node
//===----------------------------------------------------------------------===//

std::optional<PartitioningDecision>
MultiNodeHeuristic::evaluate(const PartitioningContext &ctx) const {
  if (!ctx.machine)
    return std::nullopt;

  ///  Only applies to multi-node deployments
  if (ctx.machine->getNodeCount() <= 1)
    return std::nullopt;

  ///  Use per-acquire voting: check both aggregate and per-acquire capabilities
  bool canDoChunked = ctx.canChunked || ctx.anyCanChunked();
  bool canDoElementWise = ctx.canElementWise || ctx.anyCanElementWise();

  PartitioningDecision decision;
  if (canDoChunked) {
    decision.mode = RewriterMode::Chunked;
    decision.outerRank = 1;
    decision.innerRank = ctx.memrefRank; ///  Chunked preserves conceptual rank
    decision.rationale =
        "H1.4: Multi-node prefers chunked for network efficiency";
  } else if (canDoElementWise) {
    decision.mode = RewriterMode::ElementWise;
    ///  Use maxPinnedDimCount if available, otherwise default to 1
    unsigned outerRank =
        ctx.pinnedDimCount > 0 ? ctx.pinnedDimCount : ctx.maxPinnedDimCount();
    decision.outerRank = outerRank > 0 ? outerRank : 1;
    decision.innerRank = ctx.memrefRank > decision.outerRank
                             ? ctx.memrefRank - decision.outerRank
                             : 0;
    decision.rationale = "H1.4: Multi-node prefers fine-grained";
  } else {
    return std::nullopt;
  }

  ARTS_DEBUG("H1.4 applied: " << decision.rationale);
  return decision;
}

//===----------------------------------------------------------------------===//
// H1.5: Stencil Pattern
//===----------------------------------------------------------------------===//

std::optional<PartitioningDecision>
StencilPatternHeuristic::evaluate(const PartitioningContext &ctx) const {
  const auto &patterns = ctx.accessPatterns;

  /// Case 1: Has indexed access → Element-wise (unpredictable access pattern)
  /// Indexed access patterns cannot be handled by chunked partitioning since
  /// the access locations are data-dependent.
  if (patterns.hasIndexed) {
    PartitioningDecision decision;
    decision.mode = RewriterMode::ElementWise;
    decision.outerRank = 1;
    decision.innerRank = ctx.memrefRank > 0 ? ctx.memrefRank - 1 : 0;
    decision.rationale = "H1.5: Indexed access requires element-wise";
    ARTS_DEBUG("H1.5 applied: " << decision.rationale);
    return decision;
  }

  /// Case 2: Has stencil (with or without uniform) → Stencil mode (chunked + ESD)
  /// Chunked + ESD handles both access patterns:
  /// - Uniform access: worker accesses only its owned chunk (no halo needed)
  /// - Stencil access: worker accesses owned chunk + ESD delivers halos
  /// The ESD machinery activates when element_offsets is non-empty on db_acquire.
  if (patterns.hasStencil) {
    if (!ctx.canChunked) {
      PartitioningDecision decision;
      decision.mode = RewriterMode::ElementWise;
      decision.outerRank = 1;
      decision.innerRank = ctx.memrefRank > 0 ? ctx.memrefRank - 1 : 0;
      decision.rationale =
          "H1.5: Stencil detected but chunked unsupported; fallback to element-wise";
      ARTS_DEBUG("H1.5 applied: " << decision.rationale);
      return decision;
    }

    PartitioningDecision decision;
    decision.mode = RewriterMode::Stencil;
    decision.outerRank = 1;
    decision.innerRank = ctx.memrefRank > 0 ? ctx.memrefRank - 1 : 0;
    decision.rationale =
        patterns.hasUniform
            ? "H1.5: Mixed (uniform+stencil) uses Stencil mode - chunked handles both"
            : "H1.5: Pure stencil uses ESD mode";
    ARTS_DEBUG("H1.5 applied: " << decision.rationale);
    return decision;
  }

  /// Not a stencil pattern - let other heuristics decide
  return std::nullopt;
}

//===----------------------------------------------------------------------===//
// Factory Function
//===----------------------------------------------------------------------===//

llvm::SmallVector<std::unique_ptr<PartitioningHeuristic>>
mlir::arts::createDefaultPartitioningHeuristics() {
  llvm::SmallVector<std::unique_ptr<PartitioningHeuristic>> heuristics;

  ///  Add heuristics (will be sorted by priority in HeuristicsConfig)
  heuristics.push_back(std::make_unique<ReadOnlySingleNodeHeuristic>());
  heuristics.push_back(std::make_unique<StencilPatternHeuristic>());
  heuristics.push_back(std::make_unique<MultiNodeHeuristic>());
  heuristics.push_back(std::make_unique<AccessUniformityHeuristic>());
  heuristics.push_back(std::make_unique<CostModelHeuristic>());

  ARTS_DEBUG("Created " << heuristics.size()
                        << " default partitioning heuristics");
  return heuristics;
}
