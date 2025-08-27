///==========================================================================
// File: ConvertArtsToLLVM.cpp
//
// This file implements a pass to convert ARTS dialect operations into
// LLVM dialect operations. Since preprocessing is handled by a separate
// pass, this pass focuses purely on ARTS-specific conversion logic.
///==========================================================================

/// Dialects
#include "mlir/Dialect/Func/IR/FuncOps.h"
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
};

/// Pattern to convert arts.get_total_workers operations
struct GetTotalWorkersPattern : public ArtsToLLVMPattern<GetTotalWorkersOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(GetTotalWorkersOp op,
                                PatternRewriter &rewriter) const override {
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
    auto result = AC->getCurrentEdtGuid(op.getLoc());
    rewriter.replaceOp(op, result);
    return success();
  }
};

/// Pattern to convert arts.barrier operations
struct BarrierPattern : public ArtsToLLVMPattern<BarrierOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(BarrierOp op,
                                PatternRewriter &rewriter) const override {

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

    auto edtGuid = op.getEdtGuid();
    auto loc = op.getLoc();

    /// Create slot allocator (local variable to track current slot)
    auto slotAllocType = MemRefType::get({}, AC->Int32);
    auto slotAlloc = rewriter.create<memref::AllocaOp>(loc, slotAllocType);
    auto initialSlot = AC->createIntConstant(0, AC->Int32, loc);
    rewriter.create<memref::StoreOp>(loc, initialSlot, slotAlloc);

    /// Process each datablock (handle both single and multidimensional)
    for (Value datablock : op.getDatablocks()) {
      addDepsForDatablock(rewriter, datablock, edtGuid, slotAlloc, loc);
    }

    rewriter.eraseOp(op);
    return success();
  }

