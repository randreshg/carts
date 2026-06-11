///==========================================================================///
/// File: SdeToArtsBoundary.cpp
///
/// Direct SDE-to-ARTS boundary materialization.
///==========================================================================///

#include "carts/dialect/arts/Utils/DbBackedMemrefUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/passes/Passes.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include <algorithm>
#include <tuple>

namespace mlir::carts::arts {
#define GEN_PASS_DEF_SDESTORAGETOARTSDB
#define GEN_PASS_DEF_SDEACCESSESTOARTSDEPS
#define GEN_PASS_DEF_FINALIZESDETOARTS
#include "carts/passes/Passes.h.inc"
} // namespace mlir::carts::arts

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

struct HaloRedistPlan {
  ArrayAttr ownerDims;
  ArrayAttr blockShape;
  ArrayAttr haloShape;
};

static bool isScalarParamType(Type type) {
  return type.isIndex() || isa<IntegerType, FloatType>(type);
}

static bool isConstantLikeValue(Value value) {
  Operation *def = value ? value.getDefiningOp() : nullptr;
  return def && def->hasTrait<OpTrait::ConstantLike>();
}

static bool isDefinedInside(Value value, Operation *scope) {
  if (!value || !scope)
    return false;
  Block *block = value.getParentBlock();
  if (!block)
    return false;
  for (Operation *parent = block->getParentOp(); parent;
       parent = parent->getParentOp())
    if (parent == scope)
      return true;
  return false;
}

static FailureOr<ArtsMode> convertAccessMode(sde::SdeAccessMode mode,
                                             Operation *context) {
  switch (mode) {
  case sde::SdeAccessMode::read:
    return ArtsMode::in;
  case sde::SdeAccessMode::write:
    return ArtsMode::out;
  case sde::SdeAccessMode::readwrite:
    return ArtsMode::inout;
  }
  context->emitError()
      << "has unsupported SDE access mode at the SDE-to-ARTS boundary";
  return failure();
}

static FailureOr<ArtsDepPattern> convertPattern(sde::SdePattern pattern,
                                                Operation *context) {
  switch (pattern) {
  case sde::SdePattern::uniform:
    return ArtsDepPattern::uniform;
  case sde::SdePattern::stencil_tiling_nd:
    return ArtsDepPattern::stencil_tiling_nd;
  case sde::SdePattern::cross_dim_stencil_3d:
    return ArtsDepPattern::cross_dim_stencil_3d;
  case sde::SdePattern::higher_order_stencil:
    return ArtsDepPattern::higher_order_stencil;
  case sde::SdePattern::wavefront_2d:
    return ArtsDepPattern::wavefront_2d;
  case sde::SdePattern::alternating_buffer_stencil:
    return ArtsDepPattern::alternating_buffer_stencil;
  case sde::SdePattern::matmul:
    return ArtsDepPattern::matmul;
  case sde::SdePattern::elementwise_pipeline:
    return ArtsDepPattern::elementwise_pipeline;
  case sde::SdePattern::reduction:
    return ArtsDepPattern::reduction;
  }
  context->emitError()
      << "has unsupported SDE pattern at the SDE-to-ARTS boundary";
  return failure();
}

static FailureOr<EdtDistributionKind>
convertDistributionKind(sde::SdeDistributionKind kind, Operation *context) {
  switch (kind) {
  case sde::SdeDistributionKind::owner_compute:
  case sde::SdeDistributionKind::blocked:
    return EdtDistributionKind::block;
  case sde::SdeDistributionKind::cyclic:
    return EdtDistributionKind::block_cyclic;
  }
  context->emitError()
      << "has unsupported SDE distribution kind at the SDE-to-ARTS boundary";
  return failure();
}

static FailureOr<ArtsPlanRepetitionStructure>
convertRepetitionStructure(sde::SdeRepetitionStructure structure,
                           Operation *context) {
  switch (structure) {
  case sde::SdeRepetitionStructure::none:
    return ArtsPlanRepetitionStructure::none;
  case sde::SdeRepetitionStructure::pair_step:
    return ArtsPlanRepetitionStructure::pair_step;
  case sde::SdeRepetitionStructure::k_step:
    return ArtsPlanRepetitionStructure::k_step;
  case sde::SdeRepetitionStructure::full_timestep:
    return ArtsPlanRepetitionStructure::full_timestep;
  }
  context->emitError()
      << "has unsupported SDE repetition structure at the SDE-to-ARTS boundary";
  return failure();
}

static LogicalResult attachCommittedSdeFacts(sde::SdeSuIterateOp source,
                                             arts::EdtOp task) {
  MLIRContext *ctx = source.getContext();
  Operation *taskOp = task.getOperation();
  if (auto pattern = source.getPatternAttr()) {
    FailureOr<ArtsDepPattern> depPattern =
        convertPattern(pattern.getValue(), source.getOperation());
    if (failed(depPattern))
      return failure();
    arts::setDepPattern(taskOp, *depPattern);
  }
  if (auto kind = source.getDistributionKindAttr()) {
    FailureOr<EdtDistributionKind> converted =
        convertDistributionKind(kind.getValue(), source.getOperation());
    if (failed(converted))
      return failure();
    arts::setEdtDistributionKind(taskOp, *converted);
  }
  if (auto topology = source.getIterationTopologyAttr())
    arts::setPlanIterationTopologyAttr(
        taskOp,
        ArtsPlanIterationTopologyAttr::get(
            ctx, static_cast<ArtsPlanIterationTopology>(topology.getValue())));
  if (auto repetition = source.getRepetitionStructureAttr()) {
    FailureOr<ArtsPlanRepetitionStructure> converted =
        convertRepetitionStructure(repetition.getValue(),
                                   source.getOperation());
    if (failed(converted))
      return failure();
    arts::setPlanRepetitionStructureAttr(
        taskOp, ArtsPlanRepetitionStructureAttr::get(ctx, *converted));
  }
  if (auto strategy = source.getReductionStrategyAttr())
    task.setReductionStrategyAttr(ArtsReductionStrategyAttr::get(
        ctx, static_cast<ArtsReductionStrategy>(strategy.getValue())));
  if (source.getPartialReductionAttr())
    task.setPartialReductionAttr(UnitAttr::get(ctx));
  if (auto dims = source.getPartialReductionDimsAttr())
    task.setPartialReductionDimsAttr(dims);
  if (auto ownerDims = source.getPartialReductionOwnerDimsAttr())
    task.setPartialReductionOwnerDimsAttr(ownerDims);
  if (auto ownerDims = source.getPhysicalOwnerDimsAttr())
    arts::setPlanOwnerDimsAttr(taskOp, ownerDims);
  if (auto blockShape = source.getPhysicalBlockShapeAttr())
    arts::setPlanPhysicalBlockShapeAttr(taskOp, blockShape);
  if (auto workerSlice = source.getLogicalWorkerSliceAttr())
    arts::setPlanLogicalWorkerSliceAttr(taskOp, workerSlice);
  if (auto haloShape = source.getPhysicalHaloShapeAttr())
    arts::setPlanHaloShapeAttr(taskOp, haloShape);
  if (auto minOffsets = source.getAccessMinOffsetsAttr())
    task->setAttr(task.getStencilMinOffsetsAttrName(), minOffsets);
  if (auto maxOffsets = source.getAccessMaxOffsetsAttr())
    task->setAttr(task.getStencilMaxOffsetsAttrName(), maxOffsets);
  if (auto ownerDims = source.getOwnerDimsAttr())
    task->setAttr(task.getStencilOwnerDimsAttrName(), ownerDims);
  if (auto spatialDims = source.getSpatialDimsAttr())
    task->setAttr(task.getStencilSpatialDimsAttrName(), spatialDims);
  if (auto writeFootprint = source.getWriteFootprintAttr())
    task->setAttr(task.getStencilWriteFootprintAttrName(), writeFootprint);
  if (source.getInPlaceSafeAttr())
    task.setInPlaceSafeAttr(UnitAttr::get(ctx));
  if (source.getInPlaceSharedStateAttr())
    task.setInPlaceSharedStateAttr(UnitAttr::get(ctx));
  return success();
}

static LogicalResult attachUnpartitionedSdeFacts(sde::SdeSuIterateOp source,
                                                 arts::EdtOp task) {
  MLIRContext *ctx = source.getContext();
  Operation *taskOp = task.getOperation();
  if (auto pattern = source.getPatternAttr()) {
    FailureOr<ArtsDepPattern> depPattern =
        convertPattern(pattern.getValue(), source.getOperation());
    if (failed(depPattern))
      return failure();
    arts::setDepPattern(taskOp, *depPattern);
  }
  if (source.getInPlaceSafeAttr())
    task.setInPlaceSafeAttr(UnitAttr::get(ctx));
  if (source.getInPlaceSharedStateAttr())
    task.setInPlaceSharedStateAttr(UnitAttr::get(ctx));
  return success();
}

static ArrayAttr ownerDimsForExpandedWindow(MLIRContext *ctx,
                                            unsigned ownerDimCount) {
  SmallVector<int64_t, 4> ownerDims;
  ownerDims.reserve(ownerDimCount);
  for (unsigned idx = 0; idx < ownerDimCount; ++idx)
    ownerDims.push_back(idx);
  return Builder(ctx).getI64ArrayAttr(ownerDims);
}

static FailureOr<ArrayAttr>
blockShapeForExpandedWindow(sde::SdeMuAccessWindowOp window,
                            MemRefType memrefType) {
  unsigned ownerDimCount = static_cast<unsigned>(window.getOwnerDimCount());
  std::optional<SmallVector<int64_t, 4>> validExtents =
      readI64ArrayAttr(window.getValidExtents());
  if (!validExtents)
    return failure();
  if (memrefType.getRank() !=
      static_cast<int64_t>(ownerDimCount + validExtents->size()))
    return failure();

  SmallVector<int64_t, 4> blockShape(ownerDimCount, 1);
  blockShape.append(validExtents->begin(), validExtents->end());
  return Builder(window.getContext()).getI64ArrayAttr(blockShape);
}

static LogicalResult
requireCompatibleWindows(sde::SdeMuAllocOp op,
                         ArrayRef<sde::SdeMuAccessWindowOp> windows,
                         ArrayAttr &ownerDims, ArrayAttr &blockShape) {
  if (windows.empty())
    return success();
  auto memrefType = dyn_cast<MemRefType>(op.getMemref().getType());
  if (!memrefType)
    return op.emitOpError() << "requires a memref type for ARTS DB lowering";

  sde::SdeMuAccessWindowOp firstWindow = windows.front();
  unsigned ownerDimCount =
      static_cast<unsigned>(firstWindow.getOwnerDimCount());
  ownerDims = ownerDimsForExpandedWindow(op.getContext(), ownerDimCount);
  FailureOr<ArrayAttr> maybeBlockShape =
      blockShapeForExpandedWindow(firstWindow, memrefType);
  if (failed(maybeBlockShape))
    return firstWindow.emitOpError()
           << "has window shape incompatible with the rank-expanded MU";
  blockShape = *maybeBlockShape;

  for (sde::SdeMuAccessWindowOp window : windows) {
    if (static_cast<unsigned>(window.getOwnerDimCount()) != ownerDimCount)
      return window.emitOpError()
             << "conflicts with another access-window for the same MU";
    FailureOr<ArrayAttr> candidate =
        blockShapeForExpandedWindow(window, memrefType);
    if (failed(candidate) || *candidate != blockShape)
      return window.emitOpError()
             << "commits a block shape that conflicts with another "
                "access-window for the same MU";
  }
  return success();
}

static void eraseDeallocUsers(Value memref) {
  SmallVector<memref::DeallocOp> deallocs;
  for (Operation *user : llvm::make_early_inc_range(memref.getUsers())) {
    auto dealloc = dyn_cast<memref::DeallocOp>(user);
    if (dealloc && dealloc.getMemref() == memref)
      deallocs.push_back(dealloc);
  }
  for (memref::DeallocOp dealloc : deallocs)
    dealloc.erase();
}

static std::optional<SmallVector<Value>>
getDynamicSizesForMaterializedTaskDepRoot(Operation *root) {
  if (auto alloc = dyn_cast_or_null<memref::AllocOp>(root))
    return SmallVector<Value>(alloc.getDynamicSizes().begin(),
                              alloc.getDynamicSizes().end());
  if (auto alloca = dyn_cast_or_null<memref::AllocaOp>(root))
    return SmallVector<Value>(alloca.getDynamicSizes().begin(),
                              alloca.getDynamicSizes().end());
  return std::nullopt;
}

