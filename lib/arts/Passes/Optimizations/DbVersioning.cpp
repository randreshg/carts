///===========================================================================
/// DbVersioning.cpp - Insert datablock copies for mixed access patterns
///
/// Creates arts.db_copy and arts.db_sync operations for allocations that
/// mix direct chunked writes with indirect indexed reads.
///===========================================================================

#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallVector.h"

ARTS_DEBUG_SETUP(db_versioning);

using namespace mlir;
using namespace mlir::arts;

namespace {
struct DbVersioningPass : public arts::DbVersioningBase<DbVersioningPass> {
  DbVersioningPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override;

private:
  ArtsAnalysisManager *AM = nullptr;
};
} // namespace

void DbVersioningPass::runOnOperation() {
  ModuleOp module = getOperation();
  bool changed = false;

  module.walk([&](func::FuncOp func) {
    DbGraph &graph = AM->getDbGraph(func);

    graph.forEachAllocNode([&](DbAllocNode *allocNode) {
      if (!allocNode)
        return;

      DbAllocOp allocOp = allocNode->getDbAllocOp();
      if (!allocOp || allocOp.getElementSizes().empty())
        return;

      // Only handle rank-1 copies for now
      if (allocOp.getElementSizes().size() != 1)
        return;

      bool hasChunked = false;
      SmallVector<DbAcquireOp, 4> indirectReads;

      allocNode->forEachChildNode([&](NodeBase *child) {
        auto *acqNode = dyn_cast<DbAcquireNode>(child);
        if (!acqNode)
          return;
        DbAcquireOp acqOp = acqNode->getDbAcquireOp();
        if (!acqOp)
          return;

        auto mode = DatablockUtils::getPartitionMode(acqOp);
        if (mode == PartitionMode::Chunked)
          hasChunked = true;

        if (acqOp.getMode() == ArtsMode::in) {
          bool isIndirect =
              acqNode->hasIndirectAccess() || !acqOp.getIndices().empty();
          if (isIndirect)
            indirectReads.push_back(acqOp);
        }
      });

      if (!hasChunked || indirectReads.empty())
        return;

      ARTS_DEBUG("DbVersioning: creating copy for alloc " << allocOp);

      OpBuilder builder(allocOp);
      builder.setInsertionPointAfter(allocOp);

      auto copyOp = builder.create<DbCopyOp>(
          allocOp.getLoc(), allocOp.getGuid().getType(),
          allocOp.getPtr().getType(), allocOp.getPtr(),
          PromotionMode::fine_grained);

      Value copyGuid = copyOp.getGuid();
      Value copyPtr = copyOp.getPtr();

      for (DbAcquireOp acqOp : indirectReads) {
        builder.setInsertionPoint(acqOp);

        // Get per-acquire hints for partial sync (set by ForLowering)
        SmallVector<Value> offsets(acqOp.getOffsetHints().begin(),
                                   acqOp.getOffsetHints().end());
        SmallVector<Value> sizes(acqOp.getSizeHints().begin(),
                                 acqOp.getSizeHints().end());

        bool fullSync = offsets.empty();
        builder.create<DbSyncOp>(acqOp.getLoc(), copyPtr, allocOp.getPtr(),
                                 ValueRange(offsets), ValueRange(sizes),
                                 fullSync);

        acqOp.getSourceGuidMutable().assign(copyGuid);
        acqOp.getSourcePtrMutable().assign(copyPtr);
      }

      changed = true;
    });
  });

  if (changed)
    AM->invalidate();
}

std::unique_ptr<Pass> mlir::arts::createDbVersioningPass(
    ArtsAnalysisManager *AM) {
  return std::make_unique<DbVersioningPass>(AM);
}
