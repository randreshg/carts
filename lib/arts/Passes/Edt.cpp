///==========================================================================
/// File: Edt.cpp
///==========================================================================

/// Dialects
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/RegionUtils.h"
#include "polygeist/Dialect.h"
#include "polygeist/Ops.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/Analysis/DataBlockAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
/// Debug
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>

#define DEBUG_TYPE "edt"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct EdtPass : public arts::EdtBase<EdtPass> {
  void runOnOperation() override;
  // void handleParallel(EdtOp &op);
  // void handleSingle(EdtOp &op);
  // void handleEdt(EdtOp &op);
  void convertParallelIntoSingle(EdtOp &op);

private:
  SetVector<Operation *> opsToRemove;
};
} // end namespace

void EdtPass::convertParallelIntoSingle(EdtOp &op) {
  /// Analyze the parallel region to locate the unique single-edt op.
  uint32_t numOps = 0;
  arts::EdtOp singleOp = nullptr;

  /// Iterate over the immediate operations in the region.
  for (auto &block : op.getRegion()) {
    for (auto &inst : block) {
      ++numOps;
      if (auto edt = dyn_cast<arts::EdtOp>(&inst)) {
        if (edt.isSingle()) {
          if (singleOp)
            llvm_unreachable(
                "Multiple single ops in parallel op not supported");
          singleOp = edt;
        }
      } else if (!isa<arts::BarrierOp>(&inst) && !isa<arts::YieldOp>(&inst)) {
        llvm_unreachable("Unknown op in parallel op - not supported");
      }
    }
  }
  assert(singleOp && numOps == 4 && "Invalid parallel op region structure");

  LLVM_DEBUG(DBGS() << "Converting parallel EDT into single EDT\n");
  /// Insert the single operation before the parallel op and remove the "single"
  /// attribute.
  singleOp->moveBefore(op);
  singleOp.clearIsSingleAttr();

  /// Set sync attribute
  singleOp.setIsSyncAttr();

  /// Mark the parallel op for removal.
  opsToRemove.insert(op);
}

void EdtPass::runOnOperation() {
  ModuleOp module = getOperation();
  LLVM_DEBUG(dbgs() << "\n" << line << "EdtPass STARTED\n" << line);

  /// Convert parallel EDTs into single EDTs.
  SmallVector<EdtOp> parallelOps;
  module.walk([&](EdtOp edt) {
    if (edt.isParallel())
      parallelOps.push_back(edt);
  });

  for (auto op : parallelOps)
    convertParallelIntoSingle(op);

  /// Analyzes the EDT environment to check if there is any parameter that loads
  /// a memref and indices of any dependency. If so, it creates a new load
  /// operation at the start of the region and replaces all the uses of the
  /// parameter.
  module.walk([&](EdtOp edt) {
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

      /// Return if the value is not an LLVM pointer type
      if (!val.getDefiningOp() || val.getType().isIntOrIndexOrFloat() ||
          val.getType().isa<MemRefType>())
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
      /// Skip non-pointer types
      if (!operand.getType().isa<LLVM::LLVMPointerType>())
        continue;

      /// Clone operation and its dependencies recursively
      OpBuilder builder(edtRegion.getContext());
      builder.setInsertionPointToStart(&edtRegion.front());
      Value newVal = cloneWithDeps(operand, builder, valueMap);
      if (newVal != operand) {
        replaceInRegion(edtRegion, operand, newVal);
      }
    }
  });

  /// Remove all the operations marked for removal.
  OpBuilder builder(module.getContext());
  removeOps(module, builder, opsToRemove);

  LLVM_DEBUG(dbgs() << line << "EdtPass FINISHED\n" << line);
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createEdtPass() { return std::make_unique<EdtPass>(); }
} // namespace arts
} // namespace mlir
