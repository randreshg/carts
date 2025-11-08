///==========================================================================///
/// File: ArtsDialect.cpp
/// Defines the Arts dialect and the operations within it.
///==========================================================================///
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/IntegerSet.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/TypeUtilities.h"
#include "mlir/IR/Types.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/TypeSwitch.h"
#include <cassert>
#include <cstdint>

#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsUtils.h"
#include "polygeist/Ops.h"

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
// Arts dialect.
///===----------------------------------------------------------------------===///
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

///===----------------------------------------------------------------------===///
// Arts Dialect Operations - method definitions
///===----------------------------------------------------------------------===///
#define GET_OP_CLASSES
#include "arts/ArtsOps.cpp.inc"

namespace {
struct FoldDbDimFromDbOps : public OpRewritePattern<DbDimOp> {
  using OpRewritePattern<DbDimOp>::OpRewritePattern;
  LogicalResult matchAndRewrite(DbDimOp op,
                                PatternRewriter &rewriter) const override {
    auto cstIdx = op.getDim().getDefiningOp<arith::ConstantIndexOp>();
    if (!cstIdx)
      return failure();
    int64_t idx = cstIdx.getValue().cast<IntegerAttr>().getInt();

    if (auto dbAlloc = op.getSource().getDefiningOp<DbAllocOp>()) {
      auto sizes = dbAlloc.getSizes();
      if ((long long)sizes.size() > idx) {
        rewriter.replaceOp(op, sizes[idx]);
        return success();
      }
    }
    if (auto dbAcq = op.getSource().getDefiningOp<DbAcquireOp>()) {
      auto sizes = dbAcq.getSizes();
      if ((long long)sizes.size() > idx) {
        rewriter.replaceOp(op, sizes[idx]);
        return success();
      }
    }
    /// Fallback: query with memref.dim if source is not a DB op
    rewriter.replaceOpWithNewOp<memref::DimOp>(op, op.getSource(), idx);
    return success();
  }
};
} // namespace

void DbDimOp::getCanonicalizationPatterns(RewritePatternSet &results,
                                          MLIRContext *context) {
  results.add<FoldDbDimFromDbOps>(context);
}

bool isArtsRegion(Operation *op) { return isa<EdtOp>(op) || isa<EpochOp>(op); }
bool isArtsOp(Operation *op) {
  return isArtsRegion(op) ||
         isa<arts::EdtOp, arts::EpochOp, arts::BarrierOp, arts::AllocOp,
             arts::DbAllocOp, arts::DbAcquireOp, arts::DbReleaseOp,
             arts::DbFreeOp, arts::DbControlOp, arts::GetTotalWorkersOp,
             arts::GetTotalNodesOp, arts::GetCurrentWorkerOp,
             arts::GetCurrentNodeOp>(op);
}

///===----------------------------------------------------------------------===///
// Arts Dialect Types - method definitions
///===----------------------------------------------------------------------===///
#define GET_TYPEDEF_CLASSES
#include "arts/ArtsOpsTypes.cpp.inc"

///===----------------------------------------------------------------------===///
// Arts Dialect Attributes - method definitions
///===----------------------------------------------------------------------===///
#define GET_ATTRDEF_CLASSES
#include "arts/ArtsOpsAttributes.cpp.inc"

///===----------------------------------------------------------------------===///
// Arts Dialect Enums - method definitions
///===----------------------------------------------------------------------===///
#include "arts/ArtsOpsEnums.cpp.inc"

///===----------------------------------------------------------------------===///
// UndefOp
///===----------------------------------------------------------------------===///
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

///===----------------------------------------------------------------------===///
// EdtOp
///===----------------------------------------------------------------------===///
SmallVector<Value> mlir::arts::EdtOp::getDependenciesAsVector() {
  SmallVector<Value> deps(getDependencies().begin(), getDependencies().end());
  return deps;
}

