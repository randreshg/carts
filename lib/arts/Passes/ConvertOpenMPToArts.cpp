///==========================================================================
/// File: ConvertOpenMPToArts.cpp
//
// This file implements a module pass that converts OpenMP ops
// (omp.parallel, omp.master, omp.task, etc.) into ARTS ops
///==========================================================================

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "polygeist/Ops.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsIR.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsTypes.h"
#include "arts/Utils/ArtsUtils.h"
/// Others
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/DialectConversion.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
/// Debug
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#define DEBUG_TYPE "convert-openmp-to-arts"
#define LINE "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;

//===----------------------------------------------------------------------===//
// Conversion Patterns
//===----------------------------------------------------------------------===//
/// Pattern to replace `omp.parallel` with `arts.edt` with `parallel` attribute
struct OMPParallelToARTSPattern : public OpRewritePattern<omp::ParallelOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::ParallelOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    LLVM_DEBUG(DBGS() << "Converting omp.parallel to arts.parallel\n");

    /// Create a new `arts.edt` operation.
    auto parOp = rewriter.create<EdtOp>(loc, EdtType::parallel);
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

/// Pattern to replace 'scf.parallel' with an 'scf.for' loop that creates an
/// 'arts.edt' operation and embodies the operation in an 'arts.epoch'
/// operation.
struct SCFParallelToArtsPattern : public OpRewritePattern<scf::ParallelOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(scf::ParallelOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    LLVM_DEBUG(DBGS() << "Converting scf.parallel to arts.parallel\n");
    rewriter.setInsertionPoint(op);

    /// Create an `arts.epoch` operation and add a region to it.
    auto syncEdtOp = rewriter.create<EdtOp>(loc, EdtType::sync);
    Block &syncEdtBlock = syncEdtOp.getBody().emplaceBlock();

    /// Create the for loop inside the epoch
    rewriter.setInsertionPointToStart(&syncEdtBlock);
    Value lb = op.getLowerBound().front();
    Value ub = op.getUpperBound().front();
    Value st = op.getStep().front();
    auto forOp = rewriter.create<scf::ForOp>(loc, lb, ub, st);
    Block *forBody = forOp.getBody();
    Value inductionVar = forOp.getInductionVar();

    /// Create a new EDT operation inside the for loop body
    rewriter.setInsertionPointToStart(forBody);
    auto edtOp = rewriter.create<EdtOp>(loc, EdtType::task);
    Block &edtBlock = edtOp.getBody().emplaceBlock();
    rewriter.setInsertionPointToStart(&edtBlock);

    /// Map the original parallel loop induction variable to our new for loop
    /// induction variable
    IRMapping mapper;
    if (!op.getInductionVars().empty())
      mapper.map(op.getInductionVars().front(), inductionVar);

    /// Clone the original parallel loop body operations into the EDT body
    Block &srcBlock = op.getRegion().front();
    for (Operation &srcOp : srcBlock.without_terminator())
      rewriter.clone(srcOp, mapper);

    /// Add the yield operation to the EDT body
    rewriter.create<arts::YieldOp>(loc);

    /// Add the yield operation to the epoch block
    rewriter.setInsertionPointToEnd(&syncEdtBlock);
    rewriter.create<arts::YieldOp>(loc);

    /// Remove the original operation
    rewriter.eraseOp(op);

    return success();
  }
};

/// Pattern to replace `omp.master` with `arts.edt` with `single` attribute
struct MasterToARTSPattern : public OpRewritePattern<omp::MasterOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::MasterOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();

    /// Create a new `arts.single` operation.
    auto artsSingle = rewriter.create<EdtOp>(loc, EdtType::single);
    artsSingle.getBody().emplaceBlock();

    /// Move the region's operations.
    Block &old = op.getRegion().front();
    Block &blk = artsSingle.getBody().front();
    blk.getOperations().splice(blk.end(), old.getOperations());

    /// Remove the original operation.
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace `omp.task` with `arts.edt`
struct TaskToARTSPattern : public OpRewritePattern<omp::TaskOp> {
  using OpRewritePattern::OpRewritePattern;

  ArtsMode getDbMode(omp::ClauseTaskDepend taskClause) const {
    switch (taskClause) {
    case omp::ClauseTaskDepend::taskdependin:
      return ArtsMode::in;
    case omp::ClauseTaskDepend::taskdependout:
      return ArtsMode::out;
    case omp::ClauseTaskDepend::taskdependinout:
      return ArtsMode::inout;
    }
    llvm_unreachable("Unknown ClauseTaskDepend value");
  }

