///==========================================================================///
/// File: PartitioningHeuristics.cpp
///
/// H1 partitioning heuristics evaluation and PartitioningHint utilities.
///==========================================================================///

#include "arts/dialect/core/Analysis/heuristics/PartitioningHeuristics.h"
#include "arts/Dialect.h"
#include "arts/dialect/core/Analysis/heuristics/HeuristicUtils.h"
#include "arts/utils/ValueAnalysis.h"
#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include "mlir/IR/BuiltinAttributes.h"

ARTS_DEBUG_SETUP(partitioning_heuristics)

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool
isTinyReadOnlyStencilCoefficientTable(const PartitioningContext &ctx) {
  return ctx.existingAllocMode == PartitionMode::coarse &&
         ctx.allocAccessPattern == DbAccessPattern::stencil &&
         ctx.accessMode == ArtsMode::in && ctx.allocDbMode == DbMode::read &&
         ctx.staticElementCount && *ctx.staticElementCount > 0 &&
         *ctx.staticElementCount <= 8;
}

static bool hasJacobiLikeStencilContract(const PartitioningContext &ctx) {
  return llvm::any_of(ctx.acquires, [](const AcquireInfo &info) {
    return info.depPattern == ArtsDepPattern::jacobi_alternating_buffers;
  });
}

static bool
hasNonLeadingOrMultiDimOwnerContract(const PartitioningContext &ctx) {
  return llvm::any_of(ctx.acquires, [](const AcquireInfo &info) {
    /// Ownership conflicts remain relevant even if DbPartitioning already
    /// disabled block capability on the specific acquire. A dropped block vote
    /// still tells us that some phase expects a non-leading/N-D owner layout
    /// the allocation cannot safely satisfy together with other phases.
    if (!info.hasDistributionContract || info.ownerDimsCount == 0)
      return false;
    if (info.ownerDimsCount > 1)
      return true;
    return info.ownerDims[0] > 0;
  });
}

static bool
hasNonLeadingOrMultiDimOwnerWriterContract(const PartitioningContext &ctx) {
  return llvm::any_of(ctx.acquires, [](const AcquireInfo &info) {
    if (!info.hasDistributionContract || info.ownerDimsCount == 0)
      return false;
    if (info.accessMode == ArtsMode::in)
      return false;
    if (info.ownerDimsCount > 1)
      return true;
    return info.ownerDims[0] > 0;
  });
}

static bool
hasReadOnlyNonLeadingOrMultiDimOwnerConflict(const PartitioningContext &ctx) {
  bool sawReadOnlyNonLeadingContract = false;

  for (const AcquireInfo &info : ctx.acquires) {
    if (!info.hasDistributionContract || info.ownerDimsCount == 0)
      continue;

    bool isNonLeadingOrMultiDim =
        info.ownerDimsCount > 1 || info.ownerDims[0] > 0;
    if (!isNonLeadingOrMultiDim)
      continue;

    if (info.accessMode != ArtsMode::in)
      return false;

    sawReadOnlyNonLeadingContract = true;
  }

  return sawReadOnlyNonLeadingContract;
}

/// Upgrade mixed-orientation read-only-after-init inputs to a 2-D block plan
/// when a one-time non-leading writer feeds leading-dimension stencil readers.
/// This matches pointer-backed setup phases such as poisson-for, where the
/// producer writes by columns once but the hot repeated consumers read by row.
static bool shouldUseMixedOrientationBlockNDForReadOnlyAfterInit(
    const PartitioningContext &ctx) {
  if (!ctx.readOnlyAfterInit || !ctx.canBlock ||
      !ctx.accessPatterns.hasStencil || ctx.memrefRank < 2)
    return false;

  bool sawNonLeadingWriter = false;
  bool sawLeadingReader = false;

  for (const AcquireInfo &info : ctx.acquires) {
    if (!info.hasDistributionContract || info.ownerDimsCount == 0)
      continue;

    bool isLeadingOwner = info.ownerDimsCount == 1 && info.ownerDims[0] == 0;
    bool isNonLeadingOrMultiDim =
        info.ownerDimsCount > 1 || info.ownerDims[0] > 0;

    if (info.accessMode == ArtsMode::in) {
      if (!isLeadingOwner || !info.canBlock)
        return false;
      sawLeadingReader = true;
      continue;
    }

    if (!isNonLeadingOrMultiDim)
      return false;
    sawNonLeadingWriter = true;
  }

  return sawNonLeadingWriter && sawLeadingReader;
}

