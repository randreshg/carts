///==========================================================================///=
/// EdtPtrRematerialization
///==========================================================================///=

/// Dialects
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Value.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

/// Arts
#include "arts/ArtsDialect.h"
#include "arts/Transforms/EdtPtrRematerialization.h"
#include "arts/Utils/ArtsUtils.h"

/// Others
#include "mlir/IR/Operation.h"
#include "mlir/Transforms/RegionUtils.h"
#include "llvm/ADT/SmallVector.h"

/// LLVM support
// #include "arts/Utils/ArtsDebug.h"
// ARTS_DEBUG_SETUP(edt_ptr_rematerialization);

namespace mlir {
namespace arts {

void rematerializePointersInEdt(EdtOp edt) {
  /// Analyzes the EDT environment to check if there is any parameter that
  /// loads a memref and indices of any dependency. If so, it creates a new
  /// load operation at the start of the region and replaces all the uses of
  /// the parameter.
  SetVector<Value> usedValues;
  auto &edtRegion = edt.getRegion();
  getUsedValuesDefinedAbove(edtRegion, usedValues);

  /// Helper function to recursively clone operations and their pointer
  /// dependencies
  std::function<Value(Value, OpBuilder &, DenseMap<Value, Value> &)>
      cloneWithDeps = [&](Value val, OpBuilder &builder,
                          DenseMap<Value, Value> &valueMap) -> Value {
    /// If already cloned, return the mapped value
    if (valueMap.count(val))
      return valueMap[val];

    /// Skip if there's no defining operation
    Operation *defOp = val.getDefiningOp();
    if (!defOp)
      return val;

    /// Check if operation has side effects that prevent rematerialization
    /// (same pattern as EdtLowering.cpp)
    bool hasRegions = defOp->getNumRegions() != 0;
    bool hasSideEffects = false;
    if (auto memInterface = dyn_cast<MemoryEffectOpInterface>(defOp)) {
      hasSideEffects = memInterface.hasEffect<MemoryEffects::Write>() ||
                       memInterface.hasEffect<MemoryEffects::Allocate>() ||
                       memInterface.hasEffect<MemoryEffects::Free>();
    } else {
      hasSideEffects = !isMemoryEffectFree(defOp);
    }

    /// Skip operations with regions or write/allocate/free side effects
    if (hasRegions || hasSideEffects)
      return val;

    /// First clone dependencies
    SmallVector<Value, 4> newOperands;
    for (Value operand : defOp->getOperands()) {
      /// Check if operand needs to be cloned (has defining op and can be
      /// rematerialized)
      if (operand.getDefiningOp()) {
        Operation *opDefOp = operand.getDefiningOp();
        bool opHasRegions = opDefOp->getNumRegions() != 0;
        bool opHasSideEffects = false;
        if (auto memInterface = dyn_cast<MemoryEffectOpInterface>(opDefOp)) {
          opHasSideEffects =
              memInterface.hasEffect<MemoryEffects::Write>() ||
              memInterface.hasEffect<MemoryEffects::Allocate>() ||
              memInterface.hasEffect<MemoryEffects::Free>();
        } else {
          opHasSideEffects = !isMemoryEffectFree(opDefOp);
        }
        /// Clone if it's a pure operation without regions
        if (!opHasRegions && !opHasSideEffects) {
          newOperands.push_back(cloneWithDeps(operand, builder, valueMap));
        } else {
          newOperands.push_back(operand);
        }
      } else {
        newOperands.push_back(operand);
      }
    }
    /// Update operands with the cloned versions
    auto clonedOp = val.getDefiningOp()->clone();
    for (auto [idx, newOp] : llvm::enumerate(newOperands))
      clonedOp->setOperand(idx, newOp);
    builder.insert(clonedOp);

    /// Store the mapping
    valueMap[val] = clonedOp->getResult(0);
    return clonedOp->getResult(0);
  };

  DenseMap<Value, Value> valueMap;
  for (Value operand : usedValues) {
    /// Check if this operand can be safely rematerialized
    /// Only rematerialize operations without write/allocate/free side effects
    Operation *defOp = operand.getDefiningOp();
    if (!defOp)
      continue;

    /// Check if operation has side effects that prevent rematerialization
    bool hasRegions = defOp->getNumRegions() != 0;
    bool hasSideEffects = false;
    if (auto memInterface = dyn_cast<MemoryEffectOpInterface>(defOp)) {
      hasSideEffects = memInterface.hasEffect<MemoryEffects::Write>() ||
                       memInterface.hasEffect<MemoryEffects::Allocate>() ||
                       memInterface.hasEffect<MemoryEffects::Free>();
    } else {
      hasSideEffects = !isMemoryEffectFree(defOp);
    }

    /// Skip operations with regions or write/allocate/free side effects
    if (hasRegions || hasSideEffects)
      continue;

    /// Clone operation and its dependencies recursively
    OpBuilder builder(edtRegion.getContext());
    builder.setInsertionPointToStart(&edtRegion.front());
    Value newVal = cloneWithDeps(operand, builder, valueMap);
    if (newVal != operand) {
      replaceInRegion(edtRegion, operand, newVal);
    }
  }
}
} // namespace arts
} // namespace mlir
