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
/// Value Analysis Utilities
///===----------------------------------------------------------------------===///

/// Checks if the given value is a constant, including constant-like operations
/// such as constant indices and constant operations.
bool isValueConstant(Value val) {
  Operation *defOp = val.getDefiningOp();
  if (!defOp)
    return false;

  if (defOp->hasTrait<OpTrait::ConstantLike>())
    return true;

  if (isa<arith::ConstantIndexOp>(defOp) || isa<arith::ConstantOp>(defOp))
    return true;
  return false;
};

/// Attempts to extract a constant index value from the given value, supporting
/// both constant index operations and constant operations with integer
/// attributes.
bool getConstantIndex(Value v, int64_t &out) {
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
bool isNonZeroIndex(Value v) {
  if (!v)
    return false;
  if (auto c = v.getDefiningOp<arith::ConstantIndexOp>())
    return c.value() != 0;
  return true;
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
  SmallVector<int64_t> elementShape(elementSizes.size(), ShapedType::kDynamic);
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
  Value a = stripNumericCasts(lhs);
  Value b = stripNumericCasts(rhs);
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
/// EDT Analysis Utilities
///===----------------------------------------------------------------------===///

/// Determines if a value is invariant within the given EDT region, meaning it
/// is defined outside the region or not modified within it.
bool isInvariantInEdt(Region &edtRegion, Value value) {
  SmallPtrSet<Value, 16> visited;

  std::function<bool(Value)> definedInvariant = [&](Value v) -> bool {
    if (!v)
      return false;
    if (!visited.insert(v).second)
      return true;

    if (isValueConstant(v))
      return true;

    if (auto blockArg = v.dyn_cast<BlockArgument>()) {
      Block *owner = blockArg.getOwner();
      if (!owner)
        return false;
      Region *ownerRegion = owner->getParent();
      if (!ownerRegion)
        return false;

      /// Entry block arguments of the EDT region are considered invariant
      /// because their defining value is outside the region.
      if (ownerRegion == &edtRegion && owner == &edtRegion.front())
        return true;

      /// Block arguments that belong to regions outside of this EDT are also
      /// treated as invariant inputs.
      if (!edtRegion.isAncestor(ownerRegion))
        return true;

      return false;
    }

    Operation *defOp = v.getDefiningOp();
    if (!defOp)
      return false;

    if (!edtRegion.isAncestor(defOp->getParentRegion()))
      return true;

    if (isa<arith::ConstantIndexOp, arith::ConstantIntOp>(defOp))
      return true;

    if (isa<arith::AddIOp, arith::SubIOp, arith::MulIOp, arith::DivSIOp,
            arith::DivUIOp, arith::IndexCastOp, arith::ExtSIOp, arith::ExtUIOp,
            arith::TruncIOp>(defOp)) {
      for (Value operand : defOp->getOperands())
        if (!definedInvariant(operand))
          return false;
      return true;
    }

    return false;
  };

  if (!definedInvariant(value))
    return false;

  auto isPointerLike = [](Type type) {
    return type.isa<MemRefType, UnrankedMemRefType>();
  };

  if (!isPointerLike(value.getType()))
    return true;

  for (Operation *user : value.getUsers()) {
    if (!edtRegion.isAncestor(user->getParentRegion()))
      continue;

    if (auto store = dyn_cast<memref::StoreOp>(user))
      if (store.getMemref() == value)
        return false;

    if (auto atomicRmw = dyn_cast<memref::AtomicRMWOp>(user))
      if (atomicRmw.getMemref() == value)
        return false;
  }

  return true;
}

/// Checks if the target operation is reachable from the source operation in
/// the EDT control flow graph.
bool isReachable(Operation *source, Operation *target) {
  /// Early exit if either pointer is null or both are the same.
  if (!source || !target)
    return false;
  if (source == target)
    return true;

  /// If both operations are in the same block, check their order.
  if (source->getBlock() == target->getBlock())
    return source->isBeforeInBlock(target);

  /// Get the EDT parent for both operations. This acts as our upper limit.
  auto srcEdt = source->getParentOfType<EdtOp>();
  auto tgtEdt = target->getParentOfType<EdtOp>();
  if (srcEdt != tgtEdt)
    return false;

  /// Traverse up the parent chain simultaneously but stop once the EDT parent
  /// is reached.
  Operation *src = source;
  Operation *tgt = target;
  while (true) {
    if (src->getBlock() == tgt->getBlock())
      return src->isBeforeInBlock(tgt);
    if (src == srcEdt && tgt == tgtEdt)
      break;
    if (src->getParentOp() != srcEdt)
      src = src->getParentOp();
    if (tgt->getParentOp() != tgtEdt)
      tgt = tgt->getParentOp();
  }
  return false;
}

/// Finds the EdtOp that uses a DbAcquireOp and returns the corresponding block
/// argument.
std::pair<EdtOp, BlockArgument>
getEdtBlockArgumentForAcquire(DbAcquireOp acquireOp) {
  /// Find the EDT that uses this acquire's pointer result
  EdtOp edtUser = nullptr;
  Value operandValue = nullptr;

  for (auto &use : acquireOp->getUses()) {
    Operation *userOp = use.getOwner();
    if (auto edtOp = dyn_cast<EdtOp>(userOp)) {
      edtUser = edtOp;
      operandValue = edtOp->getOperand(use.getOperandNumber());
      break;
    }
  }

  if (!edtUser || !operandValue)
    return {nullptr, nullptr};

  /// Find the operand Value in the dependencies range
  /// The index within dependencies equals the block argument index
  ValueRange deps = edtUser.getDependencies();
  auto depIt = std::find(deps.begin(), deps.end(), operandValue);
  if (depIt == deps.end())
    return {nullptr, nullptr};

  unsigned blockArgIdx = std::distance(deps.begin(), depIt);

  /// Get the block argument
  Block &body = edtUser.getRegion().front();
  if (blockArgIdx >= body.getNumArguments())
    return {nullptr, nullptr};

  BlockArgument blockArg = body.getArgument(blockArgIdx);
  return {edtUser, blockArg};
}

/// Internal helper to trace the underlying value through various operations,
/// with cycle detection to avoid infinite recursion.
static Value getUnderlyingValueImpl(Value v, SmallPtrSet<Value, 8> &visited) {
  if (!v)
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
        return getUnderlyingValueImpl(operand, visited);
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
      return getUnderlyingValueImpl(dbAcquire.getSourcePtr(), visited);
    else if (auto dbGep = dyn_cast<DbGepOp>(op))
      return getUnderlyingValueImpl(dbGep.getBasePtr(), visited);
    else if (auto dbControl = dyn_cast<DbControlOp>(op))
      return getUnderlyingValueImpl(dbControl.getPtr(), visited);
    else if (auto subview = dyn_cast<memref::SubViewOp>(op))
      return getUnderlyingValueImpl(subview.getSource(), visited);
    else if (auto castOp = dyn_cast<memref::CastOp>(op))
      return getUnderlyingValueImpl(castOp.getSource(), visited);
    else if (auto p2m = dyn_cast<polygeist::Pointer2MemrefOp>(op))
      return getUnderlyingValueImpl(p2m.getSource(), visited);
    else if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(op))
      return getUnderlyingValueImpl(m2p.getSource(), visited);
    else if (isa<arts::UndefOp>(op))
      return nullptr;
    else
      return nullptr;
  } else {
    /// Value has no defining operation and is not a block argument
    return nullptr;
  }
}

