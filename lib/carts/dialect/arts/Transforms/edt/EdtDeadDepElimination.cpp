///==========================================================================///
/// File: EdtDeadDepElimination.cpp
///
/// Remove cleanup-only dep slots from EDTs.
///==========================================================================///

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/passes/Passes.h"
#include "carts/utils/RemovalUtils.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/Statistic.h"
#include <algorithm>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

#define GEN_PASS_DEF_EDTDEADDEPELIMINATION
#include "carts/passes/Passes.h.inc"

static llvm::Statistic numDeadDepsRemoved{"edt_dead_dep_elimination",
                                          "NumDeadDepsRemoved",
                                          "Number of dead deps removed"};

namespace {

struct EdtDeadDepEliminationPass
    : public ::impl::EdtDeadDepEliminationBase<EdtDeadDepEliminationPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    module.walk([&](EdtOp edt) {
      Block &body = edt.getBody().front();
      ValueRange deps = edt.getDependencies();
      unsigned numArgs = body.getNumArguments();

      if (deps.size() != numArgs)
        return;

      SmallVector<unsigned, 4> deadIndices;
      llvm::SetVector<Operation *> cleanupOpsToErase;
      for (unsigned i = 0; i < numArgs; ++i) {
        BlockArgument arg = body.getArgument(i);
        DbAcquireOp acquire = deps[i].getDefiningOp<DbAcquireOp>();

        if (acquire && acquire.getPreserveDepEdge())
          continue;

        llvm::SetVector<Operation *> cleanupChain;
        bool removable = DbUtils::collectCleanupOnlyUseChain(arg, cleanupChain,
                                                             &edt.getRegion());
        if (!removable && acquire && acquire.getMode() == ArtsMode::out)
          removable = DbUtils::collectTrueOnlyControlTokenUseChain(
              arg, cleanupChain, &edt.getRegion());
        if (!removable)
          continue;

        deadIndices.push_back(i);
        for (Operation *op : cleanupChain)
          cleanupOpsToErase.insert(op);
      }

      if (deadIndices.empty())
        return;

      RemovalUtils removalMgr;
      for (Operation *op : cleanupOpsToErase)
        removalMgr.markForRemoval(op);
      removalMgr.removeAllMarked(module, /*recursive=*/true);

      llvm::sort(deadIndices, std::greater<>());
      deadIndices.erase(std::unique(deadIndices.begin(), deadIndices.end()),
                        deadIndices.end());

      DenseSet<unsigned> deadIndexSet(deadIndices.begin(), deadIndices.end());
      SmallVector<Value> newDeps;
      for (unsigned i = 0; i < deps.size(); ++i)
        if (!deadIndexSet.contains(i))
          newDeps.push_back(deps[i]);

      SmallVector<DbAcquireOp, 4> acquireCandidates;
      for (unsigned idx : deadIndices) {
        Value dep = deps[idx];
        if (auto acquire = dep.getDefiningOp<DbAcquireOp>())
          acquireCandidates.push_back(acquire);
      }

      for (unsigned idx : deadIndices)
        body.eraseArgument(idx);

      edt.setDependencies(newDeps);

      for (DbAcquireOp acquire : acquireCandidates)
        if (acquire.getGuid().use_empty() && acquire.getPtr().use_empty())
          acquire.erase();

      numDeadDepsRemoved += deadIndices.size();
    });
  }
};

} // namespace

namespace mlir {
namespace carts::arts {
std::unique_ptr<Pass> createEdtDeadDepEliminationPass() {
  return std::make_unique<EdtDeadDepEliminationPass>();
}
} // namespace carts::arts
} // namespace mlir
