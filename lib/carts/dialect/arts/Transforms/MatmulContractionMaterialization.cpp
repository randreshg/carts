///==========================================================================///
/// File: MatmulContractionMaterialization.cpp
///
/// Materializes a planned block-matmul contraction reduction into
/// per-(owner-block, k-tile) partial-product producer EDTs plus a per-block
/// summing settle, so the cross-node contraction over F avoids a single coarse
/// <inout> replica.
///
/// Why a dedicated ARTS pass (not the CodirToArts bridge, not the existing
/// PartialReductionSplitMaterialization):
///   - codir.codelet is IsolatedFromAbove, so a k-loop body transform that must
///     reference the cross-node all-gather replica (the `perBlockReplicated` DB
///     emitted by emitPerBlockAllGatherWriteBack) can only run AFTER
///     lowerCodelet when G is an arts.edt. This pass runs at
///     post-db-refinement, after the replica exists.
///   - PartialReductionSplitMaterialization splits scalar rank-1 add
///   reductions.
///     This pass handles rank-2 block matmul contractions whose result is an
///     owner block rather than a scalar element, using the same per-tile EDT
///     and outside-the-EDT block-arg acquire discipline.
///
/// ABI legality: every DB an EDT touches must arrive as a block-arg dep backed
/// by a db_acquire emitted outside the EDT.
///==========================================================================///

#define GEN_PASS_DEF_MATMULCONTRACTIONMATERIALIZATION
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/STLExtras.h"
#include <functional>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

/// One validated chained-matmul G consumer ready for contraction split.
///
///   gEdt         : the lowered arts.edt with `partialReductionDims`.
///   ownerLoop    : the enclosing owner-block dispatch loop (IV is a G param).
///   resultDep    : index of G's <inout> block result dependency.
///   eDep         : index of G's <in> block LHS (E) dependency.
///   coarseFDep   : index of G's <in> coarse-F dependency (`replicatedRead`).
///   replicaAlloc : the matching `perBlockReplicated` all-gather replica of F.
///   blockRows    : replica owner-block extent along the contraction dim.
///   numTiles     : number of replica blocks (= contraction length /
///   blockRows).
struct MatmulContractionTarget {
  EdtOp gEdt;
  scf::ForOp ownerLoop;
  unsigned resultDep = 0;
  unsigned eDep = 0;
  unsigned coarseFDep = 0;
  DbAllocOp replicaAlloc;
  int64_t blockRows = 0;
  int64_t numTiles = 0;
  // Matmul body landmarks (in G's EDT body), captured for the producer clone.
  scf::ForOp kLoop;     // the contraction k-loop (reads F[k, col]).
  memref::LoadOp eLoad; // E[row, k].
  memref::LoadOp fLoad; // F[k, col].
};

static DbAllocOp getDepAlloc(EdtOp edt, unsigned depIndex) {
  if (depIndex >= edt.getDependencies().size())
    return {};
  Value dep = edt.getDependencies()[depIndex];
  if (auto acq = dep.getDefiningOp<DbAcquireOp>())
    return acq.getSourcePtr().getDefiningOp<DbAllocOp>();
  return {};
}

static DbAcquireOp getDepAcquire(EdtOp edt, unsigned depIndex) {
  if (depIndex >= edt.getDependencies().size())
    return {};
  return edt.getDependencies()[depIndex].getDefiningOp<DbAcquireOp>();
}

static std::optional<int64_t> foldConst(Value v) {
  return ValueAnalysis::tryFoldConstantIndex(v);
}

/// Block-mode <in>/<out> acquire of `alloc` at outer index `offset` (size 1),
/// emitted OUTSIDE any EDT so the resulting ptr can be a block-arg dep. ARTS-RT
/// ABI requires every DB an EDT touches to arrive this way.
static DbAcquireOp materializeBridgeAcquireBlock(OpBuilder &builder,
                                                 Location loc, DbAllocOp alloc,
                                                 ArtsMode mode, Value offset,
                                                 Value size) {
  return DbAcquireOp::create(builder, loc, mode, alloc.getGuid(),
                             alloc.getPtr(), PartitionMode::block,
                             /*indices=*/SmallVector<Value>{},
                             /*offsets=*/SmallVector<Value>{offset},
                             /*sizes=*/SmallVector<Value>{size},
                             /*partitionIndices=*/SmallVector<Value>{},
                             /*partitionOffsets=*/SmallVector<Value>{},
                             /*partitionSizes=*/SmallVector<Value>{},
                             /*boundsValid=*/Value{},
                             /*elementOffsets=*/SmallVector<Value>{},
                             /*elementSizes=*/SmallVector<Value>{});
}

