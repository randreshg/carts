///==========================================================================///
/// File: CodirToArtsBridgeAcquireUtils.h
///
/// Shared bridge acquire and flat block-coordinate materialization helpers.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEACQUIREUTILS_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEACQUIREUTILS_H

#include "CodirToArtsHostCoarseDb.h"

namespace {

static inline arts::DbAcquireOp materializeBridgeAcquire(
    OpBuilder &builder, Location loc, arts::DbAllocOp alloc,
    arts::ArtsMode mode, arts::PartitionMode partitionMode,
    ArrayRef<Value> offsets, ArrayRef<Value> sizes, Value boundsValid = Value{},
    ArrayRef<Value> elementOffsets = {}, ArrayRef<Value> elementSizes = {}) {
  return arts::DbAcquireOp::create(
      builder, loc, mode, alloc.getGuid(), alloc.getPtr(), partitionMode,
      /*indices=*/SmallVector<Value>{},
      SmallVector<Value>(offsets.begin(), offsets.end()),
      SmallVector<Value>(sizes.begin(), sizes.end()),
      /*partitionIndices=*/SmallVector<Value>{},
      /*partitionOffsets=*/SmallVector<Value>{},
      /*partitionSizes=*/SmallVector<Value>{}, boundsValid,
      /*elementOffsets=*/
      SmallVector<Value>(elementOffsets.begin(), elementOffsets.end()),
      /*elementSizes=*/
      SmallVector<Value>(elementSizes.begin(), elementSizes.end()));
}

static inline arts::DbAcquireOp materializeBridgeAcquire(
    OpBuilder &builder, Location loc, arts::DbAllocOp alloc,
    arts::ArtsMode mode, arts::PartitionMode partitionMode, Value offset,
    Value size, Value boundsValid = Value{},
    ArrayRef<Value> elementOffsets = {}, ArrayRef<Value> elementSizes = {}) {
  SmallVector<Value, 1> offsets{offset};
  SmallVector<Value, 1> sizes{size};
  return materializeBridgeAcquire(builder, loc, alloc, mode, partitionMode,
                                  offsets, sizes, boundsValid, elementOffsets,
                                  elementSizes);
}

static inline Value materializeProduct(OpBuilder &builder, Location loc,
                                       ValueRange values) {
  Value product = createOneIndex(builder, loc);
  for (Value value : values)
    product = arith::MulIOp::create(builder, loc, product, value);
  return product;
}

static inline SmallVector<Value>
materializeRowMajorCoordinates(OpBuilder &builder, Location loc, Value ordinal,
                               ValueRange sizes) {
  SmallVector<Value> coords(sizes.size());
  Value remaining = ordinal;
  for (int64_t dim = static_cast<int64_t>(sizes.size()) - 1; dim >= 0; --dim) {
    Value size = sizes[dim];
    coords[dim] = arith::RemUIOp::create(builder, loc, remaining, size);
    if (dim != 0)
      remaining = arith::DivUIOp::create(builder, loc, remaining, size);
  }
  return coords;
}

struct FlatNodeBlockGroupLoop {
  scf::ForOp loop;
  Value nodeOrdinal;
  Value blockBase;
};

static inline FlatNodeBlockGroupLoop
materializeFlatNodeBlockGroupLoop(OpBuilder &builder, Location loc,
                                  Value totalNodes, Value blockCount,
                                  int64_t blockGroupSize) {
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockStep =
      createConstantIndex(builder, loc, std::max<int64_t>(1, blockGroupSize));
  Value blockGroupCount =
      materializeNonNegativeCeilDiv(builder, loc, blockCount, blockStep);
  Value workItemCount =
      arith::MulIOp::create(builder, loc, totalNodes, blockGroupCount);

  auto flatLoop = scf::ForOp::create(builder, loc, zero, workItemCount, one);
  builder.setInsertionPointToStart(flatLoop.getBody());
  Value launchOrdinal = flatLoop.getInductionVar();

  // The loop body is unreachable when blockGroupCount is zero, but keep the
  // divisor nonzero so zero-sized dynamic inputs still have well-formed IR.
  Value emptyBlocks = arith::CmpIOp::create(
      builder, loc, arith::CmpIPredicate::eq, blockGroupCount, zero);
  Value safeBlockGroupCount =
      arith::SelectOp::create(builder, loc, emptyBlocks, one, blockGroupCount);
  Value nodeOrdinal =
      arith::DivUIOp::create(builder, loc, launchOrdinal, safeBlockGroupCount);
  Value blockGroupOrdinal =
      arith::RemUIOp::create(builder, loc, launchOrdinal, safeBlockGroupCount);
  Value blockBase =
      arith::MulIOp::create(builder, loc, blockGroupOrdinal, blockStep);
  return {flatLoop, nodeOrdinal, blockBase};
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEACQUIREUTILS_H
