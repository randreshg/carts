///==========================================================================///
/// File: ArtsHeuristics.h
///
/// Compile-time heuristics for the CARTS compiler.
///
/// Heuristic Families:
///   H1: Partitioning - Decide coarse/block/element-wise allocation
///   H2: Twin-Diff    - Decide when twin-diff can be disabled
///
/// H1 Partitioning Heuristics (evaluated in priority order):
///   H1.1: Read-only single-node -> coarse
///   H1.1b: Read-only + all full-range block acquires -> coarse
///   H1.1c: Read-only stencil on single-node -> coarse
///   H1.2: Mixed access (block writes + indirect reads) -> block
///   H1.3: Stencil/indexed patterns -> stencil or block/element-wise
///   H1.4: Uniform direct access -> block
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

#ifndef ARTS_ANALYSIS_ARTSHEURISTICS_H
#define ARTS_ANALYSIS_ARTSHEURISTICS_H

#include "arts/Analysis/Graphs/Db/DbAccessPattern.h"
#include "arts/ArtsDialect.h"
#include "arts/Transforms/Datablock/DbRewriter.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/JSON.h"
#include <climits>
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

class DbAcquireNode;

/// Per-acquire decision from heuristics.
/// Used to centralize needsFullRange decisions based on canPartitionWithOffset.
struct AcquireDecision {
  bool needsFullRange = false;
  bool canContributeBlockSize = true;
  std::string rationale;
};

/// Per-acquire info for heuristic voting.
struct AcquireInfo {
  ArtsMode accessMode = ArtsMode::uninitialized;
  PartitionMode partitionMode = PartitionMode::coarse;
  bool canElementWise = false;
  bool canBlock = false;

  /// Unified partition infos from DbAcquireOp::getPartitionInfos()
  /// One entry per depend clause entry on this acquire.
  SmallVector<PartitionInfo> partitionInfos;
};

/// Context for partitioning decisions
struct PartitioningContext {
  const ArtsAbstractMachine *machine = nullptr;

  /// DB partitioning capabilities
  bool canElementWise = false, canBlock = false;
  unsigned pinnedDimCount = 0;
  ArtsMode accessMode = ArtsMode::uninitialized;
  std::optional<int64_t> totalElements, blockSize, depsPerEDT;
  bool isUniformAccess = false;
  AcquirePatternSummary accessPatterns;
  bool hasIndirectAccess = false, hasIndirectRead = false;
  bool hasIndirectWrite = false, hasDirectAccess = false;
  ///  Memref info
  unsigned memrefRank = 0;
  bool elementTypeIsMemRef = false;
  /// True if all block-capable acquires require full-range access.
  bool allBlockFullRange = false;

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

  /// Returns true if any acquire can use block partitioning.
  bool anyCanBlock() const {
    return llvm::any_of(acquires,
                        [](const AcquireInfo &a) { return a.canBlock; });
  }

  /// Returns the maximum partition dimension count across all acquires.
  /// Computed from partitionInfos for uniformity analysis in heuristics.
  unsigned maxPinnedDimCount() const {
    unsigned maxVal = 0;
    for (const auto &a : acquires) {
      for (const auto &pinfo : a.partitionInfos) {
        maxVal = std::max(maxVal, pinfo.dimCount());
      }
    }
    return maxVal;
  }

  /// Returns the minimum partition dimension count across all acquires.
  unsigned minPinnedDimCount() const {
    unsigned minVal = UINT_MAX;
    for (const auto &a : acquires) {
      for (const auto &pinfo : a.partitionInfos) {
        unsigned d = pinfo.dimCount();
        if (d > 0)
          minVal = std::min(minVal, d);
      }
    }
    return minVal == UINT_MAX ? 0 : minVal;
  }
};

/// Recommended partitioning strategy
struct PartitioningDecision {
  PartitionMode mode = PartitionMode::fine_grained;
  unsigned outerRank = 0, innerRank = 0;
  std::string rationale;

