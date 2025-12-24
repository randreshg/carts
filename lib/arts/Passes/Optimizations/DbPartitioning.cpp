///==========================================================================///
/// File: DbPartitioning.cpp
/// Datablock partitioning and chunking transformations.
/// This pass chooses a datablock partitioning strategy with two goals:
///   1) Preserve correctness via conservative twin-diff use.
///   2) Maximize concurrency while avoiding unnecessary data movement.
///
/// Key points:
///   - Chunking only occurs when chunk hints/info are available; otherwise we
///     fall back to element-wise to preserve parallelism (safe default).
///   - Concurrency is part of the decision: if chunking cannot be justified
///     by hints and cost model, element-wise is preferred over coarse.
///   - Twin-diff stays enabled unless we can prove disjoint access after
///     partitioning; this avoids corruption from overlapping writes.
///   - Stencil/mixed patterns default to element-wise for now; a future path
///
/// Decision Flow (allocation-level):
///
///   canBePartitioned()? ──no──> keep coarse
///            │
///            yes
///            │
///   H1: read-only + single-node? ──yes──> keep coarse
///            │
///            no
///            │
///   access-pattern summary (per-acquire → alloc):
///     - any stencil? → element-wise (default for now)
///     - mixed patterns? → element-wise (safe fallback)
///     - uniform only? → evaluate H2 for chunking
///            │
///   H2 (cost model, concurrency-aware): if passes → chunked, else →
///   element-wise
///            │
///   Note: Chunked is only possible when chunk info/hints exist; otherwise
///         fallback is element-wise to preserve concurrency.
///
/// Compact equation:
///   Decision =
///     if !canBePartitioned -> Coarse
///     else if H1 -> Coarse
///     else if hasStencil || mixed -> ElementWise
///     else if H2 && hasChunkInfo -> Chunked
///     else -> ElementWise
///
///===----------------------------------------------------------------------===///
/// Twin-Diff Policy - See HeuristicsConfig::shouldUseTwinDiff()
///
/// Decision: Single-node OR proof exists --> DISABLE
///           Otherwise                   --> ENABLE (safe default)
///
/// Proof methods: IndexedControl, PartitionSuccess, AliasAnalysis
///===----------------------------------------------------------------------===///

/// Dialects
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
/// Arts
#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/Loop/LoopNode.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Visitors.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"
/// Debug
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtNode.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cstdint>
#include <optional>

#include "arts/Transforms/DbTransforms.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DatablockUtils.h"
ARTS_DEBUG_SETUP(db_partitioning);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;

namespace {
static DbRewritePlan makeRewritePlan(bool useChunked,
                                     ArrayRef<Value> newInnerSizes) {
  RewriterMode mode =
      useChunked ? RewriterMode::Chunked : RewriterMode::ElementWise;
  Value chunkSize =
      useChunked && !newInnerSizes.empty() ? newInnerSizes[0] : Value();
  return {mode, chunkSize, std::nullopt};
}
} // namespace

///===----------------------------------------------------------------------===///
// Pass Implementation
// Partition Datablocks for fine-grained parallel access
///===----------------------------------------------------------------------===///
namespace {
struct DbPartitioningPass
    : public arts::DbPartitioningBase<DbPartitioningPass> {
  DbPartitioningPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override;

  /// Partitioning
  bool partitionDb();

  /// Stencil bounds analysis
  void analyzeStencilBounds();
  void generateBoundsValid(DbAcquireOp acquireOp,
                           ArrayRef<int64_t> boundsCheckFlags, Value loopIV);

  /// Graph rebuild
  void invalidateAndRebuildGraph();
  FailureOr<DbAllocOp>
  promoteAllocForChunking(DbAllocOp allocOp, DbAllocNode *allocNode,
                          ArrayRef<DbRewriteAcquire> acquireInfo);

private:
  ModuleOp module;
  ArtsAnalysisManager *AM = nullptr;
};
} // namespace

