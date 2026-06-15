///==========================================================================///
/// File: SdeToArtsBoundaryAccessLowering.cpp
/// SDE access-window analysis, halo realization, and carrier lowering.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCoarseSu.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCommon.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCuTask.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryDepAnalysis.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHaloLowering.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHelpers.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundarySuIterate.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryTypes.h"
#include "carts/dialect/arts/Utils/DbBackedMemrefUtils.h"
#include "carts/dialect/arts/Utils/DbLayoutFactsUtils.h"
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
#include "mlir/Dialect/Affine/IR/AffineMemoryOpInterfaces.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/AffineMap.h"
#include <algorithm>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {

FailureOr<CompactHaloColumnSpec>
realizeCompactHaloColumnPacks(sde::SdeSuIterateOp source, DirectDepSpec dep,
                              ArrayRef<int64_t> groupBlockCounts,
                              OpBuilder &builder, Location loc) {
  if (dep.ownerDimCount != 2) {
    return source.emitOpError()
           << "commits a halo dependency whose owner rank is not supported by "
              "ARTS compact 2D unit-halo face realization; refusing a "
              "full-block halo byte-window";
  }
  if (hasGroupedOwnerBlocks(groupBlockCounts)) {
    return source.emitOpError()
           << "commits grouped halo CUs, but ARTS compact halo face "
              "realization currently requires one CU per DB block; "
              "refusing a full-block halo byte-window";
  }

  FailureOr<SmallVector<int64_t, 4>> haloRadii = getOwnerHaloRadii(
      dep.haloShape, dep.ownerDimCount, source.getOperation());
  if (failed(haloRadii))
    return failure();
  if (haloRadii->size() != 2 || (*haloRadii)[0] != 1 || (*haloRadii)[1] != 1) {
    return source.emitOpError()
           << "commits a halo dependency outside the supported 2D unit-halo "
              "shape; ARTS must realize the exact halo graph instead of "
              "widening to a full-block byte window";
  }

  if (dep.alloc.getSizes().size() != 2 ||
      dep.alloc.getElementSizes().size() != 4) {
    return source.emitOpError()
           << "commits a rank shape that ARTS compact 2D unit-halo face "
              "realization cannot represent; refusing a full-block halo "
              "byte-window";
  }

  FailureOr<int64_t> rowExtentStatic = requireStaticPositiveIndex(
      dep.alloc.getElementSizes()[2], source.getOperation(), "halo row extent");
  FailureOr<int64_t> colExtentStatic =
      requireStaticPositiveIndex(dep.alloc.getElementSizes()[3],
                                 source.getOperation(), "halo column extent");
  if (failed(rowExtentStatic) || failed(colExtentStatic))
    return failure();

  SmallVector<int64_t, 4> physicalBlockShape;
  physicalBlockShape.reserve(dep.alloc.getElementSizes().size());
  for (Value size : dep.alloc.getElementSizes()) {
    FailureOr<int64_t> constant = requireStaticPositiveIndex(
        size, source.getOperation(), "compact halo block extent");
    if (failed(constant))
      return failure();
    physicalBlockShape.push_back(*constant);
  }

  MLIRContext *ctx = source.getContext();
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value route = arts::createCurrentNodeRoute(builder, loc);
  Value rowExtent = dep.alloc.getElementSizes()[2];
  Value colExtent = dep.alloc.getElementSizes()[3];
  SmallVector<Value, 4> compactElementSizes{one, one, rowExtent,
                                            createOneIndex(builder, loc)};
  SmallVector<int64_t, 4> compactPhysicalBlockShape(physicalBlockShape);
  compactPhysicalBlockShape[3] = 1;

  auto createCompactDb = [&]() -> FailureOr<arts::DbAllocOp> {
    auto db = arts::DbAllocOp::create(
        builder, loc, ArtsMode::inout, route, DbAllocType::heap, DbMode::write,
        dep.alloc.getElementType(),
        SmallVector<Value>(dep.alloc.getSizes().begin(),
                           dep.alloc.getSizes().end()),
        SmallVector<Value>(compactElementSizes.begin(),
                           compactElementSizes.end()),
        PartitionMode::block);
    db.setCompactHaloPayloadAttr(UnitAttr::get(ctx));
    copyDistributionAttrs(dep.alloc.getOperation(), db.getOperation());
    if (hasDistributedDbAllocation(dep.alloc.getOperation()) &&
        !destDbOwnerRouteMatchesSourcePolicy(dep.alloc, db))
      return db.emitOpError()
             << "cannot derive compact halo owner routes from destination DB "
                "grid and source distribution kind";
    return db;
  };

  CompactHaloColumnSpec spec;
  FailureOr<arts::DbAllocOp> leftColumnDb = createCompactDb();
  if (failed(leftColumnDb))
    return failure();
  FailureOr<arts::DbAllocOp> rightColumnDb = createCompactDb();
  if (failed(rightColumnDb))
    return failure();
  spec.leftColumnDb = *leftColumnDb;
  spec.rightColumnDb = *rightColumnDb;
  spec.rowExtent = rowExtent;
  spec.colExtent = colExtent;

  auto outer = scf::ForOp::create(builder, loc, zero, dep.alloc.getSizes()[0],
                                  createOneIndex(builder, loc));
  builder.setInsertionPointToStart(outer.getBody());
  auto inner = scf::ForOp::create(builder, loc, zero, dep.alloc.getSizes()[1],
                                  createOneIndex(builder, loc));
  builder.setInsertionPointToStart(inner.getBody());

  Value blockI = outer.getInductionVar();
  Value blockJ = inner.getInductionVar();
  SmallVector<Value> blockOffsets{blockI, blockJ};
  SmallVector<Value> blockSizes{createOneIndex(builder, loc),
                                createOneIndex(builder, loc)};

  auto sourceAcquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::in, dep.alloc.getGuid(), dep.alloc.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, blockOffsets, blockSizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, Value{}, SmallVector<Value>{},
      SmallVector<Value>{});
  sourceAcquire.setPreserveAccessMode();
  auto leftAcquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::out, spec.leftColumnDb.getGuid(),
      spec.leftColumnDb.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, blockOffsets, blockSizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, Value{}, SmallVector<Value>{},
      SmallVector<Value>{});
  leftAcquire.setPreserveAccessMode();
  auto rightAcquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::out, spec.rightColumnDb.getGuid(),
      spec.rightColumnDb.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, blockOffsets, blockSizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, Value{}, SmallVector<Value>{},
      SmallVector<Value>{});
  rightAcquire.setPreserveAccessMode();

  SmallVector<Value, 4> packDeps{sourceAcquire.getPtr(), leftAcquire.getPtr(),
                                 rightAcquire.getPtr()};
  SmallVector<Value, 4> packParams{rowExtent, colExtent};
  auto packEdt = arts::EdtOp::create(
      builder, loc, arts::EdtType::task, arts::EdtConcurrency::intranode,
      arts::createCurrentNodeRoute(builder, loc), packDeps, packParams);
  packEdt.setCompactHaloPackAttr(UnitAttr::get(ctx));

  Block &packBlock = packEdt.getBody().front();
  for (Value depValue : packDeps)
    packBlock.addArgument(depValue.getType(), loc);
  unsigned paramOffset = packBlock.getNumArguments();
  for (Value param : packParams)
    packBlock.addArgument(param.getType(), loc);

  OpBuilder bodyBuilder(packEdt.getContext());
  bodyBuilder.setInsertionPointToStart(&packBlock);
  Value sourcePayload =
      arts::realizeDbInnerPayload(bodyBuilder, loc, packBlock.getArgument(0));
  Value leftPayload =
      arts::realizeDbInnerPayload(bodyBuilder, loc, packBlock.getArgument(1));
  Value rightPayload =
      arts::realizeDbInnerPayload(bodyBuilder, loc, packBlock.getArgument(2));
  Value rowLimit = packBlock.getArgument(paramOffset);
  Value colLimit = packBlock.getArgument(paramOffset + 1);
  Value lastCol = arith::SubIOp::create(bodyBuilder, loc, colLimit,
                                        createOneIndex(bodyBuilder, loc));
  auto rowLoop =
      scf::ForOp::create(bodyBuilder, loc, createZeroIndex(bodyBuilder, loc),
                         rowLimit, createOneIndex(bodyBuilder, loc));
  bodyBuilder.setInsertionPointToStart(rowLoop.getBody());
  Value row = rowLoop.getInductionVar();
  SmallVector<Value, 4> sourceLeftIdx{createZeroIndex(bodyBuilder, loc),
                                      createZeroIndex(bodyBuilder, loc), row,
                                      createZeroIndex(bodyBuilder, loc)};
  Value leftValue =
      memref::LoadOp::create(bodyBuilder, loc, sourcePayload, sourceLeftIdx);
  memref::StoreOp::create(bodyBuilder, loc, leftValue, leftPayload,
                          sourceLeftIdx);
  SmallVector<Value, 4> sourceRightIdx{createZeroIndex(bodyBuilder, loc),
                                       createZeroIndex(bodyBuilder, loc), row,
                                       lastCol};
  SmallVector<Value, 4> compactRightIdx{createZeroIndex(bodyBuilder, loc),
                                        createZeroIndex(bodyBuilder, loc), row,
                                        createZeroIndex(bodyBuilder, loc)};
  Value rightValue =
      memref::LoadOp::create(bodyBuilder, loc, sourcePayload, sourceRightIdx);
  memref::StoreOp::create(bodyBuilder, loc, rightValue, rightPayload,
                          compactRightIdx);
  bodyBuilder.setInsertionPointToEnd(&packBlock);
  arts::YieldOp::create(bodyBuilder, loc);

  builder.setInsertionPointAfter(outer);
  return spec;
}