private:
  void addDepsForDatablock(PatternRewriter &rewriter, Value datablock,
                           Value edtGuid, Value slotAlloc, Location loc) const {
    auto dbType = datablock.getType().dyn_cast<MemRefType>();
    if (!dbType) {
      /// Non-MemRef datablocks - handle as single element
      addSingleDep(rewriter, datablock, edtGuid, slotAlloc, loc);
      return;
    }

    auto rank = dbType.getRank();
    if (rank <= 1) {
      /// Single dimension or scalar - simple case
      addSingleDep(rewriter, datablock, edtGuid, slotAlloc, loc);
      return;
    }

    /// Multidimensional datablock - create nested loops (following original
    /// EdtCodegen)

    /// Get datablock dimensions
    SmallVector<Value> dbSizes;
    for (int i = 0; i < rank; ++i) {
      auto dimSize = rewriter.create<memref::DimOp>(loc, datablock, i);
      dbSizes.push_back(dimSize);
    }

    /// Create offsets (start from 0 for each dimension)
    SmallVector<Value> dbOffsets;
    auto zeroIndex = AC->createIndexConstant(0, loc);
    for (int i = 0; i < rank; ++i) {
      dbOffsets.push_back(zeroIndex);
    }

    /// Generate nested loops for multidimensional access
    addDepsForMultiDb(rewriter, datablock, edtGuid, slotAlloc, dbSizes,
                      dbOffsets, loc);
  }

  void addSingleDep(PatternRewriter &rewriter, Value datablock, Value edtGuid,
                    Value slotAlloc, Location loc) const {
    /// Check if datablock is from DbAcquireOp (which returns tuple)
    /// or from other sources (which return single value)
    Value dbGuid;
    if (auto acquireOp = datablock.getDefiningOp<DbAcquireOp>()) {
      /// Datablock is from DbAcquireOp, use the GUID result
      dbGuid = acquireOp.getGuid();
    } else {
      /// Datablock is from other sources, get GUID using artsDbGetGuid
      auto guidCall = AC->createRuntimeCall(types::ARTSRTL_artsDbGetGuid,
                                            {datablock}, loc);
      dbGuid = guidCall.getResult(0);
    }

    /// Load current slot, add dependency, increment slot
    auto currentSlot = rewriter.create<memref::LoadOp>(loc, slotAlloc);
    AC->createRuntimeCall(types::ARTSRTL_artsDbAddDependence,
                          {dbGuid, edtGuid, currentSlot}, loc);

    auto incrementConstant = AC->createIntConstant(1, AC->Int32, loc);
    auto nextSlot =
        rewriter.create<arith::AddIOp>(loc, currentSlot, incrementConstant);
    rewriter.create<memref::StoreOp>(loc, nextSlot, slotAlloc);
  }

  void addDepsForMultiDb(PatternRewriter &rewriter, Value datablock, Value edtGuid,
                         Value slotAlloc, const SmallVector<Value> &dbSizes,
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
            auto currentSlot = rewriter.create<memref::LoadOp>(loc, slotAlloc);

            /// Load the specific datablock element using indices
            auto loadedDbElement =
                rewriter.create<memref::LoadOp>(loc, datablock, indices);

            /// Get GUID from the loaded element (could be from DbAcquireOp or other sources)
            Value elementGuid;
            if (auto acquireOp = datablock.getDefiningOp<arts::DbAcquireOp>()) {
              elementGuid = acquireOp.getGuid();
            } else {
              auto guidCall = AC->createRuntimeCall(types::ARTSRTL_artsDbGetGuid,
                                                    {loadedDbElement}, loc);
              elementGuid = guidCall.getResult(0);
            }

            AC->createRuntimeCall(types::ARTSRTL_artsDbAddDependence,
                                  {elementGuid, edtGuid, currentSlot}, loc);

            /// Increment slot for next element
            auto nextSlot = rewriter.create<arith::AddIOp>(loc, currentSlot,
                                                           incrementConstant);
            rewriter.create<memref::StoreOp>(loc, nextSlot, slotAlloc);
            return;
          }

          /// Recursive case: create loop for current dimension
          auto lowerBound = dbOffsets[dim];
          auto upperBound =
              rewriter.create<arith::AddIOp>(loc, lowerBound, dbSizes[dim]);
          auto loopOp = rewriter.create<scf::ForOp>(loc, lowerBound, upperBound,
                                                    stepConstant);

          auto &loopBlock = loopOp.getRegion().front();
          rewriter.setInsertionPointToStart(&loopBlock);
          indices.push_back(loopOp.getInductionVar());
          addDependenciesRecursive(dim + 1, indices);
          indices.pop_back();
          rewriter.setInsertionPointAfter(loopOp);
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

    auto loc = op.getLoc();

    /// Create slot allocator for tracking slot counter (needed for
    /// multidimensional)
    auto slotAllocType = MemRefType::get({}, AC->Int32);
    auto slotAlloc = rewriter.create<memref::AllocaOp>(loc, slotAllocType);
    auto initialSlot = AC->createIntConstant(0, AC->Int32, loc);
    rewriter.create<memref::StoreOp>(loc, initialSlot, slotAlloc);

    /// Process each datablock (handle both single and multidimensional)
    for (Value datablock : op.getDatablocks()) {
      incrementLatchForDatablock(rewriter, datablock, slotAlloc, loc);
    }

    rewriter.eraseOp(op);
    return success();
  }

