///==========================================================================///
/// File: PartialReductionSplit.cpp
///
/// Splits ARTS partial-reduction facts into concrete DB/EDT structure before
/// ARTS-RT lowering.
///==========================================================================///

#define GEN_PASS_DEF_PARTIALREDUCTIONSPLIT
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/dialect/sde/Utils/IterationSizingUtils.h"
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

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

struct ReductionLoopMatch {
  DbRefOp resultRef;
  scf::ForOp loop;
  memref::StoreOp resultStore;
  arith::AddFOp add;
  memref::LoadOp seedLoad;
};

struct SplitFacts {
  unsigned resultDepIndex = 0;
  unsigned ownerParamIndex = 0;
  int64_t splitFactor = 0;
  int64_t ownerTaskCount = 0;
  int64_t targetWorkerCount = 0;
  int64_t resultElementCount = 1;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> reductionDims;
  SmallVector<int64_t, 4> splitDims;
  ReductionLoopMatch reduction;
};

static bool isInEdtBody(EdtOp parent, Operation *op) {
  EdtOp enclosing = op ? op->getParentOfType<EdtOp>() : EdtOp{};
  return enclosing == parent;
}

static Value createScalarZero(OpBuilder &builder, Location loc, Type type) {
  if (auto floatType = dyn_cast<FloatType>(type))
    return arith::ConstantOp::create(builder, loc, type,
                                     builder.getFloatAttr(floatType, 0.0));
  return {};
}

static FailureOr<SmallVector<Value, 4>>
rebuildDominatingIndices(ValueRange indices, Operation *insertBefore,
                         OpBuilder &builder, Location loc) {
  Operation *domRoot = insertBefore->getParentOfType<func::FuncOp>();
  if (!domRoot)
    domRoot = insertBefore->getParentOfType<ModuleOp>();
  if (!domRoot)
    return failure();

  DominanceInfo domInfo(domRoot);
  SmallVector<Value, 4> dominatingIndices;
  dominatingIndices.reserve(indices.size());
  for (Value index : indices) {
    Value rebuilt = ValueAnalysis::traceValueToDominating(
        index, insertBefore, builder, domInfo, loc);
    if (!rebuilt)
      return failure();
    dominatingIndices.push_back(rebuilt);
  }
  return dominatingIndices;
}

static bool hasMultinodeRuntime(EdtOp edt) {
  ModuleOp module = edt->getParentOfType<ModuleOp>();
  std::optional<int64_t> totalNodes = arts::getRuntimeTotalNodes(module);
  return totalNodes && *totalNodes > 1;
}

static std::optional<int64_t> getRuntimeWorkersPerNode(EdtOp edt) {
  ModuleOp module = edt->getParentOfType<ModuleOp>();
  std::optional<int64_t> totalWorkers = arts::getRuntimeTotalWorkers(module);
  std::optional<int64_t> totalNodes = arts::getRuntimeTotalNodes(module);
  if (!totalWorkers || !totalNodes || *totalWorkers <= 0 || *totalNodes <= 0)
    return std::nullopt;
  return (*totalWorkers + *totalNodes - 1) / *totalNodes;
}

static LogicalResult inferResultElementCount(EdtOp edt, SplitFacts &facts) {
  if (facts.resultDepIndex >= edt.getDependencies().size())
    return failure();
  auto acquire =
      edt.getDependencies()[facts.resultDepIndex].getDefiningOp<DbAcquireOp>();
  if (!acquire)
    return failure();
  auto alloc = dyn_cast_or_null<DbAllocOp>(
      DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr()));
  if (!alloc || alloc.getElementSizes().empty())
    return failure();
  std::optional<int64_t> elementCount = ValueAnalysis::tryFoldConstantIndex(
      ValueAnalysis::stripNumericCasts(alloc.getElementSizes().front()));
  if (!elementCount || *elementCount <= 0)
    return edt.emitOpError()
           << "partial-reduction split requires a static result element "
              "count from the result dependency DB";
  facts.resultElementCount = *elementCount;
  return success();
}

static void clearSplitAttrs(EdtOp edt) {
  edt->removeAttr(edt.getPartialReductionSplitRequiredAttrName());
  edt->removeAttr(edt.getPartialReductionSplitDimsAttrName());
  edt->removeAttr(edt.getPartialReductionSplitFactorAttrName());
  edt->removeAttr(edt.getPartialReductionSplitOwnerTaskCountAttrName());
  edt->removeAttr(edt.getPartialReductionSplitTargetWorkerCountAttrName());
}

static void recordEffectiveSplitAttrs(EdtOp edt, const SplitFacts &facts) {
  MLIRContext *ctx = edt.getContext();
  auto i64 = IntegerType::get(ctx, 64);
  edt.setPartialReductionSplitFactorAttr(
      IntegerAttr::get(i64, facts.splitFactor));
  edt.setPartialReductionSplitTargetWorkerCountAttr(
      IntegerAttr::get(i64, facts.targetWorkerCount));
}

