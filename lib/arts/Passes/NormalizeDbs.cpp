//===----------------------------------------------------------------------===//
// NormalizeDbs.cpp - Normalize DataBlock (DB) handle types prior to lowering:
///
/// - If the DB source was stack-allocated, create a replacement arts.db_alloc
///   whose pointer result is heap-resident and opaque (memref<memref<?xi8>>),
///   and rewire pointer-result uses to the new handle.
/// - If the source was heap/global, preserve shape and make the pointer opaque
///   (element i8).
/// Only pointer-result uses are updated; GUID uses are preserved.
/// arts.db_acquire/release are recreated to match the new opaque types.
///
/// Design note:
/// - Datablocks live in heap memory. If a DB's source was stack-allocated,
///   we create a replacement DB allocation whose pointer result is a heap
///   pointer to the original memref (i.e., memref<memref<?xi8>>), effectively
///   migrating the DB handle to the heap and making it opaque in one step.
/// - If the source was already heap/global, we only make the pointer opaque
///   (element type becomes i8) and preserve the shape.
/// - In both cases, users of the DB pointer (EDTs, acquires/releases, loads/
///   stores) are rewired to the new opaque pointer result. GUID uses are left
///   unchanged.
//===----------------------------------------------------------------------===//

/// Dialects
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
/// Others
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"

/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(normalize_dbs);

using namespace mlir;
using namespace arts;

namespace {
struct NormalizeDbsPass : public arts::NormalizeDbsBase<NormalizeDbsPass> {
  ~NormalizeDbsPass() { delete AC; }

  void runOnOperation() override;

private:
  /// Process all DB allocation operations
  void convertDbAllocOps();

  /// Process all DB access operations (acquire/release)
  void convertDbAccessOps();

  /// Update all users of a DB operation
  void updateUsers(Operation *oldOp, Operation *newOp);

private:
  SetVector<Operation *> opsToRemove;
  ModuleOp module;
  ArtsCodegen *AC = nullptr;
};
} // namespace

