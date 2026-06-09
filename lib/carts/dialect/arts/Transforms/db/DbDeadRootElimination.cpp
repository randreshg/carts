///==========================================================================///
/// File: DbDeadRootElimination.cpp
///
/// Remove dead DB root chains.
///==========================================================================///

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/utils/RemovalUtils.h"
#define GEN_PASS_DEF_DBDEADROOTELIMINATION
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/Statistic.h"

using namespace mlir;
using namespace mlir::func;
using namespace mlir::carts;
using namespace mlir::carts::arts;

#include "carts/utils/Debug.h"
ARTS_DEBUG_SETUP(db_dead_root_elimination);

static llvm::Statistic numDeadDbRootsEliminated{
    "db_dead_root_elimination", "NumDeadDbRootsEliminated",
    "Number of dead datablock root chains eliminated"};

namespace {
static unsigned eliminateDeadRoots(ModuleOp module) {
  unsigned removedRoots = 0;

  module.walk([&](func::FuncOp func) {
    SmallVector<DbAllocOp, 16> allocs;
    func.walk([&](DbAllocOp alloc) { allocs.push_back(alloc); });

    llvm::SetVector<Operation *> opsToRemove;
    DenseSet<Operation *> scheduledRoots;

    for (DbAllocOp alloc : allocs) {
      if (!alloc || scheduledRoots.contains(alloc.getOperation()))
        continue;

      llvm::SetVector<Operation *> cleanupChain;
      if (!DbUtils::collectCleanupOnlyUseChain(alloc.getGuid(), cleanupChain))
        continue;
      if (!DbUtils::collectCleanupOnlyUseChain(alloc.getPtr(), cleanupChain))
        continue;

      cleanupChain.insert(alloc.getOperation());
      for (Operation *op : cleanupChain) {
        opsToRemove.insert(op);
        if (isa<DbAllocOp>(op))
          scheduledRoots.insert(op);
      }

      ++removedRoots;
      ARTS_DEBUG("removing dead DB root " << alloc);
    }

    if (opsToRemove.empty())
      return;

    RemovalUtils removalMgr;
    for (Operation *op : opsToRemove)
      removalMgr.markForRemoval(op);
    removalMgr.removeAllMarked(module, /*recursive=*/true);
  });

  return removedRoots;
}

struct DbDeadRootEliminationPass
    : public impl::DbDeadRootEliminationBase<DbDeadRootEliminationPass> {
  DbDeadRootEliminationPass() = default;

  void runOnOperation() override {
    ARTS_INFO_HEADER(DbDeadRootEliminationPass);
    unsigned count = eliminateDeadRoots(getOperation());
    numDeadDbRootsEliminated += count;
    if (count > 0)
      ARTS_INFO("eliminated " << count << " dead DB roots");
    ARTS_INFO_FOOTER(DbDeadRootEliminationPass);
  }
};
} // namespace

std::unique_ptr<Pass> mlir::carts::arts::createDbDeadRootEliminationPass() {
  return std::make_unique<DbDeadRootEliminationPass>();
}