static bool reconcileSplitTopology(EdtOp edt, SplitFacts &facts,
                                   bool &distributedTopology) {
  bool multinode = hasMultinodeRuntime(edt);
  bool depsAllowDistributed =
      !DbUtils::hasLocalOnlyDistributedLaunchDependency(edt);
  distributedTopology = multinode && depsAllowDistributed;
  if (distributedTopology || !multinode)
    return true;

  std::optional<int64_t> localWorkers = getRuntimeWorkersPerNode(edt);
  if (!localWorkers || *localWorkers <= 0)
    return true;

  int64_t effectiveTarget = *localWorkers;
  if (effectiveTarget > facts.targetWorkerCount)
    effectiveTarget = facts.targetWorkerCount;
  if (effectiveTarget <= facts.ownerTaskCount) {
    clearSplitAttrs(edt);
    return false;
  }

  int64_t requestedFactor =
      sde::ceilDivPositive(effectiveTarget, facts.ownerTaskCount);
  if (requestedFactor <= 1) {
    clearSplitAttrs(edt);
    return false;
  }
  if (requestedFactor < facts.splitFactor)
    facts.splitFactor = requestedFactor;
  facts.targetWorkerCount = effectiveTarget;
  recordEffectiveSplitAttrs(edt, facts);
  return true;
}

static Value createOwnerOrdinal(OpBuilder &builder, Location loc,
                                scf::ForOp ownerLoop, Value ownerIndex) {
  Value relative = arith::SubIOp::create(builder, loc, ownerIndex,
                                         ownerLoop.getLowerBound());
  return arith::DivUIOp::create(builder, loc, relative, ownerLoop.getStep());
}

static Value createDistributedRoute(OpBuilder &builder, Location loc,
                                    scf::ForOp ownerLoop, Value ownerIndex,
                                    Value tileIndex, int64_t tileCount) {
  Value ownerOrdinal = createOwnerOrdinal(builder, loc, ownerLoop, ownerIndex);
  Value tileCountValue = createConstantIndex(builder, loc, tileCount);
  Value ownerBase =
      arith::MulIOp::create(builder, loc, ownerOrdinal, tileCountValue);
  Value linearIndex = arith::AddIOp::create(builder, loc, ownerBase, tileIndex);
  Value linearIndexI32 = arith::IndexCastOp::create(
      builder, loc, builder.getI32Type(), linearIndex);
  auto totalNodes =
      RuntimeQueryOp::create(builder, loc, RuntimeQueryKind::totalNodes);
  return arith::RemUIOp::create(builder, loc, linearIndexI32,
                                totalNodes.getResult());
}

static void markDistributedRemoteUse(Value dep) {
  Operation *underlying = DbUtils::getUnderlyingDbAlloc(dep);
  if (auto alloc = dyn_cast_or_null<DbAllocOp>(underlying))
    alloc.removeLocalOnlyAttr();
}

static void markReductionSplitDistribution(Operation *op, bool distributed) {
  if (!op)
    return;
  setEdtDistributionPattern(op, EdtDistributionPattern::uniform);
  setDistributionVersion(op, 1);
  if (distributed)
    setEdtDistributionKind(op, EdtDistributionKind::block_cyclic);
}

static LogicalResult readRequiredI64Array(EdtOp edt, ArrayAttr attr,
                                          StringRef name,
                                          SmallVectorImpl<int64_t> &values) {
  auto parsed = readI64ArrayAttr(attr);
  if (!parsed)
    return edt.emitOpError() << "requires integer array attribute '" << name
                             << "' for partial-reduction split";
  values.assign(parsed->begin(), parsed->end());
  return success();
}

static LogicalResult findResultDependencyIndex(EdtOp edt, SplitFacts &facts) {
  ArrayAttr depMaps = edt.getPartialReductionDepResultDimMapsAttr();
  if (!depMaps)
    return edt.emitOpError()
           << "requires partialReductionDepResultDimMaps to identify the "
              "partial-reduction result dependency";
  if (depMaps.size() != edt.getDependencies().size())
    return edt.emitOpError()
           << "partialReductionDepResultDimMaps entry count (" << depMaps.size()
           << ") must match dependency count (" << edt.getDependencies().size()
           << ")";

  std::optional<unsigned> resultIndex;
  for (auto [idx, attr] : llvm::enumerate(depMaps)) {
    auto mapAttr = dyn_cast<ArrayAttr>(attr);
    auto dims = readI64ArrayAttr(mapAttr);
    if (!dims)
      return edt.emitOpError() << "partialReductionDepResultDimMaps entry #"
                               << idx << " must be an integer array";
    if (dims->empty())
      continue;
    if (llvm::is_contained(*dims, -1))
      continue;

    bool coversOwnerDims = true;
    for (int64_t ownerDim : facts.ownerDims)
      coversOwnerDims &= llvm::is_contained(*dims, ownerDim);
    if (!coversOwnerDims)
      continue;

    if (auto acquire = edt.getDependencies()[idx].getDefiningOp<DbAcquireOp>())
      if (acquire.getMode() == ArtsMode::in)
        continue;

    if (resultIndex)
      return edt.emitOpError() << "partial-reduction split found multiple "
                                  "candidate result dependencies";
    resultIndex = static_cast<unsigned>(idx);
  }

  if (!resultIndex)
    return edt.emitOpError()
           << "partial-reduction split could not identify a "
              "result dependency with owner dims and no reduction-only dims";
  facts.resultDepIndex = *resultIndex;
  return success();
}

