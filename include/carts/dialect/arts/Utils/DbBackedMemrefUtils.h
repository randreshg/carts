///==========================================================================///
/// File: DbBackedMemrefUtils.h
///
/// Helpers for building memref values backed by ARTS DB allocations.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_UTILS_DBBACKEDMEMREFUTILS_H
#define CARTS_DIALECT_ARTS_UTILS_DBBACKEDMEMREFUTILS_H

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Support/LLVM.h"

namespace mlir {
namespace carts::arts {

FailureOr<SmallVector<Value>>
buildDbBackedMemrefElementSizes(OpBuilder &builder, Location loc,
                                MemRefType memrefType, ValueRange dynamicSizes);

Value realizeDbInnerPayload(OpBuilder &builder, Location loc, Value sourcePtr);

LogicalResult createCoarseDbBackedMemref(OpBuilder &builder, Location loc,
                                         MemRefType memrefType,
                                         ValueRange dynamicSizes,
                                         Value &memref);

LogicalResult createBlockDbBackedMemref(OpBuilder &builder, Location loc,
                                        MemRefType memrefType,
                                        ValueRange dynamicSizes,
                                        ArrayAttr ownerDims,
                                        ArrayAttr blockShape, Value &memref,
                                        bool dropPromotedOwnerDims = false);

/// Resolve the ARTS DbAlloc backing a boundary memref, including CU-yielded
/// aliases produced during SDE storage realization.
DbAllocOp resolveBoundaryDbAlloc(Value memref);

} // namespace carts::arts
} // namespace mlir

#endif // CARTS_DIALECT_ARTS_UTILS_DBBACKEDMEMREFUTILS_H
