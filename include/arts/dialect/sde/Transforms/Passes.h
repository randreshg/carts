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

#include "arts/Dialect.h"
#include "arts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Bufferization/IR/Bufferization.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/Pass/Pass.h"

namespace mlir::arts {

#define GEN_PASS_DECL
#include "arts/dialect/sde/Transforms/Passes.h.inc"

} // namespace mlir::arts

namespace mlir::arts::sde {

class SDECostModel;

std::unique_ptr<Pass>
createConvertOpenMPToSdePass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass>
createSdeChunkOptimizationPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass>
createSdeScheduleRefinementPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass>
createSdeReductionStrategyPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass>
createSdeScopeSelectionPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass>
createSdeTensorOptimizationPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass> createSdeStructuredSummariesPass();
std::unique_ptr<Pass>
createSdeDistributionPlanningPass(SDECostModel *costModel = nullptr);
std::unique_ptr<Pass> createSdeElementwiseFusionPass();
std::unique_ptr<Pass> createConvertSdeToArtsPass();
std::unique_ptr<Pass> createVerifySdeLoweredPass();
std::unique_ptr<Pass> createRaiseToLinalgPass();
std::unique_ptr<Pass> createRaiseToTensorPass();

} // namespace mlir::arts::sde

#endif // ARTS_DIALECT_SDE_TRANSFORMS_PASSES_H
