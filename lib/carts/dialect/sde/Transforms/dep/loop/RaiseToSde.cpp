///==========================================================================///
/// File: RaiseToSde.cpp
///
/// SDE raise-to-sde CORE (partial Step 11): raises proven-independent
/// sequential `scf.for` nests into bare `sde.su_iterate` + `sde.cu_region`
/// skeletons via `buildSuIterate` / `buildCuRegion` with zero optional attrs.
///
/// Per-axis split: a contiguous parallel outer prefix becomes an N-D
/// `su_iterate` domain; dependence-carrying inner axes stay as `scf.for`
/// inside the proof-derived `cu_region<parallel>`. Reduction loops are
/// admitted only with a reassociation license (integer add/mul or fast-math
/// reassoc on float); plain sequential float `+=` fails closed.
///
/// Subsumes the parallel-promotion slice of `sde-parallelize` and will
/// eventually fold the initial `sde-cu-normalization` pipeline stage (the
/// post-realization re-normalization remains a separate pass). Re-entrancy guard: skip loop nests
/// already under `sde.su_iterate`; allow direct children of residual
/// `cu_region<single>` wrappers or raw host `scf.for` at function scope.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_RAISETOSDE
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SdeCuNormalizationUtils.h"
#include "carts/dialect/sde/Utils/SdeCuStructure.h"
#include "carts/utils/Debug.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/Matchers.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallBitVector.h"

ARTS_DEBUG_SETUP(sde_raise_to_sde);

using namespace mlir;
using namespace mlir::carts;

