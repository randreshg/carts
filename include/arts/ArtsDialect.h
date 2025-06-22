//===- Dialect.h - Arts dialect --------------------------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

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

//===----------------------------------------------------------------------===//
// Arts Dialect
//===----------------------------------------------------------------------===//
#include "arts/ArtsOpsDialect.h.inc"
namespace mlir {
namespace arts {
/// DbAccessOp represents operations that can be used as datablock access
/// operations This includes both SubViewOp (original case) and CastOp (after
/// canonicalization)
struct DbAccessOp {
  mlir::Operation *op;

  DbAccessOp() : op(nullptr) {}
  DbAccessOp(mlir::memref::SubViewOp subview) : op(subview.getOperation()) {}
  DbAccessOp(mlir::memref::CastOp cast) : op(cast.getOperation()) {}
  DbAccessOp(mlir::Operation *operation) : op(operation) {}

  mlir::Operation *getOperation() const { return op; }
  mlir::Value getResult() const {
    return op ? op->getResult(0) : mlir::Value();
  }

  operator bool() const { return op != nullptr; }
  bool operator==(const DbAccessOp &other) const { return op == other.op; }
  bool operator!=(const DbAccessOp &other) const { return op != other.op; }

  /// Check if this is a SubViewOp
  bool isSubView() const {
    return op && mlir::isa<mlir::memref::SubViewOp>(op);
  }
  /// Check if this is a CastOp
  bool isCast() const { return op && mlir::isa<mlir::memref::CastOp>(op); }

  /// Get as SubViewOp (returns null if not a SubViewOp)
  mlir::memref::SubViewOp getAsSubView() const {
    return isSubView() ? mlir::cast<mlir::memref::SubViewOp>(op)
                       : mlir::memref::SubViewOp();
  }

  /// Get as CastOp (returns null if not a CastOp)
  mlir::memref::CastOp getAsCast() const {
    return isCast() ? mlir::cast<mlir::memref::CastOp>(op)
                    : mlir::memref::CastOp();
  }
};

/// Helper function to get DbAccessOp from a Value's defining operation
inline DbAccessOp getDbAccessOp(mlir::Value value) {
  assert(value && "Value cannot be null");
  mlir::Operation *defOp = value.getDefiningOp();
  assert(defOp && "Value must have a defining operation");

  if (auto subview = mlir::dyn_cast<mlir::memref::SubViewOp>(defOp))
    return DbAccessOp(subview);
  if (auto cast = mlir::dyn_cast<mlir::memref::CastOp>(defOp))
    return DbAccessOp(cast);

  llvm_unreachable(
      "Operation must be either SubViewOp or CastOp for DbAccessOp");
}

/// Helper function to get DbAccessOp from an Operation
inline DbAccessOp getDbAccessOp(mlir::Operation *op) {
  assert(op && "Operation cannot be null");

  if (auto subview = mlir::dyn_cast<mlir::memref::SubViewOp>(op))
    return DbAccessOp(subview);
  if (auto cast = mlir::dyn_cast<mlir::memref::CastOp>(op))
    return DbAccessOp(cast);

  llvm_unreachable(
      "Operation must be either SubViewOp or CastOp for DbAccessOp");
}

} // namespace arts
} // namespace mlir

bool isArtsRegion(mlir::Operation *op);
bool isArtsOp(mlir::Operation *op);
//===----------------------------------------------------------------------===//
// Arts Dialect Types
//===----------------------------------------------------------------------===//
#define GET_TYPEDEF_CLASSES
#include "arts/ArtsOpsTypes.h.inc"

//===----------------------------------------------------------------------===//
// Arts Dialect Attributes
//===----------------------------------------------------------------------===//
#define GET_ATTRDEF_CLASSES
#include "arts/ArtsOpsAttributes.h.inc"

//===----------------------------------------------------------------------===//
// Arts Dialect Operations
//===----------------------------------------------------------------------===//
#define GET_OP_CLASSES
#include "arts/ArtsOps.h.inc"

//===----------------------------------------------------------------------===//
// DenseMapInfo specialization for DbAccessOp
//===----------------------------------------------------------------------===//
namespace llvm {
template <> struct DenseMapInfo<mlir::arts::DbAccessOp> {
  static mlir::arts::DbAccessOp getEmptyKey() {
    auto ptr = llvm::DenseMapInfo<void *>::getEmptyKey();
    return mlir::arts::DbAccessOp(static_cast<mlir::Operation *>(ptr));
  }

  static mlir::arts::DbAccessOp getTombstoneKey() {
    auto ptr = llvm::DenseMapInfo<void *>::getTombstoneKey();
    return mlir::arts::DbAccessOp(static_cast<mlir::Operation *>(ptr));
  }

  static unsigned getHashValue(const mlir::arts::DbAccessOp &val) {
    return llvm::DenseMapInfo<mlir::Operation *>::getHashValue(
        val.getOperation());
  }

  static bool isEqual(const mlir::arts::DbAccessOp &lhs,
                      const mlir::arts::DbAccessOp &rhs) {
    return lhs.getOperation() == rhs.getOperation();
  }
};
} // namespace llvm

#endif // CARTS_DIALECT_H
