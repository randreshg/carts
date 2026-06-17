///==========================================================================///
/// File: Tiling.cpp
///
/// Pattern-driven memref tiling in SDE. The pass rewrites SDE scheduling units
/// and their executable memref loop bodies so CU/SU/MU tiling intent is
/// explicit before boundary realization.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_TILING
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Utils/IterationSizingUtils.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/LoopUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

#include "mlir/Dialect/Arith/IR/Arith.h"

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Affine/LoopUtils.h"
#include "mlir/Dialect/SCF/Utils/Utils.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/MathExtras.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace mlir;
using namespace mlir::carts;

namespace {

/// Tiling may still rewrite owner steps until CU group block counts are
/// committed. Shape recovery on layout roots is not a tiling grain lock.
static bool hasCommittedTilingGrainFacts(sde::SdeSuIterateOp op) {
  if (sde::SdeCuRegionOp cu = sde::findSuComputeCuRegion(op))
    return cu.getGroupBlockCountAttr() != nullptr;
  return false;
}

static bool usesOwnerLocalPipelineGrain(sde::SdeSuIterateOp op) {
  auto classification = sde::queryStructuredClassification(op);
  return classification &&
         *classification ==
             sde::SdeStructuredClassification::elementwise_pipeline &&
         sde::isOwnerLocalPipelineReduction(op);
}

static Value buildRelaxedOwnerLocalPipelineMinIterations(
    OpBuilder &builder, Location loc, sde::SdeSuIterateOp op,
    Value clampedTripCount, Value targetTasks, int64_t minIterations) {
  Value minValue =
      createConstantIndex(builder, loc, std::max<int64_t>(1, minIterations));
  if (!op || !clampedTripCount || !targetTasks ||
      !usesOwnerLocalPipelineGrain(op) || minIterations <= 1)
    return minValue;

  Value one = createConstantIndex(builder, loc, 1);
  Value relaxationThreshold =
      arith::MulIOp::create(builder, loc, targetTasks, minValue);
  Value underfilledOwnerDomain =
      arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ult,
                            clampedTripCount, relaxationThreshold);
  return arith::SelectOp::create(builder, loc, underfilledOwnerDomain, one,
                                 minValue);
}

static int64_t getMinTileIterations(sde::SdeSuIterateOp op,
                                    sde::SDECostModel &costModel) {
  if (usesOwnerLocalPipelineGrain(op))
    return std::max<int64_t>(1,
                             costModel.getMinPipelineOwnerIterationsPerTask());
  return std::max<int64_t>(1, costModel.getMinIterationsPerWorker());
}

static int64_t getTargetTaskWaves(sde::SdeSuIterateOp op,
                                  sde::SDECostModel &costModel) {
  if (usesOwnerLocalPipelineGrain(op))
    return std::max<int64_t>(1,
                             costModel.getOwnerLocalPipelineTargetTaskWaves());
  return std::max<int64_t>(1, costModel.getInterLocalityTaskWaves());
}

static int64_t getTargetTileTasks(sde::SdeSuIterateOp op,
                                  sde::SDECostModel &costModel) {
  int64_t workers = std::max<int64_t>(1, costModel.getLogicalWorkerCapacity());
  int64_t waves = getTargetTaskWaves(op, costModel);
  if (workers > std::numeric_limits<int64_t>::max() / waves)
    return std::numeric_limits<int64_t>::max();
  return workers * waves;
}

static Value buildTileIterationValue(OpBuilder &builder, Location loc,
                                     sde::SdeSuIterateOp op,
                                     sde::SDECostModel &costModel) {
  Value tripCount = sde::buildTripCountValue(builder, loc, op);
  if (!tripCount)
    return Value();

  Value one = createConstantIndex(builder, loc, 1);
  Value workerCountValue = sde::buildLogicalWorkerCapacityValue(builder, loc);
  int64_t targetWaves = getTargetTaskWaves(op, costModel);
  if (targetWaves > 1) {
    Value waveCountValue = createConstantIndex(builder, loc, targetWaves);
    workerCountValue =
        arith::MulIOp::create(builder, loc, workerCountValue, waveCountValue);
  }
  Value clampedTripCount = arith::MaxUIOp::create(builder, loc, tripCount, one);
  Value minIterationsValue = buildRelaxedOwnerLocalPipelineMinIterations(
      builder, loc, op, clampedTripCount, workerCountValue,
      getMinTileIterations(op, costModel));
  Value balancedTile = arith::CeilDivUIOp::create(
      builder, loc, clampedTripCount, workerCountValue);
  Value preferredTile =
      arith::MaxUIOp::create(builder, loc, balancedTile, minIterationsValue);
  return arith::MinUIOp::create(builder, loc, preferredTile, clampedTripCount);
}

static SmallVector<Value> buildPerDimTripCounts(OpBuilder &builder,
                                                Location loc,
                                                sde::SdeSuIterateOp op) {
  SmallVector<Value> tripCounts;
  unsigned numDims = op.getLowerBounds().size();

  for (unsigned d = 0; d < numDims; ++d) {
    Value lb = op.getLowerBounds()[d];
    Value ub = op.getUpperBounds()[d];
    Value step = op.getSteps()[d];
    Value zero = createConstantIndex(builder, loc, 0);
    Value one = createConstantIndex(builder, loc, 1);

    Value span = arith::SubIOp::create(builder, loc, ub, lb);

    int64_t constantStep = 0;
    Value safeStep = step;
    if (::mlir::carts::ValueAnalysis::getConstantIndex(step, constantStep)) {
      if (constantStep <= 0)
        return {};
    } else {
      Value stepIsTooSmall = arith::CmpIOp::create(
          builder, loc, arith::CmpIPredicate::sle, step, zero);
      safeStep =
          arith::SelectOp::create(builder, loc, stepIsTooSmall, one, step);
    }

    Value spanIsNegative = arith::CmpIOp::create(
        builder, loc, arith::CmpIPredicate::slt, span, zero);
    Value nonNegativeSpan =
        arith::SelectOp::create(builder, loc, spanIsNegative, zero, span);
    tripCounts.push_back(
        arith::CeilDivSIOp::create(builder, loc, nonNegativeSpan, safeStep));
  }
  return tripCounts;
}

static SmallVector<Value>
buildPerDimTileIterations(OpBuilder &builder, Location loc,
                          sde::SdeSuIterateOp op,
                          sde::SDECostModel &costModel) {
  SmallVector<Value> tripCounts = buildPerDimTripCounts(builder, loc, op);
  if (tripCounts.empty())
    return {};

  unsigned numDims = tripCounts.size();
  int workersPerDim =
      std::max(1, static_cast<int>(std::ceil(std::pow(
                      getTargetTileTasks(op, costModel), 1.0 / numDims))));
  int64_t minIter = getMinTileIterations(op, costModel);

  SmallVector<Value> tileIterations;
  for (unsigned d = 0; d < numDims; ++d) {
    Value one = createConstantIndex(builder, loc, 1);
    Value workersVal = createConstantIndex(builder, loc, workersPerDim);
    Value clampedTrip =
        arith::MaxUIOp::create(builder, loc, tripCounts[d], one);
    Value minIterVal = buildRelaxedOwnerLocalPipelineMinIterations(
        builder, loc, op, clampedTrip, workersVal, minIter);
    Value balanced =
        arith::CeilDivUIOp::create(builder, loc, clampedTrip, workersVal);
    Value preferred =
        arith::MaxUIOp::create(builder, loc, balanced, minIterVal);
    Value tile = arith::MinUIOp::create(builder, loc, preferred, clampedTrip);
    tileIterations.push_back(tile);
  }
  return tileIterations;
}

static int64_t chooseMatmulColumnWorkers(int64_t workers) {
  workers = std::max<int64_t>(1, workers);
  return std::max<int64_t>(1, static_cast<int64_t>(std::ceil(
                                  std::sqrt(static_cast<double>(workers)))));
}

static int64_t chooseStaticMatmulTile(int64_t extent, int64_t participants,
                                      int64_t minIterations) {
  if (extent <= 1)
    return std::max<int64_t>(1, extent);
  int64_t balanced = sde::ceilDivPositive(extent, participants);
  int64_t preferred =
      std::max<int64_t>(balanced, std::max<int64_t>(1, minIterations));
  return std::clamp<int64_t>(preferred, 1, extent);
}

struct DirectMatmulTileShape {
  sde::LoopIndexedOutputShape output;
  int64_t rowTile = 1;
  int64_t columnTile = 1;
  Value rowTileValue;
  Value columnTileValue;
};

struct PhysicalTileShape {
  SmallVector<int64_t, 4> ownerPhysicalDims;
  SmallVector<int64_t, 4> logicalShape;
  SmallVector<int64_t, 4> blockShape;
  SmallVector<int64_t, 4> logicalWorkerSlice;
  SmallVector<int64_t, 4> haloShape;
  SmallVector<int64_t, 4> tileIterations;
  sde::SdeIterationTopology topology = sde::SdeIterationTopology::owner_strip;
  bool alignSameOwnerReadLayouts = false;
};

static std::optional<sde::LayoutGraphFact>
selectSingleBudgetWriteLayoutFact(sde::SdeSuIterateOp op,
                                  bool allowSingleOwnerDim);
static bool
allExternalStoresCoverOwnerDims(sde::SdeSuIterateOp op,
                                ArrayRef<int64_t> ownerDims,
                                ArrayRef<int64_t> physicalDimToLoopDim = {});

static std::optional<SmallVector<int64_t, 4>>
findBudgetWriteLogicalShape(sde::SdeSuIterateOp op,
                            const sde::LayoutGraphFact &writeLayout) {
  if (writeLayout.id < 0 || writeLayout.budgetBlockShape.empty())
    return std::nullopt;
  std::optional<SmallVector<int64_t, 4>> rootShape =
      sde::findWriteArrayRootShape(op, writeLayout.id);
  if (!rootShape)
    return std::nullopt;
  return sde::collapseRankExpandedRootShape(*rootShape, writeLayout.ownerDims,
                                            writeLayout.budgetBlockShape);
}

static bool hasBudgetReconciledLayoutCandidate(sde::SdeSuIterateOp op) {
  if (!op || sde::hasCommittedWriterBlockLayout(op) ||
      op.getReductionAccumulators().size() != 0)
    return false;

  if (auto classification = sde::queryStructuredClassification(op)) {
    if (*classification != sde::SdeStructuredClassification::elementwise &&
        *classification !=
            sde::SdeStructuredClassification::elementwise_pipeline)
      return false;
  }

  std::optional<sde::LayoutGraphFact> writeLayout =
      selectSingleBudgetWriteLayoutFact(op, /*allowSingleOwnerDim=*/true);
  if (!writeLayout || writeLayout->ownerDims.empty() ||
      op.getLowerBounds().size() < writeLayout->ownerDims.size() ||
      op.getSteps().size() != op.getLowerBounds().size())
    return false;
  if (!findBudgetWriteLogicalShape(op, *writeLayout))
    return false;
  return allExternalStoresCoverOwnerDims(op, writeLayout->ownerDims);
}

static std::optional<DirectMatmulTileShape>
buildDirectMatmulTileShape(OpBuilder &builder, Location loc,
                           sde::SdeSuIterateOp op,
                           sde::SDECostModel &costModel) {
  if (op.getLowerBounds().size() != 1)
    return std::nullopt;
  if (!sde::hasDistinctExternalMatmulInputRoots(op))
    return std::nullopt;

  std::optional<sde::LoopIndexedOutputShape> outputPlan =
      sde::findLoopIndexedOutputShape(op);
  if (!outputPlan || outputPlan->shape.size() < 2)
    return std::nullopt;

  int64_t rows = outputPlan->shape[0];
  int64_t columns = outputPlan->shape[1];
  if (rows <= 1 || columns <= 1)
    return std::nullopt;

  int64_t workers = std::max<int64_t>(1, costModel.getLogicalWorkerCapacity());
  int64_t rowWorkers = std::max<int64_t>(1, getTargetTileTasks(op, costModel));
  int64_t columnWorkers = chooseMatmulColumnWorkers(workers);
  int64_t minIterations =
      std::max<int64_t>(1, costModel.getMinIterationsPerWorker());

  DirectMatmulTileShape plan;
  plan.output = std::move(*outputPlan);
  /// Keep the SDE-owned row dimension exposed to the worker distributor.
  /// The column tile is an inner locality tile; row tiling must not collapse
  /// the distributed task count down to the square-root worker grid.
  plan.rowTile = chooseStaticMatmulTile(rows, rowWorkers, /*minIterations=*/1);
  plan.columnTile =
      chooseStaticMatmulTile(columns, columnWorkers, minIterations);
  if (plan.rowTile <= 1 && plan.columnTile <= 1)
    return std::nullopt;

  plan.rowTileValue = createConstantIndex(builder, loc, plan.rowTile);
  plan.columnTileValue = createConstantIndex(builder, loc, plan.columnTile);
  return plan;
}