namespace {

struct Counter {
  Value mem;
  int64_t init;
  int64_t step;
  Type elemType;
};

struct ParallelNest {
  SmallVector<scf::ForOp, 4> loops;
  SmallVector<int64_t, 4> extents;
  SmallVector<memref::StoreOp, 4> arrayStores;
  SmallVector<Value, 4> roots;
  SmallVector<Counter, 2> counters;
  sde::SdeCuRegionOp enclosingSingleCu;
  unsigned parallelPrefix = 0;
};

static bool isPureScalarOp(Operation *op);

static bool collectPerfectForChain(scf::ForOp outer,
                                   SmallVectorImpl<scf::ForOp> &loops) {
  scf::ForOp cur = outer;
  while (true) {
    loops.push_back(cur);
    Block &body = cur.getRegion().front();
    scf::ForOp next;
    for (Operation &op : body.without_terminator()) {
      if (auto nested = dyn_cast<scf::ForOp>(op)) {
        if (next)
          return true;
        next = nested;
        continue;
      }
      if (!isPureScalarOp(&op))
        return true;
    }
    if (next) {
      cur = next;
      continue;
    }
    return true;
  }
}

static std::optional<int64_t> constantTrip(scf::ForOp loop) {
  int64_t lb, ub, step;
  if (!ValueAnalysis::getConstantIndex(loop.getLowerBound(), lb) ||
      !ValueAnalysis::getConstantIndex(loop.getUpperBound(), ub) ||
      !ValueAnalysis::getConstantIndex(loop.getStep(), step))
    return std::nullopt;
  if (lb != 0 || step != 1 || ub <= 0)
    return std::nullopt;
  return ub;
}

static bool isPureScalarOp(Operation *op) {
  if (op->getDialect() && (op->getDialect()->getNamespace() == "arith" ||
                           op->getDialect()->getNamespace() == "math"))
    return true;
  if (isa<scf::IfOp>(op) && isMemoryEffectFree(op))
    return true;
  return false;
}

static std::optional<int64_t> matchConstInt(Value v) {
  IntegerAttr attr;
  if (matchPattern(v, m_Constant(&attr)))
    return attr.getInt();
  return std::nullopt;
}

static bool isScalarScratch(Value root) {
  auto def = root.getDefiningOp();
  if (!isa_and_nonnull<memref::AllocaOp, memref::AllocOp>(def))
    return false;
  auto mt = dyn_cast<MemRefType>(root.getType());
  return mt && mt.getRank() == 0;
}

static bool rootIn(Value root, ArrayRef<Value> roots) {
  for (Value candidate : roots)
    if (candidate == root)
      return true;
  return false;
}

static bool indexUsesLoopIv(OperandRange indices, scf::ForOp loop,
                            std::optional<unsigned> &physicalDim) {
  Value iv = loop.getInductionVar();
  for (auto [dim, rawIndex] : llvm::enumerate(indices)) {
    int64_t offset = 0;
    Value index = ValueAnalysis::stripConstantOffset(
        ValueAnalysis::stripNumericCasts(rawIndex), &offset);
    index = ValueAnalysis::stripNumericCasts(index);
    if (offset != 0 || !ValueAnalysis::sameValue(index, iv))
      continue;
    if (physicalDim)
      return false;
    physicalDim = static_cast<unsigned>(dim);
  }
  return physicalDim.has_value();
}

static bool storeUsesLoopIv(memref::StoreOp store, scf::ForOp loop,
                            std::optional<unsigned> &physicalDim) {
  return indexUsesLoopIv(store.getIndices(), loop, physicalDim);
}

static bool storeMatchesParallelPrefix(memref::StoreOp store,
                                       ArrayRef<scf::ForOp> loops,
                                       unsigned parallelPrefix,
                                       SmallVectorImpl<int64_t> &ownerPhysicalDims) {
  llvm::SmallBitVector usedPhysicalDims(store.getIndices().size(), false);
  ownerPhysicalDims.clear();
  ownerPhysicalDims.reserve(parallelPrefix);
  for (unsigned d = 0; d < parallelPrefix; ++d) {
    std::optional<unsigned> physicalDim;
    if (!storeUsesLoopIv(store, loops[d], physicalDim))
      return false;
    if (usedPhysicalDims.test(*physicalDim))
      return false;
    usedPhysicalDims.set(*physicalDim);
    ownerPhysicalDims.push_back(static_cast<int64_t>(*physicalDim));
  }
  return true;
}

static bool hasReassociationLicense(Operation *comb) {
  if (isa<arith::AddIOp, arith::MulIOp>(comb))
    return true;
  if (auto addf = dyn_cast<arith::AddFOp>(comb))
    return arith::bitEnumContainsAny(addf.getFastmath(),
                                     arith::FastMathFlags::reassoc);
  if (auto mulf = dyn_cast<arith::MulFOp>(comb))
    return arith::bitEnumContainsAny(mulf.getFastmath(),
                                     arith::FastMathFlags::reassoc);
  return false;
}

static bool reductionLoopHasLicense(scf::ForOp loop) {
  if (loop.getInitArgs().empty())
    return true;
  Block &body = loop.getRegion().front();
  auto yield = dyn_cast<scf::YieldOp>(body.getTerminator());
  if (!yield || yield.getNumOperands() != loop.getNumResults())
    return false;
  Operation *comb = yield.getOperand(0).getDefiningOp();
  return comb && hasReassociationLicense(comb);
}

static std::optional<unsigned>
firstLoopCarriedReadAxis(ArrayRef<scf::ForOp> loops,
                         ArrayRef<Value> writtenRoots) {
  std::optional<unsigned> firstAxis;
  scf::ForOp outer = loops.front();
  outer.walk([&](memref::LoadOp load) {
    Value root = ValueAnalysis::stripMemrefViewOps(load.getMemref());
    if (!rootIn(root, writtenRoots))
      return WalkResult::advance();
    for (unsigned d = 0; d < loops.size(); ++d) {
      std::optional<unsigned> ignored;
      if (!indexUsesLoopIv(load.getIndices(), loops[d], ignored))
        continue;
      if (!firstAxis || d < *firstAxis)
        firstAxis = d;
    }
    return WalkResult::advance();
  });
  return firstAxis;
}

static std::optional<unsigned>
firstSerialAxis(ArrayRef<scf::ForOp> loops, ArrayRef<Value> writtenRoots) {
  std::optional<unsigned> firstAxis;
  for (unsigned d = 0; d < loops.size(); ++d) {
    scf::ForOp loop = loops[d];
    if (!loop.getInitArgs().empty())
      firstAxis = firstAxis ? std::min(*firstAxis, d) : d;
  }
  if (std::optional<unsigned> readAxis = firstLoopCarriedReadAxis(loops, writtenRoots)) {
    if (!firstAxis || *readAxis < *firstAxis)
      firstAxis = *readAxis;
  }
  return firstAxis;
}

static bool nestBodyIsSupported(scf::ForOp outer,
                                ArrayRef<scf::ForOp> loops) {
  llvm::SmallDenseSet<Operation *> loopOps;
  for (scf::ForOp loop : loops)
    loopOps.insert(loop.getOperation());

  bool rejected = false;
  outer.walk([&](Operation *op) {
    if (rejected)
      return WalkResult::interrupt();
    if (loopOps.contains(op))
      return WalkResult::advance();
    if (isa<memref::StoreOp, memref::LoadOp, scf::YieldOp>(op))
      return WalkResult::advance();
    if (isa<scf::IfOp>(op) || isPureScalarOp(op) ||
        sde::isKnownPureScalarLibmCall(op) || isMemoryEffectFree(op))
      return WalkResult::advance();
    rejected = true;
    return WalkResult::interrupt();
  });
  return !rejected;
}

static std::optional<int64_t> findCounterInit(Value mem, scf::ForOp outer) {
  Block *parent = outer->getBlock();
  Value lastStored;
  for (Operation &op : *parent) {
    if (&op == outer.getOperation())
      break;
    if (auto st = dyn_cast<memref::StoreOp>(op))
      if (ValueAnalysis::stripMemrefViewOps(st.getMemref()) == mem)
        lastStored = st.getValue();
  }
  if (!lastStored)
    return std::nullopt;
  return matchConstInt(lastStored);
}

static bool counterUsesAreLoopLocal(Value mem, scf::ForOp outer) {
  Block *parent = outer->getBlock();
  for (Operation *user : mem.getUsers()) {
    if (outer->isAncestor(user))
      continue;
    if (user->getBlock() != parent || !user->isBeforeInBlock(outer))
      return false;
    auto store = dyn_cast<memref::StoreOp>(user);
    if (!store || ValueAnalysis::stripMemrefViewOps(store.getMemref()) != mem)
      return false;
  }
  return true;
}

/// Re-entrancy guard: skip nests already under `su_iterate`. Allow direct
/// children of residual `cu_region<single>` or raw host loops at function scope.
static bool nestHasReentrantAncestor(scf::ForOp outer) {
  if (outer->getParentOfType<sde::SdeSuIterateOp>())
    return true;
  if (auto cu = outer->getParentOfType<sde::SdeCuRegionOp>()) {
    if (outer->getParentOp() == cu.getOperation() &&
        cu.getKind() == sde::SdeCuKind::single && cu.getIterArgs().empty())
      return false;
    return true;
  }
  return false;
}

static std::optional<ParallelNest> matchParallelNest(scf::ForOp outer) {
  if (nestHasReentrantAncestor(outer))
    return std::nullopt;
  if (isa<scf::ForOp>(outer->getParentOp()))
    return std::nullopt;

  ParallelNest nest;
  if (auto cu = outer->getParentOfType<sde::SdeCuRegionOp>()) {
    if (outer->getParentOp() != cu.getOperation() ||
        cu.getKind() != sde::SdeCuKind::single || !cu.getIterArgs().empty())
      return std::nullopt;
    nest.enclosingSingleCu = cu;
  }

  collectPerfectForChain(outer, nest.loops);
  if (nest.loops.size() < 1)
    return std::nullopt;
  for (scf::ForOp loop : nest.loops) {
    if (loop.getNumResults() != 0 && loop.getInitArgs().empty())
      return std::nullopt;
    std::optional<int64_t> trip = constantTrip(loop);
    if (!trip)
      return std::nullopt;
    nest.extents.push_back(*trip);
  }

  for (scf::ForOp loop : nest.loops) {
    if (!loop.getInitArgs().empty() && !reductionLoopHasLicense(loop))
      return std::nullopt;
  }

  if (!nestBodyIsSupported(outer, nest.loops))
    return std::nullopt;

  SmallVector<memref::StoreOp, 4> stores;
  SmallVector<memref::LoadOp, 8> loads;
  outer.walk([&](Operation *op) {
    if (auto st = dyn_cast<memref::StoreOp>(op)) {
      stores.push_back(st);
      return WalkResult::advance();
    }
    if (auto ld = dyn_cast<memref::LoadOp>(op)) {
      loads.push_back(ld);
      return WalkResult::advance();
    }
    return WalkResult::advance();
  });

  llvm::SmallDenseMap<Value, int64_t> counterStep;
  SmallVector<Value, 4> writtenRoots;
  for (memref::StoreOp st : stores) {
    Value r = ValueAnalysis::stripMemrefViewOps(st.getMemref());
    if (isScalarScratch(r)) {
      auto add = st.getValue().getDefiningOp<arith::AddIOp>();
      if (!add)
        return std::nullopt;
      std::optional<int64_t> kc;
      if (auto ld = add.getLhs().getDefiningOp<memref::LoadOp>();
          ld && ValueAnalysis::stripMemrefViewOps(ld.getMemref()) == r) {
        kc = matchConstInt(add.getRhs());
      } else if (auto ld2 = add.getRhs().getDefiningOp<memref::LoadOp>();
                 ld2 &&
                 ValueAnalysis::stripMemrefViewOps(ld2.getMemref()) == r) {
        kc = matchConstInt(add.getLhs());
      }
      if (!kc)
        return std::nullopt;
      if (counterStep.count(r))
        return std::nullopt;
      counterStep[r] = *kc;
      continue;
    }

    if (!r || sde::isDefinedInside(outer.getOperation(), r))
      return std::nullopt;

    auto memRefType = dyn_cast<MemRefType>(r.getType());
    if (!memRefType || memRefType.getRank() == 0)
      return std::nullopt;
    for (int64_t dim : memRefType.getShape()) {
      if (dim == ShapedType::kDynamic)
        return std::nullopt;
    }

    if (!rootIn(r, writtenRoots))
      writtenRoots.push_back(r);
  }

  std::optional<unsigned> firstSerial =
      firstSerialAxis(nest.loops, writtenRoots);
  nest.parallelPrefix =
      firstSerial ? *firstSerial : static_cast<unsigned>(nest.loops.size());
  if (nest.parallelPrefix == 0)
    return std::nullopt;
  // Fail closed on partial prefix until move-raise of serial suffixes (especially
  // result-bearing scf.for loops) is stable end-to-end.
  if (nest.parallelPrefix < nest.loops.size())
    return std::nullopt;

  std::optional<SmallVector<int64_t, 4>> selectedOwnerDims;
  std::optional<SmallVector<int64_t, 4>> selectedShape;
  for (memref::StoreOp st : stores) {
    Value r = ValueAnalysis::stripMemrefViewOps(st.getMemref());
    if (isScalarScratch(r))
      continue;

    SmallVector<int64_t, 4> ownerPhysicalDims;
    if (!storeMatchesParallelPrefix(st, nest.loops, nest.parallelPrefix,
                                    ownerPhysicalDims))
      return std::nullopt;

    auto memRefType = cast<MemRefType>(r.getType());
    SmallVector<int64_t, 4> shape;
    shape.reserve(memRefType.getRank());
    for (int64_t dim : memRefType.getShape())
      shape.push_back(dim);

    if (!selectedOwnerDims) {
      selectedOwnerDims = ownerPhysicalDims;
      selectedShape = shape;
    } else if (*selectedOwnerDims != ownerPhysicalDims ||
               *selectedShape != shape) {
      return std::nullopt;
    }

    nest.arrayStores.push_back(st);
    if (!rootIn(r, nest.roots))
      nest.roots.push_back(r);
  }
  if (nest.arrayStores.empty())
    return std::nullopt;

  for (auto &kv : counterStep) {
    std::optional<int64_t> init = findCounterInit(kv.first, outer);
    if (!init)
      return std::nullopt;
    if (!counterUsesAreLoopLocal(kv.first, outer))
      return std::nullopt;
    auto mt = dyn_cast<MemRefType>(kv.first.getType());
    if (!mt)
      return std::nullopt;
    nest.counters.push_back({kv.first, *init, kv.second, mt.getElementType()});
  }
  auto isCounter = [&](Value r) {
    for (const Counter &c : nest.counters)
      if (c.mem == r)
        return true;
    return false;
  };
  for (memref::LoadOp ld : loads) {
    Value r = ValueAnalysis::stripMemrefViewOps(ld.getMemref());
    if (!isCounter(r))
      if (!r || isScalarScratch(r) || rootIn(r, nest.roots) ||
          sde::isDefinedInside(outer.getOperation(), r))
        return std::nullopt;
  }

  return nest;
}

static int64_t linearStride(ArrayRef<int64_t> extents, unsigned d) {
  int64_t s = 1;
  for (unsigned k = d + 1; k < extents.size(); ++k)
    s *= extents[k];
  return s;
}

static void collectEscapingValues(ArrayRef<Operation *> span, Block *block,
                                  SmallVectorImpl<Value> &escaping) {
  llvm::DenseSet<Operation *> spanSet(span.begin(), span.end());
  for (Operation *op : span) {
    for (Value result : op->getResults()) {
      for (Operation *user : result.getUsers()) {
        Operation *ancestor = sde::getBlockLevelAncestor(user, block);
        if (!ancestor || !spanSet.contains(ancestor)) {
          escaping.push_back(result);
          break;
        }
      }
    }
  }
}

static bool spanHasSourceCompute(ArrayRef<Operation *> span) {
  return llvm::any_of(span,
                      [](Operation *op) { return sde::isSourceComputeOp(op); });
}

static void moveSpanBeforeCu(ArrayRef<Operation *> span,
                             sde::SdeCuRegionOp cu) {
  if (span.empty())
    return;
  Block *sourceBlock = span.front()->getBlock();
  cu->getBlock()->getOperations().splice(
      Block::iterator(cu.getOperation()), sourceBlock->getOperations(),
      span.front()->getIterator(), std::next(span.back()->getIterator()));
}

static sde::SdeCuRegionOp moveSpanToSiblingCu(Operation *first,
                                              Operation *last) {
  Block *block = first->getBlock();
  SmallVector<Operation *> span;
  for (Operation *op = first;; op = &*std::next(op->getIterator())) {
    span.push_back(op);
    if (op == last)
      break;
  }

  OpBuilder constantBuilder(first->getContext());
  constantBuilder.setInsertionPoint(first->getParentOp());
  sde::materializeEscapingConstantLikeValues(span, block, constantBuilder);

  SmallVector<Value> escaping;
  collectEscapingValues(span, block, escaping);
  SmallVector<Type> resultTypes;
  for (Value value : escaping)
    resultTypes.push_back(value.getType());

  OpBuilder builder(first->getParentOp());
  builder.setInsertionPoint(first->getParentOp());
  auto siblingCu = sde::buildCuRegion(
      builder, first->getLoc(),
      sde::SdeCuKindAttr::get(builder.getContext(), sde::SdeCuKind::single),
      /*nowait=*/nullptr, /*iterArgs=*/ValueRange{}, resultTypes);
  siblingCu.setSerialReasonAttr(sde::SdeSerialReasonAttr::get(
      builder.getContext(), sde::SdeSerialReason::residual_source));
  Block &body = sde::ensureBlock(siblingCu.getBody());
  body.getOperations().splice(body.end(), block->getOperations(),
                              first->getIterator(),
                              std::next(last->getIterator()));
  OpBuilder yieldBuilder = OpBuilder::atBlockEnd(&body);
  sde::SdeYieldOp::create(yieldBuilder, first->getLoc(), escaping);

  for (auto [oldValue, newValue] :
       llvm::zip(escaping, siblingCu->getResults())) {
    oldValue.replaceUsesWithIf(newValue, [&](OpOperand &use) {
      return !sde::isNestedUnder(use.getOwner(), siblingCu);
    });
  }
  return siblingCu;
}

static sde::SdeCuRegionOp wrapSpanInPlaceInCu(Operation *first,
                                              Operation *last) {
  Block *block = first->getBlock();
  SmallVector<Operation *> span;
  for (Operation *op = first;; op = &*std::next(op->getIterator())) {
    span.push_back(op);
    if (op == last)
      break;
  }

  OpBuilder constantBuilder(first);
  sde::materializeEscapingConstantLikeValues(span, block, constantBuilder);

  SmallVector<Value> escaping;
  collectEscapingValues(span, block, escaping);
  SmallVector<Type> resultTypes;
  for (Value value : escaping)
    resultTypes.push_back(value.getType());

  OpBuilder builder(first);
  auto cu = sde::buildCuRegion(
      builder, first->getLoc(),
      sde::SdeCuKindAttr::get(builder.getContext(), sde::SdeCuKind::single),
      /*nowait=*/nullptr, /*iterArgs=*/ValueRange{}, resultTypes);
  cu.setSerialReasonAttr(sde::SdeSerialReasonAttr::get(
      builder.getContext(), sde::SdeSerialReason::residual_source));
  Block &body = sde::ensureBlock(cu.getBody());
  body.getOperations().splice(body.end(), block->getOperations(),
                              first->getIterator(),
                              std::next(last->getIterator()));
  OpBuilder yieldBuilder = OpBuilder::atBlockEnd(&body);
  sde::SdeYieldOp::create(yieldBuilder, first->getLoc(), escaping);

  for (auto [oldValue, newValue] : llvm::zip(escaping, cu->getResults())) {
    oldValue.replaceUsesWithIf(newValue, [&](OpOperand &use) {
      return !sde::isNestedUnder(use.getOwner(), cu);
    });
  }
  return cu;
}

static void normalizeSchedulingCarrierRegions(Operation *op);

static void normalizeSchedulingCarrierBlock(Block *block) {
  SmallVector<Operation *> ops;
  for (Operation &op : block->without_terminator())
    ops.push_back(&op);

  size_t i = 0, n = ops.size();
  while (i < n) {
    Operation *op = ops[i];
    if (sde::isSdeDialectOp(op)) {
      ++i;
      continue;
    }
    if (sde::containsCuForbiddenSchedulingOp(op)) {
      normalizeSchedulingCarrierRegions(op);
      ++i;
      continue;
    }

    size_t runEnd = i;
    while (runEnd < n && !sde::isSdeDialectOp(ops[runEnd]) &&
           !sde::containsCuForbiddenSchedulingOp(ops[runEnd]))
      ++runEnd;

    size_t lo = i, hi = runEnd;
    while (lo < hi && !sde::isSourceComputeOp(ops[lo]))
      ++lo;
    while (hi > lo && !sde::isSourceComputeOp(ops[hi - 1]))
      --hi;
    if (lo < hi)
      wrapSpanInPlaceInCu(ops[lo], ops[hi - 1]);
    i = runEnd;
  }
}

static void normalizeSchedulingCarrierRegions(Operation *op) {
  for (Region &region : op->getRegions())
    for (Block &block : region)
      normalizeSchedulingCarrierBlock(&block);
}

static void splitSingleCuAroundSchedulingOps(sde::SdeCuRegionOp cu) {
  if (!cu || cu.getKind() != sde::SdeCuKind::single)
    return;
  if (!cu.getIterArgs().empty() || cu.getBody().empty())
    return;

  Block &body = cu.getBody().front();
  bool hasScheduling = false;
  for (Operation &op : body.without_terminator()) {
    if (sde::isCuSchedulingBoundary(&op)) {
      hasScheduling = true;
      break;
    }
  }
  if (!hasScheduling)
    return;

  while (!body.without_terminator().empty()) {
    Operation *first = &body.front();
    if (sde::isCuSchedulingBoundary(first)) {
      if (!sde::isCuForbiddenSchedulingOp(first))
        normalizeSchedulingCarrierRegions(first);
      cu->getBlock()->getOperations().splice(Block::iterator(cu.getOperation()),
                                             body.getOperations(),
                                             first->getIterator());
      continue;
    }

    Operation *last = first;
    for (Operation &candidate : llvm::make_early_inc_range(
             llvm::drop_begin(body.without_terminator()))) {
      if (sde::isCuSchedulingBoundary(&candidate))
        break;
      last = &candidate;
    }
    SmallVector<Operation *> span;
    for (Operation *op = first;; op = &*std::next(op->getIterator())) {
      span.push_back(op);
      if (op == last)
        break;
    }
    if (spanHasSourceCompute(span))
      moveSpanToSiblingCu(first, last);
    else
      moveSpanBeforeCu(span, cu);
  }

  SmallVector<Value> yielded;
  if (auto yield = dyn_cast_or_null<sde::SdeYieldOp>(body.getTerminator()))
    llvm::append_range(yielded, yield.getValues());
  for (auto [oldResult, yieldedValue] : llvm::zip(cu->getResults(), yielded))
    oldResult.replaceAllUsesWith(yieldedValue);
  if (Operation *terminator = body.getTerminator())
    terminator->erase();
  cu.erase();
}

static sde::SdeSuIterateOp createSuIterateForNest(ParallelNest &nest,
                                                  OpBuilder &builder,
                                                  Operation *insertBefore) {
  scf::ForOp outer = nest.loops.front();
  Location loc = outer.getLoc();
  unsigned parallelPrefix = nest.parallelPrefix;

  if (insertBefore)
    builder.setInsertionPoint(insertBefore);

  SmallVector<Value, 4> lowerBounds;
  SmallVector<Value, 4> upperBounds;
  SmallVector<Value, 4> steps;
  lowerBounds.reserve(parallelPrefix);
  upperBounds.reserve(parallelPrefix);
  steps.reserve(parallelPrefix);
  for (unsigned d = 0; d < parallelPrefix; ++d) {
    lowerBounds.push_back(createConstantIndex(builder, loc, 0));
    upperBounds.push_back(createConstantIndex(builder, loc, nest.extents[d]));
    steps.push_back(createConstantIndex(builder, loc, 1));
  }

  auto suIter = sde::buildSuIterate(builder, loc, lowerBounds, upperBounds, steps);

  Region &dstRegion = suIter.getBody();
  if (dstRegion.empty())
    dstRegion.push_back(new Block());
  Block &dst = dstRegion.front();
  while (dst.getNumArguments() < static_cast<unsigned>(parallelPrefix))
    dst.addArgument(builder.getIndexType(), loc);

  OpBuilder::InsertionGuard ig(builder);
  builder.setInsertionPointToStart(&dst);
  auto cuRegion = sde::buildCuRegion(
      builder, loc,
      sde::SdeCuKindAttr::get(builder.getContext(), sde::SdeCuKind::parallel));
  Block &cuBody = sde::ensureBlock(cuRegion.getBody());
  builder.setInsertionPointToStart(&cuBody);
  IRMapping mapper;
  for (unsigned d = 0; d < parallelPrefix; ++d)
    mapper.map(nest.loops[d].getInductionVar(), dst.getArgument(d));

  Block &innermost = nest.loops.back().getRegion().front();
  for (Operation &op : innermost.without_terminator()) {
    if (isa<scf::ForOp>(op))
      continue;
    builder.clone(op, mapper);
  }
  sde::SdeYieldOp::create(builder, loc, ValueRange{});

  builder.setInsertionPointToEnd(&dst);
  sde::SdeYieldOp::create(builder, loc, ValueRange{});

  return suIter;
}

static void substituteCounters(ParallelNest &nest, sde::SdeSuIterateOp suIter,
                               OpBuilder &builder) {
  scf::ForOp outer = nest.loops.front();
  Location loc = outer.getLoc();
  unsigned n = nest.loops.size();

  SmallVector<Value, 4> dimIv;
  Block &suBody = suIter.getBody().front();
  for (unsigned d = 0; d < nest.parallelPrefix; ++d)
    dimIv.push_back(suBody.getArgument(d));
  {
    Block *cur = sde::getSuIterateComputeBlock(suIter);
    while (dimIv.size() < n) {
      scf::ForOp innerLoop;
      for (Operation &op : *cur)
        if (auto f = dyn_cast<scf::ForOp>(op)) {
          innerLoop = f;
          break;
        }
      if (!innerLoop)
        break;
      dimIv.push_back(innerLoop.getInductionVar());
      cur = &innerLoop.getRegion().front();
    }
  }

  for (const Counter &c : nest.counters) {
    SmallVector<memref::LoadOp, 4> ldToReplace;
    SmallVector<memref::StoreOp, 4> stToErase;
    suIter.getBody().walk([&](Operation *op) {
      if (auto ld = dyn_cast<memref::LoadOp>(op)) {
        if (ValueAnalysis::stripMemrefViewOps(ld.getMemref()) == c.mem)
          ldToReplace.push_back(ld);
      } else if (auto st = dyn_cast<memref::StoreOp>(op)) {
        if (ValueAnalysis::stripMemrefViewOps(st.getMemref()) == c.mem)
          stToErase.push_back(st);
      }
    });
    for (memref::LoadOp ld : ldToReplace) {
      OpBuilder b(ld);
      Value lin = createConstantIndex(b, loc, 0);
      for (unsigned d = 0; d < dimIv.size(); ++d) {
        int64_t s = linearStride(nest.extents, d);
        Value term = dimIv[d];
        if (s != 1)
          term = arith::MulIOp::create(b, loc, term,
                                       createConstantIndex(b, loc, s));
        lin = arith::AddIOp::create(b, loc, lin, term);
      }
      Value scaled =
          c.step == 1 ? lin
                      : arith::MulIOp::create(
                            b, loc, lin, createConstantIndex(b, loc, c.step));
      Value sum =
          c.init == 0
              ? scaled
              : arith::AddIOp::create(b, loc, scaled,
                                      createConstantIndex(b, loc, c.init));
      Value val = sum;
      if (auto it = dyn_cast<IntegerType>(c.elemType))
        val = arith::IndexCastOp::create(b, loc, it, sum);
      ld.getResult().replaceAllUsesWith(val);
      ld.erase();
    }
    for (memref::StoreOp st : stToErase)
      st.erase();
  }
}

static void raiseNest(ParallelNest &nest, OpBuilder &builder) {
  scf::ForOp outer = nest.loops.front();

  if (nest.enclosingSingleCu) {
    sde::SdeSuIterateOp suIter =
        createSuIterateForNest(nest, builder, outer.getOperation());
    substituteCounters(nest, suIter, builder);
    outer.erase();
    splitSingleCuAroundSchedulingOps(nest.enclosingSingleCu);
    return;
  }

  builder.setInsertionPoint(outer);
  sde::SdeSuIterateOp suIter = createSuIterateForNest(nest, builder, nullptr);
  substituteCounters(nest, suIter, builder);
  outer.erase();
}

static std::optional<ParallelNest> findNextParallelNest(ModuleOp module) {
  std::optional<ParallelNest> next;
  module.walk([&](scf::ForOp outer) -> WalkResult {
    if (isa<scf::ForOp>(outer->getParentOp()))
      return WalkResult::advance();
    if (std::optional<ParallelNest> nest = matchParallelNest(outer)) {
      next = std::move(*nest);
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return next;
}

struct RaiseToSdePass : public sde::impl::RaiseToSdeBase<RaiseToSdePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    OpBuilder builder(&getContext());
    while (std::optional<ParallelNest> nest = findNextParallelNest(module)) {
      ARTS_DEBUG("raise-to-sde: raising proven-independent host loop nest");
      raiseNest(*nest, builder);
    }
    if (!sde::normalizeSdeCuStructure(module))
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass> createRaiseToSdePass() {
  return std::make_unique<RaiseToSdePass>();
}

} // namespace mlir::carts::sde
