///==========================================================================///
/// File: BarrierElimination.cpp
///
/// Remove independent SDE barriers between scheduling units. Required
/// timestep barriers are annotated as stage boundaries so boundary lowering
/// can consume the already-proven SDE facts.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_BARRIERELIMINATION
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

#include "carts/utils/Debug.h"
ARTS_DEBUG_SETUP(barrier_elimination);

using namespace mlir;
using namespace mlir::carts;

namespace {

/// Find the su_iterate op that an operation represents. An su_iterate can
/// appear directly or nested inside an su_distribute wrapper.
static sde::SdeSuIterateOp findSuIterate(Operation *op) {
  if (auto suIt = dyn_cast<sde::SdeSuIterateOp>(op))
    return suIt;
  if (auto dist = dyn_cast<sde::SdeSuDistributeOp>(op)) {
    sde::SdeSuIterateOp result;
    dist.getBody().walk([&](sde::SdeSuIterateOp suIt) { result = suIt; });
    return result;
  }
  sde::SdeSuIterateOp result;
  bool multiple = false;
  op->walk([&](sde::SdeSuIterateOp suIt) {
    if (result && result.getOperation() != suIt.getOperation()) {
      multiple = true;
      return WalkResult::interrupt();
    }
    result = suIt;
    return WalkResult::advance();
  });
  if (multiple)
    return {};
  return result;
}

/// Return true if the operation is an SDE scheduling-unit container
/// (su_iterate or su_distribute wrapping one).
static bool isSuContainer(Operation *op) {
  return static_cast<bool>(findSuIterate(op));
}

static void setBarrierReason(sde::SdeSuBarrierOp barrier,
                             sde::SdeBarrierReason reason) {
  barrier.setBarrierReasonAttr(
      sde::SdeBarrierReasonAttr::get(barrier.getContext(), reason));
}

static bool haveSameIterationShape(sde::SdeSuIterateOp lhs,
                                   sde::SdeSuIterateOp rhs) {
  return ::mlir::carts::ValueAnalysis::areValueRangesEquivalent(
             lhs.getLowerBounds(), rhs.getLowerBounds()) &&
         ::mlir::carts::ValueAnalysis::areValueRangesEquivalent(
             lhs.getUpperBounds(), rhs.getUpperBounds()) &&
         ::mlir::carts::ValueAnalysis::areValueRangesEquivalent(lhs.getSteps(),
                                                                rhs.getSteps());
}

static bool haveSameIterationBounds(sde::SdeSuIterateOp lhs,
                                    sde::SdeSuIterateOp rhs) {
  return lhs.getLowerBounds().size() == rhs.getLowerBounds().size() &&
         ::mlir::carts::ValueAnalysis::areValueRangesEquivalent(
             lhs.getLowerBounds(), rhs.getLowerBounds()) &&
         ::mlir::carts::ValueAnalysis::areValueRangesEquivalent(
             lhs.getUpperBounds(), rhs.getUpperBounds());
}

static bool isTiledMultipleOfStep(Value candidate, Value baseStep) {
  if (::mlir::carts::ValueAnalysis::areValuesEquivalent(candidate, baseStep))
    return true;

  auto mul = candidate.getDefiningOp<arith::MulIOp>();
  if (!mul)
    return false;

  auto isPositiveMultiplier = [](Value value) {
    if (std::optional<int64_t> folded =
            ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(value))
      return *folded >= 1;
    return ::mlir::carts::ValueAnalysis::isConstantAtLeastOne(value) ||
           ::mlir::carts::ValueAnalysis::isProvablyNonZero(value);
  };

  if (::mlir::carts::ValueAnalysis::areValuesEquivalent(mul.getLhs(), baseStep))
    return isPositiveMultiplier(mul.getRhs());
  if (::mlir::carts::ValueAnalysis::areValuesEquivalent(mul.getRhs(), baseStep))
    return isPositiveMultiplier(mul.getLhs());
  return false;
}

static bool haveEquivalentOrTiledSteps(sde::SdeSuIterateOp lhs,
                                       sde::SdeSuIterateOp rhs) {
  if (lhs.getSteps().size() != rhs.getSteps().size())
    return false;

  for (auto [lhsStep, rhsStep] : llvm::zip(lhs.getSteps(), rhs.getSteps())) {
    if (::mlir::carts::ValueAnalysis::areValuesEquivalent(lhsStep, rhsStep))
      continue;
    std::optional<int64_t> lhsFolded =
        ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(lhsStep);
    std::optional<int64_t> rhsFolded =
        ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(rhsStep);
    if (lhsFolded && rhsFolded && *lhsFolded == *rhsFolded)
      continue;
    if (isTiledMultipleOfStep(lhsStep, rhsStep) ||
        isTiledMultipleOfStep(rhsStep, lhsStep))
      continue;
    return false;
  }
  return true;
}

static bool isUniformRepeatableStage(sde::SdeSuIterateOp op) {
  if (!op || !op.getReductionAccumulators().empty())
    return false;

  if (auto family = op.getPattern()) {
    return *family == sde::SdePattern::uniform ||
           *family == sde::SdePattern::elementwise_pipeline;
  }

  auto classification = op.getStructuredClassification();
  return classification &&
         (*classification == sde::SdeStructuredClassification::elementwise ||
          *classification ==
              sde::SdeStructuredClassification::elementwise_pipeline);
}

static bool
isOutOfPlaceStencilStage(sde::SdeSuIterateOp op,
                         const sde::StructuredMemoryEffectSummary &effects) {
  if (!op || effects.hasUnknownEffects)
    return false;
  auto classification = op.getStructuredClassification();
  if (!classification ||
      *classification != sde::SdeStructuredClassification::stencil)
    return false;
  if (sde::hasInPlaceSelfRead(effects))
    return false;
  auto family = op.getPattern();
  return !family || *family == sde::SdePattern::stencil_tiling_nd ||
         *family == sde::SdePattern::alternating_buffer_stencil;
}

static bool isWavefrontFrontierStage(sde::SdeSuIterateOp op) {
  auto family = op.getPattern();
  return family && *family == sde::SdePattern::wavefront_2d;
}

static void stampRepeatedTimestepPlan(sde::SdeSuIterateOp op) {
  if (!op)
    return;
  if (!op.getRepetitionStructureAttr())
    op.setRepetitionStructureAttr(sde::SdeRepetitionStructureAttr::get(
        op.getContext(), sde::SdeRepetitionStructure::full_timestep));
  if (!op.getAsyncStrategyAttr())
    op.setAsyncStrategyAttr(sde::SdeAsyncStrategyAttr::get(
        op.getContext(), sde::SdeAsyncStrategy::advance_stage));
}

static bool
writesIntersectReads(const sde::StructuredMemoryEffectSummary &lhs,
                     const sde::StructuredMemoryEffectSummary &rhs) {
  for (Value written : lhs.writes)
    if (rhs.reads.contains(written))
      return true;
  return false;
}

static bool
hasAlternatingBufferExchange(const sde::StructuredMemoryEffectSummary &lhs,
                             const sde::StructuredMemoryEffectSummary &rhs) {
  return writesIntersectReads(lhs, rhs) && writesIntersectReads(rhs, lhs);
}

static bool sameI64ArrayAttr(ArrayAttr lhs, ArrayAttr rhs) {
  if (!lhs || !rhs || lhs.size() != rhs.size())
    return false;
  for (auto [lhsAttr, rhsAttr] : llvm::zip(lhs, rhs)) {
    auto lhsInt = dyn_cast<IntegerAttr>(lhsAttr);
    auto rhsInt = dyn_cast<IntegerAttr>(rhsAttr);
    if (!lhsInt || !rhsInt || lhsInt.getInt() != rhsInt.getInt())
      return false;
  }
  return true;
}

static bool haveSamePhysicalTimestepPlan(sde::SdeSuIterateOp predecessor,
                                         sde::SdeSuIterateOp successor) {
  return sameI64ArrayAttr(predecessor.getPhysicalOwnerDimsAttr(),
                          successor.getPhysicalOwnerDimsAttr()) &&
         sameI64ArrayAttr(predecessor.getPhysicalBlockShapeAttr(),
                          successor.getPhysicalBlockShapeAttr());
}

static bool hasPhysicalTimestepPlan(sde::SdeSuIterateOp op) {
  return op.getPhysicalOwnerDimsAttr() || op.getPhysicalBlockShapeAttr();
}

static bool haveCompatiblePhysicalTimestepPlan(sde::SdeSuIterateOp predecessor,
                                               sde::SdeSuIterateOp successor) {
  bool predPlanned = hasPhysicalTimestepPlan(predecessor);
  bool succPlanned = hasPhysicalTimestepPlan(successor);
  if (!predPlanned && !succPlanned)
    return true;
  if (predPlanned != succPlanned)
    return false;
  return haveSamePhysicalTimestepPlan(predecessor, successor);
}

static bool sameSdeIterationTopology(sde::SdeSuIterateOp lhs,
                                     sde::SdeSuIterateOp rhs) {
  auto lhsTopology = lhs.getIterationTopology();
  auto rhsTopology = rhs.getIterationTopology();
  return lhsTopology && rhsTopology && *lhsTopology == *rhsTopology;
}

static bool haveSdeApprovedTiledTimestepPlan(sde::SdeSuIterateOp lhs,
                                             sde::SdeSuIterateOp rhs) {
  return haveSameIterationBounds(lhs, rhs) &&
         haveSamePhysicalTimestepPlan(lhs, rhs) &&
         sameI64ArrayAttr(lhs.getLogicalWorkerSliceAttr(),
                          rhs.getLogicalWorkerSliceAttr()) &&
         sameSdeIterationTopology(lhs, rhs) &&
         haveEquivalentOrTiledSteps(lhs, rhs);
}

static bool isPipelineableStructuredClassification(sde::SdeSuIterateOp op) {
  if (!op || !op.getReductionAccumulators().empty())
    return false;
  auto classification = op.getStructuredClassification();
  if (!classification)
    return false;
  switch (*classification) {
  case sde::SdeStructuredClassification::elementwise:
  case sde::SdeStructuredClassification::elementwise_pipeline:
  case sde::SdeStructuredClassification::matmul:
    return true;
  case sde::SdeStructuredClassification::stencil:
  case sde::SdeStructuredClassification::reduction:
    return false;
  }
  return false;
}

static std::optional<SmallVector<unsigned, 4>>
getPhysicalOwnerDims(sde::SdeSuIterateOp op) {
  auto ownerDims =
      ::mlir::carts::readI64ArrayAttr(op.getPhysicalOwnerDimsAttr());
  if (!ownerDims || ownerDims->empty())
    return std::nullopt;

  SmallVector<unsigned, 4> result;
  result.reserve(ownerDims->size());
  for (int64_t dim : *ownerDims) {
    if (dim < 0)
      return std::nullopt;
    result.push_back(static_cast<unsigned>(dim));
  }
  return result;
}

static bool isTokenLocalPipelineTopology(sde::SdeSuIterateOp op) {
  auto topology = op.getIterationTopology();
  return topology && (*topology == sde::SdeIterationTopology::owner_strip ||
                      *topology == sde::SdeIterationTopology::owner_tile ||
                      *topology == sde::SdeIterationTopology::owner_tile_2d);
}

static bool loopIvSelectsOwnerSlice(Value iv, Value ownerIv,
                                    llvm::SmallPtrSetImpl<Value> &seen) {
  if (!iv || !ownerIv || !seen.insert(iv).second)
    return false;
  if (iv == ownerIv || ::mlir::carts::ValueAnalysis::dependsOn(iv, ownerIv))
    return true;

  auto blockArg = dyn_cast<BlockArgument>(iv);
  if (!blockArg)
    return false;

  auto loop = dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp());
  if (!loop || loop.getInductionVar() != iv)
    return false;

