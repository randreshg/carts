///==========================================================================///
/// File: FineGrainedPartitionStrategy.cpp
///
/// Fine-grained (element-wise) partitioning strategy.
///
/// Implements H1.E1-H1.E3 heuristics:
///   H1.E1: Explicit fine-grained partition hints -> element-wise
///   H1.E2: Indexed access without block support -> element-wise
///   H1.E3: Multi-node with element-wise support -> element-wise
///
/// Also handles fallback cases from coarse strategy:
///   - H1.C3 fallback: Read-only full-range without block -> element-wise
///   - H1.C4 fallback: Read-only stencil on single-node -> element-wise
///==========================================================================///

#include "arts/dialect/core/Analysis/heuristics/HeuristicUtils.h"
#include "arts/dialect/core/Analysis/heuristics/PartitioningHeuristics.h"
#include "arts/dialect/core/Transforms/db/PartitionStrategy.h"
#include "arts/utils/Debug.h"

ARTS_DEBUG_SETUP(fine_grained_partition_strategy)

using namespace mlir;
using namespace mlir::arts;

namespace {

class FineGrainedPartitionStrategy : public PartitionStrategy {
public:
  FineGrainedPartitionStrategy() = default;
  ~FineGrainedPartitionStrategy() override = default;

  std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx,
           const AbstractMachine *machine) const override {
    if (!ctx.canElementWise && !ctx.anyCanElementWise())
      return std::nullopt;

    const auto &patterns = ctx.accessPatterns;
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

    /// H1.E1: Explicit fine-grained partition hints → Element-wise
    if (hasExplicitFineGrained) {
      unsigned outerRank = ctx.minPinnedDimCount();
      outerRank = outerRank > 0 ? outerRank : 1;
      ARTS_DEBUG("H1.E1 applied: Explicit fine-grained hints");
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
      ARTS_DEBUG("H1.E2 applied: Indexed access (outerRank=" << outerRank
                                                             << ")");
      return PartitioningDecision::elementWise(
          ctx, outerRank, "H1.E2: Indexed access requires element-wise");
    }

    /// H1.E3: Multi-node with element-wise support → Element-wise
    if (machine && machine->isDistributed()) {
      unsigned outerRank = ctx.minPinnedDimCount();
      outerRank = outerRank > 0 ? outerRank : 1;
      ARTS_DEBUG("H1.E3 applied: Multi-node prefers fine-grained");
      return PartitioningDecision::elementWise(
          ctx, outerRank, "H1.E3: Multi-node prefers fine-grained");
    }

    /// No fine-grained heuristic matched
    return std::nullopt;
  }

  llvm::StringRef getName() const override { return "FineGrainedPartition"; }

  unsigned getPriority() const override { return 300; }
};

} // namespace

///===----------------------------------------------------------------------===///
/// Factory Implementation
///===----------------------------------------------------------------------===///

namespace mlir {
namespace arts {

std::unique_ptr<PartitionStrategy>
PartitionStrategyFactory::createFineGrainedStrategy() {
  return std::make_unique<FineGrainedPartitionStrategy>();
}

} // namespace arts
} // namespace mlir
