///==========================================================================///
/// File: EpochOpt.cpp
///
/// Combined epoch optimizations pass that performs:
/// 1. Epoch fusion - merge independent consecutive epochs
/// 2. Worker loop fusion - merge compatible worker loops within epochs
///
/// Example:
///   Before:
///     arts.epoch { ... }
///     arts.epoch { ... }   // independent
///
///   After:
///     arts.epoch { ... ... }   // fused
///==========================================================================///

#include "../../ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/DbUtils.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"

#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/LoopUtils.h"
ARTS_DEBUG_SETUP(arts_epoch_opt);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Shared Utilities
///===----------------------------------------------------------------------===///

using AccessMap = DenseMap<Operation *, bool>;

static bool isWriter(ArtsMode mode) {
  if (mode == ArtsMode::uninitialized)
    return true;
  return DbUtils::isWriterMode(mode);
}

///===----------------------------------------------------------------------===///
/// Epoch Fusion
///===----------------------------------------------------------------------===///
///
/// Two epochs can be fused when they access disjoint datablocks, or when any
/// shared datablocks are read-only in both epochs. This preserves concurrency
/// while removing unnecessary barriers between epochs.
///
/// BEFORE:
///   arts.epoch {
///     // accesses datablock A[read]
///     %acq_a = arts.db_acquire [in] %db_a
///     arts.edt(%acq_a) { read from A }
///     arts.db_release %acq_a
///   }
///   arts.epoch {
///     // accesses datablock B[write]
///     %acq_b = arts.db_acquire [out] %db_b
///     arts.edt(%acq_b) { write to B }
///     arts.db_release %acq_b
///   }
///
/// AFTER:
///   arts.epoch {
///     // disjoint DBs fused into single epoch
///     %acq_a = arts.db_acquire [in] %db_a
///     arts.edt(%acq_a) { read from A }
///     arts.db_release %acq_a
///     %acq_b = arts.db_acquire [out] %db_b
///     arts.edt(%acq_b) { write to B }
///     arts.db_release %acq_b
///   }
///
///===----------------------------------------------------------------------===///

/// Collects all datablock accesses within an epoch.
/// Returns a map from allocation to whether any access is a write.
static AccessMap collectAccesses(EpochOp epoch) {
  AccessMap accesses;
  epoch.walk([&](DbAcquireOp acq) {
    Operation *alloc = DbUtils::getUnderlyingDbAlloc(acq.getPtr());
    if (!alloc)
      return;
    bool write = isWriter(acq.getMode());
    auto it = accesses.find(alloc);
    if (it == accesses.end())
      accesses[alloc] = write;
    else if (write)
      it->second = true;
  });
  return accesses;
}

/// Determines if two epochs can be safely fused.
/// Returns true if epochs access disjoint datablocks or shared DBs are
/// read-only.
static bool canFuseEpochs(EpochOp first, EpochOp second) {
  AccessMap a = collectAccesses(first);
  AccessMap b = collectAccesses(second);
  for (auto &entry : a) {
    Operation *db = entry.first;
    bool writeA = entry.second;
    auto it = b.find(db);
    if (it == b.end())
      continue;
    bool writeB = it->second;
    if (writeA || writeB)
      return false;
  }
  return true;
}

/// Fuses two epochs by moving all operations from the second epoch
/// into the first epoch's body (before the terminator).
static void fuseEpochs(EpochOp first, EpochOp second) {
  Block &firstBlock = first.getBody().front();
  Block &secondBlock = second.getBody().front();
  Operation *firstTerminator = firstBlock.getTerminator();

  for (Operation &op :
       llvm::make_early_inc_range(secondBlock.without_terminator())) {
    op.moveBefore(firstTerminator);
  }

  second.erase();
}

/// Processes a block to fuse consecutive compatible epochs.
/// Returns true if any fusion was performed.
static bool processBlockForEpochFusion(Block &block) {
  bool changed = false;
  for (auto it = block.begin(); it != block.end();) {
    auto *op = &*it;
    auto first = dyn_cast<EpochOp>(op);
    if (!first) {
      ++it;
      continue;
    }
    auto nextIt = std::next(it);
    if (nextIt == block.end()) {
      ++it;
      continue;
    }
    auto second = dyn_cast<EpochOp>(&*nextIt);
    if (!second) {
      ++it;
      continue;
    }
    if (!canFuseEpochs(first, second)) {
      ++it;
      continue;
    }

    ARTS_DEBUG("Fusing consecutive epochs");
    fuseEpochs(first, second);
    changed = true;
    /// Re-run on the same first epoch to chain-fuse.
  }
  return changed;
}

