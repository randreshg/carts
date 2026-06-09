///==========================================================================///
/// File: CodirToArtsHostBridgeLogicalSizes.h
///
/// Logical element-size recovery for host bridge materialization.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGELOGICALSIZES_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGELOGICALSIZES_H

#include "CodirToArtsHostBridgeHoist.h"

namespace {

static inline SmallVector<Value>
getBridgeLogicalElementSizes(OpBuilder &builder, Location loc, Value hostView) {
  if (arts::DbAllocOp hostAlloc = findBackingDbAlloc(hostView))
    return SmallVector<Value>(hostAlloc.getElementSizes().begin(),
                              hostAlloc.getElementSizes().end());

  SmallVector<Value> sizes;
  auto memrefType = dyn_cast<MemRefType>(hostView.getType());
  if (!memrefType)
    return sizes;
  sizes.reserve(memrefType.getRank());
  for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim) {
    if (memrefType.isDynamicDim(dim)) {
      sizes.push_back(memref::DimOp::create(builder, loc, hostView, dim));
      continue;
    }
    sizes.push_back(
        createConstantIndex(builder, loc, memrefType.getDimSize(dim)));
  }
  return sizes;
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_HOSTBRIDGELOGICALSIZES_H
