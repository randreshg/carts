///==========================================================================///
/// File: ArtsUtils.cpp
/// Defines utility functions for the ARTS dialect.
///==========================================================================///
#include "arts/Utils/ArtsUtils.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/CSE.h"
#include "mlir/Transforms/RegionUtils.h"
#include "polygeist/Ops.h"
#include <algorithm>
#include <cassert>

/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(arts_utils);

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// IR Simplification Utilities
///===----------------------------------------------------------------------===///

/// Simplifies the IR by running common subexpression elimination (CSE)
/// across the module using the provided dominance information.
bool simplifyIR(ModuleOp module, DominanceInfo &domInfo) {
  IRRewriter rewriter(module.getContext());
  bool changed = false;
  eliminateCommonSubExpressions(rewriter, domInfo, module, &changed);

  return changed;
}

///===----------------------------------------------------------------------===///
/// Type and Size Utilities
///===----------------------------------------------------------------------===///

/// Computes the byte size of the given element type, supporting integer and
/// floating-point types. When the type is a memref, all static dimensions must
/// be known; otherwise, the size is treated as unknown (returns 0).
uint64_t getElementTypeByteSize(Type elemTy) {
  /// Safety check: return 0 for null or invalid types
  if (!elemTy) {
    return 0;
  }

  if (auto memTy = dyn_cast<MemRefType>(elemTy)) {
    uint64_t elementBytes = getElementTypeByteSize(memTy.getElementType());
    if (elementBytes == 0)
      return 0;

    uint64_t total = elementBytes;
    for (int64_t dim : memTy.getShape()) {
      if (dim == ShapedType::kDynamic)
        return 0;
      total *= static_cast<uint64_t>(std::max<int64_t>(dim, 0));
    }
    return total;
  }
  if (auto intTy = dyn_cast<IntegerType>(elemTy))
    return intTy.getWidth() / 8u;
  if (auto fTy = dyn_cast<FloatType>(elemTy))
    return fTy.getWidth() / 8u;
  /// Unknown type
  return 0;
}

/// Gets the element memref type for a given element type and sizes.
MemRefType getElementMemRefType(Type elementType,
                                ArrayRef<Value> elementSizes) {
  /// Enforce scalar payloads use a single trailing dimension of 1 instead of
  /// an empty shape to keep rank handling uniform across the pipeline.
  const size_t rank = elementSizes.empty() ? 1 : elementSizes.size();
  SmallVector<int64_t> elementShape(rank, ShapedType::kDynamic);
  return MemRefType::get(elementShape, elementType);
}

///===----------------------------------------------------------------------===///
/// String Utilities
///===----------------------------------------------------------------------===///

/// Sanitizes the given string by replacing dots and dashes with underscores,
/// making it suitable for use as an identifier.
std::string sanitizeString(StringRef s) {
  std::string id = s.str();
  std::replace(id.begin(), id.end(), '.', '_');
  std::replace(id.begin(), id.end(), '-', '_');
  return id;
}

///===----------------------------------------------------------------------===///
/// Range and Value Comparison Utilities
///===----------------------------------------------------------------------===///

/// Compares two ValueRanges for equality, checking both size and element-wise
/// equivalence.
bool equalRange(ValueRange a, ValueRange b) {
  if (a.size() != b.size())
    return false;
  for (auto it = a.begin(), jt = b.begin(); it != a.end(); ++it, ++jt) {
    if (*it != *jt)
      return false;
  }
  return true;
}

/// Checks if all values in the given range are identical, returning false for
/// empty ranges.
bool allSameValue(ValueRange values) {
  if (values.empty())
    return false;
  Value first = values[0];
  return llvm::all_of(values, [&](Value v) { return v == first; });
}

/// Check if two values represent equivalent scaling factors.
/// Used to recognize patterns like (N * sizeof(T)) / sizeof(T) -> N.
bool scalesAreEquivalent(Value lhs, Value rhs) {
  Value a = ValueUtils::stripNumericCasts(lhs);
  Value b = ValueUtils::stripNumericCasts(rhs);
  if (a == b)
    return true;

  auto constantValue = [](Value v) -> std::optional<int64_t> {
    if (auto cIdx = v.getDefiningOp<arith::ConstantIndexOp>())
      return cIdx.value();
    if (auto cInt = v.getDefiningOp<arith::ConstantIntOp>())
      return cInt.value();
    return std::nullopt;
  };

  if (auto lhsConst = constantValue(a))
    if (auto rhsConst = constantValue(b))
      return lhsConst == rhsConst;

  if (auto lhsType = a.getDefiningOp<polygeist::TypeSizeOp>())
    if (auto rhsType = b.getDefiningOp<polygeist::TypeSizeOp>())
      return lhsType.getType() == rhsType.getType();

  return false;
}

