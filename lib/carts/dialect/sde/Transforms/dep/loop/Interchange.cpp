///==========================================================================///
/// LoopInterchange.cpp - Cache-optimal memref loop interchange
///
/// Two memref interchange strategies:
///
/// 1. Direct matmul accumulators: rewrites j-k accumulation nests to k-j
///    order so the B/C row dimension is stride-1 in the innermost loop.
///
/// 2. Stencil halo ordering: for stencil classification, reads
///    accessMinOffsets/accessMaxOffsets to compute per-dim halo
///    width. For 3D+ stencils with multiple inner loops (affine.for or
///    scf.for), reorders so the smallest-halo-width dim is outermost (minimizes
///    total halo volume). Affine nests use upstream `permuteLoops`.
///
/// The interchange handles imperfect nests by distributing init ops into a
/// separate loop when the j loop contains both init stores and a reduction
/// loop in the same imperfect nest.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_LOOPINTERCHANGE
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Affine/LoopUtils.h"
#include "mlir/Dialect/Affine/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/Debug.h"
#include "carts/utils/ValueAnalysis.h"
ARTS_DEBUG_SETUP(loop_interchange);

using namespace mlir;
using namespace mlir::carts;

namespace {

/// Interchange may still rewrite inner loops until CU group block counts are
/// committed. `hasCommittedCuMuPartitionFacts` also fires on layout-root shape
/// recovery and would skip interchange too early.
static bool hasCommittedInterchangeGrainFacts(sde::SdeSuIterateOp op) {
  if (sde::SdeCuRegionOp cu = sde::findSuComputeCuRegion(op))
    return cu.getGroupBlockCountAttr() != nullptr;
  return false;
}

static bool isTerminatorOp(Operation &op) {
  return op.hasTrait<OpTrait::IsTerminator>();
}

/// Collect all nested scf.for loops from the su_iterate body into a flat list.
/// Returns false if the nest structure is not a simple linear chain of scf.for
/// loops.
static bool collectInnerForChain(Block &body,
                                 SmallVectorImpl<scf::ForOp> &loops) {
  Block *current = &body;
  while (true) {
    scf::ForOp found;
    for (Operation &op : *current) {
      if (isTerminatorOp(op))
        continue;
      if (auto forOp = dyn_cast<scf::ForOp>(op)) {
        if (found)
          return false; // multiple loops at same level
        found = forOp;
      }
      // Allow non-for ops such as arith ops for index computation.
    }
    if (!found)
      break;
    loops.push_back(found);
    current = found.getBody();
  }
  return !loops.empty();
}

/// Collect a linear chain of nested affine.for loops from the su_iterate body.
static bool
collectInnerAffineForChain(Block &body,
                           SmallVectorImpl<affine::AffineForOp> &loops) {
  Block *current = &body;
  while (true) {
    affine::AffineForOp found;
    for (Operation &op : *current) {
      if (isTerminatorOp(op))
        continue;
      if (auto forOp = dyn_cast<affine::AffineForOp>(op)) {
        if (found)
          return false;
        found = forOp;
      }
    }
    if (!found)
      break;
    loops.push_back(found);
    current = found.getBody();
  }
  return !loops.empty();
}

/// Swap the innermost affine.for pair via upstream permuteLoops (k-j order).
static bool interchangeAffineLoopPair(affine::AffineForOp outerLoop,
                                      affine::AffineForOp innerLoop) {
  if (innerLoop->getParentOp() != outerLoop.getOperation())
    return false;
  if (outerLoop.getNumResults() != 0 || innerLoop.getNumResults() != 0)
    return false;

  for (Operation &op : *outerLoop.getBody()) {
    if (isTerminatorOp(op))
      continue;
    if (&op == innerLoop.getOperation())
      break;
    return false;
  }

  SmallVector<affine::AffineForOp, 2> nest = {outerLoop, innerLoop};
  if (!affine::isPerfectlyNested(nest))
    return false;

  (void)affine::permuteLoops(nest, {1, 0});
  ARTS_INFO("SDE loop interchange applied: affine k-j order (was j-k)");
  return true;
}

/// Find the two innermost scf.for loops inside the su_iterate body.
/// The su_iterate body contains one scf.for (the j loop), and j's body can
/// contain an optional init section plus one scf.for (the k loop).
static bool findInnerLoopPair(Block &body, scf::ForOp &jLoop, scf::ForOp &kLoop,
                              SmallVectorImpl<Operation *> &initOps) {
  jLoop = nullptr;
  kLoop = nullptr;

  for (Operation &op : body) {
    if (isTerminatorOp(op))
      continue;
    if (auto forOp = dyn_cast<scf::ForOp>(op)) {
      if (jLoop)
        return false; // multiple loops at this level
      jLoop = forOp;
    } else {
      return false; // unexpected non-for op
    }
  }
  if (!jLoop)
    return false;

  // Inside the j loop, find the k loop and any prefix init ops.
  Block *jBody = jLoop.getBody();
  for (Operation &op : *jBody) {
    if (isTerminatorOp(op))
      continue;
    if (auto forOp = dyn_cast<scf::ForOp>(op)) {
      if (kLoop)
        return false; // multiple nested loops
      kLoop = forOp;
    } else {
      // Everything before the k loop is an init op.
      if (!kLoop)
        initOps.push_back(&op);
      else
        return false; // ops after the inner loop
    }
  }
  return kLoop != nullptr;
}

/// Like findInnerLoopPair for affine.for nests (optional init prefix in j).
static bool findInnerAffineLoopPair(Block &body, affine::AffineForOp &jLoop,
                                    affine::AffineForOp &kLoop,
                                    SmallVectorImpl<Operation *> &initOps) {
  jLoop = nullptr;
  kLoop = nullptr;
  initOps.clear();

  for (Operation &op : body) {
    if (isTerminatorOp(op))
      continue;
    if (auto forOp = dyn_cast<affine::AffineForOp>(op)) {
      if (jLoop)
        return false;
      jLoop = forOp;
    } else {
      return false;
    }
  }
  if (!jLoop)
    return false;

  Block *jBody = jLoop.getBody();
  for (Operation &op : *jBody) {
    if (isTerminatorOp(op))
      continue;
    if (auto forOp = dyn_cast<affine::AffineForOp>(op)) {
      if (kLoop)
        return false;
      kLoop = forOp;
    } else {
      if (!kLoop)
        initOps.push_back(&op);
      else
        return false;
    }
  }
  return kLoop != nullptr;
}

static memref::StoreOp getImmediateScalarInitStore(scf::ForOp loop) {
  Operation *prev = loop->getPrevNode();
  auto store = dyn_cast_or_null<memref::StoreOp>(prev);
  if (!store)
    return nullptr;
  auto type = dyn_cast<MemRefType>(store.getMemRefType());
  if (!type || type.getRank() != 0)
    return nullptr;
  return store;
}

static bool usesScalarAccumulator(Operation *op, Value accumulator) {
  if (isa<memref::LoadOp, memref::StoreOp>(op) &&
      llvm::is_contained(op->getOperands(), accumulator))
    return true;

  for (Region &region : op->getRegions()) {
    for (Block &block : region) {
      for (Operation &nested : block) {
        if (usesScalarAccumulator(&nested, accumulator))
          return true;
      }
    }
  }
  return false;
}

static bool hasNestedScalarAccumulatorUse(Operation *op, Value accumulator) {
  for (Region &region : op->getRegions()) {
    for (Block &block : region) {
      for (Operation &nested : block) {
        if (usesScalarAccumulator(&nested, accumulator))
          return true;
      }
    }
  }
  return false;
}

static bool promoteScalarAccumulatorLoop(scf::ForOp loop,
                                         memref::StoreOp initStore) {
  if (loop.getNumResults() != 0)
    return false;

  Value accumulator = initStore.getMemref();
  Type accumulatorType =
      cast<MemRefType>(accumulator.getType()).getElementType();
  if (initStore.getValueToStore().getType() != accumulatorType)
    return false;

  SmallVector<memref::LoadOp> postLoads;
  for (Operation *op = loop->getNextNode(); op;) {
    Operation *next = op->getNextNode();
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (store.getMemref() == accumulator)
        break;
    }
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      if (load.getMemref() == accumulator) {
        postLoads.push_back(load);
        op = next;
        continue;
      }
    }
    if (hasNestedScalarAccumulatorUse(op, accumulator))
      return false;
    op = next;
  }
  if (postLoads.empty())
    return false;

  unsigned directStores = 0;
  bool sawAccumulatorStore = false;
  for (Operation &op : *loop.getBody()) {
    if (isTerminatorOp(op))
      continue;
    if (hasNestedScalarAccumulatorUse(&op, accumulator))
      return false;
    if (auto load = dyn_cast<memref::LoadOp>(&op)) {
      if (load.getMemref() == accumulator && sawAccumulatorStore)
        return false;
    }
    if (auto store = dyn_cast<memref::StoreOp>(&op)) {
      if (store.getMemref() == accumulator) {
        sawAccumulatorStore = true;
        ++directStores;
      }
    }
  }
  if (directStores != 1)
    return false;

  OpBuilder builder(loop);
  scf::ForOp promoted = scf::ForOp::create(
      builder, loop.getLoc(), loop.getLowerBound(), loop.getUpperBound(),
      loop.getStep(), ValueRange{initStore.getValueToStore()});

  Block &oldBody = *loop.getBody();
  Block &newBody = *promoted.getBody();
  Value oldIv = loop.getInductionVar();
  Value newIv = promoted.getInductionVar();
  BlockArgument accumulatorArg = newBody.getArgument(1);

  IRMapping mapping;
  mapping.map(oldIv, newIv);

  Operation *newTerminator = nullptr;
  if (!newBody.empty() && isTerminatorOp(newBody.back()))
    newTerminator = &newBody.back();

  OpBuilder bodyBuilder(promoted.getContext());
  if (newTerminator)
    bodyBuilder.setInsertionPoint(newTerminator);
  else
    bodyBuilder.setInsertionPointToEnd(&newBody);

  Value yieldedValue;
  for (Operation &op : oldBody) {
    if (isTerminatorOp(op))
      continue;
    if (auto load = dyn_cast<memref::LoadOp>(&op)) {
      if (load.getMemref() == accumulator) {
        mapping.map(load.getResult(), accumulatorArg);
        continue;
      }
    }

    if (auto store = dyn_cast<memref::StoreOp>(&op)) {
      if (store.getMemref() == accumulator) {
        yieldedValue = mapping.lookupOrDefault(store.getValueToStore());
        continue;
      }
    }

    Operation *cloned = bodyBuilder.clone(op, mapping);
    for (auto [oldResult, newResult] :
         llvm::zip(op.getResults(), cloned->getResults()))
      mapping.map(oldResult, newResult);
  }

  if (!yieldedValue) {
    promoted.erase();
    return false;
  }

  if (newTerminator)
    newTerminator->erase();
  bodyBuilder.setInsertionPointToEnd(&newBody);
  scf::YieldOp::create(bodyBuilder, loop.getLoc(), yieldedValue);

  Value promotedResult = promoted.getResult(0);
  for (memref::LoadOp load : postLoads) {
    load.getResult().replaceAllUsesWith(promotedResult);
    load.erase();
  }

  initStore.erase();
  loop.erase();
  return true;
}

