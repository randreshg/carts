///==========================================================================///
/// File: EpochOpt.cpp
///
/// Unified epoch optimizations pass that performs:
///
/// Structural optimizations:
/// 1. Epoch scope narrowing - split large epochs into independent sub-epochs
/// 2. Epoch fusion - merge independent consecutive epochs
/// 3. Worker loop fusion - merge compatible worker loops within epochs
///
/// Scheduling optimizations (gated by individual flags):
/// 4. Repetition amortization - wrap EDT bodies with repeat loops for
///    trivial repeated epoch patterns
/// 5. Finish-EDT continuation - outline epoch tail code into continuation
///    EDTs signaled via epoch's built-in termination mechanism
/// 6. CPS loop driver - wrap time-step loops containing epochs in a single
///    outer epoch+driver EDT
/// 7. CPS chain - convert time-step loops into fully asynchronous
///    continuation chains, eliminating all inner blocking waits
///==========================================================================///

#define GEN_PASS_DEF_EPOCHOPT
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "arts/passes/Passes.h.inc"
#include "arts/Dialect.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/passes/Passes.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/EdtUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

#include "arts/utils/Debug.h"
#include "arts/utils/LoopUtils.h"
ARTS_DEBUG_SETUP(epoch_opt);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Shared Utilities
///===----------------------------------------------------------------------===///

using AccessMap = DenseMap<Operation *, bool>;

/// Attribute names for continuation metadata.
constexpr llvm::StringLiteral kHasControlDep = "arts.has_control_dep";
constexpr llvm::StringLiteral kContinuationForEpoch =
    "arts.continuation_for_epoch";

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
/// Repetition Amortization
///===----------------------------------------------------------------------===///
///
/// Detects repeated loop patterns:
///   scf.for (repeat) { arts.epoch { ... arts.edt ... } ; timer-tail }
/// and amortizes by wrapping EDT bodies with repeat loops.
///===----------------------------------------------------------------------===///

/// Return static trip count for an scf.for loop when all bounds are constants.
static std::optional<int64_t> getStaticTripCount(scf::ForOp forOp) {
  auto lb = forOp.getLowerBound().getDefiningOp<arith::ConstantIndexOp>();
  auto ub = forOp.getUpperBound().getDefiningOp<arith::ConstantIndexOp>();
  auto step = forOp.getStep().getDefiningOp<arith::ConstantIndexOp>();
  if (!lb || !ub || !step)
    return std::nullopt;
  int64_t lbv = lb.value();
  int64_t ubv = ub.value();
  int64_t sv = step.value();
  if (sv <= 0 || ubv <= lbv)
    return std::nullopt;
  return (ubv - lbv + sv - 1) / sv;
}

static bool isKernelTimerTailOp(Operation *op) {
  auto callOp = dyn_cast<func::CallOp>(op);
  if (!callOp)
    return false;
  return callOp.getCallee().starts_with("carts_kernel_timer_");
}

static bool canWrapEdtBodyWithRepeatLoop(EdtOp edt) {
  Block &body = edt.getBody().front();
  bool seenRelease = false;
  bool sawRepeatableOp = false;
  for (Operation &op : body.without_terminator()) {
    if (isa<DbReleaseOp>(op)) {
      seenRelease = true;
      continue;
    }
    if (seenRelease)
      return false;
    sawRepeatableOp = true;
  }
  return sawRepeatableOp;
}

static void wrapEdtBodyWithRepeatLoop(EdtOp edt, int64_t repeatCount) {
  if (repeatCount <= 1)
    return;

  Block &body = edt.getBody().front();
  SmallVector<Operation *> repeatOps;
  for (Operation &op : body.without_terminator()) {
    if (isa<DbReleaseOp>(op))
      break;
    repeatOps.push_back(&op);
  }
  if (repeatOps.empty())
    return;

  OpBuilder builder(body.getTerminator());
  Location loc = edt.getLoc();
  Value c0 = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value cN = builder.create<arith::ConstantIndexOp>(loc, repeatCount);
  Value c1 = builder.create<arith::ConstantIndexOp>(loc, 1);
  auto repeatFor = builder.create<scf::ForOp>(loc, c0, cN, c1);

  Operation *repeatTerminator = repeatFor.getBody()->getTerminator();
  for (Operation *op : repeatOps)
    op->moveBefore(repeatTerminator);
}

