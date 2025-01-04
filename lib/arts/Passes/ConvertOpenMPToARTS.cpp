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

#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Passes/ArtsUtils.h"

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

/// Collects parameters for the EdtOp
static void collectParameters(SmallVectorImpl<Value> &parameters,
                              Region &region) {
  region.walk([&](Operation *op) {
    for (Value operand : op->getOperands()) {
      if (!region.isAncestor(operand.getParentRegion())) {
        /// Avoid duplicate entries
        if (llvm::is_contained(parameters, operand))
          continue;
        /// Check if it is a valid parameter
        if (operand.getType().isa<IntegerType>() ||
            operand.getType().isa<IndexType>() ||
            operand.getType().isa<FloatType>()) {
          parameters.push_back(operand);
        } else {
          /// @TODO: Handle other types - They become a dependency
        }
      }
    }
  });
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

  llvm::StringRef stringifyTaskDepend(omp::ClauseTaskDepend val) const {
    switch (val) {
    case omp::ClauseTaskDepend::taskdependin:
      return "in";
    case omp::ClauseTaskDepend::taskdependout:
      return "out";
    case omp::ClauseTaskDepend::taskdependinout:
      return "inout";
    }
    return "";
  }

  void collectDependencies(SmallVectorImpl<Value> &dependencies,
                           omp::TaskOp task, PatternRewriter &rewriter,
                           Location loc) const {
    auto dependList = task.getDependsAttr();
    for (unsigned i = 0, e = dependList.size(); i < e; ++i) {
      auto depClause =
          llvm::cast<mlir::omp::ClauseTaskDependAttr>(dependList[i]);
      auto depType = stringifyTaskDepend(depClause.getValue());
      auto depVar = task.getDependVars()[i];
      /// Process indices
      SmallVector<Attribute> indexAttrs;
      LLVM_DEBUG(dbgs() << "Depend clause: " << depType << " -> " << depVar
                        << "\n");

      /// It must be a memref.alloc operation
      auto depAlloc = depVar.getDefiningOp<mlir::memref::AllocaOp>();
      assert(depAlloc && "Expected a memref.alloc operation");

      /// Check uses of depAlloc
      auto usesLen =
          std::distance(depAlloc->user_begin(), depAlloc->user_end());
      assert(usesLen == 2 && "Expected exactly 2 users for depAlloc");

      /// Check if the first user is an affine.store
      mlir::affine::AffineStoreOp depStoreOp = nullptr;
      for (Operation *user : depVar.getUsers()) {
        /// ignore use in the task
        if (user == task)
          continue;
        /// Check if the user is an affine.store
        depStoreOp = dyn_cast<mlir::affine::AffineStoreOp>(user);
        break;
      }
      assert(depStoreOp && "Expected an affine.store operation");
      /// The value to store must be an affine.load
      auto depLoadOp = dyn_cast<mlir::affine::AffineLoadOp>(
          depStoreOp.getValueToStore().getDefiningOp());
      assert(depLoadOp && "Expected an affine.load operation");

      /// Get affine operands
      auto memref = depLoadOp.getMemRef();
      auto valToLoad = depLoadOp.getValue();
      auto affineMap = depLoadOp.getAffineMap();
      auto indices = depStoreOp.getIndices();
      SmallVector<Value> operands(indices.begin(), indices.end());

      LLVM_DEBUG(dbgs() << "    - Memref: " << memref << "\n");
      LLVM_DEBUG(dbgs() << "    - Value: " << valToLoad << "\n");
      LLVM_DEBUG(dbgs() << "    - AffineMap: " << AffineMapAttr::get(affineMap)
                        << "\n");

      /// Use the affineMapAttr as needed
      auto depOp = rewriter.create<arts::MakeDepOp>(loc, depType, memref,
                                                    affineMap, operands);
      /// Add the created dependency to the list
      dependencies.push_back(depOp.getResult());

      /// Remove the original operations
      replaceWithUndef(depAlloc, rewriter);
      replaceWithUndef(depLoadOp, rewriter);
    }
  }

  LogicalResult matchAndRewrite(omp::TaskOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    // SmallVector<Value> params;
    SmallVector<Value> parameters, dependencies;
    collectParameters(parameters, op.getRegion());
    collectDependencies(dependencies, op, rewriter, loc);

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

void recursivelyRemoveUses(mlir::Operation *op) {
  llvm::SmallVector<mlir::Operation *, 8> toRemove;
  /// Collect all dependent operations.
  for (mlir::Value result : op->getResults()) {
    for (mlir::Operation *user : result.getUsers()) {
      toRemove.push_back(user);
    }
  }

  /// Remove dependent operations recursively.
  for (mlir::Operation *userOp : toRemove) {
    recursivelyRemoveUses(userOp);
    userOp->erase();
  }
}

void ConvertOpenMPToARTSPass::runOnOperation() {
  auto Module = getOperation();
  Module->dump();

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
  /// ---- Remove all UndefOps ---- ///
  /// Traverse all operations in the module and collect UndefOps.
  llvm::SmallVector<mlir::arts::UndefOp, 8> undefOps;
  Module.walk(
      [&](mlir::arts::UndefOp undefOp) { undefOps.push_back(undefOp); });
  /// Process each UndefOp and remove its dependent operations.
  for (auto undefOp : undefOps) {
    recursivelyRemoveUses(undefOp);
    undefOp.erase();
  }

  LLVM_DEBUG(dbgs() << line << "ConvertOpenMPToARTSPass FINISHED\n" << line);
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