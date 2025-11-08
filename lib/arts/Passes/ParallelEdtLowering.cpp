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

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/STLExtras.h"

using namespace mlir;
using namespace mlir::arts;

ARTS_DEBUG_SETUP(parallel_edt_lowering);

namespace {
class ParallelEdtLoweringPass
    : public arts::ParallelEdtLoweringBase<ParallelEdtLoweringPass> {
public:
  void runOnOperation() override {
    ModuleOp module = getOperation();
    SmallVector<EdtOp> parallelEdts;
    module.walk([&](EdtOp edt) {
      if (edt.getType() == EdtType::parallel)
        parallelEdts.push_back(edt);
    });

    for (EdtOp edt : parallelEdts) {
      if (failed(lowerParallelEdt(edt))) {
        signalPassFailure();
        return;
      }
    }
  }

private:
  LogicalResult lowerParallelEdt(EdtOp parallelEdt) {
    Location loc = parallelEdt.getLoc();
    OpBuilder builder(parallelEdt);

    Value numWorkers = getNumWorkers(builder, loc, parallelEdt);
    Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
    Value one = builder.create<arith::ConstantIndexOp>(loc, 1);

    scf::ForOp workerLoop = builder.create<scf::ForOp>(loc, zero, numWorkers, one);
    builder.setInsertionPointToStart(workerLoop.getBody());

    Block &oldBody = parallelEdt.getBody().front();
    ValueRange deps = parallelEdt.getDependencies();
    if (oldBody.getNumArguments() != deps.size()) {
      parallelEdt.emitError("dependency/block argument mismatch while lowering parallel EDT");
      return failure();
    }

    IRMapping mapping;
    for (auto [idx, arg] : llvm::enumerate(oldBody.getArguments()))
      mapping.map(arg, deps[idx]);

    for (Operation &op : oldBody.without_terminator()) {
      if (auto getId = dyn_cast<GetParallelWorkerIdOp>(&op)) {
        mapping.map(getId.getResult(), workerLoop.getInductionVar());
        continue;
      }
      builder.clone(op, mapping);
    }

    builder.setInsertionPointToEnd(workerLoop.getBody());
    if (!workerLoop.getBody()->empty() &&
        !workerLoop.getBody()->back().hasTrait<OpTrait::IsTerminator>())
      builder.create<scf::YieldOp>(loc);

    workerLoop.walk([&](GetParallelWorkerIdOp op) {
      op.replaceAllUsesWith(workerLoop.getInductionVar());
      op.erase();
    });

    parallelEdt.erase();
    return success();
  }

  Value getNumWorkers(OpBuilder &builder, Location loc, EdtOp parallelEdt) {
    if (auto workersAttribute =
            parallelEdt->getAttrOfType<workersAttr>("workers"))
      return builder.create<arith::ConstantIndexOp>(loc,
                                                    workersAttribute.getValue());
    return builder.create<GetTotalWorkersOp>(loc).getResult();
  }
};
} // namespace

std::unique_ptr<Pass> mlir::arts::createParallelEdtLoweringPass() {
  return std::make_unique<ParallelEdtLoweringPass>();
}
