///==========================================================================
// File: ConvertArtsToLLVM.cpp
//
// This file implements a pass to convert ARTS dialect operations into
// LLVM dialect operations. Since preprocessing is handled by a separate
// pass, this pass focuses purely on ARTS-specific conversion logic.
///==========================================================================

/// Dialects
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "polygeist/Ops.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
/// Others
#include "mlir/Analysis/DataLayoutAnalysis.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include <memory>

/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(convert_arts_to_llvm);

using namespace mlir;
using namespace arts;

//===----------------------------------------------------------------------===//
// Constants and Configuration
//===----------------------------------------------------------------------===//
namespace {
/// Configuration constants
} // namespace

//===----------------------------------------------------------------------===//
// Conversion Patterns
//===----------------------------------------------------------------------===//

/// Base class for ARTS to LLVM conversion patterns with shared ArtsCodegen
template <typename OpType>
class ArtsToLLVMPattern : public OpRewritePattern<OpType> {
protected:
  ArtsCodegen *AC;

public:
  ArtsToLLVMPattern(MLIRContext *context, ArtsCodegen *ac)
      : OpRewritePattern<OpType>(context), AC(ac) {}

  /// Helper to compute linear index from multi-dimensional indices
  Value computeLinearIndex(ArrayRef<Value> sizes, ArrayRef<Value> indices,
                           Location loc) const {
    /// Convert multi-dimensional indices to linear index for 1D memref
    /// Formula: linear_index = i0 * (size1 * size2 * ... * sizeN) +
    ///                        i1 * (size2 * size3 * ... * sizeN) +
    ///                        ...
    ///                        iN
    if (indices.size() <= 1)
      return indices.empty() ? AC->createIndexConstant(0, loc) : indices[0];

    Value linearIndex = AC->createIndexConstant(0, loc);
    for (size_t i = 0; i < indices.size(); ++i) {
      /// Compute the stride for this dimension
      Value stride = AC->createIndexConstant(1, loc);
      for (size_t j = i + 1; j < sizes.size(); ++j)
        stride = AC->create<arith::MulIOp>(loc, stride, sizes[j]);

      /// Add contribution from this dimension: indices[i] * stride
      auto contribution = AC->create<arith::MulIOp>(loc, indices[i], stride);
      linearIndex = AC->create<arith::AddIOp>(loc, linearIndex, contribution);
    }

    return linearIndex;
  }
};

/// Pattern to convert arts.get_total_workers operations
struct GetTotalWorkersPattern : public ArtsToLLVMPattern<GetTotalWorkersOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(GetTotalWorkersOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering GetTotalWorkers Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto result = AC->getTotalWorkers(op.getLoc());
    rewriter.replaceOp(op, result);
    return success();
  }
};

/// Pattern to convert arts.get_total_nodes operations
struct GetTotalNodesPattern : public ArtsToLLVMPattern<GetTotalNodesOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(GetTotalNodesOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering GetTotalNodes Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto result = AC->getTotalNodes(op.getLoc());
    rewriter.replaceOp(op, result);
    return success();
  }
};

/// Pattern to convert arts.get_current_worker operations
struct GetCurrentWorkerPattern : public ArtsToLLVMPattern<GetCurrentWorkerOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(GetCurrentWorkerOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering GetCurrentWorker Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto result = AC->getCurrentWorker(op.getLoc());
    rewriter.replaceOp(op, result);
    return success();
  }
};

/// Pattern to convert arts.get_current_node operations
struct GetCurrentNodePattern : public ArtsToLLVMPattern<GetCurrentNodeOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(GetCurrentNodeOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering GetCurrentNode Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto result = AC->getCurrentNode(op.getLoc());
    rewriter.replaceOp(op, result);
    return success();
  }
};

//===----------------------------------------------------------------------===//
// Event and Barrier Patterns
//===----------------------------------------------------------------------===//

