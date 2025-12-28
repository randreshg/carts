//===- PartitioningHeuristics.cpp - H1.x partitioning heuristics ----------===//
//
// This file implements all H1.x partitioning heuristics for the CARTS compiler.
//
//===----------------------------------------------------------------------===//

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

  // Only applies: single-node + read-only + NO explicit chunked/element-wise intent
  // If canChunked or canElementWise is true, the code structure indicates the
  // programmer intended fine-grained access - respect that intent.
  if (ctx.machine->getNodeCount() == 1 && ctx.accessMode == ArtsMode::in &&
      !ctx.canChunked && !ctx.canElementWise) {
    PartitioningDecision decision;
    decision.mode = RewriterMode::ElementWise;
    decision.outerRank = 0; // Coarse
    decision.innerRank = ctx.memrefRank; // All dims are inner for coarse
    decision.rationale = "H1.1: Read-only on single-node prefers coarse";
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

  // Must have at least one fine-grained option available
  if (!ctx.canElementWise && !ctx.canChunked)
    return std::nullopt;

  auto chunkedCost = evaluateChunkedCost(ctx);
  auto elementWiseCost = evaluateElementWiseCost(ctx);

  ARTS_DEBUG("H1.2 cost evaluation: chunked="
             << (chunkedCost ? std::to_string(*chunkedCost) : "N/A")
             << ", elementWise="
             << (elementWiseCost ? std::to_string(*elementWiseCost) : "N/A"));

  // Neither option has a computable cost
  if (!chunkedCost && !elementWiseCost) {
    // Optimistic path: if canChunked is true, the code structure indicates
    // chunked intent (e.g., loop + chunk size deps). Honor that intent even
    // when we can't compute exact cost (dynamic chunk size / array size).
    if (ctx.canChunked) {
      PartitioningDecision decision;
      decision.mode = RewriterMode::Chunked;
      decision.outerRank = 1;
      decision.innerRank = ctx.memrefRank; // Chunked preserves conceptual rank
      decision.rationale = "H1.2: Optimistic chunking (dynamic sizes)";
      ARTS_DEBUG("H1.2 applied (optimistic): " << decision.rationale);
      return decision;
    }
    // Similarly for element-wise
    if (ctx.canElementWise && ctx.pinnedDimCount > 0) {
      PartitioningDecision decision;
      decision.mode = RewriterMode::ElementWise;
      decision.outerRank = ctx.pinnedDimCount;
      decision.innerRank = ctx.memrefRank - ctx.pinnedDimCount;
      decision.rationale = "H1.2: Optimistic element-wise (dynamic sizes)";
      ARTS_DEBUG("H1.2 applied (optimistic): " << decision.rationale);
      return decision;
    }
    return std::nullopt;
  }

  // Prefer chunked if available and has better/equal cost
  bool preferChunked =
      chunkedCost.has_value() &&
      (!elementWiseCost.has_value() || *chunkedCost >= *elementWiseCost);

  PartitioningDecision decision;
  if (preferChunked && ctx.canChunked) {
    decision.mode = RewriterMode::Chunked;
    decision.outerRank = 1;
    decision.innerRank = ctx.memrefRank; // Chunked preserves conceptual rank
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

  // Threshold checks (can be made configurable via HeuristicsConfig)
  constexpr int64_t kMaxChunks = 1024;
  constexpr int64_t kMinChunkBytes = 64;

  if (chunkCount > kMaxChunks) {
    ARTS_DEBUG("  H1.2 chunked rejected: chunkCount=" << chunkCount << " > "
                                                      << kMaxChunks);
    return std::nullopt;
  }

  // Assume f64 (8 bytes) for byte calculation
  int64_t chunkBytes = *ctx.chunkSize * 8;
  if (chunkBytes < kMinChunkBytes) {
    ARTS_DEBUG("  H1.2 chunked rejected: chunkBytes=" << chunkBytes << " < "
                                                      << kMinChunkBytes);
    return std::nullopt;
  }

  // Score: higher = better (fewer chunks preferred)
  int64_t score = 100 - (chunkCount / 10);
  ARTS_DEBUG("  H1.2 chunked score=" << score << " (chunkCount=" << chunkCount
                                     << ")");
  return score;
}

std::optional<int64_t> CostModelHeuristic::evaluateElementWiseCost(
    const PartitioningContext &ctx) const {
  if (!ctx.canElementWise || ctx.pinnedDimCount == 0)
    return std::nullopt;

  // Base score for element-wise
  int64_t score = 50;
  ARTS_DEBUG("  H1.2 element-wise score=" << score);
  return score;
}

//===----------------------------------------------------------------------===//
// H1.3: Access Uniformity
//===----------------------------------------------------------------------===//

std::optional<PartitioningDecision>
AccessUniformityHeuristic::evaluate(const PartitioningContext &ctx) const {
  // Non-uniform access with no fine-grained option -> coarse
  if (!ctx.isUniformAccess && !ctx.canElementWise && !ctx.canChunked) {
    PartitioningDecision decision;
    decision.mode = RewriterMode::ElementWise;
    decision.outerRank = 0;
    decision.innerRank = ctx.memrefRank; // All dims are inner for coarse
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

  // Only applies to multi-node deployments
  if (ctx.machine->getNodeCount() <= 1)
    return std::nullopt;

  PartitioningDecision decision;
  if (ctx.canChunked) {
    decision.mode = RewriterMode::Chunked;
    decision.outerRank = 1;
    decision.innerRank = ctx.memrefRank; // Chunked preserves conceptual rank
    decision.rationale =
        "H1.4: Multi-node prefers chunked for network efficiency";
  } else if (ctx.canElementWise) {
    decision.mode = RewriterMode::ElementWise;
    decision.outerRank = ctx.pinnedDimCount;
    decision.innerRank = ctx.memrefRank - ctx.pinnedDimCount;
    decision.rationale = "H1.4: Multi-node prefers fine-grained";
  } else {
    return std::nullopt;
  }

  ARTS_DEBUG("H1.4 applied: " << decision.rationale);
  return decision;
}

//===----------------------------------------------------------------------===//
// Factory Function
//===----------------------------------------------------------------------===//

llvm::SmallVector<std::unique_ptr<PartitioningHeuristic>>
mlir::arts::createDefaultPartitioningHeuristics() {
  llvm::SmallVector<std::unique_ptr<PartitioningHeuristic>> heuristics;

  // Add heuristics (will be sorted by priority in HeuristicsConfig)
  heuristics.push_back(std::make_unique<ReadOnlySingleNodeHeuristic>());
  heuristics.push_back(std::make_unique<MultiNodeHeuristic>());
  heuristics.push_back(std::make_unique<AccessUniformityHeuristic>());
  heuristics.push_back(std::make_unique<CostModelHeuristic>());

  ARTS_DEBUG("Created " << heuristics.size()
                        << " default partitioning heuristics");
  return heuristics;
}