/// The db_ref payload (rank-N memref view) of an EDT body's `depIndex` block
/// arg.
static Value getDepPayload(Block &edtBody, unsigned depIndex) {
  if (depIndex >= edtBody.getNumArguments())
    return {};
  BlockArgument arg = edtBody.getArgument(depIndex);
  for (Operation *user : arg.getUsers())
    if (auto ref = dyn_cast<DbRefOp>(user))
      return ref.getResult();
  return {};
}

/// Per-element summing nest: reduce the P partial payloads with arith.addf and
/// store once into the destination payload (the addf dual of a copy nest).
static void materializeSumNest(OpBuilder &builder, Location loc,
                               ArrayRef<Value> partialPayloads,
                               Value dstPayload, ArrayRef<Value> copySizes) {
  std::function<void(SmallVectorImpl<Value> &)> emit =
      [&](SmallVectorImpl<Value> &indices) {
        unsigned dim = indices.size();
        if (dim == copySizes.size()) {
          Value acc = memref::LoadOp::create(builder, loc,
                                             partialPayloads.front(), indices);
          for (size_t tile = 1; tile < partialPayloads.size(); ++tile) {
            Value next = memref::LoadOp::create(builder, loc,
                                                partialPayloads[tile], indices);
            acc = arith::AddFOp::create(builder, loc, acc, next);
          }
          memref::StoreOp::create(builder, loc, acc, dstPayload, indices);
          return;
        }
        Value zero = createZeroIndex(builder, loc);
        Value one = createOneIndex(builder, loc);
        auto loop = scf::ForOp::create(builder, loc, zero, copySizes[dim], one);
        OpBuilder::InsertionGuard guard(builder);
        builder.setInsertionPointToStart(loop.getBody());
        indices.push_back(loop.getInductionVar());
        emit(indices);
        indices.pop_back();
      };
  SmallVector<Value> indices;
  emit(indices);
}

/// Find the unique `perBlockReplicated` replica whose flattened element
/// footprint equals the coarse-F whole-array footprint (numTiles * per-block
/// footprint).
static DbAllocOp findMatchingReplica(ModuleOp module, DbAllocOp coarseFAlloc) {
  if (!coarseFAlloc)
    return {};
  SmallVector<int64_t> coarseElems;
  for (Value sz : coarseFAlloc.getElementSizes()) {
    std::optional<int64_t> c = foldConst(sz);
    if (!c)
      return {};
    coarseElems.push_back(*c);
  }
  int64_t wholeProd = 1;
  for (int64_t v : coarseElems)
    wholeProd *= v;

  DbAllocOp match;
  unsigned matches = 0;
  module.walk([&](DbAllocOp alloc) {
    if (!alloc.getPerBlockReplicatedAttr())
      return;
    if (alloc.getElementSizes().size() != coarseElems.size() ||
        alloc.getSizes().size() != 1)
      return;
    int64_t blockProd = 1;
    for (Value sz : alloc.getElementSizes()) {
      std::optional<int64_t> c = foldConst(sz);
      if (!c)
        return;
      blockProd *= *c;
    }
    std::optional<int64_t> blockCount = foldConst(alloc.getSizes().front());
    if (!blockCount || *blockCount <= 0)
      return;
    if (blockProd * *blockCount != wholeProd)
      return;
    match = alloc;
    ++matches;
  });
  return matches == 1 ? match : DbAllocOp{};
}

