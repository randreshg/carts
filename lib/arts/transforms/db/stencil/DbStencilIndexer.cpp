///==========================================================================///
/// File: DbStencilIndexer.cpp
///
/// Stencil-aware index localization for the ESD (Ephemeral Slice Dependencies)
/// partitioning strategy.
///
/// ESD creates 3 separate acquires per stencil array:
///   1. Owned chunk - the worker's assigned rows
///   2. Left halo  - partial slice from previous chunk (via element_offsets)
///   3. Right halo - partial slice from next chunk (via element_offsets)
///
/// Before:
///   %v0 = memref.load %ref[%i - 1, %j]
///   %v1 = memref.load %ref[%i, %j]
///   %v2 = memref.load %ref[%i + 1, %j]
///
/// After:
///   %v0 = memref.load %left_halo[%local_i - 1, %j]
///   %v1 = memref.load %owned[%local_i, %j]
///   %v2 = memref.load %right_halo[%local_i + 1 - %tile, %j]
///==========================================================================///

#include "arts/transforms/db/stencil/DbStencilIndexer.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/codegen/Codegen.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/Debug.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "db_stencil"

ARTS_DEBUG_SETUP(db_stencil_indexer);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Constructor
///===----------------------------------------------------------------------===///

DbStencilIndexer::DbStencilIndexer(const PartitionInfo &info, Value haloLeft,
                                   Value haloRight, unsigned outerRank,
                                   unsigned innerRank, Value leftAvail,
                                   Value rightAvail, Value ownedArg,
                                   Value leftHaloArg, Value rightHaloArg)
    : DbIndexerBase(info, outerRank, innerRank),
      elemOffset(info.offsets.empty() ? Value() : info.offsets.front()),
      haloLeft(haloLeft), haloRight(haloRight),
      blockSize(info.sizes.empty() ? Value() : info.sizes.front()),
      leftAvail(leftAvail), rightAvail(rightAvail),
      ownedArg(ownedArg), leftHaloArg(leftHaloArg), rightHaloArg(rightHaloArg) {
}

///===----------------------------------------------------------------------===///
/// Helper: Clamp index to valid bounds
///===----------------------------------------------------------------------===///

/// Clamp index to [0, size-1] to prevent out-of-bounds access.
static Value clampIndex(Value index, Value size, OpBuilder &builder,
                        Location loc) {
  if (!size)
    return index;

  Value zero = arts::createZeroIndex(builder, loc);
  Value one = arts::createOneIndex(builder, loc);
  Value lastValidIdx = arith::SubIOp::create(builder, loc, size, one);

  /// clamp(index, 0, lastValidIdx)
  Value clamped = arith::MaxSIOp::create(builder, loc, index, zero);
  return arith::MinSIOp::create(builder, loc, clamped, lastValidIdx);
}

///===----------------------------------------------------------------------===///
/// Helper: Detect and get stride for linearized access
///===----------------------------------------------------------------------===///

/// Returns stride if the access pattern is linearized (single index into
/// multi-dimensional memref), otherwise returns nullptr.
static Value getLinearizedStride(ValueRange indices, Type elementType,
                                 OpBuilder &builder, Location loc) {
  if (indices.size() != 1)
    return nullptr;

  auto memrefType = dyn_cast<MemRefType>(elementType);
  if (!memrefType || memrefType.getRank() < 2)
    return nullptr;

  auto staticStride = DbUtils::getStaticStride(memrefType);
  if (!staticStride || *staticStride <= 1)
    return nullptr;

  return arts::createConstantIndex(builder, loc, *staticStride);
}

/// Try to find a stable anchor (typically a loop IV) for dependency checks.
static Value findAnchor(Value v, int depth = 0) {
  if (!v || depth > 6)
    return v;
  v = ValueAnalysis::stripNumericCasts(v);
  if (auto blockArg = dyn_cast<BlockArgument>(v))
    return blockArg;
  if (Operation *op = v.getDefiningOp()) {
    for (Value operand : op->getOperands()) {
      Value anchor = findAnchor(operand, depth + 1);
      if (anchor && isa<BlockArgument>(anchor))
        return anchor;
    }
  }
  return v;
}

/// Pick the index dimension that tracks the partition anchor (defaults to 0).
static unsigned pickPartitionDim(ValueRange indices, Value anchor) {
  if (!anchor || indices.empty())
    return 0;
  for (unsigned i = 0; i < indices.size(); ++i) {
    if (ValueAnalysis::dependsOn(indices[i], anchor))
      return i;
  }
  return 0;
}

///===----------------------------------------------------------------------===///
/// Shared context for stencil rewriting helpers
///===----------------------------------------------------------------------===///

/// Groups shared state used by the extracted helper functions during
/// transformDbRefUsers. Avoids passing 15+ parameters to each helper.
struct StencilRewriteContext {
  mutable DbRefOp ref;
  const PartitionInfo &partitionInfo;
  Value elemOffset;
  Value haloLeft;
  Value haloRight;
  Value blockSize;
  Value zero;
  Type newElementType;
  Value ownedMemref;
  Value leftMemref;
  Value rightMemref;
  Value leftAvail;
  Value rightAvail;
  Value leftPtrNotNull;
  Value rightPtrNotNull;
  llvm::SetVector<Operation *> &opsToRemove;
};

///===----------------------------------------------------------------------===///
/// Helper: Compare values ignoring numeric casts
///===----------------------------------------------------------------------===///

/// Check whether two SSA values refer to the same definition, stripping
/// arith::IndexCastOps and other numeric casts.
static bool isSameValueWithCasts(Value a, Value b) {
  if (!a || !b)
    return false;
  if (a == b)
    return true;
  a = ValueAnalysis::stripNumericCasts(a);
  b = ValueAnalysis::stripNumericCasts(b);
  if (a == b)
    return true;
  if (auto cast = a.getDefiningOp<arith::IndexCastOp>())
    if (cast.getIn() == b)
      return true;
  if (auto cast = b.getDefiningOp<arith::IndexCastOp>())
    if (cast.getIn() == a)
      return true;
  return false;
}

///===----------------------------------------------------------------------===///
/// Helper: Derive constant row offset from expression
///===----------------------------------------------------------------------===///

