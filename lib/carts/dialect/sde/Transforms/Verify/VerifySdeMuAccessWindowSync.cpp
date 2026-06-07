///==========================================================================///
/// File: VerifySdeMuAccessWindowSync.cpp
///
/// Verifies that the surviving SU ordering between compute units is justified by
/// their raised MU access windows. A `sde.su_barrier` is the last-resort global
/// ordering; with canonical per-CU windows present, whether a barrier is needed
/// is a structural fact, not a guess. For each barrier this pass reads the
/// windows of the immediately-before and immediately-after CU phases and
/// classifies the cross-barrier relationship on every shared MU root (half-open
/// block ranges, `[blockLo, blockHi)` per owner dim):
///
///   JUSTIFIED  — a shared MU is written on one side and accessed on the other
///                with overlapping blocks, and any read side stays within the
///                written blocks (aligned RAW/WAW/WAR). The barrier orders a real
///                dependency: accept.
///   REDUNDANT  — every shared MU is touched on disjoint blocks or read-only on
///                both sides (and the phases are fully described by windows), so
///                the ordered CUs are independent: the barrier is avoidable.
///   MISALIGNED — a consumer reads blocks of a shared MU that the producer does
///                not write (overlap, but the read is not within the write). The
///                data must move; that redistribution is owned by a later SDE
///                distribution transform, not by this ordering. Diagnose.
///
/// Fail-closed (windows present but the ordering cannot be proven): a shared MU
/// carries inconsistent owner-dim rank, or — when concluding REDUNDANT — an
/// adjacent CU has a memory access no window describes. Out of scope (no windows
/// on either side of the barrier) is skipped, never rejected; window coverage
/// itself is `verify-sde-mu-access-window`'s job. This pass mutates no IR and
/// introduces no token, slice, dependency-graph, or distribution structure.
///
/// Distinct from effect-based barrier elimination: that decides removal from
/// structured memory effects and rewrites/stamps the barrier; this proves
/// justification from raised block-grid window facts and changes nothing. The
/// dependency is at block grain (`[blockLo, blockHi)`); the in-tile
/// `validExtents` describes coverage within a block and is not part of the
/// cross-CU block relationship.
///==========================================================================///

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/utils/ArrayAttrUtils.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYSDEMUACCESSWINDOWSYNC
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

/// One raised window, decoded into block-grid coordinates.
struct WindowFact {
  Value mu;                      ///< the rank-expanded mu_alloc result
  sde::SdeAccessMode mode;       ///< read or write (never readwrite)
  SmallVector<int64_t, 2> lo;    ///< per-owner-dim block lo
  SmallVector<int64_t, 2> hi;    ///< per-owner-dim block hi (half-open)
};

/// Half-open block rectangles overlap iff they overlap on every owner dim.
static bool rectanglesOverlap(const WindowFact &a, const WindowFact &b) {
  for (size_t k = 0; k < a.lo.size(); ++k)
    if (a.lo[k] >= b.hi[k] || b.lo[k] >= a.hi[k])
      return false;
  return true;
}

/// True iff every block `inner` touches is also touched by `outer`.
static bool rectangleWithin(const WindowFact &inner, const WindowFact &outer) {
  for (size_t k = 0; k < inner.lo.size(); ++k)
    if (inner.lo[k] < outer.lo[k] || inner.hi[k] > outer.hi[k])
      return false;
  return true;
}

/// Decode the windows directly in a CU body. Returns false (fail-closed) if a
/// window's block arrays are malformed; the op's own verifier already bounds
/// them, so this only guards against a non-array attribute.
static bool collectWindows(sde::SdeCuRegionOp cu,
                           SmallVectorImpl<WindowFact> &out) {
  for (Operation &op : cu.getBody().front()) {
    auto win = dyn_cast<sde::SdeMuAccessWindowOp>(op);
    if (!win)
      continue;
    std::optional<SmallVector<int64_t, 4>> lo = readI64ArrayAttr(win.getBlockLo());
    std::optional<SmallVector<int64_t, 4>> hi = readI64ArrayAttr(win.getBlockHi());
    if (!lo || !hi || lo->size() != hi->size())
      return false;
    out.push_back({win.getMu(), win.getMode(),
                   SmallVector<int64_t, 2>(lo->begin(), lo->end()),
                   SmallVector<int64_t, 2>(hi->begin(), hi->end())});
  }
  return true;
}

