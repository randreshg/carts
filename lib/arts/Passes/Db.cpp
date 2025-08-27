//===----------------------------------------------------------------------===//
// Db/DbPass.cpp - Pass for DB optimizations
//===----------------------------------------------------------------------===//

/// Dialects

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Db/DbPlacement.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// Pass Implementation
// transform/optimize ARTS DB IR using DbGraph/analyses.
//===----------------------------------------------------------------------===//
namespace {
struct DbPass : public arts::DbBase<DbPass> {
  void runOnOperation() override;

  /// Canonicalize memref.dim ops of datablocks.
  bool canonicalizeDimOps();

  /// DBs with single size, and "in" mode, can be converted into parameters.
  bool convertToParameters();

  /// Analyze DB usage and resize it to the minimum required based on min/max
  /// indices from acquires/releases.
  bool shrinkDb();

  /// Remove unused DB allocations.
  bool deadDbElimination();

  /// Reuse buffers for non-overlapping DB lifetimes.
  bool bufferReuse();

  /// Hoist allocations out of loops if invariant.
  bool hoistAllocs();

  /// Fuse consecutive acquires/releases on the same alloc if no intervening
  /// ops.
  bool fuseAccesses();

  /// Inline small DB allocs into acquires if single-use.
  bool inlineAllocs();

  /// Prefetch and overlap: hoist acquires earlier (after operand defs) and
  /// delay releases after last use within a block to overlap transfers.
  bool prefetchOverlap();

private:
  ModuleOp module;
};
} // end anonymous namespace

void DbPass::runOnOperation() {
  bool changed = false;
  module = getOperation();
  changed |= canonicalizeDimOps(); // disabled internals kept as scaffolding

  ARTS_DEBUG_HEADER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););

  auto &dbAnalysis = getAnalysis<DbAnalysis>();
  // Force-build graphs so downstream opts can consume them now or later.
  module.walk([&](func::FuncOp func) { (void)dbAnalysis.getOrCreateGraph(func); });

  // Order below is conservative; each stage can be independently enabled
  // changed |= deadDbElimination();

  bool ctpChanged = convertToParameters();
  changed |= ctpChanged;
  if (ctpChanged) {
    module.walk([&](func::FuncOp func) {
      (void)dbAnalysis.invalidateGraph(func);
      (void)dbAnalysis.getOrCreateGraph(func);
    });
  }

  bool shrinkChanged = shrinkDb();
  changed |= shrinkChanged;
  if (shrinkChanged) {
    module.walk([&](func::FuncOp func) {
      (void)dbAnalysis.invalidateGraph(func);
      (void)dbAnalysis.getOrCreateGraph(func);
    });
  }

  // changed |= prefetchOverlap();
  // changed |= bufferReuse();
  // changed |= hoistAllocs();
  // changed |= fuseAccesses();
  // changed |= inlineAllocs();

  // Analysis-only: emit single final DbAnalysis JSON dump and placement JSON
  ARTS_DEBUG_SECTION("db-analysis:final-json", {
    module.walk([&](func::FuncOp func) {
      if (auto *graph = dbAnalysis.getOrCreateGraph(func)) {
        llvm::SmallString<128> buf;
        llvm::raw_svector_ostream os(buf);
        graph->exportToJson(os);
        ARTS_DBGS() << os.str();
      }
    });
  });

  ARTS_DEBUG_SECTION("db-placement-json", {
    module.walk([&](func::FuncOp func) {
      auto *graph = dbAnalysis.getOrCreateGraph(func);
      if (!graph)
        return;
      DbPlacementHeuristics placer(graph);
      auto nodes = DbPlacementHeuristics::makeNodeNames(4);
      auto decisions = placer.compute(func, nodes);
      std::string js;
      llvm::raw_string_ostream jos(js);
      placer.exportToJson(func, decisions, jos);
      ARTS_DBGS() << js;
    });
  });

  if (!changed) {
    ARTS_INFO("No changes made to the module");
    markAnalysesPreserved<DbAnalysis>();
  }

  ARTS_DEBUG_FOOTER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););
}

