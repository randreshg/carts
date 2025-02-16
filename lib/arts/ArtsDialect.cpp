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
}

#include "arts/ArtsOpsDialect.cpp.inc"

//===----------------------------------------------------------------------===//
// Arts Dialect Operations - method definitions
//===----------------------------------------------------------------------===//
#define GET_OP_CLASSES
#include "arts/ArtsOps.cpp.inc"

bool isArtsRegion(Operation *op) { return isa<EdtOp>(op) || isa<EpochOp>(op); }
bool isArtsOp(Operation *op) {
  return isArtsRegion(op) || isa<DataBlockOp>(op) || isa<EventOp>(op);
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
// DataBlockOp

/// Custom printer for DataBlockOp.
// build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, Type
// subview, StringRef mode, Value base, Type elementType, Value elementTypeSize,
// ValueRange offsets, ValueRange sizes);
void DataBlockOp::build(OpBuilder &odsBuilder, OperationState &odsState,
                        Type subview, StringRef mode, Value base,
                        Type elementType, Value elementTypeSize,
                        ValueRange offsets, ValueRange sizes) {

  odsState.addOperands(base);
  odsState.addOperands(elementTypeSize);
  odsState.addOperands(offsets);
  odsState.addOperands(sizes);
  ::llvm::copy(
      ::llvm::ArrayRef<int32_t>({1, 1, static_cast<int32_t>(offsets.size()),
                                 static_cast<int32_t>(sizes.size())}),
      odsState.getOrAddProperties<Properties>().operandSegmentSizes.begin());
  odsState.getOrAddProperties<Properties>().mode =
      odsBuilder.getStringAttr(mode);
  odsState.getOrAddProperties<Properties>().elementType =
      TypeAttr::get(elementType);
  odsState.addTypes(subview);
}

ParseResult DataBlockOp::parse(OpAsmParser &parser, OperationState &result) {
  /// Parse the mode attribute.
  StringAttr modeAttr;
  if (parser.parseAttribute(modeAttr, "mode", result.attributes))
    return failure();
  if (parser.parseComma())
    return failure();

  /// Parse the base operand.
  OpAsmParser::UnresolvedOperand baseOperand;
  if (parser.parseOperand(baseOperand))
    return failure();

  /// Parse colon and the type of the base operand.
  if (parser.parseColon())
    return failure();
  Type baseType;
  if (parser.parseType(baseType))
    return failure();
  result.addAttribute("baseType", TypeAttr::get(baseType));
  if (parser.resolveOperand(baseOperand, baseType, result.operands))
    return failure();

  if (parser.parseComma())
    return failure();

  /// Parse offsets list.
  SmallVector<OpAsmParser::UnresolvedOperand, 4> offsetOperands;
  if (parser.parseLSquare())
    return failure();
  if (parser.parseOperandList(offsetOperands))
    return failure();
  if (parser.parseRSquare())
    return failure();

  if (parser.parseComma())
    return failure();

  /// Parse sizes list.
  SmallVector<OpAsmParser::UnresolvedOperand, 4> sizeOperands;
  if (parser.parseLSquare())
    return failure();
  if (parser.parseOperandList(sizeOperands))
    return failure();
  if (parser.parseRSquare())
    return failure();

  /// Optionally parse the affineOffsetMaps group.
  if (succeeded(parser.parseOptionalComma())) {
    if (parser.parseKeyword("affineMaps"))
      return failure();
    if (parser.parseEqual())
      return failure();
    Attribute affineMapsAttr;
    if (parser.parseAttribute(affineMapsAttr, "affineOffsetMaps",
                              result.attributes))
      return failure();
  }

  /// Parse the element type and the elementTypeSize operand in square brackets.
  if (parser.parseLSquare())
    return failure();
  Type elementType;
  if (parser.parseType(elementType))
    return failure();

  /// Record the element type as an attribute.
  result.addAttribute("elementType", TypeAttr::get(elementType));
  if (parser.parseComma())
    return failure();
  // Parse the elementTypeSize operand (as expected to be an index value).
  OpAsmParser::UnresolvedOperand etsOperand;
  if (parser.parseOperand(etsOperand))
    return failure();

  Builder &builder = parser.getBuilder();
  Type indexType = builder.getIndexType();
  if (parser.resolveOperand(etsOperand, indexType, result.operands))
    return failure();
  if (parser.parseRSquare())
    return failure();

  /// Parse the arrow and the result (subview) type.
  if (parser.parseArrow())
    return failure();
  Type subviewType;
  if (parser.parseType(subviewType))
    return failure();
  result.addTypes({subviewType});

  /// Parse an optional attribute dictionary.
  if (parser.parseOptionalAttrDictWithKeyword(result.attributes))
    return failure();

  /// Resolve the operand lists for offsets and sizes as index type.
  SmallVector<Type, 4> expectedOffsets(offsetOperands.size(), indexType);
  if (parser.resolveOperands(offsetOperands, expectedOffsets,
                             parser.getCurrentLocation(), result.operands))
    return failure();
  SmallVector<Type, 4> expectedSizes(sizeOperands.size(), indexType);
  if (parser.resolveOperands(sizeOperands, expectedSizes,
                             parser.getCurrentLocation(), result.operands))
    return failure();

  // Set operand segment sizes. The expected order is:
  // 1 for base, 1 for elementTypeSize, then offsets, then sizes.
  SmallVector<int32_t, 4> segmentSizes;
  segmentSizes.push_back(1);
  segmentSizes.push_back(1);
  segmentSizes.push_back(static_cast<int32_t>(offsetOperands.size()));
  segmentSizes.push_back(static_cast<int32_t>(sizeOperands.size()));
  std::copy(
      segmentSizes.begin(), segmentSizes.end(),
      result.getOrAddProperties<Properties>().operandSegmentSizes.begin());

  return success();
}

void DataBlockOp::print(OpAsmPrinter &printer) {
  auto op = getOperation();
  /// Print the mode attribute.
  printer << " " << getModeAttr();

  /// Print the base operand and its type.
  printer << " ptr[";
  printer.printOperand(getBase());
  printer << " : ";
  printer.printType(getBase().getType());
  printer << "]";

  /// Print offsets.
  auto offsets = getOffsets();
  printer << ", offsets[";
  printer.printOperands(offsets);
  printer << "]";

  /// Print sizes.
  auto sizes = getSizes();
  printer << ", sizes[";
  printer.printOperands(sizes);
  printer << "]";

  /// Print the element type attribute.
  auto typeAttr = op->getAttrOfType<TypeAttr>("elementType");
  printer << ", type[" << typeAttr.getValue() << "]";

  /// Print the elementTypeSize operand (at index 1).
  printer << ", typeSize[";
  printer.printOperand(getOperands()[1]);
  printer << "]";

  /// Optionally print the affineMap attribute if present.
  if (auto affineMap = op->getAttr("affineMap"))
    printer << ", affineMap=" << affineMap;

  // Print all remaining attributes except "operandSegmentSizes"
  // and those already printed ("mode", "elementType", "affineMap").
  printer.printOptionalAttrDict(op->getAttrs(),
                                {"mode", "elementType", "affineMap", "operandSegmentSizes"});

  /// Print arrow and the result type.
  printer << " -> ";
  printer.printType(getResult().getType());
}

//===----------------------------------------------------------------------===//
// EdtOp
//===----------------------------------------------------------------------===//
ParseResult EdtOp::parse(OpAsmParser &parser, OperationState &result) {
  /// We'll parse something like:
  ///   parameters(...) : (...)
  ///   [ optional ", dependencies(...) : (...)" ]
  ///   [ optional ", events(...) : (...)" ]
  /// Then parse attr-dict, then the region.

  /// Collect all operands in a single list, typed accordingly
  SmallVector<OpAsmParser::UnresolvedOperand, 8> depOps, evtOps;
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

  /// Optional read of "events(...) : (...)"
  if (succeeded(parser.parseOptionalKeyword("events"))) {
    if (parseGroup("events", evtOps, evtTypes))
      return failure();
    (void)parser.parseOptionalComma();
  }

  /// Parse attribute dictionary
  if (parser.parseOptionalAttrDictWithKeyword(result.attributes))
    return failure();

  /// Compute operand segment sizes
  ::llvm::copy(
      ::llvm::ArrayRef<int32_t>({static_cast<int32_t>(depOps.size()),
                                 static_cast<int32_t>(evtOps.size())}),
      result.getOrAddProperties<Properties>().operandSegmentSizes.begin());

  /// Parse region
  Region *body = result.addRegion();
  if (parser.parseRegion(*body))
    return failure();

  /// Convert operand references -> actual Value, storing in result.operands
  /// We'll accumulate them in the order: dependencies, events
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
  /// Helper to print a group "kw(%v0, %v1) : (ty0, ty1)"
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
  printGroup(" dependencies", getDeps());
  printGroup("events", getEvents());

  /// Print attributes + region
  printer.printOptionalAttrDictWithKeyword(
      getOperation()->getAttrs(),
      /*elidedAttrs=*/{"operandSegmentSizes"});
  printer << ' ';
  printer.printRegion(getOperation()->getRegion(0),
                      /*printEntryBlockArgs=*/false,
                      /*printBlockTerminators=*/true);
}

/// Builder
void EdtOp::build(OpBuilder &odsBuilder, OperationState &odsState,
                  ValueRange dependencies) {
  odsState.addOperands(dependencies);
  ::llvm::copy(
      ::llvm::ArrayRef<int32_t>({static_cast<int32_t>(dependencies.size())}),
      odsState.getOrAddProperties<Properties>().operandSegmentSizes.begin());
  (void)odsState.addRegion();
}

/// Retrieve dependencies.
SmallVector<Value> EdtOp::getDeps() {
  auto dependencies = getDependencies();
  SmallVector<Value, 4> dependenciesVector(dependencies.begin(),
                                           dependencies.end());
  return dependenciesVector;
}

/// Retrieve events.
SmallVector<Value> EdtOp::getEvnts() {
  auto events = getEvents();
  SmallVector<Value, 4> eventsVector(events.begin(), events.end());
  return eventsVector;
}

/// Others
bool EdtOp::isParallel() { return getOperation()->hasAttr("parallel"); }
bool EdtOp::isSingle() { return getOperation()->hasAttr("single"); }

//===----------------------------------------------------------------------===//
// EventOp
//===----------------------------------------------------------------------===//
// void EventOp::build(OpBuilder &builder, OperationState &state, Type type,
//                     Value size, bool isGrouped) {
//   state.addOperands(size);
//   state.addTypes(type);
//   if (isGrouped)
//     state.addAttribute("grouped", builder.getUnitAttr());
// }

//===----------------------------------------------------------------------===//
// DataBlockOp
//===----------------------------------------------------------------------===//
void DataBlockOp::getEffects(
    SmallVectorImpl<MemoryEffects::EffectInstance> &effects) {
  auto modeAttr = getModeAttr();
  assert(modeAttr && "mode attribute not found");
  Value base = getBase();
  assert(base && "base attribute not found");

  /// "in" => read, "out" => write, "inout" => both
  StringRef mode = modeAttr.getValue();
  if (mode == "in" || mode == "inout") {
    effects.emplace_back(MemoryEffects::Read::get(), base,
                         ::mlir::SideEffects::DefaultResource::get());
  }
  if (mode == "out" || mode == "inout") {
    effects.emplace_back(MemoryEffects::Write::get(), base,
                         ::mlir::SideEffects::DefaultResource::get());
  }
}

bool DataBlockOp::isLoad() { return getOperation()->hasAttr("isLoad"); }
bool DataBlockOp::isBaseDb() { return getOperation()->hasAttr("baseIsDb"); }