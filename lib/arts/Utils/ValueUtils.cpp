///==========================================================================///
/// File: ValueUtils.cpp
///
/// Defines utility functions for working with Index and Constant values.
///==========================================================================///

#include "arts/Utils/ValueUtils.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/CSE.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <algorithm>
#include <cassert>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// Value Cloning Utilities
///===----------------------------------------------------------------------===///

bool ValueUtils::canCloneOperation(
    Operation *op, bool allowMemoryEffectFree,
    llvm::function_ref<bool(Operation *)> extraAllowed) {
  if (!op)
    return false;
  if (op->hasTrait<OpTrait::ConstantLike>())
    return true;
  if (allowMemoryEffectFree && isMemoryEffectFree(op))
    return true;
  return extraAllowed ? extraAllowed(op) : false;
}

bool ValueUtils::cloneValuesIntoRegion(
    llvm::SetVector<Value> &values, Region *targetRegion, IRMapping &mapper,
    OpBuilder &builder, bool allowMemoryEffectFree,
    llvm::function_ref<bool(Operation *)> extraAllowed) {
  SmallVector<Value> remaining(values.begin(), values.end());
  bool progress = true;

  while (progress && !remaining.empty()) {
    progress = false;
    SmallVector<Value> nextRemaining;

    for (Value val : remaining) {
      if (mapper.contains(val)) {
        progress = true;
        continue;
      }

      Operation *defOp = val.getDefiningOp();
      if (!defOp) {
        nextRemaining.push_back(val);
        continue;
      }

      if (!canCloneOperation(defOp, allowMemoryEffectFree, extraAllowed)) {
        nextRemaining.push_back(val);
        continue;
      }

      bool allOperandsAvailable = true;
      for (Value operand : defOp->getOperands()) {
        if (mapper.contains(operand))
          continue;
        if (auto blockArg = operand.dyn_cast<BlockArgument>()) {
          Region *ownerRegion = blockArg.getOwner()->getParent();
          if (targetRegion->isAncestor(ownerRegion) ||
              ownerRegion->isAncestor(targetRegion))
            continue;
        }
        if (Operation *opDef = operand.getDefiningOp()) {
          Region *ownerRegion = opDef->getParentRegion();
          if (targetRegion->isAncestor(ownerRegion) ||
              ownerRegion->isAncestor(targetRegion))
            continue;
        }
        allOperandsAvailable = false;
        break;
      }

      if (!allOperandsAvailable) {
        nextRemaining.push_back(val);
        continue;
      }

      Operation *clonedOp = builder.clone(*defOp, mapper);
      for (auto [oldResult, newResult] :
           llvm::zip(defOp->getResults(), clonedOp->getResults())) {
        mapper.map(oldResult, newResult);
      }
      progress = true;
    }

    remaining = std::move(nextRemaining);
  }

  return remaining.empty();
}

///===----------------------------------------------------------------------===///
/// Constant Value Analysis
///===----------------------------------------------------------------------===///

/// Maximum recursion depth for value tracing to prevent stack overflow
static constexpr unsigned kMaxTraceDepth = 64;

bool ValueUtils::isValueConstant(Value val) {
  Operation *defOp = val.getDefiningOp();
  if (!defOp)
    return false;
  return defOp->hasTrait<OpTrait::ConstantLike>();
}

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

bool ValueUtils::isNonZeroIndex(Value v) {
  if (!v)
    return false;
  if (auto c = v.getDefiningOp<arith::ConstantIndexOp>())
    return c.value() != 0;
  return true;
}

std::optional<int64_t> ValueUtils::getConstantValue(Value v) {
  int64_t val;
  if (getConstantIndex(v, val))
    return val;
  return std::nullopt;
}

