///==========================================================================///
/// File: ArtsRtOpToLLVMPatterns.cpp
///
/// ARTS-RT operation to LLVM conversion patterns.
/// These patterns convert runtime-shaped ARTS-RT ops into LLVM runtime calls
/// via the ArtsCodegen infrastructure owned by ConvertArtsRtToLLVM.
///==========================================================================///

#include "ConvertArtsRtToLLVMInternal.h"

#include "CodegenInternal.h"
#include "carts/dialect/arts-rt/IR/RtDialect.h"
#include "carts/dialect/arts-rt/Utils/RtDbUtils.h"
#include "carts/dialect/arts/Utils/LoweringFactUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/PartitionPredicates.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Conversion/LLVMCommon/StructBuilder.h"
#include "mlir/Dialect/Arith/Utils/Utils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/PatternMatch.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"

#include "carts/utils/Debug.h"
#include "llvm/ADT/Statistic.h"
ARTS_DEBUG_SETUP(convert_arts_rt_to_llvm);

static llvm::Statistic numEdtOpsConverted{
    "arts_rt_to_llvm", "NumEdtOpsConverted",
    "Number of EDT operations converted to LLVM runtime calls"};
static llvm::Statistic numEpochOpsConverted{
    "arts_rt_to_llvm", "NumEpochOpsConverted",
    "Number of epoch operations converted to LLVM runtime calls"};
static llvm::Statistic numDepOpsConverted{
    "arts_rt_to_llvm", "NumDepOpsConverted",
    "Number of dependency operations converted to LLVM runtime calls"};
static llvm::Statistic numDbOpsConverted{
    "arts_rt_to_llvm", "NumDbOpsConverted",
    "Number of DataBlock operations converted to LLVM runtime calls"};

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;
using namespace mlir::carts::arts_rt::convert_arts_rt_to_llvm;
using namespace mlir::carts::arts_rt;

///===----------------------------------------------------------------------===///
/// Epoch Patterns
///===----------------------------------------------------------------------===///

struct CreateEpochPattern : public ArtsRtToLLVMPattern<CreateEpochOp> {
  using ArtsRtToLLVMPattern::ArtsRtToLLVMPattern;

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

    Value epochGuid = AC->createEpoch(guid, edtSlot, loc);
    rewriter.replaceOp(op, epochGuid);
    ++numEpochOpsConverted;
    return success();
  }
};

/// Pattern to convert arts.wait_on_epoch operations
struct WaitOnEpochPattern : public ArtsRtToLLVMPattern<WaitOnEpochOp> {
  using ArtsRtToLLVMPattern::ArtsRtToLLVMPattern;

  LogicalResult matchAndRewrite(WaitOnEpochOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering WaitOnEpoch Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto loc = op.getLoc();
    Value waitOk = AC->waitOnHandle(op.getEpochGuid(), loc);
    Value falseI1 = AC->create<arith::ConstantIntOp>(loc, 0, 1);
    Value waitFailed = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::eq,
                                                 waitOk, falseI1);
    auto failIf = AC->create<scf::IfOp>(loc, waitFailed, false);
    rewriter.setInsertionPointToStart(&failIf.getThenRegion().front());
    AC->createRuntimeCall(ARTSRTL_arts_shutdown, {}, loc);
    AC->create<LLVM::Trap>(loc);
    rewriter.setInsertionPointAfter(failIf);
    rewriter.eraseOp(op);
    ++numEpochOpsConverted;
    return success();
  }
};

///===----------------------------------------------------------------------===///
/// Dependency Patterns
///===----------------------------------------------------------------------===///

static Value getDepEntryFieldPtr(ArtsCodegen *AC, Value depEntryPtr,
                                 unsigned field, Location loc) {
  auto c0 = AC->createIntConstant(0, AC->Int64, loc);
  auto fieldIdx = AC->createIntConstant(field, AC->Int64, loc);
  return AC->create<LLVM::GEPOp>(loc, AC->llvmPtr, AC->ArtsEdtDep, depEntryPtr,
                                 ValueRange{c0, fieldIdx});
}

static Value buildDepReadablePayloadPtr(ArtsCodegen *AC, Value depEntryPtr,
                                        Value payloadPtr, Location loc) {
  Value flagsPtr = getDepEntryFieldPtr(AC, depEntryPtr, /*field=*/3, loc);
  Value offsetPtr = getDepEntryFieldPtr(AC, depEntryPtr, /*field=*/4, loc);
  Value flags = AC->create<LLVM::LoadOp>(loc, AC->Int32, flagsPtr);
  Value sliceOffset = AC->create<LLVM::LoadOp>(loc, AC->Int64, offsetPtr);

  Value haloMask = AC->createIntConstant(kArtsDepFlagHaloView, AC->Int32, loc);
  Value compactMask =
      AC->createIntConstant(kArtsDepFlagHaloCompact, AC->Int32, loc);
  Value zeroI32 = AC->createIntConstant(0, AC->Int32, loc);
  Value haloBits = AC->create<arith::AndIOp>(loc, flags, haloMask);
  Value compactBits = AC->create<arith::AndIOp>(loc, flags, compactMask);
  Value isHalo = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ne,
                                           haloBits, zeroI32);
  Value isCompact = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ne,
                                              compactBits, zeroI32);
  Value notCompact = AC->create<arith::XOrIOp>(
      loc, isCompact, AC->create<arith::ConstantIntOp>(loc, 1, 1));
  Value needsOffset = AC->create<arith::AndIOp>(loc, isHalo, notCompact);

  Value shifted = AC->create<LLVM::GEPOp>(loc, AC->llvmPtr, AC->Int8,
                                          payloadPtr, ValueRange{sliceOffset});
  return LLVM::SelectOp::create(AC->getBuilder(), loc, AC->llvmPtr, needsOffset,
                                shifted, payloadPtr);
}

