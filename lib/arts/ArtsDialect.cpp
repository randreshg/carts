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

bool isArtsRegion(Operation *op) {
  return isa<EdtOp>(op) || isa<ParallelOp>(op) || isa<EpochOp>(op) ||
         isa<SingleOp>(op);
}
//===----------------------------------------------------------------------===//
// Arts Dialect Types - method definitions
//===----------------------------------------------------------------------===//
#define GET_TYPEDEF_CLASSES
#include "arts/ArtsOpsTypes.cpp.inc"

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
ParseResult EdtOp::parse(OpAsmParser &parser, OperationState &result) {
  /// We'll parse something like:
  ///   parameters(...) : (...)
  ///   [ optional ", constants(...) : (...)" ]
  ///   [ optional ", dependencies(...) : (...)" ]
  ///   [ optional ", events(...) : (...)" ]
  /// Then parse attr-dict, then the region.

  /// Collect all operands in a single list, typed accordingly
  SmallVector<OpAsmParser::UnresolvedOperand, 8> paramOps, constOps, depOps,
      evtOps;
  SmallVector<Type, 8> paramTypes, constTypes, depTypes, evtTypes;

  auto parseGroup =
      [&](StringRef kw,
          SmallVectorImpl<OpAsmParser::UnresolvedOperand> &operands,
          SmallVectorImpl<Type> &types) -> ParseResult {
    /// Expect "kw(...):(...)"
    if (parser.parseOperandList(operands, OpAsmParser::Delimiter::Paren) ||
        parser.parseColon() || parser.parseLParen() ||
        parser.parseTypeList(types) || parser.parseRParen()) {
      return failure();
    }
    return success();
  };

  /// Optional read of "parameters(...) : (...)"
  if (succeeded(parser.parseOptionalKeyword("parameters"))) {
    if (parseGroup("parameters", paramOps, paramTypes))
      return failure();
    (void)parser.parseOptionalComma();
  }

  /// Optional read of "constants(...) : (...)"
  if (succeeded(parser.parseOptionalKeyword("constants"))) {
    if (parseGroup("constants", constOps, constTypes))
      return failure();
    (void)parser.parseOptionalComma();
  }

  /// Optional read of "dependencies(...) : (...)"
  if (succeeded(parser.parseOptionalKeyword("dependencies"))) {
    if (parseGroup("dependencies", depOps, depTypes))
      return failure();
    (void)parser.parseOptionalComma();
  }

  /// Optional read of "events(...) : (...)"
  if (succeeded(parser.parseOptionalKeyword("events"))) {
    if (parseGroup("events", evtOps, evtTypes))
      return failure();
    (void)parser.parseOptionalComma();
  }

  /// Parse attribute dictionary
  if (parser.parseOptionalAttrDictWithKeyword(result.attributes))
    return failure();

  /// Parse region
  Region *body = result.addRegion();
  if (parser.parseRegion(*body))
    return failure();

  /// Convert operand references -> actual Value, storing in result.operands
  /// We'll accumulate them in the order: parameters, constants, dependencies,
  /// events
  if (parser.resolveOperands(paramOps, paramTypes, parser.getCurrentLocation(),
                             result.operands))
    return failure();
  if (parser.resolveOperands(constOps, constTypes, parser.getCurrentLocation(),
                             result.operands))
    return failure();
  if (parser.resolveOperands(depOps, depTypes, parser.getCurrentLocation(),
                             result.operands))
    return failure();
  if (parser.resolveOperands(evtOps, evtTypes, parser.getCurrentLocation(),
                             result.operands))
    return failure();

  return success();
}

void EdtOp::print(OpAsmPrinter &printer) {
  bool first = true;

  // Helper to print a group "kw(%v0, %v1) : (ty0, ty1)"
  auto printGroup = [&](StringRef kw, ValueRange vals) {
    if (vals.empty())
      return;
    if (!first)
      printer << ", ";
    else
      first = false;

    printer << kw << "(";
    printer.printOperands(vals);
    printer << ") : (";

    // Collect each operand's type manually
    SmallVector<Type, 4> types;
    types.reserve(vals.size());
    for (Value v : vals)
      types.push_back(v.getType());

    llvm::interleaveComma(types, printer);
    printer << ")";
  };

  // Print each group if it has operands
  printGroup(" parameters", getParams());
  printGroup("constants", getConsts());
  printGroup("dependencies", getDeps());
  printGroup("events", getEvents());

  // Print attributes + region
  printer.printOptionalAttrDictWithKeyword(getOperation()->getAttrs());
  printer << ' ';
  printer.printRegion(getOperation()->getRegion(0),
                      /*printEntryBlockArgs=*/false,
                      /*printBlockTerminators=*/true);
}

/// Builder
void EdtOp::build(OpBuilder &odsBuilder, OperationState &odsState,
                  ValueRange parameters, ValueRange constants,
                  ValueRange dependencies) {
  odsState.addOperands(parameters);
  odsState.addOperands(constants);
  odsState.addOperands(dependencies);
  ::llvm::copy(
      ::llvm::ArrayRef<int32_t>({static_cast<int32_t>(parameters.size()),
                                 static_cast<int32_t>(constants.size()),
                                 static_cast<int32_t>(dependencies.size())}),
      odsState.getOrAddProperties<Properties>().operandSegmentSizes.begin());
  (void)odsState.addRegion();
}

/// Retrieve parameters.
SmallVector<Value> EdtOp::getParams() {
  auto parameters = getParameters();
  SmallVector<Value, 4> parametersVector(parameters.begin(), parameters.end());
  return parametersVector;
}

/// Retrieve constants.
SmallVector<Value> EdtOp::getConsts() {
  auto constants = getConstants();
  SmallVector<Value, 4> constantsVector(constants.begin(), constants.end());
  return constantsVector;
}

/// Retrieve dependencies.
SmallVector<Value> EdtOp::getDeps() {
  auto dependencies = getDependencies();
  SmallVector<Value, 4> dependenciesVector(dependencies.begin(),
                                           dependencies.end());
  return dependenciesVector;
}

//===----------------------------------------------------------------------===//
// ParallelOp
//===----------------------------------------------------------------------===//
/// Retrieve parameters.
SmallVector<Value> ParallelOp::getParams() {
  auto parameters = getParameters();
  SmallVector<Value, 4> parametersVector(parameters.begin(), parameters.end());
  return parametersVector;
}

/// Retrieve constants.
SmallVector<Value> ParallelOp::getConsts() {
  auto constants = getConstants();
  SmallVector<Value, 4> constantsVector(constants.begin(), constants.end());
  return constantsVector;
}

/// Retrieve dependencies.
SmallVector<Value> ParallelOp::getDeps() {
  auto dependencies = getDependencies();
  SmallVector<Value, 4> dependenciesVector(dependencies.begin(),
                                           dependencies.end());
  return dependenciesVector;
}

//===----------------------------------------------------------------------===//
// Utils
//===----------------------------------------------------------------------===//
namespace mlir::arts::utils {
void replaceInRegion(Region &region, Value from, Value to) {
  from.replaceUsesWithIf(to, [&](OpOperand &operand) {
    return region.isAncestor(operand.getOwner()->getParentRegion());
  });
}

void replaceInRegion(Region &region, DenseMap<Value, Value> &rewireMap) {
  for (auto &rewire : rewireMap)
    replaceInRegion(region, rewire.first, rewire.second);
}
} // namespace mlir::arts::utils