arts::DbAcquireOp
create2DUnitRowHaloAcquire(sde::SdeSuIterateOp source, DirectDepSpec dep,
                           ArrayRef<Value> currentBlockOffsets,
                           CompactHaloColumnSpec spec, bool topFace,
                           SmallVectorImpl<Value> &dbOffsets,
                           OpBuilder &builder, Location loc) {
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockI = currentBlockOffsets[0];
  Value blockJ = currentBlockOffsets[1];
  Value sourceI;
  Value boundsValid;
  Value elementRowOffset;
  if (topFace) {
    Value canShift = arith::CmpIOp::create(
        builder, loc, arith::CmpIPredicate::uge, blockI, one);
    Value shifted = arith::SubIOp::create(builder, loc, blockI, one);
    sourceI = arith::SelectOp::create(builder, loc, canShift, shifted, zero);
    boundsValid = canShift;
    elementRowOffset = arith::SubIOp::create(builder, loc, spec.rowExtent, one);
  } else {
    sourceI = arith::AddIOp::create(builder, loc, blockI, one);
    boundsValid = arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ult,
                                        sourceI, dep.alloc.getSizes()[0]);
    sourceI =
        arith::SelectOp::create(builder, loc, boundsValid, sourceI, blockI);
    elementRowOffset = zero;
  }

  SmallVector<Value> offsets{sourceI, blockJ};
  SmallVector<Value> sizes{one, createOneIndex(builder, loc)};
  dbOffsets.assign(offsets.begin(), offsets.end());
  SmallVector<Value> elementOffsets{
      createZeroIndex(builder, loc), createZeroIndex(builder, loc),
      elementRowOffset, createZeroIndex(builder, loc)};
  SmallVector<Value> elementSizes{createOneIndex(builder, loc),
                                  createOneIndex(builder, loc),
                                  createOneIndex(builder, loc), spec.colExtent};
  auto acquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::in, dep.alloc.getGuid(), dep.alloc.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, boundsValid, elementOffsets,
      elementSizes);
  acquire.setPreserveAccessMode();
  if (topFace)
    attachStencilHaloAcquireFacts(source, acquire, {-1, 0}, {0, 0});
  else
    attachStencilHaloAcquireFacts(source, acquire, {0, 0}, {1, 0});
  return acquire;
}

arts::DbAcquireOp create2DUnitCompactColumnAcquire(
    DirectDepSpec dep, ArrayRef<Value> currentBlockOffsets,
    CompactHaloColumnSpec spec, bool leftFace,
    SmallVectorImpl<Value> &dbOffsets, OpBuilder &builder, Location loc) {
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockI = currentBlockOffsets[0];
  Value blockJ = currentBlockOffsets[1];
  Value sourceJ;
  Value boundsValid;
  arts::DbAllocOp compactDb;
  if (leftFace) {
    Value canShift = arith::CmpIOp::create(
        builder, loc, arith::CmpIPredicate::uge, blockJ, one);
    Value shifted = arith::SubIOp::create(builder, loc, blockJ, one);
    sourceJ = arith::SelectOp::create(builder, loc, canShift, shifted, zero);
    boundsValid = canShift;
    compactDb = spec.rightColumnDb;
  } else {
    sourceJ = arith::AddIOp::create(builder, loc, blockJ, one);
    boundsValid = arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ult,
                                        sourceJ, dep.alloc.getSizes()[1]);
    sourceJ =
        arith::SelectOp::create(builder, loc, boundsValid, sourceJ, blockJ);
    compactDb = spec.leftColumnDb;
  }

  SmallVector<Value> offsets{blockI, sourceJ};
  SmallVector<Value> sizes{one, createOneIndex(builder, loc)};
  dbOffsets.assign(offsets.begin(), offsets.end());
  auto acquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::in, compactDb.getGuid(), compactDb.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, boundsValid,
      SmallVector<Value>{}, SmallVector<Value>{});
  acquire.setPreserveAccessMode();
  return acquire;
}

