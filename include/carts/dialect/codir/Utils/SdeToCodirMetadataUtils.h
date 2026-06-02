///==========================================================================///
/// File: SdeToCodirMetadataUtils.h
///
/// SDE-owned planning metadata translation for the SDE -> CODIR boundary.
/// These helpers carry source scheduling intent into CODIR without creating
/// ARTS runtime/orchestration objects.
///
/// The SDE and CODIR enum classes for AccessMode, Pattern, DistributionKind,
/// IterationTopology, RepetitionStructure, and ReductionStrategy have
/// identical case sets and integer codes
/// (audit-verified). Translation across the boundary is a static_cast.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_UTILS_SDETOCODIRMETADATAUTILS_H
#define CARTS_DIALECT_CODIR_UTILS_SDETOCODIRMETADATAUTILS_H

#include "carts/dialect/codir/IR/CodirDialect.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/MLIRContext.h"

namespace mlir::carts::codir::sde_to_codir {

static_assert(static_cast<int>(sde::SdeAccessMode::readwrite) ==
              static_cast<int>(CodirAccessMode::readwrite));
static_assert(static_cast<int>(sde::SdePattern::reduction) ==
              static_cast<int>(CodirPattern::reduction));
static_assert(static_cast<int>(sde::SdeDistributionKind::cyclic) ==
              static_cast<int>(CodirDistributionKind::cyclic));
static_assert(static_cast<int>(sde::SdeIterationTopology::owner_tile_2d) ==
              static_cast<int>(CodirIterationTopology::owner_tile_2d));
static_assert(static_cast<int>(sde::SdeRepetitionStructure::full_timestep) ==
              static_cast<int>(CodirRepetitionStructure::full_timestep));
static_assert(static_cast<int>(sde::SdeReductionStrategy::local_accumulate) ==
              static_cast<int>(CodirReductionStrategy::local_accumulate));

struct CodirCodeletMetadata {
  CodirPatternAttr pattern;
  CodirReductionStrategyAttr reductionStrategy;
  UnitAttr partialReduction;
  ArrayAttr partialReductionDims;
  ArrayAttr partialReductionOwnerDims;
  ArrayAttr partialReductionDepResultDimMaps;
  CodirDistributionKindAttr distributionKind;
  CodirIterationTopologyAttr iterationTopology;
  CodirRepetitionStructureAttr repetitionStructure;
  ArrayAttr depArrayIds;
  ArrayAttr planOwnerDims;
  ArrayAttr tileOwnerDims;
  ArrayAttr tileShape;
  ArrayAttr logicalWorkerSlice;
  ArrayAttr haloShape;
  ArrayAttr accessMinOffsets;
  ArrayAttr accessMaxOffsets;
  ArrayAttr spatialDims;
  ArrayAttr writeFootprint;
  UnitAttr inPlaceSafe;
  UnitAttr inPlaceSharedState;
  // SDE module-scoped layout-assignment carriers, threaded verbatim.
  ArrayAttr arrayLayout;
  ArrayAttr layoutsDisagree;
  Attribute partitionGraph;
  Attribute partitionScore;
};

inline CodirCodeletMetadata getCodirMetadataFromTask(sde::SdeCuTaskOp task) {
  CodirCodeletMetadata metadata;
  if (!task)
    return metadata;
  MLIRContext *ctx = task.getContext();
  if (auto pattern = task.getPatternAttr())
    metadata.pattern = CodirPatternAttr::get(
        ctx, static_cast<CodirPattern>(pattern.getValue()));
  return metadata;
}

inline CodirCodeletMetadata
getCodirMetadataFromSchedulingUnit(sde::SdeSuIterateOp source) {
  CodirCodeletMetadata metadata;
  if (!source)
    return metadata;
  MLIRContext *ctx = source.getContext();
  if (auto pattern = source.getPatternAttr())
    metadata.pattern = CodirPatternAttr::get(
        ctx, static_cast<CodirPattern>(pattern.getValue()));
  if (auto kind = source.getDistributionKindAttr())
    metadata.distributionKind = CodirDistributionKindAttr::get(
        ctx, static_cast<CodirDistributionKind>(kind.getValue()));
  else if (auto parentDistribute =
               source->getParentOfType<sde::SdeSuDistributeOp>())
    metadata.distributionKind = CodirDistributionKindAttr::get(
        ctx, static_cast<CodirDistributionKind>(parentDistribute.getKind()));
  if (auto topology = source.getIterationTopologyAttr())
    metadata.iterationTopology = CodirIterationTopologyAttr::get(
        ctx, static_cast<CodirIterationTopology>(topology.getValue()));
  if (auto repetition = source.getRepetitionStructureAttr())
    metadata.repetitionStructure = CodirRepetitionStructureAttr::get(
        ctx, static_cast<CodirRepetitionStructure>(repetition.getValue()));
  if (auto strategy = source.getReductionStrategyAttr())
    metadata.reductionStrategy = CodirReductionStrategyAttr::get(
        ctx, static_cast<CodirReductionStrategy>(strategy.getValue()));
  if (source.getPartialReductionAttr())
    metadata.partialReduction = UnitAttr::get(ctx);
  metadata.partialReductionDims = source.getPartialReductionDimsAttr();
  metadata.partialReductionOwnerDims =
      source.getPartialReductionOwnerDimsAttr();
  metadata.planOwnerDims = source.getOwnerDimsAttr();
  metadata.tileOwnerDims = source.getPhysicalOwnerDimsAttr();
  metadata.tileShape = source.getPhysicalBlockShapeAttr();
  metadata.logicalWorkerSlice = source.getLogicalWorkerSliceAttr();
  metadata.haloShape = source.getPhysicalHaloShapeAttr();
  metadata.accessMinOffsets = source.getAccessMinOffsetsAttr();
  metadata.accessMaxOffsets = source.getAccessMaxOffsetsAttr();
  metadata.spatialDims = source.getSpatialDimsAttr();
  metadata.writeFootprint = source.getWriteFootprintAttr();
  if (source.getInPlaceSafeAttr())
    metadata.inPlaceSafe = UnitAttr::get(ctx);
  if (source.getInPlaceSharedStateAttr())
    metadata.inPlaceSharedState = UnitAttr::get(ctx);
  // Carry the SDE layout-assignment facts verbatim across the boundary. These
  // are plain Array/Integer attrs (no SDE/CODIR enum split), so no translation
  // is needed beyond copying the attribute handle.
  metadata.arrayLayout = source.getArrayLayoutAttr();
  metadata.layoutsDisagree = source.getLayoutsDisagreeAttr();
  metadata.partitionGraph = source->getAttr(sde::AttrNames::PartitionGraph);
  metadata.partitionScore = source->getAttr(sde::AttrNames::PartitionScore);
  return metadata;
}

} // namespace mlir::carts::codir::sde_to_codir

#endif // CARTS_DIALECT_CODIR_UTILS_SDETOCODIRMETADATAUTILS_H
