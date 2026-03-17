///==========================================================================///
/// File: EpochOpt.cpp
///
/// Combined epoch optimizations pass that performs:
/// 1. Epoch scope narrowing - split large epochs into independent sub-epochs
/// 2. Epoch fusion - merge independent consecutive epochs
/// 3. Worker loop fusion - merge compatible worker loops within epochs
///
/// Example (narrowing):
///   Before:
///     arts.epoch {
///       // group A: EDT + acquires for DB_a
///       // group B: EDT + acquires for DB_b (no dependency on group A)
///     }
///
///   After:
///     arts.epoch { // group A only }
///     arts.epoch { // group B only }
///
/// Example (fusion):
///   Before:
///     arts.epoch { ... }
///     arts.epoch { ... }   // independent
///
///   After:
///     arts.epoch { ... ... }   // fused
///==========================================================================///

#include "arts/passes/PassDetails.h"
#include "arts/Dialect.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/passes/Passes.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/EdtUtils.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

#include "arts/utils/Debug.h"
#include "arts/utils/LoopUtils.h"
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
/// Epoch Scope Narrowing
///===----------------------------------------------------------------------===///
///
/// Splits a single large epoch into multiple smaller epochs when the operations
/// inside form independent groups. A "cut point" is a position in the epoch's
/// operation list where no operation after the cut depends on any value defined
/// by an operation before the cut. Splitting at cut points creates smaller
/// barrier scopes, enabling better pipelining across epoch boundaries.
///
/// BEFORE:
///   arts.epoch {
///     %acq_a = arts.db_acquire [out] %db_a
///     arts.edt(%acq_a) { write to A }
///     arts.db_release %acq_a
///     %acq_b = arts.db_acquire [out] %db_b     // no dep on group A
///     arts.edt(%acq_b) { write to B }
///     arts.db_release %acq_b
///   }
///
/// AFTER:
///   arts.epoch {
///     %acq_a = arts.db_acquire [out] %db_a
///     arts.edt(%acq_a) { write to A }
///     arts.db_release %acq_a
///   }
///   arts.epoch {
///     %acq_b = arts.db_acquire [out] %db_b
///     arts.edt(%acq_b) { write to B }
///     arts.db_release %acq_b
///   }
///
///===----------------------------------------------------------------------===///

/// Find cut points in an epoch's operation list.
/// A cut point at index i means ops [0..i) form an independent group from
/// ops [i..n). Returns indices where cuts can be made.
static SmallVector<unsigned> findEpochCutPoints(ArrayRef<Operation *> ops) {
  SmallVector<unsigned> cutPoints;
  if (ops.size() <= 1)
    return cutPoints;

  /// For each position i (1..n-1), check whether any op at position >= i
  /// depends on any op at position < i.
  /// Use an incremental approach: maintain the set of values defined by
  /// ops [0..i), then check if ops [i..n) use any of them.
  unsigned n = ops.size();

  /// For efficiency, precompute for each op which other ops (by index) it
  /// depends on. An op at index j depends on op at index k if any operand
  /// of j (or its nested ops) is a result of ops[k].
  ///
  /// Then a cut at position i is valid iff for all j >= i, none of j's
  /// dependencies have index < i.

  /// Build a map: Operation* -> index for quick lookup.
  DenseMap<Operation *, unsigned> opIndex;
  for (unsigned k = 0; k < n; ++k)
    opIndex[ops[k]] = k;

  /// For each op, compute the maximum index of any op it depends on.
  /// If maxDep[j] < i for all j >= i, then i is a valid cut point.
  /// We compute maxDep bottom-up.
  /// maxDep[j] = max index of any epoch-body op that ops[j] depends on.
  /// If ops[j] has no dependencies within the epoch, maxDep[j] stays 0.
  SmallVector<unsigned> maxDep(n, 0);
  for (unsigned j = 0; j < n; ++j) {
    unsigned depMax = 0;

    /// Check direct operands.
    auto checkOperand = [&](Value operand) {
      if (auto *defOp = operand.getDefiningOp()) {
        auto it = opIndex.find(defOp);
        if (it != opIndex.end())
          depMax = std::max(depMax, it->second);
      }
    };

    for (Value operand : ops[j]->getOperands())
      checkOperand(operand);

    /// Check operands of nested operations.
    ops[j]->walk([&](Operation *nested) {
      if (nested == ops[j])
        return WalkResult::advance();
      for (Value operand : nested->getOperands())
        checkOperand(operand);
      return WalkResult::advance();
    });

    /// Check DB-level dependencies: if ops[j] and any earlier ops[k]
    /// access the same DB allocation and at least one is a writer,
    /// treat them as dependent to prevent unsafe epoch splitting.
    for (unsigned k = 0; k < j; ++k) {
      if (DbAnalysis::hasDbConflict(ops[j], ops[k]))
        depMax = std::max(depMax, k);
    }

    maxDep[j] = depMax;
  }

  /// A cut at position i is valid iff for all j >= i: maxDep[j] < i.
  /// Equivalently: max(maxDep[i..n)) < i.
  /// Compute suffix max of maxDep.
  SmallVector<unsigned> suffixMax(n);
  suffixMax[n - 1] = maxDep[n - 1];
  for (int k = (int)n - 2; k >= 0; --k)
    suffixMax[k] = std::max(maxDep[k], suffixMax[k + 1]);

  /// Position i is a valid cut if suffixMax[i] < i.
  for (unsigned i = 1; i < n; ++i) {
    if (suffixMax[i] < i) {
      cutPoints.push_back(i);
    }
  }

  return cutPoints;
}