///===----------------------------------------------------------------------===///
/// Access Mode Utilities
///===----------------------------------------------------------------------===///

/// Combine two access modes and return the more permissive mode
ArtsMode combineAccessModes(ArtsMode mode1, ArtsMode mode2) {
  /// If either mode is uninitialized, return the other mode
  if (mode1 == ArtsMode::uninitialized)
    return mode2;
  if (mode2 == ArtsMode::uninitialized)
    return mode1;

  /// If either mode is inout, the result is inout (most permissive)
  if (mode1 == ArtsMode::inout || mode2 == ArtsMode::inout)
    return ArtsMode::inout;

  /// If one is 'in' and the other is 'out', the result is inout
  if ((mode1 == ArtsMode::in && mode2 == ArtsMode::out) ||
      (mode1 == ArtsMode::out && mode2 == ArtsMode::in))
    return ArtsMode::inout;

  /// If both are the same, return that mode
  if (mode1 == mode2)
    return mode1;

  /// Default to inout for any other combination (shouldn't happen)
  return ArtsMode::inout;
}

///===----------------------------------------------------------------------===///
/// Operation Replacement Utilities
///===----------------------------------------------------------------------===///

/// Replaces uses of one value with another under dominance constraints,
/// skipping the dominating operation.
void replaceUses(Value from, Value to, DominanceInfo &domInfo,
                 Operation *dominatingOp) {
  /// Ensure that 'from' has a defining operation.
  auto *definingOp = from.getDefiningOp();
  if (!definingOp)
    return;

  /// Replace all uses of 'from' with 'to' if the user is dominated by the
  /// defining operation of 'from'.
  from.replaceUsesWithIf(to, [&](OpOperand &operand) {
    if (operand.getOwner() == dominatingOp)
      return false;
    return domInfo.dominates(dominatingOp, operand.getOwner());
  });
}

/// Replaces uses according to a mapping of values.
void replaceUses(DenseMap<Value, Value> &rewireMap) {
  for (auto &rewire : rewireMap)
    rewire.first.replaceAllUsesWith(rewire.second);
  rewireMap.clear();
}

/// Replaces uses of a value within a specific region.
void replaceInRegion(Region &region, Value from, Value to) {
  from.replaceUsesWithIf(to, [&](OpOperand &operand) {
    return region.isAncestor(operand.getOwner()->getParentRegion());
  });
}

/// Replaces uses according to a mapping within a specific region.
void replaceInRegion(Region &region, DenseMap<Value, Value> &rewireMap,
                     bool clear) {
  for (auto &rewire : rewireMap)
    replaceInRegion(region, rewire.first, rewire.second);
  if (clear)
    rewireMap.clear();
}

///===----------------------------------------------------------------------===///
// Pattern Recognition and Analysis Utilities
///===----------------------------------------------------------------------===///