/// Detect conflicting owner dimensions across different EDT families.
/// When different phases access the same DB with different owner_dims (e.g.,
/// row access vs column access), block partitioning hurts both phases.
static bool hasConflictingOwnerDims(const PartitioningContext &ctx) {
  llvm::SmallDenseSet<uint32_t, 4> ownerDimSets;

  for (const AcquireInfo &acq : ctx.acquires) {
    if (acq.ownerDimsCount == 0)
      continue;

    // Encode owner dims as a unique hash
    uint32_t hash = 0;
    for (uint8_t i = 0; i < acq.ownerDimsCount; ++i) {
      hash = hash * 31 + static_cast<uint32_t>(acq.ownerDims[i]);
    }
    ownerDimSets.insert(hash);
  }

  // Conflict if more than one unique owner_dims pattern
  return ownerDimSets.size() > 1;
}

/// Detect a single-node producer/consumer mismatch where one phase writes
/// block-local slices but a later read phase widens back to a full-range view.
/// In that shape the block allocation keeps the write-side overhead while
/// losing locality on the consumer side, so coarse is usually better.
static bool
hasLocalProducerAndFullRangeReadConsumer(const PartitioningContext &ctx) {
  bool hasLocalWriter = false;
  bool hasFullRangeReader = false;

  for (const AcquireInfo &acq : ctx.acquires) {
    if (!acq.canBlock)
      continue;

    bool isWriter =
        acq.accessMode == ArtsMode::out || acq.accessMode == ArtsMode::inout;
    bool isReader = acq.accessMode == ArtsMode::in;

    if (isWriter && !acq.needsFullRange)
      hasLocalWriter = true;
    if (isReader && acq.needsFullRange)
      hasFullRangeReader = true;
  }

  return hasLocalWriter && hasFullRangeReader;
}

/// Preserve blocked layout for small 1-D producer/readback vectors.
/// ATAX/BICG-style scratch vectors still benefit from block-local writes even
/// when a later direct read phase widens back to the full range; the fan-in is
/// modest, but collapsing to coarse removes producer locality entirely.
static bool
shouldPreserveBlockForSmallVectorReadback(const PartitioningContext &ctx) {
  if (ctx.memrefRank != 1 || !ctx.staticElementCount)
    return false;

  int64_t totalElements = *ctx.staticElementCount;
  std::optional<int64_t> blockSize = ctx.blockSize;
  if (!blockSize) {
    for (const AcquireInfo &info : ctx.acquires) {
      for (const PartitionInfo &pinfo : info.partitionInfos) {
        if (!usesBlockLayout(pinfo.mode) || pinfo.sizes.empty())
          continue;
        int64_t constantSize = 0;
        if (!ValueAnalysis::getConstantIndex(pinfo.sizes.front(),
                                             constantSize) ||
            constantSize <= 0)
          continue;
        if (!blockSize)
          blockSize = constantSize;
        else
          blockSize = std::min(*blockSize, constantSize);
      }
    }
  }

  if (totalElements <= 0 || !blockSize || *blockSize <= 0)
    return false;

  /// Keep the carve-out narrow: only small vectors with a bounded block fan-in
  /// should bypass H1.C3b.
  constexpr int64_t kMaxElements = 65536;
  constexpr int64_t kMaxBlocks = 64;

  int64_t blockCount = (totalElements + *blockSize - 1) / *blockSize;
  return totalElements <= kMaxElements && blockCount > 1 &&
         blockCount <= kMaxBlocks;
}

} // namespace

///===----------------------------------------------------------------------===///
/// H1: Partitioning Heuristics Evaluation
///
/// Organization: Heuristics are grouped by partition mode decision:
///   - Coarse:       H1.C1-H1.C4
///   - Block:        H1.B1-H1.B4
///   - Stencil/ESD:  H1.S1-H1.S3
///   - Element-Wise: H1.E1-H1.E2
///   - Fallback:     Canonical coarse when no heuristic matches
///===----------------------------------------------------------------------===///

