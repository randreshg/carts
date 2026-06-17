///==========================================================================///
/// File: VerifyArtsCdag.cpp
///
/// Fail-closed verification of the ARTS canonical-owner DAG after CreateEpochs.
/// Rejects (does not repair) IR where a distributed acquire lacks a committed
/// DB mode, a distributed DB has inconsistent owner routing/placement, an
/// SDE-partitioned MU was coarsened or localized, single-writer-multiple-reader
/// is violated per DB block grain, or the EDT happens-before graph has a cycle.
///==========================================================================///

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbDistributedEligibility.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/EdtUtils.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#define GEN_PASS_DEF_VERIFYARTSCDAG
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"

#include <functional>
#include <map>
#include <string>
#include <tuple>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

static bool isWriterAcquire(DbAcquireOp acquire) {
  if (auto verdict = acquire.getRuntimeDbMode())
    return *verdict != RuntimeDbMode::ro;
  ArtsMode mode = acquire.getMode();
  return mode == ArtsMode::out || mode == ArtsMode::inout;
}

/// No internode ARTS task may depend on a coarse single-block aggregate user
/// DB: distributed execution requires block DB storage, a per-node-localized
/// host bridge, or a small replicated read.
static void verifyCdagDistributedDbDeps(EdtOp edt, bool &failed) {
  if (!edt || edt.getConcurrency() != EdtConcurrency::internode)
    return;

  llvm::SmallPtrSet<Operation *, 4> reported;
  for (Value dep : edt.getDependencies()) {
    auto alloc =
        dyn_cast_or_null<DbAllocOp>(DbUtils::getUnderlyingDbAlloc(dep));
    if (!DbUtils::isCoarseUserDataDb(alloc))
      continue;
    if (DbUtils::isAllowedReadOnlyCoarseDep(dep, alloc))
      continue;
    if (DbUtils::isHostWholeToComputeBlockBridgeMovement(edt))
      continue;
    if (!reported.insert(alloc.getOperation()).second)
      continue;

    InFlightDiagnostic diag =
        edt.emitError()
        << "internode ARTS task depends on a coarse single-block aggregate "
           "user DB";
    diag.attachNote(alloc.getLoc())
        << "coarse DB allocation feeding the distributed task";
    diag.attachNote(edt.getLoc())
        << "SDE must realize block DB storage before ARTS distributed "
           "execution; CreateDbs is only a coarse raw-memref bridge";
    failed = true;
  }
}

struct DbRangeDim {
  int64_t offset = 0;
  int64_t size = 0;
};

struct DbWriterRegion {
  Operation *edt = nullptr;
  Operation *acquireOp = nullptr;
  DbAcquireOp acquire;
  SmallVector<DbRangeDim, 4> ranges;
  std::string symbolicKey;
  bool whole = false;
  bool exact = false;
};

static std::string valueKey(Value value) {
  if (auto constant = ValueAnalysis::tryFoldConstantIndex(value))
    return std::to_string(*constant);
  std::string text;
  llvm::raw_string_ostream os(text);
  os << "v" << static_cast<const void *>(value.getAsOpaquePointer());
  return os.str();
}

static std::optional<DbWriterRegion> writerRegion(DbAcquireOp acquire,
                                                  Operation *edt) {
  DbWriterRegion region;
  region.edt = edt;
  region.acquireOp = acquire.getOperation();
  region.acquire = acquire;

  if (!acquire.getIndices().empty()) {
    region.exact = true;
    region.symbolicKey = "point:";
    for (Value index : acquire.getIndices()) {
      std::optional<int64_t> constant =
          ValueAnalysis::tryFoldConstantIndex(index);
      region.symbolicKey += valueKey(index);
      region.symbolicKey += ',';
      if (!constant) {
        region.exact = false;
        continue;
      }
      region.ranges.push_back(DbRangeDim{*constant, 1});
    }
    if (!region.exact)
      region.ranges.clear();
    return region;
  }

  if (!acquire.getOffsets().empty() || !acquire.getSizes().empty()) {
    if (acquire.getOffsets().size() != acquire.getSizes().size())
      return std::nullopt;
    region.exact = true;
    region.symbolicKey = "range:";
    for (auto [offset, size] :
         llvm::zip(acquire.getOffsets(), acquire.getSizes())) {
      std::optional<int64_t> offsetConstant =
          ValueAnalysis::tryFoldConstantIndex(offset);
      std::optional<int64_t> sizeConstant =
          ValueAnalysis::tryFoldConstantIndex(size);
      region.symbolicKey += valueKey(offset);
      region.symbolicKey += '+';
      region.symbolicKey += valueKey(size);
      region.symbolicKey += ',';
      if (!offsetConstant || !sizeConstant || *sizeConstant <= 0) {
        region.exact = false;
        continue;
      }
      region.ranges.push_back(DbRangeDim{*offsetConstant, *sizeConstant});
    }
    if (!region.exact)
      region.ranges.clear();
    return region;
  }

  region.whole = true;
  region.exact = true;
  region.symbolicKey = "whole";
  return region;
}

