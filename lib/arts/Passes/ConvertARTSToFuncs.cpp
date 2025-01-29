//===- ConvertARTSToFuncs.cpp - Convert ARTS Dialect to Func Dialect ------===//
//
// This file implements a pass to convert ARTS dialect operations into
// `func` dialect operations.
//
//===----------------------------------------------------------------------===//

// #include "mlir/Conversion/ARTSToFuncs/ARTSToFuncs.h"

// #include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"

#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Region.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
/// Other dialects
#include "mlir/Analysis/DataLayoutAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
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
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

#define DEBUG_TYPE "convert-arts-to-funcs"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;

namespace {

struct DatablockOpPreprocessing : public OpRewritePattern<arts::DataBlockOp> {
  using OpRewritePattern::OpRewritePattern;

  DatablockOpPreprocessing(MLIRContext *context, ArtsCodegen &AC)
      : OpRewritePattern(context), AC(AC) {}

  LogicalResult matchAndRewrite(arts::DataBlockOp op,
                                PatternRewriter &rewriter) const override {
    LLVM_DEBUG(DBGS() << "Handles arts.datablock: " << op << "\n");

    Location loc = UnknownLoc::get(op.getContext());
    AC.createDatablock(op, loc);
    return success();
  }

private:
  ArtsCodegen &AC;
};

struct DatablockOpLowering : public OpConversionPattern<arts::DataBlockOp> {
  using OpConversionPattern<arts::DataBlockOp>::OpConversionPattern;

  DatablockOpLowering(MLIRContext *context, ArtsCodegen &AC)
      : OpConversionPattern(context), AC(AC) {}

  LogicalResult
  matchAndRewrite(arts::DataBlockOp op, arts::DataBlockOp::Adaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    LLVM_DEBUG(DBGS() << "Lowering arts.datablock: " << op << "\n");

    Location loc = UnknownLoc::get(op.getContext());
    AC.createDatablock(op, loc);
    // op.erase();
    return success();
  }

private:
  ArtsCodegen &AC;
};

struct EdtOpLowering : public OpConversionPattern<arts::EdtOp> {
  using OpConversionPattern<arts::EdtOp>::OpConversionPattern;

  EdtOpLowering(MLIRContext *context, ArtsCodegen &AC)
      : OpConversionPattern(context), AC(AC) {}

  LogicalResult
  matchAndRewrite(arts::EdtOp op, arts::EdtOp::Adaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {

    LLVM_DEBUG(DBGS() << "Lowering arts.edt: " << op << "\n");
    LLVM_DEBUG(DBGS() << "Processing edt op\n");
    Location loc = UnknownLoc::get(op.getContext());
    /// Process dependencies (makes memrefs -> datablocks)
    auto deps = op.getDependencies();
    for (auto dep : deps) {
      auto dbOp = dyn_cast<arts::DataBlockOp>(dep.getDefiningOp());
      assert(dbOp && "Dependency is not a datablock op");
      AC.getOrCreateDatablock(dbOp, loc);
    }

    return success();
  }

private:
  ArtsCodegen &AC;
};

struct ParallelOpLowering : public OpConversionPattern<arts::ParallelOp> {
  using OpConversionPattern<arts::ParallelOp>::OpConversionPattern;

  ParallelOpLowering(MLIRContext *context, ArtsCodegen &AC)
      : OpConversionPattern(context), AC(AC) {}

