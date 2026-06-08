///==========================================================================///
/// Epoch Lowering Pass
/// Transforms arts.epoch into CreateEpochOp + WaitOnEpochOp, or wires a proven
/// tail EDT continuation as the epoch finish target. Propagates epoch GUIDs to
/// contained EdtCreateOps.
///
/// Example (standard path):
///   Before:
///     arts.epoch { ... arts.edt_create ... }
///
///   After:
///     %e = arts.create_epoch
///     ... arts.edt_create(..., %e) ...
///     arts.wait_on_epoch %e
///==========================================================================///

#include "carts/dialect/arts-rt/Transforms/Passes.h"
namespace mlir::carts::arts_rt {
#define GEN_PASS_DEF_EPOCHLOWERING
#include "carts/dialect/arts-rt/Transforms/Passes.h.inc"
} // namespace mlir::carts::arts_rt
#include "../ArtsRtToLLVM/CodegenInternal.h"
#include "carts/dialect/arts-rt/IR/RtDialect.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/passes/Passes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/Matchers.h"
#include "mlir/Support/LLVM.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

#include "carts/utils/Debug.h"
ARTS_DEBUG_SETUP(epoch_lowering);

#include "llvm/ADT/Statistic.h"
static llvm::Statistic numEpochsLowered{
    "epoch_lowering", "NumEpochsLowered",
    "Number of epoch operations lowered to CreateEpochOp + WaitOnEpochOp"};
static llvm::Statistic numEmptyEpochsElided{
    "epoch_lowering", "NumEmptyEpochsElided", "Number of empty epochs elided"};
static llvm::Statistic numEdtCreatesUpdatedWithEpoch{
    "epoch_lowering", "NumEdtCreatesUpdatedWithEpoch",
    "Number of EdtCreateOps updated with epoch GUID"};

using namespace mlir;
using namespace mlir::func;
using namespace mlir::carts;
using namespace mlir::carts::arts;
using namespace mlir::carts::arts_rt;

namespace {

struct TailFinishContinuation {
  SmallVector<Operation *, 4> opsToMove;
  EdtCreateOp continuation;
  Value finishSlot;
};

static bool isAllowedContinuationSetupOp(Operation *op) {
  return isa<EdtParamPackOp>(op) || op->hasTrait<OpTrait::ConstantLike>();
}

static bool
operandsDominateEpoch(Operation *op, Operation *epochOp,
                      const llvm::SmallDenseSet<Operation *, 4> &opsToMove) {
  Block *block = epochOp->getBlock();
  for (Value operand : op->getOperands()) {
    Operation *def = operand.getDefiningOp();
    if (!def || opsToMove.contains(def))
      continue;
    if (def->getBlock() == block && !def->isBeforeInBlock(epochOp))
      return false;
  }
  return true;
}

static FailureOr<TailFinishContinuation>
findTailFinishContinuation(EpochOp epochOp) {
  Block *block = epochOp->getBlock();
  if (!block)
    return failure();

  TailFinishContinuation candidate;
  auto it = std::next(Block::iterator(epochOp.getOperation()));
  for (; it != block->end(); ++it) {
    Operation *op = &*it;
    if (op->hasTrait<OpTrait::IsTerminator>())
      return failure();

    if (auto continuation = dyn_cast<EdtCreateOp>(op)) {
      if (continuation.getEpochGuid())
        return failure();
      if (!matchPattern(continuation.getDepCount(), m_Zero()))
        return failure();
      candidate.continuation = continuation;
      candidate.finishSlot = continuation.getDepCount();
      candidate.opsToMove.push_back(op);
      ++it;
      break;
    }

    if (!isAllowedContinuationSetupOp(op))
      return failure();
    candidate.opsToMove.push_back(op);
  }

  if (!candidate.continuation)
    return failure();

  for (; it != block->end(); ++it) {
    auto returnOp = dyn_cast<func::ReturnOp>(&*it);
    if (!returnOp)
      return failure();
    if (returnOp.getNumOperands() != 0)
      return failure();
  }

  llvm::SmallDenseSet<Operation *, 4> opsToMoveSet(candidate.opsToMove.begin(),
                                                   candidate.opsToMove.end());
  for (Operation *op : candidate.opsToMove)
    if (!operandsDominateEpoch(op, epochOp.getOperation(), opsToMoveSet))
      return failure();

  return candidate;
}

static TailFinishContinuation
moveTailFinishContinuationBeforeEpoch(EpochOp epochOp,
                                      TailFinishContinuation continuation) {
  for (Operation *op : continuation.opsToMove)
    op->moveBefore(epochOp);

  OpBuilder builder(continuation.continuation);
  Value one = arith::ConstantIntOp::create(
      builder, continuation.continuation.getLoc(), 1, 32);
  continuation.continuation.getDepCountMutable().set(one);
  return continuation;
}

} // namespace