static bool exactRangesOverlap(ArrayRef<DbRangeDim> lhs,
                               ArrayRef<DbRangeDim> rhs) {
  if (lhs.size() != rhs.size())
    return true;
  for (auto [left, right] : llvm::zip(lhs, rhs)) {
    int64_t leftEnd = left.offset + left.size;
    int64_t rightEnd = right.offset + right.size;
    if (leftEnd <= right.offset || rightEnd <= left.offset)
      return false;
  }
  return true;
}

static std::optional<bool> writerRegionsOverlap(const DbWriterRegion &lhs,
                                                const DbWriterRegion &rhs) {
  if (lhs.whole || rhs.whole)
    return true;
  if (lhs.exact && rhs.exact)
    return exactRangesOverlap(lhs.ranges, rhs.ranges);
  if (!lhs.symbolicKey.empty() && lhs.symbolicKey == rhs.symbolicKey)
    return true;
  return std::nullopt;
}

struct CommittedDbUseSummary {
  bool sawEdtDependency = false;
  Operation *badUser = nullptr;
};

static bool isEdtDependencyUser(Operation *user, Value value) {
  auto edt = dyn_cast_or_null<EdtOp>(user);
  return edt && llvm::is_contained(edt.getDependencies(), value);
}

static bool isCleanupTerminalUse(Operation *user, Value value) {
  if (auto release = dyn_cast_or_null<DbReleaseOp>(user))
    return release.getSource() == value;
  if (auto free = dyn_cast_or_null<DbFreeOp>(user))
    return free.getSource() == value;
  return false;
}

static bool isAcquireSourceUse(Operation *user, Value value) {
  auto acquire = dyn_cast_or_null<DbAcquireOp>(user);
  return acquire &&
         (acquire.getSourcePtr() == value ||
          (acquire.getSourceGuid() && acquire.getSourceGuid() == value));
}

static bool enqueueForwardedDbValues(Operation *user, Value value,
                                     SmallVectorImpl<Value> &worklist) {
  if (!user || !value)
    return false;

  if (auto acquire = dyn_cast<DbAcquireOp>(user)) {
    if (!isAcquireSourceUse(user, value))
      return false;
    worklist.push_back(acquire.getGuid());
    worklist.push_back(acquire.getPtr());
    return true;
  }
  if (auto ref = dyn_cast<DbRefOp>(user)) {
    if (ref.getSource() != value)
      return false;
    worklist.push_back(ref.getResult());
    return true;
  }
  if (auto cast = dyn_cast<memref::CastOp>(user)) {
    if (cast.getSource() != value)
      return false;
    worklist.push_back(cast.getResult());
    return true;
  }
  if (auto subview = dyn_cast<memref::SubViewOp>(user)) {
    if (subview.getSource() != value)
      return false;
    worklist.push_back(subview.getResult());
    return true;
  }
  if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(user)) {
    if (!llvm::is_contained(unrealized.getInputs(), value))
      return false;
    for (Value result : unrealized.getOutputs())
      if (isa<MemRefType>(result.getType()))
        worklist.push_back(result);
    return true;
  }

  return false;
}

static CommittedDbUseSummary
summarizeCommittedDbUses(Value source, bool stopAtAcquire = false) {
  CommittedDbUseSummary summary;
  SmallVector<Value, 16> worklist;
  DenseSet<Value> visited;
  worklist.push_back(source);

  while (!worklist.empty() && !summary.badUser) {
    Value current = worklist.pop_back_val();
    if (!current || !visited.insert(current).second)
      continue;

    for (Operation *user : current.getUsers()) {
      if (isEdtDependencyUser(user, current)) {
        summary.sawEdtDependency = true;
        continue;
      }
      if (isCleanupTerminalUse(user, current))
        continue;
      if (stopAtAcquire && isAcquireSourceUse(user, current))
        continue;
      if (auto access = DbUtils::getMemoryAccessInfo(user)) {
        if (access->memref == current) {
          summary.badUser = user;
          break;
        }
        continue;
      }
      if (enqueueForwardedDbValues(user, current, worklist))
        continue;
      summary.badUser = user;
      break;
    }
  }

  return summary;
}