std::optional<int64_t> ValueUtils::tryFoldConstantIndex(Value v,
                                                        unsigned depth) {
  if (!v || depth > 8)
    return std::nullopt;

  int64_t val;
  if (getConstantIndex(v, val))
    return val;

  if (auto cast = v.getDefiningOp<arith::IndexCastOp>()) {
    return tryFoldConstantIndex(cast.getIn(), depth + 1);
  }

  if (auto add = v.getDefiningOp<arith::AddIOp>()) {
    auto lhs = tryFoldConstantIndex(add.getLhs(), depth + 1);
    auto rhs = tryFoldConstantIndex(add.getRhs(), depth + 1);
    if (lhs && rhs)
      return *lhs + *rhs;
    return std::nullopt;
  }

  if (auto sub = v.getDefiningOp<arith::SubIOp>()) {
    auto lhs = tryFoldConstantIndex(sub.getLhs(), depth + 1);
    auto rhs = tryFoldConstantIndex(sub.getRhs(), depth + 1);
    if (lhs && rhs)
      return *lhs - *rhs;
    return std::nullopt;
  }

  if (auto mul = v.getDefiningOp<arith::MulIOp>()) {
    auto lhs = tryFoldConstantIndex(mul.getLhs(), depth + 1);
    auto rhs = tryFoldConstantIndex(mul.getRhs(), depth + 1);
    if (lhs && rhs)
      return *lhs * *rhs;
    return std::nullopt;
  }

  if (auto div = v.getDefiningOp<arith::DivUIOp>()) {
    auto lhs = tryFoldConstantIndex(div.getLhs(), depth + 1);
    auto rhs = tryFoldConstantIndex(div.getRhs(), depth + 1);
    if (lhs && rhs && *rhs != 0) {
      uint64_t ulhs = static_cast<uint64_t>(*lhs);
      uint64_t urhs = static_cast<uint64_t>(*rhs);
      return static_cast<int64_t>(ulhs / urhs);
    }
    return std::nullopt;
  }

  if (auto div = v.getDefiningOp<arith::DivSIOp>()) {
    auto lhs = tryFoldConstantIndex(div.getLhs(), depth + 1);
    auto rhs = tryFoldConstantIndex(div.getRhs(), depth + 1);
    if (lhs && rhs && *rhs != 0)
      return *lhs / *rhs;
    return std::nullopt;
  }

  if (auto max = v.getDefiningOp<arith::MaxUIOp>()) {
    auto lhs = tryFoldConstantIndex(max.getLhs(), depth + 1);
    auto rhs = tryFoldConstantIndex(max.getRhs(), depth + 1);
    if (lhs && rhs)
      return std::max<uint64_t>(static_cast<uint64_t>(*lhs),
                                static_cast<uint64_t>(*rhs));
    return std::nullopt;
  }

  if (auto min = v.getDefiningOp<arith::MinUIOp>()) {
    auto lhs = tryFoldConstantIndex(min.getLhs(), depth + 1);
    auto rhs = tryFoldConstantIndex(min.getRhs(), depth + 1);
    if (lhs && rhs)
      return std::min<uint64_t>(static_cast<uint64_t>(*lhs),
                                static_cast<uint64_t>(*rhs));
    return std::nullopt;
  }

  return std::nullopt;
}

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

bool ValueUtils::isConstantEqual(Value v, int64_t val) {
  if (auto cst = getConstantValue(v))
    return *cst == val;
  if (auto cst = getConstantFloat(v))
    return *cst == static_cast<double>(val);
  return false;
}

bool ValueUtils::isZeroConstant(Value v) { return isConstantEqual(v, 0); }

bool ValueUtils::isOneConstant(Value v) { return isConstantEqual(v, 1); }

bool ValueUtils::sameValue(Value a, Value b) {
  a = stripNumericCasts(a);
  b = stripNumericCasts(b);
  if (a == b)
    return true;
  auto aConst = tryFoldConstantIndex(a);
  auto bConst = tryFoldConstantIndex(b);
  return aConst && bConst && *aConst == *bConst;
}

Value ValueUtils::stripClampOne(Value v) {
  Value cur = stripNumericCasts(v);
  while (auto maxOp = cur.getDefiningOp<arith::MaxUIOp>()) {
    Value lhs = stripNumericCasts(maxOp.getLhs());
    Value rhs = stripNumericCasts(maxOp.getRhs());
    if (isOneConstant(lhs)) {
      cur = rhs;
      continue;
    }
    if (isOneConstant(rhs)) {
      cur = lhs;
      continue;
    }
    break;
  }
  return cur;
}

