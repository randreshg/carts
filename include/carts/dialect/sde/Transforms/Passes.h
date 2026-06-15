///==========================================================================///
/// File: Passes.h
///
/// Pass declarations for the SDE (Structured Decomposition Environment)
/// dialect.
///
/// This header is self-contained: it includes all dialect headers required
/// by dependentDialects in SDE Passes.td so that .h.inc can be safely
/// included without manual per-file dependency management.
///==========================================================================///

#ifndef ARTS_DIALECT_SDE_TRANSFORMS_PASSES_H
#define ARTS_DIALECT_SDE_TRANSFORMS_PASSES_H

#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Pass/Pass.h"
#include "polygeist/Dialect.h"

namespace mlir::carts::sde {
std::unique_ptr<Pass> createMemoryUnitRealizationPass();
} // namespace mlir::carts::sde

namespace mlir::carts::sde {

#define GEN_PASS_DECL
#include "carts/dialect/sde/Transforms/Passes.h.inc"

} // namespace mlir::carts::sde

namespace mlir::carts::sde {

/// Ensure a region has at least one block, creating an empty one if needed.
inline Block &ensureBlock(Region &region) {
  if (region.empty())
    region.push_back(new Block());
  return region.front();
}

/// Strip the operand segment sizes attribute when cloning/recreating an
/// SdeSuIterateOp so that the builder can recompute it from operands.
inline NamedAttrList getRewrittenAttrs(SdeSuIterateOp op) {
  NamedAttrList attrs(op->getAttrs());
  attrs.erase(op.getOperandSegmentSizesAttrName().getValue());
  return attrs;
}

/// Single construction entry point for `sde.cu_region`. Centralizes the one
/// SdeCuRegionOp::create call so an additive `cu_region` attribute (e.g. a
/// serial-license tag, or Part-6 async-default state) is one edit here, not one
/// at every construction site. Byte-identical to a direct create with the same
/// values. (Mirrors `buildSuIterate` for the SU op.)
inline SdeCuRegionOp buildCuRegion(OpBuilder &builder, Location loc,
                                   SdeCuKindAttr kind,
                                   UnitAttr nowait = nullptr,
                                   ValueRange iterArgs = {},
                                   TypeRange resultTypes = {},
                                   SdeSerialReasonAttr serialReason = nullptr) {
  return SdeCuRegionOp::create(builder, loc, resultTypes, kind, nowait,
                               iterArgs, serialReason,
                               /*groupBlockCount=*/nullptr);
}

/// The full optional attribute set of an `sde.su_iterate`, each defaulting to
/// null. Callers set only what they need; the rest stay absent. This is the
/// single carrier the positionally-threaded `SdeSuIterateOp::create` arguments
/// used to be, so that adding/removing an su_iterate attribute is one edit here
/// instead of one at every construction site (design-revision.md Part 4 Step
/// 1).
struct SuIterateAttrs {
  UnitAttr nowait = nullptr;
  ArrayAttr reductionKinds = nullptr;
  UnitAttr partialReduction = nullptr;
  ArrayAttr partialReductionDims = nullptr;
  ArrayAttr partialReductionOwnerDims = nullptr;
  SdeStructuredClassificationAttr structuredClassification = nullptr;
  SdePatternAttr pattern = nullptr;
  ArrayAttr accessMinOffsets = nullptr;
  ArrayAttr accessMaxOffsets = nullptr;
  ArrayAttr ownerDims = nullptr;
  ArrayAttr spatialDims = nullptr;
  ArrayAttr writeFootprint = nullptr;
  UnitAttr inPlaceSafe = nullptr;
  UnitAttr inPlaceSharedState = nullptr;
  ArrayAttr arrayLayout = nullptr;
  ArrayAttr layoutsDisagree = nullptr;

