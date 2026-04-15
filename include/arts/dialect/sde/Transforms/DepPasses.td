///==========================================================================///
/// Dep passes — structural transforms (loop interchange, tiling, fusion)
///==========================================================================///
#ifndef ARTS_DIALECT_SDE_TRANSFORMS_DEP_PASSES
#define ARTS_DIALECT_SDE_TRANSFORMS_DEP_PASSES

include "mlir/Pass/PassBase.td"

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

def Tiling : Pass<"tiling", "mlir::ModuleOp"> {
  let summary = "Apply cost-model-driven tiling using SDE linalg carriers";
  let description = [{
    Uses the SDE-owned cost model together with the transient tensor-backed
    linalg carriers to apply real loop tiling before SDE->ARTS lowering.
    The current implementation focuses on the first executable subset:
    one-dimensional elementwise loops are strip-mined into cost-model-sized
    tiles and the surrounding sde.su_iterate step is rewritten so the
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
  let constructor = "mlir::arts::sde::createTilingPass()";
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

def Vectorization
    : Pass<"sde-vectorization", "mlir::ModuleOp"> {
  let summary = "Carrier-aware vectorization width analysis for su_iterate bodies";
  let description = [{
    Walks carrier-authoritative `linalg.generic` ops inside `su_iterate` and
    stamps `sde.vectorize_width` based on element type and static loop ranges.
    The actual vectorization is performed downstream by LoopVectorizationHints
    (in rt/) which reads this attr and emits LLVM vectorization metadata.

    Only analyzes carriers with all-parallel iterator types and static shapes.
    Defaults: f32→8, f64→4, i32→8, i64→4.
  }];
  let dependentDialects = ["::mlir::linalg::LinalgDialect",
                           "::mlir::tensor::TensorDialect"];
  let constructor = "mlir::arts::sde::createVectorizationPass()";
}

#endif // ARTS_DIALECT_SDE_TRANSFORMS_DEP_PASSES
