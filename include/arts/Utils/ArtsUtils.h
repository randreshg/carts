///==========================================================================
/// File: ArtsUtils.h
///==========================================================================

#ifndef CARTS_UTILS_ARTSUTILS_H
#define CARTS_UTILS_ARTSUTILS_H

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Value.h"

namespace mlir {
namespace arts {

/// Simplify the IR
bool simplifyIR(ModuleOp module, DominanceInfo &domInfo);

/// Checks if a Value is invariant within a given EDT region.
/// A value is considered invariant if it's defined outside the region
/// and not modified by any operation inside the region. Constants are
/// always considered invariant.
/// This assummes the EdtInvariantCodeMotion pass has been run.
bool isInvariantInEdt(Region &edtRegion, Value value);

/// Returns true if `target` is reachable from `source` in the EDT CFG.
bool isReachable(Operation *source, Operation *target);

/// Remove a set of operations from the module.
void removeOps(ModuleOp module, OpBuilder &builder,
               llvm::SetVector<Operation *> &opsToRemove);
void recursivelyRemoveOp(Operation *op);
void removeUndefOps(ModuleOp module);

/// Replace the operation with an undef operation.
void replaceWithUndef(Operation *op, OpBuilder &builder);
void replaceUses(Value from, Value to, DominanceInfo &domInfo,
                 Operation *dominatingOp);

/// Replace all uses of `from` with `to` in the module.
void replaceUses(llvm::DenseMap<Value, Value> &rewireMap);
void replaceInRegion(Region &region, Value from, Value to);
void replaceInRegion(Region &region, llvm::DenseMap<Value, Value> &rewireMap,
                     bool clear = true);

/// Returns true if `val` is a constant value.
bool isValueConstant(Value val);

/// Try to extract an index constant from a Value.
/// Returns true and sets `out` if the Value is a constant index (via
/// arith.constant or compatible); otherwise returns false.
bool getConstantIndex(Value v, int64_t &out);

/// Returns true if `v` is an index constant not equal to zero,
/// or a non-constant value (unknown -> conservatively non-zero).
bool isNonZeroIndex(Value v);

/// Get the underlying object (root allocation) for a given value.
/// Recursively traverses through DbAcquireOp, DbGepOp, memref operations, etc.
/// to find the root allocation (DbAllocOp, memref::AllocOp, memref::AllocaOp,
/// etc.). Returns nullptr if no root object can be found.
Value getUnderlyingValue(Value v);
Operation *getUnderlyingOperation(Value v);

/// Return the Datablock operation associated with a value, if any.
/// - If the value is produced by arts.db_acquire, returns that DbAcquireOp.
/// - If the value is produced by arts.db_alloc, returns that DbAllocOp.
/// - If the value is a BlockArgument of an arts.edt body, attempts to map it
///   to its corresponding operand (skipping route if present) and returns the
///   associated DbAcquireOp if found. Traces through simple view/cast/gep ops.
/// Returns nullptr if no associated DB operation can be determined.
Operation *getUnderlyingDb(Value v);

/// Get the byte size of an element type.
/// Returns the size in bytes for IntegerType and FloatType, 0 for unknown
/// types.
uint64_t getElementTypeByteSize(Type elemTy);

/// Sanitize a string for use in DOT graph format.
/// Replaces dots and dashes with underscores to make valid DOT identifiers.
std::string sanitizeString(llvm::StringRef s);

/// Return true if two ValueRanges have the same length and element-wise equal
/// Values.
bool equalRange(ValueRange a, ValueRange b);
} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_ARTSUTILS_H