static LogicalResult materializeTaskDepMemrefStorage(ModuleOp module) {
  SetVector<Value> roots;
  bool foundError = false;
  module.walk([&](sde::SdeMuDepOp dep) {
    Value root = ValueAnalysis::stripMemrefViewOps(dep.getSource());
    if (!root) {
      dep.emitOpError() << "has no traceable memref root for ARTS storage "
                           "materialization";
      foundError = true;
      return;
    }
    if (arts::DbUtils::getUnderlyingDbAlloc(root))
      return;
    auto memrefType = dyn_cast<MemRefType>(root.getType());
    if (!memrefType) {
      dep.emitOpError() << "requires a memref source for ARTS storage "
                           "materialization";
      foundError = true;
      return;
    }
    if (isa<MemRefType>(memrefType.getElementType())) {
      dep.emitOpError() << "requires normalized memref storage; nested memref "
                           "element types must be raised before SDE-to-ARTS";
      foundError = true;
      return;
    }
    Operation *rootOp = root.getDefiningOp();
    if (!getDynamicSizesForMaterializedTaskDepRoot(rootOp)) {
      dep.emitOpError()
          << "references a memref root that direct SDE-to-ARTS task lowering "
             "cannot materialize as an ARTS DB; SDE must expose a "
             "materializable memref allocation or fail before this boundary";
      foundError = true;
      return;
    }
    roots.insert(root);
  });
  if (foundError)
    return failure();

  for (Value root : roots) {
    Operation *rootOp = root.getDefiningOp();
    auto memrefType = cast<MemRefType>(root.getType());
    std::optional<SmallVector<Value>> dynamicSizes =
        getDynamicSizesForMaterializedTaskDepRoot(rootOp);
    if (!dynamicSizes)
      return failure();

    OpBuilder builder(rootOp);
    builder.setInsertionPointAfter(rootOp);
    Value replacement;
    if (failed(arts::createCoarseDbBackedMemref(
            builder, rootOp->getLoc(), memrefType, *dynamicSizes, replacement)))
      return rootOp->emitError()
             << "could not materialize task dependency memref as an ARTS DB";
    if (replacement.getType() != memrefType)
      replacement = memref::CastOp::create(builder, rootOp->getLoc(),
                                           memrefType, replacement);

    eraseDeallocUsers(root);
    root.replaceAllUsesWith(replacement);
    if (rootOp->use_empty())
      rootOp->erase();
  }

  return success();
}

static LogicalResult
validateAndCollectHaloRedists(ModuleOp module,
                              DenseMap<Value, HaloRedistPlan> &plans,
                              SmallVectorImpl<sde::SdeRedistOp> &redists) {
  bool foundError = false;
  module.walk([&](sde::SdeRedistOp redist) {
    redists.push_back(redist);
    if (redist.getFamily() != sde::SdeMovementFamily::halo_like) {
      redist.emitOpError() << "direct SDE-to-ARTS lowering for movement family "
                           << stringifySdeMovementFamily(redist.getFamily())
                           << " requires a real ARTS materialization";
      foundError = true;
      return;
    }

    ArrayAttr haloShape = redist.getHaloShapeAttr();
    if (!haloShape) {
      redist.emitOpError() << "commits halo_like movement without haloShape";
      foundError = true;
      return;
    }
    if (redist.getSourceOwnerDims() != redist.getTargetOwnerDims() ||
        redist.getSourceBlockShape() != redist.getTargetBlockShape()) {
      redist.emitOpError()
          << "commits halo movement whose source and target layouts differ; "
             "direct ARTS halo materialization requires identical owner/block "
             "geometry";
      foundError = true;
      return;
    }

    std::optional<SmallVector<int64_t, 4>> halo = readI64ArrayAttr(haloShape);
    auto memrefType = dyn_cast<MemRefType>(redist.getMu().getType());
    if (!halo || !memrefType ||
        halo->size() != static_cast<size_t>(memrefType.getRank()) ||
        llvm::any_of(*halo, [](int64_t value) { return value < 0; })) {
      redist.emitOpError()
          << "commits a halo shape that is not a non-negative rank-length "
             "array";
      foundError = true;
      return;
    }

    HaloRedistPlan plan{redist.getSourceOwnerDims(),
                        redist.getSourceBlockShape(), haloShape};
    auto [it, inserted] = plans.try_emplace(redist.getMu(), plan);
    if (!inserted && (it->second.ownerDims != plan.ownerDims ||
                      it->second.blockShape != plan.blockShape ||
                      it->second.haloShape != plan.haloShape)) {
      redist.emitOpError()
          << "conflicts with another committed halo movement for the same MU";
      foundError = true;
      return;
    }
  });
  return failure(foundError);
}

static LogicalResult lowerMuData(sde::SdeMuDataOp op) {
  auto memrefType = dyn_cast<MemRefType>(op.getHandle().getType());
  if (!memrefType)
    return op.emitOpError()
           << "expects a memref handle before SDE-to-ARTS conversion";
  if (memrefType.getNumDynamicDims() != 0)
    return op.emitOpError()
           << "has dynamic dimensions but carries no dynamic size operands";

  OpBuilder builder(op);
  Value replacement;
  if (failed(arts::createCoarseDbBackedMemref(builder, op.getLoc(), memrefType,
                                              ValueRange{}, replacement)))
    return failure();
  if (replacement.getType() != memrefType)
    replacement =
        memref::CastOp::create(builder, op.getLoc(), memrefType, replacement);

  op.getHandle().replaceAllUsesWith(replacement);
  op.erase();
  return success();
}

static ArrayAttr getCommittedHaloShapeForWindow(sde::SdeMuAccessWindowOp window,
                                                ArrayAttr committedHaloShape);

static LogicalResult
lowerMuAlloc(sde::SdeMuAllocOp op,
             ArrayRef<sde::SdeMuAccessWindowOp> committedWindows,
             ArrayAttr committedHaloShape) {
  auto memrefType = dyn_cast<MemRefType>(op.getMemref().getType());
  if (!memrefType)
    return op.emitOpError()
           << "expects a memref result before SDE-to-ARTS conversion";

  ArrayAttr ownerDims;
  ArrayAttr blockShape;
  if (failed(requireCompatibleWindows(op, committedWindows, ownerDims,
                                      blockShape)))
    return failure();

  OpBuilder builder(op);
  Value replacement;
  if (!committedWindows.empty()) {
    if (failed(arts::createPlannedDbBackedMemref(
            builder, op.getLoc(), memrefType, op.getDynamicSizes(), ownerDims,
            blockShape, committedHaloShape, replacement)))
      return op.emitOpError()
             << "could not materialize committed SDE block layout as ARTS DB";
  } else if (committedHaloShape) {
    return op.emitOpError()
           << "commits halo movement but has no committed SDE access-window "
              "layout for ARTS DB materialization";
  } else if (failed(arts::createCoarseDbBackedMemref(
                 builder, op.getLoc(), memrefType, op.getDynamicSizes(),
                 replacement))) {
    return op.emitOpError()
           << "could not materialize unpartitioned SDE MU as ARTS DB";
  }

  if (replacement.getType() != memrefType)
    replacement =
        memref::CastOp::create(builder, op.getLoc(), memrefType, replacement);

  for (sde::SdeMuAccessWindowOp window : committedWindows) {
    OpBuilder planBuilder(window);
    ArrayAttr haloShape =
        getCommittedHaloShapeForWindow(window, committedHaloShape);
    if (window.getMode() == sde::SdeAccessMode::write)
      haloShape = {};
    else if (window.getMode() == sde::SdeAccessMode::readwrite && haloShape) {
      return window.emitOpError()
             << "commits a writable halo dependency; SDE must split the read "
                "halo and write access before ARTS materialization";
    }
    FailureOr<ArtsMode> mode =
        convertAccessMode(window.getMode(), window.getOperation());
    if (failed(mode))
      return failure();
    arts::DbAccessPlanOp::create(
        planBuilder, window.getLoc(), replacement,
        ArtsModeAttr::get(op.getContext(), *mode), window.getArrayIdAttr(),
        planBuilder.getI64IntegerAttr(
            static_cast<int64_t>(window.getOwnerDimCount())),
        window.getBlockLo(), window.getBlockHi(), window.getValidExtents(),
        haloShape);
  }
  for (sde::SdeMuAccessWindowOp window : committedWindows)
    window.erase();

  eraseDeallocUsers(op.getMemref());
  op.getMemref().replaceAllUsesWith(replacement);
  op.erase();
  return success();
}

static ArrayAttr getCommittedHaloShapeForWindow(sde::SdeMuAccessWindowOp window,
                                                ArrayAttr committedHaloShape) {
  if (!window || !committedHaloShape)
    return {};
  auto parentSu = window->getParentOfType<sde::SdeSuIterateOp>();
  if (!parentSu)
    return {};
  for (Operation *op = parentSu->getPrevNode(); op; op = op->getPrevNode()) {
    auto redist = dyn_cast<sde::SdeRedistOp>(op);
    if (!redist)
      continue;
    if (redist.getMu() == window.getMu() &&
        redist.getFamily() == sde::SdeMovementFamily::halo_like)
      return redist.getHaloShapeAttr();
  }
  return {};
}

struct DirectDepPlan {
  arts::DbAllocOp alloc;
  ArtsMode mode = ArtsMode::uninitialized;
  unsigned ownerDimCount = 0;
  SmallVector<int64_t, 4> blockLo;
  SmallVector<int64_t, 4> blockHi;
  SmallVector<int64_t, 4> validExtents;
  ArrayAttr haloShape;
};

static bool hasDistributedLaunchStoragePlan(ArrayRef<DirectDepPlan> deps) {
  return llvm::any_of(deps, [](DirectDepPlan dep) {
    return dep.alloc && hasArtsDbPhysicalLayoutPlan(dep.alloc.getOperation());
  });
}

static bool hasDistributedWriterStoragePlan(ArrayRef<DirectDepPlan> deps) {
  return llvm::any_of(deps, [](DirectDepPlan dep) {
    return dep.alloc && arts::DbUtils::isWriterMode(dep.mode) &&
           hasArtsDbPhysicalLayoutPlan(dep.alloc.getOperation());
  });
}

static LogicalResult verifyDistributedWriterGroupingOwnerLocal(
    sde::SdeSuIterateOp source, ArrayRef<DirectDepPlan> deps,
    ArrayRef<int64_t> groupBlockCounts, int64_t totalNodes) {
  if (totalNodes <= 1 ||
      !llvm::any_of(groupBlockCounts, [](int64_t count) { return count > 1; }))
    return success();

  for (const DirectDepPlan &dep : deps) {
    arts::DbAllocOp alloc = dep.alloc;
    if (!alloc || !arts::DbUtils::isWriterMode(dep.mode) ||
        !hasArtsDbPhysicalLayoutPlan(alloc.getOperation()))
      continue;

    std::optional<DbOwnerMapPlan> ownerPlan =
        deriveDbOwnerMapPlanFromSeed(alloc);
    if (!ownerPlan)
      return source.emitOpError()
             << "commits logicalWorkerSlice whose grouped distributed writer "
                "range cannot be proven owner-local from committed DB "
                "owner-map facts";

    SmallVector<Value, 4> dbSizeValues(alloc.getSizes().begin(),
                                       alloc.getSizes().end());
    std::optional<SmallVector<int64_t, 4>> dbSizes =
        foldStaticDbIndexValues(dbSizeValues);
    if (!dbSizes || dbSizes->size() != groupBlockCounts.size())
      return source.emitOpError()
             << "commits logicalWorkerSlice whose grouped distributed writer "
                "range cannot be proven owner-local from static DB block-grid "
                "facts";

    if (!isStaticDbOwnerGroupedBlockScheduleRouteLocal(
            *dbSizes, groupBlockCounts, totalNodes, *ownerPlan))
      return source.emitOpError()
             << "commits logicalWorkerSlice that groups distributed writer "
                "blocks across owner routes; SDE-to-ARTS must split writer "
                "codelets into owner-local block ranges";
  }

  return success();
}

struct CoarseSuDependency {
  arts::DbAllocOp alloc;
  ArtsMode mode = ArtsMode::uninitialized;
};

struct DirectCuDepPlan {
  arts::DbAllocOp alloc;
  ArtsMode mode = ArtsMode::uninitialized;
  unsigned ownerDimCount = 0;
  SmallVector<int64_t, 4> blockLo;
  SmallVector<int64_t, 4> blockHi;
  SmallVector<int64_t, 4> validExtents;
  ArrayAttr haloShape;
  Value acquiredPtr;
};

struct CuResultPlan {
  arts::DbAllocOp alloc;
  Value writePtr;
  Value replacement;
};

static bool hasDistributedLaunchStoragePlan(ArrayRef<DirectCuDepPlan> deps) {
  return llvm::any_of(deps, [](DirectCuDepPlan dep) {
    return dep.alloc && hasArtsDbPhysicalLayoutPlan(dep.alloc.getOperation());
  });
}

struct AccessPlanWindowFacts {
  SmallVector<int64_t, 4> blockLo;
  SmallVector<int64_t, 4> blockHi;
  SmallVector<int64_t, 4> validExtents;
};

static FailureOr<AccessPlanWindowFacts>
getAccessPlanWindowFacts(arts::DbAccessPlanOp plan, unsigned ownerDimCount) {
  std::optional<SmallVector<int64_t, 4>> blockLo =
      readI64ArrayAttr(plan.getBlockLo());
  std::optional<SmallVector<int64_t, 4>> blockHi =
      readI64ArrayAttr(plan.getBlockHi());
  std::optional<SmallVector<int64_t, 4>> validExtents =
      readI64ArrayAttr(plan.getValidExtents());
  if (!blockLo || !blockHi || !validExtents ||
      blockLo->size() != ownerDimCount || blockHi->size() != ownerDimCount) {
    plan.emitOpError()
        << "has access-window evidence inconsistent with ownerDimCount";
    return failure();
  }
  for (auto [lo, hi] : llvm::zip(*blockLo, *blockHi))
    if (hi <= lo) {
      plan.emitOpError() << "has empty or inverted block window";
      return failure();
    }
  for (int64_t extent : *validExtents)
    if (extent <= 0) {
      plan.emitOpError() << "has non-positive valid extent";
      return failure();
    }
  return AccessPlanWindowFacts{
      SmallVector<int64_t, 4>(blockLo->begin(), blockLo->end()),
      SmallVector<int64_t, 4>(blockHi->begin(), blockHi->end()),
      SmallVector<int64_t, 4>(validExtents->begin(), validExtents->end())};
}

