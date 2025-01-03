//===- Types.h - ARTS Dialect Types -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the types for the ARTS dialect.
//
//===----------------------------------------------------------------------===//

#ifndef CARTS_DIALECT_TYPES_H
#define CARTS_DIALECT_TYPES_H

#include "mlir/IR/Types.h"

//===----------------------------------------------------------------------===//
// ARTS Dialect Types
//===----------------------------------------------------------------------===//
#define GET_TYPEDEF_CLASSES
#include "carts/ArtsOpsTypes.h.inc"

#endif // CARTS_DIALECT_TYPES_H
