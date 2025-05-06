///==========================================================================
/// File: ArtsUtils.cpp
///==========================================================================
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Ops.h"
#include <cassert>

namespace mlir {
namespace arts {

bool isInvariantInEdt(Region &edtRegion, Value value) {
  /// Case 1: Value is a constant. Constants are always invariant.
  if (isValueConstant(value))
    return true;

  Operation *definingOp = value.getDefiningOp();
  bool definedInside = false;

  if (definingOp) {
    if (edtRegion.isAncestor(definingOp->getParentRegion()))
      definedInside = true;
  } else if (auto blockArg = value.dyn_cast<BlockArgument>()) {
    /// A block arguments is considered defined inside if it's an argument of
    /// a nested block within the EDT region, but not an argument of the EDT
    /// region's entry block itself.
    if (blockArg.getOwner()->getParent() != &edtRegion &&
        edtRegion.isAncestor(blockArg.getOwner()->getParent())) {
      definedInside = true;
    }
    /// Otherwise, it's an argument to the EDT region itself or defined outside,
    /// treat it as potentially invariant but check for writes below.
  } else {
    /// This case should ideally not happen in well-formed IR.
    /// A value should either be a result of an operation or a block argument.
    /// Assert or return conservatively.
    assert(false && "Value is neither defined by an op nor a block argument");
    return false;
  }

  /// Case 2: Value is defined inside the region (and not a constant).
  /// Such values are generally not invariant with respect to the region.
  if (definedInside)
    return false;

  /// Case 3: Value is defined outside the region (or is an argument to the
  /// region's entry block). Check if it is written to by any operation
  /// inside the region.
  for (Operation *user : value.getUsers()) {
    /// Only consider users that are located within the EDT region.
    if (edtRegion.isAncestor(user->getParentRegion())) {
      /// Check for known memory write operations where 'value' is the
      /// destination buffer/pointer.
      if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
        if (storeOp.getMemRef() == value) {
          /// Found a store to 'value' inside the region. Not invariant.
          return false;
        }
      }
    } else if (auto atomicOp = dyn_cast<memref::AtomicRMWOp>(user)) {
      /// Found an atomic op on 'value' inside the region. Not invariant.
      if (atomicOp.getMemref() == value) {
        return false;
      }
    }

    /// TODO: Check for function calls where 'value' might be modified.
  }

  /// If the value is defined outside and no writes/modifications were found
  /// inside the region, it is considered invariant with respect to this region.
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
  llvm::SmallVector<mlir::Operation *, 8> toRemove;
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
  llvm::SmallVector<mlir::arts::UndefOp, 8> undefOps;
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

} // namespace arts
} // namespace mlir
