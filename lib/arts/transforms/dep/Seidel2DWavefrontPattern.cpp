///==========================================================================///
/// File: Seidel2DWavefrontPattern.cpp
///
/// Rewrite Seidel-style in-place 2D stencils on ARTS IR into a macro-tiled
/// dependence DAG after OpenMP-to-ARTS conversion.
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
///     arts.epoch {
///       scf.for %tileI = 0 .. numITiles
///         scf.for %tileJ = 0 .. numJTiles
///           arts.edt <task> (%self, %pred[-1,-1], %pred[-1,0],
///                            %pred[-1,1], %pred[0,-1]) {
///             scf.for %i in macroTile
///               scf.for %j in macroTile
///                 original update
///           }
///     }
///
/// The DAG keeps the Seidel predecessor set explicit through DbControlOps, but
/// each EDT now covers a macro-tile large enough to amortize launch and
/// dependence-registration overhead.
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/analysis/heuristics/PartitioningHeuristics.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/transforms/dep/DepTransform.h"
#include "arts/utils/EdtUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include <cmath>
#include <optional>

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

static int64_t resolveWorkerCount(EdtOp parallelEdt) {
  if (!parallelEdt)
    return 1;
  if (auto workers = getWorkers(parallelEdt.getOperation());
      workers && *workers > 0)
    return *workers;
  if (auto module = parallelEdt->getParentOfType<ModuleOp>()) {
    if (auto runtimeWorkers = getRuntimeTotalWorkers(module);
        runtimeWorkers && *runtimeWorkers > 0)
      return *runtimeWorkers;
  }
  return 1;
}