  /// Copy every optional attribute verbatim from an existing op (for the
  /// clone-with-new-bounds construction sites).
  static SuIterateAttrs fromOp(SdeSuIterateOp op) {
    SuIterateAttrs a;
    a.nowait = op.getNowaitAttr();
    a.reductionKinds = op.getReductionKindsAttr();
    a.partialReduction = op.getPartialReductionAttr();
    a.partialReductionDims = op.getPartialReductionDimsAttr();
    a.partialReductionOwnerDims = op.getPartialReductionOwnerDimsAttr();
    a.structuredClassification = op.getStructuredClassificationAttr();
    a.pattern = op.getPatternAttr();
    a.accessMinOffsets = op.getAccessMinOffsetsAttr();
    a.accessMaxOffsets = op.getAccessMaxOffsetsAttr();
    a.ownerDims = op.getOwnerDimsAttr();
    a.spatialDims = op.getSpatialDimsAttr();
    a.writeFootprint = op.getWriteFootprintAttr();
    a.inPlaceSafe = op.getInPlaceSafeAttr();
    a.inPlaceSharedState = op.getInPlaceSharedStateAttr();
    a.arrayLayout = op.getArrayLayoutAttr();
    return a;
  }
};

/// Single construction entry point for `sde.su_iterate`. Forwards to
/// SdeSuIterateOp::create with the positional attribute soup hidden behind the
/// named SuIterateAttrs carrier. Byte-identical to a direct create call with
/// the same values.
inline SdeSuIterateOp buildSuIterate(OpBuilder &builder, Location loc,
                                     ValueRange lowerBounds,
                                     ValueRange upperBounds, ValueRange steps,
                                     const SuIterateAttrs &attrs = {},
                                     ValueRange reductionAccumulators = {},
                                     TypeRange resultTypes = {}) {
  return SdeSuIterateOp::create(
      builder, loc, resultTypes, lowerBounds, upperBounds, steps, attrs.nowait,
      reductionAccumulators, attrs.reductionKinds, attrs.partialReduction,
      attrs.partialReductionDims, attrs.partialReductionOwnerDims,
      attrs.structuredClassification, attrs.pattern, attrs.accessMinOffsets,
      attrs.accessMaxOffsets, attrs.ownerDims, attrs.spatialDims,
      attrs.writeFootprint, attrs.inPlaceSafe, attrs.inPlaceSharedState,
      attrs.arrayLayout);
}

class SDECostModel;

// --- Input normalization before OpenMP-to-SDE conversion ---
std::unique_ptr<Pass> createSdeInputInlinerPass();
std::unique_ptr<Pass> createSdeMemrefNormalizationPass();
std::unique_ptr<Pass> createSdeHandleDepsPass();

// --- State passes (IR cleanup before planning) ---
std::unique_ptr<Pass> createScalarForwardingPass();
std::unique_ptr<Pass> createMemoryUnitRealizationPass();
std::unique_ptr<Pass> createSdeRankExpandMuPass();
std::unique_ptr<Pass> createSdeCuNormalizationPass();
std::unique_ptr<Pass> createSdeScalarBlockReductionPass();
std::unique_ptr<Pass> createSdeCoarseAvoidancePass();
std::unique_ptr<Pass> createSdeRedistributePass();

// --- Dep passes (structural transforms) ---
std::unique_ptr<Pass> createRaiseToSdePass();
std::unique_ptr<Pass> createSdeLoopPatternFactsPass();
std::unique_ptr<Pass>
createLayoutAssignmentPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass> createLoopInterchangePass();
std::unique_ptr<Pass> createTilingPass(SDECostModel *costModel = nullptr);

// --- Effect passes (scheduling decisions) ---
std::unique_ptr<Pass>
createReductionStrategyPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass> createSdeAtomicReductionRealizationPass();
std::unique_ptr<Pass>
createDistributionPlanningPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass>
createDistributionFailClosedPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass>
createBlockGrainPlanPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass>
createOwnerDimSelectPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass>
createBarrierEliminationPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass> createMuAccessWindowSyncOptPass();

// --- Conversion passes ---
std::unique_ptr<Pass> createConvertOpenMPToSdePass();

// --- Verification ---
std::unique_ptr<Pass> createVerifySdeLoweredPass();
std::unique_ptr<Pass> createVerifySdeMuAccessWindowSyncPass();
std::unique_ptr<Pass> createVerifySdeCoarseAvoidancePass();
std::unique_ptr<Pass> createVerifySdePass();

} // namespace mlir::carts::sde

#endif // ARTS_DIALECT_SDE_TRANSFORMS_PASSES_H
