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
/// - Unused acquires (db_acquire with no memory ops and unused guid)
/// - Unused EDT dependencies (EDT block args with no uses + backing acquires)
///
/// Example:
///   Before:
///     %u = arts.undef ...
///     %x = memref.load %A[%i]
///     // %u, %x unused
///
///   After:
///     // dead ops removed
///==========================================================================///

#define GEN_PASS_DEF_DEADCODEELIMINATION
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "arts/passes/Passes.h.inc"
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/RemovalUtils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"
#include <algorithm>

ARTS_DEBUG_SETUP(dead_code_elimination);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct DeadCodeEliminationPass
    : public impl::DeadCodeEliminationBase<DeadCodeEliminationPass> {

  static bool isContractUse(Operation *user) {
    return isa<LoweringContractOp>(user);
  }

  static bool hasNonContractUse(Value value) {
    return llvm::any_of(value.getUsers(),
                        [](Operation *user) { return !isContractUse(user); });
  }

  static bool hasSingleExpectedNonContractUse(Value value,
                                              Operation *expectedUser) {
    unsigned count = 0;
    for (Operation *user : value.getUsers()) {
      if (isContractUse(user))
        continue;
      if (user != expectedUser)
        return false;
      ++count;
    }
    return count == 1;
  }

  static void collectContractUsers(Value value,
                                   SmallVectorImpl<Operation *> &contracts) {
    for (Operation *user : value.getUsers()) {
      if (isContractUse(user))
        contracts.push_back(user);
    }
  }

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
      unsigned removedAcquires = removeUnusedAcquires(module);
      unsigned removedPureOps = removeDeadPureOps(module);
      /// Conservatively keep EDT dependency operands. Some control-only
      /// dependency edges encode ordering constraints even when the block
      /// argument has no direct memory users (for example Jacobi-style
      /// alternating-buffer pipelines).
      unsigned removedEdtDeps = 0;

      unsigned removed = removedLoads + removedStores + removedAllocas +
                         removedUndefs + removedEdts + removedDbs +
                         removedAcquires + removedPureOps + removedEdtDeps;
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

  /// Remove trivially-dead, memory-effect-free ops.
  /// This cleans up residual arithmetic/type-size helper chains that remain
  /// after structural rewrites.
  unsigned removeDeadPureOps(ModuleOp module) {
    SmallVector<Operation *> toRemove;

    module.walk([&](Operation *op) {
      if (!op || !op->use_empty())
        return;
      if (op->hasTrait<OpTrait::IsTerminator>())
        return;
      if (!op->getRegions().empty())
        return;
      if (!isMemoryEffectFree(op))
        return;
      toRemove.push_back(op);
    });

    for (Operation *op : toRemove)
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
    RemovalUtils removalMgr;
    module.walk([&](UndefOp undef) { removalMgr.markForRemoval(undef); });
    ARTS_DEBUG(" - Removing " << removalMgr.size() << " undef operations");
    removalMgr.removeAllMarked(module, /*recursive=*/true);
    return 0;
  }

  /// Remove trivially empty EDTs (only yield/barrier/release ops)
  unsigned removeEmptyEdts(ModuleOp module) {
    RemovalUtils removalMgr;
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
              acq.getPtr() == dep &&
              hasSingleExpectedNonContractUse(acq.getPtr(), edt.getOperation());
          bool guidUnused = acq.getGuid().use_empty();
          if (ptrOnlyUsedHere && guidUnused) {
            SmallVector<Operation *> contracts;
            collectContractUsers(acq.getPtr(), contracts);
            for (Operation *contract : contracts)
              removalMgr.markForRemoval(contract);
            removalMgr.markForRemoval(acq);
          }
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
    RemovalUtils removalMgr;

    module.walk([&](DbAllocOp dbAlloc) {
      if (dbAlloc.getGuid().use_empty() &&
          !hasNonContractUse(dbAlloc.getPtr())) {
        SmallVector<Operation *> contracts;
        collectContractUsers(dbAlloc.getPtr(), contracts);
        for (Operation *contract : contracts)
          removalMgr.markForRemoval(contract);
        ARTS_DEBUG("Removing dead db_alloc: " << dbAlloc);
        removalMgr.markForRemoval(dbAlloc);
      }
    });

    auto opsToRemoveSize = removalMgr.getOpsToRemove().size();
    removalMgr.removeAllMarked(module, /*recursive=*/true);
    return opsToRemoveSize;
  }

  /// Remove unused db_acquire operations whose ptr and guid are both unused.
  /// This handles "phantom acquires" - acquires generated for arrays visible
  /// in a parallel scope but not actually accessed in a particular EDT.
  unsigned removeUnusedAcquires(ModuleOp module) {
    SmallVector<Operation *> toRemove;
    SmallVector<Operation *> contractsToRemove;

    module.walk([&](DbAcquireOp acquire) {
      /// Check if ptr has any uses at all (memory ops, EDT args, etc.)
      bool ptrUsed = hasNonContractUse(acquire.getPtr());

      /// Check if guid is used (for signaling/dependencies)
      bool guidUsed = !acquire.getGuid().use_empty();

      if (!ptrUsed && !guidUsed) {
        ARTS_DEBUG("Removing unused acquire: " << acquire);
        collectContractUsers(acquire.getPtr(), contractsToRemove);
        toRemove.push_back(acquire);
      }
    });

    std::sort(contractsToRemove.begin(), contractsToRemove.end());
    contractsToRemove.erase(
        std::unique(contractsToRemove.begin(), contractsToRemove.end()),
        contractsToRemove.end());
    for (auto *contract : contractsToRemove)
      contract->erase();

    for (auto *op : toRemove)
      op->erase();

    return toRemove.size();
  }

  /// Check if a block argument has only DbReleaseOp uses (no real memory ops)
  bool hasOnlyReleaseUses(BlockArgument arg) {
    for (Operation *user : arg.getUsers()) {
      if (!isa<DbReleaseOp>(user))
        return false;
    }
    return true;
  }

  /// Remove unused EDT dependencies and their backing acquires.
  /// This handles "phantom acquires" - acquires for arrays visible in a
  /// parallel scope but not actually accessed in a particular EDT.
  /// The acquire's ptr is used as EDT dependency, but the corresponding
  /// block argument inside the EDT body has no real memory uses (only
  /// releases).
  unsigned removeUnusedEdtDependencies(ModuleOp module) {
    unsigned removed = 0;

    module.walk([&](EdtOp edt) {
      Block &body = edt.getBody().front();
      SmallVector<unsigned> unusedArgIndices;
      ValueRange deps = edt.getDependencies();

      /// Find block arguments with no real uses (only DbReleaseOp uses)
      for (unsigned i = 0; i < body.getNumArguments(); ++i) {
        BlockArgument arg = body.getArgument(i);
        bool controlOnly = arg.use_empty() || hasOnlyReleaseUses(arg);
        if (!controlOnly)
          continue;
        if (i < deps.size()) {
          if (auto acq = deps[i].getDefiningOp<DbAcquireOp>();
              acq && acq.getPreserveDepEdge()) {
            ARTS_DEBUG("Keeping control-only dependency " << i
                                                          << " due to explicit "
                                                             "dependency edge");
            continue;
          }
        }
        unusedArgIndices.push_back(i);
      }

      if (unusedArgIndices.empty())
        return;

      /// Collect acquires to remove (those with single use = this EDT dep)
      SmallVector<DbAcquireOp> acquiresToRemove;
      SmallVector<Operation *> contractsToRemove;
      for (unsigned idx : unusedArgIndices) {
        if (idx >= deps.size())
          continue;
        Value dep = deps[idx];
        if (auto acq = dep.getDefiningOp<DbAcquireOp>()) {
          /// Only remove if ptr has single use (this EDT) and guid unused
          if (acq.getPtr() == dep &&
              hasSingleExpectedNonContractUse(acq.getPtr(),
                                              edt.getOperation()) &&
              acq.getGuid().use_empty()) {
            acquiresToRemove.push_back(acq);
            collectContractUsers(acq.getPtr(), contractsToRemove);
            ARTS_DEBUG("Will remove phantom acquire: " << acq);
          }
        }
      }

      /// Build new dependency list excluding unused ones
      SmallVector<Value> newDeps;
      for (unsigned i = 0; i < deps.size(); ++i) {
        if (!llvm::is_contained(unusedArgIndices, i))
          newDeps.push_back(deps[i]);
      }

      /// First, remove DbReleaseOp operations that use the unused args
      for (unsigned idx : unusedArgIndices) {
        if (idx >= body.getNumArguments())
          continue;
        BlockArgument arg = body.getArgument(idx);
        SmallVector<Operation *> releasesToRemove;
        for (Operation *user : arg.getUsers()) {
          if (isa<DbReleaseOp>(user))
            releasesToRemove.push_back(user);
        }
        for (Operation *op : releasesToRemove)
          op->erase();
      }

      /// Remove unused block arguments IN REVERSE ORDER to preserve indices
      for (auto it = unusedArgIndices.rbegin(); it != unusedArgIndices.rend();
           ++it) {
        body.eraseArgument(*it);
      }

      /// Update EDT dependencies using existing API
      edt.setDependencies(newDeps);

      /// Erase unused acquires
      std::sort(contractsToRemove.begin(), contractsToRemove.end());
      contractsToRemove.erase(
          std::unique(contractsToRemove.begin(), contractsToRemove.end()),
          contractsToRemove.end());
      for (auto *contract : contractsToRemove)
        contract->erase();

      for (auto acq : acquiresToRemove) {
        acq.erase();
        removed++;
      }

      ARTS_DEBUG("Removed " << unusedArgIndices.size()
                            << " unused deps from EDT");
    });

    return removed;
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

std::unique_ptr<Pass> mlir::arts::createDCEPass() {
  return std::make_unique<DeadCodeEliminationPass>();
}
