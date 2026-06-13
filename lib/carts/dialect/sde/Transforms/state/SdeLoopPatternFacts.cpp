///==========================================================================///
/// File: SdeLoopPatternFacts.cpp
///
/// Identify memref-backed SDE patterns and commit approved SDE facts.
///==========================================================================///

#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/dialect/sde/Utils/SdeOwnerLoopPromotion.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/MathExtras.h"
#include <algorithm>

namespace mlir::carts::sde {
#define GEN_PASS_DEF_SDELOOPPATTERNFACTS
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/utils/Debug.h"
ARTS_DEBUG_SETUP(sde_loop_pattern_facts);

using namespace mlir;
using namespace mlir::carts;

namespace {

static bool hasParallelLeafCu(sde::SdeSuIterateOp op) {
  if (!op || op.getBody().empty())
    return false;
  for (Operation &child : op.getBody().front().without_terminator()) {
    auto cuRegion = dyn_cast<sde::SdeCuRegionOp>(child);
    if (cuRegion && cuRegion.getKind() == sde::SdeCuKind::parallel)
      return true;
  }
  return false;
}

static Value accessRoot(Value value) {
  if (!value)
    return {};
  if (isa<BaseMemRefType>(value.getType()))
    return ::mlir::carts::ValueAnalysis::stripMemrefViewOps(value);
  return value;
}

static bool sameAccessRoot(Value lhs, Value rhs) {
  Value lhsRoot = accessRoot(lhs);
  Value rhsRoot = accessRoot(rhs);
  return lhsRoot && rhsRoot && lhsRoot == rhsRoot;
}

static bool hasSelfRead(const sde::SuLoopAccessSummary &summary) {
  for (const sde::MemrefAccessEntry &write : summary.writes)
    for (const sde::MemrefAccessEntry &read : summary.reads)
      if (sameAccessRoot(write.memref, read.memref))
        return true;
  return false;
}

static bool isRankZeroMemref(Value value) {
  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(value);
  if (!root)
    return false;
  auto type = dyn_cast<MemRefType>(root.getType());
  return type && type.getRank() == 0;
}

static bool isRankedDataMemrefRoot(Value value) {
  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(value);
  if (!root)
    return false;
  auto type = dyn_cast<MemRefType>(root.getType());
  return type && type.getRank() > 0;
}

static sde::StructuredMemoryEffectSummary
collectStructuredDataMemoryEffects(sde::SdeSuIterateOp op) {
  sde::StructuredMemoryEffectSummary summary;
  if (!op)
    return summary;

  op.getBody().walk([&](Operation *nested) {
    if (auto loadOp = dyn_cast<memref::LoadOp>(nested)) {
      Value root =
          ::mlir::carts::ValueAnalysis::stripMemrefViewOps(loadOp.getMemref());
      if (isRankedDataMemrefRoot(root) &&
          !sde::isDefinedInside(op.getOperation(), root))
        summary.reads.insert(root);
      return;
    }

    if (auto storeOp = dyn_cast<memref::StoreOp>(nested)) {
      Value root =
          ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storeOp.getMemref());
      if (isRankedDataMemrefRoot(root) &&
          !sde::isDefinedInside(op.getOperation(), root))
        summary.writes.insert(root);
      return;
    }

    if (sde::hasUnmodeledMemoryEffect(nested))
      summary.hasUnknownEffects = true;
  });
  return summary;
}

static bool attrMatchesValues(ArrayAttr attr, ArrayRef<int64_t> values) {
  std::optional<SmallVector<int64_t, 4>> parsed = readI64ArrayAttr(attr);
  return parsed && llvm::equal(*parsed, values);
}

static bool
explicitStencilFactsMatch(sde::SdeSuIterateOp op,
                          const sde::SuNeighborhoodAccessInfo &info) {
  if (!attrMatchesValues(op.getAccessMinOffsetsAttr(), info.minOffsets) ||
      !attrMatchesValues(op.getAccessMaxOffsetsAttr(), info.maxOffsets) ||
      !attrMatchesValues(op.getOwnerDimsAttr(), info.ownerDims) ||
      !attrMatchesValues(op.getWriteFootprintAttr(), info.writeFootprint))
    return false;
  if (op.getSpatialDimsAttr() &&
      !attrMatchesValues(op.getSpatialDimsAttr(), info.spatialDims))
    return false;
  return true;
}

static unsigned countHaloDims(const sde::SuNeighborhoodAccessInfo &info) {
  unsigned count = 0;
  for (auto [minOffset, maxOffset] :
       llvm::zip(info.minOffsets, info.maxOffsets))
    if (minOffset != 0 || maxOffset != 0)
      ++count;
  return count;
}

static bool hasHigherOrderHalo(const sde::SuNeighborhoodAccessInfo &info) {
  for (auto [minOffset, maxOffset] :
       llvm::zip(info.minOffsets, info.maxOffsets))
    if (minOffset < -1 || maxOffset > 1)
      return true;
  return false;
}

static bool isWavefront2D(const sde::SuLoopAccessSummary &summary,
                          const sde::SuNeighborhoodAccessInfo &info) {
  if (info.ownerDims.size() != 2 || !hasSelfRead(summary))
    return false;

  SmallVector<bool, 4> sawNegative(summary.nest.ivs.size(), false);
  bool sawPositiveSelfReadOffset = false;
  for (const sde::MemrefAccessEntry &write : summary.writes) {
    for (const sde::MemrefAccessEntry &read : summary.reads) {
      if (!sameAccessRoot(write.memref, read.memref))
        continue;
      for (AffineExpr result : read.indexingMap.getResults()) {
        auto dimOffset = sde::extractDimOffset(result);
        if (!dimOffset || !dimOffset->dim)
          continue;
        unsigned dim = *dimOffset->dim;
        if (dim >= sawNegative.size())
          continue;
        if (dimOffset->offset < 0)
          sawNegative[dim] = true;
        if (dimOffset->offset > 0)
          sawPositiveSelfReadOffset = true;
      }
    }
  }

  if (sawPositiveSelfReadOffset)
    return false;

  unsigned negativeOwnerDims = 0;
  for (int64_t dim : info.ownerDims)
    if (dim >= 0 && static_cast<size_t>(dim) < sawNegative.size() &&
        sawNegative[dim])
      ++negativeOwnerDims;
  return negativeOwnerDims == 2;
}

static bool
hasNonZeroHaloOnAllOwnerDims(const sde::SuNeighborhoodAccessInfo &info) {
  if (info.ownerDims.empty() ||
      info.minOffsets.size() != info.maxOffsets.size())
    return false;
  for (int64_t rawDim : info.ownerDims) {
    if (rawDim < 0 || static_cast<size_t>(rawDim) >= info.minOffsets.size())
      return false;
    unsigned dim = static_cast<unsigned>(rawDim);
    if (info.minOffsets[dim] == 0 && info.maxOffsets[dim] == 0)
      return false;
  }
  return true;
}

static bool
hasSingleExternalWriteRoot(sde::SdeSuIterateOp op,
                           const sde::SuLoopAccessSummary &summary) {
  Value selectedRoot;
  for (const sde::MemrefAccessEntry &write : summary.writes) {
    Value root = accessRoot(write.memref);
    if (!root || !isRankedDataMemrefRoot(root) ||
        sde::isDefinedInside(op.getOperation(), root))
      continue;
    if (!selectedRoot) {
      selectedRoot = root;
      continue;
    }
    if (selectedRoot != root)
      return false;
  }
  return selectedRoot != nullptr;
}

static std::optional<int64_t> getStaticTripCount(Value lowerBound,
                                                 Value upperBound, Value step) {
  int64_t lb = 0;
  int64_t ub = 0;
  int64_t stride = 0;
  if (!::mlir::carts::ValueAnalysis::getConstantIndex(lowerBound, lb) ||
      !::mlir::carts::ValueAnalysis::getConstantIndex(upperBound, ub) ||
      !::mlir::carts::ValueAnalysis::getConstantIndex(step, stride) ||
      stride <= 0)
    return std::nullopt;
  return llvm::divideCeil(std::max<int64_t>(0, ub - lb), stride);
}

static bool
hasBoundedPromotedIterationVolume(sde::SdeSuIterateOp op,
                                  ArrayRef<scf::ForOp> innerForChain) {
  constexpr int64_t kMaxPromotedStaticIterations = 32LL * 1024LL * 1024LL;
  std::optional<int64_t> outerTrip =
      getStaticTripCount(op.getLowerBounds().front(),
                         op.getUpperBounds().front(), op.getSteps().front());
  if (!outerTrip)
    return false;

  int64_t volume = std::max<int64_t>(1, *outerTrip);
  for (scf::ForOp innerFor : innerForChain) {
    std::optional<int64_t> trip = getStaticTripCount(
        innerFor.getLowerBound(), innerFor.getUpperBound(), innerFor.getStep());
    if (!trip)
      return false;
    int64_t safeTrip = std::max<int64_t>(1, *trip);
    if (volume > kMaxPromotedStaticIterations / safeTrip)
      return false;
    volume *= safeTrip;
  }
  return volume <= kMaxPromotedStaticIterations;
}

static bool regionTouchesMemrefRoot(Operation *scope, Value root) {
  if (!scope || !root)
    return false;
  bool touches = false;
  scope->walk([&](Operation *op) {
    Value memref;
    if (auto load = dyn_cast<memref::LoadOp>(op))
      memref = load.getMemref();
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      memref = store.getMemref();
    else if (auto dealloc = dyn_cast<memref::DeallocOp>(op))
      memref = dealloc.getMemref();
    else
      return WalkResult::advance();

    if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(memref) == root) {
      touches = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return touches;
}

static bool isPromotableRankZeroControlStore(Operation *op,
                                             sde::SdeSuIterateOp owner,
                                             scf::ForOp nestedFor) {
  auto store = dyn_cast<memref::StoreOp>(op);
  if (!store ||
      !sde::isScalarOrVectorValueType(store.getValueToStore().getType()) ||
      !store.getValueToStore().getDefiningOp<arith::ConstantOp>())
    return false;
  if (sde::isDefinedInside(owner.getOperation(), store.getValueToStore()))
    return false;

  Value root =
      ::mlir::carts::ValueAnalysis::stripMemrefViewOps(store.getMemref());
  if (!root || !isRankZeroMemref(root))
    return false;
  if (sde::isDefinedInside(owner.getOperation(), root))
    return false;

  return !regionTouchesMemrefRoot(nestedFor.getOperation(), root);
}

static SmallVector<scf::ForOp, 4>
findPromotableInnerForChain(sde::SdeSuIterateOp owner, Block *computeBlock) {
  SmallVector<scf::ForOp, 4> chain;
  if (!owner || !computeBlock)
    return chain;

  Block *current = computeBlock;
  while (current) {
    scf::ForOp nestedFor;
    for (Operation &op : current->without_terminator()) {
      if (auto forOp = dyn_cast<scf::ForOp>(op)) {
        if (nestedFor)
          return {};
        nestedFor = forOp;
      }
    }
    if (!nestedFor)
      return chain;

    for (Operation &op : current->without_terminator()) {
      if (&op == nestedFor.getOperation())
        continue;
      bool beforeNestedFor = op.isBeforeInBlock(nestedFor.getOperation());
      bool allowedRankZeroControlStore =
          beforeNestedFor &&
          isPromotableRankZeroControlStore(&op, owner, nestedFor);
      if (!beforeNestedFor ||
          (!isMemoryEffectFree(&op) &&
           !sde::isLocalScratchEffect(&op, owner.getOperation()) &&
           !allowedRankZeroControlStore))
        return {};
    }

    chain.push_back(nestedFor);
    current = nestedFor.getBody();
  }

  return chain;
}

static SmallVector<scf::ForOp, 4>
findPromotableInnerForPrefix(sde::SdeSuIterateOp owner, Block *computeBlock) {
  SmallVector<scf::ForOp, 4> prefix;
  if (!owner || !computeBlock)
    return prefix;

  Block *current = computeBlock;
  while (current) {
    scf::ForOp nestedFor;
    for (Operation &op : current->without_terminator()) {
      if (auto forOp = dyn_cast<scf::ForOp>(op)) {
        if (nestedFor)
          return prefix;
        nestedFor = forOp;
      }
    }
    if (!nestedFor || !nestedFor.getInitArgs().empty() ||
        nestedFor.getNumResults() != 0)
      return prefix;

    for (Operation &op : current->without_terminator()) {
      if (&op == nestedFor.getOperation())
        continue;
      bool beforeNestedFor = op.isBeforeInBlock(nestedFor.getOperation());
      bool allowedRankZeroControlStore =
          beforeNestedFor &&
          isPromotableRankZeroControlStore(&op, owner, nestedFor);
      if (!beforeNestedFor ||
          (!isMemoryEffectFree(&op) &&
           !sde::isLocalScratchEffect(&op, owner.getOperation()) &&
           !allowedRankZeroControlStore))
        return prefix;
    }

    prefix.push_back(nestedFor);
    current = nestedFor.getBody();
  }

  return prefix;
}

static bool isSafeOutOfPlaceStencilPromotion(
    sde::SdeSuIterateOp op, const sde::SuLoopAccessSummary &summary,
    const sde::SuNeighborhoodAccessInfo &neighborhood,
    ArrayRef<scf::ForOp> innerForChain,
    bool requireExistingStencilFactsMatch = false) {
  auto reject = [&](StringRef reason) {
    ARTS_DEBUG("skipped out-of-place stencil promotion: " << reason);
    return false;
  };
  if (!op || innerForChain.empty())
    return reject("missing op or promotable inner loop");
  if (op.getLowerBounds().size() != 1 || op.getUpperBounds().size() != 1 ||
      op.getSteps().size() != 1)
    return reject("owner loop is not rank-1");
  if (op.getNumResults() != 0 ||
      !op.getReductionAccumulators().empty() || op.getReductionKindsAttr())
    return reject("owner loop has chunk/results/reduction carrier");
  if (summary.classification != sde::SdeStructuredClassification::stencil)
    return reject("summary is not stencil");
  unsigned promotedRank = 1 + innerForChain.size();
  if (summary.nest.ivs.size() != promotedRank ||
      summary.iterTypes.size() != promotedRank)
    return reject("summary rank does not match promotable loop chain");
  // Rank-3+ promotion is useful for compact kernels, but very large static
  // domains can make downstream optimization cost dominate the intended gain.
  if (promotedRank > 2 && !hasBoundedPromotedIterationVolume(op, innerForChain))
    return reject("rank-3+ promoted iteration space is too large");
  if (!llvm::all_of(summary.iterTypes, [](utils::IteratorType iteratorType) {
        return iteratorType == utils::IteratorType::parallel;
      }))
    return reject("not all promoted dimensions are parallel");
  if (neighborhood.ownerDims.size() != promotedRank ||
      neighborhood.spatialDims.size() < promotedRank ||
      !hasNonZeroHaloOnAllOwnerDims(neighborhood))
    return reject("neighborhood does not cover all promoted owner dims");
  if (requireExistingStencilFactsMatch &&
      !explicitStencilFactsMatch(op, neighborhood))
    return reject("explicit stencil facts do not match recovered access");
  if (!sde::findCompatibleSuOutputLayoutFacts(summary))
    return reject("no compatible output layout facts");
  if (!hasSingleExternalWriteRoot(op, summary))
    return reject("writes do not have one external root");

  auto effects = collectStructuredDataMemoryEffects(op);
  if (effects.hasUnknownEffects || effects.writes.empty())
    return reject("memory effects are unknown/empty");
  if (sde::hasInPlaceSelfRead(effects) &&
      !sde::hasOnlyPointInPlaceSelfReads(summary))
    return reject("in-place stencil has non-point self reads");

  Block *computeBlock = sde::getSuIterateComputeBlock(op);
  if (!computeBlock)
    return reject("missing compute block");
  if (auto cuRegion =
          dyn_cast_or_null<sde::SdeCuRegionOp>(computeBlock->getParentOp()))
    if (!cuRegion.getIterArgs().empty() || cuRegion.getNumResults() != 0)
      return reject("cu_region has iter args/results");

  Value outerIv = op.getBody().front().getArgument(0);
  SmallVector<Value, 4> previousIvs{outerIv};
  for (scf::ForOp innerFor : innerForChain) {
    if (!innerFor.getInitArgs().empty() || innerFor.getNumResults() != 0)
      return reject("inner loop has init args/results");
    for (Value previousIv : previousIvs) {
      if (::mlir::carts::ValueAnalysis::dependsOn(innerFor.getLowerBound(),
                                                  previousIv) ||
          ::mlir::carts::ValueAnalysis::dependsOn(innerFor.getUpperBound(),
                                                  previousIv) ||
          ::mlir::carts::ValueAnalysis::dependsOn(innerFor.getStep(),
                                                  previousIv))
        return reject("inner loop bounds depend on previous IVs");
    }
    previousIvs.push_back(innerFor.getInductionVar());
  }

  return true;
}

static bool promotedLoopBoundsAreRectangular(sde::SdeSuIterateOp op,
                                             ArrayRef<scf::ForOp> loops) {
  if (!op || loops.empty() || op.getBody().empty())
    return false;

  SmallVector<Value, 4> previousIvs;
  unsigned loopRank = op.getLowerBounds().size();
  if (op.getBody().front().getNumArguments() < loopRank)
    return false;
  for (BlockArgument arg :
       op.getBody().front().getArguments().take_front(loopRank))
    previousIvs.push_back(arg);

  for (scf::ForOp loop : loops) {
    for (Value bound :
         {loop.getLowerBound(), loop.getUpperBound(), loop.getStep()}) {
      if (!bound || sde::isDefinedInside(op.getOperation(), bound))
        return false;
      for (Value previousIv : previousIvs)
        if (::mlir::carts::ValueAnalysis::dependsOn(bound, previousIv))
          return false;
    }
    previousIvs.push_back(loop.getInductionVar());
  }

  return true;
}

static void removeStaleShapeAttrs(sde::SdeSuIterateOp op) {
  if (sde::SdeCuRegionOp cu = sde::findSuComputeCuRegion(op))
    cu.removeGroupBlockCountAttr();
}

static void clonePromotedPreludeControlStores(
    OpBuilder &builder, sde::SdeSuIterateOp owner, Block *sourceBlock,
    ArrayRef<scf::ForOp> innerForChain, unsigned depth = 0) {
  if (!sourceBlock || depth >= innerForChain.size())
    return;

  scf::ForOp nestedFor = innerForChain[depth];
  for (Operation &bodyOp : sourceBlock->without_terminator()) {
    if (&bodyOp == nestedFor.getOperation()) {
      clonePromotedPreludeControlStores(builder, owner, nestedFor.getBody(),
                                        innerForChain, depth + 1);
      return;
    }
    if (isPromotableRankZeroControlStore(&bodyOp, owner, nestedFor))
      builder.clone(bodyOp);
  }
}

static void clonePromotedBody(OpBuilder &builder, sde::SdeSuIterateOp owner,
                              Block *sourceBlock,
                              ArrayRef<scf::ForOp> innerForChain,
                              unsigned depth, IRMapping &mapping) {
  scf::ForOp nestedFor =
      depth < innerForChain.size() ? innerForChain[depth] : scf::ForOp();
  for (Operation &bodyOp : sourceBlock->without_terminator()) {
    if (nestedFor &&
        isPromotableRankZeroControlStore(&bodyOp, owner, nestedFor))
      continue;
    if (nestedFor && &bodyOp == nestedFor.getOperation()) {
      clonePromotedBody(builder, owner, nestedFor.getBody(), innerForChain,
                        depth + 1, mapping);
      return;
    }
    builder.clone(bodyOp, mapping);
  }
}

// The prologue here (outer-bound shape, chunk/result/reduction rejection,
// CuRegion iter-arg check, inner-for shape, outer-IV independence of inner
// bounds) is shared with `isSafeOpaqueElementwiseInnerOwnerPromotion` below.
// The body walks diverge significantly (summary-driven rank>=3 store check vs
// libm-call rejection + read/write disjointness with rank>=2), so the two are
// kept as two flat predicates: factoring only the prologue saved <25 lines
// while forcing readers to context-switch to verify the invariants.
static bool hasSiblingStencilConsumingWrittenRoot(sde::SdeSuIterateOp writer,
                                                  Value writtenRoot);
static Value elementwiseExternalWrittenRoot(sde::SdeSuIterateOp op);

static bool
isSafeElementwiseInnerOwnerPromotion(sde::SdeSuIterateOp op,
                                     const sde::SuLoopAccessSummary &summary,
                                     ArrayRef<scf::ForOp> innerForChain) {
  if (!op || innerForChain.empty())
    return false;
  if (op.getLowerBounds().size() != 1 || op.getUpperBounds().size() != 1 ||
      op.getSteps().size() != 1)
    return false;
  if (op.getNumResults() != 0 ||
      !op.getReductionAccumulators().empty() || op.getReductionKindsAttr())
    return false;
  if (summary.classification != sde::SdeStructuredClassification::elementwise)
    return false;
  if (summary.nest.ivs.size() < 2 || summary.iterTypes.size() < 2)
    return false;
  if (!llvm::all_of(summary.iterTypes, [](utils::IteratorType iteratorType) {
        return iteratorType == utils::IteratorType::parallel;
      }))
    return false;

  Block *computeBlock = sde::getSuIterateComputeBlock(op);
  if (!computeBlock)
    return false;
  if (auto cuRegion =
          dyn_cast_or_null<sde::SdeCuRegionOp>(computeBlock->getParentOp()))
    if (!cuRegion.getIterArgs().empty() || cuRegion.getNumResults() != 0)
      return false;

  scf::ForOp innerFor = innerForChain.front();
  if (!innerFor || !innerFor.getInitArgs().empty() ||
      innerFor.getNumResults() != 0)
    return false;

  Value outerIv = op.getBody().front().getArgument(0);
  Value innerIv = innerFor.getInductionVar();
  if (::mlir::carts::ValueAnalysis::dependsOn(innerFor.getLowerBound(),
                                              outerIv) ||
      ::mlir::carts::ValueAnalysis::dependsOn(innerFor.getUpperBound(),
                                              outerIv) ||
      ::mlir::carts::ValueAnalysis::dependsOn(innerFor.getStep(), outerIv))
    return false;

  auto staticTripCount = [](Value lb, Value ub,
                            Value step) -> std::optional<int64_t> {
    std::optional<int64_t> lbConst =
        ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(lb);
    std::optional<int64_t> ubConst =
        ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(ub);
    std::optional<int64_t> stepConst =
        ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(step);
    if (!lbConst || !ubConst || !stepConst || *stepConst <= 0)
      return std::nullopt;
    return std::max<int64_t>(0, (*ubConst - *lbConst + *stepConst - 1) /
                                    *stepConst);
  };
  // SDE-5 reconciliation: an elementwise writer whose output is consumed by a
  // sibling stencil (the double-buffer copy `u = unew`, with the stencil
  // reading `u` on a halo) MUST adopt the stencil's owner_tile layout, or `u`
  // carries two layouts and the per-timestep host bridge cannot hoist. For that
  // single coupled case the rank-2 floor and the iteration-extent cap (both
  // default guards against promoting unrelated/huge elementwise spaces) are
  // lifted: the owner_tile of a rank-2 stencil array is required regardless of
  // extent. The gate is tight — a host-init copy no stencil consumes never
  // qualifies, so the ordinary uniform-promotion behavior is unchanged.
  bool stencilCoupledOwnerTile = false;
  if (Value writtenRoot = elementwiseExternalWrittenRoot(op))
    if (auto writtenType = dyn_cast<MemRefType>(writtenRoot.getType()))
      stencilCoupledOwnerTile =
          writtenType.getRank() == 2 &&
          hasSiblingStencilConsumingWrittenRoot(op, writtenRoot);

  const int64_t rankFloor = stencilCoupledOwnerTile ? 2 : 3;
  constexpr int64_t kMaxPromotedOwnerExtent = 1024;
  std::optional<int64_t> outerTrip =
      staticTripCount(op.getLowerBounds().front(), op.getUpperBounds().front(),
                      op.getSteps().front());
  std::optional<int64_t> innerTrip = staticTripCount(
      innerFor.getLowerBound(), innerFor.getUpperBound(), innerFor.getStep());
  // The static trip bound only feeds the iteration-extent cap, a guard against
  // promoting unrelated/huge elementwise spaces. A stencil-coupled writer must
  // adopt owner_tile regardless of extent and regardless of whether the bound
  // is a runtime value (the double-buffer size is dynamic), so both the
  // static-trip requirement and the cap are lifted only for that case.
  if (!stencilCoupledOwnerTile) {
    if (!outerTrip || !innerTrip || *outerTrip > kMaxPromotedOwnerExtent ||
        *innerTrip > kMaxPromotedOwnerExtent)
      return false;
  }

  auto effects = sde::collectStructuredMemoryEffects(op.getBody());
  if (effects.hasUnknownEffects || effects.writes.empty())
    return false;
  if (sde::hasInPlaceSelfRead(effects) &&
      !sde::hasOnlyPointInPlaceSelfReads(summary))
    return false;

  std::optional<unsigned> promotedPhysicalDim;
  bool sawRankedMemrefWrite = false;
  bool rejected = false;
  op.getBody().walk([&](memref::StoreOp storeOp) {
    if (rejected)
      return WalkResult::interrupt();
    Value root =
        ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storeOp.getMemref());
    if (sde::isDefinedInside(op.getOperation(), root))
      return WalkResult::advance();
    auto type = dyn_cast<MemRefType>(root.getType());
    // A stencil-coupled double-buffer copy also stores a loop-carried control
    // flag (rank-0/1 `memref<i1>`); that flag is not the tiled data array and
    // must be skipped, not treated as a disqualifying write. Outside the
    // coupled case the original strict rejection is preserved.
    if (type && stencilCoupledOwnerTile && type.getRank() < 2)
      return WalkResult::advance();
    if (!type || type.getRank() < rankFloor ||
        storeOp.getIndices().size() != static_cast<size_t>(type.getRank())) {
      rejected = true;
      return WalkResult::interrupt();
    }

    std::optional<unsigned> storeDim;
    std::optional<unsigned> outerDim;
    for (auto [dim, index] : llvm::enumerate(storeOp.getIndices())) {
      if (stencilCoupledOwnerTile) {
        SmallVector<Value, 1> outerOwners{outerIv};
        if (sde::isExactOwnerIndex(index, outerOwners)) {
          if (outerDim && *outerDim != dim) {
            rejected = true;
            return WalkResult::interrupt();
          }
          outerDim = static_cast<unsigned>(dim);
        }
      }
      SmallVector<Value, 1> innerOwners{innerIv};
      if (!sde::isExactOwnerIndex(index, innerOwners))
        continue;
      if (dim == 0 && !stencilCoupledOwnerTile) {
        rejected = true;
        return WalkResult::interrupt();
      }
      storeDim = static_cast<unsigned>(dim);
      break;
    }
    if (!storeDim) {
      rejected = true;
      return WalkResult::interrupt();
    }
    if (stencilCoupledOwnerTile && (!outerDim || *outerDim == *storeDim)) {
      rejected = true;
      return WalkResult::interrupt();
    }
    if (promotedPhysicalDim && *promotedPhysicalDim != *storeDim) {
      rejected = true;
      return WalkResult::interrupt();
    }
    promotedPhysicalDim = *storeDim;
    sawRankedMemrefWrite = true;
    return WalkResult::advance();
  });

  return !rejected && sawRankedMemrefWrite && promotedPhysicalDim.has_value();
}

static sde::SdeSuIterateOp
promoteElementwiseInnerOwnerLoop(sde::SdeSuIterateOp op, scf::ForOp innerFor,
                                 bool outerFirst = false) {
  OpBuilder builder(op);
  Location loc = op.getLoc();
  // Default order places the promoted inner loop as su_iterate dim 0 and the
  // original owner loop as dim 1 (inner, outer). `outerFirst` flips this to
  // (outer, inner) so the promoted owner-dim ORDER matches a sibling stencil
  // that already iterates (outer, inner): without it the elementwise copy's
  // per-dep owner dims lower to the reversed array order ([1,0] vs the
  // stencil's [0,1]) and the host-bridge compatibility check rejects the hoist.
  // Used for the stencil-coupled double-buffer copy (SDE-5 reconciliation).
  SmallVector<Value, 2> lowerBounds =
      outerFirst ? SmallVector<Value, 2>{op.getLowerBounds().front(),
                                         innerFor.getLowerBound()}
                 : SmallVector<Value, 2>{innerFor.getLowerBound(),
                                         op.getLowerBounds().front()};
  SmallVector<Value, 2> upperBounds =
      outerFirst ? SmallVector<Value, 2>{op.getUpperBounds().front(),
                                         innerFor.getUpperBound()}
                 : SmallVector<Value, 2>{innerFor.getUpperBound(),
                                         op.getUpperBounds().front()};
  SmallVector<Value, 2> steps =
      outerFirst
          ? SmallVector<Value, 2>{op.getSteps().front(), innerFor.getStep()}
          : SmallVector<Value, 2>{innerFor.getStep(), op.getSteps().front()};

  auto newOp = sde::buildSuIterate(
      builder, loc, ValueRange(lowerBounds), ValueRange(upperBounds),
      ValueRange(steps), sde::SuIterateAttrs::fromOp(op),
      op.getReductionAccumulators());
  newOp->setAttrs(sde::getRewrittenAttrs(op));
  removeStaleShapeAttrs(newOp);

  Block &newBody = sde::ensureBlock(newOp.getBody());
  while (newBody.getNumArguments() < 2)
    newBody.addArgument(builder.getIndexType(), loc);

  IRMapping mapping;
  // Keep the IV->block-arg mapping consistent with the chosen dim order: with
  // outerFirst the outer owner IV is dim 0 and the promoted inner IV is dim 1.
  mapping.map(op.getBody().front().getArgument(0),
              newBody.getArgument(outerFirst ? 0 : 1));
  mapping.map(innerFor.getInductionVar(),
              newBody.getArgument(outerFirst ? 1 : 0));

  Block *oldComputeBlock = sde::getSuIterateComputeBlock(op);
  auto oldCuRegion =
      dyn_cast_or_null<sde::SdeCuRegionOp>(oldComputeBlock->getParentOp());
  SmallVector<scf::ForOp, 1> promoted{innerFor};

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(newOp);
  clonePromotedPreludeControlStores(builder, op, oldComputeBlock, promoted);
  builder.setInsertionPointToStart(&newBody);
  Block *cloneBlock = &newBody;
  if (oldCuRegion) {
    auto newCuRegion = sde::buildCuRegion(
        builder, loc, oldCuRegion.getKindAttr(), oldCuRegion.getNowaitAttr(),
        /*iterArgs=*/ValueRange{}, /*resultTypes=*/TypeRange{},
        oldCuRegion.getSerialReasonAttr());
    cloneBlock = &sde::ensureBlock(newCuRegion.getBody());
    builder.setInsertionPointToStart(cloneBlock);
  }

  clonePromotedBody(builder, op, oldComputeBlock, promoted, 0, mapping);
  sde::SdeYieldOp::create(builder, loc, ValueRange{});

  if (oldCuRegion) {
    builder.setInsertionPointAfter(cloneBlock->getParentOp());
    sde::SdeYieldOp::create(builder, loc, ValueRange{});
  }

  op.erase();
  return newOp;
}

static bool
isSafeOpaqueElementwiseInnerOwnerPromotion(sde::SdeSuIterateOp op,
                                           ArrayRef<scf::ForOp> innerForChain) {
  if (!op || innerForChain.empty())
    return false;
  if (op.getLowerBounds().size() != 1 || op.getUpperBounds().size() != 1 ||
      op.getSteps().size() != 1)
    return false;
  if (op.getNumResults() != 0 ||
      !op.getReductionAccumulators().empty() || op.getReductionKindsAttr())
    return false;

  Block *computeBlock = sde::getSuIterateComputeBlock(op);
  if (!computeBlock)
    return false;
  if (auto cuRegion =
          dyn_cast_or_null<sde::SdeCuRegionOp>(computeBlock->getParentOp()))
    if (!cuRegion.getIterArgs().empty() || cuRegion.getNumResults() != 0)
      return false;

  scf::ForOp innerFor = innerForChain.front();
  if (!innerFor || !innerFor.getInitArgs().empty() ||
      innerFor.getNumResults() != 0)
    return false;

  Value outerIv = op.getBody().front().getArgument(0);
  Value innerIv = innerFor.getInductionVar();
  if (::mlir::carts::ValueAnalysis::dependsOn(innerFor.getLowerBound(),
                                              outerIv) ||
      ::mlir::carts::ValueAnalysis::dependsOn(innerFor.getUpperBound(),
                                              outerIv) ||
      ::mlir::carts::ValueAnalysis::dependsOn(innerFor.getStep(), outerIv))
    return false;

  llvm::DenseSet<Value> readRoots;
  llvm::DenseSet<Value> writeRoots;
  std::optional<unsigned> promotedPhysicalDim;
  bool sawExternalWrite = false;
  bool rejected = false;

  op.getBody().walk([&](Operation *nested) {
    if (rejected)
      return WalkResult::interrupt();
    if (auto call = dyn_cast<func::CallOp>(nested))
      if (!sde::isKnownPureScalarLibmCallee(call.getCallee())) {
        rejected = true;
        return WalkResult::interrupt();
      }

    if (auto loadOp = dyn_cast<memref::LoadOp>(nested)) {
      if (isa<MemRefType>(loadOp.getResult().getType()))
        return WalkResult::advance();
      Value root =
          ::mlir::carts::ValueAnalysis::stripMemrefViewOps(loadOp.getMemref());
      if (!root || sde::isDefinedInside(op.getOperation(), root))
        return WalkResult::advance();
      readRoots.insert(root);
      return WalkResult::advance();
    }

    if (auto storeOp = dyn_cast<memref::StoreOp>(nested)) {
      if (isa<MemRefType>(storeOp.getValueToStore().getType()))
        return WalkResult::advance();
      Value root =
          ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storeOp.getMemref());
      if (!root || sde::isDefinedInside(op.getOperation(), root))
        return WalkResult::advance();

      auto type = dyn_cast<MemRefType>(root.getType());
      if (!type || type.getRank() < 2 ||
          storeOp.getIndices().size() != static_cast<size_t>(type.getRank())) {
        rejected = true;
        return WalkResult::interrupt();
      }

      bool outerIndexed = false;
      std::optional<unsigned> innerIndexedDim;
      Value outerOwners[1] = {outerIv};
      Value innerOwners[1] = {innerIv};
      for (auto [dim, index] : llvm::enumerate(storeOp.getIndices())) {
        if (sde::isExactOwnerIndex(index, outerOwners))
          outerIndexed = true;
        if (sde::isExactOwnerIndex(index, innerOwners))
          innerIndexedDim = static_cast<unsigned>(dim);
      }
      if (!outerIndexed || !innerIndexedDim || *innerIndexedDim == 0) {
        rejected = true;
        return WalkResult::interrupt();
      }
      if (promotedPhysicalDim && *promotedPhysicalDim != *innerIndexedDim) {
        rejected = true;
        return WalkResult::interrupt();
      }
      promotedPhysicalDim = *innerIndexedDim;
      writeRoots.insert(root);
      sawExternalWrite = true;
      return WalkResult::advance();
    }

    return WalkResult::advance();
  });

  if (rejected || !sawExternalWrite || !promotedPhysicalDim)
    return false;
  for (Value writeRoot : writeRoots)
    if (readRoots.contains(writeRoot))
      return false;
  return true;
}

static sde::SdeSuIterateOp
tryPromoteOpaqueElementwiseInnerOwnerLoop(sde::SdeSuIterateOp op) {
  Block *computeBlock = sde::getSuIterateComputeBlock(op);
  SmallVector<scf::ForOp, 4> innerForChain =
      findPromotableInnerForChain(op, computeBlock);
  if (!isSafeOpaqueElementwiseInnerOwnerPromotion(op, innerForChain))
    return op;
  return promoteElementwiseInnerOwnerLoop(op, innerForChain.front());
}

static std::optional<bool>
shouldKeepOuterLoopFirstForPromotion(sde::SdeSuIterateOp op,
                                     scf::ForOp innerFor) {
  if (!op || !innerFor || op.getBody().empty())
    return std::nullopt;
  Value outerIv = op.getBody().front().getArgument(0);
  Value innerIv = innerFor.getInductionVar();
  std::optional<unsigned> outerDim;
  std::optional<unsigned> innerDim;
  bool rejected = false;
  op.getBody().walk([&](memref::StoreOp storeOp) {
    if (rejected)
      return WalkResult::interrupt();
    Value root =
        ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storeOp.getMemref());
    if (sde::isDefinedInside(op.getOperation(), root))
      return WalkResult::advance();
    auto type = dyn_cast<MemRefType>(root.getType());
    if (!type || type.getRank() < 2)
      return WalkResult::advance();

    std::optional<unsigned> storeOuterDim;
    std::optional<unsigned> storeInnerDim;
    for (auto [dim, index] : llvm::enumerate(storeOp.getIndices())) {
      SmallVector<Value, 1> outerOwners{outerIv};
      SmallVector<Value, 1> innerOwners{innerIv};
      if (sde::isExactOwnerIndex(index, outerOwners))
        storeOuterDim = static_cast<unsigned>(dim);
      if (sde::isExactOwnerIndex(index, innerOwners))
        storeInnerDim = static_cast<unsigned>(dim);
    }
    if (!storeOuterDim || !storeInnerDim || *storeOuterDim == *storeInnerDim) {
      rejected = true;
      return WalkResult::interrupt();
    }
    if ((outerDim && *outerDim != *storeOuterDim) ||
        (innerDim && *innerDim != *storeInnerDim)) {
      rejected = true;
      return WalkResult::interrupt();
    }
    outerDim = *storeOuterDim;
    innerDim = *storeInnerDim;
    return WalkResult::advance();
  });

