///==========================================================================///
/// File: JacobiAlternatingBuffersPattern.cpp
///
/// Elide redundant copy phases in Jacobi-style timestep loops before DB
/// creation. This targets the common copy-then-stencil schedule:
///
///   for t:
///     parallel-for  dst = src
///     parallel-for  src = stencil(dst, forcing)
///
/// and rewrites it into a single alternating stencil phase per timestep:
///
///   for t:
///     if odd(t):
///       parallel-for  dst = stencil(src, forcing)
///     else:
///       parallel-for  src = stencil(dst, forcing)
///
/// For the current implementation we require an even static timestep count so
/// the final result buffer remains unchanged after normalization.
///
/// Before:
///   scf.for %t = %c0 to %T {
///     arts.edt <parallel> { arts.for (...) { memref.store ..., %B[...] } }
///     arts.edt <parallel> { arts.for (...) { memref.store ..., %A[...] } }
///   }
///
/// After:
///   scf.for %t = %c0 to %T {
///     scf.if %odd {
///       arts.edt <parallel> { arts.for (...) { memref.store ..., %B[...] } }
///     } else {
///       arts.edt <parallel> { arts.for (...) { memref.store ..., %A[...] } }
///     }
///   }
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/transforms/dep/DepTransform.h"
#include "arts/transforms/pattern/PatternTransform.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/EdtUtils.h"
#include "arts/utils/LoopUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IntegerSet.h"
#include "mlir/IR/IRMapping.h"

using namespace mlir;
using namespace mlir::arts;