static bool isScalarExecutableOp(Operation &op) {
  if (isa<memref::LoadOp, memref::StoreOp>(op))
    return true;
  return op.getNumRegions() == 0 && isMemoryEffectFree(&op);
}

static bool isExecutableInnermostBody(Block &body) {
  for (Operation &op : body.without_terminator()) {
    if (!isScalarExecutableOp(op))
      return false;
  }
  return true;
}

static bool isStencilTileBody(Block &body);

static bool isStencilTileBodyOp(Operation &op) {
  if (isScalarExecutableOp(op))
    return true;

  // Point-local stencil bodies carry per-iteration scalar scratch as
  // `memref.alloca` slots (hoisted locals not promoted to SSA). These are
  // function-local stack scratch, not shared loop state, so they do not block
  // tiling of an otherwise point-local stencil nest.
  if (isa<memref::AllocaOp>(op))
    return true;

  if (auto ifOp = dyn_cast<scf::IfOp>(op)) {
    for (Region &region : ifOp->getRegions()) {
      if (!llvm::all_of(region,
                        [](Block &block) { return isStencilTileBody(block); }))
        return false;
    }
    return true;
  }

  return false;
}

static bool isStencilTileBody(Block &body) {
  for (Operation &op : body.without_terminator()) {
    if (!isStencilTileBodyOp(op))
      return false;
  }
  return true;
}

static bool hasPerfectNestedScalarLoopNest(Block &body, unsigned numLoops) {
  if (numLoops == 0)
    return false;
  if (numLoops == 1)
    return isExecutableInnermostBody(body);

  Operation *root = nullptr;
  for (Operation &op : body.without_terminator()) {
    if (!isa<scf::ForOp>(op) || root)
      return false;
    root = &op;
  }
  auto rootLoop = dyn_cast_or_null<scf::ForOp>(root);
  if (!rootLoop || !rootLoop.getInitArgs().empty())
    return false;

  SmallVector<scf::ForOp, 4> loops;
  mlir::getPerfectlyNestedLoops(loops, rootLoop);
  if (loops.size() != numLoops - 1)
    return false;
  if (llvm::any_of(loops,
                   [](scf::ForOp loop) { return !loop.getInitArgs().empty(); }))
    return false;
  return isExecutableInnermostBody(*loops.back().getBody());
}

static bool hasPerfectNestedAffineLoopNest(Block &body, unsigned numLoops) {
  if (numLoops == 0)
    return false;
  if (numLoops == 1)
    return isExecutableInnermostBody(body);

  Operation *root = nullptr;
  for (Operation &op : body.without_terminator()) {
    if (!isa<affine::AffineForOp>(op) || root)
      return false;
    root = &op;
  }
  auto rootLoop = dyn_cast_or_null<affine::AffineForOp>(root);
  if (!rootLoop)
    return false;

  SmallVector<affine::AffineForOp, 4> loops;
  affine::getPerfectlyNestedLoops(loops, rootLoop);
  return loops.size() == numLoops - 1 &&
         isExecutableInnermostBody(*loops.back().getBody());
}

static bool loopWritesOwnerColumn(scf::ForOp loop, Value outputRoot,
                                  Value ownerIv) {
  if (!loop || !outputRoot || !ownerIv || loop.getNumResults() != 0 ||
      !loop.getInitArgs().empty())
    return false;

  Value loopIv = loop.getInductionVar();
  bool matched = false;
  loop.walk([&](memref::StoreOp store) {
    Value root =
        ::mlir::carts::ValueAnalysis::stripMemrefViewOps(store.getMemref());
    if (root != outputRoot)
      return WalkResult::advance();

    ValueRange indices = store.getIndices();
    if (indices.size() < 2)
      return WalkResult::advance();
    if (!::mlir::carts::ValueAnalysis::dependsOn(indices.front(), ownerIv))
      return WalkResult::advance();

    for (Value index : indices.drop_front()) {
      if (::mlir::carts::ValueAnalysis::dependsOn(index, loopIv)) {
        matched = true;
        return WalkResult::interrupt();
      }
    }
    return WalkResult::advance();
  });
  return matched;
}

static bool loopWritesOwnerColumn(affine::AffineForOp loop, Value outputRoot,
                                  Value ownerIv) {
  if (!loop || !outputRoot || !ownerIv || loop.getNumResults() != 0)
    return false;

  Value loopIv = loop.getInductionVar();
  bool matched = false;
  loop.walk([&](memref::StoreOp store) {
    Value root =
        ::mlir::carts::ValueAnalysis::stripMemrefViewOps(store.getMemref());
    if (root != outputRoot)
      return WalkResult::advance();

    ValueRange indices = store.getIndices();
    if (indices.size() < 2)
      return WalkResult::advance();
    if (!::mlir::carts::ValueAnalysis::dependsOn(indices.front(), ownerIv))
      return WalkResult::advance();

    for (Value index : indices.drop_front()) {
      if (::mlir::carts::ValueAnalysis::dependsOn(index, loopIv)) {
        matched = true;
        return WalkResult::interrupt();
      }
    }
    return WalkResult::advance();
  });
  return matched;
}

static void
collectDirectMatmulColumnLoops(Block &body, Value outputRoot, Value ownerIv,
                               SmallVectorImpl<scf::ForOp> &columnLoops) {
  body.walk([&](scf::ForOp loop) {
    if (loopWritesOwnerColumn(loop, outputRoot, ownerIv))
      columnLoops.push_back(loop);
  });
}

static void collectDirectMatmulColumnAffineLoops(
    Block &body, Value outputRoot, Value ownerIv,
    SmallVectorImpl<affine::AffineForOp> &columnLoops) {
  body.walk([&](affine::AffineForOp loop) {
    if (loopWritesOwnerColumn(loop, outputRoot, ownerIv))
      columnLoops.push_back(loop);
  });
}

static bool isDirectMemoryMatmulCandidate(sde::SdeSuIterateOp op, Block &body) {
  if (op.getLowerBounds().size() != 1 ||
      op.getReductionAccumulators().size() != 0)
    return false;

  std::optional<sde::LoopIndexedOutputShape> outputPlan =
      sde::findLoopIndexedOutputShape(op);
  if (!outputPlan || outputPlan->shape.size() < 2)
    return false;

  auto effects = sde::collectStructuredMemoryEffects(op.getBody());
  if (effects.hasUnknownEffects || !effects.writes.contains(outputPlan->root))
    return false;

  Block &suBody = op.getBody().front();
  Value ownerIv = suBody.getArgument(0);
  SmallVector<scf::ForOp, 4> columnLoops;
  collectDirectMatmulColumnLoops(body, outputPlan->root, ownerIv, columnLoops);
  if (!columnLoops.empty())
    return true;
  SmallVector<affine::AffineForOp, 4> affineColumnLoops;
  collectDirectMatmulColumnAffineLoops(body, outputPlan->root, ownerIv,
                                       affineColumnLoops);
  return !affineColumnLoops.empty();
}

/// Return a per-SDE-dim mask: true = parallel (should tile), false = reduction
/// (keep original step). Memref SDE tiling only tiles scheduling-unit owner
/// dimensions here; reduction-specific tiling is handled by dedicated SDE
/// planning passes.
static SmallVector<bool> getParallelDimMask(sde::SdeSuIterateOp op) {
  unsigned numDims = op.getLowerBounds().size();
  return SmallVector<bool>(numDims, true);
}

static Value getExternalAccessRoot(sde::SdeSuIterateOp op, Value value) {
  if (!value)
    return {};
  Value root = value;
  if (isa<BaseMemRefType>(root.getType()))
    root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(root);
  if (!root || sde::isDefinedInside(op.getOperation(), root))
    return {};
  auto type = dyn_cast<MemRefType>(root.getType());
  if (!type || type.getRank() == 0)
    return {};
  return root;
}

static bool
hasNonPointExternalSelfRead(sde::SdeSuIterateOp op,
                            const sde::SuLoopAccessSummary &summary) {
  for (const sde::MemrefAccessEntry &write : summary.writes) {
    Value writeRoot = getExternalAccessRoot(op, write.memref);
    if (!writeRoot)
      continue;
    for (const sde::MemrefAccessEntry &read : summary.reads) {
      Value readRoot = getExternalAccessRoot(op, read.memref);
      if (readRoot == writeRoot && read.indexingMap != write.indexingMap)
        return true;
    }
  }
  return false;
}

static bool hasPromotedParallelOutputSchedule(sde::SdeSuIterateOp op) {
  unsigned scheduleRank = op.getLowerBounds().size();
  if (scheduleRank < 2 || op.getUpperBounds().size() != scheduleRank ||
      op.getSteps().size() != scheduleRank)
    return false;

  std::optional<sde::SuLoopAccessSummary> summary =
      sde::analyzeSuLoopAccesses(op);
  if (!summary || summary->iterTypes.size() < scheduleRank ||
      summary->nest.ivs.size() < scheduleRank)
    return false;
  for (unsigned dim = 0; dim < scheduleRank; ++dim)
    if (summary->iterTypes[dim] != utils::IteratorType::parallel)
      return false;
  if (!sde::findCompatibleSuOutputLayoutFacts(*summary))
    return false;
  return !hasNonPointExternalSelfRead(op, *summary);
}

static std::optional<PhysicalTileShape>
buildPromotedMatmulPhysicalTileShape(sde::SdeSuIterateOp op,
                                     sde::SDECostModel &costModel) {
  if (!hasPromotedParallelOutputSchedule(op))
    return std::nullopt;

  std::optional<sde::SuOutputLayoutFacts> outputPlan =
      sde::findCompatibleSuOutputLayoutFacts(op);
  if (!outputPlan || outputPlan->shape.size() < 2 ||
      outputPlan->loopDimToPhysicalDim.size() < op.getLowerBounds().size())
    return std::nullopt;
  if (!sde::hasDistinctExternalMatmulInputRoots(op))
    return std::nullopt;

  PhysicalTileShape plan;
  plan.logicalShape.assign(outputPlan->shape.begin(), outputPlan->shape.end());
  unsigned scheduleRank = op.getLowerBounds().size();
  for (unsigned loopDim = 0; loopDim < scheduleRank; ++loopDim) {
    int64_t physicalDim = outputPlan->loopDimToPhysicalDim[loopDim];
    if (physicalDim < 0 ||
        static_cast<size_t>(physicalDim) >= outputPlan->shape.size())
      return std::nullopt;
    plan.ownerPhysicalDims.push_back(physicalDim);
  }
  if (plan.ownerPhysicalDims.size() < 2)
    return std::nullopt;

  SmallVector<int64_t, 4> ownerExtents;
  ownerExtents.reserve(plan.ownerPhysicalDims.size());
  for (int64_t physicalDim : plan.ownerPhysicalDims) {
    int64_t extent = outputPlan->shape[physicalDim];
    if (extent <= 0)
      return std::nullopt;
    ownerExtents.push_back(extent);
  }

  SmallVector<int64_t, 4> workerGrid = sde::factorWorkersAcrossDims(
      std::max<int64_t>(1, getTargetTileTasks(op, costModel)), ownerExtents);
  if (workerGrid.size() != plan.ownerPhysicalDims.size())
    return std::nullopt;

  plan.blockShape.assign(outputPlan->shape.begin(), outputPlan->shape.end());
  for (auto [slot, physicalDim] : llvm::enumerate(plan.ownerPhysicalDims))
    plan.blockShape[physicalDim] =
        sde::ceilDivPositive(outputPlan->shape[physicalDim], workerGrid[slot]);

  if (std::optional<sde::LayoutGraphFact> writeLayout =
          selectSingleBudgetWriteLayoutFact(op,
                                            /*allowSingleOwnerDim=*/true)) {
    if (writeLayout->budgetBlockShape.size() != plan.blockShape.size())
      return std::nullopt;
    for (int64_t physicalDim : writeLayout->ownerDims) {
      if (!llvm::is_contained(plan.ownerPhysicalDims, physicalDim))
        return std::nullopt;
      if (physicalDim < 0 ||
          static_cast<size_t>(physicalDim) >= plan.blockShape.size())
        return std::nullopt;
      int64_t budget = writeLayout->budgetBlockShape[physicalDim];
      if (budget <= 0)
        return std::nullopt;
      plan.blockShape[physicalDim] =
          std::min<int64_t>(plan.blockShape[physicalDim], budget);
    }
  }

  plan.tileIterations.assign(scheduleRank, 1);
  for (unsigned loopDim = 0; loopDim < scheduleRank; ++loopDim) {
    int64_t physicalDim = outputPlan->loopDimToPhysicalDim[loopDim];
    int64_t tile = plan.blockShape[physicalDim];
    if (tile <= 0)
      return std::nullopt;
    std::optional<int64_t> step =
        ValueAnalysis::getPositiveConstantIndex(op.getSteps()[loopDim]);
    if (!step || *step != 1)
      return std::nullopt;
    plan.tileIterations[loopDim] = tile;
  }

  plan.logicalWorkerSlice.assign(plan.blockShape.begin(),
                                 plan.blockShape.end());
  plan.topology = plan.ownerPhysicalDims.size() == 2
                      ? sde::SdeIterationTopology::owner_tile_2d
                      : sde::SdeIterationTopology::owner_tile;
  return plan;
}

