///==========================================================================///
/// File: EpochAmortizeRepeatedLoop.cpp
///
/// Hoist a repeated uniform epoch out of its repeat loop and wrap EDT bodies in
/// inner repeat loops when the current ARTS graph has stable block deps.
///==========================================================================///

#define GEN_PASS_DEF_EPOCHAMORTIZEREPEATEDLOOP

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/EdtUtils.h"
#include "carts/dialect/arts/Utils/LoweringFactUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/Debug.h"
#include "carts/utils/LoopUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/RegionUtils.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include <climits>
#include <optional>
#include <string>

ARTS_DEBUG_SETUP(epoch_amortize_repeated_loop);

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

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

static bool epochHasStableUniformBlockDeps(EpochOp epochOp) {
  Operation *op = epochOp.getOperation();
  auto depPattern = getDepPattern(op);
  if (!depPattern || !isUniformFamilyDepPattern(*depPattern))
    return false;

  bool sawBlockDep = false;
  bool sawHaloDep = false;
  epochOp.walk([&](DbAcquireOp acquire) {
    if (auto facts = resolveAcquireFacts(acquire))
      sawHaloDep |= facts->isStencilFamily() || facts->supportsBlockHalo();
    auto alloc = dyn_cast_or_null<DbAllocOp>(
        DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr()));
    sawBlockDep |= alloc && hasArtsDbPhysicalLayout(alloc.getOperation());
  });
  return sawBlockDep && !sawHaloDep;
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

  if (!epochHasStableUniformBlockDeps(epochOp))
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

struct EpochAmortizeRepeatedLoopPass
    : public impl::EpochAmortizeRepeatedLoopBase<
          EpochAmortizeRepeatedLoopPass> {
  EpochAmortizeRepeatedLoopPass() = default;
  EpochAmortizeRepeatedLoopPass(const EpochAmortizeRepeatedLoopPass &other)
      : impl::EpochAmortizeRepeatedLoopBase<EpochAmortizeRepeatedLoopPass>(
            other) {}

  void runOnOperation() override {
    ModuleOp module = getOperation();

    ARTS_INFO_HEADER(EpochAmortizeRepeatedLoopPass);

    SmallVector<EpochOp> epochs;
    module.walk([&](EpochOp epochOp) { epochs.push_back(epochOp); });
    ARTS_INFO("Found " << epochs.size()
                       << " epoch operations to analyze for amortization");

    unsigned amortized = 0;
    for (EpochOp epochOp : epochs)
      if (tryAmortizeRepeatedEpochLoop(epochOp))
        ++amortized;

    if (amortized > 0) {
      numRepeatedEpochLoopsAmortized += amortized;
      ARTS_INFO("Amortized " << amortized << " repeated epoch loop(s)");
    } else {
      markAllAnalysesPreserved();
    }

    ARTS_INFO_FOOTER(EpochAmortizeRepeatedLoopPass);
  }

private:
  Statistic numRepeatedEpochLoopsAmortized{
      this, "num-repeated-epoch-loops-amortized",
      "Number of repeated epoch loops amortized"};
};

} // namespace

namespace mlir::carts::arts {

std::unique_ptr<Pass> createEpochAmortizeRepeatedLoopPass() {
  return std::make_unique<EpochAmortizeRepeatedLoopPass>();
}

} // namespace mlir::carts::arts