Value ValueUtils::stripSelectClamp(Value value, int maxDepth) {
  if (!value || maxDepth <= 0)
    return value;
  value = stripNumericCasts(value);

  auto selectOp = value.getDefiningOp<arith::SelectOp>();
  if (!selectOp)
    return value;

  Value trueVal = stripNumericCasts(selectOp.getTrueValue());
  Value falseVal = stripNumericCasts(selectOp.getFalseValue());

  if (auto cmp = selectOp.getCondition().getDefiningOp<arith::CmpIOp>()) {
    Value lhs = stripNumericCasts(cmp.getLhs());
    Value rhs = stripNumericCasts(cmp.getRhs());
    auto pred = cmp.getPredicate();

    bool isClampCmp = pred == arith::CmpIPredicate::slt ||
                      pred == arith::CmpIPredicate::ult ||
                      pred == arith::CmpIPredicate::sgt ||
                      pred == arith::CmpIPredicate::ugt;
    if (isClampCmp) {
      /// Match min/max clamp canonical form:
      ///   cmp(lhs, rhs) + select(cond, rhs, lhs)
      if (sameValue(trueVal, rhs) && sameValue(falseVal, lhs))
        return stripSelectClamp(lhs, maxDepth - 1);
      /// Also handle the mirrored operand form.
      if (sameValue(trueVal, lhs) && sameValue(falseVal, rhs))
        return stripSelectClamp(rhs, maxDepth - 1);
    }
  }

  /// Fallback: select(const, x) / select(x, const) behaves like a guarded
  /// clamp in many lowered index expressions; keep the non-constant side.
  auto trueConst = getConstantValue(trueVal);
  auto falseConst = getConstantValue(falseVal);
  if (trueConst && !falseConst)
    return stripSelectClamp(falseVal, maxDepth - 1);
  if (falseConst && !trueConst)
    return stripSelectClamp(trueVal, maxDepth - 1);

  return value;
}

bool ValueUtils::areValuesEquivalent(Value a, Value b, int depth) {
  if (!a || !b)
    return false;
  if (a == b)
    return true;
  if (depth > 6)
    return false;

  a = stripNumericCasts(a);
  b = stripNumericCasts(b);
  if (a == b)
    return true;

  auto ca = getConstantValue(a);
  auto cb = getConstantValue(b);
  if (ca && cb)
    return *ca == *cb;

  Operation *opA = a.getDefiningOp();
  Operation *opB = b.getDefiningOp();
  if (!opA || !opB)
    return false;
  if (opA->getName() != opB->getName())
    return false;
  if (opA->getNumOperands() != opB->getNumOperands())
    return false;

  auto eq = [&](Value x, Value y) {
    return areValuesEquivalent(x, y, depth + 1);
  };

  if (isa<arith::AddIOp, arith::MulIOp, arith::MaxUIOp, arith::MinUIOp>(opA)) {
    if (opA->getNumOperands() == 2) {
      Value a0 = opA->getOperand(0);
      Value a1 = opA->getOperand(1);
      Value b0 = opB->getOperand(0);
      Value b1 = opB->getOperand(1);
      return (eq(a0, b0) && eq(a1, b1)) || (eq(a0, b1) && eq(a1, b0));
    }
  }

  for (unsigned i = 0; i < opA->getNumOperands(); ++i) {
    if (!eq(opA->getOperand(i), opB->getOperand(i)))
      return false;
  }
  return true;
}

bool ValueUtils::isConstantAtLeastOne(Value v) {
  if (!v)
    return false;
  v = stripNumericCasts(v);
  int64_t val = 0;
  if (getConstantIndex(v, val))
    return val >= 1;
  if (auto cst = v.getDefiningOp<arith::ConstantOp>()) {
    if (auto intAttr = dyn_cast<IntegerAttr>(cst.getValue()))
      return intAttr.getInt() >= 1;
  }
  return false;
}