  return loopIvSelectsOwnerSlice(loop.getLowerBound(), ownerIv, seen) ||
         loopIvSelectsOwnerSlice(loop.getUpperBound(), ownerIv, seen);
}

static bool loopIvSelectsOwnerSlice(Value iv, Value ownerIv) {
  llvm::SmallPtrSet<Value, 8> seen;
  return loopIvSelectsOwnerSlice(iv, ownerIv, seen);
}

static bool accessMapUsesOwnerSliceAtPhysicalDim(AffineMap map,
                                                 unsigned physicalDim,
                                                 ArrayRef<Value> ivs,
                                                 unsigned ownerLoopDim) {
  if (!map || physicalDim >= map.getNumResults() || ownerLoopDim >= ivs.size())
    return false;
  std::optional<sde::AffineDimOffset> dimOffset =
      sde::extractDimOffset(map.getResult(physicalDim));
  if (!dimOffset || !dimOffset->dim || dimOffset->offset != 0 ||
      *dimOffset->dim >= ivs.size())
    return false;
  return loopIvSelectsOwnerSlice(ivs[*dimOffset->dim], ivs[ownerLoopDim]);
}

static bool
accessEntriesUseOwnerDimsForRoot(ArrayRef<sde::MemrefAccessEntry> accesses,
                                 ArrayRef<Value> ivs, Value root,
                                 ArrayRef<unsigned> physicalOwnerDims) {
  if (physicalOwnerDims.empty() || physicalOwnerDims.size() > ivs.size())
    return false;

  bool sawRoot = false;
  for (const sde::MemrefAccessEntry &access : accesses) {
    Value accessRoot =
        ::mlir::carts::ValueAnalysis::stripMemrefViewOps(access.memref);
    if (accessRoot != root)
      continue;
    sawRoot = true;

    for (auto [ownerLoopDim, physicalDim] :
         llvm::enumerate(physicalOwnerDims)) {
      if (!accessMapUsesOwnerSliceAtPhysicalDim(
              access.indexingMap, physicalDim, ivs,
              static_cast<unsigned>(ownerLoopDim)))
        return false;
    }
  }
  return sawRoot;
}

