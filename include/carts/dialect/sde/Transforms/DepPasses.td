///==========================================================================///
/// Dep passes — structural transforms (loop interchange, tiling, fusion)
///==========================================================================///
#ifndef CARTS_DIALECT_SDE_TRANSFORMS_DEP_PASSES
#define CARTS_DIALECT_SDE_TRANSFORMS_DEP_PASSES

include "mlir/Pass/PassBase.td"

def LoopInterchange
    : Pass<"loop-interchange", "mlir::ModuleOp"> {
  let summary = "Cost-model-driven loop interchange using SDE memref access analysis";
  let description = [{
    Analyzes SDE loop/access maps to determine optimal loop ordering for cache
    efficiency. For matmul patterns, moves the reduction dimension outermost
    so the parallel dimension with stride-1 access on the B operand becomes
    innermost.

    This pass replaces target-level loop reordering with a more general
    solution that operates on structured SDE memref loops before boundary
    conversion.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::scf::SCFDialect"];
  let constructor = "mlir::carts::sde::createLoopInterchangePass()";
}

def Tiling : Pass<"tiling", "mlir::ModuleOp"> {
  let summary = "Apply SDE pattern-driven tiling using memref loop/access facts";
  let description = [{
    Uses SDE-owned pattern/effect facts, abstract logical capacity, and the
    memref loop/access facts to apply real loop tiling before boundary
    conversion. Symbolic capacity is represented with
    `sde.resource_query <logical_workers>` and remains target-neutral in SDE.
    The current implementation focuses on the first executable subset:
    one-dimensional elementwise loops are strip-mined into logical-capacity
    tiles and owner-local elementwise pipelines are coarsened with the SDE cost
    model so SDE compute units amortize local pipeline stages before boundary
    lowering.
    The surrounding sde.su_iterate step is rewritten so the transformed loop
    nest survives into later lowering.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::scf::SCFDialect"];
  let constructor = "mlir::carts::sde::createTilingPass()";
}

def Parallelize : Pass<"sde-parallelize", "mlir::ModuleOp"> {
  let summary = "Parallelize + raise embarrassingly-parallel sequential loop nests to SDE scheduling units";
  let description = [{
    Detects perfectly-nested rectangular `scf.for` nests that are provably
    dependence-free (one or more external array stores at indices == the loop
    IVs, no read of a written array, read-only array inputs allowed, pure body).
    It also recognizes closed-form scalar induction counters whose final mutable
    state is not observed. The pass raises eligible raw or normalized
    leaf `sde.cu_region <single>` loop nests to sibling `sde.su_iterate`
    schedule structure before LayoutAssignment, so SDE exposes real ordering
    and work structure instead of a serial host whole-array CU. Fail-closed:
    unprovable nests are left untouched.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::scf::SCFDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::sde::createParallelizePass()";
}

def PatternAnalysis
    : Pass<"sde-pattern-analysis", "mlir::ModuleOp"> {
  let summary = "Analyze and stamp approved SDE structured patterns";
  let description = [{
    Uses SDE-owned loop/access analysis to identify approved structured pattern
    families on eligible `sde.su_iterate` operations. This pass is the first
    SDE pattern planning point after OpenMP-to-SDE conversion: later SDE
    transforms consume these facts instead of rediscovering pattern legality.

    PatternAnalysis stamps only SDE IR facts. It does not stamp target-specific
    attributes and it does not cross the dialect boundary. Later conversion may
    lower the final SDE plan, but raw pattern-analysis facts remain SDE
    ownership.

    The current implementation stamps:
    - `classification(<...>)` from the current SDE structured analysis
    - approved `pattern` values for uniform, stencil, wavefront, matmul,
      reduction, and elementwise-pipeline families
    - generic access/owner/footprint summaries for recognized ND neighborhood
      loops
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createPatternAnalysisPass()";
}

def LayoutAssignment
    : Pass<"sde-layout-assignment", "mlir::ModuleOp"> {
  let summary = "Module-scoped affine-driven per-array BLOCK layout assignment";
  let description = [{
    Assigns one element-space BLOCK layout to every external array root accessed
    by the module's `sde.su_iterate` scheduling units. This is the
    DATA-LAYOUT decision in the spirit of HPF DISTRIBUTE/ALIGN: it chooses owner
    (distributed) positions and per-block extents for each array from its affine
    access relations, minimizing an ABSTRACT communication-volume cost. It is
    pattern-free (driven only by affine maps, iterator types, and static
    shapes), names no communication operation, and knows nothing about target
    objects or placement.

    Four phases:
    - PhaseA builds module-wide per-array access profiles
      (`buildModuleSuAccessRelations`).
    - PhaseB enumerates candidate layouts per array (block-parallel owner-dim,
      block-contraction, replicated fallback).
    - PhaseC assigns one layout per array, seeding from the writer's owner dims
      (owner-computes) and minimizing total abstract `commVolumeBytes`.
    - PhaseD stamps the chosen layout as the `arrayLayout` attr on BOTH the
      writer and every reader scheduling unit (the key generalization to
      INPUTS), plus a per-edge `layoutsDisagree` geometric mismatch marker and
      an abstract `commVolumeBytes`.

    The attrs are planning inputs, not decorative breadcrumbs: SDE distribution
    planning consumes the abstract communication volume when choosing
    concurrency-preserving block shapes. Later planning uses the carried
    layout facts to select concrete storage and communication carriers without
    moving target policy into SDE.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createLayoutAssignmentPass()";
}

def ElementwiseFusion
    : Pass<"elementwise-fusion", "mlir::ModuleOp"> {
  let summary = "Fuse consecutive SDE elementwise scheduling units";
  let description = [{
    Fuses consecutive sibling `sde.su_iterate` operations that already
    carry an SDE-owned `classification(<elementwise>)`, share the same
    iteration space and scheduling policy, and write disjoint destinations.
    The fused scheduling unit is reclassified as
    `classification(<elementwise_pipeline>)`, keeping elementwise pipeline
    ownership on the SDE side of the boundary instead of inventing that family
    later in target IR.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createElementwiseFusionPass()";
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
  let constructor = "mlir::carts::sde::createIterationSpaceDecompositionPass()";
}

#endif // CARTS_DIALECT_SDE_TRANSFORMS_DEP_PASSES
