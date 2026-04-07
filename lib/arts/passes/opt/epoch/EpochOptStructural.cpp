///==========================================================================///
/// File: EpochOptStructural.cpp
///
/// Structural and local scheduling transforms used by EpochOpt.
///==========================================================================///

#include "EpochOptInternal.h"
#include "arts/utils/Debug.h"

ARTS_DEBUG_SETUP(epoch_opt);

using namespace mlir;
using namespace mlir::arts;

namespace mlir::arts::epoch_opt {
namespace {

SmallVector<unsigned> findEpochCutPoints(ArrayRef<Operation *> ops) {
  SmallVector<unsigned> cutPoints;
  if (ops.size() <= 1)
    return cutPoints;

  unsigned n = ops.size();
  DenseMap<Operation *, unsigned> opIndex;
  for (unsigned k = 0; k < n; ++k)
    opIndex[ops[k]] = k;

  SmallVector<unsigned> minDep(n, n);
  for (unsigned j = 0; j < n; ++j) {
    unsigned depMin = n;

    auto checkOperand = [&](Value operand) {
      if (auto *defOp = operand.getDefiningOp()) {
        auto it = opIndex.find(defOp);
        if (it != opIndex.end())
          depMin = std::min(depMin, it->second);
      }
    };

    for (Value operand : ops[j]->getOperands())
      checkOperand(operand);

    ops[j]->walk([&](Operation *nested) {
      if (nested == ops[j])
        return WalkResult::advance();
      for (Value operand : nested->getOperands())
        checkOperand(operand);
      return WalkResult::advance();
    });

    for (unsigned k = 0; k < j; ++k) {
      if (DbAnalysis::hasDbConflict(ops[j], ops[k]))
        depMin = std::min(depMin, k);
    }

    minDep[j] = depMin;
  }

  SmallVector<unsigned> suffixMin(n);
  suffixMin[n - 1] = minDep[n - 1];
  for (int k = static_cast<int>(n) - 2; k >= 0; --k)
    suffixMin[k] = std::min(minDep[k], suffixMin[k + 1]);

  for (unsigned i = 1; i < n; ++i) {
    if (suffixMin[i] >= i)
      cutPoints.push_back(i);
  }

  return cutPoints;
}

unsigned narrowEpoch(EpochOp epoch) {
  Block &body = epoch.getBody().front();

  SmallVector<Operation *> ops;
  for (Operation &op : body.without_terminator())
    ops.push_back(&op);

  if (ops.size() <= 1)
    return 0;

  SmallVector<unsigned> cutPoints = findEpochCutPoints(ops);
  if (cutPoints.empty())
    return 0;

  ARTS_DEBUG("Found " << cutPoints.size() << " cut point(s) in epoch");

  SmallVector<std::pair<unsigned, unsigned>> groups;
  unsigned start = 0;
  for (unsigned cp : cutPoints) {
    groups.push_back({start, cp});
    start = cp;
  }
  groups.push_back({start, static_cast<unsigned>(ops.size())});

  if (groups.size() <= 1)
    return 0;

  Location loc = epoch.getLoc();
  bool epochGuidUsed = !epoch.getEpochGuid().use_empty();
  if (epochGuidUsed) {
    ARTS_DEBUG("Epoch GUID is used externally, skipping narrowing");
    return 0;
  }

  OpBuilder builder(epoch->getBlock(), std::next(Block::iterator(epoch)));
  for (unsigned g = 1; g < groups.size(); ++g) {
    auto [gStart, gEnd] = groups[g];
    auto newEpoch = builder.create<EpochOp>(loc);
    auto &newBlock = newEpoch.getBody().emplaceBlock();
    OpBuilder innerBuilder(&newBlock, newBlock.begin());
    innerBuilder.create<YieldOp>(loc);

    Operation *terminator = newBlock.getTerminator();
    for (unsigned i = gStart; i < gEnd; ++i)
      ops[i]->moveBefore(terminator);

    builder.setInsertionPointAfter(newEpoch);
  }

  ARTS_DEBUG("Split epoch into " << groups.size() << " narrower epochs");
  return groups.size() - 1;
}

void fuseEpochs(EpochOp first, EpochOp second) {
  if (!second.getEpochGuid().use_empty())
    return;

  Block &firstBlock = first.getBody().front();
  Block &secondBlock = second.getBody().front();
  spliceBodyBeforeTerminator(secondBlock, firstBlock);

  second.erase();
}

bool isFusableWorkerLoop(scf::ForOp a, scf::ForOp b) {
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

void fuseLoops(scf::ForOp first, scf::ForOp second) {
  Block &firstBody = first.getRegion().front();
  Block &secondBody = second.getRegion().front();
  spliceBodyBeforeTerminator(secondBody, firstBody);

  Value oldIv = secondBody.getArgument(0);
  Value newIv = firstBody.getArgument(0);
  oldIv.replaceAllUsesWith(newIv);

  second.erase();
}

bool isKernelTimerTailOp(Operation *op) {
  auto callOp = dyn_cast<func::CallOp>(op);
  if (!callOp)
    return false;
  return callOp.getCallee().starts_with("carts_kernel_timer_");
}

bool canWrapEdtBodyWithRepeatLoop(EdtOp edt) {
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

void wrapEdtBodyWithRepeatLoop(EdtOp edt, int64_t repeatCount) {
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

bool epochSupportsRepetitionAmortization(EpochOp epochOp) {
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

} // namespace

EpochNarrowingCounts narrowEpochScopes(ModuleOp module) {
  EpochNarrowingCounts counts;
  SmallVector<EpochOp> epochs;
  module.walk([&](EpochOp epoch) { epochs.push_back(epoch); });

  for (EpochOp epoch : epochs) {
    if (!epoch->getBlock())
      continue;
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

unsigned processRegionForEpochFusion(Region &region,
                                     const EpochAnalysis &epochAnalysis,
                                     bool continuationEnabled) {
  unsigned fusedPairs = 0;
  for (Block &block : region) {
    for (auto it = block.begin(); it != block.end();) {
      auto first = dyn_cast<EpochOp>(&*it);
      if (!first) {
        ++it;
        continue;
      }

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
            first, second, continuationEnabled, &firstAccesses,
            &secondAccesses);
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

    for (Operation &op : block) {
      for (Region &nested : op.getRegions())
        fusedPairs += processRegionForEpochFusion(nested, epochAnalysis,
                                                  continuationEnabled);
    }
  }
  return fusedPairs;
}

unsigned fuseWorkerLoopsInEpoch(EpochOp epoch) {
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

bool tryAmortizeRepeatedEpochLoop(EpochOp epochOp) {
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

} // namespace mlir::arts::epoch_opt