/// Attempt to narrow (split) a single epoch into multiple smaller epochs.
/// Returns true if the epoch was split.
static bool narrowEpoch(EpochOp epoch) {
  Block &body = epoch.getBody().front();

  /// Collect non-terminator operations in order.
  SmallVector<Operation *> ops;
  for (Operation &op : body.without_terminator())
    ops.push_back(&op);

  if (ops.size() <= 1)
    return false;

  /// Find cut points.
  SmallVector<unsigned> cutPoints = findEpochCutPoints(ops);
  if (cutPoints.empty())
    return false;

  ARTS_DEBUG("Found " << cutPoints.size() << " cut point(s) in epoch");

  /// We will split the epoch into (cutPoints.size() + 1) new epochs.
  /// Build groups: group 0 = [0, cutPoints[0]),
  ///               group 1 = [cutPoints[0], cutPoints[1]), ...
  ///               group k = [cutPoints[k-1], ops.size())
  SmallVector<std::pair<unsigned, unsigned>> groups;
  unsigned start = 0;
  for (unsigned cp : cutPoints) {
    groups.push_back({start, cp});
    start = cp;
  }
  groups.push_back({start, (unsigned)ops.size()});

  /// If only one group, no split needed (shouldn't happen with non-empty
  /// cutPoints, but be safe).
  if (groups.size() <= 1)
    return false;

  Location loc = epoch.getLoc();

  /// Create new epochs for each group. The first group reuses the original
  /// epoch to preserve SSA dominance of the epochGuid if it's used.
  /// Check if the epochGuid result is used outside.
  bool epochGuidUsed = !epoch.getEpochGuid().use_empty();

  if (epochGuidUsed) {
    /// If the epoch GUID is used externally, we cannot safely split because
    /// multiple new epochs would each produce their own GUID. Be conservative.
    ARTS_DEBUG("Epoch GUID is used externally, skipping narrowing");
    return false;
  }

  /// Create new epochs for groups [1..n) and move ops into them.
  /// Keep group 0 in the original epoch.
  /// Insert each new epoch after the original (and after previously created
  /// new epochs) so they appear in program order.
  OpBuilder builder(epoch->getBlock(), std::next(Block::iterator(epoch)));
  for (unsigned g = 1; g < groups.size(); ++g) {
    auto [gStart, gEnd] = groups[g];
    auto newEpoch = builder.create<EpochOp>(loc);
    auto &newBlock = newEpoch.getBody().emplaceBlock();
    OpBuilder innerBuilder(&newBlock, newBlock.begin());
    innerBuilder.create<YieldOp>(loc);

    /// Move operations for this group into the new epoch.
    Operation *terminator = newBlock.getTerminator();
    for (unsigned i = gStart; i < gEnd; ++i)
      ops[i]->moveBefore(terminator);

    /// Advance builder insertion point past this new epoch for the next group.
    builder.setInsertionPointAfter(newEpoch);
  }

  /// Now trim the original epoch to only contain group 0's operations.
  /// Operations for groups [1..n) have already been moved out; the original
  /// epoch body now only has group 0 ops plus the terminator.
  /// Nothing else to do for group 0.

  ARTS_DEBUG("Split epoch into " << groups.size() << " narrower epochs");
  return true;
}

