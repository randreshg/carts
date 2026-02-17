///==========================================================================///
/// File: ConvertArtsToLLVM.cpp
///
/// This file implements a pass to convert ARTS dialect operations into
/// LLVM dialect operations.
///
/// Example:
///   Before:
///     %g = arts.edt_create ...
///     arts.record_dep %g, ...
///     arts.wait_on_epoch %e
///
///   After:
///     %g = call @artsEdtCreate(...)
///     call @artsRecordDep(...)
///     call @artsWaitOnHandle(...)
///==========================================================================///

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/NVVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Value.h"
#include "polygeist/Ops.h"
/// Arts
#include "../ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
/// Others
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(convert_arts_to_llvm);

using namespace mlir;
using namespace arts;

///===----------------------------------------------------------------------===///
// Conversion Patterns
///===----------------------------------------------------------------------===///

/// Base class for ARTS to LLVM conversion patterns with shared ArtsCodegen
template <typename OpType>
class ArtsToLLVMPattern : public OpRewritePattern<OpType> {
protected:
  ArtsCodegen *AC;

public:
  ArtsToLLVMPattern(MLIRContext *context, ArtsCodegen *ac)
      : OpRewritePattern<OpType>(context), AC(ac) {}
};

///===----------------------------------------------------------------------===///
/// Helpers for DB sizes and single-element detection
///===----------------------------------------------------------------------===///
namespace {
template <typename OpType>
static inline void getDbInfo(OpType op, SmallVector<Value> &sizesOut,
                             SmallVector<Value> &offsetsOut,
                             SmallVector<Value> &indicesOut,
                             bool &isSingleElement) {
  if (auto acqOp = dyn_cast<DbAcquireOp>(op.getOperation())) {
    sizesOut = DatablockUtils::getDependencySizesFromDb(acqOp.getOperation());
    offsetsOut =
        DatablockUtils::getDependencyOffsetsFromDb(acqOp.getOperation());
    indicesOut.assign(acqOp.getIndices().begin(), acqOp.getIndices().end());
  } else if (auto depAcqOp = dyn_cast<DepDbAcquireOp>(op.getOperation())) {
    sizesOut =
        DatablockUtils::getDependencySizesFromDb(depAcqOp.getOperation());
    offsetsOut =
        DatablockUtils::getDependencyOffsetsFromDb(depAcqOp.getOperation());
    indicesOut.assign(depAcqOp.getIndices().begin(),
                      depAcqOp.getIndices().end());
  } else {
    sizesOut.assign(op.getSizes().begin(), op.getSizes().end());
    offsetsOut.clear();
    indicesOut.clear();
  }

  isSingleElement = false;
  if (sizesOut.empty()) {
    isSingleElement = true;
    return;
  }
  if (sizesOut.size() == 1) {
    isSingleElement = mlir::arts::ValueUtils::isOneConstant(sizesOut[0]);
  }
}

static inline DbAllocOp getAllocOpFromGuid(Value dbGuid) {
  if (auto dbAcquireOp = dbGuid.getDefiningOp<DbAcquireOp>())
    return dbAcquireOp.getSourceGuid().getDefiningOp<DbAllocOp>();
  if (auto depDbAcquireOp = dbGuid.getDefiningOp<DepDbAcquireOp>())
    return depDbAcquireOp.getGuid().getDefiningOp<DbAllocOp>();
  return nullptr;
}

static Value buildDim3(ArtsCodegen *AC, Location loc, int64_t x, int64_t y,
                       int64_t z) {
  Value undef = AC->create<LLVM::UndefOp>(loc, AC->Dim3);
  Value vx = AC->createIntConstant(x, AC->Int32, loc);
  Value vy = AC->createIntConstant(y, AC->Int32, loc);
  Value vz = AC->createIntConstant(z, AC->Int32, loc);
  Value withX = AC->create<LLVM::InsertValueOp>(loc, undef, vx,
                                                llvm::ArrayRef<int64_t>{0});
  Value withY = AC->create<LLVM::InsertValueOp>(loc, withX, vy,
                                                llvm::ArrayRef<int64_t>{1});
  return AC->create<LLVM::InsertValueOp>(loc, withY, vz,
                                         llvm::ArrayRef<int64_t>{2});
}

} // namespace

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

///===----------------------------------------------------------------------===///
// Event and Barrier Patterns
///===----------------------------------------------------------------------===///

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

/// Pattern to convert arts.create_epoch operations
struct CreateEpochPattern : public ArtsToLLVMPattern<CreateEpochOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(CreateEpochOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering CreateEpoch Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto loc = op.getLoc();
    auto guid = AC->createIntConstant(0, AC->Int64, loc);
    auto edtSlot = AC->createIntConstant(DEFAULT_EDT_SLOT, AC->Int32, loc);

    /// Create epoch guid
    auto epochGuid = AC->createEpoch(guid, edtSlot, loc);
    rewriter.replaceOp(op, epochGuid);
    return success();
  }
};

/// Pattern to convert arts.wait_on_epoch operations
struct WaitOnEpochPattern : public ArtsToLLVMPattern<WaitOnEpochOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(WaitOnEpochOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering WaitOnEpoch Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    AC->waitOnHandle(op.getEpochGuid(), op.getLoc());
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to convert arts.atomic_add operations
struct AtomicAddPattern : public ArtsToLLVMPattern<AtomicAddOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(AtomicAddOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering AtomicAdd Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);

    auto loc = op.getLoc();
    auto addr = op.getAddr();
    auto value = op.getValue();

    /// Convert memref to LLVM pointer
    Value llvmPtr = rewriter.create<polygeist::Memref2PointerOp>(
        loc, LLVM::LLVMPointerType::get(rewriter.getContext()), addr);

    /// Create LLVM atomic add operation
    rewriter.create<LLVM::AtomicRMWOp>(loc, LLVM::AtomicBinOp::add, llvmPtr,
                                       value, LLVM::AtomicOrdering::seq_cst);

    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to convert __builtin_object_size calls to llvm.objectsize intrinsic.
///
/// Polygeist's cgeist bypasses Clang's CodeGen, so __builtin_object_size is
/// emitted as a regular function call instead of the llvm.objectsize intrinsic.
///
/// __builtin_object_size(ptr, type) -> llvm.objectsize(ptr, min, null_unknown,
/// dynamic)
///
/// Parameter mapping (from Clang's CGBuiltin.cpp):
///   - ptr: First argument
///   - min: (type & 2) != 0  (bit 1 of type parameter)
///   - null_is_unknown: Always true for GCC compatibility
///   - dynamic: false (true only for __builtin_dynamic_object_size)
struct BuiltinObjectSizePattern : public OpRewritePattern<func::CallOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(func::CallOp callOp,
                                PatternRewriter &rewriter) const override {
    auto callee = callOp.getCallee();
    if (callee != "__builtin_object_size")
      return failure();

    Location loc = callOp.getLoc();

    /// Get ptr argument (memref) and convert to llvm.ptr
    Value memrefArg = callOp.getOperand(0);
    Value ptr = rewriter.create<polygeist::Memref2PointerOp>(
        loc, LLVM::LLVMPointerType::get(rewriter.getContext()), memrefArg);

    Value typeArg = callOp.getOperand(1);

    /// Extract type value to compute min flag: min = (type & 2) != 0
    bool minFlag = false;
    if (auto constOp = typeArg.getDefiningOp<arith::ConstantOp>()) {
      if (auto intAttr = dyn_cast<IntegerAttr>(constOp.getValue())) {
        minFlag = (intAttr.getInt() & 2) != 0;
      }
    }

    /// Create boolean constants for intrinsic parameters
    Type i1Type = rewriter.getI1Type();
    Value min = rewriter.create<LLVM::ConstantOp>(loc, i1Type, minFlag);
    Value nullIsUnknown = rewriter.create<LLVM::ConstantOp>(loc, i1Type, true);
    Value dynamic = rewriter.create<LLVM::ConstantOp>(loc, i1Type, false);

    /// Create llvm.call_intrinsic for llvm.objectsize
    Type resultType = callOp.getResult(0).getType();
    auto intrinsicOp = rewriter.create<LLVM::CallIntrinsicOp>(
        loc, resultType, "llvm.objectsize",
        ValueRange{ptr, min, nullIsUnknown, dynamic});

    rewriter.replaceOp(callOp, intrinsicOp.getResults());
    return success();
  }
};