static bool isStencilCandidate(sde::SdeSuIterateOp op, Block &body) {
  if (!sde::queryNeighborhoodAccessInfo(op))
    return false;
  // Stencils always have at least one nested scf.for for the inner dimension.
  // Count the total loop depth: 1 SDE dim + inner scf.for loops.
  unsigned numSuDims = op.getLowerBounds().size();
  unsigned innerLoops = 0;
  Block *current = &body;
  while (true) {
    scf::ForOp nestedLoop;
    for (Operation &nested : current->without_terminator()) {
      if (auto forOp = dyn_cast<scf::ForOp>(nested)) {
        if (nestedLoop)
          return false; // imperfect nest
        nestedLoop = forOp;
      } else if (!isStencilTileBodyOp(nested)) {
        return false;
      }
    }
    if (!nestedLoop)
      break;
    ++innerLoops;
    current = nestedLoop.getBody();
  }
  return isStencilTileBody(*current) && (numSuDims + innerLoops) >= 1;
}

static SmallVector<int64_t> getStencilHaloWidths(sde::SdeSuIterateOp op) {
  SmallVector<int64_t> halos;
  std::optional<sde::SuNeighborhoodAccessInfo> neighborhood =
      sde::queryNeighborhoodAccessInfo(op);
  if (!neighborhood ||
      neighborhood->minOffsets.size() != neighborhood->maxOffsets.size())
    return {};
  for (unsigned d = 0; d < neighborhood->minOffsets.size(); ++d) {
    int64_t lo = neighborhood->minOffsets[d];
    int64_t hi = neighborhood->maxOffsets[d];
    halos.push_back(std::max<int64_t>(1, hi - lo + 1));
  }
  return halos;
}

static SmallVector<int64_t>
getStencilHaloRadiiForOwnerDims(sde::SdeSuIterateOp op,
                                unsigned ownerDimCount) {
  SmallVector<int64_t> halos;
  std::optional<sde::SuNeighborhoodAccessInfo> neighborhood =
      sde::queryNeighborhoodAccessInfo(op);
  if (!neighborhood ||
      neighborhood->minOffsets.size() != neighborhood->maxOffsets.size())
    return {};
  for (unsigned d = 0; d < ownerDimCount && d < neighborhood->minOffsets.size();
       ++d) {
    int64_t lo = neighborhood->minOffsets[d];
    int64_t hi = neighborhood->maxOffsets[d];
    halos.push_back(std::max<int64_t>(0, std::max(-lo, hi)));
  }
  return halos;
}

static int64_t relaxOwnerLocalPipelineMinIterations(sde::SdeSuIterateOp op,
                                                    int64_t tripCount,
                                                    int64_t targetTasks,
                                                    int64_t minIterations) {
  if (!usesOwnerLocalPipelineGrain(op) || targetTasks <= 1 ||
      minIterations <= 1)
    return minIterations;

  bool productOverflows =
      targetTasks > std::numeric_limits<int64_t>::max() / minIterations;
  if (productOverflows || tripCount < targetTasks * minIterations)
    return 1;
  return minIterations;
}

static std::optional<SmallVector<int64_t, 4>>
computeStaticTileIterations(sde::SdeSuIterateOp op,
                            sde::SDECostModel &costModel) {
  unsigned numDims = op.getLowerBounds().size();
  if (numDims == 0)
    return std::nullopt;

  SmallVector<int64_t, 4> tripCounts;
  tripCounts.reserve(numDims);
  for (unsigned d = 0; d < numDims; ++d) {
    int64_t lb = 0;
    int64_t ub = 0;
    int64_t step = 0;
    if (!::mlir::carts::ValueAnalysis::getConstantIndex(op.getLowerBounds()[d],
                                                        lb) ||
        !::mlir::carts::ValueAnalysis::getConstantIndex(op.getUpperBounds()[d],
                                                        ub) ||
        !::mlir::carts::ValueAnalysis::getConstantIndex(op.getSteps()[d],
                                                        step) ||
        step <= 0)
      return std::nullopt;
    tripCounts.push_back(
        sde::ceilDivPositive(std::max<int64_t>(0, ub - lb), step));
  }

  int64_t minIter = getMinTileIterations(op, costModel);
  SmallVector<int64_t, 4> tileIterations;
  tileIterations.reserve(numDims);
  if (numDims == 1) {
    int64_t tripCount = tripCounts.front();
    if (tripCount <= 0)
      return std::nullopt;
    int64_t targetTasks = getTargetTileTasks(op, costModel);
    minIter = relaxOwnerLocalPipelineMinIterations(op, tripCount, targetTasks,
                                                   minIter);
    int64_t balanced = sde::ceilDivPositive(tripCount, targetTasks);
    tileIterations.push_back(std::clamp(std::max<int64_t>(balanced, minIter),
                                        int64_t{1}, tripCount));
    return tileIterations;
  }

  int64_t workersPerDim = std::max<int64_t>(
      1, static_cast<int64_t>(std::ceil(
             std::pow(getTargetTileTasks(op, costModel), 1.0 / numDims))));
  for (int64_t tripCount : tripCounts) {
    if (tripCount <= 0)
      return std::nullopt;
    int64_t balanced = sde::ceilDivPositive(tripCount, workersPerDim);
    tileIterations.push_back(std::clamp(std::max<int64_t>(balanced, minIter),
                                        int64_t{1}, tripCount));
  }
  return tileIterations;
}

static void
applyStencilTileGuardsToStaticPlan(sde::SdeSuIterateOp op,
                                   SmallVectorImpl<int64_t> &tileIterations,
                                   unsigned numDims) {
  // Enforce the halo floor only. The former L2-cache tile cap fed on a
  // fabricated getL2CacheSize() literal (a HARD-RULE fabricated-number
  // violation, removed with the rest of the cost-model hardware-param family);
  // it only relaxed an upper bound, so dropping it preserves correctness and
  // merely widens stencil tiles where the cap was binding.
  SmallVector<int64_t> halos = getStencilHaloWidths(op);
  for (unsigned d = 0;
       d < numDims && d < halos.size() && d < tileIterations.size(); ++d)
    tileIterations[d] = std::max<int64_t>(tileIterations[d], halos[d]);
}

static std::optional<PhysicalTileShape>
buildStencilPhysicalTileShape(sde::SdeSuIterateOp op,
                              ArrayRef<int64_t> tileIterations) {
  if (sde::hasCommittedWriterBlockLayout(op) ||
      sde::queryInPlaceSharedState(op))
    return std::nullopt;
  if (auto neighborhood = sde::queryNeighborhoodAccessInfo(op))
    if (neighborhood->ownerDims.size() > op.getLowerBounds().size())
      return std::nullopt;
  // The loop-indexed output helper proves the current SDE owner IV only. For
  // multi-dimensional/component stencils, OwnerDimSelect owns the final
  // ND owner plan.
  if (op.getLowerBounds().size() != 1)
    return std::nullopt;
  if (sde::requiresNestedStencilOwnerPromotion(op))
    return std::nullopt;
  auto effects = sde::collectStructuredMemoryEffects(op.getBody());
  bool ownerLocalPipeline = [&]() {
    auto cls = sde::queryStructuredClassification(op);
    return cls &&
           *cls == sde::SdeStructuredClassification::elementwise_pipeline &&
           sde::isOwnerLocalPipelineReduction(op);
  }();
  if (effects.hasUnknownEffects ||
      (sde::hasInPlaceSelfRead(effects) && !ownerLocalPipeline))
    return std::nullopt;

  std::optional<sde::LoopIndexedOutputShape> outputPlan =
      sde::findLoopIndexedOutputShape(op);
  if (!outputPlan || outputPlan->shape.empty() ||
      outputPlan->ownerPhysicalDims.empty())
    return std::nullopt;
  if (outputPlan->ownerPhysicalDims.size() > tileIterations.size())
    return std::nullopt;

  PhysicalTileShape plan;
  plan.logicalShape.assign(outputPlan->shape.begin(), outputPlan->shape.end());
  plan.ownerPhysicalDims.assign(outputPlan->ownerPhysicalDims.begin(),
                                outputPlan->ownerPhysicalDims.end());
  plan.blockShape.assign(outputPlan->shape.begin(), outputPlan->shape.end());
  plan.tileIterations.assign(tileIterations.begin(), tileIterations.end());
  for (auto [idx, physicalDim] : llvm::enumerate(plan.ownerPhysicalDims)) {
    if (physicalDim < 0 ||
        static_cast<size_t>(physicalDim) >= plan.blockShape.size())
      return std::nullopt;
    plan.blockShape[physicalDim] = tileIterations[idx];
  }

  plan.haloShape =
      getStencilHaloRadiiForOwnerDims(op, plan.ownerPhysicalDims.size());
  plan.topology = plan.ownerPhysicalDims.size() > 1
                      ? sde::SdeIterationTopology::owner_tile
                      : sde::SdeIterationTopology::owner_strip;
  return plan;
}

