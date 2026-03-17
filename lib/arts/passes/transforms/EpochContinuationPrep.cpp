///==========================================================================///
/// File: EpochContinuationPrep.cpp
///
/// Epoch Continuation Preparation Pass - transforms epoch+tail patterns into
/// finish-EDT continuation scheduling when safe.
///
/// This pass runs in high-level ARTS form:
///   1) in the epochs stage after CreateEpochs
///   2) again in pre-lowering after ParallelEdtLowering (to catch newly
///      materialized epochs)
/// It detects safe "epoch + tail" patterns and outlines the tail into a
/// continuation arts.edt, allowing the epoch to use ARTS-native finish-EDT
/// signaling instead of blocking waits.
///
/// The pass is gated behind --arts-epoch-finish-continuation.
///
/// Pattern detected (at the arts.epoch level):
///   arts.epoch {
///     arts.edt task (%deps) { ... worker body ... }
///     arts.yield
///   } : i64
///   ... (tail operations) ...
///
/// Transformed to:
///   arts.edt task (%deps) {
///     ^bb0(%args):
///       ... (tail operations) ...
///       arts.yield
///   }  {arts.has_control_dep = 1, arts.continuation_for_epoch}
///   arts.epoch {
///     arts.edt task (%deps) { ... worker body ... }
///     arts.yield
///   } : i64
///   // tail operations REMOVED from parent block
///
/// The continuation EDT is placed BEFORE the epoch so that EdtLowering and
/// EpochLowering see it as a normal EDT. EpochLowering then detects the
/// arts.continuation_for_epoch attribute and wires the finish target.
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/passes/PassDetails.h"
#include "arts/passes/Passes.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Support/Debug.h"
#include <optional>

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(epoch_continuation_prep);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Attribute names for continuation metadata.
constexpr llvm::StringLiteral kHasControlDep = "arts.has_control_dep";
constexpr llvm::StringLiteral kContinuationForEpoch =
    "arts.continuation_for_epoch";

struct EpochContinuationPrepPass
    : public arts::EpochContinuationPrepBase<EpochContinuationPrepPass> {
  void runOnOperation() override;

private:
  /// Detect and rewrite repeated loop patterns:
  ///   scf.for (repeat) { arts.epoch { ... arts.edt ... } ; timer-tail }
  /// into:
  ///   arts.epoch { ... arts.edt(repeat loop inside body) ... } ; timer-tail
  /// This amortizes epoch create/wait and EDT creation overhead.
  bool tryAmortizeRepeatedEpochLoop(EpochOp epochOp);

  /// Check if an epoch + tail pattern is eligible for continuation.
  bool isEligible(EpochOp epochOp, SmallVectorImpl<Operation *> &tailOps);

  /// Transform an eligible epoch + tail into continuation form.
  LogicalResult transformToContinuation(EpochOp epochOp,
                                        ArrayRef<Operation *> tailOps);

  /// Collect values defined before the tail that are used by tail operations.
  void collectCapturedValues(ArrayRef<Operation *> tailOps, EpochOp epochOp,
                             SetVector<Value> &capturedDbAcquires);
};

/// Check if an operation is inside a loop.
static bool isInsideLoop(Operation *op) {
  Operation *parent = op->getParentOp();
  while (parent) {
    if (isa<arts::ForOp, scf::ForOp, scf::WhileOp, scf::ParallelOp,
            affine::AffineForOp>(parent))
      return true;
    parent = parent->getParentOp();
  }
  return false;
}

/// Return static trip count for an scf.for loop when all bounds are constants.
static std::optional<int64_t> getStaticTripCount(scf::ForOp forOp) {
  auto lb = forOp.getLowerBound().getDefiningOp<arith::ConstantIndexOp>();
  auto ub = forOp.getUpperBound().getDefiningOp<arith::ConstantIndexOp>();
  auto step = forOp.getStep().getDefiningOp<arith::ConstantIndexOp>();
  if (!lb || !ub || !step)
    return std::nullopt;
  int64_t lbv = lb.value();
  int64_t ubv = ub.value();
  int64_t sv = step.value();
  if (sv <= 0 || ubv <= lbv)
    return std::nullopt;
  return (ubv - lbv + sv - 1) / sv;
}

