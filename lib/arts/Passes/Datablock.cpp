///==========================================================================
/// File: DataBlock.cpp
///==========================================================================

/// Dialects
#include "arts/Codegen/ArtsIR.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Support/LLVM.h"

/// Arts
#include "ArtsPassDetails.h"
#include "arts/Analysis/DataBlockAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
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
  /// Datablocks with single size, and result type different to a memref, can
  /// be converted into parameter.
  bool convertToParameters(DatablockGraph *graph);
  /// Canonicalize memref.dim ops of datablocks.
  bool canonicalizeDimOps(DatablockGraph *graph);

private:
  ModuleOp module;
};
} // end anonymous namespace

void DatablockPass::runOnOperation() {
  bool changed = false;
  module = getOperation();
  LLVM_DEBUG({
    dbgs() << "\n" << line << "DatablockPass STARTED\n" << line;
    module.dump();
  });
  OpBuilder builder(module);

  /// Retrieve the shared analysis result.
  DatablockAnalysis &dbAnalysis = getAnalysis<DatablockAnalysis>();

  /// Iterate over every function in the module.
  module->walk([&](func::FuncOp func) {
    auto *graph = dbAnalysis.getOrCreateGraph(func);
    if (!graph || !graph->hasNodes()) {
      LLVM_DEBUG(dbgs() << "No graph for function: " << func.getName() << "\n");
      return;
    }
    graph->print();
    changed |= convertToParameters(graph);
    changed |= canonicalizeDimOps(graph);
  });

  /// Preserve analysis results if no changes were made.
  if (!changed) {
    LLVM_DEBUG(DBGS() << "No changes made to the module\n");
    markAnalysesPreserved<DatablockAnalysis>();
  }

  LLVM_DEBUG({
    dbgs() << line << "DatablockPass FINISHED\n" << line;
    module.dump();
  });
}

bool DatablockPass::convertToParameters(DatablockGraph *graph) {
  bool changed = false;
  auto &dbNodes = graph->getNodes();
  DenseMap<EdtOp, SetVector<DataBlockOp>> dbNodesToRemove;
  SetVector<Operation *> opsToRemove;
  OpBuilder builder(module);

  LLVM_DEBUG(dbgs() << "Converting datablocks to parameters - Analyzing "
                    << dbNodes.size() << " datablocks\n");
  for (auto *dbNode : dbNodes) {
    if (!dbNode->op)
      continue;

    if (!dbNode->hasSingleSize || isa<MemRefType>(dbNode->elementType) ||
        !dbNode->isOnlyReader() || !graph->isOnlyDependentOnEntry(*dbNode))
      continue;

    LLVM_DEBUG(dbgs() << "- Converting datablock to parameter: " << *dbNode->op
                      << "\n");
    dbNodesToRemove[dbNode->edtUser].insert(dbNode->op);
    changed = true;
    /// Create a new load op for the datablock.
    builder.setInsertionPoint(dbNode->op);
    auto newLoadOp = builder.create<memref::LoadOp>(
        dbNode->op.getLoc(), dbNode->ptr, dbNode->indices);
    LLVM_DEBUG(dbgs() << "  - New load op: " << *newLoadOp << "\n");

    /// Replace all load operations with the new load op.
    for (auto &use :
         llvm::make_early_inc_range(dbNode->op.getResult().getUses())) {
      if (isa<arts::EdtOp>(use.getOwner()))
        continue;
      auto loadOp = cast<memref::LoadOp>(use.getOwner());
      loadOp.replaceAllUsesWith(newLoadOp.getResult());
      loadOp.erase();
    }
  }

  /// If no datablocks were converted, return.
  if (!changed) {
    LLVM_DEBUG(dbgs() << " - No datablocks to convert\n");
    return false;
  }
  LLVM_DEBUG(dbgs() << " - Datablock conversion - Found "
                    << dbNodesToRemove.size() << " datablocks to convert\n");

  /// Update EDT dependencies and remove replaced datablock ops.
  for (auto &entry : dbNodesToRemove) {
    auto edtOp = entry.first;
    const auto &dbSet = entry.second;
    SmallVector<Value> newDeps;
    newDeps.reserve(dbSet.size());
    auto edtDeps = edtOp.getDependencies();
    LLVM_DEBUG(dbgs() << "Analyzing edt deps - Size: " << edtDeps.size()
                      << "\n");
    for (auto dep : edtDeps) {
      auto db = cast<DataBlockOp>(dep.getDefiningOp());
      if (dbSet.contains(db)) {
        opsToRemove.insert(db);
        continue;
      }
      newDeps.push_back(dep);
    }

    builder.setInsertionPoint(edtOp);
    auto newEdtOp =
        createEdtOp(builder, edtOp.getLoc(), getEdtType(edtOp), newDeps);
    newEdtOp.getRegion().takeBody(edtOp.getRegion());
    opsToRemove.insert(edtOp);
  }

  LLVM_DEBUG(dbgs() << "Removing datablocks " << opsToRemove.size() << "\n");
  removeOps(module, builder, opsToRemove);
  return true;
}

bool DatablockPass::canonicalizeDimOps(DatablockGraph *graph) {
  auto &dbNodes = graph->getNodes();
  OpBuilder builder(module);

  LLVM_DEBUG(dbgs() << "Canonicalizing dim ops - Analyzing " << dbNodes.size()
                    << " datablocks\n");
  for (auto *dbNode : dbNodes) {
    if (!dbNode->op)
      continue;

    /// Analyze uses of the datablock.
    for (auto *op : dbNode->op->getUsers()) {
      auto dimOp = dyn_cast<memref::DimOp>(op);
      if (!dimOp)
        continue;

      LLVM_DEBUG(dbgs() << "- Canonicalizing dim op: " << *dimOp << "\n");
      auto dim = dimOp.getConstantIndex();
      assert(dim && "Dim op must have a constant index");

      /// Replace the dim op result with the size of the datablock.
      auto size = dbNode->sizes[*dim];
      dimOp.replaceAllUsesWith(size);
      dimOp.erase();
    }
  }
  return false;
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