/// Pattern to convert arts.event operations
struct EventPattern : public ArtsToLLVMPattern<EventOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(EventOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering Event Op " << op);
    // ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    // auto result = AC->getCurrentEdtGuid(op.getLoc());
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to convert arts.barrier operations
struct BarrierPattern : public ArtsToLLVMPattern<BarrierOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(BarrierOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering Barrier Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    AC->createRuntimeCall(types::ARTSRTL_artsYield, {}, op.getLoc());
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to convert arts.record_in_dep operations
struct RecordInDepPattern : public ArtsToLLVMPattern<RecordInDepOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(RecordInDepOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering RecordInDep Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto edtGuid = op.getEdtGuid();
    auto loc = op.getLoc();

    /// Create slot allocator (local variable to track current slot)
    auto slotAllocType = MemRefType::get({}, AC->Int32);
    auto slotAlloc = AC->create<memref::AllocaOp>(loc, slotAllocType);
    auto initialSlot = AC->createIntConstant(0, AC->Int32, loc);
    AC->create<memref::StoreOp>(loc, initialSlot, slotAlloc);

    /// Process each db (handle both single and multidimensional)
    for (Value db : op.getDatablocks())
      addDepsForDatablock(db, edtGuid, slotAlloc, loc);

    rewriter.eraseOp(op);
    return success();
  }

private:
  void addDepsForDatablock(Value db, Value edtGuid, Value slotAlloc,
                           Location loc) const {
    auto dbType = db.getType().dyn_cast<MemRefType>();
    if (!dbType) {
      /// Non-MemRef datablocks - handle as single element
      addSingleDep(db, edtGuid, slotAlloc, loc);
      return;
    }

    auto rank = dbType.getRank();
    if (rank <= 1) {
      /// Single dimension or scalar - simple case
      addSingleDep(db, edtGuid, slotAlloc, loc);
      return;
    }

    /// Multidimensional db - create nested loops (following original
    /// EdtCodegen)

    /// Get db dimensions
    SmallVector<Value> dbSizes;
    for (int i = 0; i < rank; ++i) {
      auto dimSize = AC->create<memref::DimOp>(loc, db, i);
      dbSizes.push_back(dimSize);
    }

    /// Create offsets (start from 0 for each dimension)
    SmallVector<Value> dbOffsets;
    auto zeroIndex = AC->createIndexConstant(0, loc);
    for (int i = 0; i < rank; ++i)
      dbOffsets.push_back(zeroIndex);

    /// Generate nested loops for multidimensional access
    addDepsForMultiDb(db, edtGuid, slotAlloc, dbSizes, dbOffsets, loc);
  }

  void addSingleDep(Value db, Value edtGuid, Value slotAlloc,
                    Location loc) const {
    /// Check if db is from DbAcquireOp (which returns tuple)
    /// or from other sources (which return single value)
    Value dbGuid;
    if (auto acquireOp = db.getDefiningOp<DbAcquireOp>()) {
      /// db is from DbAcquireOp, use the GUID result
      dbGuid = acquireOp.getGuid();
    } else {
      /// db is from other sources, get GUID using artsDbGetGuid
      auto guidCall =
          AC->createRuntimeCall(types::ARTSRTL_artsDbGetGuid, {db}, loc);
      dbGuid = guidCall.getResult(0);
    }

    /// Load current slot, add dependency, increment slot
    auto currentSlot = AC->create<memref::LoadOp>(loc, slotAlloc);
    AC->createRuntimeCall(types::ARTSRTL_artsDbAddDependence,
                          {dbGuid, edtGuid, currentSlot}, loc);

    auto incrementConstant = AC->createIntConstant(1, AC->Int32, loc);
    auto nextSlot =
        AC->create<arith::AddIOp>(loc, currentSlot, incrementConstant);
    AC->create<memref::StoreOp>(loc, nextSlot, slotAlloc);
  }

  void addDepsForMultiDb(Value db, Value edtGuid, Value slotAlloc,
                         const SmallVector<Value> &dbSizes,
                         const SmallVector<Value> &dbOffsets,
                         Location loc) const {
    const unsigned dbRank = dbSizes.size();
    auto stepConstant = AC->createIndexConstant(1, loc);
    auto incrementConstant = AC->createIntConstant(1, AC->Int32, loc);

    std::function<void(unsigned, SmallVector<Value, 4> &)>
        addDependenciesRecursive = [&](unsigned dim,
                                       SmallVector<Value, 4> &indices) {
          if (dim == dbRank) {
            /// Base case: add dependency for this specific element
            auto currentSlot = AC->create<memref::LoadOp>(loc, slotAlloc);

            /// Load the specific db element using linear index
            SmallVector<Value> dbSizesVec(dbSizes);
            auto linearIndex = computeLinearIndex(dbSizesVec, indices, loc);
            auto loadedDbElement =
                AC->create<memref::LoadOp>(loc, db, linearIndex);

            /// Get GUID from the loaded element (could be from DbAcquireOp or
            /// other sources)
            Value elementGuid;
            if (auto acquireOp = db.getDefiningOp<arts::DbAcquireOp>()) {
              elementGuid = acquireOp.getGuid();
            } else {
              auto guidCall = AC->createRuntimeCall(
                  types::ARTSRTL_artsDbGetGuid, {loadedDbElement}, loc);
              elementGuid = guidCall.getResult(0);
            }

            AC->createRuntimeCall(types::ARTSRTL_artsDbAddDependence,
                                  {elementGuid, edtGuid, currentSlot}, loc);

            /// Increment slot for next element
            auto nextSlot =
                AC->create<arith::AddIOp>(loc, currentSlot, incrementConstant);
            AC->create<memref::StoreOp>(loc, nextSlot, slotAlloc);
            return;
          }

          /// Recursive case: create loop for current dimension
          auto lowerBound = dbOffsets[dim];
          auto upperBound =
              AC->create<arith::AddIOp>(loc, lowerBound, dbSizes[dim]);
          auto loopOp =
              AC->create<scf::ForOp>(loc, lowerBound, upperBound, stepConstant);

          auto &loopBlock = loopOp.getRegion().front();
          AC->setInsertionPointToStart(&loopBlock);
          indices.push_back(loopOp.getInductionVar());
          addDependenciesRecursive(dim + 1, indices);
          indices.pop_back();
          AC->setInsertionPointAfter(loopOp);
        };

    SmallVector<Value, 4> initIndices;
    addDependenciesRecursive(0, initIndices);
  }
};

/// Pattern to convert arts.increment_out_latch operations
struct IncrementOutLatchPattern
    : public ArtsToLLVMPattern<IncrementOutLatchOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(IncrementOutLatchOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering IncrementOutLatch Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto loc = op.getLoc();

    /// Create slot allocator for tracking slot counter (needed for
    /// multidimensional)
    auto slotAllocType = MemRefType::get({}, AC->Int32);
    auto slotAlloc = AC->create<memref::AllocaOp>(loc, slotAllocType);
    auto initialSlot = AC->createIntConstant(0, AC->Int32, loc);
    AC->create<memref::StoreOp>(loc, initialSlot, slotAlloc);

    /// Process each db (handle both single and multidimensional)
    for (Value db : op.getDatablocks())
      incrementLatchForDatablock(rewriter, db, slotAlloc, loc);

    rewriter.eraseOp(op);
    return success();
  }

private:
  void incrementLatchForDatablock(PatternRewriter &rewriter, Value db,
                                  Value slotAlloc, Location loc) const {
    auto dbType = db.getType().dyn_cast<MemRefType>();
    if (!dbType) {
      /// Non-MemRef datablocks - handle as single element
      incrementSingleLatch(db, slotAlloc, loc);
      return;
    }

    auto rank = dbType.getRank();
    if (rank <= 1) {
      /// Single dimension or scalar - simple case
      incrementSingleLatch(db, slotAlloc, loc);
      return;
    }

    /// Multidimensional db - create nested loops (following original
    /// EdtCodegen)

    /// Get db dimensions
    SmallVector<Value> dbSizes;
    for (int i = 0; i < rank; ++i) {
      auto dimSize = AC->create<memref::DimOp>(loc, db, i);
      dbSizes.push_back(dimSize);
    }

    /// Create offsets (start from 0 for each dimension)
    SmallVector<Value> dbOffsets;
    auto zeroIndex = AC->createIndexConstant(0, loc);
    for (int i = 0; i < rank; ++i)
      dbOffsets.push_back(zeroIndex);

    /// Generate nested loops for multidimensional latch increment
    incrementLatchCountsForMultiDb(db, slotAlloc, dbSizes, dbOffsets, loc);
  }

  void incrementSingleLatch(Value db, Value slotAlloc, Location loc) const {
    /// Get GUID from db (could be from DbAcquireOp or other sources)
    Value dbGuid;
    if (auto acquireOp = db.getDefiningOp<DbAcquireOp>()) {
      /// db is from DbAcquireOp, use the GUID result
      dbGuid = acquireOp.getGuid();
    } else {
      /// db is from other sources, get GUID using artsDbGetGuid
      auto guidCall =
          AC->createRuntimeCall(types::ARTSRTL_artsDbGetGuid, {db}, loc);
      dbGuid = guidCall.getResult(0);
    }

    /// Simple case: just increment the latch for single db
    AC->createRuntimeCall(types::ARTSRTL_artsDbIncrementLatch, {dbGuid}, loc);

    /// Update slot counter (for consistency with multidimensional case)
    auto currentSlot = AC->create<memref::LoadOp>(loc, slotAlloc);
    auto incrementConstant = AC->createIntConstant(1, AC->Int32, loc);
    auto nextSlot =
        AC->create<arith::AddIOp>(loc, currentSlot, incrementConstant);
    AC->create<memref::StoreOp>(loc, nextSlot, slotAlloc);
  }

  void incrementLatchCountsForMultiDb(Value db, Value slotAlloc,
                                      const SmallVector<Value> &dbSizes,
                                      const SmallVector<Value> &dbOffsets,
                                      Location loc) const {
    const unsigned dbRank = dbSizes.size();
    auto stepConstant = AC->createIndexConstant(1, loc);
    auto incrementConstant = AC->createIntConstant(1, AC->Int32, loc);

    std::function<void(unsigned, SmallVector<Value, 4> &)>
        incrementLatchRecursive = [&](unsigned dim,
                                      SmallVector<Value, 4> &indices) {
          if (dim == dbRank) {
            /// Base case: increment latch for this specific element
            SmallVector<Value> dbSizesVec(dbSizes);
            auto linearIndex = computeLinearIndex(dbSizesVec, indices, loc);
            auto loadedDbElement =
                AC->create<memref::LoadOp>(loc, db, linearIndex);

            /// Get GUID from the loaded element (could be from DbAcquireOp or
            /// other sources)
            Value elementGuid;
            if (auto acquireOp = db.getDefiningOp<arts::DbAcquireOp>()) {
              elementGuid = acquireOp.getGuid();
            } else {
              auto guidCall = AC->createRuntimeCall(
                  types::ARTSRTL_artsDbGetGuid, {loadedDbElement}, loc);
              elementGuid = guidCall.getResult(0);
            }

            AC->createRuntimeCall(types::ARTSRTL_artsDbIncrementLatch,
                                  {elementGuid}, loc);

            /// Update slot counter
            auto currentSlot = AC->create<memref::LoadOp>(loc, slotAlloc);
            auto nextSlot =
                AC->create<arith::AddIOp>(loc, currentSlot, incrementConstant);
            AC->create<memref::StoreOp>(loc, nextSlot, slotAlloc);
            return;
          }

          /// Recursive case: create loop for current dimension
          auto lowerBound = dbOffsets[dim];
          auto upperBound =
              AC->create<arith::AddIOp>(loc, lowerBound, dbSizes[dim]);
          auto loopOp =
              AC->create<scf::ForOp>(loc, lowerBound, upperBound, stepConstant);

          auto &loopBlock = loopOp.getRegion().front();
          AC->setInsertionPointToStart(&loopBlock);
          indices.push_back(loopOp.getInductionVar());
          incrementLatchRecursive(dim + 1, indices);
          indices.pop_back();
          AC->setInsertionPointAfter(loopOp);
        };

    SmallVector<Value, 4> initIndices;
    incrementLatchRecursive(0, initIndices);
  }
};