  if (rejected || !outerDim || !innerDim)
    return std::nullopt;
  return *outerDim < *innerDim;
}

static sde::SdeSuIterateOp
tryPromoteElementwiseInnerOwnerLoop(sde::SdeSuIterateOp op,
                                    const sde::SuLoopAccessSummary &summary) {
  Block *computeBlock = sde::getSuIterateComputeBlock(op);
  SmallVector<scf::ForOp, 4> innerForChain =
      findPromotableInnerForChain(op, computeBlock);
  if (!isSafeElementwiseInnerOwnerPromotion(op, summary, innerForChain))
    return op;
  // Stencil-coupled double-buffer copies must adopt the (outer, inner)
  // owner-dim order of their stencil consumer so the per-dep owner dims lower
  // in the same array order (SDE-5 reconciliation). The signal is the same
  // committed-layout coupling used to admit the rank-2 promotion; recompute it
  // here (cheap) to pick the dim order without widening
  // promoteElementwiseInnerOwnerLoop's API.
  bool outerFirst = false;
  if (Value writtenRoot = elementwiseExternalWrittenRoot(op))
    if (auto t = dyn_cast<MemRefType>(writtenRoot.getType()))
      if (t.getRank() == 2 &&
          hasSiblingStencilConsumingWrittenRoot(op, writtenRoot))
        outerFirst =
            shouldKeepOuterLoopFirstForPromotion(op, innerForChain.front())
                .value_or(true);
  return promoteElementwiseInnerOwnerLoop(op, innerForChain.front(),
                                          outerFirst);
}