// Prefetch/overlap heuristic
// - Hoist acquires upward to start transfers earlier, but never above the
//   last defining op of their operands (preserves dominance and semantics).
// - Delay releases to just after the last in-block user to avoid premature
//   invalidation and reduce thrash while still keeping scope minimal.
// Scope: intra-block only (no CFG restructuring). Conservative and local.
// Example (before → after):
//   %x = compute
//   %a = arts.db_acquire %db offsets(%o0,%o1) sizes(%s0,%s1)
//   use(%a, %x)
//   arts.db_release %a
// becomes (if uses of %a occur later)
//   %x = compute
//   %a = arts.db_acquire %db offsets(%o0,%o1) sizes(%s0,%s1)
//   use(%a, %x)
//   ...
//   arts.db_release %a            // moved after last use
bool DbPass::prefetchOverlap() {
  bool changed = false;
  // Operate within blocks to preserve simple dominance and avoid CFG changes
  module.walk([&](func::FuncOp func) {
    for (Block &block : func.getBody().getBlocks()) {
      // Build a position index for stable ordering comparisons
      llvm::DenseMap<Operation *, unsigned> pos;
      unsigned i = 0;
      for (Operation &op : block)
        pos[&op] = i++;

      // 1) Hoist acquires
      // Move each acquire as early as possible after the last operand def in
      // this block. If operands are block arguments or defined above, the
      // acquire goes to the block front.
      SmallVector<DbAcquireOp, 16> acquires;
      for (Operation &op : block)
        if (auto acq = dyn_cast<DbAcquireOp>(&op))
          acquires.push_back(acq);

      for (DbAcquireOp acq : acquires) {
        Operation *op = acq.getOperation();
        unsigned cur = pos[op];
        Operation *anchor = nullptr;
        for (Value v : op->getOperands()) {
          if (Operation *def = v.getDefiningOp()) {
            if (def->getBlock() == &block) {
              if (!anchor || pos[def] > pos[anchor])
                anchor = def;
            }
          }
        }
        unsigned targetPos = anchor ? (pos[anchor] + 1) : 0;
        if (targetPos < cur) {
          if (anchor)
            op->moveAfter(anchor);
          else
            op->moveBefore(&block.front());
          changed = true;
        }
      }

      // Rebuild positions after moves
      pos.clear();
      i = 0;
      for (Operation &op : block)
        pos[&op] = i++;

      // 2) Delay releases
      // Move each release as late as possible within the same block: just
      // after the last use of its operand(s). Skip if already in the best spot.
      SmallVector<DbReleaseOp, 16> releases;
      for (Operation &op : block)
        if (auto rel = dyn_cast<DbReleaseOp>(&op))
          releases.push_back(rel);

      for (DbReleaseOp rel : releases) {
        Operation *op = rel.getOperation();
        unsigned cur = pos[op];
        unsigned lastUsePos = cur;
        // Consider all operands; typically the acquired view
        for (Value v : op->getOperands()) {
          for (OpOperand &use : v.getUses()) {
            Operation *user = use.getOwner();
            if (user->getBlock() != &block)
              continue;
            if (user == op)
              continue;
            lastUsePos = std::max(lastUsePos, pos[user]);
          }
        }
        if (lastUsePos > cur) {
          Operation *lastUser = nullptr;
          for (Operation &it : block)
            if (pos[&it] == lastUsePos) {
              lastUser = &it;
              break;
            }
          if (lastUser) {
            op->moveAfter(lastUser);
            changed = true;
          }
        }
      }
    }
  });
  return changed;
}

