///==========================================================================///
/// File: Passes.td
///
/// Pass declarations for the SDE (Structured Decomposition Environment)
/// dialect.
///==========================================================================///
#ifndef ARTS_DIALECT_SDE_TRANSFORMS_PASSES
#define ARTS_DIALECT_SDE_TRANSFORMS_PASSES

include "mlir/Pass/PassBase.td"

def RaiseToLinalg : Pass<"raise-to-linalg", "mlir::ModuleOp"> {
  let summary = "Raise supported sde.su_iterate bodies to transient linalg.generic carriers";
  let description = [{
    Analyzes loop nests inside sde.su_iterate operations, stamps structural
    classification on the source loop, and materializes transient
    linalg.generic carriers for the currently supported subset of loop bodies.
    Unsupported bodies remain in loop form for downstream passes to handle
    via independent IR analysis.

    Recognized patterns:
    - elementwise: all parallel iterators, identity/permutation indexing maps
    - matmul: triple-nested with multiply-accumulate and reduction dimension
    - stencil: parallel iterators with constant-offset indexing maps
    - reduction: has reduction iterators with accumulation pattern

    The pass also stamps an SDE-owned classification on the source loop so
    SDE->ARTS can recover contracts even when the loop stays on the fallback
    path.

    Today, only the elementwise/matmul-style subset is materialized as a
    linalg.generic carrier. Stencil/reduction bodies currently stay on the
    stamped-attribute fallback path.

    ConvertSdeToArts consumes the transient linalg.generic ops, stamps pattern
    contracts on the created arts.for, and erases the carriers so downstream
    passes continue on loop/memref IR.
  }];
  let dependentDialects = ["::mlir::memref::MemRefDialect",
                           "::mlir::func::FuncDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::linalg::LinalgDialect",
                           "::mlir::scf::SCFDialect"];
  let constructor = "mlir::arts::sde::createRaiseToLinalgPass()";
}

def RaiseToTensor : Pass<"raise-to-tensor", "mlir::ModuleOp"> {
  let summary = "Rewrite transient SDE linalg carriers to tensor-backed form";
  let description = [{
    Rewrites transient memref-backed linalg.generic carriers produced by
    RaiseToLinalg into tensor-backed carriers so later SDE passes can apply
    tensor/linalg transformations without exposing tensor IR to ARTS-lowering
    passes. The source loop body remains in place and the tensor carriers stay
    transient: ConvertSdeToArts consumes them for contract recovery and erases
    the carrier chain afterwards.

    The current implementation keeps the rewrite deliberately narrow and valid:
    it converts ranked memref carriers only. Elementwise loops use tensor.empty
    destinations because every iteration fully overwrites the output element;
    reduction-bearing carriers keep output initialization via
    bufferization.to_tensor so the transient tensor semantics stay aligned with
    the source loop body.
  }];
  let dependentDialects = ["::mlir::bufferization::BufferizationDialect",
                           "::mlir::tensor::TensorDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::linalg::LinalgDialect"];
  let constructor = "mlir::arts::sde::createRaiseToTensorPass()";
}

