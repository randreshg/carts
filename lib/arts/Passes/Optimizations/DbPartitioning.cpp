///==========================================================================///
/// DbPartitioning.cpp - Datablock partitioning decision pass
///
/// Analyzes allocations and selects partitioning strategies:
///   Phase 1: Per-acquire analysis (chunk offset/size, capabilities)
///   Phase 2: Heuristic voting -> RewriterMode
///   (Coarse/ElementWise/Block/Stencil) Phase 3: Size computation and
///   rewriter application
///
/// Twin-diff is enabled when disjoint access cannot be proven.
///==========================================================================///

/// Dialects
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
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cstdint>
#include <optional>

#include "arts/Transforms/Datablock/DbRewriter.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/OperationAttributes.h"
ARTS_DEBUG_SETUP(db_partitioning);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;

namespace {

/// Per-acquire partition analysis results for Phase 1.
/// NOTE: This struct shares partitionOffset/partitionSize fields with
/// AcquireInfo in HeuristicsConfig.h. They serve different purposes:
///   - AcquireInfo: For heuristic voting/decision-making (capability flags)
///   - AcquirePartitionInfo: For actual partitioning transformation state
/// Future consolidation could create a common base, but current separation
/// keeps concerns cleanly separated.
struct AcquirePartitionInfo {
  DbAcquireOp acquire;
  PartitionMode mode;
  Value partitionOffset;
  Value partitionSize;
  bool isValid = false;
  bool hasIndirectAccess = false;
  /// For mixed mode: this coarse acquire needs full-range access to chunked
  /// alloc.
  bool needsFullRange = false;

