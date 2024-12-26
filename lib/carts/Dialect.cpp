//===- ArtsDialect.cpp - Arts dialect ---------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "carts/Dialect.h"
#include "mlir/IR/DialectImplementation.h"
#include "carts/Ops.h"

using namespace mlir;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// Arts dialect.
//===----------------------------------------------------------------------===//

void ArtsDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "carts/ArtsOps.cpp.inc"
      >();
}

#include "carts/ArtsOpsDialect.cpp.inc"
