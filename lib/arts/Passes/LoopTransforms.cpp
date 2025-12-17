///==========================================================================///
/// File: LoopTransforms.cpp
///
/// Additional loop transformations that complement LoopReordering.
///
/// Primary target (PolyBench-like GEMM/2MM/3MM):
///   Detect dot-product style matmul kernels inside `arts.for`:
///     for j:
///       sum = 0
///       for k: sum += A[i,k] * B[k,j]
///       C[i,j] = alpha*sum + beta*C[i,j]
///
///   Rewrite to a reduction-aware k-j update form that exposes stride-1 B and
///   C:
///     for j: C[i,j] = beta*C[i,j]
///     for k:
///       a = alpha*A[i,k]
///       for j: C[i,j] += a * B[k,j]
///
///   Optionally tile the inner j loop(s) to improve cache locality/SIMD.
///
/// This pass is designed to run BEFORE CreateDbs so that DB creation and
/// partitioning can "see" the transformed loop structure.
///==========================================================================///

#include "ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/Matchers.h"
#include "mlir/IR/PatternMatch.h"

ARTS_DEBUG_SETUP(loop_transforms);

using namespace mlir;
using namespace mlir::arts;

namespace {

static Attribute sanitizeLoopMetadataNoReductions(Attribute attr,
                                                  MLIRContext *ctx) {
  if (!attr || !ctx)
    return attr;
  auto loopAttr = dyn_cast<LoopMetadataAttr>(attr);
  if (!loopAttr)
    return attr;

  /// Some transforms (e.g. matmul dot-product -> k-j update) remove reduction
  /// structure. If we blindly copy old loop metadata, "hasReductions" may
  /// become stale and mislead downstream heuristics/debugging.
  if (!loopAttr.getHasReductions() || !loopAttr.getHasReductions().getValue())
    return attr;

  return LoopMetadataAttr::get(
      ctx,
      /*potentiallyParallel=*/loopAttr.getPotentiallyParallel(),
      /*hasReductions=*/BoolAttr::get(ctx, false),
      /*reductionKinds=*/ArrayAttr(),
      /*readCount=*/loopAttr.getReadCount(),
      /*writeCount=*/loopAttr.getWriteCount(),
      /*tripCount=*/loopAttr.getTripCount(),
      /*nestingLevel=*/loopAttr.getNestingLevel(),
      /*hasUniformStride=*/loopAttr.getHasUniformStride(),
      /*hasGatherScatter=*/loopAttr.getHasGatherScatter(),
      /*dataMovementPattern=*/loopAttr.getDataMovementPattern(),
      /*suggestedPartitioning=*/loopAttr.getSuggestedPartitioning(),
      /*hasInterIterationDeps=*/loopAttr.getHasInterIterationDeps(),
      /*memrefsWithLoopCarriedDeps=*/loopAttr.getMemrefsWithLoopCarriedDeps(),
      /*parallelClassification=*/loopAttr.getParallelClassification(),
      /*locationKey=*/loopAttr.getLocationKey());
}

static Value stripCasts(Value v) {
  while (v) {
    if (auto ext = v.getDefiningOp<arith::ExtFOp>())
      v = ext.getIn();
    else if (auto trunc = v.getDefiningOp<arith::TruncFOp>())
      v = trunc.getIn();
    else
      break;
  }
  return v;
}

static std::optional<double> getConstFloat(Value v) {
  if (!v)
    return std::nullopt;
  if (auto cst = v.getDefiningOp<arith::ConstantOp>()) {
    if (auto fa = dyn_cast<FloatAttr>(cst.getValue()))
      return fa.getValueAsDouble();
  }
  return std::nullopt;
}

static Value getOrCreateFloatConstant(OpBuilder &b, Location loc, Type ty,
                                      double value) {
  if (ty.isF32())
    return b.create<arith::ConstantFloatOp>(loc, APFloat((float)value),
                                            b.getF32Type());
  if (ty.isF64())
    return b.create<arith::ConstantFloatOp>(loc, APFloat((double)value),
                                            b.getF64Type());
  return Value();
}

static bool matchMulFOp(Value v, Value &lhs, Value &rhs) {
  if (!v)
    return false;
  if (auto mul = v.getDefiningOp<arith::MulFOp>()) {
    lhs = mul.getLhs();
    rhs = mul.getRhs();
    return true;
  }
  return false;
}

static bool matchAddFOp(Value v, Value &lhs, Value &rhs) {
  if (!v)
    return false;
  if (auto add = v.getDefiningOp<arith::AddFOp>()) {
    lhs = add.getLhs();
    rhs = add.getRhs();
    return true;
  }
  return false;
}

struct MatmulDotMatch {
  ForOp outerI;
  scf::ForOp jLoop;
  scf::ForOp kLoop;
  Value memA;
  Value memB;
  Value memC;
  bool aIsKI = false; /// A[k,i] vs A[i,k]
  bool bIsKJ = true;  /// B[k,j] vs B[j,k]
  Value sumVal;
  Value iIndex;
  Value alpha;
  Value beta;
  Type elemTy;
};

struct MatmulUpdateMatch {
  ForOp outerI;
  scf::ForOp kLoop; /// outer reduction/update loop
  scf::ForOp jLoop; /// inner update loop
  Value memA;
  Value memB;
  Value memC;
  bool aIsKI = false; /// A[k,i] vs A[i,k]
  bool bIsKJ = true;  /// B[k,j] vs B[j,k]
  Value iIndex;
  Value alpha; /// optional (null => 1.0)
  Type elemTy;
};

/// Extract (alpha, beta) from a store value that combines the dot-product sum
/// with the previous C value.
static bool matchAlphaBeta(Value storeVal, Value sumVal, Value oldCVal,
                           Value &outAlpha, Value &outBeta) {
  /// Defaults
  outAlpha = Value();
  outBeta = Value();

  /// Helper: match sum term either sum or mul(sum, alpha)
  auto matchSumTerm = [&](Value v, Value &alpha) -> bool {
    v = stripCasts(v);
    if (v == sumVal) {
      alpha = Value(); /// implies 1.0
      return true;
    }
    Value a, b;
    if (matchMulFOp(v, a, b)) {
      a = stripCasts(a);
      b = stripCasts(b);
      if (a == sumVal) {
        alpha = b;
        return true;
      }
      if (b == sumVal) {
        alpha = a;
        return true;
      }
    }
    return false;
  };

  /// Helper: match C term either oldC or mul(oldC, beta)
  auto matchCTerm = [&](Value v, Value &beta) -> bool {
    v = stripCasts(v);
    if (v == oldCVal) {
      beta = Value(); /// implies 1.0
      return true;
    }
    Value a, b;
    if (matchMulFOp(v, a, b)) {
      a = stripCasts(a);
      b = stripCasts(b);
      if (a == oldCVal) {
        beta = b;
        return true;
      }
      if (b == oldCVal) {
        beta = a;
        return true;
      }
    }
    return false;
  };

  /// Pattern 1: store = sum (alpha=1, beta=0)
  if (stripCasts(storeVal) == sumVal) {
    outAlpha = Value(); /// implies 1.0
    outBeta = Value();  /// implies 0.0 (created by caller)
    return true;
  }

  /// Pattern 1b: store = mul(sum, alpha) (beta=0)
  {
    Value a, b;
    if (matchMulFOp(storeVal, a, b)) {
      a = stripCasts(a);
      b = stripCasts(b);
      if (a == sumVal) {
        outAlpha = b;
        outBeta = Value();
        return true;
      }
      if (b == sumVal) {
        outAlpha = a;
        outBeta = Value();
        return true;
      }
    }
  }

  /// Pattern 2: store = add(sumTerm, cTerm)
  Value x, y;
  if (!matchAddFOp(storeVal, x, y))
    return false;

  Value alpha, beta;
  bool xIsSum = matchSumTerm(x, alpha);
  bool yIsSum = matchSumTerm(y, alpha);
  bool xIsC = oldCVal ? matchCTerm(x, beta) : false;
  bool yIsC = oldCVal ? matchCTerm(y, beta) : false;

  if ((xIsSum && yIsC) || (yIsSum && xIsC)) {
    outAlpha = alpha;
    outBeta = beta;
    return true;
  }

  return false;
}

static bool matchMatmulDotInArtsFor(ForOp artsFor, MatmulDotMatch &out) {
  /// Find the first scf.for directly inside arts.for: treat as j loop.
  scf::ForOp jLoop = nullptr;
  for (Operation &op : artsFor.getBody()->without_terminator()) {
    if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
      jLoop = forOp;
      break;
    }
  }
  if (!jLoop)
    return false;

