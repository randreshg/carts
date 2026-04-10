///==========================================================================///
/// MatmulReductionPattern.cpp - Matmul reduction rewrite and tiling
///
/// Transforms dot-product form to k-j update form for better cache locality.
///
/// Example (gemm):
///
///   Before:
///     for j:
///       sum = for k iter_args:
///         sum += A[i,k] * B[k,j]
///       C[i,j] = sum
///
///   After:
///     for j:
///       C[i,j] = 0
///     for k:
///       a = A[i,k]
///       for j:
///         C[i,j] += a * B[k,j]
///
///   IR sketch:
///     Before:
///       %sum = scf.for %k = %c0 to %K iter_args(%acc = %zero) -> (f64) {
///         %prod = arith.mulf %a, %b : f64
///         %next = arith.addf %acc, %prod : f64
///         scf.yield %next : f64
///       }
///       memref.store %sum, %C[%i, %j]
///
///     After:
///       memref.store %zero, %C[%i, %j]
///       scf.for %k = %c0 to %K {
///         %a = memref.load %A[%i, %k]
///         scf.for %j = %c0 to %N {
///           %old = memref.load %C[%i, %j]
///           %new = arith.addf %old, %prod
///           memref.store %new, %C[%i, %j]
///         }
///       }
///==========================================================================///

/// This file implements a kernel-form transform, not a dependence rewrite.
/// It changes a reduction-carried matmul kernel into an update form that is
/// easier for downstream ARTS distribution and DB partitioning to exploit.

#include "arts/dialect/core/Analysis/metadata/MetadataManager.h"
#include "arts/dialect/core/Transforms/kernel/KernelTransform.h"
#include "arts/dialect/core/Transforms/pattern/PatternTransform.h"
#include "arts/utils/Debug.h"
#include "arts/utils/LoopUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"

ARTS_DEBUG_SETUP(kernel_transforms);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Reduction operation kind - supports both float and integer operations.
enum class ReductionKind {
  Unknown,
  FAdd, /// sum += A*B (float)
  IAdd, /// sum += A*B (integer)
};

static bool isFloatReduction(ReductionKind kind) {
  return kind == ReductionKind::FAdd;
}

static Value getOrCreateZero(OpBuilder &b, Location loc, Type ty) {
  return arith::ConstantOp::create(b, loc, ty, b.getZeroAttr(ty));
}

static Value getOrCreateOne(OpBuilder &b, Location loc, Type ty) {
  if (ty.isF32())
    return arith::ConstantOp::create(b, loc, FloatAttr::get(ty, 1.0f));
  if (ty.isF64())
    return arith::ConstantOp::create(b, loc, FloatAttr::get(ty, 1.0));
  if (ty.isIntOrIndex())
    return arith::ConstantOp::create(b, loc, ty, b.getIntegerAttr(ty, 1));
  return Value();
}

static bool matchMulOp(Value v, Value &lhs, Value &rhs) {
  if (!v)
    return false;
  if (auto mul = v.getDefiningOp<arith::MulFOp>()) {
    lhs = mul.getLhs();
    rhs = mul.getRhs();
    return true;
  }
  if (auto mul = v.getDefiningOp<arith::MulIOp>()) {
    lhs = mul.getLhs();
    rhs = mul.getRhs();
    return true;
  }
  return false;
}

static bool matchAddOp(Value v, Value &lhs, Value &rhs) {
  if (!v)
    return false;
  if (auto add = v.getDefiningOp<arith::AddFOp>()) {
    lhs = add.getLhs();
    rhs = add.getRhs();
    return true;
  }
  if (auto add = v.getDefiningOp<arith::AddIOp>()) {
    lhs = add.getLhs();
    rhs = add.getRhs();
    return true;
  }
  return false;
}

static Value createMul(OpBuilder &b, Location loc, Value lhs, Value rhs,
                       ReductionKind kind) {
  if (isFloatReduction(kind))
    return arith::MulFOp::create(b, loc, lhs, rhs);
  else
    return arith::MulIOp::create(b, loc, lhs, rhs);
}