PartitioningDecision
mlir::arts::evaluatePartitioningHeuristics(const PartitioningContext &ctx,
                                           const AbstractMachine *machine) {
  ARTS_DEBUG("evaluatePartitioningHeuristics: canElementWise="
             << ctx.canElementWise << ", canBlock=" << ctx.canBlock
             << ", pinnedDimCount=" << ctx.pinnedDimCount
             << ", accessMode=" << static_cast<int>(ctx.accessMode)
             << ", allBlockFullRange=" << ctx.allBlockFullRange);

  const auto &patterns = ctx.accessPatterns;

  bool isReadOnly = !ctx.acquires.empty() ? ctx.allReadOnly()
                                          : (ctx.accessMode == ArtsMode::in);
  bool isSingleNode = (machine && machine->isSingleNode());
  bool hasExplicitFineGrained =
      llvm::any_of(ctx.acquires, [](const AcquireInfo &info) {
        if (info.partitionMode == PartitionMode::fine_grained)
          return true;
        for (const auto &pinfo : info.partitionInfos) {
          if (pinfo.isFineGrained() && !pinfo.indices.empty())
            return true;
        }
        return false;
      });
  bool hasTrustedStencilAcquire =
      llvm::any_of(ctx.acquires, [](const AcquireInfo &info) {
        return info.accessPattern == AccessPattern::Stencil &&
               !info.partitionDimsFromPeers;
      });
  if (!hasTrustedStencilAcquire) {
    hasTrustedStencilAcquire =
        llvm::any_of(ctx.acquires, [](const AcquireInfo &info) {
          return info.accessMode == ArtsMode::in &&
                 isStencilFamilyDepPattern(info.depPattern);
        });
  }
  bool hasJacobiStencil = hasJacobiLikeStencilContract(ctx);
  bool hasNonLeadingOwnerContract = hasNonLeadingOrMultiDimOwnerContract(ctx);
  bool preserveReadOnlyStencilOwnership =
      hasNonLeadingOwnerContract || ctx.memrefRank == 1 || ctx.memrefRank >= 3;

  ///===--------------------------------------------------------------------===///
  /// Coarse Partitioning Heuristics (H1.C*)
  ///===--------------------------------------------------------------------===///

  /// H1.C0: Tiny read-only stencil coefficient table -> Coarse
  /// Rationale: Small constant stencil tables are effectively scalar metadata.
  /// Repartitioning them can skew coefficient indexing and add dependency
  /// plumbing without improving locality.
  if (isTinyReadOnlyStencilCoefficientTable(ctx)) {
    ARTS_DEBUG("H1.C0 applied: Tiny read-only stencil coefficient table "
               "prefers coarse");
    return PartitioningDecision::coarse(
        ctx, "H1.C0: Tiny read-only stencil coefficient table prefers coarse");
  }

  /// H1.C1: Read-only single-node without fine-grained support → Coarse
  if (isSingleNode && isReadOnly && !ctx.canBlock && !ctx.canElementWise) {
    ARTS_DEBUG("H1.C1 applied: Read-only single-node prefers coarse");
    return PartitioningDecision::coarse(
        ctx, "H1.C1: Read-only single-node without partitioning support");
  }

  /// H1.C2: Explicit coarse acquire on single-node → Coarse
  /// Rationale: A later whole-array consumer outweighs any earlier parallel
  /// producer on a single node. Preserving coarse allocation avoids mixed-mode
  /// block-localization for values fundamentally consumed as single inputs.
  /// Exception: Defer to H1.B3 for stencil patterns (e.g., Jacobi) where block
  /// partitioning enables efficient halo exchange despite coarse output writes.
  ARTS_DEBUG("H1.C2 check: isSingleNode="
             << isSingleNode
             << ", anyExplicitCoarseAcquire=" << ctx.anyExplicitCoarseAcquire()
             << ", hasStencil=" << patterns.hasStencil);
  if (isSingleNode && ctx.anyExplicitCoarseAcquire() && !patterns.hasStencil &&
      !ctx.canBlock && !ctx.canElementWise) {
    ARTS_DEBUG("H1.C2 applied: Single-node coarse acquire prefers coarse when "
               "no partitioned plan is feasible");
    return PartitioningDecision::coarse(
        ctx, "H1.C2: Single-node explicit coarse acquire with no partitioning "
             "support");
  } else if (isSingleNode && ctx.anyExplicitCoarseAcquire() &&
             patterns.hasStencil) {
    ARTS_DEBUG("H1.C2 skipped: Stencil pattern detected, deferring to H1.B3");
  } else if (isSingleNode && ctx.anyExplicitCoarseAcquire() &&
             (ctx.canBlock || ctx.canElementWise)) {
    ARTS_DEBUG("H1.C2 skipped: explicit coarse hint overridden by available "
               "partitioned plan");
  }

  /// H1.C3: Read-only full-range block acquires → Coarse
  /// Rationale: When every read acquire spans all blocks, block partitioning
  /// gives no locality benefit and can inflate dependency wiring.
  /// Exception: Keep block on multi-node runs or when stencil semantics are
  /// explicit and block layout still carries ownership value even with
  /// full-range acquires (for example cross-dimension stencil cases).
  /// A generic lowering/distribution contract is not enough to preserve block
  /// on a single node once every block-capable acquire widened to full-range;
  /// at that point the contract no longer buys locality for read-only inputs.
  ///
  /// H1.C3a: Conflicting owner dims on single-node → Coarse
  /// When different EDT families access the same DB with incompatible
  /// owner_dims (for example row ownership in one phase and column ownership
  /// in another), block partitioning cannot satisfy both layouts at once.
  /// If the conflict involves a non-leading or N-D owner contract, later
  /// block rewrites often fall back to full-range views or lose the mapping
  /// proof entirely. In that shape coarse is safer and usually faster.
  ///
  /// Guard: Preserve Jacobi-style stencil layouts, which legitimately carry
  /// block ownership across alternating buffer phases.
  bool useMixedOrientationBlockND =
      shouldUseMixedOrientationBlockNDForReadOnlyAfterInit(ctx);
  if (hasConflictingOwnerDims(ctx) && isSingleNode &&
      hasNonLeadingOrMultiDimOwnerWriterContract(ctx) &&
      useMixedOrientationBlockND) {
    ARTS_DEBUG("H1.C3a redirected: mixed-orientation read_only_after_init "
               "input uses 2-D block layout");
    return PartitioningDecision::blockND(
        ctx, /*outerRank=*/2,
        "H1.B0: Mixed-orientation read_only_after_init input uses 2-D "
        "block layout");
  }
  if (hasConflictingOwnerDims(ctx) && isSingleNode &&
      hasNonLeadingOrMultiDimOwnerWriterContract(ctx) && !hasJacobiStencil &&
      !useMixedOrientationBlockND) {
    ARTS_DEBUG("H1.C3a applied: Conflicting owner_dims "
               "with non-leading/N-D contract -> coarse");
    return PartitioningDecision::coarse(
        ctx, "H1.C3a: Conflicting owner_dims with non-leading/N-D ownership "
             "prefer coarse");
  }

  bool preserveStencilBlockForReadOnlyFullRange =
      (patterns.hasStencil || hasTrustedStencilAcquire) &&
      !patterns.hasIndexed && preserveReadOnlyStencilOwnership;
  bool preserveBlockForReadOnlyFullRange =
      ctx.canBlock &&
      (!isSingleNode || preserveStencilBlockForReadOnlyFullRange);
  if (isReadOnly && ctx.allBlockFullRange &&
      !preserveBlockForReadOnlyFullRange) {
    if (!ctx.canBlock && ctx.canElementWise) {
      unsigned outerRank = ctx.minPinnedDimCount();
      outerRank = outerRank > 0 ? outerRank : 1;
      ARTS_DEBUG("H1.C3 applied: Read-only full-range without block support, "
                 "using element-wise");
      return PartitioningDecision::elementWise(
          ctx, outerRank,
          "H1.C3: Read-only full-range without block support uses "
          "element-wise");
    }
    ARTS_DEBUG("H1.C3 applied: Read-only full-range acquires prefer coarse");
    return PartitioningDecision::coarse(
        ctx, isSingleNode
                 ? "H1.C3: Read-only full-range on single-node prefers coarse"
                 : "H1.C3: Read-only full-range on multi-node prefers coarse");
  } else if (isReadOnly && ctx.allBlockFullRange &&
             preserveBlockForReadOnlyFullRange) {
    ARTS_DEBUG(
        "H1.C3 skipped: stencil/distribution semantics keep block layout for "
        "read-only full-range acquires");
  }

  /// H1.C3b: Local write producer + full-range read consumer on single-node
  /// -> Coarse
  /// Rationale: a blocked allocation no longer buys locality once the read
  /// phase widens to full-range, but it still keeps the per-block dependency
  /// and DB-management overhead from the producer phase.
  if (isSingleNode && ctx.canBlock && ctx.hasDirectAccess &&
      !ctx.hasIndirectAccess && patterns.hasUniform && !patterns.hasStencil &&
      !hasReadOnlyNonLeadingOrMultiDimOwnerConflict(ctx) &&
      hasLocalProducerAndFullRangeReadConsumer(ctx)) {
    if (shouldPreserveBlockForSmallVectorReadback(ctx)) {
      ARTS_DEBUG("H1.C3b skipped: small 1-D producer/readback vector keeps "
                 "block layout");
    } else {
      ARTS_DEBUG("H1.C3b applied: Local producer + full-range read consumer "
                 "prefers coarse");
      return PartitioningDecision::coarse(
          ctx, "H1.C3b: Single-node full-range read consumer outweighs local "
               "block producer");
    }
  }

  /// H1.C4: Read-only stencil on single-node → Coarse (when stencil is on
  /// partition dim)
  /// Rationale: Avoid halo exchange overhead on single-node where shared
  /// memory access is more efficient than block partitioning.
  /// Exception: When block partitioning is already viable (canBlock), the
  /// stencil is on a non-partition dimension (e.g., specfem3d: stencil on
  /// i-dim, partition on k-dim). In that case, block partitioning is safe
  /// and avoids the NUMA penalty of a single coarse allocation.
  if (patterns.hasStencil && isSingleNode && isReadOnly &&
      (!ctx.canBlock || (!ctx.allBlockFullRange && !hasJacobiStencil &&
                         !preserveReadOnlyStencilOwnership))) {
    if (ctx.canElementWise) {
      unsigned outerRank = ctx.minPinnedDimCount();
      outerRank = outerRank > 0 ? outerRank : 1;
      ARTS_DEBUG("H1.C4 applied: Read-only stencil on single-node without "
                 "block support, using element-wise");
      return PartitioningDecision::elementWise(
          ctx, outerRank,
          "H1.C4: Read-only stencil on single-node without block support uses "
          "element-wise");
    }
    ARTS_DEBUG("H1.C4 applied: Read-only same-dimension stencil on "
               "single-node -> coarse");
    return PartitioningDecision::coarse(ctx, "H1.C4: Read-only same-dimension "
                                             "stencil on single-node prefers "
                                             "coarse");
  } else if (patterns.hasStencil && isSingleNode && isReadOnly &&
             (ctx.canBlock || ctx.hasDistributedBlockContract())) {
    ARTS_DEBUG("H1.C4 skipped: explicit Jacobi/cross-dim/full-range stencil "
               "block layout is safe");
  }

  /// H1.C5: Non-uniform access without partitioning support → Coarse
  if (!ctx.isUniformAccess && !ctx.canElementWise && !ctx.canBlock) {
    ARTS_DEBUG("H1.C5 applied: Non-uniform access prefers coarse");
    return PartitioningDecision::coarse(
        ctx, "H1.C5: Non-uniform access without partitioning support");
  }

  ///===--------------------------------------------------------------------===///
  /// Block Partitioning Heuristics (H1.B*)
  ///===--------------------------------------------------------------------===///

  /// H1.B1: Mixed access (block writes + indirect reads) → Block
  if (ctx.canBlock && ctx.hasIndirectRead && !ctx.hasIndirectWrite &&
      ctx.hasDirectAccess) {
    ARTS_DEBUG("H1.B1 applied: Mixed block/indirect access");
    return PartitioningDecision::block(
        ctx, "H1.B1: Mixed access (block writes + full-range indirect reads)");
  }

  /// H1.B2: Mixed direct + indirect writes → Block
  if (ctx.canBlock && ctx.hasIndirectWrite && ctx.hasDirectAccess) {
    ARTS_DEBUG("H1.B2 applied: Mixed direct+indirect writes");
    return PartitioningDecision::block(
        ctx, "H1.B2: Mixed direct+indirect writes with full-range acquires");
  }

  if (patterns.hasStencil && hasExplicitFineGrained) {
    unsigned outerRank = ctx.minPinnedDimCount();
    outerRank = outerRank > 0 ? outerRank : 1;
    ARTS_DEBUG(
        "H1.B3 skipped: explicit fine-grained hints prefer element-wise");
    return PartitioningDecision::elementWise(
        ctx, outerRank,
        "H1.E1: Explicit fine-grained stencil hints prefer element-wise");
  }

  /// H1.B3: Double-buffer stencil (Jacobi-style) → Block
  /// Pattern: Stencil reads (mode=in) + uniform writes (mode=out)
  /// Rationale: Write phases don't need halo exchange, only read phases do.
  /// Enables block partitioning for writes while preserving locality for reads.
  ARTS_DEBUG("H1.B3 check: hasStencil=" << patterns.hasStencil
                                        << ", canBlock=" << ctx.canBlock);
  if (patterns.hasStencil && ctx.canBlock) {
    bool hasJacobiDepPattern =
        llvm::any_of(ctx.acquires, [](const AcquireInfo &a) {
          return a.depPattern == ArtsDepPattern::jacobi_alternating_buffers;
        });
    bool hasWavefrontDepPattern =
        llvm::any_of(ctx.acquires, [](const AcquireInfo &a) {
          return a.depPattern == ArtsDepPattern::wavefront_2d;
        });
    bool hasStencilReads = llvm::any_of(ctx.acquires, [](const AcquireInfo &a) {
      return (a.accessMode == ArtsMode::in) &&
             (a.accessPattern == AccessPattern::Stencil) &&
             !a.partitionDimsFromPeers;
    });
    if (!hasStencilReads && hasJacobiDepPattern) {
      hasStencilReads = llvm::any_of(ctx.acquires, [](const AcquireInfo &a) {
        return a.accessMode == ArtsMode::in &&
               a.depPattern == ArtsDepPattern::jacobi_alternating_buffers;
      });
    }
    bool hasWriteAcquires =
        llvm::any_of(ctx.acquires, [](const AcquireInfo &a) {
          return (a.accessMode == ArtsMode::out ||
                  a.accessMode == ArtsMode::inout);
        });
    bool allWritesUniform =
        llvm::all_of(ctx.acquires, [](const AcquireInfo &a) {
          bool isWrite = (a.accessMode == ArtsMode::out ||
                          a.accessMode == ArtsMode::inout);
          return !isWrite || (a.accessPattern == AccessPattern::Uniform ||
                              a.accessPattern == AccessPattern::Unknown);
        });

    ARTS_DEBUG("  hasStencilReads="
               << hasStencilReads << ", hasWriteAcquires=" << hasWriteAcquires
               << ", allWritesUniform=" << allWritesUniform
               << ", hasWavefrontDepPattern=" << hasWavefrontDepPattern);

    if (hasWavefrontDepPattern) {
      ARTS_DEBUG("H1.B3 skipped: wavefront_2d stencil should use stencil "
                 "mode, not Jacobi block mode");
    } else if (hasStencilReads && hasWriteAcquires && allWritesUniform) {
      ARTS_DEBUG("H1.B3 applied: Double-buffer stencil -> block");
      return PartitioningDecision::block(
          ctx, "H1.B3: Double-buffer stencil (Jacobi) with uniform writes");
    } else if (hasStencilReads && isReadOnly && ctx.allBlockFullRange) {
      /// H1.B3b: Read-only stencil with full-range acquires → Block
      /// Pattern: Stencil on non-partition dimension (e.g., specfem3d: stencil
      /// on i-dim, partition on k-dim). Block partitioning is structurally
      /// possible but acquires need full-range because stencil detection can't
      /// prove the partition dim is independent of the stencil dim.
      /// Rationale: Block allocation distributes memory across NUMA nodes and
      /// enables parallel DB creation. Full-range acquires are correct (each
      /// EDT reads all blocks) — the benefit is in allocation placement, not
      /// access restriction.
      ARTS_DEBUG(
          "H1.B3b applied: RO stencil with full-range -> block for NUMA");
      return PartitioningDecision::block(
          ctx, "H1.B3b: Read-only cross-dim stencil prefers block for NUMA");
    } else {
      ARTS_DEBUG("H1.B3 not matched - conditions not met");
    }
  }

  /// H1.B4: Indexed access with block support → Block
  if (patterns.hasIndexed && ctx.canBlock) {
    ARTS_DEBUG("H1.B4 applied: Indexed access with block support");
    return PartitioningDecision::block(
        ctx, "H1.B4: Indexed access prefers block when supported");
  }

  /// H1.B5: Uniform direct access → Block
  /// Rationale: Standard data-parallel pattern with direct 1:1 index mapping.
  bool blockSizeFits = !ctx.totalElements || !ctx.blockSize ||
                       *ctx.totalElements >= *ctx.blockSize;
  if (ctx.canBlock && ctx.hasDirectAccess && !ctx.hasIndirectAccess &&
      patterns.hasUniform && !ctx.elementTypeIsMemRef && blockSizeFits) {
    ARTS_DEBUG("H1.B5 applied: Uniform direct access prefers block");
    return PartitioningDecision::block(
        ctx, "H1.B5: Uniform direct access prefers block");
  }

  /// H1.B6: Multi-node with block support → Block
  /// Rationale: Block partitioning reduces network communication overhead
  /// compared to fine-grained partitioning in distributed environments.
  if (machine && machine->isDistributed()) {
    bool canDoBlock = ctx.canBlock || ctx.anyCanBlock();
    if (canDoBlock) {
      ARTS_DEBUG("H1.B6 applied: Multi-node prefers block");
      return PartitioningDecision::block(
          ctx, "H1.B6: Multi-node prefers block for network efficiency");
    }
  }

  ///===--------------------------------------------------------------------===///
  /// Stencil/ESD Partitioning Heuristics (H1.S*)
  ///===--------------------------------------------------------------------===///

  if (patterns.hasStencil) {
    if (!hasTrustedStencilAcquire && ctx.canBlock) {
      ARTS_DEBUG("H1.S0 applied: peer-inferred dims keep block mode");
      return PartitioningDecision::block(
          ctx, "H1.S0: Peer-inferred partition dims keep block mode");
    }

    /// H1.S1: Stencil unsupported for block → Element-wise fallback
    if (!ctx.canBlock || ctx.hasIndirectAccess) {
      ARTS_DEBUG(
          "H1.S1 applied: Stencil unsupported for block -> element-wise");
      return PartitioningDecision::elementWise(
          ctx, 1,
          "H1.S1: Stencil unsupported for block, fallback to element-wise");
    }

    /// H1.S2: Standard in-place stencil → ESD mode
    /// Default for stencil patterns that don't match other specializations.
    ARTS_DEBUG("H1.S2 applied: Standard stencil -> ESD mode");
    return PartitioningDecision::stencil(
        ctx, patterns.hasUniform
                 ? "H1.S2: Mixed (uniform+stencil) uses Stencil mode"
                 : "H1.S2: Pure stencil uses ESD mode");
  }

  ///===--------------------------------------------------------------------===///
  /// Element-Wise Partitioning Heuristics (H1.E*)
  ///===--------------------------------------------------------------------===///

  /// H1.E1: Explicit fine-grained partition hints → Element-wise
  if (hasExplicitFineGrained) {
    unsigned outerRank = ctx.minPinnedDimCount();
    outerRank = outerRank > 0 ? outerRank : 1;
    ARTS_DEBUG("H1.E1 applied: Explicit fine-grained hints -> element-wise");
    return PartitioningDecision::elementWise(
        ctx, outerRank, "H1.E1: Explicit fine-grained partition hints");
  }

  /// H1.E2: Indexed access without block support → Element-wise
  if (patterns.hasIndexed) {
    unsigned minDim = ctx.minPinnedDimCount();
    unsigned maxDim = ctx.maxPinnedDimCount();
    unsigned outerRank = minDim > 0 ? minDim : 1;

    if (minDim != maxDim && minDim > 0 && maxDim > 0) {
      ARTS_DEBUG("H1.E2: Non-uniform partition indices (min="
                 << minDim << ", max=" << maxDim << "), using min");
    }
    ARTS_DEBUG("H1.E2 applied: Indexed access requires element-wise (outerRank="
               << outerRank << ")");
    return PartitioningDecision::elementWise(
        ctx, outerRank, "H1.E2: Indexed access requires element-wise");
  }

  /// H1.E3: Multi-node with element-wise support → Element-wise
  if (machine && machine->isDistributed()) {
    bool canDoElementWise = ctx.canElementWise || ctx.anyCanElementWise();
    if (canDoElementWise) {
      ARTS_DEBUG("H1.E3 applied: Multi-node prefers fine-grained");
      unsigned outerRank = ctx.minPinnedDimCount();
      outerRank = outerRank > 0 ? outerRank : 1;
      return PartitioningDecision::elementWise(
          ctx, outerRank, "H1.E3: Multi-node prefers fine-grained");
    }
  }

  ///===--------------------------------------------------------------------===///
  /// Fallback: Canonical Coarse
  ///===--------------------------------------------------------------------===///

  ARTS_DEBUG("Fallback applied: No heuristic matched, using canonical coarse");
  return PartitioningDecision::coarse(ctx, "H1.F: Canonical coarse fallback");
}