static bool epochSupportsRepetitionAmortization(EpochOp epochOp) {
  DenseSet<Operation *> readAllocs;
  DenseSet<Operation *> writeAllocs;
  bool sawEdt = false;
  bool safe = true;

  epochOp.walk([&](EdtOp edt) {
    sawEdt = true;
    SmallVector<bool> argReads;
    SmallVector<bool> argWrites;
    classifyEdtArgAccesses(edt, argReads, argWrites);

    ValueRange deps = edt.getDependencies();
    if (deps.size() != argReads.size()) {
      safe = false;
      return WalkResult::interrupt();
    }

    for (unsigned i = 0; i < deps.size(); ++i) {
      auto acq = deps[i].getDefiningOp<DbAcquireOp>();
      if (!acq) {
        safe = false;
        return WalkResult::interrupt();
      }

      Operation *alloc = DbUtils::getUnderlyingDbAlloc(acq.getSourcePtr());
      if (!alloc) {
        safe = false;
        return WalkResult::interrupt();
      }

      if (argReads[i] && DbUtils::isWriterMode(acq.getMode())) {
        safe = false;
        return WalkResult::interrupt();
      }
      if (argWrites[i] && acq.getMode() == ArtsMode::in) {
        safe = false;
        return WalkResult::interrupt();
      }

      if (argReads[i])
        readAllocs.insert(alloc);
      if (argWrites[i])
        writeAllocs.insert(alloc);
    }
    return WalkResult::advance();
  });

  if (!safe || !sawEdt)
    return false;
  for (Operation *alloc : writeAllocs) {
    if (readAllocs.contains(alloc))
      return false;
  }
  return true;
}

/// Detect and rewrite repeated loop patterns:
///   scf.for (repeat) { arts.epoch { ... arts.edt ... } ; timer-tail }
/// into:
///   arts.epoch { ... arts.edt(repeat loop inside body) ... } ; timer-tail
/// This amortizes epoch create/wait and EDT creation overhead.
static bool tryAmortizeRepeatedEpochLoop(EpochOp epochOp) {
  if (!epochOp || !epochOp->getBlock())
    return false;

  auto repeatLoop = dyn_cast<scf::ForOp>(epochOp->getParentOp());
  if (!repeatLoop)
    return false;
  if (repeatLoop.getNumResults() != 0 || !repeatLoop.getInitArgs().empty())
    return false;
  if (!repeatLoop.getInductionVar().use_empty())
    return false;

  std::optional<int64_t> tripCount = getStaticTripCount(repeatLoop);
  if (!tripCount || *tripCount < 2)
    return false;

  Block *loopBody = repeatLoop.getBody();
  if (epochOp->getBlock() != loopBody)
    return false;

  bool seenEpoch = false;
  SmallVector<Operation *> tailOps;
  for (Operation &op : loopBody->without_terminator()) {
    if (&op == epochOp.getOperation()) {
      seenEpoch = true;
      continue;
    }
    if (!seenEpoch)
      return false;
    tailOps.push_back(&op);
  }
  if (!seenEpoch)
    return false;
  for (Operation *tailOp : tailOps) {
    if (!isKernelTimerTailOp(tailOp))
      return false;
  }

  if (!epochSupportsRepetitionAmortization(epochOp))
    return false;

  SmallVector<EdtOp> edts;
  epochOp.walk([&](EdtOp edt) { edts.push_back(edt); });
  if (edts.empty())
    return false;
  for (EdtOp edt : edts) {
    if (!canWrapEdtBodyWithRepeatLoop(edt))
      return false;
  }

  for (EdtOp edt : edts)
    wrapEdtBodyWithRepeatLoop(edt, *tripCount);

  epochOp->moveBefore(repeatLoop);
  if (repeatLoop.getBody()->without_terminator().empty())
    repeatLoop.erase();

  ARTS_INFO("  Amortized repeated epoch loop (trip count = " << *tripCount
                                                             << ")");
  return true;
}

///===----------------------------------------------------------------------===///
/// Finish-EDT Continuation
///===----------------------------------------------------------------------===///
///
/// Detects safe "epoch + tail" patterns and outlines the tail into a
/// continuation arts.edt, allowing the epoch to use ARTS-native finish-EDT
/// signaling instead of blocking waits.
///===----------------------------------------------------------------------===///

/// Check if an operation is inside a loop.
static bool isInsideLoop(Operation *op) {
  Operation *parent = op->getParentOp();
  while (parent) {
    if (isa<arts::ForOp, scf::ForOp, scf::WhileOp, scf::ParallelOp,
            affine::AffineForOp>(parent))
      return true;
    parent = parent->getParentOp();
  }
  return false;
}

/// Check if an epoch + tail pattern is eligible for continuation.
static bool isEligibleForContinuation(EpochOp epochOp,
                                      SmallVectorImpl<Operation *> &tailOps) {
  /// Rule 1: Must be in a block (not floating).
  Block *block = epochOp->getBlock();
  if (!block)
    return false;

  /// Rule 2: The epoch must not be inside a loop (conservative: no CPS yet).
  if (isInsideLoop(epochOp))
    return false;

  /// Rule 3: The epoch region must not be empty.
  if (epochOp.getRegion().empty() ||
      epochOp.getRegion().front().without_terminator().empty())
    return false;

  /// Rule 4: Collect tail operations (everything after the epoch in the same
  /// block, before the block terminator).
  bool afterEpoch = false;
  for (Operation &op : *block) {
    if (&op == epochOp.getOperation()) {
      afterEpoch = true;
      continue;
    }
    if (afterEpoch && !op.hasTrait<OpTrait::IsTerminator>())
      tailOps.push_back(&op);
  }

  /// Rule 5: Must have at least one tail operation to outline.
  if (tailOps.empty())
    return false;

  /// Rule 6: No nested epochs in the tail.
  for (Operation *op : tailOps) {
    bool hasNestedEpoch = false;
    if (isa<EpochOp, CreateEpochOp>(op))
      return false;
    op->walk([&](Operation *inner) {
      if (isa<EpochOp, CreateEpochOp>(inner))
        hasNestedEpoch = true;
    });
    if (hasNestedEpoch)
      return false;
  }

  /// Rule 6b: Tail must not capture non-DB external values.
  DenseSet<Operation *> tailOpSet(tailOps.begin(), tailOps.end());
  for (Operation *op : tailOps) {
    for (Value operand : op->getOperands()) {
      auto *defOp = operand.getDefiningOp();
      if (!defOp)
        return false;
      if (tailOpSet.contains(defOp))
        continue;
      if (!isa<DbAcquireOp>(defOp))
        return false;
    }
  }

  /// Rule 7: The epoch must not already be marked for continuation.
  if (epochOp->hasAttr(kContinuationForEpoch))
    return false;

  /// Rule 8: The block terminator must not use any values defined by tail ops.
  Operation *terminator = block->getTerminator();
  if (terminator) {
    for (Value operand : terminator->getOperands()) {
      if (auto *defOp = operand.getDefiningOp()) {
        if (tailOpSet.contains(defOp))
          return false;
      }
    }
  }

  return true;
}

