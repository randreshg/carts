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
#include "llvm/ADT/DenseMap.h"
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
  /// N-D partition support: vectors for all dimensions
  SmallVector<Value> partitionOffsets;
  SmallVector<Value> partitionSizes;
  SmallVector<unsigned> partitionDims;
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
    /// For Block mode, sizes must be same SSA value (check first dimension)
    if (mode == PartitionMode::block && !partitionSizes.empty() &&
        !other.partitionSizes.empty()) {
      Value thisSize = partitionSizes.front();
      Value otherSize = other.partitionSizes.front();
      if (thisSize != otherSize) {
        int64_t lhs = 0, rhs = 0;
        bool lhsConst = ValueUtils::getConstantIndex(thisSize, lhs);
        bool rhsConst = ValueUtils::getConstantIndex(otherSize, rhs);
        if (!(lhsConst && rhsConst && lhs == rhs))
          return false;
      }
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

/// Extract all partition offsets for N-D support.
/// Unified function that handles both single and multi-dimensional cases.
/// Use front() on the result for 1-D compatibility.
static SmallVector<Value>
getPartitionOffsetsND(DbAcquireNode *acqNode,
                      const AcquirePartitionInfo *info) {
  if (info && !info->partitionOffsets.empty())
    return info->partitionOffsets;

  SmallVector<Value> result;
  DbAcquireOp acqOp = acqNode->getDbAcquireOp();

  /// Check partition hints from partition_indices first
  auto indices = acqOp.getPartitionIndices();
  if (!indices.empty()) {
    result.append(indices.begin(), indices.end());
    return result;
  }

  /// Fall back to partition_offsets
  auto offsets = acqOp.getPartitionOffsets();
  if (!offsets.empty()) {
    result.append(offsets.begin(), offsets.end());
    return result;
  }

  /// Finally, try getPartitionInfo for single-dim case
  auto [offset, size] = acqNode->getPartitionInfo();
  if (offset)
    result.push_back(offset);

  return result;
}

/// Pick a representative partition offset (prefer non-constant).
/// Returns the chosen offset and its index via outIdx when provided.
static Value pickRepresentativePartitionOffset(ArrayRef<Value> offsets,
                                               unsigned *outIdx = nullptr) {
  if (outIdx)
    *outIdx = 0;
  for (unsigned i = 0; i < offsets.size(); ++i) {
    Value off = offsets[i];
    if (!off)
      continue;
    int64_t c = 0;
    if (!ValueUtils::getConstantIndex(ValueUtils::stripNumericCasts(off), c)) {
      if (outIdx)
        *outIdx = i;
      return off;
    }
  }
  for (unsigned i = 0; i < offsets.size(); ++i) {
    if (offsets[i]) {
      if (outIdx)
        *outIdx = i;
      return offsets[i];
    }
  }
  return Value();
}

/// Pick a partition size that matches the chosen offset index.
static Value pickRepresentativePartitionSize(ArrayRef<Value> sizes,
                                             unsigned idx) {
  if (sizes.empty())
    return Value();
  if (idx < sizes.size() && sizes[idx])
    return sizes[idx];
  return sizes.front();
}

/// Analyze a single acquire to determine its partition mode and chunk info.
static AcquirePartitionInfo computeAcquirePartitionInfo(DbAcquireOp acquire,
                                                        DbAcquireNode *acqNode,
                                                        DbAllocOp allocOp,
                                                        OpBuilder &builder,
                                                        Location loc) {
  auto refineSizeFromMinInBlock = [&](Value offset, Value sizeHint) -> Value {
    if (!offset || !sizeHint)
      return Value();
    int64_t sizeConst = 0;
    bool sizeIsConst = ValueUtils::getConstantIndex(sizeHint, sizeConst);
    auto isSameConst = [&](Value v) -> bool {
      int64_t val = 0;
      return ValueUtils::getConstantIndex(v, val) && val == sizeConst;
    };
    for (Operation &op : *acquire->getBlock()) {
      Value refined;
      auto tryRefine = [&](Value lhs, Value rhs, Value result) {
        bool lhsIsHint = (lhs == sizeHint) || (sizeIsConst && isSameConst(lhs));
        bool rhsIsHint = (rhs == sizeHint) || (sizeIsConst && isSameConst(rhs));
        if (lhsIsHint && ValueUtils::dependsOn(rhs, offset))
          refined = result;
        else if (rhsIsHint && ValueUtils::dependsOn(lhs, offset))
          refined = result;
      };

      if (auto minOp = dyn_cast<arith::MinUIOp>(&op)) {
        tryRefine(minOp.getLhs(), minOp.getRhs(), minOp.getResult());
      } else if (auto minOp = dyn_cast<arith::MinSIOp>(&op)) {
        tryRefine(minOp.getLhs(), minOp.getRhs(), minOp.getResult());
      }

      if (refined)
        return refined;
    }
    return Value();
  };

  auto inferPartitionDims = [&](AcquirePartitionInfo &info) {
    if (!acqNode || info.partitionOffsets.empty())
      return;
    info.partitionDims.clear();
    SmallVector<unsigned, 4> seen;
    for (Value off : info.partitionOffsets) {
      if (!off) {
        info.partitionDims.clear();
        return;
      }
      auto dimOpt =
          acqNode->getPartitionOffsetDim(off, /*requireLeading=*/false);
      if (!dimOpt) {
        info.partitionDims.clear();
        return;
      }
      bool alreadySeen = false;
      for (unsigned seenDim : seen) {
        if (seenDim == *dimOpt) {
          alreadySeen = true;
          break;
        }
      }
      if (alreadySeen) {
        info.partitionDims.clear();
        return;
      }
      seen.push_back(*dimOpt);
      info.partitionDims.push_back(*dimOpt);
    }
  };

  AcquirePartitionInfo info;
  info.acquire = acquire;
  info.mode = DatablockUtils::getPartitionModeFromStructure(acquire);
  if (info.mode == PartitionMode::coarse) {
    if (!acquire.getPartitionIndices().empty())
      info.mode = PartitionMode::fine_grained;
    else if (!acquire.getPartitionOffsets().empty() ||
             !acquire.getPartitionSizes().empty())
      info.mode = PartitionMode::block;
  }
  if (acqNode)
    info.hasIndirectAccess = acqNode->hasIndirectAccess();

  if (acqNode) {
    if (acqNode->getAccessPattern() == AccessPattern::Stencil)
      info.mode = PartitionMode::stencil;
  } else if (acquire.hasMultiplePartitionEntries()) {
    int64_t minOffset = 0;
    int64_t maxOffset = 0;
    if (DatablockUtils::hasMultiEntryStencilPattern(acquire, minOffset,
                                                    maxOffset)) {
      info.mode = PartitionMode::stencil;
    }
  }

  switch (info.mode) {
  case PartitionMode::coarse:
    /// No partitioning needed
    info.isValid = true;
    if (acqNode) {
      Value offset, size;
      if (succeeded(acqNode->computeBlockInfo(offset, size))) {
        info.partitionOffsets.push_back(offset);
        info.partitionSizes.push_back(size);
        unsigned offsetIdx = 0;
        Value repOffset = pickRepresentativePartitionOffset(
            info.partitionOffsets, &offsetIdx);
        Value repSize =
            pickRepresentativePartitionSize(info.partitionSizes, offsetIdx);
        if (Value refined = refineSizeFromMinInBlock(repOffset, repSize)) {
          if (offsetIdx < info.partitionSizes.size())
            info.partitionSizes[offsetIdx] = refined;
          else if (!info.partitionSizes.empty())
            info.partitionSizes[0] = refined;
          ARTS_DEBUG("  Refined partition size from min: "
                     << info.partitionSizes.front());
        }
        inferPartitionDims(info);
      } else {
        info.isValid = false;
      }
    } else {
      info.isValid = false;
    }
    break;

  case PartitionMode::fine_grained:
    /// Use partition_indices as partition offsets (element-space hints)
    /// N-D support: iterate ALL partition indices
    if (!acquire.getPartitionIndices().empty()) {
      for (Value idx : acquire.getPartitionIndices())
        info.partitionOffsets.push_back(idx);
      /// Each dimension has size 1 for fine-grained
      Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
      for (unsigned i = 0; i < info.partitionOffsets.size(); ++i)
        info.partitionSizes.push_back(one);
      info.isValid = true;
      inferPartitionDims(info);
    }
    break;

  case PartitionMode::block:
  case PartitionMode::stencil:
    /// FIRST: Trust partition hints from CreateDbs (authoritative source).
    /// CreateDbs sets partition_offsets/sizes based on DbControlOp analysis
    /// which happens BEFORE EDT outlining. Re-validation inside EDT scope
    /// fails because the partition offset (e.g., outer loop IV) is not visible.
    if (!acquire.getPartitionOffsets().empty()) {
      /// N-D support: use all partition offsets/sizes
      for (Value off : acquire.getPartitionOffsets())
        info.partitionOffsets.push_back(off);
      auto sizes = acquire.getPartitionSizes();
      Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
      for (unsigned i = 0; i < info.partitionOffsets.size(); ++i) {
        info.partitionSizes.push_back(i < sizes.size() ? sizes[i] : one);
      }
      info.isValid = true;
      unsigned offsetIdx = 0;
      Value repOffset =
          pickRepresentativePartitionOffset(info.partitionOffsets, &offsetIdx);
      Value repSize =
          pickRepresentativePartitionSize(info.partitionSizes, offsetIdx);
      ARTS_DEBUG("  Using partition hints from CreateDbs: offset="
                 << repOffset << ", size=" << repSize);

      if (Value refined = refineSizeFromMinInBlock(repOffset, repSize)) {
        if (offsetIdx < info.partitionSizes.size())
          info.partitionSizes[offsetIdx] = refined;
        else if (!info.partitionSizes.empty())
          info.partitionSizes[0] = refined;
        ARTS_DEBUG("  Refined partition size from min: "
                   << info.partitionSizes.front());
      }

      /// If loop analysis yields a dynamic size tied to the same offset,
      /// prefer it over static hints. Avoid overriding with unrelated loops.
      if (acqNode) {
        Value loopOffset, loopSize;
        if (succeeded(acqNode->computeBlockInfo(loopOffset, loopSize))) {
          bool useLoopSize = false;
          int64_t hintConst = 0;
          int64_t loopConst = 0;
          Value hintSize =
              pickRepresentativePartitionSize(info.partitionSizes, offsetIdx);
          Value hintOff = pickRepresentativePartitionOffset(
              info.partitionOffsets, &offsetIdx);
          bool hintIsConst =
              hintSize && ValueUtils::getConstantIndex(hintSize, hintConst);
          bool loopIsConst = ValueUtils::getConstantIndex(loopSize, loopConst);

          bool offsetRelated = false;
          if (hintOff && loopOffset) {
            Value hintOffStripped = ValueUtils::stripNumericCasts(hintOff);
            Value loopOff = ValueUtils::stripNumericCasts(loopOffset);
            if (hintOffStripped == loopOff)
              offsetRelated = true;
            if (!offsetRelated &&
                (ValueUtils::dependsOn(loopOff, hintOffStripped) ||
                 ValueUtils::dependsOn(hintOffStripped, loopOff)))
              offsetRelated = true;
            if (!offsetRelated) {
              int64_t hintOffConst = 0;
              int64_t loopOffConst = 0;
              if (ValueUtils::getConstantIndex(hintOffStripped, hintOffConst) &&
                  ValueUtils::getConstantIndex(loopOff, loopOffConst) &&
                  hintOffConst == loopOffConst)
                offsetRelated = true;
            }
          }

          bool loopSizeDependsOnOffset =
              hintOff && ValueUtils::dependsOn(loopSize, hintOff);

          if (loopSizeDependsOnOffset)
            useLoopSize = true;
          if (offsetRelated && !loopIsConst)
            useLoopSize = true;
          if (offsetRelated && hintIsConst && loopIsConst &&
              hintConst != loopConst)
            useLoopSize = true;

          if (useLoopSize) {
            if (offsetIdx < info.partitionSizes.size())
              info.partitionSizes[offsetIdx] = loopSize;
            else if (!info.partitionSizes.empty())
              info.partitionSizes[0] = loopSize;
            else
              info.partitionSizes.push_back(loopSize);
            ARTS_DEBUG("  Overriding partition size from loop: "
                       << info.partitionSizes.front());
          }
        }
      }
      inferPartitionDims(info);
      break;
    }
    /// FALLBACK: Try computeBlockInfo for cases without CreateDbs hints.
    /// This handles legacy paths or manually constructed IR.
    if (acqNode) {
      Value offset, size;
      if (succeeded(acqNode->computeBlockInfo(offset, size))) {
        info.partitionOffsets.push_back(offset);
        info.partitionSizes.push_back(size);
        info.isValid = true;
        ARTS_DEBUG("  Computed block info from access analysis");
        inferPartitionDims(info);
      } else {
        /// Validation failed -> fall back to coarse for correctness.
        info.mode = PartitionMode::coarse;
        ARTS_DEBUG("  Block info computation failed, falling back to coarse");
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
    /// Clear partition hints for coarse allocations to avoid redundant
    /// per-task hint plumbing and enable invariant acquire hoisting.
    acqOp.getPartitionIndicesMutable().clear();
    acqOp.getPartitionOffsetsMutable().clear();
    acqOp.getPartitionSizesMutable().clear();
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
                                     ArrayRef<unsigned> partitionedDims,
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
    if (!partitionedDims.empty()) {
      /// Non-leading partitioning: outer sizes follow partitionedDims order,
      /// inner sizes preserve original dimension order.
      SmallVector<int64_t> dimToPart(elemSizes.size(), -1);
      for (unsigned p = 0; p < partitionedDims.size(); ++p) {
        if (partitionedDims[p] < elemSizes.size())
          dimToPart[partitionedDims[p]] = static_cast<int64_t>(p);
      }

      for (unsigned p = 0; p < nPartDims; ++p) {
        unsigned dim =
            p < partitionedDims.size() ? partitionedDims[p] : p;
        if (dim >= elemSizes.size())
          continue;
        Value bs = blockSizes[p];
        if (!bs ||
            ValueUtils::isZeroConstant(ValueUtils::stripNumericCasts(bs)))
          continue;

        Value dimSize = elemSizes[dim];

        /// numBlocks = ceil(dim / blockSize) = (dim + blockSize - 1) / blockSize
        Value dimI64 =
            builder.create<arith::IndexCastOp>(loc, i64Type, dimSize);
        Value bsI64 = builder.create<arith::IndexCastOp>(loc, i64Type, bs);
        Value sum = builder.create<arith::AddIOp>(loc, dimI64, bsI64);
        Value sumMinusOne = builder.create<arith::SubIOp>(loc, sum, oneI64);
        Value numBlocksI64 =
            builder.create<arith::DivUIOp>(loc, sumMinusOne, bsI64);
        Value numBlocks = builder.create<arith::IndexCastOp>(
            loc, builder.getIndexType(), numBlocksI64);

        newOuterSizes.push_back(numBlocks);
      }

      for (unsigned d = 0; d < elemSizes.size(); ++d) {
        int64_t partIdx = dimToPart[d];
        if (partIdx >= 0 && static_cast<unsigned>(partIdx) < blockSizes.size())
          newInnerSizes.push_back(blockSizes[partIdx]);
        else
          newInnerSizes.push_back(elemSizes[d]);
      }
    } else {
      for (unsigned d = 0; d < nPartDims && d < elemSizes.size(); ++d) {
        Value bs = blockSizes[d];
        if (!bs ||
            ValueUtils::isZeroConstant(ValueUtils::stripNumericCasts(bs)))
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
    }

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

/// Fallback: derive a dominating block size from allocation size + workers.
/// This keeps block partitioning viable when hints are loop-scoped and
/// non-dominating. The formula is ceil(elemSize / workers), clamped to >= 1.
static Value computeDefaultBlockSize(DbAllocOp allocOp, OpBuilder &builder,
                                     Location loc, bool useNodes) {
  if (allocOp.getElementSizes().empty())
    return nullptr;

  Value elemSize = allocOp.getElementSizes().front();
  if (!elemSize)
    return nullptr;

  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
  Value parallelI32 = useNodes
                          ? builder.create<GetTotalNodesOp>(loc).getResult()
                          : builder.create<GetTotalWorkersOp>(loc).getResult();
  Value workers = builder.create<arith::IndexCastUIOp>(
      loc, builder.getIndexType(), parallelI32);
  Value workersClamped = builder.create<arith::MaxUIOp>(loc, workers, one);

  Value workersMinusOne =
      builder.create<arith::SubIOp>(loc, workersClamped, one);
  Value adjusted =
      builder.create<arith::AddIOp>(loc, elemSize, workersMinusOne);
  Value blockSize =
      builder.create<arith::DivUIOp>(loc, adjusted, workersClamped);
  blockSize = builder.create<arith::MaxUIOp>(loc, blockSize, one);
  return blockSize;
}

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
    if (auto blockArg = dyn_cast<BlockArgument>(blockSize)) {
      if (Value traced = traceValueThroughEdt(blockSize, allocOp, domInfo))
        return traced;
    }
    return nullptr;
  }

  auto traceOperand = [&](Value v) -> Value {
    return getTracedOrDominatingOperand(v, allocOp, builder, domInfo, loc);
  };
  auto traceCond = [&](Value v) -> Value {
    return getDominatingOperand(v, allocOp, domInfo);
  };

  /// Division patterns (block_size = N / tasks)
  if (auto op = dyn_cast<arith::DivSIOp>(defOp))
    return ValueUtils::traceBinaryOp(op, allocOp, builder, loc, traceOperand);
  if (auto op = dyn_cast<arith::DivUIOp>(defOp))
    return ValueUtils::traceBinaryOp(op, allocOp, builder, loc, traceOperand);
  if (auto op = dyn_cast<arith::CeilDivSIOp>(defOp))
    return ValueUtils::traceBinaryOp(op, allocOp, builder, loc, traceOperand);
  if (auto op = dyn_cast<arith::CeilDivUIOp>(defOp))
    return ValueUtils::traceBinaryOp(op, allocOp, builder, loc, traceOperand);

  /// Cast patterns (unwrap and trace source)
  if (auto castOp = dyn_cast<arith::IndexCastOp>(defOp)) {
    if (Value inner = traceBackBlockSize(castOp.getIn(), allocOp, builder,
                                         domInfo, loc)) {
      if (inner.getType() == castOp.getType())
        return inner;
      return builder.create<arith::IndexCastOp>(loc, castOp.getType(), inner);
    }
    return nullptr;
  }
  if (auto castOp = dyn_cast<arith::IndexCastUIOp>(defOp)) {
    if (Value inner = traceBackBlockSize(castOp.getIn(), allocOp, builder,
                                         domInfo, loc)) {
      if (inner.getType() == castOp.getType())
        return inner;
      return builder.create<arith::IndexCastUIOp>(loc, castOp.getType(), inner);
    }
    return nullptr;
  }
  if (auto castOp = dyn_cast<arith::ExtSIOp>(defOp)) {
    if (Value inner =
            traceBackBlockSize(castOp.getIn(), allocOp, builder, domInfo, loc))
      return builder.create<arith::ExtSIOp>(loc, castOp.getType(), inner);
    return nullptr;
  }
  if (auto castOp = dyn_cast<arith::ExtUIOp>(defOp)) {
    if (Value inner =
            traceBackBlockSize(castOp.getIn(), allocOp, builder, domInfo, loc))
      return builder.create<arith::ExtUIOp>(loc, castOp.getType(), inner);
    return nullptr;
  }
  if (auto castOp = dyn_cast<arith::TruncIOp>(defOp)) {
    if (Value inner =
            traceBackBlockSize(castOp.getIn(), allocOp, builder, domInfo, loc))
      return builder.create<arith::TruncIOp>(loc, castOp.getType(), inner);
    return nullptr;
  }

  /// Bounds patterns (clamping with max/min)
  if (auto op = dyn_cast<arith::MaxSIOp>(defOp))
    return ValueUtils::traceBinaryOp(op, allocOp, builder, loc, traceOperand);
  if (auto op = dyn_cast<arith::MaxUIOp>(defOp))
    return ValueUtils::traceBinaryOp(op, allocOp, builder, loc, traceOperand);
  if (auto op = dyn_cast<arith::MinSIOp>(defOp))
    return ValueUtils::traceMinSIWithFallback(op, allocOp, builder, loc,
                                              traceOperand);
  if (auto op = dyn_cast<arith::MinUIOp>(defOp))
    return ValueUtils::traceMinUIWithFallback(op, allocOp, builder, loc,
                                              traceOperand);

  /// Conditional pattern (select with fallback)
  if (auto op = dyn_cast<arith::SelectOp>(defOp))
    return ValueUtils::traceSelectWithFallback(op, allocOp, builder, loc,
                                               traceOperand, traceCond);

  /// Arithmetic patterns (add, sub, mul)
  if (auto op = dyn_cast<arith::SubIOp>(defOp))
    return ValueUtils::traceBinaryOp(op, allocOp, builder, loc, traceOperand);
  if (auto op = dyn_cast<arith::AddIOp>(defOp))
    return ValueUtils::traceBinaryOp(op, allocOp, builder, loc, traceOperand);
  if (auto op = dyn_cast<arith::MulIOp>(defOp))
    return ValueUtils::traceBinaryOp(op, allocOp, builder, loc, traceOperand);

  /// ARTS runtime queries: recreate at allocation point
  if (isa<GetTotalWorkersOp>(defOp)) {
    builder.setInsertionPoint(allocOp);
    return builder.create<GetTotalWorkersOp>(loc).getResult();
  }
  if (isa<GetTotalNodesOp>(defOp)) {
    builder.setInsertionPoint(allocOp);
    return builder.create<GetTotalNodesOp>(loc).getResult();
  }

  /// Constant patterns (recreate at allocation point)
  if (auto constOp = dyn_cast<arith::ConstantOp>(defOp)) {
    if (auto intAttr = dyn_cast<IntegerAttr>(constOp.getValue())) {
      if (intAttr.getInt() == 0)
        return nullptr;
    }
    builder.setInsertionPoint(allocOp);
    return builder.create<arith::ConstantOp>(loc, constOp.getType(),
                                             constOp.getValue());
  }
  if (auto constIdxOp = dyn_cast<arith::ConstantIndexOp>(defOp)) {
    if (constIdxOp.value() == 0)
      return nullptr;
    builder.setInsertionPoint(allocOp);
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

static SmallVector<Value> collectAccessIndicesFromUser(Operation *refUser) {
  SmallVector<Value> indices;
  if (auto load = dyn_cast<memref::LoadOp>(refUser)) {
    indices.assign(load.getIndices().begin(), load.getIndices().end());
    return indices;
  }
  if (auto store = dyn_cast<memref::StoreOp>(refUser)) {
    indices.assign(store.getIndices().begin(), store.getIndices().end());
    return indices;
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

    /// Check for stencil pattern - skip expansion only when we need ESD.
    /// If the stencil entries are explicit fine-grained deps (e.g., u[i-1],
    /// u[i], u[i+1]), prefer expansion to element-wise acquires.
    int64_t minOffset = 0, maxOffset = 0;
    if (DatablockUtils::hasMultiEntryStencilPattern(original, minOffset,
                                                    maxOffset)) {
      bool allFineGrainedEntries = true;
      for (size_t entryIdx = 0; entryIdx < numEntries; ++entryIdx) {
        if (original.getPartitionEntryMode(entryIdx) !=
            PartitionMode::fine_grained) {
          allFineGrainedEntries = false;
          break;
        }
      }

      if (!allFineGrainedEntries) {
        ARTS_DEBUG("  Stencil pattern detected for acquire with "
                   << numEntries << " entries: haloLeft=" << (-minOffset)
                   << ", haloRight=" << maxOffset
                   << " - skipping expansion for ESD mode");

        /// Set stencil partition mode to preserve multi-entry structure
        /// The partitioning pass will handle this with stencil/ESD mode
        original.setPartitionModeAttr(PartitionModeAttr::get(
            original.getContext(), PartitionMode::stencil));
        continue;
      }

      ARTS_DEBUG("  Stencil pattern detected for acquire with "
                 << numEntries << " entries"
                 << " - expanding for explicit fine-grained deps");
    }

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
      SmallVector<Operation *> users(dbRef.getResult().getUsers().begin(),
                                     dbRef.getResult().getUsers().end());
      SmallVector<std::pair<Operation *, size_t>> matchedUsers;
      SmallVector<Operation *> unmatchedUsers;
      llvm::DenseMap<size_t, unsigned> entryCounts;

      for (Operation *user : users) {
        SmallVector<Value> accessIndices = collectAccessIndicesFromUser(user);
        if (accessIndices.empty()) {
          unmatchedUsers.push_back(user);
          continue;
        }

        EntryMatch match =
            matchDbRefEntry(dbRef, accessIndices, original, numEntries);
        if (!match.found && numEntries > 1) {
          ARTS_DEBUG("      WARNING: Could not match db_ref user to partition "
                     "entry, defaulting to entry 0");
        }

        ARTS_DEBUG("      db_ref user matched entry "
                   << match.index
                   << " (access indices: " << accessIndices.size()
                   << ", found: " << match.found << ")");

        matchedUsers.emplace_back(user, match.index);
        entryCounts[match.index]++;
      }

      if (entryCounts.empty()) {
        SmallVector<Value> accessIndices = collectAccessIndices(dbRef);
        EntryMatch match =
            matchDbRefEntry(dbRef, accessIndices, original, numEntries);

        if (!match.found && numEntries > 1) {
          ARTS_DEBUG(
              "      WARNING: Could not match db_ref to partition entry, "
              "defaulting to entry 0");
        }

        ARTS_DEBUG("      db_ref matched entry "
                   << match.index
                   << " (access indices: " << accessIndices.size()
                   << ", found: " << match.found << ")");

        dbRef.getSourceMutable().assign(newBlockArgs[match.index]);
        continue;
      }

      if (entryCounts.size() == 1) {
        size_t entry = entryCounts.begin()->first;
        ARTS_DEBUG("      db_ref matched entry " << entry
                                                 << " (single-entry match)");
        dbRef.getSourceMutable().assign(newBlockArgs[entry]);
        continue;
      }

      /// Multiple entries matched: clone db_ref per entry and remap uses.
      OpBuilder refBuilder(dbRef);
      llvm::DenseMap<size_t, DbRefOp> entryRefs;
      for (const auto &entryPair : entryCounts) {
        size_t entry = entryPair.first;
        DbRefOp newRef =
            refBuilder.create<DbRefOp>(dbRef.getLoc(), dbRef.getType(),
                                       newBlockArgs[entry], dbRef.getIndices());
        entryRefs[entry] = newRef;
      }

      size_t fallbackEntry = entryRefs.count(0) ? 0 : entryRefs.begin()->first;
      DbRefOp fallbackRef = entryRefs[fallbackEntry];

      for (const auto &matchPair : matchedUsers) {
        Operation *user = matchPair.first;
        size_t entry = matchPair.second;
        DbRefOp refForEntry = entryRefs[entry];
        user->replaceUsesOfWith(dbRef.getResult(), refForEntry.getResult());
      }

      for (Operation *user : unmatchedUsers) {
        user->replaceUsesOfWith(dbRef.getResult(), fallbackRef.getResult());
      }

      dbRef.erase();
    }

    /// Create db_release operations for new block arguments (entries 1+)
    /// The original block arg (entry 0) already has a release from CreateDbs
    DbReleaseOp existingRelease = nullptr;
    for (Operation *user : use.blockArg.getUsers()) {
      if (auto rel = dyn_cast<DbReleaseOp>(user)) {
        existingRelease = rel;
        break;
      }
    }
    if (existingRelease) {
      OpBuilder releaseBuilder(existingRelease);
      for (size_t i = 1; i < numEntries; ++i) {
        releaseBuilder.create<DbReleaseOp>(original.getLoc(), newBlockArgs[i]);
      }
      ARTS_DEBUG("    Created " << (numEntries - 1)
                                << " db_release ops for expanded block args");
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
        "CreateDbs set none, attempting loop-based block detection",
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
                                              acqNode, allocOp, builder, loc);
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
  bool anyBlockCapable = false;
  bool anyBlockNotFullRange = false;

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

      /// Check access patterns for block capability decisions.
      bool hasIndirect = acqNode->hasIndirectAccess();

      /// Read partition capability from acquire's attribute.
      /// ForLowering sets this to 'chunked' when adding offset/size hints.
      /// CreateDbs sets this based on DbControlOp analysis.
      auto acquire = acqNode->getDbAcquireOp();
      auto acquireMode = getPartitionMode(acquire.getOperation());
      bool hasBlockHints = !acquire.getPartitionOffsets().empty() ||
                           !acquire.getPartitionSizes().empty();
      bool inferredBlock =
          !acqInfo.partitionOffsets.empty() && !acqInfo.partitionSizes.empty();
      bool thisAcquireCanBlock =
          (acquireMode && (*acquireMode == PartitionMode::block ||
                           *acquireMode == PartitionMode::stencil)) ||
          hasBlockHints || inferredBlock;
      /// If access is indirect and no explicit hints exist, still allow block
      /// partitioning so we can fall back to full-range acquires on a blocked
      /// allocation (improves parallelism vs coarse).
      if (!thisAcquireCanBlock &&
          (hasIndirect || (allocNode && allocNode->hasNonAffineAccesses &&
                           *allocNode->hasNonAffineAccesses))) {
        if (!ctx.totalElements || *ctx.totalElements > 1) {
          thisAcquireCanBlock = true;
          ARTS_DEBUG("  Non-affine/indirect access without hints; enabling "
                     "block capability for full-range acquires");
        }
      }
      if (!thisAcquireCanBlock &&
          acqNode->getAccessPattern() == AccessPattern::Stencil)
        thisAcquireCanBlock = true;
      bool thisAcquireCanElementWise =
          acquireMode && *acquireMode == PartitionMode::fine_grained;

      /// If the partition offset does not map to the access pattern, block
      /// partitioning would select the wrong dimension (non-leading) and is
      /// unsafe. Disable block capability in that case.
      SmallVector<Value> offsetVals = getPartitionOffsetsND(acqNode, &acqInfo);
      unsigned offsetIdx = 0;
      Value partitionOffset =
          pickRepresentativePartitionOffset(offsetVals, &offsetIdx);
      if (partitionOffset) {
        auto dimOpt =
            acqNode->getPartitionOffsetDim(partitionOffset,
                                           /*requireLeading=*/false);
        if (!dimOpt) {
          ARTS_DEBUG("  Partition offset incompatible with access pattern; "
                     "disabling block capability");
          thisAcquireCanBlock = false;
        }
      }

      if (thisAcquireCanBlock)
        anyBlockCapable = true;

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

    /// Collect DbAcquireNode* and partition offsets for heuristic decisions
    SmallVector<DbAcquireNode *> acqNodes;
    SmallVector<Value> partitionOffsets;
    idx = 0;
    allocNode->forEachChildNode([&](NodeBase *child) {
      if (auto *acqNode = dyn_cast<DbAcquireNode>(child)) {
        acqNodes.push_back(acqNode);
        auto offsets = getPartitionOffsetsND(
            acqNode, idx < acquireInfos.size() ? &acquireInfos[idx] : nullptr);
        partitionOffsets.push_back(offsets.empty() ? Value() : offsets.front());
        ++idx;
      }
    });

    /// Get per-acquire decisions from heuristics (calls needsFullRange)
    auto acquireDecisions =
        heuristics.getAcquireDecisions(ctx, acqNodes, partitionOffsets);

    /// Apply decisions to acquireInfos
    for (size_t i = 0; i < acquireDecisions.size() && i < acquireInfos.size();
         ++i) {
      if (acquireDecisions[i].needsFullRange) {
        acquireInfos[i].needsFullRange = true;
      }
    }

    /// Update anyBlockNotFullRange based on heuristic decisions
    for (size_t i = 0; i < acquireInfos.size(); ++i) {
      if (ctx.acquires[i].canBlock && !acquireInfos[i].needsFullRange)
        anyBlockNotFullRange = true;
    }
  }

  /// If every block-capable acquire is full-range, block partitioning does not
  /// improve locality but is still a valid layout choice. Keep block enabled
  /// and let heuristics decide; coarse is a last-resort fallback.
  bool allBlockFullRange = anyBlockCapable && !anyBlockNotFullRange;
  if (allBlockFullRange) {
    ARTS_DEBUG("  All block-capable acquires are full-range; deferring to "
               "heuristics");
  }

  /// Determine allocation-level capabilities from per-acquire STRUCTURAL
  /// analysis Always use per-acquire voting since we've updated acquire
  /// attributes based on structural hints (offset_hints, indices, etc.)
  ctx.canElementWise = ctx.anyCanElementWise();
  ctx.canBlock = ctx.anyCanBlock();
  if (!canUseBlock)
    ctx.canBlock = false;
  ctx.allBlockFullRange = allBlockFullRange;

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
        if (info.mode == PartitionMode::coarse)
          info.needsFullRange = true;
      }
    } else if (hasCoarse) {
      ctx.canElementWise = false;
      ARTS_DEBUG("  Coarse+chunked mix detected - disabling element-wise");
    }
  }

  /// Set pinnedDimCount for logging (heuristics compute min/max on-the-fly)
  ctx.pinnedDimCount = ctx.maxPinnedDimCount();
  ARTS_DEBUG("  Partition dim counts: min=" << ctx.minPinnedDimCount()
                                            << ", max=" << ctx.pinnedDimCount);

  /// Get access mode from allocNode
  if (allocNode)
    ctx.accessMode = allocOp.getMode();

  /// Get canonical block size for Block mode
  Value dynamicBlockSize;
  if (ctx.canBlock) {
    SmallVector<Value> blockSizeCandidates;
    int64_t staticCandidateCount = 0;

    func::FuncOp func = allocOp->getParentOfType<func::FuncOp>();
    DominanceInfo domInfo(func);

    for (const auto &info : acquireInfos) {
      if (info.mode != PartitionMode::block || info.partitionSizes.empty())
        continue;
      if (info.needsFullRange)
        continue;

      DbAcquireOp acquire = info.acquire;
      if (!acquire)
        continue;

      /// Use partition offsets/sizes from analysis if available; fall back to
      /// acquire hints otherwise. Prefer non-constant offsets.
      unsigned offsetIdx = 0;
      Value blockIndex =
          pickRepresentativePartitionOffset(info.partitionOffsets, &offsetIdx);
      Value blockSizeVal =
          pickRepresentativePartitionSize(info.partitionSizes, offsetIdx);
      if (!blockIndex) {
        unsigned opIdx = 0;
        if (!acquire.getPartitionIndices().empty()) {
          SmallVector<Value> partIndices(acquire.getPartitionIndices().begin(),
                                         acquire.getPartitionIndices().end());
          blockIndex = pickRepresentativePartitionOffset(partIndices, &opIdx);
        } else if (!acquire.getPartitionOffsets().empty()) {
          SmallVector<Value> partOffsets(acquire.getPartitionOffsets().begin(),
                                         acquire.getPartitionOffsets().end());
          blockIndex = pickRepresentativePartitionOffset(partOffsets, &opIdx);
        }
        if (!acquire.getPartitionSizes().empty()) {
          SmallVector<Value> partSizes(acquire.getPartitionSizes().begin(),
                                       acquire.getPartitionSizes().end());
          blockSizeVal = pickRepresentativePartitionSize(partSizes, opIdx);
        }
      }

      Value baseCandidate = DatablockUtils::extractBaseBlockSizeCandidate(
          blockIndex, blockSizeVal);
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
    Value refSize;
    for (const auto &info : acquireInfos) {
      if (info.mode != PartitionMode::block || info.partitionSizes.empty())
        continue;
      if (info.needsFullRange)
        continue;
      unsigned offsetIdx = 0;
      (void)pickRepresentativePartitionOffset(info.partitionOffsets,
                                              &offsetIdx);
      refSize = pickRepresentativePartitionSize(info.partitionSizes, offsetIdx);
      break;
    }
    if (consistentMode == PartitionMode::block && refSize &&
        !ValueUtils::isZeroConstant(ValueUtils::stripNumericCasts(refSize))) {
      dynamicBlockSize = refSize;
      if (auto staticSize = arts::extractBlockSizeFromHint(refSize)) {
        if (*staticSize > 0)
          ctx.blockSize = *staticSize;
      }
    } else {
      for (const auto &info : acquireInfos) {
        if (info.mode != PartitionMode::block || info.partitionSizes.empty())
          continue;
        if (info.needsFullRange)
          continue;
        unsigned offsetIdx = 0;
        (void)pickRepresentativePartitionOffset(info.partitionOffsets,
                                                &offsetIdx);
        Value infoSize =
            pickRepresentativePartitionSize(info.partitionSizes, offsetIdx);
        if (ValueUtils::isZeroConstant(
                ValueUtils::stripNumericCasts(infoSize))) {
          ARTS_DEBUG("  Skipping zero fallback block size");
          continue;
        }
        dynamicBlockSize = infoSize;
        if (auto staticSize = arts::extractBlockSizeFromHint(infoSize)) {
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
        decision = PartitioningDecision::blockND(
            ctx, nDims, "Fallback: N-D block-wise pattern");
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

  /// Also try N-D extraction for explicit block/stencil decisions that haven't
  /// extracted block sizes yet. This handles cases like Cholesky where the
  /// heuristics already chose block mode but we need to extract the N-D
  /// block sizes from loop steps for proper multi-dimensional partitioning.
  /// Check for both indexed and uniform patterns (block-wise access).
  bool hasExplicitBlockSizes = false;
  for (const auto &info : acquireInfos) {
    if (info.mode != PartitionMode::block)
      continue;
    for (Value sz : info.partitionSizes) {
      if (!sz)
        continue;
      Value stripped = ValueUtils::stripNumericCasts(sz);
      if (!ValueUtils::isOneConstant(stripped)) {
        hasExplicitBlockSizes = true;
        break;
      }
    }
    if (hasExplicitBlockSizes)
      break;
  }

  if (decision.isBlock() &&
      (ctx.accessPatterns.hasIndexed || ctx.accessPatterns.hasUniform) &&
      blockSizesForNDBlock.empty() && !hasExplicitBlockSizes) {
    ARTS_DEBUG(
        "  Block/stencil mode with indexed/uniform patterns - extracting "
        "N-D block sizes");
    for (auto &info : acquireInfos) {
      auto partitionIndices = info.acquire.getPartitionIndices();
      if (partitionIndices.empty())
        continue;

      /// For stencil acquires with multi-entry structure, use first entry's
      /// index to find the loop step (the base index, not the offset versions)
      SmallVector<Value> indicesToCheck;
      if (info.acquire.hasMultiplePartitionEntries()) {
        /// Find the center entry (offset 0) for stencil patterns
        size_t numEntries = info.acquire.getNumPartitionEntries();
        for (size_t i = 0; i < numEntries; ++i) {
          auto entryIndices = info.acquire.getPartitionIndicesForEntry(i);
          if (!entryIndices.empty())
            indicesToCheck.push_back(entryIndices[0]);
        }
        /// Use first unique index for loop detection
        if (!indicesToCheck.empty())
          indicesToCheck.resize(1);
      } else {
        indicesToCheck.assign(partitionIndices.begin(), partitionIndices.end());
      }

      /// For each partition index, try to find the enclosing loop step
      for (Value idx : indicesToCheck) {
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
      /// For stencil mode, keep the mode but update ranks based on block sizes
      /// For non-stencil block mode, upgrade to N-D block mode
      unsigned nDims = blockSizesForNDBlock.size();
      if (decision.mode != PartitionMode::stencil) {
        decision = PartitioningDecision::blockND(ctx, nDims,
                                                 "N-D block pattern detected");
        ARTS_DEBUG("  Upgraded to N-D block mode with " << nDims
                                                        << " dimensions");
        heuristics.recordDecision(
            "Partition-BlockUpgradedToND", true,
            "block mode upgraded with N-D block sizes from loop steps",
            allocOp.getOperation(), {{"numPartitionedDims", (int64_t)nDims}});
      } else {
        /// Stencil mode: keep mode, just log block size extraction
        ARTS_DEBUG("  Stencil mode: extracted " << nDims << " block sizes");
      }
    }
  }

  bool allRewriteable =
      skipAcquireInfoCheck ||
      std::all_of(acquireInfos.begin(), acquireInfos.end(), [&](auto &info) {
        /// Full-range acquires don't need offset/size - they get full
        /// allocation
        if (info.needsFullRange)
          return true;
        return info.isValid && !info.partitionOffsets.empty() &&
               !info.partitionSizes.empty();
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

    /// Collect stencil bounds from acquire nodes or multi-entry structure.
    /// For multi-entry stencil acquires (not expanded), extract halo bounds
    /// from the partition indices pattern using DatablockUtils.
    allocNode->forEachChildNode([&](NodeBase *child) {
      auto *acqNode = dyn_cast<DbAcquireNode>(child);
      if (!acqNode)
        return;
      DbAcquireOp acq = acqNode->getDbAcquireOp();
      if (!acq)
        return;

      /// First try: get bounds from DbAcquireNode analysis
      auto stencilBounds = acqNode->getStencilBounds();
      if (stencilBounds && stencilBounds->hasHalo()) {
        info.haloLeft = std::max(info.haloLeft, stencilBounds->haloLeft());
        info.haloRight = std::max(info.haloRight, stencilBounds->haloRight());
        return;
      }

      /// Second try: for multi-entry stencil acquires, extract from structure
      if (acq.hasMultiplePartitionEntries()) {
        int64_t minOffset = 0, maxOffset = 0;
        if (DatablockUtils::hasMultiEntryStencilPattern(acq, minOffset,
                                                        maxOffset)) {
          /// minOffset is negative (left halo), maxOffset is positive (right)
          int64_t haloL = -minOffset;
          int64_t haloR = maxOffset;
          info.haloLeft = std::max(info.haloLeft, haloL);
          info.haloRight = std::max(info.haloRight, haloR);
          ARTS_DEBUG("  Extracted stencil bounds from multi-entry: haloLeft="
                     << haloL << ", haloRight=" << haloR);
        }
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

    /// Check: Require both halos for true ESD stencil mode.
    /// Single-sided patterns (only left or only right halo) break the
    /// 3-buffer approach which assumes data from both neighbors exists.
    if (info.haloLeft == 0 || info.haloRight == 0) {
      ARTS_DEBUG("  Single-sided halo detected (haloLeft="
                 << info.haloLeft << ", haloRight=" << info.haloRight
                 << ") - falling back to BLOCK mode");
      decision = PartitioningDecision::blockND(
          ctx, decision.outerRank,
          "Single-sided stencil pattern - using BLOCK instead");
      stencilInfo = std::nullopt; // Clear stencil info
    }
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
        /// Fallback: derive a dominating block size from alloc size + workers.
        ARTS_DEBUG("  No block size available for block mode - attempting "
                   "alloc+workers fallback");
        bool useNodesForFallback = false;
        if (AM) {
          ArtsAbstractMachine &machine = AM->getAbstractMachine();
          if (machine.hasValidNodeCount() && machine.getNodeCount() > 1)
            useNodesForFallback = true;
        }
        blockSizeForPlan =
            computeDefaultBlockSize(allocOp, builder, loc, useNodesForFallback);
        if (blockSizeForPlan) {
          heuristics.recordDecision(
              "Partition-BlockSizeFallbackHeuristic", true,
              "derived block size from alloc size and total workers",
              allocOp.getOperation(), {});
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

  /// Determine partitioned dimensions for block mode (supports non-leading).
  SmallVector<unsigned> partitionedDimsForPlan;
  if (decision.isBlock()) {
    unsigned nPartDims = blockSizesForPlan.size();
    if (nPartDims > 0) {
      auto dimsEqual = [](ArrayRef<unsigned> a,
                          ArrayRef<unsigned> b) -> bool {
        if (a.size() != b.size())
          return false;
        for (unsigned i = 0; i < a.size(); ++i) {
          if (a[i] != b[i])
            return false;
        }
        return true;
      };

      int bestScore = std::numeric_limits<int>::max();
      bool bestFromFine = false;
      for (const auto &candidate : acquireInfos) {
        if (candidate.needsFullRange)
          continue;
        if (candidate.partitionDims.size() != nPartDims)
          continue;

        int score = 0;
        for (const auto &info : acquireInfos) {
          if (info.needsFullRange)
            continue;
          if (info.partitionDims.empty() ||
              info.partitionDims.size() != nPartDims)
            continue;
          if (!dimsEqual(info.partitionDims, candidate.partitionDims)) {
            score += (info.mode == PartitionMode::fine_grained) ? 2 : 1;
          }
        }

        bool fromFine = candidate.mode == PartitionMode::fine_grained;
        if (score < bestScore ||
            (score == bestScore && fromFine && !bestFromFine)) {
          bestScore = score;
          bestFromFine = fromFine;
          partitionedDimsForPlan.assign(candidate.partitionDims.begin(),
                                        candidate.partitionDims.end());
        }
      }
    }

    if (!partitionedDimsForPlan.empty()) {
      SmallVector<bool> seen(allocOp.getElementSizes().size(), false);
      bool valid = true;
      for (unsigned d : partitionedDimsForPlan) {
        if (d >= seen.size() || seen[d]) {
          valid = false;
          break;
        }
        seen[d] = true;
      }
      if (!valid)
        partitionedDimsForPlan.clear();
    }

    /// Non-leading partitioning is not supported by stencil ESD. Downgrade
    /// to block mode when needed.
    if (decision.mode == PartitionMode::stencil &&
        !partitionedDimsForPlan.empty()) {
      bool nonLeading = false;
      for (unsigned i = 0; i < partitionedDimsForPlan.size(); ++i) {
        if (partitionedDimsForPlan[i] != i) {
          nonLeading = true;
          break;
        }
      }
      if (nonLeading) {
        decision = PartitioningDecision::blockND(
            ctx, partitionedDimsForPlan.size(),
            "Non-leading partition dims: disable stencil ESD");
        stencilInfo = std::nullopt;
      }
    }

    /// If an acquire's inferred partition dims disagree with the chosen plan,
    /// require full-range to preserve correctness (non-leading mismatch).
    if (!partitionedDimsForPlan.empty()) {
      for (auto &info : acquireInfos) {
        if (info.needsFullRange || info.partitionDims.empty())
          continue;
        bool same =
            info.partitionDims.size() == partitionedDimsForPlan.size();
        if (same) {
          for (unsigned i = 0; i < info.partitionDims.size(); ++i) {
            if (info.partitionDims[i] != partitionedDimsForPlan[i]) {
              same = false;
              break;
            }
          }
        }
        if (!same) {
          info.needsFullRange = true;
          ARTS_DEBUG("  Acquire partition dims mismatch with plan; "
                     "forcing full-range access");
        }
      }
    }
  }

  /// For stencil ESD, inner size stays at BASE block size; halos are acquired
  /// via separate DBs (left/right) using element_offsets into neighboring
  /// chunks.
  Value firstBlockSize =
      blockSizesForPlan.empty() ? Value() : blockSizesForPlan.front();
  Value innerBlockSize = firstBlockSize;
  bool extendInnerForStencil = false;
  if (extendInnerForStencil) {
    int64_t haloTotal = stencilInfo->haloLeft + stencilInfo->haloRight;
    Value haloConst = builder.create<arith::ConstantIndexOp>(loc, haloTotal);
    innerBlockSize =
        builder.create<arith::AddIOp>(loc, firstBlockSize, haloConst);
    ARTS_DEBUG("  Extended inner size for stencil: base + " << haloTotal);
  }

  SmallVector<Value> newOuterSizes, newInnerSizes;
  computeSizesFromDecision(allocOp, decision, blockSizesForPlan,
                           partitionedDimsForPlan, newOuterSizes,
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
  DbRewritePlan plan(decision.mode);
  plan.blockSizes = blockSizesForPlan;
  plan.partitionedDims = partitionedDimsForPlan;
  plan.outerSizes.assign(newOuterSizes.begin(), newOuterSizes.end());
  plan.innerSizes.assign(newInnerSizes.begin(), newInnerSizes.end());

  ARTS_DEBUG("  Plan created: blockSizes="
             << plan.blockSizes.size() << ", outerRank=" << plan.outerRank()
             << ", innerRank=" << plan.innerRank()
             << ", numPartitionedDims=" << plan.numPartitionedDims());

  /// Mixed mode: set flag for full-range acquires
  bool hasMixedMode = llvm::any_of(
      acquireInfos, [](const auto &info) { return info.needsFullRange; });
  if (hasMixedMode && decision.isBlock()) {
    plan.isMixedMode = true;
    ARTS_DEBUG("  Mixed mode plan enabled");
  }

  /// ESD: Use early-computed stencilInfo for Stencil mode
  if (stencilInfo)
    plan.stencilInfo = *stencilInfo;
  /// Note: byte_offset/byte_size for ESD is computed in EdtLowering from
  /// element_offsets/element_sizes, using allocation's elementSizes for stride.

  /// Propagate stencil center offset to all acquires when using stencil mode.
  /// This keeps base offsets aligned for non-stencil accesses (e.g., outputs)
  /// when loop bounds are shifted (i starts at 1).
  if (decision.mode == PartitionMode::stencil) {
    std::optional<int64_t> centerOffset;
    for (const auto &info : acquireInfos) {
      if (!info.acquire)
        continue;
      if (auto centerAttr = info.acquire->getAttrOfType<IntegerAttr>(
              "stencil_center_offset")) {
        centerOffset = centerAttr.getInt();
        break;
      }
    }
    if (centerOffset) {
      IntegerAttr centerAttr = builder.getI64IntegerAttr(*centerOffset);
      for (const auto &info : acquireInfos) {
        if (!info.acquire)
          continue;
        if (!info.acquire->hasAttr("stencil_center_offset"))
          info.acquire->setAttr("stencil_center_offset", centerAttr);
      }
    }
  }

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

    if (info.acquire.hasMultiplePartitionEntries()) {
      ARTS_DEBUG("  Rewrite multi-entry acquire offset="
                 << (info.partitionOffsets.empty()
                         ? Value()
                         : info.partitionOffsets.front())
                 << " loc=" << info.acquire.getLoc());
    }

    Value one = builder.create<arith::ConstantIndexOp>(allocOp.getLoc(), 1);

    /// Populate PartitionInfo to preserve semantic context through the
    /// pipeline. This is the canonical data structure - indexers use this to
    /// distinguish between element coordinates (indices) and range starts
    /// (offsets).
    rewriteInfo.partitionInfo.mode = decision.mode;

    if (info.needsFullRange && decision.isBlock()) {
      /// Coarse acquire in mixed mode: full-range access.
      rewriteInfo.isFullRange = true;
    } else if (info.isValid) {
      if (decision.isFineGrained()) {
        /// Fine-grained: use partition indices as element COORDINATES.
        /// These identify WHICH element this EDT owns, use directly as db_ref
        /// indices.
        auto partitionIndices = info.acquire.getPartitionIndices();
        rewriteInfo.partitionInfo.indices.assign(partitionIndices.begin(),
                                                 partitionIndices.end());
        bool hasRange =
            !info.partitionSizes.empty() &&
            !ValueUtils::isOneConstant(
                ValueUtils::stripNumericCasts(info.partitionSizes.front()));
        /// Range acquires (e.g., chunked parallel-for) should preserve
        /// DB-space offsets/sizes so a single acquire can cover multiple
        /// fine-grained elements.
        if (rewriteInfo.partitionInfo.indices.empty() && hasRange) {
          /// N-D support: use all partition offsets/sizes
          for (Value off : info.partitionOffsets)
            rewriteInfo.partitionInfo.offsets.push_back(off);
          for (Value sz : info.partitionSizes)
            rewriteInfo.partitionInfo.sizes.push_back(sz);
        }
        /// Fallback if no partition indices
        if (rewriteInfo.partitionInfo.indices.empty() &&
            rewriteInfo.partitionInfo.offsets.empty() &&
            !info.partitionOffsets.empty()) {
          for (Value off : info.partitionOffsets)
            rewriteInfo.partitionInfo.indices.push_back(off);
        }
      } else if (decision.mode == PartitionMode::block &&
                 plan.numPartitionedDims() > 1) {
        /// N-D Block mode: extract partition indices as range STARTS.
        /// Each partition index represents the START of the block in that
        /// dimension. Clear any existing data from the mode-setting block above
        /// to avoid duplication.
        rewriteInfo.partitionInfo.offsets.clear();
        rewriteInfo.partitionInfo.sizes.clear();
        auto partitionIndices = info.acquire.getPartitionIndices();
        for (Value idx : partitionIndices) {
          rewriteInfo.partitionInfo.offsets.push_back(idx);
          /// For block mode, size is the block size (not 1 like fine-grained)
          unsigned dimIdx = rewriteInfo.partitionInfo.offsets.size() - 1;
          Value blockSz = plan.getBlockSize(dimIdx);
          rewriteInfo.partitionInfo.sizes.push_back(blockSz ? blockSz : one);
        }
        /// Fallback if no partition indices - use N-D partition offsets/sizes
        if (rewriteInfo.partitionInfo.offsets.empty()) {
          for (Value off : info.partitionOffsets)
            rewriteInfo.partitionInfo.offsets.push_back(off);
          for (Value sz : info.partitionSizes)
            rewriteInfo.partitionInfo.sizes.push_back(sz);
        }
      } else {
        /// 1D Block/Stencil or other modes: use N-D partition offsets/sizes
        for (Value off : info.partitionOffsets)
          rewriteInfo.partitionInfo.offsets.push_back(off);
        for (Value sz : info.partitionSizes)
          rewriteInfo.partitionInfo.sizes.push_back(sz);
      }
    } else {
      /// Invalid acquire: use Coarse fallback (full size access).
      /// The acquire still needs rewriting to use the new allocation structure.
      Value totalSize = allocOp.getElementSizes().empty()
                            ? zero
                            : allocOp.getElementSizes().front();
      if (decision.isFineGrained()) {
        /// Fine-grained mode expects indices (element coordinates)
        rewriteInfo.partitionInfo.indices.push_back(zero);
      } else {
        /// Block/stencil modes expect offsets
        rewriteInfo.partitionInfo.offsets.push_back(zero);
      }
      rewriteInfo.partitionInfo.sizes.push_back(totalSize);
      ARTS_DEBUG("  Invalid acquire " << info.acquire
                                      << " rewritten with Coarse fallback");
    }

    /// Handle indirect access fallback for fine-grained mode.
    /// For fine-grained with indirect access, use coarse fallback (full range).
    /// We still use indices (not offsets) because DbElementWiseRewriter expects
    /// indices.
    if (decision.isFineGrained() && info.hasIndirectAccess) {
      Value totalSize = allocOp.getElementSizes().empty()
                            ? zero
                            : allocOp.getElementSizes().front();
      rewriteInfo.partitionInfo.indices.clear();
      rewriteInfo.partitionInfo.offsets.clear();
      rewriteInfo.partitionInfo.sizes.clear();
      /// Use indices (element coordinates) with zero for coarse fallback in
      /// fine-grained mode.
      rewriteInfo.partitionInfo.indices.push_back(zero);
      rewriteInfo.partitionInfo.sizes.push_back(totalSize);
    }

    if (decision.isBlock() && !plan.partitionedDims.empty()) {
      rewriteInfo.partitionInfo.partitionedDims.assign(
          plan.partitionedDims.begin(), plan.partitionedDims.end());
    }

    rewriteAcquires.push_back(rewriteInfo);
  }

  auto rewriter = DbRewriter::create(allocOp, rewriteAcquires, plan);
  auto result = rewriter->apply(builder);

  /// Step 7: Set partition attribute on new alloc
  if (succeeded(result)) {
    setPartitionMode(*result, decision.mode);

    AM->getMetadataManager().transferMetadata(allocOp, result.value());

    /// Coarse allocation: clear partition hints on acquires to avoid
    /// unnecessary per-task hint plumbing and enable invariant hoisting.
    if (decision.isCoarse()) {
      for (auto &info : rewriteAcquires) {
        if (!info.acquire)
          continue;
        info.acquire.clearPartitionHints();
      }
    }

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
  /// Skip multi-entry acquires (stencil patterns) - they have a different
  /// structure and bounds are handled via ESD
  if (acquireOp.hasMultiplePartitionEntries()) {
    ARTS_DEBUG("  Skipping bounds generation for multi-entry stencil acquire");
    return;
  }

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