///===----------------------------------------------------------------------===///
/// PartitioningHint Implementation
///===----------------------------------------------------------------------===///

DictionaryAttr PartitioningHint::toAttribute(MLIRContext *ctx) const {
  SmallVector<NamedAttribute> attrs;
  attrs.push_back(
      {StringAttr::get(ctx, "mode"),
       IntegerAttr::get(IntegerType::get(ctx, 8), static_cast<uint8_t>(mode))});
  if (blockSize)
    attrs.push_back({StringAttr::get(ctx, "blockSize"),
                     IntegerAttr::get(IntegerType::get(ctx, 64), *blockSize)});
  return DictionaryAttr::get(ctx, attrs);
}

std::optional<PartitioningHint>
PartitioningHint::fromAttribute(Attribute attr) {
  auto dictAttr = dyn_cast_or_null<DictionaryAttr>(attr);
  if (!dictAttr)
    return std::nullopt;

  PartitioningHint hint;

  if (auto modeAttr = dictAttr.getAs<IntegerAttr>("mode"))
    hint.mode = static_cast<PartitionMode>(modeAttr.getInt());

  if (auto chunkAttr = dictAttr.getAs<IntegerAttr>("blockSize"))
    hint.blockSize = chunkAttr.getInt();

  return hint;
}