static arts::DbAllocOp resolveBoundaryDbAlloc(Value memref) {
  if (!memref)
    return nullptr;
  if (auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
          arts::DbUtils::getUnderlyingDbAlloc(memref)))
    return alloc;

  Value root = ValueAnalysis::stripMemrefViewOps(memref);
  auto result = dyn_cast<OpResult>(root);
  if (!result)
    return nullptr;
  auto cu = dyn_cast_or_null<sde::SdeCuRegionOp>(result.getOwner());
  if (!cu || cu.getBody().empty())
    return nullptr;
  auto yield =
      dyn_cast_or_null<sde::SdeYieldOp>(cu.getBody().front().getTerminator());
  if (!yield || result.getResultNumber() >= yield.getValues().size())
    return nullptr;
  return resolveBoundaryDbAlloc(yield.getValues()[result.getResultNumber()]);
}

static LogicalResult
recordCoarseSuAccess(sde::SdeSuIterateOp source, Operation *site, Value memref,
                     ArtsMode mode, DenseMap<Operation *, unsigned> &depIndex,
                     SmallVectorImpl<CoarseSuDependency> &deps) {
  arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
  if (!alloc) {
    Value root = ValueAnalysis::stripMemrefViewOps(memref);
    if (root && isDefinedInside(root, source.getOperation()))
      return success();
    return site->emitError()
           << "accesses external memref without ARTS DB-backed storage during "
              "coarse SDE-to-ARTS SU materialization";
  }

  std::optional<arts::PartitionMode> partitionMode = alloc.getPartitionMode();
  if (!partitionMode || *partitionMode != arts::PartitionMode::coarse)
    return site->emitError()
           << "touches a non-coarse DB without committed SDE access windows; "
              "refusing coarse SU materialization";
  if (alloc.getPlanOwnerDimsAttr() || alloc.getPlanPhysicalBlockShapeAttr() ||
      alloc.getPlanLogicalWorkerSliceAttr() || alloc.getPlanHaloShapeAttr() ||
      alloc.getDistributedAttr())
    return site->emitError()
           << "touches a planned or distributed DB without committed SDE "
              "access windows; refusing coarse SU materialization";

  auto [it, inserted] = depIndex.try_emplace(alloc.getOperation(), deps.size());
  if (inserted) {
    deps.push_back({alloc, mode});
    return success();
  }
  deps[it->second].mode = arts::combineAccessModes(deps[it->second].mode, mode);
  return success();
}

static LogicalResult
collectCoarseSuDependencies(sde::SdeSuIterateOp source,
                            SmallVectorImpl<CoarseSuDependency> &deps) {
  DenseMap<Operation *, unsigned> depIndex;
  Block *computeBlock = sde::getSuIterateComputeBlock(source);
  if (!computeBlock)
    return source.emitOpError() << "has no computable body";

  WalkResult result = computeBlock->walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return failed(recordCoarseSuAccess(source, op, load.getMemref(),
                                         ArtsMode::in, depIndex, deps))
                 ? WalkResult::interrupt()
                 : WalkResult::advance();
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return failed(recordCoarseSuAccess(source, op, store.getMemref(),
                                         ArtsMode::out, depIndex, deps))
                 ? WalkResult::interrupt()
                 : WalkResult::advance();
    if (auto copy = dyn_cast<memref::CopyOp>(op)) {
      if (failed(recordCoarseSuAccess(source, op, copy.getSource(),
                                      ArtsMode::in, depIndex, deps)))
        return WalkResult::interrupt();
      if (failed(recordCoarseSuAccess(source, op, copy.getTarget(),
                                      ArtsMode::out, depIndex, deps)))
        return WalkResult::interrupt();
      return WalkResult::advance();
    }
    if (auto atomic = dyn_cast<sde::SdeCuAtomicOp>(op))
      return failed(recordCoarseSuAccess(source, op, atomic.getAddr(),
                                         ArtsMode::inout, depIndex, deps))
                 ? WalkResult::interrupt()
                 : WalkResult::advance();
    return WalkResult::advance();
  });
  return result.wasInterrupted() ? failure() : success();
}

static void collectTouchedDbAllocs(sde::SdeSuIterateOp source,
                                   DenseSet<Operation *> &touched) {
  source.getBody().walk([&](Operation *op) {
    Value memref;
    if (auto load = dyn_cast<memref::LoadOp>(op))
      memref = load.getMemref();
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      memref = store.getMemref();
    else if (auto atomic = dyn_cast<sde::SdeCuAtomicOp>(op))
      memref = atomic.getAddr();
    else
      return;
    if (arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref))
      touched.insert(alloc.getOperation());
  });
}

static LogicalResult
recordAccessPlanDependency(arts::DbAccessPlanOp plan,
                           DenseMap<Operation *, unsigned> &depIndex,
                           SmallVectorImpl<DirectDepPlan> &deps) {
  auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
      arts::DbUtils::getUnderlyingDbAlloc(plan.getMu()));
  if (!alloc)
    return plan.emitOpError()
           << "does not reference an ARTS DB-backed MU after storage "
              "materialization";

  unsigned ownerDimCount = static_cast<unsigned>(plan.getOwnerDimCount());
  FailureOr<AccessPlanWindowFacts> facts =
      getAccessPlanWindowFacts(plan, ownerDimCount);
  if (failed(facts))
    return failure();
  if (plan.getHaloShapeAttr() && plan.getMode() != ArtsMode::in)
    return plan.emitOpError()
           << "commits a writable halo dependency; SDE must split the read "
              "halo and write access before ARTS materialization";

  auto [it, inserted] = depIndex.try_emplace(alloc.getOperation(), deps.size());
  if (inserted) {
    ArrayAttr haloShape;
    if (plan.getMode() == ArtsMode::in)
      haloShape = plan.getHaloShapeAttr();
    deps.push_back(
        {alloc, plan.getMode(), ownerDimCount,
         SmallVector<int64_t, 4>(facts->blockLo.begin(), facts->blockLo.end()),
         SmallVector<int64_t, 4>(facts->blockHi.begin(), facts->blockHi.end()),
         SmallVector<int64_t, 4>(facts->validExtents.begin(),
                                 facts->validExtents.end()),
         haloShape});
    return success();
  }

  DirectDepPlan &dep = deps[it->second];
  if (dep.ownerDimCount != ownerDimCount || dep.blockLo != facts->blockLo ||
      dep.blockHi != facts->blockHi || dep.validExtents != facts->validExtents)
    return plan.emitOpError()
           << "commits access-window evidence that conflicts with another "
              "window for the same DB";
  dep.mode = arts::combineAccessModes(dep.mode, plan.getMode());
  if (plan.getMode() == ArtsMode::in) {
    if (ArrayAttr haloShape = plan.getHaloShapeAttr()) {
      if (dep.haloShape && dep.haloShape != haloShape)
        return plan.emitOpError()
               << "commits a halo shape that conflicts with another read "
                  "window for the same DB";
      dep.haloShape = haloShape;
    }
  }
  return success();
}

static LogicalResult
recordCuAccessPlanDependency(arts::DbAccessPlanOp plan,
                             DenseMap<Operation *, unsigned> &depIndex,
                             SmallVectorImpl<DirectCuDepPlan> &deps) {
  auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
      arts::DbUtils::getUnderlyingDbAlloc(plan.getMu()));
  if (!alloc)
    return plan.emitOpError()
           << "does not reference an ARTS DB-backed MU after storage "
              "materialization";

  unsigned ownerDimCount = static_cast<unsigned>(plan.getOwnerDimCount());
  FailureOr<AccessPlanWindowFacts> facts =
      getAccessPlanWindowFacts(plan, ownerDimCount);
  if (failed(facts))
    return failure();
  if (plan.getHaloShapeAttr() && plan.getMode() != ArtsMode::in)
    return plan.emitOpError()
           << "commits a writable halo dependency; SDE must split the read "
              "halo and write access before ARTS materialization";

  auto [it, inserted] = depIndex.try_emplace(alloc.getOperation(), deps.size());
  if (inserted) {
    ArrayAttr haloShape;
    if (plan.getMode() == ArtsMode::in)
      haloShape = plan.getHaloShapeAttr();
    deps.push_back(
        {alloc, plan.getMode(), ownerDimCount,
         SmallVector<int64_t, 4>(facts->blockLo.begin(), facts->blockLo.end()),
         SmallVector<int64_t, 4>(facts->blockHi.begin(), facts->blockHi.end()),
         SmallVector<int64_t, 4>(facts->validExtents.begin(),
                                 facts->validExtents.end()),
         haloShape, Value{}});
    return success();
  }

  DirectCuDepPlan &dep = deps[it->second];
  if (dep.ownerDimCount != ownerDimCount || dep.blockLo != facts->blockLo ||
      dep.blockHi != facts->blockHi || dep.validExtents != facts->validExtents)
    return plan.emitOpError()
           << "commits access-window evidence that conflicts with another "
              "window for the same DB in one CU";
  dep.mode = arts::combineAccessModes(dep.mode, plan.getMode());
  if (plan.getMode() == ArtsMode::in) {
    if (ArrayAttr haloShape = plan.getHaloShapeAttr()) {
      if (dep.haloShape && dep.haloShape != haloShape)
        return plan.emitOpError()
               << "commits a halo shape that conflicts with another read "
                  "window for the same DB in one CU";
      dep.haloShape = haloShape;
    }
  }
  return success();
}

static LogicalResult
collectSuDependencies(sde::SdeSuIterateOp source,
                      SmallVectorImpl<DirectDepPlan> &deps,
                      DenseSet<Operation *> &consumedCuLevelAccessPlans) {
  DenseMap<Operation *, unsigned> depIndex;
  DenseSet<Operation *> touchedAllocs;
  collectTouchedDbAllocs(source, touchedAllocs);

  auto consider = [&](arts::DbAccessPlanOp plan) -> WalkResult {
    if (auto ownerSu = plan->getParentOfType<sde::SdeSuIterateOp>())
      if (ownerSu != source)
        return WalkResult::advance();
    Operation *alloc = arts::DbUtils::getUnderlyingDbAlloc(plan.getMu());
    if (!alloc || !touchedAllocs.contains(alloc))
      return WalkResult::advance();
    if (failed(recordAccessPlanDependency(plan, depIndex, deps)))
      return WalkResult::interrupt();
    if (!plan->getParentOfType<sde::SdeSuIterateOp>())
      consumedCuLevelAccessPlans.insert(plan.getOperation());
    return WalkResult::advance();
  };

  sde::SdeCuRegionOp parentCu = source->getParentOfType<sde::SdeCuRegionOp>();
  WalkResult result = parentCu ? parentCu.getBody().walk(consider)
                               : source.getBody().walk(consider);
  return result.wasInterrupted() ? failure() : success();
}

static LogicalResult
collectStandaloneCuDependencies(sde::SdeCuRegionOp source,
                                SmallVectorImpl<DirectCuDepPlan> &deps,
                                SmallVectorImpl<arts::DbAccessPlanOp> &plans) {
  DenseMap<Operation *, unsigned> depIndex;
  WalkResult result = source.getBody().walk([&](arts::DbAccessPlanOp plan) {
    plans.push_back(plan);
    if (failed(recordCuAccessPlanDependency(plan, depIndex, deps)))
      return WalkResult::interrupt();
    return WalkResult::advance();
  });
  return result.wasInterrupted() ? failure() : success();
}

static bool containsDbAccessPlan(sde::SdeCuRegionOp source) {
  bool found = false;
  source.getBody().walk([&](arts::DbAccessPlanOp) {
    found = true;
    return WalkResult::interrupt();
  });
  return found;
}

struct OwnerSlotPlan {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<unsigned, 4> loopDims;
  SmallVector<int64_t, 4> blockSizes;
  SmallVector<unsigned, 4> rawSlots;
};

static FailureOr<OwnerSlotPlan>
resolveOwnerSlotPlan(ArrayRef<int64_t> ownerDims, ArrayRef<int64_t> blockShape,
                     unsigned loopRank, Operation *context) {
  if (ownerDims.empty()) {
    context->emitError()
        << "requires at least one committed physical owner dimension";
    return failure();
  }
  if (ownerDims.size() > loopRank) {
    context->emitError()
        << "names more physical owner dimensions than loop dimensions";
    return failure();
  }
  bool ownerRankLoop = loopRank == ownerDims.size();
  if (blockShape.size() != ownerDims.size() && blockShape.size() < loopRank) {
    context->emitError()
        << "requires physicalBlockShape rank to cover owner or loop rank";
    return failure();
  }

  SmallVector<char, 4> seenOwner;
  unsigned ownerSeenSize =
      std::max<unsigned>(loopRank, static_cast<unsigned>(blockShape.size()));
  seenOwner.assign(ownerSeenSize, 0);
  SmallVector<char, 4> seenLoop(loopRank, 0);
  SmallVector<std::tuple<int64_t, unsigned, int64_t, unsigned>, 4> slots;
  slots.reserve(ownerDims.size());
  for (auto [rawSlot, ownerDim] : llvm::enumerate(ownerDims)) {
    if (ownerDim < 0) {
      context->emitError() << "owner dim is negative";
      return failure();
    }
    unsigned physicalOwnerDim = static_cast<unsigned>(ownerDim);
    if (physicalOwnerDim >= seenOwner.size())
      seenOwner.resize(physicalOwnerDim + 1, 0);
    if (seenOwner[physicalOwnerDim]) {
      context->emitError() << "commits duplicate physical owner dimensions";
      return failure();
    }
    seenOwner[physicalOwnerDim] = 1;

    unsigned loopDim = physicalOwnerDim;
    if (static_cast<unsigned>(ownerDim) >= loopRank) {
      if (!ownerRankLoop) {
        context->emitError() << "owner dim exceeds loop rank";
        return failure();
      }
      loopDim = static_cast<unsigned>(rawSlot);
    }
    if (seenLoop[loopDim]) {
      context->emitError()
          << "maps multiple physical owner dimensions to one loop dimension";
      return failure();
    }
    seenLoop[loopDim] = 1;

    int64_t blockSize = 0;
    if (blockShape.size() == ownerDims.size()) {
      blockSize = blockShape[rawSlot];
    } else if (static_cast<size_t>(ownerDim) < blockShape.size()) {
      blockSize = blockShape[ownerDim];
    } else {
      context->emitError()
          << "physicalOwnerDims must index physicalBlockShape dimensions";
      return failure();
    }
    if (blockSize <= 0) {
      context->emitError()
          << "requires a positive physical block size for every owner dim";
      return failure();
    }
    slots.emplace_back(ownerDim, loopDim, blockSize,
                       static_cast<unsigned>(rawSlot));
  }

  llvm::sort(slots, [](const auto &lhs, const auto &rhs) {
    return std::get<0>(lhs) < std::get<0>(rhs);
  });

  OwnerSlotPlan plan;
  plan.ownerDims.reserve(slots.size());
  plan.loopDims.reserve(slots.size());
  plan.blockSizes.reserve(slots.size());
  plan.rawSlots.reserve(slots.size());
  for (auto [ownerDim, loopDim, blockSize, rawSlot] : slots) {
    plan.ownerDims.push_back(ownerDim);
    plan.loopDims.push_back(loopDim);
    plan.blockSizes.push_back(blockSize);
    plan.rawSlots.push_back(rawSlot);
  }
  return plan;
}

