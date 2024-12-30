#include "PassDetails.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/SCF/Transforms/Passes.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/Matchers.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "carts/Ops.h"
#include "carts/Passes/Passes.h"

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace carts;

namespace {
struct ConvertOpenMPToARTSPass : public ConvertOpenMPToARTSBase<ConvertOpenMPToARTSPass> {
  void runOnOperation() override;
};
} // namespace


void ConvertOpenMPToARTSPass::runOnOperation() {
  ModuleOp module = getOperation();
    unsigned numOpenMPRegions = 0;

    // Traverse all operations in the module and count OpenMP ParallelOps.
    module.walk([&](omp::ParallelOp op) {
      ++numOpenMPRegions;
    });

    // Print the count of OpenMP regions.
    llvm::outs() << "Number of OpenMP ParallelOp regions: " << numOpenMPRegions << "\n";
  // mlir::RewritePatternSet rpl(getOperation()->getContext());
  // rpl.add<CombineParallel, ParallelForInterchange, ParallelIfInterchange>(
  //     getOperation()->getContext());
  // GreedyRewriteConfig config;
  // config.maxIterations = 47;
  // (void)applyPatternsAndFoldGreedily(getOperation(), std::move(rpl), config);
}

std::unique_ptr<Pass> mlir::arts::createConvertOpenMPtoARTSPass() {
  return std::make_unique<ConvertOpenMPToARTSPass>();
}
