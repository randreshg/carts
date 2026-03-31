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
///     %g = call @arts_edt_create(...)
///     call @arts_add_dependence(...)
///     call @arts_wait_on_handle(...)
///==========================================================================///

/// Dialects
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Value.h"
#include "polygeist/Ops.h"
/// Arts
#include "arts/Dialect.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/codegen/Codegen.h"
#define GEN_PASS_DEF_CONVERTARTSTOLLVM
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/utils/DbUtils.h"
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "arts/utils/Utils.h"
#include "arts/utils/abstract_machine/AbstractMachine.h"
#include "mlir/Pass/Pass.h"
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
#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(convert_arts_to_llvm);

using namespace mlir;
using namespace arts;

///===----------------------------------------------------------------------===///
/// Conversion Patterns
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

namespace {
static inline DbAllocOp getAllocOpFromGuid(Value dbGuid) {
  if (auto dbAcquireOp = dbGuid.getDefiningOp<DbAcquireOp>())
    return dbAcquireOp.getSourceGuid().getDefiningOp<DbAllocOp>();
  if (auto depDbAcquireOp = dbGuid.getDefiningOp<DepDbAcquireOp>())
    return depDbAcquireOp.getGuid().getDefiningOp<DbAllocOp>();
  return nullptr;
}

static SmallVector<Value, 4>
materializeStaticDbOuterShape(Value handle, ArtsCodegen *AC, Location loc) {
  SmallVector<Value, 4> sizes;
  auto shape = getDbStaticOuterShape(handle);
  if (!shape)
    return sizes;

  sizes.reserve(shape->size());
  for (int64_t dim : *shape)
    sizes.push_back(AC->createIndexConstant(dim, loc));
  return sizes;
}

static SmallVector<Value, 4> resolveSourceOuterSizes(Value sourceGuid,
                                                     Value sourcePtr,
                                                     ArtsCodegen *AC,
                                                     Location loc) {
  SmallVector<Value, 4> sizes;

  if (auto allocOp = dyn_cast_or_null<DbAllocOp>(sourceGuid.getDefiningOp()))
    sizes.assign(allocOp.getSizes().begin(), allocOp.getSizes().end());
  else if (auto acqOp =
               dyn_cast_or_null<DbAcquireOp>(sourceGuid.getDefiningOp()))
    sizes.assign(acqOp.getSizes().begin(), acqOp.getSizes().end());

  if (sizes.empty())
    sizes = materializeStaticDbOuterShape(sourceGuid, AC, loc);
  if (sizes.empty())
    sizes = materializeStaticDbOuterShape(sourcePtr, AC, loc);
  return sizes;
}

static SmallVector<Value, 4>
resolveOuterSizesForGuid(Value dbGuid, ArtsCodegen *AC, Location loc) {
  SmallVector<Value, 4> sizes;

  if (auto allocOp = getAllocOpFromGuid(dbGuid)) {
    sizes.assign(allocOp.getSizes().begin(), allocOp.getSizes().end());
    return sizes;
  }

  if (auto dbAcquireOp = dbGuid.getDefiningOp<DbAcquireOp>())
    return resolveSourceOuterSizes(dbAcquireOp.getSourceGuid(),
                                   dbAcquireOp.getSourcePtr(), AC, loc);

  if (auto depDbAcquireOp = dbGuid.getDefiningOp<DepDbAcquireOp>())
    return resolveSourceOuterSizes(depDbAcquireOp.getGuid(),
                                   depDbAcquireOp.getPtr(), AC, loc);

  return sizes;
}

static constexpr int32_t kArtsDepFlagPreserveShape = 1 << 1;

} // namespace

/// Pattern to convert arts.runtime_query operations to LLVM runtime calls.
/// Dispatches on the RuntimeQueryKind attribute to call the appropriate
/// ARTS runtime function (getTotalWorkers, getTotalNodes, etc.).
struct RuntimeQueryPattern : public ArtsToLLVMPattern<RuntimeQueryOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(RuntimeQueryOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering RuntimeQuery Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    Value result;
    switch (op.getKind()) {
    case RuntimeQueryKind::totalWorkers:
      result = AC->getTotalWorkers(op.getLoc());
      break;
    case RuntimeQueryKind::totalNodes:
      result = AC->getTotalNodes(op.getLoc());
      break;
    case RuntimeQueryKind::currentWorker:
      result = AC->getCurrentWorker(op.getLoc());
      break;
    case RuntimeQueryKind::currentNode:
      result = AC->getCurrentNode(op.getLoc());
      break;
    case RuntimeQueryKind::parallelWorkerId:
      return op.emitOpError("parallel_worker_id should have been replaced "
                            "by ParallelEdtLowering before LLVM lowering");
    }
    rewriter.replaceOp(op, result);
    return success();
  }
};

///===----------------------------------------------------------------------===///
/// Event and Barrier Patterns
///===----------------------------------------------------------------------===///