private:
  void incrementLatchForDatablock(PatternRewriter &rewriter, Value datablock,
                                  Value slotAlloc, Location loc) const {
    auto dbType = datablock.getType().dyn_cast<MemRefType>();
    if (!dbType) {
      /// Non-MemRef datablocks - handle as single element
      incrementSingleLatch(rewriter, datablock, slotAlloc, loc);
      return;
    }

    auto rank = dbType.getRank();
    if (rank <= 1) {
      /// Single dimension or scalar - simple case
      incrementSingleLatch(rewriter, datablock, slotAlloc, loc);
      return;
    }

    /// Multidimensional datablock - create nested loops (following original
    /// EdtCodegen)

    /// Get datablock dimensions
    SmallVector<Value> dbSizes;
    for (int i = 0; i < rank; ++i) {
      auto dimSize = rewriter.create<memref::DimOp>(loc, datablock, i);
      dbSizes.push_back(dimSize);
    }

    /// Create offsets (start from 0 for each dimension)
    SmallVector<Value> dbOffsets;
    auto zeroIndex = AC->createIndexConstant(0, loc);
    for (int i = 0; i < rank; ++i) {
      dbOffsets.push_back(zeroIndex);
    }

    /// Generate nested loops for multidimensional latch increment
    incrementLatchCountsForMultiDb(rewriter, datablock, slotAlloc, dbSizes,
                                   dbOffsets, loc);
  }

  void incrementSingleLatch(PatternRewriter &rewriter, Value datablock,
                            Value slotAlloc, Location loc) const {
    /// Get GUID from datablock (could be from DbAcquireOp or other sources)
    Value dbGuid;
    if (auto acquireOp = datablock.getDefiningOp<DbAcquireOp>()) {
      /// Datablock is from DbAcquireOp, use the GUID result
      dbGuid = acquireOp.getGuid();
    } else {
      /// Datablock is from other sources, get GUID using artsDbGetGuid
      auto guidCall = AC->createRuntimeCall(types::ARTSRTL_artsDbGetGuid,
                                            {datablock}, loc);
      dbGuid = guidCall.getResult(0);
    }

    /// Simple case: just increment the latch for single datablock
    AC->createRuntimeCall(types::ARTSRTL_artsDbIncrementLatch, {dbGuid},
                          loc);

    /// Update slot counter (for consistency with multidimensional case)
    auto currentSlot = rewriter.create<memref::LoadOp>(loc, slotAlloc);
    auto incrementConstant = AC->createIntConstant(1, AC->Int32, loc);
    auto nextSlot =
        rewriter.create<arith::AddIOp>(loc, currentSlot, incrementConstant);
    rewriter.create<memref::StoreOp>(loc, nextSlot, slotAlloc);
  }

  void incrementLatchCountsForMultiDb(PatternRewriter &rewriter, Value datablock,
                                      Value slotAlloc,
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
            auto loadedDbElement =
                rewriter.create<memref::LoadOp>(loc, datablock, indices);

            /// Get GUID from the loaded element (could be from DbAcquireOp or other sources)
            Value elementGuid;
            if (auto acquireOp = datablock.getDefiningOp<arts::DbAcquireOp>()) {
              elementGuid = acquireOp.getGuid();
            } else {
              auto guidCall = AC->createRuntimeCall(types::ARTSRTL_artsDbGetGuid,
                                                    {loadedDbElement}, loc);
              elementGuid = guidCall.getResult(0);
            }

            AC->createRuntimeCall(types::ARTSRTL_artsDbIncrementLatch,
                                  {elementGuid}, loc);

            /// Update slot counter
            auto currentSlot = rewriter.create<memref::LoadOp>(loc, slotAlloc);
            auto nextSlot = rewriter.create<arith::AddIOp>(loc, currentSlot,
                                                           incrementConstant);
            rewriter.create<memref::StoreOp>(loc, nextSlot, slotAlloc);
            return;
          }

          /// Recursive case: create loop for current dimension
          auto lowerBound = dbOffsets[dim];
          auto upperBound =
              rewriter.create<arith::AddIOp>(loc, lowerBound, dbSizes[dim]);
          auto loopOp = rewriter.create<scf::ForOp>(loc, lowerBound, upperBound,
                                                    stepConstant);

          auto &loopBlock = loopOp.getRegion().front();
          rewriter.setInsertionPointToStart(&loopBlock);
          indices.push_back(loopOp.getInductionVar());
          incrementLatchRecursive(dim + 1, indices);
          indices.pop_back();
          rewriter.setInsertionPointAfter(loopOp);
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
    auto loc = op.getLoc();
    auto params = op.getParams();
    auto resultType = op.getMemref().getType();

    /// Create memref with dynamic size equal to number of parameters
    auto numParams = AC->createIndexConstant(params.size(), loc);
    auto allocOp = rewriter.create<memref::AllocOp>(loc, resultType,
                                                    ValueRange{numParams});

    /// Store each parameter (cast to i64) into the memref
    for (unsigned i = 0; i < params.size(); ++i) {
      auto index = AC->createIndexConstant(i, loc);
      auto castParam = AC->castParameter(AC->Int64, params[i], loc);
      rewriter.create<memref::StoreOp>(loc, castParam, allocOp,
                                       ValueRange{index});
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
    auto paramMemref = op.getMemref();
    auto results = op.getUnpacked();

    SmallVector<Value> newResults;
    for (unsigned i = 0; i < results.size(); ++i) {
      auto idx = AC->createIndexConstant(i, op.getLoc());
      auto loadedParam = rewriter.create<memref::LoadOp>(
          op.getLoc(), paramMemref, ValueRange{idx});
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
    auto depMemref = op.getMemref();
    auto results = op.getUnpacked();
    auto loc = op.getLoc();

    SmallVector<Value> newResults;
    for (unsigned i = 0; i < results.size(); ++i) {
      auto idx = AC->createIndexConstant(i, loc);

      /// Load the raw EDT dependency structure from depv array
      /// This gives us the dependency structure containing both GUID and
      /// pointer
      auto depStruct =
          rewriter.create<memref::LoadOp>(loc, depMemref, ValueRange{idx});
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
    auto depStruct = op.getDepStruct();
    auto loc = op.getLoc();

    /// Extract the actual pointer from the EDT dependency structure
    /// using ArtsCodegen API (equivalent to depStruct.ptr)
    auto actualPtr = AC->getPtrFromEdtDep(depStruct, loc);

    /// Convert the raw pointer to a memref using polygeist helper
    auto resultType = op.getPtr().getType();
    auto memrefResult = rewriter.create<polygeist::Pointer2MemrefOp>(
        loc, resultType, actualPtr);

    rewriter.replaceOp(op, memrefResult);
    return success();
  }
};

/// Pattern to convert arts.dep_get_guid operations
struct DepGetGuidOpPattern : public ArtsToLLVMPattern<DepGetGuidOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DepGetGuidOp op,
                                PatternRewriter &rewriter) const override {
    auto depStruct = op.getDepStruct();
    auto loc = op.getLoc();

    /// Extract the GUID from the EDT dependency structure
    /// using ArtsCodegen API (equivalent to depStruct.guid)
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
        /// Empty memref case
        paramc = AC->createIntConstant(0, AC->Int32, op.getLoc());
      } else {
        /// Dynamic memref case - get size from memref
        auto zeroIndex = AC->createIndexConstant(0, op.getLoc());
        auto memrefSize =
            rewriter.create<memref::DimOp>(op.getLoc(), paramv, zeroIndex);
        paramc = rewriter.create<arith::IndexCastOp>(op.getLoc(), AC->Int32,
                                                     memrefSize);
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
    auto loc = op.getLoc();
    SmallVector<Value> sizes(op.getSizes());
    auto resultType = op.getPtr().getType().cast<MemRefType>();

    auto dbMemref = rewriter.create<memref::AllocOp>(loc, resultType, sizes);

    /// Create GUID memref with same shape as datablock
    auto guidType = op.getGuid().getType().cast<MemRefType>();
    auto guidMemref = rewriter.create<memref::AllocOp>(loc, guidType, sizes);

    /// Handle single and multi-dimensional DBs
    if (sizes.empty() || (sizes.size() == 1 && resultType.getRank() <= 1))
      createSingleDb(rewriter, op, dbMemref, guidMemref, {}, loc);
    else
      createMultiDimDbs(rewriter, op, dbMemref, guidMemref, sizes, loc);

    rewriter.replaceOp(op, {guidMemref, dbMemref});
    return success();
  }

