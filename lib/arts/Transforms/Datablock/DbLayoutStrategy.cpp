///==========================================================================///
/// File: DbLayoutStrategy.cpp
///
/// Layout-aware element pointer computation for datablock lowering.
///==========================================================================///

#include "arts/Transforms/Datablock/DbLayoutStrategy.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/DatablockUtils.h"
#include "mlir/IR/Operation.h"

using namespace mlir;
using namespace mlir::arts;

LayoutInfo mlir::arts::buildLayoutInfo(Value source) {
  LayoutInfo info;

  if (!source)
    return info;

  if (Operation *rootAllocOp = DatablockUtils::getUnderlyingDbAlloc(source)) {
    if (auto alloc = dyn_cast<DbAllocOp>(rootAllocOp)) {
      info.alloc = alloc;
      info.mode = DatablockUtils::getPartitionModeFromStructure(alloc);
      info.sizes.assign(alloc.getSizes().begin(), alloc.getSizes().end());
      info.elementSizes.assign(alloc.getElementSizes().begin(),
                               alloc.getElementSizes().end());
    }
  }

  if (info.sizes.empty()) {
    SmallVector<Value> sizes = DatablockUtils::getSizesFromDb(source);
    info.sizes.assign(sizes.begin(), sizes.end());
  }

  info.outerRank = static_cast<unsigned>(info.sizes.size());
  info.innerRank = static_cast<unsigned>(info.elementSizes.size());
  return info;
}

Value mlir::arts::computeDbElementPointer(ArtsCodegen &AC, Location loc,
                                          Value base, ArrayRef<Value> indices,
                                          const LayoutInfo &layout) {
  if (indices.empty())
    return AC.castToLLVMPtr(base, loc);

  SmallVector<Value> fallbackSizes;
  ArrayRef<Value> sizes = layout.sizes;
  if (sizes.empty()) {
    fallbackSizes = DatablockUtils::getSizesFromDb(base);
    sizes = fallbackSizes;
  }

  SmallVector<Value> strides = AC.computeStridesFromSizes(sizes, loc);
  SmallVector<Value> linearIndices(indices.begin(), indices.end());
  return AC.create<DbGepOp>(loc, AC.getLLVMPointerType(base), base,
                            linearIndices, strides);
}
