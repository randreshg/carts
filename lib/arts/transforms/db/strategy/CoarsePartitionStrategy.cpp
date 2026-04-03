///==========================================================================///
/// File: CoarsePartitionStrategy.cpp
///
/// Coarse partitioning strategy (single datablock).
///
/// Implements H1.C0-H1.C5 heuristics:
///   H1.C0: Tiny read-only stencil coefficient table -> coarse
///   H1.C1: Read-only single-node without partitioning support -> coarse
///   H1.C2: Explicit coarse acquire on single-node -> coarse
///   H1.C2b: Read-only mixed owner dims on single-node -> coarse
///   H1.C3: Read-only full-range block acquires -> coarse
///   H1.C4: Read-only stencil on single-node -> coarse
///   H1.C5: Non-uniform access without partitioning support -> coarse
///==========================================================================///

#include "arts/transforms/db/PartitionStrategy.h"
#include "arts/analysis/heuristics/HeuristicUtils.h"
#include "arts/analysis/heuristics/PartitioningHeuristics.h"
#include "arts/utils/Debug.h"

ARTS_DEBUG_SETUP(coarse_partition_strategy)

using namespace mlir;
using namespace mlir::arts;

namespace {

///===----------------------------------------------------------------------===///
/// Helper Functions (extracted from PartitioningHeuristics.cpp)
///===----------------------------------------------------------------------===///

static bool
isTinyReadOnlyStencilCoefficientTable(const PartitioningContext &ctx) {
  return ctx.existingAllocMode == PartitionMode::coarse &&
         ctx.allocAccessPattern == DbAccessPattern::stencil &&
         ctx.accessMode == ArtsMode::in && ctx.allocDbMode == DbMode::read &&
         ctx.staticElementCount && *ctx.staticElementCount > 0 &&
         *ctx.staticElementCount <= 8;
}

static bool
hasMixedAuthoritativeReadOnlyOwnerDims(const PartitioningContext &ctx) {
  bool sawAuthoritativeOwner = false;
  uint8_t referenceCount = 0;
  int16_t referenceOwnerDims[4] = {-1, -1, -1, -1};

  for (const AcquireInfo &info : ctx.acquires) {
    if (info.accessMode != ArtsMode::in || !info.canBlock ||
        info.partitionDimsFromPeers || info.ownerDimsCount == 0)
      continue;

    if (!sawAuthoritativeOwner) {
      sawAuthoritativeOwner = true;
      referenceCount = info.ownerDimsCount;
      for (unsigned dim = 0; dim < referenceCount; ++dim)
        referenceOwnerDims[dim] = info.ownerDims[dim];
      continue;
    }

    if (referenceCount != info.ownerDimsCount)
      return true;
    for (unsigned dim = 0; dim < referenceCount; ++dim)
      if (referenceOwnerDims[dim] != info.ownerDims[dim])
        return true;
  }

  return false;
}

///===----------------------------------------------------------------------===///
/// CoarsePartitionStrategy Implementation
///===----------------------------------------------------------------------===///

class CoarsePartitionStrategy : public PartitionStrategy {
public:
  CoarsePartitionStrategy() = default;
  ~CoarsePartitionStrategy() override = default;