bool ValueUtils::isProvablyNonZero(Value v, unsigned depth) {
  if (!v || depth > 4)
    return false;
  v = stripNumericCasts(v);
  if (isConstantAtLeastOne(v))
    return true;
  if (auto maxui = v.getDefiningOp<arith::MaxUIOp>()) {
    return isProvablyNonZero(maxui.getLhs(), depth + 1) ||
           isProvablyNonZero(maxui.getRhs(), depth + 1);
  }
  if (auto maxsi = v.getDefiningOp<arith::MaxSIOp>()) {
    return isProvablyNonZero(maxsi.getLhs(), depth + 1) ||
           isProvablyNonZero(maxsi.getRhs(), depth + 1);
  }
  return false;
}

///===----------------------------------------------------------------------===///
/// Value Type Conversion and Casting
///===----------------------------------------------------------------------===///

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

Value ValueUtils::ensureIndexType(Value value, OpBuilder &builder,
                                  Location loc) {
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
           arith::TruncIOp, arith::MinSIOp, arith::MinUIOp, arith::MaxSIOp,
           arith::MaxUIOp, arith::SelectOp, arith::CmpIOp, arith::CmpFOp>(op))
    return false;

  for (Value operand : op->getOperands())
    if (dependsOn(operand, base, depth + 1))
      return true;

  return false;
}

std::optional<int64_t> ValueUtils::getOffsetStride(Value idx, Value base,
                                                   int depth) {
  if (!idx || !base || depth > 8)
    return std::nullopt;

  idx = stripNumericCasts(idx);
  base = stripNumericCasts(base);

  if (idx == base)
    return int64_t{1};

  if (auto addOp = idx.getDefiningOp<arith::AddIOp>()) {
    Value lhs = addOp.getLhs();
    Value rhs = addOp.getRhs();
    bool lhsDep = dependsOn(lhs, base, depth + 1);
    bool rhsDep = dependsOn(rhs, base, depth + 1);
    if (lhsDep && !rhsDep)
      return getOffsetStride(lhs, base, depth + 1);
    if (rhsDep && !lhsDep)
      return getOffsetStride(rhs, base, depth + 1);
    return std::nullopt;
  }

  if (auto subOp = idx.getDefiningOp<arith::SubIOp>()) {
    Value lhs = subOp.getLhs();
    Value rhs = subOp.getRhs();
    bool lhsDep = dependsOn(lhs, base, depth + 1);
    bool rhsDep = dependsOn(rhs, base, depth + 1);
    if (lhsDep && !rhsDep)
      return getOffsetStride(lhs, base, depth + 1);
    return std::nullopt;
  }

  if (auto mulOp = idx.getDefiningOp<arith::MulIOp>()) {
    Value lhs = mulOp.getLhs();
    Value rhs = mulOp.getRhs();
    int64_t constVal = 0;
    if (getConstantIndex(lhs, constVal) && dependsOn(rhs, base, depth + 1))
      return constVal;
    if (getConstantIndex(rhs, constVal) && dependsOn(lhs, base, depth + 1))
      return constVal;
  }

  return std::nullopt;
}