static LogicalResult collectExternalScalarCaptures(sde::SdeSuIterateOp source,
                                                   SetVector<Value> &captures) {
  auto addIfExternalScalar = [&](Value value) {
    if (!value || !isScalarParamType(value.getType()))
      return;
    if (!isDefinedInside(value, source.getOperation()))
      captures.insert(value);
  };

  for (Value value : source.getLowerBounds())
    addIfExternalScalar(value);
  for (Value value : source.getUpperBounds())
    addIfExternalScalar(value);
  for (Value value : source.getSteps())
    addIfExternalScalar(value);

  Block *computeBlock = sde::getSuIterateComputeBlock(source);
  if (!computeBlock)
    return source.emitOpError() << "has no computable body";
  computeBlock->walk([&](Operation *op) {
    for (Value operand : op->getOperands())
      addIfExternalScalar(operand);
  });
  return success();
}

static LogicalResult collectExternalScalarCaptures(sde::SdeCuTaskOp source,
                                                   SetVector<Value> &captures) {
  auto addIfExternalScalar = [&](Value value) {
    if (!value || !isScalarParamType(value.getType()) ||
        isConstantLikeValue(value))
      return;
    if (!isDefinedInside(value, source.getOperation()))
      captures.insert(value);
  };

  source.getBody().walk([&](Operation *op) {
    if (isa<sde::SdeMuDepOp>(op))
      return;
    for (Value operand : op->getOperands())
      addIfExternalScalar(operand);
  });
  return success();
}

static LogicalResult collectExternalScalarCaptures(sde::SdeCuRegionOp source,
                                                   SetVector<Value> &captures) {
  auto addIfExternalScalar = [&](Value value) {
    if (!value || !isScalarParamType(value.getType()) ||
        isConstantLikeValue(value))
      return;
    if (!isDefinedInside(value, source.getOperation()))
      captures.insert(value);
  };

  source.getBody().walk([&](Operation *op) {
    if (isa<arts::DbAccessPlanOp, sde::SdeMuDepOp>(op))
      return;
    for (Value operand : op->getOperands())
      addIfExternalScalar(operand);
  });
  return success();
}

static Value remapOrSelf(IRMapping &mapper, Value value) {
  if (Value mapped = mapper.lookupOrNull(value))
    return mapped;
  return value;
}

static bool enqueueForwardedMemrefResults(Operation *user, Value value,
                                          SmallVectorImpl<Value> &worklist) {
  if (auto cast = dyn_cast<memref::CastOp>(user)) {
    if (cast.getSource() != value)
      return false;
    worklist.push_back(cast.getResult());
    return true;
  }
  if (auto subview = dyn_cast<memref::SubViewOp>(user)) {
    if (subview.getSource() != value)
      return false;
    worklist.push_back(subview.getResult());
    return true;
  }
  if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(user)) {
    if (!llvm::is_contained(unrealized.getInputs(), value))
      return false;
    for (Value result : unrealized.getOutputs())
      if (isa<MemRefType>(result.getType()))
        worklist.push_back(result);
    return true;
  }
  return false;
}

static bool isReadOnlyMemrefUseInside(Value source, Operation *scope) {
  SmallVector<Value, 8> worklist;
  DenseSet<Value> visited;
  worklist.push_back(source);

  while (!worklist.empty()) {
    Value current = worklist.pop_back_val();
    if (!current || !visited.insert(current).second)
      continue;

    for (Operation *user : current.getUsers()) {
      if (!scope->isAncestor(user))
        continue;
      if (auto access = arts::DbUtils::getMemoryAccessInfo(user)) {
        if (access->memref == current &&
            access->kind == arts::DbUtils::MemoryAccessKind::Write)
          return false;
        continue;
      }
      if (auto dim = dyn_cast<memref::DimOp>(user)) {
        if (dim.getSource() == current)
          continue;
      }
      if (enqueueForwardedMemrefResults(user, current, worklist))
        continue;
      return false;
    }
  }

  return true;
}

static LogicalResult
collectRematerializableMemrefCaptures(sde::SdeCuRegionOp source,
                                      const DenseSet<Value> &allowedDbHandles,
                                      SetVector<Value> &captures) {
  bool failed = false;
  source.getBody().walk([&](Operation *op) {
    if (isa<arts::DbAccessPlanOp, sde::SdeYieldOp>(op))
      return;
    for (Value operand : op->getOperands()) {
      if (!isa<MemRefType>(operand.getType()) ||
          isDefinedInside(operand, source.getOperation()))
        continue;
      if (allowedDbHandles.contains(operand))
        continue;
      if (operand.getDefiningOp<memref::GetGlobalOp>()) {
        if (!isReadOnlyMemrefUseInside(operand, source.getOperation())) {
          op->emitError()
              << "writes or escapes a rematerialized memref.global inside a "
                 "standalone CU; mutable global state must be represented as "
                 "an explicit SDE/ARTS dependency";
          failed = true;
          continue;
        }
        captures.insert(operand);
        continue;
      }
      if (resolveBoundaryDbAlloc(operand)) {
        op->emitError()
            << "uses a DB-backed external memref that was not remapped to an "
               "EDT dependency; SDE must provide a committed access window for "
               "this standalone CU access";
        failed = true;
      }
    }
  });
  return failure(failed);
}

static FailureOr<SmallVector<int64_t, 4>>
getOwnerHaloRadii(ArrayAttr haloShape, unsigned ownerDimCount,
                  Operation *context) {
  SmallVector<int64_t, 4> radii(ownerDimCount, 0);
  if (!haloShape)
    return radii;

  std::optional<SmallVector<int64_t, 4>> parsed = readI64ArrayAttr(haloShape);
  if (!parsed) {
    context->emitError()
        << "has non-integer halo shape on a committed read dependency";
    return failure();
  }
  if (parsed->size() < ownerDimCount) {
    context->emitError()
        << "has halo shape with fewer entries than owner dimensions";
    return failure();
  }

  for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
    int64_t radius = (*parsed)[slot];
    if (radius < 0) {
      context->emitError()
          << "has negative halo radius on a committed read dependency";
      return failure();
    }
    radii[slot] = radius;
  }
  return radii;
}

static LogicalResult
rewriteOwnerIndicesToLocal(arts::EdtOp task, ArrayRef<Value> payloads,
                           ArrayRef<SmallVector<Value, 4>> depBlockOffsetArgs,
                           ArrayRef<bool> depRequiresDbRef,
                           ArrayRef<int64_t> groupBlockCounts,
                           unsigned ownerDimCount) {
  if (ownerDimCount == 0)
    return success();
  if (payloads.size() != depBlockOffsetArgs.size() ||
      payloads.size() != depRequiresDbRef.size() ||
      groupBlockCounts.size() != ownerDimCount)
    return task.emitOpError() << "has inconsistent owner grouping metadata";
  for (ArrayRef<Value> offsets : depBlockOffsetArgs)
    if (offsets.size() != ownerDimCount)
      return task.emitOpError() << "has inconsistent dependency block offsets";

  bool hasGroupedBlocks =
      llvm::any_of(groupBlockCounts, [](int64_t count) { return count > 1; });

  DenseMap<Value, unsigned> payloadIndices;
  DenseMap<Value, Value> payloadSources;
  for (auto [idx, payload] : llvm::enumerate(payloads)) {
    if (!hasGroupedBlocks && !depRequiresDbRef[idx])
      continue;
    auto [it, inserted] = payloadIndices.try_emplace(payload, idx);
    if (!inserted)
      return task.emitOpError()
             << "has duplicate dependency payload during owner-index rewrite";
    auto ref = payload.getDefiningOp<arts::DbRefOp>();
    if (!ref)
      return task.emitOpError()
             << "cannot reindex owner blocks without a DB-ref payload";
    payloadSources[payload] = ref.getSource();
  }

  if (hasGroupedBlocks) {
    for (Value payload : payloads) {
      if (payloadSources.contains(payload))
        continue;
      auto ref = payload.getDefiningOp<arts::DbRefOp>();
      if (!ref)
        return task.emitOpError()
               << "cannot group owner blocks without a DB-ref payload";
      payloadSources[payload] = ref.getSource();
    }
  }

  OpBuilder builder(task.getContext());
  auto rewriteAccess = [&](Operation *op, Value memref,
                           MutableOperandRange indices) -> WalkResult {
    Value root = ValueAnalysis::stripMemrefViewOps(memref);
    if (!llvm::is_contained(payloads, root))
      return WalkResult::advance();
    if (indices.size() < ownerDimCount) {
      op->emitError() << "rank-expanded DB payload access has fewer indices "
                         "than committed owner dimensions";
      return WalkResult::interrupt();
    }
    builder.setInsertionPoint(op);
    auto payloadIt = payloadIndices.find(root);
    if (hasGroupedBlocks || payloadIt != payloadIndices.end()) {
      if (root != memref) {
        op->emitError()
            << "grouped DB access through a memref view is not materialized; "
               "SDE-to-ARTS must rewrite the view or fail closed";
        return WalkResult::interrupt();
      }
      auto sourceIt = payloadSources.find(root);
      if (sourceIt == payloadSources.end()) {
        op->emitError() << "has no grouped dependency source";
        return WalkResult::interrupt();
      }
      unsigned depIdx =
          payloadIt == payloadIndices.end() ? 0 : payloadIt->second;
      ArrayRef<Value> blockOffsets = depBlockOffsetArgs[depIdx];
      SmallVector<Value, 4> localBlockIndices;
      localBlockIndices.reserve(ownerDimCount);
      for (unsigned idx = 0; idx < ownerDimCount; ++idx) {
        Value local = arith::SubIOp::create(
            builder, op->getLoc(), indices[idx].get(), blockOffsets[idx]);
        localBlockIndices.push_back(local);
      }
      Value groupedPayload = arts::DbRefOp::create(
          builder, op->getLoc(), sourceIt->second, localBlockIndices);
      if (auto load = dyn_cast<memref::LoadOp>(op))
        load->setOperand(0, groupedPayload);
      else if (auto store = dyn_cast<memref::StoreOp>(op))
        store->setOperand(1, groupedPayload);
      else {
        op->emitError() << "unsupported grouped DB payload access";
        return WalkResult::interrupt();
      }
    }
    for (unsigned idx = 0; idx < ownerDimCount; ++idx)
      indices[idx].set(createZeroIndex(builder, op->getLoc()));
    return WalkResult::advance();
  };

  WalkResult result = task.getBody().walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      return rewriteAccess(op, load.getMemref(), load.getIndicesMutable());
    }
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      return rewriteAccess(op, store.getMemref(), store.getIndicesMutable());
    }
    return WalkResult::advance();
  });
  if (result.wasInterrupted())
    return failure();

  for (Value payload : payloads)
    if (Operation *op = payload.getDefiningOp())
      if (op->use_empty())
        op->erase();
  return result.wasInterrupted() ? failure() : success();
}

static LogicalResult translateSdeAtomicsToArts(Region &region) {
  SmallVector<sde::SdeCuAtomicOp> atomics;
  region.walk([&](sde::SdeCuAtomicOp op) { atomics.push_back(op); });
  for (sde::SdeCuAtomicOp atomic : atomics) {
    if (atomic.getReductionKind() != sde::SdeReductionKind::add)
      return atomic.emitOpError()
             << "cannot materialize non-add SDE atomic at the ARTS boundary";
    OpBuilder builder(atomic);
    arts::AtomicAddOp::create(builder, atomic.getLoc(), atomic.getAddr(),
                              atomic.getValue());
    atomic.erase();
  }
  return success();
}

