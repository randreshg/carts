///==========================================================================
/// File: DataBlock.cpp
///==========================================================================

/// Dialects
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Ops.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Analysis/DataBlockAnalysis.h"
#include "arts/ArtsDialect.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>

#define DEBUG_TYPE "datablock"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;


//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct DatablockPass : public arts::DatablockBase<DatablockPass> {
  void runOnOperation() override;
};
} // end anonymous namespace

void DatablockPass::runOnOperation() {
  ModuleOp module = getOperation();
  LLVM_DEBUG(dbgs() << "\n" << line << "DatablockPass STARTED\n" << line);
  OpBuilder builder(module);

  /// Retrieve the shared analysis result.
  DatablockAnalysis &dbAnalysis = getAnalysis<DatablockAnalysis>();

  /// Iterate over every function in the module.
  module->walk([&](func::FuncOp func) {
    /// Retrieve (or compute) the dependency graph for this function.
    auto &graph = dbAnalysis.getOrCreateGraph(func);
    graph.print();
  });

  // module.walk([&](EdtOp edt) {
  //   auto &region = edt.getRegion();
  //   for (auto dep : edt.getDependencies()) {
  //     auto dbOp = cast<DataBlockOp>(dep.getDefiningOp());
  //     adjustDataBlock(dbOp, region);
  //   }
  // });

  LLVM_DEBUG(dbgs() << line << "DatablockPass FINISHED\n" << line);
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDatablockPass() {
  return std::make_unique<DatablockPass>();
}
} // namespace arts
} // namespace mlir
