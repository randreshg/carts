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
#include "arts/analysis/AnalysisDependencies.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/utils/DbUtils.h"
#include "arts/utils/EdtUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/RegionUtils.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

#include "arts/utils/Debug.h"
#include "arts/utils/LoopUtils.h"
#include "polygeist/Ops.h"
ARTS_DEBUG_SETUP(epoch_opt);

using mlir::arts::AnalysisDependencyInfo;
using mlir::arts::AnalysisKind;

static const AnalysisKind kEpochOpt_reads[] = {AnalysisKind::EpochAnalysis};
static const AnalysisKind kEpochOpt_invalidates[] = {
    AnalysisKind::DbAnalysis,    AnalysisKind::EdtAnalysis,
    AnalysisKind::EpochAnalysis, AnalysisKind::LoopAnalysis,
    AnalysisKind::DbHeuristics,  AnalysisKind::EdtHeuristics};
[[maybe_unused]] static const AnalysisDependencyInfo kEpochOpt_deps = {
    kEpochOpt_reads, kEpochOpt_invalidates};

using namespace mlir;
using namespace mlir::arts;

namespace {
struct EpochNarrowingCounts {
  unsigned epochsNarrowed = 0;
  unsigned newEpochsCreated = 0;
};

static llvm::StringRef
asyncLoopStrategyToString(EpochAsyncLoopStrategy strategy) {
  switch (strategy) {
  case EpochAsyncLoopStrategy::None:
    return "none";
  case EpochAsyncLoopStrategy::AdvanceEdt:
    return "advance_edt";
  case EpochAsyncLoopStrategy::CpsChain:
    return "cps_chain";
  }
  return "none";
}
} // namespace

static bool isCpsChainExcludedDepPattern(ArtsDepPattern pattern) {
  switch (pattern) {
  case ArtsDepPattern::jacobi_alternating_buffers:
  case ArtsDepPattern::wavefront_2d:
    return true;
  default:
    return false;
  }
}

static bool
loopContainsExcludedDepPattern(scf::ForOp forOp,
                               function_ref<bool(ArtsDepPattern)> isExcluded) {
  if (!forOp)
    return false;

  auto matchesExcludedPattern = [&](Operation *op) {
    auto pattern = getEffectiveDepPattern(op);
    return pattern && isExcluded(*pattern);
  };

  if (matchesExcludedPattern(forOp.getOperation()))
    return true;

  bool found = false;
  forOp.walk([&](Operation *inner) {
    if (!matchesExcludedPattern(inner))
      return WalkResult::advance();
    found = true;
    return WalkResult::interrupt();
  });
  return found;
}

static bool loopContainsCpsChainExcludedDepPattern(scf::ForOp forOp) {
  return loopContainsExcludedDepPattern(forOp, isCpsChainExcludedDepPattern);
}

static bool isCpsDriverExcludedDepPattern(ArtsDepPattern pattern) {
  switch (pattern) {
  case ArtsDepPattern::jacobi_alternating_buffers:
  case ArtsDepPattern::wavefront_2d:
    return true;
  default:
    return false;
  }
}

static bool loopContainsCpsDriverExcludedDepPattern(scf::ForOp forOp) {
  return loopContainsExcludedDepPattern(forOp, isCpsDriverExcludedDepPattern);
}

/// Attribute names for continuation metadata.
constexpr llvm::StringLiteral kHasControlDep = "arts.has_control_dep";
using AttrNames::Operation::ContinuationForEpoch;
using AttrNames::Operation::CPSAdditiveParams;
using AttrNames::Operation::CPSAdvanceHasIvArg;
using AttrNames::Operation::CPSChainId;
using AttrNames::Operation::CPSDepRouting;
using AttrNames::Operation::CPSDirectRecreate;
using AttrNames::Operation::CPSForwardDeps;
using AttrNames::Operation::CPSInitIter;
using AttrNames::Operation::CPSIterCounterParamIdx;
using AttrNames::Operation::CPSLoopContinuation;
using AttrNames::Operation::CPSNumCarry;

static SmallVector<Value> collectCurrentEdtPackedUserValues(EdtOp edt) {
  return collectEdtPackedValues(edt);
}

static void ensureYieldTerminator(Block &block, Location loc);
static void cloneNonSlotArith(OpBuilder &builder, Block &body,
                              MutableArrayRef<EpochSlot> slots,
                              IRMapping &mapping,
                              ArrayRef<Operation *> sequentialOps = {});
static void markAsContinuation(EdtOp edt, OpBuilder &builder,
                               unsigned chainIdx);

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

  /// For each op, compute the minimum index of any earlier epoch-body op it
  /// depends on. A cut at position i is valid only when every op at position
  /// >= i depends exclusively on ops in [i, n) or on values defined outside the
  /// epoch body.
  SmallVector<unsigned> minDep(n, n);
  for (unsigned j = 0; j < n; ++j) {
    unsigned depMin = n;

    /// Check direct operands.
    auto checkOperand = [&](Value operand) {
      if (auto *defOp = operand.getDefiningOp()) {
        auto it = opIndex.find(defOp);
        if (it != opIndex.end())
          depMin = std::min(depMin, it->second);
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
        depMin = std::min(depMin, k);
    }

    minDep[j] = depMin;
  }

  /// A cut at position i is valid iff for all j >= i: minDep[j] >= i.
  /// Equivalently: min(minDep[i..n)) >= i.
  SmallVector<unsigned> suffixMin(n);
  suffixMin[n - 1] = minDep[n - 1];
  for (int k = (int)n - 2; k >= 0; --k)
    suffixMin[k] = std::min(minDep[k], suffixMin[k + 1]);

  /// Position i is a valid cut if no later op reaches back into [0, i).
  for (unsigned i = 1; i < n; ++i) {
    if (suffixMin[i] >= i) {
      cutPoints.push_back(i);
    }
  }

  return cutPoints;
}

