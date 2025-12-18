///==========================================================================///

//===- Dialect.h - Arts dialect --------------------------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
///===----------------------------------------------------------------------===///

#ifndef CARTS_DIALECT_H
#define CARTS_DIALECT_H

/// Dialects
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
/// Others
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dialect.h"
#include "mlir/IR/Matchers.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Value.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/DenseMapInfo.h"
#include <optional>

///===----------------------------------------------------------------------===///
// Arts Dialect
///===----------------------------------------------------------------------===///
#include "arts/ArtsOpsDialect.h.inc"

bool isArtsRegion(mlir::Operation *op);
bool isArtsOp(mlir::Operation *op);

///===----------------------------------------------------------------------===///
// Arts Dialect Types
///===----------------------------------------------------------------------===///
#define GET_TYPEDEF_CLASSES
#include "arts/ArtsOpsTypes.h.inc"

///===----------------------------------------------------------------------===///
// Arts Dialect Enums
///===----------------------------------------------------------------------===///
#include "arts/ArtsOpsEnums.h.inc"

///===----------------------------------------------------------------------===///
// Arts Dialect Attributes
///===----------------------------------------------------------------------===///
#define GET_ATTRDEF_CLASSES
#include "arts/ArtsOpsAttributes.h.inc"

///===----------------------------------------------------------------------===///
// Arts Dialect Operations
///===----------------------------------------------------------------------===///
#define GET_OP_CLASSES
#include "arts/ArtsOps.h.inc"

///===----------------------------------------------------------------------===///
// Arts Dialect Utility Functions
///===----------------------------------------------------------------------===///

/// Helper function to extract sizes from a datablock pointer
/// by finding the original DbAllocOp or DbAcquireOp that created it
mlir::SmallVector<mlir::Value> getSizesFromDb(mlir::Operation *dbOp);
mlir::SmallVector<mlir::Value> getElementSizesFromDb(mlir::Operation *dbOp);
mlir::SmallVector<mlir::Value> getSizesFromDb(mlir::Value datablockPtr);
mlir::SmallVector<mlir::Value> getOffsetsFromDb(mlir::Value datablockPtr);

/// Check if a datablock operation has a single size of 1
bool dbHasSingleSize(mlir::Operation *dbOp);

//===----------------------------------------------------------------------===//
// Stride Computation Functions
//===----------------------------------------------------------------------===//
// These functions compute the linearization stride for row-major indexing.
// For sizes = [D0, D1, D2, ...], stride = D1 * D2 * ... (TRAILING dimensions).
// This follows row-major linearization: index = i0 * stride + ...
//
// Examples:
//   [4, 16]     -> stride = 16
//   [8, 4, 2]   -> stride = 4 * 2 = 8
//   [64]        -> stride = 1 (single dimension)
//===----------------------------------------------------------------------===//

/// Compute static stride from sizes (compile-time constant).
/// Returns std::nullopt if any trailing dimension is dynamic.
std::optional<int64_t> getStaticStride(mlir::ValueRange sizes);

/// Overload for MemRefType shape.
std::optional<int64_t> getStaticStride(mlir::MemRefType memrefType);

/// Get element stride from DbAllocOp (uses getElementSizes()).
/// This is the stride for linearized access WITHIN each datablock element.
std::optional<int64_t> getStaticElementStride(mlir::arts::DbAllocOp alloc);

/// Get outer stride from DbAllocOp (uses getSizes()).
/// This is the stride for linearized access ACROSS datablocks.
std::optional<int64_t> getStaticOuterStride(mlir::arts::DbAllocOp alloc);

/// Compute stride as a Value (handles both static and dynamic dimensions).
/// If all trailing dimensions are static, returns an arith::ConstantIndexOp.
/// If any trailing dimension is dynamic, generates arith::MulIOp chain.
/// For single-dimension [N], returns constant 1.
/// Returns nullptr if sizes is empty.
mlir::Value getStrideValue(mlir::OpBuilder &builder, mlir::Location loc,
                           mlir::ValueRange sizes);

/// Get element stride Value from DbAllocOp (uses getElementSizes()).
mlir::Value getElementStrideValue(mlir::OpBuilder &builder, mlir::Location loc,
                                  mlir::arts::DbAllocOp alloc);

/// Get outer stride Value from DbAllocOp (uses getSizes()).
mlir::Value getOuterStrideValue(mlir::OpBuilder &builder, mlir::Location loc,
                                mlir::arts::DbAllocOp alloc);

#endif // CARTS_DIALECT_H