///===----------------------------------------------------------------------===///
/// Underlying Value Tracing Utilities
///===----------------------------------------------------------------------===///

/// Strip numeric cast operations to find the underlying value.
/// Traverses through index casts, sign/zero extensions, and truncations.
Value stripNumericCasts(Value value) {
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

/// Traces the underlying root allocation value for the given value, unwinding
/// through various MLIR operations.
Value getUnderlyingValue(Value v) {
  SmallPtrSet<Value, 8> visited;
  return getUnderlyingValueImpl(v, visited);
}

/// Retrieves the underlying operation that defines the root value for the
/// given value.
Operation *getUnderlyingOperation(Value v) {
  Value underlyingValue = getUnderlyingValue(v);
  if (!underlyingValue)
    return nullptr;

  /// If the underlying value is a result of an operation, return that operation
  if (auto definingOp = underlyingValue.getDefiningOp())
    return definingOp;

  return nullptr;
}

/// Finds the datablock-related operation (DbAllocOp or DbAcquireOp) associated
/// with the given value.
/// The depth parameter prevents infinite recursion in circular acquire chains.
Operation *getUnderlyingDb(Value v, unsigned depth) {
  if (!v)
    return nullptr;

  /// Prevent infinite recursion from circular acquire chains
  if (depth > 20) {
    ARTS_WARN("getUnderlyingDb exceeded depth limit of 20 "
              << "(possible circular acquire chain)");
    return nullptr;
  }

  /// Directly return the DbAcquireOp or DbAllocOp if present
  if (auto acq = v.getDefiningOp<DbAcquireOp>())
    return acq.getOperation();
  if (auto alloc = v.getDefiningOp<DbAllocOp>())
    return alloc.getOperation();
  if (auto dbLoad = v.getDefiningOp<DbRefOp>())
    return getUnderlyingDb(dbLoad.getSource(), depth + 1);

  /// If it's a block argument of an EDT, map to the corresponding operand and
  /// recurse. Block arguments correspond to dependencies
  if (auto blockArg = v.dyn_cast<BlockArgument>()) {
    Block *block = blockArg.getOwner();
    Operation *owner = block->getParentOp();
    if (auto edt = dyn_cast<EdtOp>(owner)) {
      unsigned argIndex = blockArg.getArgNumber();
      /// Block arguments correspond to dependencies
      ValueRange deps = edt.getDependencies();
      if (argIndex < deps.size()) {
        Value operand = deps[argIndex];
        return getUnderlyingDb(operand, depth + 1);
      }
    }
  }

  /// Peel through common view/cast/gep nodes to reach DB ops
  if (Operation *def = v.getDefiningOp()) {
    if (auto gep = dyn_cast<DbGepOp>(def))
      return getUnderlyingDb(gep.getBasePtr(), depth + 1);
    if (auto castOp = dyn_cast<memref::CastOp>(def))
      return getUnderlyingDb(castOp.getSource(), depth + 1);
    if (auto subview = dyn_cast<memref::SubViewOp>(def))
      return getUnderlyingDb(subview.getSource(), depth + 1);
  }

  /// As a last resort, try underlying root op if it's a DB op
  if (Operation *root = getUnderlyingOperation(v)) {
    if (isa<DbAcquireOp, DbAllocOp>(root))
      return root;
  }

  return nullptr;
}

/// Finds the DbAllocOp associated with the given value.
Operation *getUnderlyingDbAlloc(Value v) {
  Operation *underlyingDb = getUnderlyingDb(v);
  if (!underlyingDb)
    return nullptr;
  if (isa<DbAllocOp>(underlyingDb))
    return underlyingDb;
  auto dbAcquire = dyn_cast<DbAcquireOp>(underlyingDb);
  assert(dbAcquire);
  return getUnderlyingDbAlloc(dbAcquire.getSourcePtr());
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
// Type Casting and Conversion Utilities
///===----------------------------------------------------------------------===///

/// Cast a value to index type if needed.
Value castToIndex(Value value, OpBuilder &builder, Location loc) {
  if (!value)
    return value;
  if (value.getType().isIndex())
    return value;
  if (value.getType().isIntOrIndex())
    return builder.create<arith::IndexCastOp>(loc, builder.getIndexType(),
                                              value);
  return value;
}

///===----------------------------------------------------------------------===///
// Pattern Recognition and Analysis Utilities
///===----------------------------------------------------------------------===///

/// Extract original size from (N * scale) / scale pattern.
/// Common in malloc size calculations: malloc(N * sizeof(T)) / sizeof(T) -> N.
Value extractOriginalSize(Value numerator, Value denominator,
                          OpBuilder &builder, Location loc) {
  Value stripped = stripNumericCasts(numerator);
  if (auto mul = stripped.getDefiningOp<arith::MulIOp>()) {
    Value lhs = mul.getLhs();
    Value rhs = mul.getRhs();
    if (scalesAreEquivalent(lhs, denominator))
      return castToIndex(stripNumericCasts(rhs), builder, loc);
    if (scalesAreEquivalent(rhs, denominator))
      return castToIndex(stripNumericCasts(lhs), builder, loc);
  }
  return Value();
}

/// Extract array index from byte offset pattern: bytes = (index * elemBytes).
/// Handles common patterns where GEP indices are scaled by element byte size.
/// Returns the logical array index or null Value if pattern doesn't match.
Value extractArrayIndexFromByteOffset(Value byteOffset, Type elemType) {
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

Value ensureIndexType(Value value, OpBuilder &builder, Location loc) {
  if (!value)
    return Value();
  if (value.getType().isa<IndexType>())
    return value;
  if (auto intTy = value.getType().dyn_cast<IntegerType>())
    return builder.create<arith::IndexCastOp>(loc, builder.getIndexType(),
                                              value);
  return Value();
}

void collectWhileBounds(Value cond, Value iterArg, SmallVector<Value> &bounds) {
  if (!cond)
    return;
  cond = stripNumericCasts(cond);
  if (auto andOp = cond.getDefiningOp<arith::AndIOp>()) {
    collectWhileBounds(andOp.getLhs(), iterArg, bounds);
    collectWhileBounds(andOp.getRhs(), iterArg, bounds);
    return;
  }
  if (auto cmp = cond.getDefiningOp<arith::CmpIOp>()) {
    auto lhs = stripNumericCasts(cmp.getLhs());
    auto rhs = stripNumericCasts(cmp.getRhs());
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

} // namespace arts
} // namespace mlir
