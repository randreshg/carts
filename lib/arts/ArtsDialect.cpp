//===- ArtsDialect.cpp - Arts dialect ---------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "arts/Utils/ArtsTypes.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/IntegerSet.h"
#include "mlir/IR/Types.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/TypeSwitch.h"
#include <cassert>

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

  /// Register attributes
  addAttributes<
#define GET_ATTRDEF_LIST
#include "arts/ArtsOpsAttributes.cpp.inc"
      >();
}

#include "arts/ArtsOpsDialect.cpp.inc"

//===----------------------------------------------------------------------===//
// Arts Dialect Operations - method definitions
//===----------------------------------------------------------------------===//
#define GET_OP_CLASSES
#include "arts/ArtsOps.cpp.inc"

bool isArtsRegion(Operation *op) { return isa<EdtOp>(op) || isa<EpochOp>(op); }
bool isArtsOp(Operation *op){
    return isArtsRegion(op) ||
           isa<arts::EdtOp, arts::EpochOp, arts::BarrierOp, arts::AllocOp,
               arts::DbAllocOp, arts::DbDepOp, arts::GetTotalWorkersOp,
               arts::GetTotalNodesOp, arts::GetCurrentWorkerOp,
               arts::GetCurrentNodeOp>(op);
}

//===----------------------------------------------------------------------===//
// Arts Dialect Types - method definitions
//===----------------------------------------------------------------------===//
#define GET_TYPEDEF_CLASSES
#include "arts/ArtsOpsTypes.cpp.inc"

//===----------------------------------------------------------------------===//
// Arts Dialect Attributes - method definitions
//===----------------------------------------------------------------------===//
#define GET_ATTRDEF_CLASSES
#include "arts/ArtsOpsAttributes.cpp.inc"

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
// DbAllocOp
//===----------------------------------------------------------------------===//
ParseResult DbAllocOp::parse(OpAsmParser &parser, OperationState &result) {
  StringAttr modeAttr;
  OpAsmParser::UnresolvedOperand addressOperand;
  Type addressType;
  bool hasAddress = false;
  SmallVector<OpAsmParser::UnresolvedOperand, 4> sizeOperands;
  SmallVector<Type, 4> sizeTypes;
  Type resultType;
  StringAttr allocTypeAttr;

  // Parse mode string
  if (parser.parseAttribute(modeAttr, "mode", result.attributes))
    return failure();

  // Optionally parse address
  if (succeeded(parser.parseOptionalLParen())) {
    if (parser.parseOperand(addressOperand) || parser.parseColon() ||
        parser.parseType(addressType) || parser.parseRParen())
      return failure();
    hasAddress = true;
  }

  // Parse sizes
  if (parser.parseLSquare() ||
      parser.parseOperandList(sizeOperands, OpAsmParser::Delimiter::None) ||
      parser.parseRSquare())
    return failure();

  // Optionally parse alloc_type
  if (succeeded(parser.parseOptionalKeyword("alloc_type"))) {
    if (parser.parseEqual() ||
        parser.parseAttribute(allocTypeAttr, "allocType", result.attributes))
      return failure();
  }

  // Parse result type
  if (parser.parseArrow() ||
      parser.parseOptionalAttrDictWithKeyword(result.attributes) ||
      parser.parseColon() || parser.parseType(resultType))
    return failure();

  // Build operand segment sizes
  result.addAttribute("operandSegmentSizes",
                      parser.getBuilder().getDenseI32ArrayAttr({
                          hasAddress ? 1 : 0,                       // address
                          static_cast<int32_t>(sizeOperands.size()) // sizes
                      }));

  // Resolve operands
  if (hasAddress) {
    if (parser.resolveOperand(addressOperand, addressType, result.operands))
      return failure();
  }

  for (auto sizeOperand : sizeOperands)
    sizeTypes.push_back(parser.getBuilder().getIndexType());

  if (parser.resolveOperands(sizeOperands, sizeTypes,
                             parser.getCurrentLocation(), result.operands))
    return failure();

  result.addTypes(resultType);
  return success();
}

void DbAllocOp::print(OpAsmPrinter &printer) {
  printer << " ";
  printer.printAttributeWithoutType(getModeAttr());

  // Print address if present
  if (getAddress()) {
    printer << " (";
    printer.printOperand(getAddress());
    printer << " : ";
    printer.printType(getAddress().getType());
    printer << ")";
  }

  // Print sizes
  printer << " [";
  printer.printOperands(getSizes());
  printer << "]";

  // Print alloc_type if present
  if (auto allocType = getAllocTypeAttr()) {
    printer << " alloc_type = ";
    printer.printAttributeWithoutType(allocType);
  }

  printer << " -> ";
  printer.printOptionalAttrDictWithKeyword(
      getOperation()->getAttrs(),
      /*elidedAttrs=*/{"mode", "allocType", "operandSegmentSizes"});
  printer << " : ";
  printer.printType(getPtr().getType());
}