static sde::SdeSuIterateOp
promoteNestedParallelOwnerLoops(sde::SdeSuIterateOp op,
                                ArrayRef<scf::ForOp> innerForChain) {
  OpBuilder builder(op);
  Location loc = op.getLoc();
  SmallVector<Value, 4> lowerBounds(op.getLowerBounds().begin(),
                                    op.getLowerBounds().end());
  SmallVector<Value, 4> upperBounds(op.getUpperBounds().begin(),
                                    op.getUpperBounds().end());
  SmallVector<Value, 4> steps(op.getSteps().begin(), op.getSteps().end());
  for (scf::ForOp innerFor : innerForChain) {
    lowerBounds.push_back(innerFor.getLowerBound());
    upperBounds.push_back(innerFor.getUpperBound());
    steps.push_back(innerFor.getStep());
  }

  auto newOp = sde::buildSuIterate(
      builder, loc, ValueRange(lowerBounds), ValueRange(upperBounds),
      ValueRange(steps), sde::SuIterateAttrs::fromOp(op),
      op.getReductionAccumulators());
  newOp->setAttrs(sde::getRewrittenAttrs(op));
  removeStaleShapeAttrs(newOp);

  Block &newBody = sde::ensureBlock(newOp.getBody());
  while (newBody.getNumArguments() < lowerBounds.size())
    newBody.addArgument(builder.getIndexType(), loc);

  IRMapping mapping;
  unsigned originalRank = op.getLowerBounds().size();
  for (unsigned dim = 0; dim < originalRank; ++dim)
    mapping.map(op.getBody().front().getArgument(dim),
                newBody.getArgument(dim));
  for (auto [idx, rawInnerFor] : llvm::enumerate(innerForChain)) {
    scf::ForOp innerFor = rawInnerFor;
    mapping.map(innerFor.getInductionVar(),
                newBody.getArgument(originalRank + idx));
  }

  Block *oldComputeBlock = sde::getSuIterateComputeBlock(op);
  auto oldCuRegion =
      dyn_cast_or_null<sde::SdeCuRegionOp>(oldComputeBlock->getParentOp());

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(newOp);
  clonePromotedPreludeControlStores(builder, op, oldComputeBlock,
                                    innerForChain);
  builder.setInsertionPointToStart(&newBody);
  Block *cloneBlock = &newBody;
  if (oldCuRegion) {
    auto newCuRegion = sde::buildCuRegion(
        builder, loc, oldCuRegion.getKindAttr(), oldCuRegion.getNowaitAttr(),
        /*iterArgs=*/ValueRange{}, /*resultTypes=*/TypeRange{},
        oldCuRegion.getSerialReasonAttr());
    cloneBlock = &sde::ensureBlock(newCuRegion.getBody());
    builder.setInsertionPointToStart(cloneBlock);
  }

  clonePromotedBody(builder, op, oldComputeBlock, innerForChain, 0, mapping);
  sde::SdeYieldOp::create(builder, loc, ValueRange{});

  if (oldCuRegion) {
    builder.setInsertionPointAfter(cloneBlock->getParentOp());
    sde::SdeYieldOp::create(builder, loc, ValueRange{});
  }

  op.erase();
  return newOp;
}