static FailureOr<unsigned> findOwnerParamIndex(EdtOp edt) {
  for (Operation *parent = edt->getParentOp(); parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (!loop)
      continue;
    for (auto [idx, param] : llvm::enumerate(edt.getParams())) {
      if (param == loop.getInductionVar())
        return static_cast<unsigned>(idx);
    }
  }
  return failure();
}

static FailureOr<DbRefOp> findSingleResultRef(EdtOp edt,
                                              unsigned resultDepIndex) {
  Block &body = edt.getBody().front();
  if (resultDepIndex >= body.getNumArguments())
    return failure();
  BlockArgument resultArg = body.getArgument(resultDepIndex);

  SmallVector<DbRefOp, 2> refs;
  edt.getBody().walk([&](DbRefOp ref) {
    if (!isInEdtBody(edt, ref.getOperation()))
      return;
    if (ref.getSource() == resultArg)
      refs.push_back(ref);
  });

  if (refs.size() != 1)
    return failure();
  return refs.front();
}

static FailureOr<ReductionLoopMatch> matchReductionLoop(EdtOp edt,
                                                        SplitFacts facts) {
  FailureOr<DbRefOp> resultRef = findSingleResultRef(edt, facts.resultDepIndex);
  if (failed(resultRef))
    return failure();

  auto resultType = dyn_cast<MemRefType>((*resultRef).getResult().getType());
  if (!resultType || !isa<FloatType>(resultType.getElementType()))
    return failure();

  SmallVector<ReductionLoopMatch, 2> matches;
  edt.getBody().walk([&](scf::ForOp loop) {
    if (!isInEdtBody(edt, loop.getOperation()))
      return;

    SmallVector<memref::StoreOp, 2> stores;
    loop.getBody()->walk([&](memref::StoreOp store) {
      if (!isInEdtBody(edt, store.getOperation()))
        return;
      if (store.getMemref() == (*resultRef).getResult())
        stores.push_back(store);
    });
    if (stores.size() != 1)
      return;

    auto add = stores.front().getValue().getDefiningOp<arith::AddFOp>();
    if (!add)
      return;

    memref::LoadOp seedLoad;
    for (Value operand : add->getOperands()) {
      auto load = operand.getDefiningOp<memref::LoadOp>();
      if (load && load.getMemref() == (*resultRef).getResult()) {
        if (seedLoad)
          return;
        seedLoad = load;
      }
    }
    if (!seedLoad)
      return;

    matches.push_back({*resultRef, loop, stores.front(), add, seedLoad});
  });

  SmallVector<ReductionLoopMatch, 2> rootMatches;
  for (ReductionLoopMatch candidate : matches) {
    bool nestedInAnotherMatch = false;
    for (ReductionLoopMatch other : matches) {
      if (candidate.loop == other.loop)
        continue;
      if (other.loop->isProperAncestor(candidate.loop.getOperation())) {
        nestedInAnotherMatch = true;
        break;
      }
    }
    if (!nestedInAnotherMatch)
      rootMatches.push_back(candidate);
  }

  if (rootMatches.size() != 1)
    return failure();
  return rootMatches.front();
}

