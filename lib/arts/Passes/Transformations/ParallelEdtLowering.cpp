///==========================================================================///
/// File: ParallelEdtLowering.cpp
///
/// Lower arts.edt<parallel> into a simple worker loop. The loop induction
/// variable replaces arts.get_parallel_worker_id placeholders, worker EDTs are
/// created sequentially, and single EDTs are guarded to run only once
/// (worker==0) within the loop to preserve program order.
///
/// Example:
///   Before:
///     arts.edt <parallel> (...) { ... arts.get_parallel_worker_id ... }
///
///   After:
///     scf.for %wid = 0 to %workers step 1 {
///       arts.edt <task> route(%wid) (...) { ... %wid ... }
///     }
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Analysis/DistributionHeuristics.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/OperationAttributes.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Transforms/RegionUtils.h"

#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;
using namespace mlir::arts;

ARTS_DEBUG_SETUP(parallel_edt_lowering);

namespace {

/// Replace all arts.get_parallel_worker_id operations nested under `op` with
/// the provided worker value.
static void replaceWorkerIds(Operation *op, Value workerId) {
  SmallVector<Operation *> toErase;
  op->walk([&](GetParallelWorkerIdOp idOp) {
    idOp.replaceAllUsesWith(workerId);
    toErase.push_back(idOp.getOperation());
  });
  for (Operation *erase : toErase)
    erase->erase();
}

/// Guard all single EDTs nested under `op` to run only on worker 0.
/// We inline the EDT body directly into the if-block.
static void guardSingleEdts(Operation *op, Value workerId) {
  SmallVector<EdtOp> singleEdts;
  op->walk([&](EdtOp edt) {
    if (edt.getType() == EdtType::single)
      singleEdts.push_back(edt);
  });

  if (singleEdts.empty())
    return;

  OpBuilder builder(op->getContext());
  for (EdtOp edt : singleEdts) {
    builder.setInsertionPoint(edt);
    Location loc = edt.getLoc();
    Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
    Value isZero = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::eq,
                                                 workerId, zero);

    auto ifOp = builder.create<scf::IfOp>(loc, TypeRange{}, isZero,
                                          /*withElseRegion=*/false);

    /// Inline the EDT body
    Block *thenBlock = &ifOp.getThenRegion().front();
    Block &edtBody = edt.getBody().front();

    /// Replace uses of EDT's block arguments with the EDT's dependencies..
    ValueRange deps = edt.getDependencies();
    for (auto [blockArg, dep] : llvm::zip(edtBody.getArguments(), deps)) {
      blockArg.replaceAllUsesWith(dep);
    }

    /// Move all operations (except terminator) from EDT body to if-block
    for (Operation &bodyOp :
         llvm::make_early_inc_range(edtBody.without_terminator())) {
      bodyOp.moveBefore(thenBlock->getTerminator());
    }

    /// Erase the now-empty EDT
    edt.erase();
  }
}

/// Ensure nested EDTs capture external memrefs explicitly as dependencies.
static void ensureNestedEdtCaptures(Operation *op) {
  op->walk([&](EdtOp edt) {
    SetVector<Value> captured;
    getUsedValuesDefinedAbove(edt.getRegion(), captured);
    if (captured.empty())
      return;

    SmallVector<Value> deps(edt.getDependencies().begin(),
                            edt.getDependencies().end());
    Block &edtBlock = edt.getBody().front();
    for (Value capturedVal : captured) {
      auto memrefTy = capturedVal.getType().dyn_cast<MemRefType>();
      if (!memrefTy)
        continue;
      if (llvm::is_contained(deps, capturedVal))
        continue;
      BlockArgument newArg = edtBlock.addArgument(memrefTy, edt.getLoc());
      for (OpOperand &use : llvm::make_early_inc_range(capturedVal.getUses()))
        if (edt->isAncestor(use.getOwner()))
          use.set(newArg);
      deps.push_back(capturedVal);
    }
    edt.setDependencies(deps);
  });
}

/// Route all internode EDTs nested under `op` to the current worker.
static void routeEdtsToWorker(Operation *op, Value workerRoute) {
  if (!workerRoute)
    return;

  op->walk([&](EdtOp edt) {
    if (edt.getConcurrency() != EdtConcurrency::internode)
      return;
    edt.getRouteMutable().assign(workerRoute);
  });
}