static void collectScalarAccumulatorLoops(Block &body,
                                          SmallVectorImpl<scf::ForOp> &loops) {
  for (Operation &op : body) {
    if (isTerminatorOp(op))
      continue;
    if (auto loop = dyn_cast<scf::ForOp>(op)) {
      if (getImmediateScalarInitStore(loop))
        loops.push_back(loop);
    }
    for (Region &region : op.getRegions()) {
      for (Block &nestedBody : region)
        collectScalarAccumulatorLoops(nestedBody, loops);
    }
  }
}

static unsigned promoteScalarAccumulators(Block &body) {
  unsigned promoted = 0;
  SmallVector<scf::ForOp> loops;
  collectScalarAccumulatorLoops(body, loops);

  for (scf::ForOp loop : loops) {
    if (!loop || loop->getParentRegion() == nullptr)
      continue;
    memref::StoreOp initStore = getImmediateScalarInitStore(loop);
    if (!initStore)
      continue;
    if (promoteScalarAccumulatorLoop(loop, initStore))
      ++promoted;
  }
  return promoted;
}

static bool isKnownFloatZero(Value value) {
  if (!isa<FloatType>(value.getType()))
    return false;

  if (auto constant = value.getDefiningOp<arith::ConstantOp>()) {
    if (auto floatAttr = dyn_cast<FloatAttr>(constant.getValue()))
      return floatAttr.getValue().isZero();
  }

  if (auto mul = value.getDefiningOp<arith::MulFOp>())
    return isKnownFloatZero(mul.getLhs()) || isKnownFloatZero(mul.getRhs());

  return false;
}