static LogicalResult validateSplitFacts(EdtOp edt, SplitFacts &facts) {
  if (!edt.getPartialReductionAttr())
    return edt.emitOpError()
           << "partialReductionSplitRequired requires partialReduction";

  auto splitFactor = edt.getPartialReductionSplitFactor();
  if (!splitFactor || *splitFactor <= 1)
    return edt.emitOpError()
           << "requires static partialReductionSplitFactor > 1";
  facts.splitFactor = *splitFactor;

  auto ownerTaskCount = edt.getPartialReductionSplitOwnerTaskCount();
  if (!ownerTaskCount || *ownerTaskCount <= 0)
    return edt.emitOpError()
           << "requires positive partialReductionSplitOwnerTaskCount";
  facts.ownerTaskCount = *ownerTaskCount;

  auto targetWorkerCount = edt.getPartialReductionSplitTargetWorkerCount();
  if (!targetWorkerCount || *targetWorkerCount <= 0)
    return edt.emitOpError()
           << "requires positive partialReductionSplitTargetWorkerCount";
  facts.targetWorkerCount = *targetWorkerCount;

  if (failed(readRequiredI64Array(edt, edt.getPartialReductionOwnerDimsAttr(),
                                  "partialReductionOwnerDims",
                                  facts.ownerDims)) ||
      failed(readRequiredI64Array(edt, edt.getPartialReductionDimsAttr(),
                                  "partialReductionDims",
                                  facts.reductionDims)) ||
      failed(readRequiredI64Array(edt, edt.getPartialReductionSplitDimsAttr(),
                                  "partialReductionSplitDims",
                                  facts.splitDims)))
    return failure();

  if (facts.ownerDims.size() != 1 || facts.reductionDims.empty() ||
      facts.splitDims.empty())
    return edt.emitOpError()
           << "partial-reduction split currently supports "
              "exactly one owner dim and one or more reduction/split dims";
  for (int64_t splitDim : facts.splitDims) {
    if (llvm::is_contained(facts.reductionDims, splitDim))
      continue;
    return edt.emitOpError()
           << "partialReductionSplitDims must be contained in "
              "partialReductionDims for "
              "the supported static split shape";
  }

  if (edt.getDependencies().size() < 2)
    return edt.emitOpError()
           << "partial-reduction split requires a result "
              "dependency and at least one reduction input dependency";

  if (failed(findResultDependencyIndex(edt, facts)))
    return failure();

  FailureOr<unsigned> ownerParamIndex = findOwnerParamIndex(edt);
  if (failed(ownerParamIndex))
    return edt.emitOpError()
           << "partial-reduction split requires an enclosing "
              "owner dispatch loop whose induction variable is an EDT param";
  facts.ownerParamIndex = *ownerParamIndex;

  FailureOr<ReductionLoopMatch> reduction = matchReductionLoop(edt, facts);
  if (failed(reduction))
    return edt.emitOpError() << "partial-reduction split could not prove the "
                                "supported scalar floating add reduction loop";
  facts.reduction = *reduction;

  if (failed(inferResultElementCount(edt, facts)))
    return failure();

  return success();
}

static void copySplitEdtFacts(EdtOp source, EdtOp dest) {
  MLIRContext *ctx = dest.getContext();
  if (source.getInPlaceSafeAttr())
    dest.setInPlaceSafeAttr(UnitAttr::get(ctx));
  if (source.getInPlaceSharedStateAttr())
    dest.setInPlaceSharedStateAttr(UnitAttr::get(ctx));
  if (auto attr = source.getInterleaveCountAttr())
    dest.setInterleaveCountAttr(attr);
  if (auto attr = source.getDepPatternAttr())
    dest.setDepPatternAttr(attr);
  inheritDistributionAttrs(source.getOperation(), dest.getOperation());
  if (auto attr = source.getReductionStrategyAttr())
    dest.setReductionStrategyAttr(attr);
  if (source.getPartialReductionAttr())
    dest.setPartialReductionAttr(UnitAttr::get(ctx));
  if (auto attr = source.getPartialReductionDimsAttr())
    dest.setPartialReductionDimsAttr(attr);
  if (auto attr = source.getPartialReductionOwnerDimsAttr())
    dest.setPartialReductionOwnerDimsAttr(attr);
  if (auto attr = source.getPartialReductionDepResultDimMapsAttr())
    dest.setPartialReductionDepResultDimMapsAttr(attr);
}

static void addEdtBlockArguments(EdtOp edt, ValueRange deps, ValueRange params,
                                 Location loc) {
  Block &body = edt.getBody().front();
  for (Value dep : deps)
    body.addArgument(dep.getType(), loc);
  for (Value param : params)
    body.addArgument(param.getType(), loc);
}

static void cloneEdtBody(EdtOp source, EdtOp dest) {
  Block &sourceBlock = source.getBody().front();
  Block &destBlock = dest.getBody().front();

  IRMapping mapper;
  for (auto [index, sourceArg] : llvm::enumerate(sourceBlock.getArguments()))
    mapper.map(sourceArg, destBlock.getArgument(index));

  OpBuilder builder(dest.getContext());
  builder.setInsertionPointToStart(&destBlock);
  for (Operation &op : sourceBlock.without_terminator())
    builder.clone(op, mapper);
  YieldOp::create(builder, source.getLoc());
}

