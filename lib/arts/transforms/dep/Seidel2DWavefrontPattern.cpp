///==========================================================================///
/// File: Seidel2DWavefrontPattern.cpp
///
/// Rewrite Seidel-style in-place 2D stencils on ARTS IR into a tiled
/// wavefront schedule after OpenMP-to-ARTS conversion.
///
/// Before:
///   scf.for %t = ...
///     arts.edt <parallel> {
///       arts.for(%i = 1 .. N-1) {
///         scf.for %j = 1 .. M-1 {
///           A[%i, %j] = f(A[%i-1, %j-1], ..., A[%i+1, %j+1])
///         }
///       }
///     }
///
/// After:
///   scf.for %t = ...
///     scf.for %wave = 0 .. W {
///       arts.edt <parallel> {
///         arts.for(%tileRow = rowMin .. rowMax step tileRows) {
///           scf.for %i in tile
///             scf.for %j in tile
///               original update
///         }
///       }
///       arts.barrier
///     }
///
/// The weighted wavefront `wave = 2*tileI + tileJ` preserves the `(i-1,j+1)`
/// dependence that breaks plain anti-diagonals while keeping each wave's
/// parallel work as a top-level `arts.for` for the existing ForLowering path.
///
/// The inner `arts.for` iterates in row-space, not tile-index space. That lets
/// later acquire planning reuse the normal row-block reasoning instead of
/// rediscovering a tile-index to row-range mapping.
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/analysis/heuristics/PartitioningHeuristics.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/transforms/dep/DepTransform.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include <cmath>

using namespace mlir;
using namespace mlir::arts;

