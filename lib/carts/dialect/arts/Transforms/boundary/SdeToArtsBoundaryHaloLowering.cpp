///==========================================================================///
/// File: SdeToArtsBoundaryHaloLowering.cpp
/// Compact halo helper queries for SDE-to-ARTS boundary lowering.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHaloLowering.h"

#include "carts/dialect/arts/Utils/DbBackedMemrefUtils.h"
#include "carts/dialect/sde/Analysis/AffineIndexUtils.h"
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
                                unsigned ownerDimCount,
                                ArrayRef<Value> elementIndices) {
  SmallVector<Value, 4> indices;
  indices.reserve(ownerDimCount + elementIndices.size());
  for (unsigned idx = 0; idx < ownerDimCount; ++idx)
    indices.push_back(createZeroIndex(builder, loc));
  indices.append(elementIndices.begin(), elementIndices.end());
  return indices;
}

FailureOr<SmallVector<Value, 4>>
buildPayloadElementIndices(OpBuilder &builder, Location loc, Value payload,
                           unsigned ownerDimCount,
                           ArrayRef<Value> elementIndices) {
  auto payloadType = dyn_cast<MemRefType>(payload.getType());
  if (!payloadType)
    return emitError(loc) << "compact halo payload is not a memref";
  if (payloadType.getRank() == static_cast<int64_t>(elementIndices.size()))
    return SmallVector<Value, 4>(elementIndices.begin(), elementIndices.end());
  if (payloadType.getRank() ==
      static_cast<int64_t>(ownerDimCount + elementIndices.size()))
    return buildRankExpandedElementIndices(builder, loc, ownerDimCount,
                                           elementIndices);
  return emitError(loc)
         << "compact halo payload rank disagrees with committed SDE "
            "access-window shape";
}

LogicalResult emitCompactHaloCopy(OpBuilder &builder, Location loc,
                                  unsigned ownerDimCount,
                                  ArrayRef<unsigned> ownerPayloadDims,
                                  ArrayRef<int64_t> sourceOffsets,
                                  ArrayRef<Value> elementExtents,
                                  Value sourcePayload, Value compactPayload) {
  unsigned payloadRank = elementExtents.size();
  SmallVector<Value, 4> loopIvs(payloadRank);

  std::function<LogicalResult(unsigned)> emitAtDim = [&](unsigned dim) {
    if (dim == payloadRank) {
      SmallVector<Value, 4> sourceElementIndices;
      SmallVector<Value, 4> compactElementIndices;
      sourceElementIndices.reserve(payloadRank);
      compactElementIndices.reserve(payloadRank);
      for (unsigned slot = 0; slot < payloadRank; ++slot) {
        auto ownerIt = llvm::find(ownerPayloadDims, slot);
        if (ownerIt == ownerPayloadDims.end()) {
          sourceElementIndices.push_back(loopIvs[slot]);
          compactElementIndices.push_back(loopIvs[slot]);
          continue;
        }
        unsigned ownerSlot = static_cast<unsigned>(
            std::distance(ownerPayloadDims.begin(), ownerIt));
        if (sourceOffsets[ownerSlot] == 0) {
          sourceElementIndices.push_back(loopIvs[slot]);
          compactElementIndices.push_back(loopIvs[slot]);
          continue;
        }
        Value compactCoord = createZeroIndex(builder, loc);
        compactElementIndices.push_back(compactCoord);
        if (sourceOffsets[ownerSlot] > 0) {
          sourceElementIndices.push_back(createZeroIndex(builder, loc));
          continue;
        }
        Value last = arith::SubIOp::create(builder, loc, elementExtents[slot],
                                           createOneIndex(builder, loc));
        sourceElementIndices.push_back(last);
      }
      FailureOr<SmallVector<Value, 4>> sourceIndices =
          buildPayloadElementIndices(builder, loc, sourcePayload, ownerDimCount,
                                     sourceElementIndices);
      if (failed(sourceIndices))
        return failure();
      FailureOr<SmallVector<Value, 4>> compactIndices =
          buildPayloadElementIndices(builder, loc, compactPayload,
                                     ownerDimCount, compactElementIndices);
      if (failed(compactIndices))
        return failure();
      Value value =
          memref::LoadOp::create(builder, loc, sourcePayload, *sourceIndices);
      memref::StoreOp::create(builder, loc, value, compactPayload,
                              *compactIndices);
      return success();
    }

    auto ownerIt = llvm::find(ownerPayloadDims, dim);
    if (ownerIt != ownerPayloadDims.end()) {
      unsigned ownerSlot = static_cast<unsigned>(
          std::distance(ownerPayloadDims.begin(), ownerIt));
      if (sourceOffsets[ownerSlot] != 0) {
        return emitAtDim(dim + 1);
      }
    }

    {
      auto loop =
          scf::ForOp::create(builder, loc, createZeroIndex(builder, loc),
                             elementExtents[dim], createOneIndex(builder, loc));
      builder.setInsertionPointToStart(loop.getBody());
      loopIvs[dim] = loop.getInductionVar();
      if (failed(emitAtDim(dim + 1)))
        return failure();
    }
    return success();
  };

  return emitAtDim(/*dim=*/0);
}

