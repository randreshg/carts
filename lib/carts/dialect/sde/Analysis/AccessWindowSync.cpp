///==========================================================================///
/// File: AccessWindowSync.cpp
///
/// Implements the cross-barrier SU-ordering classifier shared by
/// `sde-mu-access-window-sync-opt` and `verify-sde-mu-access-window-sync`.
///==========================================================================///

#include "carts/dialect/sde/Analysis/AccessWindowSync.h"
#include "carts/utils/ArrayAttrUtils.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Block.h"
#include "llvm/ADT/DenseSet.h"

using namespace mlir;
using namespace mlir::carts;

namespace mlir::carts::sde {

namespace {

/// One raised window, decoded into block-grid coordinates.
struct WindowFact {
  Value mu;                   ///< the rank-expanded mu_alloc result
  SdeAccessMode mode;         ///< read or write (never readwrite)
  SmallVector<int64_t, 2> lo; ///< per-owner-dim block lo
  SmallVector<int64_t, 2> hi; ///< per-owner-dim block hi (half-open)
};

/// Half-open block rectangles overlap iff they overlap on every owner dim.
bool rectanglesOverlap(const WindowFact &a, const WindowFact &b) {
  for (size_t k = 0; k < a.lo.size(); ++k)
    if (a.lo[k] >= b.hi[k] || b.lo[k] >= a.hi[k])
      return false;
  return true;
}

/// True iff every block `inner` touches is also touched by `outer`.
bool rectangleWithin(const WindowFact &inner, const WindowFact &outer) {
  for (size_t k = 0; k < inner.lo.size(); ++k)
    if (inner.lo[k] < outer.lo[k] || inner.hi[k] > outer.hi[k])
      return false;
  return true;
}

/// Decode the windows directly in a CU body. Returns false (fail-closed) if a
/// window's block arrays are malformed; the op's own verifier already bounds
/// them, so this only guards against a non-array attribute.
bool collectWindows(SdeCuRegionOp cu, SmallVectorImpl<WindowFact> &out) {
  for (Operation &op : cu.getBody().front()) {
    auto win = dyn_cast<SdeMuAccessWindowOp>(op);
    if (!win)
      continue;
    std::optional<SmallVector<int64_t, 4>> lo =
        readI64ArrayAttr(win.getBlockLo());
    std::optional<SmallVector<int64_t, 4>> hi =
        readI64ArrayAttr(win.getBlockHi());
    if (!lo || !hi || lo->size() != hi->size())
      return false;
    out.push_back({win.getMu(), win.getMode(),
                   SmallVector<int64_t, 2>(lo->begin(), lo->end()),
                   SmallVector<int64_t, 2>(hi->begin(), hi->end())});
  }
  return true;
}

/// A phase is fully described by windows when every memref load/store in its
/// CUs targets an MU root that carries a window in the same CU. Only then can
/// the absence of a cross-barrier conflict prove the barrier redundant.
bool phaseFullyWindowed(ArrayRef<SdeCuRegionOp> phase) {
  for (SdeCuRegionOp cu : phase) {
    llvm::DenseSet<Value> windowed;
    for (Operation &op : cu.getBody().front())
      if (auto win = dyn_cast<SdeMuAccessWindowOp>(op))
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

} // namespace

BarrierSyncVerdict classifyBarrierSync(ArrayRef<SdeCuRegionOp> before,
                                       ArrayRef<SdeCuRegionOp> after) {
  if (before.empty() || after.empty())
    return BarrierSyncVerdict::OutOfScope; // orders nothing on one side

  SmallVector<WindowFact, 4> wBefore, wAfter;
  bool wellFormed = true;
  for (SdeCuRegionOp cu : before)
    wellFormed &= collectWindows(cu, wBefore);
  for (SdeCuRegionOp cu : after)
    wellFormed &= collectWindows(cu, wAfter);
  if (wBefore.empty() && wAfter.empty())
    return BarrierSyncVerdict::OutOfScope; // no access-window surface here
  if (!wellFormed)
    return BarrierSyncVerdict::Malformed;

  // Classify the cross-barrier relationship over shared MU roots. `b` is a
  // before-phase window, `a` an after-phase window.
  bool justified = false, misaligned = false, rankMismatch = false;
  for (const WindowFact &b : wBefore)
    for (const WindowFact &a : wAfter) {
      if (a.mu != b.mu)
        continue; // different MU -> no shared state
      if (a.lo.size() != b.lo.size()) {
        rankMismatch = true;
        continue;
      }
      bool bWrite = b.mode == SdeAccessMode::write;
      bool aWrite = a.mode == SdeAccessMode::write;
      if ((!bWrite && !aWrite) || !rectanglesOverlap(a, b))
        continue; // both read, or disjoint blocks -> no dependency
      // A flow dependency (producer before, consumer reads after) is misaligned
      // when the consumer reaches blocks the producer never wrote. Anti- (WAR)
      // and output (WAW) dependencies need only the overlap; the read there
      // sources its data elsewhere.
      if (bWrite && a.mode == SdeAccessMode::read &&
          !rectangleWithin(/*inner=*/a, /*outer=*/b))
        misaligned = true;
      else
        justified = true;
    }

  // A concrete verdict (a movement need, or a real ordering) wins over the rare
  // rank-mismatch ambiguity, so neither is masked.
  if (misaligned)
    return BarrierSyncVerdict::Misaligned;
  if (justified)
    return BarrierSyncVerdict::Justified;
  if (rankMismatch)
    return BarrierSyncVerdict::RankMismatch;
  if (phaseFullyWindowed(before) && phaseFullyWindowed(after))
    return BarrierSyncVerdict::Redundant;
  return BarrierSyncVerdict::Unprovable;
}

BarrierSyncPartition partitionBarrierPhases(Block &block) {
  BarrierSyncPartition partition;
  partition.phases.emplace_back();
  for (Operation &op : block) {
    if (auto cu = dyn_cast<SdeCuRegionOp>(&op))
      partition.phases.back().push_back(cu);
    else if (auto bar = dyn_cast<SdeSuBarrierOp>(&op)) {
      partition.barriers.push_back({bar, partition.phases.size() - 1});
      partition.phases.emplace_back();
    }
  }
  return partition;
}

} // namespace mlir::carts::sde