static bool hasSingleWriteReadIntermediate(
    const sde::StructuredMemoryEffectSummary &predEffects,
    const sde::StructuredMemoryEffectSummary &succEffects, Value &root) {
  if (predEffects.hasUnknownEffects || succEffects.hasUnknownEffects)
    return false;
  if (predEffects.writes.size() != 1)
    return false;

  Value candidate = *predEffects.writes.begin();
  Value rootCandidate =
      ::mlir::carts::ValueAnalysis::stripMemrefViewOps(candidate);
  if (!rootCandidate || !rootCandidate.getDefiningOp<sde::SdeMuAllocOp>())
    return false;
  if (!succEffects.reads.contains(candidate))
    return false;
  if (succEffects.writes.contains(candidate))
    return false;

  for (Value written : succEffects.writes) {
    if (predEffects.reads.contains(written) ||
        predEffects.writes.contains(written))
      return false;
  }

  root = candidate;
  return true;
}

static bool canPipelineThroughTokenLocalMemoryDeps(
    sde::SdeSuIterateOp predecessor, sde::SdeSuIterateOp successor,
    const sde::StructuredMemoryEffectSummary &predEffects,
    const sde::StructuredMemoryEffectSummary &succEffects) {
  if (!isPipelineableStructuredClassification(predecessor) ||
      !isPipelineableStructuredClassification(successor))
    return false;
  if (!haveSdeApprovedTiledTimestepPlan(predecessor, successor))
    return false;

  if (!isTokenLocalPipelineTopology(predecessor) ||
      !sameSdeIterationTopology(predecessor, successor))
    return false;

  std::optional<SmallVector<unsigned, 4>> predOwnerDims =
      getPhysicalOwnerDims(predecessor);
  std::optional<SmallVector<unsigned, 4>> succOwnerDims =
      getPhysicalOwnerDims(successor);
  if (!predOwnerDims || !succOwnerDims || *predOwnerDims != *succOwnerDims)
    return false;

  Value intermediate;
  if (!hasSingleWriteReadIntermediate(predEffects, succEffects, intermediate))
    return false;

  auto predSummary = sde::analyzeSuLoopAccesses(predecessor);
  auto succSummary = sde::analyzeSuLoopAccesses(successor);
  if (!predSummary || !succSummary)
    return false;
  if (predSummary->nest.ivs.empty() || succSummary->nest.ivs.empty())
    return false;

  return accessEntriesUseOwnerDimsForRoot(predSummary->writes,
                                          predSummary->nest.ivs, intermediate,
                                          *predOwnerDims) &&
         accessEntriesUseOwnerDimsForRoot(succSummary->reads,
                                          succSummary->nest.ivs, intermediate,
                                          *succOwnerDims);
}