// Canonicalize dimension queries for DB-backed memrefs
// Responsibility: replace `memref.dim` on DB allocs/acquires with known
// sizes when available.
// Preference order:
//   1) Explicit slice sizes on DbAcquireOp
//   2) Parent DbAllocOp sizes (via DbGraph)
// Scope: local fold-and-erase of dim ops; preserves semantics.
// Example (before → after):
//   %sz = memref.dim %acq, 1 : memref<?x?xi32>
//   // where %acq = arts.db_acquire %db ... sizes(%c4, %c8)
// becomes
//   %sz = %c8 : index
bool DbPass::canonicalizeDimOps() {
  bool changed = false;
  auto &dbAnalysis = getAnalysis<DbAnalysis>();
  module.walk([&](memref::DimOp dimOp) {
    auto cstIdx = dimOp.getConstantIndex();
    if (!cstIdx)
      return;
    Value source = dimOp.getSource();
    if (auto dbAlloc = source.getDefiningOp<DbAllocOp>()) {
      Value sz = dbAlloc.getSizes()[*cstIdx];
      dimOp.replaceAllUsesWith(sz);
      dimOp.erase();
      changed = true;
      return;
    }
    if (auto dbAcq = source.getDefiningOp<DbAcquireOp>()) {
      // Prefer the explicit acquire size slice if present; else fallback to
      // alloc sizes.
      auto sizes = dbAcq.getSizes();
      if (static_cast<long long>(sizes.size()) > *cstIdx) {
        Value sz = sizes[*cstIdx];
        dimOp.replaceAllUsesWith(sz);
        dimOp.erase();
        changed = true;
        return;
      }
      if (auto func = dimOp->getParentOfType<func::FuncOp>()) {
        if (auto *graph = dbAnalysis.getOrCreateGraph(func)) {
          if (DbAllocOp parent = graph->getParentAlloc(dbAcq.getOperation())) {
            Value sz = parent.getSizes()[*cstIdx];
            dimOp.replaceAllUsesWith(sz);
            dimOp.erase();
            changed = true;
            return;
          }
        }
      }
    }
  });
  return changed;
}

// Convert trivial DBs to parameters (disabled scaffolding)
// Intent: when a DB is effectively read-only and 1D, turn it into a parameter
// to avoid runtime traffic. Requires validating usage patterns and lifetimes.
// Current status: example scaffold left commented for future enablement.
// Example (target behavior):
//   %db = arts.db_alloc %ptr sizes(%cN)
//   %a  = arts.db_acquire %db ...
//   load %a[...]
//   --> convert to a function/block argument replacing %db/%a
bool DbPass::convertToParameters() {
  bool changed = false;
  // module.walk([&](func::FuncOp func) {
  //   auto graph = dbAnalysis.getOrCreateGraph(func);
  //   graph->forEachAllocNode([&](DbAllocNode *allocNode) {
  //     bool hasSingleSize = (allocNode->getSizes().size() == 1);
  //     bool isOnlyReader = (allocNode->getAcquireNodesSize() > 0 &&
  //     allocNode->getReleaseNodesSize() == 0);

  //     if (!hasSingleSize || !isOnlyReader) return;

  //     LLVM_DEBUG(dbgs() << "- Converting DB to parameter: " <<
  //     allocNode->getDbAllocOp() << "\n");
  //     allocNode->forEachChildNode([&](NodeBase *child) {
  //       if (auto acquireNode = dyn_cast<DbAcquireNode>(child)) {
  //         Operation *acquireOp = acquireNode->getOp();
  //         acquireOp->getResult(0).replaceAllUsesWith(allocNode->getPtr());
  //         acquireOp->erase();
  //       }
  //     });
  //     if (allocNode->getDbAllocOp().use_empty()) {
  //       allocNode->getDbAllocOp().erase();
  //     }
  //     changed = true;
  //   });
  // });
  return changed;
}

// Coarse-grain DB slices (Design Stub)
// Many pipelines create extremely fine-grained DB acquires (e.g., size-1
// along the last dimension), which can saturate the network. The correct way
// to coarsen is to normalize DB granularity at allocation time (DbAllocOp),
// e.g., switch from NxMxP DBs to N DBs of size MxP, then rewrite acquires/
// releases and EDT operands accordingly. Implementing that requires a
// dedicated transformation that touches DbAllocOps, DbAcquire/DbReleaseOps,
// and the corresponding arts.edt operands/block-args.
//
// This function is currently a no-op placeholder. The earlier subview-based
// approach is not used because EDTs consume acquired DBs across nodes; IR
// subviews alone do not express runtime transfer granularity.
//
// Example (target behavior):
//   Before:
//     %db = arts.db_alloc %ptr sizes(%cN,%cM,%cP)
//     acquires over (i,j,k)
//   After (conceptual):
//     for i in 0..N-1:
//       %db_i = arts.db_alloc %ptr_i sizes(%cM,%cP)
//     acquires only over (j,k) on %db_i chosen by i
/*
Implement a proper “DbGranularityNormalize” transformation:
Split DbAlloc by chosen dimension.
Update all downstream DbAcquire/DbRelease and arts.edt block args/operands to
reference the new coarser-grained DBs. Ensure interaction with EdtGraph/DbGraph
remains correct and verify with tests.
*/
bool DbPass::shrinkDb() { return false; }

