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

#include "../ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/Metadata/IdRegistry.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/RemovalUtils.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Ops.h"

#include "llvm/ADT/SmallVector.h"
#include <cstdint>
#include <functional>

ARTS_DEBUG_SETUP(db_lowering);
using namespace mlir;
using namespace arts;

namespace {
/// Layout information for a datablock allocation (used by DbCopy/DbSync).
struct LayoutInfo {
  DbAllocOp alloc;
  PartitionMode mode = PartitionMode::coarse;
  SmallVector<Value> sizes;
  SmallVector<Value> elementSizes;
  unsigned outerRank = 0;
  unsigned innerRank = 0;
};

static DbAllocOp getAllocFromPtr(Value ptr) {
  Operation *allocOp = DatablockUtils::getUnderlyingDbAlloc(ptr);
  return dyn_cast_or_null<DbAllocOp>(allocOp);
}

static LayoutInfo buildLayoutInfo(DbAllocOp alloc, PartitionMode mode,
                                  OpBuilder &builder, Location loc) {
  LayoutInfo info;
  info.alloc = alloc;
  info.mode = mode;
  info.sizes.assign(alloc.getSizes().begin(), alloc.getSizes().end());
  info.elementSizes.assign(alloc.getElementSizes().begin(),
                           alloc.getElementSizes().end());
  if (info.sizes.empty())
    info.sizes.push_back(builder.create<arith::ConstantIndexOp>(loc, 1));
  if (info.elementSizes.empty())
    info.elementSizes.push_back(builder.create<arith::ConstantIndexOp>(loc, 1));
  info.outerRank = (mode == PartitionMode::coarse) ? 0 : info.sizes.size();
  info.innerRank = info.elementSizes.size();
  return info;
}

/// Get logical dimensions (total elements) for a layout.
static SmallVector<Value> getLogicalDims(const LayoutInfo &info,
                                         OpBuilder &builder, Location loc) {
  SmallVector<Value> dims;
  switch (info.mode) {
  case PartitionMode::coarse:
    dims.append(info.elementSizes.begin(), info.elementSizes.end());
    break;
  case PartitionMode::fine_grained:
    dims.append(info.sizes.begin(), info.sizes.end());
    dims.append(info.elementSizes.begin(), info.elementSizes.end());
    break;
  case PartitionMode::block:
  case PartitionMode::stencil: {
    /// Stencil mode uses same layout as chunked (with halo handling)
    Value firstDim = info.elementSizes.front();
    if (!info.sizes.empty()) {
      firstDim =
          builder.create<arith::MulIOp>(loc, info.sizes.front(), firstDim);
    }
    dims.push_back(firstDim);
    for (size_t i = 1; i < info.elementSizes.size(); ++i)
      dims.push_back(info.elementSizes[i]);
    break;
  }
  }
  if (dims.empty())
    dims.push_back(builder.create<arith::ConstantIndexOp>(loc, 1));
  return dims;
}

/// Trim trailing unit dimensions from a vector.
static void trimTrailingUnitDims(SmallVector<Value> &dims) {
  int64_t value = 0;
  while (!dims.empty() && ValueUtils::getConstantIndex(dims.back(), value) &&
         value == 1) {
    dims.pop_back();
  }
}

/// Map global indices to (outer, inner) indices for a specific layout.
static void mapGlobalToLayout(ArrayRef<Value> globalIndices,
                              const LayoutInfo &info, OpBuilder &builder,
                              Location loc, SmallVector<Value> &outerIndices,
                              SmallVector<Value> &innerIndices) {
  outerIndices.clear();
  innerIndices.clear();
  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);

  if (info.mode == PartitionMode::coarse) {
    innerIndices.append(globalIndices.begin(), globalIndices.end());
    if (innerIndices.empty())
      innerIndices.push_back(zero);
    return;
  }

  if (info.mode == PartitionMode::fine_grained) {
    for (unsigned i = 0; i < info.outerRank; ++i) {
      if (i < globalIndices.size())
        outerIndices.push_back(globalIndices[i]);
      else
        outerIndices.push_back(zero);
    }
    for (unsigned i = info.outerRank; i < globalIndices.size(); ++i)
      innerIndices.push_back(globalIndices[i]);
    while (innerIndices.size() < info.innerRank)
      innerIndices.push_back(zero);
    if (innerIndices.empty())
      innerIndices.push_back(zero);
    return;
  }