static std::optional<PhysicalTileShape>
buildNdStencilPhysicalTileShape(sde::SdeSuIterateOp op,
                                sde::SDECostModel &costModel) {
  // Accept loop rank >= 1: a 1-D parallel band over a multi-dim access
  // footprint (point-local stencil) realizes a 1-D owner strip; the wider
  // access footprint must be carried as read-only halo movement along that
  // strip. The owner-dim selection below already filters `ownerDims` to entries
  // within the loop rank, so a too-wide footprint never produces an over-ranked
  // plan.
  if (op.getLowerBounds().empty() || sde::hasCommittedWriterBlockLayout(op) ||
      sde::queryInPlaceSharedState(op))
    return std::nullopt;
  auto pattern = sde::querySuPattern(op);
  if (!pattern || (*pattern != sde::SdePattern::cross_dim_stencil_3d &&
                   *pattern != sde::SdePattern::stencil_tiling_nd &&
                   *pattern != sde::SdePattern::higher_order_stencil))
    return std::nullopt;

  auto effects = sde::collectStructuredMemoryEffects(op.getBody());
  // A proven point-local stencil (`inPlaceSafe`) self-reads its own cell only;
  // that is a tileable owner-local read, not a loop-carried neighbor read.
  if (effects.hasUnknownEffects ||
      (sde::hasInPlaceSelfRead(effects) && !sde::queryInPlaceSafe(op)))
    return std::nullopt;

  std::optional<sde::SuOutputLayoutFacts> outputPlan =
      sde::findCompatibleSuOutputLayoutFacts(op);
  if (!outputPlan || outputPlan->shape.empty())
    return std::nullopt;

  auto neighborhood = sde::queryNeighborhoodAccessInfo(op);
  if (!neighborhood)
    return std::nullopt;

  PhysicalTileShape plan;
  plan.logicalShape.assign(outputPlan->shape.begin(), outputPlan->shape.end());
  plan.blockShape.assign(outputPlan->shape.begin(), outputPlan->shape.end());
  plan.tileIterations.assign(outputPlan->shape.begin(),
                             outputPlan->shape.end());
  if (plan.tileIterations.size() < op.getLowerBounds().size())
    return std::nullopt;
  plan.tileIterations.resize(op.getLowerBounds().size());

  SmallVector<int64_t, 4> ownerExtents;
  SmallVector<unsigned, 4> ownerLoopDims;
  for (auto [idx, rawLoopDim] : llvm::enumerate(neighborhood->ownerDims)) {
    if (rawLoopDim < 0 ||
        static_cast<size_t>(rawLoopDim) >= op.getLowerBounds().size() ||
        static_cast<size_t>(rawLoopDim) >=
            outputPlan->loopDimToPhysicalDim.size() ||
        idx >= neighborhood->minOffsets.size())
      continue;
    int64_t physicalDim = outputPlan->loopDimToPhysicalDim[rawLoopDim];
    if (physicalDim < 0 ||
        static_cast<size_t>(physicalDim) >= outputPlan->shape.size())
      continue;
    int64_t halo =
        std::max<int64_t>(0, std::max(-neighborhood->minOffsets[idx],
                                      neighborhood->maxOffsets[idx]));
    if (halo == 0)
      continue;
    ownerLoopDims.push_back(static_cast<unsigned>(rawLoopDim));
    plan.ownerPhysicalDims.push_back(physicalDim);
    plan.haloShape.push_back(halo);
    ownerExtents.push_back(outputPlan->shape[physicalDim]);
  }
  if (plan.ownerPhysicalDims.empty())
    return std::nullopt;

  int64_t targetWorkers =
      std::max<int64_t>(1, getTargetTileTasks(op, costModel));

  auto computeBlockShape = [&](int64_t workers,
                               SmallVectorImpl<int64_t> &blockShape) {
    blockShape.assign(outputPlan->shape.begin(), outputPlan->shape.end());
    SmallVector<int64_t, 4> grid = sde::factorStencilWorkersAcrossDims(
        std::max<int64_t>(1, workers), ownerExtents, plan.haloShape);
    for (auto [idx, physicalDim] : llvm::enumerate(plan.ownerPhysicalDims))
      blockShape[physicalDim] =
          sde::ceilDivPositive(outputPlan->shape[physicalDim], grid[idx]);
  };

  SmallVector<int64_t, 4> initialBlockShape;
  computeBlockShape(targetWorkers, initialBlockShape);

  // Do not inflate halo stencil DB/MU grain to satisfy a tile-byte floor. Any
  // coarser stencil compute slice needs lane-specific halo acquire support in
  // ARTS before SDE can group the logical worker slice.
  SmallVector<int64_t, 4> finalBlockShape(initialBlockShape.begin(),
                                          initialBlockShape.end());
  for (auto [idx, physicalDim] : llvm::enumerate(plan.ownerPhysicalDims)) {
    int64_t tile = finalBlockShape[physicalDim];
    plan.blockShape[physicalDim] = tile;
    unsigned loopDim = ownerLoopDims[idx];
    if (loopDim < plan.tileIterations.size())
      plan.tileIterations[loopDim] = tile;
  }

  plan.topology = plan.ownerPhysicalDims.size() > 1
                      ? sde::SdeIterationTopology::owner_tile
                      : sde::SdeIterationTopology::owner_strip;
  return plan;
}

static void commitPhysicalTileShape(sde::SdeSuIterateOp op,
                                    const PhysicalTileShape &plan) {
  ArrayRef<int64_t> logicalSlice =
      plan.logicalWorkerSlice.empty()
          ? ArrayRef<int64_t>(plan.blockShape)
          : ArrayRef<int64_t>(plan.logicalWorkerSlice);
  sde::commitWriterPhysicalLayoutFacts(op, plan.ownerPhysicalDims,
                                       plan.blockShape, logicalSlice);
  auto classification = sde::queryStructuredClassification(op);
  if (classification &&
      *classification == sde::SdeStructuredClassification::stencil)
    (void)sde::rewriteSameOwnerReadLayoutsToPhysicalShape(
        op, plan.ownerPhysicalDims, plan.blockShape, plan.logicalShape);
  else if (plan.alignSameOwnerReadLayouts)
    (void)sde::rewriteSameOwnerReadLayoutsToPhysicalShape(
        op, plan.ownerPhysicalDims, plan.blockShape, plan.logicalShape);
}

static std::optional<unsigned> mapLoopDimToPhysicalDim(sde::SdeSuIterateOp op,
                                                       unsigned loopDim) {
  if (std::optional<sde::LayoutGraphFact> writeLayout =
          sde::findSingleCommittedWriterBlockLayout(op)) {
    if (loopDim < writeLayout->ownerDims.size() &&
        writeLayout->ownerDims[loopDim] >= 0)
      return static_cast<unsigned>(writeLayout->ownerDims[loopDim]);
  }
  return loopDim;
}

static bool isBudgetReconciledTileCandidate(sde::SdeSuIterateOp op) {
  if (!op || sde::hasCommittedWriterBlockLayout(op) ||
      op.getReductionAccumulators().size() != 0)
    return false;

  auto classification = sde::queryStructuredClassification(op);
  if (!classification)
    return false;

  if (*classification == sde::SdeStructuredClassification::elementwise ||
      *classification ==
          sde::SdeStructuredClassification::elementwise_pipeline) {
    if (sde::queryInPlaceSharedState(op) && !sde::queryInPlaceSafe(op) &&
        !sde::isOwnerLocalPipelineReduction(op))
      return false;
    // Budget reconciliation is needed when the assigned block layout is finer
    // than the generic tiled loop, including in-place elementwise updates over
    // an already block-shaped MU. The real SU step must match that block before
    // access windows and ARTS dependencies consume it.
    return true;
  }

  if (*classification == sde::SdeStructuredClassification::stencil) {
    if (sde::queryInPlaceSharedState(op))
      return false;
    // Stencil DB/MU grain is owned by the halo-aware stencil tiler. A
    // budgetBlockShape on an assigned layout is only a candidate, not a
    // committed physical block size.
    return true;
  }

  if (*classification == sde::SdeStructuredClassification::reduction)
    return hasPromotedParallelOutputSchedule(op);

  return false;
}

// The op's representative write-role budget fact. Single-store writers return
// their one fact. An affine-disjoint MULTI-store writer (e.g. an init that
// writes A/B/C in one nest) returns a representative fact only when every
// written array names a distinct id and all writes agree on ownerDims +
// budgetBlockShape + block_parallel kind, so one shared budget grain is correct
// for all of them. A true multi-writer (same id stored twice), any
// owner/grain/kind disagreement, or a non-block layout fails closed -- the SU
// then keeps the generic tiler, never an unverifiable shared grain. Tiling owns
// this budget grain as a real loop retile while the loop is still step-1, so
// the grain is structurally true rather than an attr-only promise.
static std::optional<sde::LayoutGraphFact>
selectSingleBudgetWriteLayoutFact(sde::SdeSuIterateOp op,
                                  bool allowSingleOwnerDim) {
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return std::nullopt;

  const unsigned minOwnerDims = allowSingleOwnerDim ? 1u : 2u;
  std::optional<sde::LayoutGraphFact> rep;
  llvm::SmallDenseSet<int64_t, 4> writtenIds;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    if (fact.role != sde::LayoutGraphRole::write)
      continue;
    if (fact.id < 0 || fact.layoutKind != sde::ArrayLayoutKind::blockParallel ||
        fact.ownerDims.size() < minOwnerDims || fact.budgetBlockShape.empty())
      return std::nullopt;
    if (!writtenIds.insert(fact.id).second)
      return std::nullopt; // same id written twice => true multi-writer
    if (!rep) {
      rep = fact;
      continue;
    }
    if (rep->ownerDims != fact.ownerDims ||
        rep->budgetBlockShape != fact.budgetBlockShape)
      return std::nullopt; // arrays disagree on grain/owner => no shared block
  }
  return rep;
}

static bool
allExternalStoresCoverOwnerDims(sde::SdeSuIterateOp op,
                                ArrayRef<int64_t> ownerDims,
                                ArrayRef<int64_t> physicalDimToLoopDim) {
  if (!op || ownerDims.empty() || op.getBody().empty())
    return true;

  auto loopIvs = op.getLoopInductionVars();
  if (!loopIvs || loopIvs->empty())
    return false;

  bool sawExternalStore = false;
  bool rejected = false;
  op.getBody().walk([&](memref::StoreOp storeOp) {
    if (rejected)
      return;
    Value root =
        ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storeOp.getMemref());
    if (!root || sde::isDefinedInside(op.getOperation(), root))
      return;
    auto memrefType = dyn_cast<MemRefType>(root.getType());
    if (!memrefType) {
      rejected = true;
      return;
    }
    if (memrefType.getRank() == 0)
      return;

    sawExternalStore = true;
    OperandRange indices = storeOp.getIndices();
    for (auto [ownerSlot, ownerDim] : llvm::enumerate(ownerDims)) {
      if (ownerDim < 0 || static_cast<unsigned>(ownerDim) >= indices.size()) {
        rejected = true;
        return;
      }
      int64_t loopDim = static_cast<int64_t>(ownerSlot);
      if (!physicalDimToLoopDim.empty()) {
        if (static_cast<size_t>(ownerDim) >= physicalDimToLoopDim.size()) {
          rejected = true;
          return;
        }
        loopDim = physicalDimToLoopDim[ownerDim];
      }
      if (loopDim < 0 || static_cast<size_t>(loopDim) >= loopIvs->size()) {
        rejected = true;
        return;
      }
      if (!sde::isOwnerDependentIndex(indices[ownerDim], (*loopIvs)[loopDim])) {
        rejected = true;
        return;
      }
    }
  });

  return sawExternalStore && !rejected;
}

static ArrayRef<int64_t>
assignedPhysicalBlockShape(const sde::LayoutGraphFact &fact) {
  return fact.budgetBlockShape.empty()
             ? ArrayRef<int64_t>(fact.blockShape)
             : ArrayRef<int64_t>(fact.budgetBlockShape);
}

static std::optional<sde::LayoutGraphFact>
selectSingleUnownedAssignedWriteLayoutFact(sde::SdeSuIterateOp op) {
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return std::nullopt;

  std::optional<sde::LayoutGraphFact> selected;
  llvm::SmallDenseSet<int64_t, 4> writtenIds;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    if (fact.role != sde::LayoutGraphRole::write)
      continue;
    if (fact.id < 0 || fact.layoutKind != sde::ArrayLayoutKind::blockParallel ||
        !fact.ownerDims.empty() || fact.blockShape.empty())
      return std::nullopt;
    if (!writtenIds.insert(fact.id).second)
      return std::nullopt;
    if (!selected) {
      selected = fact;
      continue;
    }
    if (assignedPhysicalBlockShape(*selected) !=
        assignedPhysicalBlockShape(fact))
      return std::nullopt;
  }
  return selected;
}

