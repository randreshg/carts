///==========================================================================///
/// File: CodirToArtsBridgeOwnerMap.h
///
/// Bridge owner-map and static block-space helpers.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEOWNERMAP_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEOWNERMAP_H

#include "CodirToArtsBridgePayload.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"

namespace {

static inline int64_t getStaticFlatBlockCount(arts::DbAllocOp blockAlloc) {
  if (!blockAlloc)
    return 0;
  int64_t count = 1;
  for (Value size : blockAlloc.getSizes()) {
    std::optional<int64_t> constant = getPositiveStaticIndex(size);
    if (!constant)
      return 0;
    count = saturatingMul(count, *constant);
  }
  return count;
}

static inline SmallVector<int64_t, 4>
getStaticRowMajorBlockStrides(arts::DbAllocOp blockAlloc) {
  SmallVector<int64_t, 4> strides;
  if (!blockAlloc)
    return strides;
  strides.assign(blockAlloc.getSizes().size(), 0);
  int64_t stride = 1;
  for (int64_t dim = static_cast<int64_t>(blockAlloc.getSizes().size()) - 1;
       dim >= 0; --dim) {
    strides[dim] = stride;
    std::optional<int64_t> size =
        getPositiveStaticIndex(blockAlloc.getSizes()[dim]);
    if (!size) {
      strides.clear();
      return strides;
    }
    stride = saturatingMul(stride, *size);
  }
  return strides;
}

static inline bool
isStaticContiguousElementSlice(ArrayRef<int64_t> offsets,
                               ArrayRef<int64_t> sizes,
                               ArrayRef<int64_t> elementSizes) {
  if (offsets.size() != sizes.size() || offsets.size() != elementSizes.size() ||
      offsets.empty())
    return false;

  bool narrowerThanBlock = false;
  for (auto [offset, size, extent] :
       llvm::zip_equal(offsets, sizes, elementSizes)) {
    if (extent <= 0 || size <= 0 || offset < 0 || offset + size > extent)
      return false;
    narrowerThanBlock |= offset != 0 || size != extent;
  }
  if (!narrowerThanBlock)
    return false;

  for (size_t pivot = 0; pivot < sizes.size(); ++pivot) {
    bool contiguous = true;
    for (size_t dim = 0; dim < sizes.size(); ++dim) {
      if (dim < pivot) {
        contiguous &= sizes[dim] == 1;
        continue;
      }
      if (dim > pivot)
        contiguous &= offsets[dim] == 0 && sizes[dim] == elementSizes[dim];
    }
    if (contiguous)
      return true;
  }
  return false;
}

static inline std::optional<SmallVector<int64_t, 4>>
getStaticElementSizes(arts::DbAllocOp blockAlloc) {
  if (!blockAlloc)
    return std::nullopt;
  SmallVector<int64_t, 4> elementSizes;
  elementSizes.reserve(blockAlloc.getElementSizes().size());
  for (Value size : blockAlloc.getElementSizes()) {
    std::optional<int64_t> constant = getPositiveStaticIndex(size);
    if (!constant)
      return std::nullopt;
    elementSizes.push_back(*constant);
  }
  return elementSizes;
}

static inline BridgeOwnerMap buildBridgeOwnerMap(codir::CodeletOp codelet,
                                                 unsigned depIndex,
                                                 arts::DbAllocOp blockAlloc) {
  BridgeOwnerMap ownerMap;
  if (std::optional<SmallVector<unsigned, 4>> ownerDims =
          getCodirDepOwnerDims(codelet, depIndex))
    ownerMap.ownerDims.assign(ownerDims->begin(), ownerDims->end());
  if (blockAlloc) {
    if (std::optional<arts::DbOwnerMapPlan> plan =
            arts::getDbOwnerMapPlan(blockAlloc)) {
      ownerMap.ownerMapKind = plan->kind;
      ownerMap.ownerMapDims.assign(plan->dims.begin(), plan->dims.end());
    } else {
      ownerMap.ownerMapKind = arts::chooseDbOwnerMapKind(blockAlloc);
      if (std::optional<SmallVector<int64_t, 4>> dims =
              arts::getDbOwnerMapDimsFromPlan(blockAlloc))
        ownerMap.ownerMapDims.assign(dims->begin(), dims->end());
    }
    if (auto blockShape = readI64ArrayAttr(
            arts::getPlanPhysicalBlockShapeAttr(blockAlloc.getOperation())))
      ownerMap.physicalBlockShape.assign(blockShape->begin(),
                                         blockShape->end());
    ownerMap.flatBlockCount = getStaticFlatBlockCount(blockAlloc);
  }
  return ownerMap;
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEOWNERMAP_H
