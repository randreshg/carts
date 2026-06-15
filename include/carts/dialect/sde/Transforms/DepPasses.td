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
  let summary = "[DEPRECATED] Atomic per-array BLOCK layout assignment (use the "
                "sde-layout-candidate-choose -> sde-writer-layout-commit pair)";
  let description = [{
    DEPRECATED. The production layout engine is now the two-pass sequence
    `sde-layout-candidate-choose` (decide+commit the chosen logical layout) ->
    `sde-writer-layout-commit` (consume+realize it). This pass is the legacy
    single-pass atomic driver, retained only so existing pipelines and lit RUN
    lines that name `sde-layout-assignment` keep resolving. It runs PhaseA-PhaseD
    atomically and produces IR byte-identical to the two-pass sequence. New
    pipelines must run the pair.

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
  let summary = "Pass 1: choose per-array owner layouts and commit the choice "
                "as an authoritative SDE fact";
  let description = [{
    Pass 1 of the SDE per-array BLOCK layout engine (PhaseA-PhaseC). It promotes
    owner loops, builds module access relations, and for every external array
    root enumerates candidate element-space layouts (block-parallel owner-dim,
    block-contraction, replicated) from the affine access relations and
    structurally chooses one owner layout (owner-computes; HPF DISTRIBUTE/ALIGN).

    It then COMMITS the chosen logical layout per array — owner dims, block
    shape, and the disagreeing readers it implies — as a real authoritative SDE
    choice fact for `sde-writer-layout-commit` to consume. This is a committed
    decision, not an uncommitted candidate: pass 2 realizes it, never repairs or
    re-chooses it. It performs no per-SU writer/reader/physical realization.
    Pattern-free; names no communication operation.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createLayoutCandidateChoosePass()";
}

def WriterLayoutCommit
    : Pass<"sde-writer-layout-commit", "mlir::ModuleOp"> {
  let summary = "Pass 2: consume the committed layout choice and realize the "
                "per-SU writer/reader/physical layout facts";
  let description = [{
    Pass 2 of the SDE per-array BLOCK layout engine (PhaseD). It CONSUMES the
    authoritative chosen-layout fact committed by `sde-layout-candidate-choose`
    and realizes it: it reconciles per-SU writer/reader geometry over the
    committed choice (witnessedWriterOwnerPositions, inferReaderRequiredLayout)
    and writes the PhaseD facts on BOTH the writer and every disagreeing reader
    scheduling unit plus the writer physical MU-type layouts, then erases the
    now-realized choice fact.

    It does not re-choose or re-derive a different layout — it consumes,
    realizes, or rejects the committed fact. The net IR after the pair equals the
    deprecated atomic `sde-layout-assignment` exactly. When invoked standalone
    (no committed choice fact present, e.g. a textual single-pass run), it falls
    back to the self-contained atomic driver. It is pattern-free, names no
    communication operation, and knows nothing about target objects or
    placement; later SDE passes consume the committed layout facts.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createWriterLayoutCommitPass()";
}

#endif // CARTS_DIALECT_SDE_TRANSFORMS_DEP_PASSES