static bool isDerivedFromPtrImpl(Value value, Value source,
                                 SmallPtrSetImpl<Value> &visited,
                                 unsigned depth) {
  if (depth > kMaxTraceDepth)
    return false;
  if (value == source)
    return true;
  if (!visited.insert(value).second)
    return false;

  if (auto blockArg = dyn_cast<BlockArgument>(value)) {
    if (auto edt = dyn_cast<EdtOp>(blockArg.getParentBlock()->getParentOp())) {
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

  auto trace = [&](Value v) -> bool {
    return isDerivedFromPtrImpl(v, source, visited, depth + 1);
  };

  if (auto dbAcquire = dyn_cast<DbAcquireOp>(defOp))
    return trace(dbAcquire.getSourcePtr());
  if (auto dbGep = dyn_cast<DbGepOp>(defOp))
    return trace(dbGep.getBasePtr());
  if (auto dbControl = dyn_cast<DbControlOp>(defOp))
    return trace(dbControl.getPtr());
  if (auto gepOp = dyn_cast<LLVM::GEPOp>(defOp))
    return trace(gepOp.getBase());
  if (auto ptr2memref = dyn_cast<polygeist::Pointer2MemrefOp>(defOp))
    return trace(ptr2memref.getSource());
  if (auto memref2ptr = dyn_cast<polygeist::Memref2PointerOp>(defOp))
    return trace(memref2ptr.getSource());
  if (auto subview = dyn_cast<memref::SubViewOp>(defOp))
    return trace(subview.getSource());
  if (auto cast = dyn_cast<memref::CastOp>(defOp))
    return trace(cast.getSource());
  if (auto view = dyn_cast<memref::ViewOp>(defOp))
    return trace(view.getSource());
  if (auto reinterpretCast = dyn_cast<memref::ReinterpretCastOp>(defOp))
    return trace(reinterpretCast.getSource());

  /// Fallback: trace through first operand (handles polygeist.subindex etc.)
  if (defOp->getNumOperands() > 0)
    return trace(defOp->getOperand(0));

  return false;
}

bool ValueUtils::isDerivedFromPtr(Value value, Value source) {
  SmallPtrSet<Value, 16> visited;
  return isDerivedFromPtrImpl(value, source, visited, 0);
}

std::optional<int64_t> ValueUtils::inferConstantStride(Value globalIndex,
                                                       Value elemOffset) {
  if (!globalIndex || !elemOffset)
    return std::nullopt;

  Value stripped = stripNumericCasts(globalIndex);
  auto mulOp = stripped.getDefiningOp<arith::MulIOp>();
  if (!mulOp)
    return std::nullopt;

  Value lhs = mulOp.getLhs();
  Value rhs = mulOp.getRhs();

  int64_t constVal;
  if (getConstantIndex(lhs, constVal) && dependsOn(rhs, elemOffset))
    return constVal;
  if (getConstantIndex(rhs, constVal) && dependsOn(lhs, elemOffset))
    return constVal;

  return std::nullopt;
}

/// Extract constant offset from an index expression relative to loopIV and
/// chunkOffset. E.g. chunkOffset + loopIV + 5 yields offset = 5.
std::optional<int64_t>
ValueUtils::extractConstantOffset(Value idx, Value loopIV, Value chunkOffset) {
  int64_t accumulator = 0;
  Value current = idx;

  auto isBaseValue = [&](Value v) -> bool {
    return v == loopIV || v == chunkOffset;
  };

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
    if (isBaseValue(current) || isBasePattern(current))
      return accumulator;

    if (auto addOp = current.getDefiningOp<arith::AddIOp>()) {
      Value lhs = addOp.getLhs();
      Value rhs = addOp.getRhs();

      int64_t constVal;
      if (getConstantIndex(rhs, constVal)) {
        accumulator += constVal;
        current = lhs;
      } else if (getConstantIndex(lhs, constVal)) {
        accumulator += constVal;
        current = rhs;
      } else {
        bool lhsIsBase = isBaseValue(lhs) || isBasePattern(lhs);
        bool rhsIsBase = isBaseValue(rhs) || isBasePattern(rhs);

        if (lhsIsBase && rhsIsBase)
          return accumulator;
        if (lhsIsBase)
          current = rhs;
        else if (rhsIsBase)
          current = lhs;
        else
          break;
      }
    } else if (auto subOp = current.getDefiningOp<arith::SubIOp>()) {
      int64_t constVal;
      if (getConstantIndex(subOp.getRhs(), constVal)) {
        accumulator -= constVal;
        current = subOp.getLhs();
        continue;
      }
      break;
    } else if (auto indexCast = current.getDefiningOp<arith::IndexCastOp>()) {
      current = indexCast.getIn();
      continue;
    } else {
      break;
    }
  }

  return std::nullopt;
}

Value ValueUtils::stripConstantOffset(Value value, int64_t *outConst) {
  int64_t accumulator = 0;
  Value current = value;

  if (!current) {
    if (outConst)
      *outConst = 0;
    return current;
  }

  while (current) {
    current = stripNumericCasts(current);

    if (auto addOp = current.getDefiningOp<arith::AddIOp>()) {
      int64_t constVal;
      if (getConstantIndex(addOp.getLhs(), constVal)) {
        accumulator += constVal;
        current = addOp.getRhs();
        continue;
      }
      if (getConstantIndex(addOp.getRhs(), constVal)) {
        accumulator += constVal;
        current = addOp.getLhs();
        continue;
      }
    } else if (auto subOp = current.getDefiningOp<arith::SubIOp>()) {
      int64_t constVal;
      if (getConstantIndex(subOp.getRhs(), constVal)) {
        accumulator -= constVal;
        current = subOp.getLhs();
        continue;
      }
    } else if (auto indexCast = current.getDefiningOp<arith::IndexCastOp>()) {
      current = indexCast.getIn();
      continue;
    }
    break;
  }

  if (outConst)
    *outConst = accumulator;
  return current;
}

Value ValueUtils::extractArrayIndexFromByteOffset(Value byteOffset,
                                                  Type elemType) {
  Value idxSource = byteOffset;
  while (auto castOp = idxSource.getDefiningOp<arith::IndexCastOp>())
    idxSource = castOp.getIn();

  auto mulOp = idxSource.getDefiningOp<arith::MulIOp>();
  if (!mulOp)
    return Value();

  int64_t elemSize = getElementTypeByteSize(elemType);
  if (elemSize == 0)
    return Value();

  auto isElementSizeConstant = [elemSize](Value v) -> bool {
    if (auto constIdx = v.getDefiningOp<arith::ConstantIndexOp>())
      return constIdx.value() == elemSize;
    if (auto constInt = v.getDefiningOp<arith::ConstantIntOp>())
      return constInt.value() == elemSize && constInt.getType().isInteger(64);
    return false;
  };

  if (isElementSizeConstant(mulOp.getLhs()))
    return mulOp.getRhs();
  if (isElementSizeConstant(mulOp.getRhs()))
    return mulOp.getLhs();
  return Value();
}

///===----------------------------------------------------------------------===///
/// Underlying Value Tracing
///===----------------------------------------------------------------------===///

static Value getUnderlyingValueImpl(Value v, SmallPtrSet<Value, 16> &visited,
                                    unsigned depth) {
  if (!v || depth > kMaxTraceDepth || !visited.insert(v).second)
    return nullptr;

  if (auto blockArg = dyn_cast<BlockArgument>(v)) {
    if (auto edt = dyn_cast<EdtOp>(blockArg.getOwner()->getParentOp())) {
      unsigned argIndex = blockArg.getArgNumber();
      ValueRange deps = edt.getDependencies();
      if (argIndex < deps.size())
        return getUnderlyingValueImpl(deps[argIndex], visited, depth + 1);
    }
    return v;
  }

  Operation *op = v.getDefiningOp();
  if (!op)
    return nullptr;

  /// Terminal allocations
  if (isa<DbAllocOp, memref::AllocOp, memref::AllocaOp, memref::GetGlobalOp>(
          op))
    return v;

  /// Helper to recurse through a single source operand
  auto trace = [&](Value source) -> Value {
    return getUnderlyingValueImpl(source, visited, depth + 1);
  };

  if (auto dbAcquire = dyn_cast<DbAcquireOp>(op))
    return trace(dbAcquire.getSourcePtr());
  if (auto dbGep = dyn_cast<DbGepOp>(op))
    return trace(dbGep.getBasePtr());
  if (auto dbControl = dyn_cast<DbControlOp>(op))
    return trace(dbControl.getPtr());
  if (auto dbRef = dyn_cast<DbRefOp>(op))
    return trace(dbRef.getSource());
  if (auto subview = dyn_cast<memref::SubViewOp>(op))
    return trace(subview.getSource());
  if (auto castOp = dyn_cast<memref::CastOp>(op))
    return trace(castOp.getSource());
  if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(op)) {
    ValueRange inputs = unrealized.getInputs();
    return inputs.empty() ? nullptr : trace(inputs.front());
  }
  if (auto view = dyn_cast<memref::ViewOp>(op))
    return trace(view.getSource());
  if (auto reinterpret = dyn_cast<memref::ReinterpretCastOp>(op))
    return trace(reinterpret.getSource());
  if (auto p2m = dyn_cast<polygeist::Pointer2MemrefOp>(op))
    return trace(p2m.getSource());
  if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(op))
    return trace(m2p.getSource());
  if (auto gep = dyn_cast<LLVM::GEPOp>(op))
    return trace(gep.getBase());
  if (auto subindex = dyn_cast<polygeist::SubIndexOp>(op))
    return trace(subindex.getSource());

  return nullptr;
}

