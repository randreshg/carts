///==========================================================================///
/// File: BlockContractionSplit.cpp
///
/// Splits a committed owner-block contraction into per-replica-tile producer
/// EDTs plus a per-block summing settle, so cross-node contraction reads avoid
/// a single coarse <inout> replica.
///
/// Why a dedicated ARTS pass (not the ArtsToArts bridge, not the existing
/// scalar partial-reduction split pass):
///   - arts.codelet is IsolatedFromAbove, so a contraction-loop body transform
///     that must reference the cross-node all-gather replica (the
///     `perBlockReplicated` DB emitted by emitPerBlockAllGatherWriteBack) can
///     only run AFTER lowerCodelet when the consumer is an arts.edt. This pass
///     runs at post-db-refinement, after the replica exists.
///   - The scalar partial-reduction split pass handles rank-1 add reductions.
///     This pass handles owner-block add contractions with an N-D result block
///     and one replicated contraction-tile dimension, using the same per-tile
///     EDT and outside-the-EDT block-arg acquire discipline.
///
/// ABI legality: every DB an EDT touches must arrive as a block-arg dep backed
/// by a db_acquire emitted outside the EDT.
///==========================================================================///

#define GEN_PASS_DEF_BLOCKCONTRACTIONSPLIT
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/arts/Utils/LoweringFactUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "carts/utils/ArrayAttrUtils.h"
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

struct BlockInputDep {
  unsigned depIndex = 0;
  DbAcquireOp acquire;
  DbAllocOp alloc;
};