/// Inside G's EDT body, find the contraction k-loop (the scf.for whose IV is
/// the row index of a load from the coarse-F payload) and the paired E/F loads.
static LogicalResult findContractionLoads(EdtOp gEdt, unsigned coarseFDep,
                                          unsigned eDep,
                                          MatmulContractionTarget &t) {
  Block &body = gEdt.getBody().front();
  BlockArgument fArg = body.getArgument(coarseFDep);
  BlockArgument eArg = body.getArgument(eDep);

  auto payloadOf = [](BlockArgument arg) -> Value {
    for (Operation *user : arg.getUsers())
      if (auto ref = dyn_cast<DbRefOp>(user))
        return ref.getResult();
    return {};
  };
  Value fPayload = payloadOf(fArg);
  Value ePayload = payloadOf(eArg);
  if (!fPayload || !ePayload)
    return failure();

  memref::LoadOp fLoad, eLoad;
  scf::ForOp kLoop;
  for (Operation *user : fPayload.getUsers()) {
    auto load = dyn_cast<memref::LoadOp>(user);
    if (!load || load.getIndices().size() != 2)
      continue;
    Value rowIdx = load.getIndices()[0];
    auto blockArg = dyn_cast<BlockArgument>(rowIdx);
    if (!blockArg)
      continue;
    auto candidate = dyn_cast<scf::ForOp>(blockArg.getOwner()->getParentOp());
    if (!candidate || candidate.getInductionVar() != rowIdx)
      continue;
    if (fLoad)
      return failure();
    fLoad = load;
    kLoop = candidate;
  }
  if (!fLoad || !kLoop)
    return failure();

  // E load that uses the same k IV as its column index: E[row, k].
  Value kIv = kLoop.getInductionVar();
  for (Operation *user : ePayload.getUsers()) {
    auto load = dyn_cast<memref::LoadOp>(user);
    if (!load || load.getIndices().size() != 2)
      continue;
    if (load.getIndices()[1] != kIv)
      continue;
    if (eLoad)
      return failure();
    eLoad = load;
  }
  if (!eLoad)
    return failure();

  // The k-loop must be a unit-step 0..K loop, K = numTiles * blockRows.
  std::optional<int64_t> lb = foldConst(kLoop.getLowerBound());
  std::optional<int64_t> ub = foldConst(kLoop.getUpperBound());
  std::optional<int64_t> step = foldConst(kLoop.getStep());
  if (!lb || !ub || !step || *lb != 0 || *step != 1)
    return failure();
  if (*ub != t.numTiles * t.blockRows)
    return failure();

  t.kLoop = kLoop;
  t.eLoad = eLoad;
  t.fLoad = fLoad;
  return success();
}