  /// General factory method for creating partitioning decisions.
  /// All specialized factories delegate to this method.
  static PartitioningDecision create(PartitionMode mode, unsigned outerRank,
                                     unsigned memrefRank,
                                     llvm::StringRef reason) {
    unsigned inner = memrefRank > outerRank ? memrefRank - outerRank : 0;
    return {mode, outerRank, inner, reason.str()};
  }

  /// Factory for coarse allocation (single datablock).
  static PartitioningDecision coarse(const PartitioningContext &ctx,
                                     llvm::StringRef reason) {
    return create(PartitionMode::coarse, 0, ctx.memrefRank, reason);
  }

  /// Factory for element-wise partitioning (one DB per element in outer dims).
  static PartitioningDecision elementWise(const PartitioningContext &ctx,
                                          unsigned outerRank,
                                          llvm::StringRef reason) {
    return create(PartitionMode::fine_grained, outerRank, ctx.memrefRank,
                  reason);
  }

  /// Factory for block partitioning (contiguous blocks along leading dims).
  /// N-D support: uses maxPinnedDimCount from context if > 0, else defaults
  /// to 1.
  static PartitioningDecision block(const PartitioningContext &ctx,
                                    llvm::StringRef reason) {
    unsigned outerRank =
        ctx.maxPinnedDimCount() > 0 ? ctx.maxPinnedDimCount() : 1;
    return create(PartitionMode::block, outerRank, ctx.memrefRank, reason);
  }

  /// Factory for block partitioning with explicit N-D support.
  static PartitioningDecision blockND(const PartitioningContext &ctx,
                                      unsigned outerRank,
                                      llvm::StringRef reason) {
    return create(PartitionMode::block, outerRank, ctx.memrefRank, reason);
  }

  /// Factory for stencil mode (block + ESD for halo handling).
  /// N-D support: uses maxPinnedDimCount from context if > 0, else defaults
  /// to 1.
  static PartitioningDecision stencil(const PartitioningContext &ctx,
                                      llvm::StringRef reason) {
    unsigned outerRank =
        ctx.maxPinnedDimCount() > 0 ? ctx.maxPinnedDimCount() : 1;
    return create(PartitionMode::stencil, outerRank, ctx.memrefRank, reason);
  }

  bool isCoarse() const { return outerRank == 0; }
  bool isFineGrained() const { return mode == PartitionMode::fine_grained; }
  bool isBlock() const {
    return mode == PartitionMode::block || mode == PartitionMode::stencil;
  }
};

/// Consolidated structure for partitioning recommendations
struct PartitioningHint {
  /// Recommended partitioning mode
  PartitionMode mode = PartitionMode::coarse;

  /// Block size for block partitioning (optional)
  std::optional<int64_t> blockSize;

  /// Factory methods
  static PartitioningHint coarse() { return {}; }

  static PartitioningHint block(std::optional<int64_t> size) {
    PartitioningHint h;
    h.mode = PartitionMode::block;
    h.blockSize = size;
    return h;
  }

  DictionaryAttr toAttribute(MLIRContext *ctx) const;
  static std::optional<PartitioningHint> fromAttribute(Attribute attr);
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

  /// Cost model thresholds for block partitioning
  static constexpr int64_t kMaxOuterDBs = 1024;
  static constexpr int64_t kMaxDepsPerEDT = 8;
  static constexpr int64_t kMinInnerBytes = 64;

  /// H1: Partitioning Mode Evaluation
  PartitioningDecision getPartitioningMode(const PartitioningContext &ctx);

  /// H1.7: Per-acquire partitioning decisions based on canPartitionWithOffset.
  /// Evaluates each acquire to determine if it needs full-range access
  /// (when partition offset doesn't match access pattern, e.g., mean[f] in
  /// batchnorm parallel over batch b).
  SmallVector<AcquireDecision>
  getAcquireDecisions(const PartitioningContext &ctx,
                      ArrayRef<DbAcquireNode *> acquireNodes,
                      ArrayRef<Value> partitionOffsets);

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

#endif // ARTS_ANALYSIS_ARTSHEURISTICS_H
