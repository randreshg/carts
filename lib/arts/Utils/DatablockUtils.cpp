///==========================================================================///
/// File: DatablockUtils.cpp
///
/// Implementation of utility functions for working with ARTS datablocks.
///==========================================================================///

#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(datablock_utils);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Datablock Tracing Utilities
///===----------------------------------------------------------------------===///

std::optional<std::pair<Value, Value>>
DatablockUtils::traceToDbAlloc(Value dep) {
  /// Case 1: Direct DbAllocOp result (either guid or ptr)
  if (auto allocOp = dep.getDefiningOp<DbAllocOp>())
    return std::make_pair(allocOp.getGuid(), allocOp.getPtr());

  /// Case 2: DbAcquireOp - trace through sourceGuid or sourcePtr
  if (auto acqOp = dep.getDefiningOp<DbAcquireOp>()) {
    /// Prefer tracing through sourceGuid if it exists
    if (Value srcGuid = acqOp.getSourceGuid())
      return traceToDbAlloc(srcGuid);
    /// Otherwise trace through sourcePtr
    if (Value srcPtr = acqOp.getSourcePtr())
      return traceToDbAlloc(srcPtr);
  }

  /// Case 3: Block argument - trace through parent EDT's dependencies
  if (auto blockArg = dep.dyn_cast<BlockArgument>()) {
    Operation *parentOp = blockArg.getOwner()->getParentOp();
    if (auto parentEdt = dyn_cast<EdtOp>(parentOp)) {
      unsigned idx = blockArg.getArgNumber();
      ValueRange parentDeps = parentEdt.getDependencies();
      if (idx < parentDeps.size())
        return traceToDbAlloc(parentDeps[idx]);
    }
  }

  return std::nullopt;
}

Operation *DatablockUtils::getUnderlyingDb(Value v, unsigned depth) {
  if (!v)
    return nullptr;

  /// Prevent infinite recursion from circular acquire chains
  if (depth > 20) {
    ARTS_WARN("getUnderlyingDb exceeded depth limit of 20 "
              << "(possible circular acquire chain)");
    return nullptr;
  }

  /// Directly return the DbAcquireOp or DbAllocOp if present
  if (auto acq = v.getDefiningOp<DbAcquireOp>())
    return acq.getOperation();
  if (auto alloc = v.getDefiningOp<DbAllocOp>())
    return alloc.getOperation();
  if (auto copy = v.getDefiningOp<DbCopyOp>())
    return copy.getOperation();
  if (auto dbLoad = v.getDefiningOp<DbRefOp>())
    return getUnderlyingDb(dbLoad.getSource(), depth + 1);

  /// If it's a block argument of an EDT, map to the corresponding operand and
  /// recurse. Block arguments correspond to dependencies
  if (auto blockArg = v.dyn_cast<BlockArgument>()) {
    Block *block = blockArg.getOwner();
    Operation *owner = block->getParentOp();
    if (auto edt = dyn_cast<EdtOp>(owner)) {
      unsigned argIndex = blockArg.getArgNumber();
      /// Block arguments correspond to dependencies
      ValueRange deps = edt.getDependencies();
      if (argIndex < deps.size()) {
        Value operand = deps[argIndex];
        return getUnderlyingDb(operand, depth + 1);
      }
    }
  }

  /// Peel through common view/cast/gep nodes to reach DB ops
  if (Operation *def = v.getDefiningOp()) {
    if (auto gep = dyn_cast<DbGepOp>(def))
      return getUnderlyingDb(gep.getBasePtr(), depth + 1);
    if (auto castOp = dyn_cast<memref::CastOp>(def))
      return getUnderlyingDb(castOp.getSource(), depth + 1);
    if (auto subview = dyn_cast<memref::SubViewOp>(def))
      return getUnderlyingDb(subview.getSource(), depth + 1);
  }

  /// As a last resort, try underlying root op if it's a DB op
  if (Operation *root = ValueUtils::getUnderlyingOperation(v)) {
    if (isa<DbAcquireOp, DbAllocOp, DbCopyOp>(root))
      return root;
  }

  return nullptr;
}