static bool
findPromotedScalarMatmulNest(Block &body, scf::ForOp &jLoop, scf::ForOp &kLoop,
                             SmallVectorImpl<Operation *> &prefixOps,
                             SmallVectorImpl<Operation *> &postOps) {
  for (Operation &op : body) {
    if (isTerminatorOp(op))
      continue;

    if (auto forOp = dyn_cast<scf::ForOp>(op)) {
      if (jLoop)
        return false;
      jLoop = forOp;
      continue;
    }

    // Dead rank-0 alloca state may remain after scalar promotion. It is
    // cleaned up later, so do not let it block the matrix loop rewrite.
    if (isa<memref::AllocaOp>(op))
      continue;
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      auto type = dyn_cast<MemRefType>(store.getMemRefType());
      if (type && type.getRank() == 0)
        continue;
    }

    return false;
  }
  if (!jLoop)
    return false;

  bool sawKLoop = false;
  for (Operation &op : *jLoop.getBody()) {
    if (isTerminatorOp(op))
      continue;

    if (auto forOp = dyn_cast<scf::ForOp>(op)) {
      if (sawKLoop)
        return false;
      kLoop = forOp;
      sawKLoop = true;
      continue;
    }

    if (!sawKLoop)
      prefixOps.push_back(&op);
    else
      postOps.push_back(&op);
  }

  return kLoop && kLoop.getNumResults() == 1 && kLoop.getInitArgs().size() == 1;
}

static bool matchAccumulatorContribution(scf::ForOp kLoop, Value &contribution,
                                         Operation *&accumulateOp) {
  Block *body = kLoop.getBody();
  if (body->empty())
    return false;

  auto yield = dyn_cast<scf::YieldOp>(&body->back());
  if (!yield || yield.getResults().size() != 1)
    return false;

  auto add = yield.getResults()[0].getDefiningOp<arith::AddFOp>();
  if (!add)
    return false;

  Value accumulator = kLoop.getRegionIterArgs()[0];
  if (add.getLhs() == accumulator) {
    contribution = add.getRhs();
  } else if (add.getRhs() == accumulator) {
    contribution = add.getLhs();
  } else {
    return false;
  }

  accumulateOp = add.getOperation();
  return true;
}

static bool matchFinalStoreForZeroInitializedAccumulator(
    scf::ForOp kLoop, ArrayRef<Operation *> postOps, memref::StoreOp &store) {
  if (!isKnownFloatZero(kLoop.getInitArgs()[0]) || postOps.empty())
    return false;

  store = dyn_cast<memref::StoreOp>(postOps.back());
  if (!store)
    return false;

  auto type = dyn_cast<MemRefType>(store.getMemRefType());
  if (!type || type.getRank() < 2 || store.getIndices().size() < 2)
    return false;

  Value sum = kLoop.getResult(0);
  Value stored = store.getValueToStore();
  if (stored == sum)
    return true;

  auto add = stored.getDefiningOp<arith::AddFOp>();
  if (!add)
    return false;

  Value extra;
  if (add.getLhs() == sum)
    extra = add.getRhs();
  else if (add.getRhs() == sum)
    extra = add.getLhs();
  else
    return false;

  return isKnownFloatZero(extra);
}

static bool kBodyReadsOutputMemref(scf::ForOp kLoop, Value outputMemref) {
  bool readsOutput = false;
  kLoop.walk([&](memref::LoadOp load) {
    if (load.getMemref() == outputMemref)
      readsOutput = true;
  });
  return readsOutput;
}

static bool hasUnsupportedKBodySideEffects(scf::ForOp kLoop) {
  bool unsupported = false;
  kLoop.walk([&](Operation *op) {
    if (isa<memref::StoreOp>(op))
      unsupported = true;
  });
  return unsupported;
}

