///==========================================================================///
/// File: ValueUtils.cpp
/// Defines utility functions for working with Index and Constant values.
///==========================================================================///
#include "arts/Utils/ArtsUtils.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/CSE.h"
#include <algorithm>
#include <cassert>

// For getElementTypeByteSize
#include "mlir/IR/BuiltinTypes.h"

/// Debug
#include "arts/Utils/ArtsDebug.h"

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// Value Analysis Utilities
///===----------------------------------------------------------------------===///

/// Checks if the given value is a constant, including constant-like operations
/// such as constant indices and constant operations.
bool ValueUtils::isValueConstant(Value val) {
  Operation *defOp = val.getDefiningOp();
  if (!defOp)
    return false;

  if (defOp->hasTrait<OpTrait::ConstantLike>())
    return true;

  if (isa<arith::ConstantIndexOp>(defOp) || isa<arith::ConstantOp>(defOp))
    return true;
  return false;
}

/// Attempts to extract a constant index value from the given value, supporting
/// both constant index operations and constant operations with integer
/// attributes.
bool ValueUtils::getConstantIndex(Value v, int64_t &out) {
  if (!v)
    return false;
  if (auto c = v.getDefiningOp<arith::ConstantIndexOp>()) {
    out = c.value();
    return true;
  }
  if (auto c = v.getDefiningOp<arith::ConstantOp>()) {
    if (auto intAttr = dyn_cast<IntegerAttr>(c.getValue())) {
      out = intAttr.getInt();
      return true;
    }
  }
  return false;
}

/// Determines if the given value represents a non-zero index, returning true
/// for non-zero constants or unknown (non-constant) values.
bool ValueUtils::isNonZeroIndex(Value v) {
  if (!v)
    return false;
  if (auto c = v.getDefiningOp<arith::ConstantIndexOp>())
    return c.value() != 0;
  return true;
}

/// Check if a value depends on a base value through arithmetic operations.
/// Used to determine data dependencies in index expressions.
bool ValueUtils::dependsOn(Value value, Value base, int depth) {
  if (!value || !base || depth > 8)
    return false;
  if (value == base)
    return true;

  if (isa<BlockArgument>(value))
    return false;

  Operation *op = value.getDefiningOp();
  if (!op)
    return false;

  if (!isa<arith::AddIOp, arith::SubIOp, arith::MulIOp, arith::DivSIOp,
           arith::DivUIOp, arith::RemSIOp, arith::RemUIOp, arith::IndexCastOp,
           arith::IndexCastUIOp, arith::ExtSIOp, arith::ExtUIOp,
           arith::TruncIOp>(op))
    return false;

  for (Value operand : op->getOperands())
    if (dependsOn(operand, base, depth + 1))
      return true;

  return false;
}

/// Try to infer a constant linearization stride from an index expression:
/// looks for mul(constant, X) where X depends on elemOffset.
std::optional<int64_t> ValueUtils::inferConstantStride(Value globalIndex,
                                                       Value elemOffset) {
  if (!globalIndex || !elemOffset)
    return std::nullopt;

  SmallVector<Value> worklist{globalIndex};
  DenseSet<Value> visited;

  auto getConstIdx = [](Value v) -> std::optional<int64_t> {
    int64_t cst;
    if (ValueUtils::getConstantIndex(v, cst))
      return cst;
    return std::nullopt;
  };

  while (!worklist.empty()) {
    Value cur = worklist.pop_back_val();
    if (!visited.insert(cur).second)
      continue;

    if (auto mul = cur.getDefiningOp<arith::MulIOp>()) {
      Value lhs = mul.getLhs();
      Value rhs = mul.getRhs();

      if (auto lhsCst = getConstIdx(lhs)) {
        if (*lhsCst > 1 && ValueUtils::dependsOn(rhs, elemOffset))
          return lhsCst;
      }
      if (auto rhsCst = getConstIdx(rhs)) {
        if (*rhsCst > 1 && ValueUtils::dependsOn(lhs, elemOffset))
          return rhsCst;
      }

      worklist.push_back(lhs);
      worklist.push_back(rhs);
      continue;
    }

    if (auto add = cur.getDefiningOp<arith::AddIOp>()) {
      worklist.push_back(add.getLhs());
      worklist.push_back(add.getRhs());
      continue;
    }
    if (auto sub = cur.getDefiningOp<arith::SubIOp>()) {
      worklist.push_back(sub.getLhs());
      worklist.push_back(sub.getRhs());
      continue;
    }
  }

  return std::nullopt;
}

/// Strip numeric cast operations to find the underlying value.
/// Traverses through index casts, sign/zero extensions, and truncations.
Value ValueUtils::stripNumericCasts(Value value) {
  while (true) {
    if (auto idxCast = value.getDefiningOp<arith::IndexCastOp>()) {
      value = idxCast.getIn();
      continue;
    }
    if (auto ext = value.getDefiningOp<arith::ExtSIOp>()) {
      value = ext.getIn();
      continue;
    }
    if (auto ext = value.getDefiningOp<arith::ExtUIOp>()) {
      value = ext.getIn();
      continue;
    }
    if (auto trunc = value.getDefiningOp<arith::TruncIOp>()) {
      value = trunc.getIn();
      continue;
    }
    break;
  }
  return value;
}

/// Cast a value to index type if needed.
Value ValueUtils::castToIndex(Value value, OpBuilder &builder, Location loc) {
  if (!value)
    return value;
  if (value.getType().isIndex())
    return value;
  if (value.getType().isIntOrIndex())
    return builder.create<arith::IndexCastOp>(loc, builder.getIndexType(),
                                              value);
  return value;
}

