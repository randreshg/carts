///==========================================================================///
/// File: DbLowering.cpp
///
/// Ensures datablock (DB) allocations use heap-resident handles.
/// - Stack-allocated DB sources are replaced with heap allocations while
///   preserving the typed descriptor structure.
/// - Heap/global sources are left typed but recreated so downstream users
///   see a uniform heap-based handle.
/// Pointer-result uses are updated; GUID uses remain unchanged.
/// db_acquire/release ops are recreated with the new handles.
///==========================================================================///
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/IdRegistry.h"
#include "arts/Utils/OperationAttributes.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Ops.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>

ARTS_DEBUG_SETUP(db_lowering);
using namespace mlir;
using namespace arts;
namespace {
struct DbLoweringPass : public arts::DbLoweringBase<DbLoweringPass> {
  DbLoweringPass(uint64_t idStride = IdRegistry::DefaultStride)
      : idStride(idStride) {}
  ~DbLoweringPass() override { delete AC; }
  void runOnOperation() override;

private:
  /// Processes all DB allocation ops, converting to opaque pointers.
  void convertDbAllocOps();
  /// Updates all users of the old DB alloc to use the new one.
  void updateAllocUsers(DbAllocOp oldAllocOp, DbAllocOp newAllocOp);
  /// Processes a single DbAcquireOp and its nested acquire operations.
  void updateAcquireUsers(DbAcquireOp acquireOp, Value newGuid, Value newPtr,
                          SmallVector<Value> elementSizes);
  /// Helper function to get LLVM pointer from a base value and indices.
  Value getLLVMPtr(Value base, ValueRange opIndices, Location loc);

private:
  uint64_t idStride = IdRegistry::DefaultStride;
  SetVector<Operation *> opsToRemove;
  ModuleOp module;
  ArtsCodegen *AC = nullptr;
  IdRegistry idRegistry;
};
} // namespace