void DbPartitioningPass::runOnOperation() {
  bool changed = false;
  module = getOperation();

  ARTS_INFO_HEADER(DbPartitioningPass);
  ARTS_DEBUG_REGION(module.dump(););

  assert(AM && "ArtsAnalysisManager must be provided externally");

  /// Graph construction and analysis
  invalidateAndRebuildGraph();

  /// Partition DBs and analyze stencil patterns for bounds checking
  changed |= partitionDb();
  analyzeStencilBounds();

  if (changed) {
    ARTS_INFO(" Module has changed, invalidating analyses");
    AM->invalidate();
  }

  ARTS_INFO_FOOTER(DbPartitioningPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// partitionDb - Partition datablocks and set twin-diff attributes
///===----------------------------------------------------------------------===///
///
/// This function performs two key tasks:
/// 1. PARTITIONING: Attempts to promote coarse-grained allocations to
///    fine-grained ones by computing chunk info (offset, size) for each
///    acquire. This allows workers to access disjoint regions of the DB.
///
/// 2. TWIN-DIFF ASSIGNMENT: Sets the twin_diff attribute on each DbAcquireOp
///    based on whether overlapping access can be ruled out:
///    - twin_diff=FALSE: Only when partitioning succeeds AND we can prove
///      that all acquires access disjoint regions. This is safe because
///      workers won't write to overlapping memory.
///    - twin_diff=TRUE: When partitioning fails OR we cannot prove disjoint
///      access. The runtime will use the twin-diff mechanism to track changes
///      and merge updates correctly.
/// Decision Flow:
///   1. Collect candidates for partitioning (eligible acquires with chunk info)
///   2. For each allocation, attempt promotion and update acquires
///   3. If chunk rewrite succeeds -> twin_diff=FALSE (proven disjoint)
///   4. If chunk rewrite fails -> twin_diff=TRUE (potential overlap)
///   5. Final pass: any unset acquires get twin_diff=TRUE (safe default)
///===----------------------------------------------------------------------===///
bool DbPartitioningPass::partitionDb() {
  ARTS_DEBUG_HEADER(PartitionDb);
  ///==========================================================================
  /// Partitioning Decision Summary (see top-of-file for flowchart/equation)
  ///==========================================================================
  bool changed = false;
  OpBuilder attrBuilder(module.getContext());

  /// Helper lambda to set twin-diff attribute on a DbAcquireOp.
  /// Uses the consolidated TwinDiffProof to make decisions via
  /// HeuristicsConfig.
  auto setTwinAttr = [&](DbAcquireOp acq, TwinDiffProof proof) {
    if (!acq)
      return;

    TwinDiffContext twinCtx;
    twinCtx.proof = proof;
    twinCtx.op = acq.getOperation();

    bool useTwinDiff = AM->getHeuristicsConfig().shouldUseTwinDiff(twinCtx);

    if (acq.hasTwinDiff() && acq.getTwinDiff() == useTwinDiff)
      return;
    acq.setTwinDiff(useTwinDiff);
    ARTS_DEBUG("  Set twin_diff=" << (useTwinDiff ? "true" : "false"));
    changed = true;
  };
  llvm::SmallPtrSet<Operation *, 8> partitionAttempt, partitionSuccess,
      partitionFailed;

  /// Collect all (alloc, acquire, offset, size) tuples
  using AcqChunkInfo = DbRewriteAcquire;
  DenseMap<DbAllocOp, std::pair<DbAllocNode *, SmallVector<AcqChunkInfo, 4>>>
      allocsToPromote;

  /// Walk the module and check each allocation using hierarchical validation.
  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    /// Iterate through allocations and use unified canBePartitioned() API
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      if (!allocNode)
        return;

      DbAllocOp allocOp = allocNode->getDbAllocOp();
      if (!allocOp)
        return;

      partitionAttempt.insert(allocOp.getOperation());

      /// Hierarchical validation
      ARTS_DEBUG("Checking allocation: " << allocOp);
      if (!allocNode->canBePartitioned()) {
        ARTS_DEBUG("  SKIP: canBePartitioned() returned false");
        partitionFailed.insert(allocOp.getOperation());
        return;
      }

      /// H1: Skip partitioning for read-only allocations on single-node.
      if (AM->getHeuristicsConfig().shouldPreferCoarseForReadOnly(
              allocOp.getMode())) {
        ARTS_DEBUG(
            "  SKIP: H1 heuristic - read-only on single-node, keep coarse");
        return;
      }

      /// All validation passed
      bool allValid = true;
      SmallVector<std::tuple<DbAcquireNode *, Value, Value>, 4> validatedAcqs;

      allocNode->forEachChildNode([&](NodeBase *child) {
        if (!allValid)
          return;
        auto *acqNode = dyn_cast<DbAcquireNode>(child);
        if (!acqNode)
          return;

        Value chunkOffset, chunkSize;
        if (failed(acqNode->computeChunkInfo(chunkOffset, chunkSize))) {
          ARTS_DEBUG("  Failed to compute chunk info for acquire "
                     << acqNode->getDbAcquireOp());
          allValid = false;
          return;
        }
        ARTS_DEBUG("  Computed chunk info for acquire "
                   << acqNode->getDbAcquireOp() << " offset=" << chunkOffset
                   << " size=" << chunkSize);

        validatedAcqs.push_back({acqNode, chunkOffset, chunkSize});
      });

      if (!allValid) {
        ARTS_DEBUG("  Skipping alloc " << allocOp
                                       << ": not all acquires could compute "
                                          "chunk info");
        partitionFailed.insert(allocOp.getOperation());
        return;
      }

      /// All acquires validated - add to promotion list
      auto &[nodePtr, acquireList] = allocsToPromote[allocOp];
      if (!nodePtr)
        nodePtr = allocNode;
      for (auto &[node, offset, size] : validatedAcqs) {
        acquireList.push_back({node->getDbAcquireOp(), offset, size});
      }
    });
  });

  /// Now promote allocations and update acquires
  SmallVector<
      std::tuple<DbAllocOp, DbAllocNode *, SmallVector<DbRewriteAcquire, 4>>>
      allocsToProcess;
  for (auto &[allocOpPtr, nodeAndList] : allocsToPromote) {
    auto &[allocNode, acquireList] = nodeAndList;
    allocsToProcess.push_back(
        {cast<DbAllocOp>(allocOpPtr), allocNode, std::move(acquireList)});
  }

  /// Clear map to avoid using dangling pointers
  allocsToPromote.clear();

  for (auto &[allocOp, allocNode, acquireList] : allocsToProcess) {
    ARTS_DEBUG("Promoting alloc with " << acquireList.size() << " acquires");

    /// Promote allocation with partition info for all acquires
    auto promotedOr = promoteAllocForChunking(allocOp, allocNode, acquireList);
    if (failed(promotedOr)) {
      partitionFailed.insert(allocOp.getOperation());
      continue;
    }

    DbAllocOp promoted = promotedOr.value();
    partitionAttempt.insert(promoted.getOperation());
    if (promoted.getOperation() == allocOp.getOperation()) {
      ARTS_DEBUG("  Alloc already promoted");
    } else {
      ARTS_DEBUG("  Promoted alloc to sizes=" << promoted.getSizes().size());
      changed = true;
    }

    /// Set twin_diff=false for all acquires (partition info already applied)
    for (auto &[acqOp, chunkOffset, chunkSize] : acquireList) {
      if (!acqOp)
        continue;
      setTwinAttr(acqOp, TwinDiffProof::PartitionSuccess);
    }

    partitionSuccess.insert(promoted.getOperation());
  }

  /// AFTER partition attempts, BEFORE final default pass:
  /// Analyze acquires that weren't partitioned using DbAliasAnalysis and
  /// partition hints. This allows us to disable twin-diff for allocations
  /// where we can PROVE non-overlapping access even without full partitioning.
  ARTS_DEBUG("  Overlap analysis for unpartitioned allocations:");
  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      DbAllocOp allocOp = allocNode->getDbAllocOp();
      if (!allocOp)
        return;

      /// Skip if already handled by successful partitioning
      if (partitionSuccess.contains(allocOp.getOperation())) {
        ARTS_DEBUG("    Skipping alloc " << allocNode->getHierId()
                                         << " - already partitioned");
        return;
      }

      if (allocNode->getAcquireNodesSize() == 0)
        return;

      ARTS_DEBUG("    Analyzing alloc " << allocNode->getHierId() << " with "
                                        << allocNode->getAcquireNodesSize()
                                        << " acquires");

      /// Analyze using node's canProveNonOverlapping method
      bool allDisjoint = allocNode->canProveNonOverlapping();

      if (allDisjoint) {
        /// Set twin_diff=false on all acquires for this allocation
        allocNode->forEachChildNode([&](NodeBase *child) {
          if (auto *acqNode = dyn_cast<DbAcquireNode>(child)) {
            DbAcquireOp acqOp = acqNode->getDbAcquireOp();
            if (acqOp && !acqOp.hasTwinDiff()) {
              setTwinAttr(acqOp, TwinDiffProof::AliasAnalysis);
              ARTS_DEBUG("      acquire "
                         << acqNode->getHierId()
                         << ": proven disjoint -> twin_diff=false");
            }
          }
        });
      } else {
        ARTS_DEBUG("      Potential overlap detected -> will use default "
                   "twin_diff=true");
      }
    });
  });

  /// Safe default - enable twin-diff for any unanalyzed acquires
  ARTS_DEBUG(
      "  Final pass: Setting safe default twin_diff=true for unset acquires");
  module.walk([&](DbAcquireOp acq) {
    if (!acq.hasTwinDiff()) {
      setTwinAttr(acq, TwinDiffProof::None);
      ARTS_DEBUG("    Default twin_diff=true for unset acquire");
    }
  });

  /// Rebuild graph if any promotions occurred
  if (changed) {
    ARTS_DEBUG("Rebuilding graph after promotions");
    invalidateAndRebuildGraph();
  }

  ARTS_DEBUG_FOOTER(PartitionDb);
  return changed;
}

