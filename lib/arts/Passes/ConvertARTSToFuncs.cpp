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
struct EdtOpLowering : public OpConversionPattern<arts::EdtOp> {
  using OpConversionPattern<arts::EdtOp>::OpConversionPattern;

  EdtOpLowering(MLIRContext *context, ArtsCodegen &codegen)
      : OpConversionPattern(context), codegen(codegen) {}

  LogicalResult
  matchAndRewrite(arts::EdtOp op, arts::EdtOp::Adaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {

    LLVM_DEBUG(DBGS() << "Lowering arts.edt: " << op << "\n");
    LLVM_DEBUG(DBGS() << "Processing edt op\n");
    return success();
  }

private:
  ArtsCodegen &codegen;
};

struct ParallelOpLowering : public OpConversionPattern<arts::ParallelOp> {
  using OpConversionPattern<arts::ParallelOp>::OpConversionPattern;

  ParallelOpLowering(MLIRContext *context, ArtsCodegen &codegen)
      : OpConversionPattern(context), codegen(codegen) {}

  LogicalResult
  matchAndRewrite(arts::ParallelOp op, arts::ParallelOp::Adaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    LLVM_DEBUG(DBGS() << "Lowering arts.parallel\n" << op << "\n");

    Location loc = UnknownLoc::get(op.getContext());
    arts::SingleOp singleOp = nullptr;
    unsigned numOps = 0;

    // // /// Analyze the parallel region to find the singleOp
    // // mlir::Region &region = op.getRegion();
    // // region.walk([&](mlir::Operation *nested) {
    // //   /// Only consider ops directly in this region
    // //   if (nested->getParentRegion() != &region)
    // //     return;
    // //   numOps++;
    // //   if (auto single = dyn_cast<arts::SingleOp>(nested)) {
    // //     if (singleOp) {
    // //       llvm_unreachable("Multiple single ops in parallel op not supported");
    // //     }
    // //     singleOp = single;
    // //   } else if (!isa<arts::BarrierOp>(nested) && !isa<arts::YieldOp>(nested)) {
    // //     llvm_unreachable("Unknown op in parallel op - not supported");
    // //   }
    // // });

    // // assert((singleOp && (numOps == 4)) &&
    // //        "Invalid parallel op region structure");
    // // auto &singleRegion = singleOp->getRegion(0);

    // // /// Process dependencies (makes memrefs -> datablocks)
    // // auto deps = op.getDependencies();
    // // auto params = op.getParameters();
    
    // // for (auto dep : deps) {
    // //   auto makeDepOp = dyn_cast<arts::DataBlockOp>(dep.getDefiningOp());
    // //   assert(makeDepOp && "Dependency is not a datablock op");
    // //   codegen.getOrCreateDatablock(makeDepOp, loc);
    // // }

    // // auto parDone_edtFunc = codegen.createFn(loc);
    // // auto par_edtFunc = codegen.createFn(loc);

    // // /// We set insertion to the old op for creation of epoch, etc.
    // // OpBuilder::InsertionGuard guard(codegen.getBuilder());
    // // codegen.setInsertionPoint(op);

    // // auto currentNode = codegen.getCurrentNode(loc);

    // // /// Create Parallel Epoch
    // // auto parDone_depC = codegen.createIntConstant(1, codegen.Int32, loc);
    // // auto parDone_params = SmallVector<Value>{};
    // // auto parDone_edtGuid = codegen.create(parDone_edtFunc, parDone_params,
    // //                                          parDone_depC, currentNode, loc);
    // // auto parDone_slot = codegen.createIntConstant(0, codegen.Int32, loc);
    // // auto parEpoch_guid =
    // //     codegen.createEpoch(parDone_edtGuid, parDone_slot, loc);

    // // /// Create Parallel Edt
    // // auto par_params = op.getParams();
    // // auto par_deps = op.getDeps();
    // // auto par_depC = codegen.getNumDeps(par_deps, loc);
    // // auto parallelEdtGuid = codegen.createWithEpoch(
    // //     par_edtFunc, par_params, par_depC, currentNode, parEpoch_guid, loc);

    // // /// Insert Parallel Edt entry
    // // auto par_consts = op.getConsts();
    // // auto rewireMap = DenseMap<Value, Value>();
    // // codegen.insertFnEntry(par_edtFunc, singleRegion, par_params,
    // //                        par_consts, par_deps, rewireMap);

    // /// Inline the single region into the parallel edt function
    // Region &funcRegion = par_edtFunc.getBody();
    // Block &funcEntryBlock = funcRegion.front();
    // Block &entryBlock = singleRegion.front();
    // rewriter.inlineRegionBefore(singleRegion, funcRegion, funcRegion.end());

    // /// Move all ops from srcBlock to the end of destBlock
    // LLVM_DEBUG(dbgs() << "\n\nMoving ops to the end of the function\n");
    // while (!entryBlock.empty()) {
    //   Operation &op = entryBlock.front();
    //   op.moveBefore(&funcEntryBlock, funcEntryBlock.end());
    // }
    // rewriter.eraseBlock(&entryBlock);
    // par_edtFunc.dump();

    /// Finally, erase the old parallel op from the IR
    rewriter.eraseOp(op);

    return success();
  }

private:
  ArtsCodegen &codegen;
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

  // Initialize the codegen object
  OpBuilder builder(ctx);
  ArtsCodegen codegen(module, builder, llvmDL, mlirDL);

  // Create a ConversionTarget that declares ARTS dialect ops illegal
  ConversionTarget target(*ctx);
  target.addIllegalOp<arts::ParallelOp>();

  /// Mark other dialects as legal
  target.addLegalDialect<arith::ArithDialect, cf::ControlFlowDialect,
                         func::FuncDialect, memref::MemRefDialect,
                         affine::AffineDialect, polygeist::PolygeistDialect,
                         scf::SCFDialect>();
  target.addLegalOp<ModuleOp>();
  target.addLegalOp<arts::EdtOp>();
  target.addLegalOp<arts::AllocaOp>();
  target.addLegalOp<arts::DataBlockOp>();
  target.addLegalOp<arts::YieldOp>();
  target.addLegalOp<arts::BarrierOp>();

  // (Optional) If you have custom ARTS types to convert, define a TypeConverter
  TypeConverter typeConverter;

  // Pattern list
  RewritePatternSet patterns(ctx);
  patterns.add<ParallelOpLowering>(ctx, codegen);
  // patterns.add<MakeDepOpLowering>(ctx, codegen);
  // If you have arts::EdtOp, arts::EpochOp, etc., add them:
  // patterns.add<EdtOpLowering>(ctx, codegen);
  // patterns.add<EpochOpLowering>(ctx, codegen);

  // 6) Apply partial conversion
  if (failed(applyPartialConversion(module, target, std::move(patterns)))) {
    LLVM_DEBUG(dbgs() << "Conversion failed.\n");
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