Value ValueUtils::getUnderlyingValue(Value v) {
  SmallPtrSet<Value, 16> visited;
  return getUnderlyingValueImpl(v, visited, 0);
}

Operation *ValueUtils::getUnderlyingOperation(Value v) {
  Value underlyingValue = getUnderlyingValue(v);
  return underlyingValue ? underlyingValue.getDefiningOp() : nullptr;
}

///===----------------------------------------------------------------------===//
/// Value Reconstruction for Dominance
///===----------------------------------------------------------------------===//

Value ValueUtils::traceValueToDominating(Value value, Operation *insertBefore,
                                         OpBuilder &builder,
                                         DominanceInfo &domInfo, Location loc,
                                         unsigned depth) {
  if (!value || depth > 16)
    return nullptr;

  if (domInfo.properlyDominates(value, insertBefore))
    return value;

  if (auto blockArg = value.dyn_cast<BlockArgument>())
    return nullptr;

  Operation *defOp = value.getDefiningOp();
  if (!defOp)
    return nullptr;

  auto trace = [&](Value operand) -> Value {
    return traceValueToDominating(operand, insertBefore, builder, domInfo, loc,
                                  depth + 1);
  };

  if (auto op = dyn_cast<arith::AddIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::SubIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::MulIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::DivSIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::DivUIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::CeilDivSIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::CeilDivUIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::RemSIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::RemUIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::MaxSIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::MaxUIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::MinSIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::MinUIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::AndIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::OrIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);
  if (auto op = dyn_cast<arith::XOrIOp>(defOp))
    return traceBinaryOp(op, insertBefore, builder, loc, trace);

  if (auto cmpOp = dyn_cast<arith::CmpIOp>(defOp)) {
    Value lhs = trace(cmpOp.getLhs());
    Value rhs = trace(cmpOp.getRhs());
    if (lhs && rhs) {
      builder.setInsertionPoint(insertBefore);
      return builder.create<arith::CmpIOp>(loc, cmpOp.getPredicate(), lhs, rhs);
    }
    return nullptr;
  }

  if (auto selectOp = dyn_cast<arith::SelectOp>(defOp)) {
    Value cond = trace(selectOp.getCondition());
    Value trueVal = trace(selectOp.getTrueValue());
    Value falseVal = trace(selectOp.getFalseValue());
    if (cond && trueVal && falseVal) {
      builder.setInsertionPoint(insertBefore);
      return builder.create<arith::SelectOp>(loc, cond, trueVal, falseVal);
    }
    return nullptr;
  }

  if (auto castOp = dyn_cast<arith::IndexCastOp>(defOp)) {
    Value inner = trace(castOp.getIn());
    if (!inner)
      return nullptr;
    if (inner.getType() == castOp.getType())
      return inner;
    builder.setInsertionPoint(insertBefore);
    return builder.create<arith::IndexCastOp>(loc, castOp.getType(), inner);
  }
  if (auto castOp = dyn_cast<arith::IndexCastUIOp>(defOp)) {
    Value inner = trace(castOp.getIn());
    if (!inner)
      return nullptr;
    if (inner.getType() == castOp.getType())
      return inner;
    builder.setInsertionPoint(insertBefore);
    return builder.create<arith::IndexCastUIOp>(loc, castOp.getType(), inner);
  }
  if (auto castOp = dyn_cast<arith::ExtSIOp>(defOp)) {
    Value inner = trace(castOp.getIn());
    if (!inner)
      return nullptr;
    builder.setInsertionPoint(insertBefore);
    return builder.create<arith::ExtSIOp>(loc, castOp.getType(), inner);
  }
  if (auto castOp = dyn_cast<arith::ExtUIOp>(defOp)) {
    Value inner = trace(castOp.getIn());
    if (!inner)
      return nullptr;
    builder.setInsertionPoint(insertBefore);
    return builder.create<arith::ExtUIOp>(loc, castOp.getType(), inner);
  }
  if (auto castOp = dyn_cast<arith::TruncIOp>(defOp)) {
    Value inner = trace(castOp.getIn());
    if (!inner)
      return nullptr;
    builder.setInsertionPoint(insertBefore);
    return builder.create<arith::TruncIOp>(loc, castOp.getType(), inner);
  }

  if (auto constOp = dyn_cast<arith::ConstantOp>(defOp)) {
    builder.setInsertionPoint(insertBefore);
    return builder.create<arith::ConstantOp>(loc, constOp.getType(),
                                             constOp.getValue());
  }
  if (auto constIdxOp = dyn_cast<arith::ConstantIndexOp>(defOp)) {
    builder.setInsertionPoint(insertBefore);
    return builder.create<arith::ConstantIndexOp>(loc, constIdxOp.value());
  }

  return nullptr;
}