Operation *DatablockUtils::getUnderlyingDbAlloc(Value v) {
  Operation *underlyingDb = getUnderlyingDb(v);
  if (!underlyingDb)
    return nullptr;
  if (isa<DbAllocOp>(underlyingDb))
    return underlyingDb;
  auto dbAcquire = dyn_cast<DbAcquireOp>(underlyingDb);
  if (dbAcquire)
    return getUnderlyingDbAlloc(dbAcquire.getSourcePtr());
  if (auto dbCopy = dyn_cast<DbCopyOp>(underlyingDb))
    return getUnderlyingDbAlloc(dbCopy.getSourcePtr());
  return nullptr;
}

///===----------------------------------------------------------------------===///
/// Datablock Size and Offset Extraction
///===----------------------------------------------------------------------===///

SmallVector<Value> DatablockUtils::getSizesFromDb(Operation *dbOp) {
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

SmallVector<Value> DatablockUtils::getElementSizesFromDb(Operation *dbOp) {
  if (auto allocOp = dyn_cast<DbAllocOp>(dbOp)) {
    return SmallVector<Value>(allocOp.getElementSizes().begin(),
                              allocOp.getElementSizes().end());
  }
  return {};
}

SmallVector<Value> DatablockUtils::getSizesFromDb(Value datablockPtr) {
  /// Use getUnderlyingDb to find the original DB operation
  Operation *underlyingDb = getUnderlyingDb(datablockPtr);
  if (!underlyingDb)
    return {};

  return getSizesFromDb(underlyingDb);
}

bool DatablockUtils::hasSingleSize(Operation *dbOp) {
  if (!dbOp)
    return false;

  SmallVector<Value> sizes = getSizesFromDb(dbOp);

  if (sizes.empty())
    return false;

  /// Check if there's exactly one size and it's constant 1
  if (sizes.size() != 1)
    return false;

  return ValueUtils::isOneConstant(sizes[0]);
}

bool DatablockUtils::isCoarseGrained(DbAllocOp alloc) {
  if (auto mode = getPartitionMode(alloc.getOperation()))
    return *mode == PartitionMode::coarse;

  /// Fallback: check sizes for backward compatibility
  return llvm::all_of(alloc.getSizes(), [](Value v) {
    int64_t val;
    return ValueUtils::getConstantIndex(v, val) && val == 1;
  });
}

bool DatablockUtils::isFineGrained(DbAllocOp alloc) {
  if (auto mode = getPartitionMode(alloc.getOperation()))
    return *mode == PartitionMode::fine_grained;

  /// Fallback: check elementSizes for backward compatibility
  ValueRange elementSizes = alloc.getElementSizes();
  if (elementSizes.empty())
    return false;

  return llvm::all_of(elementSizes, [](Value v) {
    int64_t cst;
    return ValueUtils::getConstantIndex(v, cst) && cst == 1;
  });
}

SmallVector<Value> DatablockUtils::getOffsetsFromDb(Value datablockPtr) {
  Operation *underlyingDb = getUnderlyingDb(datablockPtr);
  if (!underlyingDb)
    return {};

  if (auto acquireOp = dyn_cast<DbAcquireOp>(underlyingDb))
    return SmallVector<Value>(acquireOp.getOffsets().begin(),
                              acquireOp.getOffsets().end());

  return {};
}

///===----------------------------------------------------------------------===///
/// Partition Mode Detection
///===----------------------------------------------------------------------===///

PartitionMode
DatablockUtils::getPartitionModeFromStructure(DbAcquireOp acquire) {
  if (auto mode = ::getPartitionMode(acquire.getOperation()))
    return *mode;

  /// Fallback: if no attribute, default to coarse
  return PartitionMode::coarse;
}

PartitionMode DatablockUtils::getPartitionModeFromStructure(DbAllocOp alloc) {
  if (auto mode = ::getPartitionMode(alloc.getOperation()))
    return *mode;

  /// Fallback: if no attribute, check structural coarseness
  if (isCoarseGrained(alloc))
    return PartitionMode::coarse;

  /// Default to fine_grained for fine-grained allocations
  return PartitionMode::fine_grained;
}

///===----------------------------------------------------------------------===///
/// Datablock Stride Computation
///===----------------------------------------------------------------------===///

std::optional<int64_t> DatablockUtils::getStaticStride(ValueRange sizes) {
  if (sizes.empty())
    return std::nullopt;

  /// Single dimension [N]: stride = 1
  if (sizes.size() == 1)
    return 1;

  /// Multi-dimensional [D0, D1, ...]: stride = D1 * D2 * ... (skip D0!)
  int64_t stride = 1;
  for (size_t i = 1; i < sizes.size(); ++i) {
    int64_t dim;
    if (!ValueUtils::getConstantIndex(sizes[i], dim))
      return std::nullopt;
    stride *= dim;
  }
  return stride;
}

std::optional<int64_t> DatablockUtils::getStaticStride(MemRefType memrefType) {
  auto shape = memrefType.getShape();
  if (shape.empty())
    return std::nullopt;

  if (shape.size() == 1)
    return 1;

  int64_t stride = 1;
  for (size_t i = 1; i < shape.size(); ++i) {
    if (shape[i] == ShapedType::kDynamic)
      return std::nullopt;
    stride *= shape[i];
  }
  return stride;
}

std::optional<int64_t> DatablockUtils::getStaticElementStride(DbAllocOp alloc) {
  return getStaticStride(alloc.getElementSizes());
}

std::optional<int64_t> DatablockUtils::getStaticOuterStride(DbAllocOp alloc) {
  return getStaticStride(alloc.getSizes());
}

Value DatablockUtils::getStrideValue(OpBuilder &builder, Location loc,
                                     ValueRange sizes) {
  if (sizes.empty())
    return nullptr;

  /// Single dimension [N]: stride = 1
  if (sizes.size() == 1)
    return builder.create<arith::ConstantIndexOp>(loc, 1);

  /// Try static first for efficiency
  if (auto staticStride = getStaticStride(sizes))
    return builder.create<arith::ConstantIndexOp>(loc, *staticStride);

  /// Dynamic: build multiplication chain for trailing dimensions
  /// stride = sizes[1] * sizes[2] * ... * sizes[n-1]
  Value stride = sizes[1];
  for (size_t i = 2; i < sizes.size(); ++i)
    stride = builder.create<arith::MulIOp>(loc, stride, sizes[i]);
  return stride;
}

Value DatablockUtils::getElementStrideValue(OpBuilder &builder, Location loc,
                                            DbAllocOp alloc) {
  return getStrideValue(builder, loc, alloc.getElementSizes());
}

Value DatablockUtils::getOuterStrideValue(OpBuilder &builder, Location loc,
                                          DbAllocOp alloc) {
  return getStrideValue(builder, loc, alloc.getSizes());
}

///===----------------------------------------------------------------------===//
/// Access Mode and Hints Analysis
///===----------------------------------------------------------------------===//

bool DatablockUtils::hasStaticHints(DbAcquireOp acqOp) {
  Value chunkIndex = acqOp.getChunkIndex();
  Value chunkSize = acqOp.getChunkSize();
  int64_t val = 0;
  bool idxConst = !chunkIndex || ValueUtils::getConstantIndex(chunkIndex, val);
  bool sizeConst = !chunkSize || ValueUtils::getConstantIndex(chunkSize, val);
  return idxConst && sizeConst;
}

bool DatablockUtils::isWriterMode(ArtsMode mode) {
  return mode == ArtsMode::out || mode == ArtsMode::inout;
}

///===----------------------------------------------------------------------===///
/// Offset Dependency and Chunk Size Analysis
///===----------------------------------------------------------------------===///

/// Check if a value depends on a partition offset (ignoring numeric casts).
bool DatablockUtils::dependsOnOffset(Value v, Value offset) {
  if (!v || !offset)
    return false;
  Value vStripped = ValueUtils::stripNumericCasts(v);
  Value oStripped = ValueUtils::stripNumericCasts(offset);
  return ValueUtils::dependsOn(vStripped, oStripped);
}

/// Try to extract an offset-independent base chunk size from size hints.
/// This peels remainder-aware patterns like min(base, total - offset).
Value DatablockUtils::extractBaseChunkSizeCandidate(Value offsetHint,
                                                    Value sizeHint, int depth) {
  if (!sizeHint || depth > 6)
    return Value();

  if (!offsetHint || !dependsOnOffset(sizeHint, offsetHint))
    return sizeHint;

  Operation *defOp = sizeHint.getDefiningOp();
  if (!defOp)
    return Value();

  if (auto minOp = dyn_cast<arith::MinUIOp>(defOp)) {
    Value lhs = minOp.getLhs();
    Value rhs = minOp.getRhs();
    bool lhsDep = dependsOnOffset(lhs, offsetHint);
    bool rhsDep = dependsOnOffset(rhs, offsetHint);
    if (lhsDep && !rhsDep)
      return extractBaseChunkSizeCandidate(offsetHint, rhs, depth + 1);
    if (rhsDep && !lhsDep)
      return extractBaseChunkSizeCandidate(offsetHint, lhs, depth + 1);
    if (Value cand = extractBaseChunkSizeCandidate(offsetHint, lhs, depth + 1))
      return cand;
    return extractBaseChunkSizeCandidate(offsetHint, rhs, depth + 1);
  }

  if (auto minOp = dyn_cast<arith::MinSIOp>(defOp)) {
    Value lhs = minOp.getLhs();
    Value rhs = minOp.getRhs();
    bool lhsDep = dependsOnOffset(lhs, offsetHint);
    bool rhsDep = dependsOnOffset(rhs, offsetHint);
    if (lhsDep && !rhsDep)
      return extractBaseChunkSizeCandidate(offsetHint, rhs, depth + 1);
    if (rhsDep && !lhsDep)
      return extractBaseChunkSizeCandidate(offsetHint, lhs, depth + 1);
    if (Value cand = extractBaseChunkSizeCandidate(offsetHint, lhs, depth + 1))
      return cand;
    return extractBaseChunkSizeCandidate(offsetHint, rhs, depth + 1);
  }

  if (auto selectOp = dyn_cast<arith::SelectOp>(defOp)) {
    Value tVal = selectOp.getTrueValue();
    Value fVal = selectOp.getFalseValue();
    bool tDep = dependsOnOffset(tVal, offsetHint);
    bool fDep = dependsOnOffset(fVal, offsetHint);
    if (tDep && !fDep)
      return extractBaseChunkSizeCandidate(offsetHint, fVal, depth + 1);
    if (fDep && !tDep)
      return extractBaseChunkSizeCandidate(offsetHint, tVal, depth + 1);
    if (Value cand = extractBaseChunkSizeCandidate(offsetHint, tVal, depth + 1))
      return cand;
    return extractBaseChunkSizeCandidate(offsetHint, fVal, depth + 1);
  }

  if (auto addOp = dyn_cast<arith::AddIOp>(defOp)) {
    int64_t lhsConst = 0, rhsConst = 0;
    bool lhsIsConst = ValueUtils::getConstantIndex(addOp.getLhs(), lhsConst);
    bool rhsIsConst = ValueUtils::getConstantIndex(addOp.getRhs(), rhsConst);
    if (lhsIsConst && lhsConst >= -16 && lhsConst <= 16)
      return extractBaseChunkSizeCandidate(offsetHint, addOp.getRhs(),
                                           depth + 1);
    if (rhsIsConst && rhsConst >= -16 && rhsConst <= 16)
      return extractBaseChunkSizeCandidate(offsetHint, addOp.getLhs(),
                                           depth + 1);
  }

  if (auto subOp = dyn_cast<arith::SubIOp>(defOp)) {
    int64_t rhsConst = 0;
    if (ValueUtils::getConstantIndex(subOp.getRhs(), rhsConst) &&
        rhsConst >= -16 && rhsConst <= 16)
      return extractBaseChunkSizeCandidate(offsetHint, subOp.getLhs(),
                                           depth + 1);
  }

  return Value();
}

/// Find the EDT operation that uses a DbControlOp result.
/// Returns the first EDT operation that uses the DbControlOp's result,
/// or nullptr if no EDT user is found.
Operation *DatablockUtils::findUserEdt(DbControlOp dbControl) {
  for (Operation *user : dbControl.getResult().getUsers()) {
    if (auto edt = dyn_cast<EdtOp>(user)) {
      return edt;
    }
  }
  return nullptr;
}

///===----------------------------------------------------------------------===///
/// Index Chain Utilities
///===----------------------------------------------------------------------===///

/// Collect full index chain from DbRefOp indices plus memory operation indices.
SmallVector<Value> DatablockUtils::collectFullIndexChain(DbRefOp dbRef,
                                                         Operation *memOp) {
  SmallVector<Value> chain(dbRef.getIndices().begin(),
                           dbRef.getIndices().end());
  ValueRange memIndices;
  if (auto load = dyn_cast<memref::LoadOp>(memOp))
    memIndices = load.getIndices();
  else if (auto store = dyn_cast<memref::StoreOp>(memOp))
    memIndices = store.getIndices();
  chain.append(memIndices.begin(), memIndices.end());
  return chain;
}
