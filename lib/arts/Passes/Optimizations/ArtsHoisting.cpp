///==========================================================================///
/// File: ArtsHoisting.cpp
///
/// Unified hoisting pass for ARTS operations that performs:
/// 1. Epoch acquire hoisting - hoists read-only db_acquire out of worker loops
/// 2. DbRef hoisting - hoists loop-invariant db_ref out of inner loops in EDTs
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Interfaces/LoopLikeInterface.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/SmallVector.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(arts_hoisting);

using namespace mlir;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// Shared Utilities
//===----------------------------------------------------------------------===//

static bool isLoopInvariant(scf::ForOp loop, Value v) {
  return loop.isDefinedOutsideOfLoop(v);
}

static bool valuesInvariant(scf::ForOp loop, ValueRange values) {
  for (Value v : values) {
    if (!v)
      continue;
    if (!isLoopInvariant(loop, v))
      return false;
  }
  return true;
}

static bool isWorkerLoop(scf::ForOp loop) {
  bool hasEdt = false;
  loop.walk([&](EdtOp) { hasEdt = true; });
  return hasEdt;
}

//===----------------------------------------------------------------------===//
// Epoch Acquire Hoisting
//===----------------------------------------------------------------------===//
///
/// Hoists loop-invariant read-only db_acquire ops out of worker loops in
/// epochs. Safety: hoist only when ALL runtime operands and partition hints are
/// loop invariant. This pass does not drop or rewrite partition hints.
///
/// BEFORE:
///   arts.epoch {
///     scf.for %w = 0 to %workers {
///       %acq = arts.db_acquire [in] %db    // acquired every iteration
///       arts.edt(%acq) { ... }
///       arts.db_release %acq               // released every iteration
///     }
///   }
///
/// AFTER:
///   arts.epoch {
///     %acq = arts.db_acquire [in] %db      // hoisted: acquired once
///     scf.for %w = 0 to %workers {
///       arts.edt(%acq) { ... }
///     }
///     arts.db_release %acq                 // single release after loop
///   }
///
//===----------------------------------------------------------------------===//

/// Checks if all runtime operands (guid, ptr, indices, offsets, sizes) of a
/// db_acquire are loop invariant.
static bool runtimeOperandsInvariant(scf::ForOp loop, DbAcquireOp acq) {
  if (!isLoopInvariant(loop, acq.getGuid()) ||
      !isLoopInvariant(loop, acq.getPtr()))
    return false;
  if (!valuesInvariant(loop, acq.getIndices()))
    return false;
  if (!valuesInvariant(loop, acq.getOffsets()))
    return false;
  if (!valuesInvariant(loop, acq.getSizes()))
    return false;
  return true;
}

/// Checks if all partition hints (partition indices, offsets, sizes) of a
/// db_acquire are loop invariant.
static bool partitionHintsInvariant(scf::ForOp loop, DbAcquireOp acq) {
  if (!valuesInvariant(loop, acq.getPartitionIndices()))
    return false;
  if (!valuesInvariant(loop, acq.getPartitionOffsets()))
    return false;
  if (!valuesInvariant(loop, acq.getPartitionSizes()))
    return false;
  return true;
}

/// Checks if there is a db_release outside the loop for the given acquire.
/// Collects all in-loop releases in the output vector.
static bool hasReleaseOutsideLoop(scf::ForOp loop, DbAcquireOp acq,
                                  SmallVectorImpl<DbReleaseOp> &releases) {
  bool outside = false;
  for (OpOperand &use : acq.getPtr().getUses()) {
    if (auto rel = dyn_cast<DbReleaseOp>(use.getOwner())) {
      if (!loop->isAncestor(rel)) {
        outside = true;
        continue;
      }
      if (rel->getParentOfType<EdtOp>())
        continue; /// release inside EDT body is unrelated
      releases.push_back(rel);
    }
  }
  return outside;
}

/// Validates that all uses of the acquire result are safe for hoisting.
/// Safe uses: releases inside loop, EDTs inside loop, casts inside loop.
static bool usesAreSafe(scf::ForOp loop, DbAcquireOp acq) {
  auto checkUses = [&](Value v) -> bool {
    for (OpOperand &use : v.getUses()) {
      Operation *owner = use.getOwner();
      if (auto rel = dyn_cast<DbReleaseOp>(owner)) {
        /// Allow releases only inside loop (handled separately).
        if (!loop->isAncestor(rel))
          return false;
        if (rel->getParentOfType<EdtOp>())
          continue;
        continue;
      }
      if (auto edt = dyn_cast<EdtOp>(owner)) {
        if (!loop->isAncestor(edt))
          return false;
        continue;
      }
      /// Allow casts inside the loop (dependency plumbing).
      if (isa<memref::CastOp, memref::ReinterpretCastOp,
              polygeist::Memref2PointerOp>(owner)) {
        if (!loop->isAncestor(owner))
          return false;
        continue;
      }
      /// Other uses inside the loop are conservatively rejected.
      if (loop->isAncestor(owner))
        return false;
      return false;
    }
    return true;
  };

  return checkUses(acq.getPtr()) && checkUses(acq.getGuid());
}