  /// Find the first scf.for inside jLoop with iter_args: treat as k reduction.
  scf::ForOp kLoop = nullptr;
  for (Operation &op : jLoop.getBody()->without_terminator()) {
    auto forOp = dyn_cast<scf::ForOp>(&op);
    if (!forOp)
      continue;
    if (forOp.getInitArgs().size() == 1) {
      kLoop = forOp;
      break;
    }
  }
  if (!kLoop)
    return false;

  if (kLoop.getNumResults() != 1)
    return false;

  Value sumVal = kLoop.getResult(0);
  Type elemTy = sumVal.getType();
  if (!elemTy.isF32() && !elemTy.isF64())
    return false;

  /// Find store to C after kLoop in jLoop.
  memref::StoreOp storeC = nullptr;
  for (Operation &op : jLoop.getBody()->without_terminator()) {
    if (&op == kLoop.getOperation())
      continue;
    if (auto st = dyn_cast<memref::StoreOp>(&op)) {
      storeC = st;
      break;
    }
  }
  if (!storeC)
    return false;

  /// Identify the old C load used in the store expression (optional).
  memref::LoadOp loadC = nullptr;
  for (Operation &op : jLoop.getBody()->without_terminator()) {
    if (auto ld = dyn_cast<memref::LoadOp>(&op)) {
      if (ld.getMemref() == storeC.getMemref() &&
          ld.getIndices() == storeC.getIndices()) {
        loadC = ld;
        break;
      }
    }
  }