private:
  void createSingleDb(PatternRewriter &rewriter, DbAllocOp op, Value dbMemref,
                      Value guidMemref, ArrayRef<Value> indices, Location loc) const {
    /// Get DB mode from operation attribute
    auto dbModeValue = static_cast<int>(op.getDbModeAttr().getValue());
    auto dbMode = AC->createIntConstant(dbModeValue, AC->Int32, loc);

    /// Reserve GUID for the DB
    Value currentNode = op.getRoute();
    auto guidCall = AC->createRuntimeCall(types::ARTSRTL_artsReserveGuidRoute,
                                          {dbMode, currentNode}, loc);
    auto guid = guidCall.getResult(0);

    /// Calculate element size
    auto resultType = op.getPtr().getType().cast<MemRefType>();
    auto elementSize = AC->computeMemrefElementSize(resultType, loc);

    /// Create the DB with GUID
    auto dbCall = AC->createRuntimeCall(types::ARTSRTL_artsDbCreateWithGuid,
                                        {guid, elementSize}, loc);
    auto dbPtr = dbCall.getResult(0);

    /// Store the DB pointer in the memref
    if (indices.empty())
      rewriter.create<memref::StoreOp>(loc, dbPtr, dbMemref);
    else
      rewriter.create<memref::StoreOp>(loc, dbPtr, dbMemref, indices);

    /// Store the GUID in the GUID memref
    if (indices.empty())
      rewriter.create<memref::StoreOp>(loc, guid, guidMemref);
    else
      rewriter.create<memref::StoreOp>(loc, guid, guidMemref, indices);
  }

  void createMultiDimDbs(PatternRewriter &rewriter, DbAllocOp op,
                         Value dbMemref, Value guidMemref, ArrayRef<Value> sizes,
                         Location loc) const {
    const unsigned rank = sizes.size();
    auto stepConstant = AC->createIndexConstant(1, loc);

    std::function<void(unsigned, SmallVector<Value, 4> &)> createDbsRecursive =
        [&](unsigned dim, SmallVector<Value, 4> &indices) {
          if (dim == rank) {
            createSingleDb(rewriter, op, dbMemref, guidMemref, indices, loc);
            return;
          }

          /// Create loop for current dimension
          auto lowerBound = AC->createIndexConstant(0, loc);
          auto upperBound = sizes[dim];
          auto loopOp = rewriter.create<scf::ForOp>(loc, lowerBound, upperBound,
                                                    stepConstant);

          auto &loopBlock = loopOp.getRegion().front();
          rewriter.setInsertionPointToStart(&loopBlock);
          indices.push_back(loopOp.getInductionVar());
          createDbsRecursive(dim + 1, indices);
          indices.pop_back();
          rewriter.setInsertionPointAfter(loopOp);
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
    auto source = op.getSource();
    auto indices = op.getIndices();
    auto loc = op.getLoc();

    Value ptrResult;
    if (indices.empty()) {
      /// Single element case - source is already the pointer
      ptrResult = source;
    } else {
      /// Multidimensional case - load pointer from indexed position
      ptrResult = rewriter.create<memref::LoadOp>(loc, source, indices);
    }

    /// Get GUID from the datablock pointer using artsDbGetGuid
    auto guidCall = AC->createRuntimeCall(types::ARTSRTL_artsDbGetGuid,
                                          {ptrResult}, loc);
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
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to convert arts.db_control operations
struct DbControlPattern : public ArtsToLLVMPattern<DbControlOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbControlOp op,
                                PatternRewriter &rewriter) const override {
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
    auto loc = op.getLoc();

    /// The db can be a DbAllocOp or a DbAcquireOp
    SmallVector<Value> sizes;
    if (auto allocOp = dyn_cast<DbAllocOp>(op.getDb().getDefiningOp())) {
      sizes = allocOp.getSizes();
    } else if (auto acquireOp =
                   dyn_cast<DbAcquireOp>(op.getDb().getDefiningOp())) {
      sizes = acquireOp.getSizes();
    }

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
        folded *= cst.getValue().cast<IntegerAttr>().getInt();
        continue;
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
    auto allocOp = rewriter.create<memref::AllocOp>(loc, scalarI32);

    Value productVal = AC->createIntConstant(1, AC->Int32, loc);
    for (Value sz : sizes) {
      Value szI32 = sz;
      if (sz.getType().isa<IndexType>())
        szI32 = rewriter.create<arith::IndexCastOp>(loc, AC->Int32, sz);
      else if (!sz.getType().isInteger(32))
        szI32 = rewriter.create<arith::TruncIOp>(loc, AC->Int32, sz);
      productVal = rewriter.create<arith::MulIOp>(loc, productVal, szI32);
    }

    rewriter.create<memref::StoreOp>(loc, productVal, allocOp);
    rewriter.replaceOp(op, allocOp.getResult());
    return success();
  }
};