FailureOr<CompactHaloNdSpec>
realizeCompactHaloNdPacks(sde::SdeSuIterateOp source, DirectDepSpec dep,
                          ArrayRef<int64_t> groupBlockCounts,
                          OpBuilder &builder, Location loc) {
  unsigned ownerDimCount = dep.ownerDimCount;
  if (ownerDimCount < 2) {
    return source.emitOpError()
           << "requires owner rank of at least 2 for compact N-D halo "
              "realization, got "
           << ownerDimCount;
  }
  if (ownerDimCount > 3) {
    return source.emitOpError()
           << "commits a halo dependency whose owner rank exceeds the "
              "implemented ARTS compact N-D unit-halo realization; refusing a "
              "full-block halo byte-window";
  }
  if (hasGroupedOwnerBlocks(groupBlockCounts)) {
    return source.emitOpError()
           << "commits grouped halo CUs, but ARTS compact N-D halo "
              "realization currently requires one CU per DB block; refusing a "
              "full-block halo byte-window";
  }

  FailureOr<SmallVector<int64_t, 4>> haloRadii = getOwnerHaloRadii(
      dep.haloShape, dep.ownerDimCount, source.getOperation());
  if (failed(haloRadii))
    return failure();
  if (llvm::any_of(*haloRadii, [](int64_t radius) { return radius != 1; })) {
    return source.emitOpError()
           << "commits a halo dependency outside the supported unit-halo "
              "shape; ARTS must realize the exact halo graph instead of "
              "widening to a full-block byte window";
  }

  if (dep.alloc.getSizes().size() != ownerDimCount ||
      dep.alloc.getElementSizes().size() != ownerDimCount * 2) {
    return source.emitOpError()
           << "commits a rank shape that ARTS compact N-D unit-halo "
              "realization cannot represent; refusing a full-block halo "
              "byte-window";
  }

  SmallVector<Value, 4> elementExtents;
  elementExtents.reserve(ownerDimCount);
  for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
    FailureOr<int64_t> extent = requireStaticPositiveIndex(
        dep.alloc.getElementSizes()[ownerDimCount + slot],
        source.getOperation(), "halo element extent");
    if (failed(extent))
      return failure();
    elementExtents.push_back(dep.alloc.getElementSizes()[ownerDimCount + slot]);
  }

  SmallVector<int64_t, 4> physicalBlockShape;
  physicalBlockShape.reserve(dep.alloc.getElementSizes().size());
  for (Value size : dep.alloc.getElementSizes()) {
    FailureOr<int64_t> constant = requireStaticPositiveIndex(
        size, source.getOperation(), "compact halo block extent");
    if (failed(constant))
      return failure();
    physicalBlockShape.push_back(*constant);
  }

  SmallVector<SmallVector<int64_t, 4>, 8> sideOffsets;
  enumerateUnitHaloSourceOffsets(ownerDimCount, sideOffsets);

  MLIRContext *ctx = source.getContext();
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value route = arts::createCurrentNodeRoute(builder, loc);

  CompactHaloNdSpec spec;
  spec.elementExtents.assign(elementExtents.begin(), elementExtents.end());
  spec.sides.reserve(sideOffsets.size());

  for (ArrayRef<int64_t> sideOffset : sideOffsets) {
    SmallVector<Value> compactElementSizes;
    compactElementSizes.reserve(ownerDimCount * 2);
    for (unsigned slot = 0; slot < ownerDimCount; ++slot)
      compactElementSizes.push_back(one);
    for (unsigned slot = 0; slot < ownerDimCount; ++slot)
      compactElementSizes.push_back(sideOffset[slot] == 0 ? elementExtents[slot]
                                                          : one);

    SmallVector<int64_t, 4> compactPhysicalBlockShape(physicalBlockShape);
    for (unsigned slot = 0; slot < ownerDimCount; ++slot)
      if (sideOffset[slot] != 0)
        compactPhysicalBlockShape[ownerDimCount + slot] = 1;

    auto payloadDb = arts::DbAllocOp::create(
        builder, loc, ArtsMode::inout, route, DbAllocType::heap, DbMode::write,
        dep.alloc.getElementType(),
        SmallVector<Value>(dep.alloc.getSizes().begin(),
                           dep.alloc.getSizes().end()),
        SmallVector<Value>(compactElementSizes.begin(),
                           compactElementSizes.end()),
        PartitionMode::block);
    payloadDb.setCompactHaloPayloadAttr(UnitAttr::get(ctx));
    copyDistributionAttrs(dep.alloc.getOperation(), payloadDb.getOperation());
    if (hasDistributedDbAllocation(dep.alloc.getOperation()) &&
        !destDbOwnerRouteMatchesSourcePolicy(dep.alloc, payloadDb))
      return payloadDb.emitOpError()
             << "cannot derive compact halo owner routes from destination DB "
                "grid and source distribution kind";
    spec.sides.push_back(
        {SmallVector<int64_t, 4>(sideOffset.begin(), sideOffset.end()),
         payloadDb});
  }

  for (CompactHaloNdSideSpec &side : spec.sides) {
    SmallVector<Value> blockOffsets;
    blockOffsets.reserve(ownerDimCount);
    scf::ForOp outerLoop;
    for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
      auto loop =
          scf::ForOp::create(builder, loc, zero, dep.alloc.getSizes()[slot],
                             createOneIndex(builder, loc));
      if (!outerLoop)
        outerLoop = loop;
      blockOffsets.push_back(loop.getInductionVar());
      builder.setInsertionPointToStart(loop.getBody());
    }

    SmallVector<Value> blockSizes(ownerDimCount, one);
    auto sourceAcquire = arts::DbAcquireOp::create(
        builder, loc, ArtsMode::in, dep.alloc.getGuid(), dep.alloc.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::block),
        SmallVector<Value>{},
        SmallVector<Value>(blockOffsets.begin(), blockOffsets.end()),
        blockSizes, SmallVector<Value>{}, SmallVector<Value>{},
        SmallVector<Value>{}, Value{}, SmallVector<Value>{},
        SmallVector<Value>{});
    sourceAcquire.setPreserveAccessMode();
    auto payloadAcquire = arts::DbAcquireOp::create(
        builder, loc, ArtsMode::out, side.payloadDb.getGuid(),
        side.payloadDb.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::block),
        SmallVector<Value>{},
        SmallVector<Value>(blockOffsets.begin(), blockOffsets.end()),
        blockSizes, SmallVector<Value>{}, SmallVector<Value>{},
        SmallVector<Value>{}, Value{}, SmallVector<Value>{},
        SmallVector<Value>{});
    payloadAcquire.setPreserveAccessMode();

    SmallVector<Value, 4> packDeps{sourceAcquire.getPtr(),
                                   payloadAcquire.getPtr()};
    SmallVector<Value, 4> packParams(elementExtents.begin(),
                                     elementExtents.end());
    auto packEdt = arts::EdtOp::create(
        builder, loc, arts::EdtType::task, arts::EdtConcurrency::intranode,
        arts::createCurrentNodeRoute(builder, loc), packDeps, packParams);
    packEdt.setCompactHaloPackAttr(UnitAttr::get(ctx));

    Block &packBlock = packEdt.getBody().front();
    for (Value depValue : packDeps)
      packBlock.addArgument(depValue.getType(), loc);
    unsigned paramOffset = packBlock.getNumArguments();
    for (Value param : packParams)
      packBlock.addArgument(param.getType(), loc);

    OpBuilder bodyBuilder(packEdt.getContext());
    bodyBuilder.setInsertionPointToStart(&packBlock);
    Value sourcePayload =
        arts::realizeDbInnerPayload(bodyBuilder, loc, packBlock.getArgument(0));
    Value compactPayload =
        arts::realizeDbInnerPayload(bodyBuilder, loc, packBlock.getArgument(1));
    SmallVector<Value, 4> bodyElementExtents;
    bodyElementExtents.reserve(ownerDimCount);
    for (unsigned slot = 0; slot < ownerDimCount; ++slot)
      bodyElementExtents.push_back(packBlock.getArgument(paramOffset + slot));
    emitCompactHaloCopy(bodyBuilder, loc, side.sourceOffsets,
                        bodyElementExtents, sourcePayload, compactPayload);
    bodyBuilder.setInsertionPointToEnd(&packBlock);
    arts::YieldOp::create(bodyBuilder, loc);

    builder.setInsertionPointAfter(outerLoop);
  }

  return spec;
}

arts::DbAcquireOp createNdCompactHaloAcquire(
    DirectDepSpec dep, ArrayRef<Value> currentBlockOffsets,
    CompactHaloNdSideSpec side, SmallVectorImpl<Value> &dbOffsets,
    OpBuilder &builder, Location loc) {
  Value one = createOneIndex(builder, loc);
  Value boundsValid = {};
  auto appendBounds = [&](Value condition) {
    boundsValid = boundsValid ? arith::AndIOp::create(builder, loc, boundsValid,
                                                      condition)
                              : condition;
  };

  SmallVector<Value> offsets;
  offsets.reserve(currentBlockOffsets.size());
  for (auto [slot, sourceOffset] : llvm::enumerate(side.sourceOffsets)) {
    Value current = currentBlockOffsets[slot];
    if (sourceOffset < 0) {
      Value canShift = arith::CmpIOp::create(
          builder, loc, arith::CmpIPredicate::uge, current, one);
      Value shifted = arith::SubIOp::create(builder, loc, current, one);
      offsets.push_back(
          arith::SelectOp::create(builder, loc, canShift, shifted, current));
      appendBounds(canShift);
      continue;
    }
    if (sourceOffset > 0) {
      Value shifted = arith::AddIOp::create(builder, loc, current, one);
      Value canShift =
          arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ult,
                                shifted, dep.alloc.getSizes()[slot]);
      offsets.push_back(
          arith::SelectOp::create(builder, loc, canShift, shifted, current));
      appendBounds(canShift);
      continue;
    }
    offsets.push_back(current);
  }

  if (!boundsValid)
    boundsValid = arith::ConstantIntOp::create(builder, loc, 1, 1);
  dbOffsets.assign(offsets.begin(), offsets.end());
  SmallVector<Value> sizes(currentBlockOffsets.size(), one);
  auto acquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::in, side.payloadDb.getGuid(),
      side.payloadDb.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, boundsValid,
      SmallVector<Value>{}, SmallVector<Value>{});
  acquire.setPreserveAccessMode();
  return acquire;
}