  /// Find mulf in kLoop body that multiplies loads from A and B.
  memref::LoadOp loadA = nullptr, loadB = nullptr;
  arith::MulFOp mul = nullptr;
  for (Operation &op : kLoop.getBody()->without_terminator()) {
    if (auto m = dyn_cast<arith::MulFOp>(&op)) {
      mul = m;
      break;
    }
  }
  if (!mul)
    return false;

  auto findLoad = [&](Value v) -> memref::LoadOp {
    v = stripCasts(v);
    return v.getDefiningOp<memref::LoadOp>();
  };
  loadA = findLoad(mul.getLhs());
  loadB = findLoad(mul.getRhs());
  if (!loadA || !loadB) {
    /// Try swapped
    loadA = findLoad(mul.getRhs());
    loadB = findLoad(mul.getLhs());
  }
  if (!loadA || !loadB)
    return false;

  /// Verify index roles: A[i,k] and B[k,j], and store C[i,j].
  if (loadA.getIndices().size() != 2 || loadB.getIndices().size() != 2 ||
      storeC.getIndices().size() != 2)
    return false;

  Value jIV = jLoop.getInductionVar();
  Value kIV = kLoop.getInductionVar();

  auto aIdx0 = loadA.getIndices()[0];
  auto aIdx1 = loadA.getIndices()[1];
  auto bIdx0 = loadB.getIndices()[0];
  auto bIdx1 = loadB.getIndices()[1];
  auto cIdx0 = storeC.getIndices()[0];
  auto cIdx1 = storeC.getIndices()[1];

  /// Determine i index: it is the non-k index in A, and must match C's first.
  Value iIndex;
  bool aIsKI = false;
  if (aIdx0 == kIV) {
    iIndex = aIdx1;
    aIsKI = true;
  } else if (aIdx1 == kIV) {
    iIndex = aIdx0;
    aIsKI = false;
  } else
    return false;

  if (cIdx0 != iIndex)
    return false;
  if (cIdx1 != jIV)
    return false;

  /// B must be indexed by (k, j) in some order.
  bool bIsKJ = false;
  bool bOk = false;
  if (bIdx0 == kIV && bIdx1 == jIV) {
    bOk = true;
    bIsKJ = true;
  } else if (bIdx0 == jIV && bIdx1 == kIV) {
    bOk = true;
    bIsKJ = false;
  }
  if (!bOk)
    return false;

  /// Extract alpha/beta from store value.
  Value alpha, beta;
  Value oldCVal = loadC ? loadC.getResult() : Value();
  if (!matchAlphaBeta(storeC.getValueToStore(), sumVal, oldCVal, alpha, beta))
    return false;

  out = MatmulDotMatch{artsFor,
                       jLoop,
                       kLoop,
                       loadA.getMemref(),
                       loadB.getMemref(),
                       storeC.getMemref(),
                       aIsKI,
                       bIsKJ,
                       sumVal,
                       iIndex,
                       alpha,
                       beta,
                       elemTy};
  return true;
}