static LogicalResult
materializeStandaloneCuAccesses(sde::SdeCuRegionOp source) {
  if (!source || source->getParentOfType<sde::SdeSuIterateOp>())
    return success();

  SmallVector<DirectCuDepPlan, 4> deps;
  SmallVector<arts::DbAccessPlanOp, 4> plans;
  if (failed(collectStandaloneCuDependencies(source, deps, plans)))
    return failure();
  if (plans.empty())
    return success();

  if (source.getBody().empty())
    return source.emitOpError()
           << "has no body during standalone CU access materialization";
  if (!source.getIterArgs().empty())
    return source.emitOpError()
           << "access-bearing standalone CU iter_args are not representable "
              "as an ARTS EDT; materialize an explicit SDE dataflow first";
  for (Type resultType : source.getResultTypes())
    if (!isScalarParamType(resultType))
      return source.emitOpError()
             << "access-bearing standalone CU non-scalar results require an "
                "explicit SDE dataflow result before ARTS materialization";
  Block &body = source.getBody().front();
  if (body.getNumArguments() != 0)
    return source.emitOpError()
           << "has region arguments during standalone CU EDT materialization";
  OpBuilder builder(source.getContext());

  builder.setInsertionPoint(source);
  Location loc = source.getLoc();
  arts::ArtsLaunchPolicy standaloneLaunch;
  if (hasDistributedLaunchStoragePlan(deps)) {
    Value zero = createZeroIndex(builder, loc);
    standaloneLaunch = arts::resolveArtsOrdinalLaunchPolicy(
        source->getParentOfType<ModuleOp>(), zero, builder, loc);
  }
  Value standaloneRoute = standaloneLaunch.route
                              ? standaloneLaunch.route
                              : arts::createCurrentNodeRoute(builder, loc);

  for (DirectCuDepPlan &dep : deps) {
    SmallVector<Value> offsets;
    SmallVector<Value> sizes;
    offsets.reserve(dep.ownerDimCount);
    sizes.reserve(dep.ownerDimCount);
    for (unsigned idx = 0; idx < dep.ownerDimCount; ++idx) {
      offsets.push_back(createConstantIndex(builder, loc, dep.blockLo[idx]));
      sizes.push_back(createConstantIndex(builder, loc,
                                          dep.blockHi[idx] - dep.blockLo[idx]));
    }

    auto acquire = arts::DbAcquireOp::create(
        builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::block),
        SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
        SmallVector<Value>{}, SmallVector<Value>{}, Value{},
        SmallVector<Value>{}, SmallVector<Value>{});
    acquire.setPreserveAccessMode();
    if (dep.haloShape) {
      acquire.setDepPatternAttr(ArtsDepPatternAttr::get(
          source.getContext(), ArtsDepPattern::stencil));
      acquire.setDistributionPatternAttr(EdtDistributionPatternAttr::get(
          source.getContext(), EdtDistributionPattern::stencil));
    }
    dep.acquiredPtr = acquire.getPtr();
  }

  SmallVector<CuResultPlan, 4> resultPlans;
  resultPlans.reserve(source.getNumResults());
  for (Type resultType : source.getResultTypes()) {
    Value one = createOneIndex(builder, loc);
    Type payloadType = arts::getElementMemRefType(resultType, 1);
    Type pointerType = MemRefType::get({ShapedType::kDynamic}, payloadType);
    auto resultDb = arts::DbAllocOp::create(
        builder, loc, ArtsMode::inout, standaloneRoute, DbAllocType::heap,
        DbMode::write, resultType, pointerType, SmallVector<Value>{one},
        SmallVector<Value>{one}, arts::PartitionMode::coarse);
    auto writeAcquire = arts::DbAcquireOp::create(
        builder, loc, ArtsMode::out, resultDb.getGuid(), resultDb.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::coarse),
        SmallVector<Value>{}, SmallVector<Value>{createZeroIndex(builder, loc)},
        SmallVector<Value>{one}, SmallVector<Value>{}, SmallVector<Value>{},
        SmallVector<Value>{}, Value{}, SmallVector<Value>{},
        SmallVector<Value>{});
    writeAcquire.setPreserveAccessMode();
    resultPlans.push_back({resultDb, writeAcquire.getPtr(), Value{}});
  }

  DenseMap<Operation *, DirectCuDepPlan *> depsByAlloc;
  for (DirectCuDepPlan &dep : deps)
    depsByAlloc[dep.alloc.getOperation()] = &dep;

  auto rewriteAccess = [&](Operation *op, Value memref,
                           MutableOperandRange indices) -> WalkResult {
    arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
    auto it = depsByAlloc.find(alloc ? alloc.getOperation() : nullptr);
    if (it == depsByAlloc.end())
      return WalkResult::advance();

    DirectCuDepPlan &dep = *it->second;
    if (indices.size() < dep.ownerDimCount) {
      op->emitError() << "rank-expanded DB payload access has fewer indices "
                         "than committed owner dimensions";
      return WalkResult::interrupt();
    }

    builder.setInsertionPoint(op);
    SmallVector<Value, 4> localBlockIndices;
    localBlockIndices.reserve(dep.ownerDimCount);
    for (unsigned idx = 0; idx < dep.ownerDimCount; ++idx) {
      Value local = indices[idx].get();
      if (dep.blockLo[idx] != 0)
        local = arith::SubIOp::create(
            builder, op->getLoc(), local,
            createConstantIndex(builder, op->getLoc(), dep.blockLo[idx]));
      localBlockIndices.push_back(local);
    }
    Value payload = arts::DbRefOp::create(builder, op->getLoc(),
                                          dep.acquiredPtr, localBlockIndices);
    if (auto load = dyn_cast<memref::LoadOp>(op))
      load->setOperand(0, payload);
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      store->setOperand(1, payload);
    else {
      op->emitError() << "unsupported direct DB payload access";
      return WalkResult::interrupt();
    }
    for (unsigned idx = 0; idx < dep.ownerDimCount; ++idx)
      indices[idx].set(createZeroIndex(builder, op->getLoc()));
    return WalkResult::advance();
  };

  WalkResult rewriteResult = body.walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return rewriteAccess(op, load.getMemref(), load.getIndicesMutable());
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return rewriteAccess(op, store.getMemref(), store.getIndicesMutable());
    return WalkResult::advance();
  });
  if (rewriteResult.wasInterrupted())
    return failure();

  for (arts::DbAccessPlanOp plan : plans)
    if (plan && plan->getBlock())
      plan.erase();

  Operation *terminator = body.getTerminator();
  for (DirectCuDepPlan &dep : deps) {
    builder.setInsertionPoint(terminator ? terminator : &body.back());
    arts::DbReleaseOp::create(builder, loc, dep.acquiredPtr);
  }

  SetVector<Value> scalarCaptures;
  if (failed(collectExternalScalarCaptures(source, scalarCaptures)))
    return failure();
  DenseSet<Value> allowedDbHandles;
  for (DirectCuDepPlan &dep : deps) {
    allowedDbHandles.insert(dep.acquiredPtr);
    allowedDbHandles.insert(dep.alloc.getPtr());
  }
  for (CuResultPlan &result : resultPlans)
    allowedDbHandles.insert(result.writePtr);
  SetVector<Value> rematerializableMemrefCaptures;
  if (failed(collectRematerializableMemrefCaptures(
          source, allowedDbHandles, rematerializableMemrefCaptures)))
    return failure();

  SmallVector<Value, 4> taskDeps;
  taskDeps.reserve(deps.size() + resultPlans.size());
  for (DirectCuDepPlan &dep : deps)
    taskDeps.push_back(dep.acquiredPtr);
  for (CuResultPlan &result : resultPlans)
    taskDeps.push_back(result.writePtr);

  SmallVector<Value, 8> taskParams(scalarCaptures.begin(),
                                   scalarCaptures.end());

  builder.setInsertionPoint(source);
  arts::ArtsLaunchPolicy launch = standaloneLaunch;
  Value route = standaloneRoute;
  auto task =
      arts::EdtOp::create(builder, loc, arts::EdtType::sync, launch.concurrency,
                          route, taskDeps, taskParams);

  Block &taskBlock = task.getBody().front();
  for (Value dep : taskDeps)
    taskBlock.addArgument(dep.getType(), loc);
  unsigned paramOffset = taskBlock.getNumArguments();
  for (Value param : taskParams)
    taskBlock.addArgument(param.getType(), loc);

  IRMapping mapper;
  for (auto [idx, dep] : llvm::enumerate(deps)) {
    Value depArg = taskBlock.getArgument(idx);
    mapper.map(dep.acquiredPtr, depArg);
    mapper.map(dep.alloc.getPtr(), depArg);
  }
  unsigned resultDepBase = deps.size();
  for (auto [idx, result] : llvm::enumerate(resultPlans))
    mapper.map(result.writePtr, taskBlock.getArgument(resultDepBase + idx));
  for (auto [idx, param] : llvm::enumerate(taskParams))
    mapper.map(param, taskBlock.getArgument(paramOffset + idx));

  auto yield = dyn_cast_or_null<sde::SdeYieldOp>(body.getTerminator());
  if (source.getNumResults() != 0 &&
      (!yield || yield.getValues().size() != source.getNumResults()))
    return source.emitOpError()
           << "has mismatched yield/result count during standalone CU EDT "
              "materialization";

  OpBuilder bodyBuilder(task.getContext());
  bodyBuilder.setInsertionPointToStart(&taskBlock);
  for (Value capture : rematerializableMemrefCaptures) {
    Operation *def = capture.getDefiningOp();
    Operation *cloned = def->clone(mapper);
    bodyBuilder.insert(cloned);
    mapper.map(capture, cloned->getResult(0));
  }
  for (Operation &nested : body) {
    if (isa<arts::DbAccessPlanOp, sde::SdeYieldOp>(&nested))
      continue;
    bodyBuilder.insert(nested.clone(mapper));
  }
  if (failed(translateSdeAtomicsToArts(task.getBody())))
    return failure();
  bodyBuilder.setInsertionPointToEnd(&taskBlock);
  for (auto [idx, result] : llvm::enumerate(resultPlans)) {
    Value zero = createZeroIndex(bodyBuilder, loc);
    Value payload =
        arts::DbRefOp::create(bodyBuilder, loc,
                              taskBlock.getArgument(resultDepBase + idx),
                              SmallVector<Value>{zero})
            .getResult();
    Value yielded = remapOrSelf(mapper, yield.getValues()[idx]);
    memref::StoreOp::create(bodyBuilder, loc, yielded, payload,
                            SmallVector<Value>{zero});
    arts::DbReleaseOp::create(bodyBuilder, loc,
                              taskBlock.getArgument(resultDepBase + idx));
  }
  arts::YieldOp::create(bodyBuilder, loc);

  builder.setInsertionPointAfter(task);
  for (auto [idx, result] : llvm::enumerate(resultPlans)) {
    Value zero = createZeroIndex(builder, loc);
    Value one = createOneIndex(builder, loc);
    auto readAcquire = arts::DbAcquireOp::create(
        builder, loc, ArtsMode::in, result.alloc.getGuid(),
        result.alloc.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::coarse),
        SmallVector<Value>{}, SmallVector<Value>{zero}, SmallVector<Value>{one},
        SmallVector<Value>{}, SmallVector<Value>{}, SmallVector<Value>{},
        Value{}, SmallVector<Value>{}, SmallVector<Value>{});
    readAcquire.setPreserveAccessMode();
    Value payload = arts::DbRefOp::create(builder, loc, readAcquire.getPtr(),
                                          SmallVector<Value>{zero})
                        .getResult();
    Value loaded =
        memref::LoadOp::create(builder, loc, payload, SmallVector<Value>{zero});
    arts::DbReleaseOp::create(builder, loc, readAcquire.getPtr());
    result.replacement = loaded;
    source.getResult(idx).replaceAllUsesWith(loaded);
  }

  source.erase();
  return success();
}

static void buildWholeDbAcquireWindow(OpBuilder &builder, Location loc,
                                      arts::DbAllocOp alloc,
                                      SmallVectorImpl<Value> &offsets,
                                      SmallVectorImpl<Value> &sizes) {
  offsets.clear();
  sizes.clear();
  offsets.reserve(alloc.getSizes().size());
  sizes.reserve(alloc.getSizes().size());
  for (Value size : alloc.getSizes()) {
    offsets.push_back(createZeroIndex(builder, loc));
    sizes.push_back(size);
  }
  if (sizes.empty()) {
    offsets.push_back(createZeroIndex(builder, loc));
    sizes.push_back(createOneIndex(builder, loc));
  }
}

