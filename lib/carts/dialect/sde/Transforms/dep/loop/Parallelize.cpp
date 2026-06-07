///==========================================================================///
/// File: Parallelize.cpp
///
/// SDE parallelization of embarrassingly-parallel SEQUENTIAL loop nests.
///
/// ConvertOpenMPToSde only lifts `omp.wsloop`/`scf.parallel` into the SDE
/// scheduling-unit form (`sde.cu_region` + `sde.su_iterate`). A plain
/// `scf.for` nest that is provably parallel (e.g. a host-side array
/// initialization `for i: for j: A[i][j] = f(i,j)`) is left as sequential host
/// code. Downstream that array then gets a COARSE `host_whole` DB plus a
/// `host_whole_to_compute_block` bridge whenever a real distributed kernel also
/// writes it, because LayoutAssignment only sees `sde.su_iterate` scheduling
/// units (a bare `scf.for` store is invisible to it). That coarse funnel is the
/// dominant megalarge anti-scaling cost.
///
/// This pass closes that gap with the three agreed steps fused into one tight,
/// fail-closed transform:
///   1. PARALLELIZE: prove a perfectly-nested rectangular `scf.for` nest is
///      dependence-free. The single array store writes at indices == the loop
///      IVs (distinct per iteration, no WAW); the written array is not read in
///      the body (no cross-iteration RAW/WAR). Any loop-carried scalar must be
///      a recognized LINEAR INDUCTION COUNTER (`int c=init; ...; c+=K;`), which
///      is a closed-form affine function of the iteration index, not a true
///      dependence. The written array must also be written by a sibling
///      `sde.su_iterate` (so it is genuinely block-distributable).
///   2. COLLAPSE: coalesce the parallel dims into one flat iteration dim,
///      delinearizing the original IVs and substituting each counter with
///      `init + K * t` (row-major flat index `t`).
///   3. RAISE TO CU: emit `sde.cu_region<parallel> { sde.su_iterate(0..N) {...}
///   }`
///      identical in shape to a converted `scf.parallel`, so LayoutAssignment
///      now sees a block writer and the array stays block-native (no coarse
///      shadow, no bridge).
///
/// Invariant: if anything is unprovable, the nest is left untouched (local,
/// sequential, exactly as today). A missed opportunity degrades to single-node;
/// it never miscompiles.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_PARALLELIZE
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SdePlanUtils.h"
#include "carts/utils/Debug.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/Matchers.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

#include "llvm/ADT/SmallPtrSet.h"

ARTS_DEBUG_SETUP(sde_parallelize);

using namespace mlir;
using namespace mlir::carts;

namespace {

// A recognized linear induction counter: a rank-0 scalar memref `mem`,
// initialized to `init` before the nest, self-incremented by `step` once per
// innermost iteration. Its value at row-major flat index t is init + step*t.
struct Counter {
  Value mem;
  int64_t init;
  int64_t step;
  Type elemType;
};

// A proven-parallel perfectly-nested rectangular scf.for nest (lb=0/step=1,
// constant trips) whose single array store writes `root` at indices == the IVs,
// plus any recognized linear-counter scalars to substitute.
struct ParallelNest {
  SmallVector<scf::ForOp, 4> loops; // outer..inner
  SmallVector<int64_t, 4> extents;  // per-loop constant trip count
  memref::StoreOp arrayStore;
  Value root; // stripped memref root written by the array store
  SmallVector<Counter, 2> counters;
};

static bool collectPerfectForChain(scf::ForOp outer,
                                   SmallVectorImpl<scf::ForOp> &loops) {
  scf::ForOp cur = outer;
  while (true) {
    loops.push_back(cur);
    Block &body = cur.getRegion().front();
    SmallVector<Operation *, 4> ops;
    for (Operation &op : body.without_terminator())
      ops.push_back(&op);
    if (ops.size() == 1)
      if (auto next = dyn_cast<scf::ForOp>(ops.front())) {
        cur = next;
        continue;
      }
    return true; // innermost reached
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
  return false;
}

static std::optional<int64_t> matchConstInt(Value v) {
  IntegerAttr attr;
  if (matchPattern(v, m_Constant(&attr)))
    return attr.getInt();
  return std::nullopt;
}

// A rank-0 scalar memref alloca used as scratch.
static bool isScalarScratch(Value root) {
  auto def = root.getDefiningOp();
  if (!isa_and_nonnull<memref::AllocaOp, memref::AllocOp>(def))
    return false;
  auto mt = dyn_cast<MemRefType>(root.getType());
  return mt && mt.getRank() == 0;
}

// Find a constant init store to `mem` in the block containing `outer`, before
// `outer`. Returns the init constant if exactly such a store exists.
static std::optional<int64_t> findCounterInit(Value mem, scf::ForOp outer) {
  // The LAST store to `mem` before the loop dominates loop entry; only it
  // matters (earlier stores are overwritten). Require that last store to be a
  // constant so the counter has a closed form at loop entry.
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

static bool hasSiblingSuIterateWriter(ModuleOp module, Value root) {
  bool found = false;
  module.walk([&](sde::SdeSuIterateOp su) {
    if (found)
      return;
    su.getBody().walk([&](memref::StoreOp st) {
      if (found)
        return;
      if (ValueAnalysis::stripMemrefViewOps(st.getMemref()) == root)
        found = true;
    });
  });
  return found;
}

static std::optional<ParallelNest> matchParallelNest(scf::ForOp outer,
                                                     ModuleOp module) {
  if (outer->getParentOfType<sde::SdeSuIterateOp>() ||
      outer->getParentOfType<sde::SdeCuRegionOp>())
    return std::nullopt;
  if (isa<scf::ForOp>(outer->getParentOp()))
    return std::nullopt;

  ParallelNest nest;
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

  llvm::SmallDenseSet<Value, 4> ivSet;
  for (scf::ForOp loop : nest.loops)
    ivSet.insert(loop.getInductionVar());

  Block &innermost = nest.loops.back().getRegion().front();

  // Classify every op in the innermost body. Allowed:
  //  - exactly one store to an EXTERNAL array at indices == IVs (the array
  //  write)
  //  - per counter: a self-increment store to a rank-0 scratch scalar
  //  - loads, pure arith/math, scf.if/yield
  // Reject anything else (calls, unknown effects, extra array stores, etc.).
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
        isMemoryEffectFree(op))
      return;
    rejected = true;
  });
  if (rejected)
    return std::nullopt;

