///==========================================================================
/// File: Edt.cpp
///==========================================================================

// TODO: Integrate DbAnalysis to perform dependency analysis before flattening,
// to ensure correct propagation of dependencies and identify opportunities for
// merging or elimination.
// TODO: Implement barrier removal by analyzing surrounding EDTs and converting
// implicit synchronization to explicit database dependencies or events.
// TODO: Enhance flattening to handle arbitrary nesting depths of EDTs, not just
// parallel/single pairs, while preserving semantics.
// TODO: Add dominance and use-def analysis to accurately collect and minimize
// dependencies during union operations.
// TODO: Optimize by merging compatible sibling EDTs (e.g., those with
// non-overlapping dependencies) into fewer tasks for reduced runtime overhead.
// TODO: Handle db_control operations explicitly: propagate them as control
// dependencies and potentially lower them to events or barriers if needed.
// TODO: Add support for dynamic worker counts and load balancing in lowered
// parallel regions.
// TODO: Implement verification checks post-transformation to ensure no
// dependency cycles or lost synchronizations.

/// Dialects
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Support/LLVM.h"
// #include "mlir/Transforms/RegionUtils.h"
// polygeist includes are not used directly here
// #include "polygeist/Dialect.h"
// #include "polygeist/Ops.h"
/// Arts
#include "ArtsPassDetails.h"
// #include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
// Graphs
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtConcurrencyGraph.h"
#include "arts/Analysis/Edt/EdtAnalysis.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(edt);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct EdtPass : public arts::EdtBase<EdtPass> {
  void runOnOperation() override;
  bool lowerParallel(EdtOp &op);
  bool lowerSingle(EdtOp &op);
  bool convertParallelIntoSingle(EdtOp &op);

  bool processParallelEdts();
  bool processSyncTaskEdts();
  bool removeBarriers();

  // Graph-driven cleanups
  bool removeRedundantBarriersWithGraphs(func::FuncOp func,
                                         arts::EdtGraph &graph);

private:
  SetVector<Operation *> opsToRemove;
  ModuleOp module;
};
} // end namespace

/// Converts a parallel EDT region into a sync-task EDT when it contains a
/// single inner `arts.edt` and no other ops beyond terminators/barriers.
/// Responsibility: structural simplification; dependency collection limited to
/// union of inner EDT dependencies. No global analysis here.
bool EdtPass::convertParallelIntoSingle(EdtOp &op) {
  /// Analyze the parallel region to locate the unique single-edt op.
  uint32_t numOps = 0;
  EdtOp singleOp = nullptr;

  /// Iterate over the immediate operations in the region.
  for (auto &block : op.getRegion()) {
    for (auto &inst : block) {
      ++numOps;
      if (auto edt = dyn_cast<arts::EdtOp>(&inst)) {
        if (edt.getType() == arts::EdtType::single) {
          if (singleOp)
            llvm_unreachable(
                "Multiple single ops in parallel op not supported");
          singleOp = edt;
        } else
          return false;

      } else if (isa<arts::YieldOp>(&inst) || isa<arts::BarrierOp>(&inst))
        continue;
      else {
        return false;
      }
    }
  }

  if (!singleOp ||
      numOps < 3) // Account for at least edt, yield, and possibly barrier
    return false;

  ARTS_INFO("Converting parallel EDT into single EDT");
  /// Insert the single operation before the parallel op and remove the "single"
  /// attribute.
  singleOp->moveBefore(op);
  singleOp.setType(arts::EdtType::task);

  /// Set task-sync attribute
  // Already set type to task above

  /// Collect unique dependencies from all inner EDTs
  SetVector<Value> allDeps;
  singleOp.walk([&](EdtOp inner) {
    if (inner == singleOp)
      return WalkResult::advance(); // Skip self
    for (Value dep : inner.getDependenciesVector()) {
      allDeps.insert(dep);
    }
    return WalkResult::advance();
  });

  /// Append the collected dependencies to the outer EDT's operands
  if (!allDeps.empty()) {
    singleOp->insertOperands(singleOp.getNumOperands(), allDeps.getArrayRef());
  }

  /// Mark the parallel op for removal.
  opsToRemove.insert(op);
  return true;
}

