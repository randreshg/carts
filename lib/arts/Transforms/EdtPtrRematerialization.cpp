///==========================================================================///=
/// EdtPtrRematerialization
///==========================================================================///=

/// Dialects
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Value.h"

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
    if (!val.getDefiningOp())
      return val;

    /// Check if this is a memref.load operation
    bool isLoadOp = isa<memref::LoadOp>(val.getDefiningOp());

    /// Return if the value is a scalar/memref and NOT from a load operation
    if (!isLoadOp && (val.getType().isIntOrIndexOrFloat() ||
                      val.getType().isa<MemRefType>()))
      return val;

    /// First clone dependencies
    SmallVector<Value, 4> newOperands;
    for (Value operand : val.getDefiningOp()->getOperands()) {
      if (operand.getDefiningOp() &&
          operand.getType().isa<LLVM::LLVMPointerType>()) {
        newOperands.push_back(cloneWithDeps(operand, builder, valueMap));
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
    /// Check if this is a pointer type that needs rematerialization
    bool isPointer = operand.getType().isa<LLVM::LLVMPointerType>();

    /// Check if this is a scalar from a memref.load that needs
    /// rematerialization
    bool isLoadFromMemref = false;
    if (auto *defOp = operand.getDefiningOp())
      isLoadFromMemref = isa<memref::LoadOp>(defOp);

    /// Skip if it's neither a pointer nor a load from memref
    if (!isPointer && !isLoadFromMemref)
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
