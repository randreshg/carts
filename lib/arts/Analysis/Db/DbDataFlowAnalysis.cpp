///==========================================================================
/// File: DbDataFlowAnalysis.cpp
/// Implementation of dataflow analysis for DB operations.
///==========================================================================

#include "arts/Analysis/Db/DbDataFlowAnalysis.h"

#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbEdge.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
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

DbDataFlowAnalysis::DbDataFlowAnalysis(DataFlowSolver &solver)
    : DenseForwardDataFlowAnalysis<DbState>(solver), aliasAA(nullptr) {}

void DbDataFlowAnalysis::initialize(DbGraph *graphIn, DbAnalysis *analysisIn) {
  graph = graphIn;
  analysis = analysisIn;
  aliasAA = analysis->getAliasAnalysis();
  assert(graph && "Graph cannot be null");
  assert(analysis && "Analysis cannot be null");
}

void DbDataFlowAnalysis::visitOperation(Operation *op, const DbState &before,
                                        DbState *after) {
  *after = before;

  if (auto allocOp = dyn_cast<DbAllocOp>(op)) {
    after->liveAllocs.insert(allocOp);
    ARTS_INFO("facts: alloc live " << allocOp);
  } else if (auto acquireOp = dyn_cast<DbAcquireOp>(op)) {
    DbAllocOp parentAlloc = graph->getParentAlloc(acquireOp.getOperation());
    if (parentAlloc && before.liveAllocs.contains(parentAlloc)) {
      auto it = before.activeAcquires.find(parentAlloc);
      if (it != before.activeAcquires.end()) {
        after->activeAcquires[parentAlloc].insert(acquireOp);
      } else {
        after->activeAcquires[parentAlloc].insert(acquireOp);
      }
      ARTS_INFO("facts: acquire active parent " << parentAlloc);
    } else {
      op->emitWarning("Acquire without live allocation");
      ARTS_WARN("Acquire without live allocation for " << acquireOp);
    }
  } else if (auto releaseOp = dyn_cast<DbReleaseOp>(op)) {
    DbAllocOp parentAlloc = graph->getParentAlloc(releaseOp.getOperation());
    if (parentAlloc) {
      auto it = before.activeAcquires.find(parentAlloc);
      if (it != before.activeAcquires.end() && !it->second.empty()) {
        after->pendingReleases[parentAlloc].insert(releaseOp);
        /// Pair in order: prefer the most recent active acquire first
        SmallVector<DbAcquireOp, 8> actives;
        auto it = before.activeAcquires.find(parentAlloc);
        if (it != before.activeAcquires.end()) {
          for (auto acquire : it->second)
            actives.push_back(acquire);
        }
        llvm::sort(actives, [&](DbAcquireOp a, DbAcquireOp b) {
          return graph->getParentAlloc(a.getOperation()) &&
                 graph->getParentAlloc(b.getOperation()) &&
                 graph->getParentAlloc(a.getOperation()) ==
                     graph->getParentAlloc(b.getOperation()) &&
                 a.getOperation()->isBeforeInBlock(b.getOperation());
        });
        for (auto acquire : actives) {
          auto *acquireNode = graph->getOrCreateAcquireNode(acquire);
          auto *releaseNode = graph->getOrCreateReleaseNode(releaseOp);
          if (acquireNode && releaseNode && aliasAA &&
              aliasAA->mayAlias(*acquireNode, *releaseNode)) {
            createLifetimeEdge(acquireNode, releaseNode);
          }
        }
        /// Reset active acquires for this allocation after matching release.
        after->activeAcquires[parentAlloc].clear();
        ARTS_INFO("facts: release processed parent " << parentAlloc);
      } else {
        op->emitWarning("Release without active acquire");
        ARTS_WARN("Release without active acquire for " << releaseOp);
      }
    }
  }

  // auto effects = getDbEffects(op);
  // if (!effects.empty()) {
  //   ARTS_INFO("Op has effects; state propagated: " << *op);
  // }
}

void DbDataFlowAnalysis::createLifetimeEdge(DbAcquireNode *acquire,
                                            DbReleaseNode *release) {
  if (!acquire || !release)
    return;

  /// Ownership of the edge is transferred to the graph.
  auto *edge = new DbLifetimeEdge(acquire, release);
  graph->addEdge(acquire, release, edge);

  ARTS_INFO("Created lifetime edge from acquire "
            << acquire->getHierId() << " to release " << release->getHierId());
}

SmallVector<MemoryEffects::EffectInstance>
DbDataFlowAnalysis::getDbEffects(Operation *op) {
  SmallVector<MemoryEffects::EffectInstance> effects;
  if (auto effectsOp = dyn_cast<MemoryEffectOpInterface>(op))
    effectsOp.getEffects(effects);
  return effects;
}

void DbDataFlowAnalysis::setToEntryState(DbState *lattice) {
  /// Default-empty state at function entry is sufficient.
  (void)lattice;
}