//===----------------------------------------------------------------------===//
// ArtsDialect.cpp - Arts dialect
// Defines the Arts dialect and the operations within it.
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/IntegerSet.h"
#include "mlir/IR/TypeUtilities.h"
#include "mlir/IR/Types.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/SmallVector.h"
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

//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// EdtOp
//===----------------------------------------------------------------------===//
SmallVector<Value> mlir::arts::EdtOp::getDependenciesAsVector() {
  SmallVector<Value> deps(getDependencies().begin(), getDependencies().end());
  return deps;
}

void EdtOp::getEffects(
    SmallVectorImpl<MemoryEffects::EffectInstance> &effects) {
  /// Collect all effects from the dependencies using safe method
  auto dependencies = getDependenciesAsVector();

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
  /// for (auto dep : dependencies) {
  /// TODO: Implement getEffects for DbDepOp
  /// if (auto dbOp = dep.getDefiningOp<DbDepOp>())
  ///   dbOp.getEffects(effects);
  /// }
}

//===----------------------------------------------------------------------===//
// ARTS Operation Builders
//===----------------------------------------------------------------------===//
void DbReleaseOp::build(OpBuilder &builder, OperationState &state,
                        Value source) {
  state.addOperands(source);
}

void DbControlOp::build(OpBuilder &builder, OperationState &state,
                        ArtsMode mode, Value ptr,
                        SmallVector<Value> pinnedIndices) {
  auto ptrOp = ptr.getDefiningOp();
  assert(ptrOp && "Input must be a defining operation.");

  /// If the defining op is a load op, obtain the base memref and pinned
  /// indices.
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

  /// Ensure the base memref type.
  auto baseType = baseMemRef.getType().dyn_cast<MemRefType>();
  assert(baseType && "Input must be a MemRefType.");

  int64_t rank = baseType.getRank();
  const auto pinnedCount = static_cast<int64_t>(pinnedIndices.size());
  assert(pinnedCount <= rank &&
         "Pinned indices exceed the rank of the memref.");

  /// Compute sizes and subshape.
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
      /// For the normal subview type, preserve the static dim if available.
      subShape.push_back(isDyn ? ShapedType::kDynamic : dimSize);
      j++;
    }
  }

  /// Compute the element type size.
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

  /// Build operand segment sizes
  state.addAttribute(
      "operandSegmentSizes",
      builder.getDenseI32ArrayAttr({1, /// ptr
                                    1, /// elementTypeSize
                                    static_cast<int32_t>(indices.size()),
                                    static_cast<int32_t>(offsets.size()),
                                    static_cast<int32_t>(sizes.size())}));

  auto resultType = MemRefType::get(subShape, elementType);
  state.addTypes(resultType);
}

void EventOp::build(OpBuilder &builder, OperationState &state,
                    ArrayRef<Value> sizes) {
  SmallVector<Value> eventSizes(sizes.begin(), sizes.end());
  /// Assuming event type is memref<?x...xi8> or similar; adjust if needed
  SmallVector<int64_t> shape(sizes.size(), ShapedType::kDynamic);
  MemRefType eventType = MemRefType::get(shape, builder.getI8Type());

  state.addOperands(eventSizes);
  state.addTypes(eventType);
}

//===----------------------------------------------------------------------===//
// EDT Ops
//===----------------------------------------------------------------------===//
void EdtOp::build(OpBuilder &builder, OperationState &state, EdtType type,
                  ValueRange dependencies) {
  state.addAttribute("type", EdtTypeAttr::get(builder.getContext(), type));
  Value route = builder.create<arith::ConstantIntOp>(state.location, 0, 32);
  state.addOperands(route);
  state.addOperands(dependencies);

  /// Create the region with a block
  Region *bodyRegion = state.addRegion();
  Block *bodyBlock = new Block();
  bodyRegion->push_back(bodyBlock);
}

void EdtOp::build(OpBuilder &builder, OperationState &state, EdtType type) {
  build(builder, state, type, ValueRange{});
}

void EdtCreateOp::build(OpBuilder &builder, OperationState &state,
                        Value param_memref, Value depCount) {
  Value route = builder.create<arith::ConstantIntOp>(state.location, 0, 32);
  state.addTypes(builder.getI64Type());
  state.addOperands({param_memref, depCount, route});
}

//===----------------------------------------------------------------------===//
// EventOp
//===----------------------------------------------------------------------===//
bool EventOp::hasSingleSize() { return getOperation()->hasAttr("singleSize"); }
void EventOp::setHasSingleSize() {
  getOperation()->setAttr("singleSize", UnitAttr::get(getContext()));
}

