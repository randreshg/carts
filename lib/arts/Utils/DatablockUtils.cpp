///==========================================================================///
/// File: DatablockUtils.cpp
///
/// Implementation of utility functions for working with ARTS datablocks.
///==========================================================================///

#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/ArtsUtils.h"
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

std::optional<std::pair<Value, Value>> DatablockUtils::traceToDbAlloc(Value dep) {
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
    if (isa<DbAcquireOp, DbAllocOp>(root))
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
  assert(dbAcquire);
  return getUnderlyingDbAlloc(dbAcquire.getSourcePtr());
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

bool DatablockUtils::dbHasSingleSize(Operation *dbOp) {
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
/// Datablock Stride Computation
///===----------------------------------------------------------------------===///

std::optional<int64_t> DatablockUtils::getStaticStride(ValueRange sizes) {
  if (sizes.empty())
    return std::nullopt;

  // Single dimension [N]: stride = 1
  if (sizes.size() == 1)
    return 1;

  // Multi-dimensional [D0, D1, ...]: stride = D1 * D2 * ... (skip D0!)
  int64_t stride = 1;
  for (size_t i = 1; i < sizes.size(); ++i) { // START AT 1
    int64_t dim;
    if (!ValueUtils::getConstantIndex(sizes[i], dim))
      return std::nullopt; // Dynamic dimension
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
  for (size_t i = 1; i < shape.size(); ++i) { // START AT 1
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

Value DatablockUtils::getStrideValue(OpBuilder &builder, Location loc, ValueRange sizes) {
  if (sizes.empty())
    return nullptr;

  // Single dimension [N]: stride = 1
  if (sizes.size() == 1)
    return builder.create<arith::ConstantIndexOp>(loc, 1);

  // Try static first for efficiency
  if (auto staticStride = getStaticStride(sizes))
    return builder.create<arith::ConstantIndexOp>(loc, *staticStride);

  // Dynamic: build multiplication chain for trailing dimensions
  // stride = sizes[1] * sizes[2] * ... * sizes[n-1]
  Value stride = sizes[1]; // Start at index 1 (skip first dimension!)
  for (size_t i = 2; i < sizes.size(); ++i) {
    stride = builder.create<arith::MulIOp>(loc, stride, sizes[i]);
  }
  return stride;
}

Value DatablockUtils::getElementStrideValue(OpBuilder &builder, Location loc, DbAllocOp alloc) {
  return getStrideValue(builder, loc, alloc.getElementSizes());
}

Value DatablockUtils::getOuterStrideValue(OpBuilder &builder, Location loc, DbAllocOp alloc) {
  return getStrideValue(builder, loc, alloc.getSizes());
}
