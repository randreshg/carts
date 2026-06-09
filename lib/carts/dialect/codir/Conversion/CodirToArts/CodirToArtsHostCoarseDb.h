///==========================================================================///
/// File: CodirToArtsHostCoarseDb.h
///
/// Coarse host DB materialization for host bridge roots.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTCOARSEDB_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTCOARSEDB_H

#include "CodirToArtsHostCopyNests.h"

namespace {

static inline FailureOr<Value>
materializeCoarseHostDbForBlockArgument(OpBuilder &builder, Location loc,
                                        BlockArgument blockArg) {
  Value root = blockArg;
  auto memrefType = dyn_cast<MemRefType>(root.getType());
  if (!memrefType)
    return failure();

  Block *owner = blockArg.getOwner();
  if (!owner)
    return failure();

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(owner);

  SmallVector<Value> elementSizes;
  if (memrefType.getRank() == 0) {
    elementSizes.push_back(createOneIndex(builder, loc));
  } else {
    elementSizes.reserve(memrefType.getRank());
    for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim) {
      if (memrefType.isDynamicDim(dim)) {
        elementSizes.push_back(memref::DimOp::create(builder, loc, root, dim));
        continue;
      }
      elementSizes.push_back(
          createConstantIndex(builder, loc, memrefType.getDimSize(dim)));
    }
  }

  SmallVector<Operation *> protectedOps;
  for (Value size : elementSizes)
    if (Operation *op = size.getDefiningOp())
      protectedOps.push_back(op);

  Value route = arts::createCurrentNodeRoute(builder, loc);
  auto dbAlloc = arts::DbAllocOp::create(
      builder, loc, arts::ArtsMode::inout, route, arts::DbAllocType::unknown,
      arts::DbMode::write, memrefType.getElementType(), root,
      SmallVector<Value>{createOneIndex(builder, loc)}, std::move(elementSizes),
      arts::PartitionMode::coarse);
  protectedOps.push_back(dbAlloc.getOperation());

  Value replacement = materializeInnerPayload(builder, loc, dbAlloc.getPtr());
  root.replaceUsesWithIf(replacement, [&](OpOperand &use) {
    return !llvm::is_contained(protectedOps, use.getOwner());
  });
  return replacement;
}

// True when the forward cone of `value` reaches a codir.codelet dep operand or
// an SDE scheduling-unit use. Such uses must stay on the canonical block DB and
// must NOT be repointed onto the coarse host DB.
static inline bool hostBridgeValueFeedsCodeletDep(Value value) {
  if (!value)
    return false;
  SmallVector<Value, 8> worklist{value};
  llvm::SmallPtrSet<Value, 16> visited;
  while (!worklist.empty()) {
    Value current = worklist.pop_back_val();
    if (!current || !visited.insert(current).second)
      continue;
    for (OpOperand &use : current.getUses()) {
      Operation *owner = use.getOwner();
      if (!owner)
        continue;
      if (isa<codir::CodeletOp>(owner))
        return true;
      if (owner->getParentOfType<codir::CodeletOp>() ||
          owner->getParentOfType<sde::SdeSuIterateOp>())
        return true;
      if (isCodirViewDep(current) || isMemrefForwardingOp(owner))
        for (Value result : owner->getResults())
          if (isa<MemRefType>(result.getType()))
            worklist.push_back(result);
    }
  }
  return false;
}

// True when the forward cone of `value` reaches a genuine host memref load/store
// outside any scheduling unit (mirrors hasHostMemrefAccessOutsideSchedulingUnit
// in CodirConversionUtils.h, but rooted at an arbitrary intermediate value).
static inline bool hostBridgeValueFeedsHostAccess(Value value) {
  if (!value)
    return false;
  SmallVector<Value, 8> worklist{value};
  llvm::SmallPtrSet<Value, 16> visited;
  while (!worklist.empty()) {
    Value current = worklist.pop_back_val();
    if (!current || !visited.insert(current).second)
      continue;
    for (Operation *user : current.getUsers()) {
      if (!user || user->getParentOfType<codir::CodeletOp>() ||
          user->getParentOfType<sde::SdeSuIterateOp>())
        continue;
      if (isa<memref::DeallocOp, memref::DimOp>(user))
        continue;
      if (isa<memref::LoadOp, memref::StoreOp>(user))
        return true;
      if (isMemrefForwardingOp(user))
        for (Value result : user->getResults())
          if (isa<MemRefType>(result.getType()))
            worklist.push_back(result);
    }
  }
  return false;
}