/// Rewrite coarse allocation using DbRewriter.
/// Computes new outer/inner sizes based on heuristics and applies the
/// transformation with partition info for all acquires.
FailureOr<DbAllocOp> DbPartitioningPass::promoteAllocForChunking(
    DbAllocOp allocOp, DbAllocNode *allocNode,
    ArrayRef<DbRewriteAcquire> acquireInfo) {
  if (!allocOp || !allocOp.getOperation())
    return failure();

  if (allocOp.getElementSizes().empty())
    return failure();

  /// Already fine-grained - no promotion needed
  if (allocNode && allocNode->isFineGrained())
    return allocOp;

  OpBuilder builder(allocOp);
  Location loc = allocOp.getLoc();
  builder.setInsertionPoint(allocOp);

  /// Extract MAX BASE chunk size across ALL acquires (WITHOUT stencil halo).
  /// Use extractChunkSizeFromHint to get the base worker iteration count,
  /// ensuring chunk boundaries align with worker iteration boundaries.
  ///
  /// Why base size (not halo-adjusted size):
  /// - Worker iterations: [0-63], [64-127], [128-191], [192-255] (64 each)
  /// - With 64-row chunks: boundaries align perfectly, each worker writes to 1
  /// chunk
  /// - With 66-row chunks: misalignment causes Worker 1 to need chunks 0 AND 1
  /// for WRITE
  ///   (rows 64-65 fall in chunk 0), creating a race condition with Worker 0
  ///
  /// Stencil halo access (e.g., row 63 for Worker 1) is handled by multi-chunk
  /// acquire spanning adjacent chunks. Index localization uses globalRow %
  /// chunkSize, which is always in [0, chunkSize-1], so 64 rows per chunk is
  /// sufficient.
  int64_t chunkSizeVal = 0;
  for (const auto &[acqOp, offset, size] : acquireInfo) {
    if (auto extracted = arts::extractChunkSizeFromHint(size);
        extracted && *extracted > chunkSizeVal) {
      chunkSizeVal = *extracted;
      ARTS_DEBUG("  Acquire " << acqOp << " base chunk size " << *extracted);
    }
  }

  /// Compute element size in bytes for H2 heuristic
  Type elemType = allocOp.getElementType();
  unsigned elemBytes = 4; /// default to 4 bytes (float)
  if (elemType.isIntOrFloat())
    elemBytes = (elemType.getIntOrFloatBitWidth() + 7) / 8;

  SmallVector<Value> newOuterSizes, newInnerSizes;
  bool useChunked = false;

  ///==========================================================================
  /// Access-pattern consistency (per-acquire → alloc-level decision)
  ///
  /// Each DbAcquire can have its own access pattern (uniform vs stencil vs
  /// indexed). We summarize patterns at the allocation level and decide whether
  /// we should avoid chunking altogether.
  ///
  /// Current default policy:
  ///   - Any stencil access => element-wise (for now)
  ///   - Mixed patterns     => element-wise (safe fallback)
  ///
  /// This keeps partitioning explicit and avoids accidental chunking for
  /// stencil-heavy or mixed-phase allocations.
  ///==========================================================================
  bool forceElementWise = false;
  if (allocNode) {
    auto summary = allocNode->summarizeAcquirePatterns();
    if (summary.hasStencil || summary.isMixed())
      forceElementWise = true;
    if (forceElementWise)
      ARTS_DEBUG("  Access-pattern policy -> element-wise (stencil or mixed)");
  }

  ///==========================================================================
  /// N-D First-Dimension Chunking: For N-D allocations (N >= 2)
  /// Chunks the first dimension, preserves remaining dimensions.
  /// Example: sizes=[1], elemSizes=[1000,1000] -> sizes=[16],
  /// elemSizes=[63,1000] Uses partial evaluation: numChunks depends on dim 0,
  /// innerBytes on dims 1..N
  ///==========================================================================
  if (!forceElementWise && allocOp.getElementSizes().size() >= 2 &&
      chunkSizeVal > 0) {
    ValueRange elemSizes = allocOp.getElementSizes();
    Value firstDimVal = elemSizes[0];

    /// Compute numChunks (requires dim 0 to be constant)
    std::optional<int64_t> numChunksOpt;
    int64_t firstDimConst;
    if (ValueUtils::getConstantIndex(firstDimVal, firstDimConst) &&
        firstDimConst > chunkSizeVal) {
      numChunksOpt = (firstDimConst + chunkSizeVal - 1) / chunkSizeVal;
    }
    /// If dim 0 is runtime -> numChunksOpt stays nullopt

    /// Compute innerBytes (requires dims 1..N to be constant)
    /// Key insight: innerBytes does NOT depend on dim 0!
    std::optional<int64_t> innerBytesOpt;
    int64_t innerElements = chunkSizeVal; /// Start with chunk size
    bool innerConstant = true;
    for (unsigned i = 1; i < elemSizes.size(); ++i) {
      int64_t dimSize;
      if (ValueUtils::getConstantIndex(elemSizes[i], dimSize))
        innerElements *= dimSize;
      else
        innerConstant = false;
    }
    if (innerConstant) {
      innerBytesOpt = innerElements * elemBytes;
    }
    /// If any dim 1..N is runtime -> innerBytesOpt stays nullopt

    /// Call H2 with partial information (uses reject-early semantics)
    /// Use ParallelFor source since we're in promoteAllocForChunking
    /// (DbControlOps would have made this fine-grained already)
    auto &heuristics = AM->getHeuristicsConfig();
    auto decision = heuristics.shouldUseFineGrained(
        numChunksOpt,                   /// May be nullopt if dim 0 is runtime
        std::optional<int64_t>(1),      /// depsPerEDT always known
        innerBytesOpt,                  /// May be nullopt if dims 1..N runtime
        PartitioningSource::ParallelFor /// Prioritize chunking for parallel_for
    );

    /// decision: true=approved, false=rejected, nullopt=partial pass
    bool chunkingApproved = !decision.has_value() || decision.value();

    if (chunkingApproved) {
      ARTS_DEBUG("  Using "
                 << elemSizes.size()
                 << "D first-dim chunking with chunkSize=" << chunkSizeVal);
      useChunked = true;

      /// Generate runtime numChunks computation
      /// Compute ceiling division manually: (firstDim + chunkSize - 1) /
      /// chunkSize Using arith.divui instead of arith.ceildivui for reliable
      /// LLVM lowering
      Type i64Type = builder.getI64Type();
      Value firstDimI64 =
          builder.create<arith::IndexCastOp>(loc, i64Type, firstDimVal);
      Value chunkSizeI64 =
          builder.create<arith::ConstantIntOp>(loc, chunkSizeVal, 64);
      Value oneI64 = builder.create<arith::ConstantIntOp>(loc, 1, 64);
      /// numChunks = (firstDim + chunkSize - 1) / chunkSize
      Value sum = builder.create<arith::AddIOp>(loc, firstDimI64, chunkSizeI64);
      Value sumMinusOne = builder.create<arith::SubIOp>(loc, sum, oneI64);
      Value numChunksI64 =
          builder.create<arith::DivUIOp>(loc, sumMinusOne, chunkSizeI64);
      Value numChunksVal = builder.create<arith::IndexCastOp>(
          loc, builder.getIndexType(), numChunksI64);
      /// Keep chunkSizeValue as index for elementSizes
      Value chunkSizeValue =
          builder.create<arith::ConstantIndexOp>(loc, chunkSizeVal);

      /// Build new sizes: [numChunks] (may be runtime)
      newOuterSizes.push_back(numChunksVal);

      /// Build new elementSizes: [chunkSize, D2, D3, ..., DN]
      newInnerSizes.push_back(chunkSizeValue);
      for (unsigned i = 1; i < elemSizes.size(); ++i)
        newInnerSizes.push_back(elemSizes[i]); /// Copy remaining dims
    }
  }

  ///==========================================================================
  /// 1D Chunking: For 1D allocations only
  /// Example: sizes=[1], elemSizes=[16M] -> sizes=[16], elemSizes=[1M]
  /// Uses partial evaluation for runtime-valued total elements
  ///==========================================================================
  if (!forceElementWise && newOuterSizes.empty() &&
      allocOp.getElementSizes().size() == 1 && chunkSizeVal > 0) {
    Value totalElementsVal = allocOp.getElementSizes()[0];

    /// Compute numChunks (requires total elements to be constant)
    std::optional<int64_t> numChunksOpt;
    int64_t totalElements;
    if (ValueUtils::getConstantIndex(totalElementsVal, totalElements) &&
        totalElements > chunkSizeVal) {
      numChunksOpt = (totalElements + chunkSizeVal - 1) / chunkSizeVal;
    }
    /// If total elements is runtime -> numChunksOpt stays nullopt

    /// innerBytes is always known for 1D (chunkSize * elemBytes)
    int64_t innerBytes = chunkSizeVal * elemBytes;

    /// Call H2 with partial information
    /// Use ParallelFor source since we're in promoteAllocForChunking
    auto &heuristics = AM->getHeuristicsConfig();
    auto decision = heuristics.shouldUseFineGrained(
        numChunksOpt,              /// May be nullopt if totalElements runtime
        std::optional<int64_t>(1), /// depsPerEDT always known
        std::optional<int64_t>(innerBytes), /// Always known for 1D
        PartitioningSource::ParallelFor /// Prioritize chunking for parallel_for
    );

    /// decision: true=approved, false=rejected, nullopt=partial pass
    bool chunkingApproved = !decision.has_value() || decision.value();

    if (chunkingApproved) {
      ARTS_DEBUG(
          "  Using 1D chunked allocation with chunkSize=" << chunkSizeVal);
      useChunked = true;

      /// Generate runtime numChunks computation
      /// Compute ceiling division manually: (totalElements + chunkSize - 1) /
      /// chunkSize Using arith.divui instead of arith.ceildivui for reliable
      /// LLVM lowering
      Type i64Type = builder.getI64Type();
      Value totalElementsI64 =
          builder.create<arith::IndexCastOp>(loc, i64Type, totalElementsVal);
      Value chunkSizeI64 =
          builder.create<arith::ConstantIntOp>(loc, chunkSizeVal, 64);
      Value oneI64 = builder.create<arith::ConstantIntOp>(loc, 1, 64);
      /// numChunks = (totalElements + chunkSize - 1) / chunkSize
      Value sum =
          builder.create<arith::AddIOp>(loc, totalElementsI64, chunkSizeI64);
      Value sumMinusOne = builder.create<arith::SubIOp>(loc, sum, oneI64);
      Value numChunksI64 =
          builder.create<arith::DivUIOp>(loc, sumMinusOne, chunkSizeI64);
      Value numChunksVal = builder.create<arith::IndexCastOp>(
          loc, builder.getIndexType(), numChunksI64);
      /// Keep chunkSizeValue as index for elementSizes
      Value chunkSizeValue =
          builder.create<arith::ConstantIndexOp>(loc, chunkSizeVal);

      newOuterSizes.push_back(numChunksVal);
      newInnerSizes.push_back(chunkSizeValue);
    } else {
      ARTS_DEBUG(
          "  H2 rejects 1D chunked allocation: innerBytes=" << innerBytes);
    }
  }

  ///==========================================================================
  /// Element-wise fallback: sizes=[1], elementSizes=[N] -> sizes=[N],
  /// elementSizes=[1]
  ///==========================================================================
  if (newOuterSizes.empty()) {
    ARTS_DEBUG("  Falling back to element-wise promotion");

    /// Move first element dimension to outer, remaining stay as inner
    ValueRange elemSizes = allocOp.getElementSizes();
    if (!elemSizes.empty()) {
      newOuterSizes.push_back(elemSizes[0]);
      for (unsigned i = 1; i < elemSizes.size(); ++i)
        newInnerSizes.push_back(elemSizes[i]);
    }

    /// Ensure non-empty inner sizes
    if (newInnerSizes.empty())
      newInnerSizes.push_back(builder.create<arith::ConstantIndexOp>(loc, 1));
  }

  DbRewritePlan plan = makeRewritePlan(useChunked, newInnerSizes);

  /// Create DbRewriter and apply (dispatcher: no policy inside)
  DbRewriter rewriter(allocOp, newOuterSizes, newInnerSizes, acquireInfo, plan);
  auto result = rewriter.apply(builder);

  /// Transfer metadata if successful
  if (succeeded(result)) {
    AM->getMetadataManager().transferMetadata(allocOp, result.value());
  }

  return result;
}

