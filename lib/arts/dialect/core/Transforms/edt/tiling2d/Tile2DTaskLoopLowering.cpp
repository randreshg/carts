///==========================================================================///
/// File: Tile2DTaskLoopLowering.cpp
/// Impl: Tile2DTaskLoopLowering
///
/// Matmul-oriented task loop lowering.
/// This path keeps ownership-safe row partitioning while applying a dedicated
/// contiguous column-slice rewrite inside each task for matmul-like loop
/// bodies.
///
/// Example transformation (post-clone contiguous slicing):
///   Before:
///     scf.for %j = %c0 to %NJ step %c1 {
///       memref.store %v, %C[%globalI, %j] : memref<?x?xf32>
///     }
///
///   After:
///     %colChunk = ceildiv(%NJ, %colWorkers)
///     %jLowerLane = %c0 + (%colWorkerId * %colChunk)
///     %jUpperLane = min(%NJ, %jLowerLane + %colChunk)
///     scf.for %j = %jLowerLane to %jUpperLane step %c1 {
///       memref.store %v, %C[%globalI, %j] : memref<?x?xf32>
///     }
///==========================================================================///

#include "arts/utils/ValueAnalysis.h"
#include "arts/dialect/core/Transforms/edt/EdtTaskLoopLowering.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/ADT/STLExtras.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool usesAsColumnOfRow(Value idx0, Value idx1, Value rowIdx, Value iv) {
  return rowIdx && iv && ValueAnalysis::dependsOn(idx0, rowIdx) &&
         ValueAnalysis::dependsOn(idx1, iv);
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

static std::pair<Value, Value>
computeColumnLaneBounds(TaskLoopPostCloneInput &input, scf::ForOp loop) {
  ArtsCodegen *AC = input.AC;
  if (!AC || !loop || !input.innerStripeLane || !input.innerStripeCount)
    return {};

  OpBuilder &builder = AC->getBuilder();
  OpBuilder::InsertionGuard guard(builder);
  Location loc = loop.getLoc();
  builder.setInsertionPoint(loop);
  Value one = AC->createIndexConstant(1, loc);

  scf::ForOp domainLoop = loop;
  for (Operation *parent = loop->getParentOp(); parent;
       parent = parent->getParentOp()) {
    auto parentLoop = dyn_cast<scf::ForOp>(parent);
    if (!parentLoop)
      continue;
    if (!ValueAnalysis::dependsOn(loop.getLowerBound(),
                                  parentLoop.getInductionVar()) &&
        !ValueAnalysis::dependsOn(loop.getUpperBound(),
                                  parentLoop.getInductionVar()))
      continue;
    domainLoop = parentLoop;
  }

  Value domainLower = domainLoop.getLowerBound();
  Value domainUpper = domainLoop.getUpperBound();
  Value domainExtent =
      arith::SubIOp::create(builder, loc, domainUpper, domainLower);
  Value workersMinusOne =
      arith::SubIOp::create(builder, loc, input.innerStripeCount, one);
  Value adjustedExtent =
      arith::AddIOp::create(builder, loc, domainExtent, workersMinusOne);
  Value laneChunk = arith::DivUIOp::create(builder, loc, adjustedExtent,
                                           input.innerStripeCount);
  Value laneOffset =
      arith::MulIOp::create(builder, loc, input.innerStripeLane, laneChunk);
  Value laneLower = arith::AddIOp::create(builder, loc, domainLower, laneOffset);
  Value laneUpperHint =
      arith::AddIOp::create(builder, loc, laneLower, laneChunk);
  Value laneUpper =
      arith::MinUIOp::create(builder, loc, laneUpperHint, domainUpper);
  return {laneLower, laneUpper};
}

static void sliceLoopByColumns(TaskLoopPostCloneInput &input, scf::ForOp loop) {
  ArtsCodegen *AC = input.AC;
  if (!AC || !loop || !input.innerStripeLane || !input.innerStripeCount)
    return;

  if (!loop.getInitArgs().empty() || loop.getNumResults() != 0)
    return;

  OpBuilder &builder = AC->getBuilder();
  OpBuilder::InsertionGuard guard(builder);
  Location loc = loop.getLoc();
  auto [laneLower, laneUpper] = computeColumnLaneBounds(input, loop);
  if (!laneLower || !laneUpper)
    return;
  builder.setInsertionPoint(loop);
  Value slicedLower =
      arith::MaxUIOp::create(builder, loc, loop.getLowerBound(), laneLower);
  Value slicedUpper =
      arith::MinUIOp::create(builder, loc, loop.getUpperBound(), laneUpper);

  scf::ForOp sliced =
      scf::ForOp::create(builder, loc, slicedLower, slicedUpper, loop.getStep());
  sliced->setAttrs(loop->getAttrs());

  IRMapping map;
  map.map(loop.getInductionVar(), sliced.getInductionVar());
  builder.setInsertionPointToStart(sliced.getBody());
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
    result.tiling2DGrid = WorkDistributionUtils::getTiling2DWorkerGrid(
        input.AC, input.loc, input.taskWorkerId, input.totalWorkers,
        input.distributionContract, input.workersPerNode);
    return result;
  }

  TaskLoopLoweringResult
  lower(TaskLoopLoweringInput &input,
        const TaskLoopLoweringMappedValues &mapped) const override {
    Tiling2DWorkerGrid grid = WorkDistributionUtils::getTiling2DWorkerGrid(
        input.AC, input.loc, input.taskWorkerId, input.totalWorkers,
        input.distributionContract, input.workersPerNode);

    TaskLoopLoweringResult result = lowerBlockStyle(input, mapped);
    result.innerStripeLane = grid.colWorkerId;
    result.innerStripeCount = grid.colWorkers;

    return result;
  }

  void postCloneAdjust(TaskLoopPostCloneInput &input) const override {
    if (!input.AC || !input.iterLoop || !input.globalIter ||
        !input.innerStripeLane || !input.innerStripeCount)
      return;

    auto colWorkersConst =
        ValueAnalysis::tryFoldConstantIndex(input.innerStripeCount);
    if (!colWorkersConst || *colWorkersConst <= 1)
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
      sliceLoopByColumns(input, loop);
    }
  }
};

} // namespace

namespace mlir {
namespace arts {
namespace detail {

namespace {

/// Block-style lowering that reuses the shared EdtTaskLoopLowering helpers.
class BlockTaskLoopLoweringImpl : public EdtTaskLoopLowering {
public:
  TaskLoopLoweringResult
  lower(TaskLoopLoweringInput &input,
        const TaskLoopLoweringMappedValues &mapped) const override {
    return lowerBlockStyle(input, mapped);
  }
};

} // namespace

std::unique_ptr<EdtTaskLoopLowering> createBlockTaskLoopLowering() {
  return std::make_unique<BlockTaskLoopLoweringImpl>();
}

std::unique_ptr<EdtTaskLoopLowering> createBlockCyclicTaskLoopLowering() {
  return std::make_unique<BlockTaskLoopLoweringImpl>();
}

std::unique_ptr<EdtTaskLoopLowering> createTile2DTaskLoopLowering() {
  return std::make_unique<Tile2DTaskLoopLowering>();
}

} // namespace detail
} // namespace arts
} // namespace mlir