static bool haveCompatibleTimestepIterationPlan(sde::SdeSuIterateOp lhs,
                                                sde::SdeSuIterateOp rhs) {
  if (haveSameIterationShape(lhs, rhs))
    return true;
  return haveSdeApprovedTiledTimestepPlan(lhs, rhs);
}

static std::optional<SmallVector<int64_t, 4>>
getUniqueStaticWrittenShape(const sde::StructuredMemoryEffectSummary &effects) {
  std::optional<SmallVector<int64_t, 4>> selectedShape;
  for (Value written : effects.writes) {
    Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(written);
    auto memrefType = dyn_cast<MemRefType>(root.getType());
    if (!memrefType || !memrefType.hasStaticShape())
      return std::nullopt;

    // Rank-0 (scalar) memrefs are loop-carried control state (predicate flags,
    // counters), not the distributed data array being double-buffered across
    // timesteps. They must not pollute the data-shape comparison: a guarded
    // jacobi copy/stencil stage writes both its data array and a `memref<i1>`
    // continue-flag, and including the flag makes the written shapes differ,
    // defeating timestep-pair recognition.
    if (memrefType.getRank() == 0)
      continue;

    SmallVector<int64_t, 4> shape(memrefType.getShape().begin(),
                                  memrefType.getShape().end());
    if (!selectedShape) {
      selectedShape = std::move(shape);
      continue;
    }
    if (*selectedShape != shape)
      return std::nullopt;
  }
  return selectedShape;
}

