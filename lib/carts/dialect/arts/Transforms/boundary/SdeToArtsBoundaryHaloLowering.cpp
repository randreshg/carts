///==========================================================================///
/// File: SdeToArtsBoundaryHaloLowering.cpp
/// Compact halo helper queries for SDE-to-ARTS boundary lowering.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHaloLowering.h"

#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/STLExtras.h"
#include <functional>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {

FailureOr<SmallVector<int64_t, 4>> getOwnerHaloRadii(ArrayAttr haloShape,
                                                     unsigned ownerDimCount,
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

bool hasGroupedOwnerBlocks(ArrayRef<int64_t> groupBlockCounts) {
  return llvm::any_of(groupBlockCounts,
                      [](int64_t count) { return count > 1; });
}

FailureOr<int64_t> requireStaticPositiveIndex(Value value, Operation *context,
                                              StringRef name) {
  std::optional<int64_t> folded = ValueAnalysis::tryFoldConstantIndex(
      ValueAnalysis::stripNumericCasts(value));
  if (!folded || *folded <= 0) {
    context->emitError() << "requires static positive " << name
                         << " for ARTS compact halo face realization";
    return failure();
  }
  return *folded;
}

void attachStencilHaloAcquireFacts(sde::SdeSuIterateOp source,
                                   arts::DbAcquireOp acquire,
                                   ArrayRef<int64_t> minOffsets,
                                   ArrayRef<int64_t> maxOffsets) {
  acquire.setDepPatternAttr(
      ArtsDepPatternAttr::get(source.getContext(), ArtsDepPattern::stencil));
  acquire.setDistributionPatternAttr(EdtDistributionPatternAttr::get(
      source.getContext(), EdtDistributionPattern::stencil));
  OpBuilder attrBuilder(source.getContext());
  acquire->setAttr(acquire.getStencilMinOffsetsAttrName(),
                   attrBuilder.getI64ArrayAttr(minOffsets));
  acquire->setAttr(acquire.getStencilMaxOffsetsAttrName(),
                   attrBuilder.getI64ArrayAttr(maxOffsets));
  if (auto ownerDims = source.getOwnerDimsAttr())
    acquire->setAttr(acquire.getStencilOwnerDimsAttrName(), ownerDims);
  if (auto spatialDims = source.getSpatialDimsAttr())
    acquire->setAttr(acquire.getStencilSpatialDimsAttrName(), spatialDims);
  acquire->setAttr(acquire.getStencilSupportedBlockHaloAttrName(),
                   UnitAttr::get(source.getContext()));
}

void enumerateUnitHaloSourceOffsets(
    unsigned rank, SmallVectorImpl<SmallVector<int64_t, 4>> &offsets) {
  SmallVector<int64_t, 4> current(rank, 0);
  std::function<void(unsigned, bool)> visit = [&](unsigned dim, bool nonzero) {
    if (dim == rank) {
      if (nonzero)
        offsets.push_back(current);
      return;
    }
    for (int64_t value : {-1, 0, 1}) {
      current[dim] = value;
      visit(dim + 1, nonzero || value != 0);
    }
  };
  visit(/*dim=*/0, /*nonzero=*/false);
}

SmallVector<Value, 4>
buildRankExpandedElementIndices(OpBuilder &builder, Location loc,
                                ArrayRef<Value> elementIndices) {
  SmallVector<Value, 4> indices;
  indices.reserve(elementIndices.size() * 2);
  for (unsigned idx = 0; idx < elementIndices.size(); ++idx)
    indices.push_back(createZeroIndex(builder, loc));
  indices.append(elementIndices.begin(), elementIndices.end());
  return indices;
}

void emitCompactHaloCopy(OpBuilder &builder, Location loc,
                         ArrayRef<int64_t> sourceOffsets,
                         ArrayRef<Value> elementExtents, Value sourcePayload,
                         Value compactPayload) {
  unsigned rank = sourceOffsets.size();
  SmallVector<Value, 4> loopIvs(rank);

  std::function<void(unsigned)> emitAtDim = [&](unsigned dim) {
    if (dim == rank) {
      SmallVector<Value, 4> sourceElementIndices;
      SmallVector<Value, 4> compactElementIndices;
      sourceElementIndices.reserve(rank);
      compactElementIndices.reserve(rank);
      for (unsigned slot = 0; slot < rank; ++slot) {
        if (sourceOffsets[slot] == 0) {
          sourceElementIndices.push_back(loopIvs[slot]);
          compactElementIndices.push_back(loopIvs[slot]);
          continue;
        }
        Value compactCoord = createZeroIndex(builder, loc);
        compactElementIndices.push_back(compactCoord);
        if (sourceOffsets[slot] > 0) {
          sourceElementIndices.push_back(createZeroIndex(builder, loc));
          continue;
        }
        Value last = arith::SubIOp::create(builder, loc, elementExtents[slot],
                                           createOneIndex(builder, loc));
        sourceElementIndices.push_back(last);
      }
      Value value = memref::LoadOp::create(
          builder, loc, sourcePayload,
          buildRankExpandedElementIndices(builder, loc, sourceElementIndices));
      memref::StoreOp::create(
          builder, loc, value, compactPayload,
          buildRankExpandedElementIndices(builder, loc, compactElementIndices));
      return;
    }

    if (sourceOffsets[dim] != 0) {
      emitAtDim(dim + 1);
      return;
    }

    auto loop =
        scf::ForOp::create(builder, loc, createZeroIndex(builder, loc),
                           elementExtents[dim], createOneIndex(builder, loc));
    builder.setInsertionPointToStart(loop.getBody());
    loopIvs[dim] = loop.getInductionVar();
    emitAtDim(dim + 1);
  };

  emitAtDim(/*dim=*/0);
}

} // namespace mlir::carts::arts::boundary
