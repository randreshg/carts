///==========================================================================///
/// File: DbPartitioning.cpp
/// Datablock partitioning and chunking transformations.
/// This pass chooses a datablock partitioning strategy with two goals:
///   1) Preserve correctness via conservative twin-diff use.
///   2) Maximize concurrency while avoiding unnecessary data movement.
///
/// For detailed documentation on partitioning decisions and per-acquire voting,
/// see: docs/heuristics/partitioning/partitioning.md
///
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
#include "arts/Utils/OperationAttributes.h"
ARTS_DEBUG_SETUP(db_partitioning);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;

namespace {

///===----------------------------------------------------------------------===///
/// Partition Mode Conversion Helper
///===----------------------------------------------------------------------===///
static PromotionMode toPromotionMode(RewriterMode mode) {
  switch (mode) {
  case RewriterMode::Chunked:
    return PromotionMode::chunked;
  case RewriterMode::ElementWise:
  case RewriterMode::Stencil: // Stencil uses element-wise attribute for now
    return PromotionMode::element_wise;
  }
  return PromotionMode::none;
}

///===----------------------------------------------------------------------===///
/// AcquirePartitionInfo - Partition info computed per-acquire
///===----------------------------------------------------------------------===///
struct AcquirePartitionInfo {
  DbAcquireOp acquire;
  PartitionMode mode;    /// From DatablockUtils::getPartitionMode
  Value partitionOffset; /// Computed from hints or loop analysis
  Value partitionSize;   /// Computed chunk/element size (dynamic!)
  bool isValid = false;