/// Collect values defined before the tail that are used by tail operations.
static void collectCapturedValues(ArrayRef<Operation *> tailOps,
                                  SetVector<Value> &capturedDbAcquires) {
  DenseSet<Operation *> tailOpSet(tailOps.begin(), tailOps.end());

  for (Operation *op : tailOps) {
    for (Value operand : op->getOperands()) {
      if (auto *defOp = operand.getDefiningOp()) {
        if (tailOpSet.contains(defOp))
          continue;
      }
      if (isa_and_nonnull<DbAcquireOp>(operand.getDefiningOp()))
        capturedDbAcquires.insert(operand);
    }
  }
}

/// Transform an eligible epoch + tail into continuation form.
static LogicalResult transformToContinuation(EpochOp epochOp,
                                             ArrayRef<Operation *> tailOps) {
  OpBuilder builder(epochOp->getContext());
  Location loc = epochOp.getLoc();

  /// Step 1: Collect DB acquires used by the tail.
  SetVector<Value> capturedDbAcquires;
  collectCapturedValues(tailOps, capturedDbAcquires);

  ARTS_DEBUG("  DB acquires captured: " << capturedDbAcquires.size());

  /// Step 2: Build the dependency list for the continuation EDT.
  SmallVector<Value> deps(capturedDbAcquires.begin(), capturedDbAcquires.end());

  /// Step 3: Create the continuation arts.edt AFTER the epoch.
  builder.setInsertionPointAfter(epochOp);

  auto edtOp = builder.create<EdtOp>(loc, EdtType::task,
                                     EdtConcurrency::intranode, deps);

  /// Mark continuation EDT with control dependency attribute.
  edtOp->setAttr(kHasControlDep,
                 builder.getIntegerAttr(builder.getI32Type(), 1));
  /// Mark as continuation for epoch linkage.
  edtOp->setAttr(kContinuationForEpoch, builder.getUnitAttr());

  /// Step 4: Populate the EDT body region.
  Block &edtBlock = edtOp.getBody().front();
  builder.setInsertionPointToStart(&edtBlock);

  /// Build value mapping: old DB acquire values -> EDT block arguments.
  IRMapping valueMapping;
  for (auto [oldDep, blockArg] : llvm::zip(deps, edtBlock.getArguments()))
    valueMapping.map(oldDep, blockArg);

  /// Step 5: Clone tail operations into the EDT body.
  for (Operation *op : tailOps) {
    Operation *cloned = builder.clone(*op, valueMapping);
    for (auto [oldRes, newRes] :
         llvm::zip(op->getResults(), cloned->getResults()))
      valueMapping.map(oldRes, newRes);
  }

  /// Step 5b: Ensure the EDT body has an arts.yield terminator.
  if (edtBlock.empty() || !edtBlock.back().hasTrait<OpTrait::IsTerminator>())
    builder.create<YieldOp>(loc);

  /// Step 6: Remove the original tail operations from the parent block.
  for (Operation *op : llvm::reverse(tailOps))
    op->erase();

  /// Step 7: Mark the epoch for continuation wiring by EpochLowering.
  epochOp->setAttr(kContinuationForEpoch, builder.getUnitAttr());

  ARTS_INFO("  Created continuation EDT with " << deps.size()
                                               << " DB deps + 1 control dep");
  return success();
}

///===----------------------------------------------------------------------===///
/// CPS Loop Driver Transform
///===----------------------------------------------------------------------===///
///
/// Wraps a time-step loop containing epochs into a single "loop driver" EDT
/// inside a wrapping epoch. The main thread waits once on the outer epoch
/// instead of spinning N times in the scheduler loop.
///===----------------------------------------------------------------------===///