static CommittedDbUseSummary summarizeAcquireUses(DbAcquireOp acquire) {
  CommittedDbUseSummary ptrSummary = summarizeCommittedDbUses(acquire.getPtr());
  if (ptrSummary.badUser)
    return ptrSummary;
  if (acquire.getGuid()) {
    CommittedDbUseSummary guidSummary =
        summarizeCommittedDbUses(acquire.getGuid());
    ptrSummary.sawEdtDependency |= guidSummary.sawEdtDependency;
    if (guidSummary.badUser)
      ptrSummary.badUser = guidSummary.badUser;
  }
  return ptrSummary;
}

static LogicalResult verifyCdagAllocDirectUses(DbAllocOp alloc) {
  if (!hasArtsDbPhysicalLayout(alloc.getOperation()))
    return success();

  CommittedDbUseSummary ptrSummary =
      summarizeCommittedDbUses(alloc.getPtr(), /*stopAtAcquire=*/true);
  if (ptrSummary.badUser)
    return alloc.emitOpError()
           << "exposes a committed SDE block-layout DB through a direct "
              "non-EDT/non-cleanup use; ARTS must access committed DB state "
              "through explicit acquires and EDT dependencies";

  CommittedDbUseSummary guidSummary =
      summarizeCommittedDbUses(alloc.getGuid(), /*stopAtAcquire=*/true);
  if (guidSummary.badUser)
    return alloc.emitOpError()
           << "exposes a committed SDE block-layout DB GUID through a direct "
              "non-EDT/non-cleanup use; ARTS must access committed DB state "
              "through explicit acquires and EDT dependencies";

  return success();
}

static bool hasAllowedNonDistributedBlockLayoutEvidence(DbAllocOp alloc) {
  return alloc.getPerBlockReplicated().value_or(false);
}

/// (A) owner-route consistency + (B) distribution preservation.
static LogicalResult verifyCdagAlloc(DbAllocOp alloc) {
  bool distributed = hasDistributedDbAllocation(alloc.getOperation());

  /// (B) An SDE-partitioned MU carries a committed physical block layout. ARTS
  /// must realize it as a distributed DB or a derived all-gather replica.
  /// A committed block grid marked `local_only` is not preservation evidence.
  bool hasNonDistributedEvidence =
      hasAllowedNonDistributedBlockLayoutEvidence(alloc);
  if (hasArtsDbPhysicalLayout(alloc.getOperation()) && !distributed &&
      !hasNonDistributedEvidence)
    return alloc.emitOpError()
           << "carries a committed SDE block layout but is neither realized as "
              "a distributed DB nor marked non-distributed; ARTS must not "
              "silently coarsen an SDE-partitioned MU";

  if (!distributed)
    return success();

  /// (A) A distributed DB must have a derivable owner route.
  DbOwnerRouteFailure failure = getDistributedDbOwnerRouteFailure(alloc);
  if (failure != DbOwnerRouteFailure::None)
    return alloc.emitOpError() << "has inconsistent distributed owner routing: "
                               << toString(failure);
  return success();
}

static bool hasCleanupOnlyAcquireUses(DbAcquireOp acquire);

static LogicalResult verifyCdagWriterLaunch(DbAcquireOp acquire,
                                            bool requiresInterNodeRouting) {
  if (!isWriterAcquire(acquire))
    return success();
  DbAllocOp alloc = DbUtils::getUnderlyingDbAllocOp(acquire.getSourcePtr());
  if (!alloc || !hasDistributedDbAllocation(alloc.getOperation()))
    return success();
  CommittedDbUseSummary uses = summarizeAcquireUses(acquire);
  if (uses.badUser)
    return acquire.emitOpError()
           << "writes a distributed DB through a non-EDT/non-cleanup use; "
              "distributed ownership requires explicit owner-routed codelets";

  auto [edt, blockArg] = EdtUtils::getBlockArgumentForAcquire(acquire);
  (void)blockArg;
  if (!edt && !hasCleanupOnlyAcquireUses(acquire))
    return acquire.emitOpError()
           << "writes a distributed DB outside an ARTS EDT; distributed "
              "ownership requires explicit owner-routed codelets";
  if (!edt)
    return success();
  if (requiresInterNodeRouting &&
      edt.getConcurrency() != EdtConcurrency::internode)
    return edt.emitOpError()
           << "writes a distributed DB from an intranode EDT after launch "
              "consistency; ARTS must promote and route owner-local writers";
  return success();
}

