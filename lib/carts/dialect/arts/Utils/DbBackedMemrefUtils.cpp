///==========================================================================///
/// File: DbBackedMemrefUtils.cpp
///==========================================================================///

#include "carts/dialect/arts/Utils/DbBackedMemrefUtils.h"

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbLayoutFactsUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

FailureOr<SmallVector<Value>>
mlir::carts::arts::buildDbBackedMemrefElementSizes(OpBuilder &builder,
                                                   Location loc,
                                                   MemRefType memrefType,
                                                   ValueRange dynamicSizes) {
  SmallVector<Value> elementSizes;
  if (memrefType.getRank() == 0) {
    if (!dynamicSizes.empty())
      return failure();
    elementSizes.push_back(createOneIndex(builder, loc));
    return elementSizes;
  }

  elementSizes.reserve(memrefType.getRank());
  unsigned dynamicIdx = 0;
  for (int64_t dim = 0, rank = memrefType.getRank(); dim < rank; ++dim) {
    if (memrefType.isDynamicDim(dim)) {
      if (dynamicIdx >= dynamicSizes.size())
        return failure();
      elementSizes.push_back(dynamicSizes[dynamicIdx++]);
      continue;
    }
    elementSizes.push_back(
        createConstantIndex(builder, loc, memrefType.getDimSize(dim)));
  }
  if (dynamicIdx != dynamicSizes.size())
    return failure();
  return elementSizes;
}

Value mlir::carts::arts::realizeDbInnerPayload(OpBuilder &builder, Location loc,
                                               Value sourcePtr) {
  unsigned rank = 1;
  if (auto ptrType = dyn_cast<MemRefType>(sourcePtr.getType()))
    rank = std::max<unsigned>(1, ptrType.getRank());
  SmallVector<Value> indices;
  indices.reserve(rank);
  for (unsigned idx = 0; idx < rank; ++idx)
    indices.push_back(createZeroIndex(builder, loc));
  return DbRefOp::create(builder, loc, sourcePtr, indices);
}

LogicalResult mlir::carts::arts::createCoarseDbBackedMemref(
    OpBuilder &builder, Location loc, MemRefType memrefType,
    ValueRange dynamicSizes, Value &memref) {
  // Callers must reject rank-expanded block-grid memrefs before reaching here;
  // those require per-block DB realization or an explicit fail-closed diagnostic
  // at the SDE-to-ARTS storage boundary.
  FailureOr<SmallVector<Value>> elementSizes =
      buildDbBackedMemrefElementSizes(builder, loc, memrefType, dynamicSizes);
  if (failed(elementSizes))
    return failure();

  SmallVector<Value> sizes{createOneIndex(builder, loc)};
  Type pointerElementType =
      getElementMemRefType(memrefType.getElementType(), memrefType.getRank());
  Type pointerType =
      MemRefType::get({ShapedType::kDynamic}, pointerElementType);
  Value route = createCurrentNodeRoute(builder, loc);
  auto dbAlloc = DbAllocOp::create(
      builder, loc, ArtsMode::inout, route, DbAllocType::heap, DbMode::write,
      memrefType.getElementType(), pointerType, std::move(sizes),
      std::move(*elementSizes), PartitionMode::coarse);
  memref = realizeDbInnerPayload(builder, loc, dbAlloc.getPtr());
  return success();
}

LogicalResult mlir::carts::arts::createBlockDbBackedMemref(
    OpBuilder &builder, Location loc, MemRefType memrefType,
    ValueRange dynamicSizes, ArrayAttr ownerDims, ArrayAttr blockShape,
    Value &memref) {
  FailureOr<SmallVector<Value>> elementSizes =
      buildDbBackedMemrefElementSizes(builder, loc, memrefType, dynamicSizes);
  if (failed(elementSizes))
    return failure();

  FailureOr<DbPhysicalLayoutFacts> physicalFacts = resolvePhysicalDbLayoutFacts(
      ownerDims, blockShape, *elementSizes, builder, loc);
  if (failed(physicalFacts))
    return failure();

  Value route = createCurrentNodeRoute(builder, loc);
  auto dbAlloc =
      DbAllocOp::create(builder, loc, ArtsMode::inout, route, DbAllocType::heap,
                        DbMode::write, memrefType.getElementType(),
                        SmallVector<Value>(physicalFacts->outerSizes.begin(),
                                           physicalFacts->outerSizes.end()),
                        SmallVector<Value>(physicalFacts->innerSizes.begin(),
                                           physicalFacts->innerSizes.end()),
                        physicalFacts->mode);
  memref = realizeDbInnerPayload(builder, loc, dbAlloc.getPtr());
  return success();
}

DbAllocOp mlir::carts::arts::resolveBoundaryDbAlloc(Value memref) {
  if (!memref)
    return nullptr;
  if (auto alloc =
          dyn_cast_or_null<DbAllocOp>(DbUtils::getUnderlyingDbAlloc(memref)))
    return alloc;

  Value root = ValueAnalysis::stripMemrefViewOps(memref);
  auto result = dyn_cast<OpResult>(root);
  if (!result)
    return nullptr;
  auto cu = dyn_cast_or_null<sde::SdeCuRegionOp>(result.getOwner());
  if (!cu || cu.getBody().empty())
    return nullptr;
  auto yield =
      dyn_cast_or_null<sde::SdeYieldOp>(cu.getBody().front().getTerminator());
  if (!yield || result.getResultNumber() >= yield.getValues().size())
    return nullptr;
  return resolveBoundaryDbAlloc(yield.getValues()[result.getResultNumber()]);
}