//===----------------------------------------------------------------------===//
// DB operations builders
//===----------------------------------------------------------------------===//
void DbAcquireOp::build(OpBuilder &builder, OperationState &state,
                        ArtsMode mode, Value source, SmallVector<Value> indices,
                        SmallVector<Value> offsets, SmallVector<Value> sizes) {
  /// Auto-compute result types
  Type guidType = MemRefType::get({}, builder.getI64Type());

  /// Infer ptr type from source memref type and sizes
  auto sourceMemRefType = llvm::cast<MemRefType>(source.getType());
  Type elementType = sourceMemRefType.getElementType();

  SmallVector<int64_t> shape;
  if (sizes.empty()) {
    /// No sizes provided, use source shape
    shape.assign(sourceMemRefType.getShape().begin(),
                 sourceMemRefType.getShape().end());
  } else {
    /// Sizes provided, create dynamic shape
    for (size_t i = 0; i < sizes.size(); ++i)
      shape.push_back(ShapedType::kDynamic);
  }
  Type ptrType = MemRefType::get(shape, elementType);

  state.addTypes({guidType, ptrType});

  /// Add operands and attributes
  ArtsModeAttr modeAttr = ArtsModeAttr::get(builder.getContext(), mode);
  state.addAttribute("mode", modeAttr);

  state.addOperands(source);
  state.addOperands(indices);
  state.addOperands(offsets);
  state.addOperands(sizes);

  /// Build operand segment sizes: [source=1, indices, offsets, sizes]
  state.addAttribute(
      "operandSegmentSizes",
      builder.getDenseI32ArrayAttr({1, static_cast<int32_t>(indices.size()),
                                    static_cast<int32_t>(offsets.size()),
                                    static_cast<int32_t>(sizes.size())}));
}

void DbAllocOp::build(OpBuilder &builder, OperationState &state, ArtsMode mode,
                      Value route, DbAllocType allocType, DbMode dbMode,
                      Value address, SmallVector<Value> sizes,
                      SmallVector<Value> payloadSizes) {
  /// Auto-compute result types
  Type guidType = MemRefType::get({}, builder.getI64Type());

  /// Build ptr type from address shape (if any) or dynamic sizes with payload
  /// element type
  Type elemTy = builder.getI8Type();
  SmallVector<int64_t> shape;
  if (address && isa<MemRefType>(address.getType())) {
    auto mt = cast<MemRefType>(address.getType());
    elemTy = mt.getElementType();
    shape.assign(mt.getShape().begin(), mt.getShape().end());
  } else {
    for (size_t i = 0; i < sizes.size(); ++i)
      shape.push_back(ShapedType::kDynamic);
  }
  Type ptrType = MemRefType::get(shape, elemTy);

  state.addTypes({guidType, ptrType});

  /// Add operands and attributes
  ArtsModeAttr modeAttr = ArtsModeAttr::get(builder.getContext(), mode);
  DbAllocTypeAttr allocTypeAttr =
      DbAllocTypeAttr::get(builder.getContext(), allocType);
  DbModeAttr dbModeAttr = DbModeAttr::get(builder.getContext(), dbMode);
  /// Retain payload element type attribute using elemTy
  TypeAttr elementTypeAttr = TypeAttr::get(elemTy);

  state.addAttribute("mode", modeAttr);
  state.addAttribute("allocType", allocTypeAttr);
  state.addAttribute("dbMode", dbModeAttr);
  state.addAttribute("elementType", elementTypeAttr);

  state.addOperands(route);
  if (address)
    state.addOperands(address);
  state.addOperands(sizes);
  state.addOperands(payloadSizes);

  /// Build operand segment sizes: [route=1, address(0/1), sizes, payloadSizes]
  state.addAttribute("operandSegmentSizes",
                     builder.getDenseI32ArrayAttr(
                         {1, static_cast<int32_t>(address ? 1 : 0),
                          static_cast<int32_t>(sizes.size()),
                          static_cast<int32_t>(payloadSizes.size())}));
}