/// (A) Every distributed acquire carries a committed DB mode; missing modes are
/// rejected here, before ARTS-RT, instead of inferred at lowering.
static LogicalResult verifyCdagAcquireMode(DbAcquireOp acquire) {
  DbAllocOp alloc = DbUtils::getUnderlyingDbAllocOp(acquire.getSourcePtr());
  if (!alloc || !hasDistributedDbAllocation(alloc.getOperation()))
    return success();
  if (!acquire.getRuntimeDbMode())
    return acquire.emitOpError()
           << "acquires a distributed DB without a committed runtime DB mode; "
              "ARTS-RT must not infer it";
  return success();
}

/// (A) Every distributed partial halo acquire carries explicit element-window
/// operands. A halo_slice is diagnostic reach metadata only; ARTS-RT must not
/// reconstruct byte slices from it. Reject here, before ARTS-RT.
static LogicalResult verifyCdagAcquireWindow(DbAcquireOp acquire) {
  DbAllocOp alloc = DbUtils::getUnderlyingDbAllocOp(acquire.getSourcePtr());
  if (!alloc || !hasDistributedDbAllocation(alloc.getOperation()))
    return success();
  if (!DbUtils::acquiresPartialHaloWindow(acquire))
    return success();
  if (DbUtils::hasCommittedDbSpaceWindow(acquire))
    return success();
  return acquire.emitOpError()
         << "acquires a partial halo window of a distributed DB without "
            "explicit "
            "element_offsets/element_sizes; ARTS-RT must not infer it";
}

static bool hasCleanupOnlyAcquireUses(DbAcquireOp acquire) {
  llvm::SetVector<Operation *> cleanupOps;
  return DbUtils::collectCleanupOnlyUseChain(acquire.getGuid(), cleanupOps) &&
         DbUtils::collectCleanupOnlyUseChain(acquire.getPtr(), cleanupOps);
}

static LogicalResult verifyCdagCommittedAcquireUser(DbAcquireOp acquire) {
  DbAllocOp alloc = DbUtils::getUnderlyingDbAllocOp(acquire.getSourcePtr());
  if (!alloc || !hasArtsDbPhysicalLayout(alloc.getOperation()))
    return success();
  CommittedDbUseSummary uses = summarizeAcquireUses(acquire);
  if (uses.badUser)
    return acquire.emitOpError()
           << "observes a committed SDE block-layout DB through a "
              "non-EDT/non-cleanup use; ARTS must realize host/final "
              "observation as explicit EDT/gather work before ARTS-RT";
  auto [edt, blockArg] = EdtUtils::getBlockArgumentForAcquire(acquire);
  (void)blockArg;
  if (edt && uses.sawEdtDependency)
    return success();
  if (hasCleanupOnlyAcquireUses(acquire))
    return success();
  return acquire.emitOpError()
         << "observes a committed SDE block-layout DB outside an ARTS EDT; "
            "ARTS must realize host/final observation as explicit "
            "EDT/gather work before ARTS-RT";
}