namespace mlir {
namespace arts {
namespace {

struct SeidelWavefrontMatch {
  EdtOp parallelEdt;
  ForOp rowFor;
  scf::ForOp innerLoop;
  Value rowIV;
  Value innerLowerBound;
  Value innerUpperBound;
  Value innerIV;
  Value stencilMemref;
  SmallVector<Operation *, 8> preludeOps;
};

static bool isUnitStep(Value step) {
  auto c = ValueAnalysis::tryFoldConstantIndex(step);
  return c && *c == 1;
}

static int64_t resolveWorkerCount(EdtOp parallelEdt) {
  if (!parallelEdt)
    return 1;
  if (auto workers = getWorkers(parallelEdt.getOperation()); workers && *workers > 0)
    return *workers;
  if (auto module = parallelEdt->getParentOfType<ModuleOp>()) {
    if (auto runtimeWorkers = getRuntimeTotalWorkers(module);
        runtimeWorkers && *runtimeWorkers > 0)
      return *runtimeWorkers;
  }
  return 1;
}

static std::pair<int64_t, int64_t>
chooseAdaptiveTileShape(const SeidelWavefrontMatch &match) {
  ForOp rowFor = match.rowFor;
  auto iTrip = ValueAnalysis::tryFoldConstantIndex(
      rowFor.getUpperBound().front());
  auto iLb = ValueAnalysis::tryFoldConstantIndex(
      rowFor.getLowerBound().front());
  auto jTrip = ValueAnalysis::tryFoldConstantIndex(match.innerUpperBound);
  auto jLb = ValueAnalysis::tryFoldConstantIndex(match.innerLowerBound);

  int64_t iExtent =
      (iTrip && iLb) ? std::max<int64_t>(1, *iTrip - *iLb) : 1;
  int64_t jExtent =
      (jTrip && jLb) ? std::max<int64_t>(1, *jTrip - *jLb) : 1;

  int64_t workers = resolveWorkerCount(match.parallelEdt);
  workers = std::max<int64_t>(1, workers);

  int64_t minRowTiles = (workers > 1 && iExtent >= 2) ? 2 : 1;
  int64_t minColTiles = (workers > 1 && jExtent >= 2) ? 2 : 1;
  int64_t maxRowTiles =
      std::max<int64_t>(minRowTiles, std::min<int64_t>(iExtent, workers * 2));
  int64_t maxColTiles =
      std::max<int64_t>(minColTiles, std::min<int64_t>(jExtent, workers * 4));

  int64_t bestNumITiles = 1;
  int64_t bestNumJTiles = 1;
  long double bestScore = std::numeric_limits<long double>::infinity();

  auto computeWeightedRankCount = [](int64_t numITiles,
                                     int64_t numJTiles) -> int64_t {
    return std::max<int64_t>(1, 2 * (numITiles - 1) + numJTiles);
  };

  auto computeWeightedFrontierWidth = [&](int64_t numITiles,
                                          int64_t numJTiles) -> int64_t {
    int64_t maxRank = 2 * (numITiles - 1) + (numJTiles - 1);
    int64_t bestWidth = 1;
    for (int64_t rank = 0; rank <= maxRank; ++rank) {
      int64_t minTileI = std::max<int64_t>(
          0, (rank - (numJTiles - 1) + 1) / 2);
      int64_t maxTileI = std::min<int64_t>(numITiles - 1, rank / 2);
      if (maxTileI < minTileI)
        continue;
      bestWidth = std::max<int64_t>(bestWidth, maxTileI - minTileI + 1);
    }
    return bestWidth;
  };

  int64_t desiredFrontier =
      std::min<int64_t>(workers,
                        computeWeightedFrontierWidth(maxRowTiles, maxColTiles));

  auto scoreCandidate = [&](int64_t numITiles,
                            int64_t numJTiles) -> long double {
    if (numITiles <= 0 || numJTiles <= 0)
      return std::numeric_limits<long double>::infinity();
    if (numITiles < minRowTiles || numJTiles < minColTiles)
      return std::numeric_limits<long double>::infinity();

    // The Seidel DAG is ordered by rank = 2*tileI + tileJ. Estimate the
    // available concurrency from the exact width of those rank frontiers
    // instead of the ordinary anti-diagonal width.
    int64_t tilesPerStep = numITiles * numJTiles;
    int64_t frontierWidth =
        computeWeightedFrontierWidth(numITiles, numJTiles);
    int64_t rankCount = computeWeightedRankCount(numITiles, numJTiles);
    long double averageFrontier =
        static_cast<long double>(tilesPerStep) /
        static_cast<long double>(std::max<int64_t>(1, rankCount));

    long double frontierPenalty = 0.0L;
    if (frontierWidth < desiredFrontier) {
      long double gap = static_cast<long double>(desiredFrontier - frontierWidth);
      frontierPenalty = gap * gap * 1.0e12L;
    }

    long double averagePenalty = 0.0L;
    long double desiredAverage =
        std::max<long double>(1.0L, static_cast<long double>(desiredFrontier) *
                                        0.65L);
    if (averageFrontier < desiredAverage) {
      long double gap = desiredAverage - averageFrontier;
      averagePenalty = gap * gap * 1.0e8L;
    }

    long double taskCost = static_cast<long double>(tilesPerStep) * 1.0e4L;
    long double rankCost = static_cast<long double>(rankCount) * 1.0e3L;
    long double shapePenalty =
        std::abs(static_cast<long double>(numJTiles) -
                 (static_cast<long double>(2 * numITiles - 1))) *
        10.0L;
    long double fullRangePenalty =
        (numITiles == 1 || numJTiles == 1) ? 1.0e15L : 0.0L;

    return frontierPenalty + averagePenalty + taskCost + rankCost +
           shapePenalty + fullRangePenalty;
  };

  for (int64_t numITiles = minRowTiles; numITiles <= maxRowTiles; ++numITiles) {
    for (int64_t numJTiles = minColTiles; numJTiles <= maxColTiles;
         ++numJTiles) {
      long double score = scoreCandidate(numITiles, numJTiles);
      if (score < bestScore) {
        bestScore = score;
        bestNumITiles = numITiles;
        bestNumJTiles = numJTiles;
      }
    }
  }

  int64_t tileRows =
      std::max<int64_t>(1, (iExtent + bestNumITiles - 1) / bestNumITiles);
  int64_t tileCols =
      std::max<int64_t>(1, (jExtent + bestNumJTiles - 1) / bestNumJTiles);
  return {tileRows, tileCols};
}

static bool matchUnitInnerLoop(Operation *op, Value &lb, Value &ub, Value &iv) {
  auto scfFor = dyn_cast<scf::ForOp>(op);
  if (!scfFor || !isUnitStep(scfFor.getStep()))
    return false;
  lb = scfFor.getLowerBound();
  ub = scfFor.getUpperBound();
  iv = scfFor.getInductionVar();
  return true;
}

static Block &getLoopBody(Operation *loopOp) {
  return cast<scf::ForOp>(loopOp).getRegion().front();
}

static bool matchOffset(Value index, Value iv, int64_t offset) {
  index = ValueAnalysis::stripNumericCasts(index);
  iv = ValueAnalysis::stripNumericCasts(iv);
  if (ValueAnalysis::sameValue(index, iv))
    return offset == 0;

  if (auto addi = index.getDefiningOp<arith::AddIOp>()) {
    auto lhs = ValueAnalysis::stripNumericCasts(addi.getLhs());
    auto rhs = ValueAnalysis::stripNumericCasts(addi.getRhs());
    if (ValueAnalysis::sameValue(lhs, iv)) {
      auto c = ValueAnalysis::tryFoldConstantIndex(rhs);
      return c && *c == offset;
    }
    if (ValueAnalysis::sameValue(rhs, iv)) {
      auto c = ValueAnalysis::tryFoldConstantIndex(lhs);
      return c && *c == offset;
    }
  }

  if (auto subi = index.getDefiningOp<arith::SubIOp>()) {
    auto lhs = ValueAnalysis::stripNumericCasts(subi.getLhs());
    auto rhs = ValueAnalysis::stripNumericCasts(subi.getRhs());
    if (ValueAnalysis::sameValue(lhs, iv)) {
      auto c = ValueAnalysis::tryFoldConstantIndex(rhs);
      return c && -*c == offset;
    }
  }

  return false;
}

static bool isSeidelStore(memref::StoreOp store, Value rowIV, Value colIV) {
  if (store.getIndices().size() != 2)
    return false;
  return ValueAnalysis::sameValue(store.getIndices()[0], rowIV) &&
         ValueAnalysis::sameValue(store.getIndices()[1], colIV);
}

static bool looksLikeSeidelStencilBody(Operation *innerLoop, Value rowIV,
                                       Value colIV, Value &stencilMemref) {
  int loadCount = 0;
  int storeCount = 0;
  Value loadMemref;
  Value storedMemref;
  bool hasRowMinusOne = false;
  bool hasRowPlusOne = false;
  bool hasColMinusOne = false;
  bool hasColPlusOne = false;

  Block &body = getLoopBody(innerLoop);
  for (Operation &op : body.without_terminator()) {
    if (auto load = dyn_cast<memref::LoadOp>(&op)) {
      if (load.getIndices().size() != 2)
        return false;
      if (!loadMemref)
        loadMemref = load.getMemRef();
      else if (load.getMemRef() != loadMemref)
        return false;
      loadCount++;
      hasRowMinusOne |= matchOffset(load.getIndices()[0], rowIV, -1);
      hasRowPlusOne |= matchOffset(load.getIndices()[0], rowIV, 1);
      hasColMinusOne |= matchOffset(load.getIndices()[1], colIV, -1);
      hasColPlusOne |= matchOffset(load.getIndices()[1], colIV, 1);
      continue;
    }

    if (auto store = dyn_cast<memref::StoreOp>(&op)) {
      if (!isSeidelStore(store, rowIV, colIV))
        return false;
      storeCount++;
      storedMemref = store.getMemRef();
      continue;
    }

    if (isa<scf::ForOp>(&op))
      return false;
  }

  return loadCount >= 8 && storeCount == 1 && storedMemref &&
         (stencilMemref = storedMemref) && loadMemref == storedMemref &&
         hasRowMinusOne && hasRowPlusOne &&
         hasColMinusOne && hasColPlusOne;
}

static ForOp getSingleTopLevelFor(EdtOp edt) {
  if (!edt)
    return nullptr;

  ForOp result = nullptr;
  for (Operation &op : edt.getBody().front().without_terminator()) {
    auto forOp = dyn_cast<ForOp>(&op);
    if (!forOp)
      return nullptr;
    if (result)
      return nullptr;
    result = forOp;
  }
  return result;
}

static bool matchSeidelWavefront(EdtOp parallelEdt,
                                 SeidelWavefrontMatch &match) {
  match = {};
  if (!parallelEdt || parallelEdt.getType() != EdtType::parallel)
    return false;

  ForOp rowFor = getSingleTopLevelFor(parallelEdt);
  if (!rowFor || rowFor.getLowerBound().size() != 1 ||
      rowFor.getUpperBound().size() != 1 || rowFor.getStep().size() != 1 ||
      !isUnitStep(rowFor.getStep().front()))
    return false;

  Block &body = rowFor.getRegion().front();
  if (body.getNumArguments() != 1)
    return false;

  Operation *innerLoop = nullptr;
  SmallVector<Operation *, 8> prelude;
  for (Operation &op : body.without_terminator()) {
    if (isa<scf::ForOp>(&op)) {
      if (innerLoop)
        return false;
      innerLoop = &op;
      continue;
    }
    if (innerLoop)
      return false;
    if (op.getNumRegions() != 0)
      return false;
    prelude.push_back(&op);
  }

  if (!innerLoop)
    return false;

  Value innerLb;
  Value innerUb;
  Value innerIV;
  Value stencilMemref;
  if (!matchUnitInnerLoop(innerLoop, innerLb, innerUb, innerIV))
    return false;
  if (!looksLikeSeidelStencilBody(innerLoop, body.getArgument(0), innerIV,
                                  stencilMemref))
    return false;

  match.parallelEdt = parallelEdt;
  match.rowFor = rowFor;
  match.innerLoop = cast<scf::ForOp>(innerLoop);
  match.rowIV = body.getArgument(0);
  match.innerLowerBound = innerLb;
  match.innerUpperBound = innerUb;
  match.innerIV = innerIV;
  match.stencilMemref = stencilMemref;
  match.preludeOps = std::move(prelude);
  return true;
}

static Value createCeilDivPositive(OpBuilder &builder, Location loc, Value num,
                                   int64_t denom) {
  Value denomVal = createConstantIndex(builder, loc, denom);
  Value bias = createConstantIndex(builder, loc, denom - 1);
  Value biased = builder.create<arith::AddIOp>(loc, num, bias);
  return builder.create<arith::DivUIOp>(loc, biased, denomVal);
}

static Value createMinIndex(OpBuilder &builder, Location loc, Value lhs,
                            Value rhs) {
  Value cmp =
      builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::ult, lhs, rhs);
  return builder.create<arith::SelectOp>(loc, cmp, lhs, rhs);
}

static Value createSubtractOrZero(OpBuilder &builder, Location loc, Value lhs,
                                  Value rhs) {
  Value canSubtract =
      builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::uge, lhs, rhs);
  Value diff = builder.create<arith::SubIOp>(loc, lhs, rhs);
  Value zero = createZeroIndex(builder, loc);
  return builder.create<arith::SelectOp>(loc, canSubtract, diff, zero);
}

static Value createMaxIndex(OpBuilder &builder, Location loc, Value lhs,
                            Value rhs) {
  Value cmp =
      builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::ugt, lhs, rhs);
  return builder.create<arith::SelectOp>(loc, cmp, lhs, rhs);
}

