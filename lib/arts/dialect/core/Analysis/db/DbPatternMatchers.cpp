///==========================================================================///
/// File: DbPatternMatchers.cpp
///
/// Shared DB-oriented loop pattern matchers used by analysis passes.
///==========================================================================///

#include "arts/dialect/core/Analysis/db/DbPatternMatchers.h"
#include "arts/dialect/core/Analysis/db/DbAnalysis.h"
#include "arts/dialect/core/Analysis/loop/LoopAnalysis.h"
#include "arts/utils/ValueAnalysis.h"
#include "arts/utils/DbUtils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Check if two stores write the same value to the same memref with
/// swapped 2D indices: one stores at [outerIV, innerIV] and the other at
/// [innerIV, outerIV].
static bool areSymmetricStores(memref::StoreOp s1, memref::StoreOp s2,
                               Value outerIV, Value innerIV) {
  if (s1.getValueToStore() != s2.getValueToStore())
    return false;

  if (!DbAnalysis::isSameMemoryObject(s1.getMemRef(), s2.getMemRef()))
    return false;

  auto idx1 = s1.getIndices();
  auto idx2 = s2.getIndices();
  if (idx1.size() != 2 || idx2.size() != 2)
    return false;

  bool s1IsIJ = (idx1[0] == outerIV && idx1[1] == innerIV);
  bool s2IsJI = (idx2[0] == innerIV && idx2[1] == outerIV);
  if (s1IsIJ && s2IsJI)
    return true;

  bool s1IsJI = (idx1[0] == innerIV && idx1[1] == outerIV);
  bool s2IsIJ = (idx2[0] == outerIV && idx2[1] == innerIV);
  return s1IsJI && s2IsIJ;
}

/// Check if a store writes to the diagonal: memref[iv, iv].
static bool isDiagonalStore(memref::StoreOp store, Value iv) {
  auto indices = store.getIndices();
  return indices.size() == 2 && indices[0] == iv && indices[1] == iv;
}

/// Count distinct memory objects under isSameMemoryObject semantics.
static unsigned countDistinctMemoryObjects(ArrayRef<Value> memrefs) {
  SmallVector<Value, 4> distinct;
  for (Value memref : memrefs) {
    bool seen = false;
    for (Value existing : distinct) {
      if (DbAnalysis::isSameMemoryObject(existing, memref)) {
        seen = true;
        break;
      }
    }
    if (!seen)
      distinct.push_back(memref);
  }
  return distinct.size();
}

static bool containsSameMemoryObject(ArrayRef<Value> memrefs, Value target) {
  for (Value memref : memrefs) {
    if (DbAnalysis::isSameMemoryObject(memref, target))
      return true;
  }
  return false;
}

static bool hasMatrixLikeStoreIndexing(memref::StoreOp store, ForOp rootFor,
                                       LoopAnalysis *loopAnalysis) {
  if (!loopAnalysis)
    return false;

  SmallVector<LoopNode *> enclosingLoops;
  loopAnalysis->collectEnclosingLoops(store, enclosingLoops);
  if (enclosingLoops.empty())
    return false;

  DenseSet<Operation *> dependentLoops;
  for (LoopNode *loopNode : enclosingLoops) {
    if (!loopNode)
      continue;
    Operation *loopOp = loopNode->getLoopOp();
    if (!loopOp)
      continue;
    if (loopOp != rootFor.getOperation() && !rootFor->isAncestor(loopOp))
      continue;

    for (Value index : store.getIndices()) {
      if (loopNode->dependsOnInductionVarNormalized(index)) {
        dependentLoops.insert(loopOp);
        break;
      }
    }
  }

  return dependentLoops.size() >= 2;
}

static void collectLoadedMemrefs(Value value, DenseSet<Value> &loadedMemrefs,
                                 DenseSet<Operation *> &visited) {
  Operation *def = value.getDefiningOp();
  if (!def || !visited.insert(def).second)
    return;

  value = ValueAnalysis::stripNumericCasts(value);
  if (Operation *strippedDef = value.getDefiningOp())
    def = strippedDef;

  if (auto load = dyn_cast<memref::LoadOp>(def))
    loadedMemrefs.insert(load.getMemRef());
  else if (auto load = dyn_cast<affine::AffineLoadOp>(def))
    loadedMemrefs.insert(load.getMemRef());

  for (Value operand : def->getOperands())
    collectLoadedMemrefs(operand, loadedMemrefs, visited);
}