static LogicalResult retileSplitWorkerLoop(EdtOp splitEdt,
                                           const SplitFacts &facts,
                                           unsigned originalParamCount) {
  FailureOr<ReductionLoopMatch> match = matchReductionLoop(splitEdt, facts);
  if (failed(match))
    return failure();

  Location loc = splitEdt.getLoc();
  Block &body = splitEdt.getBody().front();
  Value tileArg =
      body.getArgument(splitEdt.getDependencies().size() + originalParamCount);

  OpBuilder loopBuilder((*match).loop);
  Value one = createOneIndex(loopBuilder, loc);
  Value splitFactor = createConstantIndex(loopBuilder, loc, facts.splitFactor);
  Value splitFactorMinusOne =
      createConstantIndex(loopBuilder, loc, facts.splitFactor - 1);
  Value range =
      arith::SubIOp::create(loopBuilder, loc, (*match).loop.getUpperBound(),
                            (*match).loop.getLowerBound());
  Value numerator =
      arith::AddIOp::create(loopBuilder, loc, range, splitFactorMinusOne);
  Value tileSize =
      arith::DivUIOp::create(loopBuilder, loc, numerator, splitFactor);
  Value tileOffset = arith::MulIOp::create(loopBuilder, loc, tileArg, tileSize);
  Value tileBegin = arith::AddIOp::create(
      loopBuilder, loc, (*match).loop.getLowerBound(), tileOffset);
  Value rawTileEnd =
      arith::AddIOp::create(loopBuilder, loc, tileBegin, tileSize);
  Value tileEnd = arith::MinUIOp::create(loopBuilder, loc, rawTileEnd,
                                         (*match).loop.getUpperBound());
  (*match).loop.setLowerBound(tileBegin);
  (*match).loop.setUpperBound(tileEnd);
  (*match).loop.setStep(one);

  auto resultType = cast<MemRefType>((*match).resultRef.getResult().getType());
  Value identity =
      createScalarZero(loopBuilder, loc, resultType.getElementType());
  if (!identity)
    return failure();

  OpBuilder addBuilder((*match).add);
  SmallVector<Value, 4> addLoadIndices((*match).seedLoad.getIndices().begin(),
                                       (*match).seedLoad.getIndices().end());
  FailureOr<SmallVector<Value, 4>> initIndices = rebuildDominatingIndices(
      addLoadIndices, (*match).loop.getOperation(), loopBuilder, loc);
  if (failed(initIndices))
    return failure();
  memref::StoreOp::create(loopBuilder, loc, identity,
                          (*match).resultRef.getResult(), *initIndices);
  Value current = memref::LoadOp::create(
      addBuilder, loc, (*match).resultRef.getResult(), addLoadIndices);
  for (OpOperand &operand : (*match).add->getOpOperands())
    if (operand.get() == (*match).seedLoad.getResult())
      operand.set(current);

  if (!(*match).seedLoad->use_empty()) {
    OpBuilder seedBuilder((*match).seedLoad);
    Value zero =
        createScalarZero(seedBuilder, loc, resultType.getElementType());
    if (!zero)
      return failure();
    (*match).seedLoad.getResult().replaceAllUsesWith(zero);
  }
  (*match).seedLoad.erase();

  return success();
}

static DbAcquireOp createTileAcquire(OpBuilder &builder, Location loc,
                                     DbAllocOp db, ArtsMode mode,
                                     Type resultDepType, Value ownerIndex,
                                     Value tileIndex) {
  SmallVector<Value> indices{ownerIndex, tileIndex};
  auto acquire = DbAcquireOp::create(builder, loc, mode, db.getGuid(),
                                     db.getPtr(), resultDepType,
                                     PartitionMode::block, std::move(indices),
                                     /*offsets=*/SmallVector<Value>{},
                                     /*sizes=*/SmallVector<Value>{},
                                     /*partitionIndices=*/SmallVector<Value>{},
                                     /*partitionOffsets=*/SmallVector<Value>{},
                                     /*partitionSizes=*/SmallVector<Value>{},
                                     /*boundsValid=*/Value{},
                                     /*elementOffsets=*/SmallVector<Value>{},
                                     /*elementSizes=*/SmallVector<Value>{});
  inheritDistributionAttrs(db.getOperation(), acquire.getOperation());
  if (auto depPattern = db.getDepPatternAttr())
    acquire.setDepPatternAttr(depPattern);
  return acquire;
}

static DbAcquireOp createPartialTileAcquire(OpBuilder &builder, Location loc,
                                            DbAllocOp partialDb,
                                            Type resultDepType,
                                            Value ownerIndex, Value tileIndex) {
  return createTileAcquire(builder, loc, partialDb, ArtsMode::out,
                           resultDepType, ownerIndex, tileIndex);
}

static FailureOr<DbAllocOp>
createReductionBufferDb(OpBuilder &builder, Location loc, Value route,
                        Value ownerCount, Value tileCount, Value elementCount,
                        Type scalarType, bool distributed) {
  auto db = DbAllocOp::create(
      builder, loc, ArtsMode::inout, route, DbAllocType::heap, DbMode::write,
      scalarType, SmallVector<Value>{ownerCount, tileCount},
      SmallVector<Value>{elementCount}, PartitionMode::block);
  db.setDepPatternAttr(
      ArtsDepPatternAttr::get(db.getContext(), ArtsDepPattern::reduction));
  markReductionSplitDistribution(db.getOperation(), distributed);
  if (distributed) {
    db.removeLocalOnlyAttr();
  }
  return db;
}