/// Pattern to convert arts.barrier operations
struct BarrierPattern : public ArtsToLLVMPattern<BarrierOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(BarrierOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering Barrier Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    ArtsCodegen::RuntimeCallBuilder RCB(*AC, op.getLoc());
    RCB.callVoid(types::ARTSRTL_arts_yield, {});
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to convert arts.get_edt_epoch_guid operations
struct GetEdtEpochGuidPattern : public ArtsToLLVMPattern<GetEdtEpochGuidOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(GetEdtEpochGuidOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering GetEdtEpochGuid Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    Value result = AC->getEdtEpochGuid(op.getLoc());
    rewriter.replaceOp(op, result);
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

    Value guid, edtSlot;
    if (op.hasFinishTarget()) {
      guid = op.getFinishEdtGuid();
      edtSlot = op.getFinishSlot();
    } else {
      guid = AC->createIntConstant(0, AC->Int64, loc);
      edtSlot = AC->createIntConstant(DEFAULT_EDT_SLOT, AC->Int32, loc);
    }

    /// Create epoch guid
    Value epochGuid;
    if (op->hasAttr(AttrNames::Operation::NoStartEpoch))
      epochGuid = AC->createEpochNoStart(guid, edtSlot, loc);
    else
      epochGuid = AC->createEpoch(guid, edtSlot, loc);
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
    Value llvmPtr = polygeist::Memref2PointerOp::create(
        rewriter, loc, LLVM::LLVMPointerType::get(rewriter.getContext()), addr);

    /// Create LLVM atomic add operation
    LLVM::AtomicRMWOp::create(rewriter, loc, LLVM::AtomicBinOp::add, llvmPtr,
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
///   - null_is_unknown: Always true to match GCC semantics
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
    Value ptr = polygeist::Memref2PointerOp::create(
        rewriter, loc, LLVM::LLVMPointerType::get(rewriter.getContext()),
        memrefArg);

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
    Value min = LLVM::ConstantOp::create(rewriter, loc, i1Type, minFlag);
    Value nullIsUnknown = LLVM::ConstantOp::create(rewriter, loc, i1Type, true);
    Value dynamic = LLVM::ConstantOp::create(rewriter, loc, i1Type, false);

    /// Create llvm.call_intrinsic for llvm.objectsize
    Type resultType = callOp.getResult(0).getType();
    auto intrinsicOp = LLVM::CallIntrinsicOp::create(
        rewriter, loc, resultType,
        StringAttr::get(rewriter.getContext(), "llvm.objectsize"),
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
    auto depFlagsAttr = op.getDepFlags();
    ArrayRef<int32_t> depFlagValues =
        depFlagsAttr ? *depFlagsAttr : ArrayRef<int32_t>{};

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
      std::optional<int32_t> depFlags = std::nullopt;
      if (dbIdx < depFlagValues.size())
        depFlags = depFlagValues[dbIdx];
      /// Get bounds_valid for this dependency if available
      Value boundsValid =
          (dbIdx < boundsValids.size()) ? boundsValids[dbIdx] : Value();
      /// Get ESD byte offset/size for this dependency if available
      Value byteOffset =
          (dbIdx < byteOffsets.size()) ? byteOffsets[dbIdx] : Value();
      Value byteSize = (dbIdx < byteSizes.size()) ? byteSizes[dbIdx] : Value();
      recordDepsForDb(dbGuid, edtGuid, sharedSlotAlloc, accessMode, acquireMode,
                      depFlags, boundsValid, byteOffset, byteSize, loc);
      ++dbIdx;
    }

    rewriter.eraseOp(op);
    return success();
  }

private:
  SmallVector<Value, 4>
  inferStencilCenterCoordsFromContract(DbAcquireOp dbAcquireOp,
                                       const DbLoweringInfo &dbInfo,
                                       Location loc) const {
    SmallVector<Value, 4> globalCoords;
    if (!dbAcquireOp || dbInfo.sizes.empty())
      return globalCoords;

    auto contract = getAcquireStencilContract(dbAcquireOp, loc);
    if (!contract || !contract->isStencilFamily() ||
        contract->minOffsets.empty())
      return globalCoords;

    unsigned rank =
        std::min<unsigned>(dbInfo.sizes.size(), contract->minOffsets.size());
    if (!contract->writeFootprint.empty())
      rank = std::min<unsigned>(rank, contract->writeFootprint.size());
    if (rank == 0)
      return globalCoords;

    Value zero = AC->createIndexConstant(0, loc);
    Value one = AC->createIndexConstant(1, loc);

    globalCoords.reserve(rank);
    for (unsigned i = 0; i < rank; ++i) {
      Value writeCoord =
          contract->writeFootprint.empty() ? zero : contract->writeFootprint[i];
      Value minOffset = contract->minOffsets[i];
      Value dimSize = AC->castToIndex(dbInfo.sizes[i], loc);

      Value rawCoord =
          AC->create<arith::SubIOp>(loc, AC->castToIndex(writeCoord, loc),
                                    AC->castToIndex(minOffset, loc));
      Value nonNegative = AC->create<arith::MaxSIOp>(loc, rawCoord, zero);
      Value sizeMinusOne = AC->create<arith::SubIOp>(loc, dimSize, one);
      sizeMinusOne = AC->create<arith::MaxSIOp>(loc, sizeMinusOne, zero);
      Value localCoord =
          AC->create<arith::MinSIOp>(loc, nonNegative, sizeMinusOne);
      Value globalCoord = AC->create<arith::AddIOp>(
          loc, AC->castToIndex(dbInfo.offsets[i], loc), localCoord);
      globalCoords.push_back(globalCoord);
    }

    return globalCoords;
  }

  SmallVector<Value, 4> inferSymmetricStencilCenterCoords(
      int64_t centerOffset, const DbLoweringInfo &dbInfo, Location loc) const {
    SmallVector<Value, 4> globalCoords;
    if (centerOffset < 0 || dbInfo.sizes.empty())
      return globalCoords;

    Value zero = AC->createIndexConstant(0, loc);
    Value one = AC->createIndexConstant(1, loc);
    Value center = AC->createIndexConstant(centerOffset, loc);

    globalCoords.reserve(dbInfo.sizes.size());
    for (unsigned i = 0; i < dbInfo.sizes.size(); ++i) {
      Value dimSize = AC->castToIndex(dbInfo.sizes[i], loc);
      Value sizeMinusOne = AC->create<arith::SubIOp>(loc, dimSize, one);
      sizeMinusOne = AC->create<arith::MaxSIOp>(loc, sizeMinusOne, zero);
      Value localCoord = AC->create<arith::MinSIOp>(loc, center, sizeMinusOne);
      Value globalCoord = AC->create<arith::AddIOp>(
          loc, AC->castToIndex(dbInfo.offsets[i], loc), localCoord);
      globalCoords.push_back(globalCoord);
    }

    return globalCoords;
  }

  Value buildCoordsEqual(ArrayRef<Value> lhs, ArrayRef<Value> rhs,
                         Location loc) const {
    assert(lhs.size() == rhs.size() && "coordinate rank mismatch");
    Value allEqual = AC->create<arith::ConstantIntOp>(loc, 1, 1);
    for (auto [lhsCoord, rhsCoord] : llvm::zip(lhs, rhs)) {
      Value lhsIdx = AC->castToIndex(lhsCoord, loc);
      Value rhsIdx = AC->castToIndex(rhsCoord, loc);
      Value dimEqual = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::eq,
                                                 lhsIdx, rhsIdx);
      allEqual = AC->create<arith::AndIOp>(loc, allEqual, dimEqual);
    }
    return allEqual;
  }

  SmallVector<Value, 4>
  getDirectLookupStrides(Value storage, ArrayRef<Value> directIndices,
                         ArrayRef<Value> directLayoutSizes,
                         Location loc) const {
    SmallVector<Value, 4> strideSizes;
    strideSizes.reserve(directIndices.size());

    if (directLayoutSizes.size() == directIndices.size()) {
      strideSizes.append(directLayoutSizes.begin(), directLayoutSizes.end());
    } else if (auto storageTy = dyn_cast<MemRefType>(storage.getType())) {
      if (storageTy.getRank() == static_cast<int64_t>(directIndices.size())) {
        for (unsigned i = 0; i < directIndices.size(); ++i) {
          if (storageTy.isDynamicDim(i)) {
            Value dimIdx = AC->createIndexConstant(i, loc);
            strideSizes.push_back(
                AC->create<memref::DimOp>(loc, storage, dimIdx));
          } else {
            strideSizes.push_back(
                AC->createIndexConstant(storageTy.getDimSize(i), loc));
          }
        }
      }
    }

    if (strideSizes.size() != directIndices.size())
      return {};
    return SmallVector<Value, 4>(AC->computeStridesFromSizes(strideSizes, loc));
  }

  /// Load the GUID value for a dependency, handling both direct and depv paths.
  Value loadDbGuidValue(Value dbGuid, Value guidStorage, Value linearIndex,
                        ArrayRef<Value> directIndices,
                        ArrayRef<Value> directLayoutSizes, bool useDepv,
                        Value depStruct, Value baseOffset, Location loc) const {
    if (useDepv) {
      Value finalOffset =
          AC->create<arith::AddIOp>(loc, baseOffset, linearIndex);
      auto depGep =
          AC->create<DepGepOp>(loc, AC->llvmPtr, AC->llvmPtr, depStruct,
                               finalOffset, ValueRange(), ValueRange());
      return AC->create<LLVM::LoadOp>(loc, AC->Int64, depGep.getGuid());
    }
    Value storage = guidStorage ? guidStorage : dbGuid;
    auto storageTy = dyn_cast<MemRefType>(storage.getType());
    auto llvmGuidType = LLVM::LLVMPointerType::get(
        AC->getContext(), storageTy ? storageTy.getMemorySpaceAsInt() : 0);

    SmallVector<Value, 4> gepIndices;
    SmallVector<Value, 4> gepStrides;
    if (!directIndices.empty()) {
      gepIndices.append(directIndices.begin(), directIndices.end());
      gepStrides = getDirectLookupStrides(storage, directIndices,
                                          directLayoutSizes, loc);
    } else {
      gepIndices.push_back(AC->castToIndex(linearIndex, loc));
    }

    auto guidAddr =
        AC->create<DbGepOp>(loc, llvmGuidType, storage, gepIndices, gepStrides);
    return AC->create<LLVM::LoadOp>(loc, AC->Int64, guidAddr);
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
    SmallVector<Value, 4> sizes =
        resolveOuterSizesForGuid(dbGuid, AC, dbGuid.getLoc());
    if (!sizes.empty()) {
      Value totalDBs = sizes[0];
      for (size_t i = 1; i < sizes.size(); ++i)
        totalDBs =
            AC->create<arith::MulIOp>(dbGuid.getLoc(), totalDBs, sizes[i]);
      return totalDBs;
    }
    return nullptr;
  }

  /// Holds extracted dependency info for a single datablock source.
  struct DepDbInfo {
    DbLoweringInfo dbInfo;
    SmallVector<Value, 4> allocSizes;
    Value guidStorage = nullptr;
    Value depStruct = nullptr;
    Value baseOffset = nullptr;
    Value stencilCenterLinear;
    SmallVector<Value, 4> stencilCenterCoords;
    std::optional<LoweringContractInfo> stencilContract;
    SmallVector<unsigned, 4> blockOwnerDims;
    SmallVector<Value, 4> blockElementSizes;
    Value scalarSize = nullptr;
  };

  static bool isUnitExtent(Value extent) {
    auto constant = ValueAnalysis::tryFoldConstantIndex(
        ValueAnalysis::stripNumericCasts(extent));
    return constant && *constant == 1;
  }

  static Value buildTrailingExtentProduct(ArtsCodegen *AC,
                                          ArrayRef<Value> extents,
                                          unsigned firstDim, Location loc) {
    Value product = AC->createIndexConstant(1, loc);
    for (unsigned i = firstDim; i < extents.size(); ++i)
      product = AC->create<arith::MulIOp>(loc, product,
                                          AC->castToIndex(extents[i], loc));
    return product;
  }

  static std::pair<Value, Value>
  buildFaceSliceByteRange(ArtsCodegen *AC, ArrayRef<Value> blockExtents,
                          unsigned dim, int64_t haloExtent, bool upperFace,
                          Value scalarSize, Location loc) {
    Value zero = AC->createIndexConstant(0, loc);
    Value halo = AC->createIndexConstant(haloExtent, loc);
    Value dimExtent = AC->castToIndex(blockExtents[dim], loc);
    Value clampedHalo = AC->create<arith::MinUIOp>(loc, halo, dimExtent);
    Value trailingExtent =
        buildTrailingExtentProduct(AC, blockExtents, dim + 1, loc);
    Value elementOffset = zero;
    if (upperFace)
      elementOffset = AC->create<arith::SubIOp>(loc, dimExtent, clampedHalo);
    Value linearOffset =
        AC->create<arith::MulIOp>(loc, elementOffset, trailingExtent);
    Value elementCount =
        AC->create<arith::MulIOp>(loc, clampedHalo, trailingExtent);
    Value byteOffset = AC->create<arith::MulIOp>(loc, linearOffset, scalarSize);
    Value byteSize = AC->create<arith::MulIOp>(loc, elementCount, scalarSize);
    return {byteOffset, byteSize};
  }

  std::pair<Value, Value>
  inferStencilFaceSliceForSlot(const DepDbInfo &depInfo, Value linearIndex,
                               ArrayRef<Value> directIndices,
                               Location loc) const {
    if (!depInfo.stencilContract ||
        !depInfo.stencilContract->isStencilFamily() ||
        !depInfo.stencilContract->supportsBlockHalo() || !depInfo.scalarSize)
      return {Value(), Value()};

    auto staticMin = depInfo.stencilContract->getStaticMinOffsets();
    auto staticMax = depInfo.stencilContract->getStaticMaxOffsets();
    if (!staticMin || !staticMax || depInfo.blockElementSizes.empty())
      return {Value(), Value()};

    Value zero = AC->createIndexConstant(0, loc);
    Value zeroSize = AC->createIndexConstant(0, loc);
    Value trueI1 = AC->create<arith::ConstantIntOp>(loc, 1, 1);
    Value selectedOffset = zero;
    Value selectedSize = zeroSize;

    auto applyCandidate = [&](Value match, std::pair<Value, Value> slice) {
      selectedOffset =
          AC->create<arith::SelectOp>(loc, match, slice.first, selectedOffset);
      selectedSize =
          AC->create<arith::SelectOp>(loc, match, slice.second, selectedSize);
    };

    if (!directIndices.empty() && !depInfo.stencilCenterCoords.empty()) {
      unsigned ownerRank = std::min<unsigned>(
          directIndices.size(),
          std::min<unsigned>(
              depInfo.stencilCenterCoords.size(),
              std::min<unsigned>(
                  depInfo.blockOwnerDims.empty()
                      ? depInfo.blockElementSizes.size()
                      : depInfo.blockOwnerDims.size(),
                  std::min<unsigned>(staticMin->size(), staticMax->size()))));
      for (unsigned dim = 0; dim < ownerRank; ++dim) {
        unsigned physicalDim =
            depInfo.blockOwnerDims.empty() ? dim : depInfo.blockOwnerDims[dim];
        if (physicalDim >= depInfo.blockElementSizes.size())
          continue;

        // A byte slice can only represent a contiguous face. If any leading
        // extent above the narrowed dimension is wider than 1, the face would
        // become strided and must stay on the whole-DB dependency path.
        bool contiguousFace = true;
        for (unsigned lead = 0; lead < physicalDim; ++lead)
          contiguousFace &= isUnitExtent(depInfo.blockElementSizes[lead]);
        if (!contiguousFace)
          continue;

        Value sameOtherDims = trueI1;
        for (unsigned other = 0; other < ownerRank; ++other) {
          if (other == dim)
            continue;
          Value equalCoord = AC->create<arith::CmpIOp>(
              loc, arith::CmpIPredicate::eq, directIndices[other],
              depInfo.stencilCenterCoords[other]);
          sameOtherDims =
              AC->create<arith::AndIOp>(loc, sameOtherDims, equalCoord);
        }

        int64_t haloBefore = std::max<int64_t>(0, -(*staticMin)[dim]);
        int64_t haloAfter = std::max<int64_t>(0, (*staticMax)[dim]);
        if (haloBefore > 0) {
          Value isLowerNeighbor = AC->create<arith::CmpIOp>(
              loc, arith::CmpIPredicate::ult, directIndices[dim],
              depInfo.stencilCenterCoords[dim]);
          Value match =
              AC->create<arith::AndIOp>(loc, sameOtherDims, isLowerNeighbor);
          applyCandidate(match, buildFaceSliceByteRange(
                                    AC, depInfo.blockElementSizes, physicalDim,
                                    haloBefore, /*upperFace=*/true,
                                    depInfo.scalarSize, loc));
        }
        if (haloAfter > 0) {
          Value isUpperNeighbor = AC->create<arith::CmpIOp>(
              loc, arith::CmpIPredicate::ugt, directIndices[dim],
              depInfo.stencilCenterCoords[dim]);
          Value match =
              AC->create<arith::AndIOp>(loc, sameOtherDims, isUpperNeighbor);
          applyCandidate(match, buildFaceSliceByteRange(
                                    AC, depInfo.blockElementSizes, physicalDim,
                                    haloAfter, /*upperFace=*/false,
                                    depInfo.scalarSize, loc));
        }
      }
      return {selectedOffset, selectedSize};
    }

    if (depInfo.stencilCenterLinear && !depInfo.blockElementSizes.empty()) {
      unsigned ownerRank =
          depInfo.blockOwnerDims.empty()
              ? static_cast<unsigned>(staticMin->size())
              : static_cast<unsigned>(depInfo.blockOwnerDims.size());
      if (ownerRank != 1)
        return {Value(), Value()};

      unsigned physicalDim =
          depInfo.blockOwnerDims.empty() ? 0 : depInfo.blockOwnerDims.front();
      if (physicalDim >= depInfo.blockElementSizes.size())
        return {Value(), Value()};

      for (unsigned lead = 0; lead < physicalDim; ++lead)
        if (!isUnitExtent(depInfo.blockElementSizes[lead]))
          return {Value(), Value()};

      int64_t haloBefore = std::max<int64_t>(0, -(*staticMin)[0]);
      int64_t haloAfter = std::max<int64_t>(0, (*staticMax)[0]);
      if (haloBefore > 0) {
        Value isLowerNeighbor = AC->create<arith::CmpIOp>(
            loc, arith::CmpIPredicate::ult, AC->castToIndex(linearIndex, loc),
            AC->castToIndex(depInfo.stencilCenterLinear, loc));
        applyCandidate(isLowerNeighbor,
                       buildFaceSliceByteRange(AC, depInfo.blockElementSizes,
                                               physicalDim, haloBefore,
                                               /*upperFace=*/true,
                                               depInfo.scalarSize, loc));
      }
      if (haloAfter > 0) {
        Value isUpperNeighbor = AC->create<arith::CmpIOp>(
            loc, arith::CmpIPredicate::ugt, AC->castToIndex(linearIndex, loc),
            AC->castToIndex(depInfo.stencilCenterLinear, loc));
        applyCandidate(isUpperNeighbor,
                       buildFaceSliceByteRange(AC, depInfo.blockElementSizes,
                                               physicalDim, haloAfter,
                                               /*upperFace=*/false,
                                               depInfo.scalarSize, loc));
      }
      return {selectedOffset, selectedSize};
    }

    return {Value(), Value()};
  }

  Value localLinearToGlobalLinear(Value localLinear,
                                  const DbLoweringInfo &dbInfo,
                                  ArrayRef<Value> allocSizes,
                                  Location loc) const {
    if (!localLinear || dbInfo.sizes.empty() || dbInfo.offsets.empty() ||
        allocSizes.size() < dbInfo.sizes.size() ||
        dbInfo.offsets.size() < dbInfo.sizes.size()) {
      return localLinear;
    }

    unsigned rank = dbInfo.sizes.size();
    Value one = AC->createIndexConstant(1, loc);
    Value remaining = AC->castToIndex(localLinear, loc);
    SmallVector<Value, 4> globalCoords;
    globalCoords.reserve(rank);

    for (unsigned i = 0; i < rank; ++i) {
      Value stride = one;
      for (unsigned j = i + 1; j < rank; ++j)
        stride = AC->create<arith::MulIOp>(
            loc, stride, AC->castToIndex(dbInfo.sizes[j], loc));

      Value localCoord = remaining;
      if (i + 1 < rank) {
        localCoord = AC->create<arith::DivUIOp>(loc, remaining, stride);
        remaining = AC->create<arith::RemUIOp>(loc, remaining, stride);
      }

      Value globalCoord = AC->create<arith::AddIOp>(
          loc, AC->castToIndex(dbInfo.offsets[i], loc), localCoord);
      globalCoords.push_back(globalCoord);
    }

    SmallVector<Value, 4> globalExtents;
    globalExtents.reserve(rank);
    for (unsigned i = 0; i < rank; ++i)
      globalExtents.push_back(AC->castToIndex(allocSizes[i], loc));
    return AC->computeLinearIndex(globalExtents, globalCoords, loc);
  }

  std::optional<LoweringContractInfo>
  getAcquireStencilContract(DbAcquireOp dbAcquireOp, Location loc) const {
    if (!dbAcquireOp)
      return std::nullopt;

    if (auto info = getLoweringContract(dbAcquireOp.getPtr()))
      return info;
    return getLoweringContract(dbAcquireOp.getOperation(), AC->getBuilder(),
                               loc);
  }

  Value inferStencilCenterLinearFromContract(DbAcquireOp dbAcquireOp,
                                             const DbLoweringInfo &dbInfo,
                                             ArrayRef<Value> allocSizes,
                                             Location loc) const {
    if (!dbAcquireOp || dbInfo.sizes.empty())
      return nullptr;

    auto contract = getAcquireStencilContract(dbAcquireOp, loc);
    if (!contract || !contract->isStencilFamily() ||
        contract->minOffsets.empty())
      return nullptr;

    unsigned rank =
        std::min<unsigned>(dbInfo.sizes.size(), contract->minOffsets.size());
    if (!contract->writeFootprint.empty())
      rank = std::min<unsigned>(rank, contract->writeFootprint.size());
    if (rank == 0)
      return nullptr;

    Value zero = AC->createIndexConstant(0, loc);
    Value one = AC->createIndexConstant(1, loc);

    SmallVector<Value, 4> localCoords;
    localCoords.reserve(rank);
    for (unsigned i = 0; i < rank; ++i) {
      Value writeCoord =
          contract->writeFootprint.empty() ? zero : contract->writeFootprint[i];
      Value minOffset = contract->minOffsets[i];
      Value dimSize = AC->castToIndex(dbInfo.sizes[i], loc);

      Value rawCoord =
          AC->create<arith::SubIOp>(loc, AC->castToIndex(writeCoord, loc),
                                    AC->castToIndex(minOffset, loc));

      Value nonNegative = AC->create<arith::MaxSIOp>(loc, rawCoord, zero);
      Value sizeMinusOne = AC->create<arith::SubIOp>(loc, dimSize, one);
      sizeMinusOne = AC->create<arith::MaxSIOp>(loc, sizeMinusOne, zero);
      localCoords.push_back(
          AC->create<arith::MinSIOp>(loc, nonNegative, sizeMinusOne));
    }

    Value localLinearIndex = zero;
    for (unsigned i = 0; i < rank; ++i) {
      Value stride = one;
      for (unsigned j = i + 1; j < rank; ++j) {
        stride = AC->create<arith::MulIOp>(
            loc, stride, AC->castToIndex(dbInfo.sizes[j], loc));
      }
      Value contribution =
          AC->create<arith::MulIOp>(loc, localCoords[i], stride);
      localLinearIndex =
          AC->create<arith::AddIOp>(loc, localLinearIndex, contribution);
    }

    return localLinearToGlobalLinear(localLinearIndex, dbInfo, allocSizes, loc);
  }

  Value inferSymmetricStencilCenterLinear(int64_t centerOffset,
                                          const DbLoweringInfo &dbInfo,
                                          ArrayRef<Value> allocSizes,
                                          Location loc) const {
    if (centerOffset < 0 || dbInfo.sizes.empty())
      return nullptr;

    Value zero = AC->createIndexConstant(0, loc);
    Value one = AC->createIndexConstant(1, loc);
    Value center = AC->createIndexConstant(centerOffset, loc);

    Value localLinearIndex = zero;
    for (unsigned i = 0; i < dbInfo.sizes.size(); ++i) {
      Value dimSize = AC->castToIndex(dbInfo.sizes[i], loc);
      Value sizeMinusOne = AC->create<arith::SubIOp>(loc, dimSize, one);
      sizeMinusOne = AC->create<arith::MaxSIOp>(loc, sizeMinusOne, zero);
      Value localCoord = AC->create<arith::MinSIOp>(loc, center, sizeMinusOne);

      Value stride = one;
      for (unsigned j = i + 1; j < dbInfo.sizes.size(); ++j) {
        stride = AC->create<arith::MulIOp>(
            loc, stride, AC->castToIndex(dbInfo.sizes[j], loc));
      }
      Value contribution = AC->create<arith::MulIOp>(loc, localCoord, stride);
      localLinearIndex =
          AC->create<arith::AddIOp>(loc, localLinearIndex, contribution);
    }

    return localLinearToGlobalLinear(localLinearIndex, dbInfo, allocSizes, loc);
  }

  /// DepDbAcquireOp addresses dependency storage through depv plus an explicit
  /// baseOffset. RecordDep therefore iterates the local [0, size) window and
  /// lets baseOffset select the correct slice in depv.
  void rebaseDepIterationWindow(DepDbInfo &info, Location loc) const {
    if (info.dbInfo.sizes.empty())
      return;

    info.dbInfo.offsets.clear();
    info.dbInfo.offsets.reserve(info.dbInfo.sizes.size());
    for (size_t i = 0; i < info.dbInfo.sizes.size(); ++i)
      info.dbInfo.offsets.push_back(AC->createIndexConstant(0, loc));
  }

  /// Extract DB lowering info and stencil metadata from a dbGuid's defining op.
  DepDbInfo extractDbInfoForDeps(Value dbGuid,
                                 std::optional<int32_t> acquireMode,
                                 Location loc) const {
    DepDbInfo result;

    if (auto dbAcquireOp = dbGuid.getDefiningOp<DbAcquireOp>()) {
      result.dbInfo = DbUtils::extractDbLoweringInfo(dbAcquireOp);
      result.guidStorage =
          dbAcquireOp.getSourceGuid() ? dbAcquireOp.getSourceGuid() : dbGuid;
      result.allocSizes = resolveOuterSizesForGuid(dbGuid, AC, loc);
      /// Stencil writer acquires frequently cover [halo..., center, halo...]
      /// DB entries. Recording every entry as WRITE over-serializes adjacent
      /// blocks. Use the acquire's stencil contract to identify the owned
      /// center block and downgrade only the non-center entries to read-only.
      ///
      /// Prefer the full lowering contract so boundary-clamped windows keep
      /// the correct owned-center block. stencil_center_offset is only a
      /// symmetric-radius fallback when richer contract data is unavailable.
      int32_t writeMode = static_cast<int32_t>(DbMode::write);
      bool writerMode = !acquireMode || *acquireMode == writeMode;
      auto partitionMode = dbAcquireOp.getPartitionMode();
      if (writerMode && partitionMode &&
          (*partitionMode == PartitionMode::block ||
           *partitionMode == PartitionMode::stencil)) {
        result.stencilContract = getAcquireStencilContract(dbAcquireOp, loc);
        result.stencilCenterLinear = inferStencilCenterLinearFromContract(
            dbAcquireOp, result.dbInfo, result.allocSizes, loc);
        result.stencilCenterCoords = inferStencilCenterCoordsFromContract(
            dbAcquireOp, result.dbInfo, loc);
        if (!result.stencilCenterLinear)
          if (auto centerOffset =
                  getStencilCenterOffset(dbAcquireOp.getOperation())) {
            result.stencilCenterLinear = inferSymmetricStencilCenterLinear(
                *centerOffset, result.dbInfo, result.allocSizes, loc);
            if (result.stencilCenterCoords.empty())
              result.stencilCenterCoords = inferSymmetricStencilCenterCoords(
                  *centerOffset, result.dbInfo, loc);
          }
      }

      if (auto alloc = dyn_cast_or_null<DbAllocOp>(
              DbUtils::getUnderlyingDbAlloc(dbAcquireOp.getSourcePtr()))) {
        Type elementType = alloc.getElementType();
        if (auto memrefType = dyn_cast<MemRefType>(elementType))
          elementType = memrefType.getElementType();
        result.scalarSize = AC->create<polygeist::TypeSizeOp>(
            loc, IndexType::get(AC->getContext()), elementType);

        auto allocExtents = alloc.getElementSizes();
        result.blockElementSizes.assign(allocExtents.begin(),
                                        allocExtents.end());

        unsigned ownerRank = result.dbInfo.sizes.size();
        if (result.stencilContract &&
            !result.stencilContract->ownerDims.empty())
          ownerRank =
              static_cast<unsigned>(result.stencilContract->ownerDims.size());
        if (ownerRank != 0) {
          if (result.stencilContract)
            result.blockOwnerDims =
                resolveContractOwnerDims(*result.stencilContract, ownerRank);
          else
            for (unsigned dim = 0; dim < ownerRank; ++dim)
              result.blockOwnerDims.push_back(dim);
        }
      }
    } else if (auto depDbAcquireOp = dbGuid.getDefiningOp<DepDbAcquireOp>()) {
      result.dbInfo = DbUtils::extractDbLoweringInfo(depDbAcquireOp);
      rebaseDepIterationWindow(result, loc);
      result.guidStorage = depDbAcquireOp.getGuid();
      result.depStruct = depDbAcquireOp.getDepStruct();
      result.baseOffset = depDbAcquireOp.getOffset();
    } else {
      ARTS_UNREACHABLE("dbGuid must come from DbAcquireOp or DepDbAcquireOp");
    }

    return result;
  }

  /// Holds bounds-checking and allocation size info for dependency emission.
  struct DepBoundsInfo {
    bool useDepv = false;
    Value totalDBs;
    SmallVector<Value> allocSizes;
  };

  /// Compute useDepv flag, bounds-check values, and allocation sizes.
  DepBoundsInfo computeDepBounds(Value dbGuid, const DepDbInfo &depInfo,
                                 DepAccessMode accessMode,
                                 Value boundsValid) const {
    DepBoundsInfo result;
    result.useDepv = depInfo.depStruct && depInfo.baseOffset &&
                     (accessMode == DepAccessMode::from_depv ||
                      dbGuid.getDefiningOp<DepDbAcquireOp>());

    result.totalDBs = result.useDepv
                          ? Value()
                          : getTotalDBsForBoundsCheck(dbGuid, boundsValid);
    if (!result.useDepv)
      result.allocSizes = resolveOuterSizesForGuid(dbGuid, AC, dbGuid.getLoc());

    return result;
  }

  /// Iterate over DB elements and emit record-dep calls for each index.
  void emitRecordDepCalls(Value dbGuid, Value edtGuid, Value sharedSlotAlloc,
                          DepAccessMode accessMode,
                          std::optional<int32_t> acquireMode,
                          std::optional<int32_t> depFlags, Value boundsValid,
                          Value byteOffset, Value byteSize,
                          const DepDbInfo &depInfo, const DepBoundsInfo &bounds,
                          Location loc) const {
    auto guidStorageType = dyn_cast<MemRefType>(
        (depInfo.guidStorage ? depInfo.guidStorage : dbGuid).getType());
    bool useDirectCoords = !bounds.useDepv && guidStorageType &&
                           guidStorageType.getRank() > 1 &&
                           depInfo.dbInfo.sizes.size() > 1;
    if (useDirectCoords) {
      Value one = AC->createIndexConstant(1, loc);
      SmallVector<Value, 4> globalCoords;

      std::function<void(unsigned)> emitForCoords = [&](unsigned dim) {
        if (dim == depInfo.dbInfo.sizes.size()) {
          SmallVector<Value, 4> localCoords;
          localCoords.reserve(globalCoords.size());
          for (auto [coord, offset] :
               llvm::zip(globalCoords, depInfo.dbInfo.offsets)) {
            localCoords.push_back(AC->create<arith::SubIOp>(
                loc, coord, AC->castToIndex(offset, loc)));
          }

          Value linearIndex = bounds.allocSizes.empty()
                                  ? AC->computeLinearIndex(depInfo.dbInfo.sizes,
                                                           localCoords, loc)
                                  : AC->computeLinearIndex(bounds.allocSizes,
                                                           globalCoords, loc);

          recordSingleDb(dbGuid, depInfo.guidStorage, edtGuid, sharedSlotAlloc,
                         linearIndex, ArrayRef<Value>(globalCoords),
                         bounds.allocSizes, accessMode, acquireMode, depFlags,
                         boundsValid, depInfo.depStruct, depInfo.baseOffset,
                         bounds.totalDBs, byteOffset, byteSize,
                         depInfo.stencilCenterLinear,
                         depInfo.stencilCenterCoords, &depInfo, loc);
          return;
        }

        Value lowerBound = depInfo.dbInfo.offsets[dim];
        Value upperBound = AC->create<arith::AddIOp>(loc, lowerBound,
                                                     depInfo.dbInfo.sizes[dim]);
        auto loopOp = AC->create<scf::ForOp>(loc, lowerBound, upperBound, one);
        Block &loopBlock = loopOp.getRegion().front();
        AC->setInsertionPointToStart(&loopBlock);
        globalCoords.push_back(loopOp.getInductionVar());
        emitForCoords(dim + 1);
        globalCoords.pop_back();
        AC->setInsertionPointAfter(loopOp);
      };

      emitForCoords(0);
      return;
    }

    AC->iterateDbElements(
        dbGuid, edtGuid, depInfo.dbInfo.sizes, depInfo.dbInfo.offsets,
        depInfo.dbInfo.isSingleElement, loc,
        [&](Value linearIndex) {
          recordSingleDb(dbGuid, depInfo.guidStorage, edtGuid, sharedSlotAlloc,
                         linearIndex, ArrayRef<Value>(), bounds.allocSizes,
                         accessMode, acquireMode, depFlags, boundsValid,
                         depInfo.depStruct, depInfo.baseOffset, bounds.totalDBs,
                         byteOffset, byteSize, depInfo.stencilCenterLinear,
                         depInfo.stencilCenterCoords, &depInfo, loc);
        },
        bounds.allocSizes);
  }

  /// Record all dependencies for a single datablock
  void recordDepsForDb(Value dbGuid, Value edtGuid, Value sharedSlotAlloc,
                       DepAccessMode accessMode,
                       std::optional<int32_t> acquireMode,
                       std::optional<int32_t> depFlags, Value boundsValid,
                       Value byteOffset, Value byteSize, Location loc) const {
    DepDbInfo depInfo = extractDbInfoForDeps(dbGuid, acquireMode, loc);
    DepBoundsInfo bounds =
        computeDepBounds(dbGuid, depInfo, accessMode, boundsValid);
    emitRecordDepCalls(dbGuid, edtGuid, sharedSlotAlloc, accessMode,
                       acquireMode, depFlags, boundsValid, byteOffset, byteSize,
                       depInfo, bounds, loc);
  }

  /// Emit the appropriate runtime call for recording a dependency.
  /// Standard path: arts_add_dependence(dbGuid, edtGuid, slot, mode)
  /// ESD path: arts_add_dependence_at(dbGuid, edtGuid, slot, mode, offset,
  /// size)
  void emitRecordDepCall(Value dbGuidValue, Value edtGuidValue,
                         Value currentSlotI32, Value modeValue,
                         Value byteOffsetI64, Value byteSizeI64,
                         std::optional<int32_t> depFlags, Location loc) const {
    auto emitWholeDbDep = [&]() {
      ArtsCodegen::RuntimeCallBuilder RCB(*AC, loc);
      /// Standard path: full datablock dependency
      if (depFlags && *depFlags != 0) {
        Value flagsValue = AC->createIntConstant(*depFlags, AC->Int32, loc);
        RCB.callVoid(
            types::ARTSRTL_arts_add_dependence_ex,
            {dbGuidValue, edtGuidValue, currentSlotI32, modeValue, flagsValue});
      } else {
        RCB.callVoid(types::ARTSRTL_arts_add_dependence,
                     {dbGuidValue, edtGuidValue, currentSlotI32, modeValue});
      }
    };

    auto emitSliceDep = [&]() {
      ArtsCodegen::RuntimeCallBuilder RCB(*AC, loc);
      /// ESD path: partial slice dependency with byte offset/size
      if (depFlags && *depFlags != 0) {
        Value flagsValue = AC->createIntConstant(*depFlags, AC->Int32, loc);
        RCB.callVoid(types::ARTSRTL_arts_add_dependence_at_ex,
                     {dbGuidValue, edtGuidValue, currentSlotI32, modeValue,
                      byteOffsetI64, byteSizeI64, flagsValue});
      } else {
        RCB.callVoid(types::ARTSRTL_arts_add_dependence_at,
                     {dbGuidValue, edtGuidValue, currentSlotI32, modeValue,
                      byteOffsetI64, byteSizeI64});
      }
    };

    if (!byteOffsetI64 || !byteSizeI64) {
      emitWholeDbDep();
      return;
    }

    Value normalizedByteSize = ValueAnalysis::stripNumericCasts(byteSizeI64);
    /// byte_size == 0 is the cross-pass sentinel for "no partial slice".
    /// Respect it both when the zero is constant and when it only becomes
    /// known after runtime guards (whole-block or center-block fallback).
    if (ValueAnalysis::isZeroConstant(normalizedByteSize)) {
      emitWholeDbDep();
      return;
    }
    if (ValueAnalysis::isProvablyNonZero(normalizedByteSize)) {
      emitSliceDep();
      return;
    }

    Value zeroI64 = AC->createIntConstant(0, AC->Int64, loc);
    Value useWholeDbDep = AC->create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::eq, byteSizeI64, zeroI64);
    auto ifOp =
        AC->create<scf::IfOp>(loc, useWholeDbDep, /*withElseRegion=*/true);
    AC->setInsertionPointToStart(&ifOp.getThenRegion().front());
    emitWholeDbDep();
    AC->setInsertionPointToStart(&ifOp.getElseRegion().front());
    emitSliceDep();
    AC->setInsertionPointAfter(ifOp);
  }

  /// Record a single datablock dependency for an EDT slot.
  ///
  /// Two main paths:
  /// 1. Guarded (boundsValid set): if-else to handle boundary workers
  ///    - Then: arts_add_dependence[_at] for valid indices
  ///    - Else: arts_signal_edt_null for out-of-bounds
  /// 2. Unconditional: direct arts_add_dependence[_at] call
  ///
  /// Within each path, ESD vs standard is handled by emitRecordDepCall.
  void recordSingleDb(
      Value dbGuid, Value guidStorage, Value edtGuid, Value slotAlloc,
      Value linearIndex, ArrayRef<Value> directIndices,
      ArrayRef<Value> directLayoutSizes, DepAccessMode accessMode,
      std::optional<int32_t> acquireMode, std::optional<int32_t> depFlags,
      Value boundsValid, Value depStruct, Value baseOffset, Value totalDBs,
      Value byteOffset, Value byteSize, Value stencilCenterLinear,
      ArrayRef<Value> stencilCenterCoords, const DepDbInfo *depInfo,
      Location loc) const {
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
    if (auto mt = dyn_cast<MemRefType>(edtGuid.getType())) {
      auto zeroIndex = AC->createIndexConstant(0, loc);
      edtGuidValue =
          AC->create<memref::LoadOp>(loc, edtGuid, ValueRange{zeroIndex});
    }
    edtGuidValue = AC->ensureI64(edtGuidValue, loc);

    auto currentSlotI32 = AC->create<memref::LoadOp>(loc, slotAlloc);
    int32_t readMode = static_cast<int32_t>(DbMode::read);
    int32_t writeMode = static_cast<int32_t>(DbMode::write);
    int32_t modeInt = acquireMode.value_or(writeMode);
    Value modeValue = AC->createIntConstant(modeInt, AC->Int32, loc);

    Value isCenterBlock;
    if (!stencilCenterCoords.empty() && !directIndices.empty() &&
        stencilCenterCoords.size() == directIndices.size() &&
        modeInt == writeMode) {
      isCenterBlock = buildCoordsEqual(directIndices, stencilCenterCoords, loc);
      Value readValue = AC->createIntConstant(readMode, AC->Int32, loc);
      Value writeValue = AC->createIntConstant(writeMode, AC->Int32, loc);
      modeValue = AC->create<arith::SelectOp>(loc, isCenterBlock, writeValue,
                                              readValue);
    } else if (stencilCenterLinear && modeInt == writeMode) {
      Value linearIndexIdx = AC->castToIndex(linearIndex, loc);
      Value centerIdx = AC->castToIndex(stencilCenterLinear, loc);
      isCenterBlock = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::eq,
                                                linearIndexIdx, centerIdx);
      Value readValue = AC->createIntConstant(readMode, AC->Int32, loc);
      Value writeValue = AC->createIntConstant(writeMode, AC->Int32, loc);
      modeValue = AC->create<arith::SelectOp>(loc, isCenterBlock, writeValue,
                                              readValue);
    }

    // When an acquire expands into multiple DB slots, derive read-only face
    // slices from the stencil contract per emitted slot instead of widening
    // every neighbor block back to a whole-DB dependence.
    Value effectiveByteOffset = byteOffset;
    Value effectiveByteSize = byteSize;
    std::optional<int32_t> effectiveDepFlags = depFlags;
    bool preserveShape =
        depFlags && ((*depFlags & kArtsDepFlagPreserveShape) != 0);
    bool inferredPreserveShape = false;
    bool hasExplicitSlice = byteOffset && byteSize &&
                            !ValueAnalysis::isZeroConstant(
                                ValueAnalysis::stripNumericCasts(byteSize));
    if (!preserveShape && !hasExplicitSlice && depInfo) {
      auto inferredSlice = inferStencilFaceSliceForSlot(*depInfo, linearIndex,
                                                        directIndices, loc);
      if (inferredSlice.first && inferredSlice.second) {
        effectiveByteOffset = inferredSlice.first;
        effectiveByteSize = inferredSlice.second;
        /// Late stencil-face inference keeps the consumer IR on the original
        /// block coordinate system (for example, Seidel's left halo still
        /// indexes row 252 of the predecessor block). Compact slice payloads
        /// would break that contract, so inferred face slices must request the
        /// shape-preserving runtime path.
        inferredPreserveShape = true;
        int32_t depFlagBits = effectiveDepFlags.value_or(0);
        depFlagBits |= kArtsDepFlagPreserveShape;
        effectiveDepFlags = depFlagBits;
      }
    }
    if (preserveShape && !inferredPreserveShape) {
      /// Explicit preserve-shape markings currently act as an analysis-time
      /// "do not compact this acquire" contract. Keep those on the whole-DB
      /// path until the upstream acquire rewrite carries a compact index space.
      effectiveByteOffset = nullptr;
      effectiveByteSize = nullptr;
    }

    /// Prepare ESD values if needed (cast Index to Int64).
    /// byte_size == 0 denotes "no partial slice" and should use full-db deps.
    bool hasPartialSlice =
        effectiveByteOffset && effectiveByteSize &&
        !ValueAnalysis::isZeroConstant(
            ValueAnalysis::stripNumericCasts(effectiveByteSize));
    Value byteOffsetI64 =
        hasPartialSlice ? AC->ensureI64(effectiveByteOffset, loc) : nullptr;
    Value byteSizeI64 =
        hasPartialSlice ? AC->ensureI64(effectiveByteSize, loc) : nullptr;
    if (hasPartialSlice && isCenterBlock) {
      /// ARTS only supports sliced DB transport for RO slots. Keep halo
      /// dependencies as byte slices, but force the owned center block back to
      /// a whole-DB dependence by zeroing the slice at runtime.
      Value zeroI64 = AC->createIntConstant(0, AC->Int64, loc);
      byteOffsetI64 = AC->create<arith::SelectOp>(loc, isCenterBlock, zeroI64,
                                                  byteOffsetI64);
      byteSizeI64 =
          AC->create<arith::SelectOp>(loc, isCenterBlock, zeroI64, byteSizeI64);
    }

    if (effectiveBoundsValid) {
      /// GUARDED PATH: boundary workers may have invalid indices
      auto ifOp = AC->create<scf::IfOp>(loc, effectiveBoundsValid,
                                        /*withElseRegion=*/true);

      /// Then: valid index - record the dependency
      AC->setInsertionPointToStart(&ifOp.getThenRegion().front());
      Value dbGuidValue = loadDbGuidValue(dbGuid, guidStorage, linearIndex,
                                          directIndices, directLayoutSizes,
                                          useDepv, depStruct, baseOffset, loc);
      emitRecordDepCall(dbGuidValue, edtGuidValue, currentSlotI32, modeValue,
                        byteOffsetI64, byteSizeI64, effectiveDepFlags, loc);

      /// Else: invalid index - signal null dependency
      AC->setInsertionPointToStart(&ifOp.getElseRegion().front());
      ArtsCodegen::RuntimeCallBuilder RCB(*AC, loc);
      RCB.callVoid(types::ARTSRTL_arts_signal_edt_null,
                   {edtGuidValue, currentSlotI32});

      AC->setInsertionPointAfter(ifOp);
    } else {
      /// UNCONDITIONAL PATH: all indices are valid
      Value dbGuidValue = loadDbGuidValue(dbGuid, guidStorage, linearIndex,
                                          directIndices, directLayoutSizes,
                                          useDepv, depStruct, baseOffset, loc);
      emitRecordDepCall(dbGuidValue, edtGuidValue, currentSlotI32, modeValue,
                        byteOffsetI64, byteSizeI64, effectiveDepFlags, loc);
    }

    /// Increment slot counter
    auto oneI32 = AC->createIntConstant(1, AC->Int32, loc);
    auto incrementedSlot =
        AC->create<arith::AddIOp>(loc, currentSlotI32, oneI32);
    AC->create<memref::StoreOp>(loc, incrementedSlot, slotAlloc);
  }
};

