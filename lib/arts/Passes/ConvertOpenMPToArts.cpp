//===----------------- ConvertOpenMPToArtsHierarchical.cpp  --------------===//
//
// This file implements a module pass that converts OpenMP ops
// (omp.parallel, omp.master, omp.task, etc.) into ARTS ops
// (arts.parallel, arts.single, arts.edt).
//===----------------------------------------------------------------------===//

/// Dialects
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/BuiltinOps.h"
#include "polygeist/Ops.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/Analysis/EdtAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
/// Others
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/DialectConversion.h"
#include "llvm/ADT/SmallVector.h"
/// Debug
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>

#define DEBUG_TYPE "convert-openmp-to-arts"

#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "\n[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;

/// Create a new datablock operation and rewires the uses
static DataBlockOp createDatablockOp(PatternRewriter &rewriter, Region &region,
                                     Location loc, StringRef mode,
                                     Value inputMemRef) {
  bool isLoad = false;
  auto inputMemRefOp = inputMemRef.getDefiningOp();

  /// If the defining operation is a load op obtain the base memref and
  /// pinned indices.
  Value baseMemRef;
  SmallVector<Value> pinnedIndices;
  AffineMap affineMap;
  if (auto loadOp = dyn_cast<memref::LoadOp>(inputMemRefOp)) {
    baseMemRef = loadOp.getMemref();
    pinnedIndices.assign(loadOp.getIndices().begin(),
                         loadOp.getIndices().end());
    affineMap = AffineMap::getMultiDimIdentityMap(pinnedIndices.size(),
                                                  loadOp.getContext());
    isLoad = true;
  } else if (auto affineLoadOp =
                 dyn_cast<affine::AffineLoadOp>(inputMemRefOp)) {
    baseMemRef = affineLoadOp.getMemref();
    pinnedIndices.assign(affineLoadOp.getIndices().begin(),
                         affineLoadOp.getIndices().end());
    affineMap = affineLoadOp.getAffineMap();
    isLoad = true;
  } else {
    baseMemRef = inputMemRef;
  }

  /// Ensure the input memref is of type MemRefType.
  auto baseType = baseMemRef.getType().dyn_cast<MemRefType>();
  assert(baseType && "Input must be a MemRefType.");

  /// Compute the rank and verify it is positive.
  int64_t rank = baseType.getRank();

  /// Prepare arrays for subview
  const auto pinnedCount = static_cast<int64_t>(pinnedIndices.size());
  SmallVector<Value, 4> offsets(pinnedCount), sizes(rank);
  SmallVector<int64_t, 4> subShape(rank);

  OpBuilder::InsertionGuard IG(rewriter);
  if (inputMemRefOp)
    rewriter.setInsertionPointAfter(inputMemRefOp);

  /// Compute dimension sizes, offsets, sizes, and subshape
  Value oneSize = rewriter.create<arith::ConstantIndexOp>(loc, 1);
  for (int64_t i = 0; i < rank; ++i) {
    Value dimVal =
        baseType.isDynamicDim(i)
            ? rewriter.create<memref::DimOp>(loc, baseMemRef, i).getResult()
            : rewriter
                  .create<arith::ConstantIndexOp>(loc, baseType.getDimSize(i))
                  .getResult();

    if (i < pinnedCount) {
      offsets[i] = pinnedIndices[i];
      sizes[i] = oneSize;
      subShape[i] = 1;
    } else {
      sizes[i] = dimVal;
      subShape[i] = baseType.isDynamicDim(i) ? ShapedType::kDynamic
                                             : baseType.getDimSize(i);
    }
  }

  /// Now build the subview memref type
  auto elementType = baseType.getElementType();
  auto subMemRefType = MemRefType::get(subShape, elementType,
                                       baseType.getLayout().getAffineMap(),
                                       baseType.getMemorySpace());

  /// Insert polygeist.typeSizeOp to get the size of the element type
  auto elementTypeSize = rewriter
                             .create<polygeist::TypeSizeOp>(
                                 loc, rewriter.getIndexType(), elementType)
                             .getResult();

  /// Create the final arts.datablock operation with the affine map attribute.
  auto modeAttr = rewriter.getStringAttr(mode);
  DataBlockOp depOp;
  LLVM_DEBUG(dbgs() << "Creating datablock: " << subMemRefType << " - "
                    << affineMap << "\n");
  if (!affineMap) {
    depOp = rewriter.create<arts::DataBlockOp>(loc, subMemRefType, modeAttr,
                                               baseMemRef, elementType,
                                               elementTypeSize, offsets, sizes);
  } else {
    auto affineMapAttr = AffineMapAttr::get(affineMap);
    depOp = rewriter.create<arts::DataBlockOp>(
        loc, subMemRefType, modeAttr, baseMemRef, elementType, elementTypeSize,
        offsets, sizes, affineMapAttr);
  }

  if (isLoad)
    depOp->setAttr("isLoad", rewriter.getUnitAttr());
  return depOp;
}

