///==========================================================================///
/// File: SdeCuStructure.h
///
/// Shared structural predicates over the target SDE MU/CU/SU shape.
///
/// These are the shared predicates for "what is a CU / an SU / source
/// executable work / schedule plumbing" used by SDE normalization, analysis,
/// and verification. `verify-sde` applies one stricter boundary rule on top:
/// an `sde.su_iterate` body directly contains direct-boundary CUs,
/// `sde.array_layout_root` provenance, barriers, and its terminator only; an
/// `sde.su_distribute` body directly contains nested SUs, `sde.su_halo`,
/// `sde.su_reduce_scatter`, or
/// barriers only. Scalar/index plumbing that belongs to scheduled work must be
/// inside a CU even when it is memory-effect-free.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_UTILS_SDECUSTRUCTURE_H
#define CARTS_DIALECT_SDE_UTILS_SDECUSTRUCTURE_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Operation.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"

namespace mlir::carts::sde {

/// True for ops owned by the SDE dialect (MU/CU/SU structural ops, yields).
inline bool isSdeDialectOp(Operation *op) {
  return op->getDialect() ==
         op->getContext()->getLoadedDialect<CartsSdeDialect>();
}

/// CU containers. A CU is the only legal home for source executable work.
/// `sde.cu_task` is a CU container.
inline bool isCuOp(Operation *op) {
  return isa<SdeCuWorkOp, SdeCuRegionOp, SdeCuTaskOp, SdeCuReduceOp,
             SdeCuAtomicOp>(op);
}

/// SU scheduling containers. Raw compute may not be a direct child of one.
inline bool isSuOp(Operation *op) {
  return isa<SdeSuIterateOp, SdeSuDistributeOp>(op);
}

/// Scheduling operations that are forbidden anywhere inside a leaf CU.
inline bool isCuForbiddenSchedulingOp(Operation *op) {
  return isSuOp(op) ||
         isa<SdeSuBarrierOp, SdeSuHaloOp, SdeSuReduceScatterOp>(op);
}

/// True when `op` contains nested SU scheduling that must stay outside any CU.
inline bool containsCuForbiddenSchedulingOp(Operation *op) {
  bool found = false;
  op->walk([&](Operation *nested) {
    if (nested == op)
      return WalkResult::advance();
    if (isCuForbiddenSchedulingOp(nested)) {
      found = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return found;
}

/// A direct CU body boundary: either a forbidden scheduling op itself or an
/// SCF/source carrier containing forbidden scheduling in a nested region.
inline bool isCuSchedulingBoundary(Operation *op) {
  return isCuForbiddenSchedulingOp(op) || containsCuForbiddenSchedulingOp(op);
}

inline bool isSchedulePlumbing(Operation *op);

/// True for SCF control flow that carries only SDE scheduling structure and
/// index arithmetic. This lets SDE represent wavefront/frontier schedule loops
/// without hiding source compute in a CU.
inline bool isScheduleControlOp(Operation *op) {
  if (!isa<scf::ForOp, scf::IfOp>(op))
    return false;

  bool ok = true;
  for (Region &region : op->getRegions()) {
    for (Block &block : region) {
      for (Operation &nested : block) {
        if (nested.hasTrait<OpTrait::IsTerminator>())
          continue;
        if (isSdeDialectOp(&nested))
          continue;
        if (isSchedulePlumbing(&nested))
          continue;
        ok = false;
        return ok;
      }
    }
  }
  return ok;
}

/// Schedule/index/window plumbing: constants, loop-bound arithmetic,
/// dimensions, and view/type carriers. These are legal outside CUs.
inline bool isSchedulePlumbing(Operation *op) {
  if (op->hasTrait<OpTrait::ConstantLike>())
    return true;
  if (isa<memref::CastOp>(op))
    return true;
  if (isScheduleControlOp(op))
    return true;
  return isMemoryEffectFree(op) && op->getNumRegions() == 0 &&
         llvm::all_of(op->getResultTypes(), [](Type t) {
           if (isa<IndexType>(t))
             return true;
           auto intType = dyn_cast<IntegerType>(t);
           return intType && intType.getWidth() == 1;
         });
}

/// True for an op that performs source executable work — program compute,
/// memory effects, control-flow loops/guards, or calls. Such an op must live
/// inside a CU. SDE structural ops, region terminators, the func/module
/// boundary, and schedule/index plumbing are not source compute.
inline bool isSourceComputeOp(Operation *op) {
  if (isSdeDialectOp(op))
    return false;
  if (op->hasTrait<OpTrait::IsTerminator>())
    return false;
  if (isa<func::FuncOp, ModuleOp>(op))
    return false;
  if (isSchedulePlumbing(op))
    return false;
  return true;
}

/// The direct child of `block` that contains `user`, or null if `user` is not
/// nested under an operation in `block`.
inline Operation *getBlockLevelAncestor(Operation *user, Block *block) {
  Operation *cursor = user;
  while (cursor && cursor->getBlock() != block)
    cursor = cursor->getParentOp();
  return cursor;
}

inline bool isNestedUnder(Operation *op, Operation *container) {
  for (Operation *cursor = op; cursor; cursor = cursor->getParentOp())
    if (cursor == container)
      return true;
  return false;
}

inline bool opUseEscapesSpan(OpOperand &use, Block *block,
                             const llvm::DenseSet<Operation *> &spanSet) {
  Operation *ancestor = getBlockLevelAncestor(use.getOwner(), block);
  return !ancestor || !spanSet.contains(ancestor);
}

/// Clone constant-like values from a soon-to-be-wrapped source span for users
/// outside the span. These constants are schedule/index plumbing, so threading
/// them through a CU result would manufacture dynamic-looking schedule bounds.
inline void materializeEscapingConstantLikeValues(ArrayRef<Operation *> span,
                                                  Block *block,
                                                  OpBuilder &builder) {
  llvm::DenseSet<Operation *> spanSet(span.begin(), span.end());
  for (Operation *op : span) {
    if (!op->hasTrait<OpTrait::ConstantLike>() || op->getNumOperands() != 0 ||
        op->getNumResults() == 0)
      continue;

    Operation *clone = nullptr;
    for (auto [idx, result] : llvm::enumerate(op->getResults())) {
      bool hasExternalUse = llvm::any_of(result.getUses(), [&](OpOperand &use) {
        return opUseEscapesSpan(use, block, spanSet);
      });
      if (!hasExternalUse)
        continue;

      if (!clone)
        clone = builder.clone(*op);
      result.replaceUsesWithIf(clone->getResult(idx), [&](OpOperand &use) {
        return opUseEscapesSpan(use, block, spanSet);
      });
    }
  }
}

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_SDECUSTRUCTURE_H