Value ValueUtils::traceMinSIWithFallback(
    arith::MinSIOp minOp, Operation *insertBefore, OpBuilder &builder,
    Location loc, llvm::function_ref<Value(Value)> traceValueFn) {
  Value lhsTraced = traceValueFn(minOp.getLhs());
  Value rhsTraced = traceValueFn(minOp.getRhs());
  if (lhsTraced && rhsTraced) {
    if (ValueUtils::isZeroConstant(lhsTraced))
      return rhsTraced;
    if (ValueUtils::isZeroConstant(rhsTraced))
      return lhsTraced;
    builder.setInsertionPoint(insertBefore);
    return builder.create<arith::MinSIOp>(loc, lhsTraced, rhsTraced);
  }
  if (rhsTraced && !lhsTraced) {
    if (ValueUtils::isZeroConstant(rhsTraced))
      return nullptr;
    return rhsTraced;
  }
  if (lhsTraced && !rhsTraced) {
    if (ValueUtils::isZeroConstant(lhsTraced))
      return nullptr;
    return lhsTraced;
  }
  return nullptr;
}

Value ValueUtils::traceMinUIWithFallback(
    arith::MinUIOp minOp, Operation *insertBefore, OpBuilder &builder,
    Location loc, llvm::function_ref<Value(Value)> traceValueFn) {
  Value lhsTraced = traceValueFn(minOp.getLhs());
  Value rhsTraced = traceValueFn(minOp.getRhs());
  if (lhsTraced && rhsTraced) {
    if (ValueUtils::isZeroConstant(lhsTraced))
      return rhsTraced;
    if (ValueUtils::isZeroConstant(rhsTraced))
      return lhsTraced;
    builder.setInsertionPoint(insertBefore);
    return builder.create<arith::MinUIOp>(loc, lhsTraced, rhsTraced);
  }
  if (rhsTraced && !lhsTraced) {
    if (ValueUtils::isZeroConstant(rhsTraced))
      return nullptr;
    return rhsTraced;
  }
  if (lhsTraced && !rhsTraced) {
    if (ValueUtils::isZeroConstant(lhsTraced))
      return nullptr;
    return lhsTraced;
  }
  return nullptr;
}