static sde::SdeSuIterateOp tryPromoteOutOfPlaceStencilOwnerLoop(
    sde::SdeSuIterateOp op, const sde::SuLoopAccessSummary &summary,
    const sde::SuNeighborhoodAccessInfo &neighborhood,
    bool requireExistingStencilFactsMatch = false) {
  Block *computeBlock = sde::getSuIterateComputeBlock(op);
  SmallVector<scf::ForOp, 4> innerForChain =
      findPromotableInnerForChain(op, computeBlock);
  if (!isSafeOutOfPlaceStencilPromotion(op, summary, neighborhood,
                                        innerForChain,
                                        requireExistingStencilFactsMatch))
    return op;
  return promoteNestedParallelOwnerLoops(op, innerForChain);
}

static sde::SdeSuIterateOp
tryPromoteNestedParallelPrefix(sde::SdeSuIterateOp op,
                               const sde::SuLoopAccessSummary &summary) {
  if (!op || op.getNumResults() != 0 ||
      !op.getReductionAccumulators().empty() || op.getReductionKindsAttr())
    return op;
  if (sde::hasCommittedCuMuPartitionFacts(op.getOperation()))
    return op;

  unsigned loopRank = op.getLowerBounds().size();
  if (loopRank == 0 || op.getUpperBounds().size() != loopRank ||
      op.getSteps().size() != loopRank ||
      summary.iterTypes.size() <= loopRank ||
      summary.nest.ivs.size() <= loopRank)
    return op;
  if (op.getBody().front().getNumArguments() < loopRank)
    return op;

  for (unsigned dim = 0; dim < loopRank; ++dim)
    if (summary.iterTypes[dim] != utils::IteratorType::parallel)
      return op;

  unsigned requiredRank = loopRank;
  while (requiredRank < summary.iterTypes.size() &&
         summary.iterTypes[requiredRank] == utils::IteratorType::parallel)
    ++requiredRank;
  if (requiredRank == loopRank)
    return op;
  if (summary.nest.ivs.size() < requiredRank)
    return op;

  Block *computeBlock = sde::getSuIterateComputeBlock(op);
  if (!computeBlock)
    return op;
  if (auto cuRegion =
          dyn_cast_or_null<sde::SdeCuRegionOp>(computeBlock->getParentOp()))
    if (!cuRegion.getIterArgs().empty() || cuRegion.getNumResults() != 0)
      return op;

  SmallVector<scf::ForOp, 4> innerForPrefix =
      findPromotableInnerForPrefix(op, computeBlock);
  unsigned promoteCount = requiredRank - loopRank;
  if (innerForPrefix.size() < promoteCount)
    return op;
  innerForPrefix.truncate(promoteCount);

  if (!promotedLoopBoundsAreRectangular(op, innerForPrefix))
    return op;
  if (!sde::findCompatibleSuOutputLayoutFacts(summary))
    return op;

  auto effects = collectStructuredDataMemoryEffects(op);
  if (effects.hasUnknownEffects || effects.writes.empty())
    return op;
  if (sde::hasInPlaceSelfRead(effects) &&
      !sde::hasOnlyPointInPlaceSelfReads(summary))
    return op;

  if (summary.classification == sde::SdeStructuredClassification::elementwise &&
      loopRank == 1 && promoteCount == 1) {
    std::optional<bool> outerFirst =
        shouldKeepOuterLoopFirstForPromotion(op, innerForPrefix.front());
    if (outerFirst)
      return promoteElementwiseInnerOwnerLoop(op, innerForPrefix.front(),
                                              *outerFirst);
  }

  return promoteNestedParallelOwnerLoops(op, innerForPrefix);
}

