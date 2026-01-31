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
#include "polygeist/Ops.h"

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
    unsigned partitionDim = pickPartitionDim(indices, anchor);
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

    /// 3-BUFFER SELECTION MODE (BASE-OFFSET SEMANTICS)
    /// Determine which region localRow falls into and select correct buffer
    ///
    /// Region boundaries (base-offset semantics uses SIGNED comparison):
    /// - Left halo: localRow < 0 (negative indices)
    /// - Owned: 0 <= localRow < blockSize
    /// - Right halo: localRow >= blockSize
    Value isLeftHalo = builder.create<arith::CmpIOp>(
        userLoc, arith::CmpIPredicate::slt, localRow, zero);

    /// Compute buffer-local indices for each region (base-offset semantics):
    /// Left halo: index = localRow + haloLeft (shift negative to positive)
    /// Owned: index = localRow (0-based within owned chunk)
    /// Right halo: index = localRow - blockSize (0-based within right halo)

    Value leftIdx = builder.create<arith::AddIOp>(userLoc, localRow, haloLeft);
    Value ownedIdx = localRow;
    Value rightIdx =
        builder.create<arith::SubIOp>(userLoc, localRow, blockSize);

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
      /// LOAD: Select buffer+row index, then perform a single load.
      /// This avoids per-element scf.if regions and redundant loads.

      Value clampedOwnedIdx = clampIndex(ownedIdx, blockSize, builder, userLoc);
      Value clampedLeftIdx =
          leftMemref ? clampIndex(leftIdx, haloLeft, builder, userLoc)
                     : Value();
      Value clampedRightIdx =
          rightMemref ? clampIndex(rightIdx, haloRight, builder, userLoc)
                      : Value();

      Value selectedMemref = ownedMemref;
      Value selectedRowIdx = clampedOwnedIdx;

      /// Select left halo if needed (and valid), else owned.
      if (leftMemref) {
        Value safeLeftMemref =
            leftPtrNotNull
                ? builder.create<arith::SelectOp>(userLoc, leftPtrNotNull,
                                                  leftMemref, ownedMemref)
                : leftMemref;
        Value safeLeftIdx =
            leftPtrNotNull
                ? builder.create<arith::SelectOp>(
                      userLoc, leftPtrNotNull, clampedLeftIdx, clampedOwnedIdx)
                : clampedLeftIdx;
        selectedMemref = builder.create<arith::SelectOp>(
            userLoc, isLeftHalo, safeLeftMemref, selectedMemref);
        selectedRowIdx = builder.create<arith::SelectOp>(
            userLoc, isLeftHalo, safeLeftIdx, selectedRowIdx);
      }

      /// Select right halo if needed (and valid), else current selection.
      if (rightMemref) {
        Value safeRightMemref =
            rightPtrNotNull
                ? builder.create<arith::SelectOp>(userLoc, rightPtrNotNull,
                                                  rightMemref, ownedMemref)
                : rightMemref;
        Value safeRightIdx = rightPtrNotNull
                                 ? builder.create<arith::SelectOp>(
                                       userLoc, rightPtrNotNull,
                                       clampedRightIdx, clampedOwnedIdx)
                                 : clampedRightIdx;

        Value isRightHalo = builder.create<arith::CmpIOp>(
            userLoc, arith::CmpIPredicate::sge, localRow, blockSize);

        selectedMemref = builder.create<arith::SelectOp>(
            userLoc, isRightHalo, safeRightMemref, selectedMemref);
        selectedRowIdx = builder.create<arith::SelectOp>(
            userLoc, isRightHalo, safeRightIdx, selectedRowIdx);
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