/// Identify a chained-matmul G EDT eligible for contraction split.
static std::optional<MatmulContractionTarget> matchTarget(EdtOp edt) {
  if (!edt.getPartialReductionDimsAttr())
    return std::nullopt;
  ModuleOp module = edt->getParentOfType<ModuleOp>();
  if (!module || !hasArtsInterNodeRuntime(module))
    return std::nullopt;

  // Owner dispatch loop: enclosing scf.for whose IV is a G param.
  scf::ForOp ownerLoop;
  for (Operation *p = edt->getParentOp(); p; p = p->getParentOp())
    if (auto loop = dyn_cast<scf::ForOp>(p))
      if (llvm::is_contained(edt.getParams(), loop.getInductionVar())) {
        ownerLoop = loop;
        break;
      }
  if (!ownerLoop)
    return std::nullopt;

  // Locate the coarse-F `replicatedRead` <in> dep + a unique matching replica.
  std::optional<unsigned> coarseFDep;
  DbAllocOp replicaAlloc;
  for (auto [idx, dep] : llvm::enumerate(edt.getDependencies())) {
    DbAcquireOp acq = getDepAcquire(edt, static_cast<unsigned>(idx));
    if (!acq || !acq.getReplicatedReadAttr())
      continue;
    std::optional<PartitionMode> pm = acq.getPartitionMode();
    if (!pm || *pm != PartitionMode::coarse)
      continue;
    DbAllocOp coarseAlloc = acq.getSourcePtr().getDefiningOp<DbAllocOp>();
    DbAllocOp replica = findMatchingReplica(module, coarseAlloc);
    if (!replica)
      continue;
    if (coarseFDep)
      return std::nullopt; // ambiguous.
    coarseFDep = static_cast<unsigned>(idx);
    replicaAlloc = replica;
  }
  if (!coarseFDep)
    return std::nullopt;

  // Result dep: the single <inout> block dependency; E dep: the single <in>
  // block dependency.
  std::optional<unsigned> resultDep, eDep;
  for (auto [idx, dep] : llvm::enumerate(edt.getDependencies())) {
    DbAcquireOp acq = getDepAcquire(edt, static_cast<unsigned>(idx));
    if (!acq)
      return std::nullopt;
    std::optional<PartitionMode> pm = acq.getPartitionMode();
    if (!pm || *pm != PartitionMode::block)
      continue;
    if (acq.getMode() == ArtsMode::inout) {
      if (resultDep)
        return std::nullopt;
      resultDep = static_cast<unsigned>(idx);
    } else if (acq.getMode() == ArtsMode::in) {
      if (eDep)
        return std::nullopt;
      eDep = static_cast<unsigned>(idx);
    }
  }
  if (!resultDep || !eDep)
    return std::nullopt;

  // Replica geometry: owner-block extent along the contraction dim + block
  // count.
  int64_t blockRows = 0;
  if (auto ownerDims = getPlanOwnerDimsAttr(replicaAlloc.getOperation()))
    if (ownerDims.size() == 1)
      if (auto od = dyn_cast<IntegerAttr>(ownerDims[0])) {
        unsigned dim = static_cast<unsigned>(od.getInt());
        if (dim < replicaAlloc.getElementSizes().size())
          if (std::optional<int64_t> c =
                  foldConst(replicaAlloc.getElementSizes()[dim]))
            blockRows = *c;
      }
  if (blockRows <= 0)
    return std::nullopt;
  std::optional<int64_t> numTiles = foldConst(replicaAlloc.getSizes().front());
  if (!numTiles || *numTiles <= 0)
    return std::nullopt;
  std::optional<int64_t> totalNodes = arts::getRuntimeTotalNodes(module);
  if (!totalNodes || *totalNodes <= 1)
    return std::nullopt;
  // Production cost gate: this pass materializes one producer EDT per
  // (owner-block, replica-block). That is profitable only when the replica
  // blocks already describe node-granular contraction tiles. If the replica is
  // much finer than the node count, the split explodes into thousands of tiny
  // EDTs and loses to the coarser replicated-read path. CODIR still owns the
  // all_gather decision; ARTS declines this extra tiling until the contraction
  // tiler can group multiple replica blocks into one node strip.
  if (*numTiles > *totalNodes)
    return std::nullopt;

  MatmulContractionTarget t;
  t.gEdt = edt;
  t.ownerLoop = ownerLoop;
  t.resultDep = *resultDep;
  t.eDep = *eDep;
  t.coarseFDep = *coarseFDep;
  t.replicaAlloc = replicaAlloc;
  t.blockRows = blockRows;
  t.numTiles = *numTiles;
  if (failed(findContractionLoads(edt, *coarseFDep, *eDep, t)))
    return std::nullopt;
  return t;
}

/// Per-(G-block, k-tile) partials DB: a single block DB whose outer extent is
/// `numTiles` and whose element block mirrors G's settled-block footprint.
static DbAllocOp createPartialsDb(OpBuilder &builder, Location loc,
                                  DbAllocOp resultAlloc, int64_t numTiles) {
  Value route = createCurrentNodeRoute(builder, loc);
  Value tileCount = createConstantIndex(builder, loc, numTiles);
  SmallVector<Value> innerSizes(resultAlloc.getElementSizes().begin(),
                                resultAlloc.getElementSizes().end());
  auto db = DbAllocOp::create(
      builder, loc, ArtsMode::inout, route, DbAllocType::heap, DbMode::write,
      resultAlloc.getElementType(), SmallVector<Value>{tileCount},
      std::move(innerSizes), PartitionMode::block);
  if (auto ownerDims = getPlanOwnerDimsAttr(resultAlloc.getOperation()))
    setPlanOwnerDimsAttr(db.getOperation(), ownerDims);
  if (auto blockShape =
          getPlanPhysicalBlockShapeAttr(resultAlloc.getOperation()))
    setPlanPhysicalBlockShapeAttr(db.getOperation(), blockShape);
  // Replicated-local: every node holds all per-tile partials for its owned
  // block, exactly like the all-gather replica it consumes.
  db.setLocalOnlyAttr(UnitAttr::get(db.getContext()));
  db.setPerBlockReplicatedAttr(UnitAttr::get(db.getContext()));
  return db;
}

