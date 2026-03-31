///==========================================================================///
/// File: PartitioningHeuristics.h
///
/// Partitioning heuristics for the CARTS compiler (H1 family).
///
/// Decides coarse/block/element-wise allocation strategy for datablocks.
///
/// H1 Partitioning Heuristics (evaluated in priority order):
///   H1.C0: Tiny read-only stencil coefficient table -> coarse
///   H1.C1: Pointer-of-pointer element type -> coarse
///   H1.C2: Read-only single-node -> coarse
///   H1.C3: Any coarse acquire consumer -> coarse
///   H1.C4: Neither element-wise nor block capable -> coarse
///   H1.B1: Indirect reads + block writes -> block
///   H1.B2: Uniform direct access -> block
///   H1.B3: Double-buffer stencil (Jacobi) -> block
///   H1.B4: Indexed access with block support -> block
///   H1.S1: Element-wise stencil -> stencil/ESD
///   H1.S2: Block-capable stencil -> stencil/ESD
///   H1.E1: Element-wise capable -> element-wise
///   H1.F:  Fallback -> coarse or fine-grained (CLI option)
///==========================================================================///

#ifndef ARTS_ANALYSIS_HEURISTICS_PARTITIONINGHEURISTICS_H
#define ARTS_ANALYSIS_HEURISTICS_PARTITIONINGHEURISTICS_H

#include "arts/Dialect.h"
#include "arts/analysis/graphs/db/DbAccessPattern.h"
#include "arts/utils/abstract_machine/AbstractMachine.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include <climits>
#include <cstdint>
#include <optional>
#include <string>

namespace mlir {

class Operation;

namespace arts {

/// Partition fallback strategy (from CLI option --partition-fallback)
enum class PartitionFallback { Coarse, FineGrained };

///===----------------------------------------------------------------------===///
/// H1: Partitioning Heuristic Types
///===----------------------------------------------------------------------===///

/// Per-acquire decision from heuristics.
/// Used to centralize needsFullRange decisions based on canPartitionWithOffset.
struct AcquireDecision {
  bool needsFullRange = false;
  bool canContributeBlockSize = true;
  std::string rationale;
};

/// Heuristic snapshot for one acquire.
///
/// DbPartitioning builds this from DbAnalysis/DbGraph facts before calling H1.
/// The heuristic layer should not need raw DbAcquireNode queries once this
/// snapshot is filled in.
struct AcquireInfo {
  ArtsMode accessMode = ArtsMode::uninitialized;
  PartitionMode partitionMode = PartitionMode::coarse;
  ArtsDepPattern depPattern = ArtsDepPattern::unknown;
  bool canElementWise = false;
  bool canBlock = false;
  bool needsFullRange = false;
  AccessPattern accessPattern = AccessPattern::Unknown;
  bool hasDistributionContract = false;
  bool partitionDimsFromPeers = false;
  bool explicitCoarseRequest = false;
  /// Compact authoritative owner-dim summary taken from the lowering contract.
  /// Peer-inferred dims are intentionally excluded: they are useful for
  /// allocation voting, but not strong enough to prove phase-local ownership.
  uint8_t ownerDimsCount = 0;
  int16_t ownerDims[4] = {-1, -1, -1, -1};

  /// Unified partition infos from DbAcquireOp::getPartitionInfos()
  /// One entry per depend clause entry on this acquire.
  SmallVector<PartitionInfo> partitionInfos;
};

/// Allocation-level heuristic context assembled by the controller pass.
///
/// DbPartitioning owns construction of this snapshot. H1 heuristics consume it
/// as read-only policy input and return a decision; they do not mutate IR.
struct PartitioningContext {
  const AbstractMachine *machine = nullptr;

  /// DB partitioning capabilities
  bool canElementWise = false, canBlock = false;
  unsigned pinnedDimCount = 0;
  ArtsMode accessMode = ArtsMode::uninitialized;
  std::optional<int64_t> totalElements, blockSize, depsPerEDT;
  /// Existing allocation facts carried from the controller.
  /// These are raw facts about the current alloc shape/state, not policy.
  std::optional<int64_t> staticElementCount;
  PartitionMode existingAllocMode = PartitionMode::coarse;
  DbAccessPattern allocAccessPattern = DbAccessPattern::unknown;
  DbMode allocDbMode = DbMode::write;
  bool isUniformAccess = false;
  AcquirePatternSummary accessPatterns;
  bool hasIndirectAccess = false, hasIndirectRead = false;
  bool hasIndirectWrite = false, hasDirectAccess = false;
  ///  Memref info
  unsigned memrefRank = 0;
  bool elementTypeIsMemRef = false;
  /// True if all block-capable acquires require full-range access.
  bool allBlockFullRange = false;
  /// Structural owner-compute preference prepared by DbPartitioning.
  /// This is not a guessed policy; it is a controller-supplied fact that an
  /// existing owner-distribution shape should stay N-D block-aligned.
  bool preferBlockND = false;
  unsigned preferredOuterRank = 0;

  /// Per-acquire info for voting
  SmallVector<AcquireInfo> acquires;

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

  /// Returns true if any later consumer explicitly requests coarse access.
  bool anyCoarseAcquire() const {
    return llvm::any_of(acquires, [](const AcquireInfo &a) {
      return a.partitionMode == PartitionMode::coarse;
    });
  }

  /// Returns true if any acquire explicitly requested coarse partitioning.
  bool anyExplicitCoarseAcquire() const {
    return llvm::any_of(
        acquires, [](const AcquireInfo &a) { return a.explicitCoarseRequest; });
  }

  /// Returns true if any acquire already carries task distribution hints and
  /// can still participate in block/stencil partitioning.
  bool hasDistributedBlockContract() const {
    return llvm::any_of(acquires, [](const AcquireInfo &a) {
      return a.hasDistributionContract && a.canBlock;
    });
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
/// Heuristic Evaluation Functions
///===----------------------------------------------------------------------===///

/// Evaluate all H1.x heuristics and return partitioning decision
PartitioningDecision evaluatePartitioningHeuristics(
    const PartitioningContext &ctx, const AbstractMachine *machine,
    PartitionFallback fallback = PartitionFallback::Coarse);

/// PartitioningHint accessors
std::optional<PartitioningHint> getPartitioningHint(Operation *op);
void setPartitioningHint(Operation *op, const PartitioningHint &hint);

/// Copy ARTS-specific metadata attributes from source to dest operation.
/// Use only for semantically equivalent rewrites; structural rewrites should
/// not clone source loop metadata onto a new iteration space.
void copyArtsMetadataAttrs(Operation *source, Operation *dest);

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_HEURISTICS_PARTITIONINGHEURISTICS_H