void NormalizeDbsPass::runOnOperation() {
  module = getOperation();
  AC = new ArtsCodegen(module, false);

  ARTS_DEBUG_HEADER(NormalizeDbs);
  ARTS_DEBUG_REGION(module.dump(););

  /// Convert all DB operations to use opaque pointers
  convertDbAllocOps();
  convertDbAccessOps();

  /// Clean up old operations
  removeOps(module, AC->getBuilder(), opsToRemove);

  ARTS_DEBUG_FOOTER(NormalizeDbsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

/// Convert all DB allocation operations to use opaque pointers
void NormalizeDbsPass::convertDbAllocOps() {
  SmallVector<DbAllocOp, 8> oldAllocOps;
  DenseMap<DbAllocOp, DbAllocOp> allocOpMap;

  /// Walk through all DbAllocOps and create replacement versions
  module.walk([&](arts::DbAllocOp oldOp) -> mlir::WalkResult {
    if (oldOp->hasAttr("newDb"))
      return mlir::WalkResult::skip();

    AC->setInsertionPointAfter(oldOp);

    /// Create new DbAllocOp with opaque pointer element type (memref<?xi8>)
    auto route = AC->createIntConstant(0, AC->Int32, oldOp.getLoc());
    auto newOp = AC->create<DbAllocOp>(
        oldOp.getLoc(), oldOp.getMode(), route, oldOp.getAllocType(),
        oldOp.getDbMode(), AC->VoidPtr, oldOp.getAddress(), ValueRange{});

    /// Copy attributes and mark as processed
    newOp->setAttrs(oldOp->getAttrs());
    newOp->setAttr("newDb", AC->getBuilder().getUnitAttr());

    oldAllocOps.push_back(oldOp);
    allocOpMap[oldOp] = newOp;
    return mlir::WalkResult::advance();
  });

  if (allocOpMap.empty())
    return;

  /// Update all users and mark old operations for removal
  for (auto oldOp : oldAllocOps) {
    auto newOp = allocOpMap[oldOp];
    updateUsers(oldOp.getOperation(), newOp.getOperation());
    opsToRemove.insert(oldOp);
  }
}

/// Convert all DB access operations (acquire/release) to use opaque pointers
void NormalizeDbsPass::convertDbAccessOps() {
  SmallVector<Operation *, 8> oldAccessOps;
  DenseMap<Operation *, Operation *> accessOpMap;

  /// Walk through all DB access operations
  module.walk([&](Operation *op) -> mlir::WalkResult {
    if (op->hasAttr("newDb"))
      return mlir::WalkResult::skip();

    AC->setInsertionPointAfter(op);

    if (auto acquireOp = dyn_cast<DbAcquireOp>(op)) {
      /// Create new DbAcquireOp with opaque pointer type
      auto oldType = acquireOp.getPtr().getType().cast<MemRefType>();
      auto newType = MemRefType::get(oldType.getShape(), AC->VoidPtr);
      auto guidType = acquireOp.getGuid().getType();
      auto newOp = AC->create<DbAcquireOp>(
          acquireOp.getLoc(), TypeRange{guidType, newType}, acquireOp.getMode(),
          acquireOp.getSource(), acquireOp.getIndices(), acquireOp.getOffsets(),
          acquireOp.getSizes());

      newOp->setAttrs(acquireOp->getAttrs());
      newOp->setAttr("newDb", AC->getBuilder().getUnitAttr());

      oldAccessOps.push_back(acquireOp);
      accessOpMap[acquireOp] = newOp;

    } else if (auto releaseOp = dyn_cast<DbReleaseOp>(op)) {
      /// Create new DbReleaseOp
      auto newOp =
          AC->create<DbReleaseOp>(releaseOp.getLoc(), releaseOp.getSources());

      newOp->setAttrs(releaseOp->getAttrs());
      newOp->setAttr("newDb", AC->getBuilder().getUnitAttr());

      oldAccessOps.push_back(releaseOp);
      accessOpMap[releaseOp] = newOp;
    }

    return mlir::WalkResult::advance();
  });

  if (accessOpMap.empty())
    return;

  /// Update all users and mark old operations for removal
  for (auto oldOp : oldAccessOps) {
    auto newOp = accessOpMap[oldOp];
    updateUsers(oldOp, newOp);
    opsToRemove.insert(oldOp);
  }
}

/// Update all users of an old DB operation to use the new one
void NormalizeDbsPass::updateUsers(Operation *oldOp, Operation *newOp) {
  /// Helper to get the pointer value from a DB operation
  auto getPtrValue = [&](Operation *op) -> Value {
    if (auto allocOp = dyn_cast<DbAllocOp>(op))
      return allocOp.getPtr();
    if (auto acquireOp = dyn_cast<DbAcquireOp>(op))
      return acquireOp.getPtr();
    /// fallback
    return op->getResult(1);
  };

  /// Helper to get LLVM pointer, handling stack allocations specially
  auto getLLVMPtr = [&](Location loc, Operation *op) -> Value {
    Value ptrValue = getPtrValue(op);
    Value llvmPtr = AC->castToLLVMPtr(ptrValue, loc);

    /// For stack allocations, load the pointer value
    if (auto allocOp = dyn_cast<DbAllocOp>(op)) {
      if (allocOp.getAllocType() == DbAllocType::stack)
        return AC->create<LLVM::LoadOp>(loc, AC->llvmPtr, llvmPtr);
    }
    return llvmPtr;
  };

  /// Update all uses of the old operation
  for (auto &use : llvm::make_early_inc_range(oldOp->getUses())) {
    auto user = use.getOwner();
    AC->setInsertionPoint(user);

    /// Handle different types of users
    if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
      /// Convert memref load to LLVM load
      auto llvmPtr = getLLVMPtr(loadOp.getLoc(), newOp);
      auto newLoad = AC->create<LLVM::LoadOp>(
          loadOp.getLoc(), loadOp.getResult().getType(), llvmPtr);
      loadOp.getResult().replaceAllUsesWith(newLoad.getResult());
      opsToRemove.insert(loadOp);

    } else if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      /// Convert memref store to LLVM store
      auto llvmPtr = getLLVMPtr(storeOp.getLoc(), newOp);
      AC->create<LLVM::StoreOp>(storeOp.getLoc(), storeOp.getValueToStore(),
                                llvmPtr);
      opsToRemove.insert(storeOp);
    } else if (auto acquireUser = dyn_cast<DbAcquireOp>(user)) {
      Value newPtr = getPtrValue(newOp);
      use.set(newPtr);

    } else if (auto edtOp = dyn_cast<EdtOp>(user)) {
      Value newPtr = getPtrValue(newOp);
      Value oldPtr = getPtrValue(oldOp);
      if (use.get() == oldPtr)
        use.set(newPtr);

    } else if (isa<DbReleaseOp>(user)) {
      Value newPtr = getPtrValue(newOp);
      Value oldPtr = getPtrValue(oldOp);
      if (use.get() == oldPtr)
        use.set(newPtr);
    } else {
      /// Unknown user type - this should not happen
      user->emitError("Unsupported user of DB operation");
      ARTS_ERROR("Unknown use of DB op: " << *user);
    }
  }
}

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createNormalizeDbsPass() {
  return std::make_unique<NormalizeDbsPass>();
}
} // namespace arts
} // namespace mlir