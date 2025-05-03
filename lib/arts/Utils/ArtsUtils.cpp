///==========================================================================
/// File: ArtsUtils.cpp
///==========================================================================
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Ops.h"

namespace mlir {
namespace arts {

bool isInvariantInEDT(arts::EdtOp edtOp, Value value) {
  auto &region = edtOp.getRegion();

  /// Check if the value is a constant or a constant-like operation.
  Operation *op = value.getDefiningOp();
  if (op && op->hasTrait<OpTrait::ConstantLike>())
    return true;

  /// If not defined inside the region, check all users
  if (!region.isProperAncestor(value.getParentRegion())) {
    for (Operation *user : value.getUsers()) {
      if (!region.isAncestor(user->getParentRegion()))
        continue;

      if (mlir::isMemoryEffectFree(user))
        continue;

      /// Check specific memory operations where value is used as an index
      if (auto memOp = dyn_cast<memref::LoadOp>(user)) {
        if (llvm::is_contained(memOp.getIndices(), value))
          return true;
      } else if (auto memOp = dyn_cast<memref::StoreOp>(user)) {
        if (llvm::is_contained(memOp.getIndices(), value))
          return true;
        /// Check printf calls
      } else if (auto callOp = dyn_cast<func::CallOp>(user)) {
        if (callOp.getCallee() == "printf")
          return true;
      } else if (auto llvmCallOp = dyn_cast<LLVM::CallOp>(user)) {
        if (llvmCallOp.getCallee() == "printf")
          return true;
      }
      return false;
    }
    return true;
  }

  /// An operation defined inside the region is consider invariant if all of its
  /// operands are invariant.
  for (Value operand : op->getOperands()) {
    if (!isInvariantInEDT(edtOp, operand))
      return false;
  }

  /// If we hit this point, the value is invariant in the EDT region.
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

std::optional<int64_t> computeConstant(Value val) {
  /// Compute a constant integer value from a Value.
  if (auto cst = val.getDefiningOp<arith::ConstantIndexOp>())
    return cst.value();
  if (auto addOp = dyn_cast_or_null<arith::AddIOp>(val.getDefiningOp())) {
    auto lhs = computeConstant(addOp.getOperand(0));
    auto rhs = computeConstant(addOp.getOperand(1));
    if (lhs && rhs)
      return *lhs + *rhs;
  }
  if (auto mulOp = dyn_cast_or_null<arith::MulIOp>(val.getDefiningOp())) {
    auto lhs = computeConstant(mulOp.getOperand(0));
    auto rhs = computeConstant(mulOp.getOperand(1));
    if (lhs && rhs)
      return (*lhs) * (*rhs);
  }
  return std::nullopt;
}

int64_t tryParseIndexConstant(Value val) {
  /// Try to parse an index constant from a Value.
  if (auto c = computeConstant(val))
    return *c;
  ValueOrInt voi(val);
  if (!voi.isValue)
    return voi.i_val;
  return -1;
}

bool isValueConstant(Value val) {
  auto defOp = val.getDefiningOp();
  if (!defOp)
    return false;

  if (isa<arith::ConstantIndexOp>(defOp) || isa<arith::ConstantOp>(defOp))
    return true;
  return false;
};

} // namespace arts
} // namespace mlir
