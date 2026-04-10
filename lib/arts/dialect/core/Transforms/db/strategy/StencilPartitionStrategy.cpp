///==========================================================================///
/// File: StencilPartitionStrategy.cpp
///
/// Stencil/ESD partitioning strategy.
///
/// Implements H1.S0-H1.S2 heuristics:
///   H1.S0: Peer-inferred partition dims keep block mode (fallback to block)
///   H1.S1: Stencil unsupported for block -> element-wise fallback
///   H1.S2: Standard in-place stencil -> ESD mode
///
/// Stencil mode enables efficient halo exchange via ESD (Explicit Stencil
/// Dependencies) lowering, which produces separate center/left/right DB
/// allocations for each block.
///==========================================================================///

#include "arts/analysis/heuristics/HeuristicUtils.h"
#include "arts/analysis/heuristics/PartitioningHeuristics.h"
#include "arts/transforms/db/PartitionStrategy.h"
#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"

ARTS_DEBUG_SETUP(stencil_partition_strategy)

using namespace mlir;
using namespace mlir::arts;

namespace {

class StencilPartitionStrategy : public PartitionStrategy {
public:
  StencilPartitionStrategy() = default;
  ~StencilPartitionStrategy() override = default;

  std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx,
           const AbstractMachine *machine) const override {
    const auto &patterns = ctx.accessPatterns;

    /// Early exit: only applies to stencil patterns
    if (!patterns.hasStencil)
      return std::nullopt;

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

    /// H1.S0: Peer-inferred dims keep block mode
    /// Rationale: When stencil patterns are inferred from peer acquires rather
    /// than explicit contracts, the evidence isn't strong enough to enable ESD.
    /// Fall back to block mode for safety.
    if (!hasTrustedStencilAcquire && ctx.canBlock) {
      ARTS_DEBUG("H1.S0 applied: peer-inferred dims keep block mode");
      /// This returns block, not stencil - it's a fallback
      return PartitioningDecision::block(
          ctx, "H1.S0: Peer-inferred partition dims keep block mode");
    }

    /// H1.S1: Stencil unsupported for block → Element-wise fallback
    /// Rationale: When the allocation cannot support block partitioning
    /// (e.g., due to indirect access), fall back to element-wise instead
    /// of forcing coarse.
    if (!ctx.canBlock || ctx.hasIndirectAccess) {
      /// Let FineGrainedPartitionStrategy handle this fallback
      ARTS_DEBUG(
          "H1.S1: Stencil unsupported for block -> element-wise fallback");
      return PartitioningDecision::elementWise(
          ctx, 1,
          "H1.S1: Stencil unsupported for block, fallback to element-wise");
    }

    /// H1.S2: Standard in-place stencil → ESD mode
    /// This is the primary stencil heuristic: standard stencil patterns
    /// that passed the trust checks and block capability checks should
    /// use stencil/ESD mode for efficient halo exchange.
    ARTS_DEBUG("H1.S2 applied: Standard stencil -> ESD mode");
    return PartitioningDecision::stencil(
        ctx, patterns.hasUniform
                 ? "H1.S2: Mixed (uniform+stencil) uses Stencil mode"
                 : "H1.S2: Pure stencil uses ESD mode");
  }

  llvm::StringRef getName() const override { return "StencilPartition"; }

  unsigned getPriority() const override { return 200; }
};

} // namespace

///===----------------------------------------------------------------------===///
/// Factory Implementation
///===----------------------------------------------------------------------===///

namespace mlir {
namespace arts {

std::unique_ptr<PartitionStrategy>
PartitionStrategyFactory::createStencilStrategy() {
  return std::make_unique<StencilPartitionStrategy>();
}

} // namespace arts
} // namespace mlir
