///==========================================================================///
/// File: EpochOptStructural.cpp
///
/// Structural realization used by EpochOpt: hoist a committed repeated-timestep
/// epoch out of its repeat loop and wrap the EDT bodies in an inner repeat
/// loop. The legality of repeating across timesteps is a committed SDE plan
/// fact (full_timestep repetition over a stable owner topology with no halo
/// widening); this pass realizes that fact and does not re-derive it.
///==========================================================================///

#include "EpochOptInternal.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/Debug.h"

ARTS_DEBUG_SETUP(epoch_opt);

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::epoch_opt {
namespace {

bool isKernelTimerTailOp(Operation *op) {
  auto callOp = dyn_cast<func::CallOp>(op);
  if (!callOp)
    return false;
  return callOp.getCallee().starts_with("carts_kernel_timer_");
}

static bool isRankZeroLocalAlloca(Value value) {
  value = ValueAnalysis::stripNumericCasts(value);
  auto alloca = value.getDefiningOp<memref::AllocaOp>();
  return alloca && alloca.getType().getRank() == 0;
}

static bool isAmortizableSetupOp(Operation *op, Value loopIv) {
  if (!op)
    return false;
  if (isSideEffectFreeArithmeticLikeOp(op))
    return llvm::all_of(op->getOperands(), [&](Value operand) {
      return !ValueAnalysis::dependsOn(operand, loopIv);
    });

  if (auto load = dyn_cast<memref::LoadOp>(op))
    return isRankZeroLocalAlloca(load.getMemRef()) &&
           !ValueAnalysis::dependsOn(load.getMemRef(), loopIv);

  if (auto store = dyn_cast<memref::StoreOp>(op))
    return isRankZeroLocalAlloca(store.getMemRef()) &&
           !ValueAnalysis::dependsOn(store.getMemRef(), loopIv) &&
           !ValueAnalysis::dependsOn(store.getValue(), loopIv);

  return false;
}

static bool isAmortizableTailOp(Operation *op, Value loopIv) {
  if (isKernelTimerTailOp(op))
    return true;
  if (auto store = dyn_cast<memref::StoreOp>(op))
    return isRankZeroLocalAlloca(store.getMemRef()) &&
           !ValueAnalysis::dependsOn(store.getMemRef(), loopIv) &&
           !ValueAnalysis::dependsOn(store.getValue(), loopIv);
  return false;
}

static bool isStableRepeatTopology(Operation *op) {
  auto topology = getPlanIterationTopologyAttr(op);
  return topology &&
         (topology.getValue() == ArtsPlanIterationTopology::owner_strip ||
          topology.getValue() == ArtsPlanIterationTopology::owner_tile);
}

static bool hasNoPlannedHaloWidening(Operation *op) {
  ArrayAttr haloShape = getPlanHaloShapeAttr(op);
  if (!haloShape)
    return true;

  std::optional<SmallVector<int64_t, 4>> haloExtents =
      readI64ArrayAttr(haloShape);
  if (!haloExtents)
    return false;
  return llvm::all_of(*haloExtents, [](int64_t extent) { return extent == 0; });
}

/// Committed SDE authorization for repeating an epoch across timesteps: a
/// uniform-family dependence over a stable owner topology with full-timestep
/// repetition and no planned halo widening. SDE records these only after it
/// proves the cross-timestep exchange is legal; ARTS realizes that proof.
static bool epochHasRepeatStableUniformPlan(EpochOp epochOp) {
  Operation *op = epochOp.getOperation();
  auto depPattern = getDepPattern(op);
  auto repetition = getPlanRepetitionStructureAttr(op);
  return depPattern && isUniformFamilyDepPattern(*depPattern) && repetition &&
         repetition.getValue() == ArtsPlanRepetitionStructure::full_timestep &&
         isStableRepeatTopology(op) && hasNoPlannedHaloWidening(op);
}

bool canWrapEdtBodyWithRepeatLoop(EdtOp edt) {
  Block &body = edt.getBody().front();
  bool seenRelease = false;
  bool sawRepeatableOp = false;
  for (Operation &op : body.without_terminator()) {
    if (isa<DbReleaseOp>(op)) {
      seenRelease = true;
      continue;
    }
    if (seenRelease)
      return false;
    sawRepeatableOp = true;
  }
  return sawRepeatableOp;
}

void wrapEdtBodyWithRepeatLoop(EdtOp edt, int64_t repeatCount) {
  if (repeatCount <= 1)
    return;

  Block &body = edt.getBody().front();
  SmallVector<Operation *> repeatOps;
  Operation *insertionPoint = body.getTerminator();
  for (Operation &op : body.without_terminator()) {
    if (isa<DbReleaseOp>(op)) {
      insertionPoint = &op;
      break;
    }
    repeatOps.push_back(&op);
  }
  if (repeatOps.empty())
    return;

  OpBuilder builder(insertionPoint);
  Location loc = edt.getLoc();
  Value c0 = arith::ConstantIndexOp::create(builder, loc, 0);
  Value cN = arith::ConstantIndexOp::create(builder, loc, repeatCount);
  Value c1 = arith::ConstantIndexOp::create(builder, loc, 1);
  auto repeatFor = scf::ForOp::create(builder, loc, c0, cN, c1);

  Operation *repeatTerminator = repeatFor.getBody()->getTerminator();
  for (Operation *op : repeatOps)
    op->moveBefore(repeatTerminator);
}

} // namespace

bool tryAmortizeRepeatedEpochLoop(EpochOp epochOp) {
  if (!epochOp || !epochOp->getBlock())
    return false;

  scf::ForOp repeatLoop = dyn_cast<scf::ForOp>(epochOp->getParentOp());
  scf::IfOp wrappingIf = nullptr;
  if (!repeatLoop) {
    wrappingIf = dyn_cast<scf::IfOp>(epochOp->getParentOp());
    if (!wrappingIf)
      return false;
    repeatLoop = dyn_cast<scf::ForOp>(wrappingIf->getParentOp());
  }
  if (!repeatLoop)
    return false;
  if (repeatLoop.getNumResults() != 0 || !repeatLoop.getInitArgs().empty())
    return false;
  if (!repeatLoop.getInductionVar().use_empty())
    return false;

  std::optional<int64_t> tripCount =
      ::mlir::carts::getStaticTripCount(repeatLoop.getOperation());
  if (!tripCount || *tripCount < 2)
    return false;

  Value loopIv = repeatLoop.getInductionVar();
  Block *loopBody = repeatLoop.getBody();

  SmallVector<Operation *> prefixOps;
  SmallVector<Operation *> tailOps;

  if (!wrappingIf) {
    if (epochOp->getBlock() != loopBody)
      return false;

    bool seenEpoch = false;
    for (Operation &op : loopBody->without_terminator()) {
      if (&op == epochOp.getOperation()) {
        seenEpoch = true;
        continue;
      }
      if (!seenEpoch) {
        prefixOps.push_back(&op);
        continue;
      }
      tailOps.push_back(&op);
    }
    if (!seenEpoch)
      return false;
  } else {
    if (!wrappingIf.getElseRegion().empty() &&
        !wrappingIf.getElseRegion().front().without_terminator().empty())
      return false;
    if (wrappingIf->getBlock() != loopBody ||
        epochOp->getBlock() != &wrappingIf.getThenRegion().front())
      return false;
    if (ValueAnalysis::dependsOn(wrappingIf.getCondition(), loopIv))
      return false;

    bool seenIf = false;
    for (Operation &op : loopBody->without_terminator()) {
      if (&op == wrappingIf.getOperation()) {
        seenIf = true;
        continue;
      }
      if (seenIf)
        return false;
      prefixOps.push_back(&op);
    }

    bool seenEpoch = false;
    for (Operation &op :
         wrappingIf.getThenRegion().front().without_terminator()) {
      if (&op == epochOp.getOperation()) {
        seenEpoch = true;
        continue;
      }
      if (!seenEpoch) {
        if (!isAmortizableSetupOp(&op, loopIv))
          return false;
        continue;
      }
      tailOps.push_back(&op);
    }
    if (!seenEpoch)
      return false;
  }

  if (!llvm::all_of(prefixOps, [&](Operation *op) {
        return isAmortizableSetupOp(op, loopIv);
      }))
    return false;

  for (Operation *tailOp : tailOps) {
    if (!isAmortizableTailOp(tailOp, loopIv))
      return false;
  }

  if (!epochHasRepeatStableUniformPlan(epochOp))
    return false;

  SmallVector<EdtOp> edts;
  epochOp.walk([&](EdtOp edt) { edts.push_back(edt); });
  if (edts.empty())
    return false;
  for (EdtOp edt : edts) {
    if (!canWrapEdtBodyWithRepeatLoop(edt))
      return false;
  }

  for (Operation *prefixOp : prefixOps)
    prefixOp->moveBefore(repeatLoop);
  for (EdtOp edt : edts)
    wrapEdtBodyWithRepeatLoop(edt, *tripCount);
  if (wrappingIf) {
    wrappingIf->moveBefore(repeatLoop);
  } else {
    epochOp->moveBefore(repeatLoop);
    for (Operation *tailOp : tailOps)
      tailOp->moveBefore(repeatLoop);
  }
  if (repeatLoop.getBody()->without_terminator().empty())
    repeatLoop.erase();

  ARTS_INFO("  Amortized repeated epoch loop (trip count = " << *tripCount
                                                             << ")");
  return true;
}

} // namespace mlir::carts::arts::epoch_opt
