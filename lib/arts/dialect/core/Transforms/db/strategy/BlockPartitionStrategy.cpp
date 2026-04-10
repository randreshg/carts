///==========================================================================///
/// File: BlockPartitionStrategy.cpp
///
/// Block partitioning strategy.
///
/// Implements H1.B1-H1.B6 heuristics:
///   H1.B1: Mixed access (block writes + indirect reads) -> block
///   H1.B2: Mixed direct + indirect writes -> block
///   H1.B3: Double-buffer stencil (Jacobi-style) -> block
///   H1.B3b: Read-only stencil with full-range -> block for NUMA
///   H1.B4: Indexed access with block support -> block
///   H1.B5: Uniform direct access -> block
///   H1.B6: Multi-node with block support -> block
///==========================================================================///

#include "arts/analysis/heuristics/HeuristicUtils.h"
#include "arts/analysis/heuristics/PartitioningHeuristics.h"
#include "arts/transforms/db/PartitionStrategy.h"
#include "arts/utils/Debug.h"

ARTS_DEBUG_SETUP(block_partition_strategy)

using namespace mlir;
using namespace mlir::arts;

namespace {

class BlockPartitionStrategy : public PartitionStrategy {
public:
  BlockPartitionStrategy() = default;
  ~BlockPartitionStrategy() override = default;

  std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx,
           const AbstractMachine *machine) const override {
    if (!ctx.canBlock && !ctx.anyCanBlock())
      return std::nullopt;

    const auto &patterns = ctx.accessPatterns;
    bool isReadOnly = !ctx.acquires.empty() ? ctx.allReadOnly()
                                            : (ctx.accessMode == ArtsMode::in);

    /// H1.B1: Mixed access (block writes + indirect reads) → Block
    if (ctx.canBlock && ctx.hasIndirectRead && !ctx.hasIndirectWrite &&
        ctx.hasDirectAccess) {
      ARTS_DEBUG("H1.B1 applied: Mixed block/indirect access");
      return PartitioningDecision::block(
          ctx,
          "H1.B1: Mixed access (block writes + full-range indirect reads)");
    }

    /// H1.B2: Mixed direct + indirect writes → Block
    if (ctx.canBlock && ctx.hasIndirectWrite && ctx.hasDirectAccess) {
      ARTS_DEBUG("H1.B2 applied: Mixed direct+indirect writes");
      return PartitioningDecision::block(
          ctx, "H1.B2: Mixed direct+indirect writes with full-range acquires");
    }

    /// H1.B3: Double-buffer stencil (Jacobi-style) → Block
    /// Check for explicit fine-grained hints that should override
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

    if (patterns.hasStencil && hasExplicitFineGrained) {
      /// Let FineGrainedPartitionStrategy handle this
      return std::nullopt;
    }

    if (patterns.hasStencil && ctx.canBlock) {
      bool hasJacobiDepPattern =
          llvm::any_of(ctx.acquires, [](const AcquireInfo &a) {
            return a.depPattern == ArtsDepPattern::jacobi_alternating_buffers;
          });
      bool hasWavefrontDepPattern =
          llvm::any_of(ctx.acquires, [](const AcquireInfo &a) {
            return a.depPattern == ArtsDepPattern::wavefront_2d;
          });
      bool hasStencilReads =
          llvm::any_of(ctx.acquires, [](const AcquireInfo &a) {
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

      if (hasWavefrontDepPattern) {
        /// Let StencilPartitionStrategy handle wavefront patterns
        ARTS_DEBUG("H1.B3 skipped: wavefront_2d pattern -> stencil mode");
        return std::nullopt;
      } else if (hasStencilReads && hasWriteAcquires && allWritesUniform) {
        ARTS_DEBUG("H1.B3 applied: Double-buffer stencil");
        return PartitioningDecision::block(
            ctx, "H1.B3: Double-buffer stencil (Jacobi) with uniform writes");
      } else if (hasStencilReads && isReadOnly && ctx.allBlockFullRange) {
        /// H1.B3b: Read-only stencil with full-range acquires → Block
        ARTS_DEBUG("H1.B3b applied: RO stencil with full-range");
        return PartitioningDecision::block(
            ctx, "H1.B3b: Read-only cross-dim stencil prefers block for NUMA");
      } else {
        /// Let StencilPartitionStrategy handle other stencil patterns
        ARTS_DEBUG("H1.B3 not matched - deferring to stencil strategy");
        return std::nullopt;
      }
    }

    /// H1.B4: Indexed access with block support → Block
    if (patterns.hasIndexed && ctx.canBlock) {
      ARTS_DEBUG("H1.B4 applied: Indexed access with block support");
      return PartitioningDecision::block(
          ctx, "H1.B4: Indexed access prefers block when supported");
    }

    /// H1.B5: Uniform direct access → Block
    bool blockSizeFits = !ctx.totalElements || !ctx.blockSize ||
                         *ctx.totalElements >= *ctx.blockSize;
    if (ctx.canBlock && ctx.hasDirectAccess && !ctx.hasIndirectAccess &&
        patterns.hasUniform && !ctx.elementTypeIsMemRef && blockSizeFits) {
      ARTS_DEBUG("H1.B5 applied: Uniform direct access");
      return PartitioningDecision::block(
          ctx, "H1.B5: Uniform direct access prefers block");
    }

    /// H1.B6: Multi-node with block support → Block
    if (machine && machine->isDistributed()) {
      ARTS_DEBUG("H1.B6 applied: Multi-node prefers block");
      return PartitioningDecision::block(
          ctx, "H1.B6: Multi-node prefers block for network efficiency");
    }

    /// No block heuristic matched
    return std::nullopt;
  }

  llvm::StringRef getName() const override { return "BlockPartition"; }

  unsigned getPriority() const override { return 100; }
};

} // namespace

///===----------------------------------------------------------------------===///
/// Factory Implementation
///===----------------------------------------------------------------------===///

namespace mlir {
namespace arts {

std::unique_ptr<PartitionStrategy>
PartitionStrategyFactory::createBlockStrategy() {
  return std::make_unique<BlockPartitionStrategy>();
}

} // namespace arts
} // namespace mlir