template <typename RewriteT>
LogicalResult recordClonedHaloLoadRewrites(
    Operation *original, Operation *cloned,
    const DenseMap<Operation *, RewriteT> &originalRewrites,
    DenseMap<Operation *, RewriteT> &clonedRewrites) {
  auto recordIfMapped = [&](Operation *originalLoad, Operation *clonedLoad) {
    auto it = originalRewrites.find(originalLoad);
    if (it != originalRewrites.end())
      clonedRewrites[clonedLoad] = it->second;
  };

  SmallVector<memref::LoadOp, 8> originalLoads;
  SmallVector<memref::LoadOp, 8> clonedLoads;
  if (auto load = dyn_cast<memref::LoadOp>(original))
    originalLoads.push_back(load);
  else
    original->walk([&](memref::LoadOp load) { originalLoads.push_back(load); });
  if (auto load = dyn_cast<memref::LoadOp>(cloned))
    clonedLoads.push_back(load);
  else
    cloned->walk([&](memref::LoadOp load) { clonedLoads.push_back(load); });

  bool hasMappedLoad = llvm::any_of(originalLoads, [&](memref::LoadOp load) {
    return originalRewrites.contains(load.getOperation());
  });
  if (!hasMappedLoad)
    return success();
  if (originalLoads.size() != clonedLoads.size())
    return cloned->emitError()
           << "could not preserve compact halo load rewrite mapping while "
              "cloning SDE compute body";
  for (auto [originalLoad, clonedLoad] : llvm::zip(originalLoads, clonedLoads))
    recordIfMapped(originalLoad.getOperation(), clonedLoad.getOperation());
  return success();
}