/// Analyze a row expression to extract a constant offset relative to the row
/// induction variable. Returns true if a constant offset was found, with the
/// offset stored in rowOffset and whether the base offset is already included
/// stored in includesBaseOffset.
static bool deriveRowOffset(Value rowExpr, Value rowIV, Value baseOffsetVal,
                            int64_t &rowOffset, bool &includesBaseOffset) {
  int64_t constOff = 0;
  Value baseExpr = ValueAnalysis::stripConstantOffset(rowExpr, &constOff);
  baseExpr = ValueAnalysis::stripNumericCasts(baseExpr);

  if (isSameValueWithCasts(baseExpr, rowIV)) {
    rowOffset = constOff;
    includesBaseOffset = false;
    return true;
  }

  if (auto addOp = baseExpr.getDefiningOp<arith::AddIOp>()) {
    Value lhs = ValueAnalysis::stripNumericCasts(addOp.getLhs());
    Value rhs = ValueAnalysis::stripNumericCasts(addOp.getRhs());
    bool lhsIsRow = isSameValueWithCasts(lhs, rowIV);
    bool rhsIsRow = isSameValueWithCasts(rhs, rowIV);
    if (lhsIsRow || rhsIsRow) {
      Value other = lhsIsRow ? rhs : lhs;
      if (baseOffsetVal && (other == baseOffsetVal ||
                            ValueAnalysis::dependsOn(other, baseOffsetVal))) {
        rowOffset = constOff;
        includesBaseOffset = true;
        return true;
      }
      if (!ValueAnalysis::dependsOn(other, rowIV)) {
        rowOffset = constOff;
        includesBaseOffset =
            baseOffsetVal && ValueAnalysis::dependsOn(other, baseOffsetVal);
        return true;
      }
    }
  }

  if (ValueAnalysis::dependsOn(baseExpr, rowIV)) {
    rowOffset = constOff;
    includesBaseOffset =
        baseOffsetVal && ValueAnalysis::dependsOn(baseExpr, baseOffsetVal);
    return true;
  }

  return false;
}

/// Materialize an owner-local row coordinate from an access expression.
/// Some partitioned tasks already iterate in owner-local coordinates while
/// others still carry the owner base in the row expression. Only subtract the
/// owner base when the expression is known to include it.
static Value materializeLocalRow(Value rowExpr, Value baseOffset,
                                 bool includesBaseOffset, OpBuilder &builder,
                                 Location loc) {
  if (!rowExpr)
    return Value();
  if (!baseOffset || !includesBaseOffset ||
      ValueAnalysis::isZeroConstant(
          ValueAnalysis::stripNumericCasts(baseOffset)))
    return rowExpr;
  return arith::SubIOp::create(builder, loc, rowExpr, baseOffset);
}

/// Best-effort fallback when we cannot recover the exact row-loop contract.
/// Preserve already-local row expressions and only subtract the owner base
/// when the expression visibly depends on it.
static Value maybeMaterializeLocalRow(Value rowExpr, Value baseOffset,
                                      OpBuilder &builder, Location loc) {
  if (!rowExpr || !baseOffset ||
      ValueAnalysis::isZeroConstant(
          ValueAnalysis::stripNumericCasts(baseOffset)))
    return rowExpr;

  Value strippedRow = ValueAnalysis::stripNumericCasts(rowExpr);
  Value strippedBase = ValueAnalysis::stripNumericCasts(baseOffset);
  if (ValueAnalysis::areValuesEquivalent(strippedRow, strippedBase) ||
      ValueAnalysis::dependsOn(strippedRow, strippedBase))
    return arith::SubIOp::create(builder, loc, rowExpr, baseOffset);

  return rowExpr;
}

///===----------------------------------------------------------------------===///
/// Row selection types
///===----------------------------------------------------------------------===///

enum class StencilRegionKind { Left, Middle, Right };

struct RowSelection {
  Value memref;
  Value rowIdx;
  Value isLeft;
  Value isRight;
  Value leftAvail;
  Value rightAvail;
  Value leftFallbackToOwned;
  Value rightFallbackToOwned;
  Value ownedIdx;
  Value leftIdx;
  Value rightIdx;
};

///===----------------------------------------------------------------------===///
/// Helper: Build 3-buffer row selection logic
///===----------------------------------------------------------------------===///