/// A phase is fully described by windows when every memref load/store in its CUs
/// targets an MU root that carries a window in the same CU. Only then can the
/// absence of a cross-barrier conflict prove the barrier redundant.
static bool phaseFullyWindowed(ArrayRef<sde::SdeCuRegionOp> phase) {
  for (sde::SdeCuRegionOp cu : phase) {
    llvm::DenseSet<Value> windowed;
    for (Operation &op : cu.getBody().front())
      if (auto win = dyn_cast<sde::SdeMuAccessWindowOp>(op))
        windowed.insert(win.getMu());
    bool ok = true;
    cu.getBody().walk([&](Operation *op) {
      Value memref;
      if (auto load = dyn_cast<memref::LoadOp>(op))
        memref = load.getMemRef();
      else if (auto store = dyn_cast<memref::StoreOp>(op))
        memref = store.getMemRef();
      else
        return;
      if (!windowed.contains(memref))
        ok = false; // an access no window describes
    });
    if (!ok)
      return false;
  }
  return true;
}

struct VerifySdeMuAccessWindowSyncPass
    : public sde::impl::VerifySdeMuAccessWindowSyncBase<
          VerifySdeMuAccessWindowSyncPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool failed = false;

    module.walk([&](Block *block) {
      // Split the block into CU phases separated by barriers, in program order.
      SmallVector<SmallVector<sde::SdeCuRegionOp, 4>, 4> phases(1);
      SmallVector<std::pair<sde::SdeSuBarrierOp, unsigned>, 4> barriers;
      for (Operation &op : *block) {
        if (auto cu = dyn_cast<sde::SdeCuRegionOp>(&op))
          phases.back().push_back(cu);
        else if (auto bar = dyn_cast<sde::SdeSuBarrierOp>(&op)) {
          barriers.push_back({bar, phases.size() - 1});
          phases.emplace_back();
        }
      }

      for (auto [barrier, beforeIdx] : barriers) {
        ArrayRef<sde::SdeCuRegionOp> before = phases[beforeIdx];
        ArrayRef<sde::SdeCuRegionOp> after = phases[beforeIdx + 1];
        if (before.empty() || after.empty())
          continue; // orders nothing on one side -> not a window dependency

        SmallVector<WindowFact, 4> wBefore, wAfter;
        bool wellFormed = true;
        for (sde::SdeCuRegionOp cu : before)
          wellFormed &= collectWindows(cu, wBefore);
        for (sde::SdeCuRegionOp cu : after)
          wellFormed &= collectWindows(cu, wAfter);
        if (wBefore.empty() && wAfter.empty())
          continue; // no access-window surface here -> out of scope, skip
        if (!wellFormed) {
          barrier.emitOpError(
              "a malformed access window prevents proving the ordering");
          failed = true;
          continue;
        }

        // Classify the cross-barrier relationship over shared MU roots. `b` is
        // a before-phase window, `a` an after-phase window.
        bool justified = false, misaligned = false, rankMismatch = false;
        for (const WindowFact &b : wBefore)
          for (const WindowFact &a : wAfter) {
            if (a.mu != b.mu)
              continue; // different MU -> no shared state
            if (a.lo.size() != b.lo.size()) {
              rankMismatch = true;
              continue;
            }
            bool bWrite = b.mode == sde::SdeAccessMode::write;
            bool aWrite = a.mode == sde::SdeAccessMode::write;
            if ((!bWrite && !aWrite) || !rectanglesOverlap(a, b))
              continue; // both read, or disjoint blocks -> no dependency
            // A flow dependency (producer before, consumer reads after) is
            // misaligned when the consumer reaches blocks the producer never
            // wrote. Anti- (WAR) and output (WAW) dependencies need only the
            // overlap; the read there sources its data elsewhere.
            if (bWrite && a.mode == sde::SdeAccessMode::read &&
                !rectangleWithin(/*inner=*/a, /*outer=*/b))
              misaligned = true;
            else
              justified = true;
          }

        // A concrete verdict (a movement need, or a real ordering) wins over the
        // rare rank-mismatch ambiguity, so neither is masked.
        if (misaligned) {
          barrier.emitOpError(
              "a compute unit reads blocks of a shared memory unit that the "
              "ordered producer does not write; this needs a redistribution, "
              "not an ordering");
          failed = true;
        } else if (justified) {
          // A real producer/consumer overlap: the barrier orders it. Accept.
        } else if (rankMismatch) {
          barrier.emitOpError(
              "access windows on a shared memory unit disagree on owner-dim "
              "rank; the ordering cannot be proven");
          failed = true;
        } else if (phaseFullyWindowed(before) && phaseFullyWindowed(after)) {
          barrier.emitOpError(
              "the access windows of the ordered compute units prove they touch "
              "disjoint blocks (or only read shared blocks), so this ordering is "
              "redundant");
          failed = true;
        } else {
          barrier.emitOpError(
              "an ordered compute unit has a memory access no window describes; "
              "the ordering cannot be proven");
          failed = true;
        }
      }
    });

    if (failed)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createVerifySdeMuAccessWindowSyncPass() {
  return std::make_unique<VerifySdeMuAccessWindowSyncPass>();
}
} // namespace mlir::carts::sde