/// Check whether an operation is pure (no side effects).
static bool isPureArith(Operation *op) {
  if (op->hasTrait<OpTrait::IsTerminator>())
    return true;
  if (isa<arith::ConstantOp, arith::AddIOp, arith::SubIOp, arith::MulIOp,
          arith::DivUIOp, arith::DivSIOp, arith::RemUIOp, arith::RemSIOp,
          arith::CmpIOp, arith::SelectOp, arith::MaxUIOp, arith::MinUIOp,
          arith::IndexCastOp, arith::IndexCastUIOp, arith::ExtSIOp,
          arith::ExtUIOp, arith::TruncIOp, arith::SIToFPOp>(op))
    return true;
  return false;
}

/// Check if a loop body contains at least one epoch (directly or in scf.if).
static bool bodyContainsEpoch(Block &body) {
  for (Operation &op : body.without_terminator()) {
    if (isa<EpochOp>(op))
      return true;
    if (auto ifOp = dyn_cast<scf::IfOp>(op)) {
      bool found = false;
      ifOp.walk([&](EpochOp) { found = true; });
      if (found)
        return true;
    }
  }
  return false;
}

/// Check if the loop body only contains epochs, pure arithmetic, scf.if
/// wrapping epochs, and no other side-effecting ops.
static bool bodyIsEpochOnly(Block &body) {
  for (Operation &op : body.without_terminator()) {
    if (isa<EpochOp>(op))
      continue;
    if (isa<scf::IfOp>(op))
      continue; // scf.if may wrap epochs — we allow it
    if (isPureArith(&op))
      continue;
    return false; // Side-effecting non-epoch op
  }
  return true;
}

/// CPS Loop Driver Transform: Convert a time-step loop containing epochs into
/// an asynchronous continuation chain using finish-EDT signaling.
static bool tryCPSLoopTransform(scf::ForOp forOp) {
  /// C1: The loop must have no iter_args.
  if (forOp.getNumResults() > 0 || !forOp.getInitArgs().empty()) {
    ARTS_DEBUG("CPS: loop has iter_args, skipping");
    return false;
  }

  /// C2: Must not be nested inside another loop.
  if (isInsideLoop(forOp)) {
    ARTS_DEBUG("CPS: loop is nested, skipping");
    return false;
  }

  /// C3: Loop body must contain at least one epoch.
  Block &body = *forOp.getBody();
  if (!bodyContainsEpoch(body)) {
    ARTS_DEBUG("CPS: no epochs in loop body, skipping");
    return false;
  }

  /// C4: Loop body must only contain epochs, pure arithmetic, and scf.if.
  if (!bodyIsEpochOnly(body)) {
    ARTS_DEBUG("CPS: non-epoch side effects in loop body, skipping");
    return false;
  }

  /// C5: Trip count >= 2.
  std::optional<int64_t> tripCount = getStaticTripCount(forOp);
  if (tripCount && *tripCount < 2) {
    ARTS_DEBUG("CPS: trip count < 2, skipping");
    return false;
  }

  /// C6: Epochs must not already be marked for continuation.
  bool alreadyMarked = false;
  forOp.walk([&](EpochOp epoch) {
    if (epoch->hasAttr(kContinuationForEpoch))
      alreadyMarked = true;
  });
  if (alreadyMarked) {
    ARTS_DEBUG("CPS: epoch already marked for continuation, skipping");
    return false;
  }

  ARTS_INFO("CPS: Found eligible time-step loop");

  /// === Transform ===

  OpBuilder builder(forOp->getContext());
  Location loc = forOp.getLoc();

  /// The driver EDT has no explicit DB dependencies.
  SmallVector<Value> deps;

  /// Step 1: Create the wrapping epoch.
  builder.setInsertionPoint(forOp);
  auto wrapEpoch = builder.create<EpochOp>(
      loc, IntegerType::get(builder.getContext(), 64));
  /// Ensure the epoch region has a block with a yield terminator.
  Region &epochRegion = wrapEpoch.getRegion();
  if (epochRegion.empty())
    epochRegion.emplaceBlock();
  Block &epochBlock = epochRegion.front();
  if (epochBlock.empty() ||
      !epochBlock.back().hasTrait<OpTrait::IsTerminator>()) {
    OpBuilder yieldBuilder = OpBuilder::atBlockEnd(&epochBlock);
    yieldBuilder.create<YieldOp>(loc);
  }

  /// Step 2: Create the loop driver EDT inside the epoch (before the yield).
  OpBuilder epochBuilder = OpBuilder::atBlockTerminator(&epochBlock);

  auto driverEdt = epochBuilder.create<EdtOp>(loc, EdtType::task,
                                                EdtConcurrency::intranode, deps);

  /// The EdtOp builder creates a body block but without block arguments
  /// matching the dependencies. Add them now.
  Block &edtBlock = driverEdt.getBody().front();
  for (Value dep : deps)
    edtBlock.addArgument(dep.getType(), loc);

  /// Step 3: Clone the loop into the driver EDT body.

  /// Map captured values to EDT block arguments.
  IRMapping edtMapping;
  for (auto [oldDep, blockArg] : llvm::zip(deps, edtBlock.getArguments()))
    edtMapping.map(oldDep, blockArg);

  /// Clone the loop into the EDT body.
  OpBuilder edtBuilder = OpBuilder::atBlockEnd(&edtBlock);
  edtBuilder.clone(*forOp.getOperation(), edtMapping);

  /// Add yield terminator if not already present.
  if (edtBlock.empty() || !edtBlock.back().hasTrait<OpTrait::IsTerminator>())
    edtBuilder.create<YieldOp>(loc);

  /// Step 5: Erase the original loop.
  forOp.erase();

  ARTS_INFO("CPS: Wrapped time-step loop in epoch+driver EDT");
  return true;
}

