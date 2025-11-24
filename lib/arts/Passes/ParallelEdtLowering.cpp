///==========================================================================///
/// File: ParallelEdtLowering.cpp
///
/// Lower arts.edt<parallel> into a simple worker loop. The loop induction
/// variable replaces arts.get_parallel_worker_id placeholders, worker EDTs are
/// created sequentially, and single EDTs are guarded to run only once
/// (worker==0) within the loop to preserve program order.
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

    // Move EDT into then block
    Block *thenBlock = &ifOp.getThenRegion().front();
    edt->moveBefore(thenBlock->getTerminator());

    // Fix EDT type
    edt.setType(EdtType::task);
    edt->removeAttr("nowait");
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
    edt.getDependenciesMutable().assign(deps);
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
  Value getNumWorkers(OpBuilder &builder, Location loc, EdtOp parallelEdt) {
    if (auto workersAttribute =
            parallelEdt->getAttrOfType<workersAttr>("workers"))
      return builder.create<arith::ConstantIndexOp>(
          loc, workersAttribute.getValue());
    return builder.create<GetTotalWorkersOp>(loc).getResult();
  }

  LogicalResult lowerParallelEdt(EdtOp parallelEdt) {
    ARTS_DEBUG("Lowering parallel EDT " << parallelEdt);
    Block &body = parallelEdt.getBody().front();
    if (body.without_terminator().empty()) {
      parallelEdt.erase();
      return success();
    }

    ValueRange deps = parallelEdt.getDependencies();
    if (body.getNumArguments() != deps.size()) {
      parallelEdt.emitError(
          "dependency/block argument mismatch while lowering parallel EDT");
      return failure();
    }

    Location loc = parallelEdt.getLoc();
    OpBuilder builder(parallelEdt);
    builder.setInsertionPoint(parallelEdt);
    Value numWorkers = getNumWorkers(builder, loc, parallelEdt);
    if (!numWorkers.getType().isIndex())
      numWorkers = builder.create<arith::IndexCastOp>(
          loc, builder.getIndexType(), numWorkers);
    Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
    Value one = builder.create<arith::ConstantIndexOp>(loc, 1);

    IRMapping mapping;
    for (auto [arg, dep] : llvm::zip(body.getArguments(), deps))
      mapping.map(arg, dep);

    /// Create a single worker loop to preserve program order
    auto workerLoop = builder.create<scf::ForOp>(loc, zero, numWorkers, one);
    OpBuilder loopBuilder =
        OpBuilder::atBlockBegin(&workerLoop.getRegion().front());
    Value workerId = workerLoop.getInductionVar();

    for (Operation &op : body.without_terminator()) {
      /// Handle GetParallelWorkerIdOp specifically
      if (auto getId = dyn_cast<GetParallelWorkerIdOp>(op)) {
        mapping.map(getId.getResult(), workerId);
        continue;
      }

      /// Clone operation
      Operation *cloned = loopBuilder.clone(op, mapping);
      for (auto [oldRes, newRes] :
           llvm::zip(op.getResults(), cloned->getResults()))
        mapping.map(oldRes, newRes);

      replaceWorkerIds(cloned, workerId);
      guardSingleEdts(cloned, workerId);
      ensureNestedEdtCaptures(cloned);
    }

    parallelEdt.erase();
    ARTS_DEBUG("  Lowered parallel EDT into worker loop form");
    return success();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::arts::createParallelEdtLoweringPass() {
  return std::make_unique<ParallelEdtLoweringPass>();
}
