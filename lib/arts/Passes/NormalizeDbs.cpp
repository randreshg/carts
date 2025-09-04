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
///   migrating the DB handle to the heap and making it opaque.
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

  /// Update all users of a DB allocation operation
  void updateUsers(DbAllocOp oldAllocOp, DbAllocOp newAllocOp);

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

  /// Convert all DB operations to use opaque pointers and update uses
  convertDbAllocOps();

  /// Clean up old operations
  removeOps(module, AC->getBuilder(), opsToRemove);

  ARTS_DEBUG_FOOTER(NormalizeDbsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

/// Convert all DB allocation operations to use opaque pointers
void NormalizeDbsPass::convertDbAllocOps() {
  SmallVector<DbAllocOp, 8> allocOps;

  /// Collect all DbAllocOps
  module.walk([&](arts::DbAllocOp allocOp) { allocOps.push_back(allocOp); });

  if (allocOps.empty())
    return;

  /// Process each DbAllocOp
  for (auto oldOp : allocOps) {
    AC->setInsertionPointAfter(oldOp);

    /// Convert stack-backed allocations to heap and make ptr opaque
    /// memref<?xi8>
    auto route = AC->createIntConstant(0, AC->Int32, oldOp.getLoc());

    SmallVector<Value> sizes(oldOp.getSizes().begin(), oldOp.getSizes().end());
    SmallVector<Value> payloadSizes(oldOp.getPayloadSizes().begin(),
                                    oldOp.getPayloadSizes().end());
    if (Value addr = oldOp.getAddress()) {
      if (auto mt = addr.getType().dyn_cast<MemRefType>()) {
        if (mt.getRank() == 0 && sizes.empty())
          sizes.push_back(AC->createIndexConstant(1, oldOp.getLoc()));
      }
    }

    DbAllocType newAllocType = oldOp.getAllocType();
    if (newAllocType == DbAllocType::stack)
      newAllocType = DbAllocType::heap;

    /// Build new DbAlloc controlling result shape: for scalar sources (rank 0)
    /// omit the address so the builder uses 'sizes' (1D dynamic) for the ptr.
    DbAllocOp newOp;
    if (Value addr = oldOp.getAddress()) {
      if (auto mt = addr.getType().dyn_cast<MemRefType>()) {
        if (mt.getRank() == 0) {
          newOp = AC->create<DbAllocOp>(
              oldOp.getLoc(), oldOp.getMode(), route, newAllocType,
              oldOp.getDbMode(), oldOp.getElementType(), sizes, payloadSizes);
        } else {
          newOp = AC->create<DbAllocOp>(oldOp.getLoc(), oldOp.getMode(), route,
                                        newAllocType, oldOp.getDbMode(),
                                        oldOp.getElementType(), addr, sizes,
                                        payloadSizes);
        }
      } else {
        newOp = AC->create<DbAllocOp>(
            oldOp.getLoc(), oldOp.getMode(), route, newAllocType,
            oldOp.getDbMode(), oldOp.getElementType(), sizes, payloadSizes);
      }
    } else {
      newOp = AC->create<DbAllocOp>(
          oldOp.getLoc(), oldOp.getMode(), route, newAllocType,
          oldOp.getDbMode(), oldOp.getElementType(), sizes, payloadSizes);
    }

    updateUsers(oldOp, newOp);
    opsToRemove.insert(oldOp);
  }
}

/// Update all users of an old DB operation to use the new one
void NormalizeDbsPass::updateUsers(DbAllocOp oldAllocOp, DbAllocOp newAllocOp) {
  bool isStackAlloc = oldAllocOp.getAllocType() == DbAllocType::stack;

  Value oldPtr = oldAllocOp.getPtr();
  Value newPtr = newAllocOp.getPtr();
  assert(oldPtr && newPtr && "expected pointer values");

  SmallVector<Value, 16> worklist;
  SmallPtrSet<Value, 16> visited;
  DenseMap<Value, bool> isStackAllocated;
  DenseMap<Value, Value> ptrSource;

  worklist.push_back(oldPtr);
  visited.insert(oldPtr);
  isStackAllocated[oldPtr] = isStackAlloc;
  ptrSource[oldPtr] = newPtr;

  SmallVector<std::pair<Block *, unsigned>, 8> blockArgsToErase;

  /// Helper to enqueue a value for BFS with its stack flag and source ptr
  auto enqueue = [&](Value v, bool stackFlag, Value src) {
    if (visited.insert(v).second) {
      worklist.push_back(v);
      isStackAllocated[v] = stackFlag;
      ptrSource[v] = src;
    }
  };

  auto computeLLVMPtrForMemrefUse = [&](Value ptrBase, ValueRange idx,
                                        Location loc) -> Value {
    Value targetPtr = ptrBase;
    if (!idx.empty())
      targetPtr =
          AC->create<DbSubIndexOp>(loc, targetPtr.getType(), targetPtr, idx)
              .getResult();

    auto it = isStackAllocated.find(ptrBase);
    bool stack = (it != isStackAllocated.end()) ? it->second : false;

    Value llvmPtr = AC->castToLLVMPtr(targetPtr, loc);
    if (stack)
      return AC->create<LLVM::LoadOp>(loc, AC->llvmPtr, llvmPtr);
    return llvmPtr;
  };

  auto rewriteLoad = [&](memref::LoadOp loadOp, Value basePtr) {
    Location loc = loadOp.getLoc();
    Value llvmPtr =
        computeLLVMPtrForMemrefUse(basePtr, loadOp.getIndices(), loc);
    auto newLoad =
        AC->create<LLVM::LoadOp>(loc, loadOp.getResult().getType(), llvmPtr);
    loadOp.getResult().replaceAllUsesWith(newLoad.getResult());
    opsToRemove.insert(loadOp);
  };

  auto rewriteStore = [&](memref::StoreOp storeOp, Value basePtr) {
    Location loc = storeOp.getLoc();
    Value llvmPtr =
        computeLLVMPtrForMemrefUse(basePtr, storeOp.getIndices(), loc);
    AC->create<LLVM::StoreOp>(loc, storeOp.getValueToStore(), llvmPtr);
    opsToRemove.insert(storeOp);
  };

  auto queueBlockArgFollow = [&](Operation *user, unsigned operandIdx,
                                 Value fromVal) {
    if (user->getNumRegions() == 0)
      return;

    for (Region &region : user->getRegions()) {
      if (region.empty())
        continue;
      Block &entry = region.front();

      /// Adjust operand index for EDT operations (skip route parameter)
      unsigned argIdx = operandIdx;
      if (auto edtOp = dyn_cast<EdtOp>(user)) {
        if (operandIdx == 0)
          continue;              /// route parameter
        argIdx = operandIdx - 1; /// dependencies start at 0 for block args
      }

      if (argIdx >= entry.getNumArguments())
        continue;

      Value oldArg = entry.getArgument(argIdx);
      Type desiredType = newPtr.getType();
      bool stackFlag = isStackAllocated.lookup(fromVal);

      if (oldArg.getType() != desiredType) {
        // Insert new argument with correct type and replace uses
        Value newArg =
            entry.insertArgument(argIdx, desiredType, oldArg.getLoc());
        Value shiftedOldArg = entry.getArgument(argIdx + 1);
        shiftedOldArg.replaceAllUsesWith(newArg);
        enqueue(newArg, stackFlag, newArg);
        blockArgsToErase.emplace_back(&entry, argIdx + 1);
      } else {
        enqueue(oldArg, stackFlag, oldArg);
      }
    }
  };

  while (!worklist.empty()) {
    Value cur = worklist.pop_back_val();
    for (auto &use : llvm::make_early_inc_range(cur.getUses())) {
      Operation *user = use.getOwner();
      AC->setInsertionPoint(user);

      if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
        if (loadOp.getMemref() == cur) {
          Value src = ptrSource.lookup(cur);
          assert(src && "expected pointer source for tracked value");
          rewriteLoad(loadOp, src);
        }
      } else if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
        if (storeOp.getMemref() == cur) {
          Value src = ptrSource.lookup(cur);
          assert(src && "expected pointer source for tracked value");
          rewriteStore(storeOp, src);
        }
      } else if (auto acquireUser = dyn_cast<DbAcquireOp>(user)) {
        /// Rebuild db_acquire so its result ptr type matches the updated source
        auto loc = acquireUser.getLoc();
        SmallVector<Value> idx(acquireUser.getIndices().begin(),
                               acquireUser.getIndices().end());
        SmallVector<Value> off(acquireUser.getOffsets().begin(),
                               acquireUser.getOffsets().end());
        SmallVector<Value> sz(acquireUser.getSizes().begin(),
                              acquireUser.getSizes().end());
        auto newAcquire = AC->create<DbAcquireOp>(loc, acquireUser.getMode(),
                                                  newPtr, idx, off, sz);
        acquireUser.getGuid().replaceAllUsesWith(newAcquire.getGuid());
        acquireUser.getPtr().replaceAllUsesWith(newAcquire.getPtr());
        opsToRemove.insert(acquireUser);

        enqueue(newAcquire.getPtr(), isStackAllocated.lookup(cur), newPtr);
      } else if (isa<EdtOp>(user)) {
        queueBlockArgFollow(user, use.getOperandNumber(), cur);
      } else if (isa<DbReleaseOp>(user)) {
        // No-op: release will follow updated sources
      } else if (isa<DbFreeOp>(user)) {
        // No-op: free will follow updated sources
      } else if (user->getNumRegions() > 0) {
        queueBlockArgFollow(user, use.getOperandNumber(), cur);
      } else {
        llvm_unreachable("Unexpected operation");
      }
    }
  }

  /// Erase any old block arguments whose uses have been rewritten
  for (auto &it : blockArgsToErase) {
    Block *blk = it.first;
    unsigned idx = it.second;
    if (blk->getArgument(idx).use_empty())
      blk->eraseArgument(idx);
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