  LogicalResult
  matchAndRewrite(arts::ParallelOp op, arts::ParallelOp::Adaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    LLVM_DEBUG(DBGS() << "Lowering arts.parallel\n" << "\n");

    Location loc = UnknownLoc::get(op.getContext());
    arts::SingleOp singleOp = nullptr;
    unsigned numOps = 0;

    /// Analyze the parallel region to find the singleOp
    mlir::Region &region = op.getRegion();
    region.walk([&](mlir::Operation *nested) {
      /// Only consider ops directly in this region
      if (nested->getParentRegion() != &region)
        return;
      numOps++;
      if (auto single = dyn_cast<arts::SingleOp>(nested)) {
        if (singleOp)
          llvm_unreachable("Multiple single ops in parallel op not supported");

        singleOp = single;
      } else if (!isa<arts::BarrierOp>(nested) && !isa<arts::YieldOp>(nested)) {
        llvm_unreachable("Unknown op in parallel op - not supported");
      }
    });

    assert((singleOp && (numOps == 4)) &&
           "Invalid parallel op region structure");
    auto &singleRegion = singleOp->getRegion(0);

    /// Process dependencies (makes memrefs -> datablocks)
    auto deps = op.getDeps();
    for (auto dep : deps) {
      auto dbOp = dyn_cast<arts::DataBlockOp>(dep.getDefiningOp());
      assert(dbOp && "Dependency is not a datablock op");
      AC.getOrCreateDatablock(dbOp, loc);
    }
  
    /// We set insertion to the old op for creation of epoch, etc.
    OpBuilder::InsertionGuard guard(AC.getBuilder());
    AC.setInsertionPoint(op);

    /// Epoch done
    EdtCodegen parDoneEdt(AC);
    parDoneEdt.setDepC(AC.createIntConstant(1, AC.Int32, loc));
    parDoneEdt.build(loc, rewriter);

    /// Parallel Epoch
    auto parDone_slot = AC.createIntConstant(0, AC.Int32, loc);
    auto parEpoch_guid =
        AC.createEpoch(parDoneEdt.getGuid(), parDone_slot, loc);

    /// Parallel Edt
    auto par_params = op.getParams();
    auto par_deps = op.getDeps();
    auto par_consts = op.getConsts();
    AC.createEdt(&par_deps, &par_params, &par_consts, &singleRegion,
                 &parEpoch_guid, nullptr, true, &rewriter);
    /// Finally, erase the old parallel op from the IR
    rewriter.eraseOp(op);

    return success();
  }

private:
  ArtsCodegen &AC;
};

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
struct ConvertARTSToFuncsPass
    : public arts::ConvertARTSToFuncsBase<ConvertARTSToFuncsPass> {
  void runOnOperation() override;
};
} // end namespace

void ConvertARTSToFuncsPass::runOnOperation() {
  LLVM_DEBUG(dbgs() << "--- ConvertARTSToFuncsPass START ---\n");

  ModuleOp module = getOperation();
  MLIRContext *ctx = &getContext();

  /// Datalayouts
  auto llvmDLAttr = module->getAttrOfType<StringAttr>("llvm.data_layout");
  if (!llvmDLAttr) {
    module.emitError("Module does not have a data layout");
    return signalPassFailure();
  }
  llvm::DataLayout llvmDL(llvmDLAttr.getValue().str());
  mlir::DataLayout mlirDL(module);

  // Initialize the AC object
  OpBuilder builder(ctx);
  ArtsCodegen AC(module, builder, llvmDL, mlirDL);

  /// Handle datablocks first
  /// It analyzes the datablock ops and creates the necessary data structures
  // module->walk([&](arts::DataBlockOp dbOp) {
  //   AC.createDatablock(dbOp, UnknownLoc::get(ctx));
  // });

  // Create a ConversionTarget that declares ARTS dialect ops illegal
  ConversionTarget target(*ctx);
  target.addIllegalOp<arts::ParallelOp>();

  /// Mark other dialects as legal
  target.addLegalDialect<arith::ArithDialect, cf::ControlFlowDialect,
                         func::FuncDialect, memref::MemRefDialect,
                         affine::AffineDialect, polygeist::PolygeistDialect,
                         scf::SCFDialect>();
  target.addLegalOp<ModuleOp>();
  // target.addLegalOp<arts::ParallelOp>();
  target.addLegalOp<arts::EdtOp>();
  target.addLegalOp<arts::AllocaOp>();
  target.addLegalOp<arts::DataBlockOp>();
  target.addLegalOp<arts::YieldOp>();
  target.addLegalOp<arts::BarrierOp>();

  // (Optional) If you have custom ARTS types to convert, define a
  // TypeConverter TypeConverter typeConverter;

  // Pattern list
  RewritePatternSet patterns(ctx);
  patterns.add<ParallelOpLowering>(ctx, AC);
  // patterns.add<MakeDepOpLowering>(ctx, AC);
  // If you have arts::EdtOp, arts::EpochOp, etc., add them:
  // patterns.add<EdtOpLowering>(ctx, AC);
  // patterns.add<EpochOpLowering>(ctx, AC);
  // patterns.add<DatablockOpLowering>(ctx, AC);

  // 6) Apply partial conversion
  if (failed(applyPartialConversion(module, target, std::move(patterns)))) {
    LLVM_DEBUG(dbgs() << "Conversion failed.\n");
    module.dump();
    signalPassFailure();
    return;
  }

  LLVM_DEBUG(dbgs() << "=== ConvertARTSToFuncsPass COMPLETE ===\n");
  module.dump();
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