//===----------------------------------------------------------------------===//
// EdtOp
//===----------------------------------------------------------------------===//
ParseResult EdtOp::parse(OpAsmParser &parser, OperationState &result) {
  /// We'll parse something like:
  ///   parameters(...) : (...)
  ///   [ optional ", dependencies(...) : (...)" ]
  /// Then parse attr-dict, then the region.

  /// Collect all operands in a single list, typed accordingly
  SmallVector<OpAsmParser::UnresolvedOperand, 8> depOps;
  SmallVector<Type, 8> depTypes, evtTypes;

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

  /// Optional read of "dependencies(...) : (...)"
  if (succeeded(parser.parseOptionalKeyword("dependencies"))) {
    if (parseGroup("dependencies", depOps, depTypes))
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
  if (parser.resolveOperands(depOps, depTypes, parser.getCurrentLocation(),
                             result.operands))
    return failure();

  return success();
}

void EdtOp::print(OpAsmPrinter &printer) {
  bool first = true;
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

    /// Collect each operand's type manually
    SmallVector<Type, 4> types;
    types.reserve(vals.size());
    for (Value v : vals)
      types.push_back(v.getType());

    llvm::interleaveComma(types, printer);
    printer << ")";
  };

  /// Print each group if it has operands
  printGroup(" dependencies", getDependenciesVector());

  /// Print attributes + region
  printer.printOptionalAttrDictWithKeyword(
      getOperation()->getAttrs(),
      /*elidedAttrs=*/{"operandSegmentSizes"});
  printer << ' ';
  printer.printRegion(getOperation()->getRegion(0),
                      /*printEntryBlockArgs=*/false,
                      /*printBlockTerminators=*/true);
}

void EdtOp::getEffects(
    SmallVectorImpl<MemoryEffects::EffectInstance> &effects) {
  /// Collect all effects from the dependencies
  auto dependencies = getDependencies();

  /// If no dependencies, assume it has effects on all the memrefs
  /// and add read/write effects
  if (dependencies.empty()) {
    effects.emplace_back(MemoryEffects::Read::get(),
                         ::mlir::SideEffects::DefaultResource::get());
    effects.emplace_back(MemoryEffects::Write::get(),
                         ::mlir::SideEffects::DefaultResource::get());
    return;
  }

  /// Otherwise, collect effects from each dependency
  // for (auto dep : dependencies) {
    // TODO: Implement getEffects for DbDepOp
    // if (auto dbOp = dep.getDefiningOp<DbDepOp>())
    //   dbOp.getEffects(effects);
  // }
}

/// Retrieve dependencies.
SmallVector<Value> EdtOp::getDependenciesVector() {
  auto dependencies = getDependencies();
  SmallVector<Value, 4> dependenciesVector(dependencies.begin(),
                                           dependencies.end());
  return dependenciesVector;
}

/// Others
bool EdtOp::isParallel() {
  return getOperation()->hasAttr(types::toString(types::EdtType::Parallel));
}
bool EdtOp::isSingle() {
  return getOperation()->hasAttr(types::toString(types::EdtType::Single));
}
bool EdtOp::isSync() {
  return getOperation()->hasAttr(types::toString(types::EdtType::Sync));
}
bool EdtOp::isTask() {
  return getOperation()->hasAttr(types::toString(types::EdtType::Task));
}
void EdtOp::setIsParallelAttr() {
  getOperation()->setAttr(types::toString(types::EdtType::Parallel),
                          UnitAttr::get(getContext()));
}
void EdtOp::setIsSingleAttr() {
  getOperation()->setAttr(types::toString(types::EdtType::Single),
                          UnitAttr::get(getContext()));
}
void EdtOp::setIsSyncAttr() {
  getOperation()->setAttr("sync", UnitAttr::get(getContext()));
}
void EdtOp::setIsTaskAttr() {
  getOperation()->setAttr("task", UnitAttr::get(getContext()));
}
void EdtOp::clearIsParallelAttr() {
  getOperation()->removeAttr(types::toString(types::EdtType::Parallel));
}
void EdtOp::clearIsSingleAttr() {
  getOperation()->removeAttr(types::toString(types::EdtType::Single));
}
void EdtOp::clearIsSyncAttr() {
  getOperation()->removeAttr(types::toString(types::EdtType::Sync));
}
void EdtOp::clearIsTaskAttr() {
  getOperation()->removeAttr(types::toString(types::EdtType::Task));
}

//===----------------------------------------------------------------------===//
// EventOp
//===----------------------------------------------------------------------===//
bool EventOp::hasSingleSize() { return getOperation()->hasAttr("singleSize"); }
void EventOp::setHasSingleSize() {
  getOperation()->setAttr("singleSize", UnitAttr::get(getContext()));
}