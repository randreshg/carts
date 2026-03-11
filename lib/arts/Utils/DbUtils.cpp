///==========================================================================///
/// File: DbUtils.cpp
///
/// Implementation of utility functions for working with ARTS DBs.
///==========================================================================///

#include "arts/Utils/DbUtils.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(datablock_utils);

using namespace mlir;
using namespace mlir::arts;

namespace {

static void getAcquireDependencySlice(DbAcquireOp acquire,
                                      SmallVector<Value> &sizesOut,
                                      SmallVector<Value> &offsetsOut) {
  /// Dependency shape for DB acquires is defined in DB-space by the explicit
  /// offsets/sizes operands. Partition hints are loop-iteration hints and can
  /// live in a different index space.
  sizesOut.assign(acquire.getSizes().begin(), acquire.getSizes().end());
  offsetsOut.assign(acquire.getOffsets().begin(), acquire.getOffsets().end());
}

static bool isMemrefForwardingOp(Operation *op, Value source) {
  if (!op || !source)
    return false;

  if (auto castOp = dyn_cast<memref::CastOp>(op))
    return castOp.getSource() == source;
  if (auto subview = dyn_cast<memref::SubViewOp>(op))
    return subview.getSource() == source;
  if (auto view = dyn_cast<memref::ViewOp>(op))
    return view.getSource() == source;
  if (auto reinterpret = dyn_cast<memref::ReinterpretCastOp>(op))
    return reinterpret.getSource() == source;
  if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(op))
    return llvm::is_contained(unrealized.getInputs(), source);
  if (auto dbRef = dyn_cast<DbRefOp>(op))
    return dbRef.getSource() == source;

  return false;
}

static void appendDynamicSubviewOffsets(memref::SubViewOp subview,
                                        SmallVector<Value> &chain) {
  for (OpFoldResult off : subview.getMixedOffsets())
    if (auto v = off.dyn_cast<Value>())
      chain.push_back(v);
}

} // namespace

///===----------------------------------------------------------------------===///
/// Datablock Lowering Info Extraction
///===----------------------------------------------------------------------===///

/// Shared implementation: dispatches on the concrete operation type.
template <typename OpType>
DbLoweringInfo DbUtils::extractDbLoweringInfo(OpType op) {
  DbLoweringInfo info;

  if (auto acqOp = dyn_cast<DbAcquireOp>(op.getOperation())) {
    info.sizes = getDependencySizesFromDb(acqOp.getOperation());
    info.offsets = getDependencyOffsetsFromDb(acqOp.getOperation());
    info.indices.assign(acqOp.getIndices().begin(), acqOp.getIndices().end());
  } else if (auto depAcqOp = dyn_cast<DepDbAcquireOp>(op.getOperation())) {
    info.sizes = getDependencySizesFromDb(depAcqOp.getOperation());
    info.offsets = getDependencyOffsetsFromDb(depAcqOp.getOperation());
    info.indices.assign(depAcqOp.getIndices().begin(),
                        depAcqOp.getIndices().end());
  } else {
    info.sizes.assign(op.getSizes().begin(), op.getSizes().end());
    info.offsets.clear();
    info.indices.clear();
  }

  info.isSingleElement = false;
  if (info.sizes.empty()) {
    info.isSingleElement = true;
    return info;
  }
  if (info.sizes.size() == 1) {
    info.isSingleElement = ValueUtils::isOneConstant(info.sizes[0]);
  }
  return info;
}

/// Explicit template instantiations for the operation types used in lowering.
template DbLoweringInfo
DbUtils::extractDbLoweringInfo<DbAcquireOp>(DbAcquireOp op);
template DbLoweringInfo
DbUtils::extractDbLoweringInfo<DepDbAcquireOp>(DepDbAcquireOp op);
template DbLoweringInfo DbUtils::extractDbLoweringInfo<DbAllocOp>(DbAllocOp op);

///===----------------------------------------------------------------------===///
/// Datablock Tracing Utilities
///===----------------------------------------------------------------------===///