///===----------------------------------------------------------------------===///
/// CPS Chain Transform
///===----------------------------------------------------------------------===///
///
/// Converts a time-step loop containing N epochs into a CPS continuation chain.
/// Instead of blocking waits per iteration, each epoch's completion signals
/// the next continuation EDT which sets up the next epoch/iteration.
///
/// Case 1 (N=1, e.g. seidel-2d):
///   launcher → epoch_0(t=0, finish→cont0) → cont0 → epoch_0(t+s, finish→cont0) → ...
///
/// Case 2 (N=2, e.g. jacobi2d, second may be conditional):
///   launcher → epoch_0(finish→cont0) → cont0 → epoch_1(finish→cont1) → cont1 → advance
///
/// Case 3 (N>2, e.g. fdtd-2d):
///   launcher → epoch_0(finish→cont0) → cont0 → epoch_1(finish→cont1) → ... → contN-1 → advance
///
/// Structure: nested continuations inside the launcher EDT. Each continuation
/// EDT is placed AFTER the epoch it continues, in the same block, so
/// EpochLowering can find it when wiring the finish target.
///
///   arts.epoch {                              // single outer wait
///     arts.edt launcher() {
///       epoch_0 {continuation_for_epoch} { workers_0 }
///       arts.edt cont_0 {cps_chain_id="chain_0", ...} {
///         epoch_1 {continuation_for_epoch} { workers_1 }
///         arts.edt cont_1 {cps_chain_id="chain_1", ...} {
///           ... (nested for N>2) ...
///           arts.edt cont_{N-1} {cps_chain_id="chain_{N-1}", ...} {
///             t_next = t + step
///             if (t_next < ub) {
///               arts.cps_advance("chain_0") { workers_0_clone }
///             }
///           }
///         }
///       }
///     }
///   }
///
///===----------------------------------------------------------------------===///

/// Attribute name for CPS chain identification.
constexpr llvm::StringLiteral kCPSChainId = "arts.cps_chain_id";
constexpr llvm::StringLiteral kCPSLoopContinuation =
    "arts.cps_loop_continuation";

/// Epoch slot in a loop body — either a direct epoch or one inside scf.if.
struct EpochSlot {
  EpochOp epoch;
  scf::IfOp wrappingIf; // null if the epoch is not inside an scf.if
};

/// Collect all epoch "slots" directly in a loop body, in program order.
/// An epoch slot is either a bare EpochOp or an EpochOp inside an scf.if.
static void collectEpochSlots(Block &body,
                              SmallVectorImpl<EpochSlot> &slots) {
  for (Operation &op : body.without_terminator()) {
    if (auto epoch = dyn_cast<EpochOp>(op)) {
      slots.push_back({epoch, nullptr});
    } else if (auto ifOp = dyn_cast<scf::IfOp>(op)) {
      /// Check if the then-branch contains an epoch.
      for (Operation &thenOp : ifOp.getThenRegion().front()) {
        if (auto epoch = dyn_cast<EpochOp>(thenOp))
          slots.push_back({epoch, ifOp});
      }
    }
  }
}

/// Check that an epoch body only has ops that can be cloned into an EDT body.
static bool epochBodyIsClonable(EpochOp epoch, Value inductionVar) {
  (void)inductionVar;
  return !epoch.getRegion().empty() &&
         !epoch.getRegion().front().without_terminator().empty();
}

/// Check if an operation is one of the epoch slot operations.
static bool isSlotOp(Operation *op, MutableArrayRef<EpochSlot> slots) {
  for (auto &slot : slots) {
    if (op == slot.epoch.getOperation())
      return true;
    if (slot.wrappingIf && op == slot.wrappingIf.getOperation())
      return true;
  }
  return false;
}

/// Clone all non-slot pure arithmetic ops from the loop body.
static void cloneNonSlotArith(OpBuilder &builder, Block &body,
                              MutableArrayRef<EpochSlot> slots,
                              IRMapping &mapping) {
  for (Operation &op : body.without_terminator()) {
    if (isSlotOp(&op, slots))
      continue;
    Operation *cloned = builder.clone(op, mapping);
    for (auto [oldRes, newRes] :
         llvm::zip(op.getResults(), cloned->getResults()))
      mapping.map(oldRes, newRes);
  }
}

/// Ensure a block has a YieldOp terminator. If not, create one.
static void ensureYieldTerminator(Block &block, Location loc) {
  if (block.empty() || !block.back().hasTrait<OpTrait::IsTerminator>())
    OpBuilder::atBlockEnd(&block).create<YieldOp>(loc);
}