LogicalResult EdtOp::verify() {
  /// Skip verification if no_verify attribute is present
  if (getNoVerifyAttr())
    return success();

  Block &block = getBody().front();
  auto blockArgs = block.getArguments();
  auto deps = getDependenciesAsVector();

  /// Check that dependencies and block arguments match
  if (blockArgs.size() != deps.size()) {
    return emitOpError("block arguments (")
           << blockArgs.size() << ") must match dependencies (" << deps.size()
           << ")";
  }

  /// Check that all dependencies come from DbAcquireOp operations
  for (Value dep : deps) {
    auto dbAcquireOp = dep.getDefiningOp<DbAcquireOp>();
    if (!dbAcquireOp) {
      return emitOpError("dependency must be a result of DbAcquireOp, got: ")
             << dep;
    }
  }

  /// Collect all values defined outside the EDT region
  DenseSet<Value> externalValues;
  for (Value dep : deps)
    externalValues.insert(dep);

  /// Walk all operations in the EDT body
  auto walkResult = getBody().walk([&](Operation *op) {
    /// Skip the block itself
    if (op == &block.front())
      return WalkResult::advance();

    /// Skip children EDTs
    if (isa<EdtOp>(op))
      return WalkResult::advance();

    /// Check each operand
    for (Value operand : op->getOperands()) {
      /// Skip if operand is not a memref
      if (!operand.getType().isa<MemRefType>())
        continue;

      /// Skip block arguments - these are allowed
      if (llvm::is_contained(blockArgs, operand)) {
        /// Verify that the operand is a DbAcquireOp
        DbAcquireOp underlyingAcquire =
            dyn_cast<DbAcquireOp>(arts::getUnderlyingDb(operand));
        if (!underlyingAcquire) {
          op->emitOpError("EDT region uses block argument '")
              << operand << "' as a DbAcquire value.";
          return WalkResult::interrupt();
        }
        /// Block argument is valid, skip remaining checks
        continue;
      }

      /// Get the underlying operation for the operand
      auto definingOp = operand.getDefiningOp();
      if (!definingOp)
        continue;

      /// Skip if operand is defined inside the region
      if (getBody().isAncestor(definingOp->getParentRegion()))
        continue;

      /// Check if this is a dependency value used directly
      if (externalValues.contains(operand)) {
        op->emitOpError("EDT region uses external DbAcquire value '")
            << operand << "' directly instead of DbAcquire block argument. ";
        return WalkResult::interrupt();
      }

      /// If we  got this far, the operand is not a block argument, not a
      /// dependency, and not defined inside the region
      op->emitOpError("EDT region uses external value '")
          << operand
          << "' that is not a block argument, not a dependency, and not "
             "defined inside the region.\n"
          << *definingOp << "\n"
          << *op << "\n";
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });

  if (walkResult.wasInterrupted())
    return failure();

  return success();
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

///===----------------------------------------------------------------------===///
// ARTS Operation Builders
///===----------------------------------------------------------------------===///
void DbReleaseOp::build(OpBuilder &builder, OperationState &state,
                        Value source) {
  state.addOperands(source);
}

void DbDimOp::build(OpBuilder &builder, OperationState &state, Value source,
                    int64_t dim) {
  state.addOperands(source);
  auto c = builder.create<arith::ConstantIndexOp>(state.location, dim);
  state.addOperands(c.getResult());
  state.addTypes(builder.getIndexType());
}

void DbGepOp::build(OpBuilder &builder, OperationState &state, Type ptr,
                    Value base_ptr, SmallVector<Value> indices,
                    SmallVector<Value> strides) {
  state.addOperands(base_ptr);
  state.addOperands(indices);
  state.addOperands(strides);
  /// Set operand segment sizes: [base_ptr, indices..., strides...]
  SmallVector<int32_t, 3> segments = {1, (int32_t)indices.size(),
                                      (int32_t)strides.size()};
  state.addAttribute(getOperandSegmentSizesAttrName(state.name),
                     builder.getDenseI32ArrayAttr(segments));
  state.addTypes(ptr);
}

void DbControlOp::build(OpBuilder &builder, OperationState &state,
                        ArtsMode mode, Value ptr, SmallVector<Value> indices,
                        SmallVector<Value> offsets, SmallVector<Value> sizes) {
  auto subviewType =
      MemRefType::get({ShapedType::kDynamic}, builder.getI64Type());
  DbControlOp::build(builder, state, subviewType, mode, ptr, indices, offsets,
                     sizes);
}

///===----------------------------------------------------------------------===///
// EDT Ops
///===----------------------------------------------------------------------===///
void EdtOp::build(OpBuilder &builder, OperationState &state, EdtType type,
                  EdtConcurrency concurrency) {
  build(builder, state, type, concurrency, ValueRange{});
}

void EdtOp::build(OpBuilder &builder, OperationState &state, EdtType type,
                  EdtConcurrency concurrency, ValueRange dependencies) {
  Value route = builder.create<arith::ConstantIntOp>(state.location, 0, 32);
  build(builder, state, type, concurrency, route, dependencies);
}

void EdtOp::build(OpBuilder &builder, OperationState &state, EdtType type,
                  EdtConcurrency concurrency, Value route,
                  ValueRange dependencies) {
  state.addAttribute("type", EdtTypeAttr::get(builder.getContext(), type));
  state.addAttribute("concurrency", EdtConcurrencyAttr::get(
                                        builder.getContext(), concurrency));
  state.addOperands(route);
  state.addOperands(dependencies);

  /// Create the region with a block
  Region *bodyRegion = state.addRegion();
  Block *bodyBlock = new Block();
  bodyRegion->push_back(bodyBlock);
}

///===----------------------------------------------------------------------===///
// EDT Create Ops
///===----------------------------------------------------------------------===///

void EdtCreateOp::build(OpBuilder &builder, OperationState &state,
                        Value param_memref, Value depCount) {
  Value route = builder.create<arith::ConstantIntOp>(state.location, 0, 32);
  state.addTypes(builder.getI64Type());
  state.addOperands({param_memref, depCount, route});
}

void EdtCreateOp::build(OpBuilder &builder, OperationState &state,
                        Value param_memref, Value depCount, Value route) {
  state.addTypes(builder.getI64Type());
  state.addOperands({param_memref, depCount, route});
}

///===----------------------------------------------------------------------===///
// DB operations builders
///===----------------------------------------------------------------------===///
/// DbAllocOp builders
static void
buildDbAllocOpCommon(OpBuilder &builder, OperationState &state, ArtsMode mode,
                     Value route, DbAllocType allocType, DbMode dbMode,
                     Type elementType, Value address, SmallVector<Value> sizes,
                     SmallVector<Value> elementSizes, Type pointerType) {
  /// Auto-compute GUID type to match datablock dimensionality
  /// 0 or 1 size -> memref<?xi64>, n sizes -> memref<?x?x...xi64>
  SmallVector<int64_t> guidShape;
  size_t numDims = std::max(sizes.size(), size_t(1));
  for (size_t i = 0; i < numDims; ++i)
    guidShape.push_back(ShapedType::kDynamic);
  Type guidType = MemRefType::get(guidShape, builder.getI64Type());

  /// If outersizes are empty, set to 1
  if (sizes.empty())
    sizes.push_back(builder.create<arith::ConstantIndexOp>(state.location, 1));

  /// If element sizes are empty, set to 1
  if (elementSizes.empty())
    elementSizes.push_back(
        builder.create<arith::ConstantIndexOp>(state.location, 1));

  Type ptrType;
  if (pointerType) {
    ptrType = pointerType;
  } else {
    Type pointerElementType =
        arts::getElementMemRefType(elementType, elementSizes);
    SmallVector<int64_t> shape;
    shape.assign(sizes.size(), ShapedType::kDynamic);
    ptrType = MemRefType::get(shape, MemRefType::get({}, pointerElementType));
  }
  state.addTypes({guidType, ptrType});

  /// Add attributes
  ArtsModeAttr modeAttr = ArtsModeAttr::get(builder.getContext(), mode);
  DbAllocTypeAttr allocTypeAttr =
      DbAllocTypeAttr::get(builder.getContext(), allocType);
  DbModeAttr dbModeAttr = DbModeAttr::get(builder.getContext(), dbMode);
  TypeAttr elementTypeAttr = TypeAttr::get(elementType);

  state.addAttribute("mode", modeAttr);
  state.addAttribute("allocType", allocTypeAttr);
  state.addAttribute("dbMode", dbModeAttr);
  state.addAttribute("elementType", elementTypeAttr);

  /// Add operands
  state.addOperands(route);
  if (address)
    state.addOperands(address);
  state.addOperands(sizes);
  state.addOperands(elementSizes);

  /// Build operand segment sizes: [route=1, address(0/1), sizes,
  /// elementSizes]
  state.addAttribute("operandSegmentSizes",
                     builder.getDenseI32ArrayAttr(
                         {1, static_cast<int32_t>(address ? 1 : 0),
                          static_cast<int32_t>(sizes.size()),
                          static_cast<int32_t>(elementSizes.size())}));
}

void DbAllocOp::build(OpBuilder &builder, OperationState &state, ArtsMode mode,
                      Value route, DbAllocType allocType, DbMode dbMode,
                      Type elementType, Value address, SmallVector<Value> sizes,
                      SmallVector<Value> elementSizes) {
  buildDbAllocOpCommon(builder, state, mode, route, allocType, dbMode,
                       elementType, address, sizes, elementSizes, nullptr);
}

void DbAllocOp::build(OpBuilder &builder, OperationState &state, ArtsMode mode,
                      Value route, DbAllocType allocType, DbMode dbMode,
                      Type elementType, SmallVector<Value> sizes,
                      SmallVector<Value> elementSizes) {
  buildDbAllocOpCommon(builder, state, mode, route, allocType, dbMode,
                       elementType, Value{}, sizes, elementSizes, nullptr);
}

void DbAllocOp::build(OpBuilder &builder, OperationState &state, ArtsMode mode,
                      Value route, DbAllocType allocType, DbMode dbMode,
                      Type elementType, Type pointerType,
                      SmallVector<Value> sizes,
                      SmallVector<Value> elementSizes) {
  buildDbAllocOpCommon(builder, state, mode, route, allocType, dbMode,
                       elementType, Value{}, sizes, elementSizes, pointerType);
}

MemRefType DbAllocOp::getAllocatedElementType() {
  return MemRefType::get(
      {}, arts::getElementMemRefType(
              getElementType(), SmallVector<Value>(getElementSizes().begin(),
                                                   getElementSizes().end())));
}

/// DbAcquireOp builders
void DbAcquireOp::build(OpBuilder &builder, OperationState &state,
                        ArtsMode mode, Value sourceGuid, Value sourcePtr,
                        SmallVector<Value> indices, SmallVector<Value> offsets,
                        SmallVector<Value> sizes) {
  auto sourceDb = arts::getUnderlyingDb(sourcePtr);
  auto sourceDbAlloc =
      dyn_cast<DbAllocOp>(arts::getUnderlyingDbAlloc(sourcePtr));
  assert(sourceDb && "Expected sourceDb");
  assert(sourceDbAlloc && "Expected sourceDbAlloc");

  /// Get the sizes from the source datablock
  auto sourceSizes = getSizesFromDb(sourceDb);
  const uint64_t sourceRank = sourceSizes.size();
  const uint64_t pinnedDims = indices.size();
  assert(pinnedDims <= sourceRank && "Number of indices exceeds source rank");

  /// Compute the remaining rank, if it is 0, we still need a pointer to the db
  uint64_t remainingRank = sourceRank - pinnedDims;
  if (remainingRank == 0)
    remainingRank = 1;

  /// If sizes are not provided, add single size for each remaining dimension
  if (sizes.empty()) {
    sizes.reserve(remainingRank);
    for (uint64_t d = 0; d < remainingRank; ++d)
      sizes.push_back(
          builder.create<arith::ConstantIndexOp>(state.location, 1));
  }

  /// Ensure offsets are present for remaining dimensions when omitted
  if (offsets.empty()) {
    offsets.reserve(remainingRank);
    for (uint64_t d = 0; d < remainingRank; ++d)
      offsets.push_back(
          builder.create<arith::ConstantIndexOp>(state.location, 0));
  }

  /// GUID type uses dimensionality equal to number of sizes (at least 1)
  SmallVector<int64_t> guidShape;
  size_t numGuidDims = std::max(sizes.size(), size_t(1));
  guidShape.assign(numGuidDims, ShapedType::kDynamic);
  Type guidType = MemRefType::get(guidShape, builder.getI64Type());

  /// The DbAcquireOp only offsets the pointer, so the type is the same as the
  /// source pointer
  Type ptrType = sourceDbAlloc.getPtr().getType().cast<MemRefType>();

  /// Result types
  state.addTypes({guidType, ptrType});

  /// Add operands and attributes
  state.addAttribute("mode", ArtsModeAttr::get(builder.getContext(), mode));
  if (sourceGuid)
    state.addOperands(sourceGuid);
  state.addOperands(sourcePtr);
  state.addOperands(indices);
  state.addOperands(offsets);
  state.addOperands(sizes);

  /// Build operand segment sizes: [source_guid(0/1), source_ptr=1, indices,
  /// offsets, sizes]
  state.addAttribute(
      "operandSegmentSizes",
      builder.getDenseI32ArrayAttr({sourceGuid ? 1 : 0, 1,
                                    static_cast<int32_t>(indices.size()),
                                    static_cast<int32_t>(offsets.size()),
                                    static_cast<int32_t>(sizes.size())}));
}

/// DbAcquireOp builder with explicit ptr type (for block arguments)
void DbAcquireOp::build(OpBuilder &builder, OperationState &state,
                        ArtsMode mode, Value sourceGuid, Value sourcePtr,
                        Type ptrType, SmallVector<Value> indices,
                        SmallVector<Value> offsets, SmallVector<Value> sizes) {
  /// When sourceGuid is null and sourcePtr is a block argument,
  /// we can't trace back to find the DB, so we use explicit sizes/offsets

  /// If sizes/offsets not provided, use defaults for rank-1
  if (sizes.empty()) {
    sizes.push_back(builder.create<arith::ConstantIndexOp>(state.location, 1));
  }
  if (offsets.empty()) {
    for (size_t i = 0; i < sizes.size(); ++i)
      offsets.push_back(
          builder.create<arith::ConstantIndexOp>(state.location, 0));
  }

  /// GUID type uses dimensionality equal to number of sizes (at least 1)
  SmallVector<int64_t> guidShape;
  size_t numGuidDims = std::max(sizes.size(), size_t(1));
  guidShape.assign(numGuidDims, ShapedType::kDynamic);
  Type guidType = MemRefType::get(guidShape, builder.getI64Type());

  /// Result types
  state.addTypes({guidType, ptrType});

  /// Add operands and attributes
  state.addAttribute("mode", ArtsModeAttr::get(builder.getContext(), mode));
  if (sourceGuid)
    state.addOperands(sourceGuid);
  state.addOperands(sourcePtr);
  state.addOperands(indices);
  state.addOperands(offsets);
  state.addOperands(sizes);

  /// Build operand segment sizes
  state.addAttribute(
      "operandSegmentSizes",
      builder.getDenseI32ArrayAttr({sourceGuid ? 1 : 0, 1,
                                    static_cast<int32_t>(indices.size()),
                                    static_cast<int32_t>(offsets.size()),
                                    static_cast<int32_t>(sizes.size())}));
}

LogicalResult DbAcquireOp::verify() {
  /// Check that the number of offsets matches the number of sizes
  auto dbSizes = getSizes().size();
  auto dbOffsets = getOffsets().size();
  if (dbOffsets != dbSizes) {
    return emitOpError("Number of offsets (")
           << dbOffsets << ") must match Number of sizes (" << dbSizes << ")";
  }

  /// Get the source pointer type and rank
  auto srcRank = getSizesFromDb(getSourcePtr()).size();
  auto dbIndices = getIndices().size();
  auto remainingRank = srcRank - dbIndices;

  /// Check that source rank minus Db indices equals number of sizes
  if (remainingRank > 0 && remainingRank != dbSizes) {
    return emitOpError("Source rank (")
           << srcRank << ") - (" << dbIndices << ") = " << remainingRank
           << " must equal Number of sizes (" << dbSizes << ")";
  }

  /// Check if the source has a single size of 1 and we are using indices
  if (dbIndices > 0) {
    Operation *sourceDb = getUnderlyingDb(getSourcePtr());
    if (dbHasSingleSize(sourceDb)) {
      return emitOpError(
          "Cannot use indices on single-element datablock (size=1)");
    }
  }

  return success();
}

/// DbRefOp builders
void DbRefOp::build(OpBuilder &builder, OperationState &state, Value source,
                    ArrayRef<Value> indices) {
  /// The dbref
  DbAllocOp dbAllocOp = dyn_cast<DbAllocOp>(arts::getUnderlyingDbAlloc(source));
  assert(dbAllocOp && "Expected dbAllocOp");
  state.addTypes(dbAllocOp.getAllocatedElementType().getElementType());
  state.addOperands(source);
  state.addOperands(indices);
  state.addAttribute(
      "operandSegmentSizes",
      builder.getDenseI32ArrayAttr({1, static_cast<int32_t>(indices.size())}));
}

LogicalResult DbRefOp::verify() {
  auto underlyingDbOp = arts::getUnderlyingDb(getSource());
  if (!underlyingDbOp)
    return emitOpError("source must be a datablock operation");

  /// Verify outer indices
  auto outerSizes = getSizesFromDb(underlyingDbOp);
  if (getIndices().size() != outerSizes.size()) {
    return emitOpError("expects ")
           << outerSizes.size() << " index operands (got "
           << getIndices().size() << ")";
  }

  /// Verify output
  DbAllocOp dbAllocOp = nullptr;
  if (auto dbAcquire = dyn_cast<DbAcquireOp>(underlyingDbOp)) {
    dbAllocOp = dyn_cast<DbAllocOp>(
        arts::getUnderlyingDbAlloc(dbAcquire.getSourcePtr()));
  } else
    dbAllocOp = dyn_cast<DbAllocOp>(underlyingDbOp);

  /// Verify output type
  auto resultType = getResult().getType().cast<MemRefType>();
  if (resultType != dbAllocOp.getAllocatedElementType().getElementType())
    return emitOpError("result type must match source element type");
  return success();
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

/// Canonicalization pattern for DbNumElementsOp
struct FoldDbNumElementsSingleSize : public OpRewritePattern<DbNumElementsOp> {
  using OpRewritePattern<DbNumElementsOp>::OpRewritePattern;
  LogicalResult matchAndRewrite(DbNumElementsOp op,
                                PatternRewriter &rewriter) const override {
    /// If we have a single size, we can rewire the value directly
    if (op.getSizes().size() == 1) {
      rewriter.replaceOp(op, op.getSizes()[0]);
      return success();
    }
    return failure();
  }
};

/// Add canonicalization patterns for DbNumElementsOp
void DbNumElementsOp::getCanonicalizationPatterns(RewritePatternSet &results,
                                                  MLIRContext *context) {
  results.add<FoldDbNumElementsSingleSize>(context);
}

///==========================================================================///
/// OmpDepOp Builder
///==========================================================================///

void OmpDepOp::build(OpBuilder &builder, OperationState &state, ArtsMode mode,
                     Value source, SmallVector<Value> indices,
                     SmallVector<Value> sizes) {
  /// Result type is the same as source memref type
  state.addTypes(source.getType());

  /// Add mode attribute
  state.addAttribute("mode", ArtsModeAttr::get(builder.getContext(), mode));

  /// Add operands: source, indices, sizes
  state.addOperands(source);
  state.addOperands(indices);
  state.addOperands(sizes);

  /// Build operand segment sizes: [source=1, indices, sizes]
  state.addAttribute(
      "operandSegmentSizes",
      builder.getDenseI32ArrayAttr({1, static_cast<int32_t>(indices.size()),
                                    static_cast<int32_t>(sizes.size())}));
}

///==========================================================================///
/// Utility functions for Arts dialect operations
///==========================================================================///

/// Helper function to extract sizes from a datablock pointer
/// by finding the original DbAllocOp or DbAcquireOp that created it
SmallVector<Value> getSizesFromDb(Operation *dbOp) {
  if (auto allocOp = dyn_cast<DbAllocOp>(dbOp)) {
    return SmallVector<Value>(allocOp.getSizes().begin(),
                              allocOp.getSizes().end());
  }
  if (auto acquireOp = dyn_cast<DbAcquireOp>(dbOp)) {
    return SmallVector<Value>(acquireOp.getSizes().begin(),
                              acquireOp.getSizes().end());
  }
  if (auto depDbAcquireOp = dyn_cast<DepDbAcquireOp>(dbOp)) {
    return SmallVector<Value>(depDbAcquireOp.getSizes().begin(),
                              depDbAcquireOp.getSizes().end());
  }
  return {};
}

SmallVector<Value> getElementSizesFromDb(Operation *dbOp) {
  if (auto allocOp = dyn_cast<DbAllocOp>(dbOp)) {
    return SmallVector<Value>(allocOp.getElementSizes().begin(),
                              allocOp.getElementSizes().end());
  }
  return {};
}

SmallVector<Value> getSizesFromDb(Value datablockPtr) {
  /// Use getUnderlyingDb to find the original DB operation
  Operation *underlyingDb = arts::getUnderlyingDb(datablockPtr);
  if (!underlyingDb)
    return {};

  return getSizesFromDb(underlyingDb);
}

/// Check if a datablock operation has a single size of 1
bool dbHasSingleSize(Operation *dbOp) {
  if (!dbOp)
    return false;

  SmallVector<Value> sizes = getSizesFromDb(dbOp);

  if (sizes.empty())
    return false;

  /// Check if there's exactly one size and it's constant 1
  if (sizes.size() != 1)
    return false;

  if (auto constSize = sizes[0].getDefiningOp<arith::ConstantIndexOp>())
    return constSize.value() == 1;

  return false;
}

SmallVector<Value> getOffsetsFromDb(Value datablockPtr) {
  Operation *underlyingDb = arts::getUnderlyingDb(datablockPtr);
  if (!underlyingDb)
    return {};

  if (auto acquireOp = dyn_cast<DbAcquireOp>(underlyingDb))
    return SmallVector<Value>(acquireOp.getOffsets().begin(),
                              acquireOp.getOffsets().end());

  return {};
}

///===----------------------------------------------------------------------===///
// ForOp Canonicalization
///===----------------------------------------------------------------------===///

void arts::ForOp::getCanonicalizationPatterns(RewritePatternSet &patterns,
                                              MLIRContext *context) {
  /// No canonicalization patterns for now
  /// Future patterns could include:
  ///  - Loop unrolling for small iteration counts
  /// - Loop fusion for adjacent loops
  /// - Loop invariant code motion
}

///===----------------------------------------------------------------------===///
// ReductionOp Builders
///===----------------------------------------------------------------------===///

void ReductionOp::build(OpBuilder &builder, OperationState &state, Value array,
                        Value result, Value lower_bound, Value upper_bound,
                        Value step, ReductionKind reduction_kind) {
  state.addOperands({array, result, lower_bound, upper_bound, step});
  state.addAttribute(
      "reduction_kind",
      ReductionKindAttr::get(builder.getContext(), reduction_kind));
}
