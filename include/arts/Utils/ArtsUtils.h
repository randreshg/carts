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

//===----------------------------------------------------------------------===//
/// IR Simplification Utilities
//===----------------------------------------------------------------------===//
bool simplifyIR(ModuleOp module, DominanceInfo &domInfo);

//===----------------------------------------------------------------------===//
/// Value Analysis Utilities
//===----------------------------------------------------------------------===//
bool isValueConstant(Value val);
bool getConstantIndex(Value v, int64_t &out);
bool isNonZeroIndex(Value v);

//===----------------------------------------------------------------------===//
/// Type and Size Utilities
//===----------------------------------------------------------------------===//
uint64_t getElementTypeByteSize(Type elemTy);

//===----------------------------------------------------------------------===//
/// String Utilities
//===----------------------------------------------------------------------===//
std::string sanitizeString(StringRef s);

//===----------------------------------------------------------------------===//
/// Range and Value Comparison Utilities
//===----------------------------------------------------------------------===//
bool equalRange(ValueRange a, ValueRange b);
bool allSameValue(ValueRange values);

//===----------------------------------------------------------------------===//
/// EDT Analysis Utilities
//===----------------------------------------------------------------------===//
bool isInvariantInEdt(Region &edtRegion, Value value);
bool isReachable(Operation *source, Operation *target);

//===----------------------------------------------------------------------===//
/// Underlying Value Tracing Utilities
//===----------------------------------------------------------------------===//
Value getUnderlyingValue(Value v);
Operation *getUnderlyingOperation(Value v);
Operation *getUnderlyingDb(Value v);

//===----------------------------------------------------------------------===//
/// Operation Removal and Replacement Utilities
//===----------------------------------------------------------------------===//

void removeOps(ModuleOp module, SetVector<Operation *> &opsToRemove,
               bool recursive = false);
void removeUndefOps(ModuleOp module);
void replaceWithUndef(Operation *op, OpBuilder &builder);
void replaceUses(Value from, Value to, DominanceInfo &domInfo,
                 Operation *dominatingOp);
void replaceUses(DenseMap<Value, Value> &rewireMap);
void replaceInRegion(Region &region, Value from, Value to);
void replaceInRegion(Region &region, DenseMap<Value, Value> &rewireMap,
                     bool clear = true);
} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_ARTSUTILS_H