static std::optional<PhysicalTileShape>
buildCoiteratedReadWriterTilePlan(sde::SdeSuIterateOp op) {
  if (!op || sde::hasCommittedWriterBlockLayout(op))
    return std::nullopt;
  auto classification = sde::queryStructuredClassification(op);
  if (!classification ||
      (*classification != sde::SdeStructuredClassification::stencil &&
       *classification != sde::SdeStructuredClassification::elementwise &&
       *classification !=
           sde::SdeStructuredClassification::elementwise_pipeline))
    return std::nullopt;
  if (*classification == sde::SdeStructuredClassification::stencil &&
      sde::queryInPlaceSharedState(op))
    return std::nullopt;

  std::optional<sde::LayoutGraphFact> writeLayout =
      selectSingleUnownedAssignedWriteLayoutFact(op);
  if (!writeLayout)
    return std::nullopt;
  std::optional<sde::SuOutputLayoutFacts> outputPlan =
      sde::findCompatibleSuOutputLayoutFacts(op);
  if (!outputPlan || outputPlan->shape.empty() ||
      outputPlan->loopDimToPhysicalDim.size() < op.getLowerBounds().size() ||
      outputPlan->physicalDimToLoopDim.size() != outputPlan->shape.size())
    return std::nullopt;

  ArrayRef<int64_t> writeBlock = assignedPhysicalBlockShape(*writeLayout);
  if (writeBlock.size() != outputPlan->shape.size())
    return std::nullopt;

  std::optional<sde::LayoutGraphFact> selectedRead;
  for (const sde::LayoutGraphFact &fact :
       sde::parseArrayLayoutFacts(op.getArrayLayoutAttr())) {
    if (fact.role != sde::LayoutGraphRole::read ||
        fact.layoutKind != sde::ArrayLayoutKind::blockParallel ||
        fact.ownerDims.size() != op.getLowerBounds().size())
      continue;
    if (assignedPhysicalBlockShape(fact) != writeBlock)
      continue;
    if (!allExternalStoresCoverOwnerDims(op, fact.ownerDims,
                                         outputPlan->physicalDimToLoopDim))
      continue;
    if (!selectedRead) {
      selectedRead = fact;
      continue;
    }
    if (selectedRead->ownerDims != fact.ownerDims)
      return std::nullopt;
  }
  if (!selectedRead)
    return std::nullopt;

  PhysicalTileShape plan;
  plan.logicalShape.assign(outputPlan->shape.begin(), outputPlan->shape.end());
  plan.ownerPhysicalDims.assign(selectedRead->ownerDims.begin(),
                                selectedRead->ownerDims.end());
  plan.blockShape.assign(writeBlock.begin(), writeBlock.end());
  plan.tileIterations.assign(op.getLowerBounds().size(), 1);
  for (unsigned loopDim = 0; loopDim < op.getLowerBounds().size(); ++loopDim) {
    if (loopDim >= outputPlan->loopDimToPhysicalDim.size())
      return std::nullopt;
    int64_t physicalDim = outputPlan->loopDimToPhysicalDim[loopDim];
    if (physicalDim < 0 ||
        static_cast<size_t>(physicalDim) >= plan.blockShape.size())
      return std::nullopt;
    std::optional<int64_t> step =
        ValueAnalysis::getPositiveConstantIndex(op.getSteps()[loopDim]);
    if (!step || *step != 1)
      return std::nullopt;
    plan.tileIterations[loopDim] = plan.blockShape[physicalDim];
  }
  if (llvm::all_of(plan.tileIterations, [](int64_t tile) { return tile <= 1; }))
    return std::nullopt;
  plan.logicalWorkerSlice.assign(plan.blockShape.begin(),
                                 plan.blockShape.end());
  if (*classification == sde::SdeStructuredClassification::stencil)
    plan.haloShape =
        getStencilHaloRadiiForOwnerDims(op, plan.ownerPhysicalDims.size());
  plan.topology = plan.ownerPhysicalDims.size() > 1
                      ? sde::SdeIterationTopology::owner_tile
                      : sde::SdeIterationTopology::owner_strip;
  return plan;
}

static std::optional<PhysicalTileShape>
buildBudgetReconciledElementwiseTilePlan(sde::SdeSuIterateOp op,
                                         sde::SDECostModel &costModel) {
  if (!isBudgetReconciledTileCandidate(op) &&
      !hasBudgetReconciledLayoutCandidate(op))
    return std::nullopt;

  auto classification = sde::queryStructuredClassification(op);
  bool allowSingleOwnerDim = true;
  std::optional<sde::LayoutGraphFact> writeLayout =
      selectSingleBudgetWriteLayoutFact(op, allowSingleOwnerDim);
  if (!writeLayout)
    return std::nullopt;

  unsigned numDims = op.getLowerBounds().size();
  if (numDims < writeLayout->ownerDims.size() ||
      op.getSteps().size() != numDims)
    return std::nullopt;

  SmallVector<int64_t, 4> logicalShape;
  SmallVector<int64_t, 4> loopDimToPhysicalDim(numDims, -1);
  SmallVector<int64_t, 4> physicalDimToLoopDim;
  std::optional<sde::SuOutputLayoutFacts> outputPlan =
      sde::findCompatibleSuOutputLayoutFacts(op);
  if (outputPlan) {
    logicalShape.assign(outputPlan->shape.begin(), outputPlan->shape.end());
    loopDimToPhysicalDim.assign(outputPlan->loopDimToPhysicalDim.begin(),
                                outputPlan->loopDimToPhysicalDim.end());
    physicalDimToLoopDim.assign(outputPlan->physicalDimToLoopDim.begin(),
                                outputPlan->physicalDimToLoopDim.end());
  } else if (std::optional<SmallVector<int64_t, 4>> shape =
                 findBudgetWriteLogicalShape(op, *writeLayout)) {
    logicalShape.assign(shape->begin(), shape->end());
    physicalDimToLoopDim.assign(logicalShape.size(), -1);
    for (auto [loopDim, physicalDim] :
         llvm::enumerate(writeLayout->ownerDims)) {
      if (physicalDim < 0 ||
          static_cast<size_t>(physicalDim) >= physicalDimToLoopDim.size())
        return std::nullopt;
      loopDimToPhysicalDim[loopDim] = physicalDim;
      physicalDimToLoopDim[physicalDim] = loopDim;
    }
  }
  if (logicalShape.empty() ||
      logicalShape.size() != writeLayout->budgetBlockShape.size() ||
      physicalDimToLoopDim.size() != logicalShape.size())
    return std::nullopt;

  SmallVector<int64_t, 4> orderedOwnerPhysicalDims;
  orderedOwnerPhysicalDims.reserve(writeLayout->ownerDims.size());
  for (unsigned loopDim = 0; loopDim < numDims; ++loopDim) {
    if (loopDim >= loopDimToPhysicalDim.size())
      return std::nullopt;
    int64_t physicalDim = loopDimToPhysicalDim[loopDim];
    if (physicalDim < 0)
      continue;
    if (static_cast<size_t>(physicalDim) >= physicalDimToLoopDim.size())
      return std::nullopt;
    if (llvm::is_contained(writeLayout->ownerDims, physicalDim))
      orderedOwnerPhysicalDims.push_back(physicalDim);
  }
  if (orderedOwnerPhysicalDims.size() != writeLayout->ownerDims.size())
    return std::nullopt;
  if (!allExternalStoresCoverOwnerDims(op, orderedOwnerPhysicalDims,
                                       physicalDimToLoopDim))
    return std::nullopt;

  PhysicalTileShape plan;
  plan.logicalShape.assign(logicalShape.begin(), logicalShape.end());
  plan.ownerPhysicalDims.assign(orderedOwnerPhysicalDims.begin(),
                                orderedOwnerPhysicalDims.end());
  plan.blockShape.assign(writeLayout->budgetBlockShape.begin(),
                         writeLayout->budgetBlockShape.end());
  plan.alignSameOwnerReadLayouts = true;
  bool isStencil = classification &&
                   *classification == sde::SdeStructuredClassification::stencil;
  if (isStencil) {
    plan.haloShape =
        getStencilHaloRadiiForOwnerDims(op, plan.ownerPhysicalDims.size());
    SmallVector<int64_t, 4> ownerExtents;
    ownerExtents.reserve(plan.ownerPhysicalDims.size());
    for (int64_t physicalDim : plan.ownerPhysicalDims) {
      if (physicalDim < 0 ||
          static_cast<size_t>(physicalDim) >= logicalShape.size())
        return std::nullopt;
      ownerExtents.push_back(logicalShape[physicalDim]);
    }
    SmallVector<int64_t, 4> workerGrid = sde::factorStencilWorkersAcrossDims(
        std::max<int64_t>(1, getTargetTileTasks(op, costModel)), ownerExtents,
        plan.haloShape);
    if (workerGrid.size() != plan.ownerPhysicalDims.size())
      return std::nullopt;
    for (auto [slot, physicalDim] : llvm::enumerate(plan.ownerPhysicalDims)) {
      int64_t balancedBlock =
          sde::ceilDivPositive(logicalShape[physicalDim], workerGrid[slot]);
      plan.blockShape[physicalDim] = balancedBlock;
    }
    if (llvm::any_of(plan.blockShape,
                     [](int64_t extent) { return extent <= 0; }))
      return std::nullopt;
  } else if (!sde::enforceOwnerBlockConcurrencyFloor(
                 logicalShape, plan.ownerPhysicalDims,
                 getTargetTileTasks(op, costModel), plan.blockShape)) {
    return std::nullopt;
  }
  plan.tileIterations.assign(numDims, 1);

  for (unsigned loopDim = 0; loopDim < numDims; ++loopDim) {
    if (loopDim >= loopDimToPhysicalDim.size())
      return std::nullopt;
    int64_t physicalDim = loopDimToPhysicalDim[loopDim];
    if (physicalDim < 0)
      continue;
    if (static_cast<size_t>(physicalDim) >= plan.blockShape.size())
      return std::nullopt;
    int64_t tile = plan.blockShape[physicalDim];
    if (tile <= 0)
      return std::nullopt;
    std::optional<int64_t> step =
        ValueAnalysis::getPositiveConstantIndex(op.getSteps()[loopDim]);
    if (!step || *step != 1)
      return std::nullopt;
    plan.tileIterations[loopDim] = tile;
  }

  if (llvm::all_of(plan.tileIterations, [](int64_t tile) { return tile <= 1; }))
    return std::nullopt;

  if (isStencil) {
    plan.logicalWorkerSlice.assign(plan.blockShape.begin(),
                                   plan.blockShape.end());
  } else if (!sde::buildBlockAlignedLogicalWorkerSlice(
                 logicalShape, plan.ownerPhysicalDims, plan.blockShape,
                 costModel.getLogicalWorkerCapacity(),
                 plan.logicalWorkerSlice)) {
    plan.logicalWorkerSlice.assign(plan.blockShape.begin(),
                                   plan.blockShape.end());
  }

  plan.topology = plan.ownerPhysicalDims.size() > 1
                      ? sde::SdeIterationTopology::owner_tile
                      : sde::SdeIterationTopology::owner_strip;
  return plan;
}

static std::optional<SmallVector<int64_t, 4>>
alignStaticShapeAttrToSteps(sde::SdeSuIterateOp op, ArrayAttr attr,
                            ArrayRef<Value> tiledSteps,
                            ArrayRef<bool> parallelMask) {
  std::optional<SmallVector<int64_t, 4>> shape = readI64ArrayAttr(attr);
  if (!shape)
    return std::nullopt;

  for (unsigned dim = 0, e = std::min(tiledSteps.size(),
                                      static_cast<size_t>(parallelMask.size()));
       dim < e; ++dim) {
    if (!parallelMask[dim])
      continue;
    std::optional<int64_t> step =
        ValueAnalysis::getPositiveConstantIndex(tiledSteps[dim]);
    if (!step || *step <= 1)
      continue;
    std::optional<unsigned> physicalDim = mapLoopDimToPhysicalDim(op, dim);
    if (!physicalDim || *physicalDim >= shape->size())
      continue;
    int64_t &extent = (*shape)[*physicalDim];
    if (extent <= 0)
      return std::nullopt;
    int64_t chunks = (extent + *step - 1) / *step;
    if (chunks > std::numeric_limits<int64_t>::max() / *step)
      return std::nullopt;
    extent = chunks * *step;
  }

  return shape;
}

