///==========================================================================///
/// File: Utils.h
///
/// Utility functions for ARTS.
///==========================================================================///

#ifndef CARTS_UTILS_ARTSUTILS_H
#define CARTS_UTILS_ARTSUTILS_H

#include "arts/Dialect.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Value.h"
#include <optional>

namespace mlir {
namespace arts {

/// IR Simplification Utilities
bool simplifyIR(ModuleOp module, DominanceInfo &domInfo);

/// Type and Size Utilities
uint64_t getElementTypeByteSize(Type elementType);
MemRefType getElementMemRefType(Type elementType, ArrayRef<Value> elementSizes);

/// String Utilities
std::string sanitizeString(StringRef s);

/// Access Mode Utilities
ArtsMode combineAccessModes(ArtsMode mode1, ArtsMode mode2);

/// Constant Index Creation Utilities
Value createConstantIndex(OpBuilder &builder, Location loc, int64_t val);
Value createZeroIndex(OpBuilder &builder, Location loc);
Value createOneIndex(OpBuilder &builder, Location loc);

/// ARTS Runtime Query Utilities
bool isArtsRuntimeQuery(Value val);

/// Operation Replacement Utilities
void replaceUses(Value from, Value to, DominanceInfo &domInfo,
                 Operation *dominatingOp);
void replaceUses(DenseMap<Value, Value> &rewireMap);
void replaceInRegion(Region &region, Value from, Value to);
void replaceInRegion(Region &region, DenseMap<Value, Value> &rewireMap,
                     bool clear = true);

/// Transfer attributes from source to destination operation.
void transferAttributes(Operation *src, Operation *dst,
                        ArrayRef<StringRef> excludes = {});

/// Check if a value is defined inside a given region (either as a block
/// argument owned by a block in the region, or as the result of an operation
/// whose parent region is within the given region).
inline bool isDefinedInRegion(Value val, Region *region) {
  if (auto blockArg = val.dyn_cast<BlockArgument>())
    return region->isAncestor(blockArg.getOwner()->getParent());
  if (Operation *defOp = val.getDefiningOp())
    return region->isAncestor(defOp->getParentRegion());
  return false;
}

/// Whitelist check for pure arithmetic operations that are safe to clone
/// into EDT bodies (start-block cloning). Returns true for constant, div,
/// rem, add, sub, mul, max, min, select, and index_cast operations.
inline bool isStartBlockArithmeticOp(Operation *op) {
  return isa<arith::ConstantIndexOp, arith::ConstantIntOp, arith::DivUIOp,
             arith::DivSIOp, arith::RemUIOp, arith::RemSIOp, arith::AddIOp,
             arith::SubIOp, arith::MulIOp, arith::MaxUIOp, arith::MinUIOp,
             arith::SelectOp, arith::IndexCastOp>(op);
}

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_ARTSUTILS_H