/// Build the selection IR that picks the correct buffer (owned, left halo, or
/// right halo) and computes the buffer-local row index based on localRow.
/// Returns a RowSelection with the selected memref and row index.
static RowSelection buildSelection(OpBuilder &selBuilder, Location selLoc,
                                   Value localRowVal, Value ownedRowsVal,
                                   const StencilRewriteContext &ctx) {
  Value ownedRows = ownedRowsVal ? ownedRowsVal : ctx.blockSize;
  Value ownedMemrefRows = ctx.blockSize ? ctx.blockSize : ownedRows;
  Value isLeft = arith::CmpIOp::create(
      selBuilder, selLoc, arith::CmpIPredicate::slt, localRowVal, ctx.zero);
  Value isRight = arith::CmpIOp::create(
      selBuilder, selLoc, arith::CmpIPredicate::sge, localRowVal, ownedRows);
  Value falseI1 = arith::ConstantIntOp::create(selBuilder, selLoc, 0, 1);
  Value trueI1 = arith::ConstantIntOp::create(selBuilder, selLoc, 1, 1);
  Value leftAvail = ctx.leftAvail ? ctx.leftAvail
                                  : (ctx.leftPtrNotNull ? ctx.leftPtrNotNull
                                                        : falseI1);
  Value rightAvail = ctx.rightAvail ? ctx.rightAvail
                                    : (ctx.rightPtrNotNull ? ctx.rightPtrNotNull
                                                           : falseI1);
  Value leftIdx =
      arith::AddIOp::create(selBuilder, selLoc, localRowVal, ctx.haloLeft);
  Value ownedIdx = clampIndex(localRowVal, ownedMemrefRows, selBuilder, selLoc);
  Value rightIdx =
      arith::SubIOp::create(selBuilder, selLoc, localRowVal, ownedRows);

  Value clampedOwnedIdx = clampIndex(ownedIdx, ownedRows, selBuilder, selLoc);
  Value clampedLeftIdx =
      ctx.leftMemref ? clampIndex(leftIdx, ctx.haloLeft, selBuilder, selLoc)
                     : Value();
  Value clampedRightIdx =
      ctx.rightMemref ? clampIndex(rightIdx, ctx.haloRight, selBuilder, selLoc)
                      : Value();
  Value diagLeftIdx = clampedLeftIdx ? clampedLeftIdx : clampedOwnedIdx;
  Value diagRightIdx = clampedRightIdx ? clampedRightIdx : clampedOwnedIdx;

  Value selectedMemref = ctx.ownedMemref;
  Value selectedRowIdx = clampedOwnedIdx;

  auto selectValue = [&](Value cond, Value trueVal, Value falseVal) -> Value {
    bool forceIf = isa<MemRefType>(trueVal.getType());
    if (!forceIf && cond && cond.getType().isInteger(1))
      return arith::SelectOp::create(selBuilder, selLoc, cond, trueVal,
                                     falseVal);
    auto ifOp = scf::IfOp::create(selBuilder, selLoc, trueVal.getType(), cond,
                                  /*withElseRegion=*/true);
    {
      OpBuilder::InsertionGuard g(selBuilder);
      selBuilder.setInsertionPointToStart(&ifOp.getThenRegion().front());
      scf::YieldOp::create(selBuilder, selLoc, trueVal);
      selBuilder.setInsertionPointToStart(&ifOp.getElseRegion().front());
      scf::YieldOp::create(selBuilder, selLoc, falseVal);
    }
    return ifOp.getResult(0);
  };

  if (ctx.leftMemref) {
    Value safeLeftMemref =
        selectValue(leftAvail, ctx.leftMemref, ctx.ownedMemref);
    Value safeLeftIdx =
        selectValue(leftAvail, clampedLeftIdx, clampedOwnedIdx);
    selectedMemref = selectValue(isLeft, safeLeftMemref, selectedMemref);
    selectedRowIdx = selectValue(isLeft, safeLeftIdx, selectedRowIdx);
  }

  if (ctx.rightMemref) {
    Value safeRightMemref =
        selectValue(rightAvail, ctx.rightMemref, ctx.ownedMemref);
    Value safeRightIdx =
        selectValue(rightAvail, clampedRightIdx, clampedOwnedIdx);

    selectedMemref = selectValue(isRight, safeRightMemref, selectedMemref);
    selectedRowIdx = selectValue(isRight, safeRightIdx, selectedRowIdx);
  }

  Value leftFallbackToOwned = arith::AndIOp::create(
      selBuilder, selLoc, isLeft,
      arith::XOrIOp::create(selBuilder, selLoc, leftAvail, trueI1));
  Value rightFallbackToOwned = arith::AndIOp::create(
      selBuilder, selLoc, isRight,
      arith::XOrIOp::create(selBuilder, selLoc, rightAvail, trueI1));

  return {selectedMemref,
          selectedRowIdx,
          isLeft,
          isRight,
          leftAvail,
          rightAvail,
          leftFallbackToOwned,
          rightFallbackToOwned,
          clampedOwnedIdx,
          diagLeftIdx,
          diagRightIdx};
}

///===----------------------------------------------------------------------===///
/// Helper: Resolve partition dimension and global row for a user op
///===----------------------------------------------------------------------===///

/// Determine the partition dimension and global row index for a load/store
/// operation on the stencil buffer. Handles fallback to db_ref indices when
/// the load/store indices don't carry the partition dimension.
struct ResolvedRowInfo {
  unsigned partitionDim;
  Value globalRow;
};

static ResolvedRowInfo resolvePartitionRow(ValueRange indices,
                                           const StencilRewriteContext &ctx,
                                           OpBuilder &builder, Location loc) {
  Value anchor = findAnchor(ctx.elemOffset);
  unsigned partitionDim = 0;
  if (!ctx.partitionInfo.partitionedDims.empty() &&
      ctx.partitionInfo.partitionedDims.front() < indices.size()) {
    partitionDim = ctx.partitionInfo.partitionedDims.front();
  } else {
    partitionDim = pickPartitionDim(indices, anchor);
  }
  Value globalRow = indices[partitionDim];

  /// Handle linearized access: extract row from linear index
  Value stride = getLinearizedStride(indices, ctx.newElementType, builder, loc);
  if (stride) {
    globalRow = arith::DivUIOp::create(builder, loc, indices[0], stride);
  }

  return {partitionDim, globalRow};
}

///===----------------------------------------------------------------------===///
/// Helper: Rewrite a single user op for a specific versioned region
///===----------------------------------------------------------------------===///