/// Emit the CPS advance logic: check loop bounds, create CPSAdvanceOp.
/// This is the code in the LAST continuation that advances to the next
/// iteration by creating a new epoch_0 via self-referential continuation.
static void emitAdvanceLogic(OpBuilder &builder, Location loc, Value iv,
                             Value lb, Value ub, Value step, Block &body,
                             MutableArrayRef<EpochSlot> slots) {
  /// t_next = lb + step (lb is stand-in for "current t")
  Value tNext = builder.create<arith::AddIOp>(loc, lb, step);
  /// cond = t_next < ub
  Value cond = builder.create<arith::CmpIOp>(
      loc, arith::CmpIPredicate::slt, tNext, ub);

  auto ifOp = builder.create<scf::IfOp>(loc, cond, /*withElse=*/false);
  {
    Block &thenBlock = ifOp.getThenRegion().front();
    OpBuilder thenBuilder = OpBuilder::atBlockTerminator(&thenBlock);

    IRMapping advanceMapping;
    advanceMapping.map(iv, tNext);
    cloneNonSlotArith(thenBuilder, body, slots, advanceMapping);

    SmallVector<Value> nextParams = {tNext};
    auto advance = thenBuilder.create<CPSAdvanceOp>(
        loc, nextParams, thenBuilder.getStringAttr("chain_0"));

    /// Clone the first epoch's body into the cps_advance region.
    Region &advanceRegion = advance.getEpochBody();
    if (advanceRegion.empty())
      advanceRegion.emplaceBlock();
    Block &advBlock = advanceRegion.front();
    ensureYieldTerminator(advBlock, loc);
    OpBuilder advBuilder = OpBuilder::atBlockTerminator(&advBlock);

    Block &epochBody = slots[0].epoch.getBody().front();
    for (Operation &epochOp : epochBody.without_terminator())
      advBuilder.clone(epochOp, advanceMapping);
  }
}

/// Mark an EDT as a CPS continuation with the given chain ID.
static void markAsContinuation(EdtOp edt, OpBuilder &builder,
                                unsigned chainIdx) {
  std::string chainId = "chain_" + std::to_string(chainIdx);
  edt->setAttr(kHasControlDep,
               builder.getIntegerAttr(builder.getI32Type(), 1));
  edt->setAttr(kContinuationForEpoch, builder.getUnitAttr());
  edt->setAttr(kCPSChainId, builder.getStringAttr(chainId));
  edt->setAttr(kCPSLoopContinuation, builder.getUnitAttr());
}