/// Pattern to convert arts.record_dep operations
struct RecordDepPattern : public ArtsToLLVMPattern<RecordDepOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(RecordDepOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering RecordInDep Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto edtGuid = op.getEdtGuid();
    auto loc = op.getLoc();

    /// Get access mode from attribute
    auto accessMode = op.getAccessMode();
    auto acquireModesAttr = op.getAcquireModes();
    ArrayRef<int32_t> acquireModeValues =
        acquireModesAttr ? *acquireModesAttr : ArrayRef<int32_t>{};

    /// Get bounds validity flags for stencil boundary guarding
    auto boundsValids = op.getBoundsValids();

    /// Get ESD byte_offsets and byte_sizes for halo dependencies
    auto byteOffsets = op.getByteOffsets();
    auto byteSizes = op.getByteSizes();

    /// Create shared slot counter for all dependencies
    auto slotTy = MemRefType::get({}, AC->Int32);
    Value sharedSlotAlloc = AC->create<memref::AllocaOp>(loc, slotTy);
    Value zeroI32 = AC->createIntConstant(0, AC->Int32, loc);
    AC->create<memref::StoreOp>(loc, zeroI32, sharedSlotAlloc);

    /// Add dependencies for each datablock using shared slot counter
    unsigned dbIdx = 0;
    for (Value dbGuid : op.getDatablocks()) {
      std::optional<int32_t> acquireMode = std::nullopt;
      if (dbIdx < acquireModeValues.size())
        acquireMode = acquireModeValues[dbIdx];
      /// Get bounds_valid for this dependency if available
      Value boundsValid =
          (dbIdx < boundsValids.size()) ? boundsValids[dbIdx] : Value();
      /// Get ESD byte offset/size for this dependency if available
      Value byteOffset =
          (dbIdx < byteOffsets.size()) ? byteOffsets[dbIdx] : Value();
      Value byteSize = (dbIdx < byteSizes.size()) ? byteSizes[dbIdx] : Value();
      recordDepsForDb(dbGuid, edtGuid, sharedSlotAlloc, accessMode, acquireMode,
                      boundsValid, byteOffset, byteSize, loc);
      ++dbIdx;
    }

    rewriter.eraseOp(op);
    return success();
  }

private:
  /// Load the GUID value for a dependency, handling both direct and depv paths.
  Value loadDbGuidValue(Value dbGuid, Value linearIndex, bool useDepv,
                        Value depStruct, Value baseOffset, Location loc) const {
    if (useDepv) {
      Value finalOffset =
          AC->create<arith::AddIOp>(loc, baseOffset, linearIndex);
      auto depGep =
          AC->create<DepGepOp>(loc, AC->llvmPtr, AC->llvmPtr, depStruct,
                               finalOffset, ValueRange(), ValueRange());
      return AC->create<LLVM::LoadOp>(loc, AC->Int64, depGep.getGuid());
    }
    return AC->create<memref::LoadOp>(loc, dbGuid, ValueRange{linearIndex});
  }

  /// Get totalDBs from source allocation for bounds checking (stencil cases).
  /// Returns nullptr if boundsValid is not present or allocation not found.
  Value getTotalDBsForBoundsCheck(Value dbGuid, Value boundsValid) const {
    if (!boundsValid)
      return nullptr;

    DbAllocOp allocOp = getAllocOpFromGuid(dbGuid);

    if (allocOp && !allocOp.getSizes().empty()) {
      auto sizes = allocOp.getSizes();
      Value totalDBs = sizes[0];
      for (size_t i = 1; i < sizes.size(); ++i)
        totalDBs =
            AC->create<arith::MulIOp>(allocOp.getLoc(), totalDBs, sizes[i]);
      return totalDBs;
    }
    return nullptr;
  }

  void recordDepsForDb(Value dbGuid, Value edtGuid, Value sharedSlotAlloc,
                       DepAccessMode accessMode,
                       std::optional<int32_t> acquireMode, Value boundsValid,
                       Value byteOffset, Value byteSize, Location loc) const {
    SmallVector<Value> dbSizes, dbOffsets, dbIndices;
    bool isSingle = false;
    Value depStruct = nullptr;
    Value baseOffset = nullptr;

    /// Handle both DbAcquireOp and DepDbAcquireOp as sources
    if (auto dbAcquireOp = dbGuid.getDefiningOp<DbAcquireOp>()) {
      getDbInfo(dbAcquireOp, dbSizes, dbOffsets, dbIndices, isSingle);
    } else if (auto depDbAcquireOp = dbGuid.getDefiningOp<DepDbAcquireOp>()) {
      getDbInfo(depDbAcquireOp, dbSizes, dbOffsets, dbIndices, isSingle);
      depStruct = depDbAcquireOp.getDepStruct();
      baseOffset = depDbAcquireOp.getOffset();
    } else {
      ARTS_UNREACHABLE("dbGuid must come from DbAcquireOp or DepDbAcquireOp");
    }

    const bool useDepv = depStruct && baseOffset &&
                         (accessMode == DepAccessMode::from_depv ||
                          dbGuid.getDefiningOp<DepDbAcquireOp>());

    /// Get totalDBs for bounds check (only for stencil cases)
    Value totalDBs =
        useDepv ? Value() : getTotalDBsForBoundsCheck(dbGuid, boundsValid);
    SmallVector<Value> allocSizes;
    if (!useDepv) {
      if (auto allocOp = getAllocOpFromGuid(dbGuid)) {
        auto sizes = allocOp.getSizes();
        allocSizes.assign(sizes.begin(), sizes.end());
      }
    }

    AC->iterateDbElements(
        dbGuid, edtGuid, dbSizes, dbOffsets, isSingle, loc,
        [&](Value linearIndex) {
          recordSingleDb(dbGuid, edtGuid, sharedSlotAlloc, linearIndex,
                         accessMode, acquireMode, boundsValid, depStruct,
                         baseOffset, totalDBs, byteOffset, byteSize, loc);
        },
        allocSizes);
  }

  /// Emit the appropriate runtime call for recording a dependency.
  /// Standard path: artsRecordDep(dbGuid, edtGuid, slot, mode)
  /// ESD path: artsRecordDepAt(dbGuid, edtGuid, slot, mode, offset, size)
  void emitRecordDepCall(Value dbGuidValue, Value edtGuidValue,
                         Value currentSlotI32, Value modeValue,
                         Value byteOffsetI64, Value byteSizeI64,
                         Location loc) const {
    bool useRecordDepAt = byteOffsetI64 && byteSizeI64;
    /// byte_sizes=0 marks "no partial slice" for non-ESD dependencies.
    /// Fall back to full-db dependency in that case.
    if (useRecordDepAt && ValueUtils::isZeroConstant(
                              ValueUtils::stripNumericCasts(byteSizeI64))) {
      useRecordDepAt = false;
    }

    if (useRecordDepAt) {
      /// ESD path: partial slice dependency with byte offset/size
      AC->createRuntimeCall(types::ARTSRTL_artsRecordDepAt,
                            {dbGuidValue, edtGuidValue, currentSlotI32,
                             modeValue, byteOffsetI64, byteSizeI64},
                            loc);
    } else {
      /// Standard path: full datablock dependency
      AC->createRuntimeCall(
          types::ARTSRTL_artsRecordDep,
          {dbGuidValue, edtGuidValue, currentSlotI32, modeValue}, loc);
    }
  }

  /// Record a single datablock dependency for an EDT slot.
  ///
  /// Two main paths:
  /// 1. Guarded (boundsValid set): if-else to handle boundary workers
  ///    - Then: artsRecordDep[At] for valid indices
  ///    - Else: artsSignalEdtNull for out-of-bounds
  /// 2. Unconditional: direct artsRecordDep[At] call
  ///
  /// Within each path, ESD vs standard is handled by emitRecordDepCall.
  void recordSingleDb(Value dbGuid, Value edtGuid, Value slotAlloc,
                      Value linearIndex, DepAccessMode accessMode,
                      std::optional<int32_t> acquireMode, Value boundsValid,
                      Value depStruct, Value baseOffset, Value totalDBs,
                      Value byteOffset, Value byteSize, Location loc) const {
    const bool useDepv = depStruct && baseOffset &&
                         (accessMode == DepAccessMode::from_depv ||
                          dbGuid.getDefiningOp<DepDbAcquireOp>());

    /// Extend boundsValid with DB index check for stencil cases
    Value effectiveBoundsValid = boundsValid;
    if (totalDBs) {
      Value indexInBounds = AC->create<arith::CmpIOp>(
          loc, arith::CmpIPredicate::ult, linearIndex, totalDBs);
      effectiveBoundsValid =
          AC->create<arith::AndIOp>(loc, boundsValid, indexInBounds);
    }

    /// Prepare common values
    Value edtGuidValue = edtGuid;
    if (auto mt = edtGuid.getType().dyn_cast<MemRefType>()) {
      auto zeroIndex = AC->createIndexConstant(0, loc);
      edtGuidValue =
          AC->create<memref::LoadOp>(loc, edtGuid, ValueRange{zeroIndex});
    }
    edtGuidValue = AC->castToInt(AC->Int64, edtGuidValue, loc);

    auto currentSlotI32 = AC->create<memref::LoadOp>(loc, slotAlloc);
    int32_t modeInt = acquireMode.value_or(static_cast<int32_t>(DbMode::write));
    Value modeValue = AC->createIntConstant(modeInt, AC->Int32, loc);

    /// Prepare ESD values if needed (cast Index to Int64).
    /// byte_size == 0 denotes "no partial slice" and should use full-db deps.
    bool hasPartialSlice =
        byteOffset && byteSize &&
        !ValueUtils::isZeroConstant(ValueUtils::stripNumericCasts(byteSize));
    Value byteOffsetI64 =
        hasPartialSlice ? AC->castToInt(AC->Int64, byteOffset, loc) : nullptr;
    Value byteSizeI64 =
        hasPartialSlice ? AC->castToInt(AC->Int64, byteSize, loc) : nullptr;

    if (effectiveBoundsValid) {
      /// GUARDED PATH: boundary workers may have invalid indices
      auto ifOp = AC->create<scf::IfOp>(loc, effectiveBoundsValid,
                                        /*withElseRegion=*/true);

      /// Then: valid index - record the dependency
      AC->setInsertionPointToStart(&ifOp.getThenRegion().front());
      Value dbGuidValue = loadDbGuidValue(dbGuid, linearIndex, useDepv,
                                          depStruct, baseOffset, loc);
      emitRecordDepCall(dbGuidValue, edtGuidValue, currentSlotI32, modeValue,
                        byteOffsetI64, byteSizeI64, loc);

      /// Else: invalid index - signal null dependency
      AC->setInsertionPointToStart(&ifOp.getElseRegion().front());
      AC->createRuntimeCall(types::ARTSRTL_artsSignalEdtNull,
                            {edtGuidValue, currentSlotI32}, loc);

      AC->setInsertionPointAfter(ifOp);
    } else {
      /// UNCONDITIONAL PATH: all indices are valid
      Value dbGuidValue = loadDbGuidValue(dbGuid, linearIndex, useDepv,
                                          depStruct, baseOffset, loc);
      emitRecordDepCall(dbGuidValue, edtGuidValue, currentSlotI32, modeValue,
                        byteOffsetI64, byteSizeI64, loc);
    }

    /// Increment slot counter
    auto oneI32 = AC->createIntConstant(1, AC->Int32, loc);
    auto incrementedSlot =
        AC->create<arith::AddIOp>(loc, currentSlotI32, oneI32);
    AC->create<memref::StoreOp>(loc, incrementedSlot, slotAlloc);
  }
};