static bool
haveSameStaticWrittenShape(const sde::StructuredMemoryEffectSummary &lhs,
                           const sde::StructuredMemoryEffectSummary &rhs) {
  auto lhsShape = getUniqueStaticWrittenShape(lhs);
  auto rhsShape = getUniqueStaticWrittenShape(rhs);
  return lhsShape && rhsShape && *lhsShape == *rhsShape;
}

static bool isTimestepInterstitialOp(Operation *op) {
  if (!op)
    return false;
  if (op->getNumRegions() != 0) {
    auto effects = sde::collectStructuredMemoryEffects(op);
    return !effects.hasUnknownEffects && effects.empty();
  }
  return !sde::hasUnmodeledMemoryEffect(op);
}

static void stampAlternatingBufferTimestepPlan(sde::SdeSuIterateOp predecessor,
                                               sde::SdeSuIterateOp successor,
                                               bool predecessorIsStencil,
                                               bool successorIsStencil) {
  stampRepeatedTimestepPlan(predecessor);
  stampRepeatedTimestepPlan(successor);
  if (predecessorIsStencil)
    predecessor.setPatternAttr(sde::SdePatternAttr::get(
        predecessor.getContext(), sde::SdePattern::alternating_buffer_stencil));
  if (successorIsStencil)
    successor.setPatternAttr(sde::SdePatternAttr::get(
        successor.getContext(), sde::SdePattern::alternating_buffer_stencil));
}

