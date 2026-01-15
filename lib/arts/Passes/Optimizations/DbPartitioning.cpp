///==========================================================================///
/// DbPartitioning.cpp - Datablock partitioning decision pass
///
/// Analyzes allocations and selects partitioning strategies:
///   Phase 1: Per-acquire analysis (chunk offset/size, capabilities)
///   Phase 2: Heuristic voting → RewriterMode
///   (Coarse/ElementWise/Chunked/Stencil) Phase 3: Size computation and
///   rewriter application
///
/// Twin-diff is enabled when disjoint access cannot be proven.
///==========================================================================///

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
  case RewriterMode::Stencil:
    return PromotionMode::chunked;
  case RewriterMode::ElementWise:
    return PromotionMode::fine_grained;
  }
  return PromotionMode::coarse;
}

/// Per-acquire partition analysis results for Phase 1.
struct AcquirePartitionInfo {
  DbAcquireOp acquire;
  PartitionMode mode;    /// From DatablockUtils::getPartitionMode
  Value partitionOffset; /// Computed from hints or loop analysis
  Value partitionSize;   /// Computed chunk/element size (dynamic!)
  bool isValid = false;
  /// For mixed mode: this coarse acquire needs full-range access to chunked
  /// alloc.
  bool needsFullRange = false;

  /// Check consistency with another acquire.
  /// Returns true if both acquires can use the same partitioning strategy.
  bool isConsistentWith(const AcquirePartitionInfo &other) const {
    if (mode != other.mode)
      return false;
    /// For Chunked mode, sizes must be same SSA value
    if (mode == PartitionMode::Chunked &&
        partitionSize != other.partitionSize) {
      int64_t lhs = 0, rhs = 0;
      bool lhsConst = ValueUtils::getConstantIndex(partitionSize, lhs);
      bool rhsConst = ValueUtils::getConstantIndex(other.partitionSize, rhs);
      if (!(lhsConst && rhsConst && lhs == rhs))
        return false;
    }
    return true;
  }
};

/// Extract partition offset with cascading fallback: info → node → hints →
/// offsets.
static Value getPartitionOffset(DbAcquireNode *acqNode,
                                const AcquirePartitionInfo *info) {
  if (info && info->partitionOffset)
    return info->partitionOffset;

  auto [offset, size] = acqNode->getPartitionInfo();
  if (offset)
    return offset;

  DbAcquireOp acqOp = acqNode->getDbAcquireOp();
  auto hints = acqOp.getOffsetHints();
  if (!hints.empty())
    return hints.front();

  auto offsets = acqOp.getOffsets();
  if (!offsets.empty())
    return offsets.front();

  return nullptr;
}

/// Analyze a single acquire to determine its partition mode and chunk info.
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
    if (acqNode) {
      Value offset, size;
      if (succeeded(acqNode->computeChunkInfo(offset, size))) {
        info.partitionOffset = offset;
        info.partitionSize = size;
      } else {
        info.isValid = false;
      }
    } else {
      info.isValid = false;
    }
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
        /// Validation failed -> fall back to coarse for correctness.
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
    Value strippedChunkSize = ValueUtils::stripNumericCasts(chunkSize);
    if (ValueUtils::isZeroConstant(strippedChunkSize))
      return;
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

/// Trace chunk size through arithmetic ops to recreate at allocation point.
/// Supports division, bounds (minsi/maxsi), select, arithmetic, and casts.
/// For minsi/select, uses partial operand fallback when only one arm traces.
static Value traceBackChunkSize(Value chunkSize, DbAllocOp allocOp,
                                OpBuilder &builder, DominanceInfo &domInfo,
                                Location loc);

/// Check if operand dominates allocation, or trace through EDT block args.
static Value getDominatingOperand(Value operand, DbAllocOp allocOp,
                                  DominanceInfo &domInfo) {
  if (domInfo.properlyDominates(operand, allocOp))
    return operand;
  return traceValueThroughEdt(operand, allocOp, domInfo);
}

/// Check dominance first, then attempt full trace-back with reconstruction.
static Value getTracedOrDominatingOperand(Value operand, DbAllocOp allocOp,
                                          OpBuilder &builder,
                                          DominanceInfo &domInfo,
                                          Location loc) {
  if (Value dom = getDominatingOperand(operand, allocOp, domInfo))
    return dom;
  return traceBackChunkSize(operand, allocOp, builder, domInfo, loc);
}