namespace mlir {
namespace arts {
namespace {

struct JacobiLoopMatch {
  scf::ForOp timeLoop;
  Operation *copyAnchor = nullptr;
  EdtOp copyEdt;
  ForOp copyFor;
  Operation *stencilAnchor = nullptr;
  EdtOp stencilEdt;
  ForOp stencilFor;
  Value stencilInput;
  Value stencilOutput;
};

struct JacobiStencilPairMatch {
  scf::ForOp timeLoop;
  Operation *firstAnchor = nullptr;
  EdtOp firstEdt;
  ForOp firstFor;
  Value firstInput;
  Value firstOutput;
  Operation *secondAnchor = nullptr;
  EdtOp secondEdt;
  ForOp secondFor;
  Value secondInput;
  Value secondOutput;
};

struct MatchedJacobiEdt {
  Operation *anchor = nullptr;
  EdtOp edt;
};

static void stampJacobiAlternatingBuffers(Operation *op) {
  static const StencilNDPatternContract contract(
      ArtsDepPattern::jacobi_alternating_buffers, ArrayRef<int64_t>{0},
      ArrayRef<int64_t>{-1}, ArrayRef<int64_t>{1}, ArrayRef<int64_t>{0},
      /*revision=*/1, /*blockShape=*/{}, /*spatialDims=*/{0, 1});
  contract.stamp(op);
}

static std::optional<std::pair<int64_t, int64_t>>
getStaticInclusiveLoopRange(scf::ForOp loop) {
  if (!loop)
    return std::nullopt;

  auto lb = ValueAnalysis::tryFoldConstantIndex(loop.getLowerBound());
  auto ub = ValueAnalysis::tryFoldConstantIndex(loop.getUpperBound());
  auto step = ValueAnalysis::tryFoldConstantIndex(loop.getStep());
  if (!lb || !ub || !step || *step <= 0)
    return std::nullopt;

  int64_t diff = *ub - *lb;
  if (diff <= 0 || (diff % *step) != 0)
    return std::nullopt;

  return std::pair<int64_t, int64_t>{*lb, *ub - *step};
}

static bool extractLinearForm(Value value, Value iv, int64_t &coeff,
                              int64_t &constant) {
  value = ValueAnalysis::stripNumericCasts(value);
  if (!value)
    return false;

  if (value == iv) {
    coeff = 1;
    constant = 0;
    return true;
  }

  if (auto folded = ValueAnalysis::tryFoldConstantIndex(value)) {
    coeff = 0;
    constant = *folded;
    return true;
  }

  if (auto add = value.getDefiningOp<arith::AddIOp>()) {
    int64_t lhsCoeff = 0, lhsConst = 0, rhsCoeff = 0, rhsConst = 0;
    if (!extractLinearForm(add.getLhs(), iv, lhsCoeff, lhsConst) ||
        !extractLinearForm(add.getRhs(), iv, rhsCoeff, rhsConst))
      return false;
    coeff = lhsCoeff + rhsCoeff;
    constant = lhsConst + rhsConst;
    return true;
  }

  if (auto sub = value.getDefiningOp<arith::SubIOp>()) {
    int64_t lhsCoeff = 0, lhsConst = 0, rhsCoeff = 0, rhsConst = 0;
    if (!extractLinearForm(sub.getLhs(), iv, lhsCoeff, lhsConst) ||
        !extractLinearForm(sub.getRhs(), iv, rhsCoeff, rhsConst))
      return false;
    coeff = lhsCoeff - rhsCoeff;
    constant = lhsConst - rhsConst;
    return true;
  }

  if (auto mul = value.getDefiningOp<arith::MulIOp>()) {
    int64_t factor = 0;
    int64_t innerCoeff = 0, innerConst = 0;
    if (auto lhsConst = ValueAnalysis::tryFoldConstantIndex(mul.getLhs())) {
      factor = *lhsConst;
      if (!extractLinearForm(mul.getRhs(), iv, innerCoeff, innerConst))
        return false;
    } else if (auto rhsConst =
                   ValueAnalysis::tryFoldConstantIndex(mul.getRhs())) {
      factor = *rhsConst;
      if (!extractLinearForm(mul.getLhs(), iv, innerCoeff, innerConst))
        return false;
    } else {
      return false;
    }

    coeff = factor * innerCoeff;
    constant = factor * innerConst;
    return true;
  }

  return false;
}

static bool extractAffineLinearForm(AffineExpr expr, ArrayRef<Value> dims,
                                    ArrayRef<Value> symbols, Value iv,
                                    int64_t &coeff, int64_t &constant) {
  if (auto cst = dyn_cast<AffineConstantExpr>(expr)) {
    coeff = 0;
    constant = cst.getValue();
    return true;
  }

  if (auto dim = dyn_cast<AffineDimExpr>(expr)) {
    if (dim.getPosition() >= dims.size())
      return false;
    return extractLinearForm(dims[dim.getPosition()], iv, coeff, constant);
  }

  if (auto sym = dyn_cast<AffineSymbolExpr>(expr)) {
    if (sym.getPosition() >= symbols.size())
      return false;
    return extractLinearForm(symbols[sym.getPosition()], iv, coeff, constant);
  }

  if (auto binary = dyn_cast<AffineBinaryOpExpr>(expr)) {
    int64_t lhsCoeff = 0, lhsConst = 0, rhsCoeff = 0, rhsConst = 0;
    if (!extractAffineLinearForm(binary.getLHS(), dims, symbols, iv, lhsCoeff,
                                 lhsConst) ||
        !extractAffineLinearForm(binary.getRHS(), dims, symbols, iv, rhsCoeff,
                                 rhsConst))
      return false;

    switch (binary.getKind()) {
    case AffineExprKind::Add:
      coeff = lhsCoeff + rhsCoeff;
      constant = lhsConst + rhsConst;
      return true;
    case AffineExprKind::Mul:
      if (lhsCoeff == 0) {
        coeff = lhsConst * rhsCoeff;
        constant = lhsConst * rhsConst;
        return true;
      }
      if (rhsCoeff == 0) {
        coeff = rhsConst * lhsCoeff;
        constant = rhsConst * lhsConst;
        return true;
      }
      return false;
    default:
      return false;
    }
  }

  return false;
}

static bool proveLinearPredicateAlwaysTrue(arith::CmpIPredicate pred,
                                           int64_t coeff, int64_t constant,
                                           int64_t first, int64_t last) {
  int64_t minValue = coeff >= 0 ? coeff * first + constant
                                : coeff * last + constant;
  int64_t maxValue = coeff >= 0 ? coeff * last + constant
                                : coeff * first + constant;

  switch (pred) {
  case arith::CmpIPredicate::eq:
    return minValue == 0 && maxValue == 0;
  case arith::CmpIPredicate::ne:
    return maxValue < 0 || minValue > 0;
  case arith::CmpIPredicate::sge:
  case arith::CmpIPredicate::uge:
    return minValue >= 0;
  case arith::CmpIPredicate::sgt:
  case arith::CmpIPredicate::ugt:
    return minValue > 0;
  case arith::CmpIPredicate::sle:
  case arith::CmpIPredicate::ule:
    return maxValue <= 0;
  case arith::CmpIPredicate::slt:
  case arith::CmpIPredicate::ult:
    return maxValue < 0;
  }
  return false;
}

static bool isGuardConditionAlwaysTrue(Value condition, scf::ForOp timeLoop) {
  auto loopRange = getStaticInclusiveLoopRange(timeLoop);
  if (!loopRange)
    return false;

  auto cmp = ValueAnalysis::stripNumericCasts(condition).getDefiningOp<
      arith::CmpIOp>();
  if (!cmp)
    return false;

  int64_t lhsCoeff = 0, lhsConst = 0, rhsCoeff = 0, rhsConst = 0;
  if (!extractLinearForm(cmp.getLhs(), timeLoop.getInductionVar(), lhsCoeff,
                         lhsConst) ||
      !extractLinearForm(cmp.getRhs(), timeLoop.getInductionVar(), rhsCoeff,
                         rhsConst))
    return false;

  return proveLinearPredicateAlwaysTrue(cmp.getPredicate(),
                                        lhsCoeff - rhsCoeff,
                                        lhsConst - rhsConst, loopRange->first,
                                        loopRange->second);
}

static bool isAffineIfAlwaysTrue(affine::AffineIfOp ifOp, scf::ForOp timeLoop) {
  auto loopRange = getStaticInclusiveLoopRange(timeLoop);
  if (!ifOp || !loopRange)
    return false;

  IntegerSet set = ifOp.getIntegerSet();
  if (!set || set.getNumDims() == 0)
    return false;

  SmallVector<Value, 4> operands(ifOp.getOperands().begin(),
                                 ifOp.getOperands().end());
  ArrayRef<Value> dims(operands.data(), set.getNumDims());
  ArrayRef<Value> symbols = ArrayRef<Value>(operands).drop_front(set.getNumDims());

  for (unsigned i = 0; i < set.getNumConstraints(); ++i) {
    int64_t coeff = 0;
    int64_t constant = 0;
    if (!extractAffineLinearForm(set.getConstraint(i), dims, symbols,
                                 timeLoop.getInductionVar(), coeff, constant))
      return false;

    int64_t minValue = coeff >= 0 ? coeff * loopRange->first + constant
                                  : coeff * loopRange->second + constant;
    int64_t maxValue = coeff >= 0 ? coeff * loopRange->second + constant
                                  : coeff * loopRange->first + constant;

    if (set.isEq(i)) {
      if (minValue != 0 || maxValue != 0)
        return false;
      continue;
    }

    if (minValue < 0)
      return false;
  }

  return true;
}

static EdtOp getSingleThenEdt(Region &region) {
  if (region.empty())
    return nullptr;

  EdtOp result = nullptr;
  for (Operation &op : region.front().without_terminator()) {
    auto edt = dyn_cast<EdtOp>(&op);
    if (!edt || result)
      return nullptr;
    result = edt;
  }
  return result;
}

static bool regionHasNoWork(Region &region) {
  return region.empty() || region.front().without_terminator().empty();
}

static bool isGuardPrepOp(Operation *op) {
  return isa<arith::AddIOp, arith::SubIOp, arith::MulIOp, arith::CmpIOp,
             arith::ConstantOp, arith::IndexCastOp, arith::SelectOp>(op);
}

static bool matchTopLevelJacobiEdt(Operation *op, scf::ForOp timeLoop,
                                   MatchedJacobiEdt &out) {
  out = {};
  if (!op)
    return false;

  if (auto edt = dyn_cast<EdtOp>(op)) {
    out.anchor = op;
    out.edt = edt;
    return true;
  }

  if (auto ifOp = dyn_cast<scf::IfOp>(op)) {
    if (!regionHasNoWork(ifOp.getElseRegion()) ||
        !isGuardConditionAlwaysTrue(ifOp.getCondition(), timeLoop))
      return false;
    if (EdtOp edt = getSingleThenEdt(ifOp.getThenRegion())) {
      out.anchor = op;
      out.edt = edt;
      return true;
    }
    return false;
  }

  if (auto ifOp = dyn_cast<affine::AffineIfOp>(op)) {
    if (!regionHasNoWork(ifOp.getElseRegion()) ||
        !isAffineIfAlwaysTrue(ifOp, timeLoop))
      return false;
    if (EdtOp edt = getSingleThenEdt(ifOp.getThenRegion())) {
      out.anchor = op;
      out.edt = edt;
      return true;
    }
  }

  return false;
}

static bool collectJacobiBodyEdts(scf::ForOp timeLoop,
                                  SmallVectorImpl<MatchedJacobiEdt> &matches) {
  matches.clear();

  for (Operation &op : timeLoop.getBody()->without_terminator()) {
    MatchedJacobiEdt match;
    if (matchTopLevelJacobiEdt(&op, timeLoop, match)) {
      matches.push_back(match);
      continue;
    }

    if (matches.size() == 1 && isGuardPrepOp(&op))
      continue;

    return false;
  }

  return matches.size() == 2;
}

static bool matchSimpleCopyFor(ForOp forOp, Value &srcMemref,
                               Value &dstMemref) {
  srcMemref = Value();
  dstMemref = Value();
  if (!forOp || forOp.getBody()->getNumArguments() != 1)
    return false;

  Value outerIV = forOp.getBody()->getArgument(0);

  scf::ForOp innerLoop = nullptr;
  for (Operation &op : forOp.getBody()->without_terminator()) {
    auto nested = dyn_cast<scf::ForOp>(&op);
    if (!nested)
      return false;
    if (innerLoop)
      return false;
    innerLoop = nested;
  }
  if (!innerLoop || innerLoop.getBody()->getNumArguments() != 1)
    return false;

  Value innerIV = innerLoop.getInductionVar();
  memref::LoadOp load = nullptr;
  memref::StoreOp store = nullptr;
  for (Operation &op : innerLoop.getBody()->without_terminator()) {
    if (auto ld = dyn_cast<memref::LoadOp>(&op)) {
      if (load)
        return false;
      load = ld;
      continue;
    }
    if (auto st = dyn_cast<memref::StoreOp>(&op)) {
      if (store)
        return false;
      store = st;
      continue;
    }
    return false;
  }

  if (!load || !store)
    return false;
  if (store.getValueToStore() != load.getResult())
    return false;
  if (load.getIndices().size() != 2 || store.getIndices().size() != 2)
    return false;
  if (load.getIndices()[0] != outerIV || load.getIndices()[1] != innerIV)
    return false;
  if (store.getIndices()[0] != outerIV || store.getIndices()[1] != innerIV)
    return false;
  if (DbAnalysis::isSameMemoryObject(load.getMemRef(), store.getMemRef()))
    return false;

  srcMemref = load.getMemRef();
  dstMemref = store.getMemRef();
  return true;
}

static bool matchStencilFor(ForOp forOp, Value expectedInputMemref,
                            Value expectedOutputMemref,
                            Value &actualInputMemref,
                            Value &actualOutputMemref) {
  if (!forOp || forOp.getBody()->getNumArguments() != 1)
    return false;

  actualInputMemref = Value();
  actualOutputMemref = Value();
  int inputLoads = 0;
  int outputStores = 0;
  bool invalid = false;

  forOp.walk([&](Operation *op) {
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (!DbAnalysis::isSameMemoryObject(store.getMemRef(),
                                          expectedOutputMemref)) {
        invalid = true;
        return WalkResult::interrupt();
      }
      if (!actualOutputMemref)
        actualOutputMemref = store.getMemRef();
      else if (actualOutputMemref != store.getMemRef()) {
        invalid = true;
        return WalkResult::interrupt();
      }
      outputStores++;
      return WalkResult::advance();
    }

    auto load = dyn_cast<memref::LoadOp>(op);
    if (!load)
      return WalkResult::advance();

    if (DbAnalysis::isSameMemoryObject(load.getMemRef(),
                                       expectedOutputMemref)) {
      invalid = true;
      return WalkResult::interrupt();
    }
    if (DbAnalysis::isSameMemoryObject(load.getMemRef(), expectedInputMemref)) {
      if (!actualInputMemref)
        actualInputMemref = load.getMemRef();
      else if (actualInputMemref != load.getMemRef()) {
        invalid = true;
        return WalkResult::interrupt();
      }
      inputLoads++;
    }
    return WalkResult::advance();
  });

