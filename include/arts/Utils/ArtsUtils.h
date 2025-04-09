///==========================================================================
/// File: ArtsUtils.h
///==========================================================================

#ifndef MLIR_UTILS_ARTSUTILS_H
#define MLIR_UTILS_ARTSUTILS_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/Operation.h"
#include <optional>

namespace mlir {
namespace arts {
/// Analyzes the EDT region and checks if the value is invariant in the EDT.
bool isInvariantInEDT(arts::EdtOp op, Value value);

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
std::optional<int64_t> computeConstant(Value val);
int64_t tryParseIndexConstant(Value val);
} // namespace arts
} // namespace mlir

#endif // MLIR_UTILS_ARTSUTILS_H