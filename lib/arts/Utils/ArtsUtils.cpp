///==========================================================================
/// File: ArtsUtils.cpp
///==========================================================================
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Ops.h"

namespace mlir {
namespace arts {

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
  if (op)
    op->erase();
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

void replaceInRegion(Region &region, DenseMap<Value, Value> &rewireMap) {
  for (auto &rewire : rewireMap)
    replaceInRegion(region, rewire.first, rewire.second);
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

  auto constantOp = dyn_cast<arith::ConstantOp>(defOp);
  if (!constantOp)
    return false;
  return true;
};

} // namespace arts
} // namespace mlir
