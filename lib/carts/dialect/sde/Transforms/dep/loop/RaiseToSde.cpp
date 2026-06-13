///==========================================================================///
/// File: RaiseToSde.cpp
///
/// SDE raise-to-sde CORE (partial Step 11): raises proven-independent
/// sequential `scf.for` nests into bare `sde.su_iterate` + `sde.cu_region`
/// skeletons via `buildSuIterate` / `buildCuRegion` with zero optional attrs.
///
/// Subsumes the parallel-promotion slice of `sde-parallelize` and will
/// eventually fold `sde-cu-normalization`. Re-entrancy guard: skip loop nests
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

static bool isLoopIndexedStore(memref::StoreOp store,
                               ArrayRef<scf::ForOp> loops,
                               SmallVectorImpl<int64_t> &ownerPhysicalDims) {
  OperandRange indices = store.getIndices();
  if (indices.size() != loops.size())
    return false;

  llvm::SmallBitVector usedPhysicalDims(indices.size(), false);
  ownerPhysicalDims.clear();
  ownerPhysicalDims.reserve(loops.size());
  for (scf::ForOp loop : loops) {
    std::optional<unsigned> selectedPhysicalDim;
    Value iv = loop.getInductionVar();
    for (auto [physicalDim, rawIndex] : llvm::enumerate(indices)) {
      int64_t offset = 0;
      Value index = ValueAnalysis::stripConstantOffset(
          ValueAnalysis::stripNumericCasts(rawIndex), &offset);
      index = ValueAnalysis::stripNumericCasts(index);
      if (offset != 0 || !ValueAnalysis::sameValue(index, iv))
        continue;
      if (selectedPhysicalDim)
        return false;
      selectedPhysicalDim = static_cast<unsigned>(physicalDim);
    }
    if (!selectedPhysicalDim || usedPhysicalDims.test(*selectedPhysicalDim))
      return false;
    usedPhysicalDims.set(*selectedPhysicalDim);
    ownerPhysicalDims.push_back(static_cast<int64_t>(*selectedPhysicalDim));
  }
  return true;
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

static bool rootIn(Value root, ArrayRef<Value> roots) {
  for (Value candidate : roots)
    if (candidate == root)
      return true;
  return false;
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
    if (!loop.getInitArgs().empty() || loop.getNumResults() != 0)
      return std::nullopt;
    std::optional<int64_t> trip = constantTrip(loop);
    if (!trip)
      return std::nullopt;
    nest.extents.push_back(*trip);
  }

  Block &innermost = nest.loops.back().getRegion().front();

  SmallVector<memref::StoreOp, 4> stores;
  SmallVector<memref::LoadOp, 8> loads;
  bool rejected = false;
  innermost.walk([&](Operation *op) {
    if (rejected || op == nest.loops.back().getOperation())
      return;
    if (auto st = dyn_cast<memref::StoreOp>(op)) {
      stores.push_back(st);
      return;
    }
    if (auto ld = dyn_cast<memref::LoadOp>(op)) {
      loads.push_back(ld);
      return;
    }
    if (isa<scf::IfOp, scf::YieldOp>(op) || isPureScalarOp(op) ||
        sde::isKnownPureScalarLibmCall(op) || isMemoryEffectFree(op))
      return;
    rejected = true;
  });
  if (rejected)
    return std::nullopt;

  llvm::SmallDenseMap<Value, int64_t> counterStep;
  std::optional<SmallVector<int64_t, 4>> selectedOwnerDims;
  std::optional<SmallVector<int64_t, 4>> selectedShape;
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
    SmallVector<int64_t, 4> shape;
    shape.reserve(memRefType.getRank());
    for (int64_t dim : memRefType.getShape()) {
      if (dim == ShapedType::kDynamic)
        return std::nullopt;
      shape.push_back(dim);
    }

    SmallVector<int64_t, 4> ownerPhysicalDims;
    if (!isLoopIndexedStore(st, nest.loops, ownerPhysicalDims))
      return std::nullopt;
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

  if (insertBefore)
    builder.setInsertionPoint(insertBefore);
  Value lb = createConstantIndex(builder, loc, 0);
  Value ub = createConstantIndex(builder, loc, nest.extents.front());
  Value one = createConstantIndex(builder, loc, 1);

  auto suIter = sde::buildSuIterate(builder, loc, ValueRange{lb},
                                    ValueRange{ub}, ValueRange{one});

  Region &dstRegion = suIter.getBody();
  if (dstRegion.empty())
    dstRegion.push_back(new Block());
  Block &dst = dstRegion.front();
  if (dst.getNumArguments() == 0)
    dst.addArgument(builder.getIndexType(), loc);
  Value outerIv = dst.getArgument(0);

  OpBuilder::InsertionGuard ig(builder);
  builder.setInsertionPointToStart(&dst);
  auto cuRegion = sde::buildCuRegion(
      builder, loc,
      sde::SdeCuKindAttr::get(builder.getContext(), sde::SdeCuKind::parallel));
  Block &cuBody = sde::ensureBlock(cuRegion.getBody());
  builder.setInsertionPointToStart(&cuBody);
  IRMapping mapper;
  mapper.map(outer.getInductionVar(), outerIv);
  Block &outerBody = outer.getRegion().front();
  for (Operation &op : outerBody.without_terminator())
    builder.clone(op, mapper);
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
  dimIv.push_back(suIter.getBody().front().getArgument(0));
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
  }
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass> createRaiseToSdePass() {
  return std::make_unique<RaiseToSdePass>();
}

} // namespace mlir::carts::sde
