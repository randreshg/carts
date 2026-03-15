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
///
/// Example:
///   Before:
///     arts.db_alloc ... alloc_type = stack
///
///   After:
///     arts.db_alloc ... alloc_type = heap
///     (users rewritten to the lowered pointer representation)
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/codegen/Codegen.h"
#include "arts/passes/PassDetails.h"
#include "arts/passes/Passes.h"
#include "arts/transforms/db/DbLayoutStrategy.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/Debug.h"
#include "arts/utils/EdtUtils.h"
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/RemovalUtils.h"
#include "arts/utils/metadata/IdRegistry.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Ops.h"

#include "llvm/ADT/SmallVector.h"
#include <cstdint>
#include <memory>

ARTS_DEBUG_SETUP(db_lowering);
using namespace mlir;
using namespace arts;

namespace {

static void normalizeBlockHaloAcquireSlice(ArtsCodegen *AC, DbAcquireOp acquire,
                                           Value sourcePtr) {
  if (!AC || !acquire || acquire.getMode() != ArtsMode::in || !sourcePtr)
    return;

  auto mode = acquire.getPartitionMode();
  if (!mode ||
      (*mode != PartitionMode::block && *mode != PartitionMode::stencil))
    return;
  auto contractInfo = getLoweringContract(acquire.getPtr());
  if (!contractInfo || !contractInfo->supportedBlockHalo)
    return;

  /// Upstream passes already encode the dependency window in DB-space on the
  /// acquire itself. Recomputing it here from partition hints would
  /// double-convert block windows for stencil-family reads.
  if (!acquire.getOffsets().empty() && !acquire.getSizes().empty())
    return;

  auto alloc =
      dyn_cast_or_null<DbAllocOp>(DbUtils::getUnderlyingDbAlloc(sourcePtr));
  if (!alloc)
    return;

  unsigned rank = std::min<unsigned>(acquire.getPartitionOffsets().size(),
                                     acquire.getPartitionSizes().size());
  if (rank == 0)
    return;

  AcquireRewriteContract rewriteContract =
      deriveAcquireRewriteContract(acquire);
  bool usePartitionSlice = rewriteContract.usePartitionSliceAsDepWindow;
  auto offsetRange =
      usePartitionSlice ? acquire.getPartitionOffsets() : acquire.getOffsets();
  auto sizeRange =
      usePartitionSlice ? acquire.getPartitionSizes() : acquire.getSizes();
  if (offsetRange.empty() || sizeRange.empty())
    return;

  rank = std::min<unsigned>(offsetRange.size(), sizeRange.size());
  if (rank == 0)
    return;

  SmallVector<unsigned, 4> dims = resolveContractOwnerDims(*contractInfo, rank);

  auto outerSizes = alloc.getSizes();
  auto elementSizes = alloc.getElementSizes();
  if (dims.size() > outerSizes.size() || dims.size() > elementSizes.size())
    return;

  Location loc = acquire.getLoc();
  OpBuilder::InsertionGuard guard(AC->getBuilder());
  AC->setInsertionPoint(acquire);

  Value zero = AC->createIndexConstant(0, loc);
  Value one = AC->createIndexConstant(1, loc);
  SmallVector<Value, 4> offsets;
  SmallVector<Value, 4> sizes;
  offsets.reserve(dims.size());
  sizes.reserve(dims.size());

  for (unsigned i = 0; i < dims.size(); ++i) {
    unsigned dim = dims[i];
    Value elementOffset = AC->castToIndex(offsetRange[i], loc);
    Value elementSize = AC->castToIndex(sizeRange[i], loc);
    Value blockSpan = AC->castToIndex(elementSizes[dim], loc);
    Value totalBlocks = AC->castToIndex(outerSizes[dim], loc);

    blockSpan = AC->create<arith::MaxUIOp>(loc, blockSpan, one);
    elementSize = AC->create<arith::MaxUIOp>(loc, elementSize, one);

    Value startBlock =
        AC->create<arith::DivUIOp>(loc, elementOffset, blockSpan);
    Value endElem = AC->create<arith::AddIOp>(loc, elementOffset, elementSize);
    endElem = AC->create<arith::SubIOp>(loc, endElem, one);
    Value endBlock = AC->create<arith::DivUIOp>(loc, endElem, blockSpan);
    Value maxBlock = AC->create<arith::SubIOp>(loc, totalBlocks, one);
    Value startAboveMax = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::ugt, startBlock, maxBlock);
    Value clampedEnd = AC->create<arith::MinUIOp>(loc, endBlock, maxBlock);
    endBlock =
        AC->create<arith::SelectOp>(loc, startAboveMax, endBlock, clampedEnd);

    Value blockCount = AC->create<arith::SubIOp>(loc, endBlock, startBlock);
    blockCount = AC->create<arith::AddIOp>(loc, blockCount, one);
    startBlock =
        AC->create<arith::SelectOp>(loc, startAboveMax, zero, startBlock);
    blockCount =
        AC->create<arith::SelectOp>(loc, startAboveMax, zero, blockCount);

    offsets.push_back(startBlock);
    sizes.push_back(blockCount);
  }

