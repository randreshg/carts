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

def VerifySdeMuLayout : Pass<"verify-sde-mu-layout", "mlir::ModuleOp"> {
  let summary = "Verify the structural block-grid carrier on sde.mu_alloc";
  let description = [{
    Checks the structural block-grid carrier invariants:
      * no stale logical-rank access (or rank-reducing view) against a converted
        rank-expanded block-grid MU.

    Conservative (flat) MUs and out-of-scope layouts are skipped.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::sde::createVerifySdeMuLayoutPass()";
}

def VerifySdeMuAccessWindowSync
    : Pass<"verify-sde-mu-access-window-sync", "mlir::ModuleOp"> {
  let summary = "Verify SU ordering is justified by MU access windows";
  let description = [{
    Checks that each surviving `sde.su_barrier` orders a real dependency between
    its before/after CU phases, read off query-derived MU access windows from
    in-CU memory ops. On every shared MU root (half-open block ranges per owner dim):

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
    Out of scope (no query-derived windows on either side of a barrier) is skipped;
    MU layout soundness is `verify-sde-mu-layout`'s job. Unlike effect-based
    barrier elimination, which removes barriers from structured memory effects,
    this proves justification from query-derived block-grid windows. Reads current IR
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

      * in the single-contiguous-owner block-grid realize scope (committed facts
        proving a real grid) yet left flat — an AVOIDABLE coarse MU
        `sde-coarse-avoidance` should have rank-expanded; leaving it coarse would
        force a coarse single-writer DB the ARTS CDAG cannot split into parallel
        EDT waves;
      * otherwise an unsupported last resort (dynamic, in-place, reduction,
        matmul, multi-owner, aliasing use) that cannot be block-partitioned in the
        current IR — surfaced with evidence instead of silently.

    An MU with no committed facts and no coarse-relevant structure (e.g. private
    scratch) is legitimately non-distributed and accepted. The verifier shares the
    realize gate with rank expansion and the pass and reads the committed facts
    verbatim; it never recomputes owner dims or block shape.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::sde::createVerifySdeCoarseAvoidancePass()";
}

def VerifySde : Pass<"verify-sde", "mlir::ModuleOp"> {
  let summary = "Verify SDE MU/CU/SU structural legality at the SDE boundary";
  let description = [{
    Structural verifier for the SDE boundary. It enforces that:

    - all source executable work lives inside a CU. The owning SDE op verifiers
      enforce direct SU body shape: `sde.su_iterate` directly contains only
      direct-boundary CUs, `sde.array_layout_root`, `sde.su_barrier`, and
      `sde.yield`, while `sde.su_distribute` directly contains only nested SUs,
      `sde.su_halo`, `sde.su_reduce_scatter`, or `sde.su_barrier`; raw scf/source compute is legal under
      SDE only inside a CU;
    - SDE carries no target dependency graph: no consumed sde.mu_dep dependency
      graph and no generic token/dataflow
      dependency graph (token-carrying sde.su_barrier / consumed
      sde.control_token);
    - CU ops are executable bodies, not scheduling scopes. Sibling CU work with
      a provable MU access conflict must be ordered explicitly (SU
      sequence/parallel-wave structure, source effect/control ordering, or a
      last-resort plain sde.su_barrier), not by textual position alone.

    It deliberately does NOT require CU `IsolatedFromAbove`, an `sde.mu_token`,
    a slice op, or any specific access-window carrier; isolation is a ARTS
    concern and access windows are derived on demand by queryAccessWindows. The
    pass reads only current IR and local op structure; it writes no metadata
    and branches on no downstream promise.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createVerifySdePass()";
}

#endif // ARTS_DIALECT_SDE_TRANSFORMS_VERIFY_PASSES