static bool stampTimestepPlanIfRecognized(
    sde::SdeSuIterateOp predecessor, sde::SdeSuIterateOp successor,
    const sde::StructuredMemoryEffectSummary &predEffects,
    const sde::StructuredMemoryEffectSummary &succEffects,
    bool allowStencilStencilPlan, bool allowUniformUniformPlan) {
  if (!predecessor || !successor)
    return false;
  if (predEffects.hasUnknownEffects || succEffects.hasUnknownEffects)
    return false;

  bool compatibleIterationPlan =
      haveCompatibleTimestepIterationPlan(predecessor, successor);

  if (isUniformRepeatableStage(predecessor) &&
      isUniformRepeatableStage(successor) && compatibleIterationPlan &&
      allowUniformUniformPlan &&
      writesIntersectReads(predEffects, succEffects) &&
      haveCompatiblePhysicalTimestepPlan(predecessor, successor)) {
    stampRepeatedTimestepPlan(predecessor);
    stampRepeatedTimestepPlan(successor);
    return true;
  }

  bool predStencil = isOutOfPlaceStencilStage(predecessor, predEffects);
  bool succStencil = isOutOfPlaceStencilStage(successor, succEffects);
  bool predUniform = isUniformRepeatableStage(predecessor);
  bool succUniform = isUniformRepeatableStage(successor);
  if (!hasAlternatingBufferExchange(predEffects, succEffects))
    return false;

  if (predStencil && succStencil && compatibleIterationPlan &&
      allowStencilStencilPlan) {
    stampRepeatedTimestepPlan(predecessor);
    stampRepeatedTimestepPlan(successor);
    return true;
  }

  if (((predStencil && succUniform) || (predUniform && succStencil)) &&
      (compatibleIterationPlan ||
       haveSameStaticWrittenShape(predEffects, succEffects))) {
    stampAlternatingBufferTimestepPlan(predecessor, successor, predStencil,
                                       succStencil);
    return true;
  }

  return false;
}

static bool stampAdjacentTimestepPair(Operation *predOp, Operation *succOp) {
  sde::SdeSuIterateOp predecessor = findSuIterate(predOp);
  sde::SdeSuIterateOp successor = findSuIterate(succOp);
  if (!predecessor || !successor)
    return false;
  if (!predecessor.getStructuredClassificationAttr() ||
      !successor.getStructuredClassificationAttr())
    return false;

  auto predEffects =
      sde::collectStructuredMemoryEffects(predecessor.getOperation());
  auto succEffects =
      sde::collectStructuredMemoryEffects(successor.getOperation());
  return stampTimestepPlanIfRecognized(predecessor, successor, predEffects,
                                       succEffects,
                                       /*allowStencilStencilPlan=*/true,
                                       /*allowUniformUniformPlan=*/false);
}

static unsigned stampAdjacentTimestepPairsInLoop(scf::ForOp loop) {
  Operation *previousStage = nullptr;
  unsigned stamped = 0;

  for (Operation &op : loop.getBody()->without_terminator()) {
    if (findSuIterate(&op)) {
      if (previousStage && stampAdjacentTimestepPair(previousStage, &op))
        ++stamped;
      previousStage = &op;
      continue;
    }

    if (isTimestepInterstitialOp(&op))
      continue;

    previousStage = nullptr;
  }

  return stamped;
}