  /// Block: assume chunking along the leading dimension
  Value g0 = globalIndices.empty() ? zero : globalIndices.front();
  Value blockSize = info.elementSizes.empty()
                        ? builder.create<arith::ConstantIndexOp>(loc, 1)
                        : info.elementSizes.front();
  outerIndices.push_back(builder.create<arith::DivUIOp>(loc, g0, blockSize));
  while (outerIndices.size() < info.outerRank)
    outerIndices.push_back(zero);

  innerIndices.push_back(builder.create<arith::RemUIOp>(loc, g0, blockSize));
  for (unsigned i = 1; i < globalIndices.size(); ++i)
    innerIndices.push_back(globalIndices[i]);
  while (innerIndices.size() < info.innerRank)
    innerIndices.push_back(zero);
  if (innerIndices.empty())
    innerIndices.push_back(zero);
}
} // namespace

namespace {
struct DbLoweringPass : public arts::DbLoweringBase<DbLoweringPass> {
  DbLoweringPass(uint64_t idStride = IdRegistry::DefaultStride)
      : idStride(idStride) {}
  ~DbLoweringPass() override { delete AC; }
  void runOnOperation() override;

private:
  void lowerDbCopyOps();
  void lowerDbSyncOps();
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
  AC = new ArtsCodegen(module, false);
  ARTS_INFO_HEADER(DbLowering);
  ARTS_DEBUG_REGION(module.dump(););

  lowerDbCopyOps();
  lowerDbSyncOps();
  convertDbAllocOps();