/// (C) Single-writer per distributed DB block grain. The writer is the EDT that
/// consumes the acquired pointer; two distinct EDTs writing the same block
/// within one epoch (concurrent) violates SWMR.
static void verifyCdagSwmr(ModuleOp module, bool &failed) {
  /// (alloc, epoch) -> writer regions already seen in this concurrent epoch.
  std::map<std::pair<Operation *, Operation *>, SmallVector<DbWriterRegion, 4>>
      writers;
  module.walk([&](DbAcquireOp acquire) {
    if (!isWriterAcquire(acquire))
      return;
    DbAllocOp alloc = DbUtils::getUnderlyingDbAllocOp(acquire.getSourcePtr());
    if (!alloc || !hasDistributedDbAllocation(alloc.getOperation()))
      return;
    for (Operation *user : acquire.getPtr().getUsers()) {
      auto edt = dyn_cast<EdtOp>(user);
      if (!edt)
        continue;
      auto epoch = edt->getParentOfType<EpochOp>();
      if (!epoch)
        continue;

      std::optional<DbWriterRegion> current =
          writerRegion(acquire, edt.getOperation());
      if (!current) {
        InFlightDiagnostic diag =
            edt.emitOpError()
            << "writes a distributed DB through malformed partition operands; "
               "the block grain allows a single writer";
        diag.attachNote(acquire.getLoc())
            << "writer acquire has mismatched DB-space offsets/sizes";
        failed = true;
        continue;
      }

      auto id = std::make_pair(alloc.getOperation(), epoch.getOperation());
      auto &priorWriters = writers[id];
      bool recorded = false;
      for (const DbWriterRegion &prior : priorWriters) {
        if (prior.edt == edt.getOperation()) {
          recorded = true;
          continue;
        }
        std::optional<bool> overlaps = writerRegionsOverlap(*current, prior);
        if (overlaps && !*overlaps)
          continue;
        if (!overlaps) {
          InFlightDiagnostic diag =
              edt.emitOpError()
              << "writes a distributed DB range whose block disjointness "
                 "cannot "
                 "be proven within one epoch; the block grain allows a single "
                 "writer";
          diag.attachNote(prior.acquireOp->getLoc())
              << "prior writer range in the same epoch";
          diag.attachNote(acquire.getLoc())
              << "current writer range with dynamic DB-space coordinates";
          failed = true;
          continue;
        }
        InFlightDiagnostic diag =
            edt.emitOpError()
            << "is a second concurrent writer of an overlapping distributed DB "
               "block range within one epoch; the block grain allows a single "
               "writer";
        diag.attachNote(prior.acquireOp->getLoc())
            << "first writer of an overlapping block range in this epoch";
        failed = true;
      }
      if (recorded)
        continue;
      priorWriters.push_back(std::move(*current));
    }
  });
}

/// (D) Happens-before acyclicity. An edge producer->consumer exists when a
/// consumer EDT depends on a DB value produced inside a producer EDT. A cycle
/// in this graph is a CreateEpochs ordering inconsistency.
static void verifyCdagHappensBefore(ModuleOp module, bool &failed) {
  llvm::DenseMap<Operation *, SmallVector<Operation *, 4>> succ;
  module.walk([&](EdtOp consumer) {
    for (Value dep : consumer.getDependencies()) {
      Operation *def = dep.getDefiningOp();
      if (!def)
        continue;
      auto producer = def->getParentOfType<EdtOp>();
      if (producer && producer != consumer)
        succ[producer.getOperation()].push_back(consumer.getOperation());
    }
  });

  /// DFS with white(0)/grey(1)/black(2) coloring; a grey successor is a cycle.
  llvm::DenseMap<Operation *, int> color;
  std::function<bool(Operation *)> hasCycleFrom = [&](Operation *node) -> bool {
    color[node] = 1;
    for (Operation *next : succ.lookup(node)) {
      int c = color.lookup(next);
      if (c == 1 || (c == 0 && hasCycleFrom(next)))
        return true;
    }
    color[node] = 2;
    return false;
  };
  for (auto &entry : succ) {
    if (color.lookup(entry.first) != 0)
      continue;
    if (hasCycleFrom(entry.first)) {
      entry.first->emitOpError()
          << "is part of an EDT happens-before cycle after epoch creation";
      failed = true;
      return;
    }
  }
}

struct VerifyArtsCdagPass
    : public impl::VerifyArtsCdagBase<VerifyArtsCdagPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool requiresInterNodeRouting = requiresArtsInterNodeOwnerRouting(module);
    bool failed = false;
    if (mlir::failed(EdtUtils::verifyNoMixedRootDependencies(module)))
      failed = true;
    module.walk([&](DbAllocOp alloc) {
      if (mlir::failed(verifyCdagAlloc(alloc)))
        failed = true;
      if (mlir::failed(verifyCdagAllocDirectUses(alloc)))
        failed = true;
    });
    module.walk([&](DbAcquireOp acquire) {
      if (mlir::failed(verifyCdagAcquireMode(acquire)))
        failed = true;
      if (mlir::failed(verifyCdagAcquireWindow(acquire)))
        failed = true;
      if (mlir::failed(verifyCdagCommittedAcquireUser(acquire)))
        failed = true;
      if (mlir::failed(
              verifyCdagWriterLaunch(acquire, requiresInterNodeRouting)))
        failed = true;
    });
    module.walk([&](EdtOp edt) { verifyCdagDistributedDbDeps(edt, failed); });
    verifyCdagSwmr(module, failed);
    verifyCdagHappensBefore(module, failed);
    if (failed)
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::arts::createVerifyArtsCdagPass() {
  return std::make_unique<VerifyArtsCdagPass>();
}