static void
alignExistingStaticPhysicalPlanToSteps(sde::SdeSuIterateOp op,
                                       ArrayRef<Value> tiledSteps,
                                       ArrayRef<bool> parallelMask) {
  if (std::optional<sde::LayoutGraphFact> budgetLayout =
          selectSingleBudgetWriteLayoutFact(op, /*allowSingleOwnerDim=*/true)) {
    SmallVector<int64_t, 4> blockShape(budgetLayout->blockShape.begin(),
                                       budgetLayout->blockShape.end());
    bool changed = false;
    for (unsigned dim = 0,
                  e = std::min(tiledSteps.size(),
                               static_cast<size_t>(parallelMask.size()));
         dim < e; ++dim) {
      if (!parallelMask[dim])
        continue;
      if (dim >= budgetLayout->ownerDims.size())
        continue;
      int64_t physicalDim = budgetLayout->ownerDims[dim];
      if (physicalDim < 0 ||
          static_cast<size_t>(physicalDim) >= blockShape.size())
        return;
      std::optional<int64_t> step =
          ValueAnalysis::getPositiveConstantIndex(tiledSteps[dim]);
      if (!step || *step <= 1)
        continue;
      blockShape[physicalDim] = *step;
      changed = true;
    }
    if (changed) {
      sde::commitWriterPhysicalLayoutFacts(op, budgetLayout->ownerDims,
                                           blockShape, blockShape);
      if (std::optional<SmallVector<int64_t, 4>> logicalShape =
              findBudgetWriteLogicalShape(op, *budgetLayout))
        (void)sde::rewriteSameOwnerReadLayoutsToPhysicalShape(
            op, budgetLayout->ownerDims, blockShape, *logicalShape);
      return;
    }
  }

  std::optional<sde::LayoutGraphFact> writeLayout =
      sde::findSingleCommittedWriterBlockLayout(op);
  if (!writeLayout)
    return;

  SmallVector<int64_t, 4> blockShape(writeLayout->blockShape.begin(),
                                     writeLayout->blockShape.end());
  for (unsigned dim = 0, e = std::min(tiledSteps.size(),
                                      static_cast<size_t>(parallelMask.size()));
       dim < e; ++dim) {
    if (!parallelMask[dim])
      continue;
    std::optional<int64_t> step =
        ValueAnalysis::getPositiveConstantIndex(tiledSteps[dim]);
    if (!step || *step <= 1)
      continue;
    std::optional<unsigned> physicalDim = mapLoopDimToPhysicalDim(op, dim);
    if (!physicalDim || *physicalDim >= blockShape.size())
      continue;
    int64_t &extent = blockShape[*physicalDim];
    if (extent <= 0)
      return;
    int64_t chunks = (extent + *step - 1) / *step;
    if (chunks > std::numeric_limits<int64_t>::max() / *step)
      return;
    extent = chunks * *step;
  }

  sde::commitWriterPhysicalLayoutFacts(op, writeLayout->ownerDims, blockShape);

  sde::SdeCuRegionOp cu = sde::findSuComputeCuRegion(op);
  if (!cu || !cu.getGroupBlockCountAttr())
    return;

  SmallVector<int64_t, 4> workerSlice(blockShape.begin(), blockShape.end());
  if (auto groupCounts = readI64ArrayAttr(cu.getGroupBlockCountAttr())) {
    for (auto [idx, rawDim] : llvm::enumerate(writeLayout->ownerDims)) {
      if (idx >= groupCounts->size() || rawDim < 0 ||
          static_cast<size_t>(rawDim) >= workerSlice.size())
        return;
      workerSlice[rawDim] = blockShape[rawDim] * (*groupCounts)[idx];
    }
  }

  if (std::optional<SmallVector<int64_t, 4>> alignedSlice =
          alignStaticShapeAttrToSteps(
              op, buildI64ArrayAttr(op.getContext(), workerSlice), tiledSteps,
              parallelMask))
    sde::commitCuGroupBlockCounts(cu, writeLayout->ownerDims, blockShape,
                                  *alignedSlice);
}

static bool isTilingCandidate(sde::SdeSuIterateOp op, Block &body) {
  if (op->getParentOfType<sde::SdeSuIterateOp>())
    return false;
  if (op.getLowerBounds().empty())
    return false;
  auto classification = sde::queryStructuredClassification(op);
  if (!classification)
    return false;

  switch (*classification) {
  case sde::SdeStructuredClassification::stencil:
    return op.getReductionAccumulators().size() == 0 &&
           isStencilCandidate(op, body);
  case sde::SdeStructuredClassification::elementwise:
    if (op.getReductionAccumulators().size() != 0)
      return false;
    return isExecutableInnermostBody(body) ||
           hasPerfectNestedScalarLoopNest(body, /*numLoops=*/2) ||
           hasPerfectNestedAffineLoopNest(body, /*numLoops=*/2);
  case sde::SdeStructuredClassification::elementwise_pipeline:
    if (op.getReductionAccumulators().size() != 0)
      return false;
    return isExecutableInnermostBody(body) ||
           hasPerfectNestedScalarLoopNest(body, /*numLoops=*/2) ||
           hasPerfectNestedAffineLoopNest(body, /*numLoops=*/2) ||
           sde::isOwnerLocalPipelineReduction(op);
  case sde::SdeStructuredClassification::matmul:
    if (op.getReductionAccumulators().size() != 0)
      return false;
    return isDirectMemoryMatmulCandidate(op, body) ||
           hasPromotedParallelOutputSchedule(op);
  case sde::SdeStructuredClassification::reduction:
    if (op.getReductionAccumulators().size() != 0)
      return false;
    if (!sde::isOwnerLocalPipelineReduction(op))
      return hasPromotedParallelOutputSchedule(op);
    return isExecutableInnermostBody(body) ||
           hasPerfectNestedScalarLoopNest(body, /*numLoops=*/2) ||
           hasPerfectNestedAffineLoopNest(body, /*numLoops=*/2);
  }
  return false;
}

static void cloneBodyIntoTileLoop(Block &srcBody, IRMapping &mapper,
                                  OpBuilder &builder) {
  for (Operation &op : srcBody.without_terminator()) {
    builder.clone(op, mapper);
  }
}

static bool stripMineLoop(scf::ForOp loop, Value tileIterations) {
  if (!loop || !tileIterations || loop.getNumResults() != 0 ||
      !loop.getInitArgs().empty())
    return false;

  int64_t tileConstant = 0;
  if (::mlir::carts::ValueAnalysis::getConstantIndex(tileIterations,
                                                     tileConstant) &&
      tileConstant <= 1)
    return false;

  return !mlir::tilePerfectlyNested(loop, {tileIterations}).empty();
}

static bool stripMineAffineLoop(affine::AffineForOp loop, int64_t tileSize) {
  if (!loop || tileSize <= 1 || loop.getNumResults() != 0)
    return false;

  SmallVector<affine::AffineForOp, 1> band = {loop};
  if (failed(
          affine::tilePerfectlyNested(band, {static_cast<unsigned>(tileSize)})))
    return false;
  return true;
}

static bool isKnownFloatZero(Value value) {
  if (!value || !isa<FloatType>(value.getType()))
    return false;
  if (ValueAnalysis::isZeroConstant(value))
    return true;
  if (auto mul = value.getDefiningOp<arith::MulFOp>())
    return isKnownFloatZero(mul.getLhs()) || isKnownFloatZero(mul.getRhs());
  return false;
}

static bool matchAccumulatorContribution(scf::ForOp reductionLoop,
                                         Value &contribution,
                                         Operation *&accumulateOp) {
  if (!reductionLoop || reductionLoop.getNumResults() != 1 ||
      reductionLoop.getInitArgs().size() != 1)
    return false;

  auto yield = dyn_cast<scf::YieldOp>(reductionLoop.getBody()->back());
  if (!yield || yield.getResults().size() != 1)
    return false;

  auto add = yield.getResults()[0].getDefiningOp<arith::AddFOp>();
  if (!add)
    return false;

  Value accumulator = reductionLoop.getRegionIterArgs()[0];
  if (add.getLhs() == accumulator)
    contribution = add.getRhs();
  else if (add.getRhs() == accumulator)
    contribution = add.getLhs();
  else
    return false;

  accumulateOp = add.getOperation();
  return true;
}

static bool splitSumTerm(Value value, Value sum, Value &scale) {
  if (value == sum)
    return true;

  auto mul = value.getDefiningOp<arith::MulFOp>();
  if (!mul)
    return false;

  if (mul.getLhs() == sum) {
    scale = mul.getRhs();
    return true;
  }
  if (mul.getRhs() == sum) {
    scale = mul.getLhs();
    return true;
  }
  return false;
}

static bool splitFinalStoreTerm(Value stored, Value sum, Value &initValue,
                                Value &sumScale) {
  if (splitSumTerm(stored, sum, sumScale))
    return true;

  auto add = stored.getDefiningOp<arith::AddFOp>();
  if (!add)
    return false;

  Value scale;
  if (splitSumTerm(add.getLhs(), sum, scale)) {
    initValue = add.getRhs();
    sumScale = scale;
    return true;
  }
  if (splitSumTerm(add.getRhs(), sum, scale)) {
    initValue = add.getLhs();
    sumScale = scale;
    return true;
  }
  return false;
}

static Value cloneInitValue(OpBuilder &builder, Value value, IRMapping &mapping,
                            Value forbidden, Operation *cloneScope) {
  if (!value)
    return {};
  if (Value mapped = mapping.lookupOrNull(value))
    return mapped;
  if (value == forbidden || ValueAnalysis::dependsOn(value, forbidden))
    return {};

  Operation *def = value.getDefiningOp();
  if (!def)
    return mapping.lookupOrDefault(value);
  if (!cloneScope || !cloneScope->isAncestor(def))
    return mapping.lookupOrDefault(value);
  if (def->getNumRegions() != 0 || isa<memref::StoreOp>(def))
    return {};
  if (!isa<memref::LoadOp>(def) && !isMemoryEffectFree(def))
    return {};

  SmallVector<Value, 4> operands;
  operands.reserve(def->getNumOperands());
  for (Value operand : def->getOperands()) {
    Value mappedOperand =
        cloneInitValue(builder, operand, mapping, forbidden, cloneScope);
    if (!mappedOperand)
      return {};
    operands.push_back(mappedOperand);
  }

  OperationState state(def->getLoc(), def->getName().getStringRef());
  state.addOperands(operands);
  state.addTypes(def->getResultTypes());
  state.addAttributes(def->getAttrs());
  Operation *cloned = builder.create(state);
  for (auto [oldResult, newResult] :
       llvm::zip(def->getResults(), cloned->getResults()))
    mapping.map(oldResult, newResult);
  return mapping.lookupOrNull(value);
}

static bool matchFinalStoreWithReductionContribution(
    scf::ForOp reductionLoop, ArrayRef<Operation *> postOps,
    memref::StoreOp &store, Value &initValue, Value &sumScale) {
  if (!isKnownFloatZero(reductionLoop.getInitArgs()[0]) || postOps.empty())
    return false;

  store = dyn_cast<memref::StoreOp>(postOps.back());
  if (!store)
    return false;

  auto type = dyn_cast<MemRefType>(store.getMemRefType());
  if (!type || type.getRank() < 2 || store.getIndices().size() < 2)
    return false;

  Value sum = reductionLoop.getResult(0);
  Value stored = store.getValueToStore();
  if (!splitFinalStoreTerm(stored, sum, initValue, sumScale))
    return false;
  return !initValue || !ValueAnalysis::dependsOn(initValue, sum);
}

static bool loopBodyReadsMemref(scf::ForOp loop, Value memref) {
  bool readsMemref = false;
  loop.walk([&](memref::LoadOp load) {
    if (load.getMemref() == memref)
      readsMemref = true;
  });
  return readsMemref;
}

static bool hasUnsupportedStoreInLoop(scf::ForOp loop) {
  bool unsupported = false;
  loop.walk([&](memref::StoreOp) { unsupported = true; });
  return unsupported;
}

static bool outputStoresAreColumnLocal(scf::ForOp reductionLoop,
                                       Value outputRoot, Value reductionIv) {
  if (!reductionLoop || !outputRoot || !reductionIv)
    return false;

  bool sawStore = false;
  bool rejected = false;
  reductionLoop.walk([&](memref::StoreOp store) {
    if (rejected)
      return;
    Value root =
        ::mlir::carts::ValueAnalysis::stripMemrefViewOps(store.getMemref());
    if (root != outputRoot) {
      rejected = true;
      return;
    }
    for (Value index : store.getIndices()) {
      if (::mlir::carts::ValueAnalysis::dependsOn(index, reductionIv)) {
        rejected = true;
        return;
      }
    }
    sawStore = true;
  });
  return sawStore && !rejected;
}

