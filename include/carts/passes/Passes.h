///==========================================================================///
/// File: Passes.h
///
/// This file declares pass creation APIs and pass registration hooks
/// for the ARTS dialect.
///==========================================================================///

#ifndef CARTS_PASSES_PASSES_H
#define CARTS_PASSES_PASSES_H

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "mlir/Conversion/LLVMCommon/LoweringOptions.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlow.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/Vector/IR/VectorOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "polygeist/Dialect.h"

namespace mlir {
class PatternRewriter;
class RewritePatternSet;
class DominanceInfo;

namespace carts::arts {
class RuntimeConfig;

/// Eliminate dead ARTS operations and shared dead helper IR.
std::unique_ptr<Pass> createDCEPass();

/// EDT and loop-structure transformation passes.
std::unique_ptr<Pass> createSdeStorageToArtsDbPass();
std::unique_ptr<Pass> createSdeAccessesToArtsDepsPass();
std::unique_ptr<Pass> createFinalizeSdeToArtsPass();
std::unique_ptr<Pass> createCreateDbsPass();
std::unique_ptr<Pass> createDbModeTighteningPass(bool forceInout = false);
std::unique_ptr<Pass> createDbScratchEliminationPass();
std::unique_ptr<Pass> createDbDistributedOwnershipRealizationPass();
std::unique_ptr<Pass> createDbDistributedRuntimeInitPass();
std::unique_ptr<Pass> createDbCommitDistributedDepsPass();
std::unique_ptr<Pass> createDbConsolidateStencilHalosPass();
std::unique_ptr<Pass> createDbStorageBridgeCopyPlacementPass();
std::unique_ptr<Pass> createDbShortenLifetimesPass();
std::unique_ptr<Pass> createDbDeadRootEliminationPass();
std::unique_ptr<Pass> createPartialReductionSplitPass();
std::unique_ptr<Pass> createBlockContractionSplitPass();
std::unique_ptr<Pass> createDistributedLaunchConsistencyPass();
/// Realize EDT distribution facts (family, version, block-halo capability)
/// from the committed dep pattern and access-window facts.
std::unique_ptr<Pass> createRealizeEdtDistributionPass();
std::unique_ptr<Pass> createCreateEpochsPass();

/// EDT-local cleanup and ARTS object refinement passes.
std::unique_ptr<Pass> createEdtAllocaSinkingPass();
std::unique_ptr<Pass> createEdtDeadDepEliminationPass();
std::unique_ptr<Pass> createEdtInlineNoDepTasksPass();
std::unique_ptr<Pass> createEdtPtrRematerializationPass();

/// Amortize committed repeated-timestep epoch loops.
std::unique_ptr<Pass> createEpochAmortizeRepeatedLoopPass();
std::unique_ptr<Pass> createEpochTailContinuationPass();
std::unique_ptr<Pass> createHoistingPass();

/// Verification passes at lowering boundaries.
std::unique_ptr<Pass> createVerifyArtsObjectsOnlyPass();
std::unique_ptr<Pass> createVerifyArtsCdagPass();
} // namespace carts::arts
} // namespace mlir

namespace mlir {
/// Forward declaration from Dialect.h
template <typename ConcreteDialect>
void registerDialect(DialectRegistry &registry);

namespace omp {
class OpenMPDialect;
} // namespace omp

namespace memref {
class MemRefDialect;
} // namespace memref

namespace LLVM {
class LLVMDialect;
} // namespace LLVM

namespace func {
class FuncDialect;
} // namespace func

namespace arith {
class ArithDialect;
} // namespace arith

namespace polygeist {
class PolygeistDialect;
} // namespace polygeist

// ARTS passes (generated from include/carts/passes/Passes.td)
#define GEN_PASS_DECL
#define GEN_PASS_REGISTRATION
#include "carts/passes/Passes.h.inc"

} // namespace mlir

// Per-dialect pass declarations (self-contained headers with dialect deps)
#include "carts/dialect/arts-rt/Transforms/Passes.h"
#include "carts/dialect/sde/Transforms/Passes.h"

// Per-dialect pass registrations (need create functions visible from above)
#define GEN_PASS_REGISTRATION
#include "carts/dialect/arts-rt/Transforms/Passes.h.inc"

namespace mlir::carts::sde {
#define GEN_PASS_REGISTRATION
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#endif /// CARTS_PASSES_PASSES_H