/// True when `root` is the output of a SIBLING `sde.su_iterate` (a distributed
/// scheduling unit) — a computed distributed intermediate rather than a
/// host-initialized array. The tight gate distinguishes a sibling-written
/// intermediate from a host-init loop output: only the former is
/// block-distributed and demands cross-owner contraction tiling when consumed
/// on its contraction axis.
static bool isSiblingDistributedIntermediate(sde::SdeSuIterateOp consumer,
                                             Value root) {
  if (!root)
    return false;
  Operation *scope = consumer->getParentOfType<ModuleOp>();
  if (!scope)
    return false;
  bool found = false;
  scope->walk([&](sde::SdeSuIterateOp producer) {
    if (found || producer == consumer)
      return;
    bool writesRoot = false;
    producer.getBody().walk([&](memref::StoreOp storeOp) {
      if (writesRoot)
        return;
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(
              storeOp.getMemref()) == root)
        writesRoot = true;
    });
    if (writesRoot)
      found = true;
  });
  return found;
}

/// True when a SIBLING `sde.su_iterate` consumes `writtenRoot` as a stencil
/// (neighborhood) read. In an iterative double-buffer timestep, the elementwise
/// copy `u[i][j] = unew[i][j]` writes `u`, and the stencil
/// `unew[i][j] = f(u[i-1][j], u[i+1][j], ...)` reads `u` with halo access. The
/// copy alone classifies as a 1-D owner_strip write, while the stencil consumer
/// fixes `u` as a 2-D owner_tile array. Unless the elementwise writer adopts
/// the same owner_tile layout, `u` carries two layouts and the host bridge
/// cannot hoist out of the timestep loop (the SDE-5 divergence). This predicate
/// is the tight, order-independent gate that authorizes promoting such a writer
/// to the owner_tile of its stencil consumer; it never fires for a host-init
/// copy whose output no stencil reads with a neighborhood.
static bool hasSiblingStencilConsumingWrittenRoot(sde::SdeSuIterateOp writer,
                                                  Value writtenRoot) {
  if (!writtenRoot)
    return false;
  Operation *scope = writer->getParentOfType<ModuleOp>();
  if (!scope)
    return false;
  bool found = false;
  scope->walk([&](sde::SdeSuIterateOp consumer) {
    if (found || consumer == writer)
      return;
    std::optional<sde::SuLoopAccessSummary> summary =
        sde::analyzeSuLoopAccesses(consumer);
    if (!summary ||
        summary->classification != sde::SdeStructuredClassification::stencil)
      return;
    consumer.getBody().walk([&](memref::LoadOp loadOp) {
      if (found)
        return;
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(
              loadOp.getMemref()) == writtenRoot)
        found = true;
    });
  });
  return found;
}