static bool isKernelTimerTailOp(Operation *op) {
  auto callOp = dyn_cast<func::CallOp>(op);
  if (!callOp)
    return false;
  return callOp.getCallee().starts_with("carts_kernel_timer_");
}

static Value stripMemrefWrapperOps(Value value) {
  while (value) {
    if (auto castOp = value.getDefiningOp<memref::CastOp>()) {
      value = castOp.getSource();
      continue;
    }
    if (auto subviewOp = value.getDefiningOp<memref::SubViewOp>()) {
      value = subviewOp.getSource();
      continue;
    }
    if (auto reinterpretOp = value.getDefiningOp<memref::ReinterpretCastOp>()) {
      value = reinterpretOp.getSource();
      continue;
    }
    break;
  }
  return value;
}

static std::optional<unsigned> mapMemrefToEdtArg(EdtOp edt, Value memrefValue) {
  if (!memrefValue)
    return std::nullopt;
  Value current = stripMemrefWrapperOps(memrefValue);
  if (auto dbRef = current.getDefiningOp<DbRefOp>())
    current = stripMemrefWrapperOps(dbRef.getSource());

  auto blockArg = dyn_cast<BlockArgument>(current);
  if (!blockArg)
    return std::nullopt;
  Block *edtBody = &edt.getBody().front();
  if (blockArg.getOwner() != edtBody)
    return std::nullopt;
  return blockArg.getArgNumber();
}

static void classifyEdtArgAccesses(EdtOp edt, SmallVectorImpl<bool> &reads,
                                   SmallVectorImpl<bool> &writes) {
  reads.assign(edt.getDependencies().size(), false);
  writes.assign(edt.getDependencies().size(), false);

  auto markRead = [&](Value memrefValue) {
    auto argIdx = mapMemrefToEdtArg(edt, memrefValue);
    if (argIdx && *argIdx < reads.size())
      reads[*argIdx] = true;
  };
  auto markWrite = [&](Value memrefValue) {
    auto argIdx = mapMemrefToEdtArg(edt, memrefValue);
    if (argIdx && *argIdx < writes.size())
      writes[*argIdx] = true;
  };

  edt.walk([&](Operation *nested) {
    if (auto load = dyn_cast<memref::LoadOp>(nested))
      markRead(load.getMemRef());
    else if (auto store = dyn_cast<memref::StoreOp>(nested))
      markWrite(store.getMemRef());
    else if (auto affineLoad = dyn_cast<affine::AffineLoadOp>(nested))
      markRead(affineLoad.getMemRef());
    else if (auto affineStore = dyn_cast<affine::AffineStoreOp>(nested))
      markWrite(affineStore.getMemRef());
  });
}