static std::optional<std::pair<int64_t, int64_t>>
chooseAdaptiveTileShape(const SeidelWavefrontMatch &match) {
  ForOp rowFor = match.rowFor;
  auto iTrip =
      ValueAnalysis::tryFoldConstantIndex(rowFor.getUpperBound().front());
  auto iLb =
      ValueAnalysis::tryFoldConstantIndex(rowFor.getLowerBound().front());
  auto jTrip = ValueAnalysis::tryFoldConstantIndex(match.innerUpperBound);
  auto jLb = ValueAnalysis::tryFoldConstantIndex(match.innerLowerBound);

  int64_t iExtent = (iTrip && iLb) ? std::max<int64_t>(1, *iTrip - *iLb) : 1;
  int64_t jExtent = (jTrip && jLb) ? std::max<int64_t>(1, *jTrip - *jLb) : 1;

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
  bool foundCandidate = false;
  long double bestScore = std::numeric_limits<long double>::infinity();
  long double totalCells =
      static_cast<long double>(iExtent) * static_cast<long double>(jExtent);
  /// Keep enough interior work in each macro-tile to amortize EDT launch and
  /// dependence setup, but still allow multiple tiles per worker when the
  /// interior is large enough to sustain it.
  constexpr int64_t minCellsPerTask = 1 << 14;
  int64_t maxTilesByWork = std::max<int64_t>(
      1, static_cast<int64_t>(std::ceil(
             totalCells / static_cast<long double>(minCellsPerTask))));
  maxTilesByWork =
      std::min<int64_t>(maxTilesByWork, std::max<int64_t>(4, workers * 2));

  auto computeWeightedRankCount = [](int64_t numITiles,
                                     int64_t numJTiles) -> int64_t {
    return std::max<int64_t>(1, 2 * (numITiles - 1) + numJTiles);
  };

  auto computeWeightedFrontierWidth = [&](int64_t numITiles,
                                          int64_t numJTiles) -> int64_t {
    int64_t maxRank = 2 * (numITiles - 1) + (numJTiles - 1);
    int64_t bestWidth = 1;
    for (int64_t rank = 0; rank <= maxRank; ++rank) {
      int64_t minTileI = std::max<int64_t>(0, (rank - (numJTiles - 1) + 1) / 2);
      int64_t maxTileI = std::min<int64_t>(numITiles - 1, rank / 2);
      if (maxTileI < minTileI)
        continue;
      bestWidth = std::max<int64_t>(bestWidth, maxTileI - minTileI + 1);
    }
    return bestWidth;
  };

  /// Seidel macro-tiles must amortize EDT launch and dependence-registration
  /// overhead. Bound the total tile count by a minimum work-per-task target,
  /// then pick the best weighted-wavefront shape that fits inside that budget.
  /// If the interior cannot sustain even a 2-D tiled wavefront under that
  /// budget, fall back to the sequentialized form instead of forcing an
  /// unprofitable task DAG.
  if (maxTilesByWork < 4)
    return std::nullopt;

  int64_t desiredFrontier = std::clamp<int64_t>(
      static_cast<int64_t>(
          std::ceil(std::sqrt(static_cast<long double>(maxTilesByWork)))),
      int64_t{1}, workers);
  long double desiredTasks = static_cast<long double>(maxTilesByWork);
  long double targetWorkPerTask =
      totalCells / std::max<long double>(1.0L, desiredTasks);

  auto scoreCandidate = [&](int64_t numITiles,
                            int64_t numJTiles) -> long double {
    if (numITiles <= 0 || numJTiles <= 0)
      return std::numeric_limits<long double>::infinity();
    if (numITiles < minRowTiles || numJTiles < minColTiles)
      return std::numeric_limits<long double>::infinity();

    /// Seidel DAG is ordered by rank = 2*tileI + tileJ. Estimate concurrency
    /// from the exact width of those rank frontiers, not anti-diagonal width.
    int64_t tilesPerStep = numITiles * numJTiles;
    if (tilesPerStep > maxTilesByWork)
      return std::numeric_limits<long double>::infinity();
    int64_t frontierWidth = computeWeightedFrontierWidth(numITiles, numJTiles);
    int64_t rankCount = computeWeightedRankCount(numITiles, numJTiles);
    long double averageFrontier =
        static_cast<long double>(tilesPerStep) /
        static_cast<long double>(std::max<int64_t>(1, rankCount));
    long double workPerTask =
        totalCells /
        static_cast<long double>(std::max<int64_t>(1, tilesPerStep));

    long double frontierPenalty = 0.0L;
    if (frontierWidth < desiredFrontier) {
      long double gap =
          static_cast<long double>(desiredFrontier - frontierWidth);
      frontierPenalty = gap * gap * 1.0e8L;
    }

    long double averagePenalty = 0.0L;
    long double desiredAverage = std::max<long double>(
        1.0L, static_cast<long double>(desiredFrontier) * 0.5L);
    if (averageFrontier < desiredAverage) {
      long double gap = desiredAverage - averageFrontier;
      averagePenalty = gap * gap * 1.0e6L;
    }

    long double workPenalty = 0.0L;
    if (workPerTask < targetWorkPerTask) {
      long double gap = targetWorkPerTask - workPerTask;
      workPenalty = gap * gap * 1.0e3L;
    }

    long double taskCost = static_cast<long double>(tilesPerStep) * 1.0e3L;
    long double rankCost = static_cast<long double>(rankCount) * 1.0e3L;
    long double shapePenalty =
        std::abs(static_cast<long double>(numJTiles) -
                 (static_cast<long double>(2 * numITiles - 1))) *
        1.0L;
    long double fullRangePenalty =
        (numITiles == 1 || numJTiles == 1) ? 1.0e15L : 0.0L;

    return frontierPenalty + averagePenalty + workPenalty + taskCost +
           rankCost + shapePenalty + fullRangePenalty;
  };

  for (int64_t numITiles = minRowTiles; numITiles <= maxRowTiles; ++numITiles) {
    for (int64_t numJTiles = minColTiles; numJTiles <= maxColTiles;
         ++numJTiles) {
      long double score = scoreCandidate(numITiles, numJTiles);
      if (score < bestScore) {
        bestScore = score;
        bestNumITiles = numITiles;
        bestNumJTiles = numJTiles;
        foundCandidate = true;
      }
    }
  }

  if (!foundCandidate)
    return std::nullopt;

  int64_t tileRows =
      std::max<int64_t>(1, (iExtent + bestNumITiles - 1) / bestNumITiles);
  int64_t tileCols =
      std::max<int64_t>(1, (jExtent + bestNumJTiles - 1) / bestNumJTiles);
  return std::make_pair(tileRows, tileCols);
}

static bool matchUnitInnerLoop(Operation *op, Value &lb, Value &ub, Value &iv) {
  auto scfFor = dyn_cast<scf::ForOp>(op);
  if (!scfFor || !ValueAnalysis::isOneConstant(scfFor.getStep()))
    return false;
  lb = scfFor.getLowerBound();
  ub = scfFor.getUpperBound();
  iv = scfFor.getInductionVar();
  return true;
}

static Block &getLoopBody(Operation *loopOp) {
  if (auto artsFor = dyn_cast<ForOp>(loopOp))
    return artsFor.getRegion().front();
  if (auto scfFor = dyn_cast<scf::ForOp>(loopOp))
    return scfFor.getRegion().front();
  llvm::report_fatal_error(
      "Seidel2DWavefrontPattern expected arts.for or scf.for loop body");
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
         hasRowMinusOne && hasRowPlusOne && hasColMinusOne && hasColPlusOne;
}

