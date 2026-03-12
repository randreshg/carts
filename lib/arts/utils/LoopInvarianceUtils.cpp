///==========================================================================///
/// File: LoopInvarianceUtils.cpp
///
/// Consolidated loop invariance checking and hoisting safety utilities.
/// Previously duplicated across Hoisting.cpp, DataPointerHoisting.cpp,
/// and ScalarReplacement.cpp.
///==========================================================================///

#include "arts/utils/LoopInvarianceUtils.h"
#include "arts/utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

namespace mlir {
namespace arts {

bool isLoopInvariant(scf::ForOp loop, Value v) {
  if (!v)
    return true;
  if (loop.isDefinedOutsideOfLoop(v))
    return true;
  Value stripped = ValueUtils::stripNumericCasts(v);
  return ValueUtils::isValueConstant(stripped);
}

bool isSafeToHoistDivRem(scf::ForOp loop, Operation *op) {
  if (auto div = dyn_cast<arith::DivUIOp>(op)) {
    Value denom = div.getRhs();
    return isLoopInvariant(loop, denom) &&
           ValueUtils::isProvablyNonZero(denom);
  }
  if (auto rem = dyn_cast<arith::RemUIOp>(op)) {
    Value denom = rem.getRhs();
    return isLoopInvariant(loop, denom) &&
           ValueUtils::isProvablyNonZero(denom);
  }
  return false;
}

bool isSafeDivRemToHoist(Operation *op, scf::ForOp loop,
                         DominanceInfo &domInfo) {
  if (!loop || !loop->isAncestor(op))
    return false;
  Value denom;
  if (auto div = dyn_cast<arith::DivUIOp>(op))
    denom = div.getRhs();
  else if (auto rem = dyn_cast<arith::RemUIOp>(op))
    denom = rem.getRhs();
  else
    return false;

  if (!ValueUtils::isProvablyNonZero(denom))
    return false;
  if (!allOperandsDominate(op, loop, domInfo))
    return false;
  return true;
}

bool isDefinedOutside(Region &region, Value value) {
  return !region.isAncestor(value.getParentRegion());
}

bool allOperandsDefinedOutside(Operation *op, Region &region) {
  for (Value operand : op->getOperands()) {
    if (!isDefinedOutside(region, operand))
      return false;
  }
  return true;
}

bool allOperandsDominate(Operation *op, Operation *insertionPoint,
                         DominanceInfo &domInfo) {
  for (Value operand : op->getOperands()) {
    if (!domInfo.properlyDominates(operand, insertionPoint))
      return false;
  }
  return true;
}

} // namespace arts
} // namespace mlir