static Value buildDepReadablePayloadSlotPtr(ArtsCodegen *AC, Value depEntryPtr,
                                            Value dataPtrAddr, Location loc) {
  Value payloadPtr = AC->create<LLVM::LoadOp>(loc, AC->llvmPtr, dataPtrAddr);
  Value adjustedPayload =
      buildDepReadablePayloadPtr(AC, depEntryPtr, payloadPtr, loc);

  Value flagsPtr = getDepEntryFieldPtr(AC, depEntryPtr, /*field=*/3, loc);
  Value flags = AC->create<LLVM::LoadOp>(loc, AC->Int32, flagsPtr);
  Value haloMask = AC->createIntConstant(kArtsDepFlagHaloView, AC->Int32, loc);
  Value compactMask =
      AC->createIntConstant(kArtsDepFlagHaloCompact, AC->Int32, loc);
  Value zeroI32 = AC->createIntConstant(0, AC->Int32, loc);
  Value haloBits = AC->create<arith::AndIOp>(loc, flags, haloMask);
  Value compactBits = AC->create<arith::AndIOp>(loc, flags, compactMask);
  Value isHalo = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ne,
                                           haloBits, zeroI32);
  Value isCompact = AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ne,
                                              compactBits, zeroI32);
  Value notCompact = AC->create<arith::XOrIOp>(
      loc, isCompact, AC->create<arith::ConstantIntOp>(loc, 1, 1));
  Value needsOffset = AC->create<arith::AndIOp>(loc, isHalo, notCompact);

  auto slotType = MemRefType::get({1}, AC->llvmPtr);
  Value adjustedSlot = AC->create<memref::AllocaOp>(loc, slotType);
  Value zeroIdx = AC->createIndexConstant(0, loc);
  AC->create<memref::StoreOp>(loc, adjustedPayload, adjustedSlot,
                              ValueRange{zeroIdx});
  Value adjustedSlotPtr = AC->castToLLVMPtr(adjustedSlot, loc);
  return LLVM::SelectOp::create(AC->getBuilder(), loc, AC->llvmPtr, needsOffset,
                                adjustedSlotPtr, dataPtrAddr);
}

/// Pattern to convert arts.record_dep operations
struct RecordDepPattern : public ArtsRtToLLVMPattern<RecordDepOp> {
  using ArtsRtToLLVMPattern::ArtsRtToLLVMPattern;

  LogicalResult matchAndRewrite(RecordDepOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering RecordInDep Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto loc = op.getLoc();
    auto edtGuid = op.getEdtGuid();

    auto accessMode = op.getAccessMode();
    auto acquireModesAttr = op.getAcquireModes();
    if (!acquireModesAttr)
      return op.emitOpError()
             << "requires acquire_modes for every datablock; ARTS-RT must not "
                "infer DB dependency modes";
    ArrayRef<int32_t> acquireModeValues = *acquireModesAttr;
    auto depFlagsAttr = op.getDepFlags();
    ArrayRef<int32_t> depFlagValues =
        depFlagsAttr ? *depFlagsAttr : ArrayRef<int32_t>{};

    auto boundsValids = op.getBoundsValids();

    auto byteOffsets = op.getByteOffsets();
    auto byteSizes = op.getByteSizes();

    auto slotTy = MemRefType::get({}, AC->Int32);
    Value sharedSlotAlloc = AC->create<memref::AllocaOp>(loc, slotTy);
    Value zeroI32 = AC->createIntConstant(0, AC->Int32, loc);
    AC->create<memref::StoreOp>(loc, zeroI32, sharedSlotAlloc);
    unsigned dbIdx = 0;
    for (Value dbGuid : op.getDatablocks()) {
      std::optional<int32_t> acquireMode = std::nullopt;
      if (dbIdx < acquireModeValues.size())
        acquireMode = acquireModeValues[dbIdx];
      std::optional<int32_t> depFlags = std::nullopt;
      if (dbIdx < depFlagValues.size())
        depFlags = depFlagValues[dbIdx];
      Value boundsValid =
          (dbIdx < boundsValids.size()) ? boundsValids[dbIdx] : Value();
      Value byteOffset =
          (dbIdx < byteOffsets.size()) ? byteOffsets[dbIdx] : Value();
      Value byteSize = (dbIdx < byteSizes.size()) ? byteSizes[dbIdx] : Value();
      if (failed(recordDepsForDb(dbGuid, edtGuid, sharedSlotAlloc, accessMode,
                                 acquireMode, depFlags, boundsValid, byteOffset,
                                 byteSize, loc)))
        return failure();
      ++dbIdx;
    }

    rewriter.eraseOp(op);
    ++numDepOpsConverted;
    return success();
  }

private:
  SmallVector<Value, 4>
  inferStencilCenterCoordsFromFacts(DbAcquireOp dbAcquireOp,
                                    const DbLoweringInfo &dbInfo,
                                    Location loc) const {
    SmallVector<Value, 4> globalCoords;
    if (!dbAcquireOp || dbInfo.sizes.empty())
      return globalCoords;

    auto facts = getAcquireStencilFacts(dbAcquireOp, loc);
    if (!facts ||
        !(facts->isStencilFamily() || facts->usesStencilDistribution()))
      return globalCoords;
    if (facts->spatial.minOffsets.empty()) {
      if (facts->spatial.centerOffset)
        return inferSymmetricStencilCenterCoords(*facts->spatial.centerOffset,
                                                 dbInfo, loc);
      return globalCoords;
    }

    unsigned rank = std::min<unsigned>(dbInfo.sizes.size(),
                                       facts->spatial.minOffsets.size());
    if (!facts->spatial.writeFootprint.empty())
      rank = std::min<unsigned>(rank, facts->spatial.writeFootprint.size());
    if (rank == 0)
      return globalCoords;

    Value zero = AC->createIndexConstant(0, loc);
    Value one = AC->createIndexConstant(1, loc);

    globalCoords.reserve(rank);
    for (unsigned i = 0; i < rank; ++i) {
      Value writeCoord = facts->spatial.writeFootprint.empty()
                             ? zero
                             : facts->spatial.writeFootprint[i];
      Value minOffset = facts->spatial.minOffsets[i];
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

    DbAllocOp allocOp = RtDbUtils::getAllocOpFromGuid(dbGuid);

    if (allocOp && !allocOp.getSizes().empty())
      return AC->computeTotalElements(allocOp.getSizes(), allocOp.getLoc());
    SmallVector<Value, 4> sizes = resolveOuterSizesForGuid(dbGuid);
    if (!sizes.empty())
      return AC->computeTotalElements(sizes, dbGuid.getLoc());
    return nullptr;
  }

  /// Holds extracted dependency info for a single datablock source.
  struct DepDbInfo {
    DbLoweringInfo dbInfo;
    /// The originating acquire, used only to fail closed if a stale halo_slice
    /// reaches ARTS-RT without explicit byte windows.
    DbAcquireOp dbAcquireOp = nullptr;
    SmallVector<Value, 4> allocSizes;
    Value guidStorage = nullptr;
    Value depStruct = nullptr;
    Value baseOffset = nullptr;
    Value stencilCenterLinear;
    SmallVector<Value, 4> stencilCenterCoords;
    std::optional<LoweringFactInfo> stencilFacts;
  };
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
    SmallVector<Value> localCoords = AC->computeIndicesFromLinearIndex(
        ArrayRef<Value>(dbInfo.sizes).take_front(rank), localLinear, loc);
    if (localCoords.size() != rank)
      return AC->castToIndex(localLinear, loc);

    SmallVector<OpFoldResult, 4> globalCoords;
    globalCoords.reserve(rank);
    for (auto [localCoord, offset] : llvm::zip(localCoords, dbInfo.offsets)) {
      Value globalCoord = AC->create<arith::AddIOp>(
          loc, localCoord, AC->castToIndex(offset, loc));
      globalCoords.push_back(globalCoord);
    }

    SmallVector<Value> globalCoordValues;
    globalCoordValues.reserve(globalCoords.size());
    for (OpFoldResult coord : globalCoords)
      globalCoordValues.push_back(
          getValueOrCreateConstantIndexOp(AC->getBuilder(), loc, coord));

    return AC->computeLinearIndex(allocSizes.take_front(rank),
                                  globalCoordValues, loc);
  }

