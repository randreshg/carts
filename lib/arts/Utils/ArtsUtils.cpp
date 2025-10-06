///==========================================================================
/// File: ArtsUtils.cpp
/// Defines utility functions for the ARTS dialect.
///==========================================================================
#include "arts/Utils/ArtsUtils.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/CSE.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <algorithm>
#include <cassert>

namespace mlir {
namespace arts {

bool simplifyIR(ModuleOp module, DominanceInfo &domInfo) {
  IRRewriter rewriter(module.getContext());
  bool changed = false;
  eliminateCommonSubExpressions(rewriter, domInfo, module, &changed);
  return changed;
}

bool isInvariantInEdt(Region &edtRegion, Value value) {
  llvm::SmallPtrSet<Value, 16> visited;

  std::function<bool(Value)> definedInvariant = [&](Value v) -> bool {
    if (!v)
      return false;
    if (!visited.insert(v).second)
      return true;

    if (isValueConstant(v))
      return true;

    if (auto blockArg = v.dyn_cast<BlockArgument>()) {
      Block *owner = blockArg.getOwner();
      if (!owner)
        return false;
      Region *ownerRegion = owner->getParent();
      if (!ownerRegion)
        return false;

      /// Entry block arguments of the EDT region are considered invariant
      /// because their defining value is outside the region.
      if (ownerRegion == &edtRegion && owner == &edtRegion.front())
        return true;

      /// Block arguments that belong to regions outside of this EDT are also
      /// treated as invariant inputs.
      if (!edtRegion.isAncestor(ownerRegion))
        return true;

      return false;
    }

    Operation *defOp = v.getDefiningOp();
    if (!defOp)
      return false;

    if (!edtRegion.isAncestor(defOp->getParentRegion()))
      return true;

    if (isa<arith::ConstantIndexOp, arith::ConstantIntOp>(defOp))
      return true;

    if (isa<arith::AddIOp, arith::SubIOp, arith::MulIOp, arith::DivSIOp,
            arith::DivUIOp, arith::IndexCastOp, arith::ExtSIOp, arith::ExtUIOp,
            arith::TruncIOp>(defOp)) {
      for (Value operand : defOp->getOperands())
        if (!definedInvariant(operand))
          return false;
      return true;
    }

    return false;
  };

  if (!definedInvariant(value))
    return false;

  auto isPointerLike = [](Type type) {
    return type.isa<MemRefType, UnrankedMemRefType>();
  };

  if (!isPointerLike(value.getType()))
    return true;

  for (Operation *user : value.getUsers()) {
    if (!edtRegion.isAncestor(user->getParentRegion()))
      continue;

    if (auto store = dyn_cast<memref::StoreOp>(user))
      if (store.getMemref() == value)
        return false;

    if (auto atomicRmw = dyn_cast<memref::AtomicRMWOp>(user))
      if (atomicRmw.getMemref() == value)
        return false;
  }

  return true;
}

bool isReachable(Operation *source, Operation *target) {
  /// Early exit if either pointer is null or both are the same.
  if (!source || !target)
    return false;
  if (source == target)
    return true;

  /// If both operations are in the same block, check their order.
  if (source->getBlock() == target->getBlock())
    return source->isBeforeInBlock(target);

  /// Get the EDT parent for both operations. This acts as our upper limit.
  auto srcEdt = source->getParentOfType<EdtOp>();
  auto tgtEdt = target->getParentOfType<EdtOp>();
  if (srcEdt != tgtEdt)
    return false;

  /// Traverse up the parent chain simultaneously but stop once the EDT parent
  /// is reached.
  Operation *src = source;
  Operation *tgt = target;
  while (true) {
    if (src->getBlock() == tgt->getBlock())
      return src->isBeforeInBlock(tgt);
    if (src == srcEdt && tgt == tgtEdt)
      break;
    if (src->getParentOp() != srcEdt)
      src = src->getParentOp();
    if (tgt->getParentOp() != tgtEdt)
      tgt = tgt->getParentOp();
  }
  return false;
}

void removeOps(mlir::ModuleOp module, OpBuilder &builder,
               llvm::SetVector<mlir::Operation *> &opsToRemove) {
  for (auto op : opsToRemove) {
    if (!op)
      continue;
    replaceWithUndef(op, builder);
    recursivelyRemoveOp(op);
  }
  removeUndefOps(module);
  opsToRemove.clear();
}

void recursivelyRemoveOp(mlir::Operation *op) {
  /// Collect all dependent operations.
  SmallVector<mlir::Operation *, 8> toRemove;
  for (mlir::Value result : op->getResults()) {
    for (mlir::Operation *user : result.getUsers()) {
      toRemove.push_back(user);
    }
  }

  /// Remove dependent operations recursively.
  for (mlir::Operation *userOp : toRemove)
    recursivelyRemoveOp(userOp);

  /// Remove the operation.
  if (op) {
    op->erase();
    op = nullptr;
  }
}

void removeUndefOps(mlir::ModuleOp module) {
  /// Traverse all operations in the module and collect UndefOps.
  SmallVector<mlir::arts::UndefOp, 8> undefOps;
  module.walk(
      [&](mlir::arts::UndefOp undefOp) { undefOps.push_back(undefOp); });
  /// Process each UndefOp and remove its dependent operations.
  for (auto undefOp : undefOps)
    recursivelyRemoveOp(undefOp);
}

void replaceWithUndef(mlir::Operation *op, OpBuilder &builder) {
  if (op->getNumResults() == 0)
    return;

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(op);
  for (mlir::Value result : op->getResults()) {
    auto undefOp =
        builder.create<mlir::arts::UndefOp>(op->getLoc(), result.getType());
    result.replaceAllUsesWith(undefOp.getResult());
  }
}

void replaceUses(mlir::Value from, mlir::Value to, DominanceInfo &domInfo,
                 Operation *dominatingOp) {
  /// Ensure that 'from' has a defining operation.
  auto *definingOp = from.getDefiningOp();
  if (!definingOp)
    return;

  /// Replace all uses of 'from' with 'to' if the user is dominated by the
  /// defining operation of 'from'.
  from.replaceUsesWithIf(to, [&](OpOperand &operand) {
    if (operand.getOwner() == dominatingOp)
      return false;

    return domInfo.dominates(dominatingOp, operand.getOwner());
  });
}

void replaceUses(DenseMap<Value, Value> &rewireMap) {
  for (auto &rewire : rewireMap)
    rewire.first.replaceAllUsesWith(rewire.second);
  rewireMap.clear();
}

void replaceInRegion(Region &region, Value from, Value to) {
  from.replaceUsesWithIf(to, [&](OpOperand &operand) {
    return region.isAncestor(operand.getOwner()->getParentRegion());
  });
}

void replaceInRegion(Region &region, DenseMap<Value, Value> &rewireMap,
                     bool clear) {
  for (auto &rewire : rewireMap)
    replaceInRegion(region, rewire.first, rewire.second);
  if (clear)
    rewireMap.clear();
}

bool isValueConstant(Value val) {
  Operation *defOp = val.getDefiningOp();
  if (!defOp)
    return false;

  if (defOp->hasTrait<OpTrait::ConstantLike>())
    return true;

  if (isa<arith::ConstantIndexOp>(defOp) || isa<arith::ConstantOp>(defOp))
    return true;
  return false;
};

bool getConstantIndex(Value v, int64_t &out) {
  if (!v)
    return false;
  if (auto c = v.getDefiningOp<arith::ConstantIndexOp>()) {
    out = c.value();
    return true;
  }
  if (auto c = v.getDefiningOp<arith::ConstantOp>()) {
    if (auto intAttr = llvm::dyn_cast<mlir::IntegerAttr>(c.getValue())) {
      out = intAttr.getInt();
      return true;
    }
  }
  return false;
}

bool isNonZeroIndex(Value v) {
  if (!v)
    return false;
  if (auto c = v.getDefiningOp<arith::ConstantIndexOp>())
    return c.value() != 0;
  return true;
}

/// Internal helper with recursion detection
static Value getUnderlyingValueImpl(Value v,
                                    llvm::SmallPtrSet<Value, 8> &visited) {
  if (!v)
    return nullptr;

  /// Check for cycles
  if (!visited.insert(v).second)
    return nullptr;

  if (v.isa<BlockArgument>()) {
    Block *block = v.getParentBlock();
    Operation *owner = block->getParentOp();
    if (auto edt = dyn_cast<EdtOp>(owner)) {
      unsigned argIndex = v.cast<BlockArgument>().getArgNumber();
      /// Block arguments correspond directly to EDT operands
      if (argIndex < edt.getNumOperands()) {
        Value operand = edt.getOperand(argIndex);
        return getUnderlyingValueImpl(operand, visited);
      } else {
        return v;
      }
    } else {
      /// Function argument
      return v;
    }
  } else if (auto op = v.getDefiningOp()) {
    /// Handle different operation types
    if (isa<DbAllocOp, memref::AllocOp, memref::AllocaOp, memref::GetGlobalOp>(
            op))
      return v;
    else if (auto dbAcquire = dyn_cast<DbAcquireOp>(op))
      return getUnderlyingValueImpl(dbAcquire.getSourcePtr(), visited);
    else if (auto dbGep = dyn_cast<DbGepOp>(op))
      return getUnderlyingValueImpl(dbGep.getBasePtr(), visited);
    else if (auto subview = dyn_cast<memref::SubViewOp>(op))
      return getUnderlyingValueImpl(subview.getSource(), visited);
    else if (auto castOp = dyn_cast<memref::CastOp>(op))
      return getUnderlyingValueImpl(castOp.getSource(), visited);
    else
      return nullptr;
  } else {
    /// Value has no defining operation and is not a block argument
    return nullptr;
  }
}

Value getUnderlyingValue(Value v) {
  llvm::SmallPtrSet<Value, 8> visited;
  return getUnderlyingValueImpl(v, visited);
}

Operation *getUnderlyingOperation(Value v) {
  Value underlyingValue = getUnderlyingValue(v);
  if (!underlyingValue)
    return nullptr;

  /// If the underlying value is a result of an operation, return that operation
  if (auto definingOp = underlyingValue.getDefiningOp())
    return definingOp;

  return nullptr;
}

/// Returns the datablock-producing/consuming op tied to a value if present.
Operation *getUnderlyingDb(Value v) {
  if (!v)
    return nullptr;

  /// Directly return the DbAcquireOp or DbAllocOp if present
  if (auto acq = v.getDefiningOp<DbAcquireOp>())
    return acq.getOperation();
  if (auto alloc = v.getDefiningOp<DbAllocOp>())
    return alloc.getOperation();

  /// If it's a block argument of an EDT, map to the corresponding operand and
  /// recurse. Block arguments correspond to dependencies (after optional
  /// route).
  if (auto blockArg = v.dyn_cast<BlockArgument>()) {
    Block *block = blockArg.getOwner();
    Operation *owner = block->getParentOp();
    if (auto edt = dyn_cast<EdtOp>(owner)) {
      unsigned argIndex = blockArg.getArgNumber();
      /// EDT operands layout: [optional route] [dependencies...]
      unsigned numOperands = edt.getNumOperands();
      if (argIndex < numOperands) {
        Value operand = edt.getOperand(argIndex);
        return getUnderlyingDb(operand);
      }
    }
  }

  /// Peel through common view/cast/gep nodes to reach DB ops
  if (Operation *def = v.getDefiningOp()) {
    if (auto gep = dyn_cast<DbGepOp>(def))
      return getUnderlyingDb(gep.getBasePtr());
    if (auto castOp = dyn_cast<memref::CastOp>(def))
      return getUnderlyingDb(castOp.getSource());
    if (auto subview = dyn_cast<memref::SubViewOp>(def))
      return getUnderlyingDb(subview.getSource());
  }

  /// As a last resort, try underlying root op if it's a DB op
  if (Operation *root = getUnderlyingOperation(v)) {
    if (isa<DbAcquireOp, DbAllocOp>(root))
      return root;
  }

  return nullptr;
}

uint64_t getElementTypeByteSize(Type elemTy) {
  if (auto intTy = dyn_cast<IntegerType>(elemTy))
    return intTy.getWidth() / 8u;
  if (auto fTy = dyn_cast<FloatType>(elemTy))
    return fTy.getWidth() / 8u;
  return 0; /// unknown
}

std::string sanitizeString(StringRef s) {
  std::string id = s.str();
  std::replace(id.begin(), id.end(), '.', '_');
  std::replace(id.begin(), id.end(), '-', '_');
  return id;
}

bool equalRange(ValueRange a, ValueRange b) {
  if (a.size() != b.size())
    return false;
  for (auto it = a.begin(), jt = b.begin(); it != a.end(); ++it, ++jt) {
    if (*it != *jt)
      return false;
  }
  return true;
}

bool allSameValue(ValueRange values) {
  if (values.empty())
    return false;
  Value first = values[0];
  return llvm::all_of(values, [&](Value v) { return v == first; });
}

} // namespace arts
} // namespace mlir
