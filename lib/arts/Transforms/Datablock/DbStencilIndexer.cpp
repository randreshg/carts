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

#include "arts/Transforms/Datablock/DbStencilIndexer.h"
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

DbStencilIndexer::DbStencilIndexer(Value haloLeft, Value haloRight,
                                   Value chunkSize, unsigned outerRank,
                                   unsigned innerRank, Value elemOffset,
                                   Value ownedArg, Value leftHaloArg,
                                   Value rightHaloArg)
    : DbIndexerBase(outerRank, innerRank), elemOffset_(elemOffset),
      haloLeft_(haloLeft), haloRight_(haloRight), chunkSize_(chunkSize),
      ownedArg_(ownedArg), leftHaloArg_(leftHaloArg),
      rightHaloArg_(rightHaloArg) {}

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

/// Pick the index dimension that tracks the partition offset (defaults to 0).
static unsigned pickPartitionDim(ValueRange indices, Value elemOffset) {
  if (!elemOffset || indices.empty())
    return 0;
  for (unsigned i = 0; i < indices.size(); ++i) {
    if (ValueUtils::dependsOn(indices[i], elemOffset))
      return i;
  }
  return 0;
}

//===----------------------------------------------------------------------===//
// Index Localization (interface methods - stencil uses 3-buffer mode instead)
//===----------------------------------------------------------------------===//

LocalizedIndices DbStencilIndexer::localize(ArrayRef<Value>, OpBuilder &,
                                            Location) {
  llvm_unreachable("DbStencilIndexer uses 3-buffer mode, not localize()");
}

LocalizedIndices DbStencilIndexer::localizeLinearized(Value, Value, OpBuilder &,
                                                      Location) {
  llvm_unreachable("DbStencilIndexer uses 3-buffer mode, not localizeLinearized()");
}

//===----------------------------------------------------------------------===//
// Operation Rewriting with 3-Buffer Selection
//===----------------------------------------------------------------------===//

