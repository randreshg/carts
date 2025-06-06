///==========================================================================
/// File: Edt.cpp
///==========================================================================

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
#include "arts/Analysis/DbAnalysis.h"
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
#define line "-----------------------------------------\n"
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
  // void handleParallel(EdtOp &op);
  // void handleSingle(EdtOp &op);
  // void handleEdt(EdtOp &op);
  bool lowerParallel(EdtOp &op);
  bool lowerSingle(EdtOp &op);
  bool convertParallelIntoSingle(EdtOp &op);

  bool processParallelEdts();
  bool processSyncTaskEdts();

private:
  SetVector<Operation *> opsToRemove;
  ModuleOp module;
};
} // end namespace

/// Converts a parallel EDT region into a single EDT by locating its unique
/// single-edt operation and refactoring the IR to reflect a single-threaded
/// execution model. Conversion is performed only if the parallel region
/// contains only one single-edt and no other operations.
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

  if (!singleOp || numOps != 4)
    return false;

  LLVM_DEBUG(DBGS() << "Converting parallel EDT into single EDT\n");
  /// Insert the single operation before the parallel op and remove the "single"
  /// attribute.
  singleOp->moveBefore(op);
  singleOp.clearIsSingleAttr();

  /// Set task-sync attribute
  singleOp.setIsTaskAttr();
  singleOp.setIsSyncAttr();

  /// Mark the parallel op for removal.
  opsToRemove.insert(op);
  return true;
}

/// Lower a parallel EDT into an arts::EpochOp wrapping an scf.for loop over
/// workers.
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
  return true;
}

bool EdtPass::lowerSingle(EdtOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering single EDT\n");
  llvm_unreachable("Lowering single EDT not implemented");
  return false;
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

  /// Collect all single-EDT ops in the module.
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
    dbgs() << "\n" << line << "EdtPass STARTED\n" << line;
    module->dump();
  });

  processParallelEdts();
  // processSyncTaskEdts();

  /// Remove all operations that were marked for deletion during conversion or
  /// lowering
  OpBuilder builder(module.getContext());
  removeOps(module, builder, opsToRemove);

  LLVM_DEBUG({
    dbgs() << line << "EdtPass FINISHED\n" << line;
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