/// Rewrite a promoted matmul-style scalar accumulator:
///
///   for j:
///     sum = for k iter_args(acc = 0) { yield acc + A[i,k] * B[k,j] }
///     C[i,j] = sum
///
/// into:
///
///   for j: C[i,j] = 0
///   for k:
///     for j: C[i,j] += A[i,k] * B[k,j]
///
/// This keeps the per-element k accumulation order while making the inner loop
/// stride-1 for the B row and C row, which is the layout CARTS receives for the
/// PolyBench matrix kernels.
static bool interchangePromotedScalarMatmulAccumulator(Block &body) {
  scf::ForOp jLoop;
  scf::ForOp kLoop;
  SmallVector<Operation *> prefixOps;
  SmallVector<Operation *> postOps;
  if (!findPromotedScalarMatmulNest(body, jLoop, kLoop, prefixOps, postOps))
    return false;

  if (jLoop.getNumResults() != 0) {
    ARTS_DEBUG("LoopInterchange: result-bearing matmul outer loop is not "
               "supported");
    return false;
  }

  if (!prefixOps.empty())
    return false;

  Value contribution;
  Operation *accumulateOp = nullptr;
  if (!matchAccumulatorContribution(kLoop, contribution, accumulateOp))
    return false;

  memref::StoreOp finalStore;
  if (!matchFinalStoreForZeroInitializedAccumulator(kLoop, postOps, finalStore))
    return false;

  if (kBodyReadsOutputMemref(kLoop, finalStore.getMemref()) ||
      hasUnsupportedKBodySideEffects(kLoop))
    return false;

  Value oldJ = jLoop.getInductionVar();
  Value oldK = kLoop.getInductionVar();
  Value zero = kLoop.getInitArgs()[0];
  Location loc = jLoop.getLoc();

  OpBuilder builder(jLoop);

  scf::ForOp::create(
      builder, loc, jLoop.getLowerBound(), jLoop.getUpperBound(),
      jLoop.getStep(), ValueRange{},
      [&](OpBuilder &initBuilder, Location initLoc, Value newJ, ValueRange) {
        IRMapping mapping;
        mapping.map(oldJ, newJ);
        SmallVector<Value> mappedIndices;
        for (Value index : finalStore.getIndices())
          mappedIndices.push_back(mapping.lookupOrDefault(index));
        memref::StoreOp::create(initBuilder, initLoc, zero,
                                finalStore.getMemref(), mappedIndices);
        scf::YieldOp::create(initBuilder, initLoc);
      });

  scf::ForOp::create(
      builder, loc, kLoop.getLowerBound(), kLoop.getUpperBound(),
      kLoop.getStep(), ValueRange{},
      [&](OpBuilder &outerBuilder, Location outerLoc, Value newK, ValueRange) {
        scf::ForOp::create(
            outerBuilder, outerLoc, jLoop.getLowerBound(),
            jLoop.getUpperBound(), jLoop.getStep(), ValueRange{},
            [&](OpBuilder &innerBuilder, Location innerLoc, Value newJ,
                ValueRange) {
              IRMapping mapping;
              mapping.map(oldK, newK);
              mapping.map(oldJ, newJ);

              for (Operation &op : *kLoop.getBody()) {
                if (isTerminatorOp(op) || &op == accumulateOp)
                  continue;
                Operation *cloned = innerBuilder.clone(op, mapping);
                for (auto [oldResult, newResult] :
                     llvm::zip(op.getResults(), cloned->getResults()))
                  mapping.map(oldResult, newResult);
              }

              SmallVector<Value> mappedIndices;
              for (Value index : finalStore.getIndices())
                mappedIndices.push_back(mapping.lookupOrDefault(index));

              Value oldValue =
                  memref::LoadOp::create(innerBuilder, innerLoc,
                                         finalStore.getMemref(), mappedIndices);
              Value mappedContribution = mapping.lookupOrDefault(contribution);
              Value updated = arith::AddFOp::create(
                  innerBuilder, innerLoc, oldValue, mappedContribution);
              memref::StoreOp::create(innerBuilder, innerLoc, updated,
                                      finalStore.getMemref(), mappedIndices);
              scf::YieldOp::create(innerBuilder, innerLoc);
            });
        scf::YieldOp::create(outerBuilder, outerLoc);
      });

  ARTS_INFO("LoopInterchange: scalar matmul accumulator rewritten to k-j "
            "order");
  jLoop.erase();
  return true;
}

/// Collect prefix ops from initOps that are used by kLoop's body.
/// These must be rematerialized into the rebuilt loop nest.
static bool
collectRematerializablePrefixOps(ArrayRef<Operation *> initOps,
                                 Operation *kLoop,
                                 SmallVectorImpl<Operation *> &out) {
  DenseSet<Operation *> prefixSet(initOps.begin(), initOps.end());
  DenseSet<Operation *> needed;
  bool supported = true;

  std::function<void(Value)> visitValue = [&](Value value) {
    if (!supported)
      return;
    Operation *defOp = value.getDefiningOp();
    if (!defOp || !prefixSet.contains(defOp))
      return;
    if (!needed.insert(defOp).second)
      return;
    if (!isMemoryEffectFree(defOp)) {
      supported = false;
      return;
    }
    for (Value operand : defOp->getOperands())
      visitValue(operand);
  };

  kLoop->walk([&](Operation *op) {
    for (Value operand : op->getOperands())
      visitValue(operand);
  });

  if (!supported)
    return false;

  for (Operation *op : initOps) {
    if (needed.contains(op))
      out.push_back(op);
  }
  return true;
}

static bool
collectRematerializablePrefixOps(ArrayRef<Operation *> initOps,
                                 scf::ForOp kLoop,
                                 SmallVectorImpl<Operation *> &out) {
  return collectRematerializablePrefixOps(initOps, kLoop.getOperation(), out);
}

static bool
collectRematerializablePrefixOps(ArrayRef<Operation *> initOps,
                                 affine::AffineForOp kLoop,
                                 SmallVectorImpl<Operation *> &out) {
  return collectRematerializablePrefixOps(initOps, kLoop.getOperation(), out);
}

static Value materializeAffineBound(OpBuilder &builder, Location loc,
                                    AffineMap map, ValueRange operands,
                                    bool isUpper) {
  std::optional<SmallVector<Value, 8>> expanded =
      affine::expandAffineMap(builder, loc, map, operands);
  if (!expanded || expanded->empty())
    return Value();

  Value bound = (*expanded)[0];
  for (Value candidate : llvm::drop_begin(*expanded, 1)) {
    if (isUpper)
      bound = arith::MinSIOp::create(builder, loc, bound, candidate);
    else
      bound = arith::MaxSIOp::create(builder, loc, bound, candidate);
  }
  return bound;
}

static Value materializeAffineLoopStep(OpBuilder &builder,
                                       affine::AffineForOp loop) {
  return arith::ConstantIndexOp::create(builder, loop.getLoc(),
                                        loop.getStepAsInt());
}

