///==========================================================================///
/// File: Passes.td
///
/// Pass declarations for the SDE (Structured Decomposition Environment)
/// dialect.
///==========================================================================///
#ifndef ARTS_DIALECT_SDE_TRANSFORMS_PASSES
#define ARTS_DIALECT_SDE_TRANSFORMS_PASSES

include "mlir/Pass/PassBase.td"

def RaiseMemrefToTensor : Pass<"raise-memref-to-tensor", "mlir::ModuleOp"> {
  let summary = "Lift SDE memref-fallback tasks into cu_codelet tensor form.";
  let description = [{
    See docs/compiler/raise-memref-to-tensor-rfc.md. Converts raisable
    cu_task ops whose bodies read/write shared memrefs into cu_codelet
    ops whose bodies consume !sde.token operands, threading the affected
    memrefs as tensor SSA through the enclosing SDE regions.

    This is a scaffold: v1 implementation is a follow-up.
  }];
  let dependentDialects = ["::mlir::tensor::TensorDialect",
                           "::mlir::bufferization::BufferizationDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::arts::sde::createRaiseMemrefToTensorPass()";
}

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

def TensorOpt : Pass<"tensor-opt", "mlir::ModuleOp"> {
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
  let constructor = "mlir::arts::sde::createTensorOptPass()";
}

def StructuredSummaries
    : Pass<"structured-summaries", "mlir::ModuleOp"> {
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
  let constructor = "mlir::arts::sde::createStructuredSummariesPass()";
}

def ElementwiseFusion
    : Pass<"elementwise-fusion", "mlir::ModuleOp"> {
  let summary = "Fuse consecutive SDE elementwise scheduling units";
  let description = [{
    Fuses consecutive sibling `arts_sde.su_iterate` operations that already
    carry an SDE-owned `classification(<elementwise>)`, share the same
    iteration space and scheduling policy, and write disjoint destinations.
    The fused scheduling unit is reclassified as
    `classification(<elementwise_pipeline>)`, keeping elementwise pipeline
    ownership on the SDE side of the boundary instead of inventing that family
    later on ARTS IR.
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect"];
  let constructor = "mlir::arts::sde::createElementwiseFusionPass()";
}

def DistributionPlanning
    : Pass<"distribution-planning", "mlir::ModuleOp"> {
  let summary = "Derive SDE-owned distribution intent from structured SDE loops";
  let description = [{
    Uses SDE-owned structured classification, concurrency scope, and the active
    SDE cost model to wrap eligible `arts_sde.su_iterate` operations in
    `arts_sde.su_distribute`. The current implementation is deliberately
    narrow: it only authors a blocked advisory for local elementwise loops and
    an owner-compute advisory for distributed stencil loops with enough work to
    amortize distribution.

    `ConvertSdeToArts` is responsible for materializing the advisory as ARTS
    distribution attrs at the lowering boundary.
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect"];
  let constructor = "mlir::arts::sde::createDistributionPlanningPass()";
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

def ChunkOpt : Pass<"chunk-opt", "mlir::ModuleOp"> {
  let summary = "Fill missing SDE chunk sizes from the cost model";
  let description = [{
    Uses the SDE-owned cost model to synthesize chunk sizes for eligible
    sde.su_iterate operations when the source program left them unspecified.
    The current implementation handles one-dimensional dynamic/guided loops
    with constant trip counts and preserves explicit source chunk sizes.
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect",
                           "::mlir::arith::ArithDialect"];
  let constructor = "mlir::arts::sde::createChunkOptPass()";
}

def ScheduleRefinement
    : Pass<"schedule-refinement", "mlir::ModuleOp"> {
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
  let constructor = "mlir::arts::sde::createScheduleRefinementPass()";
}

def ReductionStrategy : Pass<"reduction-strategy", "mlir::ModuleOp"> {
  let summary = "Select an SDE reduction strategy from the active cost model";
  let description = [{
    Uses the SDE-owned cost model to annotate eligible reduction-bearing
    sde.su_iterate operations with a reduction strategy recommendation.
    The current implementation is deliberately narrow: it only handles
    single-reduction loops with preserved reduction kinds, chooses between
    `atomic` and `tree`, and leaves lowering behavior unchanged.
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect"];
  let constructor = "mlir::arts::sde::createReductionStrategyPass()";
}

def ScopeSelection : Pass<"scope-selection", "mlir::ModuleOp"> {
  let summary = "Select SDE concurrency scope from the active cost model";
  let description = [{
    Uses the SDE-owned cost model to choose the concurrency scope for
    sde.cu_region <parallel> operations. The current implementation maps the
    active machine topology directly: single-node builds become `scope(<local>)`
    and multi-node builds become `scope(<distributed>)`.
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect"];
  let constructor = "mlir::arts::sde::createScopeSelectionPass()";
}

def IterationSpaceDecomposition
    : Pass<"iteration-space-decomposition", "mlir::ModuleOp"> {
  let summary = "Decompose scheduling-unit iteration spaces into interior and boundary regions";
  let description = [{
    Splits scheduling-unit inner loops that mix interior and boundary
    computations behind control flow into separate loops: a branch-free
    interior loop suitable for tiling and distribution, plus dedicated
    boundary loops that preserve the original edge behavior.

    The current implementation handles the neighborhood boundary pattern
    where row/column boundary checks guard stencil-like inner loops.
    Future decompositions (e.g., triangular peeling, remainder splitting)
    can be added as additional matchers within this pass.
  }];
  let dependentDialects = ["::mlir::func::FuncDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::scf::SCFDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::arts::sde::createIterationSpaceDecompositionPass()";
}

def BarrierElimination
    : Pass<"barrier-elimination", "mlir::ModuleOp"> {
  let summary = "Eliminate redundant SDE barriers between independent scheduling units";
  let description = [{
    Analyzes read and write sets of consecutive sde.su_iterate operations
    separated by sde.su_barrier ops. When the write-set of the predecessor
    loop is provably disjoint from the read-set of the successor loop, the
    barrier is marked for elimination. ConvertSdeToArts skips generating
    arts.barrier for eliminated barriers.
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::arts::sde::createBarrierEliminationPass()";
}

def LoopInterchange
    : Pass<"loop-interchange", "mlir::ModuleOp"> {
  let summary = "Cost-model-driven loop interchange using SDE linalg carriers";
  let description = [{
    Analyzes the indexing maps and iterator types of transient linalg.generic
    carriers to determine optimal loop ordering for cache efficiency. For
    matmul patterns, moves the reduction dimension outermost so the parallel
    dimension with stride-1 access on the B operand becomes innermost.

    This pass replaces the ARTS-level LoopReordering pass with a more general
    solution that operates on structured SDE carriers before ARTS lowering.
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect",
                           "::mlir::linalg::LinalgDialect",
                           "::mlir::scf::SCFDialect"];
  let constructor = "mlir::arts::sde::createLoopInterchangePass()";
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

def VerifySdeLowered : Pass<"verify-sde-lowered", "mlir::ModuleOp"> {
  let summary = "Verify no SDE operations survive past SDE-to-ARTS conversion";
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect"];
  let constructor = "mlir::arts::sde::createVerifySdeLoweredPass()";
}

#endif // ARTS_DIALECT_SDE_TRANSFORMS_PASSES