  std::optional<LoweringFactInfo>
  getAcquireStencilFacts(DbAcquireOp dbAcquireOp, Location loc) const {
    if (!dbAcquireOp)
      return std::nullopt;

    if (auto info = getLoweringFacts(dbAcquireOp.getPtr()))
      return info;
    return getLoweringFacts(dbAcquireOp.getOperation(), AC->getBuilder(), loc);
  }

  Value inferStencilCenterLinearFromFacts(DbAcquireOp dbAcquireOp,
                                          const DbLoweringInfo &dbInfo,
                                          ArrayRef<Value> allocSizes,
                                          Location loc) const {
    if (!dbAcquireOp || dbInfo.sizes.empty())
      return nullptr;

    auto facts = getAcquireStencilFacts(dbAcquireOp, loc);
    if (!facts ||
        !(facts->isStencilFamily() || facts->usesStencilDistribution()))
      return nullptr;
    if (facts->spatial.minOffsets.empty()) {
      if (facts->spatial.centerOffset)
        return inferSymmetricStencilCenterLinear(*facts->spatial.centerOffset,
                                                 dbInfo, allocSizes, loc);
      return nullptr;
    }

    unsigned rank = std::min<unsigned>(dbInfo.sizes.size(),
                                       facts->spatial.minOffsets.size());
    if (!facts->spatial.writeFootprint.empty())
      rank = std::min<unsigned>(rank, facts->spatial.writeFootprint.size());
    if (rank == 0)
      return nullptr;

    Value zero = AC->createIndexConstant(0, loc);
    Value one = AC->createIndexConstant(1, loc);

    SmallVector<Value, 4> localCoords;
    localCoords.reserve(rank);
    for (unsigned i = 0; i < rank; ++i) {
      Value writeCoord = facts->spatial.writeFootprint.empty()
                             ? zero
                             : facts->spatial.writeFootprint[i];
      Value minOffset = facts->spatial.minOffsets[i];
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

    Value localLinearIndex = AC->computeLinearIndex(
        ArrayRef<Value>(dbInfo.sizes).take_front(rank), localCoords, loc);

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

    SmallVector<Value, 4> localCoords;
    localCoords.reserve(dbInfo.sizes.size());
    for (unsigned i = 0; i < dbInfo.sizes.size(); ++i) {
      Value dimSize = AC->castToIndex(dbInfo.sizes[i], loc);
      Value sizeMinusOne = AC->create<arith::SubIOp>(loc, dimSize, one);
      sizeMinusOne = AC->create<arith::MaxSIOp>(loc, sizeMinusOne, zero);
      localCoords.push_back(
          AC->create<arith::MinSIOp>(loc, center, sizeMinusOne));
    }

    Value localLinearIndex =
        AC->computeLinearIndex(dbInfo.sizes, localCoords, loc);

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
  FailureOr<DepDbInfo> extractDbInfoForDeps(Value dbGuid,
                                            std::optional<int32_t> acquireMode,
                                            Location loc) const {
    DepDbInfo result;

    /// Resolve the underlying DB operation, tracing through pointer casts
    /// (polygeist.pointer2memref, memref.cast, etc.) that may wrap the GUID.
    auto dbAcquireOp = dbGuid.getDefiningOp<DbAcquireOp>();
    auto depDbAcquireOp = dbGuid.getDefiningOp<DepDbAcquireOp>();
    if (!dbAcquireOp && !depDbAcquireOp) {
      Operation *underlying = RtDbUtils::getUnderlyingDb(dbGuid);
      if (underlying) {
        dbAcquireOp = dyn_cast<DbAcquireOp>(underlying);
        depDbAcquireOp = dyn_cast<DepDbAcquireOp>(underlying);
      }
    }

    if (dbAcquireOp) {
      result.dbInfo = RtDbUtils::extractDbLoweringInfo(dbAcquireOp);
      result.dbAcquireOp = dbAcquireOp;
      result.guidStorage =
          dbAcquireOp.getSourceGuid() ? dbAcquireOp.getSourceGuid() : dbGuid;
      result.allocSizes = resolveOuterSizesForGuid(dbGuid);
      /// Stencil writer acquires frequently cover [halo..., center, halo...]
      /// DB entries. Recording every entry as WRITE over-serializes adjacent
      /// blocks. Use the acquire's stencil facts to identify the owned
      /// center block and downgrade only the non-center entries to read-only.
      ///
      /// Prefer the full lowering facts so boundary-clamped windows keep
      /// the correct owned-center block. stencil_center_offset is only a
      /// symmetric-radius fallback when richer facts data is unavailable.
      int32_t writeMode = static_cast<int32_t>(DbMode::write);
      bool writerMode = acquireMode && *acquireMode == writeMode;
      auto partitionMode = dbAcquireOp.getPartitionMode();
      if (writerMode && partitionMode && usesBlockLayout(*partitionMode)) {
        result.stencilFacts = getAcquireStencilFacts(dbAcquireOp, loc);
        result.stencilCenterLinear = inferStencilCenterLinearFromFacts(
            dbAcquireOp, result.dbInfo, result.allocSizes, loc);
        result.stencilCenterCoords =
            inferStencilCenterCoordsFromFacts(dbAcquireOp, result.dbInfo, loc);
      }
    } else if (depDbAcquireOp) {
      result.dbInfo = RtDbUtils::extractDbLoweringInfo(depDbAcquireOp);
      rebaseDepIterationWindow(result, loc);
      result.guidStorage = depDbAcquireOp.getGuid();
      result.depStruct = depDbAcquireOp.getDepStruct();
      result.baseOffset = depDbAcquireOp.getOffset();
    } else {
      emitError(loc)
          << "cannot recover DB acquire provenance for dependency GUID; "
             "refusing single-element DB shape fallback";
      return failure();
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
    result.useDepv =
        depInfo.depStruct && depInfo.baseOffset &&
        (accessMode == DepAccessMode::from_depv ||
         isa_and_nonnull<DepDbAcquireOp>(RtDbUtils::getUnderlyingDb(dbGuid)));

    result.totalDBs = result.useDepv
                          ? Value()
                          : getTotalDBsForBoundsCheck(dbGuid, boundsValid);
    if (!result.useDepv)
      result.allocSizes = resolveOuterSizesForGuid(dbGuid);

    return result;
  }

  /// Iterate over DB elements and emit record-dep calls for each index.
  LogicalResult
  emitRecordDepCalls(Value dbGuid, Value edtGuid, Value sharedSlotAlloc,
                     DepAccessMode accessMode,
                     std::optional<int32_t> acquireMode,
                     std::optional<int32_t> depFlags, Value boundsValid,
                     Value byteOffset, Value byteSize, const DepDbInfo &depInfo,
                     const DepBoundsInfo &bounds, Location loc) const {
    auto guidStorageType = dyn_cast<MemRefType>(
        (depInfo.guidStorage ? depInfo.guidStorage : dbGuid).getType());
    bool useDirectCoords = !bounds.useDepv && guidStorageType &&
                           guidStorageType.getRank() > 1 &&
                           depInfo.dbInfo.sizes.size() > 1;
    if (useDirectCoords) {
      Value one = AC->createIndexConstant(1, loc);
      SmallVector<Value, 4> globalCoords;
      LogicalResult result = success();

      std::function<void(unsigned)> emitForCoords = [&](unsigned dim) {
        if (failed(result))
          return;
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

          result = recordSingleDb(
              dbGuid, depInfo.guidStorage, edtGuid, sharedSlotAlloc,
              linearIndex, ArrayRef<Value>(globalCoords), bounds.allocSizes,
              accessMode, acquireMode, depFlags, boundsValid, depInfo.depStruct,
              depInfo.baseOffset, bounds.totalDBs, byteOffset, byteSize,
              depInfo.stencilCenterLinear, depInfo.stencilCenterCoords,
              &depInfo, loc);
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
      return result;
    }

    if (!bounds.useDepv && depInfo.dbInfo.isSingleElement &&
        !depInfo.dbInfo.indices.empty()) {
      Value zero = AC->createIndexConstant(0, loc);
      return recordSingleDb(
          dbGuid, depInfo.guidStorage, edtGuid, sharedSlotAlloc, zero,
          depInfo.dbInfo.indices, bounds.allocSizes, accessMode, acquireMode,
          depFlags, boundsValid, depInfo.depStruct, depInfo.baseOffset,
          bounds.totalDBs, byteOffset, byteSize, depInfo.stencilCenterLinear,
          depInfo.stencilCenterCoords, &depInfo, loc);
    }

    LogicalResult result = success();
    AC->iterateDbElements(
        dbGuid, edtGuid, depInfo.dbInfo.sizes, depInfo.dbInfo.offsets,
        depInfo.dbInfo.isSingleElement, loc,
        [&](Value linearIndex) {
          if (failed(result))
            return;
          result = recordSingleDb(
              dbGuid, depInfo.guidStorage, edtGuid, sharedSlotAlloc,
              linearIndex, ArrayRef<Value>(), bounds.allocSizes, accessMode,
              acquireMode, depFlags, boundsValid, depInfo.depStruct,
              depInfo.baseOffset, bounds.totalDBs, byteOffset, byteSize,
              depInfo.stencilCenterLinear, depInfo.stencilCenterCoords,
              &depInfo, loc);
        },
        bounds.allocSizes);
    return result;
  }

  /// Record all dependencies for a single datablock
  LogicalResult recordDepsForDb(Value dbGuid, Value edtGuid,
                                Value sharedSlotAlloc, DepAccessMode accessMode,
                                std::optional<int32_t> acquireMode,
                                std::optional<int32_t> depFlags,
                                Value boundsValid, Value byteOffset,
                                Value byteSize, Location loc) const {
    if (!acquireMode)
      return emitError(loc)
             << "arts_rt.rec_dep requires an explicit acquire mode for every "
                "datablock; refusing write-mode fallback";
    FailureOr<DepDbInfo> maybeDepInfo =
        extractDbInfoForDeps(dbGuid, acquireMode, loc);
    if (failed(maybeDepInfo))
      return failure();
    DepDbInfo depInfo = *maybeDepInfo;
    DepBoundsInfo bounds =
        computeDepBounds(dbGuid, depInfo, accessMode, boundsValid);
    return emitRecordDepCalls(dbGuid, edtGuid, sharedSlotAlloc, accessMode,
                              acquireMode, depFlags, boundsValid, byteOffset,
                              byteSize, depInfo, bounds, loc);
  }

  static bool hasDepFlag(std::optional<int32_t> flags, int32_t mask) {
    return flags && ((*flags & mask) != 0);
  }

  static std::optional<int32_t> clearDepFlags(std::optional<int32_t> flags,
                                              int32_t mask) {
    if (!flags)
      return std::nullopt;
    int32_t bits = *flags & ~mask;
    if (bits == 0)
      return std::nullopt;
    return bits;
  }

  /// Emit the appropriate runtime call for recording a dependency.
  /// Standard path: arts_add_dependence(dbGuid, edtGuid, slot, mode)
  /// Halo path: arts_add_halo_dependence(dbGuid, edtGuid, slot, offset, size)
  LogicalResult emitRecordDepCall(Value dbGuidValue, Value edtGuidValue,
                                  Value currentSlotI32, Value modeValue,
                                  Value byteOffsetI64, Value byteSizeI64,
                                  std::optional<int32_t> depFlags,
                                  Location loc) const {
    const bool haloView = hasDepFlag(depFlags, kArtsDepFlagHaloView);
    std::optional<int32_t> wholeDbFlags =
        clearDepFlags(depFlags, kArtsDepFlagHaloView | kArtsDepFlagHaloCompact);
    std::optional<int32_t> haloFlags =
        clearDepFlags(depFlags, kArtsDepFlagHaloView | kArtsDepFlagHaloCompact);

    auto emitWholeDbDep = [&]() {
      ArtsCodegen::RuntimeCallBuilder RCB(*AC, loc);
      if (wholeDbFlags && *wholeDbFlags != 0) {
        Value flagsValue = AC->createIntConstant(*wholeDbFlags, AC->Int32, loc);
        RCB.callVoid(
            types::ARTSRTL_arts_add_dependence_ex,
            {dbGuidValue, edtGuidValue, currentSlotI32, modeValue, flagsValue});
      } else {
        RCB.callVoid(types::ARTSRTL_arts_add_dependence,
                     {dbGuidValue, edtGuidValue, currentSlotI32, modeValue});
      }
    };

    auto emitHaloDep = [&]() {
      ArtsCodegen::RuntimeCallBuilder RCB(*AC, loc);
      if (haloFlags && *haloFlags != 0) {
        Value flagsValue = AC->createIntConstant(*haloFlags, AC->Int32, loc);
        RCB.callVoid(types::ARTSRTL_arts_add_halo_dependence_ex,
                     {dbGuidValue, edtGuidValue, currentSlotI32, byteOffsetI64,
                      byteSizeI64, flagsValue});
      } else {
        RCB.callVoid(types::ARTSRTL_arts_add_halo_dependence,
                     {dbGuidValue, edtGuidValue, currentSlotI32, byteOffsetI64,
                      byteSizeI64});
      }
    };

    if (haloView) {
      if (!byteOffsetI64 || !byteSizeI64) {
        return mlir::emitError(loc)
               << "HALO_VIEW dependency requires an explicit byte window";
      }
      Value normalizedByteSize = ValueAnalysis::stripNumericCasts(byteSizeI64);
      if (!ValueAnalysis::isProvablyNonZero(normalizedByteSize))
        return mlir::emitError(loc)
               << "HALO_VIEW dependency requires provably nonzero byte_size";
      emitHaloDep();
      return success();
    }

    if (!byteOffsetI64 || !byteSizeI64) {
      emitWholeDbDep();
      return success();
    }

    Value normalizedByteSize = ValueAnalysis::stripNumericCasts(byteSizeI64);
    /// byte_size == 0 is the cross-pass sentinel for "no partial slice".
    /// Respect it both when the zero is constant and when it only becomes
    /// known after runtime guards (whole-block or center-block fallback).
    if (ValueAnalysis::isZeroConstant(normalizedByteSize)) {
      emitWholeDbDep();
      return success();
    }
    return mlir::emitError(loc)
           << "explicit byte window without HALO_VIEW is unsupported; ARTS-RT "
              "must lower stencil halos through arts_add_halo_dependence or "
              "use a whole-DB dependency";
  }

  /// Record a single DB dependency, using a guarded null signal for invalid
  /// boundary slots and the halo runtime API for explicit byte windows.
  LogicalResult recordSingleDb(
      Value dbGuid, Value guidStorage, Value edtGuid, Value slotAlloc,
      Value linearIndex, ArrayRef<Value> directIndices,
      ArrayRef<Value> directLayoutSizes, DepAccessMode accessMode,
      std::optional<int32_t> acquireMode, std::optional<int32_t> depFlags,
      Value boundsValid, Value depStruct, Value baseOffset, Value totalDBs,
      Value byteOffset, Value byteSize, Value stencilCenterLinear,
      ArrayRef<Value> stencilCenterCoords, const DepDbInfo *depInfo,
      Location loc) const {
    const bool useDepv =
        depStruct && baseOffset &&
        (accessMode == DepAccessMode::from_depv ||
         isa_and_nonnull<DepDbAcquireOp>(RtDbUtils::getUnderlyingDb(dbGuid)));

    /// Extend boundsValid with DB index check for stencil cases.
    /// When the allocation is coarse (totalDBs == 1), all workers bind to the
    /// single partition — clamp linearIndex to 0 so the bounds check passes
    /// and all workers receive the dependency.
    Value effectiveBoundsValid = boundsValid;
    if (totalDBs) {
      if (ValueAnalysis::isOneConstant(totalDBs))
        linearIndex = AC->createIndexConstant(0, loc);
      Value indexInBounds = AC->create<arith::CmpIOp>(
          loc, arith::CmpIPredicate::ult, linearIndex, totalDBs);
      effectiveBoundsValid =
          AC->create<arith::AndIOp>(loc, boundsValid, indexInBounds);
    }

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
    if (!acquireMode)
      return emitError(loc)
             << "arts_rt.rec_dep requires an explicit acquire mode for every "
                "datablock; refusing write-mode fallback";
    int32_t modeInt = *acquireMode;
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

    Value effectiveByteOffset = byteOffset;
    Value effectiveByteSize = byteSize;
    std::optional<int32_t> effectiveDepFlags = depFlags;
    bool preserveShape =
        depFlags && ((*depFlags & kArtsDepFlagPreserveShape) != 0);
    bool hasExplicitSlice = byteOffset && byteSize &&
                            ValueAnalysis::isProvablyNonZero(
                                ValueAnalysis::stripNumericCasts(byteSize));
    DbAcquireOp sourceAcquire = depInfo ? depInfo->dbAcquireOp : DbAcquireOp();
    if (!hasExplicitSlice && sourceAcquire && sourceAcquire.getHaloSliceAttr())
      return sourceAcquire.emitOpError()
             << "carries halo_slice without an explicit provably nonzero byte "
                "window; ARTS-RT must not infer halo face slices";
    if (preserveShape) {
      /// Explicit preserve-shape markings currently act as an analysis-time
      /// "do not compact this acquire" facts. Keep those on the whole-DB
      /// path until the upstream acquire rewrite carries a compact index space.
      effectiveByteOffset = nullptr;
      effectiveByteSize = nullptr;
      if (effectiveDepFlags) {
        int32_t depFlagBits = *effectiveDepFlags & ~kArtsDepFlagPreserveShape;
        if (depFlagBits == 0)
          effectiveDepFlags.reset();
        else
          effectiveDepFlags = depFlagBits;
      }
    }

    bool hasPartialSlice =
        effectiveByteOffset && effectiveByteSize &&
        !ValueAnalysis::isZeroConstant(
            ValueAnalysis::stripNumericCasts(effectiveByteSize));
    if (hasPartialSlice && modeInt != readMode && !isCenterBlock)
      return emitError(loc)
             << "write-mode dependency carries a committed byte window but has "
                "no center-block semantics; refusing whole-DB widening "
                "fallback";
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
      auto ifOp = AC->create<scf::IfOp>(loc, effectiveBoundsValid,
                                        /*withElseRegion=*/true);

      AC->setInsertionPointToStart(&ifOp.getThenRegion().front());
      Value dbGuidValue = loadDbGuidValue(dbGuid, guidStorage, linearIndex,
                                          directIndices, directLayoutSizes,
                                          useDepv, depStruct, baseOffset, loc);
      if (failed(emitRecordDepCall(dbGuidValue, edtGuidValue, currentSlotI32,
                                   modeValue, byteOffsetI64, byteSizeI64,
                                   effectiveDepFlags, loc)))
        return failure();

      AC->setInsertionPointToStart(&ifOp.getElseRegion().front());
      ArtsCodegen::RuntimeCallBuilder RCB(*AC, loc);
      RCB.callVoid(types::ARTSRTL_arts_signal_edt_null,
                   {edtGuidValue, currentSlotI32});

      AC->setInsertionPointAfter(ifOp);
    } else {
      Value dbGuidValue = loadDbGuidValue(dbGuid, guidStorage, linearIndex,
                                          directIndices, directLayoutSizes,
                                          useDepv, depStruct, baseOffset, loc);
      if (failed(emitRecordDepCall(dbGuidValue, edtGuidValue, currentSlotI32,
                                   modeValue, byteOffsetI64, byteSizeI64,
                                   effectiveDepFlags, loc)))
        return failure();
    }

    auto oneI32 = AC->createIntConstant(1, AC->Int32, loc);
    auto incrementedSlot =
        AC->create<arith::AddIOp>(loc, currentSlotI32, oneI32);
    AC->create<memref::StoreOp>(loc, incrementedSlot, slotAlloc);
    return success();
  }
};

///===----------------------------------------------------------------------===///
/// EDT Patterns
///===----------------------------------------------------------------------===///

/// Pattern to convert arts.edt_param_pack operations
struct EdtParamPackPattern : public ArtsRtToLLVMPattern<EdtParamPackOp> {
  using ArtsRtToLLVMPattern::ArtsRtToLLVMPattern;

  LogicalResult matchAndRewrite(EdtParamPackOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering EdtParamPack Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);

    auto loc = op.getLoc();
    auto params = op.getParams();
    auto resultType = dyn_cast<MemRefType>(op.getMemref().getType());
    if (!resultType)
      return op.emitError("Expected MemRef type for result");

    memref::AllocaOp allocOp;
    if (params.empty() ||
        (resultType.hasStaticShape() && resultType.getNumElements() == 0)) {
      /// Empty parameter pack: allocate dynamic memref<?xi64> with size 0
      auto dynamicType = MemRefType::get({ShapedType::kDynamic}, AC->Int64);
      auto zeroIndex = AC->createIndexConstant(0, loc);
      allocOp =
          AC->create<memref::AllocaOp>(loc, dynamicType, ValueRange{zeroIndex});
    } else if (resultType.getNumDynamicDims() > 0) {
      /// Dynamic memref: allocate with runtime size and store parameters.
      auto numParams = AC->createIndexConstant(params.size(), loc);
      allocOp =
          AC->create<memref::AllocaOp>(loc, resultType, ValueRange{numParams});
    } else {
      /// Static non-empty pack: preserve its compile-time extent.
      allocOp = AC->create<memref::AllocaOp>(loc, resultType, ValueRange{});
    }

    for (unsigned i = 0; i < params.size(); ++i) {
      auto index = AC->createIndexConstant(i, loc);
      auto castParam = AC->castParameter(
          AC->Int64, params[i], loc, ArtsCodegen::ParameterCastMode::Bitwise);
      AC->create<memref::StoreOp>(loc, castParam, allocOp, ValueRange{index});
    }

    rewriter.replaceOp(op, allocOp);
    ++numEdtOpsConverted;
    return success();
  }
};

/// Pattern to convert arts.edt_param_unpack operations
struct EdtParamUnpackPattern : public ArtsRtToLLVMPattern<EdtParamUnpackOp> {
  using ArtsRtToLLVMPattern::ArtsRtToLLVMPattern;

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
    ++numEdtOpsConverted;
    return success();
  }
};

///===----------------------------------------------------------------------===///
/// Dependency GEP and Acquire Patterns
///===----------------------------------------------------------------------===///

/// Pattern to convert arts.dep_gep operations
struct DepGepOpPattern : public ArtsRtToLLVMPattern<DepGepOp> {
  using ArtsRtToLLVMPattern::ArtsRtToLLVMPattern;

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

