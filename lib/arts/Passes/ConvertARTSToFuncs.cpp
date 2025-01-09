//===- ConvertARTSToFuncs.cpp - Convert ARTS Dialect to Func Dialect ------===//
//
// This file implements a pass to convert ARTS dialect operations into
// `func` dialect operations.
//
//===----------------------------------------------------------------------===//

// #include "mlir/Conversion/ARTSToFuncs/ARTSToFuncs.h"

// #include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Region.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Analysis/DataLayoutAnalysis.h"
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

/*------------------------------------------------------------------------------
 * A small struct to store info about a loop:
 *   - whether it has a constant upper bound, and
 *   - either that const or the dynamic Value.
 *----------------------------------------------------------------------------*/
struct LoopInfo {
  bool isConstant = false;
  int64_t constUpper = -1;
  mlir::Value dynamicUpper = nullptr;
};

/*------------------------------------------------------------------------------
 * ArtsAnalysis: Helper class to do multi-step analysis on the module
 *----------------------------------------------------------------------------*/
class ArtsAnalysis {
public:
  explicit ArtsAnalysis(mlir::ModuleOp &module, arts::ArtsCodegen &codegen)
      : module(module), codegen(codegen) {}

  void start();
  void detectAndAnalyzeLoops(mlir::ModuleOp module);
  void collectMakeDepOps(mlir::ModuleOp module);
  void analyzeAndProcessOp(mlir::Operation *op);

private:
  /// Data structures for storing "make_dep" results
  struct DepItem {
    std::string mode;
    mlir::Value memRef;
    mlir::Location loc;
  };

  void processEdtOp(arts::EdtOp edt);
  void processParallelOp(arts::ParallelOp parOp);
  void processMakeDepOp(arts::MakeDepOp makeDepOp);
  void processEpochOp(arts::EpochOp epochOp);

private:
  mlir::ModuleOp &module;
  arts::ArtsCodegen &codegen;
  llvm::SmallVector<LoopInfo, 8> loopInfos;
  llvm::SmallVector<DepItem, 8> depsInfo;
};

void ArtsAnalysis::start() {
  /// Start with parallel ops.
  /// This is assuming that parallel ops are the top-level ops.
  module.walk([&](arts::ParallelOp op) { processParallelOp(op); });
}

void ArtsAnalysis::detectAndAnalyzeLoops(mlir::ModuleOp module) {
  loopInfos.clear();
  module.walk([&](mlir::Operation *op) {
    // scf::ForOp
    if (auto forOp = mlir::dyn_cast<mlir::scf::ForOp>(op)) {
      LoopInfo info;
      // Check if upper bound is a constant Index
      if (auto ubCstOp = forOp.getUpperBound()
                             .getDefiningOp<mlir::arith::ConstantIndexOp>()) {
        info.isConstant = true;
        info.constUpper = ubCstOp.value();
        DBGS() << "Found scf.for with constant upper bound = "
               << info.constUpper << "\n";
      } else {
        info.isConstant = false;
        info.dynamicUpper = forOp.getUpperBound();
        DBGS() << "Found scf.for with dynamic upper bound = "
               << info.dynamicUpper << "\n";
      }
      loopInfos.push_back(info);
    }
    // affine.for
    else if (auto affFor = mlir::dyn_cast<affine::AffineForOp>(op)) {
      LoopInfo info;
      if (affFor.hasConstantUpperBound()) {
        info.isConstant = true;
        info.constUpper = affFor.getConstantUpperBound();
        DBGS() << "Found affine.for with constant upper bound = "
               << info.constUpper << "\n";
      } else {
        // We store a placeholder Value if we want it.
        // It's a bit trickier for affine.for,
        // but for demonstration, let's just store the original op as
        // "dynamicUpper".
        info.isConstant = false;
        info.dynamicUpper = affFor.getOperation()->getResult(0);
        // Not typical;
        // a real code might glean from affFor.getUpperBoundMapOperands() ...
        DBGS() << "Found affine.for with dynamic upper bound.\n";
      }
      loopInfos.push_back(info);
    }
  });
  DBGS() << "Collected " << loopInfos.size() << " loops.\n";
}

void ArtsAnalysis::analyzeAndProcessOp(mlir::Operation *op) {
  if (auto edtOp = mlir::dyn_cast<arts::EdtOp>(op)) {
    processEdtOp(edtOp);
  } else if (auto parOp = mlir::dyn_cast<arts::ParallelOp>(op)) {
    processParallelOp(parOp);
  } else if (auto makeDepOp = mlir::dyn_cast<arts::MakeDepOp>(op)) {
    processMakeDepOp(makeDepOp);
  } else if (auto epochOp = mlir::dyn_cast<arts::EpochOp>(op)) {
    processEpochOp(epochOp);
  }
}