// Dead DB elimination
// Responsibility: remove DbAllocOps with no associated acquires/releases and
// no remaining uses. Uses DbGraph to count children; then checks use_empty.
// Example (before → after):
//   %db = arts.db_alloc %ptr sizes(%cN,%cM)
// no uses, no acquires/releases
// becomes: (alloc erased)
bool DbPass::deadDbElimination() {
  bool changed = false;
  auto &dbAnalysis = getAnalysis<DbAnalysis>();
  module.walk([&](func::FuncOp func) {
    auto *graph = dbAnalysis.getOrCreateGraph(func);
    if (!graph)
      return;
    SmallVector<DbAllocNode *> deadAllocs;
    graph->forEachAllocNode([&](DbAllocNode *allocNode) {
      if (allocNode->getAcquireNodesSize() == 0 &&
          allocNode->getReleaseNodesSize() == 0) {
        deadAllocs.push_back(allocNode);
      }
    });
    for (auto *allocNode : deadAllocs) {
      auto dbOp = allocNode->getDbAllocOp();
      if (dbOp && dbOp->use_empty()) {
        dbOp.erase();
        changed = true;
      }
    }
  });
  return changed;
}

// Buffer reuse across non-overlapping lifetimes (disabled scaffolding)
// Intent: reuse storage of two DBs when lifetimes do not overlap and aliasing
// is safe. Requires alias and lifetime queries. Kept as future work.
bool DbPass::bufferReuse() {
  bool changed = false;
  // module.walk([&](func::FuncOp func) {
  //   auto graph = dbAnalysis.getOrCreateGraph(func);
  //   SmallVector<std::pair<DbAllocNode *, DbAllocNode *>> reusePairs;
  //   graph->forEachAllocNode([&](DbAllocNode *alloc1) {
  //     graph->forEachAllocNode([&](DbAllocNode *alloc2) {
  //       if (alloc1 != alloc2 &&
  //       !graph->isAllocReachable(alloc1->getDbAllocOp(),
  //       alloc2->getDbAllocOp()) &&
  //           !graph->isAllocReachable(alloc2->getDbAllocOp(),
  //           alloc1->getDbAllocOp()) &&
  //           dbAnalysis.getAliasAnalysis()->mayAlias(alloc1->getDbAllocOp(),
  //           alloc2->getDbAllocOp())) {
  //         reusePairs.push_back({alloc1, alloc2});
  //       }
  //     });
  //   });
  //   for (auto &[alloc1, alloc2] : reusePairs) {
  //     alloc2->forEachChildNode([&](NodeBase *child) {
  //       if (auto acquire = dyn_cast<DbAcquireNode>(child)) {
  //         acquire->getOp()->setOperand(0,
  //         alloc1->getDbAllocOp().getResult(0));
  //       } else if (auto release = dyn_cast<DbReleaseNode>(child)) {
  //         release->getOp()->setOperand(0,
  //         alloc1->getDbAllocOp().getResult(0));
  //       }
  //     });
  //     alloc2->getDbAllocOp().erase();
  //     changed = true;
  //   }
  // });
  return changed;
}

// Hoist loop-invariant allocations (disabled scaffolding)
// Intent: move DbAllocOps out of loops when invariant to reduce allocation
// overhead. Requires LoopAnalysis integration. Kept as future work.
bool DbPass::hoistAllocs() {
  bool changed = false;
  // module.walk([&](func::FuncOp func) {
  //   auto graph = dbAnalysis.getOrCreateGraph(func);
  //   auto *loopAnalysis = dbAnalysis.getLoopAnalysis();
  //   graph->forEachAllocNode([&](DbAllocNode *allocNode) {
  //     Operation *allocOp = allocNode->getOp();
  //     if (auto enclosingLoop = loopAnalysis->getEnclosingLoop(allocOp)) {
  //       if (loopAnalysis->isLoopInvariant(allocOp, enclosingLoop)) {
  //         allocOp->moveBefore(enclosingLoop);
  //         changed = true;
  //       }
  //     }
  //   });
  // });
  return changed;
}