    /// Extract both guid (field #0) and ptr (field #1) from depv.
    auto c0 = AC->createIntConstant(0, AC->Int64, loc);
    auto cPtrIdx = AC->createIntConstant(1, AC->Int64, loc);

    /// Get the guid pointer (field #0)
    auto guidPtr = AC->create<LLVM::GEPOp>(loc, AC->llvmPtr, AC->ArtsEdtDep,
                                           depEntryPtr, ValueRange{c0, c0});

    /// Get the data pointer (field #1)
    auto dataPtr = AC->create<LLVM::GEPOp>(
        loc, AC->llvmPtr, AC->ArtsEdtDep, depEntryPtr, ValueRange{c0, cPtrIdx});
    Value readableDataPtr =
        buildDepReadablePayloadSlotPtr(AC, depEntryPtr, dataPtr, loc);

    /// Return both: guid pointer and data pointer
    rewriter.replaceOp(op, ValueRange{guidPtr, readableDataPtr});
    ++numDepOpsConverted;
    return success();
  }
};

/// Pattern to convert arts.dep_db_acquire operations
struct DepDbAcquireOpPattern : public ArtsRtToLLVMPattern<DepDbAcquireOp> {
  using ArtsRtToLLVMPattern::ArtsRtToLLVMPattern;