//===----------------------------------------------------------------------===//
// EDT  Patterns
//===----------------------------------------------------------------------===//

/// Pattern to convert arts.edt_param_pack operations
struct EdtParamPackPattern : public ArtsToLLVMPattern<EdtParamPackOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(EdtParamPackOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering EdtParamPack Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);

    auto loc = op.getLoc();
    auto params = op.getParams();
    auto resultType = op.getMemref().getType().dyn_cast<MemRefType>();
    if (!resultType)
      return op.emitError("Expected MemRef type for result");

    // Check if result type has dynamic dimensions
    bool hasDynamicDims = resultType.getNumDynamicDims() > 0;

    memref::AllocOp allocOp;
    if (hasDynamicDims) {
      /// Dynamic memref: allocate with size and store parameters
      auto numParams = AC->createIndexConstant(params.size(), loc);
      allocOp =
          AC->create<memref::AllocOp>(loc, resultType, ValueRange{numParams});

      for (unsigned i = 0; i < params.size(); ++i) {
        auto index = AC->createIndexConstant(i, loc);
        auto castParam = AC->castParameter(AC->Int64, params[i], loc);
        AC->create<memref::StoreOp>(loc, castParam, allocOp, ValueRange{index});
      }
    } else {
      /// Empty parameter pack: allocate dynamic memref<?xi64> with size 0
      auto dynamicType = MemRefType::get({ShapedType::kDynamic}, AC->Int64);
      auto zeroIndex = AC->createIndexConstant(0, loc);
      allocOp =
          AC->create<memref::AllocOp>(loc, dynamicType, ValueRange{zeroIndex});
    }

    rewriter.replaceOp(op, allocOp);
    return success();
  }
};

