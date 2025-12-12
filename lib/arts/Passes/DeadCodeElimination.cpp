///==========================================================================///
/// File: DeadCodeElimination.cpp
///
/// This pass performs dead code elimination for ARTS and memref operations.
/// It removes:
/// - Dead loads (loads whose results are unused)
/// - Dead stores (stores to allocas that are never loaded)
/// - Dead allocas (allocas with no remaining uses)
/// - Dead arts.undef operations (undef values with no uses)
/// - Trivially empty EDTs (EDTs with only yield/barrier/release ops)
/// - Dead datablocks (db_alloc where both guid and ptr are unused)
///
/// This is particularly useful after openmp-to-arts conversion where OMP
/// dependency proxy allocas become unused, and after EDT lowering where
/// placeholder undef values may remain.
///==========================================================================///

#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/OpRemovalManager.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Pass/Pass.h"

ARTS_DEBUG_SETUP(dce);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct DeadCodeEliminationPass
    : public DeadCodeEliminationBase<DeadCodeEliminationPass> {

  void runOnOperation() override {
    auto module = getOperation();
    bool changed = true;
    unsigned iterations = 0;
    unsigned totalRemoved = 0;

    ARTS_DEBUG("Starting DeadCodeElimination pass");

    /// Iterate until no more changes
    while (changed) {
      changed = false;
      iterations++;

      /// Memref DCE
      unsigned removedLoads = removeDeadLoads(module);
      unsigned removedStores = removeDeadStores(module);
      unsigned removedAllocas = removeDeadAllocas(module);

      /// ARTS-specific DCE
      unsigned removedUndefs = removeDeadUndefs(module);
      unsigned removedEdts = removeEmptyEdts(module);
      unsigned removedDbs = removeDeadDbs(module);

      unsigned removed = removedLoads + removedStores + removedAllocas +
                         removedUndefs + removedEdts + removedDbs;
      totalRemoved += removed;
      changed = (removed > 0);
    }

    ARTS_DEBUG("Completed DCE: removed " << totalRemoved << " operations in "
                                         << iterations << " iterations");

    /// Also remove dead symbols (functions/globals)
    removeDeadSymbols(module);
  }

  /// Remove memref.load operations whose results have no uses
  unsigned removeDeadLoads(ModuleOp module) {
    SmallVector<Operation *> toRemove;

    module.walk([&](memref::LoadOp load) {
      if (load.getResult().use_empty()) {
        ARTS_DEBUG("Removing dead load: " << load);
        toRemove.push_back(load);
      }
    });

    for (auto *op : toRemove)
      op->erase();

    return toRemove.size();
  }

  /// Remove memref.store operations to allocas that have no loads
  unsigned removeDeadStores(ModuleOp module) {
    SmallVector<Operation *> toRemove;

    module.walk([&](memref::AllocaOp alloca) {
      bool hasLoads = false;
      SmallVector<memref::StoreOp> stores;

      for (auto *user : alloca->getUsers()) {
        if (isa<memref::LoadOp>(user)) {
          hasLoads = true;
        } else if (auto store = dyn_cast<memref::StoreOp>(user)) {
          /// Only consider stores TO this alloca (not stores of alloca value)
          if (store.getMemref() == alloca.getResult())
            stores.push_back(store);
        }
      }

      /// If no loads from this alloca, all stores to it are dead
      if (!hasLoads) {
        for (auto store : stores) {
          ARTS_DEBUG("Removing dead store: " << store);
          toRemove.push_back(store);
        }
      }
    });

    for (auto *op : toRemove)
      op->erase();

    return toRemove.size();
  }

  /// Remove memref.alloca operations with no users
  unsigned removeDeadAllocas(ModuleOp module) {
    SmallVector<Operation *> toRemove;

    module.walk([&](memref::AllocaOp alloca) {
      if (alloca->use_empty()) {
        ARTS_DEBUG("Removing dead alloca: " << alloca);
        toRemove.push_back(alloca);
      }
    });

    for (auto *op : toRemove)
      op->erase();

    return toRemove.size();
  }

  ///===----------------------------------------------------------------------===///
  /// ARTS-specific Dead Code Elimination
  ///===----------------------------------------------------------------------===///

  /// Remove arts.undef operations whose results have no uses
  /// These are created as placeholders during EDT outlining and may remain
  /// after transformation if not all uses were replaced.
  unsigned removeDeadUndefs(ModuleOp module) {
    OpRemovalManager removalMgr;
    module.walk([&](UndefOp undef) { removalMgr.markForRemoval(undef); });
    ARTS_DEBUG(" - Removing " << removalMgr.size() << " undef operations");
    removalMgr.removeAllMarked(module, /*recursive=*/true);
    return 0;
  }

  /// Remove trivially empty EDTs (only yield/barrier/release ops)
  unsigned removeEmptyEdts(ModuleOp module) {
    OpRemovalManager removalMgr;
    module.walk([&](EdtOp edt) {
      bool hasWork = false;
      for (Operation &op : edt.getBody().front()) {
        if (isa<arts::YieldOp>(op) || isa<arts::BarrierOp>(op))
          continue;
        if (isa<arts::DbReleaseOp>(op))
          continue;
        hasWork = true;
        break;
      }
      if (hasWork)
        return;

      /// Remove the EDT and any single-use acquires that only feed it.
      for (Value dep : edt.getDependencies()) {
        if (auto acq = dep.getDefiningOp<arts::DbAcquireOp>()) {
          bool ptrOnlyUsedHere =
              acq.getPtr() == dep && acq.getPtr().hasOneUse();
          bool guidUnused = acq.getGuid().use_empty();
          if (ptrOnlyUsedHere && guidUnused)
            removalMgr.markForRemoval(acq);
        }
      }

      removalMgr.markForRemoval(edt);
    });
    auto opsToRemoveSize = removalMgr.getOpsToRemove().size();
    removalMgr.removeAllMarked(module, /*recursive=*/true);
    return opsToRemoveSize;
  }

  /// Remove arts.db_alloc operations where both guid and ptr are unused
  unsigned removeDeadDbs(ModuleOp module) {
    OpRemovalManager removalMgr;

    module.walk([&](DbAllocOp dbAlloc) {
      if (dbAlloc.getGuid().use_empty() && dbAlloc.getPtr().use_empty()) {
        ARTS_DEBUG("Removing dead db_alloc: " << dbAlloc);
        removalMgr.markForRemoval(dbAlloc);
      }
    });

    auto opsToRemoveSize = removalMgr.getOpsToRemove().size();
    removalMgr.removeAllMarked(module, /*recursive=*/true);
    return opsToRemoveSize;
  }

  /// Remove dead symbols (functions/globals) that are private and unused
  void removeDeadSymbols(ModuleOp module) {
    SmallVector<Operation *> toRemove;
    SymbolTableCollection symbolTable;

    /// Collect all symbol uses
    DenseSet<Operation *> usedSymbols;
    module.walk([&](Operation *op) {
      /// Look for symbol references in this operation
      for (auto attr : op->getAttrs()) {
        if (auto symbolRef = dyn_cast<SymbolRefAttr>(attr.getValue())) {
          if (Operation *symbol =
                  symbolTable.lookupNearestSymbolFrom(op, symbolRef))
            usedSymbols.insert(symbol);
        }
      }
    });

    /// Find dead private symbols
    module.walk([&](Operation *op) {
      if (auto symbol = dyn_cast<SymbolOpInterface>(op)) {
        if (symbol.isPrivate() && !usedSymbols.count(op)) {
          ARTS_DEBUG("Removing dead symbol: " << op->getName());
          toRemove.push_back(op);
        }
      }
    });

    for (auto *op : toRemove)
      op->erase();

    if (!toRemove.empty())
      ARTS_DEBUG("Removed " << toRemove.size() << " dead symbols");
  }
};

} // namespace

std::unique_ptr<Pass> mlir::arts::createDeadCodeEliminationPass() {
  return std::make_unique<DeadCodeEliminationPass>();
}