/// Trace binary op: trace both operands, recreate if both succeed.
template <typename OpType>
static Value traceBinaryOp(OpType op, DbAllocOp allocOp, OpBuilder &builder,
                           DominanceInfo &domInfo, Location loc,
                           StringRef opName) {
  Value lhs =
      getTracedOrDominatingOperand(op.getLhs(), allocOp, builder, domInfo, loc);
  Value rhs =
      getTracedOrDominatingOperand(op.getRhs(), allocOp, builder, domInfo, loc);
  if (lhs && rhs) {
    builder.setInsertionPoint(allocOp);
    ARTS_DEBUG("  Recreating " << opName << " at allocation point");
    return builder.create<OpType>(loc, lhs, rhs);
  }
  return nullptr;
}

/// Trace minsi with partial operand fallback.
static Value traceMinSI(arith::MinSIOp minOp, DbAllocOp allocOp,
                        OpBuilder &builder, DominanceInfo &domInfo,
                        Location loc) {
  Value lhsTraced = getTracedOrDominatingOperand(minOp.getLhs(), allocOp,
                                                 builder, domInfo, loc);
  Value rhsTraced = getTracedOrDominatingOperand(minOp.getRhs(), allocOp,
                                                 builder, domInfo, loc);
  if (lhsTraced && rhsTraced) {
    ARTS_DEBUG("  Recreating minsi at allocation point");
    return builder.create<arith::MinSIOp>(loc, lhsTraced, rhsTraced);
  }
  /// Partial operand fallback: use whichever operand traces successfully
  /// This handles patterns like minsi(start_row + block_size, output_size)
  /// where block_size dominates but start_row doesn't
  if (rhsTraced && !lhsTraced) {
    ARTS_DEBUG("  Using rhs of minsi as max chunk size");
    return rhsTraced;
  }
  if (lhsTraced && !rhsTraced) {
    ARTS_DEBUG("  Using lhs of minsi as max chunk size");
    return lhsTraced;
  }
  return nullptr;
}

/// Trace minui with partial operand fallback.
static Value traceMinUI(arith::MinUIOp minOp, DbAllocOp allocOp,
                        OpBuilder &builder, DominanceInfo &domInfo,
                        Location loc) {
  Value lhsTraced = getTracedOrDominatingOperand(minOp.getLhs(), allocOp,
                                                 builder, domInfo, loc);
  Value rhsTraced = getTracedOrDominatingOperand(minOp.getRhs(), allocOp,
                                                 builder, domInfo, loc);
  if (lhsTraced && rhsTraced) {
    ARTS_DEBUG("  Recreating minui at allocation point");
    return builder.create<arith::MinUIOp>(loc, lhsTraced, rhsTraced);
  }
  if (rhsTraced && !lhsTraced) {
    ARTS_DEBUG("  Using rhs of minui as max chunk size");
    return rhsTraced;
  }
  if (lhsTraced && !rhsTraced) {
    ARTS_DEBUG("  Using lhs of minui as max chunk size");
    return lhsTraced;
  }
  return nullptr;
}

