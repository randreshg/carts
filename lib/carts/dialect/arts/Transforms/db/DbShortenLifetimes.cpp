///==========================================================================///
/// File: DbShortenLifetimes.cpp
///
/// Remove cleanup-only DB acquire chains.
///==========================================================================///

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/utils/RemovalUtils.h"
#define GEN_PASS_DEF_DBSHORTENLIFETIMES
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
ARTS_DEBUG_SETUP(db_shorten_lifetimes);

static llvm::Statistic numCleanupOnlyAcquireChainsRemoved{
    "db_shorten_lifetimes", "NumCleanupOnlyAcquireChainsRemoved",
    "Number of cleanup-only acquire chains removed from the DB graph"};

namespace {
static unsigned shortenLifetimes(ModuleOp module) {
  unsigned removedAcquireChains = 0;

  module.walk([&](func::FuncOp func) {
    SmallVector<DbAcquireOp, 16> acquires;
    func.walk([&](DbAcquireOp acquire) { acquires.push_back(acquire); });

    llvm::SetVector<Operation *> opsToRemove;
    DenseSet<Operation *> scheduledRoots;

    for (DbAcquireOp acquire : acquires) {
      if (!acquire || scheduledRoots.contains(acquire.getOperation()))
        continue;

      llvm::SetVector<Operation *> cleanupChain;
      if (!DbUtils::collectCleanupOnlyUseChain(acquire.getGuid(), cleanupChain))
        continue;
      if (!DbUtils::collectCleanupOnlyUseChain(acquire.getPtr(), cleanupChain))
        continue;

      cleanupChain.insert(acquire.getOperation());
      for (Operation *op : cleanupChain) {
        opsToRemove.insert(op);
        if (isa<DbAcquireOp>(op))
          scheduledRoots.insert(op);
      }

      ++removedAcquireChains;
      ARTS_DEBUG("removing cleanup-only acquire chain rooted at " << acquire);
    }

    if (opsToRemove.empty())
      return;

    RemovalUtils removalMgr;
    for (Operation *op : opsToRemove)
      removalMgr.markForRemoval(op);
    removalMgr.removeAllMarked(module, /*recursive=*/true);
  });

  return removedAcquireChains;
}

struct DbShortenLifetimesPass
    : public impl::DbShortenLifetimesBase<DbShortenLifetimesPass> {
  DbShortenLifetimesPass() = default;

  void runOnOperation() override {
    ARTS_INFO_HEADER(DbShortenLifetimesPass);
    unsigned count = shortenLifetimes(getOperation());
    numCleanupOnlyAcquireChainsRemoved += count;
    if (count > 0)
      ARTS_INFO("shortened " << count << " cleanup-only acquire lifetimes");
    ARTS_INFO_FOOTER(DbShortenLifetimesPass);
  }
};
} // namespace

std::unique_ptr<Pass> mlir::carts::arts::createDbShortenLifetimesPass() {
  return std::make_unique<DbShortenLifetimesPass>();
}
