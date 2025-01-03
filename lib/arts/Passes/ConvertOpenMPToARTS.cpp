//===----------------- ConvertOpenMPToARTSHierarchical.cpp  --------------===//
//
// This file implements a module pass that converts OpenMP ops
// (omp.parallel, omp.master, omp.task, etc.) into ARTS ops
// (arts.parallel, arts.single, arts.edt).
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/DialectConversion.h"
#include "mlir/Transforms/RegionUtils.h"

#include "llvm/ADT/SmallVector.h"

#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "ArtsPassDetails.h"


#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "convert-openmp-to-arts"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace std;
using namespace mlir;
using namespace arts;

namespace {
struct ConvertOpenMPToARTSPass
    : public mlir::arts::ConvertOpenMPToARTSBase<ConvertOpenMPToARTSPass> {
  void runOnOperation() override;
};
} // namespace

/// Collects parameters for any OpenMP region.
/// Parameters are values used in the region but defined outside it.
// static SmallVector<Value> collectParameters(Region &region) {
//   SmallVector<Value> parameters;
//   // Iterate through all operations in the region to collect external values
//   region.walk([&](Operation *op) {
//     for (Value operand : op->getOperands()) {
//       if (auto definingOp = operand.getDefiningOp()) {
//         // If the operand is defined outside the region, it's a parameter
//         if (!region.isAncestor(definingOp->getParentRegion())) {
//           if (llvm::find(parameters, operand) == parameters.end()) {
//             parameters.push_back(operand);
//           }
//         }
//       } else if (operand.isa<BlockArgument>()) {
//         // Handle block arguments (e.g., loop-carried values)
//         auto blockArg = operand.cast<BlockArgument>();
//         if (!region.isAncestor(blockArg.getParentRegion())) {
//           if (llvm::find(parameters, operand) == parameters.end()) {
//             parameters.push_back(operand);
//           }
//         }
//       }
//     }
//   });

/// Collects parameters for the EdtOp
static ArrayAttr collectParameters(Region &region, PatternRewriter &rewriter) {
  SmallVector<Attribute> parameterAttrs;

  // Collect all external values (parameters) used in the region
  region.walk([&](Operation *op) {
    for (Value operand : op->getOperands()) {
      if (!region.isAncestor(operand.getParentRegion())) {
        // Convert Value to Attribute using the rewriter context
        parameterAttrs.push_back(rewriter.getIndexAttr(reinterpret_cast<int64_t>(operand.getAsOpaquePointer())));
      }
    }
  });

  // Create and return ArrayAttr for the parameters
  return rewriter.getArrayAttr(parameterAttrs);
}

/// Pattern to replace `omp.parallel` with `arts.parallel`
struct ParallelToARTSPattern : public OpRewritePattern<omp::ParallelOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::ParallelOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();

    /// Create a new `arts.parallel` operation.
    auto artsPar = rewriter.create<arts::ParallelOp>(loc);
    artsPar.getBody().emplaceBlock();
    Block &blk = artsPar.getBody().front();

    /// Move the region's operations.
    Block &old = op.getRegion().front();
    blk.getOperations().splice(blk.end(), old.getOperations());

    /// Remove the original operation.
    rewriter.eraseOp(op);
    return success();
  }
};
;

/// Pattern to replace `omp.master` with `arts.single`
struct MasterToARTSPattern : public OpRewritePattern<omp::MasterOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::MasterOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();

    /// Create a new `arts.single` operation.
    auto artsSingle = rewriter.create<arts::SingleOp>(loc);
    artsSingle.getBody().emplaceBlock();
    Block &blk = artsSingle.getBody().front();

    /// Move the region's operations.
    Block &old = op.getRegion().front();
    blk.getOperations().splice(blk.end(), old.getOperations());

    /// Remove the original operation.
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace `omp.task` with `arts.edt`
struct TaskToARTSPattern : public OpRewritePattern<omp::TaskOp> {
  using OpRewritePattern::OpRewritePattern;

  // ArrayAttr getDepArray(omp::TaskOp task, PatternRewriter &rewriter) const {
  //   SmallVector<Attribute, 4> dependencies;
  //   auto dependList = task.getDependsAttr();
  //   for (unsigned i = 0, e = dependList.size(); i < e; ++i) {
  //     auto depClause =
  //         llvm::cast<mlir::omp::ClauseTaskDependAttr>(dependList[i]);
  //     auto depType = stringifyClauseTaskDepend(depClause.getValue());
  //     auto depVar = task.getDependVars()[i];
  //     /// Process indices
  //     SmallVector<Attribute> indexAttrs;
  //     LLVM_DEBUG(dbgs() << "Depend clause: " << depType << " -> " << depVar
  //                       << "\n");
  //     auto dep = rewriter.create<arts::MakeDepAffineOp>(
  //           loc, rewriter.getStringAttr(mode), memref,
  //           rewriter.getAffineMapAttr(map), operands);
  //     // Build a dictionary for each dependency.
  //     // auto depDict = rewriter.getDictionaryAttr(
  //     //     {rewriter.getNamedAttr("mode", rewriter.getStringAttr(depType)),
  //     //      rewriter.getNamedAttr("memref", FlatSymbolRefAttr::get(subView)),
  //     //      rewriter.getNamedAttr("indices",
  //     //                            rewriter.getArrayAttr(indexAttrs))});
  //     // dependencies.push_back(depDict);
  //   }
  //   return rewriter.getArrayAttr(dependencies);
  // }

  LogicalResult matchAndRewrite(omp::TaskOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();

    /// Placeholder: build dependency and parameter arrays.
    // SmallVector<Value> params;
    ArrayAttr parameters = collectParameters(op.getRegion(), rewriter);
    SmallVector<Value> dependencies;

    /// Create a new `arts.edt` operation.
    auto edtOp = rewriter.create<arts::EdtOp>(loc, parameters, dependencies);
    edtOp.getBody().emplaceBlock();
    Block &blk = edtOp.getBody().front();

    /// Move the region's operations.
    Block &old = op.getRegion().front();
    blk.getOperations().splice(blk.end(), old.getOperations());

    /// Remove the original `omp.task`.
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace all terminators with `arts.yield`
struct TerminatorToARTSPattern : public OpRewritePattern<omp::TerminatorOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::TerminatorOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    rewriter.create<arts::YieldOp>(loc);
    rewriter.eraseOp(op);
    return success();
  }
};

void ConvertOpenMPToARTSPass::runOnOperation() {
  LLVM_DEBUG(dbgs() << line << "ConvertOpenMPToARTSPass STARTED\n" << line);
  MLIRContext *context = &getContext();
  RewritePatternSet patterns(context);
  patterns.add<ParallelToARTSPattern, MasterToARTSPattern, TaskToARTSPattern,
               TerminatorToARTSPattern>(context);
  GreedyRewriteConfig config;
  if (failed(applyPatternsAndFoldGreedily(getOperation(), std::move(patterns),
                                          config))) {
    LLVM_DEBUG(dbgs() << "Conversion failed\n");
    signalPassFailure();
    return;
  }
  LLVM_DEBUG(dbgs() << line << "ConvertOpenMPToARTSPass FINISHED\n" << line);
  auto Module = getOperation();
  Module->dump();
}
//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
unique_ptr<Pass> createConvertOpenMPtoARTSPass() {
  return std::make_unique<ConvertOpenMPToARTSPass>();
}
} // namespace arts
} // namespace mlir