static bool matchMatmulUpdateInArtsFor(ForOp artsFor, MatmulUpdateMatch &out) {
  /// Match the "already update-form" produced by LoopReordering auto-detect:
  ///   arts.for(i) {
  ///     scf.for(k) {
  ///       scf.for(j) {
  ///         C[i,j] = C[i,j] + (alpha*A[i,k])*B[k,j]
  ///       }
  ///     }
  ///   }
  //
  /// This pattern is common in PolyBench 3mm after LoopReordering performs
  /// distribution + interchange on memory-based reductions.

  /// Find candidate k loops directly under arts.for.
  for (Operation &op : artsFor.getBody()->without_terminator()) {
    auto kLoop = dyn_cast<scf::ForOp>(&op);
    if (!kLoop || !kLoop.getInitArgs().empty())
      continue;

    /// Find the first nested scf.for under kLoop: treat as j loop.
    scf::ForOp jLoop = nullptr;
    for (Operation &kop : kLoop.getBody()->without_terminator()) {
      if (auto jl = dyn_cast<scf::ForOp>(&kop)) {
        jLoop = jl;
        break;
      }
    }
    if (!jLoop || !jLoop.getInitArgs().empty())
      continue;

    /// Conservative: require kLoop body to contain only the jLoop.
    {
      int nonTermOps = 0;
      for (Operation &kop : kLoop.getBody()->without_terminator()) {
        (void)kop;
        nonTermOps++;
      }
      if (nonTermOps != 1)
        continue;
    }

    Value jIV = jLoop.getInductionVar();
    Value kIV = kLoop.getInductionVar();

    /// Find store to C in j loop.
    memref::StoreOp storeC = nullptr;
    for (Operation &jop : jLoop.getBody()->without_terminator()) {
      if (auto st = dyn_cast<memref::StoreOp>(&jop)) {
        storeC = st;
        break;
      }
    }
    if (!storeC)
      continue;

    if (storeC.getIndices().size() != 2)
      continue;

    Value iIndex = storeC.getIndices()[0];
    if (storeC.getIndices()[1] != jIV)
      continue;

    Type elemTy = storeC.getValueToStore().getType();
    if (!elemTy.isF32() && !elemTy.isF64())
      continue;

    /// Match store value: addf(cOld, prod)
    Value addL, addR;
    if (!matchAddFOp(storeC.getValueToStore(), addL, addR))
      continue;

    auto findCLoad = [&](Value v) -> memref::LoadOp {
      v = stripCasts(v);
      auto ld = v.getDefiningOp<memref::LoadOp>();
      if (!ld)
        return nullptr;
      if (ld.getMemref() != storeC.getMemref())
        return nullptr;
      if (ld.getIndices() != storeC.getIndices())
        return nullptr;
      return ld;
    };

    memref::LoadOp loadC = findCLoad(addL);
    Value prodVal = addR;
    if (!loadC) {
      loadC = findCLoad(addR);
      prodVal = addL;
    }
    if (!loadC)
      continue;

    /// Match prod: mulf(x, y) where x/y are loads (optionally scaled by alpha).
    Value mulX, mulY;
    if (!matchMulFOp(prodVal, mulX, mulY))
      continue;

    struct ScaledLoad {
      memref::LoadOp load;
      Value scale; /// optional scalar multiply (null => 1.0)
    };
    auto extractScaledLoad = [&](Value v) -> std::optional<ScaledLoad> {
      v = stripCasts(v);
      if (auto ld = v.getDefiningOp<memref::LoadOp>())
        return ScaledLoad{ld, Value()};

      Value a, b;
      if (matchMulFOp(v, a, b)) {
        a = stripCasts(a);
        b = stripCasts(b);
        if (auto ld = a.getDefiningOp<memref::LoadOp>())
          return ScaledLoad{ld, b};
        if (auto ld = b.getDefiningOp<memref::LoadOp>())
          return ScaledLoad{ld, a};
      }
      return std::nullopt;
    };

    auto sx = extractScaledLoad(mulX);
    auto sy = extractScaledLoad(mulY);
    if (!sx || !sy)
      continue;

    auto classify = [&](memref::LoadOp ld, bool &isA, bool &isB, bool &aIsKI,
                        bool &bIsKJ) {
      isA = false;
      isB = false;
      aIsKI = false;
      bIsKJ = true;
      if (ld.getIndices().size() != 2)
        return;
      Value idx0 = ld.getIndices()[0];
      Value idx1 = ld.getIndices()[1];

      bool usesK = (idx0 == kIV) || (idx1 == kIV);
      bool usesJ = (idx0 == jIV) || (idx1 == jIV);
      bool usesI = (idx0 == iIndex) || (idx1 == iIndex);

      /// A: uses {i,k} but not j.
      if (usesK && usesI && !usesJ) {
        isA = true;
        aIsKI = (idx0 == kIV);
        return;
      }
      /// B: uses {k,j} but not i.
      if (usesK && usesJ && !usesI) {
        isB = true;
        bIsKJ = (idx0 == kIV);
        return;
      }
    };

    bool xIsA = false, xIsB = false, yIsA = false, yIsB = false;
    bool xAIsKI = false, xBIsKJ = true;
    bool yAIsKI = false, yBIsKJ = true;
    classify(sx->load, xIsA, xIsB, xAIsKI, xBIsKJ);
    classify(sy->load, yIsA, yIsB, yAIsKI, yBIsKJ);

    /// We need one A and one B.
    memref::LoadOp loadA = nullptr;
    memref::LoadOp loadB = nullptr;
    bool aIsKI = false;
    bool bIsKJ = true;

    if (xIsA && yIsB) {
      loadA = sx->load;
      loadB = sy->load;
      aIsKI = xAIsKI;
      bIsKJ = yBIsKJ;
    } else if (xIsB && yIsA) {
      loadA = sy->load;
      loadB = sx->load;
      aIsKI = yAIsKI;
      bIsKJ = xBIsKJ;
    } else {
      continue;
    }

    /// Choose alpha from whichever multiplicand is scaled (if any). We allow
    /// scaling on either A or B and always hoist it onto A (commutative).
    Value alpha;
    Value alphaX = sx->scale;
    Value alphaY = sy->scale;
    if (alphaX && alphaY)
      continue;
    if (alphaX)
      alpha = alphaX;
    if (alphaY)
      alpha = alphaY;

    /// Require A load to be inside jLoop so we actually have something to
    /// hoist.
    if (!jLoop->isAncestor(loadA.getOperation()))
      continue;

    out = MatmulUpdateMatch{artsFor,
                            kLoop,
                            jLoop,
                            loadA.getMemref(),
                            loadB.getMemref(),
                            storeC.getMemref(),
                            aIsKI,
                            bIsKJ,
                            iIndex,
                            alpha,
                            elemTy};
    return true;
  }

  return false;
}