/// Extract chunk size from ForLowering's size hint.
/// Patterns handled:
///   Case 1: Direct constant %c1048576 -> returns 1048576
///   Case 2: minui(%remaining, %c1048576) -> returns larger constant
///   Case 3: Nested minui -> recursively finds largest constant
///   Case 4: addi(%baseSize, %halo) -> recurse on non-constant operand
///           (for stencil patterns where chunkSize = baseChunk + haloAdjust)
std::optional<int64_t> extractChunkSizeFromHint(Value sizeHint, int depth) {
  if (!sizeHint || depth > 4)
    return std::nullopt;

  /// Case 1: Direct constant
  int64_t val;
  if (ValueUtils::getConstantIndex(sizeHint, val))
    return val;

  /// Case 2/3: minui pattern - return the larger constant (nominal size)
  if (auto minOp = sizeHint.getDefiningOp<arith::MinUIOp>()) {
    int64_t lhsVal = 0, rhsVal = 0;
    bool hasLhs = ValueUtils::getConstantIndex(minOp.getLhs(), lhsVal);
    bool hasRhs = ValueUtils::getConstantIndex(minOp.getRhs(), rhsVal);

    if (hasLhs && hasRhs)
      return std::max(lhsVal, rhsVal);
    if (hasLhs)
      return lhsVal;
    if (hasRhs)
      return rhsVal;

    /// Recurse for nested minui
    auto lhsExtracted = extractChunkSizeFromHint(minOp.getLhs(), depth + 1);
    auto rhsExtracted = extractChunkSizeFromHint(minOp.getRhs(), depth + 1);
    if (lhsExtracted && rhsExtracted)
      return std::max(*lhsExtracted, *rhsExtracted);
    if (lhsExtracted)
      return lhsExtracted;
    if (rhsExtracted)
      return rhsExtracted;
  }

  /// Case 4: addi pattern - stencil halo adjustment
  /// For stencil patterns, chunkSize = addi(baseChunkSize, haloAdjustment)
  /// where haloAdjustment is a small constant (e.g., 2 for i-1, i, i+1 pattern)
  /// We want to extract baseChunkSize, which is the actual partition chunk size
  if (auto addOp = sizeHint.getDefiningOp<arith::AddIOp>()) {
    int64_t lhsVal = 0, rhsVal = 0;
    bool hasLhsConst = ValueUtils::getConstantIndex(addOp.getLhs(), lhsVal);
    bool hasRhsConst = ValueUtils::getConstantIndex(addOp.getRhs(), rhsVal);

    /// If one operand is a small constant (halo adjustment), recurse on the
    /// other
    if (hasRhsConst && std::abs(rhsVal) <= 16) {
      /// rhsVal is halo adjustment, lhs is base chunk size
      return extractChunkSizeFromHint(addOp.getLhs(), depth + 1);
    }
    if (hasLhsConst && std::abs(lhsVal) <= 16) {
      /// lhsVal is halo adjustment, rhs is base chunk size
      return extractChunkSizeFromHint(addOp.getRhs(), depth + 1);
    }

    /// Both constants or both large - try both sides
    auto lhsExtracted = extractChunkSizeFromHint(addOp.getLhs(), depth + 1);
    auto rhsExtracted = extractChunkSizeFromHint(addOp.getRhs(), depth + 1);
    if (lhsExtracted)
      return lhsExtracted;
    if (rhsExtracted)
      return rhsExtracted;
  }

  return std::nullopt;
}

/// Extract chunk size INCLUDING any halo adjustments for allocation sizing.
/// Unlike extractChunkSizeFromHint which extracts BASE size for loop bound
/// matching, this function preserves stencil halo adjustments since allocations
/// need to be large enough to hold the full adjusted size.
///
/// Examples:
///   minui(%remaining, %c64) -> 64
///   addi(minui(%remaining, %c64), %c2) -> 66 (base + halo)
std::optional<int64_t> extractChunkSizeForAllocation(Value sizeHint,
                                                     int depth) {
  if (!sizeHint || depth > 4)
    return std::nullopt;

  /// Case 1: Direct constant
  int64_t val;
  if (ValueUtils::getConstantIndex(sizeHint, val))
    return val;

  /// Case 2: minui pattern - return the larger constant (nominal size)
  if (auto minOp = sizeHint.getDefiningOp<arith::MinUIOp>()) {
    int64_t lhsVal = 0, rhsVal = 0;
    bool hasLhs = ValueUtils::getConstantIndex(minOp.getLhs(), lhsVal);
    bool hasRhs = ValueUtils::getConstantIndex(minOp.getRhs(), rhsVal);

    if (hasLhs && hasRhs)
      return std::max(lhsVal, rhsVal);
    if (hasLhs)
      return lhsVal;
    if (hasRhs)
      return rhsVal;

    /// Recurse for nested minui
    auto lhsExtracted =
        extractChunkSizeForAllocation(minOp.getLhs(), depth + 1);
    auto rhsExtracted =
        extractChunkSizeForAllocation(minOp.getRhs(), depth + 1);
    if (lhsExtracted && rhsExtracted)
      return std::max(*lhsExtracted, *rhsExtracted);
    if (lhsExtracted)
      return lhsExtracted;
    if (rhsExtracted)
      return rhsExtracted;
  }

  /// Case 3: addi pattern - INCLUDE the halo adjustment for allocation
  /// For stencil patterns: chunkSize = addi(baseChunkSize, haloAdjustment)
  /// Allocation needs full size: base + halo
  if (auto addOp = sizeHint.getDefiningOp<arith::AddIOp>()) {
    int64_t lhsVal = 0, rhsVal = 0;
    bool hasLhsConst = ValueUtils::getConstantIndex(addOp.getLhs(), lhsVal);
    bool hasRhsConst = ValueUtils::getConstantIndex(addOp.getRhs(), rhsVal);

    /// If one operand is a small constant (halo), add it to the extracted base
    if (hasRhsConst && std::abs(rhsVal) <= 16) {
      auto baseSize = extractChunkSizeForAllocation(addOp.getLhs(), depth + 1);
      if (baseSize)
        return *baseSize + rhsVal; /// base + halo
    }
    if (hasLhsConst && std::abs(lhsVal) <= 16) {
      auto baseSize = extractChunkSizeForAllocation(addOp.getRhs(), depth + 1);
      if (baseSize)
        return *baseSize + lhsVal; /// base + halo
    }

    /// Both operands non-constant - try both sides
    auto lhsExtracted =
        extractChunkSizeForAllocation(addOp.getLhs(), depth + 1);
    auto rhsExtracted =
        extractChunkSizeForAllocation(addOp.getRhs(), depth + 1);
    if (lhsExtracted)
      return lhsExtracted;
    if (rhsExtracted)
      return rhsExtracted;
  }

  return std::nullopt;
}