/// Hoists read-only db_acquire operations out of a single worker loop.
/// Returns true if any hoisting was performed.
static bool hoistAcquiresInWorkerLoop(scf::ForOp loop) {
  bool changed = false;
  SmallVector<DbAcquireOp> candidates;

  loop.walk([&](Operation *op) -> WalkResult {
    if (isa<EdtOp>(op))
      return WalkResult::skip();
    if (auto acq = dyn_cast<DbAcquireOp>(op))
      candidates.push_back(acq);
    return WalkResult::advance();
  });

  for (DbAcquireOp acq : candidates) {
    if (acq.getMode() != ArtsMode::in)
      continue;
    if (acq->getParentOfType<EdtOp>())
      continue;
    if (!runtimeOperandsInvariant(loop, acq))
      continue;
    if (!partitionHintsInvariant(loop, acq))
      continue;
    if (!usesAreSafe(loop, acq))
      continue;

    SmallVector<DbReleaseOp> releases;
    if (hasReleaseOutsideLoop(loop, acq, releases))
      continue;

    ARTS_DEBUG("Hoisting read-only db_acquire out of worker loop");
    acq->moveBefore(loop);
    changed = true;

    /// If the acquire had per-iteration releases in the loop body, replace
    /// them with a single release after the loop to avoid double-release.
    if (!releases.empty()) {
      for (DbReleaseOp rel : releases)
        rel.erase();

      OpBuilder builder(loop);
      builder.setInsertionPointAfter(loop);
      builder.create<DbReleaseOp>(loop.getLoc(), acq.getPtr());
    }
  }

  return changed;
}

/// Processes an epoch to hoist acquires from all worker loops within it.
/// Returns true if any hoisting was performed.
static bool hoistAcquiresInEpoch(EpochOp epoch) {
  bool changed = false;
  Block &body = epoch.getBody().front();
  for (Operation &op : body) {
    auto loop = dyn_cast<scf::ForOp>(op);
    if (!loop)
      continue;
    if (!isWorkerLoop(loop))
      continue;
    changed |= hoistAcquiresInWorkerLoop(loop);
  }
  return changed;
}

//===----------------------------------------------------------------------===//
// DbRef Hoisting
//===----------------------------------------------------------------------===//
///
/// Hoists loop-invariant db_ref ops out of inner loops, reducing repeated
/// db_ref creation inside tight loops.
///
/// BEFORE:
///   scf.for %i = 0 to %N {
///     scf.for %j = 0 to %M {
///       %ref = arts.db_ref %arg0[%i]       // created every inner iteration
///       use %ref
///     }
///   }
///
/// AFTER:
///   scf.for %i = 0 to %N {
///     %ref = arts.db_ref %arg0[%i]         // hoisted to outer loop
///     scf.for %j = 0 to %M {
///       use %ref
///     }
///   }
///
//===----------------------------------------------------------------------===//

/// Checks if a db_ref operation is loop invariant (source and all indices
/// are defined outside the loop).
static bool refInvariantInLoop(scf::ForOp loop, DbRefOp ref) {
  if (!isLoopInvariant(loop, ref.getSource()))
    return false;
  for (Value idx : ref.getIndices()) {
    if (!isLoopInvariant(loop, idx))
      return false;
  }
  return true;
}

/// Hoists loop-invariant db_ref operations out of a single loop.
/// Returns true if any hoisting was performed.
static bool hoistDbRefsInLoop(scf::ForOp loop) {
  bool changed = false;
  SmallVector<DbRefOp> refs;

  loop.walk([&](Operation *op) -> WalkResult {
    if (op != loop.getOperation() && isa<scf::ForOp>(op))
      return WalkResult::skip();
    if (auto ref = dyn_cast<DbRefOp>(op))
      refs.push_back(ref);
    return WalkResult::advance();
  });

  for (DbRefOp ref : refs) {
    if (!refInvariantInLoop(loop, ref))
      continue;
    if (!loop->isAncestor(ref))
      continue;

    ARTS_DEBUG("Hoisting loop-invariant db_ref");
    ref->moveBefore(loop);
    changed = true;
  }
  return changed;
}

/// Processes an EDT to hoist db_refs from all loops within it.
/// Returns true if any hoisting was performed.
static bool hoistDbRefsInEdt(EdtOp edt) {
  bool changed = false;
  edt.walk([&](scf::ForOp loop) { changed |= hoistDbRefsInLoop(loop); });
  return changed;
}

//===----------------------------------------------------------------------===//
// Pass Definition
//===----------------------------------------------------------------------===//

namespace {
struct ArtsHoistingPass : public arts::ArtsHoistingBase<ArtsHoistingPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool changed = false;

    if (enableAcquireHoisting) {
      module.walk(
          [&](EpochOp epoch) { changed |= hoistAcquiresInEpoch(epoch); });
      if (changed)
        ARTS_INFO("Epoch acquire hoisting applied");
    }

    if (enableRefHoisting) {
      bool refChanged = false;
      module.walk([&](EdtOp edt) { refChanged |= hoistDbRefsInEdt(edt); });
      if (refChanged) {
        ARTS_INFO("DbRef hoisting applied");
        changed = true;
      }
    }
  }
};
} // namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createArtsHoistingPass() {
  return std::make_unique<ArtsHoistingPass>();
}
} // namespace arts
} // namespace mlir