/// Ensure a value is of index type, casting if necessary.
Value ValueUtils::ensureIndexType(Value value, OpBuilder &builder, Location loc) {
  if (!value)
    return Value();
  if (value.getType().isa<IndexType>())
    return value;
  if (auto intTy = value.getType().dyn_cast<IntegerType>())
    return builder.create<arith::IndexCastOp>(loc, builder.getIndexType(),
                                              value);
  return Value();
}

/// Extract array index from byte offset pattern: bytes = (index * elemBytes).
/// Handles common patterns where GEP indices are scaled by element byte size.
/// Returns the logical array index or null Value if pattern doesn't match.
Value ValueUtils::extractArrayIndexFromByteOffset(Value byteOffset, Type elemType) {
  /// Strip any index casts to get to the core computation
  Value idxSource = byteOffset;
  while (auto castOp = idxSource.getDefiningOp<arith::IndexCastOp>())
    idxSource = castOp.getIn();

  /// Look for multiplication: index * elemBytes
  auto mulOp = idxSource.getDefiningOp<arith::MulIOp>();
  if (!mulOp)
    return Value();

  int64_t elemSize = getElementTypeByteSize(elemType);
  if (elemSize == 0)
    return Value();

  /// Identify which operand is the constant element size
  auto isElementSizeConstant = [elemSize](Value v) -> bool {
    if (auto constIdx = v.getDefiningOp<arith::ConstantIndexOp>())
      return constIdx.value() == elemSize;
    if (auto constInt = v.getDefiningOp<arith::ConstantIntOp>())
      return constInt.value() == elemSize && constInt.getType().isInteger(64);
    return false;
  };

  Value lhs = mulOp.getLhs();
  Value rhs = mulOp.getRhs();

  if (isElementSizeConstant(lhs))
    return rhs;
  if (isElementSizeConstant(rhs))
    return lhs;
  /// Pattern doesn't match expected form
  return Value();
}

/// Generic constant value extraction supporting both index and integer constants.
std::optional<int64_t> ValueUtils::getConstantValue(Value v) {
  if (auto constOp = v.getDefiningOp<arith::ConstantIndexOp>())
    return constOp.value();
  if (auto constOp = v.getDefiningOp<arith::ConstantIntOp>())
    return constOp.value();
  return std::nullopt;
}

/// Extract constant offset from an index expression involving loop IV and chunk offset.
/// This function analyzes arithmetic expressions to extract constant offsets from
/// expressions like: chunkOffset + loopIV + constant.
///
/// Examples:
///   chunkOffset + loopIV -> offset = 0
///   chunkOffset + loopIV + 5 -> offset = 5
///   chunkOffset + loopIV - 1 -> offset = -1 (stencil pattern)
std::optional<int64_t> ValueUtils::extractConstantOffset(Value idx, Value loopIV,
                                                         Value chunkOffset) {
  int64_t accumulator = 0;
  Value current = idx;

  /// Helper to check if a value is one of our base values (loopIV or chunkOffset)
  auto isBaseValue = [&](Value v) -> bool {
    return v == loopIV || v == chunkOffset;
  };

  /// Helper to check if current is the base pattern (loopIV + chunkOffset)
  auto isBasePattern = [&](Value v) -> bool {
    if (auto addOp = v.getDefiningOp<arith::AddIOp>()) {
      Value lhs = addOp.getLhs();
      Value rhs = addOp.getRhs();
      return (lhs == loopIV && rhs == chunkOffset) ||
             (lhs == chunkOffset && rhs == loopIV);
    }
    return false;
  };

  while (true) {
    /// Check if we've reached a terminal state (a base value or base pattern)
    if (isBaseValue(current) || isBasePattern(current)) {
      return accumulator;
    }

    if (auto addOp = current.getDefiningOp<arith::AddIOp>()) {
      Value lhs = addOp.getLhs();
      Value rhs = addOp.getRhs();

      int64_t constVal;
      if (getConstantIndex(rhs, constVal)) {
        /// Pattern: expr + constant -> accumulate and continue with expr
        accumulator += constVal;
        current = lhs;
      } else if (getConstantIndex(lhs, constVal)) {
        /// Pattern: constant + expr -> accumulate and continue with expr
        accumulator += constVal;
        current = rhs;
      } else {
        /// Neither operand is a constant
        /// Check if one operand is a base value, continue with the other
        bool lhsIsBase = isBaseValue(lhs) || isBasePattern(lhs);
        bool rhsIsBase = isBaseValue(rhs) || isBasePattern(rhs);

        if (lhsIsBase && rhsIsBase) {
          /// Both are base values/patterns: chunkOffset + loopIV
          return accumulator;
        } else if (lhsIsBase) {
          /// LHS is base, continue checking RHS
          current = rhs;
        } else if (rhsIsBase) {
          /// RHS is base, continue checking LHS
          current = lhs;
        } else {
          /// Neither is a base, give up
          break;
        }
      }
    } else if (auto subOp = current.getDefiningOp<arith::SubIOp>()) {
      Value lhs = subOp.getLhs();
      Value rhs = subOp.getRhs();

      int64_t constVal;
      if (getConstantIndex(rhs, constVal)) {
        /// Pattern: expr - constant -> accumulate negative and continue
        accumulator -= constVal;
        current = lhs;
        continue;
      }
      /// Subtraction with non-constant RHS - give up
      break;
    } else if (auto indexCast = current.getDefiningOp<arith::IndexCastOp>()) {
      /// Handle index_cast(i32_value) - common pattern where arithmetic
      /// is done in i32 then cast back to index
      current = indexCast.getIn();
      continue;
    } else {
      break;
    }
  }

  return std::nullopt;
}

} // namespace arts
} // namespace mlir