static Value createScalarTileRef(OpBuilder &builder, Location loc,
                                 Value depArg) {
  Value zero = createZeroIndex(builder, loc);
  return DbRefOp::create(builder, loc, depArg, SmallVector<Value>{zero})
      .getResult();
}

static void copyCombineMetadata(EdtOp source, EdtOp dest) {
  if (auto depPattern = source.getDepPatternAttr())
    dest.setDepPatternAttr(depPattern);
  if (auto reductionStrategy = source.getReductionStrategyAttr())
    dest.setReductionStrategyAttr(reductionStrategy);
  inheritDistributionAttrs(source.getOperation(), dest.getOperation());
}

static LogicalResult createIntermediateCombineBody(EdtOp combineEdt,
                                                   Type scalarType,
                                                   Value elementCount,
                                                   bool hasRightInput) {
  if (!isa<FloatType>(scalarType))
    return failure();

  Location loc = combineEdt.getLoc();
  Block &body = combineEdt.getBody().front();
  BlockArgument outputArg = body.getArgument(0);
  BlockArgument leftArg = body.getArgument(1);

  OpBuilder builder(combineEdt.getContext());
  builder.setInsertionPointToStart(&body);

  Value outputRef = createScalarTileRef(builder, loc, outputArg);
  Value leftRef = createScalarTileRef(builder, loc, leftArg);
  Value rightRef;
  if (hasRightInput)
    rightRef = createScalarTileRef(builder, loc, body.getArgument(2));

  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  auto loop = scf::ForOp::create(builder, loc, zero, elementCount, one);
  {
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToStart(loop.getBody());
    Value idx = loop.getInductionVar();
    SmallVector<Value> indices{idx};
    Value accumulated = memref::LoadOp::create(builder, loc, leftRef, indices);
    if (hasRightInput) {
      Value right = memref::LoadOp::create(builder, loc, rightRef, indices);
      accumulated = arith::AddFOp::create(builder, loc, accumulated, right);
    }
    memref::StoreOp::create(builder, loc, accumulated, outputRef, indices);
  }
  YieldOp::create(builder, loc);
  return success();
}

static LogicalResult createFinalCombineBody(EdtOp combineEdt, Type scalarType,
                                            Value elementCount,
                                            bool hasRightInput) {
  if (!isa<FloatType>(scalarType))
    return failure();

  Location loc = combineEdt.getLoc();
  Block &body = combineEdt.getBody().front();
  BlockArgument finalArg = body.getArgument(0);
  BlockArgument leftArg = body.getArgument(1);

  OpBuilder builder(combineEdt.getContext());
  builder.setInsertionPointToStart(&body);

  Value finalRef = createScalarTileRef(builder, loc, finalArg);
  Value leftRef = createScalarTileRef(builder, loc, leftArg);
  Value rightRef;
  if (hasRightInput)
    rightRef = createScalarTileRef(builder, loc, body.getArgument(2));

  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  auto loop = scf::ForOp::create(builder, loc, zero, elementCount, one);
  {
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToStart(loop.getBody());
    Value idx = loop.getInductionVar();
    SmallVector<Value> indices{idx};
    Value accumulated = memref::LoadOp::create(builder, loc, finalRef, indices);
    Value left = memref::LoadOp::create(builder, loc, leftRef, indices);
    accumulated = arith::AddFOp::create(builder, loc, accumulated, left);
    if (hasRightInput) {
      Value right = memref::LoadOp::create(builder, loc, rightRef, indices);
      accumulated = arith::AddFOp::create(builder, loc, accumulated, right);
    }
    memref::StoreOp::create(builder, loc, accumulated, finalRef, indices);
  }
  YieldOp::create(builder, loc);
  return success();
}

