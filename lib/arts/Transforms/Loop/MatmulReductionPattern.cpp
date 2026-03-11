///==========================================================================///
/// MatmulReductionPattern.cpp - Reduction distribution for matmul patterns
///
/// Transforms dot-product form to k-j update form for better cache locality.
/// Handles patterns with scf.for iter_args (e.g., gemm, 2mm, 3mm).
///
/// Example (gemm):
///
///   for j:                           for j: C[i,j] = 0
///     sum = for k iter_args:   =>    for k:
///       sum += A[i,k]*B[k,j]           a = A[i,k]                /// hoisted
///     C[i,j] = sum                     for j: C[i,j] += a*B[k,j] /// stride-1
///==========================================================================///

#include "arts/Transforms/Loop/LoopNormalizer.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"
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

  if (!loopAttr.getHasReductions() || !loopAttr.getHasReductions().getValue())
    return attr;

  return LoopMetadataAttr::get(
      ctx,
      /*potentiallyParallel=*/loopAttr.getPotentiallyParallel(),
      /*hasReductions=*/BoolAttr::get(ctx, false),
      /*reductionKinds=*/ArrayAttr(),
      /*tripCount=*/loopAttr.getTripCount(),
      /*nestingLevel=*/loopAttr.getNestingLevel(),
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

static Value getOrCreateZero(OpBuilder &b, Location loc, Type ty) {
  return b.create<arith::ConstantOp>(loc, ty, b.getZeroAttr(ty));
}

static Value getOrCreateOne(OpBuilder &b, Location loc, Type ty) {
  if (ty.isF32())
    return b.create<arith::ConstantOp>(loc, FloatAttr::get(ty, 1.0f));
  if (ty.isF64())
    return b.create<arith::ConstantOp>(loc, FloatAttr::get(ty, 1.0));
  if (ty.isIntOrIndex())
    return b.create<arith::ConstantOp>(loc, ty, b.getIntegerAttr(ty, 1));
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

static Value createMul(OpBuilder &b, Location loc, Value lhs, Value rhs,
                       ReductionKind kind) {
  if (isFloatReduction(kind))
    return b.create<arith::MulFOp>(loc, lhs, rhs);
  else
    return b.create<arith::MulIOp>(loc, lhs, rhs);
}

static Value createAdd(OpBuilder &b, Location loc, Value lhs, Value rhs,
                       ReductionKind kind) {
  if (isFloatReduction(kind))
    return b.create<arith::AddFOp>(loc, lhs, rhs);
  else
    return b.create<arith::AddIOp>(loc, lhs, rhs);
}

/// Reduction dot-product pattern match result.
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
  ReductionKind kind = ReductionKind::FAdd;
};

static bool matchAlphaBeta(Value storeVal, Value sumVal, Value oldCVal,
                           Value &outAlpha, Value &outBeta) {
  outAlpha = Value();
  outBeta = Value();

  auto matchSumTerm = [&](Value v, Value &alpha) -> bool {
    v = ValueUtils::stripNumericCasts(v);
    if (v == sumVal) {
      alpha = Value();
      return true;
    }
    Value a, b;
    if (matchMulFOp(v, a, b)) {
      a = ValueUtils::stripNumericCasts(a);
      b = ValueUtils::stripNumericCasts(b);
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

  auto matchCTerm = [&](Value v, Value &beta) -> bool {
    v = ValueUtils::stripNumericCasts(v);
    if (v == oldCVal) {
      beta = Value();
      return true;
    }
    Value a, b;
    if (matchMulFOp(v, a, b)) {
      a = ValueUtils::stripNumericCasts(a);
      b = ValueUtils::stripNumericCasts(b);
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
  if (ValueUtils::stripNumericCasts(storeVal) == sumVal) {
    outAlpha = Value();
    outBeta = Value();
    return true;
  }

  /// Pattern 1b: store = mul(sum, alpha) (beta=0)
  {
    Value a, b;
    if (matchMulFOp(storeVal, a, b)) {
      a = ValueUtils::stripNumericCasts(a);
      b = ValueUtils::stripNumericCasts(b);
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

static bool matchMatmulDotInArtsFor(ForOp artsFor, ReductionDotMatch &out) {
  scf::ForOp jLoop = nullptr;
  for (Operation &op : artsFor.getBody()->without_terminator()) {
    if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
      jLoop = forOp;
      break;
    }
  }
  if (!jLoop)
    return false;

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

  ReductionKind kind = ReductionKind::Unknown;
  if (elemTy.isF32() || elemTy.isF64())
    kind = ReductionKind::FAdd;
  else if (elemTy.isIntOrIndex())
    kind = ReductionKind::IAdd;
  else
    return false;

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
    v = ValueUtils::stripNumericCasts(v);
    return v.getDefiningOp<memref::LoadOp>();
  };
  loadA = findLoad(mulLhs);
  loadB = findLoad(mulRhs);
  if (!loadA || !loadB) {
    loadA = findLoad(mulRhs);
    loadB = findLoad(mulLhs);
  }
  if (!loadA || !loadB)
    return false;

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
  bool hasConstStep =
      ValueUtils::getConstantIndex(originalLoop.getStep(), stepC);
  bool hasConstLb =
      ValueUtils::getConstantIndex(originalLoop.getLowerBound(), lbC);
  bool hasConstUb =
      ValueUtils::getConstantIndex(originalLoop.getUpperBound(), ubC);
  if (!hasConstStep || stepC != 1 || !hasConstLb || !hasConstUb)
    return nullptr;

  int64_t trip = ubC - lbC;
  if (trip < minTripCount)
    return nullptr;

  auto tileStepVal = arts::createConstantIndex(b, loc, tileSize);
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
  bool hasConstStep = ValueUtils::getConstantIndex(loop.getStep(), stepC);
  bool hasConstLb = ValueUtils::getConstantIndex(loop.getLowerBound(), lbC);
  bool hasConstUb = ValueUtils::getConstantIndex(loop.getUpperBound(), ubC);
  if (!hasConstStep || stepC != 1 || !hasConstLb || !hasConstUb)
    return false;
  int64_t trip = ubC - lbC;
  return trip >= minTripCount;
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
    if (ValueUtils::isZeroConstant(beta)) {
      scaled = zero;
    } else if (ValueUtils::isOneConstant(beta)) {
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
  if (auto idAttr = kLoop->getAttr(AttrNames::Operation::ArtsId))
    newK->setAttr(AttrNames::Operation::ArtsId, idAttr);
  if (auto loopAttr = kLoop->getAttr(AttrNames::LoopMetadata::Name))
    newK->setAttr(AttrNames::LoopMetadata::Name,
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
  if (ValueUtils::isOneConstant(alpha))
    aAlpha = aLoad;
  else
    aAlpha = createMul(b, loc, aLoad, alpha, kind);

  Operation *aAlphaOp = aAlpha.getDefiningOp();
  if (!aAlphaOp)
    return;

  auto emitUpdateBody = [&](OpBuilder &ub, Location uloc, Value jIV) {
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
            if (auto idAttr = jLoop->getAttr(AttrNames::Operation::ArtsId))
              innerJ->setAttr(AttrNames::Operation::ArtsId, idAttr);
            if (auto loopAttr = jLoop->getAttr(AttrNames::LoopMetadata::Name))
              innerJ->setAttr(
                  AttrNames::LoopMetadata::Name,
                  sanitizeLoopMetadataNoReductions(loopAttr, b.getContext()));
            break;
          }
        }
        createdTiledUpdate = true;
      }
    }

    if (!createdTiledUpdate) {
      auto newJ = b.create<scf::ForOp>(loc, jLoop.getLowerBound(),
                                       jLoop.getUpperBound(), jLoop.getStep());
      if (auto idAttr = jLoop->getAttr(AttrNames::Operation::ArtsId))
        newJ->setAttr(AttrNames::Operation::ArtsId, idAttr);
      if (auto loopAttr = jLoop->getAttr(AttrNames::LoopMetadata::Name))
        newJ->setAttr(
            AttrNames::LoopMetadata::Name,
            sanitizeLoopMetadataNoReductions(loopAttr, b.getContext()));
      b.setInsertionPointToStart(newJ.getBody());
      emitUpdateBody(b, loc, newJ.getInductionVar());
    }
  }

  /// Erase original jLoop (includes original kLoop and store).
  jLoop.erase();
}

} // namespace

namespace mlir {
namespace arts {

class MatmulReductionPattern : public LoopPattern {
public:
  MatmulReductionPattern(bool enableTiling, int64_t tileJ, int64_t minTripCount)
      : enableTiling(enableTiling), tileJ(tileJ), minTripCount(minTripCount) {}

  bool match(ForOp artsFor) override {
    matchResult = {};
    if (!matchMatmulDotInArtsFor(artsFor, matchResult))
      return false;

    /// Profitability gate: only rewrite when B is accessed as B[k,j]
    if (!matchResult.bIsKJ) {
      ARTS_DEBUG("Skipping reduction distribution: B is indexed as B[j,k] "
                 "(already friendly for k-inner dot-product)");
      return false;
    }

    /// Require tiling to be enabled and applicable
    if (!enableTiling ||
        !isTilingApplicable(matchResult.jLoop, tileJ, minTripCount)) {
      ARTS_DEBUG("Skipping reduction distribution: tiling disabled or not "
                 "applicable (requires constant step=1 and tripCount >= "
                 "minTripCount)");
      return false;
    }

    return true;
  }

  LogicalResult apply(OpBuilder &builder) override {
    int64_t effectiveTileJ = enableTiling ? tileJ : 1;
    ARTS_INFO("Detected reduction dot-product pattern inside arts.for; "
              << "rewriting to k-j update form");
    rewriteReductionDotToKJUpdate(matchResult, effectiveTileJ, minTripCount);
    return success();
  }

  StringRef getName() const override { return "matmul-reduction"; }

private:
  bool enableTiling;
  int64_t tileJ;
  int64_t minTripCount;
  ReductionDotMatch matchResult;
};

std::unique_ptr<LoopPattern>
createMatmulReductionPattern(bool enableTiling, int64_t tileJ,
                             int64_t minTripCount) {
  return std::make_unique<MatmulReductionPattern>(enableTiling, tileJ,
                                                  minTripCount);
}

} // namespace arts
} // namespace mlir