/// CPS Chain Transform for N epochs per iteration.
///
/// Handles Cases 1-3: single epoch, multi-epoch with conditional, N sequential.
/// Continuation EDTs are nested inside the launcher so that each cont_i
/// appears after epoch_i in the same block — required for EpochLowering to
/// wire the finish target.
static bool tryCPSChainTransform(scf::ForOp forOp) {
  /// C1: No iter_args.
  if (forOp.getNumResults() > 0 || !forOp.getInitArgs().empty())
    return false;

  /// C2: Not nested inside another loop.
  if (isInsideLoop(forOp))
    return false;

  /// C3: Trip count >= 2.
  std::optional<int64_t> tripCount = getStaticTripCount(forOp);
  if (tripCount && *tripCount < 2)
    return false;

  Block &body = *forOp.getBody();

  /// C4: Collect epoch slots in the body.
  SmallVector<EpochSlot> slots;
  collectEpochSlots(body, slots);
  if (slots.empty())
    return false;

  Value iv = forOp.getInductionVar();

  /// C5: Already marked? Skip.
  for (const auto &slot : slots)
    if (slot.epoch->hasAttr(kContinuationForEpoch))
      return false;

  /// C6: All epoch bodies must be clonable.
  for (const auto &slot : slots)
    if (!epochBodyIsClonable(slot.epoch, iv))
      return false;

  /// C7: All non-slot ops in body must be pure arithmetic.
  for (Operation &op : body.without_terminator()) {
    if (isSlotOp(&op, slots))
      continue;
    if (!isPureArith(&op))
      return false;
  }

  unsigned N = slots.size();
  ARTS_INFO("CPS Chain: Found eligible " << N << "-epoch loop");

  /// === Transform ===

  OpBuilder builder(forOp->getContext());
  Location loc = forOp.getLoc();
  Value lb = forOp.getLowerBound();
  Value ub = forOp.getUpperBound();
  Value step = forOp.getStep();

  /// Step 1: Create the outer wrapping epoch BEFORE the loop.
  builder.setInsertionPoint(forOp);
  auto outerEpoch = builder.create<EpochOp>(
      loc, IntegerType::get(builder.getContext(), 64));
  Region &outerRegion = outerEpoch.getRegion();
  if (outerRegion.empty())
    outerRegion.emplaceBlock();
  Block &outerBlock = outerRegion.front();
  /// Ensure the block has a yield terminator.
  if (outerBlock.empty() ||
      !outerBlock.back().hasTrait<OpTrait::IsTerminator>()) {
    OpBuilder::atBlockEnd(&outerBlock).create<YieldOp>(loc);
  }

  /// Step 2: Create the launcher EDT inside the outer epoch.
  OpBuilder outerBuilder = OpBuilder::atBlockTerminator(&outerBlock);
  auto launcherEdt = outerBuilder.create<EdtOp>(loc, EdtType::task,
                                                 EdtConcurrency::intranode,
                                                 SmallVector<Value>{});
  Block &launcherBlock = launcherEdt.getBody().front();
  ensureYieldTerminator(launcherBlock, loc);
  OpBuilder launcherBuilder = OpBuilder::atBlockTerminator(&launcherBlock);

  /// Clone non-slot pure arithmetic into launcher (mapped to first iter).
  IRMapping firstIterMapping;
  firstIterMapping.map(iv, lb);
  cloneNonSlotArith(launcherBuilder, body, slots, firstIterMapping);

  /// Clone epoch_0 into the launcher.
  Operation *clonedEpoch0Op =
      launcherBuilder.clone(*slots[0].epoch.getOperation(), firstIterMapping);
  auto clonedEpoch0 = cast<EpochOp>(clonedEpoch0Op);
  clonedEpoch0->setAttr(kContinuationForEpoch, launcherBuilder.getUnitAttr());

  /// Step 3: Build the nested continuation chain.
  /// cont_0 goes after epoch_0 in the launcher block.
  /// cont_1 goes after epoch_1 inside cont_0's body, etc.
  /// cont_{N-1} (last) contains the advance logic.
  ///
  /// We use atBlockTerminator so new ops are inserted before the implicit
  /// yield, and the yield always remains last.
  OpBuilder *currentBuilder = &launcherBuilder;

  for (unsigned i = 0; i < N; ++i) {
    /// Create continuation EDT cont_i in the current block.
    auto contEdt = currentBuilder->create<EdtOp>(
        loc, EdtType::task, EdtConcurrency::intranode, SmallVector<Value>{});
    markAsContinuation(contEdt, *currentBuilder, i);

    Block &contBlock = contEdt.getBody().front();
    ensureYieldTerminator(contBlock, loc);
    OpBuilder contBuilder = OpBuilder::atBlockTerminator(&contBlock);

    if (i == N - 1) {
      /// Last continuation: emit advance logic (check bounds, CPSAdvanceOp).
      emitAdvanceLogic(contBuilder, loc, iv, lb, ub, step, body, slots);
    } else {
      /// Middle continuation: set up epoch_{i+1} and nest cont_{i+1}.
      IRMapping contMapping;
      contMapping.map(iv, lb); // lb is stand-in for "current t"
      cloneNonSlotArith(contBuilder, body, slots, contMapping);

      EpochSlot &nextSlot = slots[i + 1];
      if (nextSlot.wrappingIf) {
        /// Conditional epoch: clone the scf.if, mark the epoch inside its
        /// then-branch for continuation. The else-branch gets the advance
        /// logic directly, since the epoch doesn't run.
        Operation *clonedIfOp =
            contBuilder.clone(*nextSlot.wrappingIf.getOperation(), contMapping);
        auto clonedIf = cast<scf::IfOp>(clonedIfOp);

        /// Mark the epoch inside the then-branch for continuation.
        for (Operation &thenOp : clonedIf.getThenRegion().front()) {
          if (auto epoch = dyn_cast<EpochOp>(thenOp)) {
            epoch->setAttr(kContinuationForEpoch, contBuilder.getUnitAttr());
            break;
          }
        }

        /// Need an else-branch with advance logic for when condition is false.
        if (clonedIf.getElseRegion().empty()) {
          /// Recreate the if with an else branch.
          Value ifCond = clonedIf.getCondition();
          OpBuilder preIfBuilder(clonedIf);
          auto newIf =
              preIfBuilder.create<scf::IfOp>(loc, ifCond, /*withElse=*/true);

          /// Move then-block contents to the new if's then-block.
          Block &oldThenBlock = clonedIf.getThenRegion().front();
          Block &newThenBlock = newIf.getThenRegion().front();
          for (Operation &op :
               llvm::make_early_inc_range(oldThenBlock.without_terminator()))
            op.moveBefore(newThenBlock.getTerminator());

          /// Emit advance logic in the else-branch.
          Block &elseBlock = newIf.getElseRegion().front();
          OpBuilder elseBuilder = OpBuilder::atBlockTerminator(&elseBlock);
          emitAdvanceLogic(elseBuilder, loc, iv, lb, ub, step, body, slots);

          clonedIf.erase();

          /// Point builder into the new then-block for next continuation.
          Block &finalThenBlock = newIf.getThenRegion().front();
          contBuilder.setInsertionPoint(finalThenBlock.getTerminator());
          currentBuilder = &contBuilder;
        } else {
          /// else-branch already exists; add advance logic to it.
          Block &elseBlock = clonedIf.getElseRegion().front();
          OpBuilder elseBuilder = OpBuilder::atBlockTerminator(&elseBlock);
          emitAdvanceLogic(elseBuilder, loc, iv, lb, ub, step, body, slots);

          /// Point builder into the then-block for next continuation.
          Block &thenBlock = clonedIf.getThenRegion().front();
          contBuilder.setInsertionPoint(thenBlock.getTerminator());
          currentBuilder = &contBuilder;
        }
      } else {
        /// Unconditional epoch: clone and mark for continuation.
        Operation *clonedEpochOp =
            contBuilder.clone(*nextSlot.epoch.getOperation(), contMapping);
        cast<EpochOp>(clonedEpochOp)->setAttr(kContinuationForEpoch,
                                               contBuilder.getUnitAttr());
        /// Next cont goes after this epoch in the same block.
        /// contBuilder is already at the terminator position so next
        /// iteration's create<EdtOp> will be placed before the yield.
        currentBuilder = &contBuilder;
      }
    }
  }

  /// Step 4: Erase the original loop.
  forOp.erase();

  ARTS_INFO("CPS Chain: Transformed " << N
                                      << "-epoch loop into continuation chain");
  return true;
}