/// Apply the j-k to k-j interchange with init loop distribution.
///
/// BEFORE:
///   su_iterate(i) {
///     for j: [init ops] for k: { body }
///   }
///
/// AFTER:
///   su_iterate(i) {
///     for j: { init ops }        (only if init has side effects)
///     for k: for j: { body }     (interchanged)
///   }
static bool interchangeLoops(scf::ForOp jLoop, scf::ForOp kLoop,
                             ArrayRef<Operation *> initOps) {
  if (kLoop->getParentOp() != jLoop.getOperation()) {
    ARTS_DEBUG("LoopInterchange: k loop is not directly inside j loop");
    return false;
  }

  if (jLoop.getNumResults() != 0 || kLoop.getNumResults() != 0) {
    ARTS_DEBUG("LoopInterchange: loop-carried values are not supported");
    return false;
  }

  SmallVector<Operation *, 8> rematerializedPrefixOps;
  if (!collectRematerializablePrefixOps(initOps, kLoop,
                                        rematerializedPrefixOps)) {
    ARTS_DEBUG("LoopInterchange: reduction body captures side-effecting "
               "prefix ops");
    return false;
  }

  OpBuilder builder(jLoop);

  Value lb1 = jLoop.getLowerBound();
  Value ub1 = jLoop.getUpperBound();
  Value step1 = jLoop.getStep();
  Value iv1 = jLoop.getInductionVar();

  Value lb2 = kLoop.getLowerBound();
  Value ub2 = kLoop.getUpperBound();
  Value step2 = kLoop.getStep();
  Value iv2 = kLoop.getInductionVar();

  bool needsInitLoop = llvm::any_of(
      initOps, [](Operation *op) { return !isMemoryEffectFree(op); });

  if (needsInitLoop) {
    scf::ForOp::create(builder, jLoop.getLoc(), lb1, ub1, step1, ValueRange{},
                       [&](OpBuilder &nestedBuilder, Location loc,
                           Value initLoopIV, ValueRange) {
                         IRMapping mapping;
                         mapping.map(iv1, initLoopIV);
                         for (Operation *op : initOps)
                           nestedBuilder.clone(*op, mapping);
                         scf::YieldOp::create(nestedBuilder, loc);
                       });
  }

  scf::ForOp::create(
      builder, jLoop.getLoc(), lb2, ub2, step2, ValueRange{},
      [&](OpBuilder &outerBuilder, Location loc, Value newOuterIV, ValueRange) {
        scf::ForOp::create(
            outerBuilder, loc, lb1, ub1, step1, ValueRange{},
            [&](OpBuilder &innerBuilder, Location innerLoc, Value newInnerIV,
                ValueRange) {
              IRMapping mapping;
              mapping.map(iv1, newInnerIV); // j IV -> new inner IV
              mapping.map(iv2, newOuterIV); // k IV -> new outer IV

              for (Operation *op : rematerializedPrefixOps)
                innerBuilder.clone(*op, mapping);

              for (Operation &op : *kLoop.getBody()) {
                if (isTerminatorOp(op))
                  continue;
                innerBuilder.clone(op, mapping);
              }

              scf::YieldOp::create(innerBuilder, innerLoc);
            });
        scf::YieldOp::create(outerBuilder, loc);
      });

  ARTS_INFO("SDE loop interchange applied: k-j order (was j-k)");

  jLoop->erase();
  return true;
}

static bool indicesContain(ValueRange indices, Value value) {
  return llvm::is_contained(indices, value);
}

static bool isDirectMemoryMatmulAccumulator(scf::ForOp jLoop, scf::ForOp kLoop,
                                            ArrayRef<Operation *> initOps) {
  if (initOps.empty())
    return false;

  memref::StoreOp initStore;
  for (Operation *op : llvm::reverse(initOps)) {
    initStore = dyn_cast<memref::StoreOp>(op);
    if (initStore)
      break;
  }
  if (!initStore)
    return false;

  auto outputType = dyn_cast<MemRefType>(initStore.getMemRefType());
  if (!outputType || outputType.getRank() < 2)
    return false;

  Value oldJ = jLoop.getInductionVar();
  Value oldK = kLoop.getInductionVar();
  Value output = initStore.getMemref();
  ValueRange outputIndices = initStore.getIndices();
  if (!indicesContain(outputIndices, oldJ) ||
      indicesContain(outputIndices, oldK))
    return false;

  unsigned outputLoads = 0;
  unsigned outputStores = 0;
  bool hasKJInputLoad = false;
  bool hasAccumulatingStore = false;

  for (Operation &op : *kLoop.getBody()) {
    if (isTerminatorOp(op))
      continue;

    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      if (load.getMemref() == output) {
        if (!::mlir::carts::ValueAnalysis::areValueRangesIdentical(
                load.getIndices(), outputIndices))
          return false;
        ++outputLoads;
      } else if (indicesContain(load.getIndices(), oldK) &&
                 indicesContain(load.getIndices(), oldJ)) {
        hasKJInputLoad = true;
      }
      continue;
    }

    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (store.getMemref() != output)
        return false;
      if (!::mlir::carts::ValueAnalysis::areValueRangesIdentical(
              store.getIndices(), outputIndices))
        return false;
      ++outputStores;

      if (auto add = store.getValueToStore().getDefiningOp<arith::AddFOp>()) {
        if (auto load = add.getLhs().getDefiningOp<memref::LoadOp>();
            load && load.getMemref() == output &&
            ::mlir::carts::ValueAnalysis::areValueRangesIdentical(
                load.getIndices(), outputIndices))
          hasAccumulatingStore = true;
        if (auto load = add.getRhs().getDefiningOp<memref::LoadOp>();
            load && load.getMemref() == output &&
            ::mlir::carts::ValueAnalysis::areValueRangesIdentical(
                load.getIndices(), outputIndices))
          hasAccumulatingStore = true;
      }
      continue;
    }
  }

  return outputLoads == 1 && outputStores == 1 && hasKJInputLoad &&
         hasAccumulatingStore;
}

static bool isDirectMemoryMatmulAccumulator(affine::AffineForOp jLoop,
                                            affine::AffineForOp kLoop,
                                            ArrayRef<Operation *> initOps) {
  if (initOps.empty())
    return false;

  memref::StoreOp initStore;
  for (Operation *op : llvm::reverse(initOps)) {
    initStore = dyn_cast<memref::StoreOp>(op);
    if (initStore)
      break;
  }
  if (!initStore)
    return false;

  auto outputType = dyn_cast<MemRefType>(initStore.getMemRefType());
  if (!outputType || outputType.getRank() < 2)
    return false;

  Value oldJ = jLoop.getInductionVar();
  Value oldK = kLoop.getInductionVar();
  Value output = initStore.getMemref();
  ValueRange outputIndices = initStore.getIndices();
  if (!indicesContain(outputIndices, oldJ) ||
      indicesContain(outputIndices, oldK))
    return false;

  unsigned outputLoads = 0;
  unsigned outputStores = 0;
  bool hasKJInputLoad = false;
  bool hasAccumulatingStore = false;

  for (Operation &op : *kLoop.getBody()) {
    if (isTerminatorOp(op))
      continue;

    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      if (load.getMemref() == output) {
        if (!::mlir::carts::ValueAnalysis::areValueRangesIdentical(
                load.getIndices(), outputIndices))
          return false;
        ++outputLoads;
      } else if (indicesContain(load.getIndices(), oldK) &&
                 indicesContain(load.getIndices(), oldJ)) {
        hasKJInputLoad = true;
      }
      continue;
    }

    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (store.getMemref() != output)
        return false;
      if (!::mlir::carts::ValueAnalysis::areValueRangesIdentical(
              store.getIndices(), outputIndices))
        return false;
      ++outputStores;

      if (auto add = store.getValueToStore().getDefiningOp<arith::AddFOp>()) {
        if (auto load = add.getLhs().getDefiningOp<memref::LoadOp>();
            load && load.getMemref() == output &&
            ::mlir::carts::ValueAnalysis::areValueRangesIdentical(
                load.getIndices(), outputIndices))
          hasAccumulatingStore = true;
        if (auto load = add.getRhs().getDefiningOp<memref::LoadOp>();
            load && load.getMemref() == output &&
            ::mlir::carts::ValueAnalysis::areValueRangesIdentical(
                load.getIndices(), outputIndices))
          hasAccumulatingStore = true;
      }
      continue;
    }
  }

  return outputLoads == 1 && outputStores == 1 && hasKJInputLoad &&
         hasAccumulatingStore;
}

