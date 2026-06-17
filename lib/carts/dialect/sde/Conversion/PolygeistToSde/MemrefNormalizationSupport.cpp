///===----------------------------------------------------------------------===///
/// File: MemrefNormalizationSupport.cpp
///
/// Pass-private source and wrapper graph helpers for SdeMemrefNormalization.
///===----------------------------------------------------------------------===///

#include "MemrefNormalizationInternal.h"

#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Interfaces/LoopLikeInterface.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"

using namespace mlir;

namespace mlir::carts::sde::memref_normalization {

Operation *findNearestLoop(Operation *op) {
  for (Operation *cur = op->getParentOp(); cur; cur = cur->getParentOp()) {
    if (isa<LoopLikeOpInterface>(cur) || isa<omp::WsloopOp>(cur))
      return cur;
  }
  return nullptr;
}

Value getForwardedMemrefAliasSource(Value value) {
  if (!value)
    return nullptr;

  Operation *defOp = value.getDefiningOp();
  if (!defOp)
    return nullptr;

  if (auto castOp = dyn_cast<memref::CastOp>(defOp))
    return castOp.getSource();
  if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(defOp)) {
    if (unrealized.getInputs().size() == 1 &&
        isa<MemRefType>(unrealized.getInputs().front().getType()))
      return unrealized.getInputs().front();
  }

  return nullptr;
}

Value getForwardedMemrefAliasResult(Operation *user, Value current) {
  if (!user || !current)
    return nullptr;

  if (auto castOp = dyn_cast<memref::CastOp>(user))
    return castOp.getSource() == current ? castOp.getResult() : Value();
  if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(user)) {
    if (unrealized.getInputs().size() == 1 &&
        unrealized.getInputs().front() == current &&
        unrealized.getOutputs().size() == 1 &&
        isa<MemRefType>(unrealized.getOutputs().front().getType()))
      return unrealized.getOutputs().front();
  }

  return nullptr;
}

bool isMemrefContainerValue(Value value) {
  auto memrefType = dyn_cast_or_null<MemRefType>(value.getType());
  return memrefType && isa<MemRefType>(memrefType.getElementType());
}

static bool isSameUnderlyingValue(Value value, Value expected) {
  if (value == expected)
    return true;
  Value underlying = ::mlir::carts::ValueAnalysis::getUnderlyingValue(value);
  return underlying && underlying == expected;
}

static void collectWrappersStoringValue(Value value,
                                        llvm::SetVector<Value> &wrappers,
                                        DenseSet<Value> &visitedValues) {
  if (!value || !visitedValues.insert(value).second)
    return;

  for (Operation *user : value.getUsers()) {
    if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      if (isSameUnderlyingValue(storeOp.getValue(), value) &&
          isMemrefContainerValue(storeOp.getMemref()))
        wrappers.insert(storeOp.getMemref());
      continue;
    }

    if (Value forwarded = getForwardedMemrefAliasResult(user, value))
      collectWrappersStoringValue(forwarded, wrappers, visitedValues);
  }
}

static bool wrapperLoadEscapesIf(Value wrapper, scf::IfOp parentIf,
                                 DenseSet<Value> &visitedWrappers) {
  if (!wrapper || !visitedWrappers.insert(wrapper).second)
    return false;

  for (Operation *user : wrapper.getUsers()) {
    if (Value forwarded = getForwardedMemrefAliasResult(user, wrapper)) {
      if (wrapperLoadEscapesIf(forwarded, parentIf, visitedWrappers))
        return true;
      continue;
    }

    Value loaded;
    if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
      if (loadOp.getMemref() != wrapper)
        continue;
      if (!parentIf->isAncestor(loadOp))
        return true;
      loaded = loadOp.getResult();
    } else if (auto loadOp = dyn_cast<affine::AffineLoadOp>(user)) {
      if (loadOp.getMemref() != wrapper)
        continue;
      if (!parentIf->isAncestor(loadOp))
        return true;
      loaded = loadOp.getResult();
    } else {
      continue;
    }

    for (Operation *loadUser : loaded.getUsers()) {
      if (!parentIf->isAncestor(loadUser))
        return true;
    }
  }

  return false;
}

bool allocEscapesIf(memref::AllocOp allocOp, scf::IfOp parentIf) {
  for (Operation *user : allocOp->getUsers()) {
    if (!parentIf->isAncestor(user))
      return true;
  }

  llvm::SetVector<Value> wrappers;
  DenseSet<Value> visitedValues;
  collectWrappersStoringValue(allocOp.getResult(), wrappers, visitedValues);
  for (Value wrapper : wrappers) {
    DenseSet<Value> visitedWrappers;
    if (wrapperLoadEscapesIf(wrapper, parentIf, visitedWrappers))
      return true;
  }

  return false;
}

Value traceWrapperLoadToAlloc(Value val) {
  constexpr int MAX_DEPTH = 10;
  for (int depth = 0; depth < MAX_DEPTH; ++depth) {
    auto loadOp = val.getDefiningOp<memref::LoadOp>();
    if (!loadOp)
      break;

    Value wrapper = loadOp.getMemref();
    auto wrapperType = dyn_cast<MemRefType>(wrapper.getType());
    if (!wrapperType || wrapperType.getRank() != 0)
      break;

    Value nextVal;
    for (Operation *user : wrapper.getUsers()) {
      if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
        if (storeOp.getMemref() == wrapper) {
          Value storedVal = storeOp.getValue();
          Value underlying =
              ::mlir::carts::ValueAnalysis::getUnderlyingValue(storedVal);
          if (underlying && underlying.getDefiningOp<memref::AllocOp>())
            return underlying;
          nextVal = storedVal;
          break;
        }
      }
    }
    if (!nextVal)
      break;
    val = nextVal;
  }
  return Value();
}

bool isInnerWrapperOfInlinedPattern(Value alloc) {
  for (Operation *user : alloc.getUsers()) {
    if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
      for (Operation *loadUser : loadOp.getResult().getUsers()) {
        if (auto storeOp = dyn_cast<memref::StoreOp>(loadUser)) {
          auto storeDest = storeOp.getMemref();
          auto storeDestType = dyn_cast<MemRefType>(storeDest.getType());
          if (storeDestType && storeDestType.getRank() == 0 &&
              storeDest.getDefiningOp<memref::AllocaOp>()) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

} // namespace mlir::carts::sde::memref_normalization
