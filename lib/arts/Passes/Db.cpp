///==========================================================================
/// File: Db.cpp
/// Pass for DB optimizations and memory management.
///==========================================================================

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
#include "mlir/IR/Visitors.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>
/// File operations
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
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
  DbPass() = default;
  DbPass(bool exportJson) { this->exportJson = exportJson; }

  void runOnOperation() override;

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

  /// Prefetch and overlap: hoist acquires earlier (after operand defs) and
  /// delay releases after last use within a block to overlap transfers.
  bool prefetchOverlap();

  /// Export DB analysis to JSON files in output/ directory.
  void exportToJson();

private:
  ModuleOp module;
};
} // namespace

void DbPass::runOnOperation() {
  bool changed = false;
  module = getOperation();

  ARTS_DEBUG_HEADER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););

  auto &dbAnalysis = getAnalysis<DbAnalysis>();
  module.walk([&](func::FuncOp func) {
    DbGraph *dbGraph = dbAnalysis.getOrCreateGraphNodesOnly(func);
    if (!dbGraph)
      return;
  });

  /// Export JSON if requested
  if (this->exportJson) {
    exportToJson();
  }
  // auto invalidateResults = [&]() {
  //   module.walk(
  //       [&](func::FuncOp func) { (void)dbAnalysis.invalidateGraph(func); });
  // };

  /// Shrink DBs
  // bool shrinkChanged = shrinkDb();
  // changed |= shrinkChanged;
  // if (shrinkChanged)
  //   invalidateResults();

  // bool ctpChanged = convertToParameters();
  // changed |= ctpChanged;
  // if (ctpChanged) {
  //   module.walk([&](func::FuncOp func) {
  //     (void)dbAnalysis.invalidateGraph(func);
  //     (void)dbAnalysis.getOrCreateGraph(func);
  //   });
  // }

  /// bool ctpChanged = convertToParameters();
  /// changed |= prefetchOverlap();
  /// changed |= bufferReuse();
  /// changed |= hoistAllocs();
  /// changed |= fuseAccesses();
  /// changed |= inlineAllocs();

  if (!changed) {
    ARTS_INFO("No changes made to the module");
    markAnalysesPreserved<DbAnalysis>();
  }

  ARTS_DEBUG_FOOTER(DbPass);
  ARTS_DEBUG_REGION(module.dump(););
}