static Value createAdd(OpBuilder &b, Location loc, Value lhs, Value rhs,
                       ReductionKind kind) {
  if (isFloatReduction(kind))
    return arith::AddFOp::create(b, loc, lhs, rhs);
  else
    return arith::AddIOp::create(b, loc, lhs, rhs);
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
    v = ValueAnalysis::stripNumericCasts(v);
    if (v == sumVal) {
      alpha = Value();
      return true;
    }
    Value a, b;
    if (matchMulOp(v, a, b)) {
      a = ValueAnalysis::stripNumericCasts(a);
      b = ValueAnalysis::stripNumericCasts(b);
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
    v = ValueAnalysis::stripNumericCasts(v);
    if (v == oldCVal) {
      beta = Value();
      return true;
    }
    Value a, b;
    if (matchMulOp(v, a, b)) {
      a = ValueAnalysis::stripNumericCasts(a);
      b = ValueAnalysis::stripNumericCasts(b);
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
  if (ValueAnalysis::stripNumericCasts(storeVal) == sumVal) {
    outAlpha = Value();
    outBeta = Value();
    return true;
  }

  /// Pattern 1b: store = mul(sum, alpha) (beta=0)
  {
    Value a, b;
    if (matchMulOp(storeVal, a, b)) {
      a = ValueAnalysis::stripNumericCasts(a);
      b = ValueAnalysis::stripNumericCasts(b);
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
  if (!matchAddOp(storeVal, x, y))
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

  auto matchScalarAccumulatorReduction =
      [&](scf::ForOp reductionLoop, Value &sumVal, Type &elemTy) -> bool {
    Value accMemref;
    bool sawReductionLoop = false;
    memref::LoadOp finalLoad = nullptr;

    for (Operation &op : jLoop.getBody()->without_terminator()) {
      if (&op == reductionLoop.getOperation()) {
        sawReductionLoop = true;
        continue;
      }

      if (!sawReductionLoop) {
        auto store = dyn_cast<memref::StoreOp>(&op);
        if (!store || !store.getIndices().empty())
          continue;

        Value candidateMemref =
            ValueAnalysis::stripMemrefViewOps(store.getMemRef());
        if (!candidateMemref)
          continue;
        if (!isa_and_nonnull<memref::AllocOp, memref::AllocaOp>(
                candidateMemref.getDefiningOp()))
          continue;
        accMemref = candidateMemref;
        continue;
      }

      auto load = dyn_cast<memref::LoadOp>(&op);
      if (!load || !load.getIndices().empty())
        continue;

      Value loadedMemref = ValueAnalysis::stripMemrefViewOps(load.getMemRef());
      if (loadedMemref != accMemref)
        continue;

      finalLoad = load;
      break;
    }

    if (!accMemref || !finalLoad)
      return false;

    memref::LoadOp loopLoad = nullptr;
    memref::StoreOp loopStore = nullptr;
    reductionLoop.getBody()->walk([&](Operation *nestedOp) {
      if (nestedOp == reductionLoop.getOperation())
        return;

      if (!loopLoad) {
        if (auto load = dyn_cast<memref::LoadOp>(nestedOp);
            load && load.getIndices().empty() &&
            ValueAnalysis::stripMemrefViewOps(load.getMemRef()) == accMemref) {
          loopLoad = load;
        }
      }

      if (!loopStore) {
        if (auto store = dyn_cast<memref::StoreOp>(nestedOp);
            store && store.getIndices().empty() &&
            ValueAnalysis::stripMemrefViewOps(store.getMemRef()) == accMemref) {
          loopStore = store;
        }
      }
    });

    if (!loopLoad || !loopStore)
      return false;

    Value addLhs, addRhs;
    if (!matchAddOp(loopStore.getValueToStore(), addLhs, addRhs))
      return false;

    addLhs = ValueAnalysis::stripNumericCasts(addLhs);
    addRhs = ValueAnalysis::stripNumericCasts(addRhs);
    if (addLhs != loopLoad.getResult() && addRhs != loopLoad.getResult())
      return false;

    sumVal = finalLoad.getResult();
    elemTy = sumVal.getType();
    return true;
  };

  scf::ForOp kLoop = nullptr;
  Value sumVal;
  Type elemTy;
  for (Operation &op : jLoop.getBody()->without_terminator()) {
    auto forOp = dyn_cast<scf::ForOp>(&op);
    if (!forOp)
      continue;

    if (forOp.getInitArgs().size() == 1 && forOp.getNumResults() == 1) {
      kLoop = forOp;
      sumVal = forOp.getResult(0);
      elemTy = sumVal.getType();
      break;
    }

    if (forOp.getInitArgs().empty() && forOp.getNumResults() == 0 &&
        matchScalarAccumulatorReduction(forOp, sumVal, elemTy)) {
      kLoop = forOp;
      break;
    }
  }
  if (!kLoop || !sumVal || !elemTy)
    return false;

  ReductionKind kind = ReductionKind::Unknown;
  if (elemTy.isF32() || elemTy.isF64())
    kind = ReductionKind::FAdd;
  else if (elemTy.isIntOrIndex())
    kind = ReductionKind::IAdd;
  else
    return false;

  memref::StoreOp storeC = nullptr;
  bool sawKLoop = false;
  for (Operation &op : jLoop.getBody()->without_terminator()) {
    if (&op == kLoop.getOperation()) {
      sawKLoop = true;
      continue;
    }
    if (auto st = dyn_cast<memref::StoreOp>(&op)) {
      if (!sawKLoop)
        continue;
      auto memrefType = dyn_cast<MemRefType>(st.getMemref().getType());
      if (!memrefType || memrefType.getRank() != 2 ||
          st.getIndices().size() != 2)
        continue;
      if (st.getIndices()[1] != jLoop.getInductionVar())
        continue;
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
    v = ValueAnalysis::stripNumericCasts(v);
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
      ValueAnalysis::getConstantIndex(originalLoop.getStep(), stepC);
  bool hasConstLb =
      ValueAnalysis::getConstantIndex(originalLoop.getLowerBound(), lbC);
  bool hasConstUb =
      ValueAnalysis::getConstantIndex(originalLoop.getUpperBound(), ubC);
  if (!hasConstStep || stepC != 1 || !hasConstLb || !hasConstUb)
    return nullptr;

  int64_t trip = ubC - lbC;
  if (trip < minTripCount)
    return nullptr;

  auto tileStepVal = arts::createConstantIndex(b, loc, tileSize);
  auto outer = scf::ForOp::create(b, loc, originalLoop.getLowerBound(),
                                  originalLoop.getUpperBound(), tileStepVal);

  b.setInsertionPointToStart(outer.getBody());
  Value tileBase = outer.getInductionVar();
  auto tileEnd = arith::AddIOp::create(b, loc, tileBase, tileStepVal);
  auto innerUb =
      arith::MinUIOp::create(b, loc, tileEnd, originalLoop.getUpperBound());

  auto inner =
      scf::ForOp::create(b, loc, tileBase, innerUb, originalLoop.getStep());
  b.setInsertionPointToStart(inner.getBody());
  innerBodyBuilder(b, loc, inner.getInductionVar());

  return outer;
}

static bool isTilingApplicable(scf::ForOp loop, int64_t tileSize,
                               int64_t minTripCount) {
  if (!loop || tileSize <= 1)
    return false;
  int64_t stepC = 0, lbC = 0, ubC = 0;
  bool hasConstStep = ValueAnalysis::getConstantIndex(loop.getStep(), stepC);
  bool hasConstLb = ValueAnalysis::getConstantIndex(loop.getLowerBound(), lbC);
  bool hasConstUb = ValueAnalysis::getConstantIndex(loop.getUpperBound(), ubC);
  if (!hasConstStep || stepC != 1 || !hasConstLb || !hasConstUb)
    return false;
  int64_t trip = ubC - lbC;
  return trip >= minTripCount;
}

static void rewriteReductionDotToKJUpdate(const ReductionDotMatch &m,
                                          int64_t tileJ, int64_t minTripCount,
                                          MetadataManager &metadataManager) {
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
        memref::LoadOp::create(ib, iloc, m.memC, ValueRange{iIndex, jIV});
    Value scaled;
    if (ValueAnalysis::isZeroConstant(beta)) {
      scaled = zero;
    } else if (ValueAnalysis::isOneConstant(beta)) {
      scaled = cOld;
    } else {
      scaled = createMul(ib, iloc, cOld, beta, kind);
    }
    memref::StoreOp::create(ib, iloc, scaled, m.memC, ValueRange{iIndex, jIV});
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
    auto initLoop = scf::ForOp::create(b, loc, jLoop.getLowerBound(),
                                       jLoop.getUpperBound(), jLoop.getStep());
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
  auto newK = scf::ForOp::create(b, loc, kLoop.getLowerBound(),
                                 kLoop.getUpperBound(), kLoop.getStep());
  metadataManager.rewriteLoopMetadata(kLoop.getOperation(), newK.getOperation(),
                                      clearReductionLoopFacts);

  b.setInsertionPointToStart(newK.getBody());
  Value kIV = newK.getInductionVar();

  /// Load A[i,k]
  Value aLoad;
  if (m.aIsKI)
    aLoad = memref::LoadOp::create(b, loc, m.memA, ValueRange{kIV, iIndex});
  else
    aLoad = memref::LoadOp::create(b, loc, m.memA, ValueRange{iIndex, kIV});

  Value aAlpha;
  if (ValueAnalysis::isOneConstant(alpha))
    aAlpha = aLoad;
  else
    aAlpha = createMul(b, loc, aLoad, alpha, kind);

  Operation *aAlphaOp = aAlpha.getDefiningOp();
  if (!aAlphaOp)
    return;

  auto emitUpdateBody = [&](OpBuilder &ub, Location uloc, Value jIV) {
    Value bVal;
    if (m.bIsKJ)
      bVal = memref::LoadOp::create(ub, uloc, m.memB, ValueRange{kIV, jIV});
    else
      bVal = memref::LoadOp::create(ub, uloc, m.memB, ValueRange{jIV, kIV});

    auto cOld =
        memref::LoadOp::create(ub, uloc, m.memC, ValueRange{iIndex, jIV});
    Value prod = createMul(ub, uloc, aAlpha, bVal, kind);
    Value cNew = createAdd(ub, uloc, cOld, prod, kind);
    memref::StoreOp::create(ub, uloc, cNew, m.memC, ValueRange{iIndex, jIV});
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
            metadataManager.rewriteLoopMetadata(jLoop.getOperation(),
                                                innerJ.getOperation(),
                                                clearReductionLoopFacts);
            break;
          }
        }
        createdTiledUpdate = true;
      }
    }

    if (!createdTiledUpdate) {
      auto newJ = scf::ForOp::create(b, loc, jLoop.getLowerBound(),
                                     jLoop.getUpperBound(), jLoop.getStep());
      metadataManager.rewriteLoopMetadata(
          jLoop.getOperation(), newJ.getOperation(), clearReductionLoopFacts);
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

class MatmulReductionPattern : public KernelPatternTransform {
public:
  MatmulReductionPattern(bool enableTiling, int64_t tileJ, int64_t minTripCount,
                         MetadataManager &metadataManager)
      : enableTiling(enableTiling), tileJ(tileJ), minTripCount(minTripCount),
        metadataManager(metadataManager) {}

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
    rewriteReductionDotToKJUpdate(matchResult, effectiveTileJ, minTripCount,
                                  metadataManager);

    /// Stamp matmul contract on the transformed loop and parent EDT.
    /// This is a lightweight marker-only contract - see MatmulPatternContract
    /// documentation for why we don't carry tiling/layout metadata.
    MatmulPatternContract contract(getRevision());
    contract.stamp(matchResult.outerI.getOperation());
    if (auto parentEdt = matchResult.outerI->getParentOfType<EdtOp>())
      contract.stamp(parentEdt.getOperation());

    return success();
  }

  StringRef getName() const override { return "matmul-reduction"; }
  ArtsDepPattern getFamily() const override { return ArtsDepPattern::matmul; }
  int64_t getRevision() const override { return 1; }

private:
  bool enableTiling;
  int64_t tileJ;
  int64_t minTripCount;
  MetadataManager &metadataManager;
  ReductionDotMatch matchResult;
};

std::unique_ptr<KernelPatternTransform>
createMatmulReductionPattern(bool enableTiling, int64_t tileJ,
                             int64_t minTripCount,
                             MetadataManager &metadataManager) {
  return std::make_unique<MatmulReductionPattern>(
      enableTiling, tileJ, minTripCount, metadataManager);
}

} // namespace arts
} // namespace mlir