LogicalResult rewriteOwnerIndicesToLocal(
    arts::EdtOp task, ArrayRef<Value> payloads,
    ArrayRef<SmallVector<Value, 4>> depBlockOffsetArgs,
    ArrayRef<bool> depRequiresDbRef, ArrayRef<unsigned> depOwnerDimCounts,
    ArrayRef<SmallVector<int64_t, 4>> depGroupBlockCounts) {
  if (payloads.size() != depBlockOffsetArgs.size() ||
      payloads.size() != depRequiresDbRef.size() ||
      payloads.size() != depOwnerDimCounts.size() ||
      payloads.size() != depGroupBlockCounts.size())
    return task.emitOpError() << "has inconsistent owner grouping facts";
  for (auto [offsets, ownerDimCount, groupCounts] : llvm::zip_equal(
           depBlockOffsetArgs, depOwnerDimCounts, depGroupBlockCounts))
    if (offsets.size() != ownerDimCount || groupCounts.size() != ownerDimCount)
      return task.emitOpError() << "has inconsistent dependency block offsets";

  auto depHasGroupedBlocks = [&](unsigned depIdx) {
    return llvm::any_of(depGroupBlockCounts[depIdx],
                        [](int64_t count) { return count > 1; });
  };

  DenseMap<Value, unsigned> payloadToDepIndex;
  DenseMap<Value, Value> payloadSources;
  for (auto [idx, payload] : llvm::enumerate(payloads)) {
    auto [it, inserted] = payloadToDepIndex.try_emplace(payload, idx);
    if (!inserted)
      return task.emitOpError()
             << "has duplicate dependency payload during owner-index rewrite";
    if (!depHasGroupedBlocks(idx) && !depRequiresDbRef[idx])
      continue;
    auto ref = payload.getDefiningOp<arts::DbRefOp>();
    if (!ref)
      return task.emitOpError()
             << "cannot reindex owner blocks without a DB-ref payload";
    payloadSources[payload] = ref.getSource();
  }

  OpBuilder builder(task.getContext());
  auto rewriteAccess = [&](Operation *op, Value memref,
                           MutableOperandRange indices) -> WalkResult {
    Value root = ValueAnalysis::stripMemrefViewOps(memref);
    auto depIt = payloadToDepIndex.find(root);
    if (depIt == payloadToDepIndex.end())
      return WalkResult::advance();
    unsigned depIdx = depIt->second;
    unsigned ownerDimCount = depOwnerDimCounts[depIdx];
    if (ownerDimCount == 0)
      return WalkResult::advance();
    if (indices.size() < ownerDimCount) {
      op->emitError() << "rank-expanded DB payload access has fewer indices "
                         "than committed owner dimensions";
      return WalkResult::interrupt();
    }
    builder.setInsertionPoint(op);
    if (depHasGroupedBlocks(depIdx) || depRequiresDbRef[depIdx]) {
      if (root != memref) {
        op->emitError()
            << "grouped DB access through a memref view is not realized; "
               "SDE-to-ARTS must rewrite the view or fail closed";
        return WalkResult::interrupt();
      }
      auto sourceIt = payloadSources.find(root);
      if (sourceIt == payloadSources.end()) {
        op->emitError() << "has no grouped dependency source";
        return WalkResult::interrupt();
      }
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

LogicalResult
convertSuIterate(sde::SdeSuIterateOp source,
                 DenseSet<Operation *> &consumedCuLevelAccessWindows,
                 SmallVectorImpl<Operation *> &consumedRedists) {
  if (source.getNumResults() != 0 || !source.getReductionAccumulators().empty())
    return source.emitOpError()
           << "direct SDE-to-ARTS lowering requires reduction/result facts to "
              "be authored as explicit SDE-to-ARTS reduction operations";

  SmallVector<DirectDepSpec, 4> deps;
  if (failed(collectSuDependencies(source, deps, consumedCuLevelAccessWindows,
                                   consumedRedists)))
    return failure();
  if (deps.empty()) {
    if (hasCommittedPartialReductionFacts(source))
      return source.emitOpError()
             << "commits partial-reduction facts without SDE access windows; "
                "SDE must expose block dependencies before ARTS lowering";
    return tryConvertCoarseSuIterate(source);
  }

  std::optional<CommittedPhysicalLayout> physicalLayout =
      readCommittedPhysicalLayout(source, deps);
  if (!physicalLayout || physicalLayout->blockShape.empty() ||
      physicalLayout->ownerDims.empty())
    return tryConvertCoarseSuIterate(source);

  ArrayRef<int64_t> ownerDims = physicalLayout->ownerDims;
  ArrayRef<int64_t> blockShape = physicalLayout->blockShape;

  unsigned loopRank = source.getUpperBounds().size();
  if (source.getLowerBounds().size() != loopRank ||
      source.getSteps().size() != loopRank ||
      source.getBody().front().getNumArguments() < loopRank)
    return source.emitOpError() << "has inconsistent loop bounds";

  FailureOr<ArtsOwnerSlotMapping> ownerRouteping = resolveArtsOwnerSlotMapping(
      ownerDims, blockShape, loopRank, source.getOperation());
  if (failed(ownerRouteping))
    return failure();

  ArrayRef<int64_t> ownerSlotDims = ownerRouteping->ownerDims;
  unsigned ownerDimCount = ownerSlotDims.size();
  for (DirectDepSpec &dep : deps)
    if (dep.accessSlots.size() != dep.ownerDimCount && !dep.reduceScatter)
      return source.emitOpError()
             << "dependency access-window coordinates do not match owner rank";

  SmallVector<int64_t, 4> ownerBlockSizes(ownerRouteping->blockSizes.begin(),
                                          ownerRouteping->blockSizes.end());

  SmallVector<int64_t, 4> workerSpans(ownerBlockSizes.begin(),
                                      ownerBlockSizes.end());
  SmallVector<int64_t, 4> groupBlockCounts(ownerDimCount, 1);
  if (sde::SdeCuRegionOp computeCu = sde::findSuComputeCuRegion(source)) {
    if (auto groupCounts =
            readI64ArrayAttr(computeCu.getGroupBlockCountAttr())) {
      // groupBlockCount may be committed per-owner-slot (size == ownerDimCount)
      // or per-array-dim (a full-rank elementwise writer commits one count per
      // logical dim, with count 1 on undistributed dims). Accept both: index
      // per-array-dim counts by the resolved owner dims.
      bool perOwner = groupCounts->size() == ownerDimCount;
      bool perArrayDim =
          !perOwner && llvm::all_of(ownerSlotDims, [&](int64_t d) {
            return d >= 0 && static_cast<size_t>(d) < groupCounts->size();
          });
      if (!perOwner && !perArrayDim)
        return source.emitOpError()
               << "commits groupBlockCount whose rank does not match the "
                  "committed owner rank";
      for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
        int64_t count = perOwner ? (*groupCounts)[slot]
                                 : (*groupCounts)[ownerSlotDims[slot]];
        int64_t blockSize = ownerBlockSizes[slot];
        if (count <= 0)
          return source.emitOpError()
                 << "commits groupBlockCount that cannot be represented as "
                    "a whole-number group of physical DB blocks";
        workerSpans[slot] = blockSize * count;
        groupBlockCounts[slot] = count;
      }
    }
  }

  bool splitToOwnerLocalGroups = false;
  if (hasDistributedWriterStorageFacts(deps)) {
    std::optional<int64_t> totalNodes =
        arts::getRuntimeTotalNodes(source->getParentOfType<ModuleOp>());
    if (!totalNodes)
      return source.emitOpError()
             << "requires runtime node count to keep grouped distributed "
                "writers owner-local";
    if (failed(ensureDistributedWriterOwnerLocalGroups(
            source, deps, groupBlockCounts, workerSpans, ownerBlockSizes,
            *totalNodes, splitToOwnerLocalGroups)))
      return failure();
  }

  SetVector<Value> scalarCaptures;
  if (failed(collectExternalScalarCaptures(source, scalarCaptures)))
    return failure();

  Location loc = source.getLoc();
  OpBuilder builder(source);
  Block *computeBlock = sde::getSuIterateComputeBlock(source);
  if (!computeBlock)
    return source.emitOpError() << "has no computable body";

  SmallVector<CompactHaloColumnSpec, 4> compactHaloColumnSpecs;
  DenseMap<unsigned, unsigned> compactColumnSpecByDepIndex;
  SmallVector<CompactHaloNdSpec, 4> compactHaloNdSpecs;
  DenseMap<unsigned, unsigned> compactNdSpecByDepIndex;
  for (auto [depIndex, dep] : llvm::enumerate(deps)) {
    if (!dep.haloShape)
      continue;
    if (dep.mode != ArtsMode::in)
      return source.emitOpError()
             << "commits a halo dependency that is not read-only; ARTS cannot "
                "realize a writable halo window";
    bool useExactNdHalo = false;
    if (dep.ownerDimCount == 2) {
      FailureOr<bool> needsExact =
          needsExactNdHaloFor2D(source, dep, computeBlock);
      if (failed(needsExact))
        return failure();
      useExactNdHalo = *needsExact;
    }
    if (dep.ownerDimCount == 2 && !useExactNdHalo) {
      FailureOr<CompactHaloColumnSpec> compactSpec =
          realizeCompactHaloColumnPacks(source, dep, groupBlockCounts, builder,
                                        loc);
      if (failed(compactSpec))
        return failure();
      compactColumnSpecByDepIndex[depIndex] =
          static_cast<unsigned>(compactHaloColumnSpecs.size());
      compactHaloColumnSpecs.push_back(*compactSpec);
      continue;
    }
    FailureOr<CompactHaloNdSpec> compactSpec =
        realizeCompactHaloNdPacks(source, dep, groupBlockCounts, builder, loc);
    if (failed(compactSpec))
      return failure();
    compactNdSpecByDepIndex[depIndex] =
        static_cast<unsigned>(compactHaloNdSpecs.size());
    compactHaloNdSpecs.push_back(*compactSpec);
  }
  if (!compactHaloColumnSpecs.empty() || !compactHaloNdSpecs.empty()) {
    auto reason = arts::ArtsBarrierReasonAttr::get(
        source.getContext(), arts::ArtsBarrierReason::required_memory);
    arts::BarrierOp::create(builder, loc, reason);
  }

  SmallVector<Value, 4> dispatchBases;
  SmallVector<Value, 4> dispatchBlockOffsets;
  scf::ForOp dispatchRoot;
  for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
    unsigned physicalDim = ownerRouteping->loopDims[slot];
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
    unsigned physicalDim = ownerRouteping->loopDims[slot];
    Value base = dispatchBases[slot];
    Value lower = source.getLowerBounds()[physicalDim];
    Value delta = arith::SubIOp::create(builder, loc, base, lower);
    Value blockSize = createConstantIndex(builder, loc, ownerBlockSizes[slot]);
    dispatchBlockOffsets.push_back(
        arith::DivUIOp::create(builder, loc, delta, blockSize));
  }
  DenseMap<unsigned, unsigned> dispatchSlotByLoopDim;
  for (auto [slot, loopDim] : llvm::enumerate(ownerRouteping->loopDims))
    dispatchSlotByLoopDim.try_emplace(loopDim, static_cast<unsigned>(slot));

  unsigned ndHaloSideCount = 0;
  for (const CompactHaloNdSpec &spec : compactHaloNdSpecs)
    ndHaloSideCount += spec.sides.size();
  SmallVector<Value, 4> taskDeps;
  taskDeps.reserve(deps.size() + compactHaloColumnSpecs.size() * 4 +
                   ndHaloSideCount);
  SmallVector<Attribute, 4> partialReductionDepResultDimMaps;
  const bool hasPartialReduction = hasCommittedPartialReductionFacts(source);
  if (hasPartialReduction)
    partialReductionDepResultDimMaps.reserve(taskDeps.capacity());
  SmallVector<SmallVector<Value, 4>> depBlockOffsets;
  SmallVector<bool> depRequiresDbRef;
  SmallVector<unsigned, 4> taskDepOwnerDimCounts;
  SmallVector<SmallVector<int64_t, 4>> taskDepGroupBlockCounts;
  SmallVector<unsigned, 4> primaryTaskDepForDep(deps.size(), 0);
  SmallVector<Halo2DTaskWork, 4> haloTaskWorks;
  DenseMap<unsigned, unsigned> haloTaskWorkByDepIndex;
  SmallVector<HaloNdTaskWork, 4> haloNdTaskWorks;
  DenseMap<unsigned, unsigned> haloNdTaskWorkByDepIndex;
  depBlockOffsets.reserve(taskDeps.capacity());
  depRequiresDbRef.reserve(taskDeps.capacity());
  taskDepOwnerDimCounts.reserve(taskDeps.capacity());
  taskDepGroupBlockCounts.reserve(taskDeps.capacity());

  auto appendTaskDep = [&](Value depPtr, ArrayRef<Value> blockOffsets,
                           bool requiresDbRef, unsigned depOwnerDimCount,
                           ArrayRef<int64_t> depGroupBlockCounts,
                           ArrayAttr depResultDimMap) -> unsigned {
    unsigned taskDepIndex = static_cast<unsigned>(taskDeps.size());
    taskDeps.push_back(depPtr);
    if (hasPartialReduction)
      partialReductionDepResultDimMaps.push_back(
          depResultDimMap ? depResultDimMap
                          : Builder(source.getContext()).getArrayAttr({}));
    depBlockOffsets.push_back(
        SmallVector<Value, 4>(blockOffsets.begin(), blockOffsets.end()));
    depRequiresDbRef.push_back(requiresDbRef);
    taskDepOwnerDimCounts.push_back(depOwnerDimCount);
    taskDepGroupBlockCounts.push_back(SmallVector<int64_t, 4>(
        depGroupBlockCounts.begin(), depGroupBlockCounts.end()));
    return taskDepIndex;
  };

  for (auto [depIndex, dep] : llvm::enumerate(deps)) {
    SmallVector<Value> offsets;
    SmallVector<Value> sizes;
    SmallVector<int64_t, 4> depGroupBlockCounts(dep.ownerDimCount, 1);
    offsets.reserve(dep.ownerDimCount);
    sizes.reserve(dep.ownerDimCount);

    if (dep.reduceScatter) {
      for (unsigned slot = 0; slot < dep.ownerDimCount; ++slot) {
        offsets.push_back(createZeroIndex(builder, loc));
        sizes.push_back(dep.alloc.getSizes()[slot]);
      }
    } else {
      if (dep.accessSlots.size() != dep.ownerDimCount)
        return source.emitOpError() << "dependency access-window coordinates "
                                       "do not cover owner rank";
      for (unsigned slot = 0; slot < dep.ownerDimCount; ++slot) {
        const DepOwnerAccessSlot &access = dep.accessSlots[slot];
        if (access.fullWindow) {
          Value offset = createConstantIndex(builder, loc, dep.blockLo[slot]);
          int64_t staticGroupCount =
              std::max<int64_t>(1, dep.blockHi[slot] - dep.blockLo[slot]);
          Value requested = createConstantIndex(builder, loc, staticGroupCount);
          Value remaining = arith::SubIOp::create(
              builder, loc, dep.alloc.getSizes()[slot], offset);
          offsets.push_back(offset);
          sizes.push_back(
              arith::MinUIOp::create(builder, loc, remaining, requested));
          depGroupBlockCounts[slot] = staticGroupCount;
          continue;
        }
        if (access.fixedBlock) {
          offsets.push_back(
              createConstantIndex(builder, loc, *access.fixedBlock));
          sizes.push_back(createOneIndex(builder, loc));
          depGroupBlockCounts[slot] = 1;
          continue;
        }
        if (!access.loopDim ||
            *access.loopDim >= source.getUpperBounds().size())
          return source.emitOpError()
                 << "dependency access-window coordinate has no loop dimension";
        unsigned physicalDim = *access.loopDim;
        FailureOr<unsigned> depPayloadDim =
            getAccessWindowPayloadDim(source, dep, slot, physicalDim);
        if (failed(depPayloadDim))
          return failure();
        FailureOr<int64_t> payloadExtent =
            getAccessWindowPayloadExtent(source, dep, *depPayloadDim);
        if (failed(payloadExtent))
          return failure();
        (void)payloadExtent;
        int64_t coordinateBlockSize = access.coordinateBlockSize;
        if (coordinateBlockSize <= 0)
          return source.emitOpError()
                 << "dependency access-window coordinate has non-positive "
                    "block size";
        Value lower = source.getLowerBounds()[physicalDim];
        Value upper = source.getUpperBounds()[physicalDim];
        Value coordinateBlockSizeValue =
            createConstantIndex(builder, loc, coordinateBlockSize);
        Value base = lower;
        Value groupEnd = upper;
        int64_t staticGroupCount =
            std::max<int64_t>(1, dep.blockHi[slot] - dep.blockLo[slot]);
        auto dispatchIt = dispatchSlotByLoopDim.find(physicalDim);
        if (dispatchIt != dispatchSlotByLoopDim.end()) {
          unsigned dispatchSlot = dispatchIt->second;
          base = dispatchBases[dispatchSlot];
          Value groupSpan =
              createConstantIndex(builder, loc, workerSpans[dispatchSlot]);
          groupEnd = arith::MinUIOp::create(
              builder, loc,
              arith::AddIOp::create(builder, loc, base, groupSpan), upper);
          staticGroupCount =
              std::max<int64_t>(1, ceilDivPositiveI64(workerSpans[dispatchSlot],
                                                      coordinateBlockSize));
        }
        Value rawOffset = arith::DivUIOp::create(builder, loc, base,
                                                 coordinateBlockSizeValue);
        Value rawEnd = ceilDivPositiveIndex(builder, loc, groupEnd,
                                            coordinateBlockSizeValue);
        Value offset = rawOffset;
        if (dep.blockLo[slot] != 0) {
          Value windowLo = createConstantIndex(builder, loc, dep.blockLo[slot]);
          offset = arith::MaxUIOp::create(builder, loc, rawOffset, windowLo);
        }
        Value windowHi = createConstantIndex(builder, loc, dep.blockHi[slot]);
        Value end = arith::MinUIOp::create(builder, loc, rawEnd, windowHi);
        Value remaining = arith::SubIOp::create(
            builder, loc, dep.alloc.getSizes()[slot], offset);
        Value count = arith::SubIOp::create(builder, loc, end, offset);
        // Bound the span by the static group count so
        // DistributedLaunchConsistency can prove owner-local routing (a
        // dynamic-only span has no known bound).
        Value staticBound = createConstantIndex(builder, loc, staticGroupCount);
        count = arith::MinUIOp::create(builder, loc, count, staticBound);
        sizes.push_back(arith::MinUIOp::create(builder, loc, remaining, count));
        offsets.push_back(offset);
        depGroupBlockCounts[slot] = staticGroupCount;
      }
    }
    bool requiresDbRef = dep.reduceScatter.has_value() ||
                         llvm::any_of(depGroupBlockCounts,
                                      [](int64_t count) { return count > 1; });
    FailureOr<ArrayAttr> depResultDimMap =
        buildPartialReductionDepResultDimMap(source, dep);
    if (failed(depResultDimMap))
      return failure();

    if (dep.haloShape) {
      auto centerAcquire = arts::DbAcquireOp::create(
          builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
          std::optional<arts::PartitionMode>(arts::PartitionMode::block),
          SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
          SmallVector<Value>{}, SmallVector<Value>{}, Value{},
          SmallVector<Value>{}, SmallVector<Value>{});
      centerAcquire.setPreserveAccessMode();
      unsigned centerTaskDep = appendTaskDep(
          centerAcquire.getPtr(), offsets, requiresDbRef, dep.ownerDimCount,
          depGroupBlockCounts, *depResultDimMap);
      primaryTaskDepForDep[depIndex] = centerTaskDep;

      auto columnSpecIt = compactColumnSpecByDepIndex.find(depIndex);
      if (columnSpecIt != compactColumnSpecByDepIndex.end()) {
        const CompactHaloColumnSpec &compactSpec =
            compactHaloColumnSpecs[columnSpecIt->second];

        SmallVector<Value, 4> topOffsets;
        auto topAcquire = create2DUnitRowHaloAcquire(
            source, dep, dispatchBlockOffsets, compactSpec, /*topFace=*/true,
            topOffsets, builder, loc);
        unsigned topTaskDep = appendTaskDep(
            topAcquire.getPtr(), topOffsets,
            /*requiresDbRef=*/false, dep.ownerDimCount,
            SmallVector<int64_t, 4>(dep.ownerDimCount, 1), ArrayAttr{});

        SmallVector<Value, 4> bottomOffsets;
        auto bottomAcquire = create2DUnitRowHaloAcquire(
            source, dep, dispatchBlockOffsets, compactSpec, /*topFace=*/false,
            bottomOffsets, builder, loc);
        unsigned bottomTaskDep = appendTaskDep(
            bottomAcquire.getPtr(), bottomOffsets, /*requiresDbRef=*/false,
            dep.ownerDimCount, SmallVector<int64_t, 4>(dep.ownerDimCount, 1),
            ArrayAttr{});

        SmallVector<Value, 4> leftOffsets;
        auto leftAcquire = create2DUnitCompactColumnAcquire(
            dep, dispatchBlockOffsets, compactSpec, /*leftFace=*/true,
            leftOffsets, builder, loc);
        unsigned leftTaskDep = appendTaskDep(
            leftAcquire.getPtr(), leftOffsets,
            /*requiresDbRef=*/false, dep.ownerDimCount,
            SmallVector<int64_t, 4>(dep.ownerDimCount, 1), ArrayAttr{});

        SmallVector<Value, 4> rightOffsets;
        auto rightAcquire = create2DUnitCompactColumnAcquire(
            dep, dispatchBlockOffsets, compactSpec, /*leftFace=*/false,
            rightOffsets, builder, loc);
        unsigned rightTaskDep = appendTaskDep(
            rightAcquire.getPtr(), rightOffsets,
            /*requiresDbRef=*/false, dep.ownerDimCount,
            SmallVector<int64_t, 4>(dep.ownerDimCount, 1), ArrayAttr{});

        Halo2DTaskWork haloTaskWork;
        haloTaskWork.centerTaskDepIndex = centerTaskDep;
        haloTaskWork.topTaskDepIndex = topTaskDep;
        haloTaskWork.bottomTaskDepIndex = bottomTaskDep;
        haloTaskWork.leftTaskDepIndex = leftTaskDep;
        haloTaskWork.rightTaskDepIndex = rightTaskDep;
        haloTaskWork.rowExtent = compactSpec.rowExtent;
        haloTaskWork.colExtent = compactSpec.colExtent;
        haloTaskWorkByDepIndex[depIndex] =
            static_cast<unsigned>(haloTaskWorks.size());
        haloTaskWorks.push_back(haloTaskWork);
        continue;
      }

      auto specIt = compactNdSpecByDepIndex.find(depIndex);
      if (specIt == compactNdSpecByDepIndex.end())
        return source.emitOpError()
               << "lost compact N-D halo payload state for committed halo "
                  "dependency";
      const CompactHaloNdSpec &compactSpec = compactHaloNdSpecs[specIt->second];
      HaloNdTaskWork haloTaskWork;
      haloTaskWork.centerTaskDepIndex = centerTaskDep;
      haloTaskWork.elementExtents.assign(compactSpec.elementExtents.begin(),
                                         compactSpec.elementExtents.end());
      SmallVector<int64_t, 4> singleBlockCounts(dep.ownerDimCount, 1);
      for (const CompactHaloNdSideSpec &side : compactSpec.sides) {
        SmallVector<Value, 4> sideOffsets;
        auto sideAcquire = createNdCompactHaloAcquire(
            dep, dispatchBlockOffsets, side, sideOffsets, builder, loc);
        unsigned sideTaskDep =
            appendTaskDep(sideAcquire.getPtr(), sideOffsets,
                          /*requiresDbRef=*/false, dep.ownerDimCount,
                          singleBlockCounts, ArrayAttr{});
        haloTaskWork.sideSourceOffsets.push_back(side.sourceOffsets);
        haloTaskWork.sideTaskDepIndices.push_back(sideTaskDep);
      }
      haloNdTaskWorkByDepIndex[depIndex] =
          static_cast<unsigned>(haloNdTaskWorks.size());
      haloNdTaskWorks.push_back(std::move(haloTaskWork));
      continue;
    }

    std::optional<arts::PartitionMode> partitionMode =
        std::optional<arts::PartitionMode>(arts::PartitionMode::block);
    if (dep.ownerDimCount == 0) {
      buildWholeDbAcquireWindow(builder, loc, dep.alloc, offsets, sizes);
      partitionMode = arts::PartitionMode::coarse;
    }
    auto acquire = arts::DbAcquireOp::create(
        builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
        partitionMode, SmallVector<Value>{}, offsets, sizes,
        SmallVector<Value>{}, SmallVector<Value>{}, SmallVector<Value>{},
        Value{}, SmallVector<Value>{}, SmallVector<Value>{});
    acquire.setPreserveAccessMode();
    if (dep.reduceScatter)
      acquire.setReplicatedReadAttr(UnitAttr::get(source.getContext()));
    primaryTaskDepForDep[depIndex] =
        appendTaskDep(acquire.getPtr(), offsets, requiresDbRef,
                      dep.ownerDimCount, depGroupBlockCounts, *depResultDimMap);
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
      hasDistributedLaunchStorageFacts(deps), builder, loc);
  Value route =
      launch.route ? launch.route : arts::createCurrentNodeRoute(builder, loc);
  auto task =
      arts::EdtOp::create(builder, loc, arts::EdtType::task, launch.concurrency,
                          route, taskDeps, taskParams);
  if (failed(attachCommittedSdeFacts(source, task)))
    return failure();
  if (hasPartialReduction) {
    if (partialReductionDepResultDimMaps.size() != taskDeps.size())
      return source.emitOpError()
             << "lost partial-reduction dependency/result mapping while "
                "building ARTS task dependencies";
    task.setPartialReductionDepResultDimMapsAttr(
        builder.getArrayAttr(partialReductionDepResultDimMaps));
  }
  if (splitToOwnerLocalGroups)
    task.setOwnerLocalWriterSplitAttr(UnitAttr::get(source.getContext()));

  Block &taskBlock = task.getBody().front();
  for (Value dep : taskDeps)
    taskBlock.addArgument(dep.getType(), loc);
  unsigned paramOffset = taskBlock.getNumArguments();
  for (Value param : taskParams)
    taskBlock.addArgument(param.getType(), loc);

  IRMapping mapper;
  SmallVector<Value, 4> payloads;
  payloads.reserve(taskDeps.size());
  OpBuilder bodyBuilder(task.getContext());
  bodyBuilder.setInsertionPointToStart(&taskBlock);
  for (auto [idx, dep] : llvm::enumerate(taskDeps)) {
    Value payload = arts::realizeDbInnerPayload(bodyBuilder, loc,
                                                taskBlock.getArgument(idx));
    payloads.push_back(payload);
  }
  auto mapIfAbsent = [&](Value from, Value to) {
    if (from && !mapper.lookupOrNull(from))
      mapper.map(from, to);
  };
  for (auto [depIdx, dep] : llvm::enumerate(deps)) {
    unsigned taskDepIndex = primaryTaskDepForDep[depIdx];
    mapIfAbsent(dep.alloc.getPtr(), taskBlock.getArgument(taskDepIndex));
  }
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
    unsigned depOwnerDimCount = taskDepOwnerDimCounts[depIdx];
    offsets.reserve(depOwnerDimCount);
    for (unsigned slot = 0; slot < depOwnerDimCount; ++slot) {
      unsigned paramIndex = depBlockOffsetParamIndices[depIdx][slot];
      offsets.push_back(taskBlock.getArgument(paramOffset + paramIndex));
    }
    taskDepBlockOffsetArgs.push_back(std::move(offsets));
  }

  WalkResult accessWindowMapResult =
      source.getBody().walk([&](arts::DbAccessWindowOp window) {
        auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
            arts::DbUtils::getUnderlyingDbAlloc(window.getMu()));
        if (!alloc) {
          window.emitOpError() << "lost backing DB allocation during direct "
                                  "SDE-to-ARTS lowering";
          return WalkResult::interrupt();
        }
        auto it = llvm::find_if(
            deps, [&](const DirectDepSpec &dep) { return dep.alloc == alloc; });
        if (it == deps.end()) {
          window.emitOpError() << "has no matching direct ARTS dependency";
          return WalkResult::interrupt();
        }
        return WalkResult::advance();
      });
  if (accessWindowMapResult.wasInterrupted())
    return failure();

  DenseMap<Operation *, HaloLoadRewrite> originalHaloLoadRewrites;
  if (!haloTaskWorks.empty()) {
    if (ownerDimCount != 2 || ownerRouteping->loopDims.size() != 2)
      return source.emitOpError() << "commits a halo dependency whose owner "
                                     "rank is not supported by "
                                     "ARTS compact 2D unit-halo load rewriting";
    WalkResult classifyResult =
        computeBlock->walk([&](memref::LoadOp load) {
          arts::DbAllocOp alloc = resolveBoundaryDbAlloc(load.getMemref());
          if (!alloc)
            return WalkResult::advance();
          auto depIt = llvm::find_if(deps, [&](const DirectDepSpec &dep) {
            return dep.alloc == alloc;
          });
          if (depIt == deps.end())
            return WalkResult::advance();
          unsigned depIdx =
              static_cast<unsigned>(std::distance(deps.begin(), depIt));
          auto haloIt = haloTaskWorkByDepIndex.find(depIdx);
          if (haloIt == haloTaskWorkByDepIndex.end())
            return WalkResult::advance();

          SmallVector<Value, 4> loopIvs;
          for (Operation *parent = load->getParentOp(); parent;
               parent = parent->getParentOp())
            if (auto loop = dyn_cast<scf::ForOp>(parent))
              loopIvs.push_back(loop.getInductionVar());
          if (loopIvs.size() < 2) {
            load.emitOpError()
                << "is not nested in the 2D compute loops required for ARTS "
                   "compact unit-halo load rewriting";
            return WalkResult::interrupt();
          }
          Value rowIv = loopIvs[1];
          Value colIv = loopIvs[0];
          FailureOr<std::optional<HaloLoadRewrite>> rewrite =
              classify2DUnitHaloLoad(load, haloIt->second,
                                     haloTaskWorks[haloIt->second], rowIv,
                                     colIv);
          if (failed(rewrite))
            return WalkResult::interrupt();
          if (rewrite->has_value())
            originalHaloLoadRewrites[load.getOperation()] = **rewrite;
          return WalkResult::advance();
        });
    if (classifyResult.wasInterrupted())
      return failure();
  }

  DenseMap<Operation *, HaloNdLoadRewrite> originalHaloNdLoadRewrites;
  if (!haloNdTaskWorks.empty()) {
    WalkResult classifyResult =
        computeBlock->walk([&](memref::LoadOp load) {
          arts::DbAllocOp alloc = resolveBoundaryDbAlloc(load.getMemref());
          if (!alloc)
            return WalkResult::advance();
          auto depIt = llvm::find_if(deps, [&](const DirectDepSpec &dep) {
            return dep.alloc == alloc;
          });
          if (depIt == deps.end())
            return WalkResult::advance();
          unsigned depIdx =
              static_cast<unsigned>(std::distance(deps.begin(), depIt));
          auto haloIt = haloNdTaskWorkByDepIndex.find(depIdx);
          if (haloIt == haloNdTaskWorkByDepIndex.end())
            return WalkResult::advance();

          const HaloNdTaskWork &work = haloNdTaskWorks[haloIt->second];
          unsigned rank = work.elementExtents.size();
          SmallVector<Value, 4> loopIvs;
          for (Operation *parent = load->getParentOp(); parent;
               parent = parent->getParentOp())
            if (auto loop = dyn_cast<scf::ForOp>(parent))
              loopIvs.push_back(loop.getInductionVar());
          if (loopIvs.size() < rank) {
            load.emitOpError()
                << "is not nested in the N-D compute loops required for ARTS "
                   "compact unit-halo load rewriting";
            return WalkResult::interrupt();
          }

          SmallVector<Value, 4> ownerLoopIvs;
          ownerLoopIvs.reserve(rank);
          for (unsigned slot = 0; slot < rank; ++slot)
            ownerLoopIvs.push_back(loopIvs[rank - 1 - slot]);
          FailureOr<std::optional<HaloNdLoadRewrite>> rewrite =
              classifyNdUnitHaloLoad(load, haloIt->second, work, ownerLoopIvs);
          if (failed(rewrite))
            return WalkResult::interrupt();
          if (rewrite->has_value())
            originalHaloNdLoadRewrites[load.getOperation()] = **rewrite;
          return WalkResult::advance();
        });
    if (classifyResult.wasInterrupted())
      return failure();
  }

  for (unsigned dim = 0; dim < loopRank; ++dim) {
    auto ownerIt = llvm::find(ownerRouteping->loopDims, dim);
    Value lower;
    Value upper = remapOrSelf(mapper, source.getUpperBounds()[dim]);
    Value step = remapOrSelf(mapper, source.getSteps()[dim]);
    if (ownerIt != ownerRouteping->loopDims.end()) {
      unsigned slot = static_cast<unsigned>(
          std::distance(ownerRouteping->loopDims.begin(), ownerIt));
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

  DenseMap<Operation *, HaloLoadRewrite> clonedHaloLoadRewrites;
  DenseMap<Operation *, HaloNdLoadRewrite> clonedHaloNdLoadRewrites;
  for (Operation &nested : computeBlock->without_terminator()) {
    if (isa<arts::DbAccessWindowOp>(&nested))
      continue;
    Operation *cloned = nested.clone(mapper);
    bodyBuilder.insert(cloned);
    if (!originalHaloLoadRewrites.empty())
      if (failed(recordClonedHaloLoadRewrites(&nested, cloned,
                                              originalHaloLoadRewrites,
                                              clonedHaloLoadRewrites)))
        return failure();
    if (!originalHaloNdLoadRewrites.empty())
      if (failed(recordClonedHaloLoadRewrites(&nested, cloned,
                                              originalHaloNdLoadRewrites,
                                              clonedHaloNdLoadRewrites)))
        return failure();
  }

  auto rewriteClonedAccess = [&](Operation *op, Value memref,
                                 ArtsMode mode) -> WalkResult {
    arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
    if (!alloc)
      return WalkResult::advance();
    std::optional<unsigned> depIdx =
        findDirectDepIndexForAccess(deps, alloc, mode,
                                    /*preferHaloRead=*/true);
    if (!depIdx) {
      op->emitError()
          << "has no committed SDE access-window dependency for direct "
             "ARTS lowering";
      return WalkResult::interrupt();
    }
    unsigned taskDepIndex = primaryTaskDepForDep[*depIdx];
    if (taskDepIndex >= payloads.size()) {
      op->emitError() << "lost direct dependency payload while lowering "
                         "SDE access window";
      return WalkResult::interrupt();
    }
    if (auto load = dyn_cast<memref::LoadOp>(op))
      load.getMemrefMutable().assign(payloads[taskDepIndex]);
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      store.getMemrefMutable().assign(payloads[taskDepIndex]);
    return WalkResult::advance();
  };
  WalkResult rewriteResult = task.getBody().walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return rewriteClonedAccess(op, load.getMemref(), ArtsMode::in);
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return rewriteClonedAccess(op, store.getMemref(), ArtsMode::out);
    return WalkResult::advance();
  });
  if (rewriteResult.wasInterrupted())
    return failure();

  if (failed(rewriteCloned2DUnitHaloLoads(task, clonedHaloLoadRewrites,
                                          haloTaskWorks, payloads,
                                          taskDepBlockOffsetArgs)))
    return failure();
  if (failed(rewriteClonedNdUnitHaloLoads(task, clonedHaloNdLoadRewrites,
                                          haloNdTaskWorks, payloads,
                                          taskDepBlockOffsetArgs)))
    return failure();
  if (failed(translateSdeAtomicsToArts(task.getBody())))
    return failure();
  if (failed(rewriteOwnerIndicesToLocal(task, payloads, taskDepBlockOffsetArgs,
                                        depRequiresDbRef, taskDepOwnerDimCounts,
                                        taskDepGroupBlockCounts)))
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