//===----------------------------------------------------------------------===//
// Shrink DBs
// Shrink datablocks to the minimum required size based on acquires/releases.
// The create dbpass creates extremely fine-grained DB acquires (e.g., size-1
// along the last dimension), which can saturate the network. The correct way
// to coarsen is to normalize DB granularity at allocation time (DbAllocOp),
// e.g., switch from NxMxP DBs to N DBs of size MxP, then rewrite
// acquires/releases, EDT operands accordingly and DB uses.
//
// Example:
//   Before:
//     %db = arts.db_alloc %ptr sizes(%cN,%cM,%cP)
//     acquires over (i,j,k)
//   After (conceptual):
//     for i in 0..N-1:
//       %db_i = arts.db_alloc %ptr_i sizes(%cM,%cP)
//     acquires only over (j,k) on %db_i chosen by i
//===----------------------------------------------------------------------===//
bool DbPass::shrinkDb() {
  bool changed = false;
  auto &dbAnalysis = getAnalysis<DbAnalysis>();

  module.walk([&](func::FuncOp func) {
    auto *graph = dbAnalysis.getOrCreateGraphNodesOnly(func);
    if (!graph)
      return;

    func.walk([&](arts::EdtOp edt) {
      Region &region = edt.getRegion();
      Block &body = region.front();
      SmallVector<Value> deps = edt.getDependenciesAsVector();
      unsigned nArgs = body.getNumArguments();
      assert(deps.size() == nArgs && "deps size mismatch");

      for (unsigned idx = 0; idx < nArgs; ++idx) {
        Value dep = deps[idx];
        Value arg = body.getArgument(idx);

        auto acqOp = dep.getDefiningOp<arts::DbAcquireOp>();
        if (!acqOp)
          continue;

        auto *acqNode = graph->getDbAcquireNode(acqOp);
        if (!acqNode)
          continue;

        const DbAcquireInfo &info = acqNode->getInfo();
        const uint64_t rank = info.getRank();
        const uint64_t pinnedCount = info.indices.size();
        assert(pinnedCount <= rank && "pinnedCount must be <= rank");

        /// Skip if there are no indices indices
        if (pinnedCount == 0)
          continue;

        /// Skip if current indices already equal the analyzed indices set
        if (arts::equalRange(acqOp.getIndices(), info.indices))
          continue;

        OpBuilder builder(edt.getContext());
        builder.setInsertionPoint(acqOp);
        Location loc = edt.getLoc();

        SmallVector<Value, 4> newOffsets = info.offsets;
        SmallVector<Value, 4> newSizes = info.sizes;
        /// If offsets/sizes already match the refined shape, skip to avoid
        /// recreating the same acquire and retyping the argument without any
        /// change.
        if (arts::equalRange(acqOp.getOffsets(), ValueRange(newOffsets)) &&
            arts::equalRange(acqOp.getSizes(), ValueRange(newSizes)))
          continue;

        llvm::SmallVector<Value> idxVec(info.indices.begin(),
                                        info.indices.end());
        llvm::SmallVector<Value> offVec(newOffsets.begin(), newOffsets.end());
        llvm::SmallVector<Value> sizVec(newSizes.begin(), newSizes.end());
        auto refined = builder.create<arts::DbAcquireOp>(
            loc, acqOp.getMode(), acqOp.getSourceGuid(), acqOp.getSourcePtr(),
            idxVec, offVec, sizVec);

        deps[idx] = refined.getPtr();
        SmallVector<Value> newOperands;
        if (Value route = edt.getRoute())
          newOperands.push_back(route);
        newOperands.append(deps.begin(), deps.end());
        edt->setOperands(newOperands);

        arg.setType(refined.getPtr().getType());

        // region.walk([&](Operation *op) {
        //   if (auto load = dyn_cast<memref::LoadOp>(op)) {
        //     if (load.getMemref() != arg)
        //       return WalkResult::advance();
        //     SmallVector<Value, 4> idxs;
        //     auto oldIdxs = load.getIndices();
        //     size_t pinnedCountZ = static_cast<size_t>(pinnedCount);
        //     for (size_t d = pinnedCountZ; d < oldIdxs.size(); ++d) {
        //       Value idx = oldIdxs[d];
        //       size_t pos = d - pinnedCountZ;
        //       /// Subtract only when the corresponding offset is non-zero to
        //       /// avoid emitting useless arithmetic.
        //       if (pos < static_cast<size_t>(tailRank) &&
        //           arts::isNonZeroIndex(newOffsets[pos])) {
        //         OpBuilder innerBuilder(op);
        //         idx = innerBuilder.create<arith::SubIOp>(op->getLoc(), idx,
        //                                                  newOffsets[pos]);
        //       }
        //       idxs.push_back(idx);
        //     }
        //     load.getIndicesMutable().assign(idxs);
        //     return WalkResult::advance();
        //   }
        //   if (auto store = dyn_cast<memref::StoreOp>(op)) {
        //     if (store.getMemref() != arg)
        //       return WalkResult::advance();
        //     SmallVector<Value, 4> idxs;
        //     auto oldIdxs = store.getIndices();
        //     size_t pinnedCountZ = static_cast<size_t>(pinnedCount);
        //     for (size_t d = pinnedCountZ; d < oldIdxs.size(); ++d) {
        //       Value idx = oldIdxs[d];
        //       size_t pos = d - pinnedCountZ;
        //       if (pos < static_cast<size_t>(tailRank) &&
        //           arts::isNonZeroIndex(newOffsets[pos])) {
        //         OpBuilder innerBuilder(op);
        //         idx = innerBuilder.create<arith::SubIOp>(op->getLoc(), idx,
        //                                                  newOffsets[pos]);
        //       }
        //       idxs.push_back(idx);
        //     }
        //     store.getIndicesMutable().assign(idxs);
        //     return WalkResult::advance();
        //   }
        //   if (auto gep = dyn_cast<arts::DbGepOp>(op)) {
        //     if (gep.getBasePtr() != arg)
        //       return WalkResult::advance();
        //     SmallVector<Value, 4> idxs;
        //     auto oldIdxs = gep.getIndices();
        //     size_t pinnedCountZ = static_cast<size_t>(pinnedCount);
        //     for (size_t d = pinnedCountZ; d < oldIdxs.size(); ++d) {
        //       Value idx = oldIdxs[d];
        //       size_t pos = d - pinnedCountZ;
        //       if (pos < static_cast<size_t>(tailRank) &&
        //           arts::isNonZeroIndex(newOffsets[pos])) {
        //         OpBuilder innerBuilder(op);
        //         idx = innerBuilder.create<arith::SubIOp>(op->getLoc(), idx,
        //                                                  newOffsets[pos]);
        //       }
        //       idxs.push_back(idx);
        //     }
        //     SmallVector<Value, 4> strides;
        //     auto oldStrides = gep.getStrides();
        //     for (size_t d = static_cast<size_t>(pinnedCount);
        //          d < oldStrides.size(); ++d)
        //       strides.push_back(oldStrides[d]);
        //     OpBuilder innerBuilder(op);
        //     llvm::SmallVector<Value> idxVec2(idxs.begin(), idxs.end());
        //     llvm::SmallVector<Value> strideVec(strides.begin(),
        //     strides.end()); auto newGep = innerBuilder.create<arts::DbGepOp>(
        //         op->getLoc(), arg, idxVec2, strideVec);
        //     op->getResult(0).replaceAllUsesWith(newGep.getResult());
        //     op->erase();
        //     return WalkResult::advance();
        //   }
        //   return WalkResult::advance();
        // });

        acqOp.getGuid().replaceAllUsesWith(refined.getGuid());
        acqOp.getPtr().replaceAllUsesWith(refined.getPtr());

        if (acqOp->use_empty())
          acqOp.erase();

        deps[idx] = refined.getPtr();
        changed = true;
      }
    });
  });

  return changed;
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
//   arts.db_release %a            /// moved after last use
bool DbPass::prefetchOverlap() {
  bool changed = false;
  /// Operate within blocks to preserve simple dominance and avoid CFG changes
  module.walk([&](func::FuncOp func) {
    for (Block &block : func.getBody().getBlocks()) {
      /// Build a position index for stable ordering comparisons
      llvm::DenseMap<Operation *, unsigned> pos;
      unsigned i = 0;
      for (Operation &op : block)
        pos[&op] = i++;

      /// 1) Hoist acquires
      /// Move each acquire as early as possible after the last operand def in
      /// this block. If operands are block arguments or defined above, the
      /// acquire goes to the block front.
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

      /// Rebuild positions after moves
      pos.clear();
      i = 0;
      for (Operation &op : block)
        pos[&op] = i++;

      /// 2) Delay releases
      /// Move each release as late as possible within the same block: just
      /// after the last use of its operand(s). Skip if already in the best
      /// spot.
      SmallVector<DbReleaseOp, 16> releases;
      for (Operation &op : block)
        if (auto rel = dyn_cast<DbReleaseOp>(&op))
          releases.push_back(rel);

      for (DbReleaseOp rel : releases) {
        Operation *op = rel.getOperation();
        unsigned cur = pos[op];
        unsigned lastUsePos = cur;
        /// Consider all operands; typically the acquired view
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

//===----------------------------------------------------------------------===//
// Convert trivial DBs to parameters
// When a DB is effectively read-only and 1D, turn it into a parameter
// to avoid runtime traffic.
// Example:
//   %db = arts.db_alloc %ptr sizes(%cN)
//   %a  = arts.db_acquire %db ...
//   load %a[...]
//   --> convert to a function/block argument replacing %db/%a
//===----------------------------------------------------------------------===//
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
  /// });
  return changed;
}

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
  /// module.walk([&](func::FuncOp func) {
  ///   auto graph = dbAnalysis.getOrCreateGraph(func);
  ///   SmallVector<std::pair<DbAllocNode *, DbAllocNode *>> reusePairs;
  ///   graph->forEachAllocNode([&](DbAllocNode *alloc1) {
  ///     graph->forEachAllocNode([&](DbAllocNode *alloc2) {
  ///       if (alloc1 != alloc2 &&
  ///       !graph->isAllocReachable(alloc1->getDbAllocOp(),
  ///       alloc2->getDbAllocOp()) &&
  ///           !graph->isAllocReachable(alloc2->getDbAllocOp(),
  ///           alloc1->getDbAllocOp()) &&
  ///           dbAnalysis.getAliasAnalysis()->mayAlias(alloc1->getDbAllocOp(),
  ///           alloc2->getDbAllocOp())) {
  ///         reusePairs.push_back({alloc1, alloc2});
  ///       }
  ///     });
  ///   });
  ///   for (auto &[alloc1, alloc2] : reusePairs) {
  ///     alloc2->forEachChildNode([&](NodeBase *child) {
  ///       if (auto acquire = dyn_cast<DbAcquireNode>(child)) {
  ///         acquire->getOp()->setOperand(0,
  ///         alloc1->getDbAllocOp().getResult(0));
  ///       } else if (auto release = dyn_cast<DbReleaseNode>(child)) {
  ///         release->getOp()->setOperand(0,
  ///         alloc1->getDbAllocOp().getResult(0));
  ///       }
  ///     });
  ///     alloc2->getDbAllocOp().erase();
  ///     changed = true;
  ///   }
  /// });
  return changed;
}

// Hoist loop-invariant allocations (disabled scaffolding)
// Intent: move DbAllocOps out of loops when invariant to reduce allocation
// overhead. Requires LoopAnalysis integration. Kept as future work.
bool DbPass::hoistAllocs() {
  bool changed = false;
  /// module.walk([&](func::FuncOp func) {
  ///   auto graph = dbAnalysis.getOrCreateGraph(func);
  ///   auto *loopAnalysis = dbAnalysis.getLoopAnalysis();
  ///   graph->forEachAllocNode([&](DbAllocNode *allocNode) {
  ///     Operation *allocOp = allocNode->getOp();
  ///     if (auto enclosingLoop = loopAnalysis->getEnclosingLoop(allocOp)) {
  ///       if (loopAnalysis->isLoopInvariant(allocOp, enclosingLoop)) {
  ///         allocOp->moveBefore(enclosingLoop);
  ///         changed = true;
  ///       }
  ///     }
  ///   });
  /// });
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
                arts::equalRange(prevAcq.getIndices(), acq.getIndices()) &&
                arts::equalRange(prevAcq.getOffsets(), acq.getOffsets()) &&
                arts::equalRange(prevAcq.getSizes(), acq.getSizes())) {
              acq.getPtr().replaceAllUsesWith(prevAcq.getPtr());
              acq.erase();
              changed = true;
              /// Do not update prevAcq; continue chaining
              continue;
            }
          }
          prevAcq = acq;
          prevRel = nullptr;
          continue;
        }
        if (auto rel = dyn_cast<DbReleaseOp>(&op)) {
          if (prevRel && prevRel->getNextNode() == &op) {
            /// Fuse identical consecutive releases of the same source.
            if (prevRel.getSource() && rel.getSource() &&
                prevRel.getSource() == rel.getSource()) {
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

void DbPass::exportToJson() {
  auto &dbAnalysis = getAnalysis<DbAnalysis>();

  /// Create output directory if it doesn't exist
  std::string outputDir = "output";
  std::error_code ec = llvm::sys::fs::create_directories(outputDir);
  if (ec) {
    ARTS_ERROR("Failed to create output directory: " << ec.message());
    return;
  }

  module.walk([&](func::FuncOp func) {
    DbGraph *dbGraph = dbAnalysis.getOrCreateGraph(func);
    if (!dbGraph)
      return;

    /// Create filename based on function name
    std::string filename = outputDir + "/" + func.getName().str() + "_db.json";

    /// Open file for writing
    std::error_code ec;
    llvm::raw_fd_ostream file(filename, ec);
    if (ec) {
      ARTS_ERROR("Failed to open file " << filename << ": " << ec.message());
      return;
    }

    /// Export the graph to JSON
    dbGraph->exportToJson(file, true);
    file.close();

    ARTS_INFO("Exported DB analysis for function '" << func.getName() << "' to "
                                                    << filename);
  });
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createDbPass() { return std::make_unique<DbPass>(); }
std::unique_ptr<Pass> createDbPass(bool exportJson) {
  return std::make_unique<DbPass>(exportJson);
}
} // namespace arts
} // namespace mlir