static Value createSignedMinIndex(OpBuilder &builder, Location loc, Value lhs,
                                  Value rhs) {
  Value cmp =
      builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::slt, lhs, rhs);
  return builder.create<arith::SelectOp>(loc, cmp, lhs, rhs);
}

static Value createSignedMaxIndex(OpBuilder &builder, Location loc, Value lhs,
                                  Value rhs) {
  Value cmp =
      builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::sgt, lhs, rhs);
  return builder.create<arith::SelectOp>(loc, cmp, lhs, rhs);
}

static Value createClampIndex(OpBuilder &builder, Location loc, Value value,
                              int64_t minVal, int64_t maxVal) {
  Value lower = createConstantIndex(builder, loc, minVal);
  Value upper = createConstantIndex(builder, loc, maxVal);
  return createSignedMinIndex(
      builder, loc, createSignedMaxIndex(builder, loc, value, lower), upper);
}

static Value createAnd(OpBuilder &builder, Location loc, Value lhs, Value rhs) {
  return builder.create<arith::AndIOp>(loc, lhs, rhs);
}

static std::pair<Value, Value>
computeTileBounds(OpBuilder &builder, Location loc, Value tileCoord,
                  Value baseLowerBound, Value globalUpperBound,
                  Value tileExtent) {
  Value tileStart = builder.create<arith::AddIOp>(
      loc, baseLowerBound,
      builder.create<arith::MulIOp>(loc, tileCoord, tileExtent));
  Value tileEnd = createMinIndex(
      builder, loc,
      builder.create<arith::AddIOp>(loc, tileStart, tileExtent),
      globalUpperBound);
  return {tileStart, tileEnd};
}