static Value getCommonDivRemSource(Value divValue, Value remValue,
                                   Value expectedDivisor) {
  auto div = ValueAnalysis::stripNumericCasts(divValue)
                 .getDefiningOp<arith::DivUIOp>();
  if (!div)
    return {};

  auto same = [](Value lhs, Value rhs) {
    return ValueAnalysis::sameValue(lhs, rhs) ||
           ValueAnalysis::areValuesEquivalent(lhs, rhs);
  };
  if (!same(div.getRhs(), expectedDivisor))
    return {};

  auto sourceFromRem = [&](Value value) -> Value {
    auto rem =
        ValueAnalysis::stripNumericCasts(value).getDefiningOp<arith::RemUIOp>();
    if (!rem || !same(div.getLhs(), rem.getLhs()) ||
        !same(div.getRhs(), rem.getRhs()))
      return {};
    return div.getLhs();
  };

  if (Value source = sourceFromRem(remValue))
    return source;

  auto sourceFromOffset = [&](Value offset) -> Value {
    if (std::optional<int64_t> constant = ValueAnalysis::tryFoldConstantIndex(
            ValueAnalysis::stripNumericCasts(offset)))
      if (*constant == 0)
        return div.getLhs();
    auto sub =
        ValueAnalysis::stripNumericCasts(offset).getDefiningOp<arith::SubIOp>();
    if (!sub || !same(sub.getRhs(), div.getLhs()))
      return {};
    return sub.getLhs();
  };

  auto add =
      ValueAnalysis::stripNumericCasts(remValue).getDefiningOp<arith::AddIOp>();
  if (!add)
    return {};
  if (sourceFromRem(add.getLhs()))
    return sourceFromOffset(add.getRhs());
  if (sourceFromRem(add.getRhs()))
    return sourceFromOffset(add.getLhs());
  return {};
}

FailureOr<SmallVector<Value, 4>>
getCompactHaloOwnerLoopIvs(sde::SdeSuIterateOp source, memref::LoadOp load,
                           ArrayRef<unsigned> ownerLoopDims,
                           llvm::StringRef diagnosticRank) {
  unsigned ownerRank = ownerLoopDims.size();
  auto sourceIvs = source.getLoopInductionVars();
  if (!sourceIvs || ownerRank == 0) {
    load.emitOpError() << "is not nested in the " << diagnosticRank
                       << " compute loops required for ARTS compact unit-halo "
                          "load rewriting";
    return failure();
  }
  for (unsigned loopDim : ownerLoopDims) {
    if (loopDim >= sourceIvs->size()) {
      load.emitOpError() << "is not nested in the " << diagnosticRank
                         << " compute loops required for ARTS compact "
                            "unit-halo load rewriting";
      return failure();
    }
  }

  SmallVector<scf::ForOp, 4> loops;
  for (Operation *parent = load->getParentOp(); parent;
       parent = parent->getParentOp())
    if (auto loop = dyn_cast<scf::ForOp>(parent))
      loops.push_back(loop);

  auto valueLoopDim = [&](Value value) -> std::optional<unsigned> {
    std::optional<unsigned> selected;
    for (auto [dim, sourceIv] : llvm::enumerate(*sourceIvs)) {
      if (!ValueAnalysis::sameValue(value, sourceIv) &&
          !ValueAnalysis::dependsOn(value, sourceIv))
        continue;
      if (selected && *selected != dim)
        return std::nullopt;
      selected = static_cast<unsigned>(dim);
    }
    return selected;
  };
  auto loopDimFor = [&](scf::ForOp loop) -> std::optional<unsigned> {
    if (std::optional<unsigned> ivDim = valueLoopDim(loop.getInductionVar()))
      return ivDim;
    std::optional<unsigned> lowerDim = valueLoopDim(loop.getLowerBound());
    std::optional<unsigned> upperDim = valueLoopDim(loop.getUpperBound());
    if (lowerDim && upperDim && *lowerDim != *upperDim)
      return std::nullopt;
    return lowerDim ? lowerDim : upperDim;
  };

  SmallVector<Value, 4> ownerLoopIvs;
  ownerLoopIvs.reserve(ownerRank);
  for (unsigned loopDim : ownerLoopDims) {
    Value ownerIv;
    for (scf::ForOp loop : loops) {
      std::optional<unsigned> mappedDim = loopDimFor(loop);
      if (mappedDim && *mappedDim == loopDim) {
        ownerIv = loop.getInductionVar();
        break;
      }
    }
    if (!ownerIv)
      ownerIv = (*sourceIvs)[loopDim];
    if (!ownerIv) {
      load.emitOpError() << "is not nested in the " << diagnosticRank
                         << " compute loops required for ARTS compact "
                            "unit-halo load rewriting";
      return failure();
    }
    ownerLoopIvs.push_back(ownerIv);
  }
  return ownerLoopIvs;
}