/// Pattern to convert arts.increment_dep operations
struct IncrementDepPattern : public ArtsToLLVMPattern<IncrementDepOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(IncrementDepOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering IncrementOutLatch Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto loc = op.getLoc();

    /// Get access mode from attribute
    auto accessMode = op.getAccessMode();

    /// Create shared slot counter for all latch increments
    auto slotTy = MemRefType::get({}, AC->Int32);
    Value sharedSlotAlloc = AC->create<memref::AllocaOp>(loc, slotTy);
    Value zeroI32 = AC->createIntConstant(0, AC->Int32, loc);
    AC->create<memref::StoreOp>(loc, zeroI32, sharedSlotAlloc);

    /// Increment latch for each datablock using shared slot counter
    for (Value dbGuid : op.getDatablocks())
      incLatchForDb(dbGuid, sharedSlotAlloc, accessMode, loc);

    rewriter.eraseOp(op);
    return success();
  }

private:
  void incLatchForDb(Value dbGuid, Value sharedSlotAlloc,
                     DepAccessMode accessMode, Location loc) const {
    SmallVector<Value> dbSizes, dbOffsets, dbIndices;
    bool isSingle = false;
    SmallVector<Value> allocSizes;

    /// Extract base offset and dep_struct for this specific dependency
    Value depStruct = nullptr;
    Value baseOffset = nullptr;

    /// Handle both DbAcquireOp and DepDbAcquireOp as sources
    if (auto dbAcquireOp = dbGuid.getDefiningOp<DbAcquireOp>()) {
      getDbInfo(dbAcquireOp, dbSizes, dbOffsets, dbIndices, isSingle);
      /// depStruct and baseOffset remain null for DbAcquireOp
    } else if (auto depDbAcquireOp = dbGuid.getDefiningOp<DepDbAcquireOp>()) {
      getDbInfo(depDbAcquireOp, dbSizes, dbOffsets, dbIndices, isSingle);
      /// For DepDbAcquireOp, always extract dep_struct and base offset
      depStruct = depDbAcquireOp.getDepStruct();
      baseOffset = depDbAcquireOp.getOffset();
    } else {
      ARTS_ERROR("dbGuid must come from DbAcquireOp or DepDbAcquireOp"
                 << dbGuid);
      ARTS_UNREACHABLE("dbGuid must come from DbAcquireOp or DepDbAcquireOp");
    }
    if (auto allocOp = getAllocOpFromGuid(dbGuid)) {
      auto sizes = allocOp.getSizes();
      allocSizes.assign(sizes.begin(), sizes.end());
    }

    const bool useDepv = depStruct && baseOffset &&
                         (accessMode == DepAccessMode::from_depv ||
                          dbGuid.getDefiningOp<DepDbAcquireOp>());
    /// Use shared slot counter and iterate over all db elements
    AC->iterateDbElements(
        dbGuid, Value(), dbSizes, dbOffsets, isSingle, loc,
        [&](Value linearIndex) {
          incSingleLatch(dbGuid, sharedSlotAlloc, linearIndex, accessMode,
                         depStruct, baseOffset, loc);
        },
        useDepv ? ArrayRef<Value>() : allocSizes);
  }

  void incSingleLatch(Value dbGuid, Value slotAlloc, Value linearIndex,
                      DepAccessMode accessMode, Value depStruct,
                      Value baseOffset, Location loc) const {
    /// Load dbGuid value - check if we actually have depStruct (from
    /// DepDbAcquireOp) or if this specific dependency is from DbAcquireOp
    Value dbGuidValue;
    if (depStruct && baseOffset) {
      /// Access GUID via DepGepOp from EDT depv structure
      /// Must add baseOffset to linearIndex to get correct position in depv
      Value finalOffset =
          AC->create<arith::AddIOp>(loc, baseOffset, linearIndex);
      auto depGep =
          AC->create<DepGepOp>(loc, AC->llvmPtr, AC->llvmPtr, depStruct,
                               finalOffset, ValueRange(), ValueRange());
      dbGuidValue = AC->create<LLVM::LoadOp>(loc, AC->Int64, depGep.getGuid());
    } else {
      /// Direct access via memref.load for DbAcquireOp
      dbGuidValue =
          AC->create<memref::LoadOp>(loc, dbGuid, ValueRange{linearIndex});
    }
    AC->createRuntimeCall(types::ARTSRTL_artsDbIncrementLatch, {dbGuidValue},
                          loc);

    /// Increment the shared slot counter for next latch increment
    auto currentSlotI32 = AC->create<memref::LoadOp>(loc, slotAlloc);
    auto oneI32 = AC->createIntConstant(1, AC->Int32, loc);
    auto incrementedSlot =
        AC->create<arith::AddIOp>(loc, currentSlotI32, oneI32);
    AC->create<memref::StoreOp>(loc, incrementedSlot, slotAlloc);
  }
};

