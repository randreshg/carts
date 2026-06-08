///==========================================================================///
/// File: SdeCuStructure.h
///
/// Shared structural predicates over the target SDE MU/CU/SU shape.
///
/// These are the SINGLE SOURCE OF TRUTH for "what is a CU / an SU / source
/// executable work / schedule plumbing" so that the SDE CU-normalization
/// transform (`sde-cu-normalization`) and the SDE boundary verifier
/// (`verify-sde`) agree EXACTLY: the transform wraps precisely the work the
/// verifier requires inside a CU, and leaves outside a CU precisely the
/// schedule plumbing the verifier permits there. Any divergence between the two
/// would yield IR the verifier rejects (or wrapping the verifier never asked
/// for), so both consumers must read these predicates rather than re-deriving
/// them.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_UTILS_SDECUSTRUCTURE_H
#define CARTS_DIALECT_SDE_UTILS_SDECUSTRUCTURE_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Operation.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
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
         llvm::all_of(op->getResultTypes(),
                      [](Type t) { return isa<IndexType>(t); });
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

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_SDECUSTRUCTURE_H