void ArtsAnalysis::processEdtOp(arts::EdtOp op) {
  auto currLoc = op.getLoc();
  auto edtFunc = codegen.createEdtFunction(currLoc);
  // auto edtCall = codegen.insertEdtCall(op, edtFunc, currLoc);
}

void ArtsAnalysis::processParallelOp(arts::ParallelOp op) {
  auto currLoc = op.getLoc();
  auto &region = op.getRegion();

  /// If the parallel op has only 3 operands:
  /// arts.barrier->arts.single->arts.barrier
    auto &block = region.front();
    arts::SingleOp *singleOp = nullptr;
    unsigned numOps = 0;

    for (auto &op : block) {
      if (auto single = dyn_cast<arts::SingleOp>(&op)) {
        if (singleOp) {
          llvm_unreachable("Multiple single ops in parallel op not supported");
        }
        singleOp = &single;
      } else if (!isa<arts::BarrierOp>(&op)) {
        llvm_unreachable("Unknown op in parallel op - not supported");
      }
      numOps++;
    }

    assert((singleOp && (numOps == 3)) && "Invalid parallel op");

  /// 1. Create an empty parallel done function
  auto parallelDoneEdtFunc = codegen.createEdtFunction(currLoc);
  ///  Get parameters and dependencies
  auto parameters = op.getParametersValues();
  auto dependencies = op.getDependenciesValues();
  codegen.createEdtCallWithGuid(op, parallelDoneEdtFunc, 0, currLoc);
  // codegen.insertEdtCall(op, parallelDoneEdtFunc, currLoc);
  /// 2. Create an epoch to sync the children edts
  ///    artsGuid_t parallelDoneEdtGuid =
  ///        artsEdtCreate((artsEdt_t)parallelDoneEdt, currentNode, 0, NULL,
  ///        1);
  ///    artsGuid_t parallelDoneEpochGuid =
  ///        artsInitializeAndStartEpoch(parallelDoneEdtGuid, 0);
  /// 3. Analyze the code to get the parameters and dependencies
  /// 4. Create a single instance of the code inside the single op
  ///    uint64_t parallelParams[1] = {(uint64_t)N};
  ///    uint64_t parallelDeps = 2 * N;
  ///    artsGuid_t parallelEdtGuid = artsEdtCreateWithEpoch(
  ///        (artsEdt_t)parallelEdt, currentNode, 1, parallelParams,
  ///        parallelDeps, parallelDoneEpochGuid);
  /// 5. Signal the known dependencies
  ///    artsSignalDbs(A_array, parallelEdtGuid, 0, N);
  ///    artsSignalDbs(B_array, parallelEdtGuid, N, N);
  /// 6. Wait for the parallel epoch to finish
  ///    artsWaitOnHandle(parallelDoneEpochGuid);
}

void ArtsAnalysis::processMakeDepOp(arts::MakeDepOp makeDepOp) {
  DBGS() << "Processing single MakeDepOp at " << makeDepOp.getLoc() << "\n";
  // This was mostly collected in collectMakeDepOps(...).
  // If you want to do more direct rewriting, you can do it here as well.
}

void ArtsAnalysis::processEpochOp(arts::EpochOp epochOp) {
  DBGS() << "Processing arts.epoch at " << epochOp.getLoc() << "\n";
  // Could create a function that outlines the epoch region, etc.
}

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct ConvertARTSToFuncsPass
    : public arts::ConvertARTSToFuncsBase<ConvertARTSToFuncsPass> {
  void runOnOperation() override;
};
} // end namespace

void ConvertARTSToFuncsPass::runOnOperation() {
  LLVM_DEBUG(dbgs() << line << "ConvertARTSToFuncsPass STARTED\n" << line);
  ModuleOp module = getOperation();
  module->dump();
  MLIRContext *context = &getContext();
  OpBuilder builder(context);
  // auto voidPtrTy = LLVM::LLVMPointerType::get(context);
  auto dataLayoutAttr = module->getAttrOfType<mlir::StringAttr>("llvm.data_layout");
  if (!dataLayoutAttr) {
    module.emitError("Module does not have a data layout");
    return signalPassFailure();
  }
  llvm::DataLayout dataLayout(dataLayoutAttr.getValue().str());
  

  ArtsCodegen codegen(module, builder, dataLayout);
  RewritePatternSet patterns(context);
  /// Create our analysis object
  ArtsAnalysis analysis(module, codegen);
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