void DbStencilIndexer::transformDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {

  /// Stencil indexer requires 3-buffer mode (owned + left/right halos)
  bool has3BufferMode = leftHaloArg_ || rightHaloArg_;
  assert(has3BufferMode && "DbStencilIndexer requires halo arguments");

  ARTS_DEBUG("DbStencilIndexer::transformDbRefUsers (3-buffer mode)");

  SmallVector<Operation *> users(ref.getResult().getUsers().begin(),
                                 ref.getResult().getUsers().end());

  if (users.empty()) {
    opsToRemove.insert(ref.getOperation());
    return;
  }

  /// Insert DbRefOps at the START of EDT block to ensure dominance for all users.
  /// Users can be in different blocks (loop iterations, branches), so inserting
  /// at users.front() would cause SSA dominance errors.
  Block *edtBlock = ownedArg_.cast<BlockArgument>().getOwner();
  Location loc = edtBlock->front().getLoc();
  OpBuilder::InsertionGuard topGuard(builder);
  builder.setInsertionPointToStart(edtBlock);

  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);

  /// Create 3 DbRefOps ONCE at the start, reuse for all accesses
  assert(haloLeft_ && chunkSize_ && "Stencil mode requires haloLeft and chunkSize");

  Value ownedMemref, leftMemref, rightMemref;

  /// Create owned memref (always needed)
  auto ownedRef = builder.create<DbRefOp>(loc, newElementType, ownedArg_,
                                          SmallVector<Value>{zero});
  ownedMemref = ownedRef.getResult();

  /// Create left halo memref if available (null at left boundary)
  if (leftHaloArg_) {
    auto leftRef = builder.create<DbRefOp>(loc, newElementType, leftHaloArg_,
                                           SmallVector<Value>{zero});
    leftMemref = leftRef.getResult();
  }

  /// Create right halo memref if available (null at right boundary)
  if (rightHaloArg_) {
    auto rightRef = builder.create<DbRefOp>(loc, newElementType, rightHaloArg_,
                                            SmallVector<Value>{zero});
    rightMemref = rightRef.getResult();
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
    unsigned partitionDim = pickPartitionDim(indices, elemOffset_);
    Value globalRow = indices[partitionDim];

    /// Handle linearized access: extract row from linear index
    Value stride = getLinearizedStride(indices, newElementType, builder, userLoc);
    if (stride) {
      globalRow = builder.create<arith::DivUIOp>(userLoc, indices[0], stride);
    }

    /// Compute localRow = globalRow - elemOffset
    Value localRow =
        elemOffset_ ? builder.create<arith::SubIOp>(userLoc, globalRow, elemOffset_)
                    : globalRow;

    /// 3-BUFFER SELECTION MODE
    /// Determine which region localRow falls into and select correct buffer
    ///
    /// Region boundaries:
      /// - Left halo: localRow < haloLeft
      /// - Owned: haloLeft <= localRow < haloLeft + chunkSize
      /// - Right halo: localRow >= haloLeft + chunkSize
      Value isLeftHalo = builder.create<arith::CmpIOp>(
          userLoc, arith::CmpIPredicate::slt, localRow, haloLeft_);

      Value haloLeftPlusChunk =
          builder.create<arith::AddIOp>(userLoc, haloLeft_, chunkSize_);

      /// Compute buffer-local indices for each region:
      /// Left halo: index = localRow (clamped)
      /// Owned: index = localRow - haloLeft
      /// Right halo: index = localRow - haloLeft - chunkSize

      Value leftIdx = localRow;
      Value ownedIdx = builder.create<arith::SubIOp>(userLoc, localRow, haloLeft_);
      Value rightIdx =
          builder.create<arith::SubIOp>(userLoc, localRow, haloLeftPlusChunk);

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
        /// LOAD: Generate loads from all available buffers, then select
        /// IMPORTANT: Use conditional execution (scf.if) to avoid loading from
        /// NULL pointers for boundary workers.

        /// Load from owned buffer (always valid)
        Value clampedOwnedIdx = clampIndex(ownedIdx, chunkSize_, builder, userLoc);
        SmallVector<Value> ownedIndices = buildAccessIndices(clampedOwnedIdx);

        auto ownedLoad =
            builder.create<memref::LoadOp>(userLoc, ownedMemref, ownedIndices);
        Value result = ownedLoad.getResult();

        /// Helper: Get LLVM pointer type for null pointer comparison
        auto llvmPtrTy = LLVM::LLVMPointerType::get(builder.getContext());

        /// Load from left halo if available (with null pointer guard)
        if (leftMemref) {
          /// Check if left halo buffer pointer is null (boundary workers)
          /// Convert memref to llvm.ptr, then compare with null
          Value leftPtr = builder.create<polygeist::Memref2PointerOp>(
              userLoc, llvmPtrTy, leftMemref);
          Value nullPtr = builder.create<LLVM::ZeroOp>(userLoc, llvmPtrTy);
          Value leftPtrNotNull = builder.create<LLVM::ICmpOp>(
              userLoc, LLVM::ICmpPredicate::ne, leftPtr, nullPtr);

          /// Conditionally load from left halo (avoid null pointer dereference)
          auto leftIfOp = builder.create<scf::IfOp>(
              userLoc, result.getType(), leftPtrNotNull,
              /*withElseRegion=*/true);

          /// Then block: load from left halo
          {
            OpBuilder::InsertionGuard g(builder);
            builder.setInsertionPointToStart(&leftIfOp.getThenRegion().front());
            Value clampedLeftIdx =
                clampIndex(leftIdx, haloLeft_, builder, userLoc);
            SmallVector<Value> leftIndices = buildAccessIndices(clampedLeftIdx);
            auto leftLoad =
                builder.create<memref::LoadOp>(userLoc, leftMemref, leftIndices);
            builder.create<scf::YieldOp>(userLoc, leftLoad.getResult());
          }

          /// Else block: use owned value as fallback
          {
            OpBuilder::InsertionGuard g(builder);
            builder.setInsertionPointToStart(&leftIfOp.getElseRegion().front());
            builder.create<scf::YieldOp>(userLoc, result);
          }

          /// Select: if isLeftHalo, use conditional result, else use owned
          Value safeLeftVal = leftIfOp.getResult(0);
          result = builder.create<arith::SelectOp>(userLoc, isLeftHalo,
                                                   safeLeftVal, result);
        }

        /// Load from right halo if available (with null pointer guard)
        if (rightMemref) {
          /// Check if right halo buffer pointer is null (boundary workers)
          /// Convert memref to llvm.ptr, then compare with null
          Value rightPtr = builder.create<polygeist::Memref2PointerOp>(
              userLoc, llvmPtrTy, rightMemref);
          Value nullPtrRight = builder.create<LLVM::ZeroOp>(userLoc, llvmPtrTy);
          Value rightPtrNotNull = builder.create<LLVM::ICmpOp>(
              userLoc, LLVM::ICmpPredicate::ne, rightPtr, nullPtrRight);

          /// Conditionally load from right halo (avoid null pointer dereference)
          auto rightIfOp = builder.create<scf::IfOp>(
              userLoc, result.getType(), rightPtrNotNull,
              /*withElseRegion=*/true);

          /// Then block: load from right halo
          {
            OpBuilder::InsertionGuard g(builder);
            builder.setInsertionPointToStart(&rightIfOp.getThenRegion().front());
            Value clampedRightIdx =
                clampIndex(rightIdx, haloRight_, builder, userLoc);
            SmallVector<Value> rightIndices = buildAccessIndices(clampedRightIdx);
            auto rightLoad = builder.create<memref::LoadOp>(userLoc, rightMemref,
                                                            rightIndices);
            builder.create<scf::YieldOp>(userLoc, rightLoad.getResult());
          }

          /// Else block: use current result as fallback
          {
            OpBuilder::InsertionGuard g(builder);
            builder.setInsertionPointToStart(&rightIfOp.getElseRegion().front());
            builder.create<scf::YieldOp>(userLoc, result);
          }

          /// isRightHalo = localRow >= haloLeftPlusChunk (beyond owned region)
          Value isRightHalo = builder.create<arith::CmpIOp>(
              userLoc, arith::CmpIPredicate::sge, localRow, haloLeftPlusChunk);

          /// Select: if isRightHalo, use conditional result, else use current
          Value safeRightVal = rightIfOp.getResult(0);
          result = builder.create<arith::SelectOp>(userLoc, isRightHalo,
                                                   safeRightVal, result);
        }

        auto load = cast<memref::LoadOp>(user);
        load.replaceAllUsesWith(result);
        opsToRemove.insert(load);

      } else {
        /// STORE: Only store to owned buffer (halos are read-only)
        Value clampedOwnedIdx = clampIndex(ownedIdx, chunkSize_, builder, userLoc);
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
      llvm_unreachable("DbStencilIndexer: direct load/store not supported, "
                       "use DbRefOp for 3-buffer mode");
  }
}