///===----------------------------------------------------------------------===///
/// Analyze stencil patterns to detect out-of-bounds indices
///
/// For stencil patterns like depend(in: u[i-1], u[i], u[i+1]), the boundary
/// iterations may access out-of-bounds indices (e.g., u[-1] when i=0).
/// This analysis detects such patterns by:
/// 1. Finding DbAcquireOps with indexed dependencies
/// 2. Using LoopAnalysis::collectEnclosingLoops() to find enclosing loops
/// 3. Using LoopNode::analyzeIndexExpr to detect offset patterns (e.g., i-1,
/// i+1)
/// 4. Generating runtime bounds checks and replacing DbAcquireOps with
/// bounds_valid
///===----------------------------------------------------------------------===///

/// Check if op is in else branch of `if (iv == 0)` where iv is the loop IV.
/// If so, we know iv > 0 (since loop starts at 0), so idx = iv-1 >= 0.
static bool isLowerBoundGuaranteedByControlFlow(Operation *op, Value loopIV) {
  if (!loopIV)
    return false;

  /// Helper: check if value matches loopIV (directly or via index_cast)
  auto matchesIV = [&loopIV](Value v) {
    if (v == loopIV)
      return true;
    if (auto cast = v.getDefiningOp<arith::IndexCastOp>())
      return cast.getIn() == loopIV;
    return false;
  };

  /// Walk up parent chain looking for scf.if with condition `iv == 0`
  for (Operation *p = op->getParentOp(); p; p = p->getParentOp()) {
    auto ifOp = dyn_cast<scf::IfOp>(p);
    if (!ifOp || ifOp.getElseRegion().empty())
      continue;

    /// Check if we're in else region
    if (!ifOp.getElseRegion().isAncestor(op->getParentRegion()))
      continue;

    /// Check if condition is `iv == 0`
    auto cmp = ifOp.getCondition().getDefiningOp<arith::CmpIOp>();
    if (!cmp || cmp.getPredicate() != arith::CmpIPredicate::eq)
      continue;

    Value lhs = cmp.getLhs(), rhs = cmp.getRhs();
    if ((matchesIV(lhs) && ValueUtils::isZeroConstant(rhs)) ||
        (matchesIV(rhs) && ValueUtils::isZeroConstant(lhs))) {
      ARTS_DEBUG("  Lower bound guaranteed by control flow (else of iv==0)");
      return true;
    }
  }
  return false;
}