void DbAllocOp::build(OpBuilder &builder, OperationState &state, ArtsMode mode,
                      Value route, DbAllocType allocType, DbMode dbMode,
                      Type elementType, Value address, SmallVector<Value> sizes,
                      SmallVector<Value> payloadSizes) {
  /// Auto-compute result types
  Type guidType = MemRefType::get({}, builder.getI64Type());

  /// Build ptr type using provided elementType and address shape or sizes
  SmallVector<int64_t> shape;
  if (address && isa<MemRefType>(address.getType())) {
    auto mt = cast<MemRefType>(address.getType());
    shape.assign(mt.getShape().begin(), mt.getShape().end());
  } else {
    for (size_t i = 0; i < sizes.size(); ++i)
      shape.push_back(ShapedType::kDynamic);
  }
  Type ptrType = MemRefType::get(shape, elementType);

  state.addTypes({guidType, ptrType});

  /// Add operands and attributes
  ArtsModeAttr modeAttr = ArtsModeAttr::get(builder.getContext(), mode);
  DbAllocTypeAttr allocTypeAttr =
      DbAllocTypeAttr::get(builder.getContext(), allocType);
  DbModeAttr dbModeAttr = DbModeAttr::get(builder.getContext(), dbMode);
  TypeAttr elementTypeAttr = TypeAttr::get(elementType);

  state.addAttribute("mode", modeAttr);
  state.addAttribute("allocType", allocTypeAttr);
  state.addAttribute("dbMode", dbModeAttr);
  state.addAttribute("elementType", elementTypeAttr);

  state.addOperands(route);
  if (address)
    state.addOperands(address);
  state.addOperands(sizes);
  state.addOperands(payloadSizes);

  /// Build operand segment sizes: [route=1, address(0/1), sizes, payloadSizes]
  state.addAttribute("operandSegmentSizes",
                     builder.getDenseI32ArrayAttr(
                         {1, static_cast<int32_t>(address ? 1 : 0),
                          static_cast<int32_t>(sizes.size()),
                          static_cast<int32_t>(payloadSizes.size())}));
}

void DbAllocOp::build(OpBuilder &builder, OperationState &state, ArtsMode mode,
                      Value route, DbAllocType allocType, DbMode dbMode,
                      Type elementType, SmallVector<Value> sizes,
                      SmallVector<Value> payloadSizes) {
  /// Auto-compute result types
  Type guidType = MemRefType::get({}, builder.getI64Type());

  /// Build ptr type using provided elementType and dynamic shape from sizes
  SmallVector<int64_t> shape;
  for (size_t i = 0; i < sizes.size(); ++i)
    shape.push_back(ShapedType::kDynamic);
  Type ptrType = MemRefType::get(shape, elementType);

  state.addTypes({guidType, ptrType});

  /// Add operands and attributes
  ArtsModeAttr modeAttr = ArtsModeAttr::get(builder.getContext(), mode);
  DbAllocTypeAttr allocTypeAttr =
      DbAllocTypeAttr::get(builder.getContext(), allocType);
  DbModeAttr dbModeAttr = DbModeAttr::get(builder.getContext(), dbMode);
  TypeAttr elementTypeAttr = TypeAttr::get(elementType);

  state.addAttribute("mode", modeAttr);
  state.addAttribute("allocType", allocTypeAttr);
  state.addAttribute("dbMode", dbModeAttr);
  state.addAttribute("elementType", elementTypeAttr);

  state.addOperands(route);
  // No address operand for this builder
  state.addOperands(sizes);
  state.addOperands(payloadSizes);

  /// Build operand segment sizes: [route=1, address=0, sizes, payloadSizes]
  state.addAttribute("operandSegmentSizes",
                     builder.getDenseI32ArrayAttr(
                         {1, 0, static_cast<int32_t>(sizes.size()),
                          static_cast<int32_t>(payloadSizes.size())}));
}

/// Custom builders for DbNumElementsOp that automatically extract sizes
void DbNumElementsOp::build(OpBuilder &builder, OperationState &state,
                            DbAllocOp allocOp) {
  /// Extract sizes from the DbAllocOp
  SmallVector<Value> sizes = allocOp.getSizes();
  build(builder, state, sizes);
}

void DbNumElementsOp::build(OpBuilder &builder, OperationState &state,
                            DbAcquireOp acquireOp) {
  /// Extract sizes from the DbAcquireOp
  SmallVector<Value> sizes = acquireOp.getSizes();
  build(builder, state, sizes);
}

///==========================================================================
/// Utility functions for Arts dialect operations
///==========================================================================

/// Helper function to extract sizes from a datablock pointer
/// by finding the original DbAllocOp or DbAcquireOp that created it
SmallVector<Value> getSizesFromDb(Value datablockPtr) {
  /// Find the defining operation of the datablock pointer
  Operation *defOp = datablockPtr.getDefiningOp();
  if (!defOp)
    return {};

  /// Check if it's a DbAllocOp result
  if (auto allocOp = dyn_cast<DbAllocOp>(defOp)) {
    return SmallVector<Value>(allocOp.getSizes().begin(),
                              allocOp.getSizes().end());
  }

  /// Check if it's a DbAcquireOp result
  if (auto acquireOp = dyn_cast<DbAcquireOp>(defOp)) {
    return SmallVector<Value>(acquireOp.getSizes().begin(),
                              acquireOp.getSizes().end());
  }

  // If we can't find the sizes, return empty (will result in 1 element)
  return {};
}