  return !invalid && actualInputMemref && actualOutputMemref &&
         inputLoads >= 4 && outputStores > 0;
}

static bool extractStencilForIO(ForOp forOp, Value &inputMemref,
                                Value &outputMemref) {
  if (!forOp || forOp.getBody()->getNumArguments() != 1)
    return false;

  inputMemref = Value();
  outputMemref = Value();
  int inputLoads = 0;
  int outputStores = 0;
  bool invalid = false;

  forOp.walk([&](Operation *op) {
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (!outputMemref)
        outputMemref = store.getMemRef();
      else if (!DbAnalysis::isSameMemoryObject(outputMemref,
                                               store.getMemRef())) {
        invalid = true;
        return WalkResult::interrupt();
      }
      outputStores++;
      return WalkResult::advance();
    }

    auto load = dyn_cast<memref::LoadOp>(op);
    if (!load)
      return WalkResult::advance();

    if (outputMemref &&
        DbAnalysis::isSameMemoryObject(load.getMemRef(), outputMemref)) {
      invalid = true;
      return WalkResult::interrupt();
    }
    if (!inputMemref)
      inputMemref = load.getMemRef();
    else if (!DbAnalysis::isSameMemoryObject(inputMemref, load.getMemRef())) {
      invalid = true;
      return WalkResult::interrupt();
    }
    inputLoads++;
    return WalkResult::advance();
  });