///===----------------------------------------------------------------------===///
/// Pass Definition
///===----------------------------------------------------------------------===///

namespace {
struct EpochOptPass : public arts::impl::EpochOptBase<EpochOptPass> {
  EpochOptPass() = default;

  EpochOptPass(bool narrowing, bool fusion, bool loopFusion, bool amortization,
               bool continuation, bool cpsDriver, bool cpsChain = false) {
    enableEpochNarrowing = narrowing;
    enableEpochFusion = fusion;
    enableLoopFusion = loopFusion;
    enableAmortization = amortization;
    enableContinuation = continuation;
    enableCPSDriver = cpsDriver;
    enableCPSChain = cpsChain;
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool changed = false;

    ARTS_INFO_HEADER(EpochOptPass);

    /// === Scheduling optimizations (run FIRST, restructure loops/epochs) ===

    /// Phase 0a: CPS chain — convert time-step loops into continuation chains.
    /// Must run FIRST because it restructures loops and creates new epochs.
    /// CPS chain replaces the loop entirely; CPS driver wraps it. They are
    /// mutually exclusive — CPS chain takes priority when both are enabled.
    if (enableCPSChain) {
      unsigned chainTransformed = 0;
      SmallVector<scf::ForOp> chainForOps;
      module.walk([&](scf::ForOp forOp) { chainForOps.push_back(forOp); });
      for (scf::ForOp forOp : chainForOps) {
        if (tryCPSChainTransform(forOp))
          ++chainTransformed;
      }
      if (chainTransformed > 0) {
        ARTS_INFO("CPS-chain-transformed " << chainTransformed
                                           << " epoch loop(s)");
        changed = true;
      }
    }

    /// Phase 0b: CPS loop driver — convert time-step loops with epochs into
    /// async continuation chains. Must run BEFORE individual epoch analysis
    /// because it restructures loops and creates new epochs.
    if (enableCPSDriver) {
      unsigned cpsTransformed = 0;
      SmallVector<scf::ForOp> forOps;
      module.walk([&](scf::ForOp forOp) { forOps.push_back(forOp); });
      for (scf::ForOp forOp : forOps) {
        if (tryCPSLoopTransform(forOp))
          ++cpsTransformed;
      }
      if (cpsTransformed > 0) {
        ARTS_INFO("CPS-transformed " << cpsTransformed << " epoch loop(s)");
        changed = true;
      }
    }

    /// Phase 1: Repetition amortization and continuation.
    SmallVector<EpochOp> epochOps;
    if (enableAmortization || enableContinuation) {
      module.walk([&](EpochOp epochOp) { epochOps.push_back(epochOp); });

      ARTS_INFO("Found " << epochOps.size()
                         << " epoch operations to analyze for scheduling");
    }

    if (enableAmortization) {
      unsigned amortized = 0;
      for (EpochOp epochOp : epochOps) {
        if (tryAmortizeRepeatedEpochLoop(epochOp))
          ++amortized;
      }
      if (amortized > 0) {
        ARTS_INFO("Amortized " << amortized << " repeated epoch loop(s)");
        changed = true;
      }
    }

    if (enableContinuation) {
      unsigned transformed = 0;
      for (EpochOp epochOp : epochOps) {
        SmallVector<Operation *> tailOps;
        if (!isEligibleForContinuation(epochOp, tailOps)) {
          ARTS_DEBUG("  Epoch not eligible for continuation");
          continue;
        }

        ARTS_INFO("  Transforming eligible epoch to continuation form");
        if (succeeded(transformToContinuation(epochOp, tailOps)))
          ++transformed;
      }
      if (transformed > 0) {
        ARTS_INFO("Transformed " << transformed
                                << " epochs to continuation form");
        changed = true;
      }
    }

    /// === Structural optimizations ===

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

    ARTS_INFO_FOOTER(EpochOptPass);

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

std::unique_ptr<Pass> createEpochOptPass(bool amortization, bool continuation,
                                         bool cpsDriver, bool cpsChain) {
  return std::make_unique<EpochOptPass>(
      /*narrowing=*/true, /*fusion=*/true, /*loopFusion=*/true,
      amortization, continuation, cpsDriver, cpsChain);
}

std::unique_ptr<Pass> createEpochOptSchedulingPass(bool amortization,
                                                    bool continuation,
                                                    bool cpsDriver,
                                                    bool cpsChain) {
  return std::make_unique<EpochOptPass>(
      /*narrowing=*/false, /*fusion=*/false, /*loopFusion=*/false,
      amortization, continuation, cpsDriver, cpsChain);
}
} // namespace arts
} // namespace mlir