/// Pattern to convert arts.edt_param_unpack operations
struct EdtParamUnpackPattern : public ArtsToLLVMPattern<EdtParamUnpackOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(EdtParamUnpackOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering EdtParamUnpack Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto paramMemref = op.getMemref();
    auto results = op.getUnpacked();

    SmallVector<Value> newResults;
    for (unsigned i = 0; i < results.size(); ++i) {
      auto idx = AC->createIndexConstant(i, op.getLoc());
      auto loadedParam =
          AC->create<memref::LoadOp>(op.getLoc(), paramMemref, ValueRange{idx});
      auto castedParam =
          AC->castParameter(results[i].getType(), loadedParam, op.getLoc());
      newResults.push_back(castedParam);
    }

    rewriter.replaceOp(op, newResults);
    return success();
  }
};

/// Pattern to convert arts.edt_dep_unpack operations
struct EdtDepUnpackPattern : public ArtsToLLVMPattern<EdtDepUnpackOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(EdtDepUnpackOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering EdtDepUnpack Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto depMemref = op.getMemref();
    auto results = op.getUnpacked();
    auto loc = op.getLoc();

    /// Get the size of the ArtsEdtDep struct
    auto depStructSize = AC->create<polygeist::TypeSizeOp>(
        loc, AC->getBuilder().getIndexType(), AC->ArtsEdtDep);

    /// Convert depMemref to LLVM pointer for GEP operations
    auto depMemrefPtr = AC->castToLLVMPtr(depMemref, loc);

    SmallVector<Value> newResults;
    for (unsigned i = 0; i < results.size(); ++i) {
      /// Calculate memory offset: i * depStructSize
      auto idx = AC->createIndexConstant(i, loc);
      auto offset = AC->create<arith::MulIOp>(loc, idx, depStructSize);
      auto intOffset = AC->castToInt(AC->PtrSize, offset, loc);

      /// Use GEP to access the dependency struct
      auto depStructPtr = AC->create<LLVM::GEPOp>(
          loc, AC->llvmPtr, AC->PtrSize, depMemrefPtr, ValueRange{intOffset});

      /// Load the struct from the pointer
      auto resultType = results[i].getType();
      auto depStruct = AC->create<LLVM::LoadOp>(loc, resultType, depStructPtr);

      newResults.push_back(depStruct);
    }

    rewriter.replaceOp(op, newResults);
    return success();
  }
};

