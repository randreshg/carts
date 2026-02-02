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
#include "mlir/IR/Location.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/FunctionExtras.h"
#include <optional>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// Value Utilities
///===----------------------------------------------------------------------===///
/// Utility class for working with MLIR Values, constants, and casts.
class ValueUtils {
public:
  ///===----------------------------------------------------------------------===///
  /// Constant Value Analysis
  ///===----------------------------------------------------------------------===///
  /// Functions for checking and extracting constant values

  /// Checks if the given value is a constant, including constant-like
  /// operations such as constant indices and constant operations.
  static bool isValueConstant(Value val);

  /// Attempts to extract a constant index value from the given value,
  /// supporting both constant index operations and constant operations with
  /// integer attributes.
  static bool getConstantIndex(Value v, int64_t &out);

  /// Generic constant value extraction supporting both index and integer
  /// constants.
  static std::optional<int64_t> getConstantValue(Value v);

  /// Try to fold a constant index expression composed of basic arith ops.
  /// Supports common integer/index arithmetic patterns (add/sub/mul/div/casts).
  static std::optional<int64_t> tryFoldConstantIndex(Value v,
                                                     unsigned depth = 0);

  /// Extract a constant floating-point value from a Value.
  static std::optional<double> getConstantFloat(Value v);

  /// Check if a value is a zero constant (float or integer).
  static bool isZeroConstant(Value v);

  /// Check if a value is a one constant (float or integer).
  static bool isOneConstant(Value v);

  /// Check if two values are equivalent after stripping casts
  /// (same Value or same constant).
  static bool sameValue(Value a, Value b);

  /// Strip numeric casts and max(x, 1) clamping.
  static Value stripClampOne(Value v);

  /// Shallow structural equivalence for index expressions.
  static bool areValuesEquivalent(Value a, Value b, int depth = 0);

  /// Check if value is a constant >= 1 (strips casts first).
  static bool isConstantAtLeastOne(Value v);

  /// Recursively prove value is non-zero (for div/rem safety).
  static bool isProvablyNonZero(Value v, unsigned depth = 0);

  /// Determines if the given value represents a non-zero index, returning true
  /// for non-zero constants or unknown (non-constant) values.
  static bool isNonZeroIndex(Value v);

  ///===----------------------------------------------------------------------===///
  /// Value Type Conversion and Casting
  ///===----------------------------------------------------------------------===///
  /// Functions for type conversions and stripping casts

  /// Strip numeric cast operations to find the underlying value.
  /// Traverses through index casts, sign/zero extensions, truncations, and
  /// float extensions/truncations.
  static Value stripNumericCasts(Value value);

  /// Cast a value to index type if needed.
  static Value castToIndex(Value value, OpBuilder &builder, Location loc);

  /// Ensure a value is of index type, casting if necessary.
  static Value ensureIndexType(Value value, OpBuilder &builder, Location loc);

  ///===----------------------------------------------------------------------===///
  /// Value Dependencies and Analysis
  ///===----------------------------------------------------------------------===///
  /// Functions for analyzing value relationships and extracting patterns

  /// Check if a value depends on a base value through arithmetic operations.
  /// Used to determine data dependencies in index expressions.
  /// Traces through: add, sub, mul, div, rem, index casts, ext, trunc.
  static bool dependsOn(Value value, Value base, int depth = 0);

  /// Try to detect a constant stride for a base value within an index
  /// expression. Returns 1 when idx is base, constant C when idx contains
  /// base * C combined with additive terms, or std::nullopt if stride cannot
  /// be determined.
  static std::optional<int64_t> getOffsetStride(Value idx, Value base,
                                                int depth = 0);

  /// Check if a pointer/memref value is derived from a source value.
  /// Used for alias analysis and scope generation.
  /// Traces through: GEP, Pointer2Memref, Memref2Pointer, SubView, Cast, View,
  /// ReinterpretCast, and similar pointer-manipulating operations.
  static bool isDerivedFromPtr(Value value, Value source);

  /// Try to infer a constant linearization stride from an index expression.
  /// Looks for mul(constant, X) where X depends on elemOffset.
  static std::optional<int64_t> inferConstantStride(Value globalIndex,
                                                    Value elemOffset);

  /// Extract constant offset from an index expression involving loop IV and
  /// chunk offset.
  static std::optional<int64_t> extractConstantOffset(Value idx, Value loopIV,
                                                      Value chunkOffset);

  /// Strip constant add/sub offsets from an index expression.
  /// Returns the base value and accumulates the constant offset (if provided).
  static Value stripConstantOffset(Value value, int64_t *outConst = nullptr);

  /// Extract array index from byte offset pattern: bytes = (index * elemBytes).
  /// Handles common patterns where GEP indices are scaled by element byte size.
  /// Returns the logical array index or null Value if pattern doesn't match.
  static Value extractArrayIndexFromByteOffset(Value byteOffset, Type elemType);

  ///===----------------------------------------------------------------------===///
  /// Underlying Value Tracing
  ///===----------------------------------------------------------------------===///
  /// Functions for tracing values back to their root allocation

  /// Traces the underlying root allocation value for the given value, unwinding
  /// through various MLIR operations.
  static Value getUnderlyingValue(Value v);

  /// Retrieves the underlying operation that defines the root value for the
  /// given value.
  static Operation *getUnderlyingOperation(Value v);

  ///===----------------------------------------------------------------------===//
  /// Value Reconstruction for Dominance
  ///===----------------------------------------------------------------------===//

  /// Trace a value to a dominating point by reconstructing arithmetic ops.
  /// If the value already dominates insertBefore, returns it directly.
  /// Otherwise, recursively traces through arithmetic operations and recreates
  /// them at the insertion point to create an equivalent dominating value.
  /// Returns nullptr if the value cannot be traced/reconstructed.
  static Value traceValueToDominating(Value value, Operation *insertBefore,
                                      OpBuilder &builder,
                                      DominanceInfo &domInfo, Location loc,
                                      unsigned depth = 0);

  /// Recreate a binary op at a dominating point using a custom trace callback.
  /// The callback should return a dominating value for each operand or null.
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
};

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_VALUEUTILS_H
