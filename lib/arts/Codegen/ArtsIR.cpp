///==========================================================================
/// File: ArtsIR.cpp
///
/// This file provides utility functions for creating and manipulating ARTS IR
/// operations. It is organized into logical sections for EDT operations,
/// DataBlock operations, and helper functions.
///==========================================================================

#include "arts/Codegen/ArtsIR.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Ops.h"

/// Debug
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "arts-ir"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

namespace mlir {
namespace arts {

//===----------------------------------------------------------------------===//
// EDT Creation and Manipulation Functions
//===----------------------------------------------------------------------===//

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

//===----------------------------------------------------------------------===//
// DataBlock Creation Functions
//===----------------------------------------------------------------------===//

DbAllocOp createDbAllocOp(OpBuilder &builder, Location loc, StringRef mode,
                          Value address, SmallVector<Value> sizes) {
  auto addressType = address.getType().cast<MemRefType>();

  /// If sizes are not provided, extract them from the address memref
  if (sizes.empty()) {
    for (int64_t i = 0; i < addressType.getRank(); ++i) {
      if (addressType.isDynamicDim(i)) {
        sizes.push_back(builder.create<memref::DimOp>(loc, address, i));
      } else {
        sizes.push_back(builder.create<arith::ConstantIndexOp>(
            loc, addressType.getDimSize(i)));
      }
    }
  }

  /// Create result types
  auto ptrType =
      MemRefType::get(addressType.getShape(), addressType.getElementType());

  /// Create the DbAllocOp with optional allocType parameter (nullptr for
  /// default)
  return builder.create<DbAllocOp>(loc, ptrType, builder.getStringAttr(mode),
                                   address, sizes, nullptr);
}

DbControlOp createDbControlOp(OpBuilder &builder, Location loc,
                              types::DbAccessType mode, Value ptr,
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
  SmallVector<Value, 4> offsets(rank - pinnedCount,
                                builder.create<arith::ConstantIndexOp>(loc, 0));
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
    return builder.create<arts::DbControlOp>(
        loc, resultType, modeAttr, baseMemRef, resultType, elementTySize,
        indices, offsets, newSizes);
  } else {
    return builder.create<arts::DbControlOp>(
        loc, resultType, modeAttr, baseMemRef, elementType, elementTypeSize,
        indices, offsets, sizes);
  }
}

//===----------------------------------------------------------------------===//
// DataBlock Helper Functions
//===----------------------------------------------------------------------===//

bool isDbAllocOp(Operation *op) { return isa<DbAllocOp>(op); }

bool isDbAccessOp(Operation *op) {
  return isa<memref::SubViewOp>(op) || isa<memref::CastOp>(op);
}

bool isDbControlOp(Operation *op) { return isa<DbControlOp>(op); }

//===----------------------------------------------------------------------===//
// DataBlock Attribute Management
//===----------------------------------------------------------------------===//

types::DbAllocType getDbAllocType(DbAllocOp dbAllocOp) {
  if (auto allocTypeAttr = dbAllocOp->getAttrOfType<StringAttr>("allocType"))
    return types::getDbAllocType(allocTypeAttr.getValue());
  return types::DbAllocType::Unknown;
}

void setDbAllocType(DbAllocOp dbAllocOp, types::DbAllocType type,
                    OpBuilder &builder) {
  dbAllocOp->setAttr("allocType", builder.getStringAttr(types::toString(type)));
}

//===----------------------------------------------------------------------===//
// Allocation Type Analysis and Classification
//===----------------------------------------------------------------------===//

types::DbAllocType getAllocType(Value ptr) {
  if (!ptr)
    return types::DbAllocType::Unknown;

  Operation *definingOp = ptr.getDefiningOp();
  if (!definingOp)
    return types::DbAllocType::Parameter;

  if (isa<memref::AllocaOp>(definingOp))
    return types::DbAllocType::Stack;
  if (isa<memref::AllocOp>(definingOp))
    return types::DbAllocType::Heap;
  if (isa<memref::GetGlobalOp>(definingOp))
    return types::DbAllocType::Global;
  if (auto callOp = dyn_cast<func::CallOp>(definingOp))
    return types::DbAllocType::Dynamic;
  if (isa<memref::SubViewOp>(definingOp))
    return getAllocType(cast<memref::SubViewOp>(definingOp).getSource());
  if (isa<memref::CastOp>(definingOp))
    return getAllocType(cast<memref::CastOp>(definingOp).getSource());

  return types::DbAllocType::Unknown;
}

bool isStackAlloc(DbAllocOp dbAllocOp) {
  auto allocType = getDbAllocType(dbAllocOp);
  return allocType == types::DbAllocType::Stack ||
         allocType == types::DbAllocType::Parameter;
}

bool isDynamicAlloc(DbAllocOp dbAllocOp) {
  auto allocType = getDbAllocType(dbAllocOp);
  return allocType == types::DbAllocType::Heap ||
         allocType == types::DbAllocType::Dynamic;
}

bool isGlobalAlloc(DbAllocOp dbAllocOp) {
  auto allocType = getDbAllocType(dbAllocOp);
  return allocType == types::DbAllocType::Global;
}

} // namespace arts
} // namespace mlir