  RemovalUtils removalMgr;
  for (Operation *op : opsToRemove)
    removalMgr.markForRemoval(op);
  removalMgr.removeAllMarked(module, /*recursive=*/false);
  ARTS_INFO_FOOTER(DbLoweringPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Lower DbCopyOp to new DbAllocOp + data copy loop
///===----------------------------------------------------------------------===///
void DbLoweringPass::lowerDbCopyOps() {
  SmallVector<DbCopyOp, 4> copyOps;
  module.walk([&](DbCopyOp op) { copyOps.push_back(op); });

  if (copyOps.empty())
    return;

  ARTS_DEBUG("Lowering " << copyOps.size() << " DbCopyOps");

  auto insertDbFree = [&](DbAllocOp allocOp, OpBuilder &builder) {
    Block *allocBlock = allocOp->getBlock();
    if (!allocBlock)
      return;
    Operation *terminator = allocBlock->getTerminator();
    if (!terminator)
      return;
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPoint(terminator);
    builder.create<DbFreeOp>(allocOp.getLoc(), allocOp.getGuid());
    builder.create<DbFreeOp>(allocOp.getLoc(), allocOp.getPtr());
  };

  for (DbCopyOp copyOp : copyOps) {
    DbAllocOp sourceAlloc = getAllocFromPtr(copyOp.getSourcePtr());
    if (!sourceAlloc) {
      ARTS_DEBUG("  DbCopyLowering: missing source alloc for " << copyOp);
      continue;
    }

    OpBuilder builder(copyOp);
    builder.setInsertionPoint(copyOp);
    Location loc = copyOp.getLoc();

    PartitionMode destMode = copyOp.getDestPartition();
    PartitionMode sourceMode = getPartitionMode(sourceAlloc.getOperation())
                                   .value_or(PartitionMode::coarse);

    SmallVector<Value> newSizes;
    SmallVector<Value> newElementSizes;
    Value one = builder.create<arith::ConstantIndexOp>(loc, 1);

    if (destMode == PartitionMode::fine_grained &&
        sourceAlloc.getElementSizes().size() == 1 &&
        sourceAlloc.getSizes().size() == 1) {
      /// Block -> Fine-grained: total elements = numBlocks * blockSize
      Value numBlocks = sourceAlloc.getSizes().front();
      Value blockSize = sourceAlloc.getElementSizes().front();
      Value totalSize =
          builder.create<arith::MulIOp>(loc, numBlocks, blockSize);
      newSizes.push_back(totalSize);
      newElementSizes.push_back(one);
    } else {
      newSizes.append(sourceAlloc.getSizes().begin(),
                      sourceAlloc.getSizes().end());
      newElementSizes.append(sourceAlloc.getElementSizes().begin(),
                             sourceAlloc.getElementSizes().end());
      if (newSizes.empty())
        newSizes.push_back(one);
      if (newElementSizes.empty())
        newElementSizes.push_back(one);
    }

    DbAllocOp destAlloc = builder.create<DbAllocOp>(
        loc, sourceAlloc.getMode(), sourceAlloc.getRoute(),
        sourceAlloc.getAllocType(), sourceAlloc.getDbMode(),
        sourceAlloc.getElementType(), Value(), newSizes, newElementSizes,
        destMode);
    insertDbFree(destAlloc, builder);

    LayoutInfo sourceInfo =
        buildLayoutInfo(sourceAlloc, sourceMode, builder, loc);
    LayoutInfo destInfo = buildLayoutInfo(destAlloc, destMode, builder, loc);
    destInfo.sizes = newSizes;
    destInfo.elementSizes = newElementSizes;
    destInfo.outerRank =
        (destMode == PartitionMode::coarse) ? 0 : newSizes.size();
    destInfo.innerRank = newElementSizes.size();

    SmallVector<Value> logicalDims = getLogicalDims(sourceInfo, builder, loc);
    Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
    SmallVector<Value> ivs;

    std::function<void(size_t, OpBuilder &)> buildLoopNest =
        [&](size_t dim, OpBuilder &nestBuilder) {
          if (dim == logicalDims.size()) {
            SmallVector<Value> srcOuter, srcInner;
            SmallVector<Value> dstOuter, dstInner;
            mapGlobalToLayout(ivs, sourceInfo, nestBuilder, loc, srcOuter,
                              srcInner);
            mapGlobalToLayout(ivs, destInfo, nestBuilder, loc, dstOuter,
                              dstInner);

            Value srcBase = sourceAlloc.getPtr();
            if (sourceInfo.outerRank > 0) {
              srcBase = nestBuilder.create<DbRefOp>(
                  loc, sourceAlloc.getAllocatedElementType(), srcBase,
                  srcOuter);
            }
            Value dstBase = destAlloc.getPtr();
            if (destInfo.outerRank > 0) {
              dstBase = nestBuilder.create<DbRefOp>(
                  loc, destAlloc.getAllocatedElementType(), dstBase, dstOuter);
            }

            Value val =
                nestBuilder.create<memref::LoadOp>(loc, srcBase, srcInner);
            nestBuilder.create<memref::StoreOp>(loc, val, dstBase, dstInner);
            return;
          }

          Value ub = logicalDims[dim];
          auto loop = nestBuilder.create<scf::ForOp>(loc, zero, ub, one);
          OpBuilder innerBuilder = OpBuilder::atBlockBegin(loop.getBody());
          ivs.push_back(loop.getInductionVar());
          buildLoopNest(dim + 1, innerBuilder);
          ivs.pop_back();
        };

    ARTS_DEBUG("  DbCopyLowering: building copy loop for " << copyOp);
    buildLoopNest(0, builder);

    copyOp.getGuid().replaceAllUsesWith(destAlloc.getGuid());
    copyOp.getPtr().replaceAllUsesWith(destAlloc.getPtr());
    opsToRemove.insert(copyOp.getOperation());
  }
}

/// Lower DbSyncOp to data copy loop
void DbLoweringPass::lowerDbSyncOps() {
  SmallVector<DbSyncOp, 4> syncOps;
  module.walk([&](DbSyncOp op) { syncOps.push_back(op); });

  if (syncOps.empty())
    return;

  ARTS_DEBUG("Lowering " << syncOps.size() << " DbSyncOps");

  for (DbSyncOp syncOp : syncOps) {
    DbAllocOp destAlloc = getAllocFromPtr(syncOp.getDestPtr());
    DbAllocOp sourceAlloc = getAllocFromPtr(syncOp.getSourcePtr());
    if (!destAlloc || !sourceAlloc) {
      ARTS_DEBUG("  DbSyncLowering: missing alloc for " << syncOp);
      opsToRemove.insert(syncOp.getOperation());
      continue;
    }

    OpBuilder builder(syncOp);
    builder.setInsertionPoint(syncOp);
    Location loc = syncOp.getLoc();

    PartitionMode destMode = getPartitionMode(destAlloc.getOperation())
                                 .value_or(PartitionMode::coarse);
    PartitionMode sourceMode = getPartitionMode(sourceAlloc.getOperation())
                                   .value_or(PartitionMode::coarse);

    LayoutInfo destInfo = buildLayoutInfo(destAlloc, destMode, builder, loc);
    LayoutInfo sourceInfo =
        buildLayoutInfo(sourceAlloc, sourceMode, builder, loc);

    SmallVector<Value> sourceLogical = getLogicalDims(sourceInfo, builder, loc);
    SmallVector<Value> destLogical = getLogicalDims(destInfo, builder, loc);
    trimTrailingUnitDims(sourceLogical);
    trimTrailingUnitDims(destLogical);
    if (sourceLogical.size() != destLogical.size()) {
      ARTS_DEBUG("  DbSyncLowering: logical rank mismatch for " << syncOp);
      opsToRemove.insert(syncOp.getOperation());
      continue;
    }

    SmallVector<Value> offsets;
    SmallVector<Value> sizes;
    if (!syncOp.getFullSync()) {
      offsets.assign(syncOp.getElementOffsets().begin(),
                     syncOp.getElementOffsets().end());
      sizes.assign(syncOp.getElementSizes().begin(),
                   syncOp.getElementSizes().end());
    }

    if (sizes.empty())
      sizes.append(sourceLogical.begin(), sourceLogical.end());
    if (offsets.empty()) {
      Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
      offsets.assign(sizes.size(), zero);
    }

    if (offsets.size() != sizes.size() ||
        sizes.size() != sourceLogical.size()) {
      ARTS_DEBUG("  DbSyncLowering: invalid offset/size for " << syncOp);
      opsToRemove.insert(syncOp.getOperation());
      continue;
    }

    Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
    SmallVector<Value> ivs;

    std::function<void(size_t, OpBuilder &)> buildLoopNest =
        [&](size_t dim, OpBuilder &nestBuilder) {
          if (dim == sizes.size()) {
            SmallVector<Value> srcOuter, srcInner;
            SmallVector<Value> dstOuter, dstInner;
            mapGlobalToLayout(ivs, sourceInfo, nestBuilder, loc, srcOuter,
                              srcInner);
            mapGlobalToLayout(ivs, destInfo, nestBuilder, loc, dstOuter,
                              dstInner);

            Value srcBase = syncOp.getSourcePtr();
            if (sourceInfo.outerRank > 0) {
              srcBase = nestBuilder.create<DbRefOp>(
                  loc, sourceAlloc.getAllocatedElementType(), srcBase,
                  srcOuter);
            }
            Value dstBase = syncOp.getDestPtr();
            if (destInfo.outerRank > 0) {
              dstBase = nestBuilder.create<DbRefOp>(
                  loc, destAlloc.getAllocatedElementType(), dstBase, dstOuter);
            }

            Value val =
                nestBuilder.create<memref::LoadOp>(loc, srcBase, srcInner);
            nestBuilder.create<memref::StoreOp>(loc, val, dstBase, dstInner);
            return;
          }

          Value lb = offsets[dim];
          Value ub =
              nestBuilder.create<arith::AddIOp>(loc, offsets[dim], sizes[dim]);
          auto loop = nestBuilder.create<scf::ForOp>(loc, lb, ub, one);
          OpBuilder innerBuilder = OpBuilder::atBlockBegin(loop.getBody());
          ivs.push_back(loop.getInductionVar());
          buildLoopNest(dim + 1, innerBuilder);
          ivs.pop_back();
        };

    ARTS_DEBUG("  DbSyncLowering: building sync loop for " << syncOp);
    buildLoopNest(0, builder);
    opsToRemove.insert(syncOp.getOperation());
  }
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
    int64_t baseId = getArtsId(oldOp);
    if (!baseId)
      baseId = idRegistry.getOrCreate(oldOp);

    /// Set create_id = base_id * stride
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

  auto [edtUser, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquireOp);
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
  if (opIndices.empty())
    return AC->castToLLVMPtr(base, loc);

  SmallVector<Value> sizes = DatablockUtils::getSizesFromDb(base);
  SmallVector<Value> strides = AC->computeStridesFromSizes(sizes, loc);
  SmallVector<Value> indices(opIndices.begin(), opIndices.end());
  return AC->create<DbGepOp>(loc, AC->getLLVMPointerType(base), base, indices,
                             strides);
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
