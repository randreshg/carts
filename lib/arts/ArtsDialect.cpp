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
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/IntegerSet.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/TypeSwitch.h"

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
// Arts Dialect Operations - method definitions
//===----------------------------------------------------------------------===//
#define GET_OP_CLASSES
#include "arts/ArtsOps.cpp.inc"

//===----------------------------------------------------------------------===//
// Arts Dialect Types - method definitions
//===----------------------------------------------------------------------===//
#define GET_TYPEDEF_CLASSES
#include "arts/ArtsOpsTypes.cpp.inc"

//===----------------------------------------------------------------------===//
// MakeDepOp
//===----------------------------------------------------------------------===//
void MakeDepOp::build(OpBuilder &builder, OperationState &result,
                      StringRef mode, Value memref, AffineMap affine_map,
                      ValueRange affine_operands) {
  result.addTypes(arts::DepType::get(builder.getContext()));
  result.addAttribute("mode", builder.getStringAttr(mode));
  result.addOperands({memref});
  result.addAttribute("affine_map", AffineMapAttr::get(affine_map));
  result.addOperands(affine_operands);
}

void MakeDepOp::build(OpBuilder &builder, OperationState &result,
                      StringRef mode, Value memref) {
  result.addTypes(arts::DepType::get(builder.getContext()));
  result.addAttribute("mode", builder.getStringAttr(mode));
  result.addOperands({memref});
}

//===----------------------------------------------------------------------===//
// UndefOp
//===----------------------------------------------------------------------===//
class UndefToLLVM final : public OpRewritePattern<UndefOp> {
public:
  using OpRewritePattern<UndefOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(UndefOp uop,
                                PatternRewriter &rewriter) const override {
    auto ty = uop.getResult().getType();
    if (!LLVM::isCompatibleType(ty))
      return failure();
    rewriter.replaceOpWithNewOp<LLVM::UndefOp>(uop, ty);
    return success();
  }
};

void UndefOp::getCanonicalizationPatterns(RewritePatternSet &results,
                                          MLIRContext *context) {
  results.insert<UndefToLLVM>(context);
}

//===----------------------------------------------------------------------===//
// EdtOp
//===----------------------------------------------------------------------===//
/// Retrieve parameters.
SmallVector<Value> EdtOp::getParametersValues() {
  auto parameters = getParameters();
  SmallVector<Value, 4> parametersVector(parameters.begin(), parameters.end());
  return parametersVector;
}

/// Retrieve dependencies.
SmallVector<Value> EdtOp::getDependenciesValues() {
  auto dependencies = getDependencies();
  SmallVector<Value, 4> dependenciesVector(dependencies.begin(),
                                           dependencies.end());
  return dependenciesVector;
}

/// Get the number of parameters.
unsigned EdtOp::getNumParameters() {
  auto parameters = getParameters();
  return parameters.size();
}

/// Get the number of dependencies.
unsigned EdtOp::getNumDependencies() {
  auto dependencies = getDependencies();
  return dependencies.size();
}