FailureOr<std::optional<HaloLoadRewrite>>
classify2DUnitHaloLoad(memref::LoadOp load, unsigned haloWorkIndex,
                       const Halo2DTaskWork &work, Value rowIv, Value colIv) {
  OperandRange indices = load.getIndices();
  if (indices.size() != 4) {
    load.emitOpError()
        << "uses a rank shape unsupported by ARTS compact 2D unit-halo "
           "realization";
    return failure();
  }

  Value rowExpr = getCommonDivRemSource(indices[0], indices[2], work.rowExtent);
  Value colExpr = getCommonDivRemSource(indices[1], indices[3], work.colExtent);
  if (!rowExpr || !colExpr) {
    load.emitOpError()
        << "does not expose div/rem rank-expanded indices required for ARTS "
           "compact 2D unit-halo load rewriting";
    return failure();
  }

  std::optional<int64_t> rowOffset =
      sde::tryGetUnitNeighborhoodOffset(rowExpr, rowIv);
  std::optional<int64_t> colOffset =
      sde::tryGetUnitNeighborhoodOffset(colExpr, colIv);
  if (!rowOffset || !colOffset) {
    load.emitOpError()
        << "does not expose affine unit-neighborhood indices required for ARTS "
           "compact 2D unit-halo load rewriting";
    return failure();
  }
  if (*rowOffset == 0 && *colOffset == 0)
    return std::optional<HaloLoadRewrite>{
        HaloLoadRewrite{haloWorkIndex, Halo2DFace::Center}};
  if (*rowOffset != 0 && *colOffset != 0) {
    load.emitOpError()
        << "requires corner halo realization, which ARTS has not "
           "committed; refusing a full-block halo byte-window";
    return failure();
  }
  if (*rowOffset == -1 && *colOffset == 0)
    return std::optional<HaloLoadRewrite>{
        HaloLoadRewrite{haloWorkIndex, Halo2DFace::Top}};
  if (*rowOffset == 1 && *colOffset == 0)
    return std::optional<HaloLoadRewrite>{
        HaloLoadRewrite{haloWorkIndex, Halo2DFace::Bottom}};
  if (*rowOffset == 0 && *colOffset == -1)
    return std::optional<HaloLoadRewrite>{
        HaloLoadRewrite{haloWorkIndex, Halo2DFace::Left}};
  if (*rowOffset == 0 && *colOffset == 1)
    return std::optional<HaloLoadRewrite>{
        HaloLoadRewrite{haloWorkIndex, Halo2DFace::Right}};

  load.emitOpError()
      << "requires non-unit halo realization, which ARTS has not "
         "committed; refusing a full-block halo byte-window";
  return failure();
}

