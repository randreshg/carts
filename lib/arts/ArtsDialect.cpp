//===- ArtsDialect.cpp - Arts dialect ---------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/Utils/Utils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/IntegerSet.h"
#include "llvm/ADT/SetVector.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/TypeSwitch.h"
#include "llvm/Support/Debug.h"


#include "arts/ArtsDialect.h"

using namespace mlir;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// Arts dialect.
//===----------------------------------------------------------------------===//
void ArtsDialect::initialize() {
  /// Register operations
  addOperations<
    #define GET_OP_LIST
    #include "arts/ArtsOps.cpp.inc"
          >();

  /// Register types
  addTypes<
    #define GET_TYPEDEF_LIST
    #include "arts/ArtsOpsTypes.cpp.inc"
      >();
}

#include "arts/ArtsOpsDialect.cpp.inc"

//===----------------------------------------------------------------------===//
// Arts Dialect Operations
//===----------------------------------------------------------------------===//
#define GET_OP_CLASSES
#include "arts/ArtsOps.cpp.inc"

//===----------------------------------------------------------------------===//
// Arts Dialect Types
//===----------------------------------------------------------------------===//
#define GET_TYPEDEF_CLASSES
#include "arts/ArtsOpsTypes.cpp.inc"