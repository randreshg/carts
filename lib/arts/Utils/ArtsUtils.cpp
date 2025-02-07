///==========================================================================
/// File: ArtsUtils.cpp
///==========================================================================
#include "arts/Utils/ArtsUtils.h"
#include "polygeist/Ops.h"

namespace mlir {
namespace arts {

void recursivelyRemoveUses(mlir::Operation *op) {
  /// Collect all dependent operations.
  llvm::SmallVector<mlir::Operation *, 8> toRemove;
  for (mlir::Value result : op->getResults()) {
    for (mlir::Operation *user : result.getUsers()) {
      toRemove.push_back(user);
    }
  }

  /// Remove dependent operations recursively.
  for (mlir::Operation *userOp : toRemove) {
    recursivelyRemoveUses(userOp);
    userOp->erase();
  }
}

void removeUndefOps(mlir::ModuleOp module) {
  /// Traverse all operations in the module and collect UndefOps.
  llvm::SmallVector<mlir::arts::UndefOp, 8> undefOps;
  module.walk(
      [&](mlir::arts::UndefOp undefOp) { undefOps.push_back(undefOp); });
  /// Process each UndefOp and remove its dependent operations.
  for (auto undefOp : undefOps) {
    recursivelyRemoveUses(undefOp);
    undefOp.erase();
  }
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

void replaceInRegion(Region &region, Value from, Value to) {
  from.replaceUsesWithIf(to, [&](OpOperand &operand) {
    return region.isAncestor(operand.getOwner()->getParentRegion());
  });
}

void replaceInRegion(Region &region, DenseMap<Value, Value> &rewireMap) {
  for (auto &rewire : rewireMap)
    replaceInRegion(region, rewire.first, rewire.second);
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

} // namespace arts
} // namespace mlir
