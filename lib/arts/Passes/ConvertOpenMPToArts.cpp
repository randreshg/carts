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
#include "mlir/IR/BuiltinOps.h"
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
  /// If input is a memref.load, get the base
  Value baseMemRef = inputMemRef;
  if (auto loadOp = dyn_cast<memref::LoadOp>(inputMemRef.getDefiningOp())) {
    baseMemRef = loadOp.getMemref();
    isLoad = true;
  }

  /// memref type must be MemRefType
  auto baseType = baseMemRef.getType().dyn_cast<MemRefType>();
  assert(baseType && "Input must be a MemRefType.");

  /// Rank must be > 0
  int64_t rank = baseType.getRank();
  assert(rank > 0 && "MemRef rank is 0? Unexpected.");

  /// Indices pinned by load if present
  SmallVector<Value> pinnedIndices;
  if (auto loadOp = dyn_cast<memref::LoadOp>(inputMemRef.getDefiningOp())) {
    pinnedIndices.assign(loadOp.getIndices().begin(),
                         loadOp.getIndices().end());

    /// Identify zeroIndices
    SmallVector<int64_t> zeroIndices;
    for (auto [i, index] : llvm::enumerate(pinnedIndices)) {
      if (auto cstOp = index.getDefiningOp<arith::ConstantIndexOp>()) {
        if (cstOp.value() == 0)
          zeroIndices.push_back(i);
      }
    }

    if (!zeroIndices.empty()) {
      assert(zeroIndices.back() == rank - 1 &&
             "Last index must be zero for now");

      /// Lambda to find a matching load/store in the region
      auto checkOp = [&](Operation *op, ValueRange opIndices) {
        if (opIndices.size() < pinnedIndices.size()) {
          llvm::errs() << "Error: Loading a chunk of the memref is not "
                          "supported yet\n";
          return;
        }
        if (opIndices[zeroIndices.back()] !=
            pinnedIndices[zeroIndices.back()]) {
          pinnedIndices.pop_back();
          zeroIndices.pop_back();
        }
      };

      /// Region walk
      region.walk([&](Operation *op) {
        if (zeroIndices.empty())
          return;
        if (auto load = dyn_cast<memref::LoadOp>(op)) {
          if (load.getMemref() == baseMemRef)
            checkOp(op, load.getIndices());
        } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
          if (store.getMemref() == baseMemRef)
            checkOp(op, store.getIndices());
        }
      });
    }
  }

  /// Prepare arrays for subview
  SmallVector<Value, 4> offsets(rank), sizes(rank), strides(rank);
  SmallVector<int64_t, 4> subShape(rank);

  OpBuilder::InsertionGuard g(rewriter);

  /// Compute dimension sizes for use in stride calculation
  SmallVector<Value> dimVals(rank);
  for (int64_t i = 0; i < rank; ++i) {
    if (baseType.isDynamicDim(i)) {
      dimVals[i] = rewriter.create<memref::DimOp>(loc, baseMemRef, i);
    } else {
      dimVals[i] =
          rewriter.create<arith::ConstantIndexOp>(loc, baseType.getDimSize(i));
    }
  }

  /// Compute row-major strides by multiplying subsequent dimensions
  Value one = rewriter.create<arith::ConstantIndexOp>(loc, 1);
  strides[rank - 1] = one;
  for (int64_t i = rank - 2; i >= 0; --i) {
    strides[i] =
        rewriter.create<arith::MulIOp>(loc, strides[i + 1], dimVals[i + 1]);
  }

  /// If dimension i is pinned => offset= pinnedIndices[i], size=1
  /// else => offset=0, size= dimVals[i]
  Value zero = rewriter.create<arith::ConstantIndexOp>(loc, 0);
  Value oneSize = rewriter.create<arith::ConstantIndexOp>(loc, 1);
  for (int64_t i = 0; i < rank; ++i) {
    if (i < (int64_t)pinnedIndices.size()) {
      offsets[i] = pinnedIndices[i];
      sizes[i] = oneSize;
      subShape[i] = 1;
    } else {
      offsets[i] = zero;
      sizes[i] = dimVals[i];
      subShape[i] = baseType.isDynamicDim(i) ? ShapedType::kDynamic
                                             : baseType.getDimSize(i);
    }
  }

  /// Now build the subview memref type
  auto subMemRefType = MemRefType::get(subShape, baseType.getElementType(),
                                       baseType.getLayout().getAffineMap(),
                                       baseType.getMemorySpace());

  /// Create the final arts.datablock operation
  auto depOp = rewriter.create<arts::DataBlockOp>(
      loc, subMemRefType, rewriter.getStringAttr(mode), baseMemRef, offsets,
      sizes, strides);
  if (isLoad)
    depOp->setAttr("isLoad", rewriter.getUnitAttr());
  return depOp;
}

