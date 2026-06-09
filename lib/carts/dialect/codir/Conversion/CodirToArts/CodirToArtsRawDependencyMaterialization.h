///==========================================================================///
/// File: CodirToArtsRawDependencyMaterialization.h
///
/// Raw CODIR dependency DB materialization entrypoint.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_RAWDEPENDENCYMATERIALIZATION_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_RAWDEPENDENCYMATERIALIZATION_H

#include "CodirToArtsHostWholeToBlockBridge.h"

namespace {

static inline bool
canMaterializeRawCodirDependencyWithPlan(Value root,
                                         codir::CodeletOp planSource) {
  if (!hasCodirTileOwnerSlicePlan(planSource))
    return false;
  std::optional<unsigned> depIndex =
      findCodirDependencyIndexForRoot(planSource, root);
  if (!depIndex)
    return false;
  if (!codirDepAllowsComputeBlockStorage(planSource, *depIndex))
    return false;
  return codirDepCanUseBlockStorageAccess(planSource, *depIndex);
}

static inline bool rawCodirDependencyNeedsHostBridge(Value root) {
  if (!root)
    return false;
  if (isa<BlockArgument>(root))
    return true;
  return hasHostMemrefAccessOutsideSchedulingUnit(root);
}

static inline LogicalResult
materializeRawCodirDependency(Value dep, codir::CodeletOp planSource,
                              unsigned depIndex) {
  if (findBackingDbAlloc(dep))
    return success();
  if (failed(requireFinalizedCodirDepOwnerDimsForMaterialization(planSource,
                                                                 depIndex)))
    return failure();

  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep);
  auto memrefType = dyn_cast<MemRefType>(root.getType());
  if (!memrefType)
    return failure();

  Operation *def = root.getDefiningOp();
  OpBuilder builder(root.getContext());
  Value replacement;
  bool usePlan = canMaterializeRawCodirDependencyWithPlan(root, planSource);
  bool needsHostBridge = rawCodirDependencyNeedsHostBridge(root);
  if (usePlan && needsHostBridge) {
    if (auto blockArg = dyn_cast<BlockArgument>(root)) {
      FailureOr<Value> hostView = materializeCoarseHostDbForBlockArgument(
          builder, root.getLoc(), blockArg);
      if (failed(hostView))
        return failure();
      FailureOr<Value> bridged = materializeHostWholeToComputeBlockBridge(
          planSource, depIndex, *hostView);
      return failed(bridged) ? failure() : success();
    }

    FailureOr<Value> bridged =
        materializeHostWholeToComputeBlockBridge(planSource, depIndex, root);
    if (failed(bridged))
      return failure();
    return success();
  }
  if (!def) {
    auto blockArg = dyn_cast<BlockArgument>(root);
    if (!blockArg)
      return failure();

    Block *owner = blockArg.getOwner();
    if (!owner)
      return failure();
    builder.setInsertionPointToStart(owner);

    SmallVector<Value> elementSizes;
    SmallVector<Value> dynamicSizes;
    if (memrefType.getRank() == 0) {
      elementSizes.push_back(createOneIndex(builder, root.getLoc()));
    } else {
      elementSizes.reserve(memrefType.getRank());
      for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim) {
        if (memrefType.isDynamicDim(dim)) {
          Value size = memref::DimOp::create(builder, root.getLoc(), root, dim);
          elementSizes.push_back(size);
          dynamicSizes.push_back(size);
        } else {
          elementSizes.push_back(createConstantIndex(
              builder, root.getLoc(), memrefType.getDimSize(dim)));
        }
      }
    }

    SmallVector<Operation *> dimOps;
    for (Value size : elementSizes)
      if (Operation *op = size.getDefiningOp())
        dimOps.push_back(op);

    arts::DbAllocOp createdDbAlloc;
    if (usePlan) {
      if (failed(createDbBackedMemref(builder, root.getLoc(), memrefType,
                                      dynamicSizes, replacement, planSource,
                                      depIndex)))
        return failure();
    } else {
      Value route = arts::createCurrentNodeRoute(builder, root.getLoc());
      auto dbAlloc = arts::DbAllocOp::create(
          builder, root.getLoc(), arts::ArtsMode::inout, route,
          arts::DbAllocType::unknown, arts::DbMode::write,
          memrefType.getElementType(), root,
          SmallVector<Value>{createOneIndex(builder, root.getLoc())},
          std::move(elementSizes), arts::PartitionMode::coarse);
      createdDbAlloc = dbAlloc;
      replacement =
          materializeInnerPayload(builder, root.getLoc(), dbAlloc.getPtr());
    }

    root.replaceUsesWithIf(replacement, [&](OpOperand &use) {
      Operation *owner = use.getOwner();
      if (createdDbAlloc && owner == createdDbAlloc.getOperation())
        return false;
      return !llvm::is_contained(dimOps, owner);
    });
    return success();
  }

  builder.setInsertionPointAfter(def);
  if (auto alloc = dyn_cast<memref::AllocOp>(def)) {
    if (failed(usePlan
                   ? createDbBackedMemref(builder, alloc.getLoc(), memrefType,
                                          alloc.getDynamicSizes(), replacement,
                                          planSource, depIndex)
                   : createDbBackedMemref(builder, alloc.getLoc(), memrefType,
                                          alloc.getDynamicSizes(),
                                          replacement)))
      return failure();
  } else if (auto alloca = dyn_cast<memref::AllocaOp>(def)) {
    if (failed(usePlan
                   ? createDbBackedMemref(builder, alloca.getLoc(), memrefType,
                                          alloca.getDynamicSizes(), replacement,
                                          planSource, depIndex)
                   : createDbBackedMemref(builder, alloca.getLoc(), memrefType,
                                          alloca.getDynamicSizes(),
                                          replacement)))
      return failure();
  } else {
    return failure();
  }

  SmallVector<memref::DeallocOp> deallocs;
  for (Operation *user : llvm::make_early_inc_range(root.getUsers())) {
    auto dealloc = dyn_cast<memref::DeallocOp>(user);
    if (dealloc && dealloc.getMemref() == root)
      deallocs.push_back(dealloc);
  }
  for (memref::DeallocOp dealloc : deallocs)
    dealloc.erase();

  root.replaceAllUsesWith(replacement);
  if (def->use_empty())
    def->erase();
  return success();
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_RAWDEPENDENCYMATERIALIZATION_H
