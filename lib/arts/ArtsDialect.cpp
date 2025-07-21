//===- ArtsDialect.cpp - Arts dialect ---------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "arts/Utils/ArtsTypes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/IntegerSet.h"
#include "mlir/IR/Types.h"
#include "mlir/IR/TypeUtilities.h"
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
  return isArtsRegion(op) ||
         isa<arts::EdtOp, arts::EpochOp, arts::BarrierOp, arts::AllocOp,
             arts::DbAllocOp, arts::DbAcquireOp, arts::DbReleaseOp,
             arts::DbControlOp, arts::GetTotalWorkersOp, arts::GetTotalNodesOp,
             arts::GetCurrentWorkerOp, arts::GetCurrentNodeOp>(op);
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

// TODO: Fix inferReturnTypes method signature
// LogicalResult
// DbAllocOp::inferReturnTypes(MLIRContext *context,
//                             std::optional<Location> location,
//                             ValueShapeRange operands, DictionaryAttr attributes,
//                             OpaqueProperties properties, RegionRange regions,
//                             SmallVectorImpl<Type> &inferredReturnTypes) {
//   // Extract sizes from operands
//   SmallVector<int64_t> shape;
//   for (Value operand : operands) {
//     if (auto constOp = operand.getDefiningOp<arith::ConstantIndexOp>()) {
//       shape.push_back(constOp.value());
//     } else {
//       shape.push_back(ShapedType::kDynamic);
//     }
//   }
//   
//   // Get element type from attributes or default to f32
//   Type elementType = FloatType::getF32(context);
//   if (auto elementTypeAttr = attributes.getAs<TypeAttr>("elementType")) {
//     elementType = elementTypeAttr.getValue();
//   }
//   
//   inferredReturnTypes.push_back(MemRefType::get(shape, elementType));
//   return success();
// }

//===----------------------------------------------------------------------===//
// EdtOp
//===----------------------------------------------------------------------===//

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

//===----------------------------------------------------------------------===//
// EventOp
//===----------------------------------------------------------------------===//
bool EventOp::hasSingleSize() { return getOperation()->hasAttr("singleSize"); }
void EventOp::setHasSingleSize() {
  getOperation()->setAttr("singleSize", UnitAttr::get(getContext()));
}

// TODO: Fix inferReturnTypes method signature
// LogicalResult DbAcquireOp::inferReturnTypes(
//     MLIRContext *context, std::optional<Location> location,
//     ValueShapeRange operands, DictionaryAttr attributes,
//     OpaqueProperties properties, RegionRange regions,
//     SmallVectorImpl<Type> &inferredReturnTypes) {
//   // Infer from source type and subregion params
//   auto sourceType = dyn_cast<MemRefType>(getSource().getType());
//   if (!sourceType)
//     return failure();
//   ArrayRef<int64_t> shapeRef = sourceType.getShape();
//   SmallVector<int64_t> shape(shapeRef.begin(), shapeRef.end());
//   // Adjust shape based on sizes/offsets (simplified)
//   inferredReturnTypes.push_back(
//       MemRefType::get(shape, sourceType.getElementType()));
//   return success();
// }