static scf::ForOp createTiledForIfBeneficial(
    OpBuilder &b, Location loc, scf::ForOp originalLoop, int64_t tileSize,
    int64_t minTripCount,
    function_ref<void(OpBuilder &, Location, Value)> innerBodyBuilder) {
  if (tileSize <= 1)
    return nullptr;

  OpBuilder::InsertionGuard guard(b);

  int64_t stepC = 0, lbC = 0, ubC = 0;
  bool hasConstStep = getConstantIndex(originalLoop.getStep(), stepC);
  bool hasConstLb = getConstantIndex(originalLoop.getLowerBound(), lbC);
  bool hasConstUb = getConstantIndex(originalLoop.getUpperBound(), ubC);
  if (!hasConstStep || stepC != 1 || !hasConstLb || !hasConstUb)
    return nullptr;

  int64_t trip = ubC - lbC;
  if (trip < minTripCount)
    return nullptr;

  auto tileStepVal = b.create<arith::ConstantIndexOp>(loc, tileSize);
  auto outer = b.create<scf::ForOp>(loc, originalLoop.getLowerBound(),
                                    originalLoop.getUpperBound(), tileStepVal);

  b.setInsertionPointToStart(outer.getBody());
  Value tileBase = outer.getInductionVar();
  auto tileEnd = b.create<arith::AddIOp>(loc, tileBase, tileStepVal);
  auto innerUb =
      b.create<arith::MinUIOp>(loc, tileEnd, originalLoop.getUpperBound());

  auto inner =
      b.create<scf::ForOp>(loc, tileBase, innerUb, originalLoop.getStep());
  b.setInsertionPointToStart(inner.getBody());
  innerBodyBuilder(b, loc, inner.getInductionVar());

  return outer;
}

static bool isTilingApplicable(scf::ForOp loop, int64_t tileSize,
                               int64_t minTripCount) {
  if (!loop || tileSize <= 1)
    return false;
  int64_t stepC = 0, lbC = 0, ubC = 0;
  bool hasConstStep = getConstantIndex(loop.getStep(), stepC);
  bool hasConstLb = getConstantIndex(loop.getLowerBound(), lbC);
  bool hasConstUb = getConstantIndex(loop.getUpperBound(), ubC);
  if (!hasConstStep || stepC != 1 || !hasConstLb || !hasConstUb)
    return false;
  int64_t trip = ubC - lbC;
  return trip >= minTripCount;
}