  /// Check consistency with another acquire
  bool isConsistentWith(const AcquirePartitionInfo &other) const {
    if (mode != other.mode)
      return false;
    /// For Chunked mode, sizes must be same SSA value
    if (mode == PartitionMode::Chunked && partitionSize != other.partitionSize)
      return false;
    return true;
  }
};

/// Compute partition info for a single acquire
static AcquirePartitionInfo computeAcquirePartitionInfo(DbAcquireOp acquire,
                                                        DbAcquireNode *acqNode,
                                                        OpBuilder &builder,
                                                        Location loc) {
  AcquirePartitionInfo info;
  info.acquire = acquire;
  info.mode = DatablockUtils::getPartitionMode(acquire);

  switch (info.mode) {
  case PartitionMode::Coarse:
    /// No partitioning needed
    info.isValid = true;
    break;

  case PartitionMode::ElementWise:
    /// Use indices as partition offset
    if (!acquire.getIndices().empty()) {
      info.partitionOffset = acquire.getIndices().front();
      info.partitionSize = builder.create<arith::ConstantIndexOp>(loc, 1);
      info.isValid = true;
    }
    break;

  case PartitionMode::Chunked:
    /// Use offsets/sizes or compute from loop structure
    if (acqNode) {
      Value offset, size;
      if (succeeded(acqNode->computeChunkInfo(offset, size))) {
        info.partitionOffset = offset;
        info.partitionSize = size;
        info.isValid = true;
      } else {
        info.mode = PartitionMode::Coarse;
      }
    }
    break;
  }

  return info;
}

///===----------------------------------------------------------------------===///
/// Compute sizes from PartitioningDecision (uses outerRank/innerRank)
///
/// Allocation modes:
///   - Coarse: sizes=[1], elementSizes=all dims (single datablock)
///   - ElementWise: sizes=first outerRank dims, elementSizes=remaining dims
///   - Chunked: sizes=[numChunks], elementSizes=[chunkSize, remaining dims]
///===----------------------------------------------------------------------===///
static void computeSizesFromDecision(DbAllocOp allocOp,
                                     const PartitioningDecision &decision,
                                     Value chunkSize,
                                     SmallVector<Value> &newOuterSizes,
                                     SmallVector<Value> &newInnerSizes) {
  OpBuilder builder(allocOp);
  Location loc = allocOp.getLoc();
  ValueRange elemSizes = allocOp.getElementSizes();

  if (decision.isChunked() && chunkSize) {
    /// Chunked: numChunks = ceil(firstDim / chunkSize)
    Value firstDim = elemSizes[0];

    /// Compute ceiling division using arith ops
    /// numChunks = (firstDim + chunkSize - 1) / chunkSize
    Type i64Type = builder.getI64Type();
    Value firstDimI64 =
        builder.create<arith::IndexCastOp>(loc, i64Type, firstDim);
    Value chunkSizeI64 =
        builder.create<arith::IndexCastOp>(loc, i64Type, chunkSize);
    Value oneI64 = builder.create<arith::ConstantIntOp>(loc, 1, 64);
    Value sum = builder.create<arith::AddIOp>(loc, firstDimI64, chunkSizeI64);
    Value sumMinusOne = builder.create<arith::SubIOp>(loc, sum, oneI64);
    Value numChunksI64 =
        builder.create<arith::DivUIOp>(loc, sumMinusOne, chunkSizeI64);
    Value numChunks = builder.create<arith::IndexCastOp>(
        loc, builder.getIndexType(), numChunksI64);

    newOuterSizes.push_back(numChunks);
    newInnerSizes.push_back(chunkSize);

    /// Remaining dims go to inner
    for (unsigned i = 1; i < elemSizes.size(); ++i)
      newInnerSizes.push_back(elemSizes[i]);

  } else if (decision.isFineGrained()) {
    /// ElementWise: use outerRank from decision
    unsigned outerRank = decision.outerRank;

    /// First outerRank dims go to outer
    for (unsigned i = 0; i < outerRank && i < elemSizes.size(); ++i)
      newOuterSizes.push_back(elemSizes[i]);

    /// Remaining dims go to inner
    for (unsigned i = outerRank; i < elemSizes.size(); ++i)
      newInnerSizes.push_back(elemSizes[i]);

    /// Ensure non-empty inner
    if (newInnerSizes.empty())
      newInnerSizes.push_back(builder.create<arith::ConstantIndexOp>(loc, 1));

  } else {
    /// Coarse-grained: single datablock with all dims as element sizes
    /// sizes=[1], elementSizes=all element dimensions
    newOuterSizes.push_back(builder.create<arith::ConstantIndexOp>(loc, 1));
    for (Value dim : elemSizes)
      newInnerSizes.push_back(dim);
    /// Ensure non-empty inner
    if (newInnerSizes.empty())
      newInnerSizes.push_back(builder.create<arith::ConstantIndexOp>(loc, 1));
  }
}

/// Trace a value through EDT block arguments to find a dominating value.
/// If the value is an EDT block argument, trace to the corresponding
/// dependency. Recursively handles nested EDT structures.
static Value traceValueThroughEdt(Value v, DbAllocOp allocOp,
                                  DominanceInfo &domInfo, unsigned depth = 0) {
  if (!v || depth > 16)
    return nullptr;

  /// If the value already dominates, return it directly
  if (domInfo.properlyDominates(v, allocOp))
    return v;

  /// Handle EDT block arguments
  if (auto blockArg = dyn_cast<BlockArgument>(v)) {
    Block *block = blockArg.getOwner();
    if (auto edt = dyn_cast<EdtOp>(block->getParentOp())) {
      unsigned argIndex = blockArg.getArgNumber();
      ValueRange deps = edt.getDependencies();
      if (argIndex < deps.size()) {
        Value dep = deps[argIndex];
        ARTS_DEBUG("    Tracing block arg #" << argIndex << " through EDT");
        /// Recursively trace (could be nested EDTs or DbAcquire)
        return traceValueThroughEdt(dep, allocOp, domInfo, depth + 1);
      }
    }
  }

  /// Handle DbAcquireOp - trace to source
  if (auto acqOp = v.getDefiningOp<DbAcquireOp>()) {
    /// For firstprivate values, the acquire's source might dominate
    if (Value src = acqOp.getSourcePtr())
      return traceValueThroughEdt(src, allocOp, domInfo, depth + 1);
  }

  return nullptr;
}

/// Trace back through the defining operation to recreate chunk size
/// at a point that dominates the allocation.
///
/// Handles common patterns:
///   - arith.divsi %N, %num_tasks  (block_size = N / tasks)
///   - arith.divui %N, %num_tasks
///   - arith.ceildivsi/ceildivui for ceiling division
///   - index_cast wrapping any of the above
///
/// Also traces operands through EDT block arguments when needed.
///
static Value traceBackChunkSize(Value chunkSize, DbAllocOp allocOp,
                                OpBuilder &builder, DominanceInfo &domInfo,
                                Location loc) {
  Operation *defOp = chunkSize.getDefiningOp();
  if (!defOp) {
    /// Chunk size is a block argument - try to trace through EDT
    ARTS_DEBUG("  Trace-back: chunk size has no defining op (block arg)");
    if (auto blockArg = dyn_cast<BlockArgument>(chunkSize)) {
      if (Value traced = traceValueThroughEdt(chunkSize, allocOp, domInfo)) {
        ARTS_DEBUG("  Traced block arg to dominating value");
        return traced;
      }
    }
    return nullptr;
  }

  ARTS_DEBUG("  Trace-back: examining " << defOp->getName().getStringRef());

  /// Helper to get dominating operand - direct or via EDT trace
  auto getDominatingOperand = [&](Value operand) -> Value {
    if (domInfo.properlyDominates(operand, allocOp))
      return operand;
    /// Try to trace through EDT block arguments
    return traceValueThroughEdt(operand, allocOp, domInfo);
  };

  /// Helper to get a dominating operand - check dominance first, then
  /// trace-back This order is important: values like scf.while results may
  /// dominate directly
  auto getTracedOrDominatingOperand = [&](Value operand) -> Value {
    /// First check if it dominates directly (handles scf.while results, etc.)
    if (Value dom = getDominatingOperand(operand))
      return dom;
    /// Only try trace-back if direct dominance check fails
    return traceBackChunkSize(operand, allocOp, builder, domInfo, loc);
  };

  /// Pattern 1: arith.divsi
  if (auto divOp = dyn_cast<arith::DivSIOp>(defOp)) {
    Value lhs = getTracedOrDominatingOperand(divOp.getLhs());
    Value rhs = getTracedOrDominatingOperand(divOp.getRhs());
    if (lhs && rhs) {
      builder.setInsertionPoint(allocOp);
      ARTS_DEBUG("  Recreating divsi at allocation point");
      return builder.create<arith::DivSIOp>(loc, lhs, rhs);
    } else {
      ARTS_DEBUG("  divsi operands don't dominate: lhs="
                 << (lhs ? "ok" : "no") << " rhs=" << (rhs ? "ok" : "no"));
    }
  }

  /// Pattern 2: arith.divui
  if (auto divOp = dyn_cast<arith::DivUIOp>(defOp)) {
    Value lhs = getTracedOrDominatingOperand(divOp.getLhs());
    Value rhs = getTracedOrDominatingOperand(divOp.getRhs());
    if (lhs && rhs) {
      builder.setInsertionPoint(allocOp);
      ARTS_DEBUG("  Recreating divui at allocation point");
      return builder.create<arith::DivUIOp>(loc, lhs, rhs);
    }
  }

  /// Pattern 3: arith.ceildivsi
  if (auto divOp = dyn_cast<arith::CeilDivSIOp>(defOp)) {
    Value lhs = getTracedOrDominatingOperand(divOp.getLhs());
    Value rhs = getTracedOrDominatingOperand(divOp.getRhs());
    if (lhs && rhs) {
      builder.setInsertionPoint(allocOp);
      ARTS_DEBUG("  Recreating ceildivsi at allocation point");
      return builder.create<arith::CeilDivSIOp>(loc, lhs, rhs);
    }
  }

  /// Pattern 4: arith.ceildivui
  if (auto divOp = dyn_cast<arith::CeilDivUIOp>(defOp)) {
    Value lhs = getTracedOrDominatingOperand(divOp.getLhs());
    Value rhs = getTracedOrDominatingOperand(divOp.getRhs());
    if (lhs && rhs) {
      builder.setInsertionPoint(allocOp);
      ARTS_DEBUG("  Recreating ceildivui at allocation point");
      return builder.create<arith::CeilDivUIOp>(loc, lhs, rhs);
    }
  }

  /// Pattern 5: Recursive trace-back for index_cast
  /// e.g., index_cast(divsi(...))
  if (auto castOp = dyn_cast<arith::IndexCastOp>(defOp)) {
    if (Value inner = traceBackChunkSize(castOp.getIn(), allocOp, builder,
                                         domInfo, loc)) {
      ARTS_DEBUG("  Wrapping with index_cast");
      if (inner.getType() == castOp.getType())
        return inner;
      return builder.create<arith::IndexCastOp>(loc, castOp.getType(), inner);
    }
  }

  /// Pattern 6: arith.maxsi - clamping chunk size to non-negative
  /// e.g., maxsi(divsi(...), 0)
  if (auto maxOp = dyn_cast<arith::MaxSIOp>(defOp)) {
    Value lhs = maxOp.getLhs();
    Value rhs = maxOp.getRhs();
    Value lhsTraced = getTracedOrDominatingOperand(lhs);
    Value rhsTraced = getTracedOrDominatingOperand(rhs);
    if (lhsTraced && rhsTraced) {
      ARTS_DEBUG("  Recreating maxsi at allocation point");
      return builder.create<arith::MaxSIOp>(loc, lhsTraced, rhsTraced);
    }
  }

  /// Pattern 6.5: arith.select - conditional chunk size (e.g., abs/min guards)
  if (auto selectOp = dyn_cast<arith::SelectOp>(defOp)) {
    Value trueVal = selectOp.getTrueValue();
    Value falseVal = selectOp.getFalseValue();
    Value trueTraced = getTracedOrDominatingOperand(trueVal);
    Value falseTraced = getTracedOrDominatingOperand(falseVal);
    ARTS_DEBUG("  Select trace: true=" << (trueTraced ? "ok" : "no")
                                       << " false="
                                       << (falseTraced ? "ok" : "no"));
    if (trueTraced && falseTraced) {
      Value condDom = getDominatingOperand(selectOp.getCondition());
      builder.setInsertionPoint(allocOp);
      if (condDom) {
        ARTS_DEBUG("  Recreating select at allocation point");
        return builder.create<arith::SelectOp>(loc, condDom, trueTraced,
                                               falseTraced);
      }
      ARTS_DEBUG("  Select condition doesn't dominate - using max of arms");
      return builder.create<arith::MaxSIOp>(loc, trueTraced, falseTraced);
    }
    if (trueTraced && !falseTraced) {
      ARTS_DEBUG("  Using true arm of select as max chunk size");
      return trueTraced;
    }
    if (falseTraced && !trueTraced) {
      ARTS_DEBUG("  Using false arm of select as max chunk size");
      return falseTraced;
    }
  }

  /// Pattern 7: arith.minsi - clamping chunk size to max bound
  /// For allocation, we use the MAX chunk size (the clamping bound).
  /// Pattern: minsi(variable_bound, max_chunk) -> use max_chunk for allocation
  if (auto minOp = dyn_cast<arith::MinSIOp>(defOp)) {
    Value lhs = minOp.getLhs();
    Value rhs = minOp.getRhs();
    Value lhsTraced = getTracedOrDominatingOperand(lhs);
    Value rhsTraced = getTracedOrDominatingOperand(rhs);
    if (lhsTraced && rhsTraced) {
      ARTS_DEBUG("  Recreating minsi at allocation point");
      return builder.create<arith::MinSIOp>(loc, lhsTraced, rhsTraced);
    }
    /// If only one operand traces successfully, use it as max chunk size
    /// This handles: minsi(start_row + block_size, output_size) where
    /// block_size dominates but start_row doesn't
    if (rhsTraced && !lhsTraced) {
      ARTS_DEBUG("  Using rhs of minsi as max chunk size");
      return rhsTraced;
    }
    if (lhsTraced && !rhsTraced) {
      ARTS_DEBUG("  Using lhs of minsi as max chunk size");
      return lhsTraced;
    }
  }

  /// Pattern 8: arith.subi - subtraction in chunk size computation
  if (auto subOp = dyn_cast<arith::SubIOp>(defOp)) {
    Value lhs = subOp.getLhs();
    Value rhs = subOp.getRhs();
    Value lhsTraced = getTracedOrDominatingOperand(lhs);
    Value rhsTraced = getTracedOrDominatingOperand(rhs);
    if (lhsTraced && rhsTraced) {
      ARTS_DEBUG("  Recreating subi at allocation point");
      return builder.create<arith::SubIOp>(loc, lhsTraced, rhsTraced);
    }
  }

  /// Pattern 9: arith.addi - addition in chunk size computation
  if (auto addOp = dyn_cast<arith::AddIOp>(defOp)) {
    Value lhs = addOp.getLhs();
    Value rhs = addOp.getRhs();
    Value lhsTraced = getTracedOrDominatingOperand(lhs);
    Value rhsTraced = getTracedOrDominatingOperand(rhs);
    if (lhsTraced && rhsTraced) {
      ARTS_DEBUG("  Recreating addi at allocation point");
      return builder.create<arith::AddIOp>(loc, lhsTraced, rhsTraced);
    }
  }

  /// Pattern 10: arith.muli - multiplication in chunk size computation
  if (auto mulOp = dyn_cast<arith::MulIOp>(defOp)) {
    Value lhs = mulOp.getLhs();
    Value rhs = mulOp.getRhs();
    Value lhsTraced = getTracedOrDominatingOperand(lhs);
    Value rhsTraced = getTracedOrDominatingOperand(rhs);
    if (lhsTraced && rhsTraced) {
      ARTS_DEBUG("  Recreating muli at allocation point");
      return builder.create<arith::MulIOp>(loc, lhsTraced, rhsTraced);
    }
  }

  /// Pattern 11: arith.constant - constants can just be recreated
  if (auto constOp = dyn_cast<arith::ConstantOp>(defOp)) {
    builder.setInsertionPoint(allocOp);
    ARTS_DEBUG("  Recreating constant at allocation point");
    return builder.create<arith::ConstantOp>(loc, constOp.getType(),
                                             constOp.getValue());
  }
  if (auto constIdxOp = dyn_cast<arith::ConstantIndexOp>(defOp)) {
    builder.setInsertionPoint(allocOp);
    ARTS_DEBUG("  Recreating constant index at allocation point");
    return builder.create<arith::ConstantIndexOp>(loc, constIdxOp.value());
  }

  return nullptr; /// Cannot trace back
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

  /// Partition an allocation based on per-acquire analysis and heuristics
  FailureOr<DbAllocOp> partitionAlloc(DbAllocOp allocOp,
                                      DbAllocNode *allocNode);

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

  /// Walk the module and partition each allocation.
  /// partitionAlloc handles all validation and analysis internally.
  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    /// Iterate through allocations and attempt partitioning
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      if (!allocNode)
        return;

      DbAllocOp allocOp = allocNode->getDbAllocOp();
      if (!allocOp)
        return;

      partitionAttempt.insert(allocOp.getOperation());
      ARTS_DEBUG("Checking allocation: " << allocOp);

      /// Pre-check: can this allocation be partitioned at all?
      if (!allocNode->canBePartitioned()) {
        ARTS_DEBUG("  SKIP: canBePartitioned() returned false");
        partitionFailed.insert(allocOp.getOperation());
        return;
      }

      /// Call partitionAlloc - it handles all analysis and consistency checks
      auto promotedOr = partitionAlloc(allocOp, allocNode);
      if (failed(promotedOr)) {
        partitionFailed.insert(allocOp.getOperation());
        return;
      }

      DbAllocOp promoted = promotedOr.value();
      if (promoted.getOperation() == allocOp.getOperation()) {
        ARTS_DEBUG("  Alloc kept original (coarse or already partitioned)");
      } else {
        ARTS_DEBUG(
            "  Partitioned alloc to sizes=" << promoted.getSizes().size());
        changed = true;
        partitionSuccess.insert(promoted.getOperation());

        /// Set twin_diff=false for all child acquires (partition success)
        allocNode->forEachChildNode([&](NodeBase *child) {
          if (auto *acqNode = dyn_cast<DbAcquireNode>(child)) {
            setTwinAttr(acqNode->getDbAcquireOp(),
                        TwinDiffProof::PartitionSuccess);
          }
        });
      }
    });
  });

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

