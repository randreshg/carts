///==========================================================================///
/// LoopTransforms.cpp - Reduction distribution for iter_args matmul patterns
///
/// Transforms dot-product form to k-j update form for better cache locality.
/// Handles patterns with scf.for iter_args (e.g., gemm).
///
/// Example (gemm):
///
///   for j:                           for j: C[i,j] = 0
///     sum = for k iter_args:   =>    for k:
///       sum += A[i,k]*B[k,j]           a = A[i,k]              // hoisted
///     C[i,j] = sum                     for j: C[i,j] += a*B[k,j]  // stride-1
///
/// See LoopReordering.cpp for explicit-init patterns (e.g., 2mm, 3mm).
/// Supports FAdd/IAdd reductions. Must run BEFORE CreateDbs.
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

/// Reduction operation kind - supports both float and integer operations.
enum class ReductionKind {
  Unknown,
  FAdd, /// sum += A*B (float)
  IAdd, /// sum += A*B (integer)
  FMul, /// product *= A*B (float, future)
  IMul, /// product *= A*B (integer, future)
};

static bool isFloatReduction(ReductionKind kind) {
  return kind == ReductionKind::FAdd || kind == ReductionKind::FMul;
}

[[maybe_unused]] static bool isIntegerReduction(ReductionKind kind) {
  return kind == ReductionKind::IAdd || kind == ReductionKind::IMul;
}

/// Strip cast operations (float and integer) to find the underlying value.
static Value stripCasts(Value v) {
  while (v) {
    if (auto ext = v.getDefiningOp<arith::ExtFOp>())
      v = ext.getIn();
    else if (auto trunc = v.getDefiningOp<arith::TruncFOp>())
      v = trunc.getIn();
    else if (auto extSI = v.getDefiningOp<arith::ExtSIOp>())
      v = extSI.getIn();
    else if (auto extUI = v.getDefiningOp<arith::ExtUIOp>())
      v = extUI.getIn();
    else if (auto truncI = v.getDefiningOp<arith::TruncIOp>())
      v = truncI.getIn();
    else
      break;
  }
  return v;
}

/// Try to extract a constant floating-point value.
static std::optional<double> getConstFloat(Value v) {
  if (!v)
    return std::nullopt;
  if (auto cst = v.getDefiningOp<arith::ConstantOp>()) {
    if (auto fa = dyn_cast<FloatAttr>(cst.getValue()))
      return fa.getValueAsDouble();
  }
  return std::nullopt;
}

/// Try to extract a constant integer value.
static std::optional<int64_t> getConstInt(Value v) {
  if (!v)
    return std::nullopt;
  if (auto cst = v.getDefiningOp<arith::ConstantOp>()) {
    if (auto ia = dyn_cast<IntegerAttr>(cst.getValue()))
      return ia.getInt();
  }
  return std::nullopt;
}

/// Create a constant for the given type (float or integer).
static Value getOrCreateConstant(OpBuilder &b, Location loc, Type ty,
                                 double floatVal, int64_t intVal) {
  if (ty.isF32())
    return b.create<arith::ConstantFloatOp>(loc, APFloat((float)floatVal),
                                            b.getF32Type());
  if (ty.isF64())
    return b.create<arith::ConstantFloatOp>(loc, APFloat(floatVal),
                                            b.getF64Type());
  if (ty.isIntOrIndex()) {
    return b.create<arith::ConstantIntOp>(loc, intVal, ty);
  }
  return Value();
}

/// Create zero constant for the given type.
static Value getOrCreateZero(OpBuilder &b, Location loc, Type ty) {
  return getOrCreateConstant(b, loc, ty, 0.0, 0);
}

/// Create one constant for the given type.
static Value getOrCreateOne(OpBuilder &b, Location loc, Type ty) {
  return getOrCreateConstant(b, loc, ty, 1.0, 1);
}

/// Match a multiplication operation (float or integer).
[[maybe_unused]] static bool matchMulOp(Value v, Value &lhs, Value &rhs,
                                        ReductionKind &kind) {
  if (!v)
    return false;
  if (auto mul = v.getDefiningOp<arith::MulFOp>()) {
    lhs = mul.getLhs();
    rhs = mul.getRhs();
    kind = ReductionKind::FAdd; /// Multiplication in add-reduction context
    return true;
  }
  if (auto mul = v.getDefiningOp<arith::MulIOp>()) {
    lhs = mul.getLhs();
    rhs = mul.getRhs();
    kind = ReductionKind::IAdd;
    return true;
  }
  return false;
}

/// Match multiplication (float only) for backward compatibility.
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

/// Match an addition operation (float or integer).
[[maybe_unused]] static bool matchAddOp(Value v, Value &lhs, Value &rhs,
                                        ReductionKind &kind) {
  if (!v)
    return false;
  if (auto add = v.getDefiningOp<arith::AddFOp>()) {
    lhs = add.getLhs();
    rhs = add.getRhs();
    kind = ReductionKind::FAdd;
    return true;
  }
  if (auto add = v.getDefiningOp<arith::AddIOp>()) {
    lhs = add.getLhs();
    rhs = add.getRhs();
    kind = ReductionKind::IAdd;
    return true;
  }
  return false;
}

/// Match addition (float only) for backward compatibility.
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

/// Create a multiplication operation for the appropriate type.
static Value createMul(OpBuilder &b, Location loc, Value lhs, Value rhs,
                       ReductionKind kind) {
  if (isFloatReduction(kind))
    return b.create<arith::MulFOp>(loc, lhs, rhs);
  else
    return b.create<arith::MulIOp>(loc, lhs, rhs);
}