static void rewriteMatmulDotToKJUpdate(const MatmulDotMatch &m, int64_t tileJ,
                                       int64_t minTripCount) {
  scf::ForOp jLoop = m.jLoop;
  scf::ForOp kLoop = m.kLoop;

  OpBuilder b(jLoop);
  Location loc = jLoop.getLoc();

  Value alpha = m.alpha;
  Value beta = m.beta;
  Value iIndex = m.iIndex;

  Value one = getOrCreateFloatConstant(b, loc, m.elemTy, 1.0);
  Value zero = getOrCreateFloatConstant(b, loc, m.elemTy, 0.0);
  if (!one || !zero)
    return;

  if (!alpha)
    alpha = one;
  if (!beta)
    beta = zero;

  /// Create C init loop: for j: C[i,j] = beta*C[i,j]
  auto emitInitBody = [&](OpBuilder &ib, Location iloc, Value jIV) {
    auto cOld =
        ib.create<memref::LoadOp>(iloc, m.memC, ValueRange{iIndex, jIV});
    Value scaled;
    if (auto betaC = getConstFloat(beta); betaC && *betaC == 0.0) {
      scaled = zero;
    } else if (auto betaC = getConstFloat(beta); betaC && *betaC == 1.0) {
      scaled = cOld;
    } else {
      scaled = ib.create<arith::MulFOp>(iloc, cOld, beta);
    }
    ib.create<memref::StoreOp>(iloc, scaled, m.memC, ValueRange{iIndex, jIV});
  };

  Operation *initAnchor = nullptr;
  if (tileJ > 1) {
    if (auto tiledInitOuter = createTiledForIfBeneficial(
            b, loc, jLoop, tileJ, minTripCount,
            [&](OpBuilder &tb, Location tloc, Value jIV) {
              emitInitBody(tb, tloc, jIV);
            })) {
      initAnchor = tiledInitOuter.getOperation();
    }
  }
  if (!initAnchor) {
    auto initLoop = b.create<scf::ForOp>(
        loc, jLoop.getLowerBound(), jLoop.getUpperBound(), jLoop.getStep());
    {
      OpBuilder::InsertionGuard g(b);
      b.setInsertionPointToStart(initLoop.getBody());
      emitInitBody(b, loc, initLoop.getInductionVar());
    }
    initAnchor = initLoop.getOperation();
  }

  /// Insert the update loop nest after the init loop.
  b.setInsertionPointAfter(initAnchor);

  /// Create k loop: for k: a = alpha*A[i,k]; for j: C[i,j] += a*B[k,j]
  auto newK = b.create<scf::ForOp>(loc, kLoop.getLowerBound(),
                                   kLoop.getUpperBound(), kLoop.getStep());
  /// Preserve original k loop attrs if any.
  if (auto idAttr = kLoop->getAttr("arts.id"))
    newK->setAttr("arts.id", idAttr);
  if (auto loopAttr = kLoop->getAttr("arts.loop"))
    newK->setAttr("arts.loop",
                  sanitizeLoopMetadataNoReductions(loopAttr, b.getContext()));

  b.setInsertionPointToStart(newK.getBody());
  Value kIV = newK.getInductionVar();

  /// Load A[i,k]
  Value aLoad;
  if (m.aIsKI)
    aLoad = b.create<memref::LoadOp>(loc, m.memA, ValueRange{kIV, iIndex});
  else
    aLoad = b.create<memref::LoadOp>(loc, m.memA, ValueRange{iIndex, kIV});

  Value aAlpha;
  if (auto alphaC = getConstFloat(alpha); alphaC && *alphaC == 1.0)
    aAlpha = aLoad;
  else
    aAlpha = b.create<arith::MulFOp>(loc, aLoad, alpha);

  Operation *aAlphaOp = aAlpha.getDefiningOp();
  if (!aAlphaOp)
    return;

  auto emitUpdateBody = [&](OpBuilder &ub, Location uloc, Value jIV) {
    /// Load B[k,j] (or [j,k] depending on original)
    Value bVal;
    if (m.bIsKJ)
      bVal = ub.create<memref::LoadOp>(uloc, m.memB, ValueRange{kIV, jIV});
    else
      bVal = ub.create<memref::LoadOp>(uloc, m.memB, ValueRange{jIV, kIV});

    auto cOld =
        ub.create<memref::LoadOp>(uloc, m.memC, ValueRange{iIndex, jIV});
    auto prod = ub.create<arith::MulFOp>(uloc, aAlpha, bVal);
    auto cNew = ub.create<arith::AddFOp>(uloc, cOld, prod);
    ub.create<memref::StoreOp>(uloc, cNew, m.memC, ValueRange{iIndex, jIV});
  };

  bool createdTiledUpdate = false;
  {
    OpBuilder::InsertionGuard g(b);
    b.setInsertionPointAfter(aAlphaOp);

    if (tileJ > 1) {
      if (auto tiledUpdateOuter = createTiledForIfBeneficial(
              b, loc, jLoop, tileJ, minTripCount,
              [&](OpBuilder &tb, Location tloc, Value jIV) {
                emitUpdateBody(tb, tloc, jIV);
              })) {
        for (Operation &op : tiledUpdateOuter.getBody()->without_terminator()) {
          if (auto innerJ = dyn_cast<scf::ForOp>(&op)) {
            if (auto idAttr = jLoop->getAttr("arts.id"))
              innerJ->setAttr("arts.id", idAttr);
            if (auto loopAttr = jLoop->getAttr("arts.loop"))
              innerJ->setAttr("arts.loop", sanitizeLoopMetadataNoReductions(
                                               loopAttr, b.getContext()));
            break;
          }
        }
        createdTiledUpdate = true;
      }
    }

    if (!createdTiledUpdate) {
      auto newJ = b.create<scf::ForOp>(loc, jLoop.getLowerBound(),
                                       jLoop.getUpperBound(), jLoop.getStep());
      if (auto idAttr = jLoop->getAttr("arts.id"))
        newJ->setAttr("arts.id", idAttr);
      if (auto loopAttr = jLoop->getAttr("arts.loop"))
        newJ->setAttr("arts.loop", sanitizeLoopMetadataNoReductions(
                                       loopAttr, b.getContext()));
      b.setInsertionPointToStart(newJ.getBody());
      emitUpdateBody(b, loc, newJ.getInductionVar());
    }
  }

  /// Erase original jLoop (includes original kLoop and store).
  jLoop.erase();
}

