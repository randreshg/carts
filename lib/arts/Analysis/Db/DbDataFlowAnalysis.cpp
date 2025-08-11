//===----------------------------------------------------------------------===//
// Db/DbDataFlowAnalysis.cpp - Implementation of DbDataFlowAnalysis
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Db/DbDataFlowAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbEdge.h"
#include "llvm/Support/Debug.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_dataflow);

using namespace mlir;
using namespace mlir::arts;
using namespace mlir::dataflow;

ChangeResult DbState::join(const AbstractDenseLattice &rhs) {
  const DbState &other = static_cast<const DbState &>(rhs);
  bool changed = false;

  for (auto alloc : other.liveAllocs) {
    if (liveAllocs.insert(alloc).second)
      changed = true;
  }

  for (const auto &pair : other.activeAcquires) {
    auto &acquires = activeAcquires[pair.first];
    for (auto acquire : pair.second) {
      if (acquires.insert(acquire).second)
        changed = true;
    }
  }

  for (const auto &pair : other.pendingReleases) {
    auto &releases = pendingReleases[pair.first];
    for (auto release : pair.second) {
      if (releases.insert(release).second)
        changed = true;
    }
  }

  return changed ? ChangeResult::Change : ChangeResult::NoChange;
}

void DbState::print(llvm::raw_ostream &os) const {
  os << "DbState:\n";
  os << "  Live Allocs: " << liveAllocs.size() << "\n";
  for (const auto &alloc : liveAllocs) {
    os << "    - " << alloc << "\n";
  }
  os << "  Active Acquires:\n";
  for (const auto &pair : activeAcquires) {
    os << "    Alloc: " << pair.first << " Acquires: " << pair.second.size()
       << "\n";
  }
  os << "  Pending Releases:\n";
  for (const auto &pair : pendingReleases) {
    os << "    Alloc: " << pair.first << " Releases: " << pair.second.size()
       << "\n";
  }
}

DbDataFlowAnalysis::DbDataFlowAnalysis(DataFlowSolver &solver, DbGraph *graph,
                                       DbAnalysis *analysis)
    : DenseForwardDataFlowAnalysis<DbState>(solver), graph(graph),
      analysis(analysis), aliasAA(analysis) {
  assert(graph && "Graph cannot be null");
  assert(analysis && "Analysis cannot be null");
}

void DbDataFlowAnalysis::visitOperation(Operation *op, const DbState &before,
                                        DbState *after) {
  *after = before;

  // if (auto allocOp = dyn_cast<DbAllocOp>(op)) {
  //   after->liveAllocs.insert(allocOp);
  //   ARTS_INFO("facts: alloc live " << allocOp);
  // } else if (auto acquireOp = dyn_cast<DbAcquireOp>(op)) {
  //   DbAllocOp parentAlloc = graph->findRootAllocOp(acquireOp.getOperation());
  //   if (parentAlloc && before.liveAllocs.contains(parentAlloc)) {
  //     after->activeAcquires[parentAlloc].insert(acquireOp);
  //     ARTS_INFO("facts: acquire active parent " << parentAlloc);
  //   } else {
  //     op->emitWarning("Acquire without live allocation");
  //     ARTS_WARN("Acquire without live allocation for " << acquireOp);
  //   }
  // } else if (auto releaseOp = dyn_cast<DbReleaseOp>(op)) {
  //   DbAllocOp parentAlloc = graph->findRootAllocOp(releaseOp.getOperation());
  //   if (parentAlloc && !before.activeAcquires[parentAlloc].empty()) {
  //     after->pendingReleases[parentAlloc].insert(releaseOp);
  //     for (auto acquire : before.activeAcquires[parentAlloc]) {
  //       DbAcquireNode *acquireNode = static_cast<DbAcquireNode *>(
  //           graph->getNode(acquire.getOperation()));
  //       DbReleaseNode *releaseNode = static_cast<DbReleaseNode *>(
  //           graph->getNode(releaseOp.getOperation()));
  //       if (aliasAA.mayAlias(*acquireNode, *releaseNode)) {
  //         createLifetimeEdge(acquireNode, releaseNode);
  //       }
  //     }
  //     after->activeAcquires[parentAlloc].clear();
  //     ARTS_INFO("facts: release processed parent " << parentAlloc);
  //   } else {
  //     op->emitWarning("Release without active acquire");
  //     ARTS_WARN("Release without active acquire for " << releaseOp);
  //   }
  // }

  auto effects = getDbEffects(op);
  if (!effects.empty()) {
    ARTS_INFO("Op has effects; state propagated: " << *op);
  }
}

void DbDataFlowAnalysis::analyze() {
  ARTS_INFO("Running DB data flow analysis");

  // auto *funcOp = graph->getFunction();
  // initializeAndRun(funcOp);

  // // Post-process for unmatched states
  // const DbState &finalState =
  //     getLatticeElementAfter(funcOp->getBlock()->getTerminator());
  // for (const auto &pair : finalState.activeAcquires) {
  //   if (!pair.second.empty()) {
  //     for (auto acquire : pair.second) {
  //       acquire.getOperation()->emitWarning(
  //           "Unmatched acquire at end of function");
  //     }
  //   }
  // }
  // for (const auto &pair : finalState.pendingReleases) {
  //   if (!pair.second.empty()) {
  //     for (auto release : pair.second) {
  //       release.getOperation()->emitWarning(
  //           "Pending release at end of function");
  //     }
  //   }
  // }

  ARTS_INFO("DB data flow analysis completed");
}

void DbDataFlowAnalysis::createLifetimeEdge(DbAcquireNode *acquire,
                                            DbReleaseNode *release) {
  if (!acquire || !release)
    return;

  auto edge = std::make_unique<DbLifetimeEdge>(acquire, release);
  graph->addEdge(acquire, release, edge.get());
  // edge.release();

  ARTS_INFO("Created lifetime edge from acquire "
            << acquire->getHierId() << " to release " << release->getHierId());
}

SmallVector<MemoryEffects::EffectInstance>
DbDataFlowAnalysis::getDbEffects(Operation *op) {
  SmallVector<MemoryEffects::EffectInstance> effects;
  if (auto effectsOp = dyn_cast<MemoryEffectOpInterface>(op)) {
    effectsOp.getEffects(effects);
  }
  return effects;
}