/// Rewrites any memref.load/memref.store in 'region' that references
/// 'baseMemRef'. The new memref is 'newSubview'.
static void rewireDatablockUses(PatternRewriter &rewriter, Region &region,
                                arts::DataBlockOp depOp) {
  Value newSubview = depOp.getResult();
  Value baseMemRef = depOp.getBase();

  // LLVM_DEBUG(dbgs() << line);
  // LLVM_DEBUG(DBGS() << "Rewiring uses of: " << depOp << "\n");
  auto offsets = depOp.getOffsets();
  auto sizes = depOp.getSizes();
  auto subType = newSubview.getType().dyn_cast<MemRefType>();
  assert(subType && "Expected MemRefType");
  int64_t rank = subType.getRank();

  /// Collect the dimensions that are pinned
  SetVector<int64_t> pinnedDims;
  for (int64_t i = 0; i < rank; ++i) {
    if (auto cstOp = sizes[i].getDefiningOp<arith::ConstantIndexOp>()) {
      if (cstOp.value() == 1)
        pinnedDims.insert(i);
    }
  }

  /// Walk the region and replace the loads/stores
  auto isMatchingMemrefAndIndices = [&](auto op) {
    // TODO: Alias analysis
    if (op.getMemref() != baseMemRef)
      return false;
    auto oldIdx = op.getIndices();
    if ((int64_t)oldIdx.size() != rank)
      return false;
    for (auto dim : pinnedDims) {
      if (offsets[dim] != oldIdx[dim])
        return false;
    }
    return true;
  };

  region.walk([&](Operation *op) {
    /// Load operation
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      if (!isMatchingMemrefAndIndices(load))
        return;
      rewriter.updateRootInPlace(load, [&]() {
        SmallVector<Value> newIdx(rank);
        Location loc = load.getLoc();
        Value c0 = rewriter.create<arith::ConstantIndexOp>(loc, 0);
        for (int64_t i = 0; i < rank; ++i)
          newIdx[i] = pinnedDims.contains(i) ? c0 : load.getIndices()[i];
        /// Replace the load with the new subview
        load.getMemrefMutable().assign(newSubview);
        load.getIndicesMutable().assign(newIdx);
      });
    }
    /// Store operation
    else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (!isMatchingMemrefAndIndices(store))
        return;
      rewriter.updateRootInPlace(store, [&]() {
        SmallVector<Value> newIdx(rank);
        Location loc = store.getLoc();
        Value c0 = rewriter.create<arith::ConstantIndexOp>(loc, 0);
        for (int64_t i = 0; i < rank; ++i)
          newIdx[i] = pinnedDims.contains(i) ? c0 : store.getIndices()[i];
        /// Replace the store with the new subview
        store.getMemrefMutable().assign(newSubview);
        store.getIndicesMutable().assign(newIdx);
      });
    }
  });
  // LLVM_DEBUG(dbgs() << line);
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
  edtEnv.print();

  auto dependencies = edtEnv.getDependencies();
  for (Value dep : dependencies) {
    if (auto depOp = dyn_cast<arts::DataBlockOp>(dep.getDefiningOp())) {
      if (depOp.isLoad())
        rewireDatablockUses(rewriter, region, depOp);
      else
        replaceInRegion(region, depOp.getBase(), depOp.getResult());
    }
  }
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

      /// The valued must be loaded from a memrer through an memref.load
      auto depLoadOp = dyn_cast<memref::LoadOp>(
          depStoreOp.getValueToStore().getDefiningOp());
      assert(depLoadOp && "Expected a memref.load operation");

      /// Add the dependency to the edt environment
      edtEnv.addDependency(depLoadOp, depType);

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