  acquire.getOffsetsMutable().assign(offsets);
  acquire.getSizesMutable().assign(sizes);
}

struct DbLoweringPass : public arts::DbLoweringBase<DbLoweringPass> {
  DbLoweringPass(uint64_t idStride = IdRegistry::DefaultStride)
      : idStride(idStride) {}
  void runOnOperation() override;

private:
  void convertDbAllocOps();
  void updateAllocUsers(DbAllocOp oldAllocOp, DbAllocOp newAllocOp);
  void updateAcquireUsers(DbAcquireOp acquireOp, Value newGuid, Value newPtr,
                          SmallVector<Value> elementSizes);
  Value getLLVMPtr(Value base, ValueRange opIndices, Location loc);

private:
  uint64_t idStride = IdRegistry::DefaultStride;
  SetVector<Operation *> opsToRemove;
  ModuleOp module;
  ArtsCodegen *AC = nullptr;
  IdRegistry idRegistry;
};
} // namespace

/// Lower datablock allocations to use opaque pointers
void DbLoweringPass::runOnOperation() {
  module = getOperation();
  auto ownedAC = std::make_unique<ArtsCodegen>(module, false);
  AC = ownedAC.get();
  ARTS_INFO_HEADER(DbLowering);
  ARTS_DEBUG_REGION(module.dump(););

  convertDbAllocOps();

  RemovalUtils removalMgr;
  for (Operation *op : opsToRemove)
    removalMgr.markForRemoval(op);
  removalMgr.removeAllMarked(module, /*recursive=*/false);
  AC = nullptr;
  ARTS_INFO_FOOTER(DbLoweringPass);
  ARTS_DEBUG_REGION(module.dump(););
}

/// Convert all DB allocation operations to use opaque pointers
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

    SmallVector<Value> sizes(oldOp.getSizes().begin(), oldOp.getSizes().end());
    SmallVector<Value> elementSizes(oldOp.getElementSizes().begin(),
                                    oldOp.getElementSizes().end());
    MemRefType allocatedElementType = oldOp.getAllocatedElementType();
    Type elementType = LLVM::LLVMPointerType::get(
        AC->getContext(), allocatedElementType.getMemorySpaceAsInt());
    SmallVector<int64_t> shape;
    shape.assign(sizes.size(), ShapedType::kDynamic);
    auto ptrType = MemRefType::get(shape, elementType);
    PartitionMode partitionMode =
        getPartitionMode(oldOp.getOperation()).value_or(PartitionMode::coarse);
    DbAllocOp newOp = AC->create<DbAllocOp>(
        oldOp.getLoc(), oldOp.getMode(), oldOp.getRoute(), DbAllocType::heap,
        oldOp.getDbMode(), oldOp.getElementType(), ptrType, sizes, elementSizes,
        partitionMode);
    ARTS_DEBUG("  - New DbAllocOp: " << newOp);
    for (auto &attr : oldOp->getAttrs()) {
      if (!attr.getName().getValue().starts_with("arts."))
        continue;
      if (attr.getName() == AttrNames::Operation::ArtsCreateId)
        continue;
      newOp->setAttr(attr.getName(), attr.getValue());
    }
    /// Preserve non-arts distributed ownership marker so ConvertArtsToLLVM can
    /// route datablock reservation by node.
    if (hasDistributedDbAllocation(oldOp.getOperation()))
      setDistributedDbAllocation(newOp.getOperation(), /*enabled=*/true);

    int64_t baseId = getArtsId(oldOp);
    if (!baseId)
      baseId = idRegistry.getOrCreate(oldOp);

    /// Set create_id = base_id * stride
    if (baseId) {
      int64_t createId = baseId * static_cast<int64_t>(idStride);
      setArtsCreateId(newOp, createId);
      ARTS_DEBUG("  - DB arts.create_id=" << createId << " (base=" << baseId
                                          << " x stride=" << idStride << ")");
    }
    transferValueContract(oldOp.getPtr(), newOp.getPtr(), AC->getBuilder(),
                          newOp.getLoc());
    updateAllocUsers(oldOp, newOp);
    opsToRemove.insert(oldOp);
  }
}