void DbPartitioningPass::analyzeStencilBounds() {
  ARTS_DEBUG_HEADER(AnalyzeStencilBounds);

  LoopAnalysis &loopAnalysis = AM->getLoopAnalysis();

  /// Collect DbAcquireOps that need bounds checking along with flags and loopIV
  SmallVector<std::tuple<DbAcquireOp, SmallVector<int64_t>, Value>> toModify;

  module.walk([&](DbAcquireOp acquireOp) {
    /// Skip if already has bounds_valid
    if (acquireOp.getBoundsValid())
      return;

    auto indices = acquireOp.getIndices();
    if (indices.empty())
      return;

    /// Use LoopAnalysis to find enclosing loops
    SmallVector<LoopNode *> enclosingLoops;
    loopAnalysis.collectEnclosingLoops(acquireOp, enclosingLoops);
    if (enclosingLoops.empty())
      return;

    /// Use innermost enclosing loop for IV analysis
    LoopNode *loopNode = enclosingLoops.back();

    /// Compute boundsCheckFlags using analyzeIndexExpr (ONCE per index)
    SmallVector<int64_t> boundsCheckFlags;
    bool needsBoundsCheck = false;

    for (Value idx : indices) {
      int64_t flag = 0;
      LoopNode::IVExpr expr = loopNode->analyzeIndexExpr(idx);
      if (expr.isAnalyzable() && expr.offset.has_value() && *expr.offset != 0) {
        flag = 1;
        needsBoundsCheck = true;
        ARTS_DEBUG("  Index with offset " << *expr.offset
                                          << " needs bounds check");
      } else if (expr.dependsOnIV && !expr.isAnalyzable()) {
        flag = 1;
        needsBoundsCheck = true;
        ARTS_DEBUG("  Complex IV-dependent index needs bounds check");
      }
      boundsCheckFlags.push_back(flag);
    }

    if (needsBoundsCheck) {
      Value loopIV = loopNode->getInductionVar();
      toModify.push_back({acquireOp, std::move(boundsCheckFlags), loopIV});
    }
  });

  /// Generate boundsValid and update DbAcquireOps
  for (auto &[acquireOp, flags, loopIV] : toModify)
    generateBoundsValid(acquireOp, flags, loopIV);

  ARTS_DEBUG_FOOTER(AnalyzeStencilBounds);
}