///===----------------------------------------------------------------------===///
/// Epoch Lowering Pass Implementation
///===----------------------------------------------------------------------===///
struct EpochLoweringPass
    : public arts_rt::impl::EpochLoweringBase<EpochLoweringPass> {
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

  for (EpochOp epochOp : epochOps) {
    ARTS_INFO("Lowering Epoch Op " << epochOp);

    /// Elide empty epochs.
    auto &epochRegion = epochOp.getRegion();
    if (epochRegion.empty() ||
        epochRegion.front().without_terminator().empty()) {
      ++numEmptyEpochsElided;
      epochOp.erase();
      continue;
    }

    FailureOr<TailFinishContinuation> tailContinuation =
        findTailFinishContinuation(epochOp);
    bool hasTailContinuation = succeeded(tailContinuation);
    if (hasTailContinuation)
      *tailContinuation =
          moveTailFinishContinuationBeforeEpoch(epochOp, *tailContinuation);

    /// Create the CreateEpochOp.
    AC->setInsertionPoint(epochOp);
    Value finishEdtGuid = hasTailContinuation
                              ? tailContinuation->continuation.getGuid()
                              : Value();
    Value finishSlot =
        hasTailContinuation ? tailContinuation->finishSlot : Value();
    auto createEpochOp = AC->create<CreateEpochOp>(
        epochOp.getLoc(), IntegerType::get(AC->getContext(), 64), finishEdtGuid,
        finishSlot);
    auto currentEpoch = createEpochOp.getEpochGuid();

    /// Collect EdtCreateOps that need the epoch GUID.
    SmallVector<EdtCreateOp, 8> edtCreatesToUpdate;
    epochOp.walk([&](EdtCreateOp edtCreateOp) {
      if (!edtCreateOp.getEpochGuid())
        edtCreatesToUpdate.push_back(edtCreateOp);
    });

    ARTS_INFO("Updating " << edtCreatesToUpdate.size()
                          << " EdtCreateOps with epoch GUID");

    numEdtCreatesUpdatedWithEpoch += edtCreatesToUpdate.size();
    for (EdtCreateOp edtCreateOp : edtCreatesToUpdate) {
      AC->setInsertionPoint(edtCreateOp);
      auto newEdtCreateOp = AC->create<EdtCreateOp>(
          edtCreateOp.getLoc(), edtCreateOp.getParamMemref(),
          edtCreateOp.getDepCount(), edtCreateOp.getRoute(), currentEpoch);
      for (auto attr : edtCreateOp->getAttrs())
        newEdtCreateOp->setAttr(attr.getName(), attr.getValue());
      edtCreateOp->replaceAllUsesWith(newEdtCreateOp);
      edtCreateOp->erase();
    }

    /// Move operations out of the epoch region, tracking where to insert
    /// the wait afterward.
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

    if (!hasTailContinuation) {
      AC->setInsertionPointAfter(insertionAfter);
      AC->create<WaitOnEpochOp>(epochOp.getLoc(), currentEpoch);
    }
    ++numEpochsLowered;

    /// Replace the epoch op with the epoch GUID.
    epochOp.replaceAllUsesWith(currentEpoch);
    epochOp.erase();
  }

  SmallVector<Operation *> duplicateReleases;
  module.walk([&](Operation *op) {
    for (Region &region : op->getRegions()) {
      for (Block &block : region) {
        llvm::SmallDenseSet<Value, 8> releasedValues;
        for (Operation &nested : block) {
          auto release = dyn_cast<DbReleaseOp>(&nested);
          if (!release)
            continue;
          if (!releasedValues.insert(release.getSource()).second)
            duplicateReleases.push_back(release);
        }
      }
    }
  });
  for (Operation *release : duplicateReleases) {
    ARTS_DEBUG("Removing duplicate db_release introduced during epoch "
               "lowering: "
               << *release);
    release->erase();
  }

  ARTS_INFO_FOOTER(EpochLoweringPass);
  AC = nullptr;
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Pass Registration
///===----------------------------------------------------------------------===///

namespace mlir {
namespace carts::arts_rt {

std::unique_ptr<Pass> createEpochLoweringPass() {
  return std::make_unique<EpochLoweringPass>();
}

} // namespace carts::arts_rt
} // namespace mlir