std::optional<std::pair<Value, Value>> DbUtils::traceToDbAlloc(Value dep) {
  if (auto allocOp = dep.getDefiningOp<DbAllocOp>())
    return std::make_pair(allocOp.getGuid(), allocOp.getPtr());

  if (auto acqOp = dep.getDefiningOp<DbAcquireOp>()) {
    if (Value srcGuid = acqOp.getSourceGuid())
      return traceToDbAlloc(srcGuid);
    if (Value srcPtr = acqOp.getSourcePtr())
      return traceToDbAlloc(srcPtr);
  }

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

Operation *DbUtils::getUnderlyingDb(Value v, unsigned depth) {
  if (!v)
    return nullptr;

  /// Prevent infinite recursion from circular acquire chains
  if (depth > 20) {
    ARTS_WARN("getUnderlyingDb exceeded depth limit");
    return nullptr;
  }

  /// Case 1: Direct DbAllocOp result (either guid or ptr)
  if (auto acq = v.getDefiningOp<DbAcquireOp>())
    return acq.getOperation();
  if (auto alloc = v.getDefiningOp<DbAllocOp>())
    return alloc.getOperation();
  /// Case 2: DbAcquireOp - trace through sourceGuid or sourcePtr
  if (auto dbLoad = v.getDefiningOp<DbRefOp>())
    return getUnderlyingDb(dbLoad.getSource(), depth + 1);

  /// Case 3: Block argument - trace through parent EDT's dependencies
  /// If it's a block argument of an EDT, map to the corresponding operand and
  /// recurse. Block arguments correspond to dependencies.
  if (auto blockArg = v.dyn_cast<BlockArgument>()) {
    Block *block = blockArg.getOwner();
    Operation *owner = block->getParentOp();
    if (auto edt = dyn_cast<EdtOp>(owner)) {
      unsigned argIndex = blockArg.getArgNumber();
      ValueRange deps = edt.getDependencies();
      if (argIndex < deps.size())
        return getUnderlyingDb(deps[argIndex], depth + 1);
    }
  }

  if (Operation *def = v.getDefiningOp()) {
    if (auto gep = dyn_cast<DbGepOp>(def))
      return getUnderlyingDb(gep.getBasePtr(), depth + 1);
    if (auto castOp = dyn_cast<memref::CastOp>(def))
      return getUnderlyingDb(castOp.getSource(), depth + 1);
    if (auto subview = dyn_cast<memref::SubViewOp>(def))
      return getUnderlyingDb(subview.getSource(), depth + 1);
  }

  if (Operation *root = ValueUtils::getUnderlyingOperation(v))
    if (isa<DbAcquireOp, DbAllocOp>(root))
      return root;

  return nullptr;
}

Operation *DbUtils::getUnderlyingDbAlloc(Value v) {
  Operation *underlyingDb = getUnderlyingDb(v);
  if (!underlyingDb)
    return nullptr;
  if (isa<DbAllocOp>(underlyingDb))
    return underlyingDb;
  auto dbAcquire = dyn_cast<DbAcquireOp>(underlyingDb);
  if (dbAcquire)
    return getUnderlyingDbAlloc(dbAcquire.getSourcePtr());
  return nullptr;
}

bool DbUtils::isSameMemoryObject(Value lhsMemref, Value rhsMemref) {
  lhsMemref = ValueUtils::stripNumericCasts(lhsMemref);
  rhsMemref = ValueUtils::stripNumericCasts(rhsMemref);

  Operation *lhsRoot = getUnderlyingDbAlloc(lhsMemref);
  Operation *rhsRoot = getUnderlyingDbAlloc(rhsMemref);
  if (lhsRoot && rhsRoot)
    return lhsRoot == rhsRoot;

  lhsRoot = ValueUtils::getUnderlyingOperation(lhsMemref);
  rhsRoot = ValueUtils::getUnderlyingOperation(rhsMemref);
  if (lhsRoot && rhsRoot)
    return lhsRoot == rhsRoot;

  return lhsMemref == rhsMemref;
}

///===----------------------------------------------------------------------===///
/// Datablock Size and Offset Extraction
///===----------------------------------------------------------------------===///

SmallVector<Value> DbUtils::getSizesFromDb(Operation *dbOp) {
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

SmallVector<Value> DbUtils::getElementSizesFromDb(Operation *dbOp) {
  if (auto allocOp = dyn_cast<DbAllocOp>(dbOp)) {
    return SmallVector<Value>(allocOp.getElementSizes().begin(),
                              allocOp.getElementSizes().end());
  }
  return {};
}

SmallVector<Value> DbUtils::getSizesFromDb(Value datablockPtr) {
  /// Use getUnderlyingDb to find the original DB operation
  Operation *underlyingDb = getUnderlyingDb(datablockPtr);
  if (!underlyingDb)
    return {};

  return getSizesFromDb(underlyingDb);
}

SmallVector<Value> DbUtils::getDependencySizesFromDb(Operation *dbOp) {
  if (auto allocOp = dyn_cast_or_null<DbAllocOp>(dbOp))
    return SmallVector<Value>(allocOp.getSizes().begin(),
                              allocOp.getSizes().end());

  if (auto acquireOp = dyn_cast_or_null<DbAcquireOp>(dbOp)) {
    SmallVector<Value> sizes;
    SmallVector<Value> offsets;
    getAcquireDependencySlice(acquireOp, sizes, offsets);
    return sizes;
  }

  if (auto depAcquireOp = dyn_cast_or_null<DepDbAcquireOp>(dbOp))
    return SmallVector<Value>(depAcquireOp.getSizes().begin(),
                              depAcquireOp.getSizes().end());

  return {};
}

SmallVector<Value> DbUtils::getDependencySizesFromDb(Value datablockPtr) {
  Operation *underlyingDb = getUnderlyingDb(datablockPtr);
  if (!underlyingDb)
    return {};
  return getDependencySizesFromDb(underlyingDb);
}

SmallVector<Value> DbUtils::getDependencyOffsetsFromDb(Operation *dbOp) {
  if (auto acquireOp = dyn_cast_or_null<DbAcquireOp>(dbOp)) {
    SmallVector<Value> sizes;
    SmallVector<Value> offsets;
    getAcquireDependencySlice(acquireOp, sizes, offsets);
    return offsets;
  }

  if (auto depAcquireOp = dyn_cast_or_null<DepDbAcquireOp>(dbOp))
    return SmallVector<Value>(depAcquireOp.getOffsets().begin(),
                              depAcquireOp.getOffsets().end());

  return {};
}

SmallVector<Value> DbUtils::getDependencyOffsetsFromDb(Value datablockPtr) {
  Operation *underlyingDb = getUnderlyingDb(datablockPtr);
  if (!underlyingDb)
    return {};
  return getDependencyOffsetsFromDb(underlyingDb);
}

bool DbUtils::hasSingleSize(Operation *dbOp) {
  if (!dbOp)
    return false;

  auto isOneLike = [](Value size) -> bool {
    if (ValueUtils::isOneConstant(size))
      return true;

    auto addOp = size.getDefiningOp<arith::AddIOp>();
    if (!addOp)
      return false;

    Value lhs = addOp.getLhs();
    Value rhs = addOp.getRhs();
    Value other;
    if (ValueUtils::isOneConstant(lhs))
      other = rhs;
    else if (ValueUtils::isOneConstant(rhs))
      other = lhs;
    else
      return false;

    auto subOp = other.getDefiningOp<arith::SubIOp>();
    if (!subOp)
      return false;

    Value subLhs = subOp.getLhs();
    Value subRhs = subOp.getRhs();
    if (subLhs == subRhs)
      return true;

    if (auto minOp = subLhs.getDefiningOp<arith::MinUIOp>()) {
      if (minOp.getLhs() == subRhs || minOp.getRhs() == subRhs)
        return true;
    }
    return false;
  };

  SmallVector<Value> sizes = getSizesFromDb(dbOp);
  if (sizes.empty())
    return true;

  for (Value size : sizes) {
    if (!isOneLike(size))
      return false;
  }
  return true;
}

bool DbUtils::isCoarseGrained(DbAllocOp alloc) {
  if (auto mode = getPartitionMode(alloc.getOperation()))
    return *mode == PartitionMode::coarse;

  return llvm::all_of(alloc.getSizes(), [](Value v) {
    int64_t val;
    return ValueUtils::getConstantIndex(v, val) && val == 1;
  });
}

bool DbUtils::isFineGrained(DbAllocOp alloc) {
  if (auto mode = getPartitionMode(alloc.getOperation()))
    return *mode == PartitionMode::fine_grained;

  ValueRange elementSizes = alloc.getElementSizes();
  if (elementSizes.empty())
    return false;

  return llvm::all_of(elementSizes, [](Value v) {
    int64_t cst;
    return ValueUtils::getConstantIndex(v, cst) && cst == 1;
  });
}

SmallVector<Value> DbUtils::getOffsetsFromDb(Value datablockPtr) {
  if (auto *underlyingDb = getUnderlyingDb(datablockPtr))
    if (auto acquireOp = dyn_cast<DbAcquireOp>(underlyingDb))
      return SmallVector<Value>(acquireOp.getOffsets().begin(),
                                acquireOp.getOffsets().end());
  return {};
}

///===----------------------------------------------------------------------===///
/// Partition Mode Detection
///===----------------------------------------------------------------------===///

PartitionMode DbUtils::getPartitionModeFromStructure(DbAcquireOp acquire) {
  if (auto mode = ::getPartitionMode(acquire.getOperation()))
    return *mode;
  return PartitionMode::coarse;
}

PartitionMode DbUtils::getPartitionModeFromStructure(DbAllocOp alloc) {
  if (auto mode = ::getPartitionMode(alloc.getOperation()))
    return *mode;

  if (isCoarseGrained(alloc))
    return PartitionMode::coarse;

  return PartitionMode::fine_grained;
}

///===----------------------------------------------------------------------===///
/// Datablock Stride Computation
///===----------------------------------------------------------------------===///

std::optional<int64_t> DbUtils::getStaticStride(ValueRange sizes) {
  if (sizes.empty())
    return std::nullopt;

  if (sizes.size() == 1)
    return 1;

  int64_t stride = 1;
  for (size_t i = 1; i < sizes.size(); ++i) {
    int64_t dim;
    if (!ValueUtils::getConstantIndex(sizes[i], dim))
      return std::nullopt;
    stride *= dim;
  }
  return stride;
}

std::optional<int64_t> DbUtils::getStaticStride(MemRefType memrefType) {
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

std::optional<int64_t> DbUtils::getStaticElementStride(DbAllocOp alloc) {
  return getStaticStride(alloc.getElementSizes());
}

std::optional<int64_t> DbUtils::getStaticOuterStride(DbAllocOp alloc) {
  return getStaticStride(alloc.getSizes());
}

Value DbUtils::getStrideValue(OpBuilder &builder, Location loc,
                              ValueRange sizes) {
  if (sizes.empty())
    return nullptr;

  if (sizes.size() == 1)
    return arts::createOneIndex(builder, loc);

  if (auto staticStride = getStaticStride(sizes))
    return arts::createConstantIndex(builder, loc, *staticStride);

  Value stride = sizes[1];
  for (size_t i = 2; i < sizes.size(); ++i)
    stride = builder.create<arith::MulIOp>(loc, stride, sizes[i]);
  return stride;
}

Value DbUtils::getElementStrideValue(OpBuilder &builder, Location loc,
                                     DbAllocOp alloc) {
  return getStrideValue(builder, loc, alloc.getElementSizes());
}

Value DbUtils::getOuterStrideValue(OpBuilder &builder, Location loc,
                                   DbAllocOp alloc) {
  return getStrideValue(builder, loc, alloc.getSizes());
}

bool DbUtils::hasStaticHints(DbAcquireOp acqOp) {
  /// Check partition hints (element-space) for static values
  Value offset = acqOp.getPartitionOffsets().empty()
                     ? nullptr
                     : acqOp.getPartitionOffsets().front();
  Value size = acqOp.getPartitionSizes().empty()
                   ? nullptr
                   : acqOp.getPartitionSizes().front();
  int64_t val = 0;
  bool offsetConst = !offset || ValueUtils::getConstantIndex(offset, val);
  bool sizeConst = !size || ValueUtils::getConstantIndex(size, val);
  return offsetConst && sizeConst;
}

bool DbUtils::isWriterMode(ArtsMode mode) {
  return mode == ArtsMode::out || mode == ArtsMode::inout;
}

bool DbUtils::dependsOnOffset(Value v, Value offset) {
  if (!v || !offset)
    return false;
  Value vStripped = ValueUtils::stripNumericCasts(v);
  Value oStripped = ValueUtils::stripNumericCasts(offset);
  return ValueUtils::dependsOn(vStripped, oStripped);
}

Value DbUtils::extractBaseBlockSizeCandidate(Value offsetHint, Value sizeHint,
                                             int depth) {
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
      return extractBaseBlockSizeCandidate(offsetHint, rhs, depth + 1);
    if (rhsDep && !lhsDep)
      return extractBaseBlockSizeCandidate(offsetHint, lhs, depth + 1);
    if (Value cand = extractBaseBlockSizeCandidate(offsetHint, lhs, depth + 1))
      return cand;
    return extractBaseBlockSizeCandidate(offsetHint, rhs, depth + 1);
  }

  if (auto minOp = dyn_cast<arith::MinSIOp>(defOp)) {
    Value lhs = minOp.getLhs();
    Value rhs = minOp.getRhs();
    bool lhsDep = dependsOnOffset(lhs, offsetHint);
    bool rhsDep = dependsOnOffset(rhs, offsetHint);
    if (lhsDep && !rhsDep)
      return extractBaseBlockSizeCandidate(offsetHint, rhs, depth + 1);
    if (rhsDep && !lhsDep)
      return extractBaseBlockSizeCandidate(offsetHint, lhs, depth + 1);
    if (Value cand = extractBaseBlockSizeCandidate(offsetHint, lhs, depth + 1))
      return cand;
    return extractBaseBlockSizeCandidate(offsetHint, rhs, depth + 1);
  }

  if (auto selectOp = dyn_cast<arith::SelectOp>(defOp)) {
    Value tVal = selectOp.getTrueValue();
    Value fVal = selectOp.getFalseValue();
    bool tDep = dependsOnOffset(tVal, offsetHint);
    bool fDep = dependsOnOffset(fVal, offsetHint);
    if (tDep && !fDep)
      return extractBaseBlockSizeCandidate(offsetHint, fVal, depth + 1);
    if (fDep && !tDep)
      return extractBaseBlockSizeCandidate(offsetHint, tVal, depth + 1);
    if (Value cand = extractBaseBlockSizeCandidate(offsetHint, tVal, depth + 1))
      return cand;
    return extractBaseBlockSizeCandidate(offsetHint, fVal, depth + 1);
  }

  if (auto addOp = dyn_cast<arith::AddIOp>(defOp)) {
    int64_t lhsConst = 0, rhsConst = 0;
    bool lhsIsConst = ValueUtils::getConstantIndex(addOp.getLhs(), lhsConst);
    bool rhsIsConst = ValueUtils::getConstantIndex(addOp.getRhs(), rhsConst);
    if (lhsIsConst && lhsConst >= -16 && lhsConst <= 16)
      return extractBaseBlockSizeCandidate(offsetHint, addOp.getRhs(),
                                           depth + 1);
    if (rhsIsConst && rhsConst >= -16 && rhsConst <= 16)
      return extractBaseBlockSizeCandidate(offsetHint, addOp.getLhs(),
                                           depth + 1);
  }

  if (auto subOp = dyn_cast<arith::SubIOp>(defOp)) {
    int64_t rhsConst = 0;
    if (ValueUtils::getConstantIndex(subOp.getRhs(), rhsConst) &&
        rhsConst >= -16 && rhsConst <= 16)
      return extractBaseBlockSizeCandidate(offsetHint, subOp.getLhs(),
                                           depth + 1);
  }

  return Value();
}

Operation *DbUtils::findUserEdt(DbControlOp dbControl) {
  for (Operation *user : dbControl.getResult().getUsers()) {
    if (auto edt = dyn_cast<EdtOp>(user)) {
      return edt;
    }
  }
  return nullptr;
}

SmallVector<Value> DbUtils::collectFullIndexChain(DbRefOp dbRef,
                                                  Operation *memOp) {
  SmallVector<Value> chain(dbRef.getIndices().begin(),
                           dbRef.getIndices().end());
  Value accessedMemref = getAccessedMemref(memOp);
  Value anchor = dbRef.getResult();

  SmallVector<Operation *, 8> forwardingOps;
  DenseSet<Value> visitedMemrefs;
  Value current = accessedMemref;
  while (current && current != anchor &&
         visitedMemrefs.insert(current).second) {
    Operation *def = current.getDefiningOp();
    if (!def)
      break;

    if (auto castOp = dyn_cast<memref::CastOp>(def)) {
      forwardingOps.push_back(def);
      current = castOp.getSource();
      continue;
    }
    if (auto subview = dyn_cast<memref::SubViewOp>(def)) {
      forwardingOps.push_back(def);
      current = subview.getSource();
      continue;
    }
    if (auto view = dyn_cast<memref::ViewOp>(def)) {
      forwardingOps.push_back(def);
      current = view.getSource();
      continue;
    }
    if (auto reinterpret = dyn_cast<memref::ReinterpretCastOp>(def)) {
      forwardingOps.push_back(def);
      current = reinterpret.getSource();
      continue;
    }
    if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(def)) {
      if (unrealized.getInputs().empty())
        break;
      forwardingOps.push_back(def);
      current = unrealized.getInputs().front();
      continue;
    }
    break;
  }

  for (Operation *op : llvm::reverse(forwardingOps))
    if (auto subview = dyn_cast<memref::SubViewOp>(op))
      appendDynamicSubviewOffsets(subview, chain);

  SmallVector<Value> memIndices = getMemoryAccessIndices(memOp);
  chain.append(memIndices.begin(), memIndices.end());
  return chain;
}

SmallVector<Value> DbUtils::getMemoryAccessIndices(Operation *memOp) {
  if (auto access = getMemoryAccessInfo(memOp))
    return access->indices;
  return {};
}

std::optional<DbUtils::MemoryAccessInfo>
DbUtils::getMemoryAccessInfo(Operation *memOp) {
  if (!memOp)
    return std::nullopt;

  if (auto load = dyn_cast<memref::LoadOp>(memOp))
    return MemoryAccessInfo{
        memOp, load.getMemRef(),
        SmallVector<Value>(load.getIndices().begin(), load.getIndices().end()),
        MemoryAccessKind::Read};
  if (auto store = dyn_cast<memref::StoreOp>(memOp))
    return MemoryAccessInfo{memOp, store.getMemRef(),
                            SmallVector<Value>(store.getIndices().begin(),
                                               store.getIndices().end()),
                            MemoryAccessKind::Write};
  if (auto load = dyn_cast<affine::AffineLoadOp>(memOp))
    return MemoryAccessInfo{memOp, load.getMemRef(),
                            SmallVector<Value>(load.getMapOperands().begin(),
                                               load.getMapOperands().end()),
                            MemoryAccessKind::Read};
  if (auto store = dyn_cast<affine::AffineStoreOp>(memOp))
    return MemoryAccessInfo{memOp, store.getMemRef(),
                            SmallVector<Value>(store.getMapOperands().begin(),
                                               store.getMapOperands().end()),
                            MemoryAccessKind::Write};
  return std::nullopt;
}

Value DbUtils::getAccessedMemref(Operation *memOp) {
  if (auto access = getMemoryAccessInfo(memOp))
    return access->memref;
  return Value();
}

void DbUtils::forEachReachableMemoryAccess(
    Value source, llvm::function_ref<WalkResult(const MemoryAccessInfo &)> fn,
    Region *scope) {
  if (!source)
    return;

  SmallVector<Value, 16> worklist;
  DenseSet<Value> visited;
  worklist.push_back(source);

  while (!worklist.empty()) {
    Value current = worklist.pop_back_val();
    if (!visited.insert(current).second)
      continue;

    for (Operation *user : current.getUsers()) {
      if (scope && !scope->isAncestor(user->getParentRegion()))
        continue;

      if (auto access = getMemoryAccessInfo(user)) {
        if (access->memref == current) {
          if (fn(*access).wasInterrupted())
            return;
        }
        continue;
      }

      if (!isMemrefForwardingOp(user, current))
        continue;
      if (user->getNumResults() == 0)
        continue;

      for (Value result : user->getResults())
        if (result.getType().isa<MemRefType>())
          worklist.push_back(result);
    }
  }
}

void DbUtils::collectReachableMemoryOps(Value source,
                                        llvm::SetVector<Operation *> &memOps,
                                        Region *scope) {
  forEachReachableMemoryAccess(
      source,
      [&](const MemoryAccessInfo &access) {
        memOps.insert(access.op);
        return WalkResult::advance();
      },
      scope);
}

///===----------------------------------------------------------------------===///
/// Multi-Entry Stencil Pattern Detection
///===----------------------------------------------------------------------===///

std::optional<int64_t> DbUtils::getConstantOffsetBetween(Value idx,
                                                         Value base) {
  if (!idx || !base)
    return std::nullopt;

  /// Same value means offset 0
  if (idx == base)
    return 0;

  /// Strip numeric casts (index casts, sign/zero extensions, etc.)
  Value strippedIdx = ValueUtils::stripNumericCasts(idx);
  Value strippedBase = ValueUtils::stripNumericCasts(base);

  if (strippedIdx == strippedBase)
    return 0;

  /// Check if idx = base + constant
  if (auto addOp = strippedIdx.getDefiningOp<arith::AddIOp>()) {
    int64_t constVal;
    if (addOp.getLhs() == strippedBase &&
        ValueUtils::getConstantIndex(addOp.getRhs(), constVal))
      return constVal;
    if (addOp.getRhs() == strippedBase &&
        ValueUtils::getConstantIndex(addOp.getLhs(), constVal))
      return constVal;
  }

  /// Check if idx = base - constant
  if (auto subOp = strippedIdx.getDefiningOp<arith::SubIOp>()) {
    int64_t constVal;
    if (subOp.getLhs() == strippedBase &&
        ValueUtils::getConstantIndex(subOp.getRhs(), constVal))
      return -constVal;
  }

  /// Check the reverse: base = idx + constant means idx = base - constant
  if (auto addOp = strippedBase.getDefiningOp<arith::AddIOp>()) {
    int64_t constVal;
    if (addOp.getLhs() == strippedIdx &&
        ValueUtils::getConstantIndex(addOp.getRhs(), constVal))
      return -constVal;
    if (addOp.getRhs() == strippedIdx &&
        ValueUtils::getConstantIndex(addOp.getLhs(), constVal))
      return -constVal;
  }

  if (auto subOp = strippedBase.getDefiningOp<arith::SubIOp>()) {
    int64_t constVal;
    if (subOp.getLhs() == strippedIdx &&
        ValueUtils::getConstantIndex(subOp.getRhs(), constVal))
      return constVal;
  }

  return std::nullopt;
}

bool DbUtils::hasMultiEntryStencilPattern(DbAcquireOp acquire,
                                          int64_t &minOffset,
                                          int64_t &maxOffset) {
  size_t numEntries = acquire.getNumPartitionEntries();
  if (numEntries < 2)
    return false;

  /// Get indices for all entries
  SmallVector<SmallVector<Value>> allIndices;
  for (size_t i = 0; i < numEntries; ++i) {
    allIndices.push_back(acquire.getPartitionIndicesForEntry(i));
  }

  /// All entries must have the same number of indices (same dimensionality)
  size_t numDims = allIndices[0].size();
  if (numDims == 0)
    return false;
  for (const auto &indices : allIndices) {
    if (indices.size() != numDims)
      return false;
  }

  /// For each dimension, check if indices form a stencil pattern
  /// A stencil pattern means all indices are base +/- small constant
  bool foundStencilDim = false;
  minOffset = 0;
  maxOffset = 0;

  for (size_t dim = 0; dim < numDims; ++dim) {
    /// Try each entry as potential base
    bool dimIsStencil = false;
    int64_t dimMin = 0, dimMax = 0;

    for (size_t baseEntry = 0; baseEntry < numEntries && !dimIsStencil;
         ++baseEntry) {
      Value base = allIndices[baseEntry][dim];
      if (!base)
        continue;

      bool allMatch = true;
      int64_t localMin = 0, localMax = 0;

      for (size_t i = 0; i < numEntries; ++i) {
        Value idx = allIndices[i][dim];
        auto offset = getConstantOffsetBetween(idx, base);
        if (!offset || std::abs(*offset) > 2) {
          /// Not a small constant offset - not stencil in this dimension
          allMatch = false;
          break;
        }
        localMin = std::min(localMin, *offset);
        localMax = std::max(localMax, *offset);
      }

      if (allMatch && (localMin != localMax)) {
        /// Found stencil pattern in this dimension
        dimIsStencil = true;
        dimMin = localMin;
        dimMax = localMax;
      }
    }

    if (dimIsStencil) {
      foundStencilDim = true;
      /// Accumulate bounds (for multi-dimensional stencils, use the widest)
      minOffset = std::min(minOffset, dimMin);
      maxOffset = std::max(maxOffset, dimMax);
    }
  }

  return foundStencilDim;
}