/// Rewrite a load/store operation to use the correct halo or owned buffer
/// based on which region (Left/Middle/Right) the enclosing versioned row loop
/// covers. Used by tryVersionRowLoop after the row loop has been split into
/// three copies.
static void rewriteUserForRegion(Operation *user, StencilRegionKind region,
                                 scf::ForOp rowLoop, OpBuilder &builder,
                                 const StencilRewriteContext &ctx) {
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(user);
  Location userLoc = user->getLoc();

  ValueRange indices;
  bool isLoad = false;
  Value storeValue;
  if (auto load = dyn_cast<memref::LoadOp>(user)) {
    indices = load.getIndices();
    isLoad = true;
  } else if (auto store = dyn_cast<memref::StoreOp>(user)) {
    indices = store.getIndices();
    storeValue = store.getValue();
  } else {
    return;
  }

  if (indices.empty())
    return;

  auto rowInfo = resolvePartitionRow(indices, ctx, builder, userLoc);
  unsigned partitionDim = rowInfo.partitionDim;
  Value globalRow = rowInfo.globalRow;

  Value baseOffset = ctx.partitionInfo.offsets.empty()
                         ? ctx.zero
                         : ctx.partitionInfo.offsets.front();
  int64_t rowOffset = 0;
  bool includesBaseOffset = false;
  if (!deriveRowOffset(globalRow, rowLoop.getInductionVar(), baseOffset,
                       rowOffset, includesBaseOffset)) {
    return;
  }
  Value localRow = materializeLocalRow(globalRow, baseOffset,
                                       includesBaseOffset, builder, userLoc);

  Value ownedRows = ctx.blockSize;
  int64_t stepConst = 0;
  if (ValueAnalysis::getConstantIndex(rowLoop.getStep(), stepConst) &&
      stepConst == 1) {
    Value lb = rowLoop.getLowerBound();
    Value ub = rowLoop.getUpperBound();
    if (lb && ub && ValueAnalysis::isZeroConstant(lb))
      ownedRows = ub;
  }

  auto selectValue = [&](Value cond, Value trueVal, Value falseVal) -> Value {
    bool forceIf = isa<MemRefType>(trueVal.getType());
    if (!forceIf && cond && cond.getType().isInteger(1))
      return arith::SelectOp::create(builder, userLoc, cond, trueVal, falseVal);
    auto ifOp = scf::IfOp::create(builder, userLoc, trueVal.getType(), cond,
                                  /*withElseRegion=*/true);
    {
      OpBuilder::InsertionGuard g(builder);
      builder.setInsertionPointToStart(&ifOp.getThenRegion().front());
      scf::YieldOp::create(builder, userLoc, trueVal);
      builder.setInsertionPointToStart(&ifOp.getElseRegion().front());
      scf::YieldOp::create(builder, userLoc, falseVal);
    }
    return ifOp.getResult(0);
  };

  Value selectedRowIdx = clampIndex(
      localRow, ctx.blockSize ? ctx.blockSize : ownedRows, builder, userLoc);
  Value selectedMemref = ctx.ownedMemref;

  if (region == StencilRegionKind::Left && rowOffset < 0) {
    Value leftIdx =
        arith::AddIOp::create(builder, userLoc, localRow, ctx.haloLeft);
    Value clampedLeftIdx =
        ctx.leftMemref ? clampIndex(leftIdx, ctx.haloLeft, builder, userLoc)
                       : Value();
    if (ctx.leftMemref && ctx.leftPtrNotNull) {
      selectedMemref =
          selectValue(ctx.leftPtrNotNull, ctx.leftMemref, ctx.ownedMemref);
      selectedRowIdx =
          selectValue(ctx.leftPtrNotNull, clampedLeftIdx, selectedRowIdx);
    } else if (ctx.leftMemref) {
      selectedMemref = ctx.leftMemref;
      selectedRowIdx = clampedLeftIdx;
    }
  } else if (region == StencilRegionKind::Right && rowOffset > 0) {
    Value rightIdx =
        arith::SubIOp::create(builder, userLoc, localRow, ownedRows);
    Value clampedRightIdx =
        ctx.rightMemref ? clampIndex(rightIdx, ctx.haloRight, builder, userLoc)
                        : Value();
    if (ctx.rightMemref && ctx.rightPtrNotNull) {
      selectedMemref =
          selectValue(ctx.rightPtrNotNull, ctx.rightMemref, ctx.ownedMemref);
      selectedRowIdx =
          selectValue(ctx.rightPtrNotNull, clampedRightIdx, selectedRowIdx);
    } else if (ctx.rightMemref) {
      selectedMemref = ctx.rightMemref;
      selectedRowIdx = clampedRightIdx;
    }
  }

  /// Build full index lists by replacing the partitioned dimension.
  Value stride =
      getLinearizedStride(indices, ctx.newElementType, builder, userLoc);
  auto buildAccessIndices = [&](Value rowIdx) {
    SmallVector<Value> out;
    out.reserve(indices.size());
    if (stride) {
      /// Linearized: re-linearize with rowIdx and extracted column.
      Value col = arith::RemUIOp::create(builder, userLoc, indices[0], stride);
      Value linear = arith::MulIOp::create(builder, userLoc, rowIdx, stride);
      linear = arith::AddIOp::create(builder, userLoc, linear, col);
      out.push_back(linear);
      return out;
    }
    for (size_t i = 0; i < indices.size(); ++i) {
      if (i == partitionDim)
        out.push_back(rowIdx);
      else
        out.push_back(indices[i]);
    }
    return out;
  };

  SmallVector<Value> selectedIndices = buildAccessIndices(selectedRowIdx);
  if (isLoad) {
    auto newLoad = memref::LoadOp::create(builder, userLoc, selectedMemref,
                                          selectedIndices);
    auto load = cast<memref::LoadOp>(user);
    load.replaceAllUsesWith(newLoad.getResult());
    ctx.opsToRemove.insert(load);
  } else {
    memref::StoreOp::create(builder, userLoc, storeValue, selectedMemref,
                            selectedIndices);
    ctx.opsToRemove.insert(user);
  }
}

///===----------------------------------------------------------------------===///
/// Helper: Try to version the row loop into Left/Middle/Right regions
///===----------------------------------------------------------------------===///

