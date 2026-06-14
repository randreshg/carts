///==========================================================================///
/// File: AccessWindowSync.cpp
///
/// Implements the cross-barrier SU-ordering classifier shared by
/// `sde-mu-access-window-sync-opt` and `verify-sde-mu-access-window-sync`.
///==========================================================================///

#include "carts/dialect/sde/Analysis/AccessWindowSync.h"
#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/Affine/IR/AffineMemoryOpInterfaces.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Block.h"
#include "llvm/ADT/DenseSet.h"

using namespace mlir;
using namespace mlir::carts;

namespace mlir::carts::sde {

namespace {

/// One access window, decoded into block-grid coordinates.
struct WindowFact {
  Value mu;                   ///< the rank-expanded mu_alloc result
  SdeAccessMode mode;         ///< read, write, or readwrite
  SmallVector<int64_t, 2> lo; ///< per-owner-dim block lo
  SmallVector<int64_t, 2> hi; ///< per-owner-dim block hi (half-open)
};

bool hasReadEffect(SdeAccessMode mode) {
  return mode == SdeAccessMode::read || mode == SdeAccessMode::readwrite;
}

bool hasWriteEffect(SdeAccessMode mode) {
  return mode == SdeAccessMode::write || mode == SdeAccessMode::readwrite;
}

bool rectanglesOverlap(const WindowFact &a, const WindowFact &b) {
  for (size_t k = 0; k < a.lo.size(); ++k)
    if (a.lo[k] >= b.hi[k] || b.lo[k] >= a.hi[k])
      return false;
  return true;
}

bool rectangleWithin(const WindowFact &inner, const WindowFact &outer) {
  for (size_t k = 0; k < inner.lo.size(); ++k)
    if (inner.lo[k] < outer.lo[k] || inner.hi[k] > outer.hi[k])
      return false;
  return true;
}

static bool isDirectMuMemoryAccess(Operation *op) {
  return isa<memref::LoadOp, memref::StoreOp>(op) ||
         isa<affine::AffineReadOpInterface, affine::AffineWriteOpInterface>(op);
}

static SdeAccessMode accessModeForMemoryOp(Operation *op) {
  if (isa<memref::LoadOp, affine::AffineReadOpInterface>(op))
    return SdeAccessMode::read;
  return SdeAccessMode::write;
}

static Value memrefForMemoryOp(Operation *op) {
  if (auto load = dyn_cast<memref::LoadOp>(op))
    return load.getMemRef();
  if (auto store = dyn_cast<memref::StoreOp>(op))
    return store.getMemRef();
  if (auto read = dyn_cast<affine::AffineReadOpInterface>(op))
    return read.getMemRef();
  return cast<affine::AffineWriteOpInterface>(op).getMemRef();
}

static bool appendWindowFact(Value mu, SdeAccessMode mode,
                             SmallVectorImpl<WindowFact> &out) {
  std::optional<sde::MuAccessWindowGeometry> geom =
      deriveMuAccessWindowGeometry(mu);
  if (!geom || geom->blockLo.size() != geom->blockHi.size())
    return false;
  out.push_back({mu, mode,
                 SmallVector<int64_t, 2>(geom->blockLo.begin(),
                                         geom->blockLo.end()),
                 SmallVector<int64_t, 2>(geom->blockHi.begin(),
                                         geom->blockHi.end())});
  return true;
}

bool collectWindows(SdeCuRegionOp cu, SmallVectorImpl<WindowFact> &out) {
  llvm::DenseSet<std::pair<Value, SdeAccessMode>> seen;
  bool ok = true;
  cu.getBody().walk([&](Operation *op) {
    if (!ok || !isDirectMuMemoryAccess(op))
      return;
    Value root = ValueAnalysis::stripMemrefViewOps(memrefForMemoryOp(op));
    auto mu = root.getDefiningOp<SdeMuAllocOp>();
    if (!mu)
      return;
    SdeAccessMode mode = accessModeForMemoryOp(op);
    bool matchedQuery = false;
    for (const RaisedWindowSpec &spec : queryAccessWindows(mu)) {
      if (spec.cu != cu)
        continue;
      matchedQuery = true;
      if (!seen.insert({spec.mu, spec.mode}).second)
        continue;
      if (!appendWindowFact(spec.mu, spec.mode, out))
        ok = false;
    }
    if (!matchedQuery) {
      if (!seen.insert({root, mode}).second)
        return;
      if (!appendWindowFact(root, mode, out))
        ok = false;
    }
  });
  return ok;
}

static bool accessCoveredByQuery(SdeCuRegionOp cu, SdeMuAllocOp mu,
                                 SdeAccessMode mode) {
  for (const RaisedWindowSpec &spec : queryAccessWindows(mu)) {
    if (spec.cu != cu)
      continue;
    if (spec.mode == mode)
      return true;
    if (spec.mode == SdeAccessMode::readwrite &&
        (mode == SdeAccessMode::read || mode == SdeAccessMode::write))
      return true;
  }
  return false;
}

bool phaseFullyWindowed(ArrayRef<SdeCuRegionOp> phase) {
  for (SdeCuRegionOp cu : phase) {
    bool ok = true;
    cu.getBody().walk([&](Operation *op) {
      if (!isDirectMuMemoryAccess(op))
        return;
      Value root = ValueAnalysis::stripMemrefViewOps(memrefForMemoryOp(op));
      auto mu = root.getDefiningOp<SdeMuAllocOp>();
      if (!mu)
        return;
      SdeAccessMode mode = accessModeForMemoryOp(op);
      if (accessCoveredByQuery(cu, mu, mode))
        return;
      if (deriveMuAccessWindowGeometry(root).has_value())
        return;
      ok = false;
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
    return BarrierSyncVerdict::OutOfScope;

  SmallVector<WindowFact, 4> wBefore, wAfter;
  bool wellFormed = true;
  for (SdeCuRegionOp cu : before)
    wellFormed &= collectWindows(cu, wBefore);
  for (SdeCuRegionOp cu : after)
    wellFormed &= collectWindows(cu, wAfter);
  if (wBefore.empty() && wAfter.empty())
    return BarrierSyncVerdict::OutOfScope;
  if (!wellFormed)
    return BarrierSyncVerdict::Malformed;

  bool justified = false, misaligned = false, rankMismatch = false;
  for (const WindowFact &b : wBefore)
    for (const WindowFact &a : wAfter) {
      if (a.mu != b.mu)
        continue;
      if (a.lo.size() != b.lo.size()) {
        rankMismatch = true;
        continue;
      }
      bool bWrite = hasWriteEffect(b.mode);
      if ((!bWrite && !hasWriteEffect(a.mode)) || !rectanglesOverlap(a, b))
        continue;
      if (bWrite && hasReadEffect(a.mode) &&
          !rectangleWithin(/*inner=*/a, /*outer=*/b))
        misaligned = true;
      else
        justified = true;
    }

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

static void collectLeafCuRegions(Operation *op,
                                 SmallVectorImpl<SdeCuRegionOp> &phase) {
  if (auto cu = dyn_cast<SdeCuRegionOp>(op)) {
    phase.push_back(cu);
    return;
  }
  if (auto iterate = dyn_cast<SdeSuIterateOp>(op)) {
    if (iterate.getBody().empty())
      return;
    for (Operation &child : iterate.getBody().front())
      if (auto cu = dyn_cast<SdeCuRegionOp>(&child))
        phase.push_back(cu);
    return;
  }
  if (auto dist = dyn_cast<SdeSuDistributeOp>(op)) {
    if (dist.getBody().empty())
      return;
    for (Operation &child : dist.getBody().front())
      collectLeafCuRegions(&child, phase);
  }
}

BarrierSyncPartition partitionBarrierPhases(Block &block) {
  BarrierSyncPartition partition;
  partition.phases.emplace_back();
  for (Operation &op : block) {
    if (auto bar = dyn_cast<SdeSuBarrierOp>(&op)) {
      partition.barriers.push_back({bar, partition.phases.size() - 1});
      partition.phases.emplace_back();
      continue;
    }
    collectLeafCuRegions(&op, partition.phases.back());
  }
  return partition;
}

} // namespace mlir::carts::sde