/// Trace select with partial operand fallback; uses max if condition doesn't
/// dominate.
static Value traceSelect(arith::SelectOp selectOp, DbAllocOp allocOp,
                         OpBuilder &builder, DominanceInfo &domInfo,
                         Location loc) {
  Value trueTraced = getTracedOrDominatingOperand(
      selectOp.getTrueValue(), allocOp, builder, domInfo, loc);
  Value falseTraced = getTracedOrDominatingOperand(
      selectOp.getFalseValue(), allocOp, builder, domInfo, loc);
  ARTS_DEBUG("  Select trace: true=" << (trueTraced ? "ok" : "no") << " false="
                                     << (falseTraced ? "ok" : "no"));

  if (trueTraced && falseTraced) {
    Value condDom =
        getDominatingOperand(selectOp.getCondition(), allocOp, domInfo);
    builder.setInsertionPoint(allocOp);
    if (condDom) {
      ARTS_DEBUG("  Recreating select at allocation point");
      return builder.create<arith::SelectOp>(loc, condDom, trueTraced,
                                             falseTraced);
    }
    /// Condition doesn't dominate - use max of arms as safe upper bound
    ARTS_DEBUG("  Select condition doesn't dominate - using max of arms");
    return builder.create<arith::MaxSIOp>(loc, trueTraced, falseTraced);
  }
  /// Partial operand fallback: use whichever arm traces successfully
  if (trueTraced && !falseTraced) {
    ARTS_DEBUG("  Using true arm of select as max chunk size");
    return trueTraced;
  }
  if (falseTraced && !trueTraced) {
    ARTS_DEBUG("  Using false arm of select as max chunk size");
    return falseTraced;
  }
  return nullptr;
}

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

  //===--------------------------------------------------------------------===//
  // Division patterns (block_size = N / tasks)
  //===--------------------------------------------------------------------===//
  if (auto op = dyn_cast<arith::DivSIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "divsi");
  if (auto op = dyn_cast<arith::DivUIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "divui");
  if (auto op = dyn_cast<arith::CeilDivSIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "ceildivsi");
  if (auto op = dyn_cast<arith::CeilDivUIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "ceildivui");

  //===--------------------------------------------------------------------===//
  // Cast pattern (unwrap and trace source)
  //===--------------------------------------------------------------------===//
  if (auto castOp = dyn_cast<arith::IndexCastOp>(defOp)) {
    if (Value inner = traceBackChunkSize(castOp.getIn(), allocOp, builder,
                                         domInfo, loc)) {
      ARTS_DEBUG("  Wrapping with index_cast");
      if (inner.getType() == castOp.getType())
        return inner;
      return builder.create<arith::IndexCastOp>(loc, castOp.getType(), inner);
    }
    return nullptr;
  }

  //===--------------------------------------------------------------------===//
  // Bounds patterns (clamping with max/min)
  //===--------------------------------------------------------------------===//
  if (auto op = dyn_cast<arith::MaxSIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "maxsi");
  if (auto op = dyn_cast<arith::MinSIOp>(defOp))
    return traceMinSI(op, allocOp, builder, domInfo, loc);
  if (auto op = dyn_cast<arith::MinUIOp>(defOp))
    return traceMinUI(op, allocOp, builder, domInfo, loc);

  //===--------------------------------------------------------------------===//
  // Conditional pattern (select with fallback)
  //===--------------------------------------------------------------------===//
  if (auto op = dyn_cast<arith::SelectOp>(defOp))
    return traceSelect(op, allocOp, builder, domInfo, loc);

  //===--------------------------------------------------------------------===//
  // Arithmetic patterns (add, sub, mul)
  //===--------------------------------------------------------------------===//
  if (auto op = dyn_cast<arith::SubIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "subi");
  if (auto op = dyn_cast<arith::AddIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "addi");
  if (auto op = dyn_cast<arith::MulIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "muli");

  //===--------------------------------------------------------------------===//
  // Constant patterns (recreate at allocation point)
  //===--------------------------------------------------------------------===//
  if (auto constOp = dyn_cast<arith::ConstantOp>(defOp)) {
    if (auto intAttr = dyn_cast<IntegerAttr>(constOp.getValue())) {
      if (intAttr.getInt() == 0)
        return nullptr;
    }
    builder.setInsertionPoint(allocOp);
    ARTS_DEBUG("  Recreating constant at allocation point");
    return builder.create<arith::ConstantOp>(loc, constOp.getType(),
                                             constOp.getValue());
  }
  if (auto constIdxOp = dyn_cast<arith::ConstantIndexOp>(defOp)) {
    if (constIdxOp.value() == 0)
      return nullptr;
    builder.setInsertionPoint(allocOp);
    ARTS_DEBUG("  Recreating constant index at allocation point");
    return builder.create<arith::ConstantIndexOp>(loc, constIdxOp.value());
  }

  return nullptr; /// Cannot trace back - unsupported pattern
}

} // namespace

namespace {
struct DbPartitioningPass
    : public arts::DbPartitioningBase<DbPartitioningPass> {
  DbPartitioningPass(ArtsAnalysisManager *AM,
                     bool useFineGrainedFallback = false)
      : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
    this->useFineGrainedFallback = useFineGrainedFallback;
  }