/// Extract original size from (N * scale) / scale pattern.
/// Common in malloc size calculations: malloc(N * sizeof(T)) / sizeof(T) -> N.
Value extractOriginalSize(Value numerator, Value denominator,
                          OpBuilder &builder, Location loc) {
  Value stripped = ValueUtils::stripNumericCasts(numerator);
  if (auto mul = stripped.getDefiningOp<arith::MulIOp>()) {
    Value lhs = mul.getLhs();
    Value rhs = mul.getRhs();
    if (scalesAreEquivalent(lhs, denominator))
      return ValueUtils::castToIndex(ValueUtils::stripNumericCasts(rhs),
                                     builder, loc);
    if (scalesAreEquivalent(rhs, denominator))
      return ValueUtils::castToIndex(ValueUtils::stripNumericCasts(lhs),
                                     builder, loc);
  }
  return Value();
}

void collectWhileBounds(Value cond, Value iterArg, SmallVector<Value> &bounds) {
  if (!cond)
    return;
  cond = ValueUtils::stripNumericCasts(cond);
  if (auto andOp = cond.getDefiningOp<arith::AndIOp>()) {
    collectWhileBounds(andOp.getLhs(), iterArg, bounds);
    collectWhileBounds(andOp.getRhs(), iterArg, bounds);
    return;
  }
  if (auto cmp = cond.getDefiningOp<arith::CmpIOp>()) {
    auto lhs = ValueUtils::stripNumericCasts(cmp.getLhs());
    auto rhs = ValueUtils::stripNumericCasts(cmp.getRhs());
    auto pred = cmp.getPredicate();
    auto isLessPred =
        pred == arith::CmpIPredicate::slt || pred == arith::CmpIPredicate::ult;
    if (lhs == iterArg && isLessPred) {
      bounds.push_back(cmp.getRhs());
      return;
    }
    if (rhs == iterArg && (pred == arith::CmpIPredicate::sgt ||
                           pred == arith::CmpIPredicate::ugt)) {
      bounds.push_back(cmp.getLhs());
      return;
    }
  }
}

///===----------------------------------------------------------------------===///
/// Attribute Transfer Utilities
///===----------------------------------------------------------------------===///

void transferAttributes(Operation *src, Operation *dst,
                        ArrayRef<StringRef> excludes) {
  /// Build set of attributes to exclude
  llvm::SmallDenseSet<StringRef> excludeSet;
  /// Always exclude auto-generated MLIR attributes
  excludeSet.insert("operandSegmentSizes");
  excludeSet.insert("resultSegmentSizes");
  /// Exclude builder-set attributes for ARTS ops
  excludeSet.insert("mode");
  /// Add any additional excludes
  for (StringRef name : excludes)
    excludeSet.insert(name);

  /// Transfer all non-excluded attributes
  for (NamedAttribute attr : src->getAttrs()) {
    if (!excludeSet.contains(attr.getName().getValue()))
      dst->setAttr(attr.getName(), attr.getValue());
  }
}

} // namespace arts
} // namespace mlir