// Fuse adjacent identical accesses
// Responsibility: within a block, fuse back-to-back identical DbAcquireOps on
// the same parent alloc (replace later result with earlier), and similarly
// collapse consecutive identical DbReleaseOps on the same source.
// Scope: adjacency-only; does not reorder or cross block boundaries.
// Example (before → after):
//   %a0 = arts.db_acquire %db indices(%i,%j) sizes(%c4,%c4)
//   %a1 = arts.db_acquire %db indices(%i,%j) sizes(%c4,%c4)
//   use(%a1)
// becomes
//   %a0 = arts.db_acquire %db indices(%i,%j) sizes(%c4,%c4)
//   use(%a0)
bool DbPass::fuseAccesses() {
  bool changed = false;
  auto &dbAnalysis = getAnalysis<DbAnalysis>();
  auto equalRange = [](ValueRange a, ValueRange b) -> bool {
    if (a.size() != b.size())
      return false;
    for (auto it = a.begin(), jt = b.begin(); it != a.end(); ++it, ++jt) {
      if (*it != *jt)
        return false;
    }
    return true;
  };
  module.walk([&](func::FuncOp func) {
    auto *graph = dbAnalysis.getOrCreateGraph(func);
    if (!graph)
      return;
    for (Block &block : func.getBody().getBlocks()) {
      DbAcquireOp prevAcq = nullptr;
      DbReleaseOp prevRel = nullptr;
      for (Operation &op : block) {
        if (auto acq = dyn_cast<DbAcquireOp>(&op)) {
          if (prevAcq && prevAcq->getNextNode() == &op) {
            auto parentPrev = graph->getParentAlloc(prevAcq.getOperation());
            auto parentNow = graph->getParentAlloc(acq.getOperation());
            if (parentPrev && parentNow && parentPrev == parentNow &&
                prevAcq.getMode() == acq.getMode() &&
                equalRange(prevAcq.getIndices(), acq.getIndices()) &&
                equalRange(prevAcq.getOffsets(), acq.getOffsets()) &&
                equalRange(prevAcq.getSizes(), acq.getSizes())) {
              acq.getPtr().replaceAllUsesWith(prevAcq.getPtr());
              acq.erase();
              changed = true;
              // Do not update prevAcq; continue chaining
              continue;
            }
          }
          prevAcq = acq;
          prevRel = nullptr;
          continue;
        }
        if (auto rel = dyn_cast<DbReleaseOp>(&op)) {
          if (prevRel && prevRel->getNextNode() == &op) {
            // Fuse identical consecutive releases of the same source.
            if (!prevRel.getSources().empty() && !rel.getSources().empty() &&
                prevRel.getSources()[0] == rel.getSources()[0]) {
              rel.erase();
              changed = true;
              continue;
            }
          }
          prevRel = rel;
          prevAcq = nullptr;
          continue;
        }
        prevAcq = nullptr;
        prevRel = nullptr;
      }
    }
  });
  return changed;
}

// Inline single-use allocs (disabled scaffolding)
// Intent: for allocs used by exactly one access, inline the pointer and erase
// the alloc. Requires careful ownership and lifetime checks. Kept as future.
bool DbPass::inlineAllocs() {
  bool changed = false;
  // module.walk([&](func::FuncOp func) {
  //   auto graph = dbAnalysis.getOrCreateGraph(func);
  //   graph->forEachAllocNode([&](DbAllocNode *allocNode) {
  //     if (allocNode->getAcquireNodesSize() + allocNode->getReleaseNodesSize()
  //     == 1) {
  //       NodeBase *child = nullptr;
  //       allocNode->forEachChildNode([&](NodeBase *c) { child = c; });
  //       if (child) {
  //         Operation *childOp = child->getOp();
  //         childOp->setOperand(0, allocNode->getPtr());  // Inline ptr
  //         directly allocNode->getDbAllocOp().erase(); changed = true;
  //       }
  //     }
  //   });
  // });
  return changed;
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbPass() { return std::make_unique<DbPass>(); }
} // namespace arts
} // namespace mlir