/// Generate runtime bounds checks and create new DbAcquireOp with bounds_valid
void DbPartitioningPass::generateBoundsValid(DbAcquireOp acquireOp,
                                             ArrayRef<int64_t> boundsCheckFlags,
                                             Value loopIV) {
  Location loc = acquireOp.getLoc();
  OpBuilder builder(acquireOp);

  auto indices = acquireOp.getIndices();
  SmallVector<Value> sourceSizes =
      DatablockUtils::getSizesFromDb(acquireOp.getSourcePtr());

  /// Check if lower bound is guaranteed by control flow (inside else of `if
  /// iv==0`)
  bool lowerBoundGuarded =
      isLowerBoundGuaranteedByControlFlow(acquireOp, loopIV);

  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value boundsValid;

  for (size_t i = 0; i < boundsCheckFlags.size() && i < indices.size(); ++i) {
    if (boundsCheckFlags[i] != 0 && i < sourceSizes.size()) {
      Value idx = indices[i];
      Value size = sourceSizes[i];
      Value dimValid;

      if (lowerBoundGuarded) {
        /// Only check upper bound: idx < size (lower bound guaranteed by
        /// control flow)
        dimValid = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::slt,
                                                 idx, size);
      } else {
        /// Full check: 0 <= idx < size
        Value geZero = builder.create<arith::CmpIOp>(
            loc, arith::CmpIPredicate::sge, idx, zero);
        Value ltSize = builder.create<arith::CmpIOp>(
            loc, arith::CmpIPredicate::slt, idx, size);
        dimValid = builder.create<arith::AndIOp>(loc, geZero, ltSize);
      }

      boundsValid =
          boundsValid
              ? builder.create<arith::AndIOp>(loc, boundsValid, dimValid)
              : dimValid;
    }
  }

  if (!boundsValid)
    return;

  /// Recreate DbAcquireOp with bounds_valid
  SmallVector<Value> indicesVec(indices.begin(), indices.end());
  SmallVector<Value> offsetsVec(acquireOp.getOffsets().begin(),
                                acquireOp.getOffsets().end());
  SmallVector<Value> sizesVec(acquireOp.getSizes().begin(),
                              acquireOp.getSizes().end());
  SmallVector<Value> offsetHintsVec(acquireOp.getOffsetHints().begin(),
                                    acquireOp.getOffsetHints().end());
  SmallVector<Value> sizeHintsVec(acquireOp.getSizeHints().begin(),
                                  acquireOp.getSizeHints().end());

  auto newAcquire = builder.create<DbAcquireOp>(
      loc, acquireOp.getMode(), acquireOp.getSourceGuid(),
      acquireOp.getSourcePtr(), indicesVec, offsetsVec, sizesVec,
      offsetHintsVec, sizeHintsVec, boundsValid);
  transferAttributes(acquireOp, newAcquire);

  acquireOp.getGuid().replaceAllUsesWith(newAcquire.getGuid());
  acquireOp.getPtr().replaceAllUsesWith(newAcquire.getPtr());
  acquireOp.erase();

  ARTS_DEBUG("Added bounds_valid to DbAcquireOp: " << newAcquire);
}

///===----------------------------------------------------------------------===///
/// Invalidate and rebuild the graph
///===----------------------------------------------------------------------===///
void DbPartitioningPass::invalidateAndRebuildGraph() {
  module.walk([&](func::FuncOp func) {
    AM->invalidateFunction(func);
    (void)AM->getDbGraph(func);
  });
}

////===----------------------------------------------------------------------===////
/// Pass creation
////===----------------------------------------------------------------------===////
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbPartitioningPass(ArtsAnalysisManager *AM) {
  return std::make_unique<DbPartitioningPass>(AM);
}
} // namespace arts
} // namespace mlir
