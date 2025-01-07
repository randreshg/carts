//===- ConvertARTSToFuncs.cpp - Convert ARTS Dialect to Func Dialect ------===//
//
// This file implements a pass to convert ARTS dialect operations into
// `func` dialect operations.
//
//===----------------------------------------------------------------------===//

// #include "mlir/Conversion/ARTSToFuncs/ARTSToFuncs.h"

// #include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
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

  MakeDepOpLowering(ArtsCodegen &codegen, MLIRContext *ctx)
      : OpRewritePattern<MakeDepOp>(ctx), codegen(codegen) {}

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

private:
  ArtsCodegen &codegen;
};

/// Conversion for `arts.edt`
struct EdtOpLowering : public OpRewritePattern<arts::EdtOp> {
  using OpRewritePattern::OpRewritePattern;

  EdtOpLowering(ArtsCodegen &codegen, MLIRContext *ctx)
      : OpRewritePattern<EdtOp>(ctx), codegen(codegen) {}

  LogicalResult matchAndRewrite(arts::EdtOp op,
                                PatternRewriter &rewriter) const override {
    /// Replace `arts.parallel` with a function that encapsulates the parallel
    /// region
    auto currLoc = op.getLoc();
    auto module = op->getParentOfType<ModuleOp>();
    auto edtFunc = codegen.createEdtFunction(currLoc);
    auto edtCall = codegen.insertEdtCall(op, edtFunc, currLoc);

    // LLVM_DEBUG(dbgs() << "Created EDT function\n" << edtFuncOp << "\n");

    /// debug module
    // rewriter.replaceOp(op, edtCall);
    // module.dump();
    module.print(llvm::outs());
    return success();
  }

private:
  ArtsCodegen &codegen;
};

/// Conversion for `arts.epoch`
struct EpochOpLowering : public OpRewritePattern<arts::EpochOp> {
  using OpRewritePattern::OpRewritePattern;

  EpochOpLowering(ArtsCodegen &codegen, MLIRContext *ctx)
      : OpRewritePattern<EpochOp>(ctx), codegen(codegen) {}

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

private:
  ArtsCodegen &codegen;
};

/// Conversion for `arts.parallel`
struct ParallelOpLowering : public OpRewritePattern<arts::ParallelOp> {
  using OpRewritePattern::OpRewritePattern;

  ParallelOpLowering(ArtsCodegen &codegen, MLIRContext *ctx)
      : OpRewritePattern<ParallelOp>(ctx), codegen(codegen) {}

  LogicalResult matchAndRewrite(arts::ParallelOp op,
                                PatternRewriter &rewriter) const override {
    LLVM_DEBUG(dbgs() << "Converting parallel op\n");
    auto currLoc = op.getLoc();
    /// If the parallel op has only 3 operands: arts.barrier->arts.single->arts.barrier
    /// 1. Create an empty parallel done function
    auto parallelDoneEdtFunc = codegen.createEdtFunction(currLoc);
    // codegen.insertEdtCall(op, parallelDoneEdtFunc, currLoc);
    /// 2. Create an epoch to sync the children edts
    ///    artsGuid_t parallelDoneEdtGuid =
    ///        artsEdtCreate((artsEdt_t)parallelDoneEdt, currentNode, 0, NULL, 1);
    ///    artsGuid_t parallelDoneEpochGuid =
    ///        artsInitializeAndStartEpoch(parallelDoneEdtGuid, 0);
    /// 3. Analyze the code to get the parameters and dependencies
    /// 4. Create a single instance of the code inside the single op
    ///    uint64_t parallelParams[1] = {(uint64_t)N};
    ///    uint64_t parallelDeps = 2 * N;
    ///    artsGuid_t parallelEdtGuid = artsEdtCreateWithEpoch(
    ///        (artsEdt_t)parallelEdt, currentNode, 1, parallelParams, parallelDeps,
    ///        parallelDoneEpochGuid);
    /// 5. Signal the known dependencies
    ///    artsSignalDbs(A_array, parallelEdtGuid, 0, N);
    ///    artsSignalDbs(B_array, parallelEdtGuid, N, N);
    /// 6. Wait for the parallel epoch to finish
    ///    artsWaitOnHandle(parallelDoneEpochGuid);


    return success();
  }

private:
  ArtsCodegen &codegen;
};

/// Conversion for `arts.yield`
struct YieldOpLowering : public OpRewritePattern<arts::YieldOp> {
  using OpRewritePattern::OpRewritePattern;

  YieldOpLowering(ArtsCodegen &codegen, MLIRContext *ctx)
      : OpRewritePattern<YieldOp>(ctx), codegen(codegen) {}

  LogicalResult matchAndRewrite(arts::YieldOp op,
                                PatternRewriter &rewriter) const override {
    // Replace `arts.yield` with a return operation
    // rewriter.replaceOpWithNewOp<func::ReturnOp>(op, op.getOperands());
    return success();
  }

private:
  ArtsCodegen &codegen;
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
  ModuleOp module = getOperation();
  MLIRContext *context = &getContext();
  OpBuilder builder(context);
  ArtsCodegen codegen(module, builder);

  RewritePatternSet patterns(context);
  // patterns.add<MakeDepOpLowering, EdtOpLowering, EpochOpLowering,
  //              ParallelOpLowering, YieldOpLowering>(codegen, context);
  patterns.add<EdtOpLowering>(codegen, context);

  ConversionTarget target(*context);
  // target.addLegalDialect<func::FuncDialect>();
  // target.addIllegalDialect<arts::ArtsDialect>();
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