/// Emit one per-tile partial-product producer EDT. The EDT acquires OUTSIDE its
/// body: the replica F strip for tile `tileIdx` (<in>), the E block (<in>), and
/// the partial tile `tileIdx` (<out>). Its body recomputes the matmul over the
/// k' rows of the tile, accumulating into the partial.
static LogicalResult emitTileProducer(OpBuilder &builder, Location loc,
                                      MatmulContractionTarget &t,
                                      int64_t tileIdx, DbAllocOp partialsDb,
                                      Value ownerOrdinal) {
  ModuleOp module = t.gEdt->getParentOfType<ModuleOp>();
  Value one = createOneIndex(builder, loc);
  Value tileVal = createConstantIndex(builder, loc, tileIdx);

  // OUTSIDE-the-EDT acquires (block-arg deps).
  DbAcquireOp eAcq = getDepAcquire(t.gEdt, t.eDep);
  DbAcquireOp fStrip = materializeBridgeAcquireBlock(
      builder, loc, t.replicaAlloc, ArtsMode::in, tileVal, one);
  DbAllocOp eAlloc = getDepAlloc(t.gEdt, t.eDep);
  if (!eAlloc)
    return failure();
  // Reuse G's own E block offset (it is the same owner block) by re-acquiring
  // the E block at G's result-block offset.
  DbAcquireOp gResultAcq = getDepAcquire(t.gEdt, t.resultDep);
  if (!eAcq || !gResultAcq)
    return failure();
  Value eOffset =
      eAcq.getOffsets().empty() ? Value() : eAcq.getOffsets().front();
  Value eSize = eAcq.getSizes().empty() ? one : eAcq.getSizes().front();
  if (!eOffset)
    return failure();
  DbAcquireOp eBlock = materializeBridgeAcquireBlock(
      builder, loc, eAlloc, ArtsMode::in, eOffset, eSize);
  DbAcquireOp partialAcq = materializeBridgeAcquireBlock(
      builder, loc, partialsDb, ArtsMode::out, tileVal, one);

  SmallVector<Value> deps{partialAcq.getPtr(), eBlock.getPtr(),
                          fStrip.getPtr()};
  // Carry G's worker-shape params (block rows + owner offset) so the cloned
  // body keeps the same output-block iteration bounds.
  SmallVector<Value> params(t.gEdt.getParams().begin(),
                            t.gEdt.getParams().end());

  ArtsLaunchPolicy launch =
      resolveArtsOrdinalLaunchPolicy(module, ownerOrdinal, builder, loc);
  Value route =
      launch.route ? launch.route : createCurrentNodeRoute(builder, loc);
  auto producer = EdtOp::create(builder, loc, EdtType::task, launch.concurrency,
                                route, deps, params);
  // Carry the plan metadata so downstream placement/contract passes treat the
  // producer like the original owner-block matmul worker.
  if (auto ownerDims = t.gEdt.getPlanOwnerDimsAttr())
    producer.setPlanOwnerDimsAttr(ownerDims);
  if (auto blockShape = t.gEdt.getPlanPhysicalBlockShapeAttr())
    producer.setPlanPhysicalBlockShapeAttr(blockShape);
  if (auto slice = t.gEdt.getPlanLogicalWorkerSliceAttr())
    producer.setPlanLogicalWorkerSliceAttr(slice);

  Block &body = producer.getBody().front();
  for (Value dep : deps)
    body.addArgument(dep.getType(), loc);
  for (Value param : params)
    body.addArgument(param.getType(), loc);

  // Clone G's matmul body into the producer, mapping G's dep/param block args
  // to the producer's (result->partial, E->E, F->replicaStrip, params->params),
  // so every cloned db_ref/load is self-contained. The producer body args are
  // [partial, E, replicaStrip, params...]; G's are [..deps.., params..].
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(&body);
  // Tile bounds are created FIRST so they dominate the cloned k-loop they bound
  // and the reindexed F loads beneath it.
  Value tileBase = createConstantIndex(builder, loc, tileIdx * t.blockRows);
  Value tileEnd =
      createConstantIndex(builder, loc, (tileIdx + 1) * t.blockRows);

  Block &gBody = t.gEdt.getBody().front();
  IRMapping mapper;
  mapper.map(gBody.getArgument(t.resultDep), body.getArgument(0));
  mapper.map(gBody.getArgument(t.eDep), body.getArgument(1));
  mapper.map(gBody.getArgument(t.coarseFDep), body.getArgument(2));
  for (auto [pi, gParam] : llvm::enumerate(t.gEdt.getParams()))
    mapper.map(gBody.getArgument(t.gEdt.getDependencies().size() + pi),
               body.getArgument(deps.size() + pi));
  for (Operation &op : gBody.without_terminator())
    builder.clone(op, mapper);

  // Constrain this producer's contraction loop to its tile range
  // [tileIdx*blockRows, (tileIdx+1)*blockRows) and reindex the F (now replica
  // strip) loads from whole-array row k to block-local row k' = k - tileBase.
  auto clonedKLoop =
      dyn_cast_or_null<scf::ForOp>(mapper.lookupOrNull(t.kLoop.getOperation()));
  Value clonedFLoad = mapper.lookupOrNull(t.fLoad.getResult());
  if (!clonedKLoop || !clonedFLoad)
    return failure();
  clonedKLoop.setLowerBound(tileBase);
  clonedKLoop.setUpperBound(tileEnd);

  // The cloned F db_ref payload is a db_ref of producer arg 2 (the replica
  // strip), so its loads address the block directly; rebase the row index.
  Value clonedFPayload =
      mapper.lookupOrNull(getDepPayload(gBody, t.coarseFDep));
  if (!clonedFPayload)
    return failure();
  SmallVector<memref::LoadOp> fLoads;
  clonedKLoop->walk([&](memref::LoadOp ld) {
    if (ld.getMemRef() == clonedFPayload && ld.getIndices().size() == 2)
      fLoads.push_back(ld);
  });
  for (memref::LoadOp ld : fLoads) {
    OpBuilder::InsertionGuard lg(builder);
    builder.setInsertionPoint(ld);
    Value kp =
        arith::SubIOp::create(builder, loc, ld.getIndices()[0], tileBase);
    auto fixed =
        memref::LoadOp::create(builder, loc, clonedFPayload,
                               SmallVector<Value>{kp, ld.getIndices()[1]});
    ld.replaceAllUsesWith(fixed.getResult());
    ld.erase();
  }

  // Each tile writes its OWN partial DB and zero-inits it (the cloned G
  // zero-init nest), so the per-tile contribution is exact; the settle sums the
  // P partials.
  YieldOp::create(builder, loc);
  return success();
}