// A use is served by the coarse host DB only when the consuming op (the use's
// owner) is, or forwards to, a genuine host load/store outside any scheduling
// unit and NEVER a codelet dep. The classification is per-use, not per-value:
// the MU root feeds both compute_block subviews (kept on the block DB) and the
// host read (moved to the coarse DB), so we must reason about THIS use's owner
// cone, not the shared root's whole cone. Mirrors the
// materializeCoarseHostDbForBlockArgument replaceUsesWithIf intent.
static inline bool hostBridgeUseServedByCoarseHostDb(OpOperand &use) {
  Operation *owner = use.getOwner();
  if (!owner)
    return false;
  // Inside a scheduling unit: this is compute storage, keep it on the block DB.
  if (owner->getParentOfType<codir::CodeletOp>() ||
      owner->getParentOfType<sde::SdeSuIterateOp>())
    return false;
  // Genuine host access directly on the root: served by the coarse host DB.
  if (isa<memref::LoadOp, memref::StoreOp>(owner))
    return true;
  // Deallocs/dims are never the discriminator and stay with whichever storage
  // ultimately owns the root.
  if (isa<memref::DeallocOp, memref::DimOp>(owner))
    return false;
  // View/forwarding op: follow its results. Move it to the coarse host DB only
  // when its forward cone reaches a host access and never a codelet dep.
  if (isCodirViewDep(use.get()) || isMemrefForwardingOp(owner)) {
    for (Value result : owner->getResults()) {
      if (!isa<MemRefType>(result.getType()))
        continue;
      if (hostBridgeValueFeedsCodeletDep(result))
        return false;
    }
    for (Value result : owner->getResults()) {
      if (!isa<MemRefType>(result.getType()))
        continue;
      if (hostBridgeValueFeedsHostAccess(result))
        return true;
    }
  }
  return false;
}

static inline FailureOr<Value>
materializeCoarseHostDbForHostBridge(OpBuilder &builder, Location loc,
                                     Value hostView) {
  if (!hostView)
    return failure();
  if (findBackingDbAlloc(hostView))
    return hostView;

  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(hostView);
  if (root != hostView)
    return failure();

  auto memrefType = dyn_cast<MemRefType>(root.getType());
  if (!memrefType)
    return failure();

  if (auto blockArg = dyn_cast<BlockArgument>(root))
    return materializeCoarseHostDbForBlockArgument(builder, loc, blockArg);

  Operation *def = root.getDefiningOp();
  if (!def)
    return failure();

  SmallVector<Value> dynamicSizes;
  OpBuilder::InsertionGuard guard(builder);
  if (auto alloc = dyn_cast<memref::AllocOp>(def)) {
    dynamicSizes.assign(alloc.getDynamicSizes().begin(),
                        alloc.getDynamicSizes().end());
    builder.setInsertionPointAfter(alloc);
  } else if (auto alloca = dyn_cast<memref::AllocaOp>(def)) {
    dynamicSizes.assign(alloca.getDynamicSizes().begin(),
                        alloca.getDynamicSizes().end());
    builder.setInsertionPointAfter(alloca);
  } else if (auto muAlloc = dyn_cast<sde::SdeMuAllocOp>(def)) {
    dynamicSizes.assign(muAlloc.getDynamicSizes().begin(),
                        muAlloc.getDynamicSizes().end());
    builder.setInsertionPointAfter(muAlloc);
  } else {
    return failure();
  }

  SmallVector<memref::DeallocOp> deallocs;
  for (Operation *user : llvm::make_early_inc_range(root.getUsers())) {
    auto dealloc = dyn_cast<memref::DeallocOp>(user);
    if (dealloc && dealloc.getMemref() == root)
      deallocs.push_back(dealloc);
  }

  Value replacement;
  if (failed(createDbBackedMemref(builder, root.getLoc(), memrefType,
                                  dynamicSizes, replacement)))
    return failure();

  Operation *replacementDef = replacement.getDefiningOp();
  root.replaceUsesWithIf(replacement, [&](OpOperand &use) {
    if (replacementDef && use.getOwner() == replacementDef)
      return false;
    return hostBridgeUseServedByCoarseHostDb(use);
  });
  if (root.use_empty()) {
    for (memref::DeallocOp dealloc : deallocs)
      dealloc.erase();
    if (def->use_empty())
      def->erase();
  }
  return replacement;
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTCOARSEDB_H