  return !invalid && inputMemref && outputMemref && inputLoads >= 4 &&
         outputStores > 0 &&
         !DbAnalysis::isSameMemoryObject(inputMemref, outputMemref);
}

static bool hasEvenStaticTripCount(scf::ForOp loop) {
  if (!loop)
    return false;

  auto lb = ValueAnalysis::tryFoldConstantIndex(loop.getLowerBound());
  auto ub = ValueAnalysis::tryFoldConstantIndex(loop.getUpperBound());
  auto step = ValueAnalysis::tryFoldConstantIndex(loop.getStep());
  if (!lb || !ub || !step || *step <= 0)
    return false;

  int64_t diff = *ub - *lb;
  if (diff < 0 || (diff % *step) != 0)
    return false;

  return ((diff / *step) % 2) == 0;
}

static bool matchJacobiTimeLoop(scf::ForOp timeLoop, JacobiLoopMatch &out) {
  out = {};
  if (!timeLoop || timeLoop.getNumResults() != 0 ||
      !hasEvenStaticTripCount(timeLoop))
    return false;

  SmallVector<MatchedJacobiEdt, 2> bodyEdts;
  if (!collectJacobiBodyEdts(timeLoop, bodyEdts))
    return false;

  EdtOp copyEdt = bodyEdts[0].edt;
  EdtOp stencilEdt = bodyEdts[1].edt;
  if (copyEdt.getType() != EdtType::parallel ||
      stencilEdt.getType() != EdtType::parallel)
    return false;

  ForOp copyFor = getSingleTopLevelFor(copyEdt);
  ForOp stencilFor = getSingleTopLevelFor(stencilEdt);
  if (!copyFor || !stencilFor || !haveSameBounds(copyFor, stencilFor))
    return false;

  Value copySrc;
  Value copyDst;
  Value stencilInput;
  Value stencilOutput;
  if (!matchSimpleCopyFor(copyFor, copySrc, copyDst))
    return false;
  if (!matchStencilFor(stencilFor, copyDst, copySrc, stencilInput,
                       stencilOutput))
    return false;

  out.timeLoop = timeLoop;
  out.copyAnchor = bodyEdts[0].anchor;
  out.copyEdt = copyEdt;
  out.copyFor = copyFor;
  out.stencilAnchor = bodyEdts[1].anchor;
  out.stencilEdt = stencilEdt;
  out.stencilFor = stencilFor;
  out.stencilInput = stencilInput;
  out.stencilOutput = stencilOutput;
  return true;
}

static bool matchJacobiStencilPairLoop(scf::ForOp timeLoop,
                                       JacobiStencilPairMatch &out) {
  out = {};
  if (!timeLoop || timeLoop.getNumResults() != 0)
    return false;

  SmallVector<MatchedJacobiEdt, 2> bodyEdts;
  if (!collectJacobiBodyEdts(timeLoop, bodyEdts))
    return false;

  EdtOp firstEdt = bodyEdts[0].edt;
  EdtOp secondEdt = bodyEdts[1].edt;
  if (firstEdt.getType() != EdtType::parallel ||
      secondEdt.getType() != EdtType::parallel)
    return false;

  ForOp firstFor = getSingleTopLevelFor(firstEdt);
  ForOp secondFor = getSingleTopLevelFor(secondEdt);
  if (!firstFor || !secondFor || !haveSameBounds(firstFor, secondFor))
    return false;

  Value firstInput;
  Value firstOutput;
  Value secondInput;
  Value secondOutput;
  if (!extractStencilForIO(firstFor, firstInput, firstOutput))
    return false;
  if (!extractStencilForIO(secondFor, secondInput, secondOutput))
    return false;

  if (!DbAnalysis::isSameMemoryObject(firstOutput, secondInput))
    return false;
  if (!DbAnalysis::isSameMemoryObject(secondOutput, firstInput))
    return false;

  out.timeLoop = timeLoop;
  out.firstAnchor = bodyEdts[0].anchor;
  out.firstEdt = firstEdt;
  out.firstFor = firstFor;
  out.firstInput = firstInput;
  out.firstOutput = firstOutput;
  out.secondAnchor = bodyEdts[1].anchor;
  out.secondEdt = secondEdt;
  out.secondFor = secondFor;
  out.secondInput = secondInput;
  out.secondOutput = secondOutput;
  return true;
}

static EdtOp cloneStencilEdt(EdtOp sourceEdt, Block *targetBlock, Value oldA,
                             Value newA, Value oldB, Value newB) {
  Operation *targetTerminator = targetBlock->getTerminator();
  OpBuilder builder(targetBlock, targetTerminator
                                     ? Block::iterator(targetTerminator)
                                     : targetBlock->end());
  auto cloned = builder.create<EdtOp>(
      sourceEdt.getLoc(), sourceEdt.getType(), sourceEdt.getConcurrency(),
      sourceEdt.getRoute(), sourceEdt.getDependencies());
  cloned->setAttrs(sourceEdt->getAttrs());
  stampJacobiAlternatingBuffers(cloned.getOperation());

  Block &src = sourceEdt.getBody().front();
  Block &dst = cloned.getBody().front();
  IRMapping mapper;

  for (auto [srcArg, dstArg] :
       llvm::zip(src.getArguments(), dst.getArguments()))
    mapper.map(srcArg, dstArg);
  if (oldA && newA)
    mapper.map(oldA, newA);
  if (oldB && newB)
    mapper.map(oldB, newB);

  OpBuilder dstBuilder = OpBuilder::atBlockBegin(&dst);
  for (Operation &op : src.getOperations())
    dstBuilder.clone(op, mapper);

  if (ForOp topLevelFor = getSingleTopLevelFor(cloned))
    stampJacobiAlternatingBuffers(topLevelFor.getOperation());

  return cloned;
}

static bool rewriteJacobiTimeLoop(JacobiLoopMatch &match) {
  OpBuilder builder(match.copyAnchor);
  Location loc = match.timeLoop.getLoc();
  Value timeIV = match.timeLoop.getInductionVar();

  /// Select branches by normalized iteration ordinal instead of absolute
  /// time-IV parity, so behavior is correct for arbitrary loop lower bounds.
  Value lb = match.timeLoop.getLowerBound();
  Value step = match.timeLoop.getStep();
  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value two = builder.create<arith::ConstantIndexOp>(loc, 2);
  Value delta = builder.create<arith::SubIOp>(loc, timeIV, lb);
  Value iterOrdinal = builder.create<arith::DivUIOp>(loc, delta, step);
  Value rem = builder.create<arith::RemUIOp>(loc, iterOrdinal, two);
  Value isSwapIter =
      builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::eq, rem, zero);

