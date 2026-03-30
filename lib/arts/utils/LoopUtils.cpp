///==========================================================================///
/// File: LoopUtils.cpp
///
/// Implementation of non-inline loop utility functions.
///==========================================================================///

#include "arts/utils/LoopUtils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "llvm/ADT/StringRef.h"

namespace mlir {
namespace arts {

static std::optional<int64_t> getStaticTripCount(scf::ForOp loop) {
  auto lb = ValueAnalysis::tryFoldConstantIndex(loop.getLowerBound());
  auto ub = ValueAnalysis::tryFoldConstantIndex(loop.getUpperBound());
  auto step = ValueAnalysis::tryFoldConstantIndex(loop.getStep());
  if (!lb || !ub || !step || *step <= 0)
    return std::nullopt;
  int64_t span = std::max<int64_t>(0, *ub - *lb);
  return (span + *step - 1) / *step;
}

static std::optional<int64_t> getStaticTripCount(arts::ForOp loop) {
  if (!loop || loop.getLowerBound().size() != 1 || loop.getUpperBound().size() != 1 ||
      loop.getStep().size() != 1)
    return std::nullopt;

  auto lb = ValueAnalysis::tryFoldConstantIndex(loop.getLowerBound().front());
  auto ub = ValueAnalysis::tryFoldConstantIndex(loop.getUpperBound().front());
  auto step = ValueAnalysis::tryFoldConstantIndex(loop.getStep().front());
  if (!lb || !ub || !step || *step <= 0)
    return std::nullopt;
  int64_t span = std::max<int64_t>(0, *ub - *lb);
  return (span + *step - 1) / *step;
}

static std::optional<int64_t> getStaticTripCount(affine::AffineForOp loop) {
  if (!loop.hasConstantLowerBound() || !loop.hasConstantUpperBound())
    return std::nullopt;
  int64_t step = loop.getStep().getSExtValue();
  if (step <= 0)
    return std::nullopt;
  int64_t span =
      std::max<int64_t>(0, loop.getConstantUpperBound() - loop.getConstantLowerBound());
  return (span + step - 1) / step;
}

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

std::optional<int64_t> getStaticTripCount(Operation *loopOp) {
  if (!loopOp)
    return std::nullopt;
  if (auto scfFor = dyn_cast<scf::ForOp>(loopOp))
    return getStaticTripCount(scfFor);
  if (auto artsFor = dyn_cast<arts::ForOp>(loopOp))
    return getStaticTripCount(artsFor);
  if (auto affineFor = dyn_cast<affine::AffineForOp>(loopOp))
    return getStaticTripCount(affineFor);
  return std::nullopt;
}

int64_t getRepeatedParentTripProduct(Operation *op, int64_t maxProduct) {
  if (!op)
    return 1;

  int64_t product = 1;
  for (Operation *parent = op->getParentOp(); parent;
       parent = parent->getParentOp()) {
    if (isa<func::FuncOp>(parent))
      break;
    auto tripOpt = getStaticTripCount(parent);
    if (!tripOpt || *tripOpt <= 1)
      continue;
    if (*tripOpt >= maxProduct / product)
      return maxProduct;
    product *= *tripOpt;
    if (product >= maxProduct)
      return maxProduct;
  }
  return product;
}

static bool hasFloatingPointType(Type type) {
  if (!type)
    return false;
  if (type.isF16() || type.isBF16() || type.isF32() || type.isF64() ||
      type.isF80() || type.isF128())
    return true;
  if (auto vectorType = dyn_cast<VectorType>(type))
    return hasFloatingPointType(vectorType.getElementType());
  return false;
}

static bool operationTouchesFloatingPoint(Operation *op) {
  for (Value operand : op->getOperands()) {
    if (hasFloatingPointType(operand.getType()))
      return true;
  }
  for (Value result : op->getResults()) {
    if (hasFloatingPointType(result.getType()))
      return true;
  }
  return false;
}

static bool isFusionHostileMathOp(Operation *op) {
  llvm::StringRef name = op->getName().getStringRef();
  return name == "math.exp" || name == "math.exp2" ||
         name == "math.expm1" || name == "math.log" ||
         name == "math.log1p" || name == "math.log2" ||
         name == "math.log10" || name == "math.sin" ||
         name == "math.cos" || name == "math.tan" ||
         name == "math.tanh" || name == "math.erf" ||
         name == "math.powf";
}

PointwiseLoopComputeClass classifyPointwiseLoopCompute(arts::ForOp loop) {
  if (!loop)
    return PointwiseLoopComputeClass::arithmeticOnly;

  PointwiseLoopComputeClass computeClass =
      PointwiseLoopComputeClass::arithmeticOnly;
  loop.walk([&](Operation *op) {
    if (auto call = dyn_cast<func::CallOp>(op)) {
      if (operationTouchesFloatingPoint(call)) {
        computeClass = PointwiseLoopComputeClass::scalarCall;
        return WalkResult::interrupt();
      }
      return WalkResult::advance();
    }

    if (computeClass == PointwiseLoopComputeClass::arithmeticOnly &&
        isFusionHostileMathOp(op) && operationTouchesFloatingPoint(op))
      computeClass = PointwiseLoopComputeClass::vectorMath;
    return WalkResult::advance();
  });
  return computeClass;
}

bool loopHasFusionHostileFpWork(arts::ForOp loop) {
  return classifyPointwiseLoopCompute(loop) !=
         PointwiseLoopComputeClass::arithmeticOnly;
}

} // namespace arts
} // namespace mlir
