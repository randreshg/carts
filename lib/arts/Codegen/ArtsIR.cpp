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
  MemRefType ptrType;
  
  if (address) {
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
    ptrType = MemRefType::get(addressType.getShape(), addressType.getElementType());
  } else {
    /// If no address is provided, we need to infer the type from sizes or use a default
    /// For now, create a basic i32 memref type - this should be refined based on usage
    SmallVector<int64_t> shape;
    for (auto size : sizes) {
      shape.push_back(ShapedType::kDynamic); // Dynamic dimensions
    }
    ptrType = MemRefType::get(shape, builder.getI32Type());
  }

  /// Create the DbAllocOp with optional allocType parameter (nullptr for default)
  return builder.create<DbAllocOp>(loc, ptrType, builder.getStringAttr(mode),
                                   address, sizes, nullptr);
}

DbDepOp createDbDepOp(OpBuilder &builder, Location loc,
                  types::DbDepType mode, Value source,
                  SmallVector<Value> pinnedIndices,
                  SmallVector<Value> offsets,
                  SmallVector<Value> sizes) {
  auto modeAttr = builder.getStringAttr(types::toString(mode));
  auto resultType = source.getType();
  
  return builder.create<arts::DbDepOp>(
      loc, resultType, modeAttr, source, pinnedIndices, offsets, sizes);
}

//===----------------------------------------------------------------------===//
// DataBlock Helper Functions
//===----------------------------------------------------------------------===//

bool isDbAllocOp(Operation *op) { return isa<DbAllocOp>(op); }

bool isDbDepOp(Operation *op) {
  return isa<DbDepOp>(op);
}

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