static bool canWrapEdtBodyWithRepeatLoop(EdtOp edt) {
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

static void wrapEdtBodyWithRepeatLoop(EdtOp edt, int64_t repeatCount) {
  if (repeatCount <= 1)
    return;

  Block &body = edt.getBody().front();
  SmallVector<Operation *> repeatOps;
  for (Operation &op : body.without_terminator()) {
    if (isa<DbReleaseOp>(op))
      break;
    repeatOps.push_back(&op);
  }
  if (repeatOps.empty())
    return;

  OpBuilder builder(body.getTerminator());
  Location loc = edt.getLoc();
  Value c0 = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value cN = builder.create<arith::ConstantIndexOp>(loc, repeatCount);
  Value c1 = builder.create<arith::ConstantIndexOp>(loc, 1);
  auto repeatFor = builder.create<scf::ForOp>(loc, c0, cN, c1);

  Operation *repeatTerminator = repeatFor.getBody()->getTerminator();
  for (Operation *op : repeatOps)
    op->moveBefore(repeatTerminator);
}

static bool epochSupportsRepetitionAmortization(EpochOp epochOp) {
  DenseSet<Operation *> readAllocs;
  DenseSet<Operation *> writeAllocs;
  bool sawEdt = false;
  bool safe = true;

  epochOp.walk([&](EdtOp edt) {
    sawEdt = true;
    SmallVector<bool> argReads;
    SmallVector<bool> argWrites;
    classifyEdtArgAccesses(edt, argReads, argWrites);

    ValueRange deps = edt.getDependencies();
    if (deps.size() != argReads.size()) {
      safe = false;
      return WalkResult::interrupt();
    }

    for (unsigned i = 0; i < deps.size(); ++i) {
      auto acq = deps[i].getDefiningOp<DbAcquireOp>();
      if (!acq) {
        safe = false;
        return WalkResult::interrupt();
      }

      Operation *alloc = DbUtils::getUnderlyingDbAlloc(acq.getSourcePtr());
      if (!alloc) {
        safe = false;
        return WalkResult::interrupt();
      }

      if (argReads[i] && DbUtils::isWriterMode(acq.getMode())) {
        safe = false;
        return WalkResult::interrupt();
      }
      if (argWrites[i] && acq.getMode() == ArtsMode::in) {
        safe = false;
        return WalkResult::interrupt();
      }

      if (argReads[i])
        readAllocs.insert(alloc);
      if (argWrites[i])
        writeAllocs.insert(alloc);
    }
    return WalkResult::advance();
  });

  if (!safe || !sawEdt)
    return false;
  for (Operation *alloc : writeAllocs) {
    if (readAllocs.contains(alloc))
      return false;
  }
  return true;
}

} // namespace

void EpochContinuationPrepPass::runOnOperation() {
  ModuleOp module = getOperation();

  ARTS_INFO_HEADER(EpochContinuationPrepPass);

  /// Collect all epoch ops that have tail code after them.
  SmallVector<EpochOp> epochOps;
  module.walk([&](EpochOp epochOp) { epochOps.push_back(epochOp); });

  ARTS_INFO("Found " << epochOps.size() << " epoch operations to analyze");

  unsigned amortized = 0;
  unsigned transformed = 0;
  for (EpochOp epochOp : epochOps) {
    if (tryAmortizeRepeatedEpochLoop(epochOp))
      ++amortized;

    SmallVector<Operation *> tailOps;
    if (!isEligible(epochOp, tailOps)) {
      ARTS_DEBUG("  Epoch not eligible for continuation");
      continue;
    }

    ARTS_INFO("  Transforming eligible epoch to continuation form");
    if (succeeded(transformToContinuation(epochOp, tailOps)))
      ++transformed;
  }

  ARTS_INFO("Amortized " << amortized << " repeated epoch loop(s)");
  ARTS_INFO("Transformed " << transformed << " epochs to continuation form");
  ARTS_INFO_FOOTER(EpochContinuationPrepPass);
}

bool EpochContinuationPrepPass::tryAmortizeRepeatedEpochLoop(EpochOp epochOp) {
  if (!epochOp || !epochOp->getBlock())
    return false;

  auto repeatLoop = dyn_cast<scf::ForOp>(epochOp->getParentOp());
  if (!repeatLoop)
    return false;
  if (repeatLoop.getNumResults() != 0 || !repeatLoop.getInitArgs().empty())
    return false;
  if (!repeatLoop.getInductionVar().use_empty())
    return false;

  std::optional<int64_t> tripCount = getStaticTripCount(repeatLoop);
  if (!tripCount || *tripCount < 2)
    return false;

  Block *loopBody = repeatLoop.getBody();
  if (epochOp->getBlock() != loopBody)
    return false;

  bool seenEpoch = false;
  SmallVector<Operation *> tailOps;
  for (Operation &op : loopBody->without_terminator()) {
    if (&op == epochOp.getOperation()) {
      seenEpoch = true;
      continue;
    }
    if (!seenEpoch)
      return false;
    tailOps.push_back(&op);
  }
  if (!seenEpoch)
    return false;
  for (Operation *tailOp : tailOps) {
    if (!isKernelTimerTailOp(tailOp))
      return false;
  }

  if (!epochSupportsRepetitionAmortization(epochOp))
    return false;

  SmallVector<EdtOp> edts;
  epochOp.walk([&](EdtOp edt) { edts.push_back(edt); });
  if (edts.empty())
    return false;
  for (EdtOp edt : edts) {
    if (!canWrapEdtBodyWithRepeatLoop(edt))
      return false;
  }

  for (EdtOp edt : edts)
    wrapEdtBodyWithRepeatLoop(edt, *tripCount);

  epochOp->moveBefore(repeatLoop);
  if (repeatLoop.getBody()->without_terminator().empty())
    repeatLoop.erase();

  ARTS_INFO("  Amortized repeated epoch loop (trip count = " << *tripCount
                                                             << ")");
  return true;
}