FailureOr<std::optional<HaloNdLoadRewrite>>
classifyNdUnitHaloLoad(memref::LoadOp load, unsigned haloWorkIndex,
                       const HaloNdTaskWork &work,
                       ArrayRef<Value> ownerLoopIvs) {
  unsigned ownerDimCount = ownerLoopIvs.size();
  unsigned payloadRank = work.elementExtents.size();
  OperandRange indices = load.getIndices();
  if (ownerDimCount == 0 || ownerDimCount > payloadRank ||
      indices.size() != ownerDimCount + payloadRank) {
    load.emitOpError()
        << "uses a rank shape unsupported by ARTS compact N-D unit-halo "
           "realization";
    return failure();
  }

  bool hasHaloOffset = false;
  for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
    if (slot >= work.ownerPayloadDims.size() ||
        work.ownerPayloadDims[slot] >= payloadRank) {
      load.emitOpError()
          << "has inconsistent compact N-D owner payload mapping";
      return failure();
    }
    unsigned payloadDim = work.ownerPayloadDims[slot];
    Value expr = getCommonDivRemSource(indices[slot],
                                       indices[ownerDimCount + payloadDim],
                                       work.elementExtents[payloadDim]);
    if (!expr) {
      load.emitOpError()
          << "does not expose div/rem rank-expanded indices required for ARTS "
             "compact N-D unit-halo load rewriting";
      return failure();
    }
    std::optional<int64_t> offset =
        sde::tryGetUnitNeighborhoodOffset(expr, ownerLoopIvs[slot]);
    if (!offset) {
      load.emitOpError()
          << "does not expose affine unit-neighborhood indices required for "
             "ARTS compact N-D unit-halo load rewriting";
      return failure();
    }
    if (*offset < -1 || *offset > 1) {
      load.emitOpError()
          << "requires non-unit halo realization, which ARTS has not "
             "committed; refusing a full-block halo byte-window";
      return failure();
    }
    hasHaloOffset |= *offset != 0;
  }

  if (!hasHaloOffset)
    return std::optional<HaloNdLoadRewrite>{};
  return std::optional<HaloNdLoadRewrite>{HaloNdLoadRewrite{haloWorkIndex}};
}

FailureOr<bool> needsExactNdHaloFor2D(sde::SdeSuIterateOp source,
                                      DirectDepSpec dep, Block *computeBlock,
                                      ArrayRef<unsigned> ownerLoopDims) {
  if (dep.ownerDimCount != 2)
    return false;
  if (ownerLoopDims.size() != 2)
    return source.emitOpError()
           << "commits a halo dependency whose owner rank is not supported by "
              "ARTS compact 2D unit-halo load rewriting";
  bool cleanPayloadShape =
      dep.alloc.getElementSizes().size() == dep.validExtents.size();
  bool expandedPayloadShape = dep.alloc.getElementSizes().size() ==
                              dep.ownerDimCount + dep.validExtents.size();
  if (!cleanPayloadShape && !expandedPayloadShape) {
    source.emitOpError() << "commits a rank shape that ARTS compact 2D "
                            "unit-halo realization cannot represent";
    return failure();
  }
  unsigned rowExtentDim = cleanPayloadShape ? 0 : dep.ownerDimCount;
  unsigned colExtentDim = cleanPayloadShape ? 1 : dep.ownerDimCount + 1;
  bool sawCorner = false;
  bool failedScan = false;
  WalkResult result = computeBlock->walk([&](memref::LoadOp load) {
    if (resolveBoundaryDbAlloc(load.getMemref()) != dep.alloc)
      return WalkResult::advance();
    OperandRange indices = load.getIndices();
    if (indices.size() != 4) {
      load.emitOpError()
          << "uses a rank shape unsupported by ARTS compact 2D unit-halo "
             "realization";
      failedScan = true;
      return WalkResult::interrupt();
    }
    FailureOr<SmallVector<Value, 4>> ownerLoopIvs =
        getCompactHaloOwnerLoopIvs(source, load, ownerLoopDims, "2D");
    if (failed(ownerLoopIvs)) {
      failedScan = true;
      return WalkResult::interrupt();
    }
    Value rowIv = (*ownerLoopIvs)[0];
    Value colIv = (*ownerLoopIvs)[1];
    Value rowExpr = getCommonDivRemSource(
        indices[0], indices[2], dep.alloc.getElementSizes()[rowExtentDim]);
    Value colExpr = getCommonDivRemSource(
        indices[1], indices[3], dep.alloc.getElementSizes()[colExtentDim]);
    if (!rowExpr || !colExpr) {
      load.emitOpError()
          << "does not expose div/rem rank-expanded indices required for ARTS "
             "compact 2D unit-halo load rewriting";
      failedScan = true;
      return WalkResult::interrupt();
    }
    std::optional<int64_t> rowOffset =
        sde::tryGetUnitNeighborhoodOffset(rowExpr, rowIv);
    std::optional<int64_t> colOffset =
        sde::tryGetUnitNeighborhoodOffset(colExpr, colIv);
    if (!rowOffset || !colOffset) {
      load.emitOpError()
          << "does not expose affine unit-neighborhood indices required for "
             "ARTS compact 2D unit-halo load rewriting";
      failedScan = true;
      return WalkResult::interrupt();
    }
    if (*rowOffset != 0 && *colOffset != 0)
      sawCorner = true;
    return WalkResult::advance();
  });
  if (result.wasInterrupted() || failedScan)
    return failure();
  return sawCorner;
}