  // Separate the array store from counter increment stores.
  memref::StoreOp arrayStore;
  llvm::SmallDenseMap<Value, int64_t> counterStep; // counter mem -> step
  for (memref::StoreOp st : stores) {
    Value r = ValueAnalysis::stripMemrefViewOps(st.getMemref());
    if (isScalarScratch(r)) {
      // Must be a self-increment: store(addi(load(r), C), r) (or C+load).
      auto add = st.getValue().getDefiningOp<arith::AddIOp>();
      if (!add)
        return std::nullopt;
      Value other;
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
        return std::nullopt; // more than one increment store -> bail
      counterStep[r] = *kc;
      continue;
    }
    // External array store: there must be exactly one.
    if (arrayStore)
      return std::nullopt;
    arrayStore = st;
  }
  if (!arrayStore)
    return std::nullopt;

  nest.arrayStore = arrayStore;
  nest.root = ValueAnalysis::stripMemrefViewOps(arrayStore.getMemref());
  if (!nest.root || isScalarScratch(nest.root))
    return std::nullopt;
  // Array root must be external (defined outside the nest).
  if (sde::isDefinedInside(outer.getOperation(), nest.root))
    return std::nullopt;

  // Array store indices must be exactly the IVs (a permutation, each once).
  OperandRange idx = arrayStore.getIndices();
  if (idx.size() != nest.loops.size())
    return std::nullopt;
  llvm::SmallDenseSet<Value, 4> usedIvs;
  for (Value i : idx) {
    if (!ivSet.contains(i) || usedIvs.contains(i))
      return std::nullopt;
    usedIvs.insert(i);
  }