Value ValueUtils::traceSelectWithFallback(
    arith::SelectOp selectOp, Operation *insertBefore, OpBuilder &builder,
    Location loc, llvm::function_ref<Value(Value)> traceValueFn,
    llvm::function_ref<Value(Value)> traceCondFn) {
  Value trueTraced = traceValueFn(selectOp.getTrueValue());
  Value falseTraced = traceValueFn(selectOp.getFalseValue());

  if (trueTraced && falseTraced) {
    Value cond = traceCondFn(selectOp.getCondition());
    builder.setInsertionPoint(insertBefore);
    if (cond) {
      return builder.create<arith::SelectOp>(loc, cond, trueTraced,
                                             falseTraced);
    }
    /// Condition doesn't dominate -- use max of arms as safe upper bound.
    if (trueTraced.getType().isa<IndexType>()) {
      return builder.create<arith::MaxUIOp>(loc, trueTraced, falseTraced);
    }
    return builder.create<arith::MaxSIOp>(loc, trueTraced, falseTraced);
  }

  if (trueTraced && !falseTraced) {
    if (ValueUtils::isZeroConstant(trueTraced))
      return nullptr;
    return trueTraced;
  }
  if (falseTraced && !trueTraced) {
    if (ValueUtils::isZeroConstant(falseTraced))
      return nullptr;
    return falseTraced;
  }
  return nullptr;
}

} // namespace arts
} // namespace mlir