static LogicalResult createIntermediateCombineEdt(
    OpBuilder &builder, Location loc, EdtOp sourceEdt, DbAllocOp inputDb,
    DbAllocOp outputDb, Type resultDepType, Value ownerIndex, int64_t leftIndex,
    std::optional<int64_t> rightIndex, int64_t outputIndex, Type scalarType,
    Value elementCount, EdtConcurrency concurrency, Value route) {
  Value leftTile = createConstantIndex(builder, loc, leftIndex);
  Value outputTile = createConstantIndex(builder, loc, outputIndex);
  DbAcquireOp output = createTileAcquire(builder, loc, outputDb, ArtsMode::out,
                                         resultDepType, ownerIndex, outputTile);
  DbAcquireOp left = createTileAcquire(builder, loc, inputDb, ArtsMode::in,
                                       resultDepType, ownerIndex, leftTile);

  SmallVector<Value> deps{output.getPtr(), left.getPtr()};
  if (rightIndex) {
    Value rightTile = createConstantIndex(builder, loc, *rightIndex);
    DbAcquireOp right = createTileAcquire(builder, loc, inputDb, ArtsMode::in,
                                          resultDepType, ownerIndex, rightTile);
    deps.push_back(right.getPtr());
  }

  auto combineEdt = EdtOp::create(builder, loc, EdtType::task, concurrency,
                                  route, deps, ValueRange{});
  addEdtBlockArguments(combineEdt, deps, ValueRange{}, loc);
  copyCombineMetadata(sourceEdt, combineEdt);
  markReductionSplitDistribution(combineEdt.getOperation(),
                                 concurrency == EdtConcurrency::internode);
  return createIntermediateCombineBody(combineEdt, scalarType, elementCount,
                                       rightIndex.has_value());
}

static LogicalResult createFinalCombineEdt(OpBuilder &builder, Location loc,
                                           EdtOp sourceEdt, DbAllocOp inputDb,
                                           Value finalDep, Type resultDepType,
                                           Value ownerIndex, int64_t inputCount,
                                           Type scalarType, Value elementCount,
                                           EdtConcurrency concurrency,
                                           Value route) {
  if (inputCount < 1 || inputCount > 2)
    return failure();

  Value leftTile = createZeroIndex(builder, loc);
  DbAcquireOp left = createTileAcquire(builder, loc, inputDb, ArtsMode::in,
                                       resultDepType, ownerIndex, leftTile);

  SmallVector<Value> deps{finalDep, left.getPtr()};
  if (inputCount == 2) {
    Value rightTile = createOneIndex(builder, loc);
    DbAcquireOp right = createTileAcquire(builder, loc, inputDb, ArtsMode::in,
                                          resultDepType, ownerIndex, rightTile);
    deps.push_back(right.getPtr());
  }

  auto combineEdt = EdtOp::create(builder, loc, EdtType::task, concurrency,
                                  route, deps, ValueRange{});
  addEdtBlockArguments(combineEdt, deps, ValueRange{}, loc);
  copyCombineMetadata(sourceEdt, combineEdt);
  markReductionSplitDistribution(combineEdt.getOperation(),
                                 concurrency == EdtConcurrency::internode);
  // The final combine RO-acquires the per-tile partials and writes the result
  // block once, so downstream lowering can treat it as a block-native settle
  // rather than a shared-frontier <inout> accumulate.
  combineEdt.setPerBlockSummingSettleAttr(
      UnitAttr::get(combineEdt.getContext()));
  return createFinalCombineBody(combineEdt, scalarType, elementCount,
                                inputCount == 2);
}

