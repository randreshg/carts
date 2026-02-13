///==========================================================================///
/// File: Tile2DTaskLoopLowering.cpp
/// Impl: Tile2DTaskLoopLowering
///
/// Matmul-oriented task loop lowering.
/// This path keeps ownership-safe row partitioning while applying a dedicated
/// column-striping rewrite inside each task for matmul-like loop bodies.
///
/// Example transformation (post-clone striping):
///   Before:
///     scf.for %j = %c0 to %NJ step %c1 {
///       memref.store %v, %C[%globalI, %j] : memref<?x?xf32>
///     }
///
///   After:
///     %jLowerLane = %c0 + (%colWorkerId * %c1)
///     %jStepLane = %c1 * %colWorkers
///     scf.for %j = %jLowerLane to %NJ step %jStepLane {
///       memref.store %v, %C[%globalI, %j] : memref<?x?xf32>
///     }
///==========================================================================///

#include "arts/Transforms/Edt/EdtTaskLoopLowering.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/ADT/STLExtras.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool usesAsColumnOfRow(Value idx0, Value idx1, Value rowIdx, Value iv) {
  return rowIdx && iv && ValueUtils::dependsOn(idx0, rowIdx) &&
         ValueUtils::dependsOn(idx1, iv);
}

static bool isColumnLoopForGlobalIter(scf::ForOp loop, Value globalIter) {
  if (!loop || !globalIter)
    return false;
  if (!loop.getInitArgs().empty() || loop.getNumResults() != 0)
    return false;

  Value iv = loop.getInductionVar();
  bool matches = false;
  loop.walk([&](Operation *op) {
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      ValueRange idx = store.getIndices();
      if (idx.size() >= 2 &&
          usesAsColumnOfRow(idx[0], idx[1], globalIter, iv)) {
        matches = true;
        return WalkResult::interrupt();
      }
    }
    return WalkResult::advance();
  });
  return matches;
}

static void stripeLoopByColumns(TaskLoopPostCloneInput &input,
                                scf::ForOp loop) {
  ArtsCodegen *AC = input.AC;
  if (!AC || !loop || !input.innerStripeLane || !input.innerStripeCount)
    return;

  if (!loop.getInitArgs().empty() || loop.getNumResults() != 0)
    return;

  OpBuilder &builder = AC->getBuilder();
  OpBuilder::InsertionGuard guard(builder);
  Location loc = loop.getLoc();
  builder.setInsertionPoint(loop);
  Value lane = input.innerStripeLane;
  Value step = loop.getStep();
  Value laneOffset = builder.create<arith::MulIOp>(loc, lane, step);
  Value stripedLower =
      builder.create<arith::AddIOp>(loc, loop.getLowerBound(), laneOffset);
  Value stripedStep =
      builder.create<arith::MulIOp>(loc, step, input.innerStripeCount);

  scf::ForOp striped = builder.create<scf::ForOp>(
      loc, stripedLower, loop.getUpperBound(), stripedStep);
  striped->setAttrs(loop->getAttrs());

  IRMapping map;
  map.map(loop.getInductionVar(), striped.getInductionVar());
  builder.setInsertionPointToStart(striped.getBody());
  for (Operation &op : loop.getBody()->without_terminator())
    builder.clone(op, map);

  loop.erase();
}

class Tile2DTaskLoopLowering final : public EdtTaskLoopLowering {
public:
  TaskAcquirePlanningResult
  planAcquireRewrite(const TaskLoopLoweringInput &input,
                     Value chunkOffset) const override {
    TaskAcquirePlanningResult result =
        EdtTaskLoopLowering::planAcquireRewrite(input, chunkOffset);
    result.tiling2DGrid = DistributionHeuristics::getTiling2DWorkerGrid(
        input.AC, input.loc, input.taskWorkerId, input.totalWorkers);
    return result;
  }

  TaskLoopLoweringResult
  lower(TaskLoopLoweringInput &input,
        const TaskLoopLoweringMappedValues &mapped) const override {
    Tiling2DWorkerGrid grid = DistributionHeuristics::getTiling2DWorkerGrid(
        input.AC, input.loc, input.taskWorkerId, input.totalWorkers);

    TaskLoopLoweringResult result = lowerBlockStyle(input, mapped);
    result.innerStripeLane = grid.colWorkerId;
    result.innerStripeCount = grid.colWorkers;

    return result;
  }

  void postCloneAdjust(TaskLoopPostCloneInput &input) const override {
    if (!input.AC || !input.iterLoop || !input.globalIter ||
        !input.innerStripeLane || !input.innerStripeCount)
      return;

    auto stripeConst = ValueUtils::tryFoldConstantIndex(input.innerStripeCount);
    if (!stripeConst || *stripeConst <= 1)
      return;

    SmallVector<scf::ForOp, 8> columnLoops;
    input.iterLoop.walk([&](scf::ForOp nested) {
      if (nested == input.iterLoop)
        return;
      if (isColumnLoopForGlobalIter(nested, input.globalIter))
        columnLoops.push_back(nested);
    });

    for (scf::ForOp loop : llvm::reverse(columnLoops)) {
      if (!loop || !loop->getBlock())
        continue;
      stripeLoopByColumns(input, loop);
    }
  }
};

} // namespace

std::unique_ptr<EdtTaskLoopLowering>
mlir::arts::detail::createTile2DTaskLoopLowering() {
  return std::make_unique<Tile2DTaskLoopLowering>();
}
