//===- PassDetails.h - carts pass class details ----------------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Stuff shared between the different carts passes.
//
//===----------------------------------------------------------------------===//

// clang-tidy seems to expect the absolute path in the header guard on some
// systems, so just disable it.
// NOLINTNEXTLINE(llvm-header-guard)
#ifndef DIALECT_CARTS_TRANSFORMS_PASSDETAILS_H
#define DIALECT_CARTS_TRANSFORMS_PASSDETAILS_H

#include "mlir/Pass/Pass.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"

namespace mlir {
class FunctionOpInterface;

// Forward declaration from Dialect.h
template <typename ConcreteDialect>
void registerDialect(DialectRegistry &registry);

namespace arts {
/// Register passes
#define GEN_PASS_CLASSES
#include "arts/Passes/ArtsPasses.h.inc"

} // namespace arts
} // namespace mlir

#endif // DIALECT_CARTS_TRANSFORMS_PASSDETAILS_H
