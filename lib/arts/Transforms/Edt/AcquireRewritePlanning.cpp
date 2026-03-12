///==========================================================================///
/// File: AcquireRewritePlanning.cpp
///
/// Plans per-task DbAcquire rewrite inputs by distribution strategy.
///
/// Example (tiling_2d output owner hints):
///   Before:
///     %acq = arts.db_acquire[<inout>] ... offsets[%rowOff], sizes[%rowSize]
///
///   Planned rewrite:
///     - keep row partitioning in offsets/sizes
///     - append [colOff, colSize] to partition_offsets/partition_sizes
///       so DbPartitioning can materialize 2D owner layout for outputs.
///==========================================================================///

#include "arts/Transforms/Edt/AcquireRewritePlanning.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DbUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

using namespace mlir;
using namespace mlir::arts;

ARTS_DEBUG_SETUP(acquire_rewrite_planning);

namespace {

} // namespace

/// Plan how to rewrite a per-task DbAcquire based on distribution strategy.
///
/// This function determines:
///   1. Whether to use block-based or stencil-based partitioning
///   2. Extra dimensions for 2D tiling strategies
///   3. Halo/extent information for stencil patterns
///   4. Whether to force coarse-grained partitioning for certain patterns
///
/// Key decisions:
///   - Stencil halo rewriting: Enabled for 'in' mode acquires with detected
///     stencil patterns (multiple offset accesses). Adds halo regions to
///     localized blocks to satisfy neighbor element dependencies.
///
///   - Single-node inout handling: On single-node machines, inout acquires
///     with cross-element self-reads (Seidel-style stencils) must stay coarse
///     because the same DB is both read and written at different offsets.
///     Jacobi-style double-buffer outputs can use block partitioning.
///
///   - 2D tiling: For tiling_2d distribution with inout mode, adds column
///     dimension partitioning to complement row partitioning for owner-
///     compute layout alignment.
///
/// Returns: AcquireRewritePlan with rewrite inputs and stencil flag
AcquireRewritePlan
mlir::arts::planAcquireRewrite(AcquireRewritePlanningInput input) {
  /// Helper to create default plan with minimal partitioning
  auto makePlan = [&]() {
    return AcquireRewritePlan{
        AcquireRewriteInput{input.AC, input.loc, input.parentAcquire,
                            input.rootGuid, input.rootPtr, input.acquireOffset,
                            input.acquireSize, input.acquireHintSize,
                            /*extraOffsets=*/SmallVector<Value, 4>{},
                            /*extraSizes=*/SmallVector<Value, 4>{},
                            /*extraHintSizes=*/SmallVector<Value, 4>{},
                            input.step, input.stepIsUnit,
                            /*singleElement=*/false,
                            /*forceCoarse=*/
                            input.distributionKind ==
                                DistributionKind::BlockCyclic,
                            /*stencilExtent=*/Value()},
        /*useStencilRewriter=*/false};
  };

  /// Early exit: missing required inputs
  if (!input.AC || !input.parentAcquire)
    return makePlan();

  Value zero = input.AC->createIndexConstant(0, input.loc);
  Value one = input.AC->createIndexConstant(1, input.loc);
  AcquireRewritePlan plan{makePlan()};

  bool isSingleElement = false;
  bool needsStencilHalo = false;
  bool forceCoarseRewrite = plan.rewriteInput.forceCoarse;
  Value stencilExtent;
  SmallVector<Value, 4> extraOffsets;
  SmallVector<Value, 4> extraSizes;
  SmallVector<Value, 4> extraHintSizes;

  if (Operation *rootAllocOp = DbUtils::getUnderlyingDbAlloc(input.rootPtr)) {
    if (auto dbAlloc = dyn_cast<DbAllocOp>(rootAllocOp)) {
      auto elemSizes = dbAlloc.getElementSizes();
      if (!elemSizes.empty()) {
        isSingleElement = llvm::all_of(elemSizes, [](Value value) {
          int64_t constant = 0;
          return ValueUtils::getConstantIndex(value, constant) && constant == 1;
        });

        /// Determine if this acquire needs stencil halo handling.
        /// This is a critical decision point for partitioning correctness.
        auto accessPattern = getDbAccessPattern(dbAlloc.getOperation());
        const ArtsMode mode = input.parentAcquire.getMode();

        /// Check if this inout acquire reads at cross-element offsets in the
        /// loop. This distinguishes Seidel-style (A[i] reads from A[i-1]) from
        /// Jacobi-style (A[i] writes, B[i] reads from B[i-1]).
        const bool inoutReadsCrossElementSelf =
            mode == ArtsMode::inout && input.analysisManager &&
            input.analysisManager->getDbAnalysis()
                .hasCrossElementSelfReadInLoop(input.parentAcquire,
                                               input.loopOp);

        /// Determine which modes require stencil halo based on access pattern
        const bool modeNeedsPatternStencilHalo =
            mode == ArtsMode::in || inoutReadsCrossElementSelf;
        const bool patternSaysStencil =
            accessPattern && *accessPattern == DbAccessPattern::stencil;

        /// Check if the EDT distribution strategy indicates stencil pattern
        const bool strategySaysStencil =
            input.distributionPattern &&
            *input.distributionPattern == EdtDistributionPattern::stencil;

        /// Only 'in' mode needs halo for strategy-based stencil detection
        const bool strategyNeedsStencilHalo = mode == ArtsMode::in;

        /// Final decision: enable stencil halo if pattern or strategy indicates
        /// stencil and the mode permits it (excluding single-element
        /// allocations)
        needsStencilHalo =
            !isSingleElement &&
            ((patternSaysStencil && modeNeedsPatternStencilHalo) ||
             (strategySaysStencil && strategyNeedsStencilHalo));

        ARTS_DEBUG("Acquire rewrite plan: mode="
                   << static_cast<int>(mode)
                   << " patternSaysStencil=" << patternSaysStencil
                   << " strategySaysStencil=" << strategySaysStencil
                   << " inoutReadsCrossElementSelf="
                   << inoutReadsCrossElementSelf);

        /// In-place stencil updates (Seidel-style) must stay coarse because
        /// the same DB is both written and read at cross-element offsets.
        /// This is unsafe under both single-node and internode lowering:
        /// block/stencil partitioning can expose stale or missing neighbor
        /// data while the task is still mutating the source DB in place.
        ///
        /// Uniform self-reads without cross-element offsets remain eligible
        /// for partitioning.
        if (mode == ArtsMode::inout && inoutReadsCrossElementSelf &&
            (patternSaysStencil || strategySaysStencil)) {
          ARTS_DEBUG("Keeping inout acquire coarse due to cross-element "
                     "self-read (Seidel-style pattern)");
          forceCoarseRewrite = true;
          needsStencilHalo = false;
        }

        if (needsStencilHalo)
          stencilExtent = input.AC->castToIndex(elemSizes.front(), input.loc);

        if (input.distributionKind == DistributionKind::Tiling2D &&
            input.parentAcquire.getMode() == ArtsMode::inout &&
            elemSizes.size() > 1 && !isSingleElement && input.tiling2DGrid &&
            input.tiling2DGrid->colWorkers) {
          Value totalCols = input.AC->castToIndex(elemSizes[1], input.loc);
          Value colWorkers =
              input.AC->castToIndex(input.tiling2DGrid->colWorkers, input.loc);
          Value colWorkerId =
              input.AC->castToIndex(input.tiling2DGrid->colWorkerId, input.loc);

          Value colWorkersMinusOne =
              input.AC->create<arith::SubIOp>(input.loc, colWorkers, one);
          Value colAdjusted = input.AC->create<arith::AddIOp>(
              input.loc, totalCols, colWorkersMinusOne);
          Value colChunk = input.AC->create<arith::DivUIOp>(
              input.loc, colAdjusted, colWorkers);
          Value colOffset =
              input.AC->create<arith::MulIOp>(input.loc, colWorkerId, colChunk);

          Value colNeedZero = input.AC->create<arith::CmpIOp>(
              input.loc, arith::CmpIPredicate::uge, colOffset, totalCols);
          Value colRemaining =
              input.AC->create<arith::SubIOp>(input.loc, totalCols, colOffset);
          Value colRemainingNonNeg = input.AC->create<arith::SelectOp>(
              input.loc, colNeedZero, zero, colRemaining);
          Value colCount = input.AC->create<arith::MinUIOp>(input.loc, colChunk,
                                                            colRemainingNonNeg);

          extraOffsets.push_back(colOffset);
          extraSizes.push_back(colCount);
          extraHintSizes.push_back(colCount);
        }
      }
    }
  }

  plan.useStencilRewriter = needsStencilHalo;
  plan.rewriteInput.singleElement = isSingleElement;
  plan.rewriteInput.forceCoarse = forceCoarseRewrite;
  plan.rewriteInput.stencilExtent = stencilExtent;
  plan.rewriteInput.extraOffsets = std::move(extraOffsets);
  plan.rewriteInput.extraSizes = std::move(extraSizes);
  plan.rewriteInput.extraHintSizes = std::move(extraHintSizes);
  return plan;
}
