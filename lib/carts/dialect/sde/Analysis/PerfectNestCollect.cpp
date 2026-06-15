///==========================================================================///
/// File: PerfectNestCollect.cpp
///
/// Perfect loop-nest detection for SDE scheduling-unit loops, including the
/// local-scratch side-effect tolerance (memref and libc malloc/free scratch)
/// that lets a nest with surrounding scratch bookkeeping still count as a
/// perfect nest. Backs the public analyzeSuLoopAccesses wrapper.
///==========================================================================///

#include "SuLoopAccessAnalysisDetail.h"

#include "carts/dialect/sde/Analysis/AffineIndexUtils.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/SmallPtrSet.h"

using namespace mlir;
using namespace mlir::carts;

namespace mlir::carts::sde {
namespace {

static SmallVector<Operation *> getBodyOps(Block &body) {
  SmallVector<Operation *> ops;
  for (auto &op : body) {
    if (op.hasTrait<OpTrait::IsTerminator>())
      continue;
    if (isa<SdeYieldOp>(op))
      continue;
    ops.push_back(&op);
  }
  return ops;
}

static bool isLocalScratchMemref(Value value, Block &scope) {
  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(value);
  if (!root)
    return false;

  Operation *def = root.getDefiningOp();
  return def && def->getBlock() == &scope;
}

static bool isUsedInside(Operation *ancestor, Value value) {
  if (!ancestor || !value)
    return false;
  for (OpOperand &use : value.getUses())
    if (ancestor->isAncestor(use.getOwner()))
      return true;
  return false;
}

static bool isUsedInsideAny(ArrayRef<Operation *> ancestors, Value value) {
  if (!value)
    return false;
  for (Operation *ancestor : ancestors)
    if (isUsedInside(ancestor, value))
      return true;
  return false;
}

// Polygeist can preserve C local scratch allocation as libc calls instead of
// memref.alloc.  The analyzer may ignore those effects only for exact
// allocator/free pairs whose result cannot escape the analyzed body.
static bool isLibcAllocatorCallee(StringRef callee) {
  return callee == "malloc" || callee == "calloc" || callee == "aligned_alloc";
}

static bool isLibcFreeCallee(StringRef callee) { return callee == "free"; }

static bool hasBaseMemrefType(Value value) {
  return value && isa<BaseMemRefType>(value.getType());
}

static bool isLibcFreeCallUsing(func::CallOp call, Value root) {
  return call && isLibcFreeCallee(call.getCallee()) &&
         call->getNumOperands() == 1 && call.getNumResults() == 0 &&
         ::mlir::carts::ValueAnalysis::sameMemrefRoot(call.getOperand(0), root);
}

static bool isLocalLibcAllocatorRoot(Value root, Block &scope) {
  root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(root);
  if (!hasBaseMemrefType(root))
    return false;

  auto call = root.getDefiningOp<func::CallOp>();
  return call && call->getBlock() == &scope &&
         isLibcAllocatorCallee(call.getCallee());
}

static bool hasLocalLibcFreeUse(Value root) {
  root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(root);
  for (OpOperand &use : root.getUses())
    if (auto call = dyn_cast<func::CallOp>(use.getOwner()))
      if (isLibcFreeCallUsing(call, root))
        return true;
  return false;
}

static bool opIsInsideAny(ArrayRef<Operation *> ancestors, Operation *op) {
  if (!op)
    return false;
  for (Operation *ancestor : ancestors)
    if (ancestor && ancestor->isAncestor(op))
      return true;
  return false;
}

static bool onlyUsedByRegionsOrLocalFree(Value value,
                                         ArrayRef<Operation *> regions,
                                         Block &scope, Value root,
                                         llvm::SmallPtrSetImpl<Value> &seen) {
  if (!value || !seen.insert(value).second)
    return true;

  for (OpOperand &use : value.getUses()) {
    Operation *owner = use.getOwner();
    if (!owner || owner->hasTrait<OpTrait::IsTerminator>())
      return false;

    if (auto storeOp = dyn_cast<memref::StoreOp>(owner))
      if (::mlir::carts::ValueAnalysis::sameMemrefRoot(
              storeOp.getValueToStore(), root))
        return false;

    if (auto call = dyn_cast<func::CallOp>(owner)) {
      if (!isLibcFreeCallUsing(call, root))
        return false;
      if (call->getBlock() != &scope && !opIsInsideAny(regions, owner))
        return false;
      continue;
    }

    bool hasDerivedMemrefResult = false;
    for (Value result : owner->getResults())
      hasDerivedMemrefResult |= hasBaseMemrefType(result);
    if (hasDerivedMemrefResult) {
      if (!isMemoryEffectFree(owner))
        return false;
      for (Value result : owner->getResults()) {
        if (!hasBaseMemrefType(result))
          continue;
        if (!onlyUsedByRegionsOrLocalFree(result, regions, scope, root, seen))
          return false;
      }
      continue;
    }

    if (!opIsInsideAny(regions, owner))
      return false;
  }

  return true;
}

static bool onlyUsedByRegionsOrLocalFree(Value root,
                                         ArrayRef<Operation *> regions,
                                         Block &scope) {
  root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(root);
  llvm::SmallPtrSet<Value, 8> seen;
  return onlyUsedByRegionsOrLocalFree(root, regions, scope, root, seen);
}

static bool isLocalLibcAllocatorScratch(Value root, Block &scope,
                                        ArrayRef<Operation *> regions) {
  root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(root);
  return isLocalLibcAllocatorRoot(root, scope) &&
         isUsedInsideAny(regions, root) && hasLocalLibcFreeUse(root) &&
         onlyUsedByRegionsOrLocalFree(root, regions, scope);
}

static bool isLocalScratchSideEffectUsedByLoop(Operation *op, Block &scope,
                                               scf::ForOp loop) {
  if (!op)
    return false;

  if (isa<memref::AllocOp, memref::AllocaOp>(op)) {
    for (Value result : op->getResults()) {
      if (!isLocalScratchMemref(result, scope))
        return false;
      if (!isUsedInside(loop.getOperation(), result))
        return false;
    }
    return true;
  }

  auto isLoopScratchAccess = [&](Value memref) {
    Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(memref);
    return isLocalScratchMemref(root, scope) &&
           isUsedInside(loop.getOperation(), root);
  };

  if (auto loadOp = dyn_cast<memref::LoadOp>(op))
    return isLoopScratchAccess(loadOp.getMemref());

  if (auto storeOp = dyn_cast<memref::StoreOp>(op))
    return isLoopScratchAccess(storeOp.getMemref());

  if (auto deallocOp = dyn_cast<memref::DeallocOp>(op))
    return isLoopScratchAccess(deallocOp.getMemref());

  return false;
}

static bool
isLocalScratchSideEffectUsedByRegions(Operation *op, Block &scope,
                                      ArrayRef<Operation *> regions) {
  if (!op || regions.empty())
    return false;

  if (detail::isLocalLibcAllocatorScratchCall(op, scope, regions) ||
      detail::isLocalLibcFreeScratchCall(op, scope, regions))
    return true;

  for (Operation *region : regions)
    if (auto loop = dyn_cast_or_null<scf::ForOp>(region))
      if (isLocalScratchSideEffectUsedByLoop(op, scope, loop))
        return true;
  return false;
}

static bool isRankZeroScalarMemrefAccess(Operation *op) {
  Value memref;
  if (auto loadOp = dyn_cast_or_null<memref::LoadOp>(op)) {
    if (isa<MemRefType>(loadOp.getResult().getType()))
      return false;
    memref = loadOp.getMemref();
  } else if (auto storeOp = dyn_cast_or_null<memref::StoreOp>(op)) {
    if (isa<MemRefType>(storeOp.getValueToStore().getType()))
      return false;
    memref = storeOp.getMemref();
  } else {
    return false;
  }

  auto memrefType = dyn_cast<MemRefType>(memref.getType());
  return memrefType && memrefType.getRank() == 0;
}

static bool isEffectFreeBoundaryOp(Operation *op) {
  return isMemoryEffectFree(op) || isRankZeroScalarMemrefAccess(op);
}

static bool
boundarySideEffectsAreLocalScratch(ArrayRef<Operation *> sideEffects,
                                   Block &body, ArrayRef<Operation *> regions) {
  for (Operation *op : sideEffects)
    if (!isLocalScratchSideEffectUsedByRegions(op, body, regions))
      return false;
  return true;
}

static bool collectInner(Block &body, LoopNestInfo &info) {
  SmallVector<Operation *> ops = getBodyOps(body);

  if (ops.size() == 1) {
    if (auto innerFor = dyn_cast<scf::ForOp>(ops.front())) {
      info.ivs.push_back(innerFor.getInductionVar());
      return collectInner(*innerFor.getBody(), info);
    }
    if (auto innerFor = dyn_cast<affine::AffineForOp>(ops.front())) {
      info.ivs.push_back(innerFor.getInductionVar());
      return collectInner(*innerFor.getBody(), info);
    }
  }

  if (ops.size() > 1) {
    SmallVector<scf::ForOp, 2> nestedFors;
    SmallVector<affine::AffineForOp, 2> nestedAffineFors;
    SmallVector<Operation *> sideEffectsAroundInnerLoop;
    for (Operation *op : ops) {
      if (auto nestedFor = dyn_cast<scf::ForOp>(op)) {
        nestedFors.push_back(nestedFor);
        continue;
      }
      if (auto nestedFor = dyn_cast<affine::AffineForOp>(op)) {
        nestedAffineFors.push_back(nestedFor);
        continue;
      }
      if (!isEffectFreeBoundaryOp(op))
        sideEffectsAroundInnerLoop.push_back(op);
    }

    if (nestedFors.size() == 1 && nestedAffineFors.empty()) {
      scf::ForOp innerFor = nestedFors.front();
      SmallVector<Operation *, 2> nestedForOps{innerFor.getOperation()};
      bool hasUnsupportedSideEffectsAroundInnerLoop =
          !boundarySideEffectsAreLocalScratch(sideEffectsAroundInnerLoop, body,
                                              nestedForOps);
      info.ivs.push_back(innerFor.getInductionVar());
      if (hasUnsupportedSideEffectsAroundInnerLoop) {
        info.innermostBody = &body;
        return true;
      }
      return collectInner(*innerFor.getBody(), info);
    }

    if (nestedAffineFors.size() == 1 && nestedFors.empty()) {
      affine::AffineForOp innerFor = nestedAffineFors.front();
      SmallVector<Operation *, 2> nestedForOps{innerFor.getOperation()};
      if (!boundarySideEffectsAreLocalScratch(sideEffectsAroundInnerLoop, body,
                                              nestedForOps))
        return false;
      info.ivs.push_back(innerFor.getInductionVar());
      return collectInner(*innerFor.getBody(), info);
    }

    if (nestedFors.size() > 1) {
      SmallVector<Operation *, 2> nestedForOps;
      nestedForOps.reserve(nestedFors.size());
      for (scf::ForOp nestedFor : nestedFors)
        nestedForOps.push_back(nestedFor.getOperation());
      if (!boundarySideEffectsAreLocalScratch(sideEffectsAroundInnerLoop, body,
                                              nestedForOps))
        return false;
      info.innermostBody = &body;
      return true;
    }
  }

  info.innermostBody = &body;
  return true;
}

} // namespace

namespace detail {

bool isLocalLibcAllocatorScratchCall(Operation *op, Block &scope,
                                     ArrayRef<Operation *> regions) {
  auto call = dyn_cast_or_null<func::CallOp>(op);
  if (!call || call->getBlock() != &scope ||
      !isLibcAllocatorCallee(call.getCallee()) || call.getNumResults() == 0)
    return false;

  for (Value result : call.getResults())
    if (!isLocalLibcAllocatorScratch(result, scope, regions))
      return false;
  return true;
}

bool isLocalLibcFreeScratchCall(Operation *op, Block &scope,
                                ArrayRef<Operation *> regions) {
  auto call = dyn_cast_or_null<func::CallOp>(op);
  if (!isLibcFreeCallUsing(
          call, call && call->getNumOperands() == 1
                    ? ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
                          call.getOperand(0))
                    : Value{}))
    return false;
  Value root =
      ::mlir::carts::ValueAnalysis::stripMemrefViewOps(call.getOperand(0));
  return isLocalLibcAllocatorScratch(root, scope, regions);
}

bool collectPerfectNest(SdeSuIterateOp iterOp, LoopNestInfo &info) {
  info.rootIterOp = iterOp;

  Region &region = iterOp.getBody();
  if (region.empty() || region.front().getNumArguments() == 0)
    return false;

  // SdeSuIterateOp block arguments are laid out as induction variables
  // followed by iter_args.  Only the loop-rank prefix participates in affine
  // access maps; treating carried values as IVs fabricates reduction
  // dimensions for result-bearing elementwise loops.
  unsigned numIvs = iterOp.getLowerBounds().size();
  if (region.front().getNumArguments() < numIvs)
    return false;
  for (BlockArgument arg : region.front().getArguments().take_front(numIvs))
    info.ivs.push_back(arg);
  return collectInner(*getSuIterateComputeBlock(iterOp), info);
}

} // namespace detail

} // namespace mlir::carts::sde
