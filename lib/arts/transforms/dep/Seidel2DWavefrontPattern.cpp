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
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"

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
  SmallVector<Operation *, 8> preludeOps;
};

static constexpr int64_t kTileRows = 64;
static constexpr int64_t kTileCols = 64;

static bool isUnitStep(Value step) {
  auto c = ValueAnalysis::tryFoldConstantIndex(step);
  return c && *c == 1;
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
                                       Value colIV) {
  int loadCount = 0;
  int storeCount = 0;
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

  return loadCount >= 8 && storeCount == 1 && storedMemref && hasRowMinusOne &&
         hasRowPlusOne && hasColMinusOne && hasColPlusOne;
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
  if (!matchUnitInnerLoop(innerLoop, innerLb, innerUb, innerIV))
    return false;
  if (!looksLikeSeidelStencilBody(innerLoop, body.getArgument(0), innerIV))
    return false;

  match.parallelEdt = parallelEdt;
  match.rowFor = rowFor;
  match.innerLoop = cast<scf::ForOp>(innerLoop);
  match.rowIV = body.getArgument(0);
  match.innerLowerBound = innerLb;
  match.innerUpperBound = innerUb;
  match.innerIV = innerIV;
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

static void rewriteSeidelWavefront(SeidelWavefrontMatch &match) {
  Location loc = match.parallelEdt.getLoc();
  OpBuilder builder(match.parallelEdt);

  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value two = createConstantIndex(builder, loc, 2);
  Value tileRows = createConstantIndex(builder, loc, kTileRows);
  Value tileCols = createConstantIndex(builder, loc, kTileCols);

  Value iLb = match.rowFor.getLowerBound().front();
  Value iUb = match.rowFor.getUpperBound().front();
  Value jLb = match.innerLowerBound;
  Value jUb = match.innerUpperBound;

  Value iTrip = builder.create<arith::SubIOp>(loc, iUb, iLb);
  Value jTrip = builder.create<arith::SubIOp>(loc, jUb, jLb);
  Value numITiles = createCeilDivPositive(builder, loc, iTrip, kTileRows);
  Value numJTiles = createCeilDivPositive(builder, loc, jTrip, kTileCols);

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

  Block &tileParallelBlock = tileParallel.getBody().front();
  OpBuilder tileBuilder = OpBuilder::atBlockBegin(&tileParallelBlock);
  Value tileRowLb = tileBuilder.create<arith::AddIOp>(
      loc, iLb, tileBuilder.create<arith::MulIOp>(loc, biMin, tileRows));
  Value tileRowUb = tileBuilder.create<arith::AddIOp>(
      loc, iLb,
      tileBuilder.create<arith::MulIOp>(loc, biMaxExclusive, tileRows));

  auto tileFor = tileBuilder.create<arts::ForOp>(
      loc, ValueRange{tileRowLb}, ValueRange{tileRowUb}, ValueRange{tileRows},
      /*schedule*/ nullptr, /*reductionAccumulators*/ ValueRange{});
  copyArtsMetadataAttrs(match.rowFor.getOperation(), tileFor.getOperation());
  setDepPattern(tileFor.getOperation(), ArtsDepPattern::wavefront_2d);

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
