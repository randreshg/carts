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
bool isArtsOp(Operation *op) {
  return isArtsRegion(op) || isa<DataBlockOp>(op) || isa<EventOp>(op) ||
         isa<BarrierOp>(op);
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
// DataBlockOp
//===----------------------------------------------------------------------===//
void DataBlockOp::build(OpBuilder &odsBuilder, OperationState &odsState,
                        Type subview, StringRef mode, Value ptr,
                        Type elementType, Value elementTypeSize,
                        ValueRange indices, ValueRange sizes) {
  odsState.addOperands(ptr);
  odsState.addOperands(elementTypeSize);
  odsState.addOperands(indices);
  odsState.addOperands(sizes);
  ::llvm::copy(
      ::llvm::ArrayRef<int32_t>({1, 1, static_cast<int32_t>(indices.size()),
                                 static_cast<int32_t>(sizes.size())}),
      odsState.getOrAddProperties<Properties>().operandSegmentSizes.begin());
  odsState.getOrAddProperties<Properties>().mode =
      odsBuilder.getStringAttr(mode);
  odsState.getOrAddProperties<Properties>().elementType =
      TypeAttr::get(elementType);
  odsState.addTypes(subview);
}

void DataBlockOp::build(OpBuilder &odsBuilder, OperationState &odsState,
                        Type subview, StringRef mode, Value ptr,
                        Type elementType, Value elementTypeSize,
                        ValueRange indices, ValueRange sizes, Value inEvent,
                        Value outEvent) {
  if (!inEvent && !outEvent)
    return build(odsBuilder, odsState, subview, mode, ptr, elementType,
                 elementTypeSize, indices, sizes);
  odsState.addOperands(ptr);
  odsState.addOperands(elementTypeSize);
  odsState.addOperands(indices);
  odsState.addOperands(sizes);
  if (inEvent)
    odsState.addOperands(inEvent);
  if (outEvent)
    odsState.addOperands(outEvent);
  ::llvm::copy(
      ::llvm::ArrayRef<int32_t>({1, 1, static_cast<int32_t>(indices.size()),
                                 static_cast<int32_t>(sizes.size()),
                                 (inEvent ? 1 : 0) + (outEvent ? 1 : 0)}),
      odsState.getOrAddProperties<Properties>().operandSegmentSizes.begin());
  odsState.getOrAddProperties<Properties>().mode =
      odsBuilder.getStringAttr(mode);
  odsState.getOrAddProperties<Properties>().elementType =
      TypeAttr::get(elementType);
  odsState.addTypes(subview);
}

void DataBlockOp::build(OpBuilder &odsBuilder, OperationState &odsState,
                        Type subview, StringRef mode, Value ptr,
                        Type elementType, Value elementTypeSize,
                        ValueRange indices, ValueRange sizes,
                        Value inEvent, Value outEvent,
                        DomainAttr domain) {
  build(odsBuilder, odsState, subview, mode, ptr, elementType,
        elementTypeSize, indices, sizes, inEvent, outEvent);
  odsState.addAttribute("domain", domain);
}

ParseResult DataBlockOp::parse(OpAsmParser &parser, OperationState &result) {
  /// Parse the mode attribute.
  StringAttr modeAttr;
  if (parser.parseAttribute(modeAttr, "mode", result.attributes))
    return failure();

  /// Parse " ptr[" literal.
  if (parser.parseKeyword("ptr") || parser.parseLSquare())
    return failure();

  /// Parse the base ptr operand.
  OpAsmParser::UnresolvedOperand ptrOperand;
  if (parser.parseOperand(ptrOperand))
    return failure();

  /// Parse colon and the type of the ptr operand.
  if (parser.parseColon())
    return failure();
  Type ptrType;
  if (parser.parseType(ptrType))
    return failure();
  if (parser.resolveOperand(ptrOperand, ptrType, result.operands))
    return failure();
  if (parser.parseRSquare())
    return failure();

  /// Parse comma then "indices[".
  if (parser.parseComma() || parser.parseKeyword("indices") ||
      parser.parseLSquare())
    return failure();
  SmallVector<OpAsmParser::UnresolvedOperand, 4> offsetOperands;
  if (parser.parseOperandList(offsetOperands))
    return failure();
  if (parser.parseRSquare())
    return failure();

  /// Parse comma then "sizes[".
  if (parser.parseComma() || parser.parseKeyword("sizes") ||
      parser.parseLSquare())
    return failure();
  SmallVector<OpAsmParser::UnresolvedOperand, 4> sizeOperands;
  if (parser.parseOperandList(sizeOperands))
    return failure();
  if (parser.parseRSquare())
    return failure();

  /// Parse comma then "type[".
  if (parser.parseComma() || parser.parseKeyword("type") ||
      parser.parseLSquare())
    return failure();
  Type elementType;
  if (parser.parseType(elementType))
    return failure();
  result.addAttribute("elementType", TypeAttr::get(elementType));
  if (parser.parseRSquare())
    return failure();

  /// Parse comma then "typeSize[".
  if (parser.parseComma() || parser.parseKeyword("typeSize") ||
      parser.parseLSquare())
    return failure();
  OpAsmParser::UnresolvedOperand tsOperand;
  if (parser.parseOperand(tsOperand))
    return failure();
  Builder &builder = parser.getBuilder();
  Type indexType = builder.getIndexType();
  if (parser.resolveOperand(tsOperand, indexType, result.operands))
    return failure();
  if (parser.parseRSquare())
    return failure();

  /// Optionally parse comma then "inEvent[" "outEvent[".
  auto parseEventOperand =
      [&](StringRef keyword,
          SmallVector<OpAsmParser::UnresolvedOperand, 1> &eventOperand)
      -> ParseResult {
    if (succeeded(parser.parseOptionalComma())) {
      if (parser.parseKeyword(keyword) || parser.parseLSquare())
        return failure();
      OpAsmParser::UnresolvedOperand eventOp;
      if (parser.parseOperand(eventOp) || parser.parseColon())
        return failure();
      Type eventType;
      if (parser.parseType(eventType) || parser.parseRSquare())
        return failure();
      if (parser.resolveOperand(eventOp, eventType, result.operands))
        return failure();
      eventOperand.push_back(eventOp);
    }
    return success();
  };

  SmallVector<OpAsmParser::UnresolvedOperand, 1> inEventOperand,
      outEventOperand;
  if (failed(parseEventOperand("inEvent", inEventOperand)))
    return failure();
  if (failed(parseEventOperand("outEvent", outEventOperand)))
    return failure();

  /// Optionally parse comma then domain attribute.
  if (succeeded(parser.parseOptionalComma())) {
    if (parser.parseKeyword("domain"))
      return failure();
    if (parser.parseEqual())
      return failure();
    DomainAttr domainAttr;
    if (parser.parseAttribute(domainAttr, "domain", result.attributes))
      return failure();
  }

  /// Parse optional attribute dictionary.
  if (parser.parseOptionalAttrDict(result.attributes))
    return failure();

  /// Parse arrow and the result (subview) type.
  if (parser.parseArrow())
    return failure();
  Type subviewType;
  if (parser.parseType(subviewType))
    return failure();
  result.addTypes(subviewType);

  /// Resolve the operand lists for indices and sizes as index type.
  SmallVector<Type, 4> expectedOffsetTypes(offsetOperands.size(), indexType);
  if (parser.resolveOperands(offsetOperands, expectedOffsetTypes,
                             parser.getCurrentLocation(), result.operands))
    return failure();
  SmallVector<Type, 4> expectedSizeTypes(sizeOperands.size(), indexType);
  if (parser.resolveOperands(sizeOperands, expectedSizeTypes,
                             parser.getCurrentLocation(), result.operands))
    return failure();

  /// Set operand segment sizes.
  /// Order: ptr (1), typeSize (1), indices, sizes, event (0 or 1).
  SmallVector<int32_t, 6> segmentSizes;
  segmentSizes.push_back(1);
  segmentSizes.push_back(1);
  segmentSizes.push_back(static_cast<int32_t>(offsetOperands.size()));
  segmentSizes.push_back(static_cast<int32_t>(sizeOperands.size()));
  segmentSizes.push_back(static_cast<int32_t>(inEventOperand.size()));
  segmentSizes.push_back(static_cast<int32_t>(outEventOperand.size()));
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
  printer.printOperand(getPtr());
  printer << " : ";
  printer.printType(getPtr().getType());
  printer << "]";

  /// Print indices.
  auto indices = getIndices();
  printer << ", indices[";
  printer.printOperands(indices);
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

  /// Optionally print the inEvent operand if present.
  if (auto inEvent = getInEvent()) {
    printer << ", inEvent[";
    printer.printOperand(inEvent);
    printer << "]";
  }

  /// Optionally print the outEvent operand if present.
  if (auto outEvent = getOutEvent()) {
    printer << ", outEvent[";
    printer.printOperand(outEvent);
    printer << "]";
  }

  /// Optionally print the affineMap attribute if present.
  if (auto domainAttr = op->getAttr("domainAttr"))
    printer << ", domainAttr=" << domainAttr;

  // Print all remaining attributes except "operandSegmentSizes"
  // and those already printed ("mode", "elementType", "domainAttr").
  printer.printOptionalAttrDict(
      op->getAttrs(),
      {"mode", "elementType", "domainAttr", "operandSegmentSizes"});

  /// Print arrow and the result type.
  printer << " -> ";
  printer.printType(getResult().getType());
}

void DataBlockOp::getEffects(
    SmallVectorImpl<MemoryEffects::EffectInstance> &effects) {
  auto modeAttr = getModeAttr();
  assert(modeAttr && "mode attribute not found");
  Value ptr = getPtr();
  assert(ptr && "ptr attribute not found");

  /// "in" => read, "out" => write, "inout" => both
  StringRef mode = modeAttr.getValue();
  if (mode == "in" || mode == "inout") {
    effects.emplace_back(MemoryEffects::Read::get(), ptr,
                         ::mlir::SideEffects::DefaultResource::get());
  }
  if (mode == "out" || mode == "inout") {
    effects.emplace_back(MemoryEffects::Write::get(), ptr,
                         ::mlir::SideEffects::DefaultResource::get());
  }
}

bool DataBlockOp::hasPtrDb() { return getOperation()->hasAttr("ptrDb"); }
bool DataBlockOp::hasSingleSize() {
  return getOperation()->hasAttr("singleSize");
}

bool DataBlockOp::hasGuid() {
  return getOperation()->hasAttr("hasGuid");
}

void DataBlockOp::setHasPtrDb() {
  getOperation()->setAttr("ptrDb", UnitAttr::get(getContext()));
}
void DataBlockOp::setHasSingleSize() {
  getOperation()->setAttr("singleSize", UnitAttr::get(getContext()));
}
void DataBlockOp::setHasGuid() {
  getOperation()->setAttr("hasGuid", UnitAttr::get(getContext()));
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

/// Retrieve dependencies.
SmallVector<Value> EdtOp::getDependenciesVector() {
  auto dependencies = getDependencies();
  SmallVector<Value, 4> dependenciesVector(dependencies.begin(),
                                           dependencies.end());
  return dependenciesVector;
}

/// Others
bool EdtOp::isParallel() { return getOperation()->hasAttr("parallel"); }
bool EdtOp::isSingle() { return getOperation()->hasAttr("single"); }
bool EdtOp::isSync() { return getOperation()->hasAttr("sync"); }
bool EdtOp::isTask() { return getOperation()->hasAttr("task"); }
void EdtOp::setIsParallelAttr() {
  getOperation()->setAttr("parallel", UnitAttr::get(getContext()));
}
void EdtOp::setIsSingleAttr() {
  getOperation()->setAttr("single", UnitAttr::get(getContext()));
}
void EdtOp::setIsSyncAttr() {
  getOperation()->setAttr("sync", UnitAttr::get(getContext()));
}
void EdtOp::setIsTaskAttr() {
  getOperation()->setAttr("task", UnitAttr::get(getContext()));
}
void EdtOp::clearIsParallelAttr() { getOperation()->removeAttr("parallel"); }
void EdtOp::clearIsSingleAttr() { getOperation()->removeAttr("single"); }
void EdtOp::clearIsSyncAttr() { getOperation()->removeAttr("sync"); }
void EdtOp::clearIsTaskAttr() { getOperation()->removeAttr("task"); }

//===----------------------------------------------------------------------===//
// EventOp
//===----------------------------------------------------------------------===//
bool EventOp::hasSingleSize() { return getOperation()->hasAttr("singleSize"); }
void EventOp::setHasSingleSize() {
  getOperation()->setAttr("singleSize", UnitAttr::get(getContext()));
}