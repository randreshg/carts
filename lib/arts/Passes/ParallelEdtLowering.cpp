///==========================================================================///
/// File: ParallelEdtLowering.cpp
///
/// This pass lowers arts.edt<parallel> operations into simple scf.for loops
/// over the number of workers. The worker induction variable replaces the
/// arts.get_parallel_worker_id() placeholder emitted by the ForLowering pass.
///==========================================================================///

#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsDebug.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Transforms/RegionUtils.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;
using namespace mlir::arts;

ARTS_DEBUG_SETUP(parallel_edt_lowering);

namespace {
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
    ARTS_DEBUG("Lowering parallel EDT " << parallelEdt);
    Block &body = parallelEdt.getBody().front();
    SmallVector<Operation *> bodyOps;
    for (Operation &op : body.without_terminator())
      bodyOps.push_back(&op);

    /// If there is nothing to rewrite, simply convert the EDT into a task EDT
    /// so downstream passes can handle it uniformly.
    if (bodyOps.empty()) {
      ARTS_DEBUG("  Parallel EDT has no body; demoting to task EDT");
      parallelEdt.setType(EdtType::task);
      parallelEdt->removeAttr("workers");
      return success();
    }

    /// Identify which top-level ops depend on the worker id either directly or
    /// through SSA use-def chains.
    SmallVector<bool> workerMask(bodyOps.size(), false);
    llvm::DenseSet<Value> workerValues;

    auto markWorkerOp = [&](uint64_t idx) {
      if (workerMask[idx])
        return;
      workerMask[idx] = true;
      for (Value result : bodyOps[idx]->getResults())
        workerValues.insert(result);
    };

    for (uint64_t idx = 0; idx < bodyOps.size(); ++idx) {
      Operation *op = bodyOps[idx];
      bool usesWorkerId = false;
      op->walk([&](GetParallelWorkerIdOp) { usesWorkerId = true; });
      if (usesWorkerId) {
        markWorkerOp(idx);
        continue;
      }

      for (Value operand : op->getOperands()) {
        if (workerValues.contains(operand)) {
          markWorkerOp(idx);
          break;
        }
      }
    }

    int64_t firstWorker = -1, lastWorker = -1;
    for (int64_t i = 0; i < static_cast<int64_t>(workerMask.size()); ++i) {
      if (workerMask[i]) {
        firstWorker = i;
        break;
      }
    }
    if (firstWorker < 0) {
      ARTS_DEBUG("  No worker-dependent operations; demoting to task EDT");
      parallelEdt.setType(EdtType::task);
      parallelEdt->removeAttr("workers");
      return success();
    }
    for (int64_t i = workerMask.size() - 1; i >= 0; --i) {
      if (workerMask[i]) {
        lastWorker = i;
        break;
      }
    }

    ARTS_DEBUG("  Worker-dependent ops range: [" << firstWorker << ", "
                                                 << lastWorker << "]");

    Location loc = parallelEdt.getLoc();
    OpBuilder builder(parallelEdt);
    builder.setInsertionPoint(parallelEdt);

    ValueRange deps = parallelEdt.getDependencies();
    if (body.getNumArguments() != deps.size()) {
      parallelEdt.emitError(
          "dependency/block argument mismatch while lowering parallel EDT");
      return failure();
    }

    IRMapping mapping;
    for (auto [idx, arg] : llvm::enumerate(body.getArguments()))
      mapping.map(arg, deps[idx]);

    auto cloneAndMap = [&](Operation *op, OpBuilder &cloneBuilder,
                           Value workerId) {
      Operation *cloned = cloneBuilder.clone(*op, mapping);
      for (auto [oldResult, newResult] :
           llvm::zip(op->getResults(), cloned->getResults()))
        mapping.map(oldResult, newResult);
      if (auto edt = dyn_cast<EdtOp>(cloned)) {
        if (edt.getType() == EdtType::single) {
          edt.setType(EdtType::task);
          edt->removeAttr("workers");
          if (workerId) {
            OpBuilder guardBuilder(edt);
            Value zeroIdx =
                guardBuilder.create<arith::ConstantIndexOp>(edt.getLoc(), 0);
            Value isWorkerZero = guardBuilder.create<arith::CmpIOp>(
                edt.getLoc(), arith::CmpIPredicate::eq, workerId, zeroIdx);
            auto ifOp = guardBuilder.create<scf::IfOp>(edt.getLoc(),
                                                       isWorkerZero, false);
            edt->moveBefore(ifOp.thenBlock()->getTerminator());
            guardBuilder.setInsertionPointToStart(ifOp.thenBlock());
            guardBuilder.create<scf::YieldOp>(edt.getLoc());
          }
        }
        SetVector<Value> captured;
        getUsedValuesDefinedAbove(edt.getRegion(), captured);
        if (!captured.empty()) {
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
            for (OpOperand &use :
                 llvm::make_early_inc_range(capturedVal.getUses())) {
              if (edt->isAncestor(use.getOwner()))
                use.set(newArg);
            }
            deps.push_back(capturedVal);
          }
          edt.getDependenciesMutable().assign(deps);
        }
      }
    };

    /// Clone prefix operations that do not rely on the worker id.
    for (int64_t i = 0; i < firstWorker; ++i)
      cloneAndMap(bodyOps[i], builder, Value());

    Value numWorkers = getNumWorkers(builder, loc, parallelEdt);
    ARTS_DEBUG("  Creating worker loop with upper bound " << numWorkers);
    Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
    Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
    scf::ForOp workerLoop =
        builder.create<scf::ForOp>(loc, zero, numWorkers, one);
    builder.setInsertionPointToStart(workerLoop.getBody());

    for (int64_t i = firstWorker; i <= lastWorker; ++i)
      cloneAndMap(bodyOps[i], builder, workerLoop.getInductionVar());

    builder.setInsertionPointToEnd(workerLoop.getBody());
    if (workerLoop.getBody()->empty() ||
        !workerLoop.getBody()->back().hasTrait<OpTrait::IsTerminator>())
      builder.create<scf::YieldOp>(loc);

    workerLoop.walk([&](GetParallelWorkerIdOp op) {
      mapping.map(op.getResult(), workerLoop.getInductionVar());
      op.replaceAllUsesWith(workerLoop.getInductionVar());
      op.erase();
    });

    builder.setInsertionPointAfter(workerLoop);
    for (uint64_t i = lastWorker + 1; i < bodyOps.size(); ++i)
      cloneAndMap(bodyOps[i], builder, Value());

    parallelEdt.erase();
    ARTS_DEBUG("  Lowered parallel EDT into worker loop form");
    return success();
  }

  Value getNumWorkers(OpBuilder &builder, Location loc, EdtOp parallelEdt) {
    if (auto workersAttribute =
            parallelEdt->getAttrOfType<workersAttr>("workers"))
      return builder.create<arith::ConstantIndexOp>(
          loc, workersAttribute.getValue());
    return builder.create<GetTotalWorkersOp>(loc).getResult();
  }
};
} // namespace

std::unique_ptr<Pass> mlir::arts::createParallelEdtLoweringPass() {
  return std::make_unique<ParallelEdtLoweringPass>();
}