/// PartitioningHint Accessor Functions
namespace mlir {
namespace arts {

std::optional<PartitioningHint> getPartitioningHint(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttr(AttrNames::Operation::PartitionHint))
    return PartitioningHint::fromAttribute(attr);
  return std::nullopt;
}

void setPartitioningHint(Operation *op, const PartitioningHint &hint) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::PartitionHint,
              hint.toAttribute(op->getContext()));
}

void copyArtsMetadataAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;

  ARTS_DEBUG("copyArtsMetadataAttrs: " << source->getName() << " -> "
                                       << dest->getName());

  /// Transfer arts.id
  if (auto id =
          source->getAttrOfType<IntegerAttr>(AttrNames::Operation::ArtsId)) {
    dest->setAttr(AttrNames::Operation::ArtsId, id);
    ARTS_DEBUG("  -> transferred arts.id=" << id.getInt());
  }

  if (auto originId = source->getAttrOfType<IntegerAttr>(
          AttrNames::Operation::MetadataOriginId))
    dest->setAttr(AttrNames::Operation::MetadataOriginId, originId);
  if (auto provenance = source->getAttrOfType<StringAttr>(
          AttrNames::Operation::MetadataProvenance))
    dest->setAttr(AttrNames::Operation::MetadataProvenance, provenance);

  /// Transfer partition_mode
  if (auto mode = source->getAttrOfType<PartitionModeAttr>(
          AttrNames::Operation::PartitionMode))
    dest->setAttr(AttrNames::Operation::PartitionMode, mode);

  /// Transfer arts.partition_hint
  if (auto hint = source->getAttr(AttrNames::Operation::PartitionHint))
    dest->setAttr(AttrNames::Operation::PartitionHint, hint);

  /// Transfer arts.loop metadata (trip count, parallelism info, etc.)
  if (auto loopAttr = source->getAttr(AttrNames::LoopMetadata::Name))
    dest->setAttr(AttrNames::LoopMetadata::Name, loopAttr);
}

} // namespace arts
} // namespace mlir