static void rewriteMatmulUpdateToHoistAndTile(const MatmulUpdateMatch &m,
                                              bool enableTiling, int64_t tileJ,
                                              int64_t minTripCount) {
  scf::ForOp kLoop = m.kLoop;
  scf::ForOp jLoop = m.jLoop;

  OpBuilder b(jLoop);
  Location loc = jLoop.getLoc();

  Value one = getOrCreateFloatConstant(b, loc, m.elemTy, 1.0);
  if (!one)
    return;

  Value alpha = m.alpha ? m.alpha : one;

  /// Insert the hoisted A load (and alpha scaling) at the start of k loop.
  b.setInsertionPointToStart(kLoop.getBody());
  Value kIV = kLoop.getInductionVar();

  Value aLoad;
  if (m.aIsKI)
    aLoad = b.create<memref::LoadOp>(loc, m.memA, ValueRange{kIV, m.iIndex});
  else
    aLoad = b.create<memref::LoadOp>(loc, m.memA, ValueRange{m.iIndex, kIV});

  Value aAlpha = aLoad;
  if (auto alphaC = getConstFloat(alpha); !(alphaC && *alphaC == 1.0))
    aAlpha = b.create<arith::MulFOp>(loc, aLoad, alpha);

  Operation *anchor = aAlpha.getDefiningOp();
  if (!anchor)
    return;

  auto emitUpdateBody = [&](OpBuilder &ub, Location uloc, Value jIV) {
    Value bVal;
    if (m.bIsKJ)
      bVal = ub.create<memref::LoadOp>(uloc, m.memB, ValueRange{kIV, jIV});
    else
      bVal = ub.create<memref::LoadOp>(uloc, m.memB, ValueRange{jIV, kIV});

    auto cOld =
        ub.create<memref::LoadOp>(uloc, m.memC, ValueRange{m.iIndex, jIV});
    auto prod = ub.create<arith::MulFOp>(uloc, aAlpha, bVal);
    auto cNew = ub.create<arith::AddFOp>(uloc, cOld, prod);
    ub.create<memref::StoreOp>(uloc, cNew, m.memC, ValueRange{m.iIndex, jIV});
  };

  b.setInsertionPointAfter(anchor);

  bool createdTiled = false;
  if (enableTiling && tileJ > 1) {
    if (auto tiledOuter = createTiledForIfBeneficial(
            b, loc, jLoop, tileJ, minTripCount,
            [&](OpBuilder &tb, Location tloc, Value jIV) {
              emitUpdateBody(tb, tloc, jIV);
            })) {
      /// Attach original j loop metadata to the new innermost j loop.
      for (Operation &op : tiledOuter.getBody()->without_terminator()) {
        if (auto innerJ = dyn_cast<scf::ForOp>(&op)) {
          if (auto idAttr = jLoop->getAttr("arts.id"))
            innerJ->setAttr("arts.id", idAttr);
          if (auto loopAttr = jLoop->getAttr("arts.loop"))
            innerJ->setAttr("arts.loop", sanitizeLoopMetadataNoReductions(
                                             loopAttr, b.getContext()));
          break;
        }
      }
      createdTiled = true;
    }
  }

  if (!createdTiled) {
    auto newJ = b.create<scf::ForOp>(loc, jLoop.getLowerBound(),
                                     jLoop.getUpperBound(), jLoop.getStep());
    if (auto idAttr = jLoop->getAttr("arts.id"))
      newJ->setAttr("arts.id", idAttr);
    if (auto loopAttr = jLoop->getAttr("arts.loop"))
      newJ->setAttr("arts.loop",
                    sanitizeLoopMetadataNoReductions(loopAttr, b.getContext()));
    b.setInsertionPointToStart(newJ.getBody());
    emitUpdateBody(b, loc, newJ.getInductionVar());
  }

  jLoop.erase();
}

