///==========================================================================///=
/// EdtPtrRematerialization
///==========================================================================///=

/// Dialects
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
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
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(edt_ptr_rematerialization);

namespace mlir {
namespace arts {

/// Check if an operation produces pointer or memref types that should be
/// rematerialized. Returns true if ANY result is a pointer-like type.
static bool producesPointerLikeType(Operation *op) {
  for (Value result : op->getResults()) {
    Type resultType = result.getType();
    /// LLVM pointers (e.g., from llvm.getelementptr)
    if (resultType.isa<LLVM::LLVMPointerType>())
      return true;
    /// MemRef types (e.g., from memref.cast, memref.subview)
    if (resultType.isa<MemRefType>())
      return true;
    /// Unranked memref types
    if (resultType.isa<UnrankedMemRefType>())
      return true;
  }
  return false;
}

void rematerializePointersInEdt(EdtOp edt) {
  ARTS_DEBUG("Processing EDT: " << edt);

  SetVector<Value> usedValues;
  auto &edtRegion = edt.getRegion();
  getUsedValuesDefinedAbove(edtRegion, usedValues);

  ARTS_DEBUG("Found " << usedValues.size() << " values used from outside EDT");

  /// Helper function to recursively clone operations and their pointer
  /// dependencies
  OpBuilder builder(edtRegion.getContext());
  builder.setInsertionPointToStart(&edtRegion.front());

  std::function<Value(Value, DenseMap<Value, Value> &)> cloneWithDeps =
      [&](Value val, DenseMap<Value, Value> &valueMap) -> Value {
    ARTS_DEBUG("cloneWithDeps: processing value " << val);

    /// If already cloned, return the mapped value
    if (valueMap.count(val)) {
      ARTS_DEBUG("  Already cloned, returning mapped value");
      return valueMap[val];
    }

    /// Skip if there's no defining operation
    Operation *defOp = val.getDefiningOp();
    if (!defOp) {
      ARTS_DEBUG("  No defining operation, returning original value");
      return val;
    }

    ARTS_DEBUG("  Defining op: " << *defOp);

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

    ARTS_DEBUG("  hasRegions=" << hasRegions
                               << ", hasSideEffects=" << hasSideEffects);

    /// Skip operations with regions or write/allocate/free side effects
    if (hasRegions || hasSideEffects) {
      ARTS_DEBUG("  Skipping due to regions or side effects");
      return val;
    }

    /// Only rematerialize operations that produce pointer-like types
    if (!producesPointerLikeType(defOp)) {
      ARTS_DEBUG("  Skipping: does not produce pointer-like type");
      return val;
    }

    /// First clone dependencies
    ARTS_DEBUG("  Cloning dependencies for " << defOp->getNumOperands()
                                             << " operands");
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
        /// Clone if it's a pure operation without regions AND produces
        /// pointer-like type
        if (!opHasRegions && !opHasSideEffects &&
            producesPointerLikeType(opDefOp)) {
          ARTS_DEBUG("    Cloning operand (pointer-like): " << operand);
          newOperands.push_back(cloneWithDeps(operand, valueMap));
        } else {
          ARTS_DEBUG("    Keeping operand as-is: " << operand);
          newOperands.push_back(operand);
        }
      } else {
        ARTS_DEBUG("    Keeping operand as-is (no defining op): " << operand);
        newOperands.push_back(operand);
      }
    }
    /// Update operands with the cloned versions
    ARTS_DEBUG("  Creating clone of operation");
    auto clonedOp = val.getDefiningOp()->clone();
    for (auto [idx, newOp] : llvm::enumerate(newOperands))
      clonedOp->setOperand(idx, newOp);
    clonedOp = builder.insert(clonedOp);
    builder.setInsertionPointAfter(clonedOp);

    ARTS_DEBUG("  Cloned operation: " << *clonedOp);
    ARTS_DEBUG("  Mapping " << val << " -> " << clonedOp->getResult(0));

    /// Store the mapping
    valueMap[val] = clonedOp->getResult(0);
    return clonedOp->getResult(0);
  };

  DenseMap<Value, Value> valueMap;
  for (Value operand : usedValues) {
    ARTS_DEBUG("\n--- Processing used value: " << operand);

    /// Check if this operand can be safely rematerialized
    /// Only rematerialize operations without write/allocate/free side effects
    Operation *defOp = operand.getDefiningOp();
    if (!defOp) {
      ARTS_DEBUG("  Skipping: no defining operation");
      continue;
    }

    ARTS_DEBUG("  Defining operation: " << *defOp);

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

    ARTS_DEBUG("  hasRegions=" << hasRegions
                               << ", hasSideEffects=" << hasSideEffects);

    /// Skip operations with regions or write/allocate/free side effects
    if (hasRegions || hasSideEffects) {
      ARTS_DEBUG(
          "  Skipping: cannot rematerialize (has regions or side effects)");
      continue;
    }

    /// Only rematerialize operations that produce pointer-like types
    if (!producesPointerLikeType(defOp)) {
      ARTS_DEBUG("  Skipping: does not produce pointer-like type");
      continue;
    }

    /// Clone operation and its dependencies recursively
    ARTS_DEBUG("  Rematerializing operation");
    Value newVal = cloneWithDeps(operand, valueMap);
    if (newVal != operand) {
      ARTS_DEBUG("  Replacing uses of " << operand << " with " << newVal);
      replaceInRegion(edtRegion, operand, newVal);
    } else {
      ARTS_DEBUG("  No replacement needed (newVal == operand)");
    }
  }
}
} // namespace arts
} // namespace mlir