/// Pattern to convert arts.dep_get_ptr operations
struct DepGetPtrOpPattern : public ArtsToLLVMPattern<DepGetPtrOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DepGetPtrOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering DepGetPtr Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto depStruct = op.getDepStruct();
    auto loc = op.getLoc();

    /// Extract the actual pointer from the EDT dependency structure
    auto actualPtr = AC->getPtrFromEdtDep(depStruct, loc);

    /// Convert the raw pointer to a memref using polygeist helper
    auto resultType = op.getPtr().getType();
    auto memrefResult =
        AC->create<polygeist::Pointer2MemrefOp>(loc, resultType, actualPtr);

    rewriter.replaceOp(op, memrefResult);
    return success();
  }
};

/// Pattern to convert arts.dep_get_guid operations
struct DepGetGuidOpPattern : public ArtsToLLVMPattern<DepGetGuidOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DepGetGuidOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering DepGetGuid Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto depStruct = op.getDepStruct();
    auto loc = op.getLoc();

    /// Extract the GUID from the EDT dependency structure
    auto guid = AC->getGuidFromEdtDep(depStruct, loc);
    rewriter.replaceOp(op, guid);
    return success();
  }
};

/// Pattern to convert arts.edt_create operations (key EDT creation)
struct EdtCreatePattern : public ArtsToLLVMPattern<EdtCreateOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(EdtCreateOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering EdtCreate Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    /// Get outlined function name
    auto funcNameAttr = op->getAttrOfType<StringAttr>("outlined_func");
    if (!funcNameAttr)
      return op.emitError("Missing outlined_func attribute");

    auto outlined =
        AC->getModule().lookupSymbol<func::FuncOp>(funcNameAttr.getValue());
    if (!outlined)
      return op.emitError("Outlined function not found");

    /// Build artsEdtCreate arguments; route required, default 0 if missing
    auto funcPtr = AC->createFnPtr(outlined, op.getLoc());
    Value route = op.getRoute();
    if (!route)
      route = AC->createIntConstant(0, AC->Int32, op.getLoc());
    auto paramv = op.getParamMemref();
    auto depc = op.getDepCount();

    /// Calculate parameter count from memref size
    Value paramc;
    if (auto memrefType = paramv.getType().dyn_cast<MemRefType>()) {
      if (memrefType.hasStaticShape() && memrefType.getNumElements() == 0) {
        /// Empty static memref case
        paramc = AC->createIntConstant(0, AC->Int32, op.getLoc());
      } else {
        /// Dynamic memref case - get size from memref
        auto zeroIndex = AC->createIndexConstant(0, op.getLoc());
        auto memrefSize =
            AC->create<memref::DimOp>(op.getLoc(), paramv, zeroIndex);
        paramc =
            AC->create<arith::IndexCastOp>(op.getLoc(), AC->Int32, memrefSize);
      }
    } else {
      paramc = AC->createIntConstant(0, AC->Int32, op.getLoc());
    }

    /// Create artsEdtCreate call
    auto call = AC->createRuntimeCall(types::ARTSRTL_artsEdtCreate,
                                      {funcPtr, route, paramc, paramv, depc},
                                      op.getLoc());
    rewriter.replaceOp(op, call.getResult(0));
    return success();
  }
};

//===----------------------------------------------------------------------===//
// DB Patterns
//===----------------------------------------------------------------------===//

/// Pattern to convert arts.db_alloc operations
struct DbAllocPattern : public ArtsToLLVMPattern<DbAllocOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbAllocOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering DbAlloc Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto loc = op.getLoc();
    SmallVector<Value> payloadSizes;
    if (Value addr = op.getAddress()) {
      if (auto mt = addr.getType().dyn_cast<MemRefType>()) {
        for (int i = 0; i < mt.getRank(); ++i) {
          if (mt.isDynamicDim(i))
            payloadSizes.push_back(AC->create<memref::DimOp>(loc, addr, i));
          else
            payloadSizes.push_back(
                AC->createIndexConstant(mt.getDimSize(i), loc));
        }
      }
    }

    /// Create scalar GUID memref<i64>
    auto guidType = MemRefType::get({}, AC->Int64);
    auto guidMemref = AC->create<memref::AllocOp>(loc, guidType);

    /// Compute payload bytes = product(payloadSizes) * sizeof(elementType)
    Value numElems = AC->computeTotalElements(payloadSizes, loc);
    Value elemSize = AC->computeElementTypeSize(op.getElementType(), loc);
    Value bytesIdx = AC->create<arith::MulIOp>(loc, numElems, elemSize);
    Value bytesI64 = AC->castToInt(AC->Int64, bytesIdx, loc);

    /// Reserve GUID and create datablock
    auto dbModeValue = static_cast<int>(op.getDbModeAttr().getValue());
    auto dbMode = AC->createIntConstant(dbModeValue, AC->Int32, loc);
    Value route = op.getRoute();
    if (!route)
      route = AC->createIntConstant(0, AC->Int32, loc);
    auto guidCall = AC->createRuntimeCall(types::ARTSRTL_artsReserveGuidRoute,
                                          {dbMode, route}, loc);
    Value guid = guidCall.getResult(0);
    auto dbCall = AC->createRuntimeCall(types::ARTSRTL_artsDbCreateWithGuid,
                                        {guid, bytesI64}, loc);
    Value dbVoid = dbCall.getResult(0);

    /// Store GUID
    AC->create<memref::StoreOp>(loc, guid, guidMemref);

    /// Cast void* -> original ptr memref type
    auto llvmPtr = AC->castToLLVMPtr(dbVoid, loc);
    auto resultPtrTy = op.getPtr().getType();
    auto typedMemref =
        AC->create<polygeist::Pointer2MemrefOp>(loc, resultPtrTy, llvmPtr);

    rewriter.replaceOp(op, {guidMemref, typedMemref});
    return success();
  }