static LogicalResult splitReductionFacts(EdtOp edt, SplitFacts &facts) {
  Location loc = edt.getLoc();
  OpBuilder builder(edt.getContext());

  scf::ForOp ownerLoop;
  for (Operation *parent = edt->getParentOp(); parent;
       parent = parent->getParentOp()) {
    auto loop = dyn_cast<scf::ForOp>(parent);
    if (!loop)
      continue;
    if (llvm::is_contained(edt.getParams(), loop.getInductionVar())) {
      ownerLoop = loop;
      break;
    }
  }
  if (!ownerLoop)
    return failure();

  auto resultType =
      cast<MemRefType>(facts.reduction.resultRef.getResult().getType());
  Type scalarType = resultType.getElementType();
  if (!isa<FloatType>(scalarType) || resultType.getRank() != 1)
    return failure();

  bool distributedTopology = false;
  if (!reconcileSplitTopology(edt, facts, distributedTopology))
    return success();
  EdtConcurrency splitConcurrency =
      distributedTopology ? EdtConcurrency::internode : edt.getConcurrency();

  builder.setInsertionPoint(ownerLoop);
  Value route = createCurrentNodeRoute(builder, loc);
  Value ownerCount = createConstantIndex(builder, loc, facts.ownerTaskCount);
  Value splitFactor = createConstantIndex(builder, loc, facts.splitFactor);
  Value resultElementCount =
      createConstantIndex(builder, loc, facts.resultElementCount);
  FailureOr<DbAllocOp> partialDbOr = createReductionBufferDb(
      builder, loc, route, ownerCount, splitFactor, resultElementCount,
      scalarType, distributedTopology);
  if (failed(partialDbOr))
    return failure();
  DbAllocOp partialDb = *partialDbOr;
  SmallVector<std::pair<DbAllocOp, int64_t>, 4> intermediateLevels;
  for (int64_t currentCount = facts.splitFactor; currentCount > 2;) {
    int64_t nextCount = (currentCount + 1) / 2;
    Value nextCountValue = createConstantIndex(builder, loc, nextCount);
    FailureOr<DbAllocOp> nextDbOr = createReductionBufferDb(
        builder, loc, route, ownerCount, nextCountValue, resultElementCount,
        scalarType, distributedTopology);
    if (failed(nextDbOr))
      return failure();
    intermediateLevels.push_back({*nextDbOr, nextCount});
    currentCount = nextCount;
  }

  Value ownerIndex = edt.getParams()[facts.ownerParamIndex];
  Type resultDepType = edt.getDependencies()[facts.resultDepIndex].getType();
  SmallVector<Value> originalDeps(edt.getDependencies().begin(),
                                  edt.getDependencies().end());
  SmallVector<Value> originalParams(edt.getParams().begin(),
                                    edt.getParams().end());
  unsigned originalParamCount = originalParams.size();
  if (distributedTopology)
    for (Value dep : originalDeps)
      markDistributedRemoteUse(dep);

  builder.setInsertionPoint(edt);
  Value ownerOrdinal = createOwnerOrdinal(builder, loc, ownerLoop, ownerIndex);
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value splitUpper = createConstantIndex(builder, loc, facts.splitFactor);
  auto splitLoop = scf::ForOp::create(builder, loc, zero, splitUpper, one);

  {
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToStart(splitLoop.getBody());
    Value tileIndex = splitLoop.getInductionVar();
    DbAcquireOp partialTile = createPartialTileAcquire(
        builder, loc, partialDb, resultDepType, ownerOrdinal, tileIndex);

    SmallVector<Value> splitDeps = originalDeps;
    splitDeps[facts.resultDepIndex] = partialTile.getPtr();
    SmallVector<Value> splitParams = originalParams;
    splitParams.push_back(tileIndex);

    Value splitRoute =
        distributedTopology
            ? createDistributedRoute(builder, loc, ownerLoop, ownerIndex,
                                     tileIndex, facts.splitFactor)
            : edt.getRoute();
    auto splitEdt = EdtOp::create(builder, loc, edt.getType(), splitConcurrency,
                                  splitRoute, splitDeps, splitParams);
    copySplitEdtFacts(edt, splitEdt);
    markReductionSplitDistribution(splitEdt.getOperation(),
                                   distributedTopology);
    addEdtBlockArguments(splitEdt, splitDeps, splitParams, loc);
    cloneEdtBody(edt, splitEdt);
    if (failed(retileSplitWorkerLoop(splitEdt, facts, originalParamCount)))
      return failure();
  }

  builder.setInsertionPointAfter(splitLoop);
  DbAllocOp currentDb = partialDb;
  int64_t currentCount = facts.splitFactor;
  for (auto [nextDb, nextCount] : intermediateLevels) {
    for (int64_t outputIndex = 0; outputIndex < nextCount; ++outputIndex) {
      int64_t leftIndex = outputIndex * 2;
      std::optional<int64_t> rightIndex;
      if (leftIndex + 1 < currentCount)
        rightIndex = leftIndex + 1;
      Value outputTile = createConstantIndex(builder, loc, outputIndex);
      Value combineRoute =
          distributedTopology
              ? createDistributedRoute(builder, loc, ownerLoop, ownerIndex,
                                       outputTile, nextCount)
              : edt.getRoute();
      if (failed(createIntermediateCombineEdt(
              builder, loc, edt, currentDb, nextDb, resultDepType, ownerOrdinal,
              leftIndex, rightIndex, outputIndex, scalarType,
              resultElementCount, splitConcurrency, combineRoute)))
        return failure();
    }
    currentDb = nextDb;
    currentCount = nextCount;
  }

  Value finalRoute =
      distributedTopology
          ? createDistributedRoute(builder, loc, ownerLoop, ownerIndex,
                                   createZeroIndex(builder, loc), 1)
          : edt.getRoute();
  if (failed(createFinalCombineEdt(
          builder, loc, edt, currentDb, originalDeps[facts.resultDepIndex],
          resultDepType, ownerOrdinal, currentCount, scalarType,
          resultElementCount, splitConcurrency, finalRoute)))
    return failure();

  edt.erase();
  return success();
}

struct PartialReductionSplitPass
    : public impl::PartialReductionSplitBase<PartialReductionSplitPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    SmallVector<EdtOp, 8> worklist;
    module.walk([&](EdtOp edt) {
      if (edt.getPartialReductionSplitRequiredAttr())
        worklist.push_back(edt);
    });

    for (EdtOp edt : worklist) {
      SplitFacts facts;
      if (failed(validateSplitFacts(edt, facts)) ||
          failed(splitReductionFacts(edt, facts))) {
        edt.emitError()
            << "failed to rewrite partial-reduction into DB/EDT structure";
        signalPassFailure();
        return;
      }
    }
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::arts::createPartialReductionSplitPass() {
  return std::make_unique<PartialReductionSplitPass>();
}