  // No load may read the written array root (no cross-iteration RAW/WAR).
  // Build the recognized-counter set; loads of counters are fine (substituted).
  for (auto &kv : counterStep) {
    std::optional<int64_t> init = findCounterInit(kv.first, outer);
    if (!init)
      return std::nullopt; // counter without a provable constant init
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
  // Only recognized counter-scalar loads are allowed: the body must be a pure
  // write of a function of the indices (e.g. `A[i][j] = f(i,j)`), with NO array
  // reads. This excludes copies (`u[i][j] = unew[i][j]`), stencils, and any
  // loop that reads another array — those interact with distribution
  // differently (e.g. the double-buffer copy in jacobi-for hits the
  // slice/ownership runtime boundary) and must be left to the normal SDE path.
  // Fail closed.
  for (memref::LoadOp ld : loads) {
    Value r = ValueAnalysis::stripMemrefViewOps(ld.getMemref());
    if (!isCounter(r))
      return std::nullopt;
  }

  if (!hasSiblingSuIterateWriter(module, nest.root))
    return std::nullopt;

  return nest;
}

// Compute the per-dim row-major linear stride: stride[d] =
// prod(extents[d+1..]).
static int64_t linearStride(ArrayRef<int64_t> extents, unsigned d) {
  int64_t s = 1;
  for (unsigned k = d + 1; k < extents.size(); ++k)
    s *= extents[k];
  return s;
}

static void raiseNest(ParallelNest &nest, OpBuilder &builder) {
  scf::ForOp outer = nest.loops.front();
  Location loc = outer.getLoc();
  MLIRContext *ctx = builder.getContext();
  unsigned n = nest.loops.size();

  // Raise ONLY the outermost loop to a 1-D su_iterate (owner dim 0), keeping
  // the inner loops nested as scf.for. This mirrors how a kernel `omp parallel
  // for` is lowered (su_iterate over the outer dim, inner loops stay scf.for),
  // so the written array's owner dim matches the kernel's and LayoutAssignment
  // keeps it block_parallel instead of coarsing. Collapsing to a flat space
  // would instead disagree with the kernel's owner-dim shape and force a coarse
  // fallback.
  builder.setInsertionPoint(outer);
  auto cuRegion = sde::SdeCuRegionOp::create(
      builder, loc, /*resultTypes=*/TypeRange{},
      sde::SdeCuKindAttr::get(ctx, sde::SdeCuKind::parallel),
      /*nowait=*/nullptr, /*iterArgs=*/ValueRange{});
  Block &parBlk = sde::ensureBlock(cuRegion.getBody());
  builder.setInsertionPointToStart(&parBlk);

  Value lb = createConstantIndex(builder, loc, 0);
  Value ub = createConstantIndex(builder, loc, nest.extents.front());
  Value one = createConstantIndex(builder, loc, 1);

  auto suIter = sde::SdeSuIterateOp::create(
      builder, loc, /*resultTypes=*/TypeRange{}, ValueRange{lb}, ValueRange{ub},
      ValueRange{one}, /*schedule=*/nullptr, /*chunkSize=*/Value(),
      /*nowait=*/nullptr, /*reductionAccumulators=*/ValueRange{},
      /*reductionKinds=*/nullptr, /*reductionStrategy=*/nullptr,
      /*partialReduction=*/nullptr, /*partialReductionDims=*/nullptr,
      /*partialReductionOwnerDims=*/nullptr,
      /*structuredClassification=*/nullptr, /*pattern=*/nullptr,
      /*accessMinOffsets=*/nullptr, /*accessMaxOffsets=*/nullptr,
      /*ownerDims=*/nullptr, /*spatialDims=*/nullptr,
      /*writeFootprint=*/nullptr,
      /*physicalOwnerDims=*/nullptr, /*physicalBlockShape=*/nullptr,
      /*logicalWorkerSlice=*/nullptr, /*physicalHaloShape=*/nullptr,
      /*iterationTopology=*/nullptr, /*repetitionStructure=*/nullptr,
      /*asyncStrategy=*/nullptr,
      /*distributionKind=*/nullptr, /*inPlaceSafe=*/nullptr,
      /*inPlaceSharedState=*/nullptr,
      /*arrayLayout=*/nullptr, /*layoutsDisagree=*/nullptr,
      /*commVolumeBytes=*/nullptr);

  Region &dstRegion = suIter.getBody();
  if (dstRegion.empty())
    dstRegion.push_back(new Block());
  Block &dst = dstRegion.front();
  if (dst.getNumArguments() == 0)
    dst.addArgument(builder.getIndexType(), loc);
  Value outerIv = dst.getArgument(0);

  // Clone the outer loop body (the inner nest) into the su_iterate body,
  // mapping the outer IV to the su_iterate IV. Inner scf.for loops (and their
  // IVs) are cloned verbatim.
  OpBuilder::InsertionGuard ig(builder);
  builder.setInsertionPointToStart(&dst);
  IRMapping mapper;
  mapper.map(outer.getInductionVar(), outerIv);
  Block &outerBody = outer.getRegion().front();
  for (Operation &op : outerBody.without_terminator())
    builder.clone(op, mapper);
  sde::SdeYieldOp::create(builder, loc, ValueRange{});
  builder.setInsertionPointToEnd(&parBlk);
  sde::SdeYieldOp::create(builder, loc, ValueRange{});

  // Now substitute the linear counters inside the cloned body. Re-collect the
  // cloned inner loop chain so we can map dim d -> its IV (dim 0 = outerIv).
  SmallVector<Value, 4> dimIv;
  dimIv.push_back(outerIv);
  {
    // Walk the cloned su_iterate body for the nested scf.for chain.
    Block *cur = &dst;
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

  llvm::SmallDenseSet<Value, 2> counterMems;
  for (const Counter &c : nest.counters)
    counterMems.insert(c.mem);

  // Replace counter loads with init + step * (row-major linear index), and
  // erase counter increment stores. The IVs are all in scope at the innermost
  // body.
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
      // lin = sum_d dimIv[d] * stride[d]
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

  outer.erase();
}

struct ParallelizePass : public sde::impl::ParallelizeBase<ParallelizePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    SmallVector<ParallelNest, 4> matches;
    module.walk([&](scf::ForOp outer) {
      if (isa<scf::ForOp>(outer->getParentOp()))
        return;
      if (std::optional<ParallelNest> nest = matchParallelNest(outer, module))
        matches.push_back(std::move(*nest));
    });

    OpBuilder builder(&getContext());
    for (ParallelNest &nest : matches) {
      ARTS_DEBUG("Parallelizing+raising init nest writing distributed array");
      raiseNest(nest, builder);
    }
  }
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass> createParallelizePass() {
  return std::make_unique<ParallelizePass>();
}

} // namespace mlir::carts::sde
