//===----------------- ConvertOpenMPToARTSHierarchical.cpp  --------------===//
//
// This file implements a module pass that converts OpenMP ops
// (omp.parallel, omp.master, omp.task, etc.) into ARTS ops
// (arts.parallel, arts.single, arts.edt).
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/IR/BuiltinOps.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/PatternMatch.h"
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
#define DBGS() (dbgs() << "\n[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;

class EdtEnvManager {
public:
  EdtEnvManager(PatternRewriter &rewriter, Region &region)
      : rewriter(rewriter), region(region) {
    naiveCollection();
  }
  ~EdtEnvManager() {}

  /// Create a new datablock operation and rewires the uses
  DataBlockOp createMakeDepOp(PatternRewriter &rewriter, Location loc,
                              StringRef mode, Value inputMemRef) {
    /// If input is a memref.load, get the base
    memref::LoadOp loadOp =
        dyn_cast_or_null<memref::LoadOp>(inputMemRef.getDefiningOp());
    Value baseMemRef = loadOp ? loadOp.getMemref() : inputMemRef;

    /// memref type must be MemRefType
    MemRefType baseType = baseMemRef.getType().dyn_cast<MemRefType>();
    assert(baseType && "Input must be a MemRefType.");

    /// Rank must be > 0
    int64_t rank = baseType.getRank();
    assert(rank > 0 && "MemRef rank is 0? Unexpected.");

    /// Indices pinned by load if present
    SmallVector<Value> pinnedIndices;
    if (loadOp)
      pinnedIndices = SmallVector<Value>(loadOp.getIndices().begin(),
                                         loadOp.getIndices().end());

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
        int64_t staticSz = baseType.getDimSize(i);
        dimVals[i] = rewriter.create<arith::ConstantIndexOp>(loc, staticSz);
      }
    }

    /// Compute row-major strides by multiplying subsequent dimensions
    /// For a rank=3, if shape = [d0, d1, d2], then:
    ///   stride0 = d1*d2, stride1 = d2, stride2 = 1
    /// We'll build from the last dimension forward
    Value one = rewriter.create<arith::ConstantIndexOp>(loc, 1);
    strides[rank - 1] = one;

    for (int64_t i = rank - 2; i >= 0; --i) {
      strides[i] =
          rewriter.create<arith::MulIOp>(loc, strides[i + 1], dimVals[i + 1]);
    }

    /// If dimension i is pinned => offset= pinnedIndices[i], size=1
    /// else => offset=0, size= dimVals[i]
    /// Sub-shape: pinned dim => 1, else => keep original if static or -1 if
    /// dynamic
    Value zero = rewriter.create<arith::ConstantIndexOp>(loc, 0);
    Value oneSize = rewriter.create<arith::ConstantIndexOp>(loc, 1);

    for (int64_t i = 0; i < rank; ++i) {
      if (i < (int64_t)pinnedIndices.size()) {
        /// pinned dimension => offset = pinnedIndices[i], size=1
        offsets[i] = pinnedIndices[i];
        sizes[i] = oneSize;
        subShape[i] = 1;
      } else {
        /// unpinned => offset=0, size=dim
        offsets[i] = zero;
        sizes[i] = dimVals[i];
        /// dynamic or static
        if (baseType.isDynamicDim(i)) {
          subShape[i] = ShapedType::kDynamic;
        } else {
          subShape[i] = baseType.getDimSize(i);
        }
      }
    }

    /// Now build the subview memref type
    auto elemTy = baseType.getElementType();
    auto space = baseType.getMemorySpace();
    auto affineMap = baseType.getLayout().getAffineMap();
    auto subMemRefType = MemRefType::get(subShape, elemTy, affineMap, space);

    /// Create the final arts.datablock operation
    auto depOp = rewriter.create<arts::DataBlockOp>(
        loc, subMemRefType, rewriter.getStringAttr(mode), baseMemRef, offsets,
        sizes, strides);

    /// Rewire the values
    if (!loadOp) {
      /// If it was not a load, simply replace the value
      utils::replaceInRegion(region, baseMemRef, depOp.getResult());
    } else {
      /// Analyze all the instuctions that have the same base memref and indices
      /// and replace them with the new depOp
      rewritePinnedOps(depOp);
    }
    return depOp;
  }

  /// Rewrites any memref.load/memref.store in 'region' that references
  /// 'baseMemRef' and matches the pinnedIndices.
  void rewritePinnedOps(arts::DataBlockOp depOp) {
    Value newSubview = depOp.getResult();
    Value baseMemRef = depOp.getBase();

    auto offsets = depOp.getOffsets();
    auto sizes = depOp.getSizes();
    auto subType = newSubview.getType().dyn_cast<mlir::MemRefType>();
    assert(subType && "Expected MemRefType");
    int64_t rank = subType.getRank();

    /// Collect the dimensions that are pinned
    mlir::SetVector<int64_t> pinnedDims;
    for (int64_t i = 0; i < rank; ++i) {
      if (auto cstOp = sizes[i].getDefiningOp<mlir::arith::ConstantIndexOp>()) {
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
        // TODO: CSE or simplify the indices
        if (offsets[dim] != oldIdx[dim])
          return false;
      }
      return true;
    };

    region.walk([&](mlir::Operation *op) {
      /// Load operation
      if (auto load = mlir::dyn_cast<mlir::memref::LoadOp>(op)) {
        if (!isMatchingMemrefAndIndices(load))
          return;
        rewriter.updateRootInPlace(load, [&]() {
          mlir::SmallVector<mlir::Value> newIdx(rank);
          mlir::Location loc = load.getLoc();
          mlir::Value c0 =
              rewriter.create<mlir::arith::ConstantIndexOp>(loc, 0);
          for (int64_t i = 0; i < rank; ++i)
            newIdx[i] = pinnedDims.contains(i) ? c0 : load.getIndices()[i];
          /// Replace the load with the new subview
          load.getMemrefMutable().assign(newSubview);
          load.getIndicesMutable().assign(newIdx);
        });
      }
      /// Store operation
      else if (auto store = mlir::dyn_cast<mlir::memref::StoreOp>(op)) {
        if (!isMatchingMemrefAndIndices(store))
          return;
        rewriter.updateRootInPlace(store, [&]() {
          mlir::SmallVector<mlir::Value> newIdx(rank);
          mlir::Location loc = store.getLoc();
          mlir::Value c0 =
              rewriter.create<mlir::arith::ConstantIndexOp>(loc, 0);
          for (int64_t i = 0; i < rank; ++i)
            newIdx[i] = pinnedDims.contains(i) ? c0 : store.getIndices()[i];
          /// Replace the store with the new subview
          store.getMemrefMutable().assign(newSubview);
          store.getIndicesMutable().assign(newIdx);
        });
      }
    });
  }

  /// Collect parameters and dependencies
  void naiveCollection() {
    LLVM_DEBUG(dbgs() << "Naive collection of parameters and dependencies: \n");

    /// Checks if the value is a constant, if so, adds it to the constants set
    auto isConstant = [&](Value val) {
      auto defOp = val.getDefiningOp();
      if (!defOp)
        return false;

      auto constantOp = dyn_cast<arith::ConstantOp>(defOp);
      if (!constantOp)
        return false;
      constants.insert(constantOp);
      return true;
    };

    /// Collect all the values used in the region that are defined above it
    SetVector<Value> usedValues;
    getUsedValuesDefinedAbove(region, usedValues);
    for (Value operand : usedValues) {
      if (operand.getType().isIntOrIndexOrFloat()) {
        if (isConstant(operand))
          continue;
        LLVM_DEBUG(dbgs() << "  Adding parameter: " << operand << "\n");
        parameters.insert(operand);
      } else {
        LLVM_DEBUG(dbgs() << "  Adding dependency: " << operand << "\n");
        dependencies.insert(operand);
      }
    }
  }

  /// Add interface
  bool addParameter(Value val) { return parameters.insert(val); }

  Value addDependency(Value val, StringRef mode = "inout", bool adjust = true) {
    LLVM_DEBUG(dbgs() << "Adding dependency: " << val << " with mode: " << mode
                      << "\n");
    if (!adjust) {
      dependencies.insert(val);
      return val;
    }
    auto depOp = dyn_cast<arts::DataBlockOp>(val.getDefiningOp());
    /// If the dependency is not a datablock operation, create one
    if (!depOp) {
      auto valToCheck = val;
      /// Check if it was naively collected, if so, remove it
      /// If the value is a memref.load, get the memref
      if (auto loadOp = dyn_cast<memref::LoadOp>(val.getDefiningOp()))
        valToCheck = loadOp.getMemRef();

      if (dependencies.contains(valToCheck)) {
        LLVM_DEBUG(
            dbgs() << "   Dependency already added as memref - Removing it "
                      "and creating makedep\n");
        dependencies.remove(valToCheck);
      }

      /// Create a new datablock operation
      depOp = createMakeDepOp(rewriter, val.getLoc(), mode, val);
    }

    /// If it was added as a parameter, remove it
    auto dep = depOp.getBase();
    if (parameters.contains(dep))
      parameters.remove(dep);
    dependencies.insert(depOp);
    return depOp;
  }

  /// Getters
  ArrayRef<Value> getParameters() { return parameters.getArrayRef(); }

  ArrayRef<Value> getConstants() { return constants.getArrayRef(); }

  ArrayRef<Value> getDependencies(bool verify = true) {
    if (verify) {
      SmallVector<Value, 4> depsToProcess(dependencies.begin(),
                                          dependencies.end());
      for (Value dep : depsToProcess) {
        if (!isa<arts::DataBlockOp>(dep.getDefiningOp())) {
          addDependency(dep);
          dependencies.remove(dep);
        }
      }
    }
    return dependencies.getArrayRef();
  }

