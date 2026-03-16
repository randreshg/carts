///==========================================================================///
/// Epoch Lowering Pass - Complete Implementation
/// Transforms arts.epoch operations into CreateEpochOp and WaitOnEpochOp
/// and propagates epoch GUIDs to contained EdtCreateOps
///
/// This pass runs after EdtLowering and implements the epoch lowering process:
/// 1. Create CreateEpochOp for each epoch operation
/// 2. Propagate the epoch GUID to all EdtCreateOps within the epoch
/// 3. Lower epoch operations to CreateEpochOp + operations + WaitOnEpochOp
/// 4. Return the epoch GUID for synchronization
///
/// When an epoch has been marked for continuation (by EpochContinuationPrep),
/// the pass wires the epoch's finish target to the continuation EDT and
/// skips emitting WaitOnEpochOp.
///
/// Example (standard path):
///   Before:
///     arts.epoch { ... arts.edt_create ... }
///
///   After:
///     %e = arts.create_epoch
///     ... arts.edt_create(..., %e) ...
///     arts.wait_on_epoch %e
///
/// Example (continuation path):
///   Before:
///     arts.epoch { ... arts.edt_create ... } {arts.continuation_for_epoch}
///     %cont = arts.edt_create(...) {arts.continuation_for_epoch, ...}
///
///   After:
///     %cont = arts.edt_create(...)
///     %e = arts.create_epoch finish(%cont_guid, %control_slot)
///     ... arts.edt_create(..., %e) ...
///     // NO wait_on_epoch
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/codegen/Codegen.h"
#include "arts/passes/PassDetails.h"
#include "arts/passes/Passes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(epoch_lowering);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

namespace {
constexpr llvm::StringLiteral kContinuationForEpoch =
    "arts.continuation_for_epoch";
} // namespace

///===----------------------------------------------------------------------===///
/// Epoch Lowering Pass Implementation
///===----------------------------------------------------------------------===///
struct EpochLoweringPass : public arts::EpochLoweringBase<EpochLoweringPass> {
  explicit EpochLoweringPass(bool debug = false) : debugMode(debug) {}

  void runOnOperation() override;

private:
  /// State
  ModuleOp module;
  ArtsCodegen *AC = nullptr;
  bool debugMode = false;
};

///===----------------------------------------------------------------------===///
/// Pass Implementation
///===----------------------------------------------------------------------===///