/// Lowers a parallel EDT into an arts::EpochOp wrapping an scf.for loop over
/// workers. Responsibility: structural lowering; optional propagation of inner
/// dependencies as a conservative union.
bool EdtPass::lowerParallel(EdtOp &op) {
  ARTS_INFO("Lowering parallel EDT");
  auto loc = op.getLoc();
  OpBuilder builder(op);

  /// Create an arts::EpochOp to scope the parallel work.
  auto epochOp = builder.create<arts::EpochOp>(loc);
  auto &region = epochOp.getRegion();
  if (region.empty())
    region.push_back(new Block());
  Block *newBlock = &region.front();

  /// Set the insertion point to the end of the new block.
  builder.setInsertionPointToEnd(newBlock);

  /// Generate the SCF for-loop.
  /// Build loop bounds: [0, numWorkers) with step = 1.
  Value lowerBound = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value workers = builder.create<arts::GetTotalWorkersOp>(loc);
  Value upperBound =
      builder.create<arith::IndexCastOp>(loc, builder.getIndexType(), workers);
  Value step = builder.create<arith::ConstantIndexOp>(loc, 1);
  auto forOp = builder.create<scf::ForOp>(loc, lowerBound, upperBound, step);

  /// Create terminator for the epochOp.
  builder.create<arts::YieldOp>(loc);

  /// Move Edt op into the loop body.
  op->moveBefore(forOp.getBody(), forOp.getBody()->begin());
  op.setType(arts::EdtType::task); // Set to task type instead

  /// Optionally propagate dependencies (similar to convert)
  SetVector<Value> allDeps;
  op.walk([&](EdtOp inner) {
    if (inner == op)
      return WalkResult::advance();
    for (Value dep : inner.getDependenciesVector()) {
      allDeps.insert(dep);
    }
    return WalkResult::advance();
  });
  if (!allDeps.empty()) {
    op->insertOperands(op.getNumOperands(), allDeps.getArrayRef());
  }

  return true;
}

/// Lower a single EDT: For now, clear single attr and set to sync-task if
/// top-level. Can be enhanced later for more complex single handling.
bool EdtPass::lowerSingle(EdtOp &op) {
  ARTS_INFO("Lowering single EDT");
  op.setType(arts::EdtType::sync);

  /// Propagate inner dependencies similar to parallel handling
  SetVector<Value> allDeps;
  op.walk([&](EdtOp inner) {
    if (inner == op)
      return WalkResult::advance();
    for (Value dep : inner.getDependenciesVector()) {
      allDeps.insert(dep);
    }
    return WalkResult::advance();
  });
  if (!allDeps.empty()) {
    op->insertOperands(op.getNumOperands(), allDeps.getArrayRef());
  }

  return true;
}

/// Remove unconditional barriers that provide no additional ordering beyond
/// computed EDT dependencies (graph-informed pruning).
bool EdtPass::removeBarriers() {
  bool changed = false;
  module.walk([&](func::FuncOp func) {
    // Build graphs on-demand for this function
    std::unique_ptr<arts::DbGraph> dbGraph =
        std::make_unique<arts::DbGraph>(func, /*DbAnalysis*/ nullptr);
    dbGraph->build();

    // If DbGraph could not be constructed, skip creating EdtGraph
    if (!dbGraph) return;
    std::unique_ptr<arts::EdtGraph> edtGraph =
        std::make_unique<arts::EdtGraph>(func, dbGraph.get());
    // Build requires DbGraph; will assert if missing
    edtGraph->build();
    if (edtGraph->getNumTasks() == 0)
      return;

    changed |= removeRedundantBarriersWithGraphs(func, *edtGraph);
  });
  return changed;
}

bool EdtPass::processParallelEdts() {
  /// Gather all parallel-EDT ops in the module.
  SmallVector<EdtOp, 8> parallelOps;
  module.walk([&](EdtOp edt) {
    if (edt.getType() == arts::EdtType::parallel)
      parallelOps.push_back(edt);
  });

  /// - Try to convert it into a single-EDT if it contains exactly one child.
  /// - Otherwise, lower it into a worker-loop enclosed in an EpochOp.
  for (EdtOp op : parallelOps) {
    ARTS_INFO("Processing parallel EDT");
    if (!convertParallelIntoSingle(op))
      lowerParallel(op);
  }
  return true;
}

bool EdtPass::processSyncTaskEdts() {
  ARTS_INFO("Processing sync task EDTs");

  /// If the given single EdtOp is not nested within another EdtOp (i.e., is
  /// top-level), and is marked as sync, embed its region's contents in an
  /// arts::EpochOp. This effectively assigns the work to the master thread,
  /// avoiding unnecessary signal/sync overhead. The EdtOp itself is erased
  /// after its body is moved.
  auto convertToEpoch = [](EdtOp &op) -> bool {
    OpBuilder builder(op);
    /// If the op is not top-level, return false.
    if (op->getParentOfType<EdtOp>())
      return false;

    /// Create an arts::EpochOp and its block
    auto loc = op.getLoc();
    auto epochOp = builder.create<arts::EpochOp>(loc);
    auto &epochBlock = epochOp.getRegion().emplaceBlock();
    builder.setInsertionPointToEnd(&epochBlock);
    builder.create<arts::YieldOp>(loc);

    /// Move all operations except the terminator from the EdtOp's region to the
    /// epoch block
    Block *edtBody = &op.getRegion().front();
    for (Operation &childOp :
         llvm::make_early_inc_range(edtBody->without_terminator())) {
      childOp.moveBefore(epochBlock.getTerminator());
    }

    /// Erase the now-empty EdtOp
    op.erase();

    builder.setInsertionPointAfter(epochOp);
    return true;
  };

  /// Collect all sync task EDTs
  SmallVector<EdtOp, 8> syncTaskOps;
  module.walk([&](EdtOp edt) {
    if (edt.getType() == arts::EdtType::task &&
        edt.getType() == arts::EdtType::sync)
      syncTaskOps.push_back(edt);
  });

  if (syncTaskOps.empty())
    return false;

  /// Try to convert each sync task-EDT to an EpochOp.
  for (EdtOp op : syncTaskOps) {
    op.setType(arts::EdtType::task);
    convertToEpoch(op);
  }
  return true;
}

