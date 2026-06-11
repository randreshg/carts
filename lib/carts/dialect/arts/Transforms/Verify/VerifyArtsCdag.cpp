///==========================================================================///
/// File: VerifyArtsCdag.cpp
///
/// Fail-closed verification of the ARTS canonical-owner DAG after CreateEpochs.
/// Rejects (does not repair) IR where a distributed acquire lacks a committed
/// DB mode, a distributed DB has inconsistent owner-map/placement facts, an
/// SDE-partitioned MU was coarsened or localized, single-writer-multiple-reader
/// is violated per DB block grain, or the EDT happens-before graph has a cycle.
///==========================================================================///

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/EdtUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#define GEN_PASS_DEF_VERIFYARTSCDAG
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"

#include <functional>
#include <map>
#include <string>
#include <tuple>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

static DbAllocOp underlyingAlloc(DbAcquireOp acquire) {
  return dyn_cast_or_null<DbAllocOp>(
      DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr()));
}

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
        << "SDE must materialize block DB storage before ARTS "
           "distributed execution; CreateDbs is only a coarse raw-memref "
           "fallback";
    failed = true;
  }
}

/// DB-space block identity for an acquire. Returns "whole" for a whole-block
/// acquire, a constant coordinate tuple when all indices fold, or std::nullopt
/// when an index is dynamic (then two writers cannot be proven to collide).
static std::optional<std::string> blockKey(DbAcquireOp acquire) {
  auto indices = acquire.getIndices();
  if (indices.empty())
    return std::string("whole");
  std::string key;
  for (Value index : indices) {
    auto constant = ValueAnalysis::tryFoldConstantIndex(index);
    if (!constant)
      return std::nullopt;
    key += std::to_string(*constant);
    key += ',';
  }
  return key;
}

/// (A) owner-map/mode/placement consistency + (B) distribution preservation.
static LogicalResult verifyCdagAlloc(DbAllocOp alloc) {
  bool distributed = hasDistributedDbAllocation(alloc.getOperation());

  /// (B) An SDE-partitioned MU carries a committed physical block layout. ARTS
  /// must realize it as a distributed DB, or leave explicit evidence of an
  /// intentional non-distributed home (a derived all-gather replica, a recorded
  /// eligibility rejection, or an explicit local-only buffer). A committed plan
  /// that is silently dropped from distribution with no such evidence is a lost
  /// SDE-partitioned grain.
  bool hasNonDistributedEvidence =
      alloc.getLocalOnly().value_or(false) ||
      alloc.getPerBlockReplicated().value_or(false) ||
      alloc.getDistributedRejectReasonAttr();
  if (hasArtsDbPhysicalLayoutPlan(alloc.getOperation()) && !distributed &&
      !hasNonDistributedEvidence)
    return alloc.emitOpError()
           << "carries a committed SDE block layout but is neither realized as "
              "a distributed DB nor marked non-distributed; ARTS must not "
              "silently coarsen an SDE-partitioned MU";

  if (!distributed)
    return success();

  /// (A) A distributed DB must carry a self-consistent owner map and scattered
  /// home. Surface the not-yet-realizable kinds with their precise reason.
  DbOwnerMapPlanFailure failure = getDistributedDbOwnerMapPlanFailure(alloc);
  if (failure == DbOwnerMapPlanFailure::UnsupportedOwnerMapKind) {
    auto plan = getDbOwnerMapPlan(alloc);
    return alloc.emitOpError() << ownerMapKindUnrealizableReason(
               plan ? plan->kind : DbOwnerMapKind::owner_dim_grid);
  }
  if (failure != DbOwnerMapPlanFailure::None)
    return alloc.emitOpError()
           << "has inconsistent distributed owner-map/placement facts: "
           << toString(failure);
  return success();
}

/// (A) Every distributed acquire carries a committed DB mode; missing modes are
/// rejected here, before ARTS-RT, instead of inferred at lowering.
static LogicalResult verifyCdagAcquireMode(DbAcquireOp acquire) {
  DbAllocOp alloc = underlyingAlloc(acquire);
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
  DbAllocOp alloc = underlyingAlloc(acquire);
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

/// (C) Single-writer per distributed DB block grain. The writer is the EDT that
/// consumes the acquired pointer; two distinct EDTs writing the same block
/// within one epoch (concurrent) violates SWMR.
static void verifyCdagSwmr(ModuleOp module, bool &failed) {
  /// (alloc, epoch, blockKey) -> first writer (edt op, acquire).
  std::map<std::tuple<Operation *, Operation *, std::string>,
           std::pair<Operation *, DbAcquireOp>>
      writers;
  module.walk([&](DbAcquireOp acquire) {
    if (!isWriterAcquire(acquire))
      return;
    DbAllocOp alloc = underlyingAlloc(acquire);
    if (!alloc || !hasDistributedDbAllocation(alloc.getOperation()))
      return;
    auto key = blockKey(acquire);
    if (!key)
      return;
    for (Operation *user : acquire.getPtr().getUsers()) {
      auto edt = dyn_cast<EdtOp>(user);
      if (!edt)
        continue;
      auto epoch = edt->getParentOfType<EpochOp>();
      if (!epoch)
        continue;
      auto id =
          std::make_tuple(alloc.getOperation(), epoch.getOperation(), *key);
      auto [it, inserted] =
          writers.try_emplace(id, std::make_pair(edt.getOperation(), acquire));
      if (inserted || it->second.first == edt.getOperation())
        continue;
      InFlightDiagnostic diag =
          edt.emitOpError()
          << "is a second concurrent writer of distributed DB block " << *key
          << " within one epoch; the block grain allows a single writer";
      diag.attachNote(it->second.second.getLoc())
          << "first writer of the same block in this epoch";
      failed = true;
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
    bool failed = false;
    if (mlir::failed(EdtUtils::verifyNoMixedRootDependencies(module)))
      failed = true;
    module.walk([&](DbAllocOp alloc) {
      if (mlir::failed(verifyCdagAlloc(alloc)))
        failed = true;
    });
    module.walk([&](DbAcquireOp acquire) {
      if (mlir::failed(verifyCdagAcquireMode(acquire)))
        failed = true;
      if (mlir::failed(verifyCdagAcquireWindow(acquire)))
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