///===----------------------------------------------------------------------===///
// EDT  Patterns
///===----------------------------------------------------------------------===///
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

    /// Check if result type has dynamic dimensions
    bool hasDynamicDims = resultType.getNumDynamicDims() > 0;

    memref::AllocOp allocOp;
    if (hasDynamicDims) {
      /// Dynamic memref: allocate with size and store parameters
      auto numParams = AC->createIndexConstant(params.size(), loc);
      allocOp =
          AC->create<memref::AllocOp>(loc, resultType, ValueRange{numParams});

      for (unsigned i = 0; i < params.size(); ++i) {
        auto index = AC->createIndexConstant(i, loc);
        auto castParam = AC->castParameter(
            AC->Int64, params[i], loc, ArtsCodegen::ParameterCastMode::Bitwise);
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
          AC->castParameter(results[i].getType(), loadedParam, op.getLoc(),
                            ArtsCodegen::ParameterCastMode::Bitwise);
      newResults.push_back(castedParam);
    }

    rewriter.replaceOp(op, newResults);
    return success();
  }
};

/// Pattern to convert arts.dep_gep operations
struct DepGepOpPattern : public ArtsToLLVMPattern<DepGepOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DepGepOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering DepGep Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto depStruct = op.getDepStruct();
    auto offset = op.getOffset();
    auto indices = op.getIndices();
    auto strides = op.getStrides();
    auto loc = op.getLoc();

    /// Cast dep_struct to typed pointer to LLVM pointer
    Value typedDepPtr = AC->castToLLVMPtr(depStruct, loc);

    /// Compute linearized index: offset + sum_i indices[i] * strides[i]
    Value linearIndex = offset;
    if (linearIndex.getType() != AC->Int64)
      linearIndex = AC->castToInt(AC->Int64, linearIndex, loc);

    for (size_t i = 0; i < indices.size(); ++i) {
      Value idx = indices[i];
      if (idx.getType() != AC->Int64)
        idx = AC->castToInt(AC->Int64, idx, loc);
      Value strideVal = (i < strides.size())
                            ? strides[i]
                            : AC->createIntConstant(1, AC->Int64, loc);
      if (strideVal.getType() != AC->Int64)
        strideVal = AC->castToInt(AC->Int64, strideVal, loc);

      Value contrib = AC->create<arith::MulIOp>(loc, idx, strideVal);
      linearIndex = AC->create<arith::AddIOp>(loc, linearIndex, contrib);
    }

    Value depEntryPtr = AC->create<LLVM::GEPOp>(
        loc, AC->llvmPtr, AC->ArtsEdtDep, typedDepPtr, ValueRange{linearIndex});

    /// Extract both guid (field #0) and ptr (field #2) from depv
    auto c0 = AC->createIntConstant(0, AC->Int64, loc);
    auto cPtrIdx = AC->createIntConstant(2, AC->Int64, loc);

    /// Get the guid pointer (field #0)
    auto guidPtr = AC->create<LLVM::GEPOp>(loc, AC->llvmPtr, AC->ArtsEdtDep,
                                           depEntryPtr, ValueRange{c0, c0});

    /// Get the data pointer (field #2)
    auto dataPtr = AC->create<LLVM::GEPOp>(
        loc, AC->llvmPtr, AC->ArtsEdtDep, depEntryPtr, ValueRange{c0, cPtrIdx});

    /// Return both: guid pointer and data pointer
    rewriter.replaceOp(op, ValueRange{guidPtr, dataPtr});
    return success();
  }
};

/// Pattern to convert arts.dep_db_acquire operations
struct DepDbAcquireOpPattern : public ArtsToLLVMPattern<DepDbAcquireOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DepDbAcquireOp op,
                                PatternRewriter &rewriter) const override {
    auto depStruct = op.getDepStruct();
    rewriter.replaceOp(op, ValueRange{depStruct, depStruct});
    return success();
  }
};