///===----------------------------------------------------------------------===///
/// Lower datablock allocations to use opaque pointers
///===----------------------------------------------------------------------===///
void DbLoweringPass::runOnOperation() {
  module = getOperation();
  AC = new ArtsCodegen(module, false);
  ARTS_INFO_HEADER(DbLowering);
  ARTS_DEBUG_REGION(module.dump(););
  convertDbAllocOps();
  OpRemovalManager removalMgr;
  for (Operation *op : opsToRemove)
    removalMgr.markForRemoval(op);
  removalMgr.removeAllMarked(module, /*recursive=*/false);
  ARTS_INFO_FOOTER(DbLoweringPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Convert all DB allocation operations to use opaque pointers
///===----------------------------------------------------------------------===///
void DbLoweringPass::convertDbAllocOps() {
  SmallVector<DbAllocOp, 8> dbAllocOps;
  module.walk([&](arts::DbAllocOp allocOp) { dbAllocOps.push_back(allocOp); });
  ARTS_INFO("Found " << dbAllocOps.size()
                     << " DB allocation operations to lower");
  if (dbAllocOps.empty())
    return;

  for (auto oldOp : dbAllocOps) {
    ARTS_DEBUG("DbAllocOp: " << oldOp);
    AC->setInsertionPointAfter(oldOp);
    auto oldPtrType = llvm::dyn_cast<MemRefType>(oldOp.getPtr().getType());
    if (!oldPtrType) {
      ARTS_DEBUG("  - Skipping non-memref pointer result");
      continue;
    }

    const bool alreadyOpaque =
        oldPtrType.getElementType().isa<LLVM::LLVMPointerType>();
    const bool alreadyHeap = oldOp.getAllocType() == DbAllocType::heap;
    if (alreadyOpaque && alreadyHeap) {
      ARTS_DEBUG("  - Allocation already lowered; skipping");
      continue;
    }

    /// Create new DbAllocOp with opaque pointer element type
    SmallVector<Value> sizes(oldOp.getSizes().begin(), oldOp.getSizes().end());
    SmallVector<Value> elementSizes(oldOp.getElementSizes().begin(),
                                    oldOp.getElementSizes().end());
    MemRefType allocatedElementType = oldOp.getAllocatedElementType();
    Type elementType = LLVM::LLVMPointerType::get(
        AC->getContext(), allocatedElementType.getMemorySpaceAsInt());
    SmallVector<int64_t> shape;
    shape.assign(sizes.size(), ShapedType::kDynamic);
    auto ptrType = MemRefType::get(shape, elementType);
    DbAllocOp newOp = AC->create<DbAllocOp>(
        oldOp.getLoc(), oldOp.getMode(), oldOp.getRoute(), DbAllocType::heap,
        oldOp.getDbMode(), oldOp.getElementType(), ptrType, sizes,
        elementSizes);
    ARTS_DEBUG("  - New DbAllocOp: " << newOp);
    for (auto &attr : oldOp->getAttrs()) {
      if (!attr.getName().getValue().starts_with("arts."))
        continue;
      if (attr.getName() == AttrNames::Operation::ArtsCreateId)
        continue;
      newOp->setAttr(attr.getName(), attr.getValue());
    }
    int64_t baseId = 0;
    if (auto idAttr =
            oldOp->getAttrOfType<IntegerAttr>(AttrNames::Operation::ArtsId))
      baseId = idAttr.getInt();
    else
      baseId = idRegistry.getOrCreate(oldOp.getOperation());

    /// Set the create id for the new operation - We multiply the base id by the
    /// stride to get the create id
    if (baseId) {
      int64_t createId = baseId * static_cast<int64_t>(idStride);
      newOp->setAttr(AttrNames::Operation::ArtsCreateId,
                     AC->getBuilder().getI64IntegerAttr(createId));
      ARTS_DEBUG("  - DB arts.create_id=" << createId << " (base=" << baseId
                                          << " x stride=" << idStride << ")");
    }
    updateAllocUsers(oldOp, newOp);
    opsToRemove.insert(oldOp);
  }
}

///===----------------------------------------------------------------------===///
/// Update all users of the old allocation to use the new lowered allocation
///===----------------------------------------------------------------------===///
void DbLoweringPass::updateAllocUsers(DbAllocOp oldAllocOp,
                                      DbAllocOp newAllocOp) {
  /// Replace all uses of the old GUID with the new GUID.
  Value oldGuid = oldAllocOp.getGuid();
  Value newGuid = newAllocOp.getGuid();
  assert(oldGuid && newGuid && "expected guid values");
  oldGuid.replaceAllUsesWith(newGuid);

  /// Replace all uses of the old pointer with the new pointer.
  Value oldPtr = oldAllocOp.getPtr();
  Value newPtr = newAllocOp.getPtr();
  assert(oldPtr && newPtr && "expected pointer values");

  /// Get element sizes
  SmallVector<Value> elementSizes(newAllocOp.getElementSizes().begin(),
                                  newAllocOp.getElementSizes().end());

  /// Process each use of the old pointer
  for (auto &use : llvm::make_early_inc_range(oldPtr.getUses())) {
    Operation *userOp = use.getOwner();
    AC->setInsertionPoint(userOp);

    /// DbRef Ops iterate over the outer sizes of the DBAllocOp.
    if (auto dbRefOp = dyn_cast<DbRefOp>(userOp)) {
      /// Extract the !llvm.ptr from newPtr using db_gep at dbRefOp location
      SmallVector<Value> dbRefIndices(dbRefOp.getIndices().begin(),
                                      dbRefOp.getIndices().end());
      Value llvmPtr = getLLVMPtr(newPtr, dbRefIndices, dbRefOp.getLoc());
      auto loadedLlvmPtr = AC->create<LLVM::LoadOp>(dbRefOp.getLoc(),
                                                    llvmPtr.getType(), llvmPtr);
      /// Cast the !llvm.ptr to the appropriate memref type at dbRefOp location
      auto originalMemrefType =
          dbRefOp.getResult().getType().cast<MemRefType>();
      Value castedMemref = AC->create<polygeist::Pointer2MemrefOp>(
          dbRefOp.getLoc(), originalMemrefType, loadedLlvmPtr);

      /// Rewrite uses of the DbRefOp so we propagate the element sizes
      for (auto &dbRefUse :
           llvm::make_early_inc_range(dbRefOp.getResult().getUses())) {
        Operation *userOp = dbRefUse.getOwner();
        AC->setInsertionPoint(userOp);

        if (auto loadOp = dyn_cast<memref::LoadOp>(userOp)) {
          SmallVector<Value> indices(loadOp.getIndices().begin(),
                                     loadOp.getIndices().end());
          auto dynLoad = AC->create<polygeist::DynLoadOp>(
              loadOp.getLoc(), loadOp.getResult().getType(), castedMemref,
              indices, elementSizes);
          loadOp.getResult().replaceAllUsesWith(dynLoad.getResult());
          opsToRemove.insert(loadOp);
        } else if (auto storeOp = dyn_cast<memref::StoreOp>(userOp)) {
          SmallVector<Value> indices(storeOp.getIndices().begin(),
                                     storeOp.getIndices().end());
          AC->create<polygeist::DynStoreOp>(
              storeOp.getLoc(), storeOp.getValueToStore(), castedMemref,
              indices, elementSizes);
          opsToRemove.insert(storeOp);
        } else {
          /// For other users, replace with the casted memref
          dbRefUse.set(castedMemref);
        }
      }
      opsToRemove.insert(dbRefOp);
      continue;
    }

    if (auto loadOp = dyn_cast<memref::LoadOp>(userOp)) {
      Value llvmPtr = getLLVMPtr(newPtr, loadOp.getIndices(), loadOp.getLoc());
      auto newLoad = AC->create<LLVM::LoadOp>(
          loadOp.getLoc(), loadOp.getResult().getType(), llvmPtr);
      loadOp.getResult().replaceAllUsesWith(newLoad.getResult());
      opsToRemove.insert(loadOp);
      continue;
    }

    if (auto storeOp = dyn_cast<memref::StoreOp>(userOp)) {
      Value llvmPtr =
          getLLVMPtr(newPtr, storeOp.getIndices(), storeOp.getLoc());
      AC->create<LLVM::StoreOp>(storeOp.getLoc(), storeOp.getValueToStore(),
                                llvmPtr);
      opsToRemove.insert(storeOp);
      continue;
    }

    if (auto acquireOp = dyn_cast<DbAcquireOp>(userOp)) {
      updateAcquireUsers(acquireOp, newGuid, newPtr, elementSizes);
      continue;
    }

    if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(userOp)) {
      Value llvmPtr = AC->castToLLVMPtr(newPtr, m2p.getLoc());
      m2p.getResult().replaceAllUsesWith(llvmPtr);
      opsToRemove.insert(m2p);
      continue;
    }

    if (auto freeOp = dyn_cast<DbFreeOp>(userOp)) {
      auto newFree = AC->create<DbFreeOp>(freeOp.getLoc(), newPtr);
      freeOp->replaceAllUsesWith(newFree);
      opsToRemove.insert(freeOp);
      continue;
    }
  }
}

///===----------------------------------------------------------------------===///
/// Process a single DbAcquireOp and its nested acquire operations
///===----------------------------------------------------------------------===///
void DbLoweringPass::updateAcquireUsers(DbAcquireOp acquireOp, Value newGuid,
                                        Value newPtr,
                                        SmallVector<Value> elementSizes) {
  /// Preserve the absence of a source GUID for nested acquires that only
  /// thread the pointer through EDT block arguments.
  Value oldSourceGuid = acquireOp.getSourceGuid();
  Value sourceGuid = oldSourceGuid ? newGuid : Value();
  Value sourcePtr = newPtr ? newPtr : acquireOp.getPtr();

  SmallVector<Value> indices(acquireOp.getIndices().begin(),
                             acquireOp.getIndices().end());
  SmallVector<Value> offsets(acquireOp.getOffsets().begin(),
                             acquireOp.getOffsets().end());
  SmallVector<Value> sizes(acquireOp.getSizes().begin(),
                           acquireOp.getSizes().end());
  SmallVector<Value> offsetHints(acquireOp.getOffsetHints().begin(),
                                 acquireOp.getOffsetHints().end());
  SmallVector<Value> sizeHints(acquireOp.getSizeHints().begin(),
                               acquireOp.getSizeHints().end());
  auto newAcquireOp = AC->create<DbAcquireOp>(
      acquireOp.getLoc(), acquireOp.getMode(), sourceGuid, sourcePtr, indices,
      offsets, sizes, offsetHints, sizeHints);
  for (auto &attr : acquireOp->getAttrs()) {
    if (attr.getName().getValue().starts_with("arts."))
      newAcquireOp->setAttr(attr.getName(), attr.getValue());
  }
  ARTS_DEBUG("  - New DbAcquireOp: " << newAcquireOp);

  auto rewriteBlockUses = [&](Value base, Value replacementBase) {
    for (auto &blockUse : llvm::make_early_inc_range(base.getUses())) {
      Operation *blockUserOp = blockUse.getOwner();
      AC->setInsertionPoint(blockUserOp);

      if (auto dbRefOp = dyn_cast<DbRefOp>(blockUserOp)) {
        SmallVector<Value> dbRefIndices(dbRefOp.getIndices().begin(),
                                        dbRefOp.getIndices().end());
        Value llvmPtr =
            getLLVMPtr(replacementBase, dbRefIndices, dbRefOp.getLoc());
        auto loadedLlvmPtr = AC->create<LLVM::LoadOp>(
            dbRefOp.getLoc(), llvmPtr.getType(), llvmPtr);
        auto originalMemrefType =
            dbRefOp.getResult().getType().cast<MemRefType>();
        Value castedMemref = AC->create<polygeist::Pointer2MemrefOp>(
            dbRefOp.getLoc(), originalMemrefType, loadedLlvmPtr);

        /// Propagate the element sizes to the uses of the DbRefOp
        for (auto &dbRefUse :
             llvm::make_early_inc_range(dbRefOp.getResult().getUses())) {
          Operation *userOp = dbRefUse.getOwner();
          AC->setInsertionPoint(userOp);

          if (auto loadOp = dyn_cast<memref::LoadOp>(userOp)) {
            SmallVector<Value> loadIndices(loadOp.getIndices().begin(),
                                           loadOp.getIndices().end());
            auto dynLoad = AC->create<polygeist::DynLoadOp>(
                loadOp.getLoc(), loadOp.getResult().getType(), castedMemref,
                loadIndices, elementSizes);
            loadOp.getResult().replaceAllUsesWith(dynLoad.getResult());
            opsToRemove.insert(loadOp);
          } else if (auto storeOp = dyn_cast<memref::StoreOp>(userOp)) {
            SmallVector<Value> storeIndices(storeOp.getIndices().begin(),
                                            storeOp.getIndices().end());
            AC->create<polygeist::DynStoreOp>(
                storeOp.getLoc(), storeOp.getValueToStore(), castedMemref,
                storeIndices, elementSizes);
            opsToRemove.insert(storeOp);
          } else {
            /// For other users, replace with the casted memref
            dbRefUse.set(castedMemref);
          }
        }
        opsToRemove.insert(dbRefOp);
        continue;
      }

      if (auto loadOp = dyn_cast<memref::LoadOp>(blockUserOp)) {
        Value llvmPtr =
            getLLVMPtr(replacementBase, loadOp.getIndices(), loadOp.getLoc());
        auto newLoad = AC->create<LLVM::LoadOp>(
            loadOp.getLoc(), loadOp.getResult().getType(), llvmPtr);
        loadOp.getResult().replaceAllUsesWith(newLoad.getResult());
        opsToRemove.insert(loadOp);
        continue;
      }

      if (auto storeOp = dyn_cast<memref::StoreOp>(blockUserOp)) {
        Value llvmPtr =
            getLLVMPtr(replacementBase, storeOp.getIndices(), storeOp.getLoc());
        AC->create<LLVM::StoreOp>(storeOp.getLoc(), storeOp.getValueToStore(),
                                  llvmPtr);
        opsToRemove.insert(storeOp);
        continue;
      }

      if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(blockUserOp)) {
        Value llvmPtr = AC->castToLLVMPtr(replacementBase, m2p.getLoc());
        m2p.getResult().replaceAllUsesWith(llvmPtr);
        opsToRemove.insert(m2p);
        continue;
      }

      if (auto nestedAcquireOp = dyn_cast<DbAcquireOp>(blockUserOp)) {
        /// Recursively process nested acquire operations
        updateAcquireUsers(nestedAcquireOp, newGuid, replacementBase,
                           elementSizes);
        continue;
      }

      if (auto releaseOp = dyn_cast<DbReleaseOp>(blockUserOp)) {
        opsToRemove.insert(releaseOp);
        continue;
      }
    }
  };

  /// Find the EDT that uses this acquire op and the corresponding block
  /// argument
  auto [edtUser, blockArg] = arts::getEdtBlockArgumentForAcquire(acquireOp);
  if (!edtUser || !blockArg) {
    ARTS_DEBUG("  - Acquire has no EDT consumer; replacing uses directly");
    rewriteBlockUses(acquireOp.getPtr(), newPtr ? newPtr : acquireOp.getPtr());
    acquireOp->replaceAllUsesWith(newAcquireOp);
    opsToRemove.insert(acquireOp);
    return;
  }

  /// Update the block argument type to match the new lowered type
  blockArg.setType(newPtr.getType());

  /// Handle the uses of the block argument within the EDT
  rewriteBlockUses(blockArg, blockArg);

  /// Replace the old acquire op with the new one
  acquireOp->replaceAllUsesWith(newAcquireOp);
  opsToRemove.insert(acquireOp);
}

///===----------------------------------------------------------------------===///
/// Helper function to get LLVM pointer from a base value and indices
///===----------------------------------------------------------------------===///
Value DbLoweringPass::getLLVMPtr(Value base, ValueRange opIndices,
                                 Location loc) {
  if (opIndices.empty())
    return AC->castToLLVMPtr(base, loc);

  SmallVector<Value> sizes = getSizesFromDb(base);
  SmallVector<Value> strides = AC->computeStridesFromSizes(sizes, loc);
  SmallVector<Value> indices(opIndices.begin(), opIndices.end());
  return AC->create<DbGepOp>(loc, AC->getLLVMPointerType(base), base, indices,
                             strides);
}

///===----------------------------------------------------------------------===///
// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbLoweringPass() {
  return std::make_unique<DbLoweringPass>();
}

std::unique_ptr<Pass> createDbLoweringPass(uint64_t idStride) {
  return std::make_unique<DbLoweringPass>(idStride);
}
} // namespace arts
} // namespace mlir