private:
  void createSingleDb(DbAllocOp op, Value dbMemref, Value guidMemref,
                      ArrayRef<Value> indices = {}) const {
    auto loc = op.getLoc();

    /// Get DB mode from operation attribute
    auto dbModeValue = static_cast<int>(op.getDbModeAttr().getValue());
    auto dbMode = AC->createIntConstant(dbModeValue, AC->Int32, loc);

    /// Reserve GUID for the DB
    Value currentNode = op.getRoute();
    auto guidCall = AC->createRuntimeCall(types::ARTSRTL_artsReserveGuidRoute,
                                          {dbMode, currentNode}, loc);
    auto guid = guidCall.getResult(0);

    /// Create the DB with GUID
    auto elementSize = AC->computeElementTypeSize(op.getElementType(), loc);
    auto dbCall = AC->createRuntimeCall(types::ARTSRTL_artsDbCreateWithGuid,
                                        {guid, elementSize}, loc);
    auto dbPtr = dbCall.getResult(0);

    /// Store the DB pointer and GUID in the memref
    if (indices.empty()) {
      /// Single element case - dbMemref is 0-dimensional, containing the dbPtr
      AC->create<memref::StoreOp>(loc, dbPtr, dbMemref);
      AC->create<memref::StoreOp>(loc, guid, guidMemref);
    } else {
      /// Multi-dimensional case - need different indexing for each memref
      /// GUID memref keeps original multi-dimensional shape
      AC->create<memref::StoreOp>(loc, guid, guidMemref, indices);

      /// DB memref is linearized - compute linear index from multi-dimensional
      /// indices
      SmallVector<Value> sizesVec(op.getSizes());
      auto linearIndex = computeLinearIndex(sizesVec, indices, loc);
      AC->create<memref::StoreOp>(loc, dbPtr, dbMemref, linearIndex);
    }
  }

  void createMultiDimDbs(DbAllocOp op, Value dbMemref, Value guidMemref,
                         ArrayRef<Value> sizes, Location loc) const {
    const unsigned rank = sizes.size();
    auto stepConstant = AC->createIndexConstant(1, loc);

    std::function<void(unsigned, SmallVector<Value, 4> &)> createDbsRecursive =
        [&](unsigned dim, SmallVector<Value, 4> &indices) {
          if (dim == rank) {
            createSingleDb(op, dbMemref, guidMemref, indices);
            return;
          }

          /// Create loop for current dimension
          auto lowerBound = AC->createIndexConstant(0, loc);
          auto upperBound = sizes[dim];
          auto loopOp =
              AC->create<scf::ForOp>(loc, lowerBound, upperBound, stepConstant);

          auto &loopBlock = loopOp.getRegion().front();
          AC->setInsertionPointToStart(&loopBlock);
          indices.push_back(loopOp.getInductionVar());
          createDbsRecursive(dim + 1, indices);
          indices.pop_back();
          AC->setInsertionPointAfter(loopOp);
        };

    SmallVector<Value, 4> initIndices;
    createDbsRecursive(0, initIndices);
  }
};

/// Pattern to convert arts.db_acquire operations
struct DbAcquirePattern : public ArtsToLLVMPattern<DbAcquireOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbAcquireOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering DbAcquire Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto source = op.getSource();
    auto indices = op.getIndices();
    auto loc = op.getLoc();

    Value ptrResult;
    if (indices.empty()) {
      /// Single element case - source is already the pointer
      ptrResult = source;
    } else {
      /// Multidimensional case - load pointer from indexed position
      ptrResult = AC->create<memref::LoadOp>(loc, source, indices);
    }

    /// Get GUID from the db pointer using artsDbGetGuid
    auto guidCall =
        AC->createRuntimeCall(types::ARTSRTL_artsDbGetGuid, {ptrResult}, loc);
    auto guidResult = guidCall.getResult(0);

    /// Return both GUID and pointer as a tuple
    SmallVector<Value> results = {guidResult, ptrResult};
    rewriter.replaceOp(op, results);
    return success();
  }
};