static bool
collectPromotedMatmulColumnBody(scf::ForOp columnLoop,
                                scf::ForOp &reductionLoop,
                                SmallVectorImpl<Operation *> &postOps) {
  if (!columnLoop || columnLoop.getNumResults() != 0 ||
      !columnLoop.getInitArgs().empty())
    return false;

  bool sawReductionLoop = false;
  for (Operation &op : columnLoop.getBody()->without_terminator()) {
    if (auto forOp = dyn_cast<scf::ForOp>(op)) {
      if (sawReductionLoop)
        return false;
      reductionLoop = forOp;
      sawReductionLoop = true;
      continue;
    }

    if (!sawReductionLoop) {
      if (isa<memref::AllocaOp>(op))
        continue;
      if (auto store = dyn_cast<memref::StoreOp>(op)) {
        auto type = dyn_cast<MemRefType>(store.getMemRefType());
        if (type && type.getRank() == 0)
          continue;
      }
      return false;
    }

    postOps.push_back(&op);
  }

  return reductionLoop && reductionLoop.getNumResults() == 1 &&
         reductionLoop.getInitArgs().size() == 1;
}

static bool collectPromotedMatmulInPlaceColumnBody(
    scf::ForOp columnLoop, SmallVectorImpl<Operation *> &initOps,
    memref::StoreOp &initStore, scf::ForOp &reductionLoop) {
  if (!columnLoop || columnLoop.getNumResults() != 0 ||
      !columnLoop.getInitArgs().empty())
    return false;

  bool sawReduction = false;
  unsigned initStoreCount = 0;
  for (Operation &op : columnLoop.getBody()->without_terminator()) {
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (sawReduction)
        return false;
      initStore = store;
      ++initStoreCount;
      initOps.push_back(&op);
      continue;
    }

    if (auto forOp = dyn_cast<scf::ForOp>(op)) {
      if (!initStore || sawReduction || forOp.getNumResults() != 0 ||
          !forOp.getInitArgs().empty())
        return false;
      reductionLoop = forOp;
      sawReduction = true;
      continue;
    }

    if (sawReduction || op.getNumRegions() != 0)
      return false;
    if (!isa<memref::LoadOp>(op) && !isMemoryEffectFree(&op))
      return false;
    initOps.push_back(&op);
  }

  if (!initStore || initStoreCount != 1 || !reductionLoop || initOps.empty() ||
      initOps.back() != initStore.getOperation())
    return false;
  Value outputRoot =
      ::mlir::carts::ValueAnalysis::stripMemrefViewOps(initStore.getMemref());
  return outputStoresAreColumnLocal(reductionLoop, outputRoot,
                                    reductionLoop.getInductionVar());
}

/// Rewrite a promoted 2-D matmul tile from row/column/scalar-k dots to
/// row/k/column update order. This preserves the committed owner tile but makes
/// the B and C row accesses stride-1 before ARTS consumes the SDE facts.
static bool rewritePromotedMatmulTileUpdate(scf::ForOp rowLoop,
                                            scf::ForOp columnLoop) {
  if (!rowLoop || !columnLoop ||
      columnLoop->getParentOp() != rowLoop.getOperation() ||
      rowLoop.getNumResults() != 0 || !rowLoop.getInitArgs().empty())
    return false;

  scf::ForOp reductionLoop;
  SmallVector<Operation *> postOps;
  if (!collectPromotedMatmulColumnBody(columnLoop, reductionLoop, postOps))
    return false;

  Value contribution;
  Operation *accumulateOp = nullptr;
  if (!matchAccumulatorContribution(reductionLoop, contribution, accumulateOp))
    return false;

  memref::StoreOp finalStore;
  Value initValue;
  Value sumScale;
  if (!matchFinalStoreWithReductionContribution(
          reductionLoop, postOps, finalStore, initValue, sumScale))
    return false;

  if (loopBodyReadsMemref(reductionLoop, finalStore.getMemref()) ||
      hasUnsupportedStoreInLoop(reductionLoop))
    return false;

  Value oldJ = columnLoop.getInductionVar();
  Value oldK = reductionLoop.getInductionVar();
  Value zero = reductionLoop.getInitArgs()[0];
  Location loc = columnLoop.getLoc();
  OpBuilder builder(columnLoop);

  scf::ForOp initLoop =
      scf::ForOp::create(builder, loc, columnLoop.getLowerBound(),
                         columnLoop.getUpperBound(), columnLoop.getStep());
  {
    OpBuilder::InsertionGuard initGuard(builder);
    builder.setInsertionPointToStart(initLoop.getBody());
    IRMapping mapping;
    mapping.map(oldJ, initLoop.getInductionVar());
    Value initial = zero;
    if (initValue) {
      initial =
          cloneInitValue(builder, initValue, mapping,
                         reductionLoop.getResult(0), columnLoop.getOperation());
      if (!initial) {
        initLoop.erase();
        return false;
      }
    }
    SmallVector<Value> indices;
    for (Value index : finalStore.getIndices())
      indices.push_back(mapping.lookupOrDefault(index));
    memref::StoreOp::create(builder, loc, initial, finalStore.getMemref(),
                            indices);
  }

  scf::ForOp::create(
      builder, loc, reductionLoop.getLowerBound(),
      reductionLoop.getUpperBound(), reductionLoop.getStep(), ValueRange{},
      [&](OpBuilder &kBuilder, Location kLoc, Value newK, ValueRange) {
        scf::ForOp::create(
            kBuilder, kLoc, columnLoop.getLowerBound(),
            columnLoop.getUpperBound(), columnLoop.getStep(), ValueRange{},
            [&](OpBuilder &jBuilder, Location jLoc, Value newJ, ValueRange) {
              IRMapping mapping;
              mapping.map(oldK, newK);
              mapping.map(oldJ, newJ);

              for (Operation &op :
                   reductionLoop.getBody()->without_terminator()) {
                if (&op == accumulateOp)
                  continue;
                Operation *cloned = jBuilder.clone(op, mapping);
                for (auto [oldResult, newResult] :
                     llvm::zip(op.getResults(), cloned->getResults()))
                  mapping.map(oldResult, newResult);
              }

              SmallVector<Value> indices;
              for (Value index : finalStore.getIndices())
                indices.push_back(mapping.lookupOrDefault(index));

              Value oldValue = memref::LoadOp::create(
                  jBuilder, jLoc, finalStore.getMemref(), indices);
              Value mappedContribution = mapping.lookupOrDefault(contribution);
              if (sumScale) {
                Value mappedScale = mapping.lookupOrDefault(sumScale);
                mappedContribution = arith::MulFOp::create(
                    jBuilder, jLoc, mappedContribution, mappedScale);
              }
              Value updated = arith::AddFOp::create(jBuilder, jLoc, oldValue,
                                                    mappedContribution);
              memref::StoreOp::create(jBuilder, jLoc, updated,
                                      finalStore.getMemref(), indices);
              scf::YieldOp::create(jBuilder, jLoc);
            });
        scf::YieldOp::create(kBuilder, kLoc);
      });

  columnLoop.erase();
  return true;
}

static bool rewritePromotedMatmulInPlaceTileUpdate(scf::ForOp rowLoop,
                                                   scf::ForOp columnLoop) {
  if (!rowLoop || !columnLoop ||
      columnLoop->getParentOp() != rowLoop.getOperation() ||
      rowLoop.getNumResults() != 0 || !rowLoop.getInitArgs().empty())
    return false;

  memref::StoreOp initStore;
  scf::ForOp reductionLoop;
  SmallVector<Operation *> initOps;
  if (!collectPromotedMatmulInPlaceColumnBody(columnLoop, initOps, initStore,
                                              reductionLoop))
    return false;

  Value oldJ = columnLoop.getInductionVar();
  Value oldK = reductionLoop.getInductionVar();
  Location loc = columnLoop.getLoc();
  OpBuilder builder(columnLoop);

  scf::ForOp initLoop =
      scf::ForOp::create(builder, loc, columnLoop.getLowerBound(),
                         columnLoop.getUpperBound(), columnLoop.getStep());
  {
    OpBuilder::InsertionGuard initGuard(builder);
    builder.setInsertionPointToStart(initLoop.getBody());
    IRMapping mapping;
    mapping.map(oldJ, initLoop.getInductionVar());
    for (Operation *op : initOps) {
      Operation *cloned = builder.clone(*op, mapping);
      for (auto [oldResult, newResult] :
           llvm::zip(op->getResults(), cloned->getResults()))
        mapping.map(oldResult, newResult);
    }
  }

  scf::ForOp::create(
      builder, loc, reductionLoop.getLowerBound(),
      reductionLoop.getUpperBound(), reductionLoop.getStep(), ValueRange{},
      [&](OpBuilder &kBuilder, Location kLoc, Value newK, ValueRange) {
        scf::ForOp::create(
            kBuilder, kLoc, columnLoop.getLowerBound(),
            columnLoop.getUpperBound(), columnLoop.getStep(), ValueRange{},
            [&](OpBuilder &jBuilder, Location jLoc, Value newJ, ValueRange) {
              IRMapping mapping;
              mapping.map(oldK, newK);
              mapping.map(oldJ, newJ);
              for (Operation &op :
                   reductionLoop.getBody()->without_terminator()) {
                Operation *cloned = jBuilder.clone(op, mapping);
                for (auto [oldResult, newResult] :
                     llvm::zip(op.getResults(), cloned->getResults()))
                  mapping.map(oldResult, newResult);
              }
              scf::YieldOp::create(jBuilder, jLoc);
            });
        scf::YieldOp::create(kBuilder, kLoc);
      });

  columnLoop.erase();
  return true;
}

static Value buildAlignedTileLowerBound(OpBuilder &builder, Location loc,
                                        Value lowerBound, Value tileStep) {
  if (!lowerBound || !tileStep)
    return lowerBound;

  int64_t lb = 0;
  int64_t step = 0;
  if (::mlir::carts::ValueAnalysis::getConstantIndex(lowerBound, lb) &&
      ::mlir::carts::ValueAnalysis::getConstantIndex(tileStep, step) &&
      step > 0) {
    int64_t aligned =
        lb >= 0 ? (lb / step) * step : -llvm::divideCeil(-lb, step) * step;
    return createConstantIndex(builder, loc, aligned);
  }

  return lowerBound;
}

static bool shouldAlignOuterTileGrid(bool alignTileGrid,
                                     ArrayRef<bool> parallelMask,
                                     unsigned dim) {
  return alignTileGrid && dim < parallelMask.size() && parallelMask[dim];
}

static unsigned stripMineDirectMatmulColumnLoops(Block &body, Value outputRoot,
                                                 Value ownerIv,
                                                 Value columnTileIterations) {
  int64_t tileConstant = 0;
  const bool hasConstantTile = ::mlir::carts::ValueAnalysis::getConstantIndex(
      columnTileIterations, tileConstant);
  if (hasConstantTile && tileConstant <= 1)
    return 0;

  unsigned tiled = 0;
  SmallVector<scf::ForOp, 4> columnLoops;
  collectDirectMatmulColumnLoops(body, outputRoot, ownerIv, columnLoops);

  for (scf::ForOp loop : llvm::reverse(columnLoops)) {
    if (!loop || loop->getParentRegion() == nullptr)
      continue;
    if (stripMineLoop(loop, columnTileIterations))
      ++tiled;
  }

  if (!hasConstantTile)
    return tiled;

  SmallVector<affine::AffineForOp, 4> affineColumnLoops;
  collectDirectMatmulColumnAffineLoops(body, outputRoot, ownerIv,
                                       affineColumnLoops);
  for (affine::AffineForOp loop : llvm::reverse(affineColumnLoops)) {
    if (!loop || loop->getParentRegion() == nullptr)
      continue;
    if (stripMineAffineLoop(loop, tileConstant))
      ++tiled;
  }
  return tiled;
}

static void commitDirectMatmulTileShape(sde::SdeSuIterateOp op,
                                        const DirectMatmulTileShape &plan) {
  SmallVector<int64_t, 4> blockShape = plan.output.shape;
  if (blockShape.empty())
    return;
  blockShape[0] = plan.rowTile;

  // Direct-memory matmul keeps full output rows in one owner task. Splitting
  // columns across owner tasks duplicates the k-sweep against coarse inputs.
  sde::commitWriterPhysicalLayoutFacts(op, SmallVector<int64_t, 1>{0},
                                       blockShape, blockShape);
}

struct TilingPass : public sde::impl::TilingBase<TilingPass> {
  explicit TilingPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    if (!costModel)
      return;

