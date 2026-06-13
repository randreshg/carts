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

def RaiseToSde : Pass<"sde-raise-to-sde", "mlir::ModuleOp"> {
  let summary = "Raise proven-independent sequential loop nests to bare SDE SU/CU skeletons";
  let description = [{
    CORE raise-to-sde pass (partial Step 11). Walks host `scf.for` nests that
    are not already under `sde.su_iterate` and promotes dependence-free nests to
    `sde.su_iterate` + proof-derived `sde.cu_region <parallel>` via
    `buildSuIterate` / `buildCuRegion` with zero optional attrs. Re-entrancy
    guard skips nests under committed SU/CU structure from OpenMP conversion.
    Residual serial wrapping and full CU-normalization are future sub-steps.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::scf::SCFDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::sde::createRaiseToSdePass()";
}

def SdeLoopPatternFacts
    : Pass<"sde-loop-pattern-facts", "mlir::ModuleOp"> {
  let summary = "[deprecated] Owner-loop promotion only (no fact stamping)";
  let description = [{
    Legacy pass retained for manual/test invocation. Production pipelines
    run owner-loop promotion inside `raise-to-sde` and query loop-access facts
    on demand via `SuLoopAccessAnalysis`. This pass performs rank-1→rank-N
    `su_iterate` rebuilds only; it stamps zero classification/pattern/offset
    attributes.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createSdeLoopPatternFactsPass()";
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
      block-contraction, replicated layout).
    - PhaseC assigns one layout per array, seeding from the writer's owner dims
      (owner-computes) and minimizing total abstract `commVolumeBytes`.
    - PhaseD writes the chosen layout as the `arrayLayout` attr on BOTH the
      writer and every reader scheduling unit (the key generalization to
      INPUTS), plus a per-edge `layoutsDisagree` geometric mismatch marker and
      an abstract `commVolumeBytes`.

    These attrs are SDE layout facts, not decorative breadcrumbs: SDE
    distribution consumes the abstract communication volume when choosing
    concurrency-preserving block shapes. Later SDE passes use the carried layout
    facts to select concrete storage and movement carriers without moving target
    policy into SDE.
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