private:
  PatternRewriter &rewriter;
  Region &region;
  SetVector<Value> parameters, constants, dependencies;
};

//===----------------------------------------------------------------------===//
// Conversion Patterns
//===----------------------------------------------------------------------===//
/// Pattern to replace `omp.parallel` with `arts.parallel`
struct ParallelToARTSPattern : public OpRewritePattern<omp::ParallelOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::ParallelOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    LLVM_DEBUG(DBGS() << "Converting omp.parallel to arts.parallel\n");
    /// Collect parameters and dependencies
    EdtEnvManager edtEnv(rewriter, op.getRegion());

    /// Create a new `arts.parallel` operation.
    auto parOp = rewriter.create<arts::ParallelOp>(loc, edtEnv.getParameters(),
                                                   edtEnv.getConstants(),
                                                   edtEnv.getDependencies());
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
      auto depOp = edtEnv.addDependency(depLoadOp, depType, true);

      /// Replace dependency array with undef
      replaceWithUndef(depAlloc, rewriter);
    }
  }

  LogicalResult matchAndRewrite(omp::TaskOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    LLVM_DEBUG(DBGS() << "Converting omp.task to arts.edt\n");
    /// Collect parameters and dependencies
    EdtEnvManager edtEnv(rewriter, op.getRegion());
    collectTaskDependencies(edtEnv, op, rewriter, loc);

    /// Create a new `arts.edt` operation.
    auto edtOp = rewriter.create<arts::EdtOp>(loc, edtEnv.getParameters(),
                                              edtEnv.getConstants(),
                                              edtEnv.getDependencies());
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

  ///
  // bool cseChanged = false;
  // DominanceInfo domInfo;
  //     mlir::eliminateCommonSubExpressions(rewriter, domInfo, target,
  //                                         &cseChanged);
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