struct LoopTransformsPass
    : public arts::ArtsLoopTransformsBase<LoopTransformsPass> {
  LoopTransformsPass(ArtsAnalysisManager *AM, bool enableMatmul,
                     bool enableTiling, int64_t tileJ, int64_t minTripCount)
      : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
    this->enableMatmul = enableMatmul;
    this->enableTiling = enableTiling;
    this->tileJ = tileJ;
    this->minTripCount = minTripCount;
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    ARTS_INFO_HEADER(LoopTransformsPass);

    int rewrites = 0;

    /// Collect arts.for ops first to avoid iterator invalidation during
    /// rewrites.
    SmallVector<ForOp, 16> artsFors;
    module.walk([&](ForOp fo) { artsFors.push_back(fo); });

    for (ForOp fo : artsFors) {
      if (!enableMatmul)
        continue;

      MatmulDotMatch dot;
      if (matchMatmulDotInArtsFor(fo, dot)) {
        /// Conservative profitability gates:
        /// - Only rewrite when B is accessed as B[k,j] (k varies in dot-product
        /// =>
        ///   strided), since the rewrite makes j the inner dimension and
        ///   enables stride-1 access.
        /// - Require tiling to be enabled and applicable to avoid exploding C
        ///   traffic for tiny loops.
        if (!dot.bIsKJ) {
          ARTS_DEBUG("Skipping matmul dot rewrite: B is indexed as B[j,k] "
                     "(already friendly for k-inner dot-product)");
          continue;
        }
        if (!enableTiling ||
            !isTilingApplicable(dot.jLoop, tileJ, minTripCount)) {
          ARTS_DEBUG(
              "Skipping matmul dot rewrite: tiling disabled or not applicable "
              "(requires constant step=1 and tripCount >= minTripCount)");
          continue;
        }

        ARTS_INFO("Detected matmul dot-product pattern inside arts.for; "
                  << "rewriting to k-j update form");

        int64_t effectiveTileJ = enableTiling ? tileJ : 1;
        rewriteMatmulDotToKJUpdate(dot, effectiveTileJ, minTripCount);
        rewrites++;
        continue;
      }

      MatmulUpdateMatch upd;
      if (!matchMatmulUpdateInArtsFor(fo, upd))
        continue;

      /// Profitability: only bother if we can make B stride-1 (k,j) and either
      /// tile or at least hoist A out of the inner loop.
      if (!upd.bIsKJ) {
        ARTS_DEBUG(
            "Skipping matmul update optimization: B is indexed as B[j,k]");
        continue;
      }

      bool canTile =
          enableTiling && isTilingApplicable(upd.jLoop, tileJ, minTripCount);
      if (!canTile && !enableTiling) {
        /// Hoisting-only is still fine, but keep the same default safety gate:
        /// if tiling is disabled globally, skip update-form rewrites to avoid
        /// unexpected IR churn in non-performance builds.
        ARTS_DEBUG("Skipping matmul update optimization: tiling disabled");
        continue;
      }

      ARTS_INFO("Detected matmul update-form pattern inside arts.for; "
                << "hoisting A and tiling j where applicable");
      rewriteMatmulUpdateToHoistAndTile(upd, /*enableTiling=*/canTile, tileJ,
                                        minTripCount);
      rewrites++;
    }

    ARTS_INFO("LoopTransformsPass: applied " << rewrites << " rewrite(s)");
    (void)AM;
    ARTS_INFO_FOOTER(LoopTransformsPass);
  }

private:
  ArtsAnalysisManager *AM;
};

} // namespace

std::unique_ptr<Pass>
mlir::arts::createLoopTransformsPass(ArtsAnalysisManager *AM, bool enableMatmul,
                                     bool enableTiling, int64_t tileJ,
                                     int64_t minTripCount) {
  return std::make_unique<LoopTransformsPass>(AM, enableMatmul, enableTiling,
                                              tileJ, minTripCount);
}
