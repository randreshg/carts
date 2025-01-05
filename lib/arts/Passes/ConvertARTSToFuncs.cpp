//===- ConvertARTSToFuncs.cpp - Convert ARTS Dialect to Func Dialect ------===//
//
// This file implements a pass to convert ARTS dialect operations into
// `func` dialect operations.
//
//===----------------------------------------------------------------------===//

// #include "mlir/Conversion/ARTSToFuncs/ARTSToFuncs.h"
// #include "mlir/Dialect/Arith/IR/Arith.h"
// #include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
/// Debug
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "convert-arts-to-funcs"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;

//===----------------------------------------------------------------------===//
// Conversion Patterns
//===----------------------------------------------------------------------===//
/// Conversion for `arts.make_dep`
struct MakeDepOpLowering : public OpRewritePattern<arts::MakeDepOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(arts::MakeDepOp op,
                                PatternRewriter &rewriter) const override {
    // Replace `arts.make_dep` with a custom operation (or function call)
    // auto depType = op.getResult().getType();
    // auto funcType =
    //     rewriter.getFunctionType({op.memref().getType()}, {depType});
    // auto funcOp =
    //     rewriter.create<func::FuncOp>(op.getLoc(), "__make_dep_func",
    //     funcType);

    // rewriter.replaceOpWithNewOp<func::CallOp>(op, funcOp, op.getOperands());
    return success();
  }
};

/// Conversion for `arts.edt`
struct EdtOpLowering : public OpRewritePattern<arts::EdtOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(arts::EdtOp op,
                                PatternRewriter &rewriter) const override {
    // Create a function for the `arts.edt` operation
    // SmallVector<Type, 4> argTypes(op.parameters().getTypes());
    // for (auto dep : op.dependencies())
    //   argTypes.push_back(dep.getType());

    // auto funcType = rewriter.getFunctionType(argTypes, {});
    // auto funcName =
    //     "__arts_edt_" +
    //     std::to_string(reinterpret_cast<uintptr_t>(op.getOperation()));

    // auto funcOp =
    //     rewriter.create<func::FuncOp>(op.getLoc(), funcName, funcType);
    // funcOp.getBody().takeBody(op.getBody());

    // rewriter.replaceOpWithNewOp<func::CallOp>(
    //     op, funcOp.getFunctionType(), funcOp.getName(), op.getOperands());
    return success();
  }
};

/// Conversion for `arts.epoch`
struct EpochOpLowering : public OpRewritePattern<arts::EpochOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(arts::EpochOp op,
                                PatternRewriter &rewriter) const override {
    // Replace `arts.epoch` with a function that encapsulates the epoch region
    // auto funcType = rewriter.getFunctionType({}, {});
    auto funcName = "__arts_epoch";

    // auto funcOp =
    //     rewriter.create<func::FuncOp>(op.getLoc(), funcName, funcType);
    // funcOp.getBody().takeBody(op.getBody());

    // rewriter.replaceOpWithNewOp<func::CallOp>(op, funcOp.getFunctionType(),
    //                                           funcOp.getName(),
    //                                           ValueRange());
    return success();
  }
};

/// Conversion for `arts.parallel`
struct ParallelOpLowering : public OpRewritePattern<arts::ParallelOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(arts::ParallelOp op,
                                PatternRewriter &rewriter) const override {
    // Replace `arts.parallel` with a function that encapsulates the parallel
    // region
    // auto funcType = rewriter.getFunctionType({}, {});
    auto funcName = "__arts_parallel";

    // auto funcOp =
    //     rewriter.create<func::FuncOp>(op.getLoc(), funcName, funcType);
    // funcOp.getBody().takeBody(op.getBody());

    // rewriter.replaceOpWithNewOp<func::CallOp>(op, funcOp.getFunctionType(),
    //                                           funcOp.getName(),
    //                                           ValueRange());
    return success();
  }
};

/// Conversion for `arts.yield`
struct YieldOpLowering : public OpRewritePattern<arts::YieldOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(arts::YieldOp op,
                                PatternRewriter &rewriter) const override {
    // Replace `arts.yield` with a return operation
    // rewriter.replaceOpWithNewOp<func::ReturnOp>(op, op.getOperands());
    return success();
  }
};

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct ConvertARTSToFuncsPass
    : public mlir::arts::ConvertARTSToFuncsBase<ConvertARTSToFuncsPass> {
  void runOnOperation() override;
};
} // end namespace

void ConvertARTSToFuncsPass::runOnOperation() {
  LLVM_DEBUG(dbgs() << line << "ConvertARTSToFuncsPass STARTED\n" << line);
  MLIRContext *context = &getContext();
  RewritePatternSet patterns(context);
  patterns.add<MakeDepOpLowering, EdtOpLowering, EpochOpLowering,
               ParallelOpLowering, YieldOpLowering>(context);

  ConversionTarget target(*context);
  target.addLegalDialect<func::FuncDialect>();
  target.addIllegalDialect<arts::ArtsDialect>();
  if (failed(applyPartialConversion(getOperation(), target,
                                    std::move(patterns)))) {
    LLVM_DEBUG(dbgs() << "Conversion failed\n");
    signalPassFailure();
    return;
  }
  LLVM_DEBUG(dbgs() << line << "ConvertARTSToFuncsPass FINISHED\n" << line);
}

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createConvertARTSToFuncsPass() {
  return std::make_unique<ConvertARTSToFuncsPass>();
}
} // namespace arts
} // namespace mlir
