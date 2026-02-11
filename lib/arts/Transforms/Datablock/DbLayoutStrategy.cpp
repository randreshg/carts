///==========================================================================///
/// File: DbLayoutStrategy.cpp
///
/// Strategy implementations for datablock layout addressing.
///==========================================================================///

#include "arts/Transforms/Datablock/DbLayoutStrategy.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/DatablockUtils.h"
#include "mlir/IR/Operation.h"
#include "llvm/Support/ErrorHandling.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

class LinearizedLayoutStrategy : public DbLayoutStrategy {
public:
  Value computeElementPointer(ArtsCodegen &AC, Location loc, Value base,
                              ArrayRef<Value> indices,
                              const LayoutInfo &layout) const override {
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
};

} // namespace

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

const DbLayoutStrategy &mlir::arts::getDbLayoutStrategy(PartitionMode mode) {
  static LinearizedLayoutStrategy linearized;

  switch (mode) {
  case PartitionMode::coarse:
  case PartitionMode::fine_grained:
  case PartitionMode::block:
  case PartitionMode::stencil:
    return linearized;
  }

  llvm_unreachable("unknown partition mode");
}