/// Pattern to convert arts.increment_dep operations.
/// In arts v2, the latch mechanism has been removed. IncrementDepOp is now a
/// no-op that simply gets erased during lowering.
struct IncrementDepPattern : public ArtsToLLVMPattern<IncrementDepOp> {
  using ArtsToLLVMPattern::ArtsToLLVMPattern;

  LogicalResult matchAndRewrite(IncrementDepOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Erasing IncrementDep Op (latch removed in v2) " << op);
    rewriter.eraseOp(op);
    return success();
  }
};

///===----------------------------------------------------------------------===///
/// EDT  Patterns
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
    auto resultType = dyn_cast<MemRefType>(op.getMemref().getType());
    if (!resultType)
      return op.emitError("Expected MemRef type for result");

    /// Check if result type has dynamic dimensions
    bool hasDynamicDims = resultType.getNumDynamicDims() > 0;

    memref::AllocaOp allocOp;
    if (hasDynamicDims) {
      /// Dynamic memref: allocate with size and store parameters
      auto numParams = AC->createIndexConstant(params.size(), loc);
      allocOp =
          AC->create<memref::AllocaOp>(loc, resultType, ValueRange{numParams});

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
          AC->create<memref::AllocaOp>(loc, dynamicType, ValueRange{zeroIndex});
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
      linearIndex = AC->ensureI64(linearIndex, loc);

    for (size_t i = 0; i < indices.size(); ++i) {
      Value idx = indices[i];
      if (idx.getType() != AC->Int64)
        idx = AC->ensureI64(idx, loc);
      Value strideVal = (i < strides.size())
                            ? strides[i]
                            : AC->createIntConstant(1, AC->Int64, loc);
      if (strideVal.getType() != AC->Int64)
        strideVal = AC->ensureI64(strideVal, loc);

      Value contrib = AC->create<arith::MulIOp>(loc, idx, strideVal);
      linearIndex = AC->create<arith::AddIOp>(loc, linearIndex, contrib);
    }

    Value depEntryPtr = AC->create<LLVM::GEPOp>(
        loc, AC->llvmPtr, AC->ArtsEdtDep, typedDepPtr, ValueRange{linearIndex});

    /// Extract both guid (field #0) and ptr (field #1) from depv
    /// v2 arts_edt_dep_t layout: { guid, ptr, mode }
    auto c0 = AC->createIntConstant(0, AC->Int64, loc);
    auto cPtrIdx = AC->createIntConstant(1, AC->Int64, loc);

    /// Get the guid pointer (field #0)
    auto guidPtr = AC->create<LLVM::GEPOp>(loc, AC->llvmPtr, AC->ArtsEdtDep,
                                           depEntryPtr, ValueRange{c0, c0});

    /// Get the data pointer (field #1)
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
    auto baseMT = dyn_cast<MemRefType>(base.getType());

    /// Pad strides with 1s to match indices length
    SmallVector<Value> paddedStrides(strides.begin(), strides.end());
    while (paddedStrides.size() < indices.size())
      paddedStrides.push_back(AC->createIndexConstant(1, loc));

    /// Compute linear element index using provided strides
    Value linearIdx =
        AC->computeLinearIndexFromStrides(paddedStrides, indices, loc);
    Value idx64 = AC->ensureI64(linearIdx, loc);

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

    /// Build arts_edt_create arguments
    auto funcPtr = AC->createFnPtr(outlined, op.getLoc());
    auto loc = op.getLoc();
    auto paramv = op.getParamMemref();
    Value depc = op.getDepCount();
    if (depc && depc.getType() != AC->Int32)
      depc = AC->castToInt(AC->Int32, depc, loc);

    /// Calculate parameter count from memref size
    Value paramc;
    if (auto memrefType = dyn_cast<MemRefType>(paramv.getType())) {
      if (memrefType.hasStaticShape() && memrefType.getNumElements() == 0) {
        paramc = AC->createIntConstant(0, AC->Int32, loc);
      } else {
        /// Dynamic memref case - get size from memref
        auto zeroIndex = AC->createIndexConstant(0, loc);
        auto memrefSize = AC->create<memref::DimOp>(loc, paramv, zeroIndex);
        paramc = AC->create<arith::IndexCastOp>(loc, AC->Int32, memrefSize);
      }
    } else {
      paramc = AC->createIntConstant(0, AC->Int32, loc);
    }

    /// Build arts_hint_t struct: { uint32_t route, uint64_t id }
    Value route = op.getRoute();
    if (!route)
      route = createCurrentNodeRoute(AC->getBuilder(), loc);

    auto createIdAttr =
        op->getAttrOfType<IntegerAttr>(AttrNames::Operation::ArtsCreateId);
    Value artsIdVal;
    if (createIdAttr)
      artsIdVal = AC->create<arith::ConstantOp>(loc, AC->Int64, createIdAttr);
    else
      artsIdVal = AC->createIntConstant(0, AC->Int64, loc);

    /// Stack-allocate arts_hint_t and populate fields
    Value hintAlloc =
        AC->create<LLVM::AllocaOp>(loc, AC->llvmPtr, AC->ArtsHintType,
                                   AC->createIntConstant(1, AC->Int32, loc));
    auto c0 = AC->createIntConstant(0, AC->Int32, loc);
    auto c1 = AC->createIntConstant(1, AC->Int32, loc);
    /// Store route into field 0
    Value routeFieldPtr = AC->create<LLVM::GEPOp>(
        loc, AC->llvmPtr, AC->ArtsHintType, hintAlloc, ValueRange{c0, c0});
    AC->create<LLVM::StoreOp>(loc, route, routeFieldPtr);
    /// Store arts_id into field 1
    Value idFieldPtr = AC->create<LLVM::GEPOp>(
        loc, AC->llvmPtr, AC->ArtsHintType, hintAlloc, ValueRange{c0, c1});
    AC->create<LLVM::StoreOp>(loc, artsIdVal, idFieldPtr);

    /// Cast !llvm.ptr to ArtsHintTypePtr memref for runtime call
    Value hintMemref = AC->create<polygeist::Pointer2MemrefOp>(
        loc, AC->ArtsHintTypePtr, hintAlloc);

    /// Create arts_edt_create call with hint as last argument
    ArtsCodegen::RuntimeCallBuilder RCB(*AC, loc);
    func::CallOp callOp;
    if (op.getEpochGuid()) {
      callOp = RCB.callOp(
          types::ARTSRTL_arts_edt_create_with_epoch,
          {funcPtr, paramc, paramv, depc, op.getEpochGuid(), hintMemref});
    } else {
      callOp = RCB.callOp(types::ARTSRTL_arts_edt_create,
                          {funcPtr, paramc, paramv, depc, hintMemref});
    }
    if (createIdAttr)
      callOp->setAttr(AttrNames::Operation::ArtsCreateId, createIdAttr);
    rewriter.replaceOp(op, callOp.getResult(0));
    return success();
  }
};

