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
///==========================================================================///

#include "arts/Transforms/Datablock/Stencil/DbStencilIndexer.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "db_stencil"

ARTS_DEBUG_SETUP(db_stencil);

using namespace mlir;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// Constructor
//===----------------------------------------------------------------------===//

DbStencilIndexer::DbStencilIndexer(const PartitionInfo &info, Value haloLeft,
                                   Value haloRight, unsigned outerRank,
                                   unsigned innerRank, Value ownedArg,
                                   Value leftHaloArg, Value rightHaloArg)
    : DbIndexerBase(info, outerRank, innerRank),
      elemOffset(info.offsets.empty() ? Value() : info.offsets.front()),
      haloLeft(haloLeft), haloRight(haloRight),
      blockSize(info.sizes.empty() ? Value() : info.sizes.front()),
      ownedArg(ownedArg), leftHaloArg(leftHaloArg), rightHaloArg(rightHaloArg) {
}

//===----------------------------------------------------------------------===//
// Helper: Clamp index to valid bounds
//===----------------------------------------------------------------------===//

/// Clamp index to [0, size-1] to prevent out-of-bounds access.
static Value clampIndex(Value index, Value size, OpBuilder &builder,
                        Location loc) {
  if (!size)
    return index;

  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
  Value lastValidIdx = builder.create<arith::SubIOp>(loc, size, one);

  /// clamp(index, 0, lastValidIdx)
  Value clamped = builder.create<arith::MaxSIOp>(loc, index, zero);
  return builder.create<arith::MinSIOp>(loc, clamped, lastValidIdx);
}

//===----------------------------------------------------------------------===//
// Helper: Detect and get stride for linearized access
//===----------------------------------------------------------------------===//

/// Returns stride if the access pattern is linearized (single index into
/// multi-dimensional memref), otherwise returns nullptr.
static Value getLinearizedStride(ValueRange indices, Type elementType,
                                 OpBuilder &builder, Location loc) {
  if (indices.size() != 1)
    return nullptr;

  auto memrefType = elementType.dyn_cast<MemRefType>();
  if (!memrefType || memrefType.getRank() < 2)
    return nullptr;

  auto staticStride = DatablockUtils::getStaticStride(memrefType);
  if (!staticStride || *staticStride <= 1)
    return nullptr;

  return builder.create<arith::ConstantIndexOp>(loc, *staticStride);
}

