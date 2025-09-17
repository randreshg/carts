///==========================================================================
/// File: ArtsUtils.h
///==========================================================================

#ifndef CARTS_UTILS_ARTSUTILS_H
#define CARTS_UTILS_ARTSUTILS_H

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"

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

/// Try to extract an index constant from a Value.
/// Returns true and sets `out` if the Value is a constant index (via
/// arith.constant or compatible); otherwise returns false.
bool getConstantIndex(mlir::Value v, int64_t &out);

/// Returns true if `v` is an index constant not equal to zero,
/// or a non-constant value (unknown -> conservatively non-zero).
bool isNonZeroIndex(Value v);

/// Get the underlying object (root allocation) for a given value.
/// Recursively traverses through DbAcquireOp, DbGepOp, memref operations, etc.
/// to find the root allocation (DbAllocOp, memref::AllocOp, memref::AllocaOp,
/// etc.). Returns nullptr if no root object can be found.
mlir::Value getUnderlyingValue(mlir::Value v);
mlir::Operation *getUnderlyingOperation(mlir::Value v);

/// Get the byte size of an element type.
/// Returns the size in bytes for IntegerType and FloatType, 0 for unknown
/// types.
uint64_t getElementTypeByteSize(mlir::Type elemTy);

/// Sanitize a string for use in DOT graph format.
/// Replaces dots and dashes with underscores to make valid DOT identifiers.
std::string sanitizeString(llvm::StringRef s);

/// Return true if two ValueRanges have the same length and element-wise equal
/// Values.
bool equalRange(mlir::ValueRange a, mlir::ValueRange b);
} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_ARTSUTILS_H