///==========================================================================///
/// File: ArtsUtils.h
///
/// Utility functions for ARTS.
///==========================================================================///

#ifndef CARTS_UTILS_ARTSUTILS_H
#define CARTS_UTILS_ARTSUTILS_H

#include "arts/ArtsDialect.h"
#include "arts/Utils/OpRemovalManager.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Value.h"
#include <optional>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// IR Simplification Utilities
///===----------------------------------------------------------------------===///
bool simplifyIR(ModuleOp module, DominanceInfo &domInfo);

///===----------------------------------------------------------------------===///
/// Value Analysis Utilities
///===----------------------------------------------------------------------===///
/// Utility class for working with Index and Constant values
class ValueUtils {
public:
  /// Checks if the given value is a constant, including constant-like operations
  /// such as constant indices and constant operations.
  static bool isValueConstant(Value val);

  /// Attempts to extract a constant index value from the given value, supporting
  /// both constant index operations and constant operations with integer
  /// attributes.
  static bool getConstantIndex(Value v, int64_t &out);

  /// Determines if the given value represents a non-zero index, returning true
  /// for non-zero constants or unknown (non-constant) values.
  static bool isNonZeroIndex(Value v);

  /// Check if a value depends on a base value through arithmetic operations.
  /// Used to determine data dependencies in index expressions.
  static bool dependsOn(Value value, Value base, int depth = 0);

  /// Try to infer a constant linearization stride from an index expression.
  /// Looks for mul(constant, X) where X depends on elemOffset.
  static std::optional<int64_t> inferConstantStride(Value globalIndex,
                                                    Value elemOffset);

  /// Strip numeric cast operations to find the underlying value.
  /// Traverses through index casts, sign/zero extensions, and truncations.
  static Value stripNumericCasts(Value value);

  /// Cast a value to index type if needed.
  static Value castToIndex(Value value, OpBuilder &builder, Location loc);

  /// Ensure a value is of index type, casting if necessary.
  static Value ensureIndexType(Value value, OpBuilder &builder, Location loc);

  /// Extract array index from byte offset pattern: bytes = (index * elemBytes).
  /// Handles common patterns where GEP indices are scaled by element byte size.
  /// Returns the logical array index or null Value if pattern doesn't match.
  static Value extractArrayIndexFromByteOffset(Value byteOffset, Type elemType);

  /// Generic constant value extraction supporting both index and integer constants.
  static std::optional<int64_t> getConstantValue(Value v);

  /// Extract constant offset from an index expression involving loop IV and chunk offset.
  static std::optional<int64_t> extractConstantOffset(Value idx, Value loopIV,
                                                      Value chunkOffset);
};

///===----------------------------------------------------------------------===///
/// Type and Size Utilities
///===----------------------------------------------------------------------===///
uint64_t getElementTypeByteSize(Type elementType);
MemRefType getElementMemRefType(Type elementType, ArrayRef<Value> elementSizes);

///===----------------------------------------------------------------------===///
/// String Utilities
///===----------------------------------------------------------------------===///
std::string sanitizeString(StringRef s);

///===----------------------------------------------------------------------===///
/// Range and Value Comparison Utilities
///===----------------------------------------------------------------------===///
bool equalRange(ValueRange a, ValueRange b);
bool allSameValue(ValueRange values);
bool scalesAreEquivalent(Value a, Value b);

///===----------------------------------------------------------------------===///
/// Access Mode Utilities
///===----------------------------------------------------------------------===///
ArtsMode combineAccessModes(ArtsMode mode1, ArtsMode mode2);

///===----------------------------------------------------------------------===///
/// EDT Analysis Utilities
///===----------------------------------------------------------------------===///
bool isInvariantInEdt(Region &edtRegion, Value value);
bool isReachable(Operation *source, Operation *target);

class EdtOp;
class DbAcquireOp;

std::pair<EdtOp, BlockArgument>
getEdtBlockArgumentForAcquire(DbAcquireOp acquireOp);

///===----------------------------------------------------------------------===///
/// Underlying Value Tracing Utilities
///===----------------------------------------------------------------------===///
Value getUnderlyingValue(Value v);
Value stripNumericCasts(Value v);
Operation *getUnderlyingOperation(Value v);
Operation *getUnderlyingDb(Value v, unsigned depth = 0);
Operation *getUnderlyingDbAlloc(Value v);

///===----------------------------------------------------------------------===///
// Type Casting and Conversion Utilities
///===----------------------------------------------------------------------===///
Value castToIndex(Value value, OpBuilder &builder, Location loc);

///===----------------------------------------------------------------------===///
// Pattern Recognition and Analysis Utilities
///===----------------------------------------------------------------------===///
std::optional<int64_t> extractChunkSizeFromHint(Value sizeHint, int depth = 0);
std::optional<int64_t> extractChunkSizeForAllocation(Value sizeHint,
                                                     int depth = 0);
Value extractOriginalSize(Value numerator, Value denominator,
                          OpBuilder &builder, Location loc);
Value extractArrayIndexFromByteOffset(Value byteOffset, Type elemType);
Value ensureIndexType(Value value, OpBuilder &builder, Location loc);
void collectWhileBounds(Value cond, Value iterArg, SmallVector<Value> &bounds);

///===----------------------------------------------------------------------===///
/// Operation Replacement Utilities
///===----------------------------------------------------------------------===///
void replaceUses(Value from, Value to, DominanceInfo &domInfo,
                 Operation *dominatingOp);
void replaceUses(DenseMap<Value, Value> &rewireMap);
void replaceInRegion(Region &region, Value from, Value to);
void replaceInRegion(Region &region, DenseMap<Value, Value> &rewireMap,
                     bool clear = true);

///===----------------------------------------------------------------------===///
/// Attribute Transfer Utilities
///===----------------------------------------------------------------------===///

/// Transfer attributes from source to destination operation.
/// Excludes auto-generated attributes (operandSegmentSizes, resultSegmentSizes)
void transferAttributes(Operation *src, Operation *dst,
                        ArrayRef<StringRef> excludes = {});

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_ARTSUTILS_H