/// Try to find a stable anchor (typically a loop IV) for dependency checks.
static Value findAnchor(Value v, int depth = 0) {
  if (!v || depth > 6)
    return v;
  v = ValueUtils::stripNumericCasts(v);
  if (auto blockArg = v.dyn_cast<BlockArgument>())
    return blockArg;
  if (Operation *op = v.getDefiningOp()) {
    for (Value operand : op->getOperands()) {
      Value anchor = findAnchor(operand, depth + 1);
      if (anchor && anchor.isa<BlockArgument>())
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
    if (ValueUtils::dependsOn(indices[i], anchor))
      return i;
  }
  return 0;
}

//===----------------------------------------------------------------------===//
// Index Localization (interface methods - stencil uses 3-buffer mode instead)
//===----------------------------------------------------------------------===//

LocalizedIndices DbStencilIndexer::localize(ArrayRef<Value>, OpBuilder &,
                                            Location) {
  ARTS_UNREACHABLE("DbStencilIndexer uses 3-buffer mode, not localize()");
}

LocalizedIndices DbStencilIndexer::localizeLinearized(Value, Value, OpBuilder &,
                                                      Location) {
  ARTS_UNREACHABLE(
      "DbStencilIndexer uses 3-buffer mode, not localizeLinearized()");
}

//===----------------------------------------------------------------------===//
// Operation Rewriting with 3-Buffer Selection
//===----------------------------------------------------------------------===//

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
  Block *edtBlock = ownedArg.cast<BlockArgument>().getOwner();
  Location loc = edtBlock->front().getLoc();
  OpBuilder::InsertionGuard topGuard(builder);
  builder.setInsertionPointToStart(edtBlock);

  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);

  /// Create 3 DbRefOps ONCE at the start, reuse for all accesses
  assert(haloLeft && blockSize &&
         "Stencil mode requires haloLeft and blockSize");

  Value ownedMemref, leftMemref, rightMemref;

  /// Create owned memref (always needed)
  auto ownedRef = builder.create<DbRefOp>(loc, newElementType, ownedArg,
                                          SmallVector<Value>{zero});
  ownedMemref = ownedRef.getResult();

  /// Create left halo memref if available (null at left boundary)
  if (leftHaloArg) {
    auto leftRef = builder.create<DbRefOp>(loc, newElementType, leftHaloArg,
                                           SmallVector<Value>{zero});
    leftMemref = leftRef.getResult();
  }

  /// Create right halo memref if available (null at right boundary)
  if (rightHaloArg) {
    auto rightRef = builder.create<DbRefOp>(loc, newElementType, rightHaloArg,
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
  Value nullPtr = builder.create<LLVM::ZeroOp>(loc, llvmPtrTy);
  Value leftPtrNotNull;
  Value rightPtrNotNull;

  if (leftMemref) {
    Value leftPtr =
        builder.create<polygeist::Memref2PointerOp>(loc, llvmPtrTy, leftMemref);
    leftPtrNotNull = builder.create<LLVM::ICmpOp>(loc, LLVM::ICmpPredicate::ne,
                                                  leftPtr, nullPtr);
  }

  if (rightMemref) {
    Value rightPtr = builder.create<polygeist::Memref2PointerOp>(loc, llvmPtrTy,
                                                                 rightMemref);
    rightPtrNotNull = builder.create<LLVM::ICmpOp>(loc, LLVM::ICmpPredicate::ne,
                                                   rightPtr, nullPtr);
  }

  ARTS_DEBUG("  Created 3 buffer refs: owned="
             << (ownedMemref ? "yes" : "no")
             << ", left=" << (leftMemref ? "yes" : "no")
             << ", right=" << (rightMemref ? "yes" : "no"));

  llvm::DenseMap<Value, std::pair<Value, Value>> selectionCache;
  struct RowSelection {
    Value memref;
    Value rowIdx;
  };
  using OffsetMap = llvm::DenseMap<int64_t, RowSelection>;
  llvm::DenseMap<Operation *, OffsetMap> rowSelectionCache;

  auto isSameValueWithCasts = [&](Value a, Value b) -> bool {
    if (!a || !b)
      return false;
    if (a == b)
      return true;
    a = ValueUtils::stripNumericCasts(a);
    b = ValueUtils::stripNumericCasts(b);
    if (a == b)
      return true;
    if (auto cast = a.getDefiningOp<arith::IndexCastOp>())
      if (cast.getIn() == b)
        return true;
    if (auto cast = b.getDefiningOp<arith::IndexCastOp>())
      if (cast.getIn() == a)
        return true;
    return false;
  };

  auto deriveRowOffset = [&](Value rowExpr, Value rowIV, Value baseOffsetVal,
                             int64_t &rowOffset,
                             bool &includesBaseOffset) -> bool {
    int64_t constOff = 0;
    Value baseExpr = ValueUtils::stripConstantOffset(rowExpr, &constOff);
    baseExpr = ValueUtils::stripNumericCasts(baseExpr);

    if (isSameValueWithCasts(baseExpr, rowIV)) {
      rowOffset = constOff;
      includesBaseOffset = false;
      return true;
    }

    if (auto addOp = baseExpr.getDefiningOp<arith::AddIOp>()) {
      Value lhs = ValueUtils::stripNumericCasts(addOp.getLhs());
      Value rhs = ValueUtils::stripNumericCasts(addOp.getRhs());
      bool lhsIsRow = isSameValueWithCasts(lhs, rowIV);
      bool rhsIsRow = isSameValueWithCasts(rhs, rowIV);
      if (lhsIsRow || rhsIsRow) {
        Value other = lhsIsRow ? rhs : lhs;
        if (baseOffsetVal && (other == baseOffsetVal ||
                              ValueUtils::dependsOn(other, baseOffsetVal))) {
          rowOffset = constOff;
          includesBaseOffset = true;
          return true;
        }
        if (!ValueUtils::dependsOn(other, rowIV)) {
          rowOffset = constOff;
          includesBaseOffset =
              baseOffsetVal && ValueUtils::dependsOn(other, baseOffsetVal);
          return true;
        }
      }
    }

    if (ValueUtils::dependsOn(baseExpr, rowIV)) {
      rowOffset = constOff;
      includesBaseOffset =
          baseOffsetVal && ValueUtils::dependsOn(baseExpr, baseOffsetVal);
      return true;
    }

    return false;
  };

  enum class RegionKind { Left, Middle, Right };

  auto rewriteUserForRegion = [&](Operation *user, RegionKind region,
                                  scf::ForOp rowLoop) {
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

    /// Extract row index for region determination
    ValueRange refIndices = ref.getIndices();
    Value anchor = findAnchor(elemOffset);
    unsigned partitionDim = 0;
    if (!partitionInfo.partitionedDims.empty() &&
        partitionInfo.partitionedDims.front() < indices.size()) {
      partitionDim = partitionInfo.partitionedDims.front();
    } else {
      partitionDim = pickPartitionDim(indices, anchor);
    }
    Value globalRow = indices[partitionDim];

    /// If load/store indices don't carry the partition dimension, fall back to
    /// the db_ref indices (common for row-partitioned 2D stencils).
    if (anchor && !ValueUtils::dependsOn(globalRow, anchor) &&
        !refIndices.empty()) {
      unsigned refPartitionDim = pickPartitionDim(refIndices, anchor);
      Value refRow = refIndices[refPartitionDim];
      if (refRow == anchor || ValueUtils::dependsOn(refRow, anchor)) {
        globalRow = refRow;
      }
    }

    /// Handle linearized access: extract row from linear index
    Value stride =
        getLinearizedStride(indices, newElementType, builder, userLoc);
    if (stride) {
      globalRow = builder.create<arith::DivUIOp>(userLoc, indices[0], stride);
    }

    /// Compute localRow = globalRow - baseOffset (using BASE offset semantics)
    Value baseOffset =
        partitionInfo.offsets.empty() ? zero : partitionInfo.offsets.front();
    Value localRow =
        baseOffset
            ? builder.create<arith::SubIOp>(userLoc, globalRow, baseOffset)
            : globalRow;

    int64_t rowOffset = 0;
    bool includesBaseOffset = false;
    if (!deriveRowOffset(globalRow, rowLoop.getInductionVar(), baseOffset,
                         rowOffset, includesBaseOffset)) {
      return;
    }

    Value selectedRowIdx = localRow;
    Value selectedMemref = ownedMemref;

    if (region == RegionKind::Left && rowOffset < 0) {
      selectedMemref = leftMemref ? leftMemref : ownedMemref;
      selectedRowIdx =
          builder.create<arith::AddIOp>(userLoc, localRow, haloLeft);
    } else if (region == RegionKind::Right && rowOffset > 0) {
      selectedMemref = rightMemref ? rightMemref : ownedMemref;
      selectedRowIdx =
          builder.create<arith::SubIOp>(userLoc, localRow, blockSize);
    }

    /// Build full index lists by replacing the partitioned dimension.
    auto buildAccessIndices = [&](Value rowIdx) {
      SmallVector<Value> out;
      out.reserve(indices.size());
      if (stride) {
        /// Linearized: re-linearize with rowIdx and extracted column.
        Value col = builder.create<arith::RemUIOp>(userLoc, indices[0], stride);
        Value linear = builder.create<arith::MulIOp>(userLoc, rowIdx, stride);
        linear = builder.create<arith::AddIOp>(userLoc, linear, col);
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
      auto newLoad = builder.create<memref::LoadOp>(userLoc, selectedMemref,
                                                    selectedIndices);
      auto load = cast<memref::LoadOp>(user);
      load.replaceAllUsesWith(newLoad.getResult());
      opsToRemove.insert(load);
    } else {
      builder.create<memref::StoreOp>(userLoc, storeValue, selectedMemref,
                                      selectedIndices);
      opsToRemove.insert(user);
    }
  };

  bool refErasedWithRowLoop = false;
  auto tryVersionRowLoop = [&]() -> bool {
    int64_t haloLeftConst = 0;
    int64_t haloRightConst = 0;
    if (!ValueUtils::getConstantIndex(haloLeft, haloLeftConst) ||
        !ValueUtils::getConstantIndex(haloRight, haloRightConst))
      return false;

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
        return false;

      ValueRange refIndices = ref.getIndices();
      Value anchor = findAnchor(elemOffset);
      unsigned partitionDim = 0;
      if (!partitionInfo.partitionedDims.empty() &&
          partitionInfo.partitionedDims.front() < indices.size()) {
        partitionDim = partitionInfo.partitionedDims.front();
      } else {
        partitionDim = pickPartitionDim(indices, anchor);
      }
      Value globalRow = indices[partitionDim];

      if (anchor && !ValueUtils::dependsOn(globalRow, anchor) &&
          !refIndices.empty()) {
        unsigned refPartitionDim = pickPartitionDim(refIndices, anchor);
        Value refRow = refIndices[refPartitionDim];
        if (refRow == anchor || ValueUtils::dependsOn(refRow, anchor)) {
          globalRow = refRow;
        }
      }

      if (indices.size() == 1) {
        if (auto memrefType = newElementType.dyn_cast<MemRefType>()) {
          if (memrefType.getRank() >= 2) {
            auto staticStride = DatablockUtils::getStaticStride(memrefType);
            if (staticStride && *staticStride > 1)
              return false;
          }
        }
      }

      scf::ForOp foundRow;
      for (Operation *parent = user->getParentOp(); parent;
           parent = parent->getParentOp()) {
        auto loop = dyn_cast<scf::ForOp>(parent);
        if (!loop)
          continue;
        if (ValueUtils::dependsOn(globalRow, loop.getInductionVar())) {
          foundRow = loop;
          break;
        }
      }
      if (!foundRow)
        return false;

      if (!rowLoop)
        rowLoop = foundRow;
      else if (rowLoop != foundRow)
        return false;

      if (!innerLoop)
        innerLoop = foundInner;
      else if (innerLoop != foundInner)
        return false;

      Value baseOffset =
          partitionInfo.offsets.empty() ? zero : partitionInfo.offsets.front();
      int64_t rowOffset = 0;
      bool includesBaseOffset = false;
      if (!deriveRowOffset(globalRow, rowLoop.getInductionVar(), baseOffset,
                           rowOffset, includesBaseOffset))
        return false;

      if (!rowIvIsLocalSet) {
        rowIvIsLocal = includesBaseOffset;
        rowIvIsLocalSet = true;
      } else if (rowIvIsLocal != includesBaseOffset) {
        return false;
      }

      minOffset = hasOffsets ? std::min(minOffset, rowOffset) : rowOffset;
      maxOffset = hasOffsets ? std::max(maxOffset, rowOffset) : rowOffset;
      hasOffsets = true;
    }

    if (!rowLoop || !innerLoop || !hasOffsets)
      return false;

    int64_t leftBand = minOffset < 0 ? -minOffset : 0;
    int64_t rightBand = maxOffset > 0 ? maxOffset : 0;
    if (leftBand > 0 && haloLeftConst < leftBand)
      return false;
    if (rightBand > 0 && haloRightConst < rightBand)
      return false;

    if (rowLoop.getNumResults() != 0)
      return false;

    int64_t stepConst = 0;
    if (!ValueUtils::getConstantIndex(rowLoop.getStep(), stepConst) ||
        stepConst != 1)
      return false;

    /// Create three row-loop regions: left, middle, right.
    OpBuilder::InsertionGuard versionGuard(builder);
    builder.setInsertionPoint(rowLoop);
    Location loopLoc = rowLoop.getLoc();

    Value lb = rowLoop.getLowerBound();
    Value ub = rowLoop.getUpperBound();
    Value step = rowLoop.getStep();
    Value zero = builder.create<arith::ConstantIndexOp>(loopLoc, 0);

    Value baseOffset =
        partitionInfo.offsets.empty() ? zero : partitionInfo.offsets.front();
    Value blockSz =
        partitionInfo.sizes.empty() ? Value() : partitionInfo.sizes.front();
    if (!baseOffset || !blockSz)
      return false;

    int64_t blockSzConst = 0;
    if (!ValueUtils::getConstantIndex(blockSz, blockSzConst))
      return false;
    if (leftBand + rightBand > blockSzConst)
      return false;

    Value leftBandVal =
        builder.create<arith::ConstantIndexOp>(loopLoc, leftBand);
    Value rightBandVal =
        builder.create<arith::ConstantIndexOp>(loopLoc, rightBand);

    /// Split at localRow boundaries: left band, middle band, right band.
    Value leftRow = rowIvIsLocal ? zero : baseOffset;
    Value leftEnd =
        builder.create<arith::AddIOp>(loopLoc, leftRow, leftBandVal);

    Value rightEnd =
        rowIvIsLocal
            ? blockSz
            : builder.create<arith::AddIOp>(loopLoc, baseOffset, blockSz);
    Value rightRow =
        builder.create<arith::SubIOp>(loopLoc, rightEnd, rightBandVal);

    /// Clamp splits to original loop bounds
    Value leftLb = builder.create<arith::MaxSIOp>(loopLoc, lb, leftRow);
    Value leftUb = builder.create<arith::MinSIOp>(loopLoc, ub, leftEnd);

    Value midLb = builder.create<arith::MaxSIOp>(loopLoc, lb, leftEnd);
    Value midUb = builder.create<arith::MinSIOp>(loopLoc, ub, rightRow);

    Value rightLb = builder.create<arith::MaxSIOp>(loopLoc, lb, rightRow);
    Value rightUb = builder.create<arith::MinSIOp>(loopLoc, ub, rightEnd);

    bool refInRowLoop = rowLoop->isAncestor(ref.getOperation());

    auto cloneRowLoop = [&](Value newLb, Value newUb,
                            RegionKind region) -> scf::ForOp {
      scf::ForOp newLoop =
          builder.create<scf::ForOp>(loopLoc, newLb, newUb, step);
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
        bool matchesRef = mem == ref.getResult();
        if (!matchesRef) {
          if (auto dbRef = mem.getDefiningOp<DbRefOp>())
            matchesRef = dbRef.getSource() == ref.getSource();
        }
        if (!matchesRef)
          return;
        rewriteUserForRegion(op, region, newLoop);
      });

      return newLoop;
    };

    cloneRowLoop(leftLb, leftUb, RegionKind::Left);
    cloneRowLoop(midLb, midUb, RegionKind::Middle);
    cloneRowLoop(rightLb, rightUb, RegionKind::Right);

    rowLoop.erase();
    refErasedWithRowLoop = refInRowLoop;
    return true;
  };

  if (tryVersionRowLoop()) {
    if (!refErasedWithRowLoop)
      opsToRemove.insert(ref.getOperation());
    return;
  }

  auto buildSelection = [&](OpBuilder &selBuilder, Location selLoc,
                            Value localRowVal) -> RowSelection {
    Value isLeft = selBuilder.create<arith::CmpIOp>(
        selLoc, arith::CmpIPredicate::slt, localRowVal, zero);
    Value leftIdx =
        selBuilder.create<arith::AddIOp>(selLoc, localRowVal, haloLeft);
    Value ownedIdx = localRowVal;
    Value rightIdx =
        selBuilder.create<arith::SubIOp>(selLoc, localRowVal, blockSize);

    Value clampedOwnedIdx = clampIndex(ownedIdx, blockSize, selBuilder, selLoc);
    Value clampedLeftIdx =
        leftMemref ? clampIndex(leftIdx, haloLeft, selBuilder, selLoc)
                   : Value();
    Value clampedRightIdx =
        rightMemref ? clampIndex(rightIdx, haloRight, selBuilder, selLoc)
                    : Value();

    Value selectedMemref = ownedMemref;
    Value selectedRowIdx = clampedOwnedIdx;

    auto selectValue = [&](Value cond, Value trueVal, Value falseVal) -> Value {
      bool forceIf = trueVal.getType().isa<MemRefType>();
      if (!forceIf && cond && cond.getType().isInteger(1))
        return selBuilder.create<arith::SelectOp>(selLoc, cond, trueVal,
                                                  falseVal);
      auto ifOp = selBuilder.create<scf::IfOp>(selLoc, trueVal.getType(), cond,
                                               /*withElseRegion=*/true);
      {
        OpBuilder::InsertionGuard g(selBuilder);
        selBuilder.setInsertionPointToStart(&ifOp.getThenRegion().front());
        selBuilder.create<scf::YieldOp>(selLoc, trueVal);
        selBuilder.setInsertionPointToStart(&ifOp.getElseRegion().front());
        selBuilder.create<scf::YieldOp>(selLoc, falseVal);
      }
      return ifOp.getResult(0);
    };

    if (leftMemref) {
      Value safeLeftMemref =
          leftPtrNotNull ? selectValue(leftPtrNotNull, leftMemref, ownedMemref)
                         : leftMemref;
      Value safeLeftIdx =
          leftPtrNotNull
              ? selectValue(leftPtrNotNull, clampedLeftIdx, clampedOwnedIdx)
              : clampedLeftIdx;
      selectedMemref = selectValue(isLeft, safeLeftMemref, selectedMemref);
      selectedRowIdx = selectValue(isLeft, safeLeftIdx, selectedRowIdx);
    }

    if (rightMemref) {
      Value isRight = selBuilder.create<arith::CmpIOp>(
          selLoc, arith::CmpIPredicate::sge, localRowVal, blockSize);

      Value safeRightMemref =
          rightPtrNotNull
              ? selectValue(rightPtrNotNull, rightMemref, ownedMemref)
              : rightMemref;
      Value safeRightIdx =
          rightPtrNotNull
              ? selectValue(rightPtrNotNull, clampedRightIdx, clampedOwnedIdx)
              : clampedRightIdx;

      selectedMemref = selectValue(isRight, safeRightMemref, selectedMemref);
      selectedRowIdx = selectValue(isRight, safeRightIdx, selectedRowIdx);
    }

    return {selectedMemref, selectedRowIdx};
  };

  for (Operation *user : users) {
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
      continue;
    }

    if (indices.empty())
      continue;

    /// Extract row index for region determination
    ValueRange refIndices = ref.getIndices();
    Value anchor = findAnchor(elemOffset);
    unsigned partitionDim = 0;
    if (!partitionInfo.partitionedDims.empty() &&
        partitionInfo.partitionedDims.front() < indices.size()) {
      partitionDim = partitionInfo.partitionedDims.front();
    } else {
      partitionDim = pickPartitionDim(indices, anchor);
    }
    Value globalRow = indices[partitionDim];

    /// If load/store indices don't carry the partition dimension, fall back to
    /// the db_ref indices (common for row-partitioned 2D stencils).
    if (anchor && !ValueUtils::dependsOn(globalRow, anchor) &&
        !refIndices.empty()) {
      unsigned refPartitionDim = pickPartitionDim(refIndices, anchor);
      Value refRow = refIndices[refPartitionDim];
      if (refRow == anchor || ValueUtils::dependsOn(refRow, anchor)) {
        globalRow = refRow;
      }
    }

    /// Handle linearized access: extract row from linear index
    Value stride =
        getLinearizedStride(indices, newElementType, builder, userLoc);
    if (stride) {
      globalRow = builder.create<arith::DivUIOp>(userLoc, indices[0], stride);
    }

    /// Compute localRow = globalRow - baseOffset (using BASE offset semantics)
    /// The baseOffset comes from partitionInfo.offsets[0]
    Value baseOffset =
        partitionInfo.offsets.empty() ? zero : partitionInfo.offsets.front();
    Value localRow =
        baseOffset
            ? builder.create<arith::SubIOp>(userLoc, globalRow, baseOffset)
            : globalRow;

    Value ownedIdx = localRow;

    /// Build full index lists by replacing the partitioned dimension.
    auto buildAccessIndices = [&](Value rowIdx) {
      SmallVector<Value> out;
      out.reserve(indices.size());
      if (stride) {
        /// Linearized: re-linearize with rowIdx and extracted column.
        Value col = builder.create<arith::RemUIOp>(userLoc, indices[0], stride);
        Value linear = builder.create<arith::MulIOp>(userLoc, rowIdx, stride);
        linear = builder.create<arith::AddIOp>(userLoc, linear, col);
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
        if (ValueUtils::dependsOn(globalRow, loop.getInductionVar())) {
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
                  builder.create<arith::ConstantIndexOp>(rowLoc, rowOffset);
              rowIdx = builder.create<arith::AddIOp>(rowLoc, rowIV, offVal);
            }

            Value localRowHoisted = rowIdx;
            if (!includesBaseOffset && baseOffset) {
              localRowHoisted =
                  builder.create<arith::SubIOp>(rowLoc, rowIdx, baseOffset);
            }

            RowSelection selection =
                buildSelection(builder, rowLoc, localRowHoisted);
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
          Block *rowDefBlock =
              globalRow.isa<BlockArgument>()
                  ? globalRow.cast<BlockArgument>().getOwner()
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
            if (ValueUtils::dependsOn(globalRow, loop.getInductionVar()))
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
              baseOffset ? builder.create<arith::SubIOp>(userLoc, globalRow,
                                                         baseOffset)
                         : globalRow;

          RowSelection selection =
              buildSelection(builder, userLoc, localRowHoisted);
          selectedMemref = selection.memref;
          selectedRowIdx = selection.rowIdx;
        }

        if (selectedMemref && selectedRowIdx)
          selectionCache[globalRow] = {selectedMemref, selectedRowIdx};
      }

      SmallVector<Value> selectedIndices = buildAccessIndices(selectedRowIdx);
      auto selectedLoad = builder.create<memref::LoadOp>(
          userLoc, selectedMemref, selectedIndices);

      auto load = cast<memref::LoadOp>(user);
      load.replaceAllUsesWith(selectedLoad.getResult());
      opsToRemove.insert(load);

    } else {
      /// STORE: Only store to owned buffer (halos are read-only)
      Value clampedOwnedIdx = clampIndex(ownedIdx, blockSize, builder, userLoc);
      SmallVector<Value> ownedIndices = buildAccessIndices(clampedOwnedIdx);

      builder.create<memref::StoreOp>(userLoc, storeValue, ownedMemref,
                                      ownedIndices);
      opsToRemove.insert(user);
    }
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