/// Attempt to split the row loop into three sub-loops (left band, middle band,
/// right band) so each region only accesses its respective buffer. Returns true
/// if versioning succeeded, false if the pattern doesn't match.
static bool tryVersionRowLoop(ArrayRef<Operation *> users, OpBuilder &builder,
                              const StencilRewriteContext &ctx,
                              bool &refErasedWithRowLoop) {
  auto fail = [&](llvm::StringRef reason) {
    ARTS_DEBUG("  Row-loop versioning skipped: " << reason);
    return false;
  };

  /// The current row-loop cloning path is not yet strong enough to prove
  /// exact local/global row rebasing for all surviving arithmetic that remains
  /// outside the rewritten load/store indices. Fall back to the per-row
  /// selection path until the specialization can rewrite against an exact
  /// cloned-ref contract.
  return fail("disabled pending exact-ref row-band rewrite");

  int64_t haloLeftConst = 0;
  int64_t haloRightConst = 0;
  if (!ValueAnalysis::getConstantIndex(ctx.haloLeft, haloLeftConst) ||
      !ValueAnalysis::getConstantIndex(ctx.haloRight, haloRightConst))
    return fail("non-constant halo sizes");

  scf::ForOp rowLoop;
  scf::ForOp innerLoop;
  int64_t minOffset = 0;
  int64_t maxOffset = 0;
  bool hasOffsets = false;
  bool rowIvIsLocal = false;
  bool rowIvIsLocalSet = false;

  for (Operation *user : users) {
    if (!isa<memref::LoadOp, memref::StoreOp>(user))
      continue;

    /// Extract indices from load/store
    ValueRange indices;
    if (auto load = dyn_cast<memref::LoadOp>(user)) {
      indices = load.getIndices();
    } else if (auto store = dyn_cast<memref::StoreOp>(user)) {
      indices = store.getIndices();
    }

    if (indices.empty())
      continue;

    /// Determine row loop and inner loop
    scf::ForOp foundInner;
    for (Operation *parent = user->getParentOp(); parent;
         parent = parent->getParentOp()) {
      if (auto loop = dyn_cast<scf::ForOp>(parent)) {
        foundInner = loop;
        break;
      }
    }
    if (!foundInner)
      return fail("missing inner loop");

    ValueRange refIndices = ctx.ref.getIndices();
    Value anchor = findAnchor(ctx.elemOffset);
    unsigned partitionDim = 0;
    if (!ctx.partitionInfo.partitionedDims.empty() &&
        ctx.partitionInfo.partitionedDims.front() < indices.size()) {
      partitionDim = ctx.partitionInfo.partitionedDims.front();
    } else {
      partitionDim = pickPartitionDim(indices, anchor);
    }
    Value globalRow = indices[partitionDim];

    if (anchor && !ValueAnalysis::dependsOn(globalRow, anchor) &&
        !refIndices.empty()) {
      unsigned refPartitionDim = pickPartitionDim(refIndices, anchor);
      Value refRow = refIndices[refPartitionDim];
      if (refRow == anchor || ValueAnalysis::dependsOn(refRow, anchor)) {
        globalRow = refRow;
      }
    }

    scf::ForOp foundRow;
    for (Operation *parent = user->getParentOp(); parent;
         parent = parent->getParentOp()) {
      auto loop = dyn_cast<scf::ForOp>(parent);
      if (!loop)
        continue;
      if (ValueAnalysis::dependsOn(globalRow, loop.getInductionVar())) {
        foundRow = loop;
        break;
      }
    }
    if (!foundRow)
      return fail("missing row loop");

    if (!rowLoop)
      rowLoop = foundRow;
    else if (rowLoop != foundRow)
      return fail("multiple row loops");

    if (!innerLoop)
      innerLoop = foundInner;
    else if (innerLoop != foundInner)
      return fail("multiple inner loops");

    Value baseOffset = ctx.partitionInfo.offsets.empty()
                           ? ctx.zero
                           : ctx.partitionInfo.offsets.front();
    int64_t rowOffset = 0;
    bool includesBaseOffset = false;
    if (!deriveRowOffset(globalRow, rowLoop.getInductionVar(), baseOffset,
                         rowOffset, includesBaseOffset))
      return fail("could not derive row offset");

    bool currentRowIvIsLocal = includesBaseOffset;
    if (!rowIvIsLocalSet) {
      rowIvIsLocal = currentRowIvIsLocal;
      rowIvIsLocalSet = true;
    } else if (rowIvIsLocal != currentRowIvIsLocal) {
      return fail("mixed local/global row IV semantics");
    }

    minOffset = hasOffsets ? std::min(minOffset, rowOffset) : rowOffset;
    maxOffset = hasOffsets ? std::max(maxOffset, rowOffset) : rowOffset;
    hasOffsets = true;
  }

  if (!rowLoop || !innerLoop || !hasOffsets)
    return fail("incomplete loop/offset discovery");

  int64_t leftBand = minOffset < 0 ? -minOffset : 0;
  int64_t rightBand = maxOffset > 0 ? maxOffset : 0;
  if (leftBand > 0 && haloLeftConst < leftBand)
    return fail("left halo too small");
  if (rightBand > 0 && haloRightConst < rightBand)
    return fail("right halo too small");

  if (rowLoop.getNumResults() != 0)
    return fail("row loop has iter args/results");

  int64_t stepConst = 0;
  if (!ValueAnalysis::getConstantIndex(rowLoop.getStep(), stepConst) ||
      stepConst != 1)
    return fail("row loop step is not constant 1");

  /// Create three row-loop regions: left, middle, right.
  OpBuilder::InsertionGuard versionGuard(builder);
  builder.setInsertionPoint(rowLoop);
  Location loopLoc = rowLoop.getLoc();

  Value lb = rowLoop.getLowerBound();
  Value ub = rowLoop.getUpperBound();
  Value step = rowLoop.getStep();
  Value zero = arts::createZeroIndex(builder, loopLoc);

  Value baseOffset = ctx.partitionInfo.offsets.empty()
                         ? zero
                         : ctx.partitionInfo.offsets.front();
  Value blockSz = ctx.partitionInfo.sizes.empty()
                      ? Value()
                      : ctx.partitionInfo.sizes.front();
  if (!baseOffset || !blockSz)
    return fail("missing base offset or block size");

  int64_t blockSzConst = 0;
  if (auto foldedBlockSz = ValueAnalysis::tryFoldConstantIndex(blockSz)) {
    blockSzConst = *foldedBlockSz;
  } else {
    return fail("non-constant block size");
  }
  if (leftBand + rightBand > blockSzConst)
    return fail("halo bands exceed block size");

  Value leftBandVal = arts::createConstantIndex(builder, loopLoc, leftBand);
  Value rightBandVal = arts::createConstantIndex(builder, loopLoc, rightBand);

  /// Split at localRow boundaries: left band, middle band, right band.
  Value leftRow = rowIvIsLocal ? zero : baseOffset;
  Value leftEnd = arith::AddIOp::create(builder, loopLoc, leftRow, leftBandVal);

  Value rightEnd = rowIvIsLocal ? blockSz
                                : arith::AddIOp::create(builder, loopLoc,
                                                        baseOffset, blockSz);
  Value rightRow =
      arith::SubIOp::create(builder, loopLoc, rightEnd, rightBandVal);

  /// Clamp splits to original loop bounds
  Value leftLb = arith::MaxSIOp::create(builder, loopLoc, lb, leftRow);
  Value leftUb = arith::MinSIOp::create(builder, loopLoc, ub, leftEnd);

  Value midLb = arith::MaxSIOp::create(builder, loopLoc, lb, leftEnd);
  Value midUb = arith::MinSIOp::create(builder, loopLoc, ub, rightRow);

  Value rightLb = arith::MaxSIOp::create(builder, loopLoc, lb, rightRow);
  Value rightUb = arith::MinSIOp::create(builder, loopLoc, ub, rightEnd);

  bool refInRowLoop = rowLoop->isAncestor(ctx.ref.getOperation());

  auto cloneRowLoop = [&](Value newLb, Value newUb,
                          StencilRegionKind region) -> scf::ForOp {
    scf::ForOp newLoop =
        scf::ForOp::create(builder, loopLoc, newLb, newUb, step);
    Block *oldBody = rowLoop.getBody();
    Block *newBody = newLoop.getBody();

    IRMapping mapping;
    mapping.map(rowLoop.getInductionVar(), newLoop.getInductionVar());

    builder.setInsertionPointToStart(newBody);
    for (Operation &op : oldBody->without_terminator())
      builder.clone(op, mapping);

    /// Rewrite loads/stores for this region.
    newLoop.getBody()->walk([&](Operation *op) {
      if (!isa<memref::LoadOp, memref::StoreOp>(op))
        return;
      auto mem = isa<memref::LoadOp>(op)
                     ? cast<memref::LoadOp>(op).getMemRef()
                     : cast<memref::StoreOp>(op).getMemRef();
      bool matchesRef = mem == ctx.ref.getResult();
      if (!matchesRef) {
        if (auto dbRef = mem.getDefiningOp<DbRefOp>())
          matchesRef = dbRef.getSource() == ctx.ref.getSource();
      }
      if (!matchesRef)
        return;
      rewriteUserForRegion(op, region, newLoop, builder, ctx);
    });

    return newLoop;
  };

  unsigned sameSourceRefsInRowLoop = 0;
  rowLoop.walk([&](DbRefOp dbRef) {
    if (dbRef.getSource() == ctx.ref.getSource())
      ++sameSourceRefsInRowLoop;
  });
  if (sameSourceRefsInRowLoop != 1)
    return fail("row loop has multiple same-source db_refs");

  cloneRowLoop(leftLb, leftUb, StencilRegionKind::Left);
  cloneRowLoop(midLb, midUb, StencilRegionKind::Middle);
  cloneRowLoop(rightLb, rightUb, StencilRegionKind::Right);

  rowLoop.erase();
  refErasedWithRowLoop = refInRowLoop;
  return true;
}