struct BarrierEliminationPass
    : public sde::impl::BarrierEliminationBase<BarrierEliminationPass> {
  explicit BarrierEliminationPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    int eliminated = 0;
    unsigned timestepPairsStamped = 0;
    SmallVector<sde::SdeSuBarrierOp, 8> redundantBarriers;

    getOperation().walk([&](sde::SdeSuBarrierOp barrier) {
      setBarrierReason(barrier, sde::SdeBarrierReason::unknown_required);

      Block *block = barrier->getBlock();
      if (!block)
        return;

      Operation *predOp = nullptr;
      Operation *succOp = nullptr;

      // Walk backward to find predecessor su_iterate or su_distribute
      for (auto it = Block::reverse_iterator(barrier->getIterator());
           it != block->rend(); ++it) {
        if (isSuContainer(&*it)) {
          predOp = &*it;
          break;
        }
        if (!isMemoryEffectFree(&*it))
          break;
      }

      // Walk forward to find successor su_iterate or su_distribute
      for (auto it = std::next(barrier->getIterator()); it != block->end();
           ++it) {
        if (isSuContainer(&*it)) {
          succOp = &*it;
          break;
        }
        if (!isMemoryEffectFree(&*it))
          break;
      }

      if (!predOp || !succOp)
        return;

      auto predecessor = findSuIterate(predOp);
      auto successor = findSuIterate(succOp);
      if (!predecessor || !successor)
        return;

      // Both must have classification (analyzed)
      if (!predecessor.getStructuredClassificationAttr() ||
          !successor.getStructuredClassificationAttr())
        return;

      // Collect root-level memory accesses on both sides of the barrier.
      // Use the outer container's region to capture all memory ops.
      auto predEffects = sde::collectStructuredMemoryEffects(predOp);
      auto succEffects = sde::collectStructuredMemoryEffects(succOp);

      if (predEffects.hasUnknownEffects || succEffects.hasUnknownEffects)
        return;

      if (predEffects.empty() && succEffects.empty())
        return;

      if (!predEffects.hasWriteConflictWith(succEffects)) {
        double syncCost = costModel ? costModel->getTaskSyncCost() : 0.0;
        redundantBarriers.push_back(barrier);
        eliminated++;
        ARTS_DEBUG("Eliminated barrier (sync cost: " << syncCost << ")");
        return;
      }

      if (isWavefrontFrontierStage(predecessor) ||
          isWavefrontFrontierStage(successor)) {
        setBarrierReason(barrier, sde::SdeBarrierReason::wavefront_frontier);
        return;
      }

      if (canPipelineThroughTokenLocalMemoryDeps(predecessor, successor,
                                                 predEffects, succEffects)) {
        setBarrierReason(barrier, sde::SdeBarrierReason::required_memory);
        ARTS_DEBUG("Preserved required-memory barrier: token-local storage "
                   "dependencies need an explicit SDE rewrite before the "
                   "global ordering can be removed");
        return;
      }

      if (stampTimestepPlanIfRecognized(predecessor, successor, predEffects,
                                        succEffects,
                                        /*allowStencilStencilPlan=*/true,
                                        /*allowUniformUniformPlan=*/true)) {
        ++timestepPairsStamped;
        setBarrierReason(barrier,
                         sde::SdeBarrierReason::timestep_stage_boundary);
        return;
      }

      setBarrierReason(barrier, sde::SdeBarrierReason::required_memory);
    });

    getOperation().walk([&](scf::ForOp loop) {
      timestepPairsStamped += stampAdjacentTimestepPairsInLoop(loop);
    });

    for (sde::SdeSuBarrierOp barrier : redundantBarriers)
      barrier.erase();

    ARTS_INFO("BarrierElimination: eliminated " << eliminated << " barrier(s)");
    ARTS_INFO("BarrierElimination: stamped " << timestepPairsStamped
                                             << " timestep pair(s)");
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass>
createBarrierEliminationPass(sde::SDECostModel *costModel) {
  return std::make_unique<BarrierEliminationPass>(costModel);
}

} // namespace mlir::carts::sde