def SdeTensorOptimization : Pass<"sde-tensor-optimization", "mlir::ModuleOp"> {
  let summary = "Apply cost-model-driven tensor optimizations in SDE";
  let description = [{
    Uses the SDE-owned cost model together with the transient tensor-backed
    linalg carriers to apply real loop transformations before SDE->ARTS
    lowering. The current implementation focuses on the first executable
    subset: one-dimensional elementwise loops are strip-mined into cost-model-
    sized tiles and the surrounding sde.su_iterate step is rewritten so the
    transformed loop nest survives into later ARTS lowering without exposing
    tensor IR past the SDE boundary.
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::bufferization::BufferizationDialect",
                           "::mlir::linalg::LinalgDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::scf::SCFDialect",
                           "::mlir::tensor::TensorDialect"];
  let constructor = "mlir::arts::sde::createSdeTensorOptimizationPass()";
}

def SdeStructuredSummaries
    : Pass<"sde-structured-summaries", "mlir::ModuleOp"> {
  let summary = "Stamp SDE-owned structured summaries";
  let description = [{
    Uses the SDE structured loop/access analysis to refresh reusable,
    runtime-neutral summaries on eligible `arts_sde.su_iterate` operations
    before `ConvertSdeToArts`. This keeps SDE responsible for structured loop
    semantics without stamping ARTS- or lowering-specific attributes on SDE IR.

    The current implementation refreshes:
    - `classification(<...>)` from the current SDE structured analysis
    - generic access/owner/footprint summaries for recognized neighborhood
      loops so the SDE->ARTS boundary can materialize the runtime-specific
      contract later
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect"];
  let constructor = "mlir::arts::sde::createSdeStructuredSummariesPass()";
}

def ConvertOpenMPToSde : Pass<"convert-openmp-to-sde", "mlir::ModuleOp"> {
  let summary = "Convert OpenMP operations to SDE operations";
  let description = [{
    Converts OpenMP ops into SDE ops, preserving information that the
    direct OMP-to-ARTS conversion loses:
    - Reduction combiner kind + identity
    - Nowait semantics
    - Schedule + chunk size
    - Task completion tokens
  }];
  let dependentDialects = ["::mlir::memref::MemRefDialect",
                           "::mlir::omp::OpenMPDialect",
                           "::mlir::arts::sde::ArtsSdeDialect",
                           "::mlir::arts::ArtsDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::scf::SCFDialect"];
  let constructor = "mlir::arts::sde::createConvertOpenMPToSdePass()";
}

def SdeChunkOptimization : Pass<"sde-chunk-optimization", "mlir::ModuleOp"> {
  let summary = "Fill missing SDE chunk sizes from the cost model";
  let description = [{
    Uses the SDE-owned cost model to synthesize chunk sizes for eligible
    sde.su_iterate operations when the source program left them unspecified.
    The current implementation handles one-dimensional dynamic/guided loops
    with constant trip counts and preserves explicit source chunk sizes.
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect",
                           "::mlir::arith::ArithDialect"];
  let constructor = "mlir::arts::sde::createSdeChunkOptimizationPass()";
}

def SdeScheduleRefinement
    : Pass<"sde-schedule-refinement", "mlir::ModuleOp"> {
  let summary = "Refine abstract SDE schedule hints into concrete schedules";
  let description = [{
    Uses the SDE-owned cost model to refine eligible `sde.su_iterate`
    schedule hints before later SDE optimizations run. The current
    implementation is deliberately narrow: it only handles one-dimensional
    loops with constant trip counts, only rewrites `auto` / `runtime`
    schedules, and chooses the lowest-overhead concrete policy among
    `static`, `dynamic`, and `guided`.
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect"];
  let constructor = "mlir::arts::sde::createSdeScheduleRefinementPass()";
}

def SdeReductionStrategy : Pass<"sde-reduction-strategy", "mlir::ModuleOp"> {
  let summary = "Select an SDE reduction strategy from the active cost model";
  let description = [{
    Uses the SDE-owned cost model to annotate eligible reduction-bearing
    sde.su_iterate operations with a reduction strategy recommendation.
    The current implementation is deliberately narrow: it only handles
    single-reduction loops with preserved reduction kinds, chooses between
    `atomic` and `tree`, and leaves lowering behavior unchanged.
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect"];
  let constructor = "mlir::arts::sde::createSdeReductionStrategyPass()";
}

def SdeScopeSelection : Pass<"sde-scope-selection", "mlir::ModuleOp"> {
  let summary = "Select SDE concurrency scope from the active cost model";
  let description = [{
    Uses the SDE-owned cost model to choose the concurrency scope for
    sde.cu_region <parallel> operations. The current implementation maps the
    active machine topology directly: single-node builds become `scope(<local>)`
    and multi-node builds become `scope(<distributed>)`.
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect"];
  let constructor = "mlir::arts::sde::createSdeScopeSelectionPass()";
}

def ConvertSdeToArts : Pass<"convert-sde-to-arts", "mlir::ModuleOp"> {
  let summary = "Convert SDE operations to ARTS operations";
  let description = [{
    Converts SDE dialect ops into ARTS dialect ops, producing the same IR
    that ConvertOpenMPToArts currently generates. When RaiseToLinalg
    materializes transient linalg.generic carriers, this pass derives the
    matching ARTS contract from them, copies semantic contract attributes, and
    erases the carriers so downstream passes continue on loop/memref IR. When
    no transient carrier is authoritative, it prefers any explicit SDE-owned
    semantic contract already stamped on `arts_sde.su_iterate`, then falls
    back to the structural SDE classification attribute.
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect",
                           "::mlir::arts::ArtsDialect",
                           "::mlir::bufferization::BufferizationDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::linalg::LinalgDialect",
                           "::mlir::tensor::TensorDialect"];
  let constructor = "mlir::arts::sde::createConvertSdeToArtsPass()";
}

#endif // ARTS_DIALECT_SDE_TRANSFORMS_PASSES
