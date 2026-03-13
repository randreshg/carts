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
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/transforms/dep/DepTransform.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"

using namespace mlir;
using namespace mlir::arts;

namespace mlir {
namespace arts {
namespace {

struct JacobiLoopMatch {
  scf::ForOp timeLoop;
  EdtOp copyEdt;
  ForOp copyFor;
  EdtOp stencilEdt;
  ForOp stencilFor;
  Value stencilInput;
  Value stencilOutput;
};

static bool isSameMemref(Value lhs, Value rhs) {
  return lhs && rhs && DbUtils::isSameMemoryObject(lhs, rhs);
}

static bool haveSameBounds(ForOp lhs, ForOp rhs) {
  if (!lhs || !rhs)
    return false;
  if (lhs.getLowerBound().size() != 1 || lhs.getUpperBound().size() != 1 ||
      lhs.getStep().size() != 1 || rhs.getLowerBound().size() != 1 ||
      rhs.getUpperBound().size() != 1 || rhs.getStep().size() != 1)
    return false;

  return ValueAnalysis::sameValue(lhs.getLowerBound().front(),
                                  rhs.getLowerBound().front()) &&
         ValueAnalysis::sameValue(lhs.getUpperBound().front(),
                                  rhs.getUpperBound().front()) &&
         ValueAnalysis::sameValue(lhs.getStep().front(), rhs.getStep().front());
}

static ForOp getSingleTopLevelFor(EdtOp edt) {
  if (!edt)
    return nullptr;

  ForOp result = nullptr;
  for (Operation &op : edt.getBody().front().without_terminator()) {
    auto forOp = dyn_cast<ForOp>(&op);
    if (!forOp)
      return nullptr;
    if (result)
      return nullptr;
    result = forOp;
  }
  return result;
}

static void stampJacobiAlternatingBuffers(Operation *op) {
  setDepPattern(op, ArtsDepPattern::jacobi_alternating_buffers);
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
  if (isSameMemref(load.getMemRef(), store.getMemRef()))
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
      if (!isSameMemref(store.getMemRef(), expectedOutputMemref)) {
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

    if (isSameMemref(load.getMemRef(), expectedOutputMemref)) {
      invalid = true;
      return WalkResult::interrupt();
    }
    if (isSameMemref(load.getMemRef(), expectedInputMemref)) {
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

  SmallVector<EdtOp, 2> bodyEdts;
  for (Operation &op : timeLoop.getBody()->without_terminator()) {
    auto edt = dyn_cast<EdtOp>(&op);
    if (!edt)
      return false;
    bodyEdts.push_back(edt);
  }

  if (bodyEdts.size() != 2)
    return false;

  EdtOp copyEdt = bodyEdts[0];
  EdtOp stencilEdt = bodyEdts[1];
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
  out.copyEdt = copyEdt;
  out.copyFor = copyFor;
  out.stencilEdt = stencilEdt;
  out.stencilFor = stencilFor;
  out.stencilInput = stencilInput;
  out.stencilOutput = stencilOutput;
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
  OpBuilder builder(match.copyEdt);
  Location loc = match.timeLoop.getLoc();
  Value timeIV = match.timeLoop.getInductionVar();

  Value ivI64 =
      builder.create<arith::IndexCastOp>(loc, builder.getI64Type(), timeIV);
  Value two = builder.create<arith::ConstantIntOp>(loc, 2, 64);
  Value one = builder.create<arith::ConstantIntOp>(loc, 1, 64);
  Value rem = builder.create<arith::RemSIOp>(loc, ivI64, two);
  Value isOdd =
      builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::eq, rem, one);

  auto ifOp = builder.create<scf::IfOp>(loc, TypeRange{}, isOdd,
                                        /*withElseRegion=*/true);
  stampJacobiAlternatingBuffers(ifOp.getOperation());

  cloneStencilEdt(match.stencilEdt, &ifOp.getThenRegion().front(),
                  match.stencilInput, match.stencilOutput, match.stencilOutput,
                  match.stencilInput);
  cloneStencilEdt(match.stencilEdt, &ifOp.getElseRegion().front(),
                  match.stencilInput, match.stencilInput, match.stencilOutput,
                  match.stencilOutput);

  match.copyEdt.erase();
  match.stencilEdt.erase();
  return true;
}

class JacobiAlternatingBuffersPattern final : public DepTransform {
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
    return rewrites;
  }

  StringRef getName() const override {
    return "JacobiAlternatingBuffersPattern";
  }
};

} // namespace

std::unique_ptr<DepTransform> createJacobiAlternatingBuffersPattern() {
  return std::make_unique<JacobiAlternatingBuffersPattern>();
}

} // namespace arts
} // namespace mlir