  std::optional<PartitioningDecision>
  evaluate(const PartitioningContext &ctx,
           const AbstractMachine *machine) const override {
    const auto &patterns = ctx.accessPatterns;
    bool isReadOnly = !ctx.acquires.empty() ? ctx.allReadOnly()
                                            : (ctx.accessMode == ArtsMode::in);
    bool isSingleNode = (machine && machine->isSingleNode());

    /// H1.C0: Tiny read-only stencil coefficient table -> Coarse
    if (isTinyReadOnlyStencilCoefficientTable(ctx)) {
      ARTS_DEBUG("H1.C0 applied: Tiny read-only stencil coefficient table");
      return PartitioningDecision::coarse(
          ctx,
          "H1.C0: Tiny read-only stencil coefficient table prefers coarse");
    }

    /// H1.C1: Read-only single-node without fine-grained support → Coarse
    if (isSingleNode && isReadOnly && !ctx.canBlock && !ctx.canElementWise) {
      ARTS_DEBUG("H1.C1 applied: Read-only single-node");
      return PartitioningDecision::coarse(
          ctx, "H1.C1: Read-only single-node without partitioning support");
    }

    /// H1.C2: Explicit coarse acquire on single-node → Coarse
    if (isSingleNode && ctx.anyExplicitCoarseAcquire() &&
        !patterns.hasStencil && !ctx.canBlock && !ctx.canElementWise) {
      ARTS_DEBUG("H1.C2 applied: Single-node explicit coarse acquire");
      return PartitioningDecision::coarse(
          ctx, "H1.C2: Single-node explicit coarse acquire with no "
               "partitioning support");
    }

    /// H1.C2b: Read-only mixed owner dims on single-node -> Coarse
    if (isSingleNode && isReadOnly && ctx.canBlock && !patterns.hasStencil &&
        hasMixedAuthoritativeReadOnlyOwnerDims(ctx) && !ctx.preferBlockND &&
        ctx.maxPinnedDimCount() <= 1) {
      ARTS_DEBUG(
          "H1.C2b applied: Read-only mixed owner dims on single-node");
      return PartitioningDecision::coarse(
          ctx,
          "H1.C2b: Read-only mixed owner dims on single-node prefer coarse");
    }

    /// H1.C3: Read-only full-range block acquires → Coarse
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

    bool preserveStencilBlockForReadOnlyFullRange =
        (patterns.hasStencil || hasTrustedStencilAcquire) &&
        !patterns.hasIndexed;
    bool preserveBlockForReadOnlyFullRange =
        ctx.canBlock &&
        (!isSingleNode || preserveStencilBlockForReadOnlyFullRange);

    if (isReadOnly && ctx.allBlockFullRange &&
        !preserveBlockForReadOnlyFullRange) {
      /// Special case: fall back to element-wise if supported
      if (!ctx.canBlock && ctx.canElementWise) {
        /// Let FineGrainedPartitionStrategy handle this
        return std::nullopt;
      }
      ARTS_DEBUG("H1.C3 applied: Read-only full-range acquires");
      return PartitioningDecision::coarse(
          ctx, isSingleNode
                   ? "H1.C3: Read-only full-range on single-node prefers coarse"
                   : "H1.C3: Read-only full-range on multi-node prefers coarse");
    }

    /// H1.C4: Read-only stencil on single-node → Coarse
    if (patterns.hasStencil && isSingleNode && isReadOnly && !ctx.canBlock &&
        !ctx.hasDistributedBlockContract()) {
      /// Special case: fall back to element-wise if supported
      if (ctx.canElementWise) {
        /// Let FineGrainedPartitionStrategy handle this
        return std::nullopt;
      }
      ARTS_DEBUG("H1.C4 applied: Read-only stencil on single-node");
      return PartitioningDecision::coarse(
          ctx, "H1.C4: Read-only stencil on single-node prefers coarse");
    }

    /// H1.C5: Non-uniform access without partitioning support → Coarse
    if (!ctx.isUniformAccess && !ctx.canElementWise && !ctx.canBlock) {
      ARTS_DEBUG("H1.C5 applied: Non-uniform access without partitioning");
      return PartitioningDecision::coarse(
          ctx, "H1.C5: Non-uniform access without partitioning support");
    }

    /// No coarse heuristic matched
    return std::nullopt;
  }

  llvm::StringRef getName() const override { return "CoarsePartition"; }

  unsigned getPriority() const override { return 0; }
};

} // namespace

///===----------------------------------------------------------------------===///
/// Factory Implementation
///===----------------------------------------------------------------------===///

namespace mlir {
namespace arts {

std::unique_ptr<PartitionStrategy>
PartitionStrategyFactory::createCoarseStrategy() {
  return std::make_unique<CoarsePartitionStrategy>();
}

} // namespace arts
} // namespace mlir