/// Rebuild a k-j matmul nest as scf loops from an affine j-k source. S4d
/// re-raise recovers affine form after interchange/tiling.
static bool interchangeAffineMatmulLoops(affine::AffineForOp jLoop,
                                         affine::AffineForOp kLoop,
                                         ArrayRef<Operation *> initOps) {
  if (kLoop->getParentOp() != jLoop.getOperation())
    return false;
  if (jLoop.getNumResults() != 0 || kLoop.getNumResults() != 0)
    return false;

  SmallVector<Operation *, 8> rematerializedPrefixOps;
  if (!collectRematerializablePrefixOps(initOps, kLoop,
                                        rematerializedPrefixOps))
    return false;

  OpBuilder builder(jLoop);
  Location loc = jLoop.getLoc();

  Value lb1 = materializeAffineBound(builder, loc, jLoop.getLowerBoundMap(),
                                     jLoop.getLowerBoundOperands(), false);
  Value ub1 = materializeAffineBound(builder, loc, jLoop.getUpperBoundMap(),
                                     jLoop.getUpperBoundOperands(), true);
  Value step1 = materializeAffineLoopStep(builder, jLoop);
  Value iv1 = jLoop.getInductionVar();

  Value lb2 = materializeAffineBound(builder, loc, kLoop.getLowerBoundMap(),
                                     kLoop.getLowerBoundOperands(), false);
  Value ub2 = materializeAffineBound(builder, loc, kLoop.getUpperBoundMap(),
                                     kLoop.getUpperBoundOperands(), true);
  Value step2 = materializeAffineLoopStep(builder, kLoop);
  Value iv2 = kLoop.getInductionVar();
  if (!lb1 || !ub1 || !lb2 || !ub2)
    return false;

  bool needsInitLoop = llvm::any_of(
      initOps, [](Operation *op) { return !isMemoryEffectFree(op); });

  if (needsInitLoop) {
    scf::ForOp::create(builder, jLoop.getLoc(), lb1, ub1, step1, ValueRange{},
                       [&](OpBuilder &nestedBuilder, Location loc,
                           Value initLoopIV, ValueRange) {
                         IRMapping mapping;
                         mapping.map(iv1, initLoopIV);
                         for (Operation *op : initOps)
                           nestedBuilder.clone(*op, mapping);
                         scf::YieldOp::create(nestedBuilder, loc);
                       });
  }

  scf::ForOp::create(
      builder, jLoop.getLoc(), lb2, ub2, step2, ValueRange{},
      [&](OpBuilder &outerBuilder, Location loc, Value newOuterIV, ValueRange) {
        scf::ForOp::create(outerBuilder, loc, lb1, ub1, step1, ValueRange{},
                           [&](OpBuilder &innerBuilder, Location innerLoc,
                               Value newInnerIV, ValueRange) {
                             IRMapping mapping;
                             mapping.map(iv1, newInnerIV);
                             mapping.map(iv2, newOuterIV);

                             for (Operation *op : rematerializedPrefixOps)
                               innerBuilder.clone(*op, mapping);

                             for (Operation &op : *kLoop.getBody()) {
                               if (isTerminatorOp(op))
                                 continue;
                               innerBuilder.clone(op, mapping);
                             }

                             scf::YieldOp::create(innerBuilder, innerLoc);
                           });
        scf::YieldOp::create(outerBuilder, loc);
      });

  ARTS_INFO(
      "SDE loop interchange applied: affine source, scf k-j order (was j-k)");

  jLoop->erase();
  return true;
}

static bool interchangeDirectMemoryMatmulAccumulator(Block &body) {
  affine::AffineForOp ajLoop;
  affine::AffineForOp akLoop;
  SmallVector<Operation *, 4> affineInitOps;
  if (findInnerAffineLoopPair(body, ajLoop, akLoop, affineInitOps) &&
      isDirectMemoryMatmulAccumulator(ajLoop, akLoop, affineInitOps))
    return interchangeAffineMatmulLoops(ajLoop, akLoop, affineInitOps);

  scf::ForOp jLoop;
  scf::ForOp kLoop;
  SmallVector<Operation *> initOps;
  if (!findInnerLoopPair(body, jLoop, kLoop, initOps))
    return false;

  if (!isDirectMemoryMatmulAccumulator(jLoop, kLoop, initOps))
    return false;

  return interchangeLoops(jLoop, kLoop, initOps);
}

static bool isSameValue(Value lhs, Value rhs) {
  return ::mlir::carts::ValueAnalysis::sameValue(lhs, rhs);
}

static bool isSameIndexPair(ValueRange indices, Value first, Value second) {
  return indices.size() == 2 && isSameValue(indices[0], first) &&
         isSameValue(indices[1], second);
}

struct CommittedWriteFact {
  int64_t arrayId = -1;
  Attribute attr;
  sde::LayoutGraphFact fact;
};