/// Update all users of the old allocation to use the new lowered allocation
void DbLoweringPass::updateAllocUsers(DbAllocOp oldAllocOp,
                                      DbAllocOp newAllocOp) {
  Value oldGuid = oldAllocOp.getGuid();
  Value newGuid = newAllocOp.getGuid();
  assert(oldGuid && newGuid && "expected guid values");
  oldGuid.replaceAllUsesWith(newGuid);

  Value oldPtr = oldAllocOp.getPtr();
  Value newPtr = newAllocOp.getPtr();
  assert(oldPtr && newPtr && "expected pointer values");

  SmallVector<Value> elementSizes(newAllocOp.getElementSizes().begin(),
                                  newAllocOp.getElementSizes().end());

  for (auto &use : llvm::make_early_inc_range(oldPtr.getUses())) {
    Operation *userOp = use.getOwner();
    AC->setInsertionPoint(userOp);

    if (auto dbRefOp = dyn_cast<DbRefOp>(userOp)) {
      SmallVector<Value> dbRefIndices(dbRefOp.getIndices().begin(),
                                      dbRefOp.getIndices().end());
      auto originalMemrefType =
          dbRefOp.getResult().getType().cast<MemRefType>();
      auto createCastedMemref = [&](Location loc) -> Value {
        Value llvmPtr = getLLVMPtr(newPtr, dbRefIndices, loc);
        auto loadedLlvmPtr =
            AC->create<LLVM::LoadOp>(loc, llvmPtr.getType(), llvmPtr);
        return AC->create<polygeist::Pointer2MemrefOp>(loc, originalMemrefType,
                                                       loadedLlvmPtr);
      };

      for (auto &dbRefUse :
           llvm::make_early_inc_range(dbRefOp.getResult().getUses())) {
        Operation *userOp = dbRefUse.getOwner();
        AC->setInsertionPoint(userOp);

        if (auto loadOp = dyn_cast<memref::LoadOp>(userOp)) {
          Value castedMemref = createCastedMemref(loadOp.getLoc());
          SmallVector<Value> indices(loadOp.getIndices().begin(),
                                     loadOp.getIndices().end());
          auto dynLoad = AC->create<polygeist::DynLoadOp>(
              loadOp.getLoc(), loadOp.getResult().getType(), castedMemref,
              indices, elementSizes);
          loadOp.getResult().replaceAllUsesWith(dynLoad.getResult());
          opsToRemove.insert(loadOp);
        } else if (auto storeOp = dyn_cast<memref::StoreOp>(userOp)) {
          Value castedMemref = createCastedMemref(storeOp.getLoc());
          SmallVector<Value> indices(storeOp.getIndices().begin(),
                                     storeOp.getIndices().end());
          AC->create<polygeist::DynStoreOp>(
              storeOp.getLoc(), storeOp.getValueToStore(), castedMemref,
              indices, elementSizes);
          opsToRemove.insert(storeOp);
        } else {
          Value castedMemref = createCastedMemref(userOp->getLoc());
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

/// Process a single DbAcquireOp and its nested acquire operations
void DbLoweringPass::updateAcquireUsers(DbAcquireOp acquireOp, Value newGuid,
                                        Value newPtr,
                                        SmallVector<Value> elementSizes) {
  Value oldSourceGuid = acquireOp.getSourceGuid();
  Value sourceGuid = oldSourceGuid ? newGuid : Value();
  Value sourcePtr = newPtr ? newPtr : acquireOp.getPtr();

  SmallVector<Value> indices(acquireOp.getIndices().begin(),
                             acquireOp.getIndices().end());
  SmallVector<Value> offsets(acquireOp.getOffsets().begin(),
                             acquireOp.getOffsets().end());
  SmallVector<Value> sizes(acquireOp.getSizes().begin(),
                           acquireOp.getSizes().end());
  SmallVector<Value> elementOffsets(acquireOp.getElementOffsets().begin(),
                                    acquireOp.getElementOffsets().end());
  SmallVector<Value> acquireElementSizes(acquireOp.getElementSizes().begin(),
                                         acquireOp.getElementSizes().end());
  /// Extract partition hints from original acquire
  SmallVector<Value> partitionIndices(acquireOp.getPartitionIndices().begin(),
                                      acquireOp.getPartitionIndices().end());
  SmallVector<Value> partitionOffsets(acquireOp.getPartitionOffsets().begin(),
                                      acquireOp.getPartitionOffsets().end());
  SmallVector<Value> partitionSizes(acquireOp.getPartitionSizes().begin(),
                                    acquireOp.getPartitionSizes().end());
  Value boundsValid = acquireOp.getBoundsValid();
  auto newAcquireOp = AC->create<DbAcquireOp>(
      acquireOp.getLoc(), acquireOp.getMode(), sourceGuid, sourcePtr,
      acquireOp.getPartitionMode(), indices, offsets, sizes, partitionIndices,
      partitionOffsets, partitionSizes, boundsValid, elementOffsets,
      acquireElementSizes);
  for (auto &attr : acquireOp->getAttrs()) {
    if (attr.getName().getValue().starts_with("arts."))
      newAcquireOp->setAttr(attr.getName(), attr.getValue());
  }
  newAcquireOp.copyPartitionSegmentsFrom(acquireOp);
  transferValueContract(acquireOp.getPtr(), newAcquireOp.getPtr(),
                        AC->getBuilder(), newAcquireOp.getLoc());
  normalizeBlockHaloAcquireSlice(AC, newAcquireOp, sourcePtr);
  ARTS_DEBUG("  - New DbAcquireOp: " << newAcquireOp);

  auto rewriteBlockUses = [&](Value base, Value replacementBase) {
    for (auto &blockUse : llvm::make_early_inc_range(base.getUses())) {
      Operation *blockUserOp = blockUse.getOwner();
      AC->setInsertionPoint(blockUserOp);

      if (auto dbRefOp = dyn_cast<DbRefOp>(blockUserOp)) {
        SmallVector<Value> dbRefIndices(dbRefOp.getIndices().begin(),
                                        dbRefOp.getIndices().end());
        auto originalMemrefType =
            dbRefOp.getResult().getType().cast<MemRefType>();
        auto createCastedMemref = [&](Location loc) -> Value {
          Value llvmPtr = getLLVMPtr(replacementBase, dbRefIndices, loc);
          auto loadedLlvmPtr =
              AC->create<LLVM::LoadOp>(loc, llvmPtr.getType(), llvmPtr);
          return AC->create<polygeist::Pointer2MemrefOp>(
              loc, originalMemrefType, loadedLlvmPtr);
        };

        for (auto &dbRefUse :
             llvm::make_early_inc_range(dbRefOp.getResult().getUses())) {
          Operation *userOp = dbRefUse.getOwner();
          AC->setInsertionPoint(userOp);

          if (auto loadOp = dyn_cast<memref::LoadOp>(userOp)) {
            Value castedMemref = createCastedMemref(loadOp.getLoc());
            SmallVector<Value> loadIndices(loadOp.getIndices().begin(),
                                           loadOp.getIndices().end());
            auto dynLoad = AC->create<polygeist::DynLoadOp>(
                loadOp.getLoc(), loadOp.getResult().getType(), castedMemref,
                loadIndices, elementSizes);
            loadOp.getResult().replaceAllUsesWith(dynLoad.getResult());
            opsToRemove.insert(loadOp);
          } else if (auto storeOp = dyn_cast<memref::StoreOp>(userOp)) {
            Value castedMemref = createCastedMemref(storeOp.getLoc());
            SmallVector<Value> storeIndices(storeOp.getIndices().begin(),
                                            storeOp.getIndices().end());
            AC->create<polygeist::DynStoreOp>(
                storeOp.getLoc(), storeOp.getValueToStore(), castedMemref,
                storeIndices, elementSizes);
            opsToRemove.insert(storeOp);
          } else {
            Value castedMemref = createCastedMemref(userOp->getLoc());
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

  auto [edtUser, blockArg] = getEdtBlockArgumentForAcquire(acquireOp);
  if (!edtUser || !blockArg) {
    ARTS_DEBUG("  - Acquire has no EDT consumer; replacing uses directly");
    rewriteBlockUses(acquireOp.getPtr(), newPtr ? newPtr : acquireOp.getPtr());
    acquireOp->replaceAllUsesWith(newAcquireOp);
    opsToRemove.insert(acquireOp);
    return;
  }

  blockArg.setType(newPtr.getType());
  rewriteBlockUses(blockArg, blockArg);
  acquireOp->replaceAllUsesWith(newAcquireOp);
  opsToRemove.insert(acquireOp);
}

/// Helper function to get LLVM pointer from a base value and indices
Value DbLoweringPass::getLLVMPtr(Value base, ValueRange opIndices,
                                 Location loc) {
  LayoutInfo layout = buildLayoutInfo(base);
  SmallVector<Value> indices(opIndices.begin(), opIndices.end());
  return computeDbElementPointer(*AC, loc, base, indices, layout);
}

///===----------------------------------------------------------------------===///
/// Pass creation
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