static bool isAccumulatorTerm(Value value, Value storeMemref) {
  DenseSet<Value> loadedMemrefSet;
  DenseSet<Operation *> visited;
  collectLoadedMemrefs(value, loadedMemrefSet, visited);

  SmallVector<Value, 4> loadedMemrefs(loadedMemrefSet.begin(),
                                      loadedMemrefSet.end());
  return countDistinctMemoryObjects(loadedMemrefs) == 1 &&
         containsSameMemoryObject(loadedMemrefs, storeMemref);
}

static bool isTwoInputProductTerm(Value value, Value storeMemref) {
  value = ValueAnalysis::stripNumericCasts(value);
  Operation *def = value.getDefiningOp();
  if (!def)
    return false;

  Value lhs;
  Value rhs;
  if (auto mul = dyn_cast<arith::MulFOp>(def)) {
    lhs = mul.getLhs();
    rhs = mul.getRhs();
  } else if (auto mul = dyn_cast<arith::MulIOp>(def)) {
    lhs = mul.getLhs();
    rhs = mul.getRhs();
  } else {
    return false;
  }

  DenseSet<Value> lhsMemrefSet;
  DenseSet<Value> rhsMemrefSet;
  DenseSet<Operation *> visitedLhs;
  DenseSet<Operation *> visitedRhs;
  collectLoadedMemrefs(lhs, lhsMemrefSet, visitedLhs);
  collectLoadedMemrefs(rhs, rhsMemrefSet, visitedRhs);

  SmallVector<Value, 4> lhsMemrefs(lhsMemrefSet.begin(), lhsMemrefSet.end());
  SmallVector<Value, 4> rhsMemrefs(rhsMemrefSet.begin(), rhsMemrefSet.end());
  if (countDistinctMemoryObjects(lhsMemrefs) != 1 ||
      countDistinctMemoryObjects(rhsMemrefs) != 1)
    return false;

  Value lhsMemref = lhsMemrefs.front();
  Value rhsMemref = rhsMemrefs.front();
  if (DbAnalysis::isSameMemoryObject(lhsMemref, rhsMemref))
    return false;
  if (DbAnalysis::isSameMemoryObject(lhsMemref, storeMemref) ||
      DbAnalysis::isSameMemoryObject(rhsMemref, storeMemref))
    return false;

  return true;
}

} // namespace

std::optional<int64_t> mlir::arts::matchTriangularOffset(Value lb,
                                                         Value outerIV) {
  if (!lb || !outerIV)
    return std::nullopt;

  Value stripped = ValueAnalysis::stripNumericCasts(lb);
  auto addOp = stripped.getDefiningOp<arith::AddIOp>();
  if (!addOp)
    return std::nullopt;

  Value lhs = addOp.getLhs();
  Value rhs = addOp.getRhs();
  Value constOperand;
  if (ValueAnalysis::sameValue(lhs, outerIV))
    constOperand = rhs;
  else if (ValueAnalysis::sameValue(rhs, outerIV))
    constOperand = lhs;
  else
    return std::nullopt;

  auto val = ValueAnalysis::getConstantValue(
      ValueAnalysis::stripNumericCasts(constOperand));
  return (val && *val > 0) ? val : std::nullopt;
}