static std::optional<CommittedWriteFact>
findCommittedWriteLayoutFact(sde::SdeSuIterateOp op, Value root) {
  if (!op || op.getBody().empty() || !root)
    return std::nullopt;

  Value canonicalRoot = ValueAnalysis::stripMemrefViewOps(root);
  std::optional<int64_t> arrayId;
  for (sde::SdeArrayLayoutRootOp layoutRoot :
       op.getBody().front().getOps<sde::SdeArrayLayoutRootOp>()) {
    if (layoutRoot.getMode() != sde::SdeAccessMode::write ||
        !ValueAnalysis::sameMemrefRoot(layoutRoot.getRoot(), canonicalRoot))
      continue;
    arrayId = static_cast<int64_t>(layoutRoot.getArrayId());
    break;
  }
  if (!arrayId)
    return std::nullopt;

  if (ArrayAttr layout = op.getArrayLayoutAttr()) {
    for (Attribute attr : layout) {
      auto dict = dyn_cast<DictionaryAttr>(attr);
      if (!dict)
        continue;
      std::optional<sde::LayoutGraphFact> fact =
          sde::parseArrayLayoutFact(dict);
      if (!fact || fact->id != *arrayId ||
          fact->role != sde::LayoutGraphRole::write ||
          fact->ownerDims.empty() || fact->blockShape.empty())
        continue;
      return CommittedWriteFact{*arrayId, attr, *fact};
    }
  }
  return std::nullopt;
}

static sde::SdeSuIterateOp
createSymmetricMirrorLoop(sde::SdeSuIterateOp source, Value output,
                          Value diagonalValue, Value sourceRowIv,
                          Value sourceColIv, scf::ForOp reductionLoop) {
  OpBuilder builder(source);
  builder.setInsertionPointAfter(source);
  Location loc = source.getLoc();
  MLIRContext *ctx = source.getContext();
  auto barrier = sde::SdeSuBarrierOp::create(
      builder, loc, ValueRange{},
      sde::SdeBarrierReasonAttr::get(ctx,
                                     sde::SdeBarrierReason::required_memory));
  builder.setInsertionPointAfter(barrier);
  Value lowerBound = source.getLowerBounds().front();
  Value upperBound = source.getUpperBounds().front();
  Value step = source.getSteps().front();

  sde::SuIterateAttrs suAttrs;
  suAttrs.nowait = source.getNowaitAttr();
  suAttrs.structuredClassification = sde::SdeStructuredClassificationAttr::get(
      ctx, sde::SdeStructuredClassification::elementwise);
  auto mirror =
      sde::buildSuIterate(builder, loc, ValueRange{lowerBound},
                          ValueRange{upperBound}, ValueRange{step}, suAttrs);

  Block &mirrorBody = sde::ensureBlock(mirror.getBody());
  while (mirrorBody.getNumArguments() < 1)
    mirrorBody.addArgument(builder.getIndexType(), loc);

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(&mirrorBody);
  auto cuRegion = sde::buildCuRegion(
      builder, loc, sde::SdeCuKindAttr::get(ctx, sde::SdeCuKind::parallel));
  Block &compute = sde::ensureBlock(cuRegion.getBody());
  builder.setInsertionPointToStart(&compute);

  Value row = mirrorBody.getArgument(0);
  memref::StoreOp::create(builder, loc, diagonalValue, output,
                          ValueRange{row, row});
  scf::ForOp::create(
      builder, loc, lowerBound, row, step, ValueRange{},
      [&](OpBuilder &colBuilder, Location colLoc, Value col, ValueRange) {
        IRMapping mapping;
        mapping.map(sourceRowIv, col);
        mapping.map(sourceColIv, row);
        auto clonedReduction = cast<scf::ForOp>(
            colBuilder.clone(*reductionLoop.getOperation(), mapping));
        memref::StoreOp::create(colBuilder, colLoc,
                                clonedReduction.getResult(0), output,
                                ValueRange{row, col});
        scf::YieldOp::create(colBuilder, colLoc);
      });
  sde::SdeYieldOp::create(builder, loc, ValueRange{});

  builder.setInsertionPointAfter(cuRegion);
  sde::SdeYieldOp::create(builder, loc, ValueRange{});
  return mirror;
}

static bool isReadLayoutFact(Attribute attr) {
  auto dict = dyn_cast<DictionaryAttr>(attr);
  if (!dict)
    return false;
  std::optional<sde::LayoutGraphFact> fact = sde::parseArrayLayoutFact(dict);
  return fact && fact->role == sde::LayoutGraphRole::read;
}

static void attachMirrorFacts(sde::SdeSuIterateOp source,
                              sde::SdeSuIterateOp mirror, Value output,
                              const CommittedWriteFact &writeFact) {
  MLIRContext *ctx = mirror.getContext();
  SmallVector<Attribute, 4> layoutFacts;
  if (ArrayAttr sourceLayout = source.getArrayLayoutAttr())
    for (Attribute attr : sourceLayout)
      if (isReadLayoutFact(attr))
        layoutFacts.push_back(attr);
  layoutFacts.push_back(writeFact.attr);
  mirror.setArrayLayoutAttr(ArrayAttr::get(ctx, layoutFacts));

  Block &body = mirror.getBody().front();
  OpBuilder builder(&body, body.begin());
  if (!source.getBody().empty()) {
    for (sde::SdeArrayLayoutRootOp root :
         source.getBody().front().getOps<sde::SdeArrayLayoutRootOp>()) {
      if (root.getMode() != sde::SdeAccessMode::read)
        continue;
      sde::SdeArrayLayoutRootOp::create(
          builder, mirror.getLoc(), root.getRoot(),
          sde::SdeAccessModeAttr::get(ctx, sde::SdeAccessMode::read),
          IntegerAttr::get(IntegerType::get(ctx, 64), root.getArrayId()));
    }
  }
  sde::SdeArrayLayoutRootOp::create(
      builder, mirror.getLoc(), output,
      sde::SdeAccessModeAttr::get(ctx, sde::SdeAccessMode::write),
      IntegerAttr::get(IntegerType::get(ctx, 64), writeFact.arrayId));
  sde::commitWriterPhysicalLayoutFacts(mirror, writeFact.fact.ownerDims,
                                       writeFact.fact.blockShape,
                                       writeFact.fact.blockShape);
}

