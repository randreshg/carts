///==========================================================================///
/// File: EpochContinuationPrep.cpp
///
/// Epoch Continuation Preparation Pass - transforms epoch+tail patterns into
/// finish-EDT continuation scheduling when safe.
///
/// This pass runs AFTER CreateEpochs but BEFORE the pre-lowering pipeline
/// (EdtLowering, EpochLowering, etc.). It detects safe "epoch + tail" patterns
/// and outlines the tail into a continuation arts.edt, allowing the epoch to
/// use ARTS-native finish-EDT signaling instead of blocking waits.
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
#include "arts/utils/OperationAttributes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Support/Debug.h"

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

} // namespace

void EpochContinuationPrepPass::runOnOperation() {
  ModuleOp module = getOperation();

  ARTS_INFO_HEADER(EpochContinuationPrepPass);

  /// Collect all epoch ops that have tail code after them.
  SmallVector<EpochOp> epochOps;
  module.walk([&](EpochOp epochOp) { epochOps.push_back(epochOp); });

  ARTS_INFO("Found " << epochOps.size() << " epoch operations to analyze");

  unsigned transformed = 0;
  for (EpochOp epochOp : epochOps) {
    SmallVector<Operation *> tailOps;
    if (!isEligible(epochOp, tailOps)) {
      ARTS_DEBUG("  Epoch not eligible for continuation");
      continue;
    }

    ARTS_INFO("  Transforming eligible epoch to continuation form");
    if (succeeded(transformToContinuation(epochOp, tailOps)))
      ++transformed;
  }

  ARTS_INFO("Transformed " << transformed << " epochs to continuation form");
  ARTS_INFO_FOOTER(EpochContinuationPrepPass);
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

  /// Rule 7: The epoch must not already be marked for continuation.
  if (epochOp->hasAttr(kContinuationForEpoch))
    return false;

  /// Rule 8: The block terminator must not use any values defined by tail ops.
  /// If it does (e.g., `return %result` where %result is from a tail load),
  /// we can't outline the tail without also handling the terminator's liveness.
  DenseSet<Operation *> tailOpSet(tailOps.begin(), tailOps.end());
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
  SmallVector<Value> deps(capturedDbAcquires.begin(),
                          capturedDbAcquires.end());

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