///===----------------------------------------------------------------------===///
/// Helper: Rewrite a single user in the fallback (non-versioned) path
///===----------------------------------------------------------------------===///

using OffsetMap = llvm::DenseMap<int64_t, RowSelection>;

/// Rewrite a single load/store user of the stencil db_ref using per-element
/// buffer selection (the fallback path when row loop versioning is not
/// applicable). Handles both load hoisting and store clamping.
static void rewriteUserFallback(
    Operation *user, OpBuilder &builder, const StencilRewriteContext &ctx,
    llvm::DenseMap<Value, std::pair<Value, Value>> &selectionCache,
    llvm::DenseMap<Operation *, OffsetMap> &rowSelectionCache,
    llvm::DenseMap<Operation *, Value> &rowExtentCache) {

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(user);
  Location userLoc = user->getLoc();

  /// Extract indices from load/store
  ValueRange indices;
  bool isLoad = false;
  Value storeValue;
  if (auto load = dyn_cast<memref::LoadOp>(user)) {
    indices = load.getIndices();
    isLoad = true;
  } else if (auto store = dyn_cast<memref::StoreOp>(user)) {
    indices = store.getIndices();
    storeValue = store.getValue();
  } else {
    return;
  }

  if (indices.empty())
    return;

  auto rowInfo = resolvePartitionRow(indices, ctx, builder, userLoc);
  unsigned partitionDim = rowInfo.partitionDim;
  Value globalRow = rowInfo.globalRow;

  Value baseOffset = ctx.partitionInfo.offsets.empty()
                         ? ctx.zero
                         : ctx.partitionInfo.offsets.front();
  Value localRow =
      maybeMaterializeLocalRow(globalRow, baseOffset, builder, userLoc);

  Value ownedIdx = localRow;

  /// Build full index lists by replacing the partitioned dimension.
  Value stride =
      getLinearizedStride(indices, ctx.newElementType, builder, userLoc);
  auto buildAccessIndices = [&](Value rowIdx) {
    SmallVector<Value> out;
    out.reserve(indices.size());
    if (stride) {
      /// Linearized: re-linearize with rowIdx and extracted column.
      Value col = arith::RemUIOp::create(builder, userLoc, indices[0], stride);
      Value linear = arith::MulIOp::create(builder, userLoc, rowIdx, stride);
      linear = arith::AddIOp::create(builder, userLoc, linear, col);
      out.push_back(linear);
      return out;
    }
    for (size_t i = 0; i < indices.size(); ++i) {
      if (i == partitionDim)
        out.push_back(rowIdx);
      else
        out.push_back(indices[i]);
    }
    return out;
  };

  if (isLoad) {
    /// LOAD: Hoist buffer selection to the row loop when possible, then do a
    /// single load inside the inner loop.

    Value selectedMemref;
    Value selectedRowIdx;

    bool usedRowSelection = false;

    /// Per-row ESD: if globalRow is rowIV + constant, hoist selection to the
    /// row loop body (one select per row, not per element).
    scf::ForOp rowLoop;
    for (Operation *parent = user->getParentOp(); parent;
         parent = parent->getParentOp()) {
      auto loop = dyn_cast<scf::ForOp>(parent);
      if (!loop)
        continue;
      if (ValueAnalysis::dependsOn(globalRow, loop.getInductionVar())) {
        rowLoop = loop;
        break;
      }
    }

    if (rowLoop) {
      Value rowIV = rowLoop.getInductionVar();
      int64_t rowOffset = 0;
      bool includesBaseOffset = false;
      if (deriveRowOffset(globalRow, rowIV, baseOffset, rowOffset,
                          includesBaseOffset)) {
        ownedIdx = materializeLocalRow(globalRow, baseOffset,
                                       includesBaseOffset, builder, userLoc);
        Value ownedRows = ctx.blockSize;
        auto extentIt = rowExtentCache.find(rowLoop.getOperation());
        if (extentIt != rowExtentCache.end()) {
          ownedRows = extentIt->second;
        } else {
          int64_t stepConst = 0;
          if (ValueAnalysis::getConstantIndex(rowLoop.getStep(), stepConst) &&
              stepConst == 1) {
            Value lb = rowLoop.getLowerBound();
            Value ub = rowLoop.getUpperBound();
            if (lb && ub && ValueAnalysis::isZeroConstant(lb))
              ownedRows = ub;
          }
          rowExtentCache[rowLoop.getOperation()] = ownedRows;
        }

        ARTS_DEBUG("  ESD row selection: offset=" << rowOffset);
        auto &offsetMap = rowSelectionCache[rowLoop.getOperation()];
        auto it = offsetMap.find(rowOffset);
        if (it != offsetMap.end()) {
          selectedMemref = it->second.memref;
          selectedRowIdx = it->second.rowIdx;
          usedRowSelection = true;
        } else {
          OpBuilder::InsertionGuard hoistGuard(builder);
          builder.setInsertionPointToStart(rowLoop.getBody());
          Location rowLoc = rowLoop.getLoc();

          Value rowIdx = rowIV;
          if (rowOffset != 0) {
            Value offVal =
                arts::createConstantIndex(builder, rowLoc, rowOffset);
            rowIdx = arith::AddIOp::create(builder, rowLoc, rowIV, offVal);
          }

          Value localRowHoisted = materializeLocalRow(
              rowIdx, baseOffset, includesBaseOffset, builder, rowLoc);

          RowSelection selection =
              buildSelection(builder, rowLoc, localRowHoisted, ownedRows, ctx);
          selectedMemref = selection.memref;
          selectedRowIdx = selection.rowIdx;
          offsetMap[rowOffset] = selection;
          usedRowSelection = true;
        }
      } else {
        std::string rowExpr;
        llvm::raw_string_ostream rowOS(rowExpr);
        globalRow.print(rowOS);
        std::string ivExpr;
        llvm::raw_string_ostream ivOS(ivExpr);
        rowIV.print(ivOS);
        ARTS_DEBUG("  ESD row selection: unable to derive row offset, row="
                   << rowOS.str() << ", iv=" << ivOS.str());
      }
    }

    if (!usedRowSelection) {
      /// Reuse selection for identical row expressions (e.g., same row for
      /// center/left/right neighbors).
      auto cached = selectionCache.find(globalRow);
      if (cached != selectionCache.end()) {
        selectedMemref = cached->second.first;
        selectedRowIdx = cached->second.second;
      } else {
        OpBuilder::InsertionGuard hoistGuard(builder);

        /// Find a loop to hoist above (if globalRow is invariant to inner
        /// loop).
        Operation *rowDefOp = globalRow.getDefiningOp();
        Block *rowDefBlock = isa<BlockArgument>(globalRow)
                                 ? cast<BlockArgument>(globalRow).getOwner()
                                 : (rowDefOp ? rowDefOp->getBlock() : nullptr);

        scf::ForOp hoistAbove;
        for (Operation *parent = user->getParentOp(); parent;
             parent = parent->getParentOp()) {
          auto loop = dyn_cast<scf::ForOp>(parent);
          if (!loop)
            continue;
          if (rowDefBlock && loop.getBody() == rowDefBlock)
            break;
          if (rowDefOp && loop->isAncestor(rowDefOp))
            break;
          if (ValueAnalysis::dependsOn(globalRow, loop.getInductionVar()))
            break;
          hoistAbove = loop;
        }

        if (hoistAbove) {
          /// Insert before the inner loop, but after the row definition if
          /// it lives in the same block.
          builder.setInsertionPoint(hoistAbove);
          if (rowDefOp && rowDefOp->getBlock() == hoistAbove->getBlock() &&
              rowDefOp->isBeforeInBlock(hoistAbove))
            builder.setInsertionPointAfter(rowDefOp);
        } else {
          /// Fall back to local hoisting at the load site.
          builder.setInsertionPoint(user);
        }

        /// Recompute row-local indices at the hoist point.
        Value localRowHoisted =
            maybeMaterializeLocalRow(globalRow, baseOffset, builder, userLoc);

        RowSelection selection = buildSelection(
            builder, userLoc, localRowHoisted, ctx.blockSize, ctx);
        selectedMemref = selection.memref;
        selectedRowIdx = selection.rowIdx;
      }

      if (!usedRowSelection && selectedMemref && selectedRowIdx)
        selectionCache[globalRow] = {selectedMemref, selectedRowIdx};
    }

    SmallVector<Value> selectedIndices = buildAccessIndices(selectedRowIdx);
    auto selectedLoad = memref::LoadOp::create(builder, userLoc, selectedMemref,
                                               selectedIndices);

    auto load = cast<memref::LoadOp>(user);
    load.replaceAllUsesWith(selectedLoad.getResult());
    ctx.opsToRemove.insert(load);

  } else {
    /// STORE: Only store to owned buffer (halos are read-only)
    Value clampedOwnedIdx =
        clampIndex(ownedIdx, ctx.blockSize, builder, userLoc);
    SmallVector<Value> ownedIndices = buildAccessIndices(clampedOwnedIdx);

    memref::StoreOp::create(builder, userLoc, storeValue, ctx.ownedMemref,
                            ownedIndices);
    ctx.opsToRemove.insert(user);
  }
}

