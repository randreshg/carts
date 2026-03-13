///==========================================================================///
/// File: Utils.cpp
/// Defines utility functions for the ARTS dialect.
///==========================================================================///
#include "arts/utils/Utils.h"
#include "arts/Dialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/CSE.h"
#include "mlir/Transforms/RegionUtils.h"
#include "llvm/ADT/StringSet.h"
#include <algorithm>
#include <cassert>

/// Debug
#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(arts_utils);
[[maybe_unused]] static const auto *kArtsUtilsDebugType = ARTS_DEBUG_TYPE_STR;

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// Constant Index Creation Utilities
///===----------------------------------------------------------------------===///

Value createConstantIndex(OpBuilder &builder, Location loc, int64_t val) {
  return builder.create<arith::ConstantIndexOp>(loc, val);
}

Value createZeroIndex(OpBuilder &builder, Location loc) {
  return createConstantIndex(builder, loc, 0);
}

Value createOneIndex(OpBuilder &builder, Location loc) {
  return createConstantIndex(builder, loc, 1);
}

///===----------------------------------------------------------------------===///
/// ARTS Runtime Query Utilities
///===----------------------------------------------------------------------===///

/// Known ARTS runtime query function names that are safe for partitioning.
/// These functions return deterministic values within an EDT context.
static const llvm::StringSet<> ArtsRuntimeQueries = {
    "arts_get_current_node", "arts_get_total_nodes", "arts_get_current_worker",
    "arts_get_total_workers"};

bool isArtsRuntimeQuery(Value val) {
  if (!val)
    return false;

  val = ValueAnalysis::stripNumericCasts(val);
  Operation *defOp = val.getDefiningOp();
  if (!defOp)
    return false;

  /// Check for ARTS dialect ops (before lowering to func::CallOp)
  if (isa<RuntimeQueryOp>(defOp))
    return true;

  /// Check for func::CallOp (after lowering)
  if (auto callOp = dyn_cast<func::CallOp>(defOp))
    return ArtsRuntimeQueries.contains(callOp.getCallee());

  return false;
}

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