static bool matchSeidelWavefront(EdtOp parallelEdt,
                                 SeidelWavefrontMatch &match) {
  match = {};
  if (!parallelEdt || parallelEdt.getType() != EdtType::parallel)
    return false;

  ForOp rowFor = getSingleTopLevelFor(parallelEdt);
  if (!rowFor || rowFor.getLowerBound().size() != 1 ||
      rowFor.getUpperBound().size() != 1 || rowFor.getStep().size() != 1 ||
      !ValueAnalysis::isOneConstant(rowFor.getStep().front()))
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

static void rewriteSeidelSequential(SeidelWavefrontMatch &match) {
  OpBuilder builder(match.rowFor);
  auto seqRowFor = builder.create<scf::ForOp>(
      match.rowFor.getLoc(), match.rowFor.getLowerBound().front(),
      match.rowFor.getUpperBound().front(), match.rowFor.getStep().front());
  copyArtsMetadataAttrs(match.rowFor.getOperation(), seqRowFor.getOperation());

  OpBuilder rowBuilder = OpBuilder::atBlockBegin(seqRowFor.getBody());
  IRMapping rowMap;
  rowMap.map(match.rowIV, seqRowFor.getInductionVar());
  for (Operation &op : getLoopBody(match.rowFor).without_terminator())
    rowBuilder.clone(op, rowMap);

  match.rowFor.erase();

  Block &edtBody = match.parallelEdt.getBody().front();
  Operation *insertBefore = match.parallelEdt.getOperation();
  for (Operation &op : llvm::make_early_inc_range(edtBody.without_terminator()))
    op.moveBefore(insertBefore);
  match.parallelEdt.erase();
}

static void rewriteSeidelWavefront(SeidelWavefrontMatch &match,
                                   std::pair<int64_t, int64_t> tileShape) {
  Location loc = match.parallelEdt.getLoc();
  OpBuilder builder(match.parallelEdt);
  auto [tileRowsConst, tileColsConst] = tileShape;

  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value two = createConstantIndex(builder, loc, 2);
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

  Value numITilesMinusOne = builder.create<arith::SubIOp>(loc, numITiles, one);
  Value numJTilesMinusOne = builder.create<arith::SubIOp>(loc, numJTiles, one);
  Value doubleITilesMinusOne =
      builder.create<arith::MulIOp>(loc, numITilesMinusOne, two);
  Value waveUbExclusive = builder.create<arith::AddIOp>(
      loc,
      builder.create<arith::AddIOp>(loc, doubleITilesMinusOne,
                                    numJTilesMinusOne),
      one);

  auto waveLoop = builder.create<scf::ForOp>(loc, zero, waveUbExclusive, one);
  copyArtsMetadataAttrs(match.parallelEdt.getOperation(),
                        waveLoop.getOperation());
  setDepPattern(waveLoop.getOperation(), ArtsDepPattern::wavefront_2d);
  setEdtDistributionPattern(waveLoop.getOperation(),
                            EdtDistributionPattern::stencil);
  setStencilSpatialDims(waveLoop.getOperation(), {0, 1});
  setStencilOwnerDims(waveLoop.getOperation(), {0, 1});
  setStencilBlockShape(waveLoop.getOperation(), {tileRowsConst, tileColsConst});
  setStencilMinOffsets(waveLoop.getOperation(), {-1, -1});
  setStencilMaxOffsets(waveLoop.getOperation(), {1, 1});
  setStencilWriteFootprint(waveLoop.getOperation(), {0, 0});
  setSupportedBlockHalo(waveLoop.getOperation());

  OpBuilder waveBuilder = OpBuilder::atBlockBegin(waveLoop.getBody());
  Value wave = waveLoop.getInductionVar();

  Value clippedTmp =
      createSubtractOrZero(waveBuilder, loc, wave, numJTilesMinusOne);
  Value biMin = createCeilDivPositive(waveBuilder, loc, clippedTmp, 2);

  Value halfWave = waveBuilder.create<arith::DivUIOp>(loc, wave, two);
  Value biMaxExclusive = createMinIndex(
      waveBuilder, loc, waveBuilder.create<arith::AddIOp>(loc, halfWave, one),
      numITiles);

  auto tileParallel = waveBuilder.create<EdtOp>(
      loc, EdtType::parallel, match.parallelEdt.getConcurrency());
  tileParallel.setNoVerifyAttr(NoVerifyAttr::get(builder.getContext()));
  copyArtsMetadataAttrs(match.parallelEdt.getOperation(),
                        tileParallel.getOperation());
  copyWorkerTopologyAttrs(match.parallelEdt.getOperation(),
                          tileParallel.getOperation());
  setDepPattern(tileParallel.getOperation(), ArtsDepPattern::wavefront_2d);
  setEdtDistributionPattern(tileParallel.getOperation(),
                            EdtDistributionPattern::stencil);
  setStencilSpatialDims(tileParallel.getOperation(), {0, 1});
  setStencilOwnerDims(tileParallel.getOperation(), {0, 1});
  setStencilBlockShape(tileParallel.getOperation(),
                       {tileRowsConst, tileColsConst});
  setStencilMinOffsets(tileParallel.getOperation(), {-1, -1});
  setStencilMaxOffsets(tileParallel.getOperation(), {1, 1});
  setStencilWriteFootprint(tileParallel.getOperation(), {0, 0});
  setSupportedBlockHalo(tileParallel.getOperation());

  Block &tileParallelBlock = tileParallel.getBody().front();
  OpBuilder tileBuilder = OpBuilder::atBlockBegin(&tileParallelBlock);
  Value tileRowLb = tileBuilder.create<arith::AddIOp>(
      loc, iLb, tileBuilder.create<arith::MulIOp>(loc, biMin, tileRows));
  Value tileRowUb = tileBuilder.create<arith::AddIOp>(
      loc, iLb,
      tileBuilder.create<arith::MulIOp>(loc, biMaxExclusive, tileRows));

  auto tileFor = tileBuilder.create<arts::ForOp>(
      loc, ValueRange{tileRowLb}, ValueRange{tileRowUb}, ValueRange{tileRows},
      /*schedule=*/nullptr, /*reductionAccumulators=*/ValueRange{});
  copyArtsMetadataAttrs(match.rowFor.getOperation(), tileFor.getOperation());
  setDepPattern(tileFor.getOperation(), ArtsDepPattern::wavefront_2d);
  setEdtDistributionPattern(tileFor.getOperation(),
                            EdtDistributionPattern::stencil);
  setStencilSpatialDims(tileFor.getOperation(), {0, 1});
  setStencilOwnerDims(tileFor.getOperation(), {0, 1});
  setStencilBlockShape(tileFor.getOperation(), {tileRowsConst, tileColsConst});
  setStencilMinOffsets(tileFor.getOperation(), {-1, -1});
  setStencilMaxOffsets(tileFor.getOperation(), {1, 1});
  setStencilWriteFootprint(tileFor.getOperation(), {0, 0});
  setSupportedBlockHalo(tileFor.getOperation());

  Region &tileRegion = tileFor.getRegion();
  if (tileRegion.empty())
    tileRegion.push_back(new Block());
  Block &tileBlock = tileRegion.front();
  if (tileBlock.getNumArguments() == 0)
    tileBlock.addArgument(builder.getIndexType(), loc);

  OpBuilder tileLoopBuilder = OpBuilder::atBlockBegin(&tileBlock);
  Value tileRowBase = tileBlock.getArgument(0);
  Value biNumerator =
      tileLoopBuilder.create<arith::SubIOp>(loc, tileRowBase, iLb);
  Value bi = tileLoopBuilder.create<arith::DivUIOp>(loc, biNumerator, tileRows);
  Value bj = tileLoopBuilder.create<arith::SubIOp>(
      loc, wave, tileLoopBuilder.create<arith::MulIOp>(loc, bi, two));

  Value iStart = tileRowBase;
  Value iEnd = createMinIndex(
      tileLoopBuilder, loc,
      tileLoopBuilder.create<arith::AddIOp>(loc, iStart, tileRows), iUb);
  Value jStart = tileLoopBuilder.create<arith::AddIOp>(
      loc, jLb, tileLoopBuilder.create<arith::MulIOp>(loc, bj, tileCols));
  Value jEnd = createMinIndex(
      tileLoopBuilder, loc,
      tileLoopBuilder.create<arith::AddIOp>(loc, jStart, tileCols), jUb);

  auto iLoop = tileLoopBuilder.create<scf::ForOp>(loc, iStart, iEnd, one);
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

  tileLoopBuilder.create<arts::YieldOp>(loc);
  tileBuilder.create<arts::YieldOp>(loc);
  waveBuilder.create<arts::BarrierOp>(loc);

  match.parallelEdt.erase();
}

class Seidel2DWavefrontPattern final : public DepPatternTransform {
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
      auto tileShape = chooseAdaptiveTileShape(match);
      bool usedSequentialFallback = !tileShape;
      if (usedSequentialFallback)
        rewriteSeidelSequential(match);
      else
        rewriteSeidelWavefront(match, *tileShape);
      rewrites++;
      if (usedSequentialFallback)
        return rewrites;
    }
    return rewrites;
  }

  StringRef getName() const override { return "Seidel2DWavefrontPattern"; }
  ArtsDepPattern getFamily() const override {
    return ArtsDepPattern::wavefront_2d;
  }
  int64_t getRevision() const override { return 1; }
};

} // namespace

std::unique_ptr<DepPatternTransform> createSeidel2DWavefrontPattern() {
  return std::make_unique<Seidel2DWavefrontPattern>();
}

} // namespace arts
} // namespace mlir