/// Create an addition operation for the appropriate type.
static Value createAdd(OpBuilder &b, Location loc, Value lhs, Value rhs,
                       ReductionKind kind) {
  if (isFloatReduction(kind))
    return b.create<arith::AddFOp>(loc, lhs, rhs);
  else
    return b.create<arith::AddIOp>(loc, lhs, rhs);
}

/// Reduction dot-product pattern match result.
/// Supports both floating-point and integer reduction operations.
struct ReductionDotMatch {
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
  ReductionKind kind = ReductionKind::FAdd; /// Default to float-add
};

/// Backward-compatible alias.
using MatmulDotMatch = ReductionDotMatch;

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

  /// Determine reduction kind based on element type.
  ReductionKind kind = ReductionKind::Unknown;
  if (elemTy.isF32() || elemTy.isF64())
    kind = ReductionKind::FAdd;
  else if (elemTy.isIntOrIndex())
    kind = ReductionKind::IAdd;
  else
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

  /// Find multiplication (float or integer) in kLoop body.
  memref::LoadOp loadA = nullptr, loadB = nullptr;
  Value mulLhs, mulRhs;
  Operation *mulOp = nullptr;

  for (Operation &op : kLoop.getBody()->without_terminator()) {
    if (auto m = dyn_cast<arith::MulFOp>(&op)) {
      mulOp = &op;
      mulLhs = m.getLhs();
      mulRhs = m.getRhs();
      break;
    }
    if (auto m = dyn_cast<arith::MulIOp>(&op)) {
      mulOp = &op;
      mulLhs = m.getLhs();
      mulRhs = m.getRhs();
      break;
    }
  }
  if (!mulOp)
    return false;

  auto findLoad = [&](Value v) -> memref::LoadOp {
    v = stripCasts(v);
    return v.getDefiningOp<memref::LoadOp>();
  };
  loadA = findLoad(mulLhs);
  loadB = findLoad(mulRhs);
  if (!loadA || !loadB) {
    /// Try swapped
    loadA = findLoad(mulRhs);
    loadB = findLoad(mulLhs);
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

  out = ReductionDotMatch{artsFor,
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
                          elemTy,
                          kind};
  return true;
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

/// Check if a value is a zero constant (float or integer).
static bool isZeroConstant(Value v) {
  if (auto floatVal = getConstFloat(v))
    return *floatVal == 0.0;
  if (auto intVal = getConstInt(v))
    return *intVal == 0;
  return false;
}

/// Check if a value is a one constant (float or integer).
static bool isOneConstant(Value v) {
  if (auto floatVal = getConstFloat(v))
    return *floatVal == 1.0;
  if (auto intVal = getConstInt(v))
    return *intVal == 1;
  return false;
}

static void rewriteReductionDotToKJUpdate(const ReductionDotMatch &m,
                                          int64_t tileJ, int64_t minTripCount) {
  scf::ForOp jLoop = m.jLoop;
  scf::ForOp kLoop = m.kLoop;

  OpBuilder b(jLoop);
  Location loc = jLoop.getLoc();

  Value alpha = m.alpha;
  Value beta = m.beta;
  Value iIndex = m.iIndex;
  ReductionKind kind = m.kind;

  Value one = getOrCreateOne(b, loc, m.elemTy);
  Value zero = getOrCreateZero(b, loc, m.elemTy);
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
    if (isZeroConstant(beta)) {
      scaled = zero;
    } else if (isOneConstant(beta)) {
      scaled = cOld;
    } else {
      scaled = createMul(ib, iloc, cOld, beta, kind);
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
  if (isOneConstant(alpha))
    aAlpha = aLoad;
  else
    aAlpha = createMul(b, loc, aLoad, alpha, kind);

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
    Value prod = createMul(ub, uloc, aAlpha, bVal, kind);
    Value cNew = createAdd(ub, uloc, cOld, prod, kind);
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

      /// Pattern 1: Dot-product to k-j update form transformation
      /// This is ARTS-specific reduction distribution that exposes better
      /// parallelism and cache locality for matmul-like patterns.
      ///
      /// Note: Hoisting and general LICM are now handled by affine passes
      /// in Stage 2 (via --enable-affine-opt). This pass focuses solely on
      /// ARTS-specific reduction pattern distribution.
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
          ARTS_DEBUG("Skipping reduction distribution: B is indexed as B[j,k] "
                     "(already friendly for k-inner dot-product)");
          continue;
        }
        if (!enableTiling ||
            !isTilingApplicable(dot.jLoop, tileJ, minTripCount)) {
          ARTS_DEBUG("Skipping reduction distribution: tiling disabled or not "
                     "applicable (requires constant step=1 and tripCount >= "
                     "minTripCount)");
          continue;
        }

        ARTS_INFO("Detected reduction dot-product pattern inside arts.for; "
                  << "rewriting to k-j update form");

        int64_t effectiveTileJ = enableTiling ? tileJ : 1;
        rewriteReductionDotToKJUpdate(dot, effectiveTileJ, minTripCount);
        rewrites++;
        continue;
      }

      /// Pattern 2 (update-form hoisting) has been REMOVED.
      /// Hoisting is now handled by affine LICM in Stage 2 via
      /// --enable-affine-opt flag. This provides better separation of concerns:
      /// - Generic MLIR passes: LICM, tiling, simplify (Stage 2, affine.for)
      /// - ARTS-specific passes: reduction distribution (Stage 6, arts.for)
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
