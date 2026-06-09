///==========================================================================///
/// File: CodirToArtsCodirDbBackedMemref.h
///
/// DB-backed memref creation from CODIR owner-slice layout facts.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_CODIRDBBACKEDMEMREF_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_CODIRDBBACKEDMEMREF_H

#include "CodirToArtsDbBackedMemref.h"
#include "CodirToArtsHaloStorage.h"

namespace {

static inline bool rankExpandedPartialReductionNeedsFullBlockShape(
    codir::CodeletOp codelet, MemRefType memrefType, ArrayAttr ownerDims,
    ArrayAttr tileShape, ArrayAttr blockShape) {
  if (!codelet || !codelet.getPartialReductionAttr())
    return false;
  std::optional<SmallVector<int64_t, 4>> owners = readI64ArrayAttr(ownerDims);
  std::optional<SmallVector<int64_t, 4>> tiles = readI64ArrayAttr(tileShape);
  if (!owners || owners->empty() || !tiles || tiles->empty())
    return false;
  if (owners->size() + tiles->size() !=
      static_cast<size_t>(memrefType.getRank()))
    return false;
  for (auto [slot, ownerDim] : llvm::enumerate(*owners))
    if (ownerDim < 0 || ownerDim != static_cast<int64_t>(slot))
      return false;
  std::optional<SmallVector<int64_t, 4>> blocks = readI64ArrayAttr(blockShape);
  return !blocks || blocks->size() != static_cast<size_t>(memrefType.getRank());
}

static inline LogicalResult
createDbBackedMemref(OpBuilder &builder, Location loc, MemRefType memrefType,
                     ValueRange dynamicSizes, Value &memref,
                     codir::CodeletOp planSource,
                     std::optional<unsigned> depIndex = std::nullopt) {
  if (!hasCodirTileOwnerSlicePlan(planSource))
    return createDbBackedMemref(builder, loc, memrefType, dynamicSizes, memref,
                                sde::SdeSuIterateOp{});

  FailureOr<SmallVector<Value>> elementSizes =
      buildElementSizes(builder, loc, memrefType, dynamicSizes);
  if (failed(elementSizes))
    return failure();

  SmallVector<Value> dbElementSizes = std::move(*elementSizes);
  ArrayAttr ownerDims = depIndex
                            ? getCodirDepOwnerDimsAttr(planSource, *depIndex)
                            : planSource.getTileOwnerDimsAttr();
  ArrayAttr blockShape =
      depIndex ? codir::getDepPhysicalBlockShapeAttr(planSource, *depIndex)
               : planSource.getTileShapeAttr();
  if (!ownerDims)
    return failure();
  if (depIndex && rankExpandedPartialReductionNeedsFullBlockShape(
                      planSource, memrefType, ownerDims,
                      planSource.getTileShapeAttr(), blockShape)) {
    planSource.emitOpError()
        << "rank-expanded partial-reduction dependency #" << *depIndex
        << " requires a full-rank physical block shape before ARTS DB "
           "materialization";
    return failure();
  }
  FailureOr<arts::DbPhysicalLayoutPlan> physicalPlan =
      arts::resolvePhysicalDbLayoutPlan(ownerDims, blockShape, dbElementSizes,
                                        builder, loc);
  if (failed(physicalPlan))
    return failure();

  SmallVector<CodirOwnerHaloWindow, 4> ownerHalos;
  if (depIndex) {
    Value backingRoot = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
        planSource.getDeps()[*depIndex]);
    ownerHalos = codirBackingBufferHaloWindows(
        backingRoot, static_cast<unsigned>(memrefType.getRank()));
    for (const CodirOwnerHaloWindow &ownerHalo : ownerHalos) {
      if (ownerHalo.empty() ||
          ownerHalo.ownerDim >= physicalPlan->innerSizes.size())
        continue;
      Value haloWidth = createConstantIndex(builder, loc, ownerHalo.width());
      physicalPlan->innerSizes[ownerHalo.ownerDim] = arith::AddIOp::create(
          builder, loc, physicalPlan->innerSizes[ownerHalo.ownerDim],
          haloWidth);
    }
  }

  Value route = arts::createCurrentNodeRoute(builder, loc);
  auto dbAlloc = arts::DbAllocOp::create(
      builder, loc, arts::ArtsMode::inout, route, arts::DbAllocType::heap,
      arts::DbMode::write, memrefType.getElementType(),
      SmallVector<Value>(physicalPlan->outerSizes.begin(),
                         physicalPlan->outerSizes.end()),
      SmallVector<Value>(physicalPlan->innerSizes.begin(),
                         physicalPlan->innerSizes.end()),
      physicalPlan->mode);
  arts::setPlanOwnerDimsAttr(dbAlloc.getOperation(), ownerDims);
  if (blockShape)
    arts::setPlanPhysicalBlockShapeAttr(dbAlloc.getOperation(), blockShape);
  if (auto haloShape = planSource.getHaloShapeAttr()) {
    arts::setPlanHaloShapeAttr(dbAlloc.getOperation(), haloShape);
  } else if (ArrayAttr haloShape = buildSymmetricPlanHaloShapeAttr(
                 dbAlloc.getContext(), ownerHalos)) {
    arts::setPlanHaloShapeAttr(dbAlloc.getOperation(), haloShape);
  }
  if (llvm::any_of(ownerHalos, [](const CodirOwnerHaloWindow &halo) {
        return !halo.empty();
      }))
    dbAlloc->setAttr(dbAlloc.getStencilSupportedBlockHaloAttrName(),
                     UnitAttr::get(dbAlloc.getContext()));

  memref = materializeInnerPayload(builder, loc, dbAlloc.getPtr());
  return success();
}

static inline arts::DbAllocOp findBackingDbAlloc(Value storage) {
  return dyn_cast_or_null<arts::DbAllocOp>(arts::DbUtils::getUnderlyingDbAlloc(
      ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storage)));
}

static inline std::optional<unsigned>
findCodirDependencyIndexForRoot(codir::CodeletOp codelet, Value root) {
  for (auto [idx, dep] : llvm::enumerate(codelet.getDeps()))
    if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) == root)
      return static_cast<unsigned>(idx);
  return std::nullopt;
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_CODIRDBBACKEDMEMREF_H