bool mlir::arts::hasTriangularBoundPattern(ForOp forOp) {
  if (!forOp || forOp.getBody()->getNumArguments() == 0)
    return false;

  Value outerIV = forOp.getBody()->getArgument(0);
  bool hasTriangularBound = false;
  forOp.walk([&](scf::ForOp nestedFor) {
    if (ValueAnalysis::dependsOn(nestedFor.getLowerBound(), outerIV) ||
        ValueAnalysis::dependsOn(nestedFor.getUpperBound(), outerIV)) {
      hasTriangularBound = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return hasTriangularBound;
}

bool mlir::arts::detectMatmulUpdatePattern(ForOp forOp,
                                           LoopAnalysis *loopAnalysis) {
  bool hasMatmulUpdate = false;
  forOp.walk([&](memref::StoreOp store) {
    Value storeValue =
        ValueAnalysis::stripNumericCasts(store.getValueToStore());
    Operation *storeValueDef = storeValue.getDefiningOp();
    if (!storeValueDef)
      return WalkResult::advance();

    Value lhs;
    Value rhs;
    if (auto add = dyn_cast<arith::AddFOp>(storeValueDef)) {
      lhs = add.getLhs();
      rhs = add.getRhs();
    } else if (auto add = dyn_cast<arith::AddIOp>(storeValueDef)) {
      lhs = add.getLhs();
      rhs = add.getRhs();
    } else {
      return WalkResult::advance();
    }

    Value storeMemref = store.getMemRef();
    bool matchesUpdateForm = (isAccumulatorTerm(lhs, storeMemref) &&
                              isTwoInputProductTerm(rhs, storeMemref)) ||
                             (isAccumulatorTerm(rhs, storeMemref) &&
                              isTwoInputProductTerm(lhs, storeMemref));

    if (!matchesUpdateForm)
      return WalkResult::advance();

    /// Restrict detection to matrix-like updates (e.g., C[i,j] += A[...] *
    /// B[...]) so GEMV-style loops (ATAX/BiCG) are not misclassified as matmul.
    if (!hasMatrixLikeStoreIndexing(store, forOp, loopAnalysis))
      return WalkResult::advance();

    hasMatmulUpdate = true;
    return WalkResult::interrupt();
  });
  return hasMatmulUpdate;
}

bool mlir::arts::detectMatmulInitReductionLoopNest(
    ForOp artsFor, LoopAnalysis *loopAnalysis,
    MatmulInitReductionLoopMatch &out) {
  out = {};
  if (!artsFor)
    return false;

  if (!detectMatmulUpdatePattern(artsFor, loopAnalysis))
    return false;

  scf::ForOp jLoop = nullptr;
  for (Operation &op : artsFor.getBody()->without_terminator()) {
    if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
      jLoop = forOp;
      break;
    }
  }
  if (!jLoop)
    return false;

  SmallVector<Operation *, 8> initOps;
  scf::ForOp kLoop = nullptr;
  for (Operation &op : jLoop.getBody()->without_terminator()) {
    if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
      kLoop = forOp;
      break;
    }
    initOps.push_back(&op);
  }

  if (initOps.empty() || !kLoop)
    return false;

  bool hasInitEffects = llvm::any_of(
      initOps, [](Operation *op) { return !isMemoryEffectFree(op); });
  if (!hasInitEffects)
    return false;

  out.jLoop = jLoop;
  out.kLoop = kLoop;
  out.initOps = initOps;
  return true;
}

bool mlir::arts::detectSymmetricTriangularPattern(
    ForOp artsFor, SymmetricTriangularPatternMatch &out) {
  out = {};
  if (!artsFor || artsFor.getBody()->getNumArguments() == 0)
    return false;

  Value outerIV = artsFor.getBody()->getArgument(0);

  scf::ForOp jLoop = nullptr;
  for (Operation &op : artsFor.getBody()->without_terminator()) {
    if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
      jLoop = forOp;
      break;
    }
  }
  if (!jLoop)
    return false;

  auto triangularOffset = matchTriangularOffset(jLoop.getLowerBound(), outerIV);
  if (!triangularOffset)
    return false;

  auto outerUBRange = artsFor.getUpperBound();
  if (outerUBRange.size() != 1)
    return false;
  if (outerUBRange[0] != jLoop.getUpperBound())
    return false;

  int forCount = 0;
  for (Operation &op : artsFor.getBody()->without_terminator())
    if (isa<scf::ForOp>(&op))
      ++forCount;
  if (forCount != 1)
    return false;

  SmallVector<memref::StoreOp> stores;
  for (Operation &op : jLoop.getBody()->without_terminator()) {
    if (auto store = dyn_cast<memref::StoreOp>(&op))
      stores.push_back(store);
  }
  if (stores.size() != 2)
    return false;

  Value jIV = jLoop.getInductionVar();
  if (!areSymmetricStores(stores[0], stores[1], outerIV, jIV))
    return false;

  auto idx0 = stores[0].getIndices();
  if (idx0[0] == outerIV && idx0[1] == jIV) {
    out.storeIJ = stores[0];
    out.storeJI = stores[1];
  } else {
    out.storeIJ = stores[1];
    out.storeJI = stores[0];
  }

  out.memC = stores[0].getMemRef();
  for (Operation &op : artsFor.getBody()->without_terminator()) {
    auto store = dyn_cast<memref::StoreOp>(&op);
    if (!store)
      continue;
    if (!DbAnalysis::isSameMemoryObject(store.getMemRef(), out.memC))
      continue;
    if (!isDiagonalStore(store, outerIV))
      continue;
    out.diagStore = store;
    out.diagValue = store.getValueToStore();
    break;
  }
  if (!out.diagStore)
    return false;

  out.jLoop = jLoop;
  out.triangularOffset = *triangularOffset;
  return true;
}
