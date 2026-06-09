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

  root.replaceAllUsesWith(replacement);
  for (memref::DeallocOp dealloc : deallocs)
    dealloc.erase();
  if (def->use_empty())
    def->erase();
  return replacement;
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTCOARSEDB_H