/// Partition an allocation based on per-acquire voting and heuristics.
/// See docs/heuristics/partitioning/partitioning.md for detailed documentation.
///
/// Flow:
///   1. Check existing partition attribute - skip if already partitioned
///   2. Analyze each acquire's PartitionMode and collect AcquireInfo
///   3. Build PartitioningContext with per-acquire voting info
///   4. Evaluate heuristics 
///   5. Apply partitioning decision (outerRank/innerRank)
///   6. Set partition attribute on new alloc
FailureOr<DbAllocOp>
DbPartitioningPass::partitionAlloc(DbAllocOp allocOp, DbAllocNode *allocNode) {
  if (!allocOp || !allocOp.getOperation())
    return failure();

  if (allocOp.getElementSizes().empty())
    return failure();

  auto &heuristics = AM->getHeuristicsConfig();

  /// Step 0: Check existing partition attribute
  if (auto existingMode = getPartitionMode(allocOp)) {
    /// Only skip if SUCCESSFULLY partitioned (element_wise or chunked)
    if (*existingMode != PromotionMode::none) {
      const char *modeName = (*existingMode == PromotionMode::element_wise)
                                 ? "element_wise"
                                 : "chunked";
      ARTS_DEBUG("  Already partitioned as " << modeName << " - SKIP");
      heuristics.recordDecision(
          "Partition-AlreadyPartitioned", false,
          "allocation already has partition attribute, skipping",
          allocOp.getOperation(), {});
      return allocOp; /// Skip - already partitioned!
    }
    /// mode == none means CreateDbs couldn't partition - continue to re-analyze
    ARTS_DEBUG("  Partition mode is none - re-analyzing with loop structure");
    heuristics.recordDecision(
        "Partition-ReanalyzeNone", true,
        "CreateDbs set none, attempting loop-based chunk detection",
        allocOp.getOperation(), {});
  }

  /// Already fine-grained by structure - skip
  if (allocNode && allocNode->isFineGrained()) {
    ARTS_DEBUG("  Already fine-grained by structure - SKIP");
    heuristics.recordDecision(
        "Partition-AlreadyFineGrained", false,
        "allocation already fine-grained by structure, skipping",
        allocOp.getOperation(), {});
    return allocOp;
  }

  OpBuilder builder(allocOp);
  Location loc = allocOp.getLoc();
  builder.setInsertionPoint(allocOp);

  /// Step 1: Analyze each acquire's PartitionMode
  SmallVector<AcquirePartitionInfo> acquireInfos;
  if (allocNode) {
    allocNode->forEachChildNode([&](NodeBase *child) {
      auto *acqNode = dyn_cast<DbAcquireNode>(child);
      if (!acqNode)
        return;

      auto info = computeAcquirePartitionInfo(acqNode->getDbAcquireOp(),
                                              acqNode, builder, loc);
      if (!info.isValid) {
        ARTS_DEBUG("  Acquire analysis failed for "
                   << acqNode->getDbAcquireOp());
      }
      acquireInfos.push_back(info);
    });
  }

  if (acquireInfos.empty()) {
    ARTS_DEBUG("  No acquires to analyze - keeping original");
    heuristics.recordDecision(
        "Partition-NoAcquires", false,
        "no acquires to analyze, keeping original allocation",
        allocOp.getOperation(), {});
    return allocOp;
  }

  /// Step 2: Determine partition capabilities from acquires
  const auto &reference = acquireInfos.front();
  PartitionMode consistentMode = reference.mode;
  bool modesConsistent = true;

  for (size_t i = 1; i < acquireInfos.size(); ++i) {
    if (!reference.isConsistentWith(acquireInfos[i])) {
      modesConsistent = false;
      ARTS_DEBUG("  Inconsistent partition modes across acquires");
      break;
    }
  }

  /// Step 3: Build PartitioningContext
  PartitioningContext ctx;
  ctx.machine = &AM->getAbstractMachine();

  /// Get access patterns FIRST - needed by H1.5 StencilPatternHeuristic
  if (allocNode) {
    ctx.accessPatterns = allocNode->summarizeAcquirePatterns();
    ARTS_DEBUG("  Access patterns: hasUniform="
               << ctx.accessPatterns.hasUniform
               << ", hasStencil=" << ctx.accessPatterns.hasStencil
               << ", hasIndexed=" << ctx.accessPatterns.hasIndexed
               << ", isMixed=" << ctx.accessPatterns.isMixed());
  }

  /// Populate per-acquire info for heuristic voting
  if (allocNode) {
    size_t idx = 0;
    allocNode->forEachChildNode([&](NodeBase *child) {
      auto *acqNode = dyn_cast<DbAcquireNode>(child);
      if (!acqNode || idx >= acquireInfos.size())
        return;

      AcquireInfo info;
      info.accessMode = acqNode->getDbAcquireOp().getMode();
      info.canElementWise =
          (acquireInfos[idx].mode == PartitionMode::ElementWise);
      info.canChunked = (acquireInfos[idx].mode == PartitionMode::Chunked);
      info.pinnedDimCount = info.canElementWise ? 1 : 0;

      ctx.acquires.push_back(info);
      ARTS_DEBUG("    Acquire["
                 << idx << "]: mode=" << static_cast<int>(info.accessMode)
                 << ", canEW=" << info.canElementWise
                 << ", canChunked=" << info.canChunked);
      ++idx;
    });
  }

  /// Set capabilities based on mode consistency
  if (modesConsistent) {
    ARTS_DEBUG(
      "  Consistent partition mode: " << static_cast<int>(consistentMode));
    /// All consistent and coarse → no partitioning needed
    if (consistentMode == PartitionMode::Coarse) {
      ARTS_DEBUG("  Coarse mode - no partitioning needed");
      heuristics.recordDecision(
          "Partition-CoarseMode", false,
          "all acquires are coarse mode, no partitioning needed",
          allocOp.getOperation(), {});
      return allocOp;
    }
    ctx.canElementWise = (consistentMode == PartitionMode::ElementWise);
    ctx.canChunked = (consistentMode == PartitionMode::Chunked);
  } else {
    /// Inconsistent modes: use per-acquire voting to determine capabilities
    ctx.canElementWise = ctx.anyCanElementWise();
    ctx.canChunked = ctx.anyCanChunked();
    // If neither is true but we have acquires, allow element-wise as fallback
    if (!ctx.canElementWise && !ctx.canChunked && !ctx.acquires.empty())
      ctx.canElementWise = true;
    ARTS_DEBUG("  Inconsistent modes - using per-acquire voting: canEW="
               << ctx.canElementWise << ", canChunked=" << ctx.canChunked);
  }

  /// Set pinnedDimCount from per-acquire info (max across all acquires)
  ctx.pinnedDimCount = ctx.maxPinnedDimCount();

  /// Get access mode from allocNode
  if (allocNode)
    ctx.accessMode = allocOp.getMode();

  /// Get chunk size for Chunked mode (extract static value if possible)
  Value dynamicChunkSize;
  if (consistentMode == PartitionMode::Chunked && reference.partitionSize) {
    dynamicChunkSize = reference.partitionSize;
    if (auto staticSize =
            arts::extractChunkSizeFromHint(reference.partitionSize))
      ctx.chunkSize = *staticSize;
  }

  /// Get memref rank
  if (auto memrefType = allocOp.getElementType().dyn_cast<MemRefType>())
    ctx.memrefRank = memrefType.getRank();
  else
    ctx.memrefRank = allocOp.getElementSizes().size();

  /// Step 4: Query heuristics → PartitioningDecision
  PartitioningDecision decision = heuristics.getPartitioningMode(ctx);

  if (decision.isCoarse()) {
    ARTS_DEBUG("  Heuristics chose coarse - keeping original");
    heuristics.recordDecision(
        "Partition-HeuristicsCoarse", false,
        "heuristics chose coarse allocation, keeping original",
        allocOp.getOperation(),
        {{"canChunked", ctx.canChunked ? 1 : 0},
         {"canElementWise", ctx.canElementWise ? 1 : 0}});
    return allocOp;
  }

  ARTS_DEBUG("  Decision: mode=" << getRewriterModeName(decision.mode)
                                 << ", outerRank=" << decision.outerRank
                                 << ", innerRank=" << decision.innerRank);

  /// Step 5: Compute sizes from decision (uses outerRank/innerRank)
  /// For chunked mode, we need a chunk size value that dominates the allocOp.
  /// Prefer static chunk size (recreate as constant), fall back to dynamic only
  /// if it dominates.
  Value chunkSizeForPlan;
  if (decision.isChunked()) {
    if (ctx.chunkSize && *ctx.chunkSize > 0) {
      /// Use static chunk size - create a constant at the allocOp point
      chunkSizeForPlan =
          builder.create<arith::ConstantIndexOp>(loc, *ctx.chunkSize);
    } else if (dynamicChunkSize) {
      /// Check if the dynamic chunk size dominates the allocOp
      DominanceInfo domInfo(allocOp->getParentOfType<func::FuncOp>());
      if (domInfo.properlyDominates(dynamicChunkSize, allocOp)) {
        chunkSizeForPlan = dynamicChunkSize;
      } else {
        /// Try to trace back and recreate the chunk size at allocation point
        ARTS_DEBUG("  Dynamic chunk size doesn't dominate - attempting "
                   "trace-back");
        chunkSizeForPlan = traceBackChunkSize(dynamicChunkSize, allocOp,
                                              builder, domInfo, loc);
        if (chunkSizeForPlan) {
          heuristics.recordDecision(
              "Partition-ChunkSizeRecreated", true,
              "recreated chunk size at allocation point via trace-back",
              allocOp.getOperation(), {});
        } else {
          /// Cannot trace back - fall back to coarse
          ARTS_DEBUG("  Cannot recreate chunk size - falling back to coarse");
          heuristics.recordDecision(
              "Partition-ChunkSizeNotDominating", false,
              "dynamic chunk size doesn't dominate and cannot be recreated",
              allocOp.getOperation(), {});
          return allocOp;
        }
      }
    }
  }

  SmallVector<Value> newOuterSizes, newInnerSizes;
  computeSizesFromDecision(allocOp, decision, chunkSizeForPlan, newOuterSizes,
                           newInnerSizes);

  if (newOuterSizes.empty()) {
    ARTS_DEBUG("  Size computation failed - keeping original");
    heuristics.recordDecision(
        "Partition-SizeComputationFailed", false,
        "failed to compute partition sizes, keeping original",
        allocOp.getOperation(), {});
    return allocOp;
  }

  /// Step 6: Apply DbRewriter
  DbRewritePlan plan(decision);
  plan.chunkSize = chunkSizeForPlan; /// Dynamic Value supported!

  /// Build DbRewriteAcquire list from AcquirePartitionInfo
  SmallVector<DbRewriteAcquire> rewriteAcquires;
  for (const auto &info : acquireInfos) {
    if (info.isValid && info.acquire) {
      rewriteAcquires.push_back(
          {info.acquire, info.partitionOffset, info.partitionSize});
    }
  }

  DbRewriter rewriter(allocOp, newOuterSizes, newInnerSizes, rewriteAcquires,
                      plan);
  auto result = rewriter.apply(builder);

  /// Step 7: Set partition attribute on new alloc
  if (succeeded(result)) {
    setPartitionMode(*result, toPromotionMode(decision.mode));
    AM->getMetadataManager().transferMetadata(allocOp, result.value());

    /// Record successful partition decision
    StringRef modeName = getRewriterModeName(decision.mode);
    ARTS_DEBUG("  Set partition attribute: " << modeName);
    heuristics.recordDecision(
        "Partition-Success", true,
        ("successfully partitioned allocation to " + modeName).str(),
        result.value().getOperation(),
        {{"outerRank", static_cast<int64_t>(decision.outerRank)},
         {"innerRank", static_cast<int64_t>(decision.innerRank)},
         {"acquireCount", static_cast<int64_t>(acquireInfos.size())}});
  } else {
    /// Record partition failure
    heuristics.recordDecision(
        "Partition-RewriterFailed", false,
        "DbRewriter failed to apply partition transformation",
        allocOp.getOperation(), {});
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
