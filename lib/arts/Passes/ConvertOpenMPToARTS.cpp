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

/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
/// Debug
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "convert-openmp-to-arts"

#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;

/// Collects parameters for the EdtOp
static void collectParametersAndDependencies(SetVector<Value> &parameters,
                                             SetVector<Value> &dependencies,
                                             Region &region) {
  SetVector<Value> usedValues;
  getUsedValuesDefinedAbove(region, usedValues);
  for (Value operand : usedValues) {
    if (operand.getType().isIntOrIndexOrFloat()) {
      parameters.insert(operand);
      LLVM_DEBUG(dbgs() << "Parameter: " << operand << "\n");
    } else {
      dependencies.insert(operand);
      LLVM_DEBUG(dbgs() << "Dependencies: " << operand << "\n");
    }
  }
}

//===----------------------------------------------------------------------===//
// Conversion Patterns
//===----------------------------------------------------------------------===//
/// Pattern to replace `omp.parallel` with `arts.parallel`
struct ParallelToARTSPattern : public OpRewritePattern<omp::ParallelOp> {
  using OpRewritePattern::OpRewritePattern;

  /// Create a new `arts.make_dep` operation from a memref operation
  SmallVector<Value>
  createDepsFromMemRefOps(const SetVector<Value> &dependencies,
                          PatternRewriter &rewriter, Location loc) const {
    SmallVector<Value> deps;
    deps.reserve(dependencies.size());
    const auto depType = "inout";
    for (const auto &memref : dependencies) {
      deps.push_back(rewriter.create<arts::MakeDepOp>(loc, depType, memref));
    }
    return deps;
  }

  LogicalResult matchAndRewrite(omp::ParallelOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    LLVM_DEBUG(dbgs() << line << "Converting omp.parallel to arts.parallel\n"
                      << line);
    /// Collect parameters and dependencies
    SetVector<Value> parameters, dependencies;
    collectParametersAndDependencies(parameters, dependencies, op.getRegion());
    SmallVector<Value> deps =
        createDepsFromMemRefOps(dependencies, rewriter, loc);
    /// Create a new `arts.parallel` operation.
    auto parOp =
        rewriter.create<arts::ParallelOp>(loc, parameters.getArrayRef(), deps);
    parOp.getBody().emplaceBlock();
    Block &blk = parOp.getBody().front();

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
    llvm_unreachable("Unknown ClauseTaskDepend value");
  }

  void collectTaskDependencies(SetVector<Value> &dependencies, omp::TaskOp task,
                               PatternRewriter &rewriter, Location loc) const {
    LLVM_DEBUG(dbgs() << "Collecting dependencies\n");
    auto dependList = task.getDependsAttr();
    for (unsigned i = 0, e = dependList.size(); i < e; ++i) {
      auto depClause = llvm::cast<omp::ClauseTaskDependAttr>(dependList[i]);
      auto depType = stringifyTaskDepend(depClause.getValue());
      auto depVar = task.getDependVars()[i];

      /// It must be a memref.alloc operation
      auto depAlloc = depVar.getDefiningOp<memref::AllocaOp>();
      assert(depAlloc && "Expected a memref.alloc operation");

      /// Check if the first user is an affine.store
      affine::AffineStoreOp depStoreOp = nullptr;
      for (Operation *user : depVar.getUsers()) {
        if (user == task)
          continue;
        depStoreOp = dyn_cast<affine::AffineStoreOp>(user);
        break;
      }
      assert(depStoreOp && "Expected an affine.store operation");

      /// The value to store must be an affine.load
      auto depLoadOp = dyn_cast<affine::AffineLoadOp>(
          depStoreOp.getValueToStore().getDefiningOp());
      assert(depLoadOp && "Expected an affine.load operation");

      /// Get affine operands
      auto memref = depLoadOp.getMemRef();
      auto affineMap = depLoadOp.getAffineMap();
      SmallVector<Value> operands(depStoreOp.getIndices().begin(),
                                  depStoreOp.getIndices().end());

      /// Use the affineMapAttr as needed
      auto depOp = rewriter.create<arts::MakeDepOp>(loc, depType, memref,
                                                    affineMap, operands);
      LLVM_DEBUG(dbgs() << "  - Dependency: " << depOp << "\n");

      /// Before adding it, verify if the memref is already in the list, if it
      /// is, remove it and add the new one
      if (dependencies.contains(memref)) {
        LLVM_DEBUG(dbgs() << "    - Removing previous dependency: " << memref
                          << "\n");
        dependencies.remove(memref);
      }
      dependencies.insert(depOp.getResult());

      /// Remove the original operations
      replaceWithUndef(depAlloc, rewriter);
      replaceWithUndef(depLoadOp, rewriter);
    }

    /// If there is any dependency that is not the result of a MakeDepOp, create
    /// it
    for (auto dep : dependencies) {
      if (!isa<arts::MakeDepOp>(dep.getDefiningOp())) {
        auto depOp = rewriter.create<arts::MakeDepOp>(loc, "inout", dep);
        dependencies.remove(dep);
        dependencies.insert(depOp.getResult());
      }
    }
  }

  LogicalResult matchAndRewrite(omp::TaskOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    LLVM_DEBUG(dbgs() << line << "Converting omp.task to arts.edt\n"
                      << line << op << "\n");
    /// Collect parameters and dependencies
    SetVector<Value> parameters, dependencies;
    collectParametersAndDependencies(parameters, dependencies, op.getRegion());
    collectTaskDependencies(dependencies, op, rewriter, loc);
    /// Create a new `arts.edt` operation.
    auto edtOp = rewriter.create<arts::EdtOp>(loc, parameters.getArrayRef(),
                                              dependencies.getArrayRef());
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

/// Pattern to replace `omp.terminator` with `arts.yield`
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

/// Pattern to replace `omp.barrier` with `arts.barrier`
struct BarrierToARTSPattern : public OpRewritePattern<omp::BarrierOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::BarrierOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    rewriter.create<arts::BarrierOp>(loc);
    rewriter.eraseOp(op);
    return success();
  }
};

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct ConvertOpenMPToARTSPass
    : public mlir::arts::ConvertOpenMPToARTSBase<ConvertOpenMPToARTSPass> {
  void runOnOperation() override;
};
} // end namespace

/// Pass to convert OpenMP operations to ARTS operations
void ConvertOpenMPToARTSPass::runOnOperation() {
  LLVM_DEBUG(dbgs() << line << "ConvertOpenMPToARTSPass STARTED\n" << line);
  auto module = getOperation();
  MLIRContext *context = &getContext();
  RewritePatternSet patterns(context);
  patterns.add<ParallelToARTSPattern, MasterToARTSPattern, TaskToARTSPattern,
               TerminatorToARTSPattern, BarrierToARTSPattern>(context);
  GreedyRewriteConfig config;
  if (failed(applyPatternsAndFoldGreedily(module, std::move(patterns),
                                          config))) {
    LLVM_DEBUG(dbgs() << "Conversion failed\n");
    signalPassFailure();
    return;
  }

  /// Remove all UndefOps
  removeUndefOps(module);

  LLVM_DEBUG(dbgs() << line << "ConvertOpenMPToARTSPass FINISHED\n" << line);
}

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createConvertOpenMPtoARTSPass() {
  return std::make_unique<ConvertOpenMPToARTSPass>();
}
} // namespace arts
} // namespace mlir