/// Pattern to convert arts.db_release operations
struct DbReleasePattern : public ArtsToLLVMPattern<DbReleaseOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbReleaseOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering DbRelease Op " << op);
    /// Simply erase the DbReleaseOp
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to convert arts.db_free operations
struct DbFreePattern : public ArtsToLLVMPattern<DbFreeOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbFreeOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering DbFree Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    AC->create<memref::DeallocOp>(op.getLoc(), op.getSource());
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to convert arts.db_control operations
struct DbControlPattern : public ArtsToLLVMPattern<DbControlOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbControlOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering DbControl Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    /// Control DBs are not supported yet
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to convert arts.db_num_elements operations
struct DbNumElementsPattern : public ArtsToLLVMPattern<DbNumElementsOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbNumElementsOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering DbNumElements Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto loc = op.getLoc();

    /// Get the sizes from the operation arguments
    SmallVector<Value> sizes = op.getSizes();

    /// If no sizes are provided, return constant 1 (i32)
    if (sizes.empty()) {
      rewriter.replaceOp(op, AC->createIntConstant(1, AC->Int32, loc));
      return success();
    }

    /// If all sizes are constants, fold to a single i32 constant
    bool allConst = true;
    int64_t folded = 1;
    for (Value sz : sizes) {
      if (auto cst = dyn_cast_or_null<arith::ConstantOp>(sz.getDefiningOp())) {
        if (auto intAttr = cst.getValue().dyn_cast<IntegerAttr>()) {
          folded *= intAttr.getInt();
          continue;
        }
      }
      allConst = false;
      break;
    }
    if (allConst) {
      rewriter.replaceOp(op, AC->createIntConstant(folded, AC->Int32, loc));
      return success();
    }

    /// Otherwise, compute product at runtime, and store into a scalar
    /// memref<i32>
    auto scalarI32 = MemRefType::get({}, AC->Int32);
    auto allocOp = AC->create<memref::AllocOp>(loc, scalarI32);

    Value productVal = AC->createIntConstant(1, AC->Int32, loc);
    for (Value sz : sizes) {
      Value szI32 = sz;
      if (sz.getType().isa<IndexType>())
        szI32 = AC->create<arith::IndexCastOp>(loc, AC->Int32, sz);
      else if (!sz.getType().isInteger(32))
        szI32 = AC->create<arith::TruncIOp>(loc, AC->Int32, sz);
      productVal = AC->create<arith::MulIOp>(loc, productVal, szI32);
    }

    AC->create<memref::StoreOp>(loc, productVal, allocOp);
    rewriter.replaceOp(op, allocOp.getResult());
    return success();
  }
};

/// Pattern to convert arts.db_subindex operations to polygeist subindex
struct DbSubIndexOpPattern : public ArtsToLLVMPattern<DbSubIndexOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbSubIndexOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering DbSubIndex Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto source = op.getSource();
    auto indices = op.getIndices();
    auto loc = op.getLoc();

    /// Chain polygeist::SubIndexOp operations for multidimensional access
    /// Each polygeist::SubIndexOp handles one dimension
    Value current = source;

    for (size_t i = 0; i < indices.size(); ++i) {
      auto currentType = current.getType().dyn_cast<MemRefType>();
      if (!currentType)
        return failure();
      auto resultType = MemRefType::get(
          currentType.getShape().drop_front(1), /// Drop one dimension
          currentType.getElementType(), currentType.getLayout(),
          currentType.getMemorySpace());

      current = AC->create<polygeist::SubIndexOp>(loc, resultType, current,
                                                  indices[i]);
    }

    rewriter.replaceOp(op, current);
    return success();
  }
};

/// Pattern to convert arts.alloc operations (ARTS memory allocation)
struct AllocPattern : public ArtsToLLVMPattern<AllocOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(AllocOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering Alloc Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto resultType = op.getResult().getType().dyn_cast<MemRefType>();
    if (!resultType)
      return op.emitError("Expected MemRef type for result");
    return success();
  }
};

//===----------------------------------------------------------------------===//
// Epoch Patterns
//===----------------------------------------------------------------------===//

/// Pattern to convert arts.epoch operations
struct EpochPattern : public ArtsToLLVMPattern<EpochOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(EpochOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering Epoch Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    /// Create a done-edt for the epoch
    auto guid = AC->getCurrentEdtGuid(op.getLoc());
    auto edtSlot =
        AC->createIntConstant(DEFAULT_EDT_SLOT, AC->Int32, op.getLoc());
    auto currentEpoch = AC->createEpoch(guid, edtSlot, op.getLoc());

    /// Move operations out of epoch region to before the epoch
    auto &epochRegion = op.getRegion();
    if (!epochRegion.empty()) {
      auto &epochBlock = epochRegion.front();
      SmallVector<Operation *> opsToMove;
      for (auto &innerOp : epochBlock.without_terminator())
        opsToMove.push_back(&innerOp);

      for (auto *opToMove : opsToMove)
        opToMove->moveBefore(op);
    }

    /// Wait on the epoch
    AC->setInsertionPointAfter(op);
    AC->waitOnHandle(currentEpoch, op.getLoc());
    rewriter.eraseOp(op);
    return success();
  }
};