/// Recursively processes all blocks in a region for epoch fusion.
static void processRegionForEpochFusion(Region &region, bool &changed) {
  for (Block &block : region) {
    changed |= processBlockForEpochFusion(block);
    for (Operation &op : block) {
      for (Region &nested : op.getRegions())
        processRegionForEpochFusion(nested, changed);
    }
  }
}

///===----------------------------------------------------------------------===///
/// Worker Loop Fusion
///===----------------------------------------------------------------------===///
///
/// Fuses consecutive worker loops inside an arts.epoch when they have identical
/// bounds and no loop-carried values. This interleaves task launches per worker
/// and reduces epoch-level synchronization overhead.
///
/// BEFORE:
///   arts.epoch {
///     scf.for %w = 0 to %workers {
///       arts.edt { task A }
///     }
///     scf.for %w = 0 to %workers {
///       arts.edt { task B }
///     }
///   }
///
/// AFTER:
///   arts.epoch {
///     scf.for %w = 0 to %workers {
///       arts.edt { task A }
///       arts.edt { task B }           // both tasks launched per worker
///     }
///   }
///
///===----------------------------------------------------------------------===///

/// Determines if two worker loops can be fused.
/// Requirements: both are worker loops, no init args, no results,
/// same induction variable type, compatible bounds.
static bool isFusableWorkerLoop(scf::ForOp a, scf::ForOp b) {
  if (!isWorkerLoop(a) || !isWorkerLoop(b))
    return false;
  if (!a.getInitArgs().empty() || !b.getInitArgs().empty())
    return false;
  if (a.getNumResults() != 0 || b.getNumResults() != 0)
    return false;
  if (a.getInductionVar().getType() != b.getInductionVar().getType())
    return false;
  return haveCompatibleBounds(a, b);
}

/// Fuses two loops by moving operations from the second loop's body
/// into the first loop's body and replacing the induction variable.
static void fuseLoops(scf::ForOp first, scf::ForOp second) {
  Block &firstBody = first.getRegion().front();
  Block &secondBody = second.getRegion().front();
  Operation *firstTerminator = firstBody.getTerminator();

  for (Operation &op :
       llvm::make_early_inc_range(secondBody.without_terminator())) {
    op.moveBefore(firstTerminator);
  }

  Value oldIv = secondBody.getArgument(0);
  Value newIv = firstBody.getArgument(0);
  oldIv.replaceAllUsesWith(newIv);

  second.erase();
}

/// Fuses consecutive compatible worker loops within an epoch.
/// Returns true if any fusion was performed.
static bool fuseWorkerLoopsInEpoch(EpochOp epoch) {
  Block &body = epoch.getBody().front();
  bool changed = false;
  for (auto it = body.begin(); it != body.end();) {
    auto first = dyn_cast<scf::ForOp>(&*it);
    if (!first) {
      ++it;
      continue;
    }
    auto nextIt = std::next(it);
    if (nextIt == body.end()) {
      ++it;
      continue;
    }
    auto second = dyn_cast<scf::ForOp>(&*nextIt);
    if (!second) {
      ++it;
      continue;
    }

    if (!isFusableWorkerLoop(first, second)) {
      ++it;
      continue;
    }

    ARTS_DEBUG("Fusing consecutive worker loops in epoch");
    fuseLoops(first, second);
    changed = true;
    /// Re-run on the same first loop to chain-fuse.
  }
  return changed;
}

///===----------------------------------------------------------------------===///
/// Pass Definition
///===----------------------------------------------------------------------===///

namespace {
struct EpochOptPass : public arts::ArtsEpochOptBase<EpochOptPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool changed = false;

    if (enableEpochFusion) {
      processRegionForEpochFusion(module.getRegion(), changed);
      if (changed)
        ARTS_INFO("Epoch fusion applied");
    }

    if (enableLoopFusion) {
      bool loopChanged = false;
      module.walk(
          [&](EpochOp epoch) { loopChanged |= fuseWorkerLoopsInEpoch(epoch); });
      if (loopChanged) {
        ARTS_INFO("Epoch worker loop fusion applied");
        changed = true;
      }
    }
  }
};
} // namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createEpochOptPass() {
  return std::make_unique<EpochOptPass>();
}
} // namespace arts
} // namespace mlir