  LogicalResult matchAndRewrite(DepDbAcquireOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering DepDbAcquire Op " << op);
    for (OpOperand &use : op.getGuid().getUses())
      if (isa<RecordDepOp>(use.getOwner()))
        return failure();
    for (Operation *user : op.getPtr().getUsers())
      if (auto acquire = dyn_cast<DbAcquireOp>(user))
        if (!acquire.getSourceGuid())
          return failure();

    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    auto loc = op.getLoc();
    auto depStruct = op.getDepStruct();

    Value typedDepPtr = AC->castToLLVMPtr(depStruct, loc);
    Value linearIndex = op.getOffset();
    if (linearIndex.getType() != AC->Int64)
      linearIndex = AC->ensureI64(linearIndex, loc);

    SmallVector<Value> strides;
    if (!op.getSizes().empty()) {
      SmallVector<Value> sizes(op.getSizes().begin(), op.getSizes().end());
      strides = AC->computeStridesFromSizes(sizes, loc);
    }

    auto indices = op.getIndices();
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

    auto c0 = AC->createIntConstant(0, AC->Int64, loc);
    auto cPtrIdx = AC->createIntConstant(1, AC->Int64, loc);

    Value guidPtr = AC->create<LLVM::GEPOp>(loc, AC->llvmPtr, AC->ArtsEdtDep,
                                            depEntryPtr, ValueRange{c0, c0});
    Value dataPtrAddr = AC->create<LLVM::GEPOp>(
        loc, AC->llvmPtr, AC->ArtsEdtDep, depEntryPtr, ValueRange{c0, cPtrIdx});
    Value payloadPtr = AC->create<LLVM::LoadOp>(loc, AC->llvmPtr, dataPtrAddr);
    Value readablePayloadPtr =
        buildDepReadablePayloadPtr(AC, depEntryPtr, payloadPtr, loc);

    /// DepDbAcquireOp feeds two distinct downstream shapes:
    /// - flat payload memrefs (e.g. memref<?x?xf32>) expect the dep entry's
    ///   ptr field to be resolved to the payload base pointer directly.
    /// - handle-table memrefs (e.g. memref<?x!llvm.ptr> or
    ///   memref<?xmemref<?xf64>>) still need one level of pointer indirection
    ///   preserved so later db_gep/load pairs can recover the concrete payload
    ///   pointer. Loading the dep entry here for those cases turns data bytes
    ///   into fake pointers at runtime.
    ///
    /// Some outlined paths may preserve memref<?xmemref<...>> types (the
    /// original pre-DbAlloc types) instead of memref<?x!llvm.ptr>. Both
    /// represent block-partitioned pointer tables and need dataPtrAddr.
    Value ptrBase = readablePayloadPtr;
    if (auto ptrType = dyn_cast<MemRefType>(op.getPtr().getType())) {
      auto elemTy = ptrType.getElementType();
      if (isa<LLVM::LLVMPointerType>(elemTy) || isa<MemRefType>(elemTy))
        ptrBase =
            buildDepReadablePayloadSlotPtr(AC, depEntryPtr, dataPtrAddr, loc);
    }

    auto guidView = AC->create<polygeist::Pointer2MemrefOp>(
        loc, op.getGuid().getType(), guidPtr);
    auto ptrView = AC->create<polygeist::Pointer2MemrefOp>(
        loc, op.getPtr().getType(), ptrBase);
    rewriter.replaceOp(op, ValueRange{guidView, ptrView});
    ++numDepOpsConverted;
    return success();
  }
};

