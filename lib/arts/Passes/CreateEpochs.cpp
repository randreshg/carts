///==========================================================================
/// File: CreateEpochs.cpp
///==========================================================================

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Location.h"
#include "mlir/Support/LLVM.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"
/// Debug
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>

#define DEBUG_TYPE "create-epochs"
#define LINE "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::arts;

/// Helper function to process synchronous EDT ops.
static void processSyncEdtOp(arts::EdtOp op) {
  /// Only process EDT ops with sync attribute.
  if (!op.isSync())
    return;

  auto loc = op.getLoc();
  OpBuilder builder(op);
  auto epochOp = builder.create<arts::EpochOp>(loc);
  auto &epochBlock = epochOp.getBody().emplaceBlock();
  builder.setInsertionPointToEnd(&epochBlock);
  builder.create<arts::YieldOp>(loc);

  /// Move the EDT op to the end of the new block.
  op->moveBefore(&epochBlock, --epochBlock.end());

  /// Remove the sync attribute and mark the op as a task.
  op.clearIsSyncAttr();
  op.setIsTaskAttr();
}

/// Helper function to process barrier ops
static void processBarrierOp(arts::BarrierOp barrier) {
  LLVM_DEBUG(dbgs() << "Processing BarrierOp\n");
  auto loc = barrier.getLoc();

  bool hasParentEdt = true;
  Operation *parentOp = barrier->getParentOfType<arts::EdtOp>();
  if (!parentOp) {
    parentOp = barrier->getParentOfType<func::FuncOp>();
    hasParentEdt = false;
  }

  /// Determine the appropriate parent insertion point.
  Region *parentIP = nullptr;
  parentOp->walk([&](arts::EdtOp childEdt) {
    /// Check if the childEDT is a direct child of the parentOp when necessary.
    if (hasParentEdt && (childEdt->getParentOfType<arts::EdtOp>() != parentOp))
      return;

    /// Skip the childEDT if it cannot reach the barrier.
    if (!isReachable(childEdt, barrier))
      return;

    /// Initialize or update the parent insertion point.
    if (!parentIP)
      parentIP = childEdt->getParentRegion();
    else {
      while (parentIP && !parentIP->isAncestor(childEdt->getParentRegion()))
        parentIP = parentIP->getParentRegion();
    }
  });
  assert(parentIP && "Parent insertion point cannot be null");

  /// Collect operations preceding the barrier to avoid iterator invalidation.
  bool finishCollection = false;
  SmallVector<Operation *, 8> opsToMove;
  for (Block &block : *parentIP) {
    for (Operation &op : block) {
      if (&op == barrier) {
        finishCollection = true;
        break;
      }
      opsToMove.push_back(&op);
    }
    if (finishCollection)
      break;
  }

  /// Create a new epoch op and prepare its region.
  OpBuilder builder(parentIP);
  auto epochOp = builder.create<arts::EpochOp>(loc);
  auto &epochRegion = epochOp.getRegion();
  if (epochRegion.empty())
    epochRegion.push_back(new Block());
  Block *newBlock = &epochRegion.front();

  /// Move collected operations into the new epoch block.
  for (Operation *op : opsToMove) {
    LLVM_DEBUG(dbgs() << "Moving operation: " << *op << "\n");
    op->moveBefore(newBlock, newBlock->end());
  }

  /// Finalize epoch block by inserting a yield op.
  builder.setInsertionPointToEnd(newBlock);
  builder.create<arts::YieldOp>(loc);

  /// Remove the barrier op.
  barrier.erase();
}

///==========================================================================
// Pass Implementation
///==========================================================================
namespace {
struct CreateEpochsPass : public arts::CreateEpochsBase<CreateEpochsPass> {
  void runOnOperation() override;
};
} // end namespace


void CreateEpochsPass::runOnOperation() {
  ModuleOp module = getOperation();
  LLVM_DEBUG({
    dbgs() << LINE << "CreateEpochPass STARTED\n" << LINE;
    module.dump();
  });

  /// Process Sync EDT Ops: for each EDT op that is sync, create an epoch op
  /// and move the EDT op inside the epoch op.
  module.walk([](arts::EdtOp op) { processSyncEdtOp(op); });

  /// Process Barrier Ops: for each barrier, collect all EDTs that are affected
  /// by the barrier and embed them in a new epoch op.
  module.walk([&](arts::BarrierOp barrier) { processBarrierOp(barrier); });

  LLVM_DEBUG({
    dbgs() << LINE << "CreateEpochPass FINISHED\n" << LINE;
    module.dump();
  });
}

//==========================================================================
/// Pass creation
//==========================================================================
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCreateEpochsPass() {
  return std::make_unique<CreateEpochsPass>();
}
} // namespace arts
} // namespace mlir
