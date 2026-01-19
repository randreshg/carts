///==========================================================================///
/// File: HeuristicsConfig.h
///
/// Compile-time heuristics for the CARTS compiler.
///
/// Heuristic Families:
///   H1: Partitioning - Decide coarse/chunked/element-wise allocation
///   H2: Twin-Diff    - Decide when twin-diff can be disabled
///
/// H1 Partitioning Heuristics (evaluated in priority order):
///   H1.1: Read-only single-node -> coarse
///   H1.2: Mixed access (chunked writes + indirect reads) -> chunked
///   H1.3: Stencil/indexed patterns -> stencil or element-wise
///   H1.4: Uniform direct access -> chunked
///   H1.5: Multi-node -> fine-grained for network efficiency
///   H1.6: Non-uniform access -> coarse
///
/// H2 Twin-Diff Heuristics:
///   Determines when twin-diff can be safely disabled based on provable
///   non-overlap of datablock accesses (currently globally disabled).
///
/// Note: Multi-rank architectural heuristics (H1-H6 in multi_rank docs) are
/// separate guidelines for distributed optimization, not implemented here.
///==========================================================================///

#ifndef ARTS_ANALYSIS_HEURISTICSCONFIG_H
#define ARTS_ANALYSIS_HEURISTICSCONFIG_H

#include "arts/Analysis/Graphs/Db/DbAccessPattern.h"
#include "arts/ArtsDialect.h"
#include "arts/Transforms/Datablock/DbRewriter.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/JSON.h"
#include <optional>
#include <string>

namespace mlir {

class Operation;

namespace arts {

/// Partition fallback strategy (from CLI option --partition-fallback)
enum class PartitionFallback { Coarse, FineGrained };

class IdRegistry;

///===----------------------------------------------------------------------===///
/// H1: Partitioning Heuristic Types
///===----------------------------------------------------------------------===///

/// Per-acquire info for heuristic voting.
struct AcquireInfo {
  ArtsMode accessMode = ArtsMode::uninitialized;
  bool canElementWise = false;
  bool canChunked = false;
  unsigned pinnedDimCount = 0;
};

/// Context for partitioning decisions
struct PartitioningContext {
  const ArtsAbstractMachine *machine = nullptr;

  /// DB partitioning capabilities
  bool canElementWise = false, canChunked = false;
  unsigned pinnedDimCount = 0;
  ArtsMode accessMode = ArtsMode::uninitialized;
  std::optional<int64_t> totalElements, chunkSize, depsPerEDT;
  bool isUniformAccess = false;
  AcquirePatternSummary accessPatterns;
  bool hasIndirectAccess = false, hasIndirectRead = false;
  bool hasIndirectWrite = false, hasDirectAccess = false;
  ///  Memref info
  unsigned memrefRank = 0;
  bool elementTypeIsMemRef = false;

  /// Per-acquire info for voting
  SmallVector<AcquireInfo> acquires;

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

/// Recommended partitioning strategy
struct PartitioningDecision {
  RewriterMode mode = RewriterMode::ElementWise;
  unsigned outerRank = 0, innerRank = 0;
  std::string rationale;

  /// Factory for coarse allocation (single datablock).
  static PartitioningDecision coarse(const PartitioningContext &ctx,
                                     llvm::StringRef reason) {
    return {RewriterMode::ElementWise, 0, ctx.memrefRank, reason.str()};
  }

  /// Factory for element-wise partitioning (one DB per element in outer dims).
  static PartitioningDecision elementWise(const PartitioningContext &ctx,
                                          unsigned outerRank,
                                          llvm::StringRef reason) {
    unsigned inner =
        ctx.memrefRank > outerRank ? ctx.memrefRank - outerRank : 0;
    return {RewriterMode::ElementWise, outerRank, inner, reason.str()};
  }

  /// Factory for chunked partitioning (contiguous chunks along leading dim).
  static PartitioningDecision chunked(const PartitioningContext &ctx,
                                      llvm::StringRef reason) {
    return {RewriterMode::Chunked, 1, ctx.memrefRank, reason.str()};
  }

  /// Factory for stencil mode (chunked + ESD for halo handling).
  static PartitioningDecision stencil(const PartitioningContext &ctx,
                                      llvm::StringRef reason) {
    unsigned inner = ctx.memrefRank > 0 ? ctx.memrefRank - 1 : 0;
    return {RewriterMode::Stencil, 1, inner, reason.str()};
  }

  bool isCoarse() const { return outerRank == 0; }
  bool isFineGrained() const { return outerRank > 0; }
  bool isChunked() const {
    return mode == RewriterMode::Chunked || mode == RewriterMode::Stencil;
  }
};

///===----------------------------------------------------------------------===///
/// H2: Twin-Diff Heuristic Types
///===----------------------------------------------------------------------===///

/// Proof method for why twin-diff can be disabled.
enum class TwinDiffProof {
  None,
  IndexedControl,
  PartitionSuccess,
  AliasAnalysis
};

/// Context for twin-diff decisions (H2 heuristic).
struct TwinDiffContext {
  TwinDiffProof proof = TwinDiffProof::None;
  bool isCoarseAllocation = false;
  Operation *op = nullptr;
};

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

  /// Twin-diff heuristic evaluation
  bool shouldUseTwinDiff(const TwinDiffContext &context);

  /// Cost model thresholds for chunked partitioning
  static constexpr int64_t kMaxOuterDBs = 1024;
  static constexpr int64_t kMaxDepsPerEDT = 8;
  static constexpr int64_t kMinInnerBytes = 64;

  /// H1: Partitioning Mode Evaluation
  PartitioningDecision getPartitioningMode(const PartitioningContext &ctx);

  /// Decision recording for diagnostics
  void recordDecision(llvm::StringRef heuristic, bool applied,
                      llvm::StringRef rationale, Operation *op,
                      const llvm::StringMap<int64_t> &inputs = {});
  llvm::ArrayRef<HeuristicDecision> getDecisions() const;
  void exportDecisionsToJson(llvm::json::OStream &J) const;

private:
  const mlir::arts::ArtsAbstractMachine &machine;
  IdRegistry &idRegistry;
  llvm::SmallVector<HeuristicDecision> decisions;
  PartitionFallback partitionFallback;
};

///===----------------------------------------------------------------------===///
/// Heuristic Evaluation Function
///===----------------------------------------------------------------------===///

/// Evaluate all H1.x heuristics and return partitioning decision
PartitioningDecision evaluatePartitioningHeuristics(
    const PartitioningContext &ctx, const ArtsAbstractMachine *machine,
    PartitionFallback fallback = PartitionFallback::Coarse);

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_HEURISTICSCONFIG_H