bool EpochContinuationPrepPass::isEligible(
    EpochOp epochOp, SmallVectorImpl<Operation *> &tailOps) {
  /// Rule 1: Must be in a block (not floating).
  Block *block = epochOp->getBlock();
  if (!block)
    return false;

  /// Rule 2: The epoch must not be inside a loop (conservative: no CPS yet).
  if (isInsideLoop(epochOp))
    return false;

  /// Rule 3: The epoch region must not be empty.
  if (epochOp.getRegion().empty() ||
      epochOp.getRegion().front().without_terminator().empty())
    return false;

  /// Rule 4: Collect tail operations (everything after the epoch in the same
  /// block, before the block terminator).
  bool afterEpoch = false;
  for (Operation &op : *block) {
    if (&op == epochOp.getOperation()) {
      afterEpoch = true;
      continue;
    }
    if (afterEpoch && !op.hasTrait<OpTrait::IsTerminator>())
      tailOps.push_back(&op);
  }

  /// Rule 5: Must have at least one tail operation to outline.
  if (tailOps.empty())
    return false;

  /// Rule 6: No nested epochs in the tail.
  for (Operation *op : tailOps) {
    bool hasNestedEpoch = false;
    if (isa<EpochOp, CreateEpochOp>(op))
      return false;
    op->walk([&](Operation *inner) {
      if (isa<EpochOp, CreateEpochOp>(inner))
        hasNestedEpoch = true;
    });
    if (hasNestedEpoch)
      return false;
  }

  /// Rule 6b: Tail must not capture non-DB external values.
  /// Continuation EdtOp bodies may only reference their block args/deps.
  /// Current continuation lowering only maps captured DbAcquire values, so any
  /// other external capture would violate EdtOp verifier constraints.
  DenseSet<Operation *> tailOpSet(tailOps.begin(), tailOps.end());
  for (Operation *op : tailOps) {
    for (Value operand : op->getOperands()) {
      auto *defOp = operand.getDefiningOp();
      if (!defOp)
        return false;
      if (tailOpSet.contains(defOp))
        continue;
      if (!isa<DbAcquireOp>(defOp))
        return false;
    }
  }

  /// Rule 7: The epoch must not already be marked for continuation.
  if (epochOp->hasAttr(kContinuationForEpoch))
    return false;

  /// Rule 8: The block terminator must not use any values defined by tail ops.
  /// If it does (e.g., `return %result` where %result is from a tail load),
  /// we can't outline the tail without also handling the terminator's liveness.
  Operation *terminator = block->getTerminator();
  if (terminator) {
    for (Value operand : terminator->getOperands()) {
      if (auto *defOp = operand.getDefiningOp()) {
        if (tailOpSet.contains(defOp))
          return false;
      }
    }
  }

  return true;
}

