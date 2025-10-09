///==========================================================================
/// File: NormalizeDbs.cpp
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
///==========================================================================

/// Dialects
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "polygeist/Ops.h"
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

  /// Create the replacement DbAlloc operation for a given source alloc.
  /// Decides the builder overload based on whether the address operand is a
  /// scalar memref (rank 0) or a shaped memref, and ensures sizes/payloadSizes
  /// are threaded correctly. For stack-backed sources, the alloc type is
  /// switched to heap so the pointer result is heap-resident.
  DbAllocOp createNormalizedAlloc(DbAllocOp oldOp, Value route,
                                  SmallVector<Value> &sizes,
                                  SmallVector<Value> &payloadSizes,
                                  DbAllocType newAllocType);

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

  ARTS_INFO_HEADER(NormalizeDbs);
  ARTS_DEBUG_REGION(module.dump(););

  /// Convert all DB operations to use opaque pointers and update uses
  convertDbAllocOps();

  /// Clean up old operations
  removeOps(module, opsToRemove);

  ARTS_INFO_FOOTER(NormalizeDbsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

/// Convert all DB allocation operations to use opaque pointers
void NormalizeDbsPass::convertDbAllocOps() {
  SmallVector<DbAllocOp, 8> dbAllocOps;

  /// Collect all DbAllocOps
  module.walk([&](arts::DbAllocOp allocOp) { dbAllocOps.push_back(allocOp); });
  if (dbAllocOps.empty())
    return;

  /// Process each DbAllocOp
  for (auto oldOp : dbAllocOps) {
    AC->setInsertionPointAfter(oldOp);

    /// Convert stack-backed allocations to heap and make the pointer opaque
    Value route = AC->createIntConstant(0, AC->Int32, oldOp.getLoc());
    SmallVector<Value> sizes(oldOp.getSizes().begin(), oldOp.getSizes().end());
    SmallVector<Value> payloadSizes(oldOp.getPayloadSizes().begin(),
                                    oldOp.getPayloadSizes().end());

    if (Value addr = oldOp.getAddress())
      if (auto mt = addr.getType().dyn_cast<MemRefType>())
        if (mt.getRank() == 0 && sizes.empty())
          sizes.push_back(AC->createIndexConstant(1, oldOp.getLoc()));

    DbAllocType newAllocType = oldOp.getAllocType();
    if (newAllocType == DbAllocType::stack)
      newAllocType = DbAllocType::heap;

    DbAllocOp newOp =
        createNormalizedAlloc(oldOp, route, sizes, payloadSizes, newAllocType);
    updateUsers(oldOp, newOp);
    opsToRemove.insert(oldOp);
  }
}

DbAllocOp NormalizeDbsPass::createNormalizedAlloc(
    DbAllocOp oldOp, Value route, SmallVector<Value> &sizes,
    SmallVector<Value> &payloadSizes, DbAllocType newAllocType) {

  /// If address is a scalar memref, omit it so the builder uses sizes for ptr
  if (Value addr = oldOp.getAddress()) {
    if (auto mt = addr.getType().dyn_cast<MemRefType>()) {
      if (mt.getRank() == 0) {
        return AC->create<DbAllocOp>(
            oldOp.getLoc(), oldOp.getMode(), route, newAllocType,
            oldOp.getDbMode(), oldOp.getElementType(), sizes, payloadSizes);
      }

      return AC->create<DbAllocOp>(
          oldOp.getLoc(), oldOp.getMode(), route, newAllocType,
          oldOp.getDbMode(), oldOp.getElementType(), addr, sizes, payloadSizes);
    }

    return AC->create<DbAllocOp>(oldOp.getLoc(), oldOp.getMode(), route,
                                 newAllocType, oldOp.getDbMode(),
                                 oldOp.getElementType(), sizes, payloadSizes);
  }

  return AC->create<DbAllocOp>(oldOp.getLoc(), oldOp.getMode(), route,
                               newAllocType, oldOp.getDbMode(),
                               oldOp.getElementType(), sizes, payloadSizes);
}

/// Update all users of an old DB operation to use the new one

void NormalizeDbsPass::updateUsers(DbAllocOp oldAllocOp, DbAllocOp newAllocOp) {

  bool isStackAlloc = oldAllocOp.getAllocType() == DbAllocType::stack;
  /// Verify we have guid and replace the old one with the new one
  Value oldGuid = oldAllocOp.getGuid();
  Value newGuid = newAllocOp.getGuid();
  assert(oldGuid && newGuid && "expected guid values");
  oldGuid.replaceAllUsesWith(newGuid);

  /// Replace the old ptr with the new one
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

  DenseMap<Block *, SmallVector<unsigned>> argsToErase;
  SmallVector<std::pair<Block *, unsigned>> blockArgsToErase;

  /// To avoid duplicate processing of EdtOps
  SmallPtrSet<Operation *, 8> processedEdts;

  /// Helper to enqueue a value for BFS with its stack flag and source ptr
  auto enqueue = [&](Value v, bool stackFlag, Value src) {
    if (visited.insert(v).second) {
      worklist.push_back(v);
      isStackAllocated[v] = stackFlag;
      ptrSource[v] = src;
    }
  };

  /// Helper to cast a DB pointer to an LLVM pointer
  /// - If stack-allocated, load the pointer from memory
  /// - If heap-allocated, return the pointer directly
  auto getLLVMPtr = [&](Value ptrBase, ValueRange indices,
                        Location loc) -> Value {
    Value targetPtr = ptrBase;
    if (!indices.empty()) {
      SmallVector<Value> sizes = getSizesFromDb(targetPtr);
      SmallVector<Value> strides = AC->computeStridesFromSizes(sizes, loc);
      targetPtr =
          AC->create<DbGepOp>(loc, AC->llvmPtr, targetPtr, indices, strides);
    }

    auto it = isStackAllocated.find(ptrBase);
    bool stack = (it != isStackAllocated.end()) ? it->second : false;
    Value llvmPtr = AC->castToLLVMPtr(targetPtr, loc);
    if (stack)
      return AC->create<LLVM::LoadOp>(loc, AC->llvmPtr, llvmPtr);
    return llvmPtr;
  };

  /// Helper to update the block arg for the dependency operand
  auto queueBlockArgFollow = [&](Operation *user, unsigned operandIdx,
                                 Value fromVal) {
    if (user->getNumRegions() == 0)
      return;

    for (Region &region : user->getRegions()) {
      if (region.empty())
        continue;

      Block &entry = region.front();

      /// Map operand index to block argument index for EDT dependencies using
      /// ODS segments
      unsigned argIdx = operandIdx;
      if (auto edtOp = dyn_cast<EdtOp>(user)) {
        auto [depsBegin, depsLen] =
            edtOp.getODSOperandIndexAndLength(/*dependencies*/ 1);

        /// If it's before the dependencies segment (i.e., the route), skip
        if (operandIdx < depsBegin)
          continue;

        /// If it's beyond the dependencies segment, skip as well
        if (operandIdx >= depsBegin + depsLen)
          continue;

        /// Dependencies map 1:1 to block arguments starting at index 0
        argIdx = operandIdx - depsBegin;
      }

      Value oldArg = entry.getArgument(argIdx);
      Type desiredType = ptrSource.lookup(fromVal).getType();
      bool stackFlag = isStackAllocated.lookup(fromVal);
      if (oldArg.getType() != desiredType) {
        /// Add new argument at the end with correct type and replace uses
        Value newArg = entry.addArgument(desiredType, oldArg.getLoc());
        oldArg.replaceAllUsesWith(newArg);
        enqueue(newArg, stackFlag, ptrSource.lookup(fromVal));

        /// Mark the old arg for erasure
        argsToErase[&entry].push_back(argIdx);
      } else {
        enqueue(oldArg, stackFlag, ptrSource.lookup(fromVal));
      }
    }
  };

  /// Rebuild various uses of the DB pointer (load, store, etc.) and queue block
  /// arguments to rewrite after the pointer is updated
  while (!worklist.empty()) {
    Value cur = worklist.pop_back_val();
    for (auto &use : llvm::make_early_inc_range(cur.getUses())) {
      Operation *user = use.getOwner();
      AC->setInsertionPoint(user);
      if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
        if (loadOp.getMemref() == cur) {
          Value src = ptrSource.lookup(cur);
          assert(src && "expected pointer source for tracked value");
          Value llvmPtr = getLLVMPtr(src, loadOp.getIndices(), loadOp.getLoc());
          auto newLoad = AC->create<LLVM::LoadOp>(
              loadOp.getLoc(), loadOp.getResult().getType(), llvmPtr);
          loadOp.getResult().replaceAllUsesWith(newLoad.getResult());
          opsToRemove.insert(loadOp);
        }
      } else if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
        if (storeOp.getMemref() == cur) {
          Value src = ptrSource.lookup(cur);
          assert(src && "expected pointer source for tracked value");
          Value llvmPtr =
              getLLVMPtr(src, storeOp.getIndices(), storeOp.getLoc());
          AC->create<LLVM::StoreOp>(storeOp.getLoc(), storeOp.getValueToStore(),
                                    llvmPtr);
          opsToRemove.insert(storeOp);
        }
      } else if (auto acquireUser = dyn_cast<DbAcquireOp>(user)) {
        auto loc = acquireUser.getLoc();
        SmallVector<Value> indices(acquireUser.getIndices().begin(),
                                   acquireUser.getIndices().end());
        SmallVector<Value> offsets(acquireUser.getOffsets().begin(),
                                   acquireUser.getOffsets().end());
        SmallVector<Value> sizes(acquireUser.getSizes().begin(),
                                 acquireUser.getSizes().end());

        Value srcGuid;
        if (auto allocOp = ptrSource[cur].getDefiningOp<DbAllocOp>())
          srcGuid = allocOp.getGuid();
        else if (auto acqSrc = ptrSource[cur].getDefiningOp<DbAcquireOp>())
          srcGuid = acqSrc.getGuid();
        auto newAcquire =
            AC->create<DbAcquireOp>(loc, acquireUser.getMode(), srcGuid,
                                    ptrSource[cur], indices, offsets, sizes);
        acquireUser.getGuid().replaceAllUsesWith(newAcquire.getGuid());
        acquireUser.getPtr().replaceAllUsesWith(newAcquire.getPtr());
        opsToRemove.insert(acquireUser);

        /// Do not propagate stack-allocated semantics through db_acquire. The
        /// pointer result of acquire should be treated as a heap handle here,
        /// avoiding extra loads injected by this normalization step.
        enqueue(newAcquire.getPtr(), false, newAcquire.getPtr());
      } else if (auto gep = dyn_cast<DbGepOp>(user)) {
        /// Propagate tracking through db_gep using the same source ptr
        enqueue(gep.getPtr(), isStackAllocated.lookup(cur),
                ptrSource.lookup(cur));
      } else if (isa<EdtOp>(user)) {
        queueBlockArgFollow(user, use.getOperandNumber(), cur);
      } else if (isa<DbReleaseOp>(user)) {
        opsToRemove.insert(user);
        continue;
      } else if (isa<DbFreeOp>(user)) {
        auto freeOp = cast<DbFreeOp>(user);
        auto allocOp = ptrSource[cur].getDefiningOp<DbAllocOp>();
        freeOp.getSourceMutable().assign(allocOp.getPtr());
        continue;
      } else if (user->getNumRegions() > 0) {
        queueBlockArgFollow(user, use.getOperandNumber(), cur);
      } else {
        llvm_unreachable("Unexpected operation");
      }
    }
  }

  /// Erase any old block arguments whose uses have been rewritten
  for (auto &pair : argsToErase) {
    Block *blk = pair.first;
    SmallVector<unsigned> idxs = pair.second;
    std::sort(idxs.begin(), idxs.end(), std::greater<unsigned>());
    for (unsigned idx : idxs) {
      if (blk->getArgument(idx).use_empty())
        blk->eraseArgument(idx);
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