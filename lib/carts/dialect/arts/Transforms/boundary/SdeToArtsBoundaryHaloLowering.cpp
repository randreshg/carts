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

void emitCompactHaloCopy(OpBuilder &builder, Location loc,
                         unsigned ownerDimCount,
                         ArrayRef<int64_t> sourceOffsets,
                         ArrayRef<Value> elementExtents, Value sourcePayload,
                         Value compactPayload) {
  unsigned payloadRank = elementExtents.size();
  SmallVector<Value, 4> loopIvs(payloadRank);

  std::function<void(unsigned)> emitAtDim = [&](unsigned dim) {
    if (dim == payloadRank) {
      SmallVector<Value, 4> sourceElementIndices;
      SmallVector<Value, 4> compactElementIndices;
      sourceElementIndices.reserve(payloadRank);
      compactElementIndices.reserve(payloadRank);
      for (unsigned slot = 0; slot < payloadRank; ++slot) {
        if (slot >= ownerDimCount || sourceOffsets[slot] == 0) {
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
          buildRankExpandedElementIndices(builder, loc, ownerDimCount,
                                          sourceElementIndices));
      memref::StoreOp::create(
          builder, loc, value, compactPayload,
          buildRankExpandedElementIndices(builder, loc, ownerDimCount,
                                          compactElementIndices));
      return;
    }

    if (dim < ownerDimCount && sourceOffsets[dim] != 0) {
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

static Value getCommonDivRemSource(Value divValue, Value remValue,
                                   Value expectedDivisor) {
  auto div = ValueAnalysis::stripNumericCasts(divValue)
                 .getDefiningOp<arith::DivUIOp>();
  auto rem = ValueAnalysis::stripNumericCasts(remValue)
                 .getDefiningOp<arith::RemUIOp>();
  if (!div || !rem)
    return {};
  if (!ValueAnalysis::sameValue(div.getLhs(), rem.getLhs()) &&
      !ValueAnalysis::areValuesEquivalent(div.getLhs(), rem.getLhs()))
    return {};
  if ((!ValueAnalysis::sameValue(div.getRhs(), rem.getRhs()) &&
       !ValueAnalysis::areValuesEquivalent(div.getRhs(), rem.getRhs())) ||
      (!ValueAnalysis::sameValue(div.getRhs(), expectedDivisor) &&
       !ValueAnalysis::areValuesEquivalent(div.getRhs(), expectedDivisor)))
    return {};
  return div.getLhs();
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
    Value expr =
        getCommonDivRemSource(indices[slot], indices[ownerDimCount + slot],
                              work.elementExtents[slot]);
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
                                      DirectDepSpec dep, Block *computeBlock) {
  if (dep.ownerDimCount != 2)
    return false;
  if (dep.alloc.getElementSizes().size() != 4) {
    source.emitOpError() << "commits a rank shape that ARTS compact 2D "
                            "unit-halo realization cannot represent";
    return failure();
  }
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
    SmallVector<Value, 4> loopIvs;
    for (Operation *parent = load->getParentOp(); parent;
         parent = parent->getParentOp())
      if (auto loop = dyn_cast<scf::ForOp>(parent))
        loopIvs.push_back(loop.getInductionVar());
    if (loopIvs.size() < 2) {
      load.emitOpError()
          << "is not nested in the 2D compute loops required for ARTS compact "
             "unit-halo load rewriting";
      failedScan = true;
      return WalkResult::interrupt();
    }
    Value rowIv = loopIvs[1];
    Value colIv = loopIvs[0];
    Value rowExpr = getCommonDivRemSource(indices[0], indices[2],
                                          dep.alloc.getElementSizes()[2]);
    Value colExpr = getCommonDivRemSource(indices[1], indices[3],
                                          dep.alloc.getElementSizes()[3]);
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

static SmallVector<Value, 4> buildRankExpandedElementIndices(OpBuilder &builder,
                                                             Location loc,
                                                             Value row,
                                                             Value col) {
  return SmallVector<Value, 4>{createZeroIndex(builder, loc),
                               createZeroIndex(builder, loc), row, col};
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
    OperandRange indices = load.getIndices();
    if (indices.size() != 4)
      return load.emitOpError()
             << "has unsupported compact halo rank after cloning";

    Location loc = load.getLoc();
    builder.setInsertionPoint(load);
    Value elemRow = indices[2];
    Value elemCol = indices[3];
    if (rewrite.face == Halo2DFace::Center) {
      load.getIndicesMutable()[0].set(createZeroIndex(builder, loc));
      load.getIndicesMutable()[1].set(createZeroIndex(builder, loc));
      continue;
    }

    unsigned ownerSlot = 0;
    unsigned faceDepIndex = work.topTaskDepIndex;
    SmallVector<Value, 4> faceIndices;
    switch (rewrite.face) {
    case Halo2DFace::Top:
      ownerSlot = 0;
      faceDepIndex = work.topTaskDepIndex;
      faceIndices = buildRankExpandedElementIndices(
          builder, loc, createZeroIndex(builder, loc), elemCol);
      break;
    case Halo2DFace::Bottom:
      ownerSlot = 0;
      faceDepIndex = work.bottomTaskDepIndex;
      faceIndices = buildRankExpandedElementIndices(
          builder, loc, createZeroIndex(builder, loc), elemCol);
      break;
    case Halo2DFace::Left:
      ownerSlot = 1;
      faceDepIndex = work.leftTaskDepIndex;
      faceIndices = buildRankExpandedElementIndices(
          builder, loc, elemRow, createZeroIndex(builder, loc));
      break;
    case Halo2DFace::Right:
      ownerSlot = 1;
      faceDepIndex = work.rightTaskDepIndex;
      faceIndices = buildRankExpandedElementIndices(
          builder, loc, elemRow, createZeroIndex(builder, loc));
      break;
    case Halo2DFace::Center:
      llvm_unreachable("center handled above");
    }

    FailureOr<Value> facePayload = requirePayload(faceDepIndex);
    if (failed(facePayload))
      return failure();

    Value centerBlock = depBlockOffsetArgs[work.centerTaskDepIndex][ownerSlot];
    Value crossesBlock =
        arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ne,
                              indices[ownerSlot], centerBlock);
    SmallVector<Type, 1> resultTypes{load.getType()};
    auto ifOp = scf::IfOp::create(builder, loc, resultTypes, crossesBlock,
                                  /*withElseRegion=*/true);

    builder.setInsertionPointToStart(&ifOp.getThenRegion().front());
    Value faceValue =
        memref::LoadOp::create(builder, loc, *facePayload, faceIndices);
    scf::YieldOp::create(builder, loc, ValueRange{faceValue});

    builder.setInsertionPointToStart(&ifOp.getElseRegion().front());
    SmallVector<Value, 4> coreIndices =
        buildRankExpandedElementIndices(builder, loc, elemRow, elemCol);
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

    Location loc = load.getLoc();
    builder.setInsertionPoint(load);
    SmallVector<Value, 4> centerElementIndices;
    centerElementIndices.reserve(payloadRank);
    for (unsigned slot = 0; slot < payloadRank; ++slot)
      centerElementIndices.push_back(indices[ownerDimCount + slot]);
    Value replacement = memref::LoadOp::create(
        builder, loc, *centerPayload,
        buildRankExpandedElementIndices(builder, loc, ownerDimCount,
                                        centerElementIndices));

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
        if (sideOffsets[slot] == 0) {
          Value sameBlock =
              arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::eq,
                                    indices[slot], centerBlock);
          condition = andValues(loc, condition, sameBlock);
          continue;
        }

        Value crossesBlock = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ne, indices[slot], centerBlock);
        Value matchesSide =
            arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::eq,
                                  indices[slot], sideBlockOffsets[slot]);
        condition = andValues(loc, condition, crossesBlock);
        condition = andValues(loc, condition, matchesSide);
      }

      SmallVector<Value, 4> sideElementIndices;
      sideElementIndices.reserve(payloadRank);
      for (unsigned slot = 0; slot < payloadRank; ++slot)
        sideElementIndices.push_back(slot < ownerDimCount &&
                                             sideOffsets[slot] != 0
                                         ? createZeroIndex(builder, loc)
                                         : indices[ownerDimCount + slot]);

      auto ifOp = scf::IfOp::create(builder, loc, load.getType(), condition,
                                    /*withElseRegion=*/true);
      builder.setInsertionPointToStart(&ifOp.getThenRegion().front());
      Value sideValue = memref::LoadOp::create(
          builder, loc, *sidePayload,
          buildRankExpandedElementIndices(builder, loc, ownerDimCount,
                                          sideElementIndices));
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