/// Attempt to narrow (split) a single epoch into multiple smaller epochs.
/// Returns the number of new epochs created by the split.
static unsigned narrowEpoch(EpochOp epoch) {
  Block &body = epoch.getBody().front();

  /// Collect non-terminator operations in order.
  SmallVector<Operation *> ops;
  for (Operation &op : body.without_terminator())
    ops.push_back(&op);

  if (ops.size() <= 1)
    return 0;

  /// Find cut points.
  SmallVector<unsigned> cutPoints = findEpochCutPoints(ops);
  if (cutPoints.empty())
    return 0;

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
    return 0;

  Location loc = epoch.getLoc();

  /// Create new epochs for each group. The first group reuses the original
  /// epoch to preserve SSA dominance of the epochGuid if it's used.
  /// Check if the epochGuid result is used outside.
  bool epochGuidUsed = !epoch.getEpochGuid().use_empty();

  if (epochGuidUsed) {
    /// If the epoch GUID is used externally, we cannot safely split because
    /// multiple new epochs would each produce their own GUID. Be conservative.
    ARTS_DEBUG("Epoch GUID is used externally, skipping narrowing");
    return 0;
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
  return groups.size() - 1;
}

/// Walk all epochs in the module and attempt to narrow them.
/// Processes epochs bottom-up so inner epochs are narrowed before outer ones.
static EpochNarrowingCounts narrowEpochScopes(ModuleOp module) {
  EpochNarrowingCounts counts;
  SmallVector<EpochOp> epochs;
  module.walk([&](EpochOp epoch) { epochs.push_back(epoch); });

  for (EpochOp epoch : epochs) {
    /// Skip if the epoch was erased by a previous narrowing.
    if (!epoch->getBlock())
      continue;
    /// Skip CPS chain epochs — their internal structure (epoch_0 + cont EDTs)
    /// must not be split since continuations depend on the enclosing epoch.
    if (epoch->hasAttr(ContinuationForEpoch))
      continue;
    unsigned created = narrowEpoch(epoch);
    if (created == 0)
      continue;
    ++counts.epochsNarrowed;
    counts.newEpochsCreated += created;
  }
  return counts;
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
/// Returns the number of consecutive epoch pairs fused.
/// Uses a cached EpochAccessSummary for the first epoch to avoid recomputing
/// effective accesses on each chain-fusion step.
static unsigned processBlockForEpochFusion(Block &block,
                                           const EpochAnalysis &epochAnalysis,
                                           bool continuationEnabled) {
  unsigned fusedPairs = 0;
  for (auto it = block.begin(); it != block.end();) {
    auto first = dyn_cast<EpochOp>(&*it);
    if (!first) {
      ++it;
      continue;
    }

    /// Cache first's accesses for chain-fusion.
    EpochAccessSummary firstAccesses =
        epochAnalysis.summarizeEpochAccess(first);
    auto nextIt = std::next(it);
    while (nextIt != block.end()) {
      auto second = dyn_cast<EpochOp>(&*nextIt);
      if (!second)
        break;
      EpochAccessSummary secondAccesses =
          epochAnalysis.summarizeEpochAccess(second);
      EpochFusionDecision decision = epochAnalysis.evaluateEpochFusion(
          first, second, continuationEnabled, &firstAccesses, &secondAccesses);
      if (!decision.shouldFuse) {
        ARTS_DEBUG("Skipping epoch fusion: " << decision.rationale);
        break;
      }

      ARTS_DEBUG("Fusing consecutive epochs");
      fuseEpochs(first, second);
      firstAccesses.mergeFrom(secondAccesses);
      ++fusedPairs;
      nextIt = std::next(it);
    }
    ++it;
  }
  return fusedPairs;
}

/// Recursively processes all blocks in a region for epoch fusion.
static unsigned processRegionForEpochFusion(Region &region,
                                            const EpochAnalysis &epochAnalysis,
                                            bool continuationEnabled) {
  unsigned fusedPairs = 0;
  for (Block &block : region) {
    fusedPairs +=
        processBlockForEpochFusion(block, epochAnalysis, continuationEnabled);
    for (Operation &op : block) {
      for (Region &nested : op.getRegions())
        fusedPairs += processRegionForEpochFusion(nested, epochAnalysis,
                                                  continuationEnabled);
    }
  }
  return fusedPairs;
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
/// Returns the number of worker loop pairs fused.
static unsigned fuseWorkerLoopsInEpoch(EpochOp epoch) {
  Block &body = epoch.getBody().front();
  unsigned fusedPairs = 0;
  (void)fuseConsecutivePairs<scf::ForOp>(
      body,
      [](scf::ForOp a, scf::ForOp b) {
        return isFusableWorkerLoop(a, b) && !DbAnalysis::hasDbConflict(a, b);
      },
      [&](scf::ForOp a, scf::ForOp b) {
        ARTS_DEBUG("Fusing consecutive worker loops in epoch");
        ++fusedPairs;
        fuseLoops(a, b);
      });
  return fusedPairs;
}

///===----------------------------------------------------------------------===///
/// Repetition Amortization
///===----------------------------------------------------------------------===///
///
/// Detects repeated loop patterns:
///   scf.for (repeat) { arts.epoch { ... arts.edt ... } ; timer-tail }
/// and amortizes by wrapping EDT bodies with repeat loops.
///===----------------------------------------------------------------------===///

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

  std::optional<int64_t> tripCount =
      arts::getStaticTripCount(repeatLoop.getOperation());
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

/// Transform an eligible epoch + tail into continuation form.
static LogicalResult
transformToContinuation(EpochOp epochOp,
                        const EpochContinuationDecision &decision) {
  OpBuilder builder(epochOp->getContext());
  Location loc = epochOp.getLoc();

  ARTS_DEBUG(
      "  DB acquires captured: " << decision.capturedDbAcquireValues.size());

  /// Step 1: Build the dependency list for the continuation EDT from the
  /// canonical continuation analysis result.
  SmallVector<Value> deps(decision.capturedDbAcquireValues.begin(),
                          decision.capturedDbAcquireValues.end());

  /// Step 2: Create the continuation arts.edt AFTER the epoch.
  builder.setInsertionPointAfter(epochOp);

  auto edtOp = builder.create<EdtOp>(loc, EdtType::task,
                                     EdtConcurrency::intranode, deps);

  /// Mark continuation EDT with control dependency attribute.
  edtOp->setAttr(kHasControlDep,
                 builder.getIntegerAttr(builder.getI32Type(), 1));
  /// Mark as continuation for epoch linkage.
  edtOp->setAttr(ContinuationForEpoch, builder.getUnitAttr());

  /// Step 3: Populate the EDT body region.
  Block &edtBlock = edtOp.getBody().front();
  builder.setInsertionPointToStart(&edtBlock);

  /// Build value mapping: old DB acquire values -> EDT block arguments.
  IRMapping valueMapping;
  for (auto [oldDep, blockArg] : llvm::zip(deps, edtBlock.getArguments()))
    valueMapping.map(oldDep, blockArg);

  /// Step 4: Clone tail operations into the EDT body.
  for (Operation *op : decision.tailOps) {
    Operation *cloned = builder.clone(*op, valueMapping);
    for (auto [oldRes, newRes] :
         llvm::zip(op->getResults(), cloned->getResults()))
      valueMapping.map(oldRes, newRes);
  }

  /// Ensure the EDT body has an arts.yield terminator.
  if (edtBlock.empty() || !edtBlock.back().hasTrait<OpTrait::IsTerminator>())
    builder.create<YieldOp>(loc);

  /// Step 5: Remove the original tail operations from the parent block.
  for (Operation *op : llvm::reverse(decision.tailOps))
    op->erase();

  /// Step 6: Mark the epoch for continuation wiring by EpochLowering.
  epochOp->setAttr(ContinuationForEpoch, builder.getUnitAttr());

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

/// CPS Loop Driver Transform: Convert a time-step loop containing epochs into
/// an asynchronous launcher/advance chain inside a single wrapping epoch.
static bool tryCPSLoopTransform(scf::ForOp forOp,
                                const EpochAnalysis &epochAnalysis) {
  if (loopContainsCpsDriverExcludedDepPattern(forOp)) {
    ARTS_INFO("CPS: skipping loop with specialized dep pattern");
    return false;
  }

  EpochAsyncLoopDecision asyncDecision =
      epochAnalysis.evaluateAsyncLoopStrategy(forOp);
  if (asyncDecision.strategy != EpochAsyncLoopStrategy::AdvanceEdt ||
      asyncDecision.slots.size() != 1) {
    ARTS_DEBUG("CPS: advance_edt requires a single epoch slot, skipping");
    return false;
  }

  EpochLoopDriverDecision decision = epochAnalysis.evaluateCPSLoopDriver(forOp);
  if (!decision.eligible) {
    ARTS_DEBUG("CPS: " << decision.rationale << ", skipping");
    return false;
  }

  ARTS_INFO("CPS: Found eligible time-step loop");

  /// === Transform ===

  OpBuilder builder(forOp->getContext());
  Location loc = forOp.getLoc();
  Block &body = *forOp.getBody();
  Value lb = forOp.getLowerBound();
  Value ub = forOp.getUpperBound();
  Value step = forOp.getStep();
  Value iv = forOp.getInductionVar();
  auto i64Ty = builder.getI64Type();
  std::string chainId =
      "async_loop_" +
      std::to_string(reinterpret_cast<uintptr_t>(forOp.getOperation()));

  builder.setInsertionPoint(forOp);
  SmallVector<Value> currentStateDeps;
  MutableArrayRef<EpochSlot> slots(asyncDecision.slots);

  /// Step 1: Create the wrapping epoch.
  auto wrapEpoch =
      builder.create<EpochOp>(loc, IntegerType::get(builder.getContext(), 64));
  wrapEpoch->setAttr(AttrNames::Operation::CPSOuterEpoch,
                     builder.getUnitAttr());
  if (auto lbCst = getConstantIntValue(lb))
    wrapEpoch->setAttr(CPSInitIter, builder.getI64IntegerAttr(*lbCst));
  else
    wrapEpoch->setAttr(CPSInitIter, builder.getI64IntegerAttr(0));
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

  /// Step 2: Create the kickoff EDT inside the epoch (before the yield).
  OpBuilder epochBuilder = OpBuilder::atBlockTerminator(&epochBlock);

  auto kickoffEdt = epochBuilder.create<EdtOp>(
      loc, EdtType::task, EdtConcurrency::intranode, currentStateDeps);
  kickoffEdt->setAttr(CPSChainId, builder.getStringAttr(chainId));
  if (!currentStateDeps.empty())
    kickoffEdt->setAttr(CPSForwardDeps, builder.getUnitAttr());

  Block &edtBlock = kickoffEdt.getBody().front();
  for (Value dep : currentStateDeps)
    edtBlock.addArgument(dep.getType(), loc);

  /// Step 3: Clone one logical iteration into continuation-managed epochs.
  IRMapping edtMapping;
  for (auto [oldDep, blockArg] :
       llvm::zip(currentStateDeps, edtBlock.getArguments()))
    edtMapping.map(oldDep, blockArg);
  edtMapping.map(iv, lb);

  OpBuilder edtBuilder = OpBuilder::atBlockEnd(&edtBlock);
  cloneNonSlotArith(edtBuilder, body, slots, edtMapping);

  SmallVector<Block *> advanceBlocks;
  auto markClonedSlotEpochs = [&](scf::IfOp ifOp, OpBuilder &attrBuilder) {
    for (Operation &op : ifOp.getThenRegion().front())
      if (auto epoch = dyn_cast<EpochOp>(op))
        epoch->setAttr(ContinuationForEpoch, attrBuilder.getUnitAttr());
    if (!ifOp.getElseRegion().empty())
      for (Operation &op : ifOp.getElseRegion().front())
        if (auto epoch = dyn_cast<EpochOp>(op))
          epoch->setAttr(ContinuationForEpoch, attrBuilder.getUnitAttr());
  };

  auto ensureElseBranch = [&](scf::IfOp ifOp) -> scf::IfOp {
    if (!ifOp.getElseRegion().empty())
      return ifOp;

    OpBuilder ifBuilder(ifOp);
    auto newIf = ifBuilder.create<scf::IfOp>(loc, ifOp.getCondition(), true);
    Block &oldThenBlock = ifOp.getThenRegion().front();
    Block &newThenBlock = newIf.getThenRegion().front();
    for (Operation &op :
         llvm::make_early_inc_range(oldThenBlock.without_terminator()))
      op.moveBefore(newThenBlock.getTerminator());
    ifOp.erase();
    return newIf;
  };

  auto cloneSlotIntoBlock = [&](OpBuilder &slotBuilder, EpochSlot &slot,
                                IRMapping &mapping) -> Operation * {
    if (slot.wrappingIf) {
      Operation *clonedIfOp =
          slotBuilder.clone(*slot.wrappingIf.getOperation(), mapping);
      auto clonedIf = cast<scf::IfOp>(clonedIfOp);
      markClonedSlotEpochs(clonedIf, slotBuilder);
      return clonedIfOp;
    }

    Operation *clonedEpochOp =
        slotBuilder.clone(*slot.epoch.getOperation(), mapping);
    cast<EpochOp>(clonedEpochOp)
        ->setAttr(ContinuationForEpoch, builder.getUnitAttr());
    return clonedEpochOp;
  };

  OpBuilder *currentBuilder = &edtBuilder;
  IRMapping currentMapping = edtMapping;
  Operation *firstClonedSlotOp =
      cloneSlotIntoBlock(*currentBuilder, slots.front(), currentMapping);

  if (slots.size() == 1) {
    if (slots.front().wrappingIf) {
      auto clonedIf = cast<scf::IfOp>(firstClonedSlotOp);
      clonedIf = ensureElseBranch(clonedIf);
      advanceBlocks.push_back(&clonedIf.getThenRegion().front());
      advanceBlocks.push_back(&clonedIf.getElseRegion().front());
    } else {
      advanceBlocks.push_back(currentBuilder->getBlock());
    }
  } else {
    for (unsigned slotIdx = 1; slotIdx < slots.size(); ++slotIdx) {
      auto contEdt = currentBuilder->create<EdtOp>(
          loc, EdtType::task, EdtConcurrency::intranode, currentStateDeps);
      markAsContinuation(contEdt, *currentBuilder, slotIdx);
      if (!currentStateDeps.empty())
        contEdt->setAttr(CPSForwardDeps, builder.getUnitAttr());

      Block &contBlock = contEdt.getBody().front();
      for (Value dep : currentStateDeps)
        contBlock.addArgument(dep.getType(), loc);
      ensureYieldTerminator(contBlock, loc);
      OpBuilder contBuilder = OpBuilder::atBlockTerminator(&contBlock);
      IRMapping contMapping;
      contMapping.map(iv, lb);
      cloneNonSlotArith(contBuilder, body, slots, contMapping);
      Operation *clonedSlotOp =
          cloneSlotIntoBlock(contBuilder, slots[slotIdx], contMapping);

      bool isLastSlot = slotIdx + 1 == slots.size();
      if (!isLastSlot) {
        currentBuilder = &contBuilder;
        currentMapping = std::move(contMapping);
        continue;
      }

      if (slots[slotIdx].wrappingIf) {
        auto clonedIf = cast<scf::IfOp>(clonedSlotOp);
        clonedIf = ensureElseBranch(clonedIf);
        advanceBlocks.push_back(&clonedIf.getThenRegion().front());
        advanceBlocks.push_back(&clonedIf.getElseRegion().front());
      } else {
        advanceBlocks.push_back(&contBlock);
      }
    }
  }

  /// Carry the kickoff EDT pack contract in the same order EdtLowering will
  /// materialize it. Forwarded dependency state still lives on the advance EDT
  /// through `currentStateDeps`, but the CPS loop-back params must preserve the
  /// kickoff ABI rather than prepend dependency-carrier memrefs ahead of the
  /// user-visible kickoff captures.
  /// Keep the carry order aligned with EdtLowering's pack contract:
  /// parameters first, DB handles next, then dependency metadata.
  SmallVector<Value> loopBackParams = collectEdtPackedValues(kickoffEdt);
  auto iterIt = llvm::find(loopBackParams, lb);
  if (iterIt != loopBackParams.end()) {
    unsigned iterIdx = std::distance(loopBackParams.begin(), iterIt);
    kickoffEdt->setAttr(CPSIterCounterParamIdx,
                        builder.getI64IntegerAttr(iterIdx));
  }

  for (Block *advanceBlock : advanceBlocks) {
    SmallVector<Value> advanceStateDeps(currentStateDeps.begin(),
                                        currentStateDeps.end());
    auto appendAdvanceStateDep = [&](Value value) {
      if (!value || !isa<BaseMemRefType>(value.getType()))
        return;
      Operation *underlyingDb = DbUtils::getUnderlyingDb(value);
      if (!isa_and_nonnull<DbAcquireOp, DepDbAcquireOp>(underlyingDb))
        return;
      if (!llvm::is_contained(advanceStateDeps, value))
        advanceStateDeps.push_back(value);
    };
    for (Value value : loopBackParams)
      appendAdvanceStateDep(value);

    ensureYieldTerminator(*advanceBlock, loc);
    OpBuilder advanceSiteBuilder = OpBuilder::atBlockTerminator(advanceBlock);
    auto advanceEdt = advanceSiteBuilder.create<EdtOp>(
        loc, EdtType::task, EdtConcurrency::intranode, advanceStateDeps);
    advanceEdt->setAttr(kHasControlDep,
                        builder.getIntegerAttr(builder.getI32Type(), 1));
    advanceEdt->setAttr(ContinuationForEpoch, builder.getUnitAttr());
    if (!advanceStateDeps.empty())
      advanceEdt->setAttr(CPSForwardDeps, builder.getUnitAttr());
    Block &advanceBlockBody = advanceEdt.getBody().front();
    for (Value dep : advanceStateDeps)
      advanceBlockBody.addArgument(dep.getType(), loc);
    ensureYieldTerminator(advanceBlockBody, loc);

    OpBuilder advanceBuilder = OpBuilder::atBlockTerminator(&advanceBlockBody);
    Value stepI64 =
        step.getType().isIndex()
            ? advanceBuilder.create<arith::IndexCastOp>(loc, i64Ty, step)
                  .getResult()
            : advanceBuilder.create<arith::ExtSIOp>(loc, i64Ty, step)
                  .getResult();
    Value ubI64 =
        ub.getType().isIndex()
            ? advanceBuilder.create<arith::IndexCastOp>(loc, i64Ty, ub)
                  .getResult()
            : advanceBuilder.create<arith::ExtSIOp>(loc, i64Ty, ub).getResult();
    auto materializeAdvanceCarry = [&](Value value) -> Value {
      if (!isa<BaseMemRefType>(value.getType()))
        return value;
      Value rawPtr = advanceBuilder.create<polygeist::Memref2PointerOp>(
          loc, LLVM::LLVMPointerType::get(advanceBuilder.getContext()), value);
      return advanceBuilder.create<LLVM::PtrToIntOp>(loc, i64Ty, rawPtr);
    };
    SmallVector<Value> localizedLoopBackParams;
    localizedLoopBackParams.reserve(loopBackParams.size());
    for (Value value : loopBackParams) {
      auto depIt = llvm::find(advanceStateDeps, value);
      if (depIt == advanceStateDeps.end()) {
        localizedLoopBackParams.push_back(materializeAdvanceCarry(value));
        continue;
      }
      unsigned depIndex = std::distance(advanceStateDeps.begin(), depIt);
      localizedLoopBackParams.push_back(
          materializeAdvanceCarry(advanceBlockBody.getArgument(depIndex)));
    }
    SmallVector<Value> nextParams = {stepI64, ubI64};
    nextParams.append(localizedLoopBackParams.begin(),
                      localizedLoopBackParams.end());
    auto advance = advanceBuilder.create<CPSAdvanceOp>(
        loc, nextParams, advanceBuilder.getStringAttr(chainId));
    advance->setAttr(CPSAdditiveParams, builder.getUnitAttr());
    advance->setAttr(CPSDirectRecreate, builder.getUnitAttr());
    advance->setAttr(CPSNumCarry,
                     builder.getI64IntegerAttr(loopBackParams.size()));
    Region &advanceRegion = advance.getEpochBody();
    if (advanceRegion.empty())
      advanceRegion.emplaceBlock();
    ensureYieldTerminator(advanceRegion.front(), loc);
  }

  if (edtBlock.empty() || !edtBlock.back().hasTrait<OpTrait::IsTerminator>())
    edtBuilder.create<YieldOp>(loc);

  /// Step 4: Erase the original loop.
  forOp.erase();

  ARTS_INFO("CPS: Wrapped time-step loop in epoch + async advance EDT");
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
///   epoch_0(t=0, finish→cont0) → cont0 → epoch_0(t+s, finish→cont0) → ...
///
/// Case 2 (N=2, e.g. jacobi2d, second may be conditional):
///   epoch_0(finish→cont0) → cont0 → epoch_1(finish→cont1) → cont1 → advance
///
/// Case 3 (N>2, e.g. fdtd-2d):
///   epoch_0(finish→cont0) → cont0 → epoch_1(finish→cont1) → ... → contN-1 →
///   advance
///
/// Each continuation EDT is placed AFTER the epoch it continues, in the same
/// block, so EpochLowering can find it when wiring the finish target.
///
///   arts.epoch {                              // single outer wait
///     epoch_0 {continuation_for_epoch} { workers_0 }
///     arts.edt cont_0 {cps_chain_id="chain_0", ...} {
///       epoch_1 {continuation_for_epoch} { workers_1 }
///       arts.edt cont_1 {cps_chain_id="chain_1", ...} {
///         ... (nested for N>2) ...
///         arts.edt cont_{N-1} {cps_chain_id="chain_{N-1}", ...} {
///           t_next = t + step
///           if (t_next < ub) {
///             arts.cps_advance("chain_0") { workers_0_clone }
///           }
///         }
///       }
///     }
///   }
///
///===----------------------------------------------------------------------===///

/// Use centralized constant from OperationAttributes.h.
constexpr auto kCPSLoopContinuation = CPSLoopContinuation;

/// Epoch slot in a loop body — either a direct epoch or one inside scf.if.
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

/// Extend the sequential clone set with same-block pure arithmetic producers
/// required by already-selected sequential sidecar ops. This keeps stream-like
/// timer/load/subf/store chains together so cloned stores never reference the
/// original loop body.
static void expandSequentialCloneOps(Block &body,
                                     MutableArrayRef<EpochSlot> slots,
                                     SmallVectorImpl<Operation *> &ops) {
  DenseSet<Operation *> cloneSet(ops.begin(), ops.end());
  SmallVector<Operation *> worklist(ops.begin(), ops.end());

  while (!worklist.empty()) {
    Operation *op = worklist.pop_back_val();
    for (Value operand : op->getOperands()) {
      Operation *defOp = operand.getDefiningOp();
      if (!defOp || cloneSet.contains(defOp))
        continue;
      if (defOp->getBlock() != &body || isSlotOp(defOp, slots) ||
          !isSideEffectFreeArithmeticLikeOp(defOp)) {
        continue;
      }
      cloneSet.insert(defOp);
      worklist.push_back(defOp);
    }
  }

  ops.clear();
  for (Operation &op : body.without_terminator())
    if (cloneSet.contains(&op))
      ops.push_back(&op);
}

/// Ensure all external values used by an operation are available in the
/// IRMapping. For values defined outside the loop body, clone their
/// definition chain if the defining ops have no side effects.
/// This handles patterns like polygeist.pointer2memref of global addresses.
static void rematerializeExternalDefs(OpBuilder &builder, Operation *op,
                                      Block *loopBody, IRMapping &mapping) {
  // First pass: collect all external ops in dependency order.
  SmallVector<Operation *> toClone;
  SmallVector<Value> worklist;
  DenseSet<Value> visited;
  DenseSet<Operation *> scheduled;

  for (Value operand : op->getOperands())
    worklist.push_back(operand);

  while (!worklist.empty()) {
    Value v = worklist.pop_back_val();
    if (!visited.insert(v).second)
      continue;
    if (mapping.contains(v))
      continue;
    if (mlir::isa<BlockArgument>(v))
      continue;
    Operation *defOp = v.getDefiningOp();
    if (!defOp)
      continue;
    // Only rematerialize ops defined outside the loop body.
    if (loopBody->findAncestorOpInBlock(*defOp))
      continue;
    // Only rematerialize ops without side effects.
    if (!isSideEffectFreeArithmeticLikeOp(defOp) && !mlir::isPure(defOp))
      continue;
    if (!scheduled.insert(defOp).second)
      continue;
    toClone.push_back(defOp);
    // Recursively process operands.
    for (Value inner : defOp->getOperands())
      worklist.push_back(inner);
  }

  // Clone in reverse order (dependencies first).
  for (auto it = toClone.rbegin(); it != toClone.rend(); ++it) {
    Operation *defOp = *it;
    Operation *cloned = builder.clone(*defOp, mapping);
    for (auto [oldRes, newRes] :
         llvm::zip(defOp->getResults(), cloned->getResults()))
      mapping.map(oldRes, newRes);
  }
}

/// Clone all non-slot pure arithmetic and DB ops from the loop body.
/// Sequential ops (scf.for, func.call, etc.) are NOT cloned here — they
/// are placed into specific continuations by the CPS chain transform.
static void cloneNonSlotArith(OpBuilder &builder, Block &body,
                              MutableArrayRef<EpochSlot> slots,
                              IRMapping &mapping,
                              ArrayRef<Operation *> sequentialOps) {
  DenseSet<Operation *> seqOpSet(sequentialOps.begin(), sequentialOps.end());
  for (Operation &op : body.without_terminator()) {
    if (isSlotOp(&op, slots))
      continue;
    if (seqOpSet.contains(&op))
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

/// Return the last payload op in `block`, or nullptr when the block contains
/// no non-terminator ops yet.
static Operation *getLastNonTerminatorOp(Block &block) {
  if (block.empty())
    return nullptr;

  Operation *lastOp = &block.back();
  return lastOp->hasTrait<OpTrait::IsTerminator>() ? lastOp->getPrevNode()
                                                   : lastOp;
}

/// Emit the CPS advance logic for the last continuation: check loop bounds
/// and create a CPSAdvanceOp to start the next iteration.
///
/// `tCurrent` is the loop lower bound, used only for MLIR-time IRMapping
/// of cloned worker ops. The actual iteration counter is injected by
/// EpochLowering as an extra parameter in the param_pack.
static void emitAdvanceLogic(OpBuilder &builder, Location loc, Value iv,
                             Value tCurrent, Value ub, Value step, Block &body,
                             MutableArrayRef<EpochSlot> slots,
                             ArrayRef<Operation *> allSequentialOps = {},
                             ArrayRef<Value> loopBackParams = {}) {
  /// The bounds check (tCurrent + step < ub) cannot be emitted here because
  /// tCurrent is the lower bound at MLIR construction time, which gets
  /// constant-folded. Instead, we emit the CPSAdvanceOp unconditionally and
  /// attach the step and upper bound so that EpochLowering can construct the
  /// bounds check using the dynamic parameter value at resolution time.
  ///
  /// The IRMapping uses tNext to correctly map loop-body arithmetic for the
  /// cloned worker ops. Since these are inside the CPSAdvance epoch body
  /// (which runs unconditionally per iteration), using the constant tNext
  /// here doesn't cause issues — the actual iteration counter comes from
  /// the param array.
  Value tNext = builder.create<arith::AddIOp>(loc, tCurrent, step);
  IRMapping advanceMapping;
  advanceMapping.map(iv, tNext);
  cloneNonSlotArith(builder, body, slots, advanceMapping, allSequentialOps);

  auto i64Ty = IntegerType::get(builder.getContext(), 64);
  Value stepI64 = builder.create<arith::IndexCastOp>(loc, i64Ty, step);
  Value ubI64 = builder.create<arith::IndexCastOp>(loc, i64Ty, ub);
  SmallVector<Value> nextParams = {stepI64, ubI64};
  /// Carry the canonical chain_0 user parameter contract through the last
  /// continuation so EpochLowering can rebuild the loop-back pack without
  /// reconstructing it from progressively narrower child packs.
  for (Value v : loopBackParams)
    nextParams.push_back(v);
  auto advance = builder.create<CPSAdvanceOp>(loc, nextParams,
                                              builder.getStringAttr("chain_0"));
  advance->setAttr(CPSAdditiveParams, builder.getUnitAttr());
  advance->setAttr(CPSNumCarry,
                   builder.getI64IntegerAttr(loopBackParams.size()));

  Region &advanceRegion = advance.getEpochBody();
  if (advanceRegion.empty())
    advanceRegion.emplaceBlock();
  Block &advBlock = advanceRegion.front();
  ensureYieldTerminator(advBlock, loc);
  OpBuilder advBuilder = OpBuilder::atBlockTerminator(&advBlock);

  // If epoch_0 has a wrapping scf.if, clone the entire conditional
  // so the next iteration can take either branch based on parity.
  if (slots[0].wrappingIf) {
    // The wrappingIf condition depends on the induction variable (iv).
    // advanceMapping maps iv → constant (lb + step), so any iv-dependent
    // computation becomes constant-foldable. The canonicalize pass would
    // fold the scf.if to a single branch, losing the parity alternation.
    //
    // Fix: add a block argument to the advance region to serve as a
    // non-constant IV placeholder. This prevents constant folding.
    // EpochLowering's CPS advance resolver replaces this block arg with
    // the actual runtime iteration counter from the param array.
    auto indexTy = IndexType::get(builder.getContext());
    Value ivPlaceholder = advBlock.addArgument(indexTy, loc);
    advance->setAttr(CPSAdvanceHasIvArg, builder.getUnitAttr());

    // Build a fresh mapping for the wrappingIf that uses the placeholder
    // IV instead of the constant tNext. Clone only the ops needed for
    // the condition chain (which depend on iv).
    IRMapping ifMapping;
    ifMapping.map(iv, ivPlaceholder);
    // Copy all non-iv mappings from advanceMapping so epoch body ops
    // can still reference the correct values.
    for (Operation &op : body.without_terminator()) {
      if (isSlotOp(&op, slots))
        continue;
      // Re-clone non-slot arith using ivPlaceholder instead of constant.
      Operation *cloned = advBuilder.clone(op, ifMapping);
      for (auto [oldRes, newRes] :
           llvm::zip(op.getResults(), cloned->getResults()))
        ifMapping.map(oldRes, newRes);
    }

    Operation *clonedIf =
        advBuilder.clone(*slots[0].wrappingIf.getOperation(), ifMapping);
    auto ifOp = cast<scf::IfOp>(clonedIf);
    for (Operation &op : ifOp.getThenRegion().front())
      if (auto epoch = dyn_cast<EpochOp>(op))
        epoch->setAttr(ContinuationForEpoch, advBuilder.getUnitAttr());
    if (!ifOp.getElseRegion().empty())
      for (Operation &op : ifOp.getElseRegion().front())
        if (auto epoch = dyn_cast<EpochOp>(op))
          epoch->setAttr(ContinuationForEpoch, advBuilder.getUnitAttr());
  } else {
    Block &epochBody = slots[0].epoch.getBody().front();
    for (Operation &epochOp : epochBody.without_terminator())
      advBuilder.clone(epochOp, advanceMapping);
  }
}

/// Mark an EDT as a CPS continuation with the given chain ID.
static void markAsContinuation(EdtOp edt, OpBuilder &builder,
                               unsigned chainIdx) {
  std::string chainId = "chain_" + std::to_string(chainIdx);
  edt->setAttr(kHasControlDep, builder.getIntegerAttr(builder.getI32Type(), 1));
  edt->setAttr(ContinuationForEpoch, builder.getUnitAttr());
  edt->setAttr(CPSChainId, builder.getStringAttr(chainId));
  edt->setAttr(kCPSLoopContinuation, builder.getUnitAttr());
}

/// Promote outer-scope memref alloc-like ops used by sequential ops to
/// DbAllocOp so they can flow through EdtEnvManager's dbHandles -> paramv
/// machinery. Async CPS continuations cannot safely capture stack-local
/// memrefs from the original loop scope, so both heap allocs and function-local
/// allocas are rewritten to explicit DB-backed storage here.
static void promoteAllocsForCPSChain(scf::ForOp forOp,
                                     ArrayRef<Operation *> sequentialOps,
                                     MutableArrayRef<EpochSlot> slots) {
  DenseSet<Operation *> promotable;

  /// Helper to trace a memref value through cast/subview ops.
  auto traceToAllocLike = [](Value v) -> Operation * {
    while (v) {
      if (auto alloc = v.getDefiningOp<memref::AllocOp>())
        return alloc.getOperation();
      if (auto alloca = v.getDefiningOp<memref::AllocaOp>())
        return alloca.getOperation();
      if (auto cast = v.getDefiningOp<memref::CastOp>()) {
        v = cast.getSource();
        continue;
      }
      if (auto subview = v.getDefiningOp<memref::SubViewOp>()) {
        v = subview.getSource();
        continue;
      }
      break;
    }
    return nullptr;
  };

  for (Operation *seqOp : sequentialOps) {
    for (Value operand : seqOp->getOperands()) {
      Operation *allocOp = traceToAllocLike(operand);
      if (!allocOp)
        continue;
      /// Only promote alloc-like ops defined outside the loop body.
      if (forOp.getBody()->findAncestorOpInBlock(*allocOp))
        continue;
      promotable.insert(allocOp);
    }
  }

  if (promotable.empty())
    return;

  for (Operation *allocLikeOp : promotable) {
    auto allocOp = dyn_cast<memref::AllocOp>(allocLikeOp);
    auto allocaOp = dyn_cast<memref::AllocaOp>(allocLikeOp);
    if (!allocOp && !allocaOp)
      continue;

    Location loc = allocLikeOp->getLoc();
    OpBuilder builder(allocLikeOp);

    Value allocValue = allocOp ? allocOp.getResult() : allocaOp.getResult();
    MemRefType memRefType = cast<MemRefType>(allocValue.getType());
    Type elementType = memRefType.getElementType();
    SmallVector<Value> dynamicSizes;
    if (allocOp)
      dynamicSizes.append(allocOp.getDynamicSizes().begin(),
                          allocOp.getDynamicSizes().end());
    else
      dynamicSizes.append(allocaOp.getDynamicSizes().begin(),
                          allocaOp.getDynamicSizes().end());

    unsigned rank = std::max<unsigned>(1, memRefType.getRank());
    SmallVector<Value> elementSizes;
    elementSizes.reserve(rank);
    unsigned dynamicIdx = 0;
    for (unsigned i = 0; i < static_cast<unsigned>(memRefType.getRank()); ++i) {
      if (memRefType.isDynamicDim(i)) {
        elementSizes.push_back(dynamicSizes[dynamicIdx++]);
      } else {
        elementSizes.push_back(builder.create<arith::ConstantIndexOp>(
            loc, memRefType.getDimSize(i)));
      }
    }
    if (memRefType.getRank() == 0)
      elementSizes.push_back(builder.create<arith::ConstantIndexOp>(loc, 1));
    SmallVector<Value> sizes = {builder.create<arith::ConstantIndexOp>(loc, 1)};

    Value route = builder.create<arith::ConstantOp>(
        loc, builder.getI32Type(), builder.getI32IntegerAttr(0));

    auto ptrType = MemRefType::get({ShapedType::kDynamic}, memRefType);
    auto dbAlloc = builder.create<DbAllocOp>(
        loc, ArtsMode::inout, route, DbAllocType::heap, DbMode::write,
        elementType, ptrType, sizes, elementSizes);
    Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
    Value dbView = builder.create<DbRefOp>(loc, dbAlloc.getPtr(),
                                           SmallVector<Value>{zero});
    Value replacement = dbView;
    if (replacement.getType() != allocValue.getType())
      replacement =
          builder.create<memref::CastOp>(loc, allocValue.getType(), dbView);

    SmallVector<Operation *> deallocOps;
    if (allocOp) {
      for (Operation *user : allocValue.getUsers())
        if (isa<memref::DeallocOp>(user))
          deallocOps.push_back(user);
    }

    allocValue.replaceAllUsesWith(replacement);

    /// Insert db_free after the loop for both guid and ptr.
    OpBuilder afterLoop(forOp->getNextNode());
    afterLoop.create<DbFreeOp>(loc, dbAlloc.getGuid());
    afterLoop.create<DbFreeOp>(loc, dbAlloc.getPtr());

    for (Operation *deallocOp : deallocOps)
      deallocOp->erase();

    ARTS_INFO("CPS Chain: Promoted loop-external memref storage to DbAllocOp "
              "at "
              << loc);
    allocLikeOp->erase();
  }
}

/// CPS Chain Transform for N epochs per iteration.
///
/// Handles Cases 1-3: single epoch, multi-epoch with conditional, N sequential.
/// Continuation EDTs are nested in the outer epoch so that each cont_i
/// appears after epoch_i in the same block, as required by EpochLowering.
static bool tryCPSChainTransform(scf::ForOp forOp,
                                 const EpochAnalysis &epochAnalysis) {
  if (loopContainsCpsChainExcludedDepPattern(forOp)) {
    ARTS_INFO("CPS Chain: skipping loop with specialized dep pattern");
    return false;
  }

  EpochChainDecision chainDecision = epochAnalysis.evaluateCPSChain(forOp);
  if (!chainDecision.eligible) {
    ARTS_DEBUG("CPS Chain: " << chainDecision.rationale << ", skipping");
    return false;
  }

  Block &body = *forOp.getBody();
  SmallVector<EpochSlot> slots = std::move(chainDecision.slots);
  SmallVector<Operation *> sequentialOps =
      std::move(chainDecision.sequentialOps);
  expandSequentialCloneOps(body, slots, sequentialOps);
  Value iv = forOp.getInductionVar();

  unsigned N = slots.size();

  /// Partition sequential ops by which epoch pair they fall between.
  /// interEpochOps[i] = ops between slot_i and slot_{i+1}
  /// interEpochOps[N-1] = ops after the last epoch (tail)
  SmallVector<SmallVector<Operation *>> interEpochOps(N);
  if (!sequentialOps.empty()) {
    DenseSet<Operation *> seqOpSet(sequentialOps.begin(), sequentialOps.end());
    unsigned currentSlot = 0;
    for (Operation &op : body.without_terminator()) {
      // Track which slot we're past
      if (isSlotOp(&op, slots)) {
        for (unsigned s = currentSlot; s < N; ++s) {
          Operation *slotOp = slots[s].wrappingIf
                                  ? slots[s].wrappingIf.getOperation()
                                  : slots[s].epoch.getOperation();
          if (&op == slotOp) {
            currentSlot = s + 1;
            break;
          }
        }
        continue;
      }
      if (seqOpSet.contains(&op)) {
        unsigned group = std::min(currentSlot, N - 1);
        interEpochOps[group].push_back(&op);
      }
    }
  }

  /// Promote outer-scope memref.alloc ops used by sequential ops to DbAllocOp
  /// so they flow through EdtEnvManager's paramv machinery.
  if (!sequentialOps.empty())
    promoteAllocsForCPSChain(forOp, sequentialOps, slots);

  ARTS_INFO("CPS Chain: Found eligible "
            << N << "-epoch loop"
            << (sequentialOps.empty() ? "" : " (with sequential ops in body)"));

  /// === Transform ===
  ///
  /// No launcher EDT: epoch_0 and its workers are placed directly in the
  /// outer epoch to avoid db_alloc capture issues with EdtLowering's outlining.
  /// The main thread blocks on the outer epoch; continuations chain subsequent
  /// iterations asynchronously.

  OpBuilder builder(forOp->getContext());
  Location loc = forOp.getLoc();
  Value lb = forOp.getLowerBound();
  Value ub = forOp.getUpperBound();
  Value step = forOp.getStep();

  /// Step 1: Create outer epoch.
  builder.setInsertionPoint(forOp);

  auto outerEpoch =
      builder.create<EpochOp>(loc, IntegerType::get(builder.getContext(), 64));
  outerEpoch->setAttr(ContinuationForEpoch, builder.getUnitAttr());
  outerEpoch->setAttr(AttrNames::Operation::CPSOuterEpoch,
                      builder.getUnitAttr());

  /// Store the initial iteration value so EpochLowering can inject it
  /// as an extra parameter. Using an attribute avoids the iteration
  /// counter being constant-folded away before EdtLowering packs params.
  if (auto lbCst = getConstantIntValue(lb))
    outerEpoch->setAttr(CPSInitIter, builder.getI64IntegerAttr(*lbCst));
  else
    outerEpoch->setAttr(CPSInitIter, builder.getI64IntegerAttr(0));
  Region &outerRegion = outerEpoch.getRegion();
  if (outerRegion.empty())
    outerRegion.emplaceBlock();
  Block &outerBlock = outerRegion.front();
  ensureYieldTerminator(outerBlock, loc);

  /// Step 2: Clone epoch_0 and its arithmetic into the outer epoch.
  /// Sequential ops are NOT cloned here — they go into specific continuations.
  OpBuilder outerBuilder = OpBuilder::atBlockTerminator(&outerBlock);
  IRMapping firstIterMapping;
  firstIterMapping.map(iv, lb);
  cloneNonSlotArith(outerBuilder, body, slots, firstIterMapping, sequentialOps);

  // If epoch_0 has a wrapping scf.if (parity conditional), clone the
  // entire conditional so the first iteration selects the correct branch.
  if (slots[0].wrappingIf) {
    Operation *clonedIfOp = outerBuilder.clone(
        *slots[0].wrappingIf.getOperation(), firstIterMapping);
    auto clonedIf = cast<scf::IfOp>(clonedIfOp);
    for (Operation &op : clonedIf.getThenRegion().front())
      if (auto epoch = dyn_cast<EpochOp>(op))
        epoch->setAttr(ContinuationForEpoch, outerBuilder.getUnitAttr());
    if (!clonedIf.getElseRegion().empty())
      for (Operation &op : clonedIf.getElseRegion().front())
        if (auto epoch = dyn_cast<EpochOp>(op))
          epoch->setAttr(ContinuationForEpoch, outerBuilder.getUnitAttr());
  } else {
    Operation *clonedEpoch0Op =
        outerBuilder.clone(*slots[0].epoch.getOperation(), firstIterMapping);
    auto clonedEpoch0 = cast<EpochOp>(clonedEpoch0Op);
    clonedEpoch0->setAttr(ContinuationForEpoch, outerBuilder.getUnitAttr());
  }

  /// Step 3: Build the nested continuation chain (value-type builder to
  /// avoid dangling pointers across loop iterations).
  OpBuilder chainBuilder(outerBuilder);
  EdtOp firstContinuation = nullptr;
  SmallVector<std::pair<Block *, Operation *>> advanceSites;

  for (unsigned i = 0; i < N; ++i) {
    auto contEdt = chainBuilder.create<EdtOp>(
        loc, EdtType::task, EdtConcurrency::intranode, SmallVector<Value>{});
    markAsContinuation(contEdt, chainBuilder, i);
    if (!firstContinuation)
      firstContinuation = contEdt;

    Block &contBlock = contEdt.getBody().front();
    ensureYieldTerminator(contBlock, loc);
    OpBuilder contBuilder = OpBuilder::atBlockTerminator(&contBlock);

    /// Use lb as the iteration counter placeholder for MLIR-time cloning.
    /// The actual iteration counter is injected as an extra parameter by
    /// EpochLowering (not captured as an SSA value, which would be
    /// constant-folded before EdtLowering packs params).
    Value tCurrent = lb;

    /// Clone inter-epoch sequential ops for this continuation.
    /// cont_i runs after epoch_i fires; its sequential ops are those
    /// between epoch_i and epoch_{i+1} in the original loop body.
    IRMapping contMapping;
    contMapping.map(iv, tCurrent);

    if (i == N - 1) {
      /// Last continuation: clone tail sequential ops; advance is added after
      /// the chain is fully built so the loop-back parameter contract can be
      /// computed from the finalized chain_0 body.
      cloneNonSlotArith(contBuilder, body, slots, contMapping, sequentialOps);
      for (Operation *seqOp : interEpochOps[N - 1]) {
        rematerializeExternalDefs(contBuilder, seqOp, &body, contMapping);
        contBuilder.clone(*seqOp, contMapping);
      }
      advanceSites.push_back({&contBlock, getLastNonTerminatorOp(contBlock)});
    } else {
      /// Middle continuation: clone inter-epoch sequential ops,
      /// then set up epoch_{i+1} and nest cont_{i+1}.
      cloneNonSlotArith(contBuilder, body, slots, contMapping, sequentialOps);
      for (Operation *seqOp : interEpochOps[i]) {
        rematerializeExternalDefs(contBuilder, seqOp, &body, contMapping);
        contBuilder.clone(*seqOp, contMapping);
      }

      EpochSlot &nextSlot = slots[i + 1];
      if (nextSlot.wrappingIf) {
        /// Conditional epoch: clone the scf.if, mark the epoch inside its
        /// then-branch for continuation. The else-branch gets the advance
        /// logic directly, since the epoch doesn't run.
        Operation *clonedIfOp =
            contBuilder.clone(*nextSlot.wrappingIf.getOperation(), contMapping);
        auto clonedIf = cast<scf::IfOp>(clonedIfOp);

        for (Operation &thenOp : clonedIf.getThenRegion().front()) {
          if (auto epoch = dyn_cast<EpochOp>(thenOp)) {
            epoch->setAttr(ContinuationForEpoch, contBuilder.getUnitAttr());
            break;
          }
        }

        /// Add else-branch with advance logic for when condition is false.
        if (clonedIf.getElseRegion().empty()) {
          Value ifCond = clonedIf.getCondition();
          OpBuilder preIfBuilder(clonedIf);
          auto newIf =
              preIfBuilder.create<scf::IfOp>(loc, ifCond, /*withElse=*/true);

          Block &oldThenBlock = clonedIf.getThenRegion().front();
          Block &newThenBlock = newIf.getThenRegion().front();
          for (Operation &op :
               llvm::make_early_inc_range(oldThenBlock.without_terminator()))
            op.moveBefore(newThenBlock.getTerminator());

          Block &elseBlock = newIf.getElseRegion().front();
          advanceSites.push_back(
              {&elseBlock, getLastNonTerminatorOp(elseBlock)});

          clonedIf.erase();

          Block &finalThenBlock = newIf.getThenRegion().front();
          chainBuilder.setInsertionPoint(finalThenBlock.getTerminator());
        } else {
          Block &elseBlock = clonedIf.getElseRegion().front();
          advanceSites.push_back(
              {&elseBlock, getLastNonTerminatorOp(elseBlock)});

          Block &thenBlock = clonedIf.getThenRegion().front();
          chainBuilder.setInsertionPoint(thenBlock.getTerminator());
        }
      } else {
        /// Unconditional epoch: clone and mark for continuation.
        Operation *clonedEpochOp =
            contBuilder.clone(*nextSlot.epoch.getOperation(), contMapping);
        cast<EpochOp>(clonedEpochOp)
            ->setAttr(ContinuationForEpoch, contBuilder.getUnitAttr());
        chainBuilder = contBuilder;
      }
    }
  }

  auto sameValueSequence = [](ArrayRef<Value> lhs, ArrayRef<Value> rhs) {
    if (lhs.size() != rhs.size())
      return false;
    for (auto [left, right] : llvm::zip(lhs, rhs))
      if (left != right)
        return false;
    return true;
  };

  auto clearAdvanceSite = [](Block *block, Operation *anchor) {
    SmallVector<Operation *> opsToErase;
    for (Operation *op = anchor ? anchor->getNextNode() : &block->front();
         op && !op->hasTrait<OpTrait::IsTerminator>(); op = op->getNextNode()) {
      opsToErase.push_back(op);
    }
    for (Operation *op : llvm::reverse(opsToErase))
      op->erase();
  };

  /// Seed loop-back params from the actual continuation EDT pack contract so
  /// CPS carry ordering stays aligned with EdtLowering. Falling back to the
  /// original loop-body captures misses dependency metadata that EdtLowering
  /// appends structurally (for example block/element-size carry needed to
  /// rebuild dynamic memrefs), which can make direct-recreate relaunch packs
  /// drift from the kickoff ABI.
  SmallVector<Value> loopBackParams;
  if (firstContinuation) {
    loopBackParams = collectCurrentEdtPackedUserValues(firstContinuation);
    if (loopBackParams.empty()) {
      llvm::SetVector<Value> originalCaptured;
      llvm::SetVector<Value> parameters, constants, dbHandles;
      getUsedValuesDefinedAbove(forOp.getRegion(), originalCaptured);
      classifyEdtUserValues(originalCaptured.getArrayRef(), parameters,
                            constants, dbHandles);

      loopBackParams.reserve(originalCaptured.size());
      for (Value param : parameters) {
        if (auto *defOp = param.getDefiningOp())
          if (defOp->getName().getStringRef() == "llvm.mlir.undef")
            continue;
        loopBackParams.push_back(param);
      }
      loopBackParams.append(dbHandles.begin(), dbHandles.end());
    }
  }

  /// =======================================================================
  /// Scratch DB with Dep Routing — route DB handles through depv instead of
  /// paramv so CPS continuations receive valid handles on remote nodes.
  ///
  /// Classification:
  ///   - Timing DBs (1-partition): flow through depv as explicit deps
  ///   - Data GUID arrays (multi-partition): stored in a scratch DB dep
  ///   - Data PTR arrays (multi-partition): dropped (workers use depv)
  ///   - Scalars: stay in paramv (already safe)
  /// =======================================================================

  /// Helper: trace a carried value to its underlying DbAllocOp, even when
  /// lowering has wrapped it in memref/pointer/int conversions.
  auto traceToDbAlloc = [](Value v) -> DbAllocOp {
    return dyn_cast_or_null<DbAllocOp>(DbUtils::getUnderlyingDbAlloc(v));
  };

  /// Helper: compute static partition count (product of sizes).
  auto computeStaticPartitionCount = [](DbAllocOp alloc) -> int64_t {
    int64_t count = 1;
    for (Value sz : alloc.getSizes()) {
      if (auto cst = getConstantIntValue(sz))
        count *= *cst;
      else
        return -1;
    }
    return count;
  };

  /// Helper: check if a value is a DbRefOp result or a cast of one.
  auto isDbRefCast = [](Operation *defOp) -> bool {
    if (isa<DbRefOp>(defOp))
      return true;
    if (auto cast = dyn_cast<memref::CastOp>(defOp))
      if (cast.getSource().getDefiningOp<DbRefOp>())
        return true;
    return false;
  };

  struct CpsDbInfo {
    DbAllocOp alloc;
    bool isGuid;
  };

  SmallVector<Value> scalarParams;
  SmallVector<std::pair<Value, CpsDbInfo>> timingDbs;
  SmallVector<std::pair<Value, CpsDbInfo>> dataGuids;
  SmallVector<std::pair<Value, CpsDbInfo>> dataPtrs;
  SmallVector<EdtOp> allContinuations;

  if (firstContinuation) {
    outerEpoch.walk([&](EdtOp edt) {
      if (edt->hasAttr(kCPSLoopContinuation))
        allContinuations.push_back(edt);
    });
  }

  DbAllocOp scratchAlloc;
  Value scratchGuidScalar;
  unsigned numTimingDbGuids = 0;
  SmallVector<Value> depGuidValues; /// All dep GUID scalars for CPS-8 lookup.

  if (firstContinuation && !loopBackParams.empty()) {
    /// Step CPS-1: Classify loopBackParams.
    for (Value v : loopBackParams) {
      if (auto *defOp = v.getDefiningOp()) {
        if (auto dbAlloc = dyn_cast<DbAllocOp>(defOp)) {
          bool isGuid = (v == dbAlloc.getGuid());
          int64_t partCount = computeStaticPartitionCount(dbAlloc);
          if (partCount == 1) {
            timingDbs.push_back({v, {dbAlloc, isGuid}});
          } else {
            if (isGuid)
              dataGuids.push_back({v, {dbAlloc, isGuid}});
            else
              dataPtrs.push_back({v, {dbAlloc, isGuid}});
          }
          continue;
        }
        if (isDbRefCast(defOp)) {
          timingDbs.push_back({v, {traceToDbAlloc(v), false}});
          continue;
        }
      }
      scalarParams.push_back(v);
    }

    /// CPS loop-back carries may only contain scalar state plus GUID-safe
    /// dep-routing values. Multi-partition DB handle families are rebuilt
    /// inside each continuation from routed deps/scratch, never transported
    /// through paramv as raw pointers.
    loopBackParams.assign(scalarParams.begin(), scalarParams.end());

    bool hasDbHandles =
        !timingDbs.empty() || !dataGuids.empty() || !dataPtrs.empty();

    if (hasDbHandles) {
      ARTS_INFO("CPS Chain: DB routing — "
                << timingDbs.size() << " timing, " << dataGuids.size()
                << " data GUID arrays, " << dataPtrs.size()
                << " data PTR arrays (dropped), " << scalarParams.size()
                << " scalars");

      /// Step CPS-2: Create scratch DB for data GUIDs (before outer epoch).
      OpBuilder preEpochBuilder(outerEpoch);
      Value zero = preEpochBuilder.create<arith::ConstantIndexOp>(loc, 0);
      Value one = preEpochBuilder.create<arith::ConstantIndexOp>(loc, 1);

      if (!dataGuids.empty()) {
        Value totalGuidSlots =
            preEpochBuilder.create<arith::ConstantIndexOp>(loc, 0);

        SmallVector<std::pair<Value, Value>> guidArrayLayout;
        for (auto &[guidValue, info] : dataGuids) {
          Value count =
              preEpochBuilder.create<DbNumElementsOp>(loc, info.alloc);
          guidArrayLayout.push_back({totalGuidSlots, count});
          totalGuidSlots =
              preEpochBuilder.create<arith::AddIOp>(loc, totalGuidSlots, count);
        }

        Value route = preEpochBuilder.create<arith::ConstantOp>(
            loc, preEpochBuilder.getI32Type(),
            preEpochBuilder.getI32IntegerAttr(0));

        auto i64Type = preEpochBuilder.getI64Type();
        auto scratchPtrType = MemRefType::get({ShapedType::kDynamic}, i64Type);
        scratchAlloc = preEpochBuilder.create<DbAllocOp>(
            loc, ArtsMode::inout, route, DbAllocType::heap, DbMode::write,
            i64Type, scratchPtrType, SmallVector<Value>{one},
            SmallVector<Value>{totalGuidSlots});

        /// Step CPS-3: Populate scratch with GUIDs.
        Value scratchView = preEpochBuilder.create<DbRefOp>(
            loc, scratchAlloc.getPtr(), SmallVector<Value>{zero});

        for (unsigned g = 0; g < dataGuids.size(); ++g) {
          Value guidArray = dataGuids[g].first;
          auto [offset, count] = guidArrayLayout[g];

          auto loop = preEpochBuilder.create<scf::ForOp>(
              loc, preEpochBuilder.create<arith::ConstantIndexOp>(loc, 0),
              count, preEpochBuilder.create<arith::ConstantIndexOp>(loc, 1));
          OpBuilder loopBody = OpBuilder::atBlockTerminator(loop.getBody());
          Value i = loop.getInductionVar();
          Value guid =
              loopBody.create<memref::LoadOp>(loc, guidArray, ValueRange{i});
          Value scratchIdx = loopBody.create<arith::AddIOp>(loc, offset, i);
          loopBody.create<memref::StoreOp>(loc, guid, scratchView,
                                           ValueRange{scratchIdx});
        }

        scratchGuidScalar = preEpochBuilder.create<memref::LoadOp>(
            loc, scratchAlloc.getGuid(), ValueRange{zero});
      }

      /// Step CPS-7: Prepend dep GUIDs to the scalar carry state.
      SmallVector<Value> depGuidPrefix;
      if (scratchGuidScalar) {
        depGuidPrefix.push_back(scratchGuidScalar);
        depGuidValues.push_back(scratchGuidScalar);
      }

      /// Load timing DB GUIDs as i64 scalars.
      for (auto &[ptrValue, info] : timingDbs) {
        if (info.isGuid || !info.alloc)
          continue;
        Value guidScalar = preEpochBuilder.create<memref::LoadOp>(
            loc, info.alloc.getGuid(), ValueRange{zero});
        depGuidPrefix.push_back(guidScalar);
        depGuidValues.push_back(guidScalar);
        ++numTimingDbGuids;
      }

      if (!depGuidPrefix.empty()) {
        depGuidPrefix.append(loopBackParams.begin(), loopBackParams.end());
        loopBackParams = std::move(depGuidPrefix);
      }
    }
  }

  /// Emit advance logic. When we have authoritative original-loop-derived
  /// params (from firstContinuation), emit once. Otherwise fall back to
  /// the stabilization loop.
  bool loopBackParamsStabilized = false;
  if (firstContinuation && !loopBackParams.empty()) {
    for (auto [advanceBlock, anchor] : advanceSites) {
      clearAdvanceSite(advanceBlock, anchor);
      OpBuilder advanceBuilder = OpBuilder::atBlockTerminator(advanceBlock);
      emitAdvanceLogic(advanceBuilder, loc, iv, lb, ub, step, body, slots,
                       sequentialOps, loopBackParams);
    }
    loopBackParamsStabilized = true;
  } else {
    constexpr unsigned kMaxLoopBackIterations = 8;
    for (unsigned iter = 0; iter < kMaxLoopBackIterations; ++iter) {
      for (auto [advanceBlock, anchor] : advanceSites) {
        clearAdvanceSite(advanceBlock, anchor);
        OpBuilder advanceBuilder = OpBuilder::atBlockTerminator(advanceBlock);
        emitAdvanceLogic(advanceBuilder, loc, iv, lb, ub, step, body, slots,
                         sequentialOps, loopBackParams);
      }

      if (!firstContinuation)
        break;

      SmallVector<Value> updatedLoopBackParams =
          collectCurrentEdtPackedUserValues(firstContinuation);
      if (sameValueSequence(loopBackParams, updatedLoopBackParams)) {
        loopBackParamsStabilized = true;
        break;
      }
      loopBackParams = std::move(updatedLoopBackParams);
    }
  }
  if (!loopBackParamsStabilized && !advanceSites.empty())
    ARTS_WARN("CPS Chain: loop-back parameter contract reached iteration cap");

  /// Step CPS-4/5/6: Wire deps and replace body captures AFTER
  /// emitAdvanceLogic. emitAdvanceLogic clones epoch_0 into the
  /// CPSAdvanceOp region, creating new references to outer-scope DB
  /// handles. Running replacement after ensures ALL uses are caught.
  bool hasDbHandles =
      !timingDbs.empty() || !dataGuids.empty() || !dataPtrs.empty();
  if (hasDbHandles && firstContinuation) {
    /// Step CPS-4: Create deps for timing DBs + scratch.
    OpBuilder preEpochBuilder(outerEpoch);
    Value zero = preEpochBuilder.create<arith::ConstantIndexOp>(loc, 0);
    Value one = preEpochBuilder.create<arith::ConstantIndexOp>(loc, 1);
    SmallVector<Value> contDeps;
    Value scratchDepGuid;
    DenseSet<Operation *> releasedDbRoots;

    auto emitEarlyRelease = [&](Value dbPtr) {
      Operation *underlyingDb = DbUtils::getUnderlyingDb(dbPtr);
      if (!underlyingDb || !releasedDbRoots.insert(underlyingDb).second)
        return;
      preEpochBuilder.create<DbReleaseOp>(loc, dbPtr);
    };

    for (auto &[guidValue, info] : dataGuids) {
      if (info.alloc)
        emitEarlyRelease(info.alloc.getPtr());
    }

    /// Timing DB deps (1-partition coarse).
    for (auto &[ptrValue, info] : timingDbs) {
      if (info.isGuid)
        continue;
      if (!info.alloc) {
        ARTS_WARN("CPS Chain: timing DB has null alloc, skipping dep");
        continue;
      }
      emitEarlyRelease(info.alloc.getPtr());
      auto acq = preEpochBuilder.create<DbAcquireOp>(
          loc, ArtsMode::inout, info.alloc.getGuid(), info.alloc.getPtr(),
          info.alloc.getPtr().getType(),
          /*partitionMode=*/std::nullopt,
          /*indices=*/SmallVector<Value>{},
          /*offsets=*/SmallVector<Value>{zero},
          /*sizes=*/SmallVector<Value>{one});
      contDeps.push_back(acq.getPtr());
    }

    /// Stable multi-partition ptr families are loop-invariant metadata.
    /// Route them through continuation deps instead of raw param captures.
    DenseSet<Value> routedDataPtrRoots;
    for (auto &[ptrValue, info] : dataPtrs) {
      if (!info.alloc)
        continue;
      Value allocPtr = info.alloc.getPtr();
      if (!allocPtr || !routedDataPtrRoots.insert(allocPtr).second)
        continue;
      emitEarlyRelease(allocPtr);
      auto acq = preEpochBuilder.create<DbAcquireOp>(
          loc, ArtsMode::in, info.alloc.getGuid(), allocPtr, allocPtr.getType(),
          /*partitionMode=*/std::nullopt,
          /*indices=*/SmallVector<Value>{},
          /*offsets=*/SmallVector<Value>{zero},
          /*sizes=*/SmallVector<Value>{one});
      contDeps.push_back(acq.getPtr());
    }

    /// Scratch DB dep.
    if (scratchAlloc) {
      emitEarlyRelease(scratchAlloc.getPtr());
      auto scratchAcq = preEpochBuilder.create<DbAcquireOp>(
          loc, ArtsMode::in, scratchAlloc.getGuid(), scratchAlloc.getPtr(),
          scratchAlloc.getPtr().getType(),
          /*partitionMode=*/std::nullopt,
          /*indices=*/SmallVector<Value>{},
          /*offsets=*/SmallVector<Value>{zero},
          /*sizes=*/SmallVector<Value>{one});
      scratchDepGuid = scratchAcq.getGuid();
      contDeps.push_back(scratchAcq.getPtr());
    }

    /// Build dep index maps.
    DenseMap<Value, unsigned> timingPtrToDepIdx;
    DenseMap<Value, unsigned> dataPtrToDepIdx;
    unsigned depIdx = 0;
    for (auto &[ptrValue, info] : timingDbs) {
      if (info.isGuid)
        continue;
      timingPtrToDepIdx[ptrValue] = depIdx++;
    }
    for (auto &[ptrValue, info] : dataPtrs) {
      if (!info.alloc)
        continue;
      Value allocPtr = info.alloc.getPtr();
      if (!allocPtr || dataPtrToDepIdx.contains(allocPtr))
        continue;
      dataPtrToDepIdx[allocPtr] = depIdx++;
    }
    unsigned scratchDepIdx = scratchAlloc ? depIdx : UINT_MAX;

    /// GUID layout for body loading is now computed INSIDE the
    /// continuation body (step CPS-6) to avoid creating outer-scope
    /// captures that would inflate the carry params.

    /// Re-collect continuations (advance logic may have restructured).
    allContinuations.clear();
    outerEpoch.walk([&](EdtOp edt) {
      if (edt->hasAttr(kCPSLoopContinuation))
        allContinuations.push_back(edt);
    });

    /// Every CPS continuation crosses an EDT boundary. Route stable deps
    /// through each continuation, but keep parent-local rewrites out of nested
    /// CPS continuations so each stage rebuilds its own local state from the
    /// routed deps instead of capturing a parent's stack temporaries.
    for (EdtOp contEdt : allContinuations) {
      Block &contBlock = contEdt.getBody().front();

      /// Add deps and corresponding block args.
      for (Value dep : contDeps) {
        contEdt.appendDependency(dep);
        contBlock.addArgument(dep.getType(), loc);
      }
      auto chainId = contEdt->getAttrOfType<StringAttr>("arts.cps_chain_id");
      ARTS_INFO("CPS Chain: Added "
                << contDeps.size() << " deps to "
                << (chainId ? chainId.getValue() : "unknown"));

      unsigned numDeps = contDeps.size();
      unsigned numBlockArgs = contBlock.getNumArguments();

      /// Replace captures in this continuation body and its worker EDTs, but
      /// let nested CPS continuations rebuild their own state from deps.
      OpBuilder bodyBuilder(&contBlock, contBlock.begin());
      auto isWithinContinuationScope = [&](OpOperand &use) {
        Operation *owner = use.getOwner();
        if (!contEdt->isAncestor(owner))
          return false;
        for (Operation *parent = owner;
             parent && parent != contEdt.getOperation();
             parent = parent->getParentOp()) {
          if (auto edtParent = dyn_cast<EdtOp>(parent)) {
            if (edtParent != contEdt &&
                edtParent->hasAttr(kCPSLoopContinuation))
              return false;
          }
        }
        return true;
      };

      /// Timing DBs: replace uses of the ptr and any db_ref/cast results with
      /// a db_ref reconstructed from the dep block arg.
      DenseSet<Value> replacedAllocPtrs;
      for (auto &[ptrValue, info] : timingDbs) {
        if (info.isGuid)
          continue;
        auto it = timingPtrToDepIdx.find(ptrValue);
        if (it == timingPtrToDepIdx.end())
          continue;
        unsigned argIdx = numBlockArgs - numDeps + it->second;
        BlockArgument depArg = contBlock.getArgument(argIdx);

        if (info.alloc && !replacedAllocPtrs.count(info.alloc.getPtr())) {
          info.alloc.getPtr().replaceUsesWithIf(depArg,
                                                isWithinContinuationScope);
          replacedAllocPtrs.insert(info.alloc.getPtr());
        }

        if (ptrValue != (info.alloc ? info.alloc.getPtr() : Value())) {
          bodyBuilder.setInsertionPoint(&contBlock, contBlock.begin());
          Value czero = bodyBuilder.create<arith::ConstantIndexOp>(loc, 0);
          Value dbView = bodyBuilder.create<DbRefOp>(loc, depArg,
                                                     SmallVector<Value>{czero});
          Value replacement = dbView;
          if (replacement.getType() != ptrValue.getType())
            replacement = bodyBuilder.create<memref::CastOp>(
                loc, ptrValue.getType(), dbView);
          ptrValue.replaceUsesWithIf(replacement, isWithinContinuationScope);
        }
      }

      /// Stable multi-partition ptr families: rewrite continuation-local uses
      /// to the routed dep block-arg so they stop inflating CPS param packs.
      DenseSet<Value> replacedDataPtrRoots;
      for (auto &[ptrValue, info] : dataPtrs) {
        if (!info.alloc)
          continue;
        Value allocPtr = info.alloc.getPtr();
        auto it = dataPtrToDepIdx.find(allocPtr);
        if (!allocPtr || it == dataPtrToDepIdx.end())
          continue;
        unsigned argIdx = numBlockArgs - numDeps + it->second;
        BlockArgument depArg = contBlock.getArgument(argIdx);

        if (!replacedDataPtrRoots.count(allocPtr)) {
          allocPtr.replaceUsesWithIf(depArg, isWithinContinuationScope);
          replacedDataPtrRoots.insert(allocPtr);
        }

        if (ptrValue != allocPtr) {
          Value replacement = depArg;
          if (replacement.getType() != ptrValue.getType())
            replacement = bodyBuilder.create<memref::CastOp>(
                loc, ptrValue.getType(), depArg);
          ptrValue.replaceUsesWithIf(replacement, isWithinContinuationScope);
        }
      }

      /// Timing DB GUIDs are compile-time handle scaffolding only.
      DenseSet<Value> replacedAllocGuids;
      for (auto &[guidValue, info] : timingDbs) {
        if (!info.isGuid)
          continue;
        auto memrefTy = dyn_cast<MemRefType>(guidValue.getType());
        if (!memrefTy)
          continue;
        bodyBuilder.setInsertionPoint(&contBlock, contBlock.begin());
        SmallVector<Value> dynSizes;
        for (int64_t dim : memrefTy.getShape()) {
          if (dim == ShapedType::kDynamic)
            dynSizes.push_back(
                bodyBuilder.create<arith::ConstantIndexOp>(loc, 1));
        }
        Value dummyGuid =
            bodyBuilder.create<memref::AllocaOp>(loc, memrefTy, dynSizes);
        guidValue.replaceUsesWithIf(dummyGuid, isWithinContinuationScope);
        if (info.alloc && info.alloc.getGuid() != guidValue &&
            !replacedAllocGuids.count(info.alloc.getGuid())) {
          info.alloc.getGuid().replaceUsesWithIf(dummyGuid,
                                                 isWithinContinuationScope);
          replacedAllocGuids.insert(info.alloc.getGuid());
        }
      }

      /// Data GUID arrays: rebuild them from scratch inside each continuation.
      if (scratchAlloc && scratchDepIdx != UINT_MAX) {
        unsigned scratchArgIdx = numBlockArgs - numDeps + scratchDepIdx;
        BlockArgument scratchArg = contBlock.getArgument(scratchArgIdx);

        bodyBuilder.setInsertionPoint(&contBlock, contBlock.begin());
        auto i64MemrefTy =
            MemRefType::get({ShapedType::kDynamic}, bodyBuilder.getI64Type());
        auto adaptScratchReplacement = [&](Value replacement,
                                           Type targetType) -> Value {
          if (!replacement || replacement.getType() == targetType)
            return replacement;
          return bodyBuilder
              .create<memref::CastOp>(loc, targetType, replacement)
              .getResult();
        };

        if (scratchDepGuid) {
          Value replacement =
              adaptScratchReplacement(scratchArg, scratchDepGuid.getType());
          scratchDepGuid.replaceUsesWithIf(replacement,
                                           isWithinContinuationScope);
        }

        Value scratchView = scratchArg;
        if (scratchView.getType() != i64MemrefTy)
          scratchView =
              bodyBuilder.create<memref::CastOp>(loc, i64MemrefTy, scratchArg);

        Value runningOffset =
            bodyBuilder.create<arith::ConstantIndexOp>(loc, 0);
        SmallVector<std::pair<Value, Value>> guidLayoutForBody;
        for (auto &[guidValue, info] : dataGuids) {
          Value count = bodyBuilder.create<DbNumElementsOp>(loc, info.alloc);
          guidLayoutForBody.push_back({runningOffset, count});
          runningOffset =
              bodyBuilder.create<arith::AddIOp>(loc, runningOffset, count);
        }

        for (unsigned g = 0; g < dataGuids.size(); ++g) {
          Value origGuidArray = dataGuids[g].first;
          auto &guidInfo = dataGuids[g].second;
          auto [offset, count] = guidLayoutForBody[g];
          auto origMemrefTy = dyn_cast<MemRefType>(origGuidArray.getType());
          if (!origMemrefTy)
            continue;

          Value localGuids = bodyBuilder.create<memref::AllocOp>(
              loc, i64MemrefTy, ValueRange{count});
          if (auto allocId =
                  DbUtils::resolveRootAllocId(origGuidArray, guidInfo.alloc))
            setDbRootAllocId(localGuids.getDefiningOp(), *allocId);

          auto copyLoop = bodyBuilder.create<scf::ForOp>(
              loc, bodyBuilder.create<arith::ConstantIndexOp>(loc, 0), count,
              bodyBuilder.create<arith::ConstantIndexOp>(loc, 1));
          OpBuilder copyBody = OpBuilder::atBlockTerminator(copyLoop.getBody());
          Value ci2 = copyLoop.getInductionVar();
          Value scratchIdx = copyBody.create<arith::AddIOp>(loc, offset, ci2);
          Value guidVal = copyBody.create<memref::LoadOp>(
              loc, scratchView, ValueRange{scratchIdx});
          copyBody.create<memref::StoreOp>(loc, guidVal, localGuids,
                                           ValueRange{ci2});

          Value replacement = localGuids;
          if (replacement.getType() != origMemrefTy) {
            auto castOp = bodyBuilder.create<memref::CastOp>(loc, origMemrefTy,
                                                             localGuids);
            if (auto allocId =
                    DbUtils::resolveRootAllocId(origGuidArray, guidInfo.alloc))
              setDbRootAllocId(castOp.getOperation(), *allocId);
            replacement = castOp.getResult();
          }

          if (guidInfo.alloc)
            guidInfo.alloc.getGuid().replaceUsesWithIf(
                replacement, isWithinContinuationScope);
          origGuidArray.replaceUsesWithIf(replacement,
                                          isWithinContinuationScope);
        }

        /// Keep original multi-partition ptr-array handles intact. Child
        /// DbAcquireOp sourcePtr operands must retain a real DB-backed trace
        /// through the original DbAllocOp; rebuilding them into local dummy
        /// memrefs loses provenance and can stall later continuation waves.
      }
    }

    ARTS_INFO("CPS Chain: DB routing complete — "
              << contDeps.size() << " deps wired to " << allContinuations.size()
              << " continuations");
  }

  /// Step CPS-8: Re-analyze chain_0's FINAL captures to rebuild carry
  /// params in the EXACT order that EdtLowering will see. This eliminates
  /// ordering mismatches between EpochOpt's initial classification and
  /// EdtLowering's getUsedValuesDefinedAbove + classifyEdtUserValues.
  ///
  /// IMPORTANT: this step is reorder-only. Later lowering still expects the
  /// original carry arity that the CPS chain was seeded with. If the live
  /// re-analysis shrinks or grows the carry list, keep the original contract
  /// intact instead of rewriting CPSAdvanceOp to a different width.
  if (firstContinuation && hasDbHandles) {
    auto setDepRoutingAttrForCarry = [&](ArrayRef<Value> carryValues) {
      if (!(numTimingDbGuids > 0 || scratchAlloc))
        return;
      int64_t hasScratch = scratchAlloc ? 1 : 0;
      SmallVector<int64_t> routingData = {(int64_t)numTimingDbGuids,
                                          hasScratch};
      for (Value dgv : depGuidValues) {
        int64_t idx = -1;
        for (unsigned i = 0; i < carryValues.size(); ++i) {
          if (carryValues[i] == dgv) {
            idx = i;
            break;
          }
        }
        routingData.push_back(idx);
      }
      outerEpoch.walk([&](CPSAdvanceOp adv) {
        adv->setAttr(CPSDepRouting,
                     DenseI64ArrayAttr::get(adv.getContext(), routingData));
      });
    };

    SmallVector<Value> reorderedCarry =
        collectEdtPackedValues(firstContinuation);

    ARTS_INFO("CPS Chain: Re-analyzed chain_0 captures — "
              << reorderedCarry.size() << " carry params");

    bool preserveOriginalCarryContract = numTimingDbGuids > 0 || scratchAlloc;
    if (preserveOriginalCarryContract) {
      if (!sameValueSequence(reorderedCarry, loopBackParams)) {
        ARTS_INFO("CPS Chain: dep-routed carry changed after re-analysis; "
                  << "preserving original loop-back contract");
      }
      setDepRoutingAttrForCarry(loopBackParams);
    } else if (!sameValueSequence(reorderedCarry, loopBackParams)) {
      ARTS_INFO("CPS Chain: carry re-analysis changed order; preserving "
                << "original kickoff carry contract");
      setDepRoutingAttrForCarry(loopBackParams);
    } else if (reorderedCarry.size() != loopBackParams.size()) {
      ARTS_INFO("CPS Chain: carry re-analysis changed arity from "
                << loopBackParams.size() << " to " << reorderedCarry.size()
                << "; preserving original loop-back contract");
      setDepRoutingAttrForCarry(loopBackParams);
    } else {
      /// Walk all CPSAdvanceOps and replace carry params with reordered list.
      outerEpoch.walk([&](CPSAdvanceOp adv) {
        auto numCarryAttr = adv->getAttrOfType<IntegerAttr>(CPSNumCarry);
        if (!numCarryAttr)
          return;
        bool isAdditive = adv->hasAttr(CPSAdditiveParams);
        unsigned paramStart = isAdditive ? 2 : 0;
        SmallVector<Value> newNextParams;
        /// Keep step+ub (or other prefix params).
        auto oldParams = adv.getNextIterParams();
        for (unsigned i = 0; i < paramStart && i < oldParams.size(); ++i)
          newNextParams.push_back(oldParams[i]);
        /// Use the reordered carry params.
        newNextParams.append(reorderedCarry.begin(), reorderedCarry.end());

        OpBuilder advBuilder(adv);
        auto targetId = adv.getTargetChainIdAttr();
        auto newAdv = advBuilder.create<CPSAdvanceOp>(adv.getLoc(),
                                                      newNextParams, targetId);
        /// Copy attributes (skip the built-in targetChainId).
        for (auto attr : adv->getAttrs())
          if (attr.getName() != "targetChainId")
            newAdv->setAttr(attr.getName(), attr.getValue());
        newAdv->setAttr(CPSNumCarry,
                        advBuilder.getI64IntegerAttr(reorderedCarry.size()));

        /// Move epoch body from old to new.
        Region &oldRegion = adv.getEpochBody();
        Region &newRegion = newAdv.getEpochBody();
        if (!oldRegion.empty()) {
          newRegion.takeBody(oldRegion);
        }
        adv.erase();
      });

      /// Update dep routing attribute on the new CPSAdvanceOps.
      /// Format: [numTimingDbs, hasScratch, guidIdx0, guidIdx1, ...]
      /// where guidIdx values are positions within the carry params
      /// (after step+ub prefix). scratch guid index comes first if present,
      /// then timing DB guid indices.
      setDepRoutingAttrForCarry(reorderedCarry);
    }

    /// Step CPS-9b: When plan attrs are present on any worker EDT within
    /// the outer epoch, stamp the new launch state schema on all
    /// continuations. This provides a structured alternative to the
    /// positional cps_dep_routing, enabling Phase 3 CPS schema cleanup.
    bool hasPlanAttrs = false;
    outerEpoch.walk([&](EdtOp edt) {
      if (edt->hasAttr(AttrNames::Operation::Plan::KernelFamily))
        hasPlanAttrs = true;
    });
    if (hasPlanAttrs) {
      /// state_schema format: [numScalars, numTimingDbs, numDataGuids,
      ///                       numDataPtrs]
      SmallVector<int64_t> stateSchema = {
          static_cast<int64_t>(scalarParams.size()),
          static_cast<int64_t>(timingDbs.size()),
          static_cast<int64_t>(dataGuids.size()),
          static_cast<int64_t>(dataPtrs.size())};
      /// dep_schema format: [numTimingDbGuids, hasScratch]
      SmallVector<int64_t> depSchema = {
          static_cast<int64_t>(numTimingDbGuids),
          static_cast<int64_t>(scratchAlloc ? 1 : 0)};
      for (EdtOp contEdt : allContinuations) {
        contEdt->setAttr(
            AttrNames::Operation::LaunchState::StateSchema,
            DenseI64ArrayAttr::get(contEdt.getContext(), stateSchema));
        contEdt->setAttr(
            AttrNames::Operation::LaunchState::DepSchema,
            DenseI64ArrayAttr::get(contEdt.getContext(), depSchema));
      }
      ARTS_INFO("CPS Chain: Stamped launch state schema on "
                << allContinuations.size() << " continuations");
    }
  }

  /// Step 4: Erase the original loop.
  forOp.erase();

  /// Step CPS-9: Free scratch DB after the outer epoch.
  if (scratchAlloc) {
    OpBuilder afterEpoch(outerEpoch->getNextNode());
    afterEpoch.create<DbFreeOp>(loc, scratchAlloc.getGuid());
    afterEpoch.create<DbFreeOp>(loc, scratchAlloc.getPtr());
  }

  ARTS_INFO("CPS Chain: Transformed " << N
                                      << "-epoch loop into continuation chain");
  return true;
}

///===----------------------------------------------------------------------===///
/// Pass Definition
///===----------------------------------------------------------------------===///

namespace {
struct EpochOptPass : public impl::EpochOptBase<EpochOptPass> {
  EpochOptPass() : EpochOptPass(/*AM=*/nullptr) {}
  EpochOptPass(const EpochOptPass &other)
      : impl::EpochOptBase<EpochOptPass>(other), AM(other.AM),
        numRepeatedEpochLoopsAmortized(
            this, "num-repeated-epoch-loops-amortized",
            "Number of repeated epoch loops amortized"),
        numContinuationEdtsCreated(
            this, "num-continuation-edts-created",
            "Number of continuation EDTs created from epoch tails"),
        numCpsDriverLoopsTransformed(
            this, "num-cps-driver-loops-transformed",
            "Number of loops wrapped in CPS driver form"),
        numCpsChainLoopsTransformed(
            this, "num-cps-chain-loops-transformed",
            "Number of loops transformed into CPS continuation chains"),
        numEpochsNarrowed(this, "num-epochs-narrowed",
                          "Number of epochs split into narrower scopes"),
        numEpochsCreatedByNarrowing(
            this, "num-epochs-created-by-narrowing",
            "Number of new epochs created while narrowing epoch scopes"),
        numEpochPairsFused(this, "num-epoch-pairs-fused",
                           "Number of consecutive epoch pairs fused"),
        numWorkerLoopsFused(this, "num-worker-loops-fused",
                            "Number of worker loop pairs fused inside epochs") {
  }
  explicit EpochOptPass(mlir::arts::AnalysisManager *AM)
      : AM(AM), numRepeatedEpochLoopsAmortized(
                    this, "num-repeated-epoch-loops-amortized",
                    "Number of repeated epoch loops amortized"),
        numContinuationEdtsCreated(
            this, "num-continuation-edts-created",
            "Number of continuation EDTs created from epoch tails"),
        numCpsDriverLoopsTransformed(
            this, "num-cps-driver-loops-transformed",
            "Number of loops wrapped in CPS driver form"),
        numCpsChainLoopsTransformed(
            this, "num-cps-chain-loops-transformed",
            "Number of loops transformed into CPS continuation chains"),
        numEpochsNarrowed(this, "num-epochs-narrowed",
                          "Number of epochs split into narrower scopes"),
        numEpochsCreatedByNarrowing(
            this, "num-epochs-created-by-narrowing",
            "Number of new epochs created while narrowing epoch scopes"),
        numEpochPairsFused(this, "num-epoch-pairs-fused",
                           "Number of consecutive epoch pairs fused"),
        numWorkerLoopsFused(this, "num-worker-loops-fused",
                            "Number of worker loop pairs fused inside epochs") {
  }

  EpochOptPass(mlir::arts::AnalysisManager *AM, bool narrowing, bool fusion,
               bool loopFusion, bool amortization, bool continuation,
               bool cpsDriver, bool cpsChain = false)
      : EpochOptPass(AM) {
    enableEpochNarrowing = narrowing;
    enableEpochFusion = fusion;
    enableLoopFusion = loopFusion;
    enableAmortization = amortization;
    enableContinuation = continuation;
    enableCPSDriver = cpsDriver;
    enableCPSChain = cpsChain;
  }

  mlir::arts::AnalysisManager &getEpochAnalysisManager(ModuleOp module) {
    if (!AM) {
      ownedAM = std::make_unique<mlir::arts::AnalysisManager>(module);
      AM = ownedAM.get();
    }
    return *AM;
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool changed = false;
    mlir::arts::AnalysisManager &epochAM = getEpochAnalysisManager(module);
    EpochAnalysis &epochAnalysis = epochAM.getEpochAnalysis();

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
        EpochAsyncLoopDecision asyncDecision =
            epochAnalysis.evaluateAsyncLoopStrategy(forOp);
        if (asyncDecision.strategy != EpochAsyncLoopStrategy::CpsChain) {
          ARTS_DEBUG("CPS Chain: selector chose "
                     << asyncLoopStrategyToString(asyncDecision.strategy)
                     << " (" << asyncDecision.rationale << "), skipping");
          continue;
        }
        if (tryCPSChainTransform(forOp, epochAnalysis))
          ++chainTransformed;
      }
      if (chainTransformed > 0) {
        numCpsChainLoopsTransformed += chainTransformed;
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
        EpochAsyncLoopDecision asyncDecision =
            epochAnalysis.evaluateAsyncLoopStrategy(forOp);
        if (asyncDecision.strategy != EpochAsyncLoopStrategy::AdvanceEdt) {
          ARTS_DEBUG("CPS Driver: selector chose "
                     << asyncLoopStrategyToString(asyncDecision.strategy)
                     << " (" << asyncDecision.rationale << "), skipping");
          continue;
        }
        if (tryCPSLoopTransform(forOp, epochAnalysis))
          ++cpsTransformed;
      }
      if (cpsTransformed > 0) {
        numCpsDriverLoopsTransformed += cpsTransformed;
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
        numRepeatedEpochLoopsAmortized += amortized;
        ARTS_INFO("Amortized " << amortized << " repeated epoch loop(s)");
        changed = true;
      }
    }

    if (enableContinuation) {
      unsigned transformed = 0;
      for (EpochOp epochOp : epochOps) {
        if (!epochOp || !epochOp->getBlock())
          continue;

        EpochContinuationDecision decision = epochAnalysis.evaluateContinuation(
            epochOp, dyn_cast_or_null<EpochOp>(epochOp->getPrevNode()),
            /*continuationEnabled=*/true);
        if (!decision.eligible) {
          ARTS_DEBUG(
              "  Epoch not eligible for continuation: " << decision.rationale);
          continue;
        }

        ARTS_INFO("  Transforming eligible epoch to continuation form");
        if (succeeded(transformToContinuation(epochOp, decision)))
          ++transformed;
      }
      if (transformed > 0) {
        numContinuationEdtsCreated += transformed;
        ARTS_INFO("Transformed " << transformed
                                 << " epochs to continuation form");
        changed = true;
      }
    }

    /// === Structural optimizations ===

    /// Narrowing runs FIRST so it can create opportunities for fusion.
    if (enableEpochNarrowing) {
      EpochNarrowingCounts narrowingCounts = narrowEpochScopes(module);
      if (narrowingCounts.epochsNarrowed != 0) {
        numEpochsNarrowed += narrowingCounts.epochsNarrowed;
        numEpochsCreatedByNarrowing += narrowingCounts.newEpochsCreated;
        ARTS_INFO("Epoch scope narrowing applied");
        changed = true;
      }
    }

    if (enableEpochFusion) {
      unsigned fusedEpochPairs = processRegionForEpochFusion(
          module.getRegion(), epochAnalysis, enableContinuation);
      if (fusedEpochPairs != 0) {
        numEpochPairsFused += fusedEpochPairs;
        ARTS_INFO("Epoch fusion applied");
        changed = true;
      }
    }

    if (enableLoopFusion) {
      unsigned fusedWorkerLoops = 0;
      module.walk([&](EpochOp epoch) {
        fusedWorkerLoops += fuseWorkerLoopsInEpoch(epoch);
      });
      if (fusedWorkerLoops != 0) {
        numWorkerLoopsFused += fusedWorkerLoops;
        ARTS_INFO("Epoch worker loop fusion applied");
        changed = true;
      }
    }

    ARTS_INFO_FOOTER(EpochOptPass);

    if (!changed)
      markAllAnalysesPreserved();
  }

private:
  mlir::arts::AnalysisManager *AM = nullptr;
  Statistic numRepeatedEpochLoopsAmortized;
  Statistic numContinuationEdtsCreated;
  Statistic numCpsDriverLoopsTransformed;
  Statistic numCpsChainLoopsTransformed;
  Statistic numEpochsNarrowed;
  Statistic numEpochsCreatedByNarrowing;
  Statistic numEpochPairsFused;
  Statistic numWorkerLoopsFused;
  std::unique_ptr<mlir::arts::AnalysisManager> ownedAM;
};
} // namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createEpochOptPass() {
  return std::make_unique<EpochOptPass>();
}

std::unique_ptr<Pass> createEpochOptPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<EpochOptPass>(AM);
}

std::unique_ptr<Pass> createEpochOptPass(mlir::arts::AnalysisManager *AM,
                                         bool amortization, bool continuation,
                                         bool cpsDriver, bool cpsChain) {
  return std::make_unique<EpochOptPass>(AM, /*narrowing=*/true, /*fusion=*/true,
                                        /*loopFusion=*/true, amortization,
                                        continuation, cpsDriver, cpsChain);
}

std::unique_ptr<Pass> createEpochOptPass(bool amortization, bool continuation,
                                         bool cpsDriver, bool cpsChain) {
  return createEpochOptPass(/*AM=*/nullptr, amortization, continuation,
                            cpsDriver, cpsChain);
}

std::unique_ptr<Pass>
createEpochOptSchedulingPass(mlir::arts::AnalysisManager *AM, bool amortization,
                             bool continuation, bool cpsDriver, bool cpsChain) {
  return std::make_unique<EpochOptPass>(
      AM, /*narrowing=*/false, /*fusion=*/false, /*loopFusion=*/false,
      amortization, continuation, cpsDriver, cpsChain);
}

std::unique_ptr<Pass> createEpochOptSchedulingPass(bool amortization,
                                                   bool continuation,
                                                   bool cpsDriver,
                                                   bool cpsChain) {
  return createEpochOptSchedulingPass(/*AM=*/nullptr, amortization,
                                      continuation, cpsDriver, cpsChain);
}
} // namespace arts
} // namespace mlir
