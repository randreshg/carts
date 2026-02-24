///==========================================================================///
/// File: ValueUtils.h
///
/// Utility class for working with MLIR Values, constants, and casts.
///==========================================================================///

#ifndef CARTS_UTILS_VALUEUTILS_H
#define CARTS_UTILS_VALUEUTILS_H

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/FunctionExtras.h"
#include "llvm/ADT/SetVector.h"
#include <optional>

namespace mlir {
namespace arts {

/// Utility class for working with MLIR Values, constants, and casts.
class ValueUtils {
public:
  ///===----------------------------------------------------------------------===///
  /// Constant Value Analysis
  ///===----------------------------------------------------------------------===///

  /// Check if value is defined by a ConstantLike operation.
  static bool isValueConstant(Value val);

  /// Extract a constant integer/index value, returns false if not constant.
  static bool getConstantIndex(Value v, int64_t &out);

  /// Like getConstantIndex but returns optional.
  static std::optional<int64_t> getConstantValue(Value v);

  /// Recursively fold constant index expressions through basic arith ops.
  static std::optional<int64_t> tryFoldConstantIndex(Value v,
                                                     unsigned depth = 0);

  static std::optional<double> getConstantFloat(Value v);
  static bool isConstantEqual(Value v, int64_t val);
  static bool isZeroConstant(Value v);
  static bool isOneConstant(Value v);

  /// Check equivalence after stripping casts (same Value or same constant).
  static bool sameValue(Value a, Value b);

  /// Strip numeric casts and max(x, 1) clamping.
  static Value stripClampOne(Value v);

  /// Strip select(cmp(...), x, y) min/max clamp patterns recursively.
  /// Peels through up to `maxDepth` layers of select-based clamping.
  static Value stripSelectClamp(Value value, int maxDepth = 8);

  /// Shallow structural equivalence for index expressions.
  static bool areValuesEquivalent(Value a, Value b, int depth = 0);

  /// Check if value is a constant >= 1 (strips casts first).
  static bool isConstantAtLeastOne(Value v);

  /// Recursively prove value is non-zero (for div/rem safety).
  static bool isProvablyNonZero(Value v, unsigned depth = 0);

  /// Returns true for non-zero constants or unknown (non-constant) values.
  static bool isNonZeroIndex(Value v);

  ///===----------------------------------------------------------------------===///
  /// Value Type Conversion and Casting
  ///===----------------------------------------------------------------------===///

  /// Strip through index casts, sign/zero extensions, truncations.
  static Value stripNumericCasts(Value value);

  /// Cast to index type if needed. Returns value unchanged if non-integer.
  static Value castToIndex(Value value, OpBuilder &builder, Location loc);

  /// Cast to index type. Returns null Value for non-integer types.
  static Value ensureIndexType(Value value, OpBuilder &builder, Location loc);

  ///===----------------------------------------------------------------------===///
  /// Value Dependencies and Analysis
  ///===----------------------------------------------------------------------===///

  /// Check if value depends on base through arithmetic operations.
  static bool dependsOn(Value value, Value base, int depth = 0);

  /// Detect constant stride for base within an index expression.
  /// Returns 1 when idx==base, C when idx contains base*C, else nullopt.
  static std::optional<int64_t> getOffsetStride(Value idx, Value base,
                                                int depth = 0);

  /// Check if a pointer/memref value is derived from source through
  /// GEP, casts, SubView, and similar pointer-manipulating operations.
  static bool isDerivedFromPtr(Value value, Value source);

  /// Infer constant stride from mul(constant, X) where X depends on offset.
  static std::optional<int64_t> inferConstantStride(Value globalIndex,
                                                    Value elemOffset);

  /// Extract constant offset from expression relative to loopIV + chunkOffset.
  static std::optional<int64_t> extractConstantOffset(Value idx, Value loopIV,
                                                      Value chunkOffset);

  /// Strip constant add/sub offsets, returning base and accumulated offset.
  static Value stripConstantOffset(Value value, int64_t *outConst = nullptr);

  /// Extract array index from byte offset pattern: bytes = (index * elemBytes).
  static Value extractArrayIndexFromByteOffset(Value byteOffset, Type elemType);

  ///===----------------------------------------------------------------------===///
  /// Underlying Value Tracing
  ///===----------------------------------------------------------------------===///

  /// Trace to root allocation through casts, acquires, GEPs, etc.
  static Value getUnderlyingValue(Value v);

  /// Like getUnderlyingValue but returns the defining operation.
  static Operation *getUnderlyingOperation(Value v);

  ///===----------------------------------------------------------------------===//
  /// Value Reconstruction for Dominance
  ///===----------------------------------------------------------------------===//

  /// Reconstruct a value at a dominating point by tracing through arithmetic.
  /// Returns the value directly if it already dominates, or nullptr on failure.
  static Value traceValueToDominating(Value value, Operation *insertBefore,
                                      OpBuilder &builder,
                                      DominanceInfo &domInfo, Location loc,
                                      unsigned depth = 0);

  /// Recreate a binary op at a dominating point. Callback traces each operand.
  template <typename OpType>
  static Value traceBinaryOp(OpType op, Operation *insertBefore,
                             OpBuilder &builder, Location loc,
                             llvm::function_ref<Value(Value)> traceValueFn) {
    Value lhs = traceValueFn(op.getLhs());
    Value rhs = traceValueFn(op.getRhs());
    if (!lhs || !rhs)
      return nullptr;
    builder.setInsertionPoint(insertBefore);
    return builder.create<OpType>(loc, lhs, rhs);
  }

  /// Trace min/select with partial operand fallback.
  static Value
  traceMinSIWithFallback(arith::MinSIOp minOp, Operation *insertBefore,
                         OpBuilder &builder, Location loc,
                         llvm::function_ref<Value(Value)> traceValueFn);
  static Value
  traceMinUIWithFallback(arith::MinUIOp minOp, Operation *insertBefore,
                         OpBuilder &builder, Location loc,
                         llvm::function_ref<Value(Value)> traceValueFn);
  static Value
  traceSelectWithFallback(arith::SelectOp selectOp, Operation *insertBefore,
                          OpBuilder &builder, Location loc,
                          llvm::function_ref<Value(Value)> traceValueFn,
                          llvm::function_ref<Value(Value)> traceCondFn);

  ///===----------------------------------------------------------------------===//
  /// Value Cloning Utilities
  ///===----------------------------------------------------------------------===//

  /// Check if an operation can be safely cloned (constant-like or pure).
  static bool
  canCloneOperation(Operation *op, bool allowMemoryEffectFree = true,
                    llvm::function_ref<bool(Operation *)> extraAllowed = {});

  /// Clone external values and their dependencies into a target region.
  static bool cloneValuesIntoRegion(
      llvm::SetVector<Value> &values, Region *targetRegion, IRMapping &mapper,
      OpBuilder &builder, bool allowMemoryEffectFree = true,
      llvm::function_ref<bool(Operation *)> extraAllowed = {});
};

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_VALUEUTILS_H