///===----------------------------------------------------------------------===///
/// DB Patterns
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

    Value route = op.getRoute();
    if (!route)
      route = createCurrentNodeRoute(AC->getBuilder(), loc);
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
    if (auto createIdAttr =
            op->getAttrOfType<IntegerAttr>(AttrNames::Operation::ArtsCreateId))
      nextId = createIdAttr.getInt();
    else
      nextId = getArtsId(op);

    DbLoweringInfo dbLowering = DbUtils::extractDbLoweringInfo(op);
    auto &dbSizes = dbLowering.sizes;
    bool isSingleElement = dbLowering.isSingleElement;

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
      createSingleDb(dbMemref, guidMemref, route, totalDbSize,
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
      createMultiDbs(dbMemref, guidMemref, dbSizes, route, totalDbSize,
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
    bool parallelInit = AC->useDistributedInitInWorkers();
    std::string nodeInitSymbol =
        baseName + (parallelInit ? "_reserve_init" : "_init");
    std::string workerInitSymbol = baseName + "_worker_init";

    auto guidDynamicType = MemRefType::get({ShapedType::kDynamic}, AC->Int64);
    auto ptrDynamicType = MemRefType::get({ShapedType::kDynamic}, AC->llvmPtr);
    auto guidHolderType = MemRefType::get({1}, guidDynamicType);
    auto ptrHolderType = MemRefType::get({1}, ptrDynamicType);
    getOrCreateMemrefGlobal(guidHolderSymbol, guidHolderType, op.getLoc());
    getOrCreateMemrefGlobal(ptrHolderSymbol, ptrHolderType, op.getLoc());

    ModuleOp module = AC->getModule();
    func::FuncOp initFn = module.lookupSymbol<func::FuncOp>(nodeInitSymbol);
    func::FuncOp workerInitFn;
    if (!initFn) {
      OpBuilder::InsertionGuard IG(AC->getBuilder());
      AC->setInsertionPoint(module);
      initFn = AC->create<func::FuncOp>(op.getLoc(), nodeInitSymbol,
                                        AC->InitPerNodeFn);
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

        if (auto blockArg = dyn_cast<BlockArgument>(value))
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
        if (auto queryOp = dyn_cast<RuntimeQueryOp>(cloneOp)) {
          auto kind = queryOp.getKind();
          return kind == RuntimeQueryKind::totalNodes ||
                 kind == RuntimeQueryKind::totalWorkers;
        }
        auto callOp = dyn_cast<func::CallOp>(cloneOp);
        if (!callOp)
          return false;
        auto callee = callOp.getCallee();
        return callee == "artsGetTotalNodes" || callee == "artsGetTotalWorkers";
      };

      if (!ValueAnalysis::cloneValuesIntoRegion(
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
      Value callbackRoute = AC->createIntConstant(0, AC->Int32, op.getLoc());
      std::optional<int64_t> callbackNextId = nextId;

      if (!parallelInit) {
        if (isSingleElement) {
          createSingleDb(
              ptrBuffer, guidBuffer, callbackRoute, callbackTotalDbSize,
              callbackNextId ? &callbackNextId : nullptr, op.getLoc(),
              /*distributedOwnership=*/true);
        } else {
          createMultiDbs(ptrBuffer, guidBuffer, callbackDbSizes, callbackRoute,
                         callbackTotalDbSize,
                         callbackNextId ? &callbackNextId : nullptr,
                         op.getLoc(),
                         /*distributedOwnership=*/true);
        }
        AC->create<func::ReturnOp>(op.getLoc());
      } else {
        /// Reserve deterministic GUIDs once per node; DB creation runs in
        /// initPerWorker.
        if (isSingleElement) {
          createSingleDb(
              ptrBuffer, guidBuffer, callbackRoute, callbackTotalDbSize,
              callbackNextId ? &callbackNextId : nullptr, op.getLoc(),
              /*distributedOwnership=*/true,
              /*createDb=*/false);
        } else {
          createMultiDbs(ptrBuffer, guidBuffer, callbackDbSizes, callbackRoute,
                         callbackTotalDbSize,
                         callbackNextId ? &callbackNextId : nullptr,
                         op.getLoc(),
                         /*distributedOwnership=*/true,
                         /*createDb=*/false);
        }
        AC->create<func::ReturnOp>(op.getLoc());

        workerInitFn = module.lookupSymbol<func::FuncOp>(workerInitSymbol);
        if (!workerInitFn) {
          AC->setInsertionPoint(module);
          workerInitFn = AC->create<func::FuncOp>(op.getLoc(), workerInitSymbol,
                                                  AC->InitPerWorkerFn);
          workerInitFn.setPrivate();

          Block *workerEntry = workerInitFn.addEntryBlock();
          AC->setInsertionPointToStart(workerEntry);

          IRMapping workerMapper;
          if (parentFn && parentFn.getNumArguments() > 0) {
            if (parentFn.getNumArguments() != 2 ||
                workerInitFn.getNumArguments() != 4)
              return failure();
            workerMapper.map(parentFn.getArgument(0),
                             workerInitFn.getArgument(2));
            workerMapper.map(parentFn.getArgument(1),
                             workerInitFn.getArgument(3));
          }

          llvm::SetVector<Value> workerValuesToClone;
          llvm::DenseSet<Value> workerVisited;
          std::function<LogicalResult(Value)> collectWorkerDependencies =
              [&](Value value) -> LogicalResult {
            if (!value)
              return failure();
            if (!workerVisited.insert(value).second ||
                workerMapper.contains(value))
              return success();

            if (auto blockArg = dyn_cast<BlockArgument>(value))
              return workerMapper.contains(blockArg) ? success() : failure();

            Operation *defOp = value.getDefiningOp();
            if (!defOp)
              return failure();
            for (Value operand : defOp->getOperands()) {
              if (failed(collectWorkerDependencies(operand)))
                return failure();
            }
            workerValuesToClone.insert(value);
            return success();
          };

          for (Value size : dbSizes) {
            if (failed(collectWorkerDependencies(size))) {
              ARTS_WARN("Failed to collect distributed worker init size "
                        "expression dependencies for "
                        << op);
              return failure();
            }
          }

          for (Value size : op.getElementSizes()) {
            if (failed(collectWorkerDependencies(size))) {
              ARTS_WARN("Failed to collect distributed worker init element "
                        "size expression dependencies for "
                        << op);
              return failure();
            }
          }

          if (!ValueAnalysis::cloneValuesIntoRegion(
                  workerValuesToClone, &workerInitFn.getBody(), workerMapper,
                  AC->getBuilder(), /*allowMemoryEffectFree=*/true,
                  isRuntimeTopologyCall)) {
            ARTS_WARN("Failed to clone distributed allocation size "
                      "expressions into worker init callback for "
                      << op);
            return failure();
          }

          auto mapWorkerValue = [&](Value v) {
            return workerMapper.lookupOrNull(v);
          };

          SmallVector<Value, 4> workerDbSizes;
          workerDbSizes.reserve(dbSizes.size());
          for (Value size : dbSizes) {
            Value mapped = mapWorkerValue(size);
            if (!mapped) {
              ARTS_WARN("Missing mapped DbAlloc size value in distributed "
                        "worker init for "
                        << op);
              return failure();
            }
            workerDbSizes.push_back(mapped);
          }

          SmallVector<Value, 4> workerElementSizes;
          workerElementSizes.reserve(op.getElementSizes().size());
          for (Value size : op.getElementSizes()) {
            Value mapped = mapWorkerValue(size);
            if (!mapped) {
              ARTS_WARN("Missing mapped DbAlloc element size value in "
                        "distributed worker init for "
                        << op);
              return failure();
            }
            workerElementSizes.push_back(mapped);
          }

          Value workerGuidHolder = AC->create<memref::GetGlobalOp>(
              op.getLoc(), guidHolderType, guidHolderSymbol);
          Value workerPtrHolder = AC->create<memref::GetGlobalOp>(
              op.getLoc(), ptrHolderType, ptrHolderSymbol);
          Value zeroIdx = AC->createIndexConstant(0, op.getLoc());
          Value workerGuidBuffer = AC->create<memref::LoadOp>(
              op.getLoc(), workerGuidHolder, ValueRange{zeroIdx});
          Value workerPtrBuffer = AC->create<memref::LoadOp>(
              op.getLoc(), workerPtrHolder, ValueRange{zeroIdx});
          Value workerTotalElems =
              AC->computeTotalElements(workerDbSizes, op.getLoc());

          Value workerElementSize =
              AC->computeElementTypeSize(op.getElementType(), op.getLoc());
          Value workerPayloadSize = AC->createIndexConstant(1, op.getLoc());
          for (Value dim : workerElementSizes) {
            workerPayloadSize =
                AC->create<arith::MulIOp>(op.getLoc(), workerPayloadSize, dim);
          }
          Value workerTotalDbSize = AC->create<arith::MulIOp>(
              op.getLoc(), workerElementSize, workerPayloadSize);

          Value workerNodeId = workerInitFn.getArgument(0);
          Value workerLocalId = workerInitFn.getArgument(1);
          Value zeroLoopIdx = AC->createIndexConstant(0, op.getLoc());
          Value oneIdx = AC->createIndexConstant(1, op.getLoc());
          Value zeroI32 = AC->createIntConstant(0, AC->Int32, op.getLoc());
          Value isPrimaryWorker = AC->create<arith::CmpIOp>(
              op.getLoc(), arith::CmpIPredicate::eq, workerLocalId, zeroI32);
          auto primaryWorkerIf =
              AC->create<scf::IfOp>(op.getLoc(), isPrimaryWorker, false);
          AC->setInsertionPointToStart(
              &primaryWorkerIf.getThenRegion().front());
          auto workerLoop = AC->create<scf::ForOp>(op.getLoc(), zeroLoopIdx,
                                                   workerTotalElems, oneIdx);
          AC->setInsertionPointToStart(&workerLoop.getRegion().front());
          Value linearIndex = workerLoop.getInductionVar();
          Value reservedGuid = AC->create<memref::LoadOp>(
              op.getLoc(), workerGuidBuffer, ValueRange{linearIndex});
          ArtsCodegen::RuntimeCallBuilder RCB(*AC, op.getLoc());
          Value guidRank =
              RCB.call(types::ARTSRTL_arts_guid_get_rank, {reservedGuid});
          Value isLocalGuid = AC->create<arith::CmpIOp>(
              op.getLoc(), arith::CmpIPredicate::eq, guidRank, workerNodeId);
          auto localGuidIf =
              AC->create<scf::IfOp>(op.getLoc(), isLocalGuid, false);
          AC->setInsertionPointToStart(&localGuidIf.getThenRegion().front());
          createDbFromGuidAtIndex(workerPtrBuffer, reservedGuid, linearIndex,
                                  workerTotalDbSize, callbackNextId,
                                  op.getLoc());
          AC->setInsertionPointAfter(localGuidIf);
          AC->setInsertionPointAfter(workerLoop);
          AC->setInsertionPointAfter(primaryWorkerIf);
          AC->create<func::ReturnOp>(op.getLoc());
        }
      }
    }

    AC->registerDistributedInitNodeCallback(initFn);
    if (parallelInit) {
      if (!workerInitFn)
        workerInitFn = module.lookupSymbol<func::FuncOp>(workerInitSymbol);
      if (workerInitFn)
        AC->registerDistributedInitWorkerCallback(workerInitFn);
    }

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

  void createDbFromGuidAtIndex(Value dbMemref, Value guid, Value linearIndex,
                               Value elementSize, std::optional<int64_t> nextId,
                               Location loc) const {
    Value elemSize64 = AC->ensureI64(elementSize, loc);

    /// Build arts_hint_t with arts_id if available.
    Value hintAlloc =
        AC->create<LLVM::AllocaOp>(loc, AC->llvmPtr, AC->ArtsHintType,
                                   AC->createIntConstant(1, AC->Int32, loc));
    auto c0 = AC->createIntConstant(0, AC->Int32, loc);
    auto c1 = AC->createIntConstant(1, AC->Int32, loc);
    Value routeFieldPtr = AC->create<LLVM::GEPOp>(
        loc, AC->llvmPtr, AC->ArtsHintType, hintAlloc, ValueRange{c0, c0});
    AC->create<LLVM::StoreOp>(loc, c0, routeFieldPtr);

    Value artsIdValue;
    if (nextId.has_value()) {
      Value baseArtsId = AC->create<arith::ConstantOp>(
          loc, AC->Int64, AC->getBuilder().getI64IntegerAttr(*nextId));
      Value linearIndex64 = AC->ensureI64(linearIndex, loc);
      artsIdValue = AC->create<arith::AddIOp>(loc, baseArtsId, linearIndex64);
    } else {
      artsIdValue = AC->createIntConstant(0, AC->Int64, loc);
    }
    Value idFieldPtr = AC->create<LLVM::GEPOp>(
        loc, AC->llvmPtr, AC->ArtsHintType, hintAlloc, ValueRange{c0, c1});
    AC->create<LLVM::StoreOp>(loc, artsIdValue, idFieldPtr);

    Value hintMemref = AC->create<polygeist::Pointer2MemrefOp>(
        loc, AC->ArtsHintTypePtr, hintAlloc);
    Value dbType = AC->createIntConstant(0, AC->Int32, loc);
    Value nullPtr = AC->create<LLVM::ZeroOp>(loc, AC->llvmPtr);
    Value nullData =
        AC->create<polygeist::Pointer2MemrefOp>(loc, AC->VoidPtr, nullPtr);

    ArtsCodegen::RuntimeCallBuilder RCB(*AC, loc);
    auto dbCall = RCB.callOp(types::ARTSRTL_arts_db_create_with_guid,
                             {guid, elemSize64, dbType, nullData, hintMemref});

    AC->create<memref::StoreOp>(loc, dbCall.getResult(0), dbMemref,
                                ValueRange{linearIndex});
  }

  void createSingleDb(
      Value dbMemref, Value guidMemref, Value route, Value elementSize,
      std::optional<int64_t> *nextId, Location loc,
      bool distributedOwnership = false, bool createDb = true,
      ArrayRef<Value> sizes = {}, ArrayRef<Value> indices = {},
      std::optional<Value> linearIndexOverride = std::nullopt) const {
    Value linearIndex;
    if (linearIndexOverride.has_value()) {
      linearIndex = *linearIndexOverride;
    } else {
      linearIndex = indices.empty()
                        ? AC->createIndexConstant(0, loc)
                        : AC->computeLinearIndex(sizes, indices, loc);
    }

    Value reserveRoute = route;
    if (distributedOwnership) {
      Value linearIndexI32 = AC->castToInt(AC->Int32, linearIndex, loc);
      Value totalNodes = AC->getTotalNodes(loc);
      reserveRoute =
          AC->create<arith::RemUIOp>(loc, linearIndexI32, totalNodes);
    }

    /// Reserve GUID for the DB (v2: use ARTS_DB type for all datablocks)
    ArtsCodegen::RuntimeCallBuilder RCB(*AC, loc);
    Value dbTypeConst = AC->createIntConstant(ARTS_DB, AC->Int32, loc);
    auto guid =
        RCB.call(types::ARTSRTL_arts_guid_reserve, {dbTypeConst, reserveRoute});

    /// Store reserved GUID in the linearized guid memref.
    AC->create<memref::StoreOp>(loc, guid, guidMemref, ValueRange{linearIndex});

    /// Optionally create DB and store pointer in the linearized db memref.
    if (createDb) {
      std::optional<int64_t> baseId = std::nullopt;
      if (nextId && nextId->has_value()) {
        if (indices.empty() && !linearIndexOverride.has_value()) {
          baseId = **nextId;
          **nextId = **nextId + 1;
        } else {
          baseId = **nextId;
        }
      }
      createDbFromGuidAtIndex(dbMemref, guid, linearIndex, elementSize, baseId,
                              loc);
    }
  }

  void createMultiDbs(Value dbMemref, Value guidMemref, ArrayRef<Value> sizes,
                      Value route, Value elementSize,
                      std::optional<int64_t> *nextId, Location loc,
                      bool distributedOwnership = false,
                      bool createDb = true) const {
    Value totalElems = AC->computeTotalElements(sizes, loc);
    /// Keep DB creation always linearized here. The dedicated GuidRangCallOpt
    /// pass handles reserve->reserve_range promotion centrally after
    /// ARTS-to-LLVM conversion.
    auto lowerBound = AC->createIndexConstant(0, loc);
    auto step = AC->createIndexConstant(1, loc);
    auto linearLoop = AC->create<scf::ForOp>(loc, lowerBound, totalElems, step);
    auto &loopBlock = linearLoop.getRegion().front();
    AC->setInsertionPointToStart(&loopBlock);
    Value linearIndex = linearLoop.getInductionVar();
    createSingleDb(dbMemref, guidMemref, route, elementSize, nextId, loc,
                   distributedOwnership, createDb, /*sizes=*/{},
                   /*indices=*/{}, /*linearIndexOverride=*/linearIndex);
    AC->setInsertionPointAfter(linearLoop);
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
    if (sourceSizes.empty())
      sourceSizes = resolveSourceOuterSizes(sourceGuid, sourcePtr, AC, loc);

    if (!sourceSizes.empty()) {
      SmallVector<Value> indices(op.getIndices().begin(),
                                 op.getIndices().end());
      SmallVector<Value> strides =
          AC->computeStridesFromSizes(sourceSizes, loc);
      /// Guid llvm type - underlying storage is always linear (rank-1)
      /// regardless of DbAcquireOp's multi-dimensional sizes
      auto origGuidMT = dyn_cast<MemRefType>(op.getGuid().getType());
      auto llvmGuidType = LLVM::LLVMPointerType::get(
          AC->getContext(), origGuidMT.getMemorySpaceAsInt());
      auto loadedGuid =
          AC->create<DbGepOp>(loc, llvmGuidType, sourceGuid, indices, strides);
      /// Ptr llvm type - same: underlying storage is linear
      auto origPtrMT = dyn_cast<MemRefType>(op.getPtr().getType());
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
    Operation *rootOp = ValueAnalysis::getUnderlyingOperation(source);
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
        if (auto intAttr = dyn_cast<IntegerAttr>(cst.getValue())) {
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
    auto resultType = dyn_cast<MemRefType>(op.getResult().getType());
    if (!resultType)
      return op.emitError("Expected MemRef type for result");
    return success();
  }
};

///===----------------------------------------------------------------------===///
/// Terminator Patterns
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

///===----------------------------------------------------------------------===///
/// Pass Implementation
///===----------------------------------------------------------------------===///
namespace {
struct ConvertArtsToLLVMPass
    : public impl::ConvertArtsToLLVMBase<ConvertArtsToLLVMPass> {

  explicit ConvertArtsToLLVMPass(bool debug = false,
                                 bool distributedInitPerWorker = false,
                                 const AbstractMachine *machine = nullptr)
      : debugMode(debug), distributedInitPerWorker(distributedInitPerWorker),
        machine(machine) {}

  void runOnOperation() override;

private:
  void populateCorePatterns(RewritePatternSet &patterns);
  void populateDbPatterns(RewritePatternSet &patterns);

  //// Member variables
  ArtsCodegen *AC = nullptr;
  bool debugMode = false;
  bool distributedInitPerWorker = false;
  const AbstractMachine *machine = nullptr;
};
} // namespace

///===----------------------------------------------------------------------===///
/// Core Pass Implementation
///===----------------------------------------------------------------------===///

void ConvertArtsToLLVMPass::runOnOperation() {
  ModuleOp module = getOperation();
  MLIRContext *context = &getContext();

  ARTS_INFO_HEADER(ConvertArtsToLLVMPass);
  ARTS_DEBUG_REGION(module.dump(););

  //// Initialize codegen infrastructure
  auto ownedAC = std::make_unique<ArtsCodegen>(module, debugMode);
  AC = ownedAC.get();
  AC->setDistributedInitInWorkers(distributedInitPerWorker);
  AC->setAbstractMachine(machine);
  ARTS_DEBUG_TYPE("ArtsCodegen initialized successfully");

  //// Apply patterns with greedy rewriter (two runs)
  GreedyRewriteConfig config;
  config.setUseTopDownTraversal(true);
  config.setRegionSimplificationLevel(GreedySimplifyRegionLevel::Aggressive);

  /// Run 1: core patterns
  {
    ARTS_INFO("Running core patterns");
    RewritePatternSet corePatterns(context);
    populateCorePatterns(corePatterns);
    if (failed(
            applyPatternsGreedily(module, std::move(corePatterns), config))) {
      ARTS_ERROR("Failed to apply core ARTS to LLVM conversion patterns");
      return signalPassFailure();
    }
  }

  /// Run 2: Db Patterns
  {
    ARTS_INFO("Running db patterns");
    RewritePatternSet dbPatterns(context);
    populateDbPatterns(dbPatterns);
    if (failed(applyPatternsGreedily(module, std::move(dbPatterns), config))) {
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
    if (failed(
            applyPatternsGreedily(module, std::move(otherPatterns), config))) {
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
  patterns.add<RuntimeQueryPattern>(context, AC);

  /// Synchronization patterns
  patterns.add<BarrierPattern, AtomicAddPattern>(context, AC);

  /// Builtin patterns (Polygeist emits these as calls, not intrinsics)
  patterns.add<BuiltinObjectSizePattern>(context);

  /// Epoch patterns
  patterns.add<CreateEpochPattern, WaitOnEpochPattern, GetEdtEpochGuidPattern>(
      context, AC);

  /// EDT patterns
  patterns.add<EdtParamPackPattern, EdtParamUnpackPattern>(context, AC);
  patterns.add<EdtCreatePattern>(context, AC);

  /// Dep patterns
  patterns.add<DepGepOpPattern>(context, AC);
  patterns.add<RecordDepPattern, IncrementDepPattern>(context, AC);
}

void ConvertArtsToLLVMPass::populateDbPatterns(RewritePatternSet &patterns) {
  MLIRContext *context = patterns.getContext();
  /// DB patterns
  patterns.add<DbControlPattern>(context, AC);
  patterns.add<DbAcquirePattern, DbReleasePattern>(context, AC);
  patterns.add<DbGepOpPattern, DepDbAcquireOpPattern>(context, AC);
}

///===----------------------------------------------------------------------===///
/// Pass Functions
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createConvertArtsToLLVMPass() {
  return std::make_unique<ConvertArtsToLLVMPass>();
}

std::unique_ptr<Pass> createConvertArtsToLLVMPass(bool debug) {
  return std::make_unique<ConvertArtsToLLVMPass>(debug, false);
}

std::unique_ptr<Pass>
createConvertArtsToLLVMPass(bool debug, bool distributedInitPerWorker) {
  return std::make_unique<ConvertArtsToLLVMPass>(
      debug, distributedInitPerWorker, nullptr);
}

std::unique_ptr<Pass>
createConvertArtsToLLVMPass(bool debug, bool distributedInitPerWorker,
                            const AbstractMachine *machine) {
  return std::make_unique<ConvertArtsToLLVMPass>(
      debug, distributedInitPerWorker, machine);
}
} // namespace arts
} // namespace mlir
