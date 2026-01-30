///==========================================================================///
/// File: CreateEpochs.cpp
///==========================================================================///

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Location.h"
#include "mlir/Support/LLVM.h"
/// Arts
#include "../ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/EdtUtils.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(create_epochs);

using namespace mlir;
using namespace mlir::arts;

static void clearIsSyncAttr(EdtOp op) {
  auto newTypeAttr = EdtTypeAttr::get(op.getContext(), EdtType::single);
  op.setTypeAttr(newTypeAttr);
}

static void setIsTaskAttr(EdtOp op) {
  auto newTypeAttr = EdtTypeAttr::get(op.getContext(), EdtType::task);
  op.setTypeAttr(newTypeAttr);
}

static bool isInsideEpoch(EdtOp op) {
  return op->getParentOfType<EpochOp>() != nullptr;
}

static void wrapEdtInEpoch(EdtOp op, bool demoteToTask) {
  if (isInsideEpoch(op))
    return;
  auto loc = op.getLoc();
  OpBuilder builder(op);
  auto epochOp = builder.create<EpochOp>(loc);
  auto &epochBlock = epochOp.getBody().emplaceBlock();
  builder.setInsertionPointToEnd(&epochBlock);
  builder.create<YieldOp>(loc);
  op->moveBefore(&epochBlock, --epochBlock.end());

  if (demoteToTask) {
    clearIsSyncAttr(op);
    setIsTaskAttr(op);
  }
}

/// Helper function to process synchronous EDT ops.
static void processSyncEdtOp(EdtOp op) {
  /// Only process EDT ops with sync attribute.
  if (op.getTypeAttr().getValue() != EdtType::sync)
    return;
  ARTS_DEBUG("Processing Sync EDT Op: " << op);
  wrapEdtInEpoch(op, /*demoteToTask=*/true);
}

/// Find the nearest common ancestor region between two regions.
static Region *findCommonRegion(Region *a, Region *b) {
  if (!a)
    return b;
  if (!b)
    return a;

  SmallPtrSet<Region *, 8> ancestors;
  for (Region *it = a; it; it = it->getParentRegion())
    ancestors.insert(it);

  for (Region *it = b; it; it = it->getParentRegion())
    if (ancestors.contains(it))
      return it;
  return nullptr;
}

/// Find the enclosing scope (EDT or function) for a barrier.
static Operation *findBarrierScope(BarrierOp barrier) {
  if (auto edt = barrier->getParentOfType<EdtOp>())
    return edt;
  return barrier->getParentOfType<func::FuncOp>();
}

/// Collect all operations that can reach the barrier within a scope.
static void collectReachableOps(Operation *scope, BarrierOp barrier,
                                SmallVectorImpl<Operation *> &reachableOps) {
  scope->walk([&](Operation *op) {
    if (op == barrier || op->isProperAncestor(barrier) || isa<YieldOp>(op))
      return;
    if (EdtUtils::isReachable(op, barrier))
      reachableOps.push_back(op);
  });
}

/// Filter EDTs from reachable ops that are direct children of parentRegion.
static void collectOpsToMoveIntoEpoch(ArrayRef<Operation *> reachableOps,
                                      Region *parentRegion, BarrierOp barrier,
                                      SmallVectorImpl<Operation *> &opsToMove) {
  DenseSet<Operation *> reachableSet(reachableOps.begin(), reachableOps.end());
  parentRegion->walk([&](Operation *op) {
    if (op == barrier || !reachableSet.contains(op))
      return;
    if (op->getParentRegion() != parentRegion || !isa<arts::EdtOp>(op))
      return;
    opsToMove.push_back(op);
  });
}

static void processBarrierOp(BarrierOp barrier) {
  ARTS_DEBUG("Processing BarrierOp");
  if (!barrier->getBlock())
    return;

  Operation *scope = findBarrierScope(barrier);
  if (!scope)
    return;

  /// Collect operations that can reach this barrier
  SmallVector<Operation *> reachableOps;
  collectReachableOps(scope, barrier, reachableOps);

  /// Find common parent region for all reachable ops
  Region *parentIP = barrier->getParentRegion();
  for (Operation *op : reachableOps)
    parentIP = findCommonRegion(parentIP, op->getParentRegion());
  if (!parentIP)
    parentIP = barrier->getParentRegion();
  if (!parentIP || parentIP->empty())
    return;

  /// Collect EDTs to move into a new epoch
  SmallVector<Operation *, 8> opsToMove;
  collectOpsToMoveIntoEpoch(reachableOps, parentIP, barrier, opsToMove);

  if (opsToMove.empty()) {
    barrier.erase();
    return;
  }

  /// Find insertion point (first op to move)
  Operation *insertionOp = nullptr;
  for (Operation *op : opsToMove) {
    if (op->getParentRegion() == parentIP) {
      insertionOp = op;
      break;
    }
  }

  Block *insertBlock =
      insertionOp ? insertionOp->getBlock() : &parentIP->front();
  if (!insertBlock)
    return;

  /// Create epoch and move operations into it
  Location loc = barrier.getLoc();
  OpBuilder builder(insertBlock, insertionOp ? Block::iterator(insertionOp)
                                             : insertBlock->begin());
  auto epochOp = builder.create<EpochOp>(loc);
  auto &epochRegion = epochOp.getRegion();
  if (epochRegion.empty())
    epochRegion.push_back(new Block());
  Block *newBlock = &epochRegion.front();

  for (Operation *op : opsToMove) {
    if (!op->getBlock())
      continue;
    ARTS_INFO("Moving operation: " << *op);
    op->moveBefore(newBlock, newBlock->end());
  }

  builder.setInsertionPointToEnd(newBlock);
  builder.create<YieldOp>(loc);

  barrier.erase();
}

///==========================================================================///
/// Pass Implementation
///==========================================================================///
namespace {
struct CreateEpochsPass : public CreateEpochsBase<CreateEpochsPass> {
  void runOnOperation() override;
};
} // end namespace

void CreateEpochsPass::runOnOperation() {
  ModuleOp module = getOperation();
  ARTS_INFO_HEADER(CreateEpochsPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Process Sync EDT Ops: for each EDT op that is sync, create an epoch op
  /// and move the EDT op inside the epoch op.
  ARTS_DEBUG_HEADER(ProcessSyncEdtOp);
  module.walk([](EdtOp op) { processSyncEdtOp(op); });
  ARTS_DEBUG_FOOTER(ProcessSyncEdtOp);

  /// Process Barrier Ops: for each barrier, collect all EDTs that are affected
  /// by the barrier and embed them in a new epoch op.
  ARTS_DEBUG_HEADER(ProcessBarrierOp);
  module.walk([&](BarrierOp barrier) { processBarrierOp(barrier); });
  ARTS_DEBUG_FOOTER(ProcessBarrierOp);

  ARTS_INFO_FOOTER(CreateEpochsPass);
  ARTS_DEBUG_REGION(module.dump(););
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