/// Pattern to convert arts.db_gep operations to LLVM GEP using element strides
struct DbGepOpPattern : public ArtsRtToLLVMPattern<DbGepOp> {
  using ArtsRtToLLVMPattern::ArtsRtToLLVMPattern;

  LogicalResult matchAndRewrite(DbGepOp op,
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

    /// Use typed GEP based on the element type; default to pointer elements.
    /// memref element types model ARTS pointer tables at this stage, not
    /// in-line memref descriptors, so address those slots as pointer-sized
    /// entries.
    Type elemTy = baseMT ? baseMT.getElementType() : AC->llvmPtr;
    if (isa<MemRefType>(elemTy))
      elemTy = AC->llvmPtr;
    Value elemAddr = AC->create<LLVM::GEPOp>(loc, AC->llvmPtr, elemTy, basePtr,
                                             ValueRange{idx64});

    rewriter.replaceOp(op, elemAddr);
    ++numDbOpsConverted;
    return success();
  }
};

/// Pattern to convert arts.edt_create operations (key EDT creation)
struct EdtCreatePattern : public ArtsRtToLLVMPattern<EdtCreateOp> {
  using ArtsRtToLLVMPattern::ArtsRtToLLVMPattern;

  LogicalResult matchAndRewrite(EdtCreateOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Lowering EdtCreate Op " << op);
    ArtsCodegen::RewriterGuard RG(*AC, rewriter);
    /// Get outlined function name
    auto funcNameAttr = op->getAttrOfType<StringAttr>(
        ::mlir::carts::arts::AttrNames::Operation::OutlinedFunc);
    if (!funcNameAttr)
      return op.emitError("Missing arts.outlined_func attribute");

    auto outlined =
        AC->getModule().lookupSymbol<func::FuncOp>(funcNameAttr.getValue());
    if (!outlined)
      return op.emitError("EDT Outlined function not found");

    /// Build arts_edt_create arguments
    auto funcPtr = AC->createFnPtr(outlined, op.getLoc());
    auto loc = op.getLoc();
    Value paramv = op.getParamMemref();
    Value depc = op.getDepCount();
    if (depc && depc.getType() != AC->Int32)
      depc = AC->castToInt(AC->Int32, depc, loc);

    /// Normalize paramv to the runtime ABI type expected by the helper decls.
    if (auto memrefType = dyn_cast<MemRefType>(paramv.getType())) {
      auto runtimeParamType =
          MemRefType::get({ShapedType::kDynamic}, AC->Int64);
      if (memrefType != runtimeParamType)
        paramv = AC->create<memref::CastOp>(loc, runtimeParamType, paramv)
                     .getResult();
    }

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

    auto createIdAttr = op->getAttrOfType<IntegerAttr>(
        ::mlir::carts::arts::AttrNames::Operation::ArtsCreateId);
    Value artsIdVal;
    if (createIdAttr)
      artsIdVal = AC->create<arith::ConstantOp>(loc, AC->Int64, createIdAttr);
    else
      artsIdVal = AC->createIntConstant(0, AC->Int64, loc);

    Value hintMemref = buildArtsHintMemref(AC, route, artsIdVal, loc);

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
      callOp->setAttr(::mlir::carts::arts::AttrNames::Operation::ArtsCreateId,
                      createIdAttr);
    rewriter.replaceOp(op, callOp.getResult(0));
    ++numEdtOpsConverted;
    return success();
  }
};

///===----------------------------------------------------------------------===///
/// Pattern Population
///===----------------------------------------------------------------------===///

namespace mlir::carts::arts_rt::convert_arts_rt_to_llvm {

void populateArtsRtOpToLLVMPatterns(RewritePatternSet &patterns,
                                    ArtsCodegen *AC) {
  MLIRContext *context = patterns.getContext();

  /// Epoch patterns
  patterns.add<CreateEpochPattern, WaitOnEpochPattern>(context, AC);

  /// EDT patterns
  patterns.add<EdtParamPackPattern, EdtParamUnpackPattern>(context, AC);
  patterns.add<EdtCreatePattern>(context, AC);

  /// Dep patterns
  patterns.add<DepGepOpPattern>(context, AC);
  patterns.add<RecordDepPattern>(context, AC);
  patterns.add<DepDbAcquireOpPattern>(context, AC);
  patterns.add<DbGepOpPattern>(context, AC);
}

} // namespace mlir::carts::arts_rt::convert_arts_rt_to_llvm