/// Pattern to convert arts.db_gep operations to LLVM GEP using element strides
struct DbGepOpPattern : public ArtsToLLVMPattern<arts::DbGepOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(arts::DbGepOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering DbGep Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto base = op.getBasePtr();
    auto indices = op.getIndices();
    auto strides = op.getStrides();
    auto loc = op.getLoc();

    /// Cast base to LLVM pointer type
    Value basePtr = AC->castToLLVMPtr(base, loc);

    /// If base is a memref of pointers, we must load the pointer element;
    /// otherwise we compute the address within the element buffer.
    auto baseMT = base.getType().dyn_cast<MemRefType>();

    /// Pad strides with 1s to match indices length
    SmallVector<Value> paddedStrides(strides.begin(), strides.end());
    while (paddedStrides.size() < indices.size())
      paddedStrides.push_back(AC->createIndexConstant(1, loc));

    /// Compute linear element index using provided strides
    Value linearIdx =
        AC->computeLinearIndexFromStrides(paddedStrides, indices, loc);
    Value idx64 = AC->castToInt(AC->Int64, linearIdx, loc);

    /// Use typed GEP based on the element type; default to pointer elements
    Type elemTy = baseMT ? baseMT.getElementType() : AC->llvmPtr;
    Value elemAddr = AC->create<LLVM::GEPOp>(loc, AC->llvmPtr, elemTy, basePtr,
                                             ValueRange{idx64});

    rewriter.replaceOp(op, elemAddr);
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
    auto funcNameAttr =
        op->getAttrOfType<StringAttr>(AttrNames::Operation::OutlinedFunc);
    if (!funcNameAttr)
      return op.emitError("Missing outlined_func attribute");

    auto outlined =
        AC->getModule().lookupSymbol<func::FuncOp>(funcNameAttr.getValue());
    if (!outlined)
      return op.emitError("EDT Outlined function not found");

    /// Build artsEdtCreate arguments; route required, default 0 if missing
    auto funcPtr = AC->createFnPtr(outlined, op.getLoc());
    Value route = op.getRoute();
    if (!route)
      route = AC->createIntConstant(0, AC->Int32, op.getLoc());
    auto paramv = op.getParamMemref();
    Value depc = op.getDepCount();
    if (depc && depc.getType() != AC->Int32)
      depc = AC->castToInt(AC->Int32, depc, op.getLoc());

    /// Calculate parameter count from memref size
    Value paramc;
    if (auto memrefType = paramv.getType().dyn_cast<MemRefType>()) {
      if (memrefType.hasStaticShape() && memrefType.getNumElements() == 0) {
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

    auto createIdAttr =
        op->getAttrOfType<IntegerAttr>(AttrNames::Operation::ArtsCreateId);
    Value artsIdVal;
    if (createIdAttr) {
      artsIdVal =
          AC->create<arith::ConstantOp>(op.getLoc(), AC->Int64, createIdAttr);
    }

    /// Create runtime call; prefer GPU runtime when launch config is attached.
    func::CallOp callOp;
    if (auto launchAttr = op->getAttrOfType<GpuLaunchConfigAttr>(
            AttrNames::Operation::GpuLaunchConfig)) {
      Value grid = buildDim3(AC, op.getLoc(), launchAttr.getGridX(),
                             launchAttr.getGridY(), launchAttr.getGridZ());
      Value block = buildDim3(AC, op.getLoc(), launchAttr.getBlockX(),
                              launchAttr.getBlockY(), launchAttr.getBlockZ());
      Value toSignal = AC->createIntConstant(0, AC->Int64, op.getLoc());
      Value slot = AC->createIntConstant(0, AC->Int32, op.getLoc());
      Value dataGuid = AC->createIntConstant(0, AC->Int64, op.getLoc());
      callOp = AC->createRuntimeCall(types::ARTSRTL_artsEdtCreateGpu,
                                     {funcPtr, route, paramc, paramv, depc,
                                      grid, block, toSignal, slot, dataGuid},
                                     op.getLoc());
    } else if (createIdAttr) {
      if (op.getEpochGuid()) {
        callOp =
            AC->createRuntimeCall(types::ARTSRTL_artsEdtCreateWithEpochArtsId,
                                  {funcPtr, route, paramc, paramv, depc,
                                   op.getEpochGuid(), artsIdVal},
                                  op.getLoc());
      } else {
        callOp = AC->createRuntimeCall(
            types::ARTSRTL_artsEdtCreateWithArtsId,
            {funcPtr, route, paramc, paramv, depc, artsIdVal}, op.getLoc());
      }
    } else {
      if (op.getEpochGuid()) {
        callOp = AC->createRuntimeCall(
            types::ARTSRTL_artsEdtCreateWithEpoch,
            {funcPtr, route, paramc, paramv, depc, op.getEpochGuid()},
            op.getLoc());
      } else {
        callOp = AC->createRuntimeCall(types::ARTSRTL_artsEdtCreate,
                                       {funcPtr, route, paramc, paramv, depc},
                                       op.getLoc());
      }
    }
    if (createIdAttr)
      callOp->setAttr(AttrNames::Operation::ArtsCreateId, createIdAttr);
    rewriter.replaceOp(op, callOp.getResult(0));
    return success();
  }
};

/// Pattern to convert arts.gpu_memcpy operations
struct GpuMemcpyPattern : public ArtsToLLVMPattern<GpuMemcpyOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(GpuMemcpyOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering GpuMemcpy Op " << op);
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to convert arts.lc_sync operations
struct LcSyncPattern : public ArtsToLLVMPattern<LcSyncOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(LcSyncOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering LcSync Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    Location loc = op.getLoc();

    Value edtGuid = AC->castToInt(AC->Int64, op.getEdtGuid(), loc);
    Value slot = AC->castToInt(AC->Int32, op.getSlot(), loc);
    Value dataGuid = op.getDataGuid();

    if (auto memrefTy = dataGuid.getType().dyn_cast<MemRefType>()) {
      (void)memrefTy;
      Value zero = AC->createIndexConstant(0, loc);
      dataGuid = AC->create<memref::LoadOp>(loc, dataGuid, zero);
    }
    dataGuid = AC->castToInt(AC->Int64, dataGuid, loc);

    AC->createRuntimeCall(types::ARTSRTL_artsLCSync, {edtGuid, slot, dataGuid},
                          loc);
    rewriter.eraseOp(op);
    return success();
  }
};

///===----------------------------------------------------------------------===///
// DB Patterns
///===----------------------------------------------------------------------===///
/// Pattern to convert arts.db_alloc operations
struct DbAllocPattern : public ArtsToLLVMPattern<DbAllocOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(DbAllocOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering DbAlloc Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto loc = op.getLoc();
    Value guidMemref, dbMemref;

    auto dbMode = AC->createIntConstant(
        static_cast<int>(op.getDbModeAttr().getValue()), AC->Int32, loc);
    Value route = op.getRoute();
    if (!route)
      route = AC->createIntConstant(0, AC->Int32, loc);
    bool distributedOwnership = hasDistributedDbAllocation(op.getOperation());
    Value elementSize = AC->computeElementTypeSize(op.getElementType(), loc);

    /// Compute payload size from elementSizes (product of all payload
    /// dimensions)
    Value payloadSize = AC->createIndexConstant(1, loc);
    for (Value payloadDim : op.getElementSizes())
      payloadSize = AC->create<arith::MulIOp>(loc, payloadSize, payloadDim);

    /// Total datablock size = elementSize * payloadSize
    Value totalDbSize =
        AC->create<arith::MulIOp>(loc, elementSize, payloadSize);
    std::optional<int64_t> nextId;
    nextId = getArtsId(op);

    SmallVector<Value> dbSizes, dbOffsets, dbIndices;
    bool isSingleElement = false;
    getDbInfo(op, dbSizes, dbOffsets, dbIndices, isSingleElement);

    if (distributedOwnership) {
      if (failed(lowerDistributedRuntimeDbAlloc(
              op, nextId, dbSizes, isSingleElement, guidMemref, dbMemref)))
        return failure();
      rewriter.replaceOp(op, {guidMemref, dbMemref});
      return success();
    }

    if (isSingleElement) {
      ARTS_DEBUG("Creating single DB");
      /// Allocate 1D linear array of GUIDs
      Value totalElems = AC->createIndexConstant(1, loc);
      auto guidType = MemRefType::get({ShapedType::kDynamic}, AC->Int64);
      guidMemref =
          AC->create<memref::AllocOp>(loc, guidType, ValueRange{totalElems});
      /// Allocate 1D linear array of DB Pointers
      auto payloadPtrType =
          MemRefType::get({ShapedType::kDynamic}, AC->llvmPtr);
      dbMemref = AC->create<memref::AllocOp>(loc, payloadPtrType,
                                             ValueRange{totalElems});
      createSingleDb(dbMemref, guidMemref, dbMode, route, totalDbSize,
                     nextId ? &nextId : nullptr, loc, distributedOwnership);
    } else {
      ARTS_DEBUG("Creating multi-dim DB");
      /// Compute total number of elements
      Value totalElems = AC->computeTotalElements(dbSizes, loc);
      /// Allocate 1D linear array of GUIDs
      auto guidType = MemRefType::get({ShapedType::kDynamic}, AC->Int64);
      guidMemref =
          AC->create<memref::AllocOp>(loc, guidType, ValueRange{totalElems});
      /// Allocate 1D linear array of DB Pointers
      auto payloadPtrType =
          MemRefType::get({ShapedType::kDynamic}, AC->llvmPtr);
      dbMemref = AC->create<memref::AllocOp>(loc, payloadPtrType,
                                             ValueRange{totalElems});
      createMultiDbs(dbMemref, guidMemref, dbSizes, dbMode, route, totalDbSize,
                     nextId ? &nextId : nullptr, loc, distributedOwnership);
    }

    rewriter.replaceOp(op, {guidMemref, dbMemref});
    return success();
  }

private:
  memref::GlobalOp getOrCreateMemrefGlobal(StringRef symbolName,
                                           MemRefType type,
                                           Location loc) const {
    ModuleOp module = AC->getModule();
    if (auto existing = module.lookupSymbol<memref::GlobalOp>(symbolName))
      return existing;

    OpBuilder::InsertionGuard IG(AC->getBuilder());
    AC->setInsertionPoint(module);
    auto global = AC->create<memref::GlobalOp>(
        loc, symbolName, StringAttr(), type, Attribute(),
        /*constant=*/false, IntegerAttr());
    global.setSymVisibility("private");
    return global;
  }

  LogicalResult
  lowerDistributedRuntimeDbAlloc(DbAllocOp op, std::optional<int64_t> nextId,
                                 ArrayRef<Value> dbSizes, bool isSingleElement,
                                 Value &guidMemref, Value &dbMemref) const {
    uint64_t baseId = getArtsId(op);
    if (baseId == 0)
      baseId =
          static_cast<uint64_t>(reinterpret_cast<uintptr_t>(op.getOperation()));
    std::string baseName = "__carts_dist_alloc_" + std::to_string(baseId);
    std::string guidHolderSymbol = baseName + "_guid_holder";
    std::string ptrHolderSymbol = baseName + "_ptr_holder";
    std::string initSymbol = baseName + "_init";

    auto guidDynamicType = MemRefType::get({ShapedType::kDynamic}, AC->Int64);
    auto ptrDynamicType = MemRefType::get({ShapedType::kDynamic}, AC->llvmPtr);
    auto guidHolderType = MemRefType::get({1}, guidDynamicType);
    auto ptrHolderType = MemRefType::get({1}, ptrDynamicType);
    getOrCreateMemrefGlobal(guidHolderSymbol, guidHolderType, op.getLoc());
    getOrCreateMemrefGlobal(ptrHolderSymbol, ptrHolderType, op.getLoc());

    ModuleOp module = AC->getModule();
    func::FuncOp initFn = module.lookupSymbol<func::FuncOp>(initSymbol);
    if (!initFn) {
      OpBuilder::InsertionGuard IG(AC->getBuilder());
      AC->setInsertionPoint(module);
      initFn =
          AC->create<func::FuncOp>(op.getLoc(), initSymbol, AC->InitPerNodeFn);
      initFn.setPrivate();

      Block *entry = initFn.addEntryBlock();
      AC->setInsertionPointToStart(entry);

      IRMapping mapper;
      auto parentFn = op->getParentOfType<func::FuncOp>();
      if (parentFn && parentFn.getNumArguments() > 0) {
        if (parentFn.getNumArguments() != 2 || initFn.getNumArguments() != 3)
          return failure();
        mapper.map(parentFn.getArgument(0), initFn.getArgument(1));
        mapper.map(parentFn.getArgument(1), initFn.getArgument(2));
      }

      llvm::SetVector<Value> valuesToClone;
      llvm::DenseSet<Value> visited;
      std::function<LogicalResult(Value)> collectDependencies =
          [&](Value value) -> LogicalResult {
        if (!value)
          return failure();
        if (!visited.insert(value).second || mapper.contains(value))
          return success();

        if (auto blockArg = value.dyn_cast<BlockArgument>())
          return mapper.contains(blockArg) ? success() : failure();

        Operation *defOp = value.getDefiningOp();
        if (!defOp)
          return failure();
        for (Value operand : defOp->getOperands()) {
          if (failed(collectDependencies(operand)))
            return failure();
        }
        valuesToClone.insert(value);
        return success();
      };

      for (Value size : dbSizes) {
        if (failed(collectDependencies(size))) {
          ARTS_WARN("Failed to collect distributed allocation size expression "
                    "dependencies for "
                    << op);
          return failure();
        }
      }
      for (Value size : op.getElementSizes()) {
        if (failed(collectDependencies(size))) {
          ARTS_WARN("Failed to collect distributed allocation element size "
                    "expression dependencies for "
                    << op);
          return failure();
        }
      }

      auto isRuntimeTopologyCall = [](Operation *cloneOp) {
        if (isa<GetTotalNodesOp, GetTotalWorkersOp>(cloneOp))
          return true;
        auto callOp = dyn_cast<func::CallOp>(cloneOp);
        if (!callOp)
          return false;
        auto callee = callOp.getCallee();
        return callee == "artsGetTotalNodes" || callee == "artsGetTotalWorkers";
      };

      if (!ValueUtils::cloneValuesIntoRegion(
              valuesToClone, &initFn.getBody(), mapper, AC->getBuilder(),
              /*allowMemoryEffectFree=*/true, isRuntimeTopologyCall)) {
        ARTS_WARN(
            "Failed to clone distributed allocation size expressions into "
            "init callback for "
            << op);
        return failure();
      }

      auto mapValue = [&](Value v) { return mapper.lookupOrNull(v); };

      SmallVector<Value, 4> callbackDbSizes;
      callbackDbSizes.reserve(dbSizes.size());
      for (Value size : dbSizes) {
        Value mapped = mapValue(size);
        if (!mapped) {
          ARTS_WARN("Missing mapped DbAlloc size value in distributed init for "
                    << op);
          return failure();
        }
        callbackDbSizes.push_back(mapped);
      }

      SmallVector<Value, 4> callbackElementSizes;
      callbackElementSizes.reserve(op.getElementSizes().size());
      for (Value size : op.getElementSizes()) {
        Value mapped = mapValue(size);
        if (!mapped) {
          ARTS_WARN("Missing mapped DbAlloc element size value in distributed "
                    "init for "
                    << op);
          return failure();
        }
        callbackElementSizes.push_back(mapped);
      }

      Value callbackTotalElems =
          AC->computeTotalElements(callbackDbSizes, op.getLoc());
      Value guidBuffer = AC->create<memref::AllocOp>(
          op.getLoc(), guidDynamicType, ValueRange{callbackTotalElems});
      Value ptrBuffer = AC->create<memref::AllocOp>(
          op.getLoc(), ptrDynamicType, ValueRange{callbackTotalElems});

      Value guidHolder = AC->create<memref::GetGlobalOp>(
          op.getLoc(), guidHolderType, guidHolderSymbol);
      Value ptrHolder = AC->create<memref::GetGlobalOp>(
          op.getLoc(), ptrHolderType, ptrHolderSymbol);
      Value zero = AC->createIndexConstant(0, op.getLoc());
      AC->create<memref::StoreOp>(op.getLoc(), guidBuffer, guidHolder,
                                  ValueRange{zero});
      AC->create<memref::StoreOp>(op.getLoc(), ptrBuffer, ptrHolder,
                                  ValueRange{zero});

      Value callbackElementSize =
          AC->computeElementTypeSize(op.getElementType(), op.getLoc());
      Value callbackPayloadSize = AC->createIndexConstant(1, op.getLoc());
      for (Value dim : callbackElementSizes) {
        callbackPayloadSize =
            AC->create<arith::MulIOp>(op.getLoc(), callbackPayloadSize, dim);
      }
      Value callbackTotalDbSize = AC->create<arith::MulIOp>(
          op.getLoc(), callbackElementSize, callbackPayloadSize);
      Value callbackDbMode =
          AC->createIntConstant(static_cast<int>(op.getDbModeAttr().getValue()),
                                AC->Int32, op.getLoc());
      Value callbackRoute = AC->createIntConstant(0, AC->Int32, op.getLoc());
      std::optional<int64_t> callbackNextId = nextId;

      if (isSingleElement) {
        createSingleDb(ptrBuffer, guidBuffer, callbackDbMode, callbackRoute,
                       callbackTotalDbSize,
                       callbackNextId ? &callbackNextId : nullptr, op.getLoc(),
                       /*distributedOwnership=*/true);
      } else {
        createMultiDbs(ptrBuffer, guidBuffer, callbackDbSizes, callbackDbMode,
                       callbackRoute, callbackTotalDbSize,
                       callbackNextId ? &callbackNextId : nullptr, op.getLoc(),
                       /*distributedOwnership=*/true);
      }

      AC->create<func::ReturnOp>(op.getLoc());
    }

    AC->registerDistributedInitCallback(initFn);

    Value zero = AC->createIndexConstant(0, op.getLoc());
    Value guidHolder = AC->create<memref::GetGlobalOp>(
        op.getLoc(), guidHolderType, guidHolderSymbol);
    Value ptrHolder = AC->create<memref::GetGlobalOp>(
        op.getLoc(), ptrHolderType, ptrHolderSymbol);
    guidMemref =
        AC->create<memref::LoadOp>(op.getLoc(), guidHolder, ValueRange{zero});
    dbMemref =
        AC->create<memref::LoadOp>(op.getLoc(), ptrHolder, ValueRange{zero});
    return success();
  }

  void createSingleDb(Value dbMemref, Value guidMemref, Value dbMode,
                      Value route, Value elementSize,
                      std::optional<int64_t> *nextId, Location loc,
                      bool distributedOwnership = false,
                      ArrayRef<Value> sizes = {},
                      ArrayRef<Value> indices = {}) const {
    Value reserveRoute = route;
    if (distributedOwnership) {
      Value linearIndex = indices.empty()
                              ? AC->createIndexConstant(0, loc)
                              : AC->computeLinearIndex(sizes, indices, loc);
      Value linearIndexI32 = AC->castToInt(AC->Int32, linearIndex, loc);
      Value totalNodes = AC->getTotalNodes(loc);
      reserveRoute =
          AC->create<arith::RemUIOp>(loc, linearIndexI32, totalNodes);
    }

    /// Reserve GUID for the DB
    auto guid = AC->createRuntimeCall(types::ARTSRTL_artsReserveGuidRoute,
                                      {dbMode, reserveRoute}, loc)
                    .getResult(0);
    /// Create the DB with GUID
    auto elemSize64 = AC->castToInt(AC->Int64, elementSize, loc);
    func::CallOp dbCall;
    if (nextId && nextId->has_value()) {
      auto baseArtsIdAttr = AC->getBuilder().getI64IntegerAttr(**nextId);
      auto baseArtsId =
          AC->create<arith::ConstantOp>(loc, AC->Int64, baseArtsIdAttr);

      Value artsIdValue;
      if (!indices.empty()) {
        /// For multi-DB case: compute runtime arts_id = base + linearIndex
        /// This ensures each DB in the loop gets a unique ID
        auto linearIndex = AC->computeLinearIndex(sizes, indices, loc);
        auto linearIndex64 = AC->castToInt(AC->Int64, linearIndex, loc);
        artsIdValue = AC->create<arith::AddIOp>(loc, baseArtsId, linearIndex64);
      } else {
        /// Single DB case: use base directly
        artsIdValue = baseArtsId;
        **nextId = **nextId + 1;
      }

      dbCall =
          AC->createRuntimeCall(types::ARTSRTL_artsDbCreateWithGuidAndArtsId,
                                {guid, elemSize64, artsIdValue}, loc);
    } else {
      dbCall = AC->createRuntimeCall(types::ARTSRTL_artsDbCreateWithGuid,
                                     {guid, elemSize64}, loc);
    }
    auto dbPtr = dbCall.getResult(0);

    /// Store the DB pointer and GUID in the linearized memrefs
    if (indices.empty()) {
      auto zeroIndex = AC->createIndexConstant(0, loc);
      AC->create<memref::StoreOp>(loc, dbPtr, dbMemref, ValueRange{zeroIndex});
      AC->create<memref::StoreOp>(loc, guid, guidMemref, ValueRange{zeroIndex});
    } else {
      auto linearIndex = AC->computeLinearIndex(sizes, indices, loc);
      AC->create<memref::StoreOp>(loc, guid, guidMemref,
                                  ValueRange{linearIndex});
      AC->create<memref::StoreOp>(loc, dbPtr, dbMemref,
                                  ValueRange{linearIndex});
    }
  }

  void createMultiDbs(Value dbMemref, Value guidMemref, ArrayRef<Value> sizes,
                      Value dbMode, Value route, Value elementSize,
                      std::optional<int64_t> *nextId, Location loc,
                      bool distributedOwnership = false) const {
    const unsigned rank = sizes.size();
    auto stepConstant = AC->createIndexConstant(1, loc);

    std::function<void(unsigned, SmallVector<Value, 4> &)> createDbsRecursive =
        [&](unsigned dim, SmallVector<Value, 4> &indices) {
          if (dim == rank) {
            createSingleDb(dbMemref, guidMemref, dbMode, route, elementSize,
                           nextId, loc, distributedOwnership, sizes, indices);
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
    auto loc = op.getLoc();

    Value sourceGuid = op.getSourceGuid();
    Value sourcePtr = op.getSourcePtr();

    /// Get source sizes from the defining op
    auto sourceOp = sourceGuid.getDefiningOp();
    SmallVector<Value> sourceSizes;
    if (auto allocOp = dyn_cast_or_null<DbAllocOp>(sourceOp))
      sourceSizes.assign(allocOp.getSizes().begin(), allocOp.getSizes().end());
    else if (auto acqOp = dyn_cast_or_null<DbAcquireOp>(sourceOp))
      sourceSizes.assign(acqOp.getSizes().begin(), acqOp.getSizes().end());

    if (!sourceSizes.empty()) {
      SmallVector<Value> indices(op.getIndices().begin(),
                                 op.getIndices().end());
      SmallVector<Value> strides =
          AC->computeStridesFromSizes(sourceSizes, loc);
      /// Guid llvm type - underlying storage is always linear (rank-1)
      /// regardless of DbAcquireOp's multi-dimensional sizes
      auto origGuidMT = op.getGuid().getType().dyn_cast<MemRefType>();
      auto llvmGuidType = LLVM::LLVMPointerType::get(
          AC->getContext(), origGuidMT.getMemorySpaceAsInt());
      auto loadedGuid =
          AC->create<DbGepOp>(loc, llvmGuidType, sourceGuid, indices, strides);
      /// Ptr llvm type - same: underlying storage is linear
      auto origPtrMT = op.getPtr().getType().dyn_cast<MemRefType>();
      auto llvmPtrType = LLVM::LLVMPointerType::get(
          AC->getContext(), origPtrMT.getMemorySpaceAsInt());
      auto loadedPtr =
          AC->create<DbGepOp>(loc, llvmPtrType, sourcePtr, indices, strides);
      /// Convert to memref type - use rank-1 linear memrefs since underlying
      /// storage from DbAllocOp is always linearized. The multi-dimensional
      /// sizes in DbAcquireOp are for logical indexing, not physical layout.
      auto guidMT = MemRefType::get({ShapedType::kDynamic}, AC->Int64);
      auto ptrMT = MemRefType::get({ShapedType::kDynamic}, AC->llvmPtr);
      auto guidMemref =
          AC->create<polygeist::Pointer2MemrefOp>(loc, guidMT, loadedGuid);
      auto ptrMemref =
          AC->create<polygeist::Pointer2MemrefOp>(loc, ptrMT, loadedPtr);
      rewriter.replaceOp(op, ValueRange{guidMemref, ptrMemref});
    } else {
      /// Fallback: just forward the source values
      rewriter.replaceOp(op, ValueRange{sourceGuid, sourcePtr});
    }

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
    Value source = op.getSource();
    Operation *rootOp = ValueUtils::getUnderlyingOperation(source);
    if (!isa_and_nonnull<memref::GetGlobalOp>(rootOp))
      AC->create<memref::DeallocOp>(op.getLoc(), source);
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

    /// If no sizes are provided, return constant 1 (index)
    if (sizes.empty()) {
      rewriter.replaceOp(op, AC->createIndexConstant(1, loc));
      return success();
    }

    /// If all sizes are constants, fold to a single index constant.
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
      rewriter.replaceOp(op, AC->createIndexConstant(folded, loc));
      return success();
    }

    /// Otherwise, compute product at runtime as index.
    Value productVal = AC->createIndexConstant(1, loc);
    for (Value sz : sizes) {
      Value szIndex = AC->castToIndex(sz, loc);
      productVal = AC->create<arith::MulIOp>(loc, productVal, szIndex);
    }

    rewriter.replaceOp(op, productVal);
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

///===----------------------------------------------------------------------===///
// Terminator Patterns
///===----------------------------------------------------------------------===///

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

/// Pattern to convert arts.undef to polygeist.undef
struct UndefPattern : public ArtsToLLVMPattern<UndefOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(UndefOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering Undef Op " << op);
    Type resultType = op.getResult().getType();
    rewriter.replaceOpWithNewOp<polygeist::UndefOp>(op, resultType);
    return success();
  }
};

/// Pattern to lower gpu.thread_id to NVVM intrinsic
struct ThreadIdOpLowering : public OpRewritePattern<gpu::ThreadIdOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(gpu::ThreadIdOp op,
                                PatternRewriter &rewriter) const override {
    Location loc = op.getLoc();
    auto i32Type = rewriter.getI32Type();
    Value result;

    switch (op.getDimension()) {
    case gpu::Dimension::x:
      result = rewriter.create<NVVM::ThreadIdXOp>(loc, i32Type);
      break;
    case gpu::Dimension::y:
      result = rewriter.create<NVVM::ThreadIdYOp>(loc, i32Type);
      break;
    case gpu::Dimension::z:
      result = rewriter.create<NVVM::ThreadIdZOp>(loc, i32Type);
      break;
    default:
      return failure();
    }

    if (op.getType().isIndex())
      result = rewriter.create<arith::IndexCastOp>(loc, op.getType(), result);
    rewriter.replaceOp(op, result);
    return success();
  }
};

/// Pattern to lower gpu.block_id to NVVM intrinsic
struct BlockIdOpLowering : public OpRewritePattern<gpu::BlockIdOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(gpu::BlockIdOp op,
                                PatternRewriter &rewriter) const override {
    Location loc = op.getLoc();
    auto i32Type = rewriter.getI32Type();
    Value result;

    switch (op.getDimension()) {
    case gpu::Dimension::x:
      result = rewriter.create<NVVM::BlockIdXOp>(loc, i32Type);
      break;
    case gpu::Dimension::y:
      result = rewriter.create<NVVM::BlockIdYOp>(loc, i32Type);
      break;
    case gpu::Dimension::z:
      result = rewriter.create<NVVM::BlockIdZOp>(loc, i32Type);
      break;
    default:
      return failure();
    }

    if (op.getType().isIndex())
      result = rewriter.create<arith::IndexCastOp>(loc, op.getType(), result);
    rewriter.replaceOp(op, result);
    return success();
  }
};

