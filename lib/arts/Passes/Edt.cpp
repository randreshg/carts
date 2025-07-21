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
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/RegionUtils.h"
#include "polygeist/Dialect.h"
#include "polygeist/Ops.h"
/// Arts
#include "ArtsPassDetails.h"
// #include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>

#define DEBUG_TYPE "edt"
#define LINE "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

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

private:
  SetVector<Operation *> opsToRemove;
  ModuleOp module;
};
} // end namespace

/// Converts a parallel EDT region into a sync-task EDT by locating its unique
/// single-edt operation, refactoring the IR to reflect a single-threaded
/// execution model with flattened nesting, and propagating unioned
/// dependencies. Conversion is performed only if the parallel region contains
/// only one single-edt and no unsupported operations. Dependencies from inner
/// EDTs are collected and added to the outer EDT.
bool EdtPass::convertParallelIntoSingle(EdtOp &op) {
  /// Analyze the parallel region to locate the unique single-edt op.
  uint32_t numOps = 0;
  EdtOp singleOp = nullptr;

  /// Iterate over the immediate operations in the region.
  for (auto &block : op.getRegion()) {
    for (auto &inst : block) {
      ++numOps;
      if (auto edt = dyn_cast<arts::EdtOp>(&inst)) {
        if (edt.isSingle()) {
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

  LLVM_DEBUG(DBGS() << "Converting parallel EDT into single EDT\n");
  /// Insert the single operation before the parallel op and remove the "single"
  /// attribute.
  singleOp->moveBefore(op);
  singleOp.clearIsSingleAttr();

  /// Set task-sync attribute
  singleOp.setIsTaskAttr();
  singleOp.setIsSyncAttr();

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

/// Lower a parallel EDT into an arts::EpochOp wrapping an scf.for loop over
/// workers. Enhanced to set sync-task attrs and propagate dependencies if
/// applicable.
bool EdtPass::lowerParallel(EdtOp &op) {
  LLVM_DEBUG(dbgs() << "Lowering parallel EDT\n");
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
  op.clearIsParallelAttr();
  op.setIsTaskAttr();
  op.setIsSyncAttr(); // Enhanced: Set sync for consistency with converted cases

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
  LLVM_DEBUG(DBGS() << "Lowering single EDT\n");
  op.clearIsSingleAttr();
  op.setIsTaskAttr();
  op.setIsSyncAttr();

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

/// New function: Remove barriers by assuming dependencies handle
/// synchronization. For now, simply erase them; enhance later with analysis.
bool EdtPass::removeBarriers() {
  SmallVector<BarrierOp, 8> barriers;
  module.walk([&](BarrierOp barrier) { barriers.push_back(barrier); });

  for (BarrierOp barrier : barriers) {
    LLVM_DEBUG(DBGS() << "Removing barrier\n");
    barrier.erase();
  }
  return true;
}

bool EdtPass::processParallelEdts() {
  /// Gather all parallel-EDT ops in the module.
  SmallVector<EdtOp, 8> parallelOps;
  module.walk([&](EdtOp edt) {
    if (edt.isParallel())
      parallelOps.push_back(edt);
  });

  /// - Try to convert it into a single-EDT if it contains exactly one child.
  /// - Otherwise, lower it into a worker-loop enclosed in an EpochOp.
  for (EdtOp op : parallelOps) {
    LLVM_DEBUG(DBGS() << "Processing parallel EDT\n");
    if (!convertParallelIntoSingle(op))
      lowerParallel(op);
  }
  return true;
}

bool EdtPass::processSyncTaskEdts() {
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

  /// Collect all single-EDT ops in the module. Enhanced: Collect sync-task
  /// regardless of single attr.
  SmallVector<EdtOp, 8> syncTaskOps;
  module.walk([&](EdtOp edt) {
    if (edt.isTask() && edt.isSync())
      syncTaskOps.push_back(edt);
  });

  /// Try to convert each sync task-EDT to an EpochOp.
  for (EdtOp op : syncTaskOps)
    convertToEpoch(op);
  return true;
}

void EdtPass::runOnOperation() {
  module = getOperation();
  LLVM_DEBUG({
    dbgs() << "\n" << LINE << "EdtPass STARTED\n" << LINE;
    module->dump();
  });

  processParallelEdts();
  removeBarriers();      // New: Remove barriers after processing parallels
  processSyncTaskEdts(); // Uncommented and enhanced

  /// Remove all operations that were marked for deletion during conversion or
  /// lowering
  OpBuilder builder(module.getContext());
  removeOps(module, builder, opsToRemove);

  LLVM_DEBUG({
    dbgs() << LINE << "EdtPass FINISHED\n" << LINE;
    module->dump();
  });
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createEdtPass() { return std::make_unique<EdtPass>(); }
} // namespace arts
} // namespace mlir