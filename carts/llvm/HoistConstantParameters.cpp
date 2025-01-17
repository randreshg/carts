//===- HoistConstantParameters.cpp - Convert ARTS Dialect to Func Dialect
//------===//
//
// This file implements a pass to convert ARTS dialect operations into
// `func` dialect operations.
//
//===----------------------------------------------------------------------===//

#include "mlir/IR/Block.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Region.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Transforms/DialectConversion.h"

/// Other dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"

/// Arts
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"

#include "llvm/ADT/SmallVector.h"

using namespace mlir;

/// Checks if 'val' is produced by an arith.constant or some other
/// "constant-like" op you want to hoist.
static bool isConstantValue(Value val) {
  if (auto *defOp = val.getDefiningOp()) {
    return isa<arith::ConstantOp>(defOp);
  }
  return false;
}

/// Hoists any constant operands of 'op' into its region.
/// - We assume the operation has exactly one region with one entry block.
/// - The block's arguments map 1:1 to the op's operands.
static LogicalResult hoistConstantParameters(Operation *op,
                                             PatternRewriter &rewriter) {
  /// Only hoist if the operation is an arts operation
  if (!isArtsRegion(op))
    return success();

  /// Must have at least 1 region to do anything.
  if (op->getNumRegions() == 0)
    return success();

  Region &region = op->getRegion(0);
  if (region.empty())
    return success();

  Block &entryBlock = region.front();

  /// Check if block args match the op's operands.
  // if (entryBlock.getNumArguments() != op->getNumOperands())
  //   return failure();

  /// Find all operand indices that come from an arith.constant.
  SmallVector<unsigned> constIndices;
  for (unsigned i = 0, e = op->getNumOperands(); i < e; ++i) {
    if (isConstantValue(op->getOperand(i))) {
      constIndices.push_back(i);
    }
  }

  /// If no constant operands, nothing to hoist.
  if (constIndices.empty())
    return success();

  /// We'll remove them in reverse order to avoid index shifting.
  for (int i = constIndices.size() - 1; i >= 0; --i) {
    unsigned idx = constIndices[i];

    Value outsideConst = op->getOperand(idx);
    BlockArgument blockArg = entryBlock.getArgument(idx);

    /// Confirm we have an arith.constant op
    auto constOp = outsideConst.getDefiningOp<arith::ConstantOp>();
    if (!constOp)
      continue;

    /// Build a new operand list for op that excludes this operand.
    SmallVector<Value> newOperands;
    newOperands.reserve(op->getNumOperands() - 1);
    for (unsigned j = 0, end = op->getNumOperands(); j < end; ++j) {
      /// skip the constant index
      if (j != idx) 
        newOperands.push_back(op->getOperand(j));
    }
    op->setOperands(newOperands);

    // 3) Clone (or re-create) the constant *inside* the region.
    // We'll insert at the start of the entry block or just before first use.
    rewriter.setInsertionPointToStart(&entryBlock);
    auto newInsideConst = rewriter.create<arith::ConstantOp>(
        constOp.getLoc(), blockArg.getType(), constOp.getValue());

    // 4) Replace all uses of the blockArg with this newInsideConst.
    blockArg.replaceAllUsesWith(newInsideConst);

    // 5) Remove the block argument itself.
    entryBlock.eraseArgument(idx);
  }

  return success();
}

/// Pattern that matches 'arts.parallel' or 'arts.edt' by name
/// and invokes hoistConstantParameters to remove constant parameters.
struct HoistConstantParamsPattern : public OpRewritePattern<Operation> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(Operation *op, PatternRewriter &rewriter) const final {
    // Adjust these checks if you have actual classes or more ops:
    StringRef opName = op->getName().getStringRef();
    if (opName == "arts.parallel" || opName == "arts.edt") {
      // For safety, check that there's at least one region with a block.
      if (op->getNumRegions() == 1 && !op->getRegion(0).empty()) {
        if (failed(hoistConstantParameters(op, rewriter))) {
          // If the transformation fails, we can either do nothing or signal pass
          // failure.
          return failure();
        }
        return success();
      }
    }
    return failure();
  }
};

/// The pass that applies the pattern across the module.
struct HoistConstantParametersPass
    : public PassWrapper<HoistConstantParametersPass, OperationPass<ModuleOp>> {
  StringRef getArgument() const final { return "hoist-constant-parameters"; }
  StringRef getDescription() const final {
    return "Hoist (duplicate) constant parameters into arts.parallel/arts.edt "
           "regions.";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    // Create a pattern list
    RewritePatternSet patterns(&getContext());
    patterns.add<HoistConstantParamsPattern>(&getContext());

    // Apply the patterns greedily
    if (failed(applyPatternsAndFoldGreedily(module, std::move(patterns)))) {
      signalPassFailure();
    }
  }
};

/// Factory function to create the pass (if you want to register it
/// programmatically).
std::unique_ptr<Pass> createHoistConstantParametersPass() {
  return std::make_unique<HoistConstantParametersPass>();
}

// Optionally, you can register the pass so it's available via command line:
// static PassRegistration<HoistConstantParametersPass> pass;