  void runOnOperation() override;

  ///===--------------------------------------------------------------------===///
  /// Main Partitioning Methods
  ///===--------------------------------------------------------------------===///

  /// Main partitioning entry point.
  /// Iterates over allocations, applies rewriters, and determines twin-diff.
  bool partitionDb();

  ///===--------------------------------------------------------------------===///
  /// Stencil Bounds Analysis
  ///===--------------------------------------------------------------------===///

  /// Analyze stencil access patterns and generate bounds check flags.
  void analyzeStencilBounds();

  /// Generate runtime bounds validation for stencil accesses.
  /// Creates conditional guards for out-of-bounds stencil offsets.
  void generateBoundsValid(DbAcquireOp acquireOp,
                           ArrayRef<int64_t> boundsCheckFlags, Value loopIV);

  ///===--------------------------------------------------------------------===///
  /// Graph Management
  ///===--------------------------------------------------------------------===///

  /// Invalidate and rebuild the datablock graph after transformations.
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

/// Partition allocations and set twin-diff attributes based on disjoint access
/// proof.
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

  // PHASE 1: Partition allocations
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
      if (!allocNode->canBePartitioned(useFineGrainedFallback)) {
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

        bool disjoint = allocNode->canProveNonOverlapping();
        if (disjoint) {
          /// Set twin_diff=false for all child acquires (partition success)
          allocNode->forEachChildNode([&](NodeBase *child) {
            if (auto *acqNode = dyn_cast<DbAcquireNode>(child)) {
              setTwinAttr(acqNode->getDbAcquireOp(),
                          TwinDiffProof::PartitionSuccess);
            }
          });
        } else {
          ARTS_DEBUG("  Partitioned alloc has potential overlap; "
                     "keeping twin_diff enabled");
        }
      }
    });
  });

  // PHASE 2: Overlap analysis for unpartitioned allocations
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

  // PHASE 3: Default twin_diff=true for remaining acquires
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

