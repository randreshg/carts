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
                           "::mlir::scf::SCFDialect",
                           "::mlir::affine::AffineDialect"];
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
                           "::mlir::scf::SCFDialect",
                           "::mlir::affine::AffineDialect"];
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
  let summary = "[DEPRECATED] Per-array BLOCK layout assignment (use "
                "sde-writer-layout-commit)";
  let description = [{
    DEPRECATED alias of `sde-writer-layout-commit`, retained so existing
    pipelines and lit RUN lines that name `sde-layout-assignment` keep
    resolving. Identical full behavior: it runs PhaseA-PhaseD atomically. New
    pipelines should use `sde-writer-layout-commit` (and, when only the chosen
    owner layouts are wanted, `sde-layout-candidate-choose`).

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
    - PhaseC assigns one layout per array structurally from access profiles
      (owner-computes; no comm-volume score).
    - PhaseD writes the chosen layout as the `arrayLayout` attr on BOTH the
      writer and every reader scheduling unit, plus a per-edge
      `layoutsDisagree` geometric mismatch marker when reader/writer shapes
      disagree.

    These attrs are SDE layout facts, not decorative breadcrumbs: SDE
    distribution consumes the abstract communication volume when choosing
    concurrency-preserving block shapes. Later SDE passes use the carried layout
    facts to select concrete storage and movement carriers without moving target
    policy into SDE.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createLayoutAssignmentPass()";
}

def LayoutCandidateChoose
    : Pass<"sde-layout-candidate-choose", "mlir::ModuleOp"> {
  let summary = "Choose per-array owner layouts and commit writer home facts";
  let description = [{
    PhaseA-PhaseC of SDE per-array BLOCK layout assignment, plus the writer-side
    commit of PhaseD. For every external array root it enumerates candidate
    element-space layouts (block-parallel owner-dim, block-contraction,
    replicated) from the affine access relations and structurally chooses one
    owner layout (owner-computes; HPF DISTRIBUTE/ALIGN), then commits the chosen
    WRITER home layout as `arrayLayout` write facts (with the realizable
    owner-dim clamp) and writer physical MU-type layouts.

    It does NOT commit the reader-side transitional layouts; those are committed
    by `sde-writer-layout-commit`. This pass is the granular front half of the
    layout engine; the production pipeline runs the atomic
    `sde-writer-layout-commit`, which performs choice and reader/writer commit
    in a single run (no metadata-promise handoff). Pattern-free; names no
    communication operation.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createLayoutCandidateChoosePass()";
}

def WriterLayoutCommit
    : Pass<"sde-writer-layout-commit", "mlir::ModuleOp"> {
  let summary = "Module-scoped affine-driven per-array BLOCK layout assignment";
  let description = [{
    The SDE DATA-LAYOUT engine (HPF DISTRIBUTE/ALIGN). It chooses one
    element-space BLOCK layout per external array root and commits the layout
    facts atomically: it asks the candidate chooser (PhaseA-PhaseC) for each
    array's owner layout, reconciles per-SU writer/reader geometry
    (witnessedWriterOwnerPositions, inferReaderRequiredLayout), and writes
    PhaseD facts on BOTH the writer and every disagreeing reader scheduling unit
    plus the writer physical MU-type layouts.

    Choice and commit happen in one pass run: it does not stamp an
    uncommitted-candidate promise for a later pass to repair. It supersedes the
    deprecated `sde-layout-assignment` (identical behavior). It is pattern-free
    (driven only by affine maps, iterator types, and static shapes), names no
    communication operation, and knows nothing about target objects or
    placement; later SDE passes consume the committed layout facts.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createWriterLayoutCommitPass()";
}

#endif // CARTS_DIALECT_SDE_TRANSFORMS_DEP_PASSES