LogicalResult collectTaskDependencies(sde::SdeCuTaskOp source,
                                      SmallVectorImpl<TaskDepSpec> &deps) {
  WalkResult result =
      source.getBody().walk([&](sde::SdeMuDepOp dep) {
        if (!dep.getDep().use_empty()) {
          dep.emitOpError()
              << "result is consumed; SDE task dependencies must remain local "
                 "declarations before ARTS realization";
          return WalkResult::interrupt();
        }
        auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
            arts::DbUtils::getUnderlyingDbAlloc(dep.getSource()));
        if (!alloc) {
          dep.emitOpError()
              << "does not reference an ARTS DB-backed memref after storage "
                 "realization";
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

LogicalResult convertCuTask(sde::SdeCuTaskOp source) {
  SmallVector<TaskDepSpec, 4> deps;
  if (failed(collectTaskDependencies(source, deps)))
    return failure();

  SetVector<Value> scalarCaptures;
  if (failed(collectExternalScalarCaptures(source, scalarCaptures)))
    return failure();

  Location loc = source.getLoc();
  OpBuilder builder(source);
  SmallVector<Value, 4> taskDeps;
  taskDeps.reserve(deps.size());
  for (TaskDepSpec &dep : deps) {
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
  for (TaskDepSpec &dep : deps) {
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
    Value payload = arts::realizeDbInnerPayload(bodyBuilder, loc,
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

} // namespace mlir::carts::arts::boundary