/// Pattern to lower gpu.block_dim to NVVM intrinsic
struct BlockDimOpLowering : public OpRewritePattern<gpu::BlockDimOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(gpu::BlockDimOp op,
                                PatternRewriter &rewriter) const override {
    Location loc = op.getLoc();
    auto i32Type = rewriter.getI32Type();
    Value result;

    switch (op.getDimension()) {
    case gpu::Dimension::x:
      result = rewriter.create<NVVM::BlockDimXOp>(loc, i32Type);
      break;
    case gpu::Dimension::y:
      result = rewriter.create<NVVM::BlockDimYOp>(loc, i32Type);
      break;
    case gpu::Dimension::z:
      result = rewriter.create<NVVM::BlockDimZOp>(loc, i32Type);
      break;
    default:
      return failure();
    }

    if (op.getType().isIndex())
      result = rewriter.create<arith::IndexCastOp>(loc, op.getType(), result);
    rewriter.replaceOp(op, result);
    return success();
  }
};

/// Pattern to lower gpu.grid_dim to NVVM intrinsic
struct GridDimOpLowering : public OpRewritePattern<gpu::GridDimOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(gpu::GridDimOp op,
                                PatternRewriter &rewriter) const override {
    Location loc = op.getLoc();
    auto i32Type = rewriter.getI32Type();
    Value result;

    switch (op.getDimension()) {
    case gpu::Dimension::x:
      result = rewriter.create<NVVM::GridDimXOp>(loc, i32Type);
      break;
    case gpu::Dimension::y:
      result = rewriter.create<NVVM::GridDimYOp>(loc, i32Type);
      break;
    case gpu::Dimension::z:
      result = rewriter.create<NVVM::GridDimZOp>(loc, i32Type);
      break;
    default:
      return failure();
    }

    if (op.getType().isIndex())
      result = rewriter.create<arith::IndexCastOp>(loc, op.getType(), result);
    rewriter.replaceOp(op, result);
    return success();
  }
};

