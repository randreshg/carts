///==========================================================================///
/// File: ValueUtils.cpp
/// Defines utility functions for working with Index and Constant values.
///==========================================================================///
#include "arts/Utils/ValueUtils.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsUtils.h"
#include "polygeist/Ops.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/CSE.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <algorithm>
#include <cassert>

// For getElementTypeByteSize
#include "mlir/IR/BuiltinTypes.h"


namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// Constant Value Analysis
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

///===----------------------------------------------------------------------===///
/// Constant Value Analysis
///===----------------------------------------------------------------------===///

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

/// Generic constant value extraction supporting both index and integer
/// constants.
std::optional<int64_t> ValueUtils::getConstantValue(Value v) {
  int64_t val;
  if (getConstantIndex(v, val))
    return val;
  return std::nullopt;
}

/// Extract a constant floating-point value from a Value.
std::optional<double> ValueUtils::getConstantFloat(Value v) {
  if (!v)
    return std::nullopt;
  if (auto c = v.getDefiningOp<arith::ConstantOp>()) {
    if (auto floatAttr = dyn_cast<FloatAttr>(c.getValue())) {
      return floatAttr.getValueAsDouble();
    }
  }
  return std::nullopt;
}

/// Check if a value is a zero constant (float or integer).
bool ValueUtils::isZeroConstant(Value v) {
  if (auto cst = getConstantValue(v))
    return *cst == 0;
  if (auto cst = getConstantFloat(v))
    return *cst == 0.0;
  return false;
}

/// Check if a value is a one constant (float or integer).
bool ValueUtils::isOneConstant(Value v) {
  if (auto cst = getConstantValue(v))
    return *cst == 1;
  if (auto cst = getConstantFloat(v))
    return *cst == 1.0;
  return false;
}

///===----------------------------------------------------------------------===///
/// Value Type Conversion and Casting
///===----------------------------------------------------------------------===///

/// Strip numeric cast operations to find the underlying value.
/// Traverses through index casts, sign/zero extensions, truncations, and
/// float extensions/truncations.
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
    if (auto extF = value.getDefiningOp<arith::ExtFOp>()) {
      value = extF.getIn();
      continue;
    }
    if (auto truncF = value.getDefiningOp<arith::TruncFOp>()) {
      value = truncF.getIn();
      continue;
    }
    break;
  }
  return value;
}

///===----------------------------------------------------------------------===///
/// Value Type Conversion and Casting
///===----------------------------------------------------------------------===///

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

///===----------------------------------------------------------------------===///
/// Value Dependencies and Analysis
///===----------------------------------------------------------------------===///

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