void EpochContinuationPrepPass::collectCapturedValues(
    ArrayRef<Operation *> tailOps, EpochOp epochOp,
    SetVector<Value> &capturedDbAcquires) {
  /// Build set of ops in the tail for quick lookup.
  DenseSet<Operation *> tailOpSet(tailOps.begin(), tailOps.end());

  for (Operation *op : tailOps) {
    for (Value operand : op->getOperands()) {
      /// Skip values defined within the tail itself.
      if (auto *defOp = operand.getDefiningOp()) {
        if (tailOpSet.contains(defOp))
          continue;
      }
      /// Only collect DB acquires as explicit dependencies.
      /// Scalar captures are handled automatically by EdtLowering's
      /// EdtEnvManager which detects free variables.
      if (isa_and_nonnull<DbAcquireOp>(operand.getDefiningOp()))
        capturedDbAcquires.insert(operand);
    }
  }
}

LogicalResult EpochContinuationPrepPass::transformToContinuation(
    EpochOp epochOp, ArrayRef<Operation *> tailOps) {
  OpBuilder builder(epochOp->getContext());
  Location loc = epochOp.getLoc();

  /// Step 1: Collect DB acquires used by the tail.
  SetVector<Value> capturedDbAcquires;
  collectCapturedValues(tailOps, epochOp, capturedDbAcquires);

  ARTS_DEBUG("  DB acquires captured: " << capturedDbAcquires.size());

  /// Step 2: Build the dependency list for the continuation EDT.
  SmallVector<Value> deps(capturedDbAcquires.begin(), capturedDbAcquires.end());

  /// Step 3: Create the continuation arts.edt AFTER the epoch.
  /// It needs to be after the epoch because the deps (DB acquires) are
  /// defined before the epoch, and the continuation EDT semantically
  /// executes after the epoch completes.
  builder.setInsertionPointAfter(epochOp);

  auto edtOp = builder.create<EdtOp>(loc, EdtType::task,
                                     EdtConcurrency::intranode, deps);

  /// Mark continuation EDT with control dependency attribute.
  edtOp->setAttr(kHasControlDep,
                 builder.getIntegerAttr(builder.getI32Type(), 1));
  /// Mark as continuation for epoch linkage.
  edtOp->setAttr(kContinuationForEpoch, builder.getUnitAttr());

  /// Step 4: Populate the EDT body region.
  Block &edtBlock = edtOp.getBody().front();
  builder.setInsertionPointToStart(&edtBlock);

  /// Build value mapping: old DB acquire values -> EDT block arguments.
  IRMapping valueMapping;
  for (auto [oldDep, blockArg] : llvm::zip(deps, edtBlock.getArguments()))
    valueMapping.map(oldDep, blockArg);

  /// Step 5: Clone tail operations into the EDT body.
  for (Operation *op : tailOps) {
    Operation *cloned = builder.clone(*op, valueMapping);
    for (auto [oldRes, newRes] :
         llvm::zip(op->getResults(), cloned->getResults()))
      valueMapping.map(oldRes, newRes);
  }

  /// Step 5b: Ensure the EDT body has an arts.yield terminator.
  /// The EdtOp builder creates the block without a terminator, and
  /// SingleBlockImplicitTerminator may or may not have added one.
  if (edtBlock.empty() || !edtBlock.back().hasTrait<OpTrait::IsTerminator>())
    builder.create<YieldOp>(loc);

  /// Step 6: Remove the original tail operations from the parent block.
  /// Erase in reverse order to avoid use-before-def issues.
  for (Operation *op : llvm::reverse(tailOps))
    op->erase();

  /// Step 7: Mark the epoch for continuation wiring by EpochLowering.
  epochOp->setAttr(kContinuationForEpoch, builder.getUnitAttr());

  ARTS_INFO("  Created continuation EDT with " << deps.size()
                                               << " DB deps + 1 control dep");
  return success();
}

//===----------------------------------------------------------------------===//
// Pass Registration
//===----------------------------------------------------------------------===//

std::unique_ptr<Pass> mlir::arts::createEpochContinuationPrepPass() {
  return std::make_unique<EpochContinuationPrepPass>();
}