/// Analyze allocation acquires and apply appropriate partitioning rewriter.
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
    if (*existingMode != PromotionMode::coarse) {
      ARTS_DEBUG("  Already partitioned as "
                 << getPartitionModeLabel(*existingMode) << " - SKIP");
      heuristics.recordDecision(
          "Partition-AlreadyPartitioned", false,
          "allocation already has partition attribute, skipping",
          allocOp.getOperation(), {});
      return allocOp; /// Skip - already partitioned!
    }
    /// mode == none means CreateDbs couldn't partition - continue to re-analyze
    ARTS_DEBUG("  Partition mode is " << getPartitionModeLabel(*existingMode)
                                      << " - re-analyzing with loop structure");
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

  /// Step 3: Build PartitioningContext and check for non-leading offsets
  PartitioningContext ctx;
  ctx.machine = &AM->getAbstractMachine();
  bool canUseChunked =
      true; /// Assume chunked is possible until proven otherwise

  if (allocNode) {
    ctx.accessPatterns =
        allocNode->summarizeAcquirePatterns(useFineGrainedFallback);
    ARTS_DEBUG("  Access patterns: hasUniform="
               << ctx.accessPatterns.hasUniform
               << ", hasStencil=" << ctx.accessPatterns.hasStencil
               << ", hasIndexed=" << ctx.accessPatterns.hasIndexed
               << ", isMixed=" << ctx.accessPatterns.isMixed());

    // Build per-acquire capabilities for heuristic voting
    size_t idx = 0;
    allocNode->forEachChildNode([&](NodeBase *child) {
      auto *acqNode = dyn_cast<DbAcquireNode>(child);
      if (!acqNode || idx >= acquireInfos.size())
        return;

      const auto &acqInfo = acquireInfos[idx];

      /// Check for non-leading partition offset (disables chunked mode).
      /// Non-leading means the first array index isn't directly derived from
      /// the partition offset, so chunk-based indexing won't work correctly.
      if (canUseChunked && acqInfo.mode == PartitionMode::Chunked) {
        Value offsetForCheck = getPartitionOffset(acqNode, &acqInfo);
        if (offsetForCheck &&
            !acqNode->canPartitionWithOffset(offsetForCheck)) {
          ARTS_DEBUG("  Non-leading partition offset detected; "
                     "disabling chunked partitioning for allocation");
          canUseChunked = false;
        }
      }

      /// Build AcquireInfo for heuristic voting
      AcquireInfo info;
      info.accessMode = acqNode->getDbAcquireOp().getMode();
      info.canElementWise = (acqInfo.mode == PartitionMode::ElementWise);
      info.canChunked =
          canUseChunked && (acqInfo.mode == PartitionMode::Chunked);
      info.pinnedDimCount = info.canElementWise ? 1 : 0;

      ctx.acquires.push_back(info);
      ARTS_DEBUG("    Acquire["
                 << idx << "]: mode=" << static_cast<int>(info.accessMode)
                 << ", canEW=" << info.canElementWise
                 << ", canChunked=" << info.canChunked);
      ++idx;
    });
  }

  // Determine allocation-level capabilities from per-acquire analysis
  if (modesConsistent) {
    ARTS_DEBUG(
        "  Consistent partition mode: " << static_cast<int>(consistentMode));
    /// All consistent and coarse -> no partitioning needed
    if (consistentMode == PartitionMode::Coarse) {
      /// When fine-grained fallback is enabled and allocation has non-affine
      /// accesses, allow element-wise partitioning instead of keeping coarse.
      if (useFineGrainedFallback && ctx.accessPatterns.hasIndexed) {
        ARTS_DEBUG("  Coarse mode but fine-grained fallback enabled with "
                   "hasIndexed - allowing element-wise");
        ctx.canElementWise = true;
        ctx.canChunked = false;
      } else {
        ARTS_DEBUG("  Coarse mode - no partitioning needed");
        heuristics.recordDecision(
            "Partition-CoarseMode", false,
            "all acquires are coarse mode, no partitioning needed",
            allocOp.getOperation(), {});
        return allocOp;
      }
    } else {
      ctx.canElementWise = (consistentMode == PartitionMode::ElementWise);
      ctx.canChunked =
          canUseChunked && (consistentMode == PartitionMode::Chunked);
    }
  } else {
    /// Inconsistent modes: use per-acquire voting to determine capabilities
    ctx.canElementWise = ctx.anyCanElementWise();
    ctx.canChunked = ctx.anyCanChunked(); /// Uses per-acquire canChunked flags
    /// If neither is true but we have acquires, allow element-wise as fallback
    if (!ctx.canElementWise && !ctx.canChunked && !ctx.acquires.empty())
      ctx.canElementWise = true;
    ARTS_DEBUG("  Inconsistent modes - using per-acquire voting: canEW="
               << ctx.canElementWise << ", canChunked=" << ctx.canChunked);
  }

  /// If any coarse acquires exist alongside chunked candidates, enable MIXED
  /// mode. This allows chunked partitioning while giving coarse acquires
  /// full-range access.
  if (ctx.canChunked) {
    bool hasCoarse =
        llvm::any_of(acquireInfos, [](const AcquirePartitionInfo &info) {
          return info.mode == PartitionMode::Coarse;
        });
    bool hasChunked =
        llvm::any_of(acquireInfos, [](const AcquirePartitionInfo &info) {
          return info.mode == PartitionMode::Chunked;
        });

    if (hasCoarse && hasChunked) {
      // MIXED MODE: chunked allocation with coarse acquires getting full-range
      ctx.canElementWise = false;
      ARTS_DEBUG(
          "  Mixed mode enabled: chunked allocation with coarse acquires");

      // Mark coarse acquires for full-range treatment
      for (auto &info : acquireInfos) {
        if (info.mode == PartitionMode::Coarse) {
          info.needsFullRange = true;
        }
      }
    } else if (hasCoarse) {
      ctx.canElementWise = false;
      ARTS_DEBUG("  Coarse+chunked mix detected - disabling element-wise");
    }
  }

  /// Set pinnedDimCount from per-acquire info (max across all acquires)
  ctx.pinnedDimCount = ctx.maxPinnedDimCount();

  /// Get access mode from allocNode
  if (allocNode)
    ctx.accessMode = allocOp.getMode();

  /// Get canonical chunk size for Chunked mode (extract static value if
  /// possible).
  Value dynamicChunkSize;
  auto isZeroCandidate = [](Value candidate) -> bool {
    if (!candidate)
      return false;
    Value stripped = ValueUtils::stripNumericCasts(candidate);
    return ValueUtils::isZeroConstant(stripped);
  };
  if (ctx.canChunked) {
    SmallVector<Value> chunkCandidates;
    int64_t staticCandidateCount = 0;

    func::FuncOp func = allocOp->getParentOfType<func::FuncOp>();
    DominanceInfo domInfo(func);

    for (const auto &info : acquireInfos) {
      if (info.mode != PartitionMode::Chunked || !info.partitionSize)
        continue;

      DbAcquireOp acquire = info.acquire;
      if (!acquire)
        continue;

      Value offsetHint = info.partitionOffset;
      Value sizeHint = info.partitionSize;
      if (!acquire.getOffsetHints().empty())
        offsetHint = acquire.getOffsetHints().front();
      if (!acquire.getSizeHints().empty())
        sizeHint = acquire.getSizeHints().front();

      Value baseCandidate =
          DatablockUtils::extractBaseChunkSizeCandidate(offsetHint, sizeHint);
      if (!baseCandidate)
        baseCandidate = sizeHint;
      if (!baseCandidate)
        continue;

      Value idxCandidate =
          ValueUtils::ensureIndexType(baseCandidate, builder, loc);
      if (!idxCandidate)
        continue;

      Value domCandidate = domInfo.properlyDominates(idxCandidate, allocOp)
                               ? idxCandidate
                               : traceBackChunkSize(idxCandidate, allocOp,
                                                    builder, domInfo, loc);
      if (!domCandidate)
        continue;

      if (isZeroCandidate(domCandidate)) {
        ARTS_DEBUG("  Skipping zero chunk size candidate");
        continue;
      }

      chunkCandidates.push_back(domCandidate);

      if (auto staticSize = arts::extractChunkSizeFromHint(domCandidate)) {
        if (*staticSize > 0) {
          ++staticCandidateCount;
          if (!ctx.chunkSize || *staticSize < *ctx.chunkSize)
            ctx.chunkSize = *staticSize;
        }
      }
    }

    if (!chunkCandidates.empty()) {
      dynamicChunkSize = chunkCandidates.front();
      for (size_t i = 1; i < chunkCandidates.size(); ++i) {
        dynamicChunkSize = builder.create<arith::MinUIOp>(loc, dynamicChunkSize,
                                                          chunkCandidates[i]);
      }
      heuristics.recordDecision(
          "Partition-CanonicalChunkSize", true,
          "canonicalized chunk size across acquires", allocOp.getOperation(),
          {{"candidateCount", static_cast<int64_t>(chunkCandidates.size())},
           {"staticCandidateCount", staticCandidateCount}});
    } else {
      heuristics.recordDecision(
          "Partition-CanonicalChunkSize", false,
          "no valid chunk size candidates from acquire hints",
          allocOp.getOperation(), {});
    }
  }

  /// Fallback: use the first observed chunk size if canonicalization failed.
  if (!dynamicChunkSize) {
    if (consistentMode == PartitionMode::Chunked && reference.partitionSize &&
        !isZeroCandidate(reference.partitionSize)) {
      dynamicChunkSize = reference.partitionSize;
      if (auto staticSize =
              arts::extractChunkSizeFromHint(reference.partitionSize)) {
        if (*staticSize > 0)
          ctx.chunkSize = *staticSize;
      }
    } else {
      for (const auto &info : acquireInfos) {
        if (info.mode != PartitionMode::Chunked || !info.partitionSize)
          continue;
        if (isZeroCandidate(info.partitionSize)) {
          ARTS_DEBUG("  Skipping zero fallback chunk size");
          continue;
        }
        dynamicChunkSize = info.partitionSize;
        if (auto staticSize =
                arts::extractChunkSizeFromHint(info.partitionSize)) {
          if (*staticSize > 0 &&
              (!ctx.chunkSize || *staticSize < *ctx.chunkSize))
            ctx.chunkSize = *staticSize;
        }
        break;
      }
    }
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

  /// For element-wise partitioning, we don't need per-acquire partition info
  /// since each acquire specifies its indices directly via offset_hints.
  bool skipAcquireInfoCheck =
      decision.isFineGrained() &&
      (useFineGrainedFallback && ctx.accessPatterns.hasIndexed);

  bool allRewriteable = skipAcquireInfoCheck ||
      std::all_of(acquireInfos.begin(), acquireInfos.end(),
                  [](const auto &info) {
                    // Full-range acquires don't need offset/size - they get full
                    // allocation
                    if (info.needsFullRange)
                      return true;
                    return info.isValid && info.partitionOffset &&
                           info.partitionSize;
                  });
  if (!allRewriteable) {
    ARTS_DEBUG("  Some acquires missing partition info - keeping original");
    heuristics.recordDecision(
        "Partition-MissingAcquireInfo", false,
        "some acquires missing partition info, cannot rewrite allocation",
        allocOp.getOperation(), {});
    return allocOp;
  }

  ARTS_DEBUG("  Decision: mode=" << getRewriterModeName(decision.mode)
                                 << ", outerRank=" << decision.outerRank
                                 << ", innerRank=" << decision.innerRank);

  /// Step 5a: Compute stencilInfo early (needed for inner size calculation)
  /// Must be done before computeSizesFromDecision() for stencil mode.
  std::optional<StencilInfo> stencilInfo;
  if (decision.mode == RewriterMode::Stencil && allocNode) {
    StencilInfo info;
    info.haloLeft = 0;
    info.haloRight = 0;

    /// Collect stencil bounds from acquire nodes
    allocNode->forEachChildNode([&](NodeBase *child) {
      auto *acqNode = dyn_cast<DbAcquireNode>(child);
      if (!acqNode)
        return;
      auto stencilBounds = acqNode->getStencilBounds();
      if (stencilBounds && stencilBounds->hasHalo()) {
        info.haloLeft = std::max(info.haloLeft, stencilBounds->haloLeft());
        info.haloRight = std::max(info.haloRight, stencilBounds->haloRight());
      }
    });

    /// Get total rows from first element dimension
    if (!allocOp.getElementSizes().empty()) {
      if (auto staticSize =
              arts::extractChunkSizeFromHint(allocOp.getElementSizes()[0]))
        info.totalRows =
            builder.create<arith::ConstantIndexOp>(loc, *staticSize);
      else
        info.totalRows = allocOp.getElementSizes()[0];
    }

    stencilInfo = info;
    ARTS_DEBUG("  Stencil mode (early): haloLeft="
               << info.haloLeft << ", haloRight=" << info.haloRight);
  }

  /// Step 5b: Compute sizes from decision (uses outerRank/innerRank)
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
    } else {
      /// No chunk size available (neither static nor dynamic) - fall back to
      /// coarse
      ARTS_DEBUG("  No chunk size available for chunked mode - falling back to "
                 "coarse");
      heuristics.recordDecision(
          "Partition-NoChunkSize", false,
          "chunked mode requested but no chunk size available",
          allocOp.getOperation(), {});
      return allocOp;
    }
  }
  if (decision.isChunked()) {
    if (!chunkSizeForPlan) {
      ARTS_DEBUG(
          "  Missing chunk size for chunked decision - keeping original");
      heuristics.recordDecision(
          "Partition-ChunkSizeMissing", false,
          "chunked decision without a valid chunk size, keeping original",
          allocOp.getOperation(), {});
      return allocOp;
    }
    Value strippedChunkSize = ValueUtils::stripNumericCasts(chunkSizeForPlan);
    if (ValueUtils::isZeroConstant(strippedChunkSize)) {
      ARTS_DEBUG("  Chunk size is zero - clamping to 1");
      heuristics.recordDecision(
          "Partition-ChunkSizeZero", false,
          "chunk size is zero; clamping to 1 to avoid divide-by-zero",
          allocOp.getOperation(), {});
      chunkSizeForPlan = builder.create<arith::ConstantIndexOp>(loc, 1);
    }
    Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
    chunkSizeForPlan =
        builder.create<arith::MaxUIOp>(loc, chunkSizeForPlan, one);
  }

  /// For stencil mode, inner size must be EXTENDED (base + halos) to hold halo
  /// data. The outer size (numChunks) must still be computed from the BASE
  /// chunk size so chunk indices remain consistent with the original grid.
  Value innerChunkSize = chunkSizeForPlan;
  bool extendInnerForStencil =
      stencilInfo && stencilInfo->hasHalo() && chunkSizeForPlan;
  if (extendInnerForStencil) {
    int64_t haloTotal = stencilInfo->haloLeft + stencilInfo->haloRight;
    Value haloConst = builder.create<arith::ConstantIndexOp>(loc, haloTotal);
    innerChunkSize =
        builder.create<arith::AddIOp>(loc, chunkSizeForPlan, haloConst);
    ARTS_DEBUG("  Extended inner size for stencil: base + " << haloTotal);
  }

  SmallVector<Value> newOuterSizes, newInnerSizes;
  computeSizesFromDecision(allocOp, decision, chunkSizeForPlan, newOuterSizes,
                           newInnerSizes);
  if (extendInnerForStencil) {
    if (!newInnerSizes.empty())
      newInnerSizes[0] = innerChunkSize;
    else
      newInnerSizes.push_back(innerChunkSize);
  }

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
  plan.chunkSize = chunkSizeForPlan;

  /// Mixed mode: set flag and store numChunks for full-range acquires
  bool hasMixedMode = llvm::any_of(
      acquireInfos, [](const auto &info) { return info.needsFullRange; });
  if (hasMixedMode && decision.isChunked()) {
    plan.isMixedMode = true;
    if (!newOuterSizes.empty())
      plan.numChunks = newOuterSizes[0];
    ARTS_DEBUG("  Mixed mode plan: numChunks=" << plan.numChunks);
  }

  /// ESD: Use early-computed stencilInfo for Stencil mode
  if (stencilInfo)
    plan.stencilInfo = *stencilInfo;
  // Note: byte_offset/byte_size for ESD is computed in EdtLowering from
  // element_offsets/element_sizes, using allocation's elementSizes for stride.

  /// Build DbRewriteAcquire list from AcquirePartitionInfo
  /// Include ALL acquires - invalid ones get Coarse fallback (offset=0,
  /// size=totalSize)
  SmallVector<DbRewriteAcquire> rewriteAcquires;
  Value zero = builder.create<arith::ConstantIndexOp>(allocOp.getLoc(), 0);
  for (const auto &info : acquireInfos) {
    if (!info.acquire)
      continue;

    DbRewriteAcquire rewriteInfo;
    rewriteInfo.acquire = info.acquire;

    if (info.needsFullRange) {
      // Coarse acquire in mixed mode: full-range access.
      rewriteInfo.isFullRange = true;
    } else if (info.isValid) {
      rewriteInfo.elemOffset = info.partitionOffset;
      rewriteInfo.elemSize = info.partitionSize;
    } else {
      /// Invalid acquire: use Coarse fallback (full size access)
      /// The acquire still needs rewriting to use the new allocation structure.
      Value totalSize = allocOp.getElementSizes().empty()
                            ? zero
                            : allocOp.getElementSizes().front();
      rewriteInfo.elemOffset = zero;
      rewriteInfo.elemSize = totalSize;
      ARTS_DEBUG("  Invalid acquire " << info.acquire
                                      << " rewritten with Coarse fallback");
    }

    rewriteAcquires.push_back(rewriteInfo);
  }

  auto rewriter = DbRewriter::create(allocOp, newOuterSizes, newInnerSizes,
                                     rewriteAcquires, plan);
  auto result = rewriter->apply(builder);

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

/// Check if op is in the else branch of `if (iv == 0)`, guaranteeing iv > 0.
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

/// Generate bounds_valid for stencil acquires; skips lower bound if
/// control-flow guarded.
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

void DbPartitioningPass::invalidateAndRebuildGraph() {
  module.walk([&](func::FuncOp func) {
    AM->invalidateFunction(func);
    (void)AM->getDbGraph(func);
  });
}

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbPartitioningPass(ArtsAnalysisManager *AM,
                                               bool useFineGrainedFallback) {
  return std::make_unique<DbPartitioningPass>(AM, useFineGrainedFallback);
}
} // namespace arts
} // namespace mlir