static LogicalResult
convertCoarseSuIterate(sde::SdeSuIterateOp source,
                       SmallVectorImpl<CoarseSuDependency> &deps) {
  if (deps.empty())
    return source.emitOpError()
           << "has no DB-backed accesses for coarse SDE-to-ARTS SU "
              "materialization";
  if (source.getPhysicalOwnerDimsAttr() || source.getPhysicalBlockShapeAttr())
    return source.emitOpError()
           << "has committed physical partition facts but no access-window "
              "dependencies; refusing coarse ARTS materialization";
  if (source.getLogicalWorkerSliceAttr() || source.getPhysicalHaloShapeAttr() ||
      source.getAccessMinOffsetsAttr() || source.getAccessMaxOffsetsAttr() ||
      source.getOwnerDimsAttr() || source.getSpatialDimsAttr() ||
      source.getWriteFootprintAttr() || source.getLayoutsDisagreeAttr())
    return source.emitOpError()
           << "has movement, halo, or physical scheduling facts without "
              "committed access windows; refusing coarse ARTS materialization";

  unsigned loopRank = source.getUpperBounds().size();
  if (source.getLowerBounds().size() != loopRank ||
      source.getSteps().size() != loopRank ||
      source.getBody().front().getNumArguments() < loopRank)
    return source.emitOpError() << "has inconsistent loop bounds";

  SetVector<Value> scalarCaptures;
  if (failed(collectExternalScalarCaptures(source, scalarCaptures)))
    return failure();

  Location loc = source.getLoc();
  OpBuilder builder(source);
  SmallVector<Value, 4> taskDeps;
  taskDeps.reserve(deps.size());
  for (CoarseSuDependency &dep : deps) {
    SmallVector<Value> offsets;
    SmallVector<Value> sizes;
    buildWholeDbAcquireWindow(builder, loc, dep.alloc, offsets, sizes);
    auto acquire = arts::DbAcquireOp::create(
        builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::coarse),
        SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
        SmallVector<Value>{}, SmallVector<Value>{}, Value{},
        SmallVector<Value>{}, SmallVector<Value>{});
    acquire.setPreserveAccessMode();
    acquire.setPreserveDepEdge();
    taskDeps.push_back(acquire.getPtr());
  }

  SmallVector<Value, 8> taskParams;
  for (Value capture : scalarCaptures)
    taskParams.push_back(capture);
  auto appendParamIfMissing = [&](Value value) {
    if (!value || !isScalarParamType(value.getType()) ||
        isConstantLikeValue(value) || llvm::is_contained(taskParams, value))
      return;
    taskParams.push_back(value);
  };
  for (CoarseSuDependency &dep : deps) {
    for (Value size : dep.alloc.getSizes())
      appendParamIfMissing(size);
    for (Value elementSize : dep.alloc.getElementSizes())
      appendParamIfMissing(elementSize);
  }

  Value route = arts::createCurrentNodeRoute(builder, loc);
  auto task = arts::EdtOp::create(builder, loc, arts::EdtType::task,
                                  arts::EdtConcurrency::intranode, route,
                                  taskDeps, taskParams);
  if (failed(attachUnpartitionedSdeFacts(source, task)))
    return failure();

  Block &taskBlock = task.getBody().front();
  for (Value dep : taskDeps)
    taskBlock.addArgument(dep.getType(), loc);
  unsigned paramOffset = taskBlock.getNumArguments();
  for (Value param : taskParams)
    taskBlock.addArgument(param.getType(), loc);

  IRMapping mapper;
  OpBuilder bodyBuilder(task.getContext());
  bodyBuilder.setInsertionPointToStart(&taskBlock);
  SmallVector<Value, 4> payloads;
  payloads.reserve(deps.size());
  for (auto [idx, dep] : llvm::enumerate(deps)) {
    Value payload = arts::materializeDbInnerPayload(bodyBuilder, loc,
                                                    taskBlock.getArgument(idx));
    payloads.push_back(payload);
    mapper.map(dep.alloc.getPtr(), taskBlock.getArgument(idx));
  }

  source.getBody().walk([&](Operation *op) {
    Value memref;
    if (auto load = dyn_cast<memref::LoadOp>(op))
      memref = load.getMemref();
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      memref = store.getMemref();
    else if (auto atomic = dyn_cast<sde::SdeCuAtomicOp>(op))
      memref = atomic.getAddr();
    else
      return;
    arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
    if (!alloc)
      return;
    auto it = llvm::find_if(deps, [&](const CoarseSuDependency &dep) {
      return dep.alloc == alloc;
    });
    if (it == deps.end())
      return;
    unsigned depIdx = static_cast<unsigned>(std::distance(deps.begin(), it));
    mapper.map(memref, payloads[depIdx]);
    Value root = ValueAnalysis::stripMemrefViewOps(memref);
    if (root && root != memref)
      mapper.map(root, payloads[depIdx]);
  });
  for (auto [idx, param] : llvm::enumerate(taskParams))
    mapper.map(param, taskBlock.getArgument(paramOffset + idx));

  for (unsigned dim = 0; dim < loopRank; ++dim) {
    Value lower = remapOrSelf(mapper, source.getLowerBounds()[dim]);
    Value upper = remapOrSelf(mapper, source.getUpperBounds()[dim]);
    Value step = remapOrSelf(mapper, source.getSteps()[dim]);
    auto localLoop = scf::ForOp::create(bodyBuilder, loc, lower, upper, step);
    mapper.map(source.getBody().front().getArgument(dim),
               localLoop.getInductionVar());
    bodyBuilder.setInsertionPointToStart(localLoop.getBody());
  }

  Block *computeBlock = sde::getSuIterateComputeBlock(source);
  if (!computeBlock)
    return source.emitOpError() << "has no computable body";
  for (Operation &nested : computeBlock->without_terminator()) {
    if (isa<arts::DbAccessPlanOp>(&nested))
      continue;
    bodyBuilder.insert(nested.clone(mapper));
  }

  if (failed(translateSdeAtomicsToArts(task.getBody())))
    return failure();
  bodyBuilder.setInsertionPointToEnd(&taskBlock);
  arts::YieldOp::create(bodyBuilder, loc);

  bool needsCompletionBarrier = !source.getNowaitAttr();
  MLIRContext *ctx = source.getContext();
  source.erase();
  if (needsCompletionBarrier) {
    OpBuilder barrierBuilder(task);
    barrierBuilder.setInsertionPointAfter(task);
    auto reason = arts::ArtsBarrierReasonAttr::get(
        ctx, arts::ArtsBarrierReason::required_memory);
    arts::BarrierOp::create(barrierBuilder, loc, reason);
  }
  return success();
}

static LogicalResult
convertSuIterate(sde::SdeSuIterateOp source,
                 DenseSet<Operation *> &consumedCuLevelAccessPlans) {
  if (source.getNumResults() != 0 || !source.getReductionAccumulators().empty())
    return source.emitOpError()
           << "direct SDE-to-ARTS lowering requires reduction/result facts to "
              "be authored as explicit SDE-to-ARTS reduction operations";

  SmallVector<DirectDepPlan, 4> deps;
  if (failed(collectSuDependencies(source, deps, consumedCuLevelAccessPlans)))
    return failure();
  if (deps.empty()) {
    SmallVector<CoarseSuDependency, 4> coarseDeps;
    if (failed(collectCoarseSuDependencies(source, coarseDeps)))
      return failure();
    return convertCoarseSuIterate(source, coarseDeps);
  }

  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(source.getPhysicalOwnerDimsAttr());
  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(source.getPhysicalBlockShapeAttr());
  if (!ownerDims || ownerDims->empty() || !blockShape || blockShape->empty())
    return source.emitOpError()
           << "requires committed physicalOwnerDims and physicalBlockShape for "
              "direct ARTS dispatch";

  unsigned loopRank = source.getUpperBounds().size();
  if (source.getLowerBounds().size() != loopRank ||
      source.getSteps().size() != loopRank ||
      source.getBody().front().getNumArguments() < loopRank)
    return source.emitOpError() << "has inconsistent loop bounds";

  FailureOr<OwnerSlotPlan> ownerPlan = resolveOwnerSlotPlan(
      *ownerDims, *blockShape, loopRank, source.getOperation());
  if (failed(ownerPlan))
    return failure();

  ArrayRef<int64_t> ownerSlotDims = ownerPlan->ownerDims;
  unsigned ownerDimCount = ownerSlotDims.size();
  for (DirectDepPlan &dep : deps)
    if (dep.ownerDimCount != ownerDimCount)
      return source.emitOpError()
             << "dependency owner-dim count does not match physicalOwnerDims";

  SmallVector<int64_t, 4> ownerBlockSizes(ownerPlan->blockSizes.begin(),
                                          ownerPlan->blockSizes.end());

  SmallVector<int64_t, 4> workerSpans(ownerBlockSizes.begin(),
                                      ownerBlockSizes.end());
  SmallVector<int64_t, 4> groupBlockCounts(ownerDimCount, 1);
  if (auto workerSlice = readI64ArrayAttr(source.getLogicalWorkerSliceAttr())) {
    if (workerSlice->size() != loopRank && workerSlice->size() != ownerDimCount)
      return source.emitOpError()
             << "commits logicalWorkerSlice whose rank does not match the "
                "iteration or owner rank";
    for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
      unsigned physicalDim = ownerPlan->loopDims[slot];
      int64_t span = workerSlice->size() == loopRank
                         ? (*workerSlice)[physicalDim]
                         : (*workerSlice)[ownerPlan->rawSlots[slot]];
      int64_t blockSize = ownerBlockSizes[slot];
      if (span <= 0 || span < blockSize || span % blockSize != 0)
        return source.emitOpError()
               << "commits logicalWorkerSlice that cannot be represented as "
                  "a whole-number group of physical DB blocks";
      workerSpans[slot] = span;
      groupBlockCounts[slot] = span / blockSize;
    }
  }

  if (hasDistributedWriterStoragePlan(deps)) {
    std::optional<int64_t> totalNodes =
        arts::getRuntimeTotalNodes(source->getParentOfType<ModuleOp>());
    if (!totalNodes)
      return source.emitOpError()
             << "requires runtime node count to keep grouped distributed "
                "writers owner-local";
    if (failed(verifyDistributedWriterGroupingOwnerLocal(
            source, deps, groupBlockCounts, *totalNodes)))
      return failure();
  }

  SetVector<Value> scalarCaptures;
  if (failed(collectExternalScalarCaptures(source, scalarCaptures)))
    return failure();

  Location loc = source.getLoc();
  OpBuilder builder(source);
  SmallVector<Value, 4> dispatchBases;
  SmallVector<Value, 4> dispatchBlockOffsets;
  scf::ForOp dispatchRoot;
  for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
    unsigned physicalDim = ownerPlan->loopDims[slot];
    Value step = createConstantIndex(builder, loc, workerSpans[slot]);
    auto loop =
        scf::ForOp::create(builder, loc, source.getLowerBounds()[physicalDim],
                           source.getUpperBounds()[physicalDim], step);
    if (!dispatchRoot)
      dispatchRoot = loop;
    dispatchBases.push_back(loop.getInductionVar());
    builder.setInsertionPointToStart(loop.getBody());
  }

  for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
    unsigned physicalDim = ownerPlan->loopDims[slot];
    Value base = dispatchBases[slot];
    Value lower = source.getLowerBounds()[physicalDim];
    Value delta = arith::SubIOp::create(builder, loc, base, lower);
    Value blockSize = createConstantIndex(builder, loc, ownerBlockSizes[slot]);
    dispatchBlockOffsets.push_back(
        arith::DivUIOp::create(builder, loc, delta, blockSize));
  }

  SmallVector<Value, 4> taskDeps;
  taskDeps.reserve(deps.size());
  SmallVector<SmallVector<Value, 4>> depBlockOffsets;
  SmallVector<bool> depRequiresDbRef;
  depBlockOffsets.reserve(deps.size());
  depRequiresDbRef.reserve(deps.size());
  for (DirectDepPlan &dep : deps) {
    SmallVector<Value> offsets(dispatchBlockOffsets.begin(),
                               dispatchBlockOffsets.end());
    SmallVector<Value> sizes;
    sizes.reserve(ownerDimCount);
    for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
      Value remaining = arith::SubIOp::create(
          builder, loc, dep.alloc.getSizes()[slot], dispatchBlockOffsets[slot]);
      sizes.push_back(arith::MinUIOp::create(
          builder, loc, remaining,
          createConstantIndex(builder, loc, groupBlockCounts[slot])));
    }
    bool needsDepSpecificDbRef = false;
    if (dep.haloShape) {
      if (dep.mode != ArtsMode::in)
        return source.emitOpError()
               << "commits a halo dependency that is not read-only; ARTS "
                  "cannot materialize a writable halo window";
      FailureOr<SmallVector<int64_t, 4>> haloRadii = getOwnerHaloRadii(
          dep.haloShape, ownerDimCount, source.getOperation());
      if (failed(haloRadii))
        return failure();
      Value zero = createZeroIndex(builder, loc);
      for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
        int64_t radius = (*haloRadii)[slot];
        if (radius <= 0)
          continue;
        Value halo = createConstantIndex(builder, loc, radius);
        Value canShiftLeft = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::uge, offsets[slot], halo);
        Value shiftedLeft =
            arith::SubIOp::create(builder, loc, offsets[slot], halo);
        Value expandedOffset = arith::SelectOp::create(
            builder, loc, canShiftLeft, shiftedLeft, zero);
        Value leftGrowth =
            arith::SubIOp::create(builder, loc, offsets[slot], expandedOffset);
        Value desired =
            arith::AddIOp::create(builder, loc, leftGrowth, sizes[slot]);
        desired = arith::AddIOp::create(builder, loc, desired, halo);
        Value remaining = arith::SubIOp::create(
            builder, loc, dep.alloc.getSizes()[slot], expandedOffset);
        offsets[slot] = expandedOffset;
        sizes[slot] = arith::MinUIOp::create(builder, loc, remaining, desired);
        needsDepSpecificDbRef = true;
      }
    }
    depBlockOffsets.push_back(
        SmallVector<Value, 4>(offsets.begin(), offsets.end()));
    depRequiresDbRef.push_back(needsDepSpecificDbRef);
    SmallVector<Value> elementOffsets;
    SmallVector<Value> elementSizes;
    if (dep.haloShape) {
      if (dep.alloc.getElementSizes().empty())
        return source.emitOpError()
               << "commits a halo dependency without DB element-size facts; "
                  "ARTS-RT must not infer halo byte windows";
      elementOffsets.reserve(dep.alloc.getElementSizes().size());
      elementSizes.reserve(dep.alloc.getElementSizes().size());
      for (Value elementSize : dep.alloc.getElementSizes()) {
        elementOffsets.push_back(createZeroIndex(builder, loc));
        elementSizes.push_back(elementSize);
      }
    }
    auto acquire = arts::DbAcquireOp::create(
        builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::block),
        SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
        SmallVector<Value>{}, SmallVector<Value>{}, Value{}, elementOffsets,
        elementSizes);
    acquire.setPreserveAccessMode();
    if (dep.haloShape) {
      acquire.setDepPatternAttr(ArtsDepPatternAttr::get(
          source.getContext(), ArtsDepPattern::stencil));
      acquire.setDistributionPatternAttr(EdtDistributionPatternAttr::get(
          source.getContext(), EdtDistributionPattern::stencil));
      if (auto minOffsets = source.getAccessMinOffsetsAttr())
        acquire->setAttr(acquire.getStencilMinOffsetsAttrName(), minOffsets);
      if (auto maxOffsets = source.getAccessMaxOffsetsAttr())
        acquire->setAttr(acquire.getStencilMaxOffsetsAttrName(), maxOffsets);
      if (auto ownerDims = source.getOwnerDimsAttr())
        acquire->setAttr(acquire.getStencilOwnerDimsAttrName(), ownerDims);
      if (auto spatialDims = source.getSpatialDimsAttr())
        acquire->setAttr(acquire.getStencilSpatialDimsAttrName(), spatialDims);
      acquire->setAttr(acquire.getStencilSupportedBlockHaloAttrName(),
                       UnitAttr::get(source.getContext()));
    }
    taskDeps.push_back(acquire.getPtr());
  }

  SmallVector<Value, 8> taskParams;
  taskParams.append(dispatchBases.begin(), dispatchBases.end());
  taskParams.append(dispatchBlockOffsets.begin(), dispatchBlockOffsets.end());

  auto appendParamIfMissing = [&](Value value) -> unsigned {
    auto it = llvm::find(taskParams, value);
    if (it != taskParams.end())
      return static_cast<unsigned>(std::distance(taskParams.begin(), it));
    taskParams.push_back(value);
    return taskParams.size() - 1;
  };

  SmallVector<SmallVector<unsigned, 4>> depBlockOffsetParamIndices;
  depBlockOffsetParamIndices.reserve(depBlockOffsets.size());
  for (ArrayRef<Value> offsets : depBlockOffsets) {
    SmallVector<unsigned, 4> paramIndices;
    paramIndices.reserve(ownerDimCount);
    for (Value offset : offsets)
      paramIndices.push_back(appendParamIfMissing(offset));
    depBlockOffsetParamIndices.push_back(std::move(paramIndices));
  }

  for (Value capture : scalarCaptures)
    appendParamIfMissing(capture);

  arts::ArtsLaunchPolicy launch = arts::resolveArtsLaunchPolicy(
      source->getParentOfType<ModuleOp>(), dispatchRoot,
      hasDistributedLaunchStoragePlan(deps), builder, loc);
  Value route =
      launch.route ? launch.route : arts::createCurrentNodeRoute(builder, loc);
  auto task =
      arts::EdtOp::create(builder, loc, arts::EdtType::task, launch.concurrency,
                          route, taskDeps, taskParams);
  if (failed(attachCommittedSdeFacts(source, task)))
    return failure();

  Block &taskBlock = task.getBody().front();
  for (Value dep : taskDeps)
    taskBlock.addArgument(dep.getType(), loc);
  unsigned paramOffset = taskBlock.getNumArguments();
  for (Value param : taskParams)
    taskBlock.addArgument(param.getType(), loc);

  IRMapping mapper;
  SmallVector<Value, 4> payloads;
  OpBuilder bodyBuilder(task.getContext());
  bodyBuilder.setInsertionPointToStart(&taskBlock);
  for (auto [idx, dep] : llvm::enumerate(deps)) {
    Value payload = arts::materializeDbInnerPayload(bodyBuilder, loc,
                                                    taskBlock.getArgument(idx));
    payloads.push_back(payload);
    mapper.map(dep.alloc.getPtr(), taskBlock.getArgument(idx));
  }
  source.getBody().walk([&](Operation *op) {
    Value memref;
    if (auto load = dyn_cast<memref::LoadOp>(op))
      memref = load.getMemref();
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      memref = store.getMemref();
    else
      return;
    arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
    if (!alloc)
      return;
    auto it = llvm::find_if(
        deps, [&](const DirectDepPlan &dep) { return dep.alloc == alloc; });
    if (it == deps.end())
      return;
    unsigned depIdx = static_cast<unsigned>(std::distance(deps.begin(), it));
    mapper.map(memref, payloads[depIdx]);
    Value root = ValueAnalysis::stripMemrefViewOps(memref);
    if (root && root != memref)
      mapper.map(root, payloads[depIdx]);
  });
  for (auto [idx, param] : llvm::enumerate(taskParams))
    mapper.map(param, taskBlock.getArgument(paramOffset + idx));
  for (auto [idx, base] : llvm::enumerate(dispatchBases))
    mapper.map(base, taskBlock.getArgument(paramOffset + idx));
  for (auto [idx, offset] : llvm::enumerate(dispatchBlockOffsets))
    mapper.map(offset,
               taskBlock.getArgument(paramOffset + ownerDimCount + idx));
  SmallVector<SmallVector<Value, 4>> taskDepBlockOffsetArgs;
  taskDepBlockOffsetArgs.reserve(depBlockOffsets.size());
  for (unsigned depIdx = 0; depIdx < depBlockOffsets.size(); ++depIdx) {
    SmallVector<Value, 4> offsets;
    offsets.reserve(ownerDimCount);
    for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
      unsigned paramIndex = depBlockOffsetParamIndices[depIdx][slot];
      offsets.push_back(taskBlock.getArgument(paramOffset + paramIndex));
    }
    taskDepBlockOffsetArgs.push_back(std::move(offsets));
  }

  WalkResult accessPlanMapResult =
      source.getBody().walk([&](arts::DbAccessPlanOp plan) {
        auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
            arts::DbUtils::getUnderlyingDbAlloc(plan.getMu()));
        if (!alloc) {
          plan.emitOpError() << "lost backing DB allocation during direct "
                                "SDE-to-ARTS lowering";
          return WalkResult::interrupt();
        }
        auto it = llvm::find_if(
            deps, [&](const DirectDepPlan &dep) { return dep.alloc == alloc; });
        if (it == deps.end()) {
          plan.emitOpError() << "has no matching direct ARTS dependency";
          return WalkResult::interrupt();
        }
        unsigned depIdx =
            static_cast<unsigned>(std::distance(deps.begin(), it));
        mapper.map(plan.getMu(), payloads[depIdx]);
        return WalkResult::advance();
      });
  if (accessPlanMapResult.wasInterrupted())
    return failure();

  for (unsigned dim = 0; dim < loopRank; ++dim) {
    auto ownerIt = llvm::find(ownerPlan->loopDims, dim);
    Value lower;
    Value upper = remapOrSelf(mapper, source.getUpperBounds()[dim]);
    Value step = remapOrSelf(mapper, source.getSteps()[dim]);
    if (ownerIt != ownerPlan->loopDims.end()) {
      unsigned slot = static_cast<unsigned>(
          std::distance(ownerPlan->loopDims.begin(), ownerIt));
      lower = mapper.lookup(dispatchBases[slot]);
      Value localEnd = arith::AddIOp::create(
          bodyBuilder, loc, lower,
          createConstantIndex(bodyBuilder, loc, workerSpans[slot]));
      upper = arith::MinUIOp::create(bodyBuilder, loc, localEnd, upper);
    } else {
      lower = remapOrSelf(mapper, source.getLowerBounds()[dim]);
    }
    auto localLoop = scf::ForOp::create(bodyBuilder, loc, lower, upper, step);
    mapper.map(source.getBody().front().getArgument(dim),
               localLoop.getInductionVar());
    bodyBuilder.setInsertionPointToStart(localLoop.getBody());
  }

  Block *computeBlock = sde::getSuIterateComputeBlock(source);
  if (!computeBlock)
    return source.emitOpError() << "has no computable body";
  for (Operation &nested : computeBlock->without_terminator()) {
    if (isa<arts::DbAccessPlanOp>(&nested))
      continue;
    bodyBuilder.insert(nested.clone(mapper));
  }

  if (failed(translateSdeAtomicsToArts(task.getBody())))
    return failure();
  if (failed(rewriteOwnerIndicesToLocal(task, payloads, taskDepBlockOffsetArgs,
                                        depRequiresDbRef, groupBlockCounts,
                                        ownerDimCount)))
    return failure();
  bodyBuilder.setInsertionPointToEnd(&taskBlock);
  arts::YieldOp::create(bodyBuilder, loc);

  bool needsCompletionBarrier = !source.getNowaitAttr();
  MLIRContext *ctx = source.getContext();
  source.erase();
  if (needsCompletionBarrier) {
    OpBuilder barrierBuilder(dispatchRoot);
    barrierBuilder.setInsertionPointAfter(dispatchRoot);
    auto reason = arts::ArtsBarrierReasonAttr::get(
        ctx, arts::ArtsBarrierReason::required_memory);
    arts::BarrierOp::create(barrierBuilder, loc, reason);
  }
  return success();
}

