//===- ArtsDialect.cpp - Arts dialect ---------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Removed: ArtsTypes.h not needed in dialect implementation
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/IntegerSet.h"
#include "mlir/IR/TypeUtilities.h"
#include "mlir/IR/Types.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/TypeSwitch.h"
#include <cassert>

#include "arts/ArtsDialect.h"
#include "polygeist/Ops.h"

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
// Arts Dialect Enums - method definitions
//===----------------------------------------------------------------------===//
#include "arts/ArtsOpsEnums.cpp.inc"

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

  for (auto _ : sizeOperands)
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
//                             ValueShapeRange operands, DictionaryAttr
//                             attributes, OpaqueProperties properties,
//                             RegionRange regions, SmallVectorImpl<Type>
//                             &inferredReturnTypes) {
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
// ARTS Operation Builders
//===----------------------------------------------------------------------===//

void DbAllocOp::build(OpBuilder &builder, OperationState &state, ArtsMode mode,
                      SmallVector<Value> sizes, Value address) {
  state.addAttribute("mode", ArtsModeAttr::get(builder.getContext(), mode));
  if (address) {
    state.addOperands(address);
  }
  state.addOperands(sizes);

  // Build operand segment sizes
  state.addAttribute(
      "operandSegmentSizes",
      builder.getDenseI32ArrayAttr(
          {address ? 1 : 0, static_cast<int32_t>(sizes.size())}));

  // Infer result type
  MemRefType ptrType;
  if (address) {
    auto addressType = address.getType().dyn_cast<MemRefType>();
    assert(addressType && "Expected a MemRefType");

    // If sizes are not provided, extract them from the address memref
    if (sizes.empty()) {
      for (int64_t i = 0; i < addressType.getRank(); ++i) {
        if (addressType.isDynamicDim(i)) {
          sizes.push_back(
              builder.create<memref::DimOp>(state.location, address, i));
        } else {
          sizes.push_back(builder.create<arith::ConstantIndexOp>(
              state.location, addressType.getDimSize(i)));
        }
      }
    }

    ptrType =
        MemRefType::get(addressType.getShape(), addressType.getElementType());
  } else {
    // If no address is provided, create a basic i32 memref type
    SmallVector<int64_t> shape;
    for (auto _ : sizes)
      shape.push_back(ShapedType::kDynamic);
    ptrType = MemRefType::get(shape, builder.getI32Type());
  }

  state.addTypes(ptrType);
}

void DbAcquireOp::build(OpBuilder &builder, OperationState &state,
                        ArtsMode mode, Value source,
                        SmallVector<Value> pinnedIndices,
                        SmallVector<Value> offsets, SmallVector<Value> sizes) {
  state.addAttribute("mode", ArtsModeAttr::get(builder.getContext(), mode));
  state.addOperands(source);
  state.addOperands(pinnedIndices);
  state.addOperands(offsets);
  state.addOperands(sizes);

  // Build operand segment sizes
  state.addAttribute(
      "operandSegmentSizes",
      builder.getDenseI32ArrayAttr({1, // source
                                    static_cast<int32_t>(pinnedIndices.size()),
                                    static_cast<int32_t>(offsets.size()),
                                    static_cast<int32_t>(sizes.size())}));

  // Infer result type
  auto resultType = source.getType().dyn_cast<MemRefType>();
  assert(resultType && "Source must be a MemRefType");

  // Adjust result type if pinned indices reduce rank
  int64_t originalRank = resultType.getRank();
  int64_t pinnedCount = pinnedIndices.size();
  if (pinnedCount > 0) {
    SmallVector<int64_t> newShape;
    for (int64_t i = pinnedCount; i < originalRank; ++i)
      newShape.push_back(resultType.getDimSize(i));
    resultType = MemRefType::get(newShape, resultType.getElementType());
  }

  state.addTypes(resultType);
}

void DbReleaseOp::build(OpBuilder &builder, OperationState &state,
                        Value source) {
  state.addOperands(source);
}

void DbControlOp::build(OpBuilder &builder, OperationState &state,
                        ArtsMode mode, Value ptr,
                        SmallVector<Value> pinnedIndices) {
  auto ptrOp = ptr.getDefiningOp();
  assert(ptrOp && "Input must be a defining operation.");

  // If the defining op is a load op, obtain the base memref and pinned indices.
  Value baseMemRef;
  auto processLoad = [&](auto loadOp) {
    baseMemRef = loadOp.getMemref();
    if (pinnedIndices.empty())
      pinnedIndices.assign(loadOp.getIndices().begin(),
                           loadOp.getIndices().end());
  };

  if (auto loadOp = dyn_cast<memref::LoadOp>(ptrOp))
    processLoad(loadOp);
  else
    baseMemRef = ptr;

  // Ensure the base memref type.
  auto baseType = baseMemRef.getType().dyn_cast<MemRefType>();
  assert(baseType && "Input must be a MemRefType.");

  int64_t rank = baseType.getRank();
  const auto pinnedCount = static_cast<int64_t>(pinnedIndices.size());
  assert(pinnedCount <= rank &&
         "Pinned indices exceed the rank of the memref.");

  // Compute sizes and subshape.
  SmallVector<Value, 4> indices(pinnedCount), sizes(rank - pinnedCount);
  SmallVector<Value, 4> offsets(
      rank - pinnedCount,
      builder.create<arith::ConstantIndexOp>(state.location, 0));
  SmallVector<int64_t, 4> subShape;
  for (int64_t i = 0, j = 0; i < rank; ++i) {
    if (i < pinnedCount) {
      indices[i] = pinnedIndices[i];
    } else {
      bool isDyn = baseType.isDynamicDim(i);
      int64_t dimSize = baseType.getDimSize(i);
      Value dimVal =
          isDyn
              ? builder.create<memref::DimOp>(state.location, baseMemRef, i)
                    .getResult()
              : builder.create<arith::ConstantIndexOp>(state.location, dimSize)
                    .getResult();
      sizes[j] = dimVal;
      // For the normal subview type, preserve the static dim if available.
      subShape.push_back(isDyn ? ShapedType::kDynamic : dimSize);
      j++;
    }
  }

  // Compute the element type size.
  auto elementType = baseType.getElementType();
  auto elementTypeSize =
      builder
          .create<polygeist::TypeSizeOp>(state.location, builder.getIndexType(),
                                         elementType)
          .getResult();

  state.addAttribute("mode", ArtsModeAttr::get(builder.getContext(), mode));
  state.addOperands(baseMemRef);
  state.addAttribute("elementType", TypeAttr::get(elementType));
  state.addOperands(elementTypeSize);
  state.addOperands(indices);
  state.addOperands(offsets);
  state.addOperands(sizes);

  // Build operand segment sizes
  state.addAttribute(
      "operandSegmentSizes",
      builder.getDenseI32ArrayAttr({1, // ptr
                                    1, // elementTypeSize
                                    static_cast<int32_t>(indices.size()),
                                    static_cast<int32_t>(offsets.size()),
                                    static_cast<int32_t>(sizes.size())}));

  auto resultType = MemRefType::get(subShape, elementType);
  state.addTypes(resultType);
}

void EventOp::build(OpBuilder &builder, OperationState &state,
                    ArrayRef<Value> sizes) {
  SmallVector<Value> eventSizes(sizes.begin(), sizes.end());
  // Assuming event type is memref<?x...xi8> or similar; adjust if needed
  SmallVector<int64_t> shape(sizes.size(), ShapedType::kDynamic);
  MemRefType eventType = MemRefType::get(shape, builder.getI8Type());

  state.addOperands(eventSizes);
  state.addTypes(eventType);
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