  auto ifOp = builder.create<scf::IfOp>(loc, TypeRange{}, isSwapIter,
                                        /*withElseRegion=*/true);
  stampJacobiAlternatingBuffers(ifOp.getOperation());

  cloneStencilEdt(match.stencilEdt, &ifOp.getThenRegion().front(),
                  match.stencilInput, match.stencilOutput, match.stencilOutput,
                  match.stencilInput);
  cloneStencilEdt(match.stencilEdt, &ifOp.getElseRegion().front(),
                  match.stencilInput, match.stencilInput, match.stencilOutput,
                  match.stencilOutput);

  match.copyAnchor->erase();
  match.stencilAnchor->erase();
  return true;
}

static bool stampJacobiStencilPair(JacobiStencilPairMatch &match) {
  if (!match.timeLoop || getDepPattern(match.timeLoop.getOperation()))
    return false;

  stampJacobiAlternatingBuffers(match.timeLoop.getOperation());
  stampJacobiAlternatingBuffers(match.firstEdt.getOperation());
  stampJacobiAlternatingBuffers(match.firstFor.getOperation());
  stampJacobiAlternatingBuffers(match.secondEdt.getOperation());
  stampJacobiAlternatingBuffers(match.secondFor.getOperation());
  return true;
}

class JacobiAlternatingBuffersPattern final : public DepPatternTransform {
public:
  int run(ModuleOp module) override {
    int rewrites = 0;
    while (true) {
      JacobiLoopMatch match;
      bool foundMatch = false;
      module.walk([&](scf::ForOp loop) {
        if (matchJacobiTimeLoop(loop, match)) {
          foundMatch = true;
          return WalkResult::interrupt();
        }
        return WalkResult::advance();
      });
      if (!foundMatch)
        break;

      if (rewriteJacobiTimeLoop(match))
        rewrites++;
    }

    module.walk([&](scf::ForOp loop) {
      JacobiStencilPairMatch match;
      if (matchJacobiStencilPairLoop(loop, match) &&
          stampJacobiStencilPair(match))
        rewrites++;
    });

    return rewrites;
  }

  StringRef getName() const override {
    return "JacobiAlternatingBuffersPattern";
  }
  ArtsDepPattern getFamily() const override {
    return ArtsDepPattern::jacobi_alternating_buffers;
  }
  int64_t getRevision() const override { return 1; }
};

} // namespace

std::unique_ptr<DepPatternTransform> createJacobiAlternatingBuffersPattern() {
  return std::make_unique<JacobiAlternatingBuffersPattern>();
}

} // namespace arts
} // namespace mlir
