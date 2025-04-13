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
  else if (type == types::EdtType::Sync)
    edtOp.setIsSyncAttr();
  else if (type == types::EdtType::Task)
    edtOp.setIsTaskAttr();
  return edtOp;
}

types::EdtType getEdtType(EdtOp edtOp) {
  if (edtOp.isParallel())
    return types::EdtType::Parallel;
  if (edtOp.isSingle())
    return types::EdtType::Single;
  if (edtOp.isSync())
    return types::EdtType::Sync;
  if (edtOp.isTask())
    return types::EdtType::Task;
  return types::EdtType::Task;
}

DataBlockOp createDatablockOp(OpBuilder &builder, Location loc,
                              types::DatablockAccessType mode, Value ptr,
                              SmallVector<Value> pinnedIndices,
                              bool coarseGrained) {
  auto ptrOp = ptr.getDefiningOp();
  assert(ptrOp && "Input must be a defining operation.");

  /// If the defining op is a load op, obtain the base memref and pinned
  /// indices.
  Value baseMemRef;
  auto processLoad = [&](auto loadOp) {
    baseMemRef = loadOp.getMemref();
    if (pinnedIndices.empty())
      pinnedIndices.assign(loadOp.getIndices().begin(),
                           loadOp.getIndices().end());
  };

  if (auto loadOp = dyn_cast<memref::LoadOp>(ptrOp))
    processLoad(loadOp);
  else
    baseMemRef = ptr;

  /// Ensure the base memref type.
  auto baseType = baseMemRef.getType().dyn_cast<MemRefType>();
  assert(baseType && "Input must be a MemRefType.");

  int64_t rank = baseType.getRank();
  const auto pinnedCount = static_cast<int64_t>(pinnedIndices.size());
  assert(pinnedCount <= rank &&
         "Pinned indices exceed the rank of the memref.");

  /// Compute sizes and subshape.
  SmallVector<Value, 4> indices(pinnedCount), sizes(rank - pinnedCount);
  SmallVector<int64_t, 4> subShape;
  for (int64_t i = 0, j = 0; i < rank; ++i) {
    if (i < pinnedCount) {
      indices[i] = pinnedIndices[i];
    } else {
      bool isDyn = baseType.isDynamicDim(i);
      int64_t dimSize = baseType.getDimSize(i);
      Value dimVal =
          isDyn ? builder.create<memref::DimOp>(loc, baseMemRef, i).getResult()
                : builder.create<arith::ConstantIndexOp>(loc, dimSize)
                      .getResult();
      sizes[j] = dimVal;
      /// For the normal subview type, preserve the static dim if available.
      subShape.push_back(isDyn ? ShapedType::kDynamic : dimSize);
      j++;
    }
  }

  /// Compute the element type size.
  auto elementType = baseType.getElementType();
  auto elementTypeSize = builder
                             .create<polygeist::TypeSizeOp>(
                                 loc, builder.getIndexType(), elementType)
                             .getResult();

  auto modeAttr = builder.getStringAttr(types::toString(mode));
  auto resultType = MemRefType::get(subShape, elementType);
  if (coarseGrained && indices.empty()) {
    auto elementTySize = elementTypeSize;
    for (auto size : sizes) {
      elementTySize =
          builder.create<arith::MulIOp>(loc, elementTySize, size).getResult();
    }
    SmallVector<Value, 4> newSizes;
    return builder.create<arts::DataBlockOp>(loc, resultType, modeAttr,
                                             baseMemRef, resultType,
                                             elementTySize, indices, newSizes);
  } else {
    return builder.create<arts::DataBlockOp>(loc, resultType, modeAttr,
                                             baseMemRef, elementType,
                                             elementTypeSize, indices, sizes);
  }
}

} // namespace arts
} // namespace mlir