/// One validated owner-block contraction consumer ready for contraction split.
///
///   consumerEdt : the original arts.edt with contraction split facts.
///   ownerLoop   : the enclosing owner-block dispatch loop.
///   resultDep   : index of the <inout> block result dependency.
///   inputDeps   : all block <in> dependencies remapped into each producer.
///   replicatedDep : index of the full-grid replicated-read contraction
///   operand. replicaAlloc  : matching `perBlockReplicated` all-gather replica.
///   tileExtent    : replica block extent along the contraction dim.
///   numTiles      : number of replica blocks.
struct BlockContractionTarget {
  EdtOp consumerEdt;
  scf::ForOp ownerLoop;
  unsigned resultDep = 0;
  SmallVector<BlockInputDep, 4> inputDeps;
  unsigned replicatedDep = 0;
  DbAllocOp replicaAlloc;
  int64_t tileExtent = 0;
  int64_t numTiles = 0;
  scf::ForOp contractionLoop;
  unsigned replicatedContractionDim = 0;
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

static bool hasFullSourceGrid(DbAcquireOp acquire) {
  std::optional<PartitionMode> mode = acquire.getPartitionMode();
  if (!mode)
    return false;
  if (*mode == PartitionMode::coarse)
    return true;
  if (*mode != PartitionMode::block)
    return false;

  DbAllocOp alloc = acquire.getSourcePtr().getDefiningOp<DbAllocOp>();
  if (!alloc || acquire.getOffsets().size() != alloc.getSizes().size() ||
      acquire.getSizes().size() != alloc.getSizes().size())
    return false;
  for (auto [offset, size, allocSize] : llvm::zip_equal(
           acquire.getOffsets(), acquire.getSizes(), alloc.getSizes())) {
    if (!ValueAnalysis::isZeroConstant(offset))
      return false;
    if (!ValueAnalysis::sameValue(size, allocSize) &&
        !ValueAnalysis::areValuesEquivalent(size, allocSize))
      return false;
  }
  return true;
}

/// Block-mode <in>/<out> acquire of `alloc` at outer index `offset` (size 1),
/// emitted OUTSIDE any EDT so the resulting ptr can be a block-arg dep. ARTS-RT
/// ABI requires every DB an EDT touches to arrive this way.
static DbAcquireOp emitBlockAcquire(OpBuilder &builder, Location loc,
                                    DbAllocOp alloc, ArtsMode mode,
                                    ArrayRef<Value> offsets,
                                    ArrayRef<Value> sizes) {
  return DbAcquireOp::create(builder, loc, mode, alloc.getGuid(),
                             alloc.getPtr(), PartitionMode::block,
                             /*indices=*/SmallVector<Value>{},
                             /*offsets=*/SmallVector<Value>(offsets),
                             /*sizes=*/SmallVector<Value>(sizes),
                             /*partitionIndices=*/SmallVector<Value>{},
                             /*partitionOffsets=*/SmallVector<Value>{},
                             /*partitionSizes=*/SmallVector<Value>{},
                             /*boundsValid=*/Value{},
                             /*elementOffsets=*/SmallVector<Value>{},
                             /*elementSizes=*/SmallVector<Value>{});
}

static DbAcquireOp emitSingleBlockAcquire(OpBuilder &builder, Location loc,
                                          DbAllocOp alloc, ArtsMode mode,
                                          Value offset, Value size) {
  return emitBlockAcquire(builder, loc, alloc, mode, SmallVector<Value>{offset},
                          SmallVector<Value>{size});
}

static std::optional<int64_t> inferSingleOwnerBlockExtent(DbAllocOp alloc) {
  if (!alloc)
    return std::nullopt;

  if (std::optional<DbOwnerRouteFacts> facts =
          deriveDbOwnerRouteFactsFromDbGrid(alloc))
    if (facts->dims.size() == 1 && facts->blockShape.size() == 1 &&
        facts->blockShape.front() > 0)
      return facts->blockShape.front();

  if (std::optional<ArtsDbPhysicalLayout> layout =
          readArtsDbPhysicalLayoutFromCommittedType(alloc)) {
    if (layout->ownerDims.size() == 1) {
      int64_t ownerDim = layout->ownerDims.front();
      if (ownerDim >= 0 &&
          static_cast<size_t>(ownerDim) < layout->physicalBlockShape.size() &&
          layout->physicalBlockShape[ownerDim] > 0)
        return layout->physicalBlockShape[ownerDim];
    }
    if (layout->ownerDims.size() == 1 &&
        layout->physicalBlockShape.size() == 1 &&
        layout->physicalBlockShape.front() > 0)
      return layout->physicalBlockShape.front();
  }

  return std::nullopt;
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
static void emitSumNest(OpBuilder &builder, Location loc,
                        ArrayRef<Value> partialPayloads, Value dstPayload,
                        ArrayRef<Value> copySizes) {
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

static int64_t getFlattenedElementCount(DbAllocOp alloc) {
  if (!alloc)
    return -1;
  int64_t product = 1;
  for (Value sz : alloc.getElementSizes()) {
    std::optional<int64_t> c = ValueAnalysis::tryFoldConstantIndex(sz);
    if (!c || *c <= 0)
      return -1;
    product *= *c;
  }
  return product;
}

static bool allGatherWritesReplicaFromSource(EdtOp copyEdt, DbAllocOp replica,
                                             DbAllocOp expectedSource) {
  if (!copyEdt || !copyEdt.getPerBlockAllGatherAttr())
    return false;
  if (copyEdt.getDependencies().size() % 2 != 0)
    return false;

  bool sawReplicaWrite = false;
  for (unsigned i = 0, e = copyEdt.getDependencies().size(); i + 1 < e;
       i += 2) {
    auto srcAcquire = copyEdt.getDependencies()[i].getDefiningOp<DbAcquireOp>();
    auto dstAcquire =
        copyEdt.getDependencies()[i + 1].getDefiningOp<DbAcquireOp>();
    if (!srcAcquire || !dstAcquire)
      return false;
    DbAllocOp dstAlloc = dstAcquire.getSourcePtr().getDefiningOp<DbAllocOp>();
    if (dstAlloc != replica)
      continue;

    DbAllocOp srcAlloc = srcAcquire.getSourcePtr().getDefiningOp<DbAllocOp>();
    if (srcAlloc != expectedSource)
      return false;
    sawReplicaWrite = true;
  }
  return sawReplicaWrite;
}

/// Find the unique `perBlockReplicated` replica written by a per-block
/// all-gather from the same source DB as the full-grid replicated-read dep.
static DbAllocOp findMatchingReplica(ModuleOp module,
                                     DbAllocOp replicaSourceAlloc) {
  if (!module || !replicaSourceAlloc)
    return {};
  int64_t wholeProd = getFlattenedElementCount(replicaSourceAlloc);
  if (wholeProd <= 0)
    return {};

  DbAllocOp match;
  unsigned matches = 0;
  module.walk([&](DbAllocOp alloc) {
    if (!alloc.getPerBlockReplicatedAttr())
      return;
    if (alloc.getElementSizes().size() !=
            replicaSourceAlloc.getElementSizes().size() ||
        alloc.getSizes().size() != 1)
      return;
    int64_t blockProd = getFlattenedElementCount(alloc);
    if (blockProd <= 0)
      return;
    std::optional<int64_t> blockCount =
        ValueAnalysis::tryFoldConstantIndex(alloc.getSizes().front());
    if (!blockCount || *blockCount <= 0)
      return;
    if (blockProd * *blockCount != wholeProd)
      return;
    bool graphMatched = false;
    module.walk([&](EdtOp edt) {
      if (graphMatched)
        return;
      graphMatched =
          allGatherWritesReplicaFromSource(edt, alloc, replicaSourceAlloc);
    });
    if (!graphMatched)
      return;
    match = alloc;
    ++matches;
  });
  return matches == 1 ? match : DbAllocOp{};
}

/// Inside the EDT body, find the contraction loop: an scf.for whose IV indexes
/// one dimension of the replicated-read payload. All direct replicated-payload
/// loads must use the same loop and same payload dimension so the producer can
/// rebase that dimension from whole-replica coordinates to tile-local
/// coordinates.
static LogicalResult
findReplicatedContractionAccesses(EdtOp consumerEdt, unsigned replicatedDep,
                                  BlockContractionTarget &t) {
  Block &body = consumerEdt.getBody().front();
  if (replicatedDep >= body.getNumArguments())
    return failure();
  BlockArgument replicatedArg = body.getArgument(replicatedDep);

  auto payloadOf = [](BlockArgument arg) -> Value {
    for (Operation *user : arg.getUsers())
      if (auto ref = dyn_cast<DbRefOp>(user))
        return ref.getResult();
    return {};
  };
  Value replicatedPayload = payloadOf(replicatedArg);
  if (!replicatedPayload)
    return failure();

  scf::ForOp contractionLoop;
  std::optional<unsigned> contractionDim;
  bool sawLoad = false;
  for (Operation *user : replicatedPayload.getUsers()) {
    if (isa<memref::StoreOp>(user))
      return failure();
    auto load = dyn_cast<memref::LoadOp>(user);
    if (!load)
      continue;

    std::optional<unsigned> loadContractionDim;
    scf::ForOp loadLoop;
    for (auto [dim, index] : llvm::enumerate(load.getIndices())) {
      auto blockArg = dyn_cast<BlockArgument>(index);
      if (!blockArg)
        continue;
      auto candidate =
          dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
      if (!candidate || candidate.getInductionVar() != index)
        continue;
      if (loadContractionDim)
        return failure();
      loadContractionDim = static_cast<unsigned>(dim);
      loadLoop = candidate;
    }
    if (!loadContractionDim)
      continue;
    if (contractionLoop &&
        (contractionLoop != loadLoop || *contractionDim != *loadContractionDim))
      return failure();
    contractionLoop = loadLoop;
    contractionDim = *loadContractionDim;
    sawLoad = true;
  }
  if (!sawLoad || !contractionLoop || !contractionDim)
    return failure();

  // The contraction loop must be a unit-step 0..K loop, where
  // K = numTiles * tileExtent.
  std::optional<int64_t> lb =
      ValueAnalysis::tryFoldConstantIndex(contractionLoop.getLowerBound());
  std::optional<int64_t> ub =
      ValueAnalysis::tryFoldConstantIndex(contractionLoop.getUpperBound());
  std::optional<int64_t> step =
      ValueAnalysis::tryFoldConstantIndex(contractionLoop.getStep());
  if (!lb || !ub || !step || *lb != 0 || *step != 1)
    return failure();
  if (*ub != t.numTiles * t.tileExtent)
    return failure();

  t.contractionLoop = contractionLoop;
  t.replicatedContractionDim = *contractionDim;
  return success();
}

static bool hasSingleReductionDimMarker(ArrayRef<int64_t> map) {
  return llvm::count(map, -1) == 1;
}

static bool requiresBlockContractionSplit(EdtOp edt) {
  if (!edt.getPartialReductionAttr() || !edt.getPartialReductionDimsAttr() ||
      !edt.getPartialReductionDepResultDimMapsAttr())
    return false;
  auto strategy = edt.getReductionStrategyAttr();
  if (!strategy ||
      strategy.getValue() != ArtsReductionStrategy::local_accumulate)
    return false;
  ModuleOp module = edt->getParentOfType<ModuleOp>();
  if (!module || !hasArtsInterNodeRuntime(module))
    return false;

  ArrayAttr depMaps = edt.getPartialReductionDepResultDimMapsAttr();
  if (depMaps.size() != edt.getDependencies().size())
    return false;
  for (auto [idx, dep] : llvm::enumerate(edt.getDependencies())) {
    DbAcquireOp acq = dep.getDefiningOp<DbAcquireOp>();
    if (!acq || !acq.getReplicatedReadAttr() || !hasFullSourceGrid(acq))
      continue;
    std::optional<SmallVector<int64_t, 4>> depMap =
        readI64ArrayAttr(dyn_cast<ArrayAttr>(depMaps[idx]));
    if (depMap && hasSingleReductionDimMarker(*depMap))
      return true;
  }
  return false;
}

/// Identify an owner-block contraction EDT eligible for contraction split.
static std::optional<BlockContractionTarget> matchTarget(EdtOp edt) {
  if (!edt.getPartialReductionAttr() || !edt.getPartialReductionDimsAttr() ||
      !edt.getPartialReductionDepResultDimMapsAttr())
    return std::nullopt;
  auto strategy = edt.getReductionStrategyAttr();
  if (!strategy ||
      strategy.getValue() != ArtsReductionStrategy::local_accumulate)
    return std::nullopt;
  ModuleOp module = edt->getParentOfType<ModuleOp>();
  if (!module || !hasArtsInterNodeRuntime(module))
    return std::nullopt;

  // Owner dispatch loop: enclosing scf.for whose IV is an EDT param.
  scf::ForOp ownerLoop;
  for (Operation *p = edt->getParentOp(); p; p = p->getParentOp())
    if (auto loop = dyn_cast<scf::ForOp>(p))
      if (llvm::is_contained(edt.getParams(), loop.getInductionVar())) {
        ownerLoop = loop;
        break;
      }
  if (!ownerLoop)
    return std::nullopt;

  auto depMaps = edt.getPartialReductionDepResultDimMapsAttr();
  if (depMaps.size() != edt.getDependencies().size())
    return std::nullopt;
  auto ownerDims = readI64ArrayAttr(edt.getPartialReductionOwnerDimsAttr());
  if (!ownerDims)
    return std::nullopt;
  auto readDepMap =
      [](ArrayAttr maps,
         unsigned depIndex) -> std::optional<SmallVector<int64_t, 4>> {
    if (depIndex >= maps.size())
      return std::nullopt;
    return readI64ArrayAttr(dyn_cast<ArrayAttr>(maps[depIndex]));
  };
  auto mapCoversOwnerDims = [&](ArrayRef<int64_t> map) {
    for (int64_t ownerDim : *ownerDims)
      if (!llvm::is_contained(map, ownerDim))
        return false;
    return true;
  };
  // Locate the full-grid `replicatedRead` <in> dep + its matching all-gather
  // replica using committed reduction maps and the ARTS all-gather graph.
  std::optional<unsigned> replicatedDep;
  DbAllocOp replicaAlloc;
  for (auto [idx, dep] : llvm::enumerate(edt.getDependencies())) {
    DbAcquireOp acq = getDepAcquire(edt, static_cast<unsigned>(idx));
    if (!acq || !acq.getReplicatedReadAttr())
      continue;
    if (!hasFullSourceGrid(acq))
      continue;
    DbAllocOp sourceAlloc = acq.getSourcePtr().getDefiningOp<DbAllocOp>();
    DbAllocOp replica = findMatchingReplica(module, sourceAlloc);
    if (!replica)
      continue;
    unsigned depIndex = static_cast<unsigned>(idx);
    std::optional<SmallVector<int64_t, 4>> depMap =
        readDepMap(depMaps, depIndex);
    if (!depMap || !hasSingleReductionDimMarker(*depMap))
      continue;
    if (replicatedDep)
      return std::nullopt; // ambiguous.
    replicatedDep = static_cast<unsigned>(idx);
    replicaAlloc = replica;
  }
  if (!replicatedDep)
    return std::nullopt;

  // Result dep: the single block <inout> dependency whose reduction map covers
  // all committed owner dims. All block <in> dependencies are remapped into
  // each producer unchanged.
  std::optional<unsigned> resultDep;
  SmallVector<BlockInputDep, 4> inputDeps;
  for (auto [idx, dep] : llvm::enumerate(edt.getDependencies())) {
    unsigned depIndex = static_cast<unsigned>(idx);
    if (depIndex == *replicatedDep)
      continue;
    DbAcquireOp acq = getDepAcquire(edt, static_cast<unsigned>(idx));
    if (!acq)
      return std::nullopt;
    std::optional<PartitionMode> pm = acq.getPartitionMode();
    if (!pm || *pm != PartitionMode::block)
      continue;
    if (acq.getMode() == ArtsMode::inout) {
      std::optional<SmallVector<int64_t, 4>> depMap =
          readDepMap(depMaps, depIndex);
      if (!depMap || !mapCoversOwnerDims(*depMap))
        continue;
      if (resultDep)
        return std::nullopt;
      resultDep = static_cast<unsigned>(idx);
    } else if (acq.getMode() == ArtsMode::in) {
      DbAllocOp alloc = acq.getSourcePtr().getDefiningOp<DbAllocOp>();
      if (!alloc || acq.getOffsets().empty())
        return std::nullopt;
      inputDeps.push_back({depIndex, acq, alloc});
    } else {
      return std::nullopt;
    }
  }
  if (!resultDep)
    return std::nullopt;
  DbAllocOp resultAlloc = getDepAlloc(edt, *resultDep);
  if (!resultAlloc || !isa<FloatType>(resultAlloc.getElementType()))
    return std::nullopt;

  // Replica geometry: owner-block extent along the contraction dim + block
  // count.
  std::optional<int64_t> tileExtent = inferSingleOwnerBlockExtent(replicaAlloc);
  if (!tileExtent)
    return std::nullopt;
  std::optional<int64_t> numTiles =
      ValueAnalysis::tryFoldConstantIndex(replicaAlloc.getSizes().front());
  if (!numTiles || *numTiles <= 0)
    return std::nullopt;
  std::optional<int64_t> totalNodes = arts::getRuntimeTotalNodes(module);
  if (!totalNodes || *totalNodes <= 1)
    return std::nullopt;

  BlockContractionTarget t;
  t.consumerEdt = edt;
  t.ownerLoop = ownerLoop;
  t.resultDep = *resultDep;
  t.inputDeps = std::move(inputDeps);
  t.replicatedDep = *replicatedDep;
  t.replicaAlloc = replicaAlloc;
  t.tileExtent = *tileExtent;
  t.numTiles = *numTiles;
  if (failed(findReplicatedContractionAccesses(edt, *replicatedDep, t)))
    return std::nullopt;
  return t;
}

/// Per-(owner-block, replica-tile) partials DB: a single block DB whose outer
/// extent is `numTiles` and whose element block mirrors the settled-block
/// footprint.
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
  // Replicated-local: every node holds all per-tile partials for its owned
  // block, exactly like the all-gather replica it consumes.
  db.setLocalOnlyAttr(UnitAttr::get(db.getContext()));
  db.setPerBlockReplicatedAttr(UnitAttr::get(db.getContext()));
  return db;
}

/// Emit one per-tile contraction producer EDT. The EDT acquires OUTSIDE its
/// body: the replica strip for tile `tileIdx` (<in>), every original block
/// input
/// (<in>), and the partial tile `tileIdx` (<out>). Its body recomputes the
/// contraction only for that tile, accumulating into the partial.
static LogicalResult emitTileProducer(OpBuilder &builder, Location loc,
                                      BlockContractionTarget &t,
                                      int64_t tileIdx, DbAllocOp partialsDb,
                                      Value ownerOrdinal) {
  ModuleOp module = t.consumerEdt->getParentOfType<ModuleOp>();
  Value one = createOneIndex(builder, loc);
  Value tileVal = createConstantIndex(builder, loc, tileIdx);

  // OUTSIDE-the-EDT acquires (block-arg deps).
  DbAcquireOp partialAcq = emitSingleBlockAcquire(builder, loc, partialsDb,
                                                  ArtsMode::out, tileVal, one);
  SmallVector<Value> deps{partialAcq.getPtr()};
  deps.reserve(t.inputDeps.size() + 2);
  for (BlockInputDep &input : t.inputDeps) {
    if (input.acquire.getOffsets().empty())
      return failure();
    SmallVector<Value> offsets(input.acquire.getOffsets().begin(),
                               input.acquire.getOffsets().end());
    SmallVector<Value> sizes(input.acquire.getSizes().begin(),
                             input.acquire.getSizes().end());
    if (sizes.empty())
      sizes.assign(offsets.size(), one);
    if (offsets.size() != sizes.size())
      return failure();
    DbAcquireOp inputBlock = emitBlockAcquire(builder, loc, input.alloc,
                                              ArtsMode::in, offsets, sizes);
    deps.push_back(inputBlock.getPtr());
  }
  DbAcquireOp replicaStrip = emitSingleBlockAcquire(
      builder, loc, t.replicaAlloc, ArtsMode::in, tileVal, one);
  unsigned replicaBodyArgIndex = deps.size();
  deps.push_back(replicaStrip.getPtr());

  // Carry the original worker-shape params so the cloned body keeps the same
  // output-block iteration bounds.
  SmallVector<Value> params(t.consumerEdt.getParams().begin(),
                            t.consumerEdt.getParams().end());

  ArtsLaunchPolicy launch =
      resolveArtsOrdinalLaunchPolicy(module, ownerOrdinal, builder, loc);
  Value route =
      launch.route ? launch.route : createCurrentNodeRoute(builder, loc);
  auto producer = EdtOp::create(builder, loc, EdtType::task, launch.concurrency,
                                route, deps, params);

  Block &body = producer.getBody().front();
  for (Value dep : deps)
    body.addArgument(dep.getType(), loc);
  for (Value param : params)
    body.addArgument(param.getType(), loc);

  // Clone the original body into the producer. The producer body args are
  // [partial, block inputs..., replica strip, params...]; the original body
  // args are [deps..., params...].
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(&body);
  // Tile bounds are created first so they dominate the cloned contraction loop
  // and the reindexed replicated-operand loads beneath it.
  Value tileBase = createConstantIndex(builder, loc, tileIdx * t.tileExtent);
  Value tileEnd =
      createConstantIndex(builder, loc, (tileIdx + 1) * t.tileExtent);

  Block &consumerBody = t.consumerEdt.getBody().front();
  IRMapping mapper;
  mapper.map(consumerBody.getArgument(t.resultDep), body.getArgument(0));
  for (auto [slot, input] : llvm::enumerate(t.inputDeps))
    mapper.map(consumerBody.getArgument(input.depIndex),
               body.getArgument(1 + slot));
  mapper.map(consumerBody.getArgument(t.replicatedDep),
             body.getArgument(replicaBodyArgIndex));
  for (unsigned pi = 0, pe = t.consumerEdt.getParams().size(); pi < pe; ++pi)
    mapper.map(
        consumerBody.getArgument(t.consumerEdt.getDependencies().size() + pi),
        body.getArgument(deps.size() + pi));
  for (Operation &op : consumerBody.without_terminator())
    builder.clone(op, mapper);

  // Constrain this producer's contraction loop to its tile range
  // [tileIdx*tileExtent, (tileIdx+1)*tileExtent) and reindex replicated-payload
  // loads from whole-replica coordinates to tile-local coordinates.
  auto clonedContractionLoop = dyn_cast_or_null<scf::ForOp>(
      mapper.lookupOrNull(t.contractionLoop.getOperation()));
  if (!clonedContractionLoop)
    return failure();
  clonedContractionLoop.setLowerBound(tileBase);
  clonedContractionLoop.setUpperBound(tileEnd);

  Value clonedReplicaPayload =
      mapper.lookupOrNull(getDepPayload(consumerBody, t.replicatedDep));
  if (!clonedReplicaPayload)
    return failure();
  SmallVector<memref::LoadOp> replicaLoads;
  clonedContractionLoop->walk([&](memref::LoadOp ld) {
    if (ld.getMemRef() == clonedReplicaPayload &&
        t.replicatedContractionDim < ld.getIndices().size())
      replicaLoads.push_back(ld);
  });
  for (memref::LoadOp ld : replicaLoads) {
    OpBuilder::InsertionGuard lg(builder);
    builder.setInsertionPoint(ld);
    SmallVector<Value> indices(ld.getIndices().begin(), ld.getIndices().end());
    indices[t.replicatedContractionDim] = arith::SubIOp::create(
        builder, loc, indices[t.replicatedContractionDim], tileBase);
    auto fixed =
        memref::LoadOp::create(builder, loc, clonedReplicaPayload, indices);
    ld.replaceAllUsesWith(fixed.getResult());
    ld.erase();
  }

  // Each tile writes its own partial DB and keeps the original zero-init/update
  // body shape, so the per-tile contribution is exact; the settle sums the
  // partials.
  YieldOp::create(builder, loc);
  return success();
}

/// Emit the per-block summing settle: sum the partial tiles into the settled
/// block (the original result DB), written <out> once.
static LogicalResult emitSettle(OpBuilder &builder, Location loc,
                                BlockContractionTarget &t, DbAllocOp partialsDb,
                                Value ownerOrdinal) {
  ModuleOp module = t.consumerEdt->getParentOfType<ModuleOp>();
  DbAllocOp resultAlloc = getDepAlloc(t.consumerEdt, t.resultDep);
  DbAcquireOp resultAcq = getDepAcquire(t.consumerEdt, t.resultDep);
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
    DbAcquireOp partialAcq = emitSingleBlockAcquire(builder, loc, partialsDb,
                                                    ArtsMode::in, tileVal, one);
    deps.push_back(partialAcq.getPtr());
  }
  DbAcquireOp dstAcq = emitSingleBlockAcquire(builder, loc, resultAlloc,
                                              ArtsMode::out, resultOffset, one);
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
    emitSumNest(builder, loc, partialPayloads, dstPayload, bodyCopySizes);
    YieldOp::create(builder, loc);
  }
  return success();
}

