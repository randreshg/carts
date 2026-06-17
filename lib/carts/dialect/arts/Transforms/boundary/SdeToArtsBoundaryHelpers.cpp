///==========================================================================///
/// File: SdeToArtsBoundaryHelpers.cpp
/// SDE→ARTS boundary lowering unit.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHelpers.h"
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
bool isScalarParamType(Type type) {
  return type.isIndex() || isa<IntegerType, FloatType>(type);
}

bool isConstantLikeValue(Value value) {
  Operation *def = value ? value.getDefiningOp() : nullptr;
  return def && def->hasTrait<OpTrait::ConstantLike>();
}

bool isDefinedInside(Value value, Operation *scope) {
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

LogicalResult collectExternalScalarCaptures(sde::SdeSuIterateOp source,
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

LogicalResult collectExternalScalarCaptures(sde::SdeCuTaskOp source,
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

LogicalResult collectExternalScalarCaptures(sde::SdeCuRegionOp source,
                                            SetVector<Value> &captures) {
  auto addIfExternalScalar = [&](Value value) {
    if (!value || !isScalarParamType(value.getType()) ||
        isConstantLikeValue(value))
      return;
    if (!isDefinedInside(value, source.getOperation()))
      captures.insert(value);
  };

  source.getBody().walk([&](Operation *op) {
    if (isa<arts::DbAccessWindowOp, sde::SdeMuDepOp>(op))
      return;
    for (Value operand : op->getOperands())
      addIfExternalScalar(operand);
  });
  return success();
}

Value remapOrSelf(IRMapping &mapper, Value value) {
  if (Value mapped = mapper.lookupOrNull(value))
    return mapped;
  return value;
}

LogicalResult translateSdeAtomicsToArts(Region &region) {
  SmallVector<sde::SdeCuAtomicOp> atomics;
  region.walk([&](sde::SdeCuAtomicOp op) { atomics.push_back(op); });
  for (sde::SdeCuAtomicOp atomic : atomics) {
    if (atomic.getReductionKind() != sde::SdeReductionKind::add)
      return atomic.emitOpError()
             << "cannot realize non-add SDE atomic at the ARTS boundary";
    OpBuilder builder(atomic);
    arts::AtomicAddOp::create(builder, atomic.getLoc(), atomic.getAddr(),
                              atomic.getValue());
    atomic.erase();
  }
  return success();
}

bool isStackScratchMemref(Value memref) {
  Value root = ValueAnalysis::stripMemrefViewOps(memref);
  return root && isa<memref::AllocaOp>(root.getDefiningOp());
}

FailureOr<ArtsMode> convertAccessMode(sde::SdeAccessMode mode,
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

FailureOr<ArtsDepPattern> convertPattern(sde::SdePattern pattern,
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

FailureOr<EdtDistributionKind>
convertDistributionKind(sde::SdeDistributionKind kind, Operation *context) {
  switch (kind) {
  case sde::SdeDistributionKind::owner_compute:
  case sde::SdeDistributionKind::blocked:
    return EdtDistributionKind::block;
  }
  context->emitError()
      << "has unsupported SDE distribution kind at the SDE-to-ARTS boundary";
  return failure();
}

bool hasCommittedPartialReductionFacts(sde::SdeSuIterateOp source) {
  return source.getPartialReductionAttr() ||
         source.getPartialReductionDimsAttr() ||
         source.getPartialReductionOwnerDimsAttr();
}

LogicalResult attachCommittedSdeFacts(sde::SdeSuIterateOp source,
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
  if (auto distribute = source->getParentOfType<sde::SdeSuDistributeOp>()) {
    FailureOr<EdtDistributionKind> converted =
        convertDistributionKind(distribute.getKind(), source.getOperation());
    if (failed(converted))
      return failure();
    arts::setEdtDistributionKind(taskOp, *converted);
  }
  if (hasCommittedPartialReductionFacts(source))
    task.setPartialReductionAttr(UnitAttr::get(ctx));
  if (auto dims = source.getPartialReductionDimsAttr())
    task.setPartialReductionDimsAttr(dims);
  if (auto ownerDims = source.getPartialReductionOwnerDimsAttr())
    task.setPartialReductionOwnerDimsAttr(ownerDims);
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

LogicalResult attachUnpartitionedSdeFacts(sde::SdeSuIterateOp source,
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

ArrayAttr ownerDimsForExpandedWindow(MLIRContext *ctx, unsigned ownerDimCount) {
  SmallVector<int64_t, 4> ownerDims;
  ownerDims.reserve(ownerDimCount);
  for (unsigned idx = 0; idx < ownerDimCount; ++idx)
    ownerDims.push_back(idx);
  return Builder(ctx).getI64ArrayAttr(ownerDims);
}

FailureOr<ArrayAttr>
blockShapeForExpandedWindow(const sde::MuAccessWindowGeometry &geom,
                            MemRefType memrefType, MLIRContext *ctx) {
  unsigned ownerDimCount = static_cast<unsigned>(geom.ownerDimCount);
  if (memrefType.getRank() == static_cast<int64_t>(geom.validExtents.size()))
    return Builder(ctx).getI64ArrayAttr(geom.validExtents);
  if (memrefType.getRank() !=
      static_cast<int64_t>(ownerDimCount + geom.validExtents.size()))
    return failure();

  SmallVector<int64_t, 4> blockShape(ownerDimCount, 1);
  blockShape.append(geom.validExtents.begin(), geom.validExtents.end());
  return Builder(ctx).getI64ArrayAttr(blockShape);
}

std::optional<SmallVector<int64_t, 4>> readCommittedPhysicalOwnerDims(
    sde::SdeSuIterateOp source,
    const std::optional<SmallVector<int64_t, 4>> &arrayOwnerDims) {
  if (arrayOwnerDims && !arrayOwnerDims->empty())
    return arrayOwnerDims;
  if (std::optional<SmallVector<int64_t, 4>> ownerDims =
          readI64ArrayAttr(source.getOwnerDimsAttr()))
    return ownerDims;
  if (std::optional<sde::CommittedSuPhysicalLayout> layout =
          sde::recoverCommittedPhysicalLayout(source))
    return layout->ownerDims;
  return std::nullopt;
}
} // namespace mlir::carts::arts::boundary