class ParallelEdtLoweringPass
    : public arts::ParallelEdtLoweringBase<ParallelEdtLoweringPass> {
public:
  void runOnOperation() override {
    ModuleOp module = getOperation();
    ARTS_INFO_HEADER(ParallelEdtLowering);
    ARTS_DEBUG_REGION(module.dump(););

    SmallVector<EdtOp> parallelEdts;
    module.walk([&](EdtOp edt) {
      if (edt.getType() == EdtType::parallel)
        parallelEdts.push_back(edt);
    });

    ARTS_DEBUG("Found " << parallelEdts.size() << " parallel EDT(s) to lower");

    for (EdtOp edt : parallelEdts) {
      if (failed(lowerParallelEdt(edt))) {
        signalPassFailure();
        return;
      }
    }

    ARTS_INFO_FOOTER(ParallelEdtLowering);
    ARTS_DEBUG_REGION(module.dump(););
  }

private:
  LogicalResult lowerParallelEdt(EdtOp parallelEdt) {
    ARTS_DEBUG("Lowering parallel EDT\n" << parallelEdt);

    Block &body = parallelEdt.getBody().front();

    ValueRange deps = parallelEdt.getDependencies();
    if (body.getNumArguments() != deps.size()) {
      parallelEdt.emitError(
          "dependency/block argument mismatch while lowering parallel EDT");
      return failure();
    }

    Location loc = parallelEdt.getLoc();
    OpBuilder builder(parallelEdt);
    builder.setInsertionPoint(parallelEdt);

    /// Wrap lowered worker loop in an epoch to provide an explicit
    auto epochOp = builder.create<EpochOp>(loc);
    Region &epochRegion = epochOp.getBody();
    if (epochRegion.empty())
      epochRegion.push_back(new Block());
    Block *epochBlock = &epochRegion.front();
    OpBuilder epochBuilder = OpBuilder::atBlockBegin(epochBlock);

    Value numWorkers = DistributionHeuristics::getDispatchWorkerCount(
        epochBuilder, loc, parallelEdt);
    Value zero = epochBuilder.create<arith::ConstantIndexOp>(loc, 0);
    Value one = epochBuilder.create<arith::ConstantIndexOp>(loc, 1);

    bool hasParallelWorkerPlaceholders = false;
    parallelEdt.walk([&](GetParallelWorkerIdOp) {
      hasParallelWorkerPlaceholders = true;
      return WalkResult::interrupt();
    });
    if (!hasParallelWorkerPlaceholders) {
      /// Replicating a parallel EDT body without worker placeholders duplicates
      /// the same work N times. Keep a single task in that case.
      numWorkers = one;
    }

    /// Create a single worker loop to preserve program order
    auto workerLoop =
        epochBuilder.create<scf::ForOp>(loc, zero, numWorkers, one);
    OpBuilder loopBuilder =
        OpBuilder::atBlockBegin(&workerLoop.getRegion().front());
    Value workerId = workerLoop.getInductionVar();

    /// For internode parallelism, map worker lanes to valid node routes.
    Value workerRoute;
    bool routeWorkers =
        hasParallelWorkerPlaceholders &&
        parallelEdt.getConcurrency() == EdtConcurrency::internode;
    if (routeWorkers) {
      Type i32Ty = loopBuilder.getIntegerType(32);
      Value nodes = loopBuilder.create<GetTotalNodesOp>(loc).getResult();
      if (!nodes.getType().isIndex())
        nodes = loopBuilder.create<arith::IndexCastOp>(
            loc, loopBuilder.getIndexType(), nodes);

      Value nodesMinusOne = loopBuilder.create<arith::SubIOp>(loc, nodes, one);
      Value ceilNumerator =
          loopBuilder.create<arith::AddIOp>(loc, numWorkers, nodesMinusOne);
      Value workersPerNode =
          loopBuilder.create<arith::DivUIOp>(loc, ceilNumerator, nodes);
      workersPerNode =
          loopBuilder.create<arith::MaxUIOp>(loc, workersPerNode, one);

      Value nodeId =
          loopBuilder.create<arith::DivUIOp>(loc, workerId, workersPerNode);
      Value outOfRange = loopBuilder.create<arith::CmpIOp>(
          loc, arith::CmpIPredicate::uge, nodeId, nodes);
      nodeId = loopBuilder.create<arith::SelectOp>(loc, outOfRange,
                                                   nodesMinusOne, nodeId);
      workerRoute = nodeId;
      if (workerRoute.getType() != i32Ty)
        workerRoute =
            loopBuilder.create<arith::IndexCastOp>(loc, i32Ty, workerRoute);
    }

    /// Clone the parallel EDT into the worker loop as a task EDT
    Operation *clonedOp = loopBuilder.clone(*parallelEdt.getOperation());
    auto clonedEdt = cast<EdtOp>(clonedOp);
    clonedEdt.setType(EdtType::task);
    clonedEdt->removeAttr(AttrNames::Operation::Workers);
    clonedEdt->removeAttr("nowait");

    /// Update route: internode uses worker route, otherwise keep existing or 0
    Value routeVal = routeWorkers ? workerRoute : clonedEdt.getRoute();
    if (!routeVal)
      routeVal = loopBuilder.create<arith::ConstantIntOp>(loc, 0, 32);
    clonedEdt.getRouteMutable().assign(routeVal);

    replaceWorkerIds(clonedEdt, workerId);
    guardSingleEdts(clonedEdt, workerId);
    ensureNestedEdtCaptures(clonedEdt);
    if (routeWorkers)
      routeEdtsToWorker(clonedEdt, routeVal);

    /// Finalize the epoch region.
    epochBuilder.setInsertionPointToEnd(epochBlock);
    epochBuilder.create<YieldOp>(loc);

    parallelEdt.erase();
    ARTS_DEBUG("  Lowered parallel EDT into worker loop form");
    return success();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::arts::createParallelEdtLoweringPass() {
  return std::make_unique<ParallelEdtLoweringPass>();
}