void EdtPass::runOnOperation() {
  module = getOperation();
  ARTS_DEBUG_HEADER(EdtPass);
  ARTS_DEBUG_REGION(module->dump(););

  processParallelEdts();
  // Graph-informed cleanups
  (void)removeBarriers();
  processSyncTaskEdts();

  /// Remove all operations that were marked for deletion during conversion or
  /// lowering
  OpBuilder builder(module.getContext());
  removeOps(module, builder, opsToRemove);

  // Optional debug/JSON emission of concurrency graph per function
  module.walk([&](func::FuncOp func) {
    std::unique_ptr<arts::DbGraph> dbGraph =
        std::make_unique<arts::DbGraph>(func, /*DbAnalysis*/ nullptr);
    dbGraph->build();
    if (!dbGraph)
      return;
    std::unique_ptr<arts::EdtGraph> edtGraph =
        std::make_unique<arts::EdtGraph>(func, dbGraph.get());
    edtGraph->build();
    if (edtGraph->getNumTasks() == 0)
      return;

    arts::EdtAnalysis edtAnalysis(module);
    edtAnalysis.analyze();
    arts::EdtConcurrencyGraph cgraph(edtGraph.get(), &edtAnalysis);
    cgraph.build();

    ARTS_DEBUG_SECTION("edt-concurrency", {
      std::string s;
      llvm::raw_string_ostream os(s);
      cgraph.print(os);
      ARTS_DBGS() << os.str();
    });

    // Emit combined JSON bundle (function, edts, concurrency)
    std::string js;
    llvm::raw_string_ostream jos(js);
    jos << "{\n  \"function\": \"" << func.getName() << "\",\n";
    edtAnalysis.emitEdtsArray(func, jos);
    jos << ",\n  \"concurrency\": ";
    {
      std::string cg;
      llvm::raw_string_ostream cgos(cg);
      cgraph.exportToJson(cgos);
      cgos.flush();
      // cgraph JSON is a full object; insert as-is but without outer braces
      if (cg.size() >= 2 && cg.front() == '{' && cg.back() == '}') {
        jos << cg;
      } else {
        jos << "{}";
      }
    }
    jos << "\n}\n";
    jos.flush();

    ARTS_DEBUG_SECTION("edt-concurrency-json", { ARTS_DBGS() << js; });
  });

  ARTS_DEBUG_FOOTER(EdtPass);
  ARTS_DEBUG_REGION(module->dump(););
}

bool EdtPass::removeRedundantBarriersWithGraphs(func::FuncOp func,
                                                arts::EdtGraph &graph) {
  bool changed = false;

  // Collect barriers within this function and check redundancy
  SmallVector<arts::BarrierOp, 8> toErase;
  func.walk([&](arts::BarrierOp barrier) {
    Block *block = barrier->getBlock();
    // Partition EDTs in the same block into before/after
    SmallVector<arts::EdtOp, 8> beforeTasks;
    SmallVector<arts::EdtOp, 8> afterTasks;
    bool pastBarrier = false;
    for (Operation &op : *block) {
      if (&op == barrier.getOperation()) {
        pastBarrier = true;
        continue;
      }
      if (auto edt = dyn_cast<arts::EdtOp>(&op)) {
        (pastBarrier ? afterTasks : beforeTasks).push_back(edt);
      }
    }

    if (beforeTasks.empty() || afterTasks.empty()) return;

    bool redundant = true;
    for (arts::EdtOp a : beforeTasks) {
      for (arts::EdtOp b : afterTasks) {
        if (!graph.isTaskReachable(a, b)) {
          redundant = false;
          break;
        }
      }
      if (!redundant) break;
    }

    if (redundant) toErase.push_back(barrier);
  });

  for (auto b : toErase) {
    ARTS_INFO("Removing redundant barrier");
    b.erase();
    changed = true;
  }
  return changed;
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createEdtPass() { return std::make_unique<EdtPass>(); }
} // namespace arts
} // namespace mlir