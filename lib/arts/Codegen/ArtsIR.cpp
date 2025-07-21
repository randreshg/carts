///==========================================================================
/// File: ArtsIR.cpp
///
/// This file provides utility functions for creating and manipulating ARTS IR
/// operations. It is organized into logical sections for EDT operations,
/// DataBlock operations, Event operations, Runtime query operations,
/// and helper functions.
///==========================================================================

#include "arts/Codegen/ArtsIR.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
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
// Event Operations
//===----------------------------------------------------------------------===//

EventOp createEventOp(OpBuilder &builder, Location loc, ArrayRef<Value> sizes) {
  SmallVector<Value> eventSizes(sizes.begin(), sizes.end());
  // Assuming event type is memref<?x...xi8> or similar; adjust if needed
  // For now, use unranked or based on sizes
  SmallVector<int64_t> shape(sizes.size(), ShapedType::kDynamic);
  MemRefType eventType = MemRefType::get(shape, builder.getI8Type());
  return builder.create<EventOp>(loc, eventType, eventSizes);
}

//===----------------------------------------------------------------------===//
// DataBlock Creation Functions
//===----------------------------------------------------------------------===//

DbAllocOp createDbAllocOp(OpBuilder &builder, Location loc, ArtsMode mode,
                          Value address, SmallVector<Value> sizes) {
  MemRefType ptrType;

  if (address) {
    auto addressType = address.getType().dyn_cast<MemRefType>();
    assert(addressType && "Expected a MemRefType");

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
    ptrType =
        MemRefType::get(addressType.getShape(), addressType.getElementType());
  } else {
    /// If no address is provided, we need to infer the type from sizes or use a
    /// default For now, create a basic i32 memref type - this should be refined
    /// based on usage
    SmallVector<int64_t> shape;
    for (auto _ : sizes)
      shape.push_back(ShapedType::kDynamic);
    ptrType = MemRefType::get(shape, builder.getI32Type());
  }

  /// Create the DbAllocOp with optional allocType parameter (nullptr for
  /// default)
  return builder.create<DbAllocOp>(loc, ptrType, mode, address, sizes, nullptr);
}

DbAcquireOp createDbAcquireOp(OpBuilder &builder, Location loc, ArtsMode mode,
                              Value source, SmallVector<Value> pinnedIndices,
                              SmallVector<Value> offsets,
                              SmallVector<Value> sizes) {
  auto resultType = source.getType().dyn_cast<MemRefType>();
  assert(resultType && "Source must be a MemRefType");

  // Adjust result type if pinned indices reduce rank
  int64_t originalRank = resultType.getRank();
  int64_t pinnedCount = pinnedIndices.size();
  if (pinnedCount > 0) {
    SmallVector<int64_t> newShape;
    for (int64_t i = pinnedCount; i < originalRank; ++i)
      newShape.push_back(resultType.getDimSize(i));
    resultType = MemRefType::get(newShape, resultType.getElementType());
  }

  return builder.create<DbAcquireOp>(loc, resultType, mode, source,
                                     pinnedIndices, offsets, sizes);
}

DbReleaseOp createDbReleaseOp(OpBuilder &builder, Location loc,
                              Value source) {
  return builder.create<DbReleaseOp>(loc, source);
}

DbControlOp createDbControlOp(OpBuilder &builder, Location loc, ArtsMode mode,
                              Value ptr, SmallVector<Value> pinnedIndices) {
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

  auto resultType = MemRefType::get(subShape, elementType);
  return builder.create<DbControlOp>(loc, resultType, mode, baseMemRef,
                                     elementType, elementTypeSize, indices,
                                     offsets, sizes);
}


//===----------------------------------------------------------------------===//
// Allocation Type Analysis and Classification
//===----------------------------------------------------------------------===//

// DbAllocType getAllocType(Value value) {
//   if (!value)
//     return DbAllocType::unknown;

//   Operation *definingOp = value.getDefiningOp();
//   if (!definingOp)
//     return DbAllocType::parameter;

//   if (isa<memref::AllocaOp>(definingOp))
//     return DbAllocType::stack;
//   if (isa<memref::AllocOp>(definingOp) || isa<AllocOp>(definingOp))
//     return DbAllocType::heap;
//   if (isa<memref::GetGlobalOp>(definingOp))
//     return DbAllocType::global;
//   if (auto callOp = dyn_cast<func::CallOp>(definingOp))
//     return DbAllocType::dynamic;
//   if (isa<memref::SubViewOp>(definingOp))
//     return getAllocType(cast<memref::SubViewOp>(definingOp).getSource());
//   if (isa<memref::CastOp>(definingOp))
//     return getAllocType(cast<memref::CastOp>(definingOp).getSource());

//   if (isa<DbAcquireOp>(definingOp))
//     return getAllocType(cast<DbAcquireOp>(definingOp).getSource());

//   return DbAllocType::unknown;
// }


} // namespace arts
} // namespace mlir