/// Emit the per-block summing settle: sum the P partial tiles into G's settled
/// block (the original result DB), written <out> once.
static LogicalResult emitSettle(OpBuilder &builder, Location loc,
                                MatmulContractionTarget &t,
                                DbAllocOp partialsDb, Value ownerOrdinal) {
  ModuleOp module = t.gEdt->getParentOfType<ModuleOp>();
  DbAllocOp resultAlloc = getDepAlloc(t.gEdt, t.resultDep);
  DbAcquireOp resultAcq = getDepAcquire(t.gEdt, t.resultDep);
  if (!resultAlloc || !resultAcq)
    return failure();
  Value one = createOneIndex(builder, loc);
  Value resultOffset =
      resultAcq.getOffsets().empty() ? Value() : resultAcq.getOffsets().front();
  if (!resultOffset)
    return failure();

  SmallVector<Value> blockElementSizes(resultAlloc.getElementSizes().begin(),
                                       resultAlloc.getElementSizes().end());

  SmallVector<Value> deps;
  deps.reserve(t.numTiles + 1);
  for (int64_t tile = 0; tile < t.numTiles; ++tile) {
    Value tileVal = createConstantIndex(builder, loc, tile);
    DbAcquireOp partialAcq = materializeBridgeAcquireBlock(
        builder, loc, partialsDb, ArtsMode::in, tileVal, one);
    deps.push_back(partialAcq.getPtr());
  }
  DbAcquireOp dstAcq = materializeBridgeAcquireBlock(
      builder, loc, resultAlloc, ArtsMode::out, resultOffset, one);
  deps.push_back(dstAcq.getPtr());

  SmallVector<Value> params(blockElementSizes.begin(), blockElementSizes.end());
  ArtsLaunchPolicy launch =
      resolveArtsOrdinalLaunchPolicy(module, ownerOrdinal, builder, loc);
  Value route =
      launch.route ? launch.route : createCurrentNodeRoute(builder, loc);
  auto settle = EdtOp::create(builder, loc, EdtType::task, launch.concurrency,
                              route, deps, params);
  settle.setStorageBridgeCopyAttr(UnitAttr::get(settle.getContext()));
  settle.setPerBlockSummingSettleAttr(UnitAttr::get(settle.getContext()));

  Block &body = settle.getBody().front();
  for (Value dep : deps)
    body.addArgument(dep.getType(), loc);
  for (Value param : params)
    body.addArgument(param.getType(), loc);
  {
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToStart(&body);
    Value bodyZero = createZeroIndex(builder, loc);
    SmallVector<Value> partialPayloads;
    for (int64_t tile = 0; tile < t.numTiles; ++tile)
      partialPayloads.push_back(DbRefOp::create(builder, loc,
                                                body.getArgument(tile),
                                                SmallVector<Value>{bodyZero})
                                    .getResult());
    Value dstPayload =
        DbRefOp::create(builder, loc, body.getArgument(t.numTiles),
                        SmallVector<Value>{bodyZero})
            .getResult();
    SmallVector<Value> bodyCopySizes;
    for (size_t i = 0; i < blockElementSizes.size(); ++i)
      bodyCopySizes.push_back(body.getArgument(t.numTiles + 1 + i));
    materializeSumNest(builder, loc, partialPayloads, dstPayload,
                       bodyCopySizes);
    YieldOp::create(builder, loc);
  }
  return success();
}