/// The external (op-output) ranked memref root an elementwise loop writes, used
/// to test stencil-consumer coupling before deciding promotion legality.
/// Returns the single distinct ranked root stored to a memref defined outside
/// `op`, or null if there is not exactly one.
static Value elementwiseExternalWrittenRoot(sde::SdeSuIterateOp op) {
  Value root;
  bool ambiguous = false;
  op.getBody().walk([&](memref::StoreOp storeOp) {
    if (ambiguous)
      return;
    Value candidate =
        ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storeOp.getMemref());
    if (sde::isDefinedInside(op.getOperation(), candidate))
      return;
    auto type = dyn_cast<MemRefType>(candidate.getType());
    // Only the distributed data array matters here. Rank-0/rank-1 loop-carried
    // control flags (e.g. the timestep `memref<i1>`) are not subject to owner
    // tiling and must not make the data root look ambiguous.
    if (!type || type.getRank() < 2)
      return;
    if (root && root != candidate) {
      ambiguous = true;
      root = nullptr;
      return;
    }
    root = candidate;
  });
  return ambiguous ? Value() : root;
}

static sde::SdeSuIterateOp runOwnerLoopPromotions(sde::SdeSuIterateOp op) {
  if (sde::hasCommittedCuMuPartitionFacts(op.getOperation()))
    return op;

  std::optional<sde::SuLoopAccessSummary> summary =
      sde::analyzeSuLoopAccesses(op);
  if (!summary) {
    sde::SdeSuIterateOp promoted =
        tryPromoteOpaqueElementwiseInnerOwnerLoop(op);
    if (promoted != op)
      ARTS_DEBUG("promoted opaque SDE elementwise owner loop");
    return promoted;
  }

  sde::SdeStructuredClassification classification = summary->classification;
  bool hasExplicitStencilFacts = false;
  if (auto existingClassification = op.getStructuredClassification();
      existingClassification &&
      *existingClassification == sde::SdeStructuredClassification::stencil &&
      op.getAccessMinOffsetsAttr() && op.getAccessMaxOffsetsAttr() &&
      op.getOwnerDimsAttr() && op.getWriteFootprintAttr()) {
    classification = *existingClassification;
    hasExplicitStencilFacts = true;
  } else if (auto existingClassification = op.getStructuredClassification();
             existingClassification &&
             *existingClassification ==
                 sde::SdeStructuredClassification::elementwise_pipeline &&
             classification ==
                 sde::SdeStructuredClassification::elementwise) {
    classification = sde::SdeStructuredClassification::elementwise_pipeline;
  } else if (auto existingClassification = op.getStructuredClassification();
             existingClassification &&
             classification == sde::SdeStructuredClassification::reduction &&
             *existingClassification !=
                 sde::SdeStructuredClassification::reduction &&
             op.getReductionAccumulators().empty()) {
    classification = *existingClassification;
  }

  if (!hasExplicitStencilFacts &&
      classification != sde::SdeStructuredClassification::elementwise) {
    if (sde::SdeSuIterateOp promoted =
            tryPromoteNestedParallelPrefix(op, *summary);
        promoted != op) {
      op = promoted;
      summary = sde::analyzeSuLoopAccesses(op);
      if (!summary)
        return op;
      classification = summary->classification;
    }
  }

  if (classification == sde::SdeStructuredClassification::elementwise) {
    sde::SdeSuIterateOp promoted =
        tryPromoteElementwiseInnerOwnerLoop(op, *summary);
    if (promoted != op)
      return promoted;
  }

  if (classification == sde::SdeStructuredClassification::stencil) {
    std::optional<sde::SuNeighborhoodAccessInfo> neighborhoodSummary =
        sde::extractNeighborhoodAccessInfo(*summary);
    if (neighborhoodSummary) {
      sde::SdeSuIterateOp promoted = tryPromoteOutOfPlaceStencilOwnerLoop(
          op, *summary, *neighborhoodSummary,
          /*requireExistingStencilFactsMatch=*/hasExplicitStencilFacts);
      if (promoted != op)
        return promoted;
    }
  }

  return op;
}

struct SdeLoopPatternFactsPass
    : public sde::impl::SdeLoopPatternFactsBase<SdeLoopPatternFactsPass> {
  using SdeLoopPatternFactsBase::SdeLoopPatternFactsBase;

  void runOnOperation() override {
    sde::promoteModuleOwnerLoops(getOperation());
  }
};

} // namespace

namespace mlir::carts::sde {

SdeSuIterateOp promoteSuIterateOwnerLoops(SdeSuIterateOp op) {
  return runOwnerLoopPromotions(op);
}

void promoteModuleOwnerLoops(ModuleOp moduleOp) {
  if (!moduleOp)
    return;
  moduleOp.walk([&](SdeSuIterateOp op) {
    (void)promoteSuIterateOwnerLoops(op);
  });
}

std::unique_ptr<Pass> createSdeLoopPatternFactsPass() {
  return std::make_unique<SdeLoopPatternFactsPass>();
}

} // namespace mlir::carts::sde