/// Pattern to convert arts.db_subindex operations to polygeist subindex
struct DbSubIndexOpPattern : public ArtsToLLVMPattern<DbSubIndexOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbSubIndexOp op,
                                PatternRewriter &rewriter) const override {
    auto source = op.getSource();
    auto indices = op.getIndices();
    auto loc = op.getLoc();

    /// Chain polygeist::SubIndexOp operations for multidimensional access
    /// Each polygeist::SubIndexOp handles one dimension
    Value current = source;

    for (size_t i = 0; i < indices.size(); ++i) {
      auto currentType = cast<MemRefType>(current.getType());
      auto resultType = MemRefType::get(
          currentType.getShape().drop_front(1), /// Drop one dimension
          currentType.getElementType(), currentType.getLayout(),
          currentType.getMemorySpace());

      current = rewriter.create<polygeist::SubIndexOp>(loc, resultType, current,
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

    auto resultType = op.getResult().getType().cast<MemRefType>();

    /// Calculate total allocation size using ArtsCodegen helpers
    auto elementSize = AC->computeMemrefElementSize(resultType, op.getLoc());
    Value totalSize = elementSize;

    /// Handle dynamic sizes
    auto dynamicSizes = op.getDynamicSizes();
    if (!dynamicSizes.empty()) {
      auto totalElements =
          AC->computeMemrefTotalElements(dynamicSizes, op.getLoc());
      totalSize =
          rewriter.create<arith::MulIOp>(op.getLoc(), totalSize, totalElements);
    } else {
      /// Handle static shape
      auto shape = resultType.getShape();
      int64_t totalElements = 1;
      for (auto dim : shape) {
        if (dim != ShapedType::kDynamic) {
          totalElements *= dim;
        }
      }
      auto totalElementsVal =
          AC->createIntConstant(totalElements, AC->Int64, op.getLoc());
      totalSize = rewriter.create<arith::MulIOp>(op.getLoc(), totalSize,
                                                 totalElementsVal);
    }

    /// Call artsMalloc and cast result
    auto artsAllocCall = AC->createRuntimeCall(types::ARTSRTL_artsMalloc,
                                               {totalSize}, op.getLoc());
    auto castPtr =
        AC->castPtr(resultType, artsAllocCall.getResult(0), op.getLoc());

    rewriter.replaceOp(op, castPtr);
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
    rewriter.setInsertionPointAfter(op);
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
  if (failed(
          applyPatternsAndFoldGreedily(module, std::move(patterns), config))) {
    ARTS_ERROR("Failed to apply ARTS to LLVM conversion patterns");
    delete AC;
    return signalPassFailure();
  }

  //// Initialize runtime (main function transformation)
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

  /// Runtime information patterns
  patterns.add<GetTotalWorkersPattern, GetTotalNodesPattern,
               GetCurrentWorkerPattern, GetCurrentNodePattern>(context, AC);

  /// Event and barrier patterns
  patterns.add<EventPattern, BarrierPattern, RecordInDepPattern,
               IncrementOutLatchPattern>(context, AC);

  /// EDT patterns
  patterns.add<EdtParamPackPattern, EdtParamUnpackPattern, EdtDepUnpackPattern,
               DepGetPtrOpPattern, DepGetGuidOpPattern, EdtCreatePattern>(
      context, AC);

  /// DB patterns
  patterns.add<DbAcquirePattern, DbReleasePattern, DbControlPattern,
               DbNumElementsPattern, DbSubIndexOpPattern, AllocPattern,
               DbAllocPattern>(context, AC);

  /// Epoch patterns
  patterns.add<EpochPattern>(context, AC);

  /// Terminator patterns
  patterns.add<YieldPattern>(context, AC);
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