/// Walk all epochs in the module and attempt to narrow them.
/// Processes epochs bottom-up so inner epochs are narrowed before outer ones.
static void narrowEpochScopes(ModuleOp module, bool &changed) {
  SmallVector<EpochOp> epochs;
  module.walk([&](EpochOp epoch) { epochs.push_back(epoch); });

  for (EpochOp epoch : epochs) {
    /// Skip if the epoch was erased by a previous narrowing.
    if (!epoch->getBlock())
      continue;
    changed |= narrowEpoch(epoch);
  }
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
    Operation *alloc = DbUtils::getUnderlyingDbAlloc(acq.getSourcePtr());
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

/// Determines if two pre-computed access maps are compatible for fusion.
/// Returns true if the epochs access disjoint datablocks or shared DBs are
/// read-only in both.
static bool canFuseWithCachedAccesses(const AccessMap &a, const AccessMap &b) {
  for (const auto &entry : a) {
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
  /// Guard: do not fuse if the second epoch's GUID is used externally
  /// (e.g., by WaitOnEpochOp). Erasing it would leave dangling references.
  if (!second.getEpochGuid().use_empty())
    return;

  Block &firstBlock = first.getBody().front();
  Block &secondBlock = second.getBody().front();
  spliceBodyBeforeTerminator(secondBlock, firstBlock);

  second.erase();
}

/// Processes a block to fuse consecutive compatible epochs.
/// Returns true if any fusion was performed.
/// Uses a cached AccessMap for the first epoch to avoid recomputing accesses
/// on each chain-fusion step.
static bool processBlockForEpochFusion(Block &block) {
  bool changed = false;
  for (auto it = block.begin(); it != block.end();) {
    auto first = dyn_cast<EpochOp>(&*it);
    if (!first) {
      ++it;
      continue;
    }

    /// Cache first's accesses for chain-fusion.
    AccessMap firstAccesses = collectAccesses(first);
    auto nextIt = std::next(it);
    while (nextIt != block.end()) {
      auto second = dyn_cast<EpochOp>(&*nextIt);
      if (!second)
        break;
      AccessMap secondAccesses = collectAccesses(second);
      if (!canFuseWithCachedAccesses(firstAccesses, secondAccesses))
        break;

      ARTS_DEBUG("Fusing consecutive epochs");
      fuseEpochs(first, second);

      /// Merge second's accesses into first's cache.
      for (auto &entry : secondAccesses) {
        auto cacheIt = firstAccesses.find(entry.first);
        if (cacheIt == firstAccesses.end())
          firstAccesses[entry.first] = entry.second;
        else if (entry.second)
          cacheIt->second = true;
      }
      changed = true;
      nextIt = std::next(it);
    }
    ++it;
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
  spliceBodyBeforeTerminator(secondBody, firstBody);

  Value oldIv = secondBody.getArgument(0);
  Value newIv = firstBody.getArgument(0);
  oldIv.replaceAllUsesWith(newIv);

  second.erase();
}

/// Fuses consecutive compatible worker loops within an epoch.
/// Returns true if any fusion was performed.
static bool fuseWorkerLoopsInEpoch(EpochOp epoch) {
  Block &body = epoch.getBody().front();
  return fuseConsecutivePairs<scf::ForOp>(
      body,
      [](scf::ForOp a, scf::ForOp b) {
        return isFusableWorkerLoop(a, b) && !DbAnalysis::hasDbConflict(a, b);
      },
      [](scf::ForOp a, scf::ForOp b) {
        ARTS_DEBUG("Fusing consecutive worker loops in epoch");
        fuseLoops(a, b);
      });
}

///===----------------------------------------------------------------------===///
/// Pass Definition
///===----------------------------------------------------------------------===///

namespace {
struct EpochOptPass : public arts::EpochOptBase<EpochOptPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool changed = false;

    /// Narrowing runs FIRST so it can create opportunities for fusion.
    if (enableEpochNarrowing) {
      bool narrowChanged = false;
      narrowEpochScopes(module, narrowChanged);
      if (narrowChanged) {
        ARTS_INFO("Epoch scope narrowing applied");
        changed = true;
      }
    }

    if (enableEpochFusion) {
      bool fusionChanged = false;
      processRegionForEpochFusion(module.getRegion(), fusionChanged);
      if (fusionChanged) {
        ARTS_INFO("Epoch fusion applied");
        changed = true;
      }
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

    if (!changed)
      markAllAnalysesPreserved();
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
