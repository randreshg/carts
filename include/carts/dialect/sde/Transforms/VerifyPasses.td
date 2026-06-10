///==========================================================================///
/// Verify passes — post-lowering verification barriers
///==========================================================================///
#ifndef ARTS_DIALECT_SDE_TRANSFORMS_VERIFY_PASSES
#define ARTS_DIALECT_SDE_TRANSFORMS_VERIFY_PASSES

include "mlir/Pass/PassBase.td"

def VerifySdeLowered : Pass<"verify-sde-lowered", "mlir::ModuleOp"> {
  let summary = "Verify no SDE operations survive past boundary conversion";
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createVerifySdeLoweredPass()";
}

def VerifySdePartitionPlan : Pass<"verify-sde-partition-plan", "mlir::ModuleOp"> {
  let summary = "Verify committed SDE CU/MU partition plans before boundary conversion";
  let description = [{
    Pre-window gate for the committed CU/MU partition plan
    (`partition_graph` + `partition_score`). It runs after the physical-plan
    consistency gate and before the rank-expand/window chain consumes the plan.
    For every `sde.su_iterate` that carries committed partition evidence it
    checks that the evidence stays runtime-neutral (only SDE CU/MU geometry,
    abstract cost, and concurrency keys/values -- no collectives, DBs, routes,
    or runtime policy), that a committed physical plan is present and realizable
    by the current SDE loop rank, and that each primary-MU (owner_block) edge's
    `ownerDims`/`blockShape` still mirrors the committed `physicalOwnerDims`/
    `physicalBlockShape`. The physical plan it mirrors is in turn proved against
    the rank-expanded MU structure by `verify-sde-mu-layout`, so the partition
    graph is a verifier-checked mirror of real structure, not a free attr.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createVerifySdePartitionPlanPass()";
}

def VerifySdePhysicalConsistency
    : Pass<"verify-sde-physical-consistency", "mlir::ModuleOp"> {
  let summary = "Fail closed on a committed SDE physical plan that disagrees "
                "with its arrayLayout, SU schedule, or CU body";
  let description = [{
    The pre-window physical-plan/body consistency gate. Before the rank-expand
    and access-window transforms turn a committed physical plan into real MU/CU/SU
    structure, this verifier rejects the stale-grain shape the boundary forbids:
    one `sde.su_iterate` carrying several incompatible truths at once (the
    jacobi-for class). For every `sde.su_iterate` that carries a committed
    physical plan (`physicalOwnerDims` + `physicalBlockShape`) it checks:

      (a) PLAN WELL-FORMEDNESS: every `physicalOwnerDims` entry indexes a
          `physicalBlockShape` dimension and every block extent is positive.

      (b) SCHEDULE CONSISTENCY: the plan names no more owner dimensions than the
          SU has realized loop dimensions.

      (c) BUDGET AGREEMENT (the jacobi-for-class catcher): for every written
          block-parallel array carrying a node-agnostic `budgetBlockShape`, no
          `physicalBlockShape` extent on that array's own owner dimensions is
          COARSER than the budget grain. The cost-model physical tile may refine
          the budget (finer or equal) but may never coarsen past it. A row strip
          (`physicalBlockShape` `[1280, 10240]`) stamped over a 2-D `[512, 512]`
          owner-tile budget fails here. The physical owner-tile dims (a CU
          compute-grain decision) need NOT equal an array's storage owner dims --
          DB/MU grain is kept separate from CU grain -- so only the budget
          relation is enforced.

    Residual global-index access against an already rank-expanded MU is the
    companion `verify-sde-mu-layout` gate's job; flat owner-tile bodies CODIR
    still localizes from committed SU-local bounds are contract-correct, so this
    gate does not re-derive body locality.

    It reads the committed facts and current IR only through generated ODS
    accessors and the shared `parseArrayLayoutFacts` view; it stamps nothing and
    recomputes no owner dims or block shape. Out-of-scope SUs (no physical plan)
    are skipped.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor =
      "mlir::carts::sde::createVerifySdePhysicalConsistencyPass()";
}

def VerifySdeMuLayout : Pass<"verify-sde-mu-layout", "mlir::ModuleOp"> {
  let summary = "Verify the structural block-grid carrier on sde.mu_alloc";
  let description = [{
    Checks the structural block-grid carrier invariants:
      * no stale logical-rank access (or rank-reducing view) against a converted
        rank-expanded block-grid MU, and
      * `ownerDims == recover(structure)` — the owner dim recovered purely from
        a converted MU's expanded memref type equals the committed
        `physicalOwnerDims` (the structure/committed-plan round-trip proof).
    Conservative (flat) MUs and out-of-scope plans are skipped.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::sde::createVerifySdeMuLayoutPass()";
}

def VerifySdeMuAccessWindow
    : Pass<"verify-sde-mu-access-window", "mlir::ModuleOp"> {
  let summary = "Verify per-CU MU access-window coverage and consistency";
  let description = [{
    Checks the access-window invariants (the companion of
    `verify-sde-mu-layout`, which proves the underlying access-arity soundness —
    chain both for end-to-end coverage):

      R1 — coverage: every in-scope MU (a converted single-owner block-grid
           MU read or written by exactly one elementwise/stencil CU, not in
           place) has EXACTLY ONE `sde.mu_access_window` in its enclosing
           `sde.cu_region`. Zero means `raise-to-mu-access-window` did not run;
           more than one means the raiser's idempotency guard is broken.

      R2 — window within allocation and consistent with the committed grain: for
           every `sde.mu_access_window`, the block range is within the expanded
           MU grid and `validExtents` within the tile dims, and `blockHi` is the
           `ceilDiv` of a real iteration extent on the writer `sde.su_iterate`
           (an INDEPENDENT fact — never re-derived from the same window), so the
           window cannot silently encode a recomputed grain.

    Conservative (out-of-scope) MUs are skipped, never rejected.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::sde::createVerifySdeMuAccessWindowPass()";
}

def VerifySdeMuAccessWindowSync
    : Pass<"verify-sde-mu-access-window-sync", "mlir::ModuleOp"> {
  let summary = "Verify SU ordering is justified by MU access windows";
  let description = [{
    Checks that each surviving `sde.su_barrier` orders a real dependency between
    its before/after CU phases, read off their raised `sde.mu_access_window`
    facts. On every shared MU root (half-open block ranges per owner dim):

      - JUSTIFIED: a write on one side overlaps an access on the other and any
        read stays within the written blocks (aligned RAW/WAW/WAR) — accept.
      - REDUNDANT: every shared MU is touched on disjoint blocks or read-only on
        both sides, and the phases are fully described by windows — the barrier
        is avoidable.
      - MISALIGNED: a consumer reads blocks of a shared MU the producer does not
        write — the data must move; that redistribution is a later SDE transform,
        not an ordering. Diagnosed, not rewritten.

    Fail-closed when windows are present but the ordering cannot be proven
    (inconsistent owner-dim rank, or an adjacent access no window describes).
    Out of scope (no windows on either side of a barrier) is skipped; window
    coverage itself is `verify-sde-mu-access-window`'s job. Unlike effect-based
    barrier elimination, which removes barriers from structured memory effects,
    this proves justification from raised block-grid windows. Reads current IR
    only and mutates nothing.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor =
      "mlir::carts::sde::createVerifySdeMuAccessWindowSyncPass()";
}

def VerifySdeCoarseAvoidance
    : Pass<"verify-sde-coarse-avoidance", "mlir::ModuleOp"> {
  let summary = "Fail closed on coarse MU shape that is not block-partitioned";
  let description = [{
    Companion of `sde-coarse-avoidance` — the fail-closed gate that makes coarse a
    diagnosed last resort, never silent. For every `sde.mu_alloc` not already
    block-partitioned (no block grid in its type), it fails closed with a reason:

      * in the single-contiguous-owner block-grid realize scope (a committed plan
        proving a real grid) yet left flat — an AVOIDABLE coarse MU
        `sde-coarse-avoidance` should have rank-expanded; leaving it coarse would
        force a coarse single-writer DB the ARTS CDAG cannot split into parallel
        EDT waves;
      * otherwise an unsupported last resort (dynamic, in-place, reduction,
        matmul, multi-owner, aliasing use) that cannot be block-partitioned in the
        current IR — surfaced with evidence instead of silently.

    An MU with no committed plan and no coarse-relevant structure (e.g. private
    scratch) is legitimately non-distributed and accepted. The verifier shares the
    realize gate with rank expansion and the pass and reads the committed plan
    verbatim; it never recomputes owner dims or block shape.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::sde::createVerifySdeCoarseAvoidancePass()";
}

def VerifySdeRedistribute
    : Pass<"verify-sde-redistribute", "mlir::ModuleOp"> {
  let summary = "Fail closed unless layout-disagreement edges are redistributed";
  let description = [{
    Companion of `sde-redistribute` — the fail-closed gate that keeps a committed
    redistribution edge from being dropped or invented:

      * COMPLETENESS: every committed `layoutsDisagree` edge is either
        represented by a matching `sde.redist` op or is an edge that is not
        representable and was left coarse — both are errors, so a movement edge
        never silently vanishes (a coarse fallback the ARTS CDAG cannot split).
      * GROUNDING: every `sde.redist` op must match a committed redistribution
        edge re-derived independently from the `arrayLayout`/`layoutsDisagree`
        facts — for the representable reduction edge that means source == target
        == the committed home and the family is the consumer's grounded
        reduction — so no endpoint or family is invented.

    It reads the committed facts verbatim through the shared `assignStableArrayIds`
    numbering; it never recomputes owner dims, block shape, or the family.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::sde::createVerifySdeRedistributePass()";
}

def VerifySde : Pass<"verify-sde", "mlir::ModuleOp"> {
  let summary = "Verify SDE MU/CU/SU structural legality at the SDE boundary";
  let description = [{
    Structural verifier for the SDE boundary. It enforces that:

    - all source executable work lives inside a CU (sde.cu_work / cu_region /
      cu_task / cu_reduce / cu_atomic); an `sde.su_iterate` body directly
      contains only CUs, `sde.su_barrier`, and `sde.yield`, while an
      `sde.su_distribute` body directly contains only nested SUs, `sde.redist`,
      or `sde.su_barrier`; raw scf/source compute is legal under SDE only inside
      a CU;
    - SDE carries no target dependency graph: no consumed sde.mu_dep dependency
      graph and no generic token/dataflow
      dependency graph (token-carrying sde.su_barrier / consumed
      sde.control_token);
    - CUs are async/schedulable by default — sibling CUs with a provable MU
      access conflict must be ordered explicitly (SU sequence/parallel-wave
      structure, source effect/control ordering, or a last-resort plain
      sde.su_barrier), not by textual position alone.

    It deliberately does NOT require CU `IsolatedFromAbove`, an `sde.mu_token`,
    a slice op, or any specific access-window carrier; isolation is a CODIR
    concern and access windows are raised later by RaiseToMuAccessWindow. The
    pass reads only current IR and local op structure; it stamps no metadata
    and branches on no downstream contract.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createVerifySdePass()";
}

#endif // ARTS_DIALECT_SDE_TRANSFORMS_VERIFY_PASSES