//===----------------------------------------------------------------------===//
// Terminator Patterns
//===----------------------------------------------------------------------===//

/// Pattern to convert arts.yield operations (terminator)
struct YieldPattern : public ArtsToLLVMPattern<YieldOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(YieldOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering Yield Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    /// arts.yield is a terminator that should be erased during conversion
    rewriter.eraseOp(op);
    return success();
  }
};

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct ConvertArtsToLLVMPass
    : public arts::ConvertArtsToLLVMBase<ConvertArtsToLLVMPass> {

  explicit ConvertArtsToLLVMPass(bool debug = false) : debugMode(debug) {}

  void runOnOperation() override;

private:
  LogicalResult initializeCodegen();
  void populatePatterns(RewritePatternSet &patterns);

  //// Member variables
  ArtsCodegen *AC = nullptr;
  bool debugMode = false;
};
} // namespace

//===----------------------------------------------------------------------===//
// Core Pass Implementation
//===----------------------------------------------------------------------===//

void ConvertArtsToLLVMPass::runOnOperation() {
  ModuleOp module = getOperation();
  MLIRContext *context = &getContext();

  ARTS_DEBUG_HEADER(ConvertArtsToLLVMPass);
  ARTS_DEBUG_REGION(module.dump(););

  //// Initialize codegen infrastructure
  if (failed(initializeCodegen())) {
    ARTS_ERROR("Failed to initialize ArtsCodegen");
    return signalPassFailure();
  }

  //// Create and populate rewrite patterns
  RewritePatternSet patterns(context);
  populatePatterns(patterns);

  //// Apply patterns with greedy rewriter
  GreedyRewriteConfig config;
  config.useTopDownTraversal = true;
  // Disable to avoid issues with partial conversion
  config.enableRegionSimplification = false;

  if (failed(
          applyPatternsAndFoldGreedily(module, std::move(patterns), config))) {
    ARTS_ERROR("Failed to apply ARTS to LLVM conversion patterns");
    delete AC;
    return signalPassFailure();
  }

  //// Initialize runtime
  AC->initRT(AC->getUnknownLoc());

  //// Cleanup
  delete AC;
  AC = nullptr;

  ARTS_DEBUG_FOOTER(ConvertArtsToLLVMPass);
  ARTS_DEBUG_REGION(module.dump(););
}

LogicalResult ConvertArtsToLLVMPass::initializeCodegen() {
  ModuleOp module = getOperation();
  AC = new ArtsCodegen(module, debugMode);

  ARTS_DEBUG_TYPE("ArtsCodegen initialized successfully");
  return success();
}

void ConvertArtsToLLVMPass::populatePatterns(RewritePatternSet &patterns) {
  MLIRContext *context = patterns.getContext();

  /// Runtime helper patterns
  patterns.add<GetTotalWorkersPattern, GetTotalNodesPattern,
               GetCurrentWorkerPattern, GetCurrentNodePattern>(context, AC);

  /// Event and barrier patterns
  // patterns.add<EventPattern, BarrierPattern>(context, AC);
  // patterns.add<RecordInDepPattern, IncrementOutLatchPattern>(context, AC);

  /// EDT patterns
  patterns.add<EdtParamPackPattern, EdtParamUnpackPattern>(context, AC);
  patterns.add<EdtCreatePattern>(context, AC);

  /// Dependency patterns
  patterns.add<EdtDepUnpackPattern>(context, AC);
  patterns.add<DepGetPtrOpPattern, DepGetGuidOpPattern>(context, AC);
  //              DepGetPtrOpPattern, DepGetGuidOpPattern, EdtCreatePattern>(
  //     context, AC);

  /// DB patterns
  patterns.add<DbAllocPattern, DbNumElementsPattern>(context, AC);
  patterns.add<DbFreePattern, DbReleasePattern, DbControlPattern>(context, AC);
  // patterns.add<AllocPattern>(context, AC);
  // patterns.add<DbAcquirePattern, DbReleasePattern, DbFreePattern,
  //              DbControlPattern, DbSubIndexOpPattern, AllocPattern>(context,
  //                                                                   AC);

  /// Epoch patterns
  // patterns.add<EpochPattern>(context, AC);

  /// Terminator patterns
  // patterns.add<YieldPattern>(context, AC);
}

//===----------------------------------------------------------------------===//
// Pass Functions
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createConvertArtsToLLVMPass() {
  return std::make_unique<ConvertArtsToLLVMPass>();
}

std::unique_ptr<Pass> createConvertArtsToLLVMPass(bool debug) {
  return std::make_unique<ConvertArtsToLLVMPass>(debug);
}
} // namespace arts
} // namespace mlir
