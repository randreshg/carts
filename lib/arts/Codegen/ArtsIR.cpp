///==========================================================================
/// File: ArtsIR.cpp
///==========================================================================
#include "arts/Codegen/ArtsIR.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "polygeist/Ops.h"

namespace mlir {
namespace arts {

EdtOp createEdtOp(OpBuilder &builder, Location loc, types::EdtType type,
                  SmallVector<Value> deps) {
  /// Create an arts.edt operation.
  auto edtOp = builder.create<EdtOp>(loc, deps);
  if (type == types::EdtType::Parallel)
    edtOp.setIsParallelAttr();
  else if (type == types::EdtType::Single)
    edtOp.setIsSingleAttr();
  else if (type == types::EdtType::Task)
    edtOp.setIsTaskAttr();
  return edtOp;
}

types::EdtType getEdtType(EdtOp edtOp) {
  if (edtOp.isParallel())
    return types::EdtType::Parallel;
  if (edtOp.isSingle())
    return types::EdtType::Single;
  if (edtOp.isTask())
    return types::EdtType::Task;
  return types::EdtType::Task;
}

DataBlockOp createDatablockOp(OpBuilder &builder, Location loc,
                              types::DatablockAccessType mode, Value ptr,
                              SmallVector<Value> pinnedIndices) {
  bool isLoad = false;
  auto ptrOp = ptr.getDefiningOp();
  assert(ptrOp && "Input must be a defining operation.");

  /// If the defining operation is a load op obtain the base memref and pinned
  /// indices.
  Value baseMemRef;

  /// A lambda to handle common load operations.
  auto processLoad = [&](auto loadOp) {
    baseMemRef = loadOp.getMemref();
    if (pinnedIndices.empty())
      pinnedIndices.assign(loadOp.getIndices().begin(),
                           loadOp.getIndices().end());
    isLoad = true;
  };

  if (auto loadOp = dyn_cast<memref::LoadOp>(ptrOp)) {
    processLoad(loadOp);
  } else {
    baseMemRef = ptr;
  }

  /// Ensure the input memref is of type MemRefType.
  auto baseType = baseMemRef.getType().dyn_cast<MemRefType>();
  assert(baseType && "Input must be a MemRefType.");

  /// Compute the rank and verify it is positive.
  int64_t rank = baseType.getRank();

  /// Prepare arrays for subview
  const auto pinnedCount = static_cast<int64_t>(pinnedIndices.size());
  SmallVector<Value, 4> indices(pinnedCount), sizes(rank - pinnedCount);
  SmallVector<int64_t, 4> subShape(rank - pinnedCount);

  /// Compute dimension sizes, indices, sizes, and subshape
  // Value oneSize = builder.create<arith::ConstantIndexOp>(loc, 1);
  for (int64_t i = 0; i < rank; ++i) {
    if (i < pinnedCount) {
      indices[i] = pinnedIndices[i];
    } else {
      bool isDyn = baseType.isDynamicDim(i);
      int64_t dimSize = baseType.getDimSize(i);
      Value dimVal =
          isDyn ? builder.create<memref::DimOp>(loc, baseMemRef, i).getResult()
                : builder.create<arith::ConstantIndexOp>(loc, dimSize)
                      .getResult();
      sizes[i - pinnedCount] = dimVal;
      subShape[i - pinnedCount] = isDyn ? ShapedType::kDynamic : dimSize;
    }
  }

  /// Build the subview memref type.
  auto elementType = baseType.getElementType();
  auto subMemRefType = MemRefType::get(subShape, elementType);

  /// Insert polygeist.typeSizeOp to get the size of the element type.
  auto elementTypeSize = builder
                             .create<polygeist::TypeSizeOp>(
                                 loc, builder.getIndexType(), elementType)
                             .getResult();

  /// Create the final arts.datablock operation.
  auto modeAttr = builder.getStringAttr(types::toString(mode));
  DataBlockOp depOp = builder.create<arts::DataBlockOp>(
      loc, subMemRefType, modeAttr, baseMemRef, elementType, elementTypeSize,
      indices, sizes);

  if (isLoad)
    depOp.setIsLoadAttr();
  return depOp;
}

} // namespace arts
} // namespace mlir