/// Internal helper for isDerivedFromPtr with cycle detection and depth limit.
static bool isDerivedFromPtrImpl(Value value, Value source,
                                  SmallPtrSetImpl<Value> &visited,
                                  unsigned depth) {
  /// Depth limit to prevent stack overflow
  if (depth > kMaxTraceDepth)
    return false;

  if (value == source)
    return true;

  /// Cycle detection
  if (!visited.insert(value).second)
    return false;

  /// Handle EdtOp block arguments (trace through to dependencies)
  if (auto blockArg = dyn_cast<BlockArgument>(value)) {
    Block *block = blockArg.getParentBlock();
    if (auto edt = dyn_cast<EdtOp>(block->getParentOp())) {
      unsigned argIndex = blockArg.getArgNumber();
      ValueRange deps = edt.getDependencies();
      if (argIndex < deps.size())
        return isDerivedFromPtrImpl(deps[argIndex], source, visited, depth + 1);
    }
    return false;
  }

  Operation *defOp = value.getDefiningOp();
  if (!defOp)
    return false;

  /// ARTS datablock operations
  if (auto dbAcquire = dyn_cast<DbAcquireOp>(defOp))
    return isDerivedFromPtrImpl(dbAcquire.getSourcePtr(), source, visited, depth + 1);

  if (auto dbGep = dyn_cast<DbGepOp>(defOp))
    return isDerivedFromPtrImpl(dbGep.getBasePtr(), source, visited, depth + 1);

  if (auto dbControl = dyn_cast<DbControlOp>(defOp))
    return isDerivedFromPtrImpl(dbControl.getPtr(), source, visited, depth + 1);

  /// LLVM GEP operation
  if (auto gepOp = dyn_cast<LLVM::GEPOp>(defOp))
    return isDerivedFromPtrImpl(gepOp.getBase(), source, visited, depth + 1);

  /// Polygeist pointer/memref conversions
  if (auto ptr2memref = dyn_cast<polygeist::Pointer2MemrefOp>(defOp))
    return isDerivedFromPtrImpl(ptr2memref.getSource(), source, visited, depth + 1);

  if (auto memref2ptr = dyn_cast<polygeist::Memref2PointerOp>(defOp))
    return isDerivedFromPtrImpl(memref2ptr.getSource(), source, visited, depth + 1);

  /// MemRef operations
  if (auto subview = dyn_cast<memref::SubViewOp>(defOp))
    return isDerivedFromPtrImpl(subview.getSource(), source, visited, depth + 1);

  if (auto cast = dyn_cast<memref::CastOp>(defOp))
    return isDerivedFromPtrImpl(cast.getSource(), source, visited, depth + 1);

  if (auto view = dyn_cast<memref::ViewOp>(defOp))
    return isDerivedFromPtrImpl(view.getSource(), source, visited, depth + 1);

  if (auto reinterpretCast = dyn_cast<memref::ReinterpretCastOp>(defOp))
    return isDerivedFromPtrImpl(reinterpretCast.getSource(), source, visited, depth + 1);

  /// Fallback: check if operation has source as first operand
  /// (handles polygeist.subindex and other similar operations)
  if (defOp->getNumOperands() > 0)
    return isDerivedFromPtrImpl(defOp->getOperand(0), source, visited, depth + 1);

  return false;
}

/// Check if a pointer/memref value is derived from a source value.
/// Recursively traces through pointer-manipulating operations.
bool ValueUtils::isDerivedFromPtr(Value value, Value source) {
  SmallPtrSet<Value, 16> visited;
  return isDerivedFromPtrImpl(value, source, visited, 0);
}

/// Try to infer a constant linearization stride from an index expression.
/// Looks for mul(constant, X) where X depends on elemOffset.
std::optional<int64_t>
ValueUtils::inferConstantStride(Value globalIndex, Value elemOffset) {
  if (!globalIndex || !elemOffset)
    return std::nullopt;

  /// Strip casts to get to the core expression
  Value stripped = stripNumericCasts(globalIndex);

  /// Look for mul(constant, X) pattern where X depends on elemOffset
  if (auto mulOp = stripped.getDefiningOp<arith::MulIOp>()) {
    Value lhs = mulOp.getLhs();
    Value rhs = mulOp.getRhs();

    /// Check if one operand is a constant and the other depends on elemOffset
    int64_t constVal;
    if (getConstantIndex(lhs, constVal)) {
      /// Pattern: mul(constant, X) - check if X depends on elemOffset
      if (dependsOn(rhs, elemOffset))
        return constVal;
    } else if (getConstantIndex(rhs, constVal)) {
      /// Pattern: mul(X, constant) - check if X depends on elemOffset
      if (dependsOn(lhs, elemOffset))
        return constVal;
    }
  }

  return std::nullopt;
}