static Value createTileControl(OpBuilder &builder, Location loc, Value memref,
                               ArtsMode mode, Value tileI, Value tileJ,
                               Value iLb, Value iUb, Value jLb, Value jUb,
                               Value tileRows, Value tileCols,
                               int64_t deltaTileI, int64_t deltaTileJ,
                               int64_t numITilesConst,
                               int64_t numJTilesConst) {
  Value depTileI = tileI;
  Value depTileJ = tileJ;
  Value valid = builder.create<arith::ConstantIntOp>(loc, 1, 1);
  if (deltaTileI != 0) {
    Value shifted =
        builder.create<arith::AddIOp>(loc, tileI,
                                      createConstantIndex(builder, loc,
                                                          deltaTileI));
    Value lowerBound = createConstantIndex(builder, loc, 0);
    Value upperBound = createConstantIndex(builder, loc, numITilesConst);
    Value geLower = builder.create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::sge, shifted, lowerBound);
    Value ltUpper = builder.create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::slt, shifted, upperBound);
    valid = createAnd(builder, loc, valid, createAnd(builder, loc, geLower,
                                                     ltUpper));
    depTileI = createClampIndex(builder, loc, shifted, 0, numITilesConst - 1);
  }
  if (deltaTileJ != 0) {
    Value shifted =
        builder.create<arith::AddIOp>(loc, tileJ,
                                      createConstantIndex(builder, loc,
                                                          deltaTileJ));
    Value lowerBound = createConstantIndex(builder, loc, 0);
    Value upperBound = createConstantIndex(builder, loc, numJTilesConst);
    Value geLower = builder.create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::sge, shifted, lowerBound);
    Value ltUpper = builder.create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::slt, shifted, upperBound);
    valid = createAnd(builder, loc, valid, createAnd(builder, loc, geLower,
                                                     ltUpper));
    depTileJ = createClampIndex(builder, loc, shifted, 0, numJTilesConst - 1);
  }

  auto [depIStart, depIEnd] =
      computeTileBounds(builder, loc, depTileI, iLb, iUb, tileRows);
  auto [depJStart, depJEnd] =
      computeTileBounds(builder, loc, depTileJ, jLb, jUb, tileCols);

  Value depISize = createSubtractOrZero(builder, loc, depIEnd, depIStart);
  Value depJSize = createSubtractOrZero(builder, loc, depJEnd, depJStart);
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  depISize = createMaxIndex(builder, loc, depISize, one);
  depJSize = createMaxIndex(builder, loc, depJSize, one);
  depISize = builder.create<arith::SelectOp>(loc, valid, depISize, zero);
  depJSize = builder.create<arith::SelectOp>(loc, valid, depJSize, zero);

  return builder
      .create<DbControlOp>(loc, mode, memref, /*indices=*/SmallVector<Value>{},
                           /*offsets=*/SmallVector<Value>{depIStart, depJStart},
                           /*sizes=*/SmallVector<Value>{depISize, depJSize})
      .getResult();
}