static SmallVector<Value, 4>
buildRankExpandedIndices(ArrayRef<Value> ownerIndices,
                         ArrayRef<Value> elementIndices) {
  SmallVector<Value, 4> indices;
  indices.reserve(ownerIndices.size() + elementIndices.size());
  indices.append(ownerIndices.begin(), ownerIndices.end());
  indices.append(elementIndices.begin(), elementIndices.end());
  return indices;
}

static Value createGroupEnd(OpBuilder &builder, Location loc, Value blockStart,
                            int64_t groupBlockCount) {
  return arith::AddIOp::create(
      builder, loc, blockStart,
      createConstantIndex(builder, loc, groupBlockCount));
}

static Value isBeforeGroup(OpBuilder &builder, Location loc, Value block,
                           Value groupStart) {
  return arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ult, block,
                               groupStart);
}

static Value isAfterOrAtGroupEnd(OpBuilder &builder, Location loc, Value block,
                                 Value groupEnd) {
  return arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::uge, block,
                               groupEnd);
}

LogicalResult rewriteCloned2DUnitHaloLoads(
    arts::EdtOp task, const DenseMap<Operation *, HaloLoadRewrite> &rewrites,
    ArrayRef<Halo2DTaskWork> haloWorks, ArrayRef<Value> payloads,
    ArrayRef<SmallVector<Value, 4>> depBlockOffsetArgs) {
  OpBuilder builder(task.getContext());

  for (const auto &entry : rewrites) {
    auto load = dyn_cast_or_null<memref::LoadOp>(entry.first);
    if (!load)
      continue;
    const HaloLoadRewrite &rewrite = entry.second;
    if (rewrite.haloWorkIndex >= haloWorks.size())
      return task.emitOpError() << "has stale compact halo load rewrite state";
    const Halo2DTaskWork &work = haloWorks[rewrite.haloWorkIndex];
    auto requirePayload = [&](unsigned index) -> FailureOr<Value> {
      if (index >= payloads.size() || index >= depBlockOffsetArgs.size() ||
          depBlockOffsetArgs[index].size() != 2) {
        task.emitOpError() << "has inconsistent compact halo dependency state";
        return failure();
      }
      return payloads[index];
    };

    FailureOr<Value> centerPayload = requirePayload(work.centerTaskDepIndex);
    if (failed(centerPayload))
      return failure();
    if (work.centerGroupBlockCounts.size() != 2)
      return task.emitOpError()
             << "has inconsistent compact halo center grouping state";
    OperandRange indices = load.getIndices();
    if (indices.size() != 4)
      return load.emitOpError()
             << "has unsupported compact halo rank after cloning";

    Location loc = load.getLoc();
    builder.setInsertionPoint(load);
    Value elemRow = indices[2];
    Value elemCol = indices[3];
    if (rewrite.face == Halo2DFace::Center) {
      load.getMemrefMutable().assign(*centerPayload);
      continue;
    }

    unsigned ownerSlot = 0;
    unsigned faceDepIndex = work.topTaskDepIndex;
    SmallVector<Value, 4> faceIndices;
    Value crossesGroup;
    switch (rewrite.face) {
    case Halo2DFace::Top:
      ownerSlot = 0;
      faceDepIndex = work.topTaskDepIndex;
      crossesGroup =
          isBeforeGroup(builder, loc, indices[0],
                        depBlockOffsetArgs[work.centerTaskDepIndex][0]);
      faceIndices = buildRankExpandedIndices(
          {indices[0], indices[1]}, {createZeroIndex(builder, loc), elemCol});
      break;
    case Halo2DFace::Bottom:
      ownerSlot = 0;
      faceDepIndex = work.bottomTaskDepIndex;
      crossesGroup = isAfterOrAtGroupEnd(
          builder, loc, indices[0],
          createGroupEnd(builder, loc,
                         depBlockOffsetArgs[work.centerTaskDepIndex][0],
                         work.centerGroupBlockCounts[0]));
      faceIndices = buildRankExpandedIndices(
          {indices[0], indices[1]}, {createZeroIndex(builder, loc), elemCol});
      break;
    case Halo2DFace::Left:
      ownerSlot = 1;
      faceDepIndex = work.leftTaskDepIndex;
      crossesGroup =
          isBeforeGroup(builder, loc, indices[1],
                        depBlockOffsetArgs[work.centerTaskDepIndex][1]);
      faceIndices = buildRankExpandedIndices(
          {indices[0], indices[1]}, {elemRow, createZeroIndex(builder, loc)});
      break;
    case Halo2DFace::Right:
      ownerSlot = 1;
      faceDepIndex = work.rightTaskDepIndex;
      crossesGroup = isAfterOrAtGroupEnd(
          builder, loc, indices[1],
          createGroupEnd(builder, loc,
                         depBlockOffsetArgs[work.centerTaskDepIndex][1],
                         work.centerGroupBlockCounts[1]));
      faceIndices = buildRankExpandedIndices(
          {indices[0], indices[1]}, {elemRow, createZeroIndex(builder, loc)});
      break;
    case Halo2DFace::Center:
      llvm_unreachable("center handled above");
    }

    FailureOr<Value> facePayload = requirePayload(faceDepIndex);
    if (failed(facePayload))
      return failure();

    (void)ownerSlot;
    SmallVector<Type, 1> resultTypes{load.getType()};
    auto ifOp = scf::IfOp::create(builder, loc, resultTypes, crossesGroup,
                                  /*withElseRegion=*/true);

    builder.setInsertionPointToStart(&ifOp.getThenRegion().front());
    Value faceValue =
        memref::LoadOp::create(builder, loc, *facePayload, faceIndices);
    scf::YieldOp::create(builder, loc, ValueRange{faceValue});

    builder.setInsertionPointToStart(&ifOp.getElseRegion().front());
    SmallVector<Value, 4> coreIndices =
        buildRankExpandedIndices({indices[0], indices[1]}, {elemRow, elemCol});
    Value coreValue =
        memref::LoadOp::create(builder, loc, *centerPayload, coreIndices);
    scf::YieldOp::create(builder, loc, ValueRange{coreValue});

    load.replaceAllUsesWith(ifOp.getResult(0));
    load.erase();
  }

  return success();
}