static LogicalResult materializeTarget(MatmulContractionTarget &t) {
  EdtOp gEdt = t.gEdt;
  Location loc = gEdt.getLoc();
  DbAllocOp resultAlloc = getDepAlloc(gEdt, t.resultDep);
  if (!resultAlloc)
    return failure();

  // Partials DB is allocated INSIDE the owner-block dispatch loop, immediately
  // before G, so each owner-block iteration reserves its own per-tile partial
  // GUIDs (no cross-block aliasing of the partial buffers).
  OpBuilder builder(gEdt);
  DbAllocOp partialsDb =
      createPartialsDb(builder, loc, resultAlloc, t.numTiles);

  // Owner ordinal for routing = the owner index used by G's result acquire
  // offset (the relative dispatch position).
  builder.setInsertionPoint(gEdt);
  DbAcquireOp resultAcq = getDepAcquire(gEdt, t.resultDep);
  Value ownerOrdinal = resultAcq && !resultAcq.getOffsets().empty()
                           ? resultAcq.getOffsets().front()
                           : createZeroIndex(builder, loc);

  // Per-tile producers.
  for (int64_t tile = 0; tile < t.numTiles; ++tile) {
    builder.setInsertionPoint(gEdt);
    if (failed(
            emitTileProducer(builder, loc, t, tile, partialsDb, ownerOrdinal)))
      return failure();
  }

  // Summing settle into G's settled block.
  builder.setInsertionPoint(gEdt);
  if (failed(emitSettle(builder, loc, t, partialsDb, ownerOrdinal)))
    return failure();

  // Retire the original coarse-contraction G EDT and its now-dead acquires.
  SmallVector<DbAcquireOp> acquires;
  for (unsigned i = 0, e = gEdt.getDependencies().size(); i < e; ++i)
    if (DbAcquireOp acq = getDepAcquire(gEdt, i))
      acquires.push_back(acq);
  gEdt.erase();
  for (DbAcquireOp acq : acquires)
    if (acq.getPtr().use_empty() &&
        (!acq.getGuid() || acq.getGuid().use_empty()))
      acq.erase();
  return success();
}

struct MatmulContractionMaterializationPass
    : public impl::MatmulContractionMaterializationBase<
          MatmulContractionMaterializationPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    SmallVector<MatmulContractionTarget, 2> targets;
    module.walk([&](EdtOp edt) {
      if (std::optional<MatmulContractionTarget> t = matchTarget(edt))
        targets.push_back(*t);
    });
    for (MatmulContractionTarget &t : targets)
      if (failed(materializeTarget(t))) {
        t.gEdt.emitError() << "failed to materialize matmul contraction split";
        signalPassFailure();
        return;
      }
  }
};

} // namespace

std::unique_ptr<Pass>
mlir::carts::arts::createMatmulContractionMaterializationPass() {
  return std::make_unique<MatmulContractionMaterializationPass>();
}