  /// Check consistency with another acquire.
  /// Returns true if both acquires can use the same partitioning strategy.
  bool isConsistentWith(const AcquirePartitionInfo &other) const {
    if (mode != other.mode)
      return false;
    /// For Block mode, sizes must be same SSA value
    if (mode == PartitionMode::block && partitionSize != other.partitionSize) {
      int64_t lhs = 0, rhs = 0;
      bool lhsConst = ValueUtils::getConstantIndex(partitionSize, lhs);
      bool rhsConst = ValueUtils::getConstantIndex(other.partitionSize, rhs);
      if (!(lhsConst && rhsConst && lhs == rhs))
        return false;
    }
    return true;
  }
};

/// Validate that element-wise partition indices match actual EDT accesses.
/// Returns false if this is a block-wise pattern (indices are block corners,
/// but accesses span a range) - indicating we should fall back to block mode.
///
/// Detection criteria for block-wise pattern:
/// 1. Partition hints have indices[] (looks like element-wise)
/// 2. BUT the enclosing loop steps by > 1 (BLOCK_SIZE)
/// 3. OR partition indices don't appear in EDT body accesses
static bool
validateElementWiseIndices(ArrayRef<AcquirePartitionInfo> acquireInfos,
                           DbAllocNode *allocNode) {
  for (const auto &info : acquireInfos) {
    DbAcquireOp acquire = info.acquire;
    auto partitionIndices = acquire.getPartitionIndices();
    if (partitionIndices.empty())
      continue;

    /// Use DbAcquireNode's validation method for the actual check
    if (allocNode) {
      auto *acqNode = allocNode->findAcquireNode(acquire);
      if (acqNode && !acqNode->validateElementWisePartitioning())
        return false;
    }
  }
  return true; /// Valid element-wise
}

/// Extract partition offset
static Value getPartitionOffset(DbAcquireNode *acqNode,
                                const AcquirePartitionInfo *info) {
  if (info && info->partitionOffset)
    return info->partitionOffset;

  auto [offset, size] = acqNode->getPartitionInfo();
  if (offset)
    return offset;

  DbAcquireOp acqOp = acqNode->getDbAcquireOp();
  /// Check partition hints from partition_indices
  if (!acqOp.getPartitionIndices().empty())
    return acqOp.getPartitionIndices().front();

  auto offsets = acqOp.getPartitionOffsets();
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
  info.mode = DatablockUtils::getPartitionModeFromStructure(acquire);
  if (acqNode)
    info.hasIndirectAccess = acqNode->hasIndirectAccess();

  switch (info.mode) {
  case PartitionMode::coarse:
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

  case PartitionMode::fine_grained:
    /// Use partition_indices as partition offset (element-space hints)
    if (!acquire.getPartitionIndices().empty()) {
      info.partitionOffset = acquire.getPartitionIndices().front();
      info.partitionSize = builder.create<arith::ConstantIndexOp>(loc, 1);
      info.isValid = true;
    }
    break;

  case PartitionMode::block:
  case PartitionMode::stencil:
    /// FIRST: Trust partition hints from CreateDbs (authoritative source).
    /// CreateDbs sets partition_offsets/sizes based on DbControlOp analysis
    /// which happens BEFORE EDT outlining. Re-validation inside EDT scope
    /// fails because the partition offset (e.g., outer loop IV) is not visible.
    if (!acquire.getPartitionOffsets().empty()) {
      info.partitionOffset = acquire.getPartitionOffsets().front();
      info.partitionSize = acquire.getPartitionSizes().empty()
                               ? builder.create<arith::ConstantIndexOp>(loc, 1)
                               : acquire.getPartitionSizes().front();
      info.isValid = true;
      ARTS_DEBUG("  Using partition hints from CreateDbs: offset="
                 << info.partitionOffset << ", size=" << info.partitionSize);
      break;
    }
    /// FALLBACK: Try computeChunkInfo for cases without CreateDbs hints.
    /// This handles legacy paths or manually constructed IR.
    if (acqNode) {
      Value offset, size;
      if (succeeded(acqNode->computeChunkInfo(offset, size))) {
        info.partitionOffset = offset;
        info.partitionSize = size;
        info.isValid = true;
        ARTS_DEBUG("  Computed chunk info from access analysis");
      } else {
        /// Validation failed -> fall back to coarse for correctness.
        info.mode = PartitionMode::coarse;
        ARTS_DEBUG("  Chunk info computation failed, falling back to coarse");
      }
    }
    break;
  }

  return info;
}

static void resetCoarseAcquireRanges(DbAllocOp allocOp, DbAllocNode *allocNode,
                                     OpBuilder &builder) {
  if (!allocNode || allocOp.getSizes().empty())
    return;

  Value zero = builder.create<arith::ConstantIndexOp>(allocOp.getLoc(), 0);
  SmallVector<Value> fullOffsets;
  SmallVector<Value> fullSizes;
  for (Value size : allocOp.getSizes()) {
    fullOffsets.push_back(zero);
    fullSizes.push_back(size);
  }

  allocNode->forEachChildNode([&](NodeBase *child) {
    auto *acqNode = dyn_cast<DbAcquireNode>(child);
    if (!acqNode)
      return;
    DbAcquireOp acqOp = acqNode->getDbAcquireOp();
    if (!acqOp)
      return;
    acqOp.getOffsetsMutable().assign(fullOffsets);
    acqOp.getSizesMutable().assign(fullSizes);
    /// Keep acquire partition attribute consistent with coarse allocation.
    setPartitionMode(acqOp.getOperation(), PartitionMode::coarse);
  });
}

/// Compute sizes from PartitioningDecision (uses outerRank/innerRank)
/// Allocation modes:
///   - Coarse: sizes=[1], elementSizes=all dims (single datablock)
///   - ElementWise: sizes=first outerRank dims, elementSizes=remaining dims
///   - Block: sizes=[numBlocks], elementSizes=[blockSize, remaining dims]
static void computeSizesFromDecision(DbAllocOp allocOp,
                                     const PartitioningDecision &decision,
                                     ArrayRef<Value> blockSizes,
                                     SmallVector<Value> &newOuterSizes,
                                     SmallVector<Value> &newInnerSizes) {
  OpBuilder builder(allocOp);
  Location loc = allocOp.getLoc();
  ValueRange elemSizes = allocOp.getElementSizes();

  if (decision.isBlock() && !blockSizes.empty()) {
    /// N-D Block partitioning: compute numBlocks per dimension
    /// For each partitioned dimension d:
    ///   newOuterSizes[d] = ceil(elemSizes[d] / blockSizes[d])
    ///   newInnerSizes[d] = blockSizes[d]
    Type i64Type = builder.getI64Type();
    Value oneI64 = builder.create<arith::ConstantIntOp>(loc, 1, 64);

    unsigned nPartDims = blockSizes.size();
    for (unsigned d = 0; d < nPartDims && d < elemSizes.size(); ++d) {
      Value bs = blockSizes[d];
      if (!bs || ValueUtils::isZeroConstant(ValueUtils::stripNumericCasts(bs)))
        continue;

      Value dim = elemSizes[d];

      /// numBlocks = ceil(dim / blockSize) = (dim + blockSize - 1) / blockSize
      Value dimI64 = builder.create<arith::IndexCastOp>(loc, i64Type, dim);
      Value bsI64 = builder.create<arith::IndexCastOp>(loc, i64Type, bs);
      Value sum = builder.create<arith::AddIOp>(loc, dimI64, bsI64);
      Value sumMinusOne = builder.create<arith::SubIOp>(loc, sum, oneI64);
      Value numBlocksI64 =
          builder.create<arith::DivUIOp>(loc, sumMinusOne, bsI64);
      Value numBlocks = builder.create<arith::IndexCastOp>(
          loc, builder.getIndexType(), numBlocksI64);

      newOuterSizes.push_back(numBlocks);
      newInnerSizes.push_back(bs);
    }

    /// Remaining non-partitioned dims go to inner
    for (unsigned d = nPartDims; d < elemSizes.size(); ++d)
      newInnerSizes.push_back(elemSizes[d]);

    /// Ensure non-empty
    if (newOuterSizes.empty())
      newOuterSizes.push_back(builder.create<arith::ConstantIndexOp>(loc, 1));
    if (newInnerSizes.empty())
      newInnerSizes.push_back(builder.create<arith::ConstantIndexOp>(loc, 1));

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

/// Trace block size through arithmetic ops to recreate at allocation point.
/// Supports division, bounds (minsi/maxsi), select, arithmetic, and casts.
/// For minsi/select, uses partial operand fallback when only one arm traces.
static Value traceBackBlockSize(Value blockSize, DbAllocOp allocOp,
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
  return traceBackBlockSize(operand, allocOp, builder, domInfo, loc);
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
    if (ValueUtils::isZeroConstant(lhsTraced))
      return rhsTraced;
    if (ValueUtils::isZeroConstant(rhsTraced))
      return lhsTraced;
    ARTS_DEBUG("  Recreating minsi at allocation point");
    return builder.create<arith::MinSIOp>(loc, lhsTraced, rhsTraced);
  }
  /// Partial operand fallback: use whichever operand traces successfully
  /// This handles patterns like minsi(start_row + block_size, output_size)
  /// where block_size dominates but start_row doesn't
  if (rhsTraced && !lhsTraced) {
    ARTS_DEBUG("  Using rhs of minsi as max block size");
    return rhsTraced;
  }
  if (lhsTraced && !rhsTraced) {
    ARTS_DEBUG("  Using lhs of minsi as max block size");
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
    if (ValueUtils::isZeroConstant(lhsTraced))
      return rhsTraced;
    if (ValueUtils::isZeroConstant(rhsTraced))
      return lhsTraced;
    ARTS_DEBUG("  Recreating minui at allocation point");
    return builder.create<arith::MinUIOp>(loc, lhsTraced, rhsTraced);
  }
  if (rhsTraced && !lhsTraced) {
    ARTS_DEBUG("  Using rhs of minui as max block size");
    return rhsTraced;
  }
  if (lhsTraced && !rhsTraced) {
    ARTS_DEBUG("  Using lhs of minui as max block size");
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
    if (ValueUtils::isZeroConstant(trueTraced))
      return nullptr;
    ARTS_DEBUG("  Using true arm of select as max block size");
    return trueTraced;
  }
  if (falseTraced && !trueTraced) {
    if (ValueUtils::isZeroConstant(falseTraced))
      return nullptr;
    ARTS_DEBUG("  Using false arm of select as max block size");
    return falseTraced;
  }
  return nullptr;
}

static Value traceBackBlockSize(Value blockSize, DbAllocOp allocOp,
                                OpBuilder &builder, DominanceInfo &domInfo,
                                Location loc) {
  if (!blockSize)
    return nullptr;

  /// If the value already dominates the allocation, reuse it directly.
  if (domInfo.properlyDominates(blockSize, allocOp))
    return blockSize;

  Operation *defOp = blockSize.getDefiningOp();
  if (!defOp) {
    /// Block size is a block argument - try to trace through EDT
    ARTS_DEBUG("  Trace-back: block size has no defining op (block arg)");
    if (auto blockArg = dyn_cast<BlockArgument>(blockSize)) {
      if (Value traced = traceValueThroughEdt(blockSize, allocOp, domInfo)) {
        ARTS_DEBUG("  Traced block arg to dominating value");
        return traced;
      }
    }
    return nullptr;
  }

  ARTS_DEBUG("  Trace-back: examining " << defOp->getName().getStringRef());

  /// Division patterns (block_size = N / tasks)
  if (auto op = dyn_cast<arith::DivSIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "divsi");
  if (auto op = dyn_cast<arith::DivUIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "divui");
  if (auto op = dyn_cast<arith::CeilDivSIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "ceildivsi");
  if (auto op = dyn_cast<arith::CeilDivUIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "ceildivui");

  /// Cast pattern (unwrap and trace source)
  if (auto castOp = dyn_cast<arith::IndexCastOp>(defOp)) {
    if (Value inner = traceBackBlockSize(castOp.getIn(), allocOp, builder,
                                         domInfo, loc)) {
      ARTS_DEBUG("  Wrapping with index_cast");
      if (inner.getType() == castOp.getType())
        return inner;
      return builder.create<arith::IndexCastOp>(loc, castOp.getType(), inner);
    }
    return nullptr;
  }

  /// Bounds patterns (clamping with max/min)
  if (auto op = dyn_cast<arith::MaxSIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "maxsi");
  if (auto op = dyn_cast<arith::MinSIOp>(defOp))
    return traceMinSI(op, allocOp, builder, domInfo, loc);
  if (auto op = dyn_cast<arith::MinUIOp>(defOp))
    return traceMinUI(op, allocOp, builder, domInfo, loc);

  /// Conditional pattern (select with fallback)
  if (auto op = dyn_cast<arith::SelectOp>(defOp))
    return traceSelect(op, allocOp, builder, domInfo, loc);

  /// Arithmetic patterns (add, sub, mul)
  if (auto op = dyn_cast<arith::SubIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "subi");
  if (auto op = dyn_cast<arith::AddIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "addi");
  if (auto op = dyn_cast<arith::MulIOp>(defOp))
    return traceBinaryOp(op, allocOp, builder, domInfo, loc, "muli");

  /// Constant patterns (recreate at allocation point)
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
  DbPartitioningPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override;

  ///===--------------------------------------------------------------------===///
  /// Main Partitioning Methods
  ///===--------------------------------------------------------------------===///

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

  /// Expand multi-entry acquires into separate DbAcquireOps.nt.
  bool expandMultiEntryAcquires();

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

  /// Phase 0: Expand multi-entry acquires into separate DbAcquireOps.
  /// This must happen BEFORE graph construction so each acquire can be
  /// analyzed independently.
  changed |= expandMultiEntryAcquires();

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

namespace {
struct EdtAcquireUse {
  EdtOp edt = nullptr;
  BlockArgument blockArg;
};

static EdtAcquireUse findEdtUse(DbAcquireOp acquire) {
  EdtAcquireUse use;
  for (Operation *user : acquire.getPtr().getUsers()) {
    auto edtOp = dyn_cast<EdtOp>(user);
    if (!edtOp)
      continue;
    use.edt = edtOp;
    for (auto [idx, dep] : llvm::enumerate(edtOp.getDependencies())) {
      if (dep == acquire.getPtr()) {
        use.blockArg = edtOp.getBody().front().getArgument(idx);
        break;
      }
    }
    break;
  }
  return use;
}

static SmallVector<DbAcquireOp> createExpandedAcquires(DbAcquireOp original,
                                                       OpBuilder &builder) {
  SmallVector<DbAcquireOp> expanded;
  Location loc = original.getLoc();
  size_t numEntries = original.getNumPartitionEntries();

  for (size_t i = 0; i < numEntries; ++i) {
    PartitionMode entryMode = original.getPartitionEntryMode(i);
    SmallVector<Value> entryIndices = original.getPartitionIndicesForEntry(i);
    SmallVector<Value> entryOffsets = original.getPartitionOffsetsForEntry(i);
    SmallVector<Value> entrySizes = original.getPartitionSizesForEntry(i);

    ARTS_DEBUG("    Entry " << i << ": mode=" << static_cast<int>(entryMode)
                            << ", indices=" << entryIndices.size()
                            << ", offsets=" << entryOffsets.size());

    auto newAcquire = builder.create<DbAcquireOp>(
        loc, original.getMode(), original.getSourceGuid(),
        original.getSourcePtr(), entryMode,
        /*indices=*/SmallVector<Value>{},
        /*offsets=*/
        SmallVector<Value>(original.getOffsets().begin(),
                           original.getOffsets().end()),
        /*sizes=*/
        SmallVector<Value>(original.getSizes().begin(),
                           original.getSizes().end()),
        /*partition_indices=*/entryIndices,
        /*partition_offsets=*/entryOffsets,
        /*partition_sizes=*/entrySizes);

    /// Preserve per-entry segmentation for single-entry acquires so
    /// getPartitionInfos() can infer dimensionality.
    SmallVector<int32_t> indicesSegs = {
        static_cast<int32_t>(entryIndices.size())};
    SmallVector<int32_t> offsetsSegs = {
        static_cast<int32_t>(entryOffsets.size())};
    SmallVector<int32_t> sizesSegs = {static_cast<int32_t>(entrySizes.size())};
    SmallVector<int32_t> entryModes = {static_cast<int32_t>(entryMode)};
    newAcquire.setPartitionSegments(indicesSegs, offsetsSegs, sizesSegs,
                                    entryModes);

    if (original.hasTwinDiff())
      newAcquire.setTwinDiff(original.getTwinDiff());

    expanded.push_back(newAcquire);
    ARTS_DEBUG("    Created expanded acquire: " << newAcquire);
  }

  return expanded;
}

static SmallVector<Value> rebuildEdtDeps(EdtOp edt, DbAcquireOp original,
                                         ArrayRef<DbAcquireOp> expanded) {
  SmallVector<Value> deps;
  for (Value dep : edt.getDependencies()) {
    if (dep == original.getPtr()) {
      for (DbAcquireOp acq : expanded)
        deps.push_back(acq.getPtr());
    } else {
      deps.push_back(dep);
    }
  }
  return deps;
}

static SmallVector<BlockArgument>
insertExpandedBlockArgs(EdtOp edt, BlockArgument originalArg, size_t numEntries,
                        Type argType, Location loc) {
  Block &edtBlock = edt.getBody().front();
  SmallVector<BlockArgument> args;
  args.push_back(originalArg);
  for (size_t i = 1; i < numEntries; ++i) {
    unsigned insertPos = originalArg.getArgNumber() + i;
    BlockArgument newArg = edtBlock.insertArgument(insertPos, argType, loc);
    args.push_back(newArg);
  }
  return args;
}

static SmallVector<DbRefOp> collectDbRefUsers(BlockArgument arg) {
  SmallVector<DbRefOp> refs;
  for (Operation *user : arg.getUsers())
    if (auto dbRef = dyn_cast<DbRefOp>(user))
      refs.push_back(dbRef);
  return refs;
}

static SmallVector<Value> collectAccessIndices(DbRefOp dbRef) {
  SmallVector<Value> indices;
  for (Operation *refUser : dbRef.getResult().getUsers()) {
    if (auto load = dyn_cast<memref::LoadOp>(refUser)) {
      indices.assign(load.getIndices().begin(), load.getIndices().end());
      break;
    }
    if (auto store = dyn_cast<memref::StoreOp>(refUser)) {
      indices.assign(store.getIndices().begin(), store.getIndices().end());
      break;
    }
  }
  return indices;
}

struct EntryMatch {
  size_t index = 0;
  bool found = false;
  int score = -1;
};

static SmallVector<Value> getEntryAnchors(DbAcquireOp original, size_t entry) {
  SmallVector<Value> anchors = original.getPartitionIndicesForEntry(entry);
  if (!anchors.empty())
    return anchors;
  return original.getPartitionOffsetsForEntry(entry);
}

static bool indexMatchesAnchor(Value idx, Value anchor) {
  if (!idx || !anchor)
    return false;
  if (idx == anchor || ValueUtils::dependsOn(idx, anchor))
    return true;
  if (auto cast = idx.getDefiningOp<arith::IndexCastOp>())
    idx = cast.getIn();
  if (auto blockArg = idx.dyn_cast<BlockArgument>()) {
    if (auto forOp = dyn_cast<scf::ForOp>(blockArg.getOwner()->getParentOp())) {
      if (blockArg == forOp.getInductionVar()) {
        Value lb = forOp.getLowerBound();
        if (lb == anchor || ValueUtils::dependsOn(lb, anchor))
          return true;
      }
    }
  }
  return false;
}

static int scoreEntry(ArrayRef<Value> indices, ArrayRef<Value> anchors) {
  int score = 0;
  size_t n = std::min(indices.size(), anchors.size());
  for (size_t d = 0; d < n; ++d) {
    Value idx = indices[d];
    Value anchor = anchors[d];
    if (!idx || !anchor)
      continue;
    if (idx == anchor) {
      score += 2;
      continue;
    }
    if (indexMatchesAnchor(idx, anchor))
      score += 1;
  }
  return score;
}

static EntryMatch matchDbRefEntry(DbRefOp dbRef, ArrayRef<Value> accessIndices,
                                  DbAcquireOp original, size_t numEntries) {
  EntryMatch match;

  auto tryMatch = [&](ArrayRef<Value> indices) {
    if (indices.empty())
      return;
    for (size_t i = 0; i < numEntries; ++i) {
      SmallVector<Value> anchors = getEntryAnchors(original, i);
      if (anchors.empty())
        continue;
      int score = scoreEntry(indices, anchors);
      if (score > match.score) {
        match.score = score;
        match.index = i;
      }
    }
  };

  SmallVector<Value> dbRefIndices(dbRef.getIndices().begin(),
                                  dbRef.getIndices().end());
  tryMatch(dbRefIndices);
  tryMatch(accessIndices);
  match.found = match.score > 0;
  return match;
}
} // namespace

/// Expand multi-entry acquires into separate DbAcquireOps.
/// When an OpenMP task has `depend(in: A[i][j], A[i-1][j])`, CreateDbs captures
/// both entries in a single DbAcquireOp with segment attributes. This function
/// expands each entry into its own acquire with a corresponding EDT block arg.
///
/// BEFORE:
///   %acq = arts.db_acquire [inout] (%guid, %ptr)
///     partition_indices = [%i, %j, %i_minus_1, %j]
///     partition_indices_segments = [2, 2]  /// Two entries, each with 2
///     indices partition_entry_modes = [fine_grained, fine_grained]
///   arts.edt (%acq) { ^bb0(%arg0): ... }
///
/// AFTER:
///   %acq0 = arts.db_acquire [inout] (%guid, %ptr)
///     partitioning(fine_grained, indices[%i, %j])
///   %acq1 = arts.db_acquire [inout] (%guid, %ptr)
///     partitioning(fine_grained, indices[%i_minus_1, %j])
///   arts.edt (%acq0, %acq1) { ^bb0(%arg0, %arg1): ... }
bool DbPartitioningPass::expandMultiEntryAcquires() {
  ARTS_DEBUG_HEADER(ExpandMultiEntryAcquires);
  bool changed = false;

  SmallVector<DbAcquireOp> acquiresToExpand;
  module.walk([&](DbAcquireOp acquire) {
    if (acquire.hasMultiplePartitionEntries())
      acquiresToExpand.push_back(acquire);
  });

  if (acquiresToExpand.empty()) {
    ARTS_DEBUG("  No multi-entry acquires to expand");
    return false;
  }

  ARTS_DEBUG("  Found " << acquiresToExpand.size()
                        << " multi-entry acquires to expand");

  for (DbAcquireOp original : acquiresToExpand) {
    size_t numEntries = original.getNumPartitionEntries();
    if (numEntries <= 1)
      continue;

    ARTS_DEBUG("  Expanding acquire with " << numEntries
                                           << " entries: " << original);

    OpBuilder builder(original);

    /// Find the EDT that uses this acquire
    EdtAcquireUse use = findEdtUse(original);
    if (!use.edt) {
      ARTS_DEBUG("    No EDT user found, skipping expansion");
      continue;
    }

    /// Create separate acquires for each partition entry
    SmallVector<DbAcquireOp> expandedAcquires =
        createExpandedAcquires(original, builder);

    /// Update EDT dependencies: replace original with all expanded acquires
    use.edt.setDependencies(
        rebuildEdtDeps(use.edt, original, expandedAcquires));

    /// Add new block arguments for expanded acquires (first replaces original)
    Type argType = use.blockArg.getType();
    SmallVector<BlockArgument> newBlockArgs = insertExpandedBlockArgs(
        use.edt, use.blockArg, numEntries, argType, original.getLoc());

    /// Remap DbRef operations to use their correct block arguments.
    for (DbRefOp dbRef : collectDbRefUsers(use.blockArg)) {
      SmallVector<Value> accessIndices = collectAccessIndices(dbRef);
      EntryMatch match =
          matchDbRefEntry(dbRef, accessIndices, original, numEntries);

      /// Fallback: For multi-entry acquires without a match, log and use entry
      /// 0.
      if (!match.found && numEntries > 1) {
        ARTS_DEBUG("      WARNING: Could not match db_ref to partition entry, "
                   "defaulting to entry 0");
      }

      ARTS_DEBUG("      db_ref matched entry "
                 << match.index << " (access indices: " << accessIndices.size()
                 << ", found: " << match.found << ")");

      dbRef.getSourceMutable().assign(newBlockArgs[match.index]);
    }

    /// Erase original acquire
    original.erase();
    changed = true;

    ARTS_DEBUG("    Expanded into " << expandedAcquires.size()
                                    << " separate acquires");
  }

  return changed;
}

/// Partition allocations and set twin-diff attributes based on disjoint access
/// proof.
bool DbPartitioningPass::partitionDb() {
  ARTS_DEBUG_HEADER(PartitionDb);
  bool changed = false;

  auto setTwinAttr = [&](DbAcquireOp acq, TwinDiffProof proof) {
    if (!acq)
      return;
    TwinDiffContext ctx{proof, false, acq.getOperation()};
    bool useTwinDiff = AM->getHeuristicsConfig().shouldUseTwinDiff(ctx);
    if (acq.hasTwinDiff() && acq.getTwinDiff() == useTwinDiff)
      return;
    acq.setTwinDiff(useTwinDiff);
    changed = true;
  };

  llvm::SmallPtrSet<Operation *, 8> partitionSuccess;

  /// PHASE 1: Partition allocations
  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      if (!allocNode)
        return;
      DbAllocOp allocOp = allocNode->getDbAllocOp();
      if (!allocOp || !allocNode->canBePartitioned())
        return;

      auto promotedOr = partitionAlloc(allocOp, allocNode);
      if (failed(promotedOr))
        return;

      DbAllocOp promoted = promotedOr.value();
      if (promoted.getOperation() != allocOp.getOperation()) {
        changed = true;
        partitionSuccess.insert(promoted.getOperation());
      }
    });
  });

  /// Rebuild graph to ensure valid operation pointers for Phase 2
  if (changed)
    invalidateAndRebuildGraph();

  /// PHASE 2: Overlap analysis - set twin_diff=false where proven disjoint
  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);
    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      DbAllocOp allocOp = allocNode->getDbAllocOp();
      if (!allocOp || allocNode->getAcquireNodesSize() == 0)
        return;

      if (!allocNode->canProveNonOverlapping())
        return;

      bool isPartitioned = partitionSuccess.contains(allocOp.getOperation());
      TwinDiffProof proof = isPartitioned ? TwinDiffProof::PartitionSuccess
                                          : TwinDiffProof::AliasAnalysis;
      allocNode->forEachChildNode([&](NodeBase *child) {
        if (auto *acqNode = dyn_cast<DbAcquireNode>(child)) {
          DbAcquireOp acqOp = acqNode->getDbAcquireOp();
          if (acqOp && !acqOp.hasTwinDiff())
            setTwinAttr(acqOp, proof);
        }
      });
    });
  });

  /// PHASE 3: Default twin_diff=true for remaining acquires
  module.walk([&](DbAcquireOp acq) {
    if (!acq.hasTwinDiff())
      setTwinAttr(acq, TwinDiffProof::None);
  });

  if (changed)
    invalidateAndRebuildGraph();

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
    if (*existingMode != PartitionMode::coarse) {
      ARTS_DEBUG("  Already partitioned as "
                 << mlir::arts::stringifyPartitionMode(*existingMode)
                 << " - SKIP");
      heuristics.recordDecision(
          "Partition-AlreadyPartitioned", false,
          "allocation already has partition attribute, skipping",
          allocOp.getOperation(), {});
      return allocOp; /// Skip - already partitioned!
    }
    /// mode == none means CreateDbs couldn't partition - continue to re-analyze
    ARTS_DEBUG("  Partition mode is "
               << mlir::arts::stringifyPartitionMode(*existingMode)
               << " - re-analyzing with loop structure");
    heuristics.recordDecision(
        "Partition-ReanalyzeNone", true,
        "CreateDbs set none, attempting loop-based chunk detection",
        allocOp.getOperation(), {});
  }

  /// Already fine-grained by structure - skip
  if (allocNode && DatablockUtils::isFineGrained(allocNode->getDbAllocOp())) {
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
  bool canUseBlock = true; /// Assume chunked is possible until proven otherwise

  if (allocNode) {
    ctx.accessPatterns = allocNode->summarizeAcquirePatterns();
    ctx.isUniformAccess = ctx.accessPatterns.hasUniform;
    ARTS_DEBUG("  Access patterns: hasUniform="
               << ctx.accessPatterns.hasUniform
               << ", hasStencil=" << ctx.accessPatterns.hasStencil
               << ", hasIndexed=" << ctx.accessPatterns.hasIndexed
               << ", isMixed=" << ctx.accessPatterns.isMixed());

    if (!allocOp.getElementSizes().empty()) {
      int64_t staticFirstDim = 0;
      if (ValueUtils::getConstantIndex(allocOp.getElementSizes().front(),
                                       staticFirstDim)) {
        ctx.totalElements = staticFirstDim;
      }
    }

    allocNode->forEachChildNode([&](NodeBase *child) {
      auto *acqNode = dyn_cast<DbAcquireNode>(child);
      if (!acqNode)
        return;
      DbAcquireOp acqOp = acqNode->getDbAcquireOp();
      if (!acqOp)
        return;
      bool hasIndirect = acqNode->hasIndirectAccess();
      if (hasIndirect) {
        ctx.hasIndirectAccess = true;
        if (acqNode->hasLoads())
          ctx.hasIndirectRead = true;
        if (acqNode->hasStores())
          ctx.hasIndirectWrite = true;
      }
      if (acqNode->hasDirectAccess())
        ctx.hasDirectAccess = true;
    });

    /// Note: Indirect access is handled via full-range acquires, not by
    /// disabling chunked. Full-range acquires allow indirect reads to access
    /// all chunks.
    if (ctx.hasIndirectAccess) {
      ARTS_DEBUG("  Indirect access detected - indirect acquires will use "
                 "full-range");
    }

    /// Build per-acquire capabilities for heuristic voting
    size_t idx = 0;
    allocNode->forEachChildNode([&](NodeBase *child) {
      auto *acqNode = dyn_cast<DbAcquireNode>(child);
      if (!acqNode || idx >= acquireInfos.size())
        return;

      auto &acqInfo = acquireInfos[idx];

      /// Check for non-leading partition offset (disables chunked mode).
      /// Non-leading means the first array index isn't directly derived from
      /// the partition offset, so chunk-based indexing won't work correctly.
      bool hasIndirect = acqNode->hasIndirectAccess();
      bool hasDirect = acqNode->hasDirectAccess();

      /// Read partition capability from acquire's attribute.
      /// ForLowering sets this to 'chunked' when adding offset/size hints.
      /// CreateDbs sets this based on DbControlOp analysis.
      auto acquire = acqNode->getDbAcquireOp();
      auto acquireMode = getPartitionMode(acquire.getOperation());
      bool thisAcquireCanBlock =
          acquireMode && *acquireMode == PartitionMode::block;
      bool thisAcquireCanElementWise =
          acquireMode && *acquireMode == PartitionMode::fine_grained;

      /// Additional validation: check for non-leading partition offset
      if (canUseBlock && thisAcquireCanBlock) {
        Value offsetForCheck = getPartitionOffset(acqNode, &acqInfo);
        if (offsetForCheck &&
            !acqNode->canPartitionWithOffset(offsetForCheck)) {
          if (acqNode->getAccessPattern() == AccessPattern::Stencil) {
            ARTS_DEBUG("  Stencil access with non-leading offset; keeping "
                       "chunked partitioning");
          } else if (hasIndirect && !hasDirect) {
            ARTS_DEBUG("  Non-leading offset on indirect access; "
                       "disabling chunked for this acquire");
            thisAcquireCanBlock = false;
          } else if (hasIndirect && hasDirect) {
            ARTS_DEBUG("  Non-leading offset on mixed access; "
                       "keeping chunked for versioning");
          } else {
            ARTS_DEBUG("  Non-leading partition offset detected; "
                       "disabling chunked partitioning for allocation");
            canUseBlock = false;
            thisAcquireCanBlock = false;
          }
        }
      }

      /// Build AcquireInfo for heuristic voting
      AcquireInfo info;
      info.accessMode = acquire.getMode();
      info.canElementWise =
          thisAcquireCanElementWise || !acquire.getPartitionIndices().empty();
      info.canBlock = canUseBlock && thisAcquireCanBlock;

      /// Populate unified partition infos from DbAcquireOp.
      /// Heuristics will compute uniformity and decide outerRank.
      info.partitionInfos = acquire.getPartitionInfos();

      /// Populate partition mode from AcquirePartitionInfo
      if (idx < acquireInfos.size()) {
        const auto &acqPartInfo = acquireInfos[idx];
        info.partitionMode = acqPartInfo.mode;
      }

      ctx.acquires.push_back(info);
      ARTS_DEBUG("    Acquire["
                 << idx << "]: mode=" << static_cast<int>(info.accessMode)
                 << ", canEW=" << info.canElementWise
                 << ", canBlock=" << info.canBlock << ", acquireAttr="
                 << (acquireMode ? static_cast<int>(*acquireMode) : -1));
      ++idx;
    });
  }

  /// Determine allocation-level capabilities from per-acquire STRUCTURAL
  /// analysis Always use per-acquire voting since we've updated acquire
  /// attributes based on structural hints (offset_hints, indices, etc.)
  ctx.canElementWise = ctx.anyCanElementWise();
  ctx.canBlock = ctx.anyCanBlock(); /// Uses per-acquire canBlock flags

  ARTS_DEBUG("  Per-acquire voting: canEW="
             << ctx.canElementWise << ", canBlock=" << ctx.canBlock
             << ", modesConsistent=" << modesConsistent);

  /// If no structural capability found, let heuristics handle it
  if (!ctx.canElementWise && !ctx.canBlock) {
    ARTS_DEBUG("  Coarse mode - no partitioning needed");
    resetCoarseAcquireRanges(allocOp, allocNode, builder);
    heuristics.recordDecision(
        "Partition-CoarseMode", false,
        "all acquires are coarse mode, no partitioning needed",
        allocOp.getOperation(), {});
    return allocOp;
  }

  /// If any coarse acquires exist alongside chunked candidates, enable MIXED
  /// mode. This allows chunked partitioning while giving coarse acquires
  /// full-range access.
  if (ctx.canBlock) {
    bool hasCoarse =
        llvm::any_of(acquireInfos, [](const AcquirePartitionInfo &info) {
          return info.mode == PartitionMode::coarse;
        });
    bool hasBlock =
        llvm::any_of(acquireInfos, [](const AcquirePartitionInfo &info) {
          return info.mode == PartitionMode::block;
        });

    if (hasCoarse && hasBlock) {
      /// MIXED MODE: chunked allocation with coarse acquires getting full-range
      ctx.canElementWise = false;
      ARTS_DEBUG(
          "  Mixed mode enabled: chunked allocation with coarse acquires");

      /// Mark coarse acquires for full-range treatment
      for (auto &info : acquireInfos) {
        if (info.mode == PartitionMode::coarse) {
          info.needsFullRange = true;
        }
      }
    } else if (hasCoarse) {
      ctx.canElementWise = false;
      ARTS_DEBUG("  Coarse+chunked mix detected - disabling element-wise");
    }

    /// Mark indirect read-only acquires for full-range treatment.
    /// This enables chunked writes + full-range indirect reads without needing
    /// DbCopy/DbSync (the versioning approach).
    if (ctx.hasIndirectRead && !ctx.hasIndirectWrite) {
      for (auto &info : acquireInfos) {
        if (info.hasIndirectAccess && info.acquire) {
          /// Check if this acquire is read-only
          ArtsMode mode = info.acquire.getMode();
          if (mode == ArtsMode::in) {
            info.needsFullRange = true;
            ARTS_DEBUG("  Marked indirect read-only acquire as full-range: "
                       << info.acquire);
          }
        }
      }
    }
  }

  /// Set pinnedDimCount for logging (heuristics compute min/max on-the-fly)
  ctx.pinnedDimCount = ctx.maxPinnedDimCount();
  ARTS_DEBUG("  Partition dim counts: min=" << ctx.minPinnedDimCount()
                                            << ", max=" << ctx.pinnedDimCount);

  /// Get access mode from allocNode
  if (allocNode)
    ctx.accessMode = allocOp.getMode();

  /// Get canonical block size for Block mode (extract static value if
  /// possible).
  Value dynamicBlockSize;
  if (ctx.canBlock) {
    SmallVector<Value> blockSizeCandidates;
    int64_t staticCandidateCount = 0;

    func::FuncOp func = allocOp->getParentOfType<func::FuncOp>();
    DominanceInfo domInfo(func);

    for (const auto &info : acquireInfos) {
      if (info.mode != PartitionMode::block || !info.partitionSize)
        continue;

      DbAcquireOp acquire = info.acquire;
      if (!acquire)
        continue;

      /// Use partition_indices or partition_offsets as partition info
      Value chunkIndex = info.partitionOffset;
      Value blockSizeVal = info.partitionSize;
      if (!acquire.getPartitionIndices().empty())
        chunkIndex = acquire.getPartitionIndices().front();
      else if (!acquire.getPartitionOffsets().empty())
        chunkIndex = acquire.getPartitionOffsets().front();
      if (!acquire.getPartitionSizes().empty())
        blockSizeVal = acquire.getPartitionSizes().front();

      Value baseCandidate = DatablockUtils::extractBaseBlockSizeCandidate(
          chunkIndex, blockSizeVal);
      if (!baseCandidate)
        baseCandidate = blockSizeVal;
      if (!baseCandidate)
        continue;

      Value idxCandidate =
          ValueUtils::ensureIndexType(baseCandidate, builder, loc);
      if (!idxCandidate)
        continue;

      Value domCandidate = domInfo.properlyDominates(idxCandidate, allocOp)
                               ? idxCandidate
                               : traceBackBlockSize(idxCandidate, allocOp,
                                                    builder, domInfo, loc);
      if (!domCandidate)
        continue;

      if (ValueUtils::isZeroConstant(
              ValueUtils::stripNumericCasts(domCandidate))) {
        ARTS_DEBUG("  Skipping zero block size candidate");
        continue;
      }

      blockSizeCandidates.push_back(domCandidate);

      if (auto staticSize = arts::extractBlockSizeFromHint(domCandidate)) {
        if (*staticSize > 0) {
          ++staticCandidateCount;
          if (!ctx.blockSize || *staticSize < *ctx.blockSize)
            ctx.blockSize = *staticSize;
        }
      }
    }

    if (!blockSizeCandidates.empty()) {
      dynamicBlockSize = blockSizeCandidates.front();
      for (size_t i = 1; i < blockSizeCandidates.size(); ++i) {
        dynamicBlockSize = builder.create<arith::MinUIOp>(
            loc, dynamicBlockSize, blockSizeCandidates[i]);
      }
      heuristics.recordDecision(
          "Partition-CanonicalBlockSize", true,
          "canonicalized block size across acquires", allocOp.getOperation(),
          {{"candidateCount", static_cast<int64_t>(blockSizeCandidates.size())},
           {"staticCandidateCount", staticCandidateCount}});
    } else {
      heuristics.recordDecision(
          "Partition-CanonicalBlockSize", false,
          "no valid block size candidates from acquire hints",
          allocOp.getOperation(), {});
    }
  }

  /// Fallback: use the first observed block size if canonicalization failed.
  if (!dynamicBlockSize) {
    if (consistentMode == PartitionMode::block && reference.partitionSize &&
        !ValueUtils::isZeroConstant(
            ValueUtils::stripNumericCasts(reference.partitionSize))) {
      dynamicBlockSize = reference.partitionSize;
      if (auto staticSize =
              arts::extractBlockSizeFromHint(reference.partitionSize)) {
        if (*staticSize > 0)
          ctx.blockSize = *staticSize;
      }
    } else {
      for (const auto &info : acquireInfos) {
        if (info.mode != PartitionMode::block || !info.partitionSize)
          continue;
        if (ValueUtils::isZeroConstant(
                ValueUtils::stripNumericCasts(info.partitionSize))) {
          ARTS_DEBUG("  Skipping zero fallback block size");
          continue;
        }
        dynamicBlockSize = info.partitionSize;
        if (auto staticSize =
                arts::extractBlockSizeFromHint(info.partitionSize)) {
          if (*staticSize > 0 &&
              (!ctx.blockSize || *staticSize < *ctx.blockSize))
            ctx.blockSize = *staticSize;
        }
        break;
      }
    }
  }

  /// Get memref rank
  if (auto memrefType = allocOp.getElementType().dyn_cast<MemRefType>()) {
    ctx.memrefRank = memrefType.getRank();
    ctx.elementTypeIsMemRef = true;
  } else {
    ctx.memrefRank = allocOp.getElementSizes().size();
    ctx.elementTypeIsMemRef = false;
  }

  /// Step 4: Query heuristics -> PartitioningDecision
  PartitioningDecision decision = heuristics.getPartitioningMode(ctx);

  if (decision.isCoarse()) {
    ARTS_DEBUG("  Heuristics chose coarse - keeping original");
    resetCoarseAcquireRanges(allocOp, allocNode, builder);
    heuristics.recordDecision(
        "Partition-HeuristicsCoarse", false,
        "heuristics chose coarse allocation, keeping original",
        allocOp.getOperation(),
        {{"canBlock", ctx.canBlock ? 1 : 0},
         {"canElementWise", ctx.canElementWise ? 1 : 0}});
    return allocOp;
  }

  /// Disabled: versioned partitioning via DbCopy/DbSync is currently off.
  /// The implementation lives in DbVersionedRewriter for future enablement.

  /// For element-wise partitioning, validate that partition indices match
  /// actual accesses. If not (block-wise pattern detected), fall back to
  /// N-D block mode for proper parallelism.
  ///
  /// Block-wise patterns (like Cholesky's 2D block dependencies) use:
  ///   - Partition indices as block identifiers (not element indices)
  ///   - Loop step as block size per dimension
  SmallVector<Value> blockSizesForNDBlock;
  bool skipAcquireInfoCheck = false;
  if (decision.isFineGrained() && ctx.accessPatterns.hasIndexed) {
    bool validElementWise = validateElementWiseIndices(acquireInfos, allocNode);
    if (validElementWise) {
      /// Valid element-wise: we can skip per-acquire partition info check
      skipAcquireInfoCheck = true;
    } else {
      /// Block-wise pattern detected. Extract block sizes from loop steps
      /// for each partition dimension and use N-D block partitioning.
      ARTS_DEBUG("  Block-wise pattern detected - extracting N-D block sizes");

      /// Find the acquire with partition indices to extract block sizes
      for (auto &info : acquireInfos) {
        auto partitionIndices = info.acquire.getPartitionIndices();
        if (partitionIndices.empty())
          continue;

        /// For each partition index, try to find the enclosing loop step
        for (Value idx : partitionIndices) {
          Value blockSize;
          Operation *parent = info.acquire->getParentOp();
          while (parent) {
            if (auto forOp = dyn_cast<scf::ForOp>(parent)) {
              Value loopIV = forOp.getInductionVar();
              if (ValueUtils::dependsOn(idx, loopIV)) {
                blockSize = forOp.getStep();
                ARTS_DEBUG(
                    "    Found block size from loop step: " << blockSize);
                break;
              }
            }
            parent = parent->getParentOp();
          }
          if (blockSize)
            blockSizesForNDBlock.push_back(blockSize);
        }
        if (!blockSizesForNDBlock.empty())
          break; /// Use first acquire with valid block sizes
      }

      if (!blockSizesForNDBlock.empty()) {
        /// Switch to N-D block mode with extracted block sizes
        unsigned nDims = blockSizesForNDBlock.size();
        decision = PartitioningDecision{
            PartitionMode::block, nDims,
            ctx.memrefRank > nDims ? ctx.memrefRank - nDims : 0,
            "Fallback: N-D block-wise pattern"};
        ARTS_DEBUG("  Falling back to N-D block mode with "
                   << nDims << " partitioned dimensions");
        heuristics.recordDecision(
            "Partition-ElementWiseFallbackToNDBlock", true,
            "block-wise pattern detected, using N-D block partitioning",
            allocOp.getOperation(), {{"numPartitionedDims", (int64_t)nDims}});
      } else {
        /// No block sizes found - fall back to coarse
        ARTS_DEBUG("  Cannot extract block sizes - falling back to coarse");
        heuristics.recordDecision("Partition-ElementWiseFallbackToCoarse",
                                  false,
                                  "block-wise pattern but no block sizes found",
                                  allocOp.getOperation(), {});
        resetCoarseAcquireRanges(allocOp, allocNode, builder);
        return allocOp;
      }
    }
  }

  /// Also try N-D extraction for explicit block decisions that haven't
  /// extracted block sizes yet. This handles cases like Cholesky where the
  /// heuristics already chose block mode but we need to extract the N-D
  /// block sizes from loop steps for proper multi-dimensional partitioning.
  /// Check for both indexed and uniform patterns (block-wise access).
  if (decision.isBlock() &&
      (ctx.accessPatterns.hasIndexed || ctx.accessPatterns.hasUniform) &&
      blockSizesForNDBlock.empty()) {
    ARTS_DEBUG("  Block mode with indexed/uniform patterns - extracting N-D "
               "block sizes");
    for (auto &info : acquireInfos) {
      auto partitionIndices = info.acquire.getPartitionIndices();
      if (partitionIndices.empty())
        continue;

      /// For each partition index, try to find the enclosing loop step
      for (Value idx : partitionIndices) {
        Value blockSize;
        Operation *parent = info.acquire->getParentOp();
        while (parent) {
          if (auto forOp = dyn_cast<scf::ForOp>(parent)) {
            Value loopIV = forOp.getInductionVar();
            if (ValueUtils::dependsOn(idx, loopIV)) {
              blockSize = forOp.getStep();
              ARTS_DEBUG("    Found block size from loop step: " << blockSize);
              break;
            }
          }
          parent = parent->getParentOp();
        }
        if (blockSize)
          blockSizesForNDBlock.push_back(blockSize);
      }
      if (!blockSizesForNDBlock.empty())
        break; /// Use first acquire with valid block sizes
    }

    if (!blockSizesForNDBlock.empty()) {
      /// Upgrade to N-D block mode with extracted block sizes
      unsigned nDims = blockSizesForNDBlock.size();
      decision = PartitioningDecision{
          PartitionMode::block, nDims,
          ctx.memrefRank > nDims ? ctx.memrefRank - nDims : 0,
          "N-D block pattern detected"};
      ARTS_DEBUG("  Upgraded to N-D block mode with " << nDims
                                                      << " dimensions");
      heuristics.recordDecision(
          "Partition-BlockUpgradedToND", true,
          "block mode upgraded with N-D block sizes from loop steps",
          allocOp.getOperation(), {{"numPartitionedDims", (int64_t)nDims}});
    }
  }

  bool allRewriteable =
      skipAcquireInfoCheck ||
      std::all_of(acquireInfos.begin(), acquireInfos.end(), [&](auto &info) {
        /// Full-range acquires don't need offset/size - they get full
        /// allocation
        if (info.needsFullRange)
          return true;
        return info.isValid && info.partitionOffset && info.partitionSize;
      });
  if (!allRewriteable) {
    ARTS_DEBUG("  Some acquires missing partition info - keeping original");
    heuristics.recordDecision(
        "Partition-MissingAcquireInfo", false,
        "some acquires missing partition info, cannot rewrite allocation",
        allocOp.getOperation(), {});
    return allocOp;
  }

  ARTS_DEBUG("  Decision: mode=" << getPartitionModeName(decision.mode)
                                 << ", outerRank=" << decision.outerRank
                                 << ", innerRank=" << decision.innerRank);

  /// Step 5a: Compute stencilInfo early (needed for inner size calculation)
  /// Must be done before computeSizesFromDecision() for stencil mode.
  std::optional<StencilInfo> stencilInfo;
  if (decision.mode == PartitionMode::stencil && allocNode) {
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
              arts::extractBlockSizeFromHint(allocOp.getElementSizes()[0]))
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
  /// For block mode, we need block sizes that dominate the allocOp.
  /// N-D block: use blockSizesForNDBlock extracted from loop steps
  /// 1D block: use ctx.blockSize or dynamicBlockSize
  SmallVector<Value> blockSizesForPlan;
  if (decision.isBlock()) {
    if (!blockSizesForNDBlock.empty()) {
      /// N-D block mode: use extracted block sizes from loop steps
      /// Need to trace/recreate them at allocation point if needed
      DominanceInfo domInfo(allocOp->getParentOfType<func::FuncOp>());
      for (Value bs : blockSizesForNDBlock) {
        if (domInfo.properlyDominates(bs, allocOp)) {
          blockSizesForPlan.push_back(bs);
        } else {
          /// Try to trace back the block size
          Value traced = traceBackBlockSize(bs, allocOp, builder, domInfo, loc);
          if (traced) {
            blockSizesForPlan.push_back(traced);
          } else {
            /// Cannot trace - try to extract constant value
            if (auto constOp = bs.getDefiningOp<arith::ConstantIndexOp>()) {
              blockSizesForPlan.push_back(
                  builder.create<arith::ConstantIndexOp>(loc, constOp.value()));
            } else {
              ARTS_DEBUG("  Cannot recreate N-D block size - clearing");
              blockSizesForPlan.clear();
              break;
            }
          }
        }
      }
      if (!blockSizesForPlan.empty()) {
        ARTS_DEBUG("  Using " << blockSizesForPlan.size()
                              << " N-D block sizes");
      }
    }

    /// Fallback to 1D block if N-D failed or wasn't needed
    if (blockSizesForPlan.empty()) {
      Value blockSizeForPlan;
      if (ctx.blockSize && *ctx.blockSize > 0) {
        /// Use static block size - create a constant at the allocOp point
        blockSizeForPlan =
            builder.create<arith::ConstantIndexOp>(loc, *ctx.blockSize);
      } else if (dynamicBlockSize) {
        /// Check if the dynamic block size dominates the allocOp
        DominanceInfo domInfo(allocOp->getParentOfType<func::FuncOp>());
        if (domInfo.properlyDominates(dynamicBlockSize, allocOp)) {
          blockSizeForPlan = dynamicBlockSize;
        } else {
          /// Try to trace back and recreate the block size at allocation point
          ARTS_DEBUG("  Dynamic block size doesn't dominate - attempting "
                     "trace-back");
          blockSizeForPlan = traceBackBlockSize(dynamicBlockSize, allocOp,
                                                builder, domInfo, loc);
          if (blockSizeForPlan) {
            heuristics.recordDecision(
                "Partition-BlockSizeRecreated", true,
                "recreated block size at allocation point via trace-back",
                allocOp.getOperation(), {});
          }
        }
      }

      if (!blockSizeForPlan) {
        /// No block size available - fall back to coarse
        ARTS_DEBUG("  No block size available for block mode - falling back to "
                   "coarse");
        heuristics.recordDecision(
            "Partition-NoBlockSize", false,
            "block mode requested but no block size available",
            allocOp.getOperation(), {});
        resetCoarseAcquireRanges(allocOp, allocNode, builder);
        return allocOp;
      }
      blockSizesForPlan.push_back(blockSizeForPlan);
    }
  }

  /// Validate block sizes
  if (decision.isBlock()) {
    if (blockSizesForPlan.empty()) {
      ARTS_DEBUG("  Missing block size for block decision - keeping original");
      heuristics.recordDecision(
          "Partition-BlockSizeMissing", false,
          "block decision without valid block sizes, keeping original",
          allocOp.getOperation(), {});
      return allocOp;
    }

    /// Clamp any zero block sizes to 1
    Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
    for (unsigned i = 0; i < blockSizesForPlan.size(); ++i) {
      Value strippedBlockSize =
          ValueUtils::stripNumericCasts(blockSizesForPlan[i]);
      if (ValueUtils::isZeroConstant(strippedBlockSize)) {
        ARTS_DEBUG("  Block size[" << i << "] is zero - clamping to 1");
        blockSizesForPlan[i] = one;
      }
      blockSizesForPlan[i] =
          builder.create<arith::MaxUIOp>(loc, blockSizesForPlan[i], one);
    }
  }

  /// For stencil mode, inner size must be EXTENDED (base + halos) to hold halo
  /// data. The outer size (numBlocks) must still be computed from the BASE
  /// block size so block indices remain consistent with the original grid.
  Value firstBlockSize =
      blockSizesForPlan.empty() ? Value() : blockSizesForPlan.front();
  Value innerBlockSize = firstBlockSize;
  bool extendInnerForStencil =
      stencilInfo && stencilInfo->hasHalo() && firstBlockSize;
  if (extendInnerForStencil) {
    int64_t haloTotal = stencilInfo->haloLeft + stencilInfo->haloRight;
    Value haloConst = builder.create<arith::ConstantIndexOp>(loc, haloTotal);
    innerBlockSize =
        builder.create<arith::AddIOp>(loc, firstBlockSize, haloConst);
    ARTS_DEBUG("  Extended inner size for stencil: base + " << haloTotal);
  }

  SmallVector<Value> newOuterSizes, newInnerSizes;
  computeSizesFromDecision(allocOp, decision, blockSizesForPlan, newOuterSizes,
                           newInnerSizes);
  if (extendInnerForStencil) {
    if (!newInnerSizes.empty())
      newInnerSizes[0] = innerBlockSize;
    else
      newInnerSizes.push_back(innerBlockSize);
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
  plan.blockSizes = blockSizesForPlan;
  if (!blockSizesForPlan.empty())
    plan.blockSize = blockSizesForPlan.front(); /// Backward compatibility
  plan.numBlocksPerDim.assign(newOuterSizes.begin(), newOuterSizes.end());

  ARTS_DEBUG("  Plan created: blockSizes=" << plan.blockSizes.size()
                                           << ", numPartitionedDims="
                                           << plan.numPartitionedDims());

  /// Mixed mode: set flag and store numBlocks for full-range acquires
  bool hasMixedMode = llvm::any_of(
      acquireInfos, [](const auto &info) { return info.needsFullRange; });
  if (hasMixedMode && decision.isBlock()) {
    plan.isMixedMode = true;
    if (!newOuterSizes.empty())
      plan.numBlocks = newOuterSizes[0];
    ARTS_DEBUG("  Mixed mode plan: numBlocks=" << plan.numBlocks);
  }

  /// ESD: Use early-computed stencilInfo for Stencil mode
  if (stencilInfo)
    plan.stencilInfo = *stencilInfo;
  /// Note: byte_offset/byte_size for ESD is computed in EdtLowering from
  /// element_offsets/element_sizes, using allocation's elementSizes for stride.

  /// Build DbRewriteAcquire list from AcquirePartitionInfo
  /// Include ALL acquires - invalid ones get Coarse fallback (offset=0,
  /// size=totalSize)
  SmallVector<DbRewriteAcquire> rewriteAcquires;
  Value zero = builder.create<arith::ConstantIndexOp>(allocOp.getLoc(), 0);
  for (auto &info : acquireInfos) {
    if (!info.acquire)
      continue;

    DbRewriteAcquire rewriteInfo;
    rewriteInfo.acquire = info.acquire;

    Value one = builder.create<arith::ConstantIndexOp>(allocOp.getLoc(), 1);

    if (info.needsFullRange && decision.isBlock()) {
      /// Coarse acquire in mixed mode: full-range access.
      rewriteInfo.isFullRange = true;
    } else if (info.isValid) {
      /// Populate multi-dimensional offsets/sizes from AcquirePartitionInfo.
      /// For fine-grained: use partition indices as offsets, sizes are 1.
      /// For chunked: use partition offset and size.
      if (decision.isFineGrained()) {
        /// Get partition indices from the acquire's partition hints
        auto partitionIndices = info.acquire.getPartitionIndices();
        for (Value idx : partitionIndices) {
          rewriteInfo.elemOffsets.push_back(idx);
          rewriteInfo.elemSizes.push_back(one);
        }
        /// Fallback if no partition indices
        if (rewriteInfo.elemOffsets.empty()) {
          rewriteInfo.elemOffsets.push_back(info.partitionOffset);
          rewriteInfo.elemSizes.push_back(info.partitionSize);
        }
      } else if (decision.isBlock() && plan.numPartitionedDims() > 1) {
        /// N-D Block mode: extract partition indices like fine-grained mode
        /// Each partition index represents the start of the block in that
        /// dimension
        auto partitionIndices = info.acquire.getPartitionIndices();
        for (Value idx : partitionIndices) {
          rewriteInfo.elemOffsets.push_back(idx);
          /// For block mode, size is the block size (not 1 like fine-grained)
          unsigned dimIdx = rewriteInfo.elemOffsets.size() - 1;
          Value blockSz = plan.getBlockSize(dimIdx);
          rewriteInfo.elemSizes.push_back(blockSz ? blockSz : one);
        }
        /// Fallback if no partition indices
        if (rewriteInfo.elemOffsets.empty()) {
          rewriteInfo.elemOffsets.push_back(info.partitionOffset);
          rewriteInfo.elemSizes.push_back(info.partitionSize);
        }
      } else {
        /// 1D Block or other modes: use single offset/size
        rewriteInfo.elemOffsets.push_back(info.partitionOffset);
        rewriteInfo.elemSizes.push_back(info.partitionSize);
      }
    } else {
      /// Invalid acquire: use Coarse fallback (full size access)
      /// The acquire still needs rewriting to use the new allocation structure.
      Value totalSize = allocOp.getElementSizes().empty()
                            ? zero
                            : allocOp.getElementSizes().front();
      rewriteInfo.elemOffsets.push_back(zero);
      rewriteInfo.elemSizes.push_back(totalSize);
      ARTS_DEBUG("  Invalid acquire " << info.acquire
                                      << " rewritten with Coarse fallback");
    }

    if (decision.isFineGrained() && info.hasIndirectAccess) {
      Value totalSize = allocOp.getElementSizes().empty()
                            ? zero
                            : allocOp.getElementSizes().front();
      rewriteInfo.elemOffsets.clear();
      rewriteInfo.elemSizes.clear();
      rewriteInfo.elemOffsets.push_back(zero);
      rewriteInfo.elemSizes.push_back(totalSize);
    }

    rewriteAcquires.push_back(rewriteInfo);
  }

  auto rewriter = DbRewriter::create(allocOp, newOuterSizes, newInnerSizes,
                                     rewriteAcquires, plan);
  auto result = rewriter->apply(builder);

  /// Step 7: Set partition attribute on new alloc
  if (succeeded(result)) {
    setPartitionMode(*result, decision.mode);

    AM->getMetadataManager().transferMetadata(allocOp, result.value());

    /// Record successful partition decision
    StringRef modeName = getPartitionModeName(decision.mode);
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
  SmallVector<Value> partIndices(acquireOp.getPartitionIndices().begin(),
                                 acquireOp.getPartitionIndices().end());
  SmallVector<Value> partOffsets(acquireOp.getPartitionOffsets().begin(),
                                 acquireOp.getPartitionOffsets().end());
  SmallVector<Value> partSizes(acquireOp.getPartitionSizes().begin(),
                               acquireOp.getPartitionSizes().end());

  auto partitionMode = getPartitionMode(acquireOp.getOperation());
  auto newAcquire = builder.create<DbAcquireOp>(
      loc, acquireOp.getMode(), acquireOp.getSourceGuid(),
      acquireOp.getSourcePtr(), partitionMode, indicesVec, offsetsVec, sizesVec,
      partIndices, partOffsets, partSizes, boundsValid);
  transferAttributes(acquireOp.getOperation(), newAcquire.getOperation(),
                     /*excludes=*/{});
  newAcquire.copyPartitionSegmentsFrom(acquireOp);

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
std::unique_ptr<Pass> createDbPartitioningPass(ArtsAnalysisManager *AM) {
  return std::make_unique<DbPartitioningPass>(AM);
}
} // namespace arts
} // namespace mlir