static LogicalResult splitTarget(BlockContractionTarget &t) {
  EdtOp consumerEdt = t.consumerEdt;
  Location loc = consumerEdt.getLoc();
  DbAllocOp resultAlloc = getDepAlloc(consumerEdt, t.resultDep);
  if (!resultAlloc)
    return failure();

  // Partials DB is allocated INSIDE the owner-block dispatch loop, immediately
  // before the original EDT, so each owner-block iteration reserves its own
  // per-tile partial GUIDs with no cross-block aliasing.
  OpBuilder builder(consumerEdt);
  DbAllocOp partialsDb =
      createPartialsDb(builder, loc, resultAlloc, t.numTiles);

  // Owner ordinal for routing = the owner index used by the result acquire
  // offset (the relative dispatch position).
  builder.setInsertionPoint(consumerEdt);
  DbAcquireOp resultAcq = getDepAcquire(consumerEdt, t.resultDep);
  Value ownerOrdinal = resultAcq && !resultAcq.getOffsets().empty()
                           ? resultAcq.getOffsets().front()
                           : createZeroIndex(builder, loc);

  // Per-tile producers.
  for (int64_t tile = 0; tile < t.numTiles; ++tile) {
    builder.setInsertionPoint(consumerEdt);
    if (failed(
            emitTileProducer(builder, loc, t, tile, partialsDb, ownerOrdinal)))
      return failure();
  }

  // Summing settle into the settled block.
  builder.setInsertionPoint(consumerEdt);
  if (failed(emitSettle(builder, loc, t, partialsDb, ownerOrdinal)))
    return failure();

  // Retire the original coarse-contraction EDT and its now-dead acquires.
  SmallVector<DbAcquireOp> acquires;
  for (unsigned i = 0, e = consumerEdt.getDependencies().size(); i < e; ++i)
    if (DbAcquireOp acq = getDepAcquire(consumerEdt, i))
      acquires.push_back(acq);
  consumerEdt.erase();
  for (DbAcquireOp acq : acquires)
    if (acq.getPtr().use_empty() &&
        (!acq.getGuid() || acq.getGuid().use_empty()))
      acq.erase();
  return success();
}

struct BlockContractionSplitPass
    : public impl::BlockContractionSplitBase<BlockContractionSplitPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    SmallVector<BlockContractionTarget, 2> targets;
    bool hasFailure = false;
    module.walk(
        [&](EdtOp edt) {
          if (std::optional<BlockContractionTarget> t = matchTarget(edt)) {
            targets.push_back(*t);
            return;
          }
          if (requiresBlockContractionSplit(edt)) {
            edt.emitOpError()
                << "has an unsplit full-grid replicated-read contraction "
                   "dependency; ARTS must create per-tile producers before "
                   "lowering";
            hasFailure = true;
          }
        });
    if (hasFailure) {
      signalPassFailure();
      return;
    }
    for (BlockContractionTarget &t : targets)
      if (failed(splitTarget(t))) {
        t.consumerEdt.emitError() << "failed to split block contraction";
        signalPassFailure();
        return;
      }
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::arts::createBlockContractionSplitPass() {
  return std::make_unique<BlockContractionSplitPass>();
}