struct TaskDepPlan {
  sde::SdeMuDepOp dep;
  arts::DbAllocOp alloc;
  ArtsMode mode = ArtsMode::uninitialized;
  SmallVector<Value, 4> offsets;
  SmallVector<Value, 4> sizes;
};

static LogicalResult
collectTaskDependencies(sde::SdeCuTaskOp source,
                        SmallVectorImpl<TaskDepPlan> &deps) {
  WalkResult result =
      source.getBody().walk([&](sde::SdeMuDepOp dep) {
        if (!dep.getDep().use_empty()) {
          dep.emitOpError()
              << "result is consumed; SDE task dependencies must remain local "
                 "declarations before ARTS materialization";
          return WalkResult::interrupt();
        }
        auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
            arts::DbUtils::getUnderlyingDbAlloc(dep.getSource()));
        if (!alloc) {
          dep.emitOpError()
              << "does not reference an ARTS DB-backed memref after storage "
                 "materialization";
          return WalkResult::interrupt();
        }
        FailureOr<ArtsMode> mode = convertAccessMode(dep.getMode(), dep);
        if (failed(mode))
          return WalkResult::interrupt();
        deps.push_back({dep, alloc, *mode,
                        SmallVector<Value, 4>(dep.getOffsets().begin(),
                                              dep.getOffsets().end()),
                        SmallVector<Value, 4>(dep.getSizes().begin(),
                                              dep.getSizes().end())});
        return WalkResult::advance();
      });
  return result.wasInterrupted() ? failure() : success();
}

static LogicalResult convertCuTask(sde::SdeCuTaskOp source) {
  SmallVector<TaskDepPlan, 4> deps;
  if (failed(collectTaskDependencies(source, deps)))
    return failure();

  SetVector<Value> scalarCaptures;
  if (failed(collectExternalScalarCaptures(source, scalarCaptures)))
    return failure();

  Location loc = source.getLoc();
  OpBuilder builder(source);
  SmallVector<Value, 4> taskDeps;
  taskDeps.reserve(deps.size());
  for (TaskDepPlan &dep : deps) {
    SmallVector<Value> offsets;
    SmallVector<Value> sizes;
    buildWholeDbAcquireWindow(builder, loc, dep.alloc, offsets, sizes);
    std::optional<arts::PartitionMode> partitionMode =
        dep.offsets.empty() && dep.sizes.empty()
            ? std::optional<arts::PartitionMode>(arts::PartitionMode::coarse)
            : std::optional<arts::PartitionMode>(arts::PartitionMode::block);
    auto acquire = arts::DbAcquireOp::create(
        builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
        partitionMode, SmallVector<Value>{}, offsets, sizes,
        SmallVector<Value>{},
        SmallVector<Value>(dep.offsets.begin(), dep.offsets.end()),
        SmallVector<Value>(dep.sizes.begin(), dep.sizes.end()), Value{},
        SmallVector<Value>{}, SmallVector<Value>{});
    acquire.setPreserveAccessMode();
    acquire.setPreserveDepEdge();
    taskDeps.push_back(acquire.getPtr());
  }

  SmallVector<Value, 8> taskParams;
  for (Value capture : scalarCaptures)
    taskParams.push_back(capture);
  auto appendParamIfMissing = [&](Value value) {
    if (!value || !isScalarParamType(value.getType()) ||
        isConstantLikeValue(value) || llvm::is_contained(taskParams, value))
      return;
    taskParams.push_back(value);
  };
  for (TaskDepPlan &dep : deps) {
    for (Value size : dep.alloc.getSizes())
      appendParamIfMissing(size);
    for (Value elementSize : dep.alloc.getElementSizes())
      appendParamIfMissing(elementSize);
  }

  Value route = arts::createCurrentNodeRoute(builder, loc);
  auto task = arts::EdtOp::create(builder, loc, arts::EdtType::task,
                                  arts::EdtConcurrency::intranode, route,
                                  taskDeps, taskParams);
  if (auto pattern = source.getPatternAttr()) {
    FailureOr<ArtsDepPattern> depPattern =
        convertPattern(pattern.getValue(), source.getOperation());
    if (failed(depPattern))
      return failure();
    arts::setDepPattern(task.getOperation(), *depPattern);
  }

  Block &taskBlock = task.getBody().front();
  for (Value dep : taskDeps)
    taskBlock.addArgument(dep.getType(), loc);
  unsigned paramOffset = taskBlock.getNumArguments();
  for (Value param : taskParams)
    taskBlock.addArgument(param.getType(), loc);

  IRMapping mapper;
  OpBuilder bodyBuilder(task.getContext());
  bodyBuilder.setInsertionPointToStart(&taskBlock);
  for (auto [idx, dep] : llvm::enumerate(deps)) {
    Value payload = arts::materializeDbInnerPayload(bodyBuilder, loc,
                                                    taskBlock.getArgument(idx));
    mapper.map(dep.alloc.getPtr(), taskBlock.getArgument(idx));
    Value sourceMemref = dep.dep.getSource();
    mapper.map(sourceMemref, payload);
    if (Value root = ValueAnalysis::stripMemrefViewOps(sourceMemref))
      mapper.map(root, payload);
  }
  for (auto [idx, param] : llvm::enumerate(taskParams))
    mapper.map(param, taskBlock.getArgument(paramOffset + idx));

  for (Operation &nested : source.getBody().front()) {
    if (isa<sde::SdeMuDepOp>(&nested))
      continue;
    bodyBuilder.insert(nested.clone(mapper));
  }

  if (failed(translateSdeAtomicsToArts(task.getBody())))
    return failure();
  bodyBuilder.setInsertionPointToEnd(&taskBlock);
  arts::YieldOp::create(bodyBuilder, loc);

  source.erase();
  return success();
}

