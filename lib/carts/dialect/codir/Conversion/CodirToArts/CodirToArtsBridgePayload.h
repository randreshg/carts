///==========================================================================///
/// File: CodirToArtsBridgePayload.h
///
/// Static bridge payload and element-slice sizing helpers.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEPAYLOAD_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEPAYLOAD_H

#include "CodirToArtsHostBridgeTypes.h"

namespace {

static inline int64_t getScalarElementBytes(Type elementType) {
  if (!elementType || !elementType.isIntOrFloat())
    return 0;
  return llvm::divideCeil(elementType.getIntOrFloatBitWidth(), 8);
}

static inline int64_t getStaticBlockPayloadBytes(arts::DbAllocOp blockAlloc) {
  int64_t bytes = getScalarElementBytes(blockAlloc.getElementType());
  if (bytes <= 0)
    return 0;
  for (Value size : blockAlloc.getElementSizes()) {
    std::optional<int64_t> constant = getPositiveStaticIndex(size);
    if (!constant)
      return 0;
    bytes = saturatingMul(bytes, *constant);
  }
  return bytes;
}

static inline int64_t getStaticHaloPayloadBytes(const BridgePlan &plan,
                                                arts::DbAllocOp blockAlloc) {
  if (!plan.seedCodelet || !blockAlloc)
    return 0;
  int64_t bytes = getScalarElementBytes(blockAlloc.getElementType());
  if (bytes <= 0)
    return 0;

  unsigned rank = static_cast<unsigned>(blockAlloc.getElementSizes().size());
  if (rank == 0)
    return 0;
  SmallVector<CodirOwnerHaloWindow, 4> ownerHalos =
      getCodirBlockStorageHaloWindows(plan.seedCodelet, plan.seedDepIndex,
                                      rank);
  std::optional<SmallVector<int64_t, 4>> ownerBlockSizes =
      getCodirTileOwnerBlockSizes(plan.seedCodelet, plan.seedDepIndex, rank);
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(plan.seedCodelet, plan.seedDepIndex);
  if (!ownerBlockSizes || !ownerDims ||
      ownerBlockSizes->size() != ownerHalos.size() ||
      ownerDims->size() != ownerHalos.size())
    return 0;

  int64_t haloBytes = 0;
  for (auto [slot, halo] : llvm::enumerate(ownerHalos)) {
    int64_t width = halo.lower + halo.upper;
    if (width <= 0)
      continue;
    int64_t faceElements = width;
    for (auto [otherSlot, otherHalo] : llvm::enumerate(ownerHalos)) {
      (void)otherHalo;
      if (otherSlot == slot)
        continue;
      if ((*ownerBlockSizes)[otherSlot] <= 0)
        return 0;
      faceElements = saturatingMul(faceElements, (*ownerBlockSizes)[otherSlot]);
    }
    int64_t faceBytes = saturatingMul(faceElements, bytes);
    if (haloBytes > std::numeric_limits<int64_t>::max() - faceBytes)
      haloBytes = std::numeric_limits<int64_t>::max();
    else
      haloBytes += faceBytes;
  }
  return haloBytes;
}

static inline int64_t
getBridgeWorkloadPayloadBytes(const BridgePlan &plan,
                              const BridgeWorkloadEvidence &evidence,
                              arts::DbAllocOp blockAlloc) {
  if (evidence.workloadKind == BridgeWorkloadKind::halo) {
    int64_t haloBytes = getStaticHaloPayloadBytes(plan, blockAlloc);
    if (haloBytes > 0)
      return haloBytes;
  }
  return getStaticBlockPayloadBytes(blockAlloc);
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEPAYLOAD_H