  void collectTaskDependencies(SmallVector<Value> &deps, omp::TaskOp task,
                               PatternRewriter &rewriter, Location loc) const {

    /// Collect the task deps clause
    auto dependList = task.getDependsAttr();
    if (!dependList) {
      LLVM_DEBUG(DBGS() << "No dependencies found for task\n");
      return;
    }
    LLVM_DEBUG(DBGS() << "Processing " << dependList.size()
                      << " dependencies\n");
    for (unsigned i = 0, e = dependList.size(); i < e; ++i) {
      /// Get dependency clause and type.
      auto depClause = dyn_cast<omp::ClauseTaskDependAttr>(dependList[i]);
      assert(depClause && "Expected a ClauseTaskDependAttr");
      Value depVar = task.getDependVars()[i];

      /// Ensure the dependency variable comes from a memref.alloc operation.
      auto depAlloc = depVar.getDefiningOp<memref::AllocaOp>();
      assert(depAlloc && "Expected a memref.alloc operation");

      /// Find the first user (except the task itself) that is an memref.store
      /// op.
      memref::StoreOp depStoreOp = nullptr;
      for (Operation *user : depVar.getUsers()) {
        if (user == task)
          continue;
        if ((depStoreOp = dyn_cast<memref::StoreOp>(user)))
          break;
      }
      assert(depStoreOp && "Expected an memref.store operation");

      /// Get the value that was stored; expect a memref.load
      Operation *valueDefOp = depStoreOp.getValueToStore().getDefiningOp();
      Value depLoadVal;
      if (auto loadOp = dyn_cast<memref::LoadOp>(valueDefOp))
        depLoadVal = loadOp.getResult();
      else
        llvm_unreachable("Expected a memref.load operation");

      /// Clone the load operation at the beginning of the edt region.
      auto &region = task.getRegion();
      {
        OpBuilder::InsertionGuard IG(rewriter);
        rewriter.setInsertionPointToStart(&region.front());

        Operation *defOp = depLoadVal.getDefiningOp();
        auto loadOp = cast<memref::LoadOp>(defOp);
        auto newLoad = rewriter.create<memref::LoadOp>(loc, loadOp.getMemref(),
                                                       loadOp.getIndices());
        replaceInRegion(region, loadOp.getResult(), newLoad.getResult());
      }

      /// Create the control dependency (will be removed in CreateDbs pass)
      {
        OpBuilder::InsertionGuard IG(rewriter);
        rewriter.setInsertionPointAfter(depLoadVal.getDefiningOp());
        SmallVector<Value> emptyIndices;
        /// Create db_control and add to deps
        Value dbControl = createDbControlOp(rewriter, depLoadVal.getLoc(),
                                            getDbMode(depClause.getValue()),
                                            depLoadVal, emptyIndices);
        deps.push_back(dbControl);
      }

      /// Replace the dependency allocation with an undefined value.
      replaceWithUndef(depAlloc.getOperation(), rewriter);
    }
  }

  LogicalResult matchAndRewrite(omp::TaskOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    LLVM_DEBUG(DBGS() << "Converting omp.task to arts.edt\n");
    /// Collect dependencies
    SmallVector<Value> deps;
    collectTaskDependencies(deps, op, rewriter, loc);

    /// Create a new `arts.edt` operation.
    auto edtOp = rewriter.create<EdtOp>(loc, EdtType::task, deps);
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

/// Pattern to replace `omp.taskwait` with `arts.barrier`
struct TaskwaitToARTSPattern : public OpRewritePattern<omp::TaskwaitOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::TaskwaitOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    rewriter.create<arts::BarrierOp>(loc);
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace 'memref.alloc' with 'arts.alloc'
struct AllocToARTSPattern : public OpRewritePattern<memref::AllocOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(memref::AllocOp allocOp,
                                PatternRewriter &rewriter) const override {
    /// Get the location of the original alloc operation.
    Location loc = allocOp.getLoc();

    /// Get the memref type from the alloc operation.
    auto memRefType = allocOp.getType().dyn_cast<MemRefType>();
    if (!memRefType)
      return failure();

    /// Collect the dynamic sizes of the memref (if any).
    SmallVector<Value, 4> dynamicSizes;
    for (Value operand : allocOp.getDynamicSizes())
      dynamicSizes.push_back(operand);

    /// Create the arts.alloc operation with the same memref type and dynamic
    /// sizes.
    auto artsAlloc =
        rewriter.create<arts::AllocOp>(loc, memRefType, dynamicSizes);

    /// Replace all uses of the original alloc with the new arts.alloc.
    rewriter.replaceOp(allocOp, artsAlloc.getResult());

    return success();
  }
};

/// Pattern to replace 'func.call' with an equivalent 'arts' call if exists.
struct CallToARTSPattern : public OpRewritePattern<func::CallOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(func::CallOp callOp,
                                PatternRewriter &rewriter) const override {
    auto callee = callOp.getCallee();
    if (callee == "omp_get_thread_num") {
      rewriter.replaceOpWithNewOp<arts::GetCurrentWorkerOp>(callOp);
      return success();
    }
    if (callee == "omp_get_num_threads" || callee == "omp_get_max_threads") {
      rewriter.replaceOpWithNewOp<arts::GetTotalWorkersOp>(callOp);
      return success();
    }
    /// nothing to do, leave the op as-is
    return failure();
  }
};

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct ConvertOpenMPToArtsPass
    : public arts::ConvertOpenMPToArtsBase<ConvertOpenMPToArtsPass> {
  void runOnOperation() override;
};
} // end namespace

/// Pass to convert OpenMP operations to ARTS operations
void ConvertOpenMPToArtsPass::runOnOperation() {
  LLVM_DEBUG(dbgs() << "\n"
                    << LINE << "ConvertOpenMPToArtsPass STARTED\n"
                    << LINE);
  ModuleOp module = getOperation();
  MLIRContext *context = &getContext();
  RewritePatternSet patterns(context);
  patterns.add<OMPParallelToARTSPattern, SCFParallelToArtsPattern,
               MasterToARTSPattern, TaskToARTSPattern, TerminatorToARTSPattern,
               BarrierToARTSPattern, AllocToARTSPattern, TaskwaitToARTSPattern,
               CallToARTSPattern>(context);
  GreedyRewriteConfig config;
  (void)applyPatternsAndFoldGreedily(module, std::move(patterns), config);
  removeUndefOps(module);
  LLVM_DEBUG({
    dbgs() << LINE << "ConvertOpenMPToArtsPass FINISHED\n" << LINE;
    module.dump();
  });
}

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createConvertOpenMPtoARTSPass() {
  return std::make_unique<ConvertOpenMPToArtsPass>();
}
} // namespace arts
} // namespace mlir