static LogicalResult lowerSdeResourceQuery(sde::SdeResourceQueryOp op) {
  OpBuilder builder(op);
  switch (op.getKind()) {
  case sde::SdeResourceQueryKind::logicalWorkers: {
    auto runtimeQuery = arts::RuntimeQueryOp::create(
        builder, op.getLoc(), arts::RuntimeQueryKind::totalWorkers);
    Value asIndex = arith::IndexCastOp::create(
        builder, op.getLoc(), builder.getIndexType(), runtimeQuery.getResult());
    op.getResult().replaceAllUsesWith(asIndex);
    op.erase();
    return success();
  }
  }
  return op.emitOpError() << "unsupported SDE resource query kind";
}

static LogicalResult lowerSdeControlBarrier(sde::SdeSuBarrierOp op) {
  OpBuilder builder(op);
  auto reasonAttr = op.getBarrierReasonAttr();
  arts::BarrierOp::create(
      builder, op.getLoc(),
      reasonAttr ? arts::ArtsBarrierReasonAttr::get(
                       op.getContext(), static_cast<arts::ArtsBarrierReason>(
                                            reasonAttr.getValue()))
                 : arts::ArtsBarrierReasonAttr{});
  op.erase();
  return success();
}

static LogicalResult inlineSdeSuDistribute(sde::SdeSuDistributeOp op) {
  if (op.getBody().empty() || op.getBody().front().getNumArguments() != 0)
    return op.emitOpError()
           << "has non-empty region arguments during SDE-to-ARTS cleanup";

  Block &body = op.getBody().front();
  for (auto iterate : body.getOps<sde::SdeSuIterateOp>())
    if (!iterate.getDistributionKindAttr())
      iterate.setDistributionKindAttr(op.getKindAttr());
  if (!body.empty())
    if (auto yield = dyn_cast<sde::SdeYieldOp>(&body.back()))
      yield.erase();

  op->getBlock()->getOperations().splice(Block::iterator(op.getOperation()),
                                         body.getOperations());
  op.erase();
  return success();
}

static LogicalResult eraseConsumedSdeControlToken(sde::SdeControlTokenOp op) {
  if (!op.getToken().use_empty())
    return op.emitOpError()
           << "survived SDE-to-ARTS boundary conversion with live users";
  op.erase();
  return success();
}

static LogicalResult inlineSdeCuRegion(sde::SdeCuRegionOp op) {
  if (op.getBody().empty())
    return op.emitOpError() << "has no body during SDE-to-ARTS cleanup";
  Block &body = op.getBody().front();
  if (body.getNumArguments() != op.getIterArgs().size())
    return op.emitOpError()
           << "has mismatched body arguments during SDE-to-ARTS cleanup";
  for (auto [arg, iterArg] : llvm::zip(body.getArguments(), op.getIterArgs()))
    arg.replaceAllUsesWith(iterArg);

  SmallVector<Value> yielded;
  if (!body.empty())
    if (auto yield = dyn_cast<sde::SdeYieldOp>(&body.back())) {
      yielded.append(yield.getValues().begin(), yield.getValues().end());
      yield.erase();
    }
  if (yielded.size() != op.getNumResults())
    return op.emitOpError()
           << "has mismatched yield/result count during SDE-to-ARTS cleanup";

  OpBuilder builder(op);
  builder.setInsertionPoint(op);
  op->getBlock()->getOperations().splice(Block::iterator(op.getOperation()),
                                         body.getOperations());
  for (auto [result, replacement] : llvm::zip(op.getResults(), yielded))
    result.replaceAllUsesWith(replacement);
  op.erase();
  return success();
}

static void eraseDbBackedMemrefDeallocs(ModuleOp module) {
  SmallVector<memref::DeallocOp> deallocs;
  module.walk([&](memref::DeallocOp dealloc) {
    if (arts::DbUtils::getUnderlyingDbAlloc(dealloc.getMemref()))
      deallocs.push_back(dealloc);
  });
  for (memref::DeallocOp dealloc : deallocs)
    dealloc.erase();
}

static LogicalResult rejectUnsupportedSdeCarriers(ModuleOp module) {
  bool found = false;
  module.walk([&](Operation *op) {
    if (isa<sde::SdeCuWorkOp>(op)) {
      op->emitError()
          << "direct SDE-to-ARTS lowering for this SDE carrier is not yet "
             "implemented; add a real ARTS materialization";
      found = true;
    }
  });
  return failure(found);
}

static LogicalResult rejectResidualSdeOps(ModuleOp module) {
  bool found = false;
  module.walk([&](Operation *op) {
    if (!op->getDialect() || op->getDialect()->getNamespace() != "sde")
      return;
    op->emitError() << "SDE operation '" << op->getName()
                    << "' remains after direct SDE-to-ARTS conversion";
    found = true;
  });
  return failure(found);
}

struct SdeStorageToArtsDbPass
    : public arts::impl::SdeStorageToArtsDbBase<SdeStorageToArtsDbPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    DenseMap<Value, HaloRedistPlan> haloPlansByMu;
    SmallVector<sde::SdeRedistOp> redists;
    if (failed(validateAndCollectHaloRedists(module, haloPlansByMu, redists))) {
      signalPassFailure();
      return;
    }

    if (failed(rejectUnsupportedSdeCarriers(module))) {
      signalPassFailure();
      return;
    }

    DenseMap<Value, SmallVector<sde::SdeMuAccessWindowOp, 4>> windowsByMu;
    module.walk([&](sde::SdeMuAccessWindowOp window) {
      windowsByMu[window.getMu()].push_back(window);
    });

    SmallVector<sde::SdeMuDataOp> muDatas;
    module.walk([&](sde::SdeMuDataOp op) { muDatas.push_back(op); });
    for (sde::SdeMuDataOp op : muDatas)
      if (failed(lowerMuData(op))) {
        signalPassFailure();
        return;
      }

    SmallVector<sde::SdeMuAllocOp> muAllocs;
    module.walk([&](sde::SdeMuAllocOp op) { muAllocs.push_back(op); });
    for (sde::SdeMuAllocOp op : muAllocs) {
      auto it = windowsByMu.find(op.getMemref());
      ArrayRef<sde::SdeMuAccessWindowOp> windows =
          it == windowsByMu.end()
              ? ArrayRef<sde::SdeMuAccessWindowOp>()
              : ArrayRef<sde::SdeMuAccessWindowOp>(it->second);
      ArrayAttr haloShape;
      auto haloIt = haloPlansByMu.find(op.getMemref());
      if (haloIt != haloPlansByMu.end())
        haloShape = haloIt->second.haloShape;
      if (failed(lowerMuAlloc(op, windows, haloShape))) {
        signalPassFailure();
        return;
      }
    }

    if (failed(materializeTaskDepMemrefStorage(module))) {
      signalPassFailure();
      return;
    }

    SmallVector<sde::SdeMuAccessWindowOp> residualWindows;
    module.walk(
        [&](sde::SdeMuAccessWindowOp op) { residualWindows.push_back(op); });
    for (sde::SdeMuAccessWindowOp window : residualWindows) {
      window.emitOpError()
          << "was not consumed during SDE storage materialization";
      signalPassFailure();
      return;
    }

    for (sde::SdeRedistOp redist : redists)
      if (redist && redist->getBlock())
        redist.erase();
  }
};

struct SdeAccessesToArtsDepsPass
    : public arts::impl::SdeAccessesToArtsDepsBase<SdeAccessesToArtsDepsPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    SmallVector<sde::SdeSuDistributeOp> distributes;
    module.walk([&](sde::SdeSuDistributeOp op) { distributes.push_back(op); });
    for (sde::SdeSuDistributeOp op : distributes)
      if (failed(inlineSdeSuDistribute(op))) {
        signalPassFailure();
        return;
      }

    SmallVector<sde::SdeCuRegionOp> standaloneRegions;
    module.walk([&](sde::SdeCuRegionOp op) {
      if (!op->getParentOfType<sde::SdeSuIterateOp>())
        standaloneRegions.push_back(op);
    });
    for (sde::SdeCuRegionOp op : standaloneRegions)
      if (op && op->getBlock() && !containsDbAccessPlan(op))
        if (failed(inlineSdeCuRegion(op))) {
          signalPassFailure();
          return;
        }

    standaloneRegions.clear();
    module.walk([&](sde::SdeCuRegionOp op) {
      if (!op->getParentOfType<sde::SdeSuIterateOp>())
        standaloneRegions.push_back(op);
    });
    for (sde::SdeCuRegionOp op : standaloneRegions)
      if (failed(materializeStandaloneCuAccesses(op))) {
        signalPassFailure();
        return;
      }
    standaloneRegions.clear();
    module.walk([&](sde::SdeCuRegionOp op) {
      if (!op->getParentOfType<sde::SdeSuIterateOp>())
        standaloneRegions.push_back(op);
    });
    for (sde::SdeCuRegionOp op : standaloneRegions)
      if (failed(inlineSdeCuRegion(op))) {
        signalPassFailure();
        return;
      }

    SmallVector<sde::SdeSuIterateOp> iterates;
    module.walk([&](sde::SdeSuIterateOp op) { iterates.push_back(op); });
    DenseSet<Operation *> consumedCuLevelAccessPlans;
    for (sde::SdeSuIterateOp op : iterates)
      if (failed(convertSuIterate(op, consumedCuLevelAccessPlans))) {
        signalPassFailure();
        return;
      }
    for (Operation *op : consumedCuLevelAccessPlans)
      if (op && op->getBlock())
        op->erase();

    SmallVector<sde::SdeCuTaskOp> tasks;
    module.walk([&](sde::SdeCuTaskOp op) { tasks.push_back(op); });
    for (sde::SdeCuTaskOp op : tasks)
      if (failed(convertCuTask(op))) {
        signalPassFailure();
        return;
      }

    bool foundAccessPlan = false;
    module.walk([&](arts::DbAccessPlanOp op) {
      op.emitOpError()
          << "was not consumed during SDE access materialization into ARTS "
             "dependencies";
      foundAccessPlan = true;
    });
    if (foundAccessPlan) {
      signalPassFailure();
      return;
    }
  }
};

struct FinalizeSdeToArtsPass
    : public arts::impl::FinalizeSdeToArtsBase<FinalizeSdeToArtsPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    SmallVector<sde::SdeResourceQueryOp> resourceQueries;
    module.walk(
        [&](sde::SdeResourceQueryOp op) { resourceQueries.push_back(op); });
    for (sde::SdeResourceQueryOp op : resourceQueries)
      if (failed(lowerSdeResourceQuery(op))) {
        signalPassFailure();
        return;
      }

    SmallVector<sde::SdeSuBarrierOp> controlBarriers;
    module.walk([&](sde::SdeSuBarrierOp op) { controlBarriers.push_back(op); });
    for (sde::SdeSuBarrierOp op : controlBarriers)
      if (failed(lowerSdeControlBarrier(op))) {
        signalPassFailure();
        return;
      }

    SmallVector<sde::SdeSuDistributeOp> distributes;
    module.walk([&](sde::SdeSuDistributeOp op) { distributes.push_back(op); });
    for (sde::SdeSuDistributeOp op : distributes)
      if (failed(inlineSdeSuDistribute(op))) {
        signalPassFailure();
        return;
      }

    SmallVector<sde::SdeControlTokenOp> controlTokens;
    module.walk(
        [&](sde::SdeControlTokenOp op) { controlTokens.push_back(op); });
    for (sde::SdeControlTokenOp op : controlTokens)
      if (failed(eraseConsumedSdeControlToken(op))) {
        signalPassFailure();
        return;
      }

    SmallVector<sde::SdeMuTokenOp> tokens;
    module.walk([&](sde::SdeMuTokenOp op) { tokens.push_back(op); });
    for (sde::SdeMuTokenOp token : tokens) {
      if (!token.getToken().use_empty()) {
        token.emitOpError()
            << "survived direct SDE-to-ARTS lowering with live users";
        signalPassFailure();
        return;
      }
      token.erase();
    }

    SmallVector<sde::SdeCuRegionOp> regions;
    module.walk([&](sde::SdeCuRegionOp op) { regions.push_back(op); });
    for (sde::SdeCuRegionOp region : regions)
      if (failed(inlineSdeCuRegion(region))) {
        signalPassFailure();
        return;
      }

    eraseDbBackedMemrefDeallocs(module);

    bool foundAccessPlan = false;
    module.walk([&](arts::DbAccessPlanOp op) {
      op.emitOpError()
          << "remains after SDE access materialization into ARTS dependencies";
      foundAccessPlan = true;
    });
    if (foundAccessPlan) {
      signalPassFailure();
      return;
    }

    if (failed(rejectResidualSdeOps(module))) {
      signalPassFailure();
      return;
    }
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::arts::createSdeStorageToArtsDbPass() {
  return std::make_unique<SdeStorageToArtsDbPass>();
}

std::unique_ptr<Pass> mlir::carts::arts::createSdeAccessesToArtsDepsPass() {
  return std::make_unique<SdeAccessesToArtsDepsPass>();
}

std::unique_ptr<Pass> mlir::carts::arts::createFinalizeSdeToArtsPass() {
  return std::make_unique<FinalizeSdeToArtsPass>();
}
