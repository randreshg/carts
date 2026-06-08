///==========================================================================///
/// File: Parallelize.cpp
///
/// SDE parallelization of embarrassingly-parallel SEQUENTIAL loop nests.
///
/// ConvertOpenMPToSde raises explicit OpenMP worksharing. SdeCuNormalization
/// wraps remaining source loops as conservative `sde.cu_region <single>` work.
/// This pass promotes only perfectly-nested rectangular loops whose memory
/// effects prove independent:
///   1. external array stores are indexed by the loop IVs, with no repeated
///      physical dimension and no read from a written root;
///   2. read-only external array inputs are allowed;
///   3. scalar loop-carried state is limited to closed-form induction counters
///      whose final mutable value is not observed after the loop.
///
/// Proven nests become `sde.su_iterate` work before LayoutAssignment.
/// Unsupported nests stay unchanged.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_PARALLELIZE
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
// constant trips) whose external array stores are indexed exactly by the IVs,
// plus any recognized linear-counter scalars to substitute.
struct ParallelNest {
  SmallVector<scf::ForOp, 4> loops; // outer..inner
  SmallVector<int64_t, 4> extents;  // per-loop constant trip count
  SmallVector<memref::StoreOp, 4> arrayStores;
  SmallVector<Value, 4> roots; // stripped external memref roots written
  SmallVector<Counter, 2> counters;
  sde::SdeCuRegionOp enclosingSingleCu;
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

static bool enclosingFunctionHasSdeOp(Operation *op) {
  auto fn = op ? op->getParentOfType<func::FuncOp>() : func::FuncOp();
  if (!fn)
    return false;
  bool found = false;
  fn.walk([&](Operation *nested) {
    if (sde::isSdeDialectOp(nested)) {
      found = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return found;
}

static std::optional<ParallelNest> matchParallelNest(scf::ForOp outer) {
  if (outer->getParentOfType<sde::SdeSuIterateOp>())
    return std::nullopt;
  if (isa<scf::ForOp>(outer->getParentOp()))
    return std::nullopt;

  ParallelNest nest;
  if (auto cu = outer->getParentOfType<sde::SdeCuRegionOp>()) {
    if (outer->getParentOp() != cu.getOperation() ||
        cu.getKind() != sde::SdeCuKind::single || !cu.getIterArgs().empty() ||
        cu.getNumResults() != 0)
      return std::nullopt;
    nest.enclosingSingleCu = cu;
  } else if (!enclosingFunctionHasSdeOp(outer.getOperation())) {
    return std::nullopt;
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

  // Classify every op in the innermost body. Allowed:
  //  - one or more stores to EXTERNAL arrays at indices == IVs
  //  - per counter: a self-increment store to a rank-0 scratch scalar
  //  - loads from read-only external arrays, pure arith/math, scf.if/yield
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
        sde::isKnownPureScalarLibmCall(op) || isMemoryEffectFree(op))
      return;
    rejected = true;
  });
  if (rejected)
    return std::nullopt;

  // Separate array stores from counter increment stores.
  llvm::SmallDenseMap<Value, int64_t> counterStep; // counter mem -> step
  std::optional<SmallVector<int64_t, 4>> selectedOwnerDims;
  std::optional<SmallVector<int64_t, 4>> selectedShape;
  for (memref::StoreOp st : stores) {
    Value r = ValueAnalysis::stripMemrefViewOps(st.getMemref());
    if (isScalarScratch(r)) {
      // Must be a self-increment: store(addi(load(r), C), r) (or C+load).
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
        return std::nullopt; // more than one increment store -> bail
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

  // No load may read the written array root (no cross-iteration RAW/WAR).
  // Build the recognized-counter set; loads of counters are fine (substituted).
  for (auto &kv : counterStep) {
    std::optional<int64_t> init = findCounterInit(kv.first, outer);
    if (!init)
      return std::nullopt; // counter without a provable constant init
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
  // Read-only array inputs are legal. Loads from any written root are rejected
  // because they would need a data-dependence or reduction carrier.
  for (memref::LoadOp ld : loads) {
    Value r = ValueAnalysis::stripMemrefViewOps(ld.getMemref());
    if (!isCounter(r))
      if (!r || isScalarScratch(r) || rootIn(r, nest.roots) ||
          sde::isDefinedInside(outer.getOperation(), r))
        return std::nullopt;
  }

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

static bool directCuBodyHasUnscheduledSourceCompute(sde::SdeCuRegionOp cu) {
  if (!cu || cu.getBody().empty())
    return false;
  for (Operation &op : cu.getBody().front().without_terminator()) {
    if (sde::isSourceComputeOp(&op))
      return true;
  }
  return false;
}

static void promoteSingleCuIfFullyScheduled(sde::SdeCuRegionOp cu) {
  if (!cu || cu.getKind() != sde::SdeCuKind::single)
    return;
  if (directCuBodyHasUnscheduledSourceCompute(cu))
    return;
  cu.setKindAttr(
      sde::SdeCuKindAttr::get(cu.getContext(), sde::SdeCuKind::parallel));
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

  OpBuilder::InsertionGuard ig(builder);
  builder.setInsertionPointToStart(&dst);
  IRMapping mapper;
  mapper.map(outer.getInductionVar(), outerIv);
  Block &outerBody = outer.getRegion().front();
  for (Operation &op : outerBody.without_terminator())
    builder.clone(op, mapper);
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
    Block *cur = &suIter.getBody().front();
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
}

static void raiseNest(ParallelNest &nest, OpBuilder &builder) {
  scf::ForOp outer = nest.loops.front();
  Location loc = outer.getLoc();
  MLIRContext *ctx = builder.getContext();

  if (nest.enclosingSingleCu) {
    sde::SdeSuIterateOp suIter =
        createSuIterateForNest(nest, builder, outer.getOperation());
    substituteCounters(nest, suIter, builder);
    outer.erase();
    promoteSingleCuIfFullyScheduled(nest.enclosingSingleCu);
    return;
  }

  builder.setInsertionPoint(outer);
  auto cuRegion = sde::SdeCuRegionOp::create(
      builder, loc, /*resultTypes=*/TypeRange{},
      sde::SdeCuKindAttr::get(ctx, sde::SdeCuKind::parallel),
      /*nowait=*/nullptr, /*iterArgs=*/ValueRange{});
  Block &parBlk = sde::ensureBlock(cuRegion.getBody());
  builder.setInsertionPointToStart(&parBlk);

  sde::SdeSuIterateOp suIter = createSuIterateForNest(nest, builder, nullptr);
  substituteCounters(nest, suIter, builder);
  builder.setInsertionPointToEnd(&parBlk);
  sde::SdeYieldOp::create(builder, loc, ValueRange{});

  outer.erase();
}

struct ParallelizePass : public sde::impl::ParallelizeBase<ParallelizePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    SmallVector<ParallelNest, 4> matches;
    module.walk([&](scf::ForOp outer) {
      if (isa<scf::ForOp>(outer->getParentOp()))
        return;
      if (std::optional<ParallelNest> nest = matchParallelNest(outer))
        matches.push_back(std::move(*nest));
    });

    OpBuilder builder(&getContext());
    for (ParallelNest &nest : matches) {
      ARTS_DEBUG("Parallelizing+raising legal host/init/check nest");
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
