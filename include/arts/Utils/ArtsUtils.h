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

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// IR Simplification Utilities
///===----------------------------------------------------------------------===///
bool simplifyIR(ModuleOp module, DominanceInfo &domInfo);

///===----------------------------------------------------------------------===///
/// Value Analysis Utilities
///===----------------------------------------------------------------------===///
bool isValueConstant(Value val);
bool getConstantIndex(Value v, int64_t &out);
bool isNonZeroIndex(Value v);

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
