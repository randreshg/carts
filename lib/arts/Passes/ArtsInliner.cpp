
///==========================================================================
/// File: ArtsInliner.cpp
///==========================================================================

#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "mlir/Analysis/CallGraph.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/InliningUtils.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "inliner"
#define DBGS() (llvm::dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;

namespace {
struct ArtsInlinerInterface : public mlir::InlinerInterface {
  using mlir::InlinerInterface::InlinerInterface;

  //===--------------------------------------------------------------------===//
  /// Analysis Hooks
  //===--------------------------------------------------------------------===//

  /// All call operations within standard ops can be inlined.
  bool isLegalToInline(mlir::Operation *call, mlir::Operation *callable,
                       bool wouldBeCloned) const final {
    return true;
  }

  /// All operations within standard ops can be inlined.
  bool isLegalToInline(mlir::Region *, mlir::Region *, bool,
                       mlir::IRMapping &) const final {
    return true;
  }

  /// All operations within standard ops can be inlined.
  bool isLegalToInline(mlir::Operation *, mlir::Region *, bool,
                       mlir::IRMapping &) const final {
    return true;
  }

  //===--------------------------------------------------------------------===//
  /// Transformation Hooks
  //===--------------------------------------------------------------------===//

  /// Handle the given inlined terminator by replacing it with a new operation
  /// as necessary.
  void handleTerminator(mlir::Operation *op, mlir::Block *newDest) const final {
    /// Only "std.return" needs to be handled here.
    auto returnOp = mlir::dyn_cast<mlir::func::ReturnOp>(op);
    if (!returnOp)
      return;

    /// Replace the return with a branch to the dest.
    mlir::OpBuilder builder(op);
    builder.create<mlir::cf::BranchOp>(op->getLoc(), newDest,
                                       returnOp.getOperands());
    op->erase();
  }

  /// Handle the given inlined terminator by replacing it with a new operation
  /// as necessary.
  void handleTerminator(mlir::Operation *op,
                        mlir::ArrayRef<mlir::Value> valuesToRepl) const final {
    /// Only "std.return" needs to be handled here.
    auto returnOp = mlir::cast<mlir::func::ReturnOp>(op);

    /// Replace the values directly with the return operands.
    assert(returnOp.getNumOperands() == valuesToRepl.size());
    for (const auto &it : llvm::enumerate(returnOp.getOperands()))
      valuesToRepl[it.index()].replaceAllUsesWith(it.value());
  }
};

[[maybe_unused]] static void inlineCall(mlir::func::CallOp caller) {
  /// Build the inliner interface.
  InlinerInterface interface(caller.getContext());

  auto callable = caller.getCallableForCallee();
  mlir::CallableOpInterface callableOp;
  if (mlir::SymbolRefAttr symRef =
          mlir::dyn_cast<mlir::SymbolRefAttr>(callable)) {
    auto *symbolOp =
        caller->getParentOfType<mlir::ModuleOp>().lookupSymbol(symRef);
    callableOp = mlir::dyn_cast_or_null<mlir::CallableOpInterface>(symbolOp);
  } else {
    return;
  }
  mlir::Region *targetRegion = callableOp.getCallableRegion();
  if (!targetRegion)
    return;
  if (targetRegion->empty())
    return;
  if (inlineCall(interface, caller, callableOp, targetRegion,
                 /*shouldCloneInlinedRegion=*/true)
          .succeeded()) {
    caller.erase();
  }
};

struct ArtsInlinerPass : public arts::ArtsInlinerBase<ArtsInlinerPass> {

  void runOnOperation() override {
    ModuleOp module = getOperation();

    LLVM_DEBUG(DBGS() << "Starting aggressive inlining pass\n");

    /// Build the inliner interface
    InlinerInterface inliner(&getContext());

    /// Use a worklist approach to handle newly created calls during inlining
    bool changed;
    unsigned iteration = 0;
    do {
      changed = false;
      iteration++;

      LLVM_DEBUG(DBGS() << "Inlining iteration " << iteration << "\n");

      /// Collect all call ops in the module
      SmallVector<func::CallOp> calls;
      module.walk([&](func::CallOp call) { calls.push_back(call); });

      LLVM_DEBUG(DBGS() << "Found " << calls.size() << " call operations\n");

      /// Process each call operation
      for (func::CallOp call : calls) {
        /// Skip already erased operations
        if (call->getParentOp() == nullptr) {
          LLVM_DEBUG(DBGS() << "Skipping erased call\n");
          continue;
        }

        /// Resolve the callable operation
        auto callable = call.getCallableForCallee();
        CallableOpInterface callableOp;
        if (SymbolRefAttr symRef = callable.dyn_cast<SymbolRefAttr>()) {
          callableOp =
              dyn_cast<CallableOpInterface>(module.lookupSymbol(symRef));
        } else if (auto func = callable.dyn_cast<Value>()) {
          callableOp = dyn_cast<CallableOpInterface>(func.getDefiningOp());
        }

        if (!callableOp || !callableOp.getCallableRegion()) {
          LLVM_DEBUG(DBGS() << "Cannot resolve callable for call\n");
          continue;
        }

        /// Skip if the callable region is empty
        if (callableOp.getCallableRegion()->empty()) {
          LLVM_DEBUG(DBGS() << "Skipping call to empty function\n");
          continue;
        }

        /// Check if this is a recursive call (basic check)
        if (auto funcOp = dyn_cast<func::FuncOp>(callableOp.getOperation())) {
          auto enclosingFn = call->getParentOfType<func::FuncOp>();
          if (enclosingFn &&
              enclosingFn.getSymName() == funcOp.getSymName()) {
            LLVM_DEBUG(DBGS() << "Skipping recursive call to "
                              << funcOp.getSymName() << "\n");
            continue;
          }
        }

        LLVM_DEBUG(DBGS() << "Attempting to inline call\n");

        /// Attempt to inline the call
        if (succeeded(mlir::inlineCall(inliner, call, callableOp,
                                       callableOp.getCallableRegion(), true))) {
          LLVM_DEBUG(DBGS() << "Successfully inlined call\n");
          call.erase();
          changed = true;
        } else {
          LLVM_DEBUG(DBGS() << "Failed to inline call\n");
        }
      }
      /// Limit iterations to prevent infinite loops
    } while (changed && iteration < 100);

    if (iteration >= 100)
      LLVM_DEBUG(DBGS() << "Warning: Hit maximum iteration limit\n");

    LLVM_DEBUG(DBGS() << "Completed aggressive inlining after " << iteration
                      << " iterations\n");

    /// Clean up unused functions
    cleanupUnusedFunctions(module);
  }

private:
  void cleanupUnusedFunctions(ModuleOp module) {
    LLVM_DEBUG(DBGS() << "Cleaning up unused functions\n");

    /// Collect all function operations
    SmallVector<func::FuncOp> functions;
    module.walk([&](func::FuncOp func) { functions.push_back(func); });

    /// Remove functions that are no longer used
    for (func::FuncOp func : functions) {
      if (func.isPrivate() && func.getSymbolUses(module)->empty()) {
        LLVM_DEBUG(DBGS() << "Removing unused function: " << func.getSymName()
                          << "\n");
        func.erase();
      }
    }
  }
};

} // namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createArtsInlinerPass() {
  return std::make_unique<ArtsInlinerPass>();
}
} // namespace arts
} // namespace mlir