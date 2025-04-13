///==========================================================================
/// File: HoistInvariantOps.cpp
///
/// This pass scans every block in each EDT region collects all invariant
/// operations (operations that depend only on values defined outside the
/// region), and then moves them to the parent block (just before the op that
/// owns the region).
/// This is fundamental to allow the creation of datablocks
///==========================================================================

/// Dialects
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Block.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/OpDefinition.h"
// #include "mlir/IR/LLVMTypes.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/Pass.h"

/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"

/// Others
#include "mlir/Analysis/SliceAnalysis.h"
#include "mlir/Transforms/RegionUtils.h"

#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"

/// LLVM support
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <functional>

#define DEBUG_TYPE "hoist-invariant"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;

namespace {
struct HoistInvariantOpsPass
    : public HoistInvariantOpsBase<HoistInvariantOpsPass> {
  void runOnOperation() override;
};
} // end anonymous namespace

void HoistInvariantOpsPass::runOnOperation() {
  ModuleOp module = getOperation();
  LLVM_DEBUG({
    dbgs() << "\n" << line << "HoistInvariantOpsPass STARTED\n" << line;
    module.dump();
  });

  module.walk([&](arts::EdtOp edtOp) {
    Region &region = edtOp.getRegion();
    LLVM_DEBUG(DBGS() << "Processing EDT \n" << edtOp << "\n";);
    SetVector<Value> externalValues;
    getUsedValuesDefinedAbove(region, externalValues);

    LLVM_DEBUG({
      dbgs() << "External values in EDT region:\n";
      for (Value v : externalValues)
        dbgs() << "  - " << v << "\n";
    });

    /// Collect invariant external values
    SetVector<Value> invariantExternals;
    LLVM_DEBUG(dbgs() << "Identifying invariant external values:\n";);
    for (Value v : externalValues) {
      if (!isInvariantInEDT(edtOp, v))
        continue;
      LLVM_DEBUG(dbgs() << "  - " << v << "\n";);
      invariantExternals.insert(v);
    }

    /// Collect and propagate invariant operations in the EDT region.
    SetVector<Operation *> invariantOps;
    SetVector<Value> invariantValues = invariantExternals;

    /// Walk only over operations directly in the region.
    for (Operation &rawOp : region.getOps()) {
      Operation *op = &rawOp;

      /// Skip operations that are already in the set
      if (invariantOps.contains(op))
        continue;

      /// Check if an internal operation is invariant.
      auto isInvariant = [&](Operation *opToCheck) -> bool {
        /// Skip terminators and inner EDTs.
        if (!mlir::isMemoryEffectFree(opToCheck))
          return false;

        if (opToCheck->hasTrait<OpTrait::IsTerminator>() || isArtsOp(opToCheck))
          return false;
        for (Value result : opToCheck->getResults()) {
          if (result.getType().isa<MemRefType>() ||
              result.getType().isa<LLVM::LLVMPointerType>())
            return false;
        }

        for (Value operand : opToCheck->getOperands()) {
          if (!invariantValues.contains(operand))
            return false;
        }
        /// Record the invariant operation and its results.
        invariantOps.insert(opToCheck);
        for (Value result : opToCheck->getResults())
          invariantValues.insert(result);
        return true;
      };

      /// If the op is invariant, mark it and propagate its invariance.
      if (isInvariant(op)) {
        /// Propagate through its forward slice.
        SetVector<Operation *> forwardSlice;
        ForwardSliceOptions options;
        options.filter = [&](Operation *sliceOp) -> bool {
          return isInvariant(sliceOp);
        };
        LLVM_DEBUG(dbgs() << "    - Invariant op found: " << *op << "\n");
        getForwardSlice(op, &forwardSlice, options);
      }
    }

    /// If no invariant operation is found, skip further hoisting.
    if (invariantOps.empty()) {
      LLVM_DEBUG(dbgs() << "    - No invariant operations found in EDT region, "
                           "skipping hoisting.\n";);
      return;
    }

    /// Hoist invariant operations to the edtOp
    OpBuilder builder(edtOp);
    for (Operation *op : invariantOps) {
      builder.setInsertionPoint(edtOp);
      op->moveBefore(edtOp);
    }

    LLVM_DEBUG({
      dbgs() << "\n"
             << line << "HoistInvariantOpsPass after hoisting\n"
             << line;
      module.dump();
      dbgs() << line;
    });
  });

  LLVM_DEBUG({
    dbgs() << "\n" << line << "HoistInvariantOpsPass FINISHED\n" << line;
    module.dump();
  });
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createHoistInvariantOpsPass() {
  return std::make_unique<HoistInvariantOpsPass>();
}

} // namespace arts
} // namespace mlir
