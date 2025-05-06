///==========================================================================
/// File: ArtsUtils.h
///==========================================================================

#ifndef CARTS_UTILS_ARTSUTILS_H
#define CARTS_UTILS_ARTSUTILS_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/Operation.h"

namespace mlir {
namespace arts {

/// Checks if a Value is invariant within a given EDT region.
/// A value is considered invariant if it's defined outside the region
/// and not modified by any operation inside the region. Constants are
/// always considered invariant.
/// This assummes the EdtInvariantCodeMotion pass has been run.
bool isInvariantInEdt(Region &edtRegion, Value value);

/// Returns true if `target` is reachable from `source` in the EDT CFG.
bool isReachable(Operation *source, Operation *target);

/// Remove a set of operations from the module.
void removeOps(mlir::ModuleOp module, OpBuilder &builder,
               llvm::SetVector<mlir::Operation *> &opsToRemove);
void recursivelyRemoveOp(mlir::Operation *op);
void removeUndefOps(mlir::ModuleOp module);

/// Replace the operation with an undef operation.
void replaceWithUndef(mlir::Operation *op, OpBuilder &builder);
void replaceUses(mlir::Value from, mlir::Value to, DominanceInfo &domInfo,
                 Operation *dominatingOp);

/// Replace all uses of `from` with `to` in the module.
void replaceUses(llvm::DenseMap<mlir::Value, mlir::Value> &rewireMap);
void replaceInRegion(mlir::Region &region, mlir::Value from, mlir::Value to);
void replaceInRegion(mlir::Region &region,
                     llvm::DenseMap<mlir::Value, mlir::Value> &rewireMap,
                     bool clear = true);

/// Returns true if `val` is a constant value.
bool isValueConstant(mlir::Value val);
} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_ARTSUTILS_H