void EpochLoweringPass::runOnOperation() {
  module = getOperation();
  auto ownedAC = std::make_unique<ArtsCodegen>(module, debugMode);
  AC = ownedAC.get();

  ARTS_INFO_HEADER(EpochLoweringPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Collect all epoch operations bottom-to-top (post-order) so inner epochs
  /// are lowered before their parents.
  SmallVector<EpochOp> epochOps;
  module.walk<WalkOrder::PostOrder>(
      [&](EpochOp epochOp) { epochOps.push_back(epochOp); });

  ARTS_INFO("Found " << epochOps.size() << " epoch operations to lower");

  /// Process each epoch operation
  for (EpochOp epochOp : epochOps) {
    ARTS_INFO("Lowering Epoch Op " << epochOp);

    /// If region is empty or contains only a terminator, elide the epoch
    auto &epochRegion = epochOp.getRegion();
    if (epochRegion.empty() ||
        epochRegion.front().without_terminator().empty()) {
      epochOp.erase();
      continue;
    }

    /// Check if this epoch has a continuation EDT (set by
    /// EpochContinuationPrep).
    bool hasContinuation = epochOp->hasAttr(kContinuationForEpoch);
    Value finishGuid;
    Value finishSlot;

    if (hasContinuation) {
      /// Find the continuation EdtCreateOp after this epoch.
      /// EpochContinuationPrep placed the continuation arts.edt after the
      /// epoch, and EdtLowering has since lowered it to an EdtCreateOp.
      EdtCreateOp contEdtCreate = nullptr;
      for (Operation *op = epochOp->getNextNode(); op; op = op->getNextNode()) {
        if (auto edt = dyn_cast<EdtCreateOp>(op)) {
          if (edt->hasAttr(kContinuationForEpoch)) {
            contEdtCreate = edt;
            break;
          }
        }
      }

      if (contEdtCreate) {
        /// Move the continuation EdtCreateOp and its operand-defining ops
        /// (e.g., edt_param_pack) to BEFORE the epoch. This ensures
        /// CreateEpochOp can reference finishGuid/finishSlot while still
        /// being placed before the worker EDTs inside the epoch.
        for (Value operand : contEdtCreate->getOperands()) {
          if (auto *defOp = operand.getDefiningOp())
            if (defOp->getBlock() == epochOp->getBlock())
              defOp->moveBefore(epochOp);
        }
        contEdtCreate->moveBefore(epochOp);

        finishGuid = contEdtCreate.getGuid();
        /// The control slot is the LAST slot: depCount - 1 (the control dep
        /// was added as +1 to depCount by EdtLowering).
        AC->setInsertionPointAfter(contEdtCreate);
        Value depCount = contEdtCreate.getDepCount();
        Value one = AC->createIntConstant(1, AC->Int32, epochOp.getLoc());
        finishSlot = AC->create<arith::SubIOp>(epochOp.getLoc(), depCount, one);
        ARTS_INFO("  Continuation path: finish GUID from "
                  << contEdtCreate << ", control slot = depCount - 1");
      } else {
        ARTS_WARN("  Epoch marked for continuation but no continuation "
                  "EdtCreateOp found; falling back to wait path");
        hasContinuation = false;
      }
    }

    /// Create CreateEpochOp for the epoch (placed just before the epoch).
    AC->setInsertionPoint(epochOp);
    auto createEpochOp = AC->create<CreateEpochOp>(
        epochOp.getLoc(), IntegerType::get(AC->getContext(), 64),
        hasContinuation ? finishGuid : Value(),
        hasContinuation ? finishSlot : Value());
    auto currentEpoch = createEpochOp.getEpochGuid();

    /// Find all EdtCreateOp operations within this epoch region and update them
    /// with the epoch GUID
    SmallVector<EdtCreateOp, 8> edtCreatesToUpdate;
    epochOp.walk([&](EdtCreateOp edtCreateOp) {
      /// Only update EdtCreateOp operations that don't already have an epoch
      /// GUID
      if (!edtCreateOp.getEpochGuid())
        edtCreatesToUpdate.push_back(edtCreateOp);
    });

    ARTS_INFO("Updating " << edtCreatesToUpdate.size()
                          << " EdtCreateOps with epoch GUID");

    /// Update all EdtCreateOps in this epoch with the epoch GUID
    for (EdtCreateOp edtCreateOp : edtCreatesToUpdate) {
      AC->setInsertionPoint(edtCreateOp);

      /// Build new EdtCreateOp with epoch GUID
      auto newEdtCreateOp = AC->create<EdtCreateOp>(
          edtCreateOp.getLoc(), edtCreateOp.getParamMemref(),
          edtCreateOp.getDepCount(), edtCreateOp.getRoute(), currentEpoch);

      /// Copy attributes from the old operation
      for (auto attr : edtCreateOp->getAttrs())
        newEdtCreateOp->setAttr(attr.getName(), attr.getValue());

      /// Replace all uses of the old EdtCreateOp with the new one
      edtCreateOp->replaceAllUsesWith(newEdtCreateOp);

      /// Replace the old EdtCreateOp
      edtCreateOp->erase();
    }

    /// Move operations out of epoch region to before the epoch, but keep
    /// track of where the epoch originally ended so we can insert the wait
    /// after all of its contents (respecting original ordering).
    auto &epochRegionForMove = epochOp.getRegion();
    Operation *insertionAfter = epochOp.getOperation();
    if (!epochRegionForMove.empty()) {
      auto &epochBlock = epochRegionForMove.front();
      SmallVector<Operation *> opsToMove;
      for (auto &innerOp : epochBlock.without_terminator()) {
        if (!isa<EpochOp>(innerOp))
          opsToMove.push_back(&innerOp);
      }
      for (auto *opToMove : opsToMove) {
        opToMove->moveBefore(epochOp);
        insertionAfter = opToMove;
      }
    }

    /// Insert wait on handle ONLY for non-continuation epochs.
    if (!hasContinuation) {
      AC->setInsertionPointAfter(insertionAfter);
      AC->create<WaitOnEpochOp>(epochOp.getLoc(), currentEpoch);
    } else {
      ARTS_INFO("  Skipping WaitOnEpochOp (continuation path)");
    }

    /// Replace the epoch operation with its result (the epoch GUID)
    epochOp.replaceAllUsesWith(currentEpoch);
    epochOp.erase();
  }

  ARTS_INFO_FOOTER(EpochLoweringPass);
  AC = nullptr;
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Pass Registration
///===----------------------------------------------------------------------===///

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createEpochLoweringPass() {
  return std::make_unique<EpochLoweringPass>();
}

} // namespace arts
} // namespace mlir