static bool splitSymmetricSelfGramStores(sde::SdeSuIterateOp op, Block &body) {
  if (op.getLowerBounds().size() != 1 || op.getUpperBounds().size() != 1 ||
      op.getSteps().size() != 1 || op.getBody().front().getNumArguments() < 1)
    return false;

  Value ownerIv = op.getBody().front().getArgument(0);
  scf::ForOp pairLoop;
  memref::StoreOp diagonalStore;
  for (Operation &nested : body.without_terminator()) {
    if (auto store = dyn_cast<memref::StoreOp>(nested)) {
      if (isSameIndexPair(store.getIndices(), ownerIv, ownerIv))
        diagonalStore = store;
      continue;
    }
    if (auto loop = dyn_cast<scf::ForOp>(nested)) {
      if (pairLoop)
        return false;
      pairLoop = loop;
    }
  }
  if (!pairLoop || !diagonalStore)
    return false;

  Value pairIv = pairLoop.getInductionVar();
  memref::StoreOp upperStore;
  memref::StoreOp lowerStore;
  scf::ForOp reductionLoop;
  for (Operation &nested : pairLoop.getBody()->without_terminator()) {
    if (auto loop = dyn_cast<scf::ForOp>(nested)) {
      if (reductionLoop)
        return false;
      reductionLoop = loop;
      continue;
    }
    auto store = dyn_cast<memref::StoreOp>(nested);
    if (!store)
      return false;
    if (isSameIndexPair(store.getIndices(), ownerIv, pairIv)) {
      upperStore = store;
      continue;
    }
    if (isSameIndexPair(store.getIndices(), pairIv, ownerIv)) {
      lowerStore = store;
      continue;
    }
    return false;
  }

  if (!reductionLoop || reductionLoop.getNumResults() != 1 || !upperStore ||
      !lowerStore || upperStore.getMemref() != lowerStore.getMemref() ||
      upperStore.getMemref() != diagonalStore.getMemref() ||
      upperStore.getValueToStore() != reductionLoop.getResult(0) ||
      lowerStore.getValueToStore() != reductionLoop.getResult(0))
    return false;

  auto outputType = dyn_cast<MemRefType>(upperStore.getMemRefType());
  if (!outputType || outputType.getRank() != 2 || !outputType.hasStaticShape())
    return false;

  Value output = upperStore.getMemref();
  Value diagonalValue = diagonalStore.getValueToStore();
  std::optional<CommittedWriteFact> writeFact =
      findCommittedWriteLayoutFact(op, output);
  if (!writeFact)
    return false;

  lowerStore.erase();
  diagonalStore.erase();
  sde::SdeSuIterateOp mirror = createSymmetricMirrorLoop(
      op, output, diagonalValue, ownerIv, pairIv, reductionLoop);
  attachMirrorFacts(op, mirror, output, *writeFact);
  ARTS_INFO("LoopInterchange: split symmetric self-Gram lower-triangle store");
  return true;
}

struct LoopInterchangePass
    : public sde::impl::LoopInterchangeBase<LoopInterchangePass> {

  /// Try stencil halo-based interchange for stencil classification.
  /// Reads accessMinOffsets/accessMaxOffsets, computes per-dim halo width,
  /// and reorders inner loops so smallest-halo-width dim is outermost.
  /// Only applies to 3D+ stencils with 2+ inner affine.for or scf.for loops.
  bool tryStencilHaloInterchange(sde::SdeSuIterateOp op, Block &body) {
    ArrayAttr minArr = op.getAccessMinOffsetsAttr();
    ArrayAttr maxArr = op.getAccessMaxOffsetsAttr();
    if (!minArr || !maxArr || minArr.size() < 2)
      return false;
    SmallVector<int64_t> haloWidths;
    for (unsigned d = 0; d < minArr.size(); ++d) {
      int64_t lo = cast<IntegerAttr>(minArr[d]).getInt();
      int64_t hi = cast<IntegerAttr>(maxArr[d]).getInt();
      haloWidths.push_back(hi - lo);
    }

    unsigned dim1 = 1;
    unsigned dim2 = 2;
    if (dim2 >= haloWidths.size())
      return false;
    if (haloWidths[dim1] <= haloWidths[dim2])
      return false; // already optimal or equal

    SmallVector<affine::AffineForOp, 4> affineInnerLoops;
    if (collectInnerAffineForChain(body, affineInnerLoops) &&
        affineInnerLoops.size() >= 2) {
      if (interchangeAffineLoopPair(affineInnerLoops[0], affineInnerLoops[1]))
        return true;
    }

    SmallVector<scf::ForOp, 4> innerLoops;
    if (!collectInnerForChain(body, innerLoops) || innerLoops.size() < 2)
      return false;

    scf::ForOp outerLoop = innerLoops[0];
    scf::ForOp innerLoop = innerLoops[1];

    if (innerLoop->getParentOp() != outerLoop.getOperation()) {
      ARTS_DEBUG("LoopInterchange: stencil inner loop not directly nested");
      return false;
    }

    // Collect any prefix ops in the outer loop body before the inner loop
    SmallVector<Operation *> prefixOps;
    for (Operation &op : *outerLoop.getBody()) {
      if (isTerminatorOp(op))
        continue;
      if (&op == innerLoop.getOperation())
        break;
      prefixOps.push_back(&op);
    }

    return interchangeLoops(outerLoop, innerLoop, prefixOps);
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    ARTS_INFO_HEADER(LoopInterchangePass);

    int interchangeCount = 0;

    SmallVector<sde::SdeSuIterateOp> iterateOps;
    module.walk([&](sde::SdeSuIterateOp op) { iterateOps.push_back(op); });

    for (sde::SdeSuIterateOp op : iterateOps) {
      if (!op || op->getParentRegion() == nullptr)
        continue;
      if (hasCommittedInterchangeGrainFacts(op))
        continue;

      Block *body = sde::getSuIterateComputeBlock(op);
      if (!body)
        continue;

      interchangeCount += promoteScalarAccumulators(*body);
      if (interchangePromotedScalarMatmulAccumulator(*body))
        ++interchangeCount;
      else if (interchangeDirectMemoryMatmulAccumulator(*body))
        ++interchangeCount;

      if (splitSymmetricSelfGramStores(op, *body))
        ++interchangeCount;

      auto classAttr = op.getStructuredClassificationAttr();
      if (!classAttr)
        continue;

      auto classification = *op.getStructuredClassification();

      // Stencil halo ordering.
      if (classification == sde::SdeStructuredClassification::stencil) {
        if (tryStencilHaloInterchange(op, *body))
          ++interchangeCount;
      }
    }

    ARTS_DEBUG("LoopInterchangePass: interchanged " << interchangeCount
                                                    << " loop nests");
  }
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass> createLoopInterchangePass() {
  return std::make_unique<LoopInterchangePass>();
}

} // namespace mlir::carts::sde
