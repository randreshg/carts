///==========================================================================///
/// File: SdeMemoryLegality.cpp
///
/// Phase B substrate: block budget legality from device capacity.
///==========================================================================///

#include "carts/dialect/sde/Analysis/SdeMemoryLegality.h"

#include "carts/dialect/sde/Utils/IterationSizingUtils.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "llvm/ADT/STLExtras.h"

#include <algorithm>

namespace mlir::carts::sde {
namespace {

static ModuleOp getParentModule(Operation *op) {
  if (!op)
    return {};
  if (auto module = dyn_cast<ModuleOp>(op))
    return module;
  return op->getParentOfType<ModuleOp>();
}

static int64_t elementBytes(Value root) {
  auto type = dyn_cast_or_null<ShapedType>(root ? root.getType() : Type{});
  if (!type)
    return 0;
  Type elementType = type.getElementType();
  if (!elementType.isIntOrFloat())
    return 0;
  return std::max<int64_t>(
      1, llvm::divideCeil(elementType.getIntOrFloatBitWidth(), 8));
}

static bool validRankedShape(ArrayRef<int64_t> shape) {
  return !shape.empty() &&
         llvm::all_of(shape, [](int64_t dim) { return dim > 0; });
}

} // namespace

SdeMemoryLegality::SdeMemoryLegality(Operation *op) : operation(op) {
  compute();
}

void SdeMemoryLegality::compute() {
  maxBlockBytes.reset();
  ModuleOp module = getParentModule(operation);
  if (!module)
    return;
  auto attr = module->getAttrOfType<IntegerAttr>(MaxBlockBytesAttr);
  if (!attr || attr.getInt() <= 0)
    return;
  maxBlockBytes = attr.getInt();
}

SmallVector<int64_t, 4> SdeMemoryLegality::capBlockShapeToBudget(
    Value root, ArrayRef<int64_t> logicalShape, ArrayRef<int64_t> ownerDims,
    ArrayRef<int64_t> currentBlockShape) const {
  SmallVector<int64_t, 4> capped(currentBlockShape.begin(),
                                 currentBlockShape.end());
  if (!maxBlockBytes || *maxBlockBytes <= 0 || ownerDims.empty() ||
      capped.size() != logicalShape.size() || !validRankedShape(capped) ||
      !validRankedShape(logicalShape))
    return capped;

  int64_t bytesPerElement = elementBytes(root);
  if (bytesPerElement <= 0)
    return capped;

  auto payloadBytes = [&]() {
    return tilePayloadBytes(capped, bytesPerElement);
  };
  if (payloadBytes() <= *maxBlockBytes)
    return capped;

  SmallVector<char, 4> isOwner(capped.size(), 0);
  for (int64_t dim : ownerDims) {
    if (dim < 0 || static_cast<size_t>(dim) >= capped.size())
      return SmallVector<int64_t, 4>(currentBlockShape.begin(),
                                     currentBlockShape.end());
    isOwner[dim] = 1;
  }

  while (payloadBytes() > *maxBlockBytes) {
    int64_t selected = -1;
    for (auto [idx, extent] : llvm::enumerate(capped)) {
      if (!isOwner[idx] || extent <= 1)
        continue;
      if (selected < 0 || extent > capped[selected])
        selected = static_cast<int64_t>(idx);
    }
    if (selected < 0)
      break;
    capped[selected] = std::max<int64_t>(1, capped[selected] / 2);
  }
  return capped;
}

bool SdeMemoryLegality::isInvalidated(
    const AnalysisManager::PreservedAnalyses &pa) {
  return !pa.isPreserved<SdeMemoryLegality>();
}

} // namespace mlir::carts::sde