///===----------------------------------------------------------------------===///
/// Index Localization (interface methods - stencil uses 3-buffer mode instead)
///===----------------------------------------------------------------------===///

LocalizedIndices DbStencilIndexer::localize(ArrayRef<Value>, OpBuilder &,
                                            Location) {
  ARTS_UNREACHABLE("DbStencilIndexer uses 3-buffer mode, not localize()");
}

LocalizedIndices DbStencilIndexer::localizeLinearized(Value, Value, OpBuilder &,
                                                      Location) {
  ARTS_UNREACHABLE(
      "DbStencilIndexer uses 3-buffer mode, not localizeLinearized()");
}

///===----------------------------------------------------------------------===///
/// Operation Rewriting with 3-Buffer Selection
///===----------------------------------------------------------------------===///

void DbStencilIndexer::transformDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {

  /// Stencil indexer requires 3-buffer mode (owned + left/right halos)
  bool has3BufferMode = leftHaloArg || rightHaloArg;
  assert(has3BufferMode && "DbStencilIndexer requires halo arguments");

  ARTS_DEBUG("DbStencilIndexer::transformDbRefUsers (3-buffer mode)");

  SmallVector<Operation *> users(ref.getResult().getUsers().begin(),
                                 ref.getResult().getUsers().end());

  if (users.empty()) {
    opsToRemove.insert(ref.getOperation());
    return;
  }

  /// Insert DbRefOps at the START of EDT block to ensure dominance for all
  /// users. Users can be in different blocks (loop iterations, branches), so
  /// inserting at users.front() would cause SSA dominance errors.
  Block *edtBlock = cast<BlockArgument>(ownedArg).getOwner();
  Location loc = edtBlock->front().getLoc();
  OpBuilder::InsertionGuard topGuard(builder);
  builder.setInsertionPointToStart(edtBlock);

  Value zero = arts::createZeroIndex(builder, loc);

  /// Create 3 DbRefOps ONCE at the start, reuse for all accesses
  assert(haloLeft && blockSize &&
         "Stencil mode requires haloLeft and blockSize");

  Value ownedMemref, leftMemref, rightMemref;

  /// Create owned memref (always needed)
  auto ownedRef = DbRefOp::create(builder, loc, newElementType, ownedArg,
                                  SmallVector<Value>{zero});
  ownedMemref = ownedRef.getResult();

  /// Create left halo memref if available (null at left boundary)
  if (leftHaloArg) {
    auto leftRef = DbRefOp::create(builder, loc, newElementType, leftHaloArg,
                                   SmallVector<Value>{zero});
    leftMemref = leftRef.getResult();
  }

  /// Create right halo memref if available (null at right boundary)
  if (rightHaloArg) {
    auto rightRef = DbRefOp::create(builder, loc, newElementType, rightHaloArg,
                                    SmallVector<Value>{zero});
    rightMemref = rightRef.getResult();
  }

  /// BASE-OFFSET SEMANTICS: Simplified region boundaries
  /// With base offset (not extended), the regions are:
  ///   localRow < 0        -> left halo (negative indices)
  ///   0 <= localRow < blockSize -> owned
  ///   localRow >= blockSize    -> right halo
  ///
  /// Buffer-local indices:
  ///   Left halo: index = localRow + haloLeft (shift negative to positive)
  ///   Owned: index = localRow (0-based within owned chunk)
  ///   Right halo: index = localRow - blockSize (0-based within right halo)

  auto llvmPtrTy = LLVM::LLVMPointerType::get(builder.getContext());
  Value nullPtr = LLVM::ZeroOp::create(builder, loc, llvmPtrTy);
  Value leftPtrNotNull;
  Value rightPtrNotNull;

  if (leftMemref) {
    Value leftPtr = polygeist::Memref2PointerOp::create(builder, loc, llvmPtrTy,
                                                        leftMemref);
    leftPtrNotNull = LLVM::ICmpOp::create(builder, loc, LLVM::ICmpPredicate::ne,
                                          leftPtr, nullPtr);
  }

  if (rightMemref) {
    Value rightPtr = polygeist::Memref2PointerOp::create(
        builder, loc, llvmPtrTy, rightMemref);
    rightPtrNotNull = LLVM::ICmpOp::create(
        builder, loc, LLVM::ICmpPredicate::ne, rightPtr, nullPtr);
  }

  ARTS_DEBUG("  Created 3 buffer refs: owned="
             << (ownedMemref ? "yes" : "no")
             << ", left=" << (leftMemref ? "yes" : "no")
             << ", right=" << (rightMemref ? "yes" : "no"));

  /// Build shared context for helper functions.
  StencilRewriteContext ctx{ref,
                            partitionInfo,
                            elemOffset,
                            haloLeft,
                            haloRight,
                            blockSize,
                            zero,
                            newElementType,
                            ownedMemref,
                            leftMemref,
                            rightMemref,
                            leftAvail,
                            rightAvail,
                            leftPtrNotNull,
                            rightPtrNotNull,
                            opsToRemove};

  /// Try row-loop versioning first (splits loop into Left/Middle/Right).
  bool refErasedWithRowLoop = false;
  if (tryVersionRowLoop(users, builder, ctx, refErasedWithRowLoop)) {
    if (!refErasedWithRowLoop)
      opsToRemove.insert(ref.getOperation());
    return;
  }

  /// Fallback: per-element buffer selection for each user.
  llvm::DenseMap<Value, std::pair<Value, Value>> selectionCache;
  llvm::DenseMap<Operation *, OffsetMap> rowSelectionCache;
  llvm::DenseMap<Operation *, Value> rowExtentCache;

  for (Operation *user : users) {
    rewriteUserFallback(user, builder, ctx, selectionCache, rowSelectionCache,
                        rowExtentCache);
  }

  opsToRemove.insert(ref.getOperation());
}

void DbStencilIndexer::transformOps(ArrayRef<Operation *> ops, Value dbPtr,
                                    Type elementType, ArtsCodegen &AC,
                                    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbStencilIndexer::transformOps - " << ops.size() << " ops");

  for (Operation *op : ops) {
    OpBuilder::InsertionGuard guard(AC.getBuilder());
    AC.setInsertionPoint(op);

    /// DbRefOp: delegate to transformDbRefUsers (uses 3-buffer mode)
    if (auto dbRef = dyn_cast<DbRefOp>(op)) {
      transformDbRefUsers(dbRef, dbPtr, elementType, AC.getBuilder(),
                          opsToRemove);
      continue;
    }

    /// Stencil mode requires all accesses through DbRefOp
    if (isa<memref::LoadOp, memref::StoreOp>(op))
      ARTS_UNREACHABLE("DbStencilIndexer: direct load/store not supported, "
                       "use DbRefOp for 3-buffer mode");
  }
}