    SmallVector<sde::SdeSuIterateOp> rewrites;
    getOperation().walk([&](sde::SdeSuIterateOp op) {
      if (hasCommittedTilingGrainFacts(op))
        return;
      Block *body = sde::getSuIterateComputeBlock(op);
      bool structuralBudgetCandidate = hasBudgetReconciledLayoutCandidate(op);
      if ((!body || !isTilingCandidate(op, *body)) &&
          !structuralBudgetCandidate)
        return;

      std::optional<int64_t> tripCount = getStaticTripCount(op.getOperation());
      if (tripCount && *tripCount <= 1)
        return;
      rewrites.push_back(op);
    });

    for (sde::SdeSuIterateOp op : rewrites) {
      PatternRewriter rewriter(op.getContext());
      rewriter.setInsertionPoint(op);
      Location loc = op.getLoc();
      unsigned numDims = op.getLowerBounds().size();
      bool directMatmul = false;
      bool promotedMatmul = false;
      std::optional<DirectMatmulTileShape> directMatmulShape;
      if (auto cls = sde::queryStructuredClassification(op);
          cls && *cls == sde::SdeStructuredClassification::matmul) {
        directMatmulShape =
            buildDirectMatmulTileShape(rewriter, loc, op, *costModel);
        if (directMatmulShape) {
          directMatmul = true;
        } else if (!hasPromotedParallelOutputSchedule(op)) {
          continue;
        } else {
          promotedMatmul = true;
        }
      }

      // Determine which dims are parallel (should tile) vs reduction (skip).
      SmallVector<bool> parallelMask = getParallelDimMask(op);

      std::optional<PhysicalTileShape> physicalTileShape;
      if (!directMatmul) {
        physicalTileShape = buildCoiteratedReadWriterTilePlan(op);
        if (!physicalTileShape) {
          physicalTileShape =
              buildPromotedMatmulPhysicalTileShape(op, *costModel);
        }
        if (!physicalTileShape)
          physicalTileShape =
              buildBudgetReconciledElementwiseTilePlan(op, *costModel);
      }

      // Compute per-dim tile iterations.
      SmallVector<Value> perDimTileIter;
      if (directMatmul) {
        perDimTileIter.push_back(directMatmulShape->rowTileValue);
      } else if (physicalTileShape) {
        for (int64_t tile : physicalTileShape->tileIterations)
          perDimTileIter.push_back(createConstantIndex(rewriter, loc, tile));
      } else if (numDims == 1) {
        // 1-D fast path: preserves existing static trip count optimization.
        if (!parallelMask[0]) {
          // Single reduction dim -- nothing to tile.
          continue;
        }
        Value tileIterations;
        if (std::optional<int64_t> tripCount =
                getStaticTripCount(op.getOperation())) {
          int64_t targetTasks = getTargetTileTasks(op, *costModel);
          int64_t minIterations = relaxOwnerLocalPipelineMinIterations(
              op, *tripCount, targetTasks,
              getMinTileIterations(op, *costModel));
          int64_t balancedTile = llvm::divideCeil(*tripCount, targetTasks);
          int64_t tileCount =
              std::clamp(std::max<int64_t>(balancedTile, minIterations),
                         int64_t{1}, *tripCount);
          if (tileCount <= 1)
            continue;
          tileIterations = createConstantIndex(rewriter, loc, tileCount);
        } else {
          tileIterations =
              buildTileIterationValue(rewriter, loc, op, *costModel);
        }
        if (!tileIterations)
          continue;
        perDimTileIter.push_back(tileIterations);
      } else {
        // N-dim path: distribute workers across dimensions.
        perDimTileIter =
            buildPerDimTileIterations(rewriter, loc, op, *costModel);
        if (perDimTileIter.empty())
          continue;
      }

      if (!physicalTileShape)
        physicalTileShape = buildNdStencilPhysicalTileShape(op, *costModel);
      if (physicalTileShape) {
        perDimTileIter.clear();
        for (int64_t tile : physicalTileShape->tileIterations)
          perDimTileIter.push_back(createConstantIndex(rewriter, loc, tile));
      }

      if (!physicalTileShape && !directMatmul &&
          sde::isOwnerLocalPipelineReduction(op)) {
        if (auto staticTileIterations =
                computeStaticTileIterations(op, *costModel))
          physicalTileShape =
              buildStencilPhysicalTileShape(op, *staticTileIterations);
      }

      // For stencils, enforce halo-aware minimum tile size per dimension.
      if (!physicalTileShape) {
        if (auto cls = sde::queryStructuredClassification(op);
            cls && *cls == sde::SdeStructuredClassification::stencil) {
          SmallVector<int64_t> halos = getStencilHaloWidths(op);
          for (unsigned d = 0; d < numDims && d < halos.size(); ++d) {
            Value haloVal = createConstantIndex(rewriter, loc, halos[d]);
            perDimTileIter[d] = arith::MaxUIOp::create(
                rewriter, loc, perDimTileIter[d], haloVal);
          }
          // The former L2-cache tile cap (on a fabricated getL2CacheSize()
          // literal) is dropped; only the halo floor above is enforced.
        }
      }

      // Validate steps and compute tiled steps per dim.
      // For reduction dims, keep the original step (no tiling).
      SmallVector<Value> tiledSteps;
      bool badStep = false;
      bool anyTiled = false;
      for (unsigned d = 0; d < numDims; ++d) {
        Value originalStep = op.getSteps()[d];
        int64_t constantStep = 0;
        if (::mlir::carts::ValueAnalysis::getConstantIndex(originalStep,
                                                           constantStep) &&
            constantStep <= 0) {
          badStep = true;
          break;
        }
        if (parallelMask[d]) {
          if (constantStep == 1)
            tiledSteps.push_back(perDimTileIter[d]);
          else
            tiledSteps.push_back(arith::MulIOp::create(
                rewriter, loc, originalStep, perDimTileIter[d]));
          anyTiled = true;
        } else {
          tiledSteps.push_back(originalStep);
        }
      }
      if (badStep || !anyTiled)
        continue;

      Block &srcBody = op.getBody().front();
      Block *computeBody = sde::getSuIterateComputeBlock(op);
      auto oldCuRegion =
          computeBody
              ? dyn_cast_or_null<sde::SdeCuRegionOp>(computeBody->getParentOp())
              : sde::SdeCuRegionOp();
      if (oldCuRegion && (!oldCuRegion.getIterArgs().empty() ||
                          oldCuRegion.getNumResults() != 0))
        continue;

      if (!physicalTileShape && !directMatmul &&
          op.getStructuredClassification() ==
              sde::SdeStructuredClassification::stencil) {
        if (auto staticTileIterations =
                computeStaticTileIterations(op, *costModel)) {
          applyStencilTileGuardsToStaticPlan(op, *staticTileIterations,
                                             numDims);
          physicalTileShape =
              buildStencilPhysicalTileShape(op, *staticTileIterations);
        }
      }

      SmallVector<Value> outerLowerBounds;
      outerLowerBounds.reserve(numDims);
      bool alignTileGrid = physicalTileShape.has_value() || directMatmul;
      for (unsigned d = 0; d < numDims; ++d) {
        Value lower = op.getLowerBounds()[d];
        if (shouldAlignOuterTileGrid(alignTileGrid, parallelMask, d))
          lower =
              buildAlignedTileLowerBound(rewriter, loc, lower, tiledSteps[d]);
        outerLowerBounds.push_back(lower);
      }

      auto newOp = sde::buildSuIterate(
          rewriter, loc, outerLowerBounds, op.getUpperBounds(),
          ValueRange{tiledSteps}, sde::SuIterateAttrs::fromOp(op),
          op.getReductionAccumulators());
      newOp->setAttrs(sde::getRewrittenAttrs(op));

      Block &newBody = sde::ensureBlock(newOp.getBody());
      for (unsigned d = newBody.getNumArguments(); d < numDims; ++d)
        newBody.addArgument(rewriter.getIndexType(), loc);

      OpBuilder::InsertionGuard guard(rewriter);
      IRMapping mapper;
      rewriter.setInsertionPointToStart(&newBody);
      for (auto root : srcBody.getOps<sde::SdeArrayLayoutRootOp>()) {
        sde::SdeArrayLayoutRootOp::create(
            rewriter, root.getLoc(), mapper.lookupOrDefault(root.getRoot()),
            root.getModeAttr(), root.getArrayIdAttr());
      }

      auto newCuRegion = sde::buildCuRegion(
          rewriter, loc,
          oldCuRegion ? oldCuRegion.getKindAttr()
                      : sde::SdeCuKindAttr::get(rewriter.getContext(),
                                                sde::SdeCuKind::single),
          oldCuRegion ? oldCuRegion.getNowaitAttr() : nullptr,
          /*iterArgs=*/ValueRange{}, /*resultTypes=*/TypeRange{},
          oldCuRegion ? oldCuRegion.getSerialReasonAttr() : nullptr);
      Block &newCuBody = sde::ensureBlock(newCuRegion.getBody());
      rewriter.setInsertionPointToStart(&newCuBody);

      if (!physicalTileShape && !directMatmul)
        alignExistingStaticPhysicalPlanToSteps(newOp, tiledSteps, parallelMask);
      if (physicalTileShape)
        commitPhysicalTileShape(newOp, *physicalTileShape);

      SmallVector<scf::ForOp, 4> tileLoops;
      for (unsigned d = 0; d < numDims; ++d) {
        Value tileBase = newBody.getArgument(d);
        if (!parallelMask[d]) {
          mapper.map(srcBody.getArgument(d), tileBase);
          continue;
        }
        Value tileLimit =
            arith::AddIOp::create(rewriter, loc, tileBase, tiledSteps[d]);
        Value tileLower = tileBase;
        if (shouldAlignOuterTileGrid(alignTileGrid, parallelMask, d))
          tileLower = arith::MaxUIOp::create(rewriter, loc, tileBase,
                                             op.getLowerBounds()[d]);
        Value tileUpper = arith::MinUIOp::create(rewriter, loc, tileLimit,
                                                 op.getUpperBounds()[d]);
        Value originalStep = op.getSteps()[d];
        auto tileLoop = scf::ForOp::create(rewriter, loc, tileLower, tileUpper,
                                           originalStep);
        tileLoops.push_back(tileLoop);
        mapper.map(srcBody.getArgument(d), tileLoop.getInductionVar());
        rewriter.setInsertionPointToStart(tileLoop.getBody());
      }

      cloneBodyIntoTileLoop(*computeBody, mapper, rewriter);

      if (directMatmul && !tileLoops.empty()) {
        Value outputRoot =
            mapper.lookupOrDefault(directMatmulShape->output.root);
        Value ownerIv = mapper.lookupOrDefault(srcBody.getArgument(0));
        unsigned tiledColumns = stripMineDirectMatmulColumnLoops(
            *tileLoops.back().getBody(), outputRoot, ownerIv,
            directMatmulShape->columnTileValue);
        if (tiledColumns == 0) {
          rewriter.eraseOp(newOp);
          continue;
        }
        commitDirectMatmulTileShape(newOp, *directMatmulShape);
      }

      if (promotedMatmul && tileLoops.size() == 2) {
        if (!rewritePromotedMatmulTileUpdate(tileLoops[0], tileLoops[1]))
          (void)rewritePromotedMatmulInPlaceTileUpdate(tileLoops[0],
                                                       tileLoops[1]);
      }
      if (promotedMatmul && physicalTileShape) {
        newOp.setPartialReductionAttr(UnitAttr::get(newOp.getContext()));
        newOp.setPartialReductionDimsAttr(buildI64ArrayAttr(
            newOp.getContext(),
            SmallVector<int64_t, 1>{static_cast<int64_t>(numDims + 2)}));
        newOp.setPartialReductionOwnerDimsAttr(buildI64ArrayAttr(
            newOp.getContext(), physicalTileShape->ownerPhysicalDims));
      }

      rewriter.setInsertionPointToEnd(&newCuBody);
      sde::SdeYieldOp::create(rewriter, loc, ValueRange{});

      rewriter.setInsertionPointToEnd(&newBody);
      sde::SdeYieldOp::create(rewriter, loc, ValueRange{});

      rewriter.eraseOp(op);
    }
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass> createTilingPass(sde::SDECostModel *costModel) {
  return std::make_unique<TilingPass>(costModel);
}

} // namespace mlir::carts::sde