static void createDatablocks(EdtEnvManager &edtEnv, PatternRewriter &rewriter) {
  auto &region = edtEnv.getRegion();
  edtEnv.naiveCollection();
  for (const auto &[input, depType] : edtEnv.getDepsToProcess()) {
    LLVM_DEBUG(dbgs() << "Processing dependency: " << input << "\n");
    auto depOp =
        createDatablockOp(rewriter, region, input.getLoc(), depType, input);
    edtEnv.addDependency(depOp.getResult());
  }

  edtEnv.clearDepsToProcess();
  edtEnv.adjust();
}

//===----------------------------------------------------------------------===//
// Conversion Patterns
//===----------------------------------------------------------------------===//
/// Pattern to replace `omp.parallel` with `arts.edt` with `parallel` attribute
struct ParallelToARTSPattern : public OpRewritePattern<omp::ParallelOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::ParallelOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    LLVM_DEBUG(DBGS() << "Converting omp.parallel to arts.parallel\n");
    /// Collect parameters and dependencies
    EdtEnvManager edtEnv(rewriter, op.getRegion());
    createDatablocks(edtEnv, rewriter);
    /// Create a new `arts.edt` operation.
    auto parOp = rewriter.create<arts::EdtOp>(loc, edtEnv.getDependencies());
    parOp.getBody().emplaceBlock();
    Block &blk = parOp.getBody().front();

    /// Add 'parallel' attribute
    parOp->setAttr("parallel", rewriter.getUnitAttr());

    /// Move the region's operations.
    Block &old = op.getRegion().front();
    blk.getOperations().splice(blk.end(), old.getOperations());

    /// Remove the original operation.
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
    SmallVector<Value> deps;
    auto artsSingle = rewriter.create<arts::EdtOp>(loc, deps);
    artsSingle.getBody().emplaceBlock();

    /// Set the 'single' attribute
    artsSingle->setAttr("single", rewriter.getUnitAttr());

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

  StringRef stringifyTaskDepend(omp::ClauseTaskDepend val) const {
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

  void collectTaskDependencies(EdtEnvManager &edtEnv, omp::TaskOp task,
                               PatternRewriter &rewriter, Location loc) const {
    /// Collect the task deps clause
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

      /// The value must be loaded from a memref through a memref.load or
      /// affine.load
      auto valueDefOp = depStoreOp.getValueToStore().getDefiningOp();
      mlir::Value depLoadVal;
      if (auto loadOp = dyn_cast<memref::LoadOp>(valueDefOp))
        depLoadVal = loadOp.getResult();
      else if (auto affineLoadOp = dyn_cast<affine::AffineLoadOp>(valueDefOp))
        depLoadVal = affineLoadOp.getResult();
      assert(depLoadVal && "Expected a memref.load or affine.load operation");

      /// Add the dependency to the edt environment
      edtEnv.addDependency(depLoadVal, depType);

      /// Replace dependency array with undef
      replaceWithUndef(depAlloc, rewriter);
    }

    /// Create datablocks
    createDatablocks(edtEnv, rewriter);
  }

  LogicalResult matchAndRewrite(omp::TaskOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    LLVM_DEBUG(DBGS() << "Converting omp.task to arts.edt\n");
    /// Collect parameters and dependencies
    EdtEnvManager edtEnv(rewriter, op.getRegion());
    collectTaskDependencies(edtEnv, op, rewriter, loc);

    /// Create a new `arts.edt` operation.
    auto edtOp = rewriter.create<arts::EdtOp>(loc, edtEnv.getDependencies());
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
      return failure(); // Not a MemRefType, so fail the match.

    /// Collect the dynamic sizes of the memref (if any).
    SmallVector<Value, 4> dynamicSizes;
    for (Value operand : allocOp.getDynamicSizes()) {
      dynamicSizes.push_back(operand);
    }

    /// Create the arts.alloc operation with the same memref type and dynamic
    /// sizes.
    auto artsAlloc =
        rewriter.create<arts::AllocOp>(loc, memRefType, dynamicSizes);

    /// Replace all uses of the original alloc with the new arts.alloc.
    rewriter.replaceOp(allocOp, artsAlloc.getResult());

    return success();
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
  LLVM_DEBUG(dbgs() << line << "ConvertOpenMPToArtsPass STARTED\n" << line);
  ModuleOp module = getOperation();
  MLIRContext *context = &getContext();
  RewritePatternSet patterns(context);
  patterns
      .add<ParallelToARTSPattern, MasterToARTSPattern, TaskToARTSPattern,
           TerminatorToARTSPattern, BarrierToARTSPattern, AllocToARTSPattern>(
          context);
  GreedyRewriteConfig config;
  if (failed(
          applyPatternsAndFoldGreedily(module, std::move(patterns), config))) {
    LLVM_DEBUG(dbgs() << "Conversion failed\n");
    signalPassFailure();
    return;
  }

  /// Remove all UndefOps
  removeUndefOps(module);
  LLVM_DEBUG(dbgs() << line << "ConvertOpenMPToArtsPass FINISHED\n" << line);
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