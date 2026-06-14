///==========================================================================///
/// File: SdeToArtsBoundaryStorage.cpp
/// SDE→ARTS boundary lowering unit.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHelpers.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCommon.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryTypes.h"
#include "carts/dialect/arts/Utils/DbBackedMemrefUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/arts/Utils/MovementLoweringUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include <algorithm>
#include <functional>
#include <limits>


using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {
std::optional<CommittedPhysicalLayout>
readPhysicalLayoutFromExpandedType(MemRefType memrefType) {
  std::optional<sde::RecoveredMuPhysicalLayout> recovered =
      sde::recoverMuPhysicalLayoutFromExpandedType(memrefType);
  if (!recovered)
    return std::nullopt;
  CommittedPhysicalLayout layout;
  layout.ownerDims.reserve(recovered->ownerDims.size());
  for (unsigned dim : recovered->ownerDims)
    layout.ownerDims.push_back(static_cast<int64_t>(dim));
  layout.blockShape.assign(recovered->physicalBlockShape.begin(),
                           recovered->physicalBlockShape.end());
  return layout;
}

LogicalResult
requireCompatibleWindows(sde::SdeMuAllocOp op,
                         ArrayAttr &ownerDims, ArrayAttr &blockShape) {
  auto memrefType = dyn_cast<MemRefType>(op.getMemref().getType());
  if (!memrefType)
    return op.emitOpError() << "requires a memref type for ARTS DB lowering";

  std::optional<sde::MuAccessWindowGeometry> firstGeom =
      sde::deriveMuAccessWindowGeometry(op.getMemref());
  if (!firstGeom)
    return op.emitOpError()
           << "could not derive access-window geometry from the rank-expanded "
              "MU type";

  unsigned ownerDimCount = static_cast<unsigned>(firstGeom->ownerDimCount);
  if (ownerDimCount == 0) {
    ownerDims = ownerDimsForExpandedWindow(op.getContext(), ownerDimCount);
    blockShape = ArrayAttr();
    return success();
  }

  ownerDims = ownerDimsForExpandedWindow(op.getContext(), ownerDimCount);
  const bool preferExpandedTypeLayout =
      memrefType.getRank() >
      static_cast<int64_t>(firstGeom->validExtents.size());
  std::optional<CommittedPhysicalLayout> expandedTypeLayout;
  if (preferExpandedTypeLayout)
    expandedTypeLayout = readPhysicalLayoutFromExpandedType(memrefType);
  if (expandedTypeLayout) {
    if (expandedTypeLayout->ownerDims.size() != ownerDimCount ||
        expandedTypeLayout->blockShape.size() + ownerDimCount !=
            memrefType.getRank())
      return op.emitOpError()
             << "has access-window owner rank incompatible with the "
                "rank-expanded MU type";
    SmallVector<int64_t, 4> paddedBlockShape(ownerDimCount, 1);
    paddedBlockShape.append(expandedTypeLayout->blockShape.begin(),
                            expandedTypeLayout->blockShape.end());
    blockShape = Builder(op.getContext()).getI64ArrayAttr(paddedBlockShape);
  } else {
    FailureOr<ArrayAttr> maybeBlockShape = blockShapeForExpandedWindow(
        *firstGeom, memrefType, op.getContext());
    if (failed(maybeBlockShape))
      return op.emitOpError()
             << "has access-window shape incompatible with the rank-expanded "
                "MU";
    blockShape = *maybeBlockShape;
  }
  return success();
}

void eraseDeallocUsers(Value memref) {
  SmallVector<memref::DeallocOp> deallocs;
  for (Operation *user : llvm::make_early_inc_range(memref.getUsers())) {
    auto dealloc = dyn_cast<memref::DeallocOp>(user);
    if (dealloc && dealloc.getMemref() == memref)
      deallocs.push_back(dealloc);
  }
  for (memref::DeallocOp dealloc : deallocs)
    dealloc.erase();
}

std::optional<SmallVector<Value>>
getDynamicSizesForTaskDepRoot(Operation *root) {
  if (auto alloc = dyn_cast_or_null<memref::AllocOp>(root))
    return SmallVector<Value>(alloc.getDynamicSizes().begin(),
                              alloc.getDynamicSizes().end());
  if (auto alloca = dyn_cast_or_null<memref::AllocaOp>(root))
    return SmallVector<Value>(alloca.getDynamicSizes().begin(),
                              alloca.getDynamicSizes().end());
  return std::nullopt;
}

std::optional<SmallVector<Value>>
getDynamicSizesForTaskDepRoot(Value root) {
  if (!root)
    return std::nullopt;
  return getDynamicSizesForTaskDepRoot(root.getDefiningOp());
}

LogicalResult exposeTaskDepMemrefRoots(ModuleOp module) {
  SetVector<Operation *> regions;
  bool foundError = false;
  module.walk([&](sde::SdeMuDepOp dep) {
    Value root = ValueAnalysis::stripMemrefViewOps(dep.getSource());
    auto result = dyn_cast<OpResult>(root);
    if (!result)
      return;
    auto cu = dyn_cast<sde::SdeCuRegionOp>(result.getOwner());
    if (!cu)
      return;
    if (containsDbAccessWindow(cu)) {
      dep.emitOpError()
          << "uses storage yielded from a CU region that already contains ARTS "
             "access windows; SDE must split the storage-producing region "
             "before ARTS storage realization";
      foundError = true;
      return;
    }
    regions.insert(cu.getOperation());
  });
  if (foundError)
    return failure();

  for (Operation *op : regions) {
    if (!op || !op->getBlock())
      continue;
    if (failed(inlineSdeCuRegion(cast<sde::SdeCuRegionOp>(op))))
      return failure();
  }
  return success();
}

LogicalResult realizeTaskDepMemrefStorage(ModuleOp module) {
  if (failed(exposeTaskDepMemrefRoots(module)))
    return failure();

  SetVector<Value> roots;
  bool foundError = false;
  module.walk([&](sde::SdeMuDepOp dep) {
    Value root = ValueAnalysis::stripMemrefViewOps(dep.getSource());
    if (!root) {
      dep.emitOpError() << "has no traceable memref root for ARTS storage "
                           "realization";
      foundError = true;
      return;
    }
    if (arts::DbUtils::getUnderlyingDbAlloc(root))
      return;
    auto memrefType = dyn_cast<MemRefType>(root.getType());
    if (!memrefType) {
      dep.emitOpError() << "requires a memref source for ARTS storage "
                           "realization";
      foundError = true;
      return;
    }
    if (isa<MemRefType>(memrefType.getElementType())) {
      dep.emitOpError() << "requires normalized memref storage; nested memref "
                           "element types must be raised before SDE-to-ARTS";
      foundError = true;
      return;
    }
    if (!getDynamicSizesForTaskDepRoot(root)) {
      dep.emitOpError()
          << "references a memref root that direct SDE-to-ARTS task lowering "
             "cannot realize as an ARTS DB; SDE must expose a "
             "representable memref allocation or fail before this boundary";
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
        getDynamicSizesForTaskDepRoot(root);
    if (!dynamicSizes)
      return failure();

    OpBuilder builder(rootOp);
    builder.setInsertionPointAfter(rootOp);
    Value replacement;
    if (failed(arts::createCoarseDbBackedMemref(
            builder, rootOp->getLoc(), memrefType, *dynamicSizes, replacement)))
      return rootOp->emitError()
             << "could not realize task dependency memref as an ARTS DB";
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

LogicalResult lowerMuData(sde::SdeMuDataOp op) {
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

static LogicalResult createDbBackedReplacement(
    OpBuilder &builder, sde::SdeMuAllocOp op, MemRefType memrefType,
    ArrayAttr ownerDims, ArrayAttr blockShape,
    const sde::MuAccessWindowGeometry &geom, Value &replacement) {
  if (geom.ownerDimCount == 0)
    return arts::createCoarseDbBackedMemref(builder, op.getLoc(), memrefType,
                                          op.getDynamicSizes(), replacement);
  return arts::createBlockDbBackedMemref(builder, op.getLoc(), memrefType,
                                         op.getDynamicSizes(), ownerDims,
                                         blockShape, replacement);
}

static ArrayAttr getCommittedHaloShapeForCu(sde::SdeCuRegionOp cu, Value mu,
                                            ArrayAttr committedHaloShape) {
  if (!cu || !committedHaloShape)
    return {};
  auto parentSu = cu->getParentOfType<sde::SdeSuIterateOp>();
  if (!parentSu)
    return {};
  for (Operation *op = parentSu->getPrevNode(); op; op = op->getPrevNode()) {
    if (auto halo = dyn_cast<sde::SdeSuHaloOp>(op)) {
      if (halo.getMu() == mu)
        return halo.getHaloShape();
    }
  }
  return {};
}

static LogicalResult emitDbAccessWindowFromSpec(
    OpBuilder &builder, Location loc, Value replacement,
    sde::RaisedWindowSpec spec, ArrayAttr committedHaloShape,
    MLIRContext *ctx) {
  std::optional<sde::MuAccessWindowGeometry> geom =
      sde::deriveMuAccessWindowGeometry(spec.mu);
  if (!geom)
    return mlir::emitError(spec.cu.getLoc())
           << "could not derive access-window geometry from the rank-expanded "
              "MU type";

  ArrayAttr haloShape =
      getCommittedHaloShapeForCu(spec.cu, spec.mu, committedHaloShape);
  if (spec.mode == sde::SdeAccessMode::write)
    haloShape = {};
  else if (spec.mode == sde::SdeAccessMode::readwrite && haloShape) {
    return mlir::emitError(spec.cu.getLoc())
           << "commits a writable halo dependency; SDE must split the read "
              "halo and write access before ARTS realization";
  }

  FailureOr<ArtsMode> mode = convertAccessMode(spec.mode, spec.cu.getOperation());
  if (failed(mode))
    return failure();

  IntegerAttr arrayIdAttr;
  if (spec.arrayId)
    arrayIdAttr = builder.getI64IntegerAttr(*spec.arrayId);

  arts::DbAccessWindowOp::create(
      builder, loc, replacement, ArtsModeAttr::get(ctx, *mode), arrayIdAttr,
      builder.getI64IntegerAttr(geom->ownerDimCount),
      builder.getI64ArrayAttr(geom->blockLo),
      builder.getI64ArrayAttr(geom->blockHi),
      builder.getI64ArrayAttr(geom->validExtents), haloShape);
  return success();
}

LogicalResult
lowerMuAlloc(sde::SdeMuAllocOp op, ArrayAttr committedHaloShape) {
  auto memrefType = dyn_cast<MemRefType>(op.getMemref().getType());
  if (!memrefType)
    return op.emitOpError()
           << "expects a memref result before SDE-to-ARTS conversion";

  llvm::SmallVector<sde::RaisedWindowSpec, 4> derivedSpecs =
      sde::queryAccessWindows(op);
  const bool hasDerivedSpecs = !derivedSpecs.empty();

  ArrayAttr ownerDims;
  ArrayAttr blockShape;
  if (hasDerivedSpecs) {
    if (failed(requireCompatibleWindows(op, ownerDims, blockShape)))
      return failure();
  }

  OpBuilder builder(op);
  Value replacement;
  if (hasDerivedSpecs) {
    std::optional<sde::MuAccessWindowGeometry> geom =
        sde::deriveMuAccessWindowGeometry(op.getMemref());
    if (!geom)
      return op.emitOpError()
             << "could not derive access-window geometry from the rank-expanded "
                "MU type";
    if (failed(createDbBackedReplacement(builder, op, memrefType, ownerDims,
                                         blockShape, *geom, replacement)))
      return op.emitOpError()
             << "could not realize query-derived SDE layout as ARTS DB";
  } else if (committedHaloShape) {
    return op.emitOpError()
           << "commits halo movement but has no query-derived SDE access-window "
              "layout for ARTS DB realization";
  } else if (failed(arts::createCoarseDbBackedMemref(
                 builder, op.getLoc(), memrefType, op.getDynamicSizes(),
                 replacement))) {
    return op.emitOpError()
           << "could not realize unpartitioned SDE MU as ARTS DB";
  }

  if (replacement.getType() != memrefType)
    replacement =
        memref::CastOp::create(builder, op.getLoc(), memrefType, replacement);

  if (hasDerivedSpecs) {
    for (const sde::RaisedWindowSpec &spec : derivedSpecs) {
      sde::SdeCuRegionOp cu = spec.cu;
      if (cu.getBody().empty())
        return op.emitOpError()
               << "query-derived access window targets an empty CU region";
      OpBuilder cuBuilder(cu);
      cuBuilder.setInsertionPointToStart(&cu.getBody().front());
      if (failed(emitDbAccessWindowFromSpec(cuBuilder, cu.getLoc(), replacement,
                                            spec, committedHaloShape,
                                            op.getContext())))
        return failure();
    }
  }

  eraseDeallocUsers(op.getMemref());
  op.getMemref().replaceAllUsesWith(replacement);
  op.erase();
  return success();
}
} // namespace mlir::carts::arts::boundary