///===----------------------------------------------------------------------===///
// Pass Implementation
///===----------------------------------------------------------------------===///
namespace {
struct ConvertArtsToLLVMPass
    : public arts::ConvertArtsToLLVMBase<ConvertArtsToLLVMPass> {

  explicit ConvertArtsToLLVMPass(bool debug = false) : debugMode(debug) {}

  void runOnOperation() override;

private:
  void populateCorePatterns(RewritePatternSet &patterns);
  void populateDbPatterns(RewritePatternSet &patterns);

  //// Member variables
  ArtsCodegen *AC = nullptr;
  bool debugMode = false;
};
} // namespace

///===----------------------------------------------------------------------===///
// Core Pass Implementation
///===----------------------------------------------------------------------===///

void ConvertArtsToLLVMPass::runOnOperation() {
  ModuleOp module = getOperation();
  MLIRContext *context = &getContext();

  ARTS_INFO_HEADER(ConvertArtsToLLVMPass);
  ARTS_DEBUG_REGION(module.dump(););

  //// Initialize codegen infrastructure
  auto ownedAC = std::make_unique<ArtsCodegen>(module, debugMode);
  AC = ownedAC.get();
  ARTS_DEBUG_TYPE("ArtsCodegen initialized successfully");

  //// Apply patterns with greedy rewriter (two runs)
  GreedyRewriteConfig config;
  config.useTopDownTraversal = true;
  config.enableRegionSimplification = true;

  /// Run 1: core patterns
  {
    ARTS_INFO("Running core patterns");
    RewritePatternSet corePatterns(context);
    populateCorePatterns(corePatterns);
    if (failed(applyPatternsAndFoldGreedily(module, std::move(corePatterns),
                                            config))) {
      ARTS_ERROR("Failed to apply core ARTS to LLVM conversion patterns");
      return signalPassFailure();
    }
  }

  /// Run 2: Db Patterns
  {
    ARTS_INFO("Running db patterns");
    RewritePatternSet dbPatterns(context);
    populateDbPatterns(dbPatterns);
    if (failed(applyPatternsAndFoldGreedily(module, std::move(dbPatterns),
                                            config))) {
      ARTS_ERROR("Failed to apply DbAcquire/DbRelease conversion patterns");
      return signalPassFailure();
    }
  }

  /// Run 3: Other Patterns
  {
    ARTS_INFO("Running other patterns");
    RewritePatternSet otherPatterns(context);
    otherPatterns.add<DbAllocPattern, DbFreePattern>(context, AC);
    otherPatterns.add<DbNumElementsPattern>(context, AC);
    otherPatterns.add<YieldPattern, UndefPattern>(context, AC);
    if (failed(applyPatternsAndFoldGreedily(module, std::move(otherPatterns),
                                            config))) {
      ARTS_ERROR("Failed to apply other conversion patterns");
      return signalPassFailure();
    }
  }
  //// Initialize runtime
  AC->initRT(AC->getUnknownLoc());

  //// Cleanup
  AC = nullptr;

  ARTS_INFO_FOOTER(ConvertArtsToLLVMPass);
  ARTS_DEBUG_REGION(module.dump(););
}