static void stampSeidelTileTask(Operation *op, int64_t blockRows,
                                int64_t blockCols) {
  if (!op)
    return;
  setDepPattern(op, ArtsDepPattern::stencil_tiling_nd);
  setStencilSpatialDims(op, {0, 1});
  setStencilOwnerDims(op, {0, 1});
  setStencilBlockShape(op, {blockRows, blockCols});
  setStencilMinOffsets(op, {-1, -1});
  setStencilMaxOffsets(op, {1, 1});
  setStencilWriteFootprint(op, {0, 0});
  setSupportedBlockHalo(op);
}

static void rewriteSeidelWavefront(SeidelWavefrontMatch &match) {
  Location loc = match.parallelEdt.getLoc();
  OpBuilder builder(match.parallelEdt);
  auto [tileRowsConst, tileColsConst] = chooseAdaptiveTileShape(match);

  auto epoch = builder.create<EpochOp>(loc);
  Region &epochRegion = epoch.getRegion();
  if (epochRegion.empty())
    epochRegion.push_back(new Block());
  Block &epochBlock = epochRegion.front();
  builder.setInsertionPointToStart(&epochBlock);

  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value tileRows = createConstantIndex(builder, loc, tileRowsConst);
  Value tileCols = createConstantIndex(builder, loc, tileColsConst);

  Value iLb = match.rowFor.getLowerBound().front();
  Value iUb = match.rowFor.getUpperBound().front();
  Value jLb = match.innerLowerBound;
  Value jUb = match.innerUpperBound;

  Value iTrip = builder.create<arith::SubIOp>(loc, iUb, iLb);
  Value jTrip = builder.create<arith::SubIOp>(loc, jUb, jLb);
  Value numITiles = createCeilDivPositive(builder, loc, iTrip, tileRowsConst);
  Value numJTiles = createCeilDivPositive(builder, loc, jTrip, tileColsConst);
  int64_t iExtentConst = std::max<int64_t>(
      1, ValueAnalysis::tryFoldConstantIndex(match.rowFor.getUpperBound().front())
                 .value_or(1) -
             ValueAnalysis::tryFoldConstantIndex(match.rowFor.getLowerBound().front())
                 .value_or(0));
  int64_t jExtentConst = std::max<int64_t>(
      1, ValueAnalysis::tryFoldConstantIndex(match.innerUpperBound).value_or(1) -
             ValueAnalysis::tryFoldConstantIndex(match.innerLowerBound)
                 .value_or(0));
  int64_t numITilesConst =
      std::max<int64_t>(1, (iExtentConst + tileRowsConst - 1) / tileRowsConst);
  int64_t numJTilesConst =
      std::max<int64_t>(1, (jExtentConst + tileColsConst - 1) / tileColsConst);
  int64_t allocRowsConst = std::max<int64_t>(
      1, (iExtentConst + 2 + numITilesConst - 1) / numITilesConst);
  int64_t allocColsConst = std::max<int64_t>(
      1, (jExtentConst + 2 + numJTilesConst - 1) / numJTilesConst);

  auto tileILoop = builder.create<scf::ForOp>(loc, zero, numITiles, one);
  OpBuilder biBuilder = OpBuilder::atBlockBegin(tileILoop.getBody());
  Value bi = tileILoop.getInductionVar();

  auto tileJLoop = biBuilder.create<scf::ForOp>(loc, zero, numJTiles, one);
  OpBuilder bjBuilder = OpBuilder::atBlockBegin(tileJLoop.getBody());
  Value bj = tileJLoop.getInductionVar();

  auto [iStart, iEnd] =
      computeTileBounds(bjBuilder, loc, bi, iLb, iUb, tileRows);
  auto [jStart, jEnd] =
      computeTileBounds(bjBuilder, loc, bj, jLb, jUb, tileCols);

  SmallVector<Value, 9> deps;
  deps.push_back(createTileControl(
      bjBuilder, loc, match.stencilMemref, ArtsMode::inout, bi, bj, iLb, iUb,
      jLb, jUb, tileRows, tileCols, 0, 0, numITilesConst, numJTilesConst));

  static constexpr std::pair<int64_t, int64_t> predecessorOffsets[] = {
      {-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
  };
  for (auto [di, dj] : predecessorOffsets) {
    deps.push_back(createTileControl(
        bjBuilder, loc, match.stencilMemref, ArtsMode::in, bi, bj, iLb, iUb,
        jLb, jUb, tileRows, tileCols, di, dj, numITilesConst,
        numJTilesConst));
  }

  auto tileTask = bjBuilder.create<EdtOp>(loc, EdtType::task,
                                          match.parallelEdt.getConcurrency(),
                                          deps);
  tileTask.setNoVerifyAttr(NoVerifyAttr::get(builder.getContext()));
  copyArtsMetadataAttrs(match.parallelEdt.getOperation(),
                        tileTask.getOperation());
  stampSeidelTileTask(tileTask.getOperation(), allocRowsConst, allocColsConst);

  Block &taskBody = tileTask.getBody().front();
  OpBuilder taskBuilder = OpBuilder::atBlockBegin(&taskBody);
  auto iLoop = taskBuilder.create<scf::ForOp>(loc, iStart, iEnd, one);
  OpBuilder iBuilder = OpBuilder::atBlockBegin(iLoop.getBody());
  IRMapping outerMap;
  outerMap.map(match.rowIV, iLoop.getInductionVar());

  for (Operation *op : match.preludeOps)
    iBuilder.clone(*op, outerMap);

  auto jLoop = iBuilder.create<scf::ForOp>(loc, jStart, jEnd, one);
  OpBuilder jBuilder = OpBuilder::atBlockBegin(jLoop.getBody());
  IRMapping innerMap = outerMap;
  innerMap.map(match.innerIV, jLoop.getInductionVar());

  for (Operation &op : getLoopBody(match.innerLoop).without_terminator())
    jBuilder.clone(op, innerMap);

  taskBuilder.setInsertionPointToEnd(&taskBody);
  taskBuilder.create<arts::YieldOp>(loc);

  OpBuilder epochBuilder = OpBuilder::atBlockEnd(&epochBlock);
  epochBuilder.create<arts::YieldOp>(loc);

  match.parallelEdt.erase();
}

class Seidel2DWavefrontPattern final : public DepTransform {
public:
  int run(ModuleOp module) override {
    int rewrites = 0;
    while (true) {
      SeidelWavefrontMatch match;
      bool found = false;
      module.walk([&](EdtOp edt) {
        if (matchSeidelWavefront(edt, match)) {
          found = true;
          return WalkResult::interrupt();
        }
        return WalkResult::advance();
      });
      if (!found)
        break;
      rewriteSeidelWavefront(match);
      rewrites++;
    }
    return rewrites;
  }

  StringRef getName() const override { return "Seidel2DWavefrontPattern"; }
};

} // namespace

std::unique_ptr<DepTransform> createSeidel2DWavefrontPattern() {
  return std::make_unique<Seidel2DWavefrontPattern>();
}

} // namespace arts
} // namespace mlir
