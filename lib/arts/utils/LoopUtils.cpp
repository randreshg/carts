///==========================================================================///
/// File: LoopUtils.cpp
///
/// Implementation of non-inline loop utility functions.
///==========================================================================///

#include "arts/utils/LoopUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"

namespace mlir {
namespace arts {

unsigned getLoopDepth(Operation *op) {
  unsigned depth = 0;
  for (Operation *parent = op ? op->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    if (isa<affine::AffineForOp, scf::ForOp, scf::ParallelOp, scf::ForallOp,
            omp::WsloopOp, arts::ForOp>(parent))
      ++depth;
  }
  return depth;
}

void collectWhileBounds(Value cond, Value iterArg, SmallVector<Value> &bounds) {
  if (!cond)
    return;
  cond = ValueAnalysis::stripNumericCasts(cond);
  if (auto andOp = cond.getDefiningOp<arith::AndIOp>()) {
    collectWhileBounds(andOp.getLhs(), iterArg, bounds);
    collectWhileBounds(andOp.getRhs(), iterArg, bounds);
    return;
  }
  if (auto cmp = cond.getDefiningOp<arith::CmpIOp>()) {
    auto lhs = ValueAnalysis::stripNumericCasts(cmp.getLhs());
    auto rhs = ValueAnalysis::stripNumericCasts(cmp.getRhs());
    auto pred = cmp.getPredicate();
    auto isLessPred =
        pred == arith::CmpIPredicate::slt || pred == arith::CmpIPredicate::ult;
    if (lhs == iterArg && isLessPred) {
      bounds.push_back(cmp.getRhs());
      return;
    }
    if (rhs == iterArg && (pred == arith::CmpIPredicate::sgt ||
                           pred == arith::CmpIPredicate::ugt)) {
      bounds.push_back(cmp.getLhs());
      return;
    }
  }
}

bool containsLoop(EdtOp edt) {
  bool found = false;
  edt.getBody().walk([&](Operation *op) {
    if (isa<scf::ForOp, scf::ParallelOp, affine::AffineForOp>(op)) {
      found = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return found;
}

} // namespace arts
} // namespace mlir