/// Extract constant offset from an index expression involving loop IV and chunk
/// offset. This function analyzes arithmetic expressions to extract constant
/// offsets from expressions like: chunkOffset + loopIV + constant.
///
/// Examples:
///   chunkOffset + loopIV -> offset = 0
///   chunkOffset + loopIV + 5 -> offset = 5
///   chunkOffset + loopIV - 1 -> offset = -1 (stencil pattern)
std::optional<int64_t>
ValueUtils::extractConstantOffset(Value idx, Value loopIV, Value chunkOffset) {
  int64_t accumulator = 0;
  Value current = idx;

  /// Helper to check if a value is one of our base values (loopIV or
  /// chunkOffset)
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

/// Extract array index from byte offset pattern: bytes = (index * elemBytes).
/// Handles common patterns where GEP indices are scaled by element byte size.
/// Returns the logical array index or null Value if pattern doesn't match.
Value ValueUtils::extractArrayIndexFromByteOffset(Value byteOffset,
                                                  Type elemType) {
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

///===----------------------------------------------------------------------===///
/// Underlying Value Tracing
///===----------------------------------------------------------------------===///

/// Maximum recursion depth for value tracing to prevent stack overflow
static constexpr unsigned kMaxTraceDepth = 64;

/// Internal helper to trace the underlying value through various operations,
/// with cycle detection and depth limit to avoid infinite recursion.
static Value getUnderlyingValueImpl(Value v, SmallPtrSet<Value, 16> &visited,
                                    unsigned depth) {
  if (!v)
    return nullptr;

  /// Depth limit to prevent stack overflow
  if (depth > kMaxTraceDepth)
    return nullptr;

  /// Check for cycles
  if (!visited.insert(v).second)
    return nullptr;

  if (v.isa<BlockArgument>()) {
    Block *block = v.getParentBlock();
    Operation *owner = block->getParentOp();
    if (auto edt = dyn_cast<EdtOp>(owner)) {
      unsigned argIndex = v.cast<BlockArgument>().getArgNumber();
      /// Block arguments correspond to dependencies
      ValueRange deps = edt.getDependencies();
      if (argIndex < deps.size()) {
        Value operand = deps[argIndex];
        return getUnderlyingValueImpl(operand, visited, depth + 1);
      } else {
        return v;
      }
    } else {
      /// Function argument
      return v;
    }
  } else if (auto op = v.getDefiningOp()) {
    /// Handle different operation types
    if (isa<DbAllocOp, memref::AllocOp, memref::AllocaOp, memref::GetGlobalOp>(
            op))
      return v;
    else if (auto dbAcquire = dyn_cast<DbAcquireOp>(op))
      return getUnderlyingValueImpl(dbAcquire.getSourcePtr(), visited,
                                    depth + 1);
    else if (auto dbGep = dyn_cast<DbGepOp>(op))
      return getUnderlyingValueImpl(dbGep.getBasePtr(), visited, depth + 1);
    else if (auto dbControl = dyn_cast<DbControlOp>(op))
      return getUnderlyingValueImpl(dbControl.getPtr(), visited, depth + 1);
    else if (auto subview = dyn_cast<memref::SubViewOp>(op))
      return getUnderlyingValueImpl(subview.getSource(), visited, depth + 1);
    else if (auto castOp = dyn_cast<memref::CastOp>(op))
      return getUnderlyingValueImpl(castOp.getSource(), visited, depth + 1);
    else if (auto view = dyn_cast<memref::ViewOp>(op))
      return getUnderlyingValueImpl(view.getSource(), visited, depth + 1);
    else if (auto reinterpret = dyn_cast<memref::ReinterpretCastOp>(op))
      return getUnderlyingValueImpl(reinterpret.getSource(), visited, depth + 1);
    else if (auto p2m = dyn_cast<polygeist::Pointer2MemrefOp>(op))
      return getUnderlyingValueImpl(p2m.getSource(), visited, depth + 1);
    else if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(op))
      return getUnderlyingValueImpl(m2p.getSource(), visited, depth + 1);
    else if (auto gep = dyn_cast<LLVM::GEPOp>(op))
      return getUnderlyingValueImpl(gep.getBase(), visited, depth + 1);
    else if (isa<arts::UndefOp>(op))
      return nullptr;
    else
      return nullptr;
  } else {
    /// Value has no defining operation and is not a block argument
    return nullptr;
  }
}

/// Traces the underlying root allocation value for the given value, unwinding
/// through various MLIR operations.
Value ValueUtils::getUnderlyingValue(Value v) {
  SmallPtrSet<Value, 16> visited;
  return getUnderlyingValueImpl(v, visited, 0);
}

/// Retrieves the underlying operation that defines the root value for the
/// given value.
Operation *ValueUtils::getUnderlyingOperation(Value v) {
  Value underlyingValue = getUnderlyingValue(v);
  if (!underlyingValue)
    return nullptr;

  /// If the underlying value is a result of an operation, return that operation
  if (auto definingOp = underlyingValue.getDefiningOp())
    return definingOp;

  return nullptr;
}

} // namespace arts
} // namespace mlir
