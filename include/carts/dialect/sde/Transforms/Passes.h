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
std::unique_ptr<Pass> createMemoryUnitMaterializationPass();
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

class SDECostModel;

// --- Input normalization before OpenMP-to-SDE conversion ---
std::unique_ptr<Pass> createSdeInputInlinerPass();
std::unique_ptr<Pass> createSdeMemrefNormalizationPass();
std::unique_ptr<Pass> createSdeHandleDepsPass();

// --- State passes (IR cleanup before planning) ---
std::unique_ptr<Pass> createScalarForwardingPass();
std::unique_ptr<Pass> createMemoryUnitMaterializationPass();
std::unique_ptr<Pass> createSdeRankExpandMuPass();
std::unique_ptr<Pass> createSdeCuNormalizationPass();
std::unique_ptr<Pass> createSdeScalarBlockReductionPass();
std::unique_ptr<Pass> createRaiseToMuAccessWindowPass();
std::unique_ptr<Pass> createSdeCoarseAvoidancePass();
std::unique_ptr<Pass> createSdeRedistributePass();

// --- Dep passes (structural transforms) ---
std::unique_ptr<Pass> createParallelizePass();
std::unique_ptr<Pass> createPatternAnalysisPass();
std::unique_ptr<Pass>
createLayoutAssignmentPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass> createLoopInterchangePass();
std::unique_ptr<Pass> createTilingPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass> createElementwiseFusionPass();
std::unique_ptr<Pass> createIterationSpaceDecompositionPass();

// --- Effect passes (scheduling decisions) ---
std::unique_ptr<Pass>
createScheduleRefinementPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass> createChunkOptPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass>
createReductionStrategyPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass>
createDistributionPlanningPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass>
createBarrierEliminationPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass> createMuAccessWindowSyncOptPass();

// --- Conversion passes ---
std::unique_ptr<Pass> createConvertOpenMPToSdePass();

// --- Verification ---
std::unique_ptr<Pass> createVerifySdePhysicalConsistencyPass();
std::unique_ptr<Pass> createVerifySdeLoweredPass();
std::unique_ptr<Pass> createVerifySdeMuLayoutPass();
std::unique_ptr<Pass> createVerifySdeMuAccessWindowPass();
std::unique_ptr<Pass> createVerifySdeMuAccessWindowSyncPass();
std::unique_ptr<Pass> createVerifySdeCoarseAvoidancePass();
std::unique_ptr<Pass> createVerifySdeRedistributePass();
std::unique_ptr<Pass> createVerifySdePass();

} // namespace mlir::carts::sde

#endif // ARTS_DIALECT_SDE_TRANSFORMS_PASSES_H