LogicalResult rewriteClonedNdUnitHaloLoads(
    arts::EdtOp task, const DenseMap<Operation *, HaloNdLoadRewrite> &rewrites,
    ArrayRef<HaloNdTaskWork> haloWorks, ArrayRef<Value> payloads,
    ArrayRef<SmallVector<Value, 4>> depBlockOffsetArgs) {
  OpBuilder builder(task.getContext());

  auto andValues = [&](Location loc, Value lhs, Value rhs) -> Value {
    return lhs ? arith::AndIOp::create(builder, loc, lhs, rhs).getResult()
               : rhs;
  };

  for (const auto &entry : rewrites) {
    auto load = dyn_cast_or_null<memref::LoadOp>(entry.first);
    if (!load)
      continue;
    const HaloNdLoadRewrite &rewrite = entry.second;
    if (rewrite.haloWorkIndex >= haloWorks.size())
      return task.emitOpError() << "has stale compact halo load rewrite state";
    const HaloNdTaskWork &work = haloWorks[rewrite.haloWorkIndex];
    unsigned ownerDimCount = work.ownerDimCount;
    unsigned payloadRank = work.elementExtents.size();
    OperandRange indices = load.getIndices();
    if (ownerDimCount == 0 || ownerDimCount > payloadRank ||
        indices.size() != ownerDimCount + payloadRank)
      return load.emitOpError() << "has unsupported compact halo rank after "
                                   "cloning";

    auto requirePayload = [&](unsigned index) -> FailureOr<Value> {
      if (index >= payloads.size() || index >= depBlockOffsetArgs.size() ||
          depBlockOffsetArgs[index].size() != ownerDimCount) {
        task.emitOpError() << "has inconsistent compact halo dependency state";
        return failure();
      }
      return payloads[index];
    };

    FailureOr<Value> centerPayload = requirePayload(work.centerTaskDepIndex);
    if (failed(centerPayload))
      return failure();
    if (work.centerGroupBlockCounts.size() != ownerDimCount)
      return task.emitOpError()
             << "has inconsistent compact halo center grouping state";

    Location loc = load.getLoc();
    builder.setInsertionPoint(load);
    SmallVector<Value, 4> ownerIndices;
    ownerIndices.reserve(ownerDimCount);
    for (unsigned slot = 0; slot < ownerDimCount; ++slot)
      ownerIndices.push_back(indices[slot]);
    SmallVector<Value, 4> centerElementIndices;
    centerElementIndices.reserve(payloadRank);
    for (unsigned slot = 0; slot < payloadRank; ++slot)
      centerElementIndices.push_back(indices[ownerDimCount + slot]);
    Value replacement = memref::LoadOp::create(
        builder, loc, *centerPayload,
        buildRankExpandedIndices(ownerIndices, centerElementIndices));

    if (work.sideSourceOffsets.size() != work.sideTaskDepIndices.size())
      return task.emitOpError() << "has inconsistent compact halo side state";

    ArrayRef<Value> centerBlockOffsets =
        depBlockOffsetArgs[work.centerTaskDepIndex];
    for (auto [sideOffsets, sideDepIndex] :
         llvm::zip_equal(work.sideSourceOffsets, work.sideTaskDepIndices)) {
      FailureOr<Value> sidePayload = requirePayload(sideDepIndex);
      if (failed(sidePayload))
        return failure();
      ArrayRef<Value> sideBlockOffsets = depBlockOffsetArgs[sideDepIndex];

      Value condition;
      for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
        Value centerBlock = centerBlockOffsets[slot];
        Value groupEnd = createGroupEnd(builder, loc, centerBlock,
                                        work.centerGroupBlockCounts[slot]);
        if (sideOffsets[slot] == 0) {
          Value atOrAfterStart =
              arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::uge,
                                    indices[slot], centerBlock);
          Value beforeEnd = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, indices[slot], groupEnd);
          condition = andValues(loc, condition, atOrAfterStart);
          condition = andValues(loc, condition, beforeEnd);
          continue;
        }

        Value crossesBlock =
            sideOffsets[slot] < 0
                ? isBeforeGroup(builder, loc, indices[slot], centerBlock)
                : isAfterOrAtGroupEnd(builder, loc, indices[slot], groupEnd);
        Value matchesSide =
            arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::eq,
                                  indices[slot], sideBlockOffsets[slot]);
        condition = andValues(loc, condition, crossesBlock);
        condition = andValues(loc, condition, matchesSide);
      }

      SmallVector<Value, 4> sideElementIndices;
      sideElementIndices.reserve(payloadRank);
      for (unsigned slot = 0; slot < payloadRank; ++slot)
        if (auto ownerIt = llvm::find(work.ownerPayloadDims, slot);
            ownerIt != work.ownerPayloadDims.end()) {
          unsigned ownerSlot = static_cast<unsigned>(
              std::distance(work.ownerPayloadDims.begin(), ownerIt));
          sideElementIndices.push_back(sideOffsets[ownerSlot] != 0
                                           ? createZeroIndex(builder, loc)
                                           : indices[ownerDimCount + slot]);
        } else {
          sideElementIndices.push_back(indices[ownerDimCount + slot]);
        }

      auto ifOp = scf::IfOp::create(builder, loc, load.getType(), condition,
                                    /*withElseRegion=*/true);
      builder.setInsertionPointToStart(&ifOp.getThenRegion().front());
      Value sideValue = memref::LoadOp::create(
          builder, loc, *sidePayload,
          buildRankExpandedIndices(ownerIndices, sideElementIndices));
      scf::YieldOp::create(builder, loc, ValueRange{sideValue});

      builder.setInsertionPointToStart(&ifOp.getElseRegion().front());
      scf::YieldOp::create(builder, loc, ValueRange{replacement});
      replacement = ifOp.getResult(0);
      builder.setInsertionPointAfter(ifOp);
    }

    load.replaceAllUsesWith(replacement);
    load.erase();
  }

  return success();
}

} // namespace mlir::carts::arts::boundary