void ConvertArtsToLLVMPass::populateCorePatterns(RewritePatternSet &patterns) {
  MLIRContext *context = patterns.getContext();

  /// Runtime helper patterns
  patterns.add<GetTotalWorkersPattern, GetTotalNodesPattern,
               GetCurrentWorkerPattern, GetCurrentNodePattern>(context, AC);

  /// Synchronization patterns
  patterns.add<BarrierPattern, AtomicAddPattern>(context, AC);

  /// Builtin patterns (Polygeist emits these as calls, not intrinsics)
  patterns.add<BuiltinObjectSizePattern>(context);

  /// Epoch patterns
  patterns.add<CreateEpochPattern, WaitOnEpochPattern>(context, AC);

  /// EDT patterns
  patterns.add<EdtParamPackPattern, EdtParamUnpackPattern>(context, AC);
  patterns.add<EdtCreatePattern>(context, AC);
  patterns.add<GpuMemcpyPattern, LcSyncPattern>(context, AC);

  /// Dependency patterns
  patterns.add<DepGepOpPattern>(context, AC);
  patterns.add<RecordDepPattern, IncrementDepPattern>(context, AC);

  /// GPU index lowering patterns (GPU dialect -> NVVM)
  patterns.add<ThreadIdOpLowering, BlockIdOpLowering, BlockDimOpLowering,
               GridDimOpLowering>(context);
}

void ConvertArtsToLLVMPass::populateDbPatterns(RewritePatternSet &patterns) {
  MLIRContext *context = patterns.getContext();
  /// DB patterns
  patterns.add<DbControlPattern>(context, AC);
  patterns.add<DbAcquirePattern, DbReleasePattern>(context, AC);
  patterns.add<DbGepOpPattern, DepDbAcquireOpPattern>(context, AC);
}

///===----------------------------------------------------------------------===///
// Pass Functions
///===----------------------------------------------------------------------===///
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
