///==========================================================================///
/// State passes — memref-state cleanup before SDE planning.
///==========================================================================///
#ifndef ARTS_DIALECT_SDE_TRANSFORMS_STATE_PASSES
#define ARTS_DIALECT_SDE_TRANSFORMS_STATE_PASSES

include "mlir/Pass/PassBase.td"

def PromoteTargetAttrs
    : Pass<"sde-promote-target-attrs", "mlir::ModuleOp"> {
  let summary = "Promote polygeist target-cpu/features to CARTS target attrs";
  let description = [{
    cgeist stamps `polygeist.target-cpu` and `polygeist.target-features` on
    the top-level module. Downstream conversion passes (notably the LLVM
    lowerings inside `llvm-ir-emission`) drop foreign module attrs, so any
    CARTS pass that reads them in the LLVM-facing stages observes a default
    x86-64 target and emits SSE-width vector hints on AVX/AVX-512 hosts.

    This pass copies the polygeist attrs into target-neutral
    `carts.target-cpu` / `carts.target-features` on the same module before
    any other CARTS pass runs. The CARTS attrs become the single source of
    truth consumers must read. The pass is a no-op when the polygeist attrs
    are absent and idempotent when the CARTS attrs are already present.
  }];
}

def ScalarForwarding : Pass<"scalar-forwarding", "mlir::ModuleOp"> {
  let summary = "Forward constant-initialized scalar memrefs across region boundaries";
  let description = [{
    Detects rank-0 memref.alloca ops initialized with a constant value and
    replaces loads inside nested regions (omp.parallel, cu_region, etc.) with
    the constant when all stores inside that region write the same value.

    This eliminates Polygeist's continuation-boolean pattern where a memref<i1>
    is used as a structured return guard, turning always-true scf.if conditions
    into foldable constants.
  }];
  let dependentDialects = ["::mlir::memref::MemRefDialect",
                           "::mlir::arith::ArithDialect"];
  let constructor = "mlir::carts::sde::createScalarForwardingPass()";
}

def SdeDeadStateCleanupPass
    : Pass<"sde-dead-state-cleanup", "mlir::ModuleOp"> {
  let summary = "Remove dead frontend helper IR before SDE planning";
  let description = [{
    Removes dead dialect-neutral helper IR exposed by SDE input
    normalization: unused memref loads, dead stack stores/allocas, no-op
    load/store self writes, unused pure leaf operations, and private symbols.

    This pass intentionally does not inspect target storage, task, placement,
    or lowering objects.
  }];
  let dependentDialects = ["::mlir::memref::MemRefDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::func::FuncDialect"];
}

def MemoryUnitMaterialization
    : Pass<"sde-memory-unit-materialization", "mlir::ModuleOp"> {
  let summary = "Materialize SDE shared memrefs as memory units";
  let description = [{
    Rewrites shared memref allocations used by SDE scheduling units into
    `sde.mu_alloc` storage before the dialect boundary. This pass is
    intentionally SDE-local: it creates target-neutral MU storage declarations
    and leaves conversion to concrete target objects to boundary lowering.

    Physical layout and owner-slice facts stay on the scheduling unit and are
    consumed at boundary conversion to form acquire windows for supported
    elementwise/matmul owner slices. Stencil, reduction, halo, and other
    complex blocked plans remain on the raw bridge until their SDE-local view
    rewrite exists. The pass does not rediscover target-object layouts or
    perform concrete index rewriting.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor =
      "mlir::carts::sde::createMemoryUnitMaterializationPass()";
}

def SdeRankExpandMu
    : Pass<"sde-rank-expand-mu", "mlir::ModuleOp"> {
  let summary = "Rank-expand committed block-grid MUs and rewrite CU accesses";
  let description = [{
    Structural owner-dim/grain carrier. For every `sde.mu_alloc` governed by a
    committed, single-contiguous-owner, fully-static elementwise/stencil BLOCK
    plan (`physicalOwnerDims`/`physicalBlockShape` on the writer
    `sde.su_iterate`), this pass makes the grain STRUCTURE rather than metadata:

      * the `sde.mu_alloc` result memref is rank-expanded so the block grid is a
        leading dim of the type (e.g. `memref<128x64xf64>` with owner dim 0 and
        block `[16,64]` becomes `memref<8x16x64xf64>`), and
      * every CU `memref.load`/`memref.store` on that MU is rewritten into the
        physical `[block, intra-block, ...]` coordinate system via div/mod
        localization.

    Owner dims become `recover(structure)` (see `verify-sde-mu-layout`); no
    owner-dim/block-shape attribute is added to the `mu_alloc`. Matmul,
    reduction, in-place, dynamic, and multi-owner plans are left in conservative
    flat form (or fail closed) — never papered over with an attribute promise or
    a compatibility attr. The pass does not recompute owner dims or block
    shape; it reads the committed plan verbatim.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::arith::ArithDialect"];
  let constructor = "mlir::carts::sde::createSdeRankExpandMuPass()";
}

def SdeCuNormalization
    : Pass<"sde-cu-normalization", "mlir::ModuleOp"> {
  let summary = "Wrap all non-CU source executable work in a conservative CU";
  let description = [{
    Structural CU-normalization. Wraps every run of source-executable work that
    is not already inside a CU — non-OpenMP host init loops, scalar
    verification/check code, sequential reductions, and scalar-effect work —
    into a conservative `sde.cu_region <single>`, so the v4 SDE boundary
    invariant "all source executable work lives in a CU" (verify-sde) holds for
    non-OpenMP work too.

    SU bodies become scheduling-only: raw scf / source compute directly under an
    SU is moved into a CU, leaving only schedule/window/barrier plumbing. Existing
    valid CUs are left unchanged.

    The pass is structural only: it adds no CU isolation, MU token, slice, or
    mu_dep graph, and introduces no CODIR concept. It makes no distribution,
    movement, DB-grain, or access-window decision; it reads current IR and
    rewrites structure, stamping no metadata and branching on no downstream
    contract.

    Conservative single-CU wrapping requires a self-contained span: if a value
    defined in the span is used outside it (an `sde.cu_region` result cannot
    express an output-only value), the pass fails closed with a diagnostic rather
    than fabricate a results/yield contract for a later layer to repair.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createSdeCuNormalizationPass()";
}

def SdeRaiseToMuAccessWindow
    : Pass<"raise-to-mu-access-window", "mlir::ModuleOp"> {
  let summary = "Raise canonical per-CU MU access windows over block-grid MUs";
  let description = [{
    Structural access-window carrier. After the final SDE shape rewrites, for
    every `sde.mu_alloc` rank-expanded into a single-contiguous-owner block-grid
    MU (elementwise/stencil, fully static), this pass raises one
    `sde.mu_access_window` fact at the top of the enclosing `sde.cu_region`, per
    (MU root, read/write mode), in the SAME block-grid coordinate system the
    carrier type already encodes.

    It is a pure ADDITIVE raiser: it reads the committed plan and the expanded
    type VERBATIM (never recomputing owner dims or block shape), proves the
    structure against the writer `sde.su_iterate` iteration domain, and emits
    explicit window structure. It introduces no `sde.mu_token`, slice,
    `sde.mu_dep`, or CODIR isolation concept, and stamps no downstream contract.

    Out-of-scope MUs are skipped conservatively (no window, no error): dynamic,
    matmul/reduction, multi-owner, in-place (read+write same MU in a CU),
    accesses spanning multiple CUs, and unsupported (non-load/store) uses. The
    pass never mutates existing ops; it only inserts window facts.

    Not part of the default `-O3` pipeline (see `verify-sde-mu-access-window`).
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::sde::createRaiseToMuAccessWindowPass()";
}

def SdeCoarseAvoidance
    : Pass<"sde-coarse-avoidance", "mlir::ModuleOp"> {
  let summary = "Avoid coarse MU allocation by realizing the committed grain";
  let description = [{
    Best-effort coarse-allocation avoidance over rank-expanded MU layout and
    access-window IR. For every `sde.mu_alloc`:

      * an MU already rank-expanded (the block grid is a leading dim of the
        memref type) already exposes the maximum legal independent blocks — skip;
      * an MU with a committed, single-contiguous-owner, fully-static
        elementwise/stencil BLOCK plan that proves a real grid is realized as
        structure via the shared block-grid realize gate and rewriter — the
        committed plan is consumed VERBATIM, never re-authored;
      * every other coarse MU is left flat. Whether a flat MU is an avoidable bug
        or a legitimate last resort (dynamic, in-place, reduction, matmul,
        multi-owner, aliasing) is the fail-closed diagnosis owned by
        `verify-sde-coarse-avoidance`, so coarse is never accepted silently.

    The only hard failure is a committed plan that cannot be realized end to end:
    rather than leave a partial owner-dim promise, the pass fails closed with
    evidence. It owns no redistribution and invents no coarse fallback. It
    introduces no `sde.mu_token`, slice, `sde.mu_dep`, owner map, DB, or CODIR
    concept. Its only mutation is the block-grid rewrite (in-place load/store
    rewrites that move no op across a CU/SU boundary), so CU legality is preserved.

    Not part of the default `-O3` pipeline (see `verify-sde-coarse-avoidance`).
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::arith::ArithDialect"];
  let constructor = "mlir::carts::sde::createSdeCoarseAvoidancePass()";
}

def SdeRedistribute
    : Pass<"sde-redistribute", "mlir::ModuleOp"> {
  let summary = "Make committed layout-disagreement edges explicit sde.redist";
  let description = [{
    Realizes the redistribution edges `sde-layout-assignment` records as the
    metadata-only `layoutsDisagree` marker into explicit `sde.redist` structure.
    The only edge currently representable from the committed facts is a
    cross-owner reduction read: when a consumer reduces a committed home's owned
    axis, the pass emits a `reduce_scatter_like` movement over that home block
    layout (source == target == the committed writer-role home; the movement is
    the reduction, not a re-layout). The home layout is read VERBATIM from the
    committed `arrayLayout` writer-role entry and the family from the consumer's
    grounded `reductionIndexed` access kind; owner dims, block shape, and family
    are never recomputed. The array root for an `arrayId` is recovered through the
    single `assignStableArrayIds` numbering shared with `sde-layout-assignment`.

    A re-layout edge (transpose/gather/halo) is NOT representable from the
    committed facts: it would need the consumer's required target layout, which
    `sde-layout-assignment` discards (it stamps the home plus a disagreement
    flag). The pass fails closed (diagnostic + signalPassFailure), inventing no
    movement, on every such edge and on a dynamic array shape, an in-place
    reader/writer, an aliasing array-root use, or a missing committed writer
    home. It introduces no `sde.mu_token`, slice, `sde.mu_dep`, owner map, DB,
    route, collective, or CODIR isolation concept.

    Not part of the default `-O3` pipeline (see `verify-sde-redistribute`).
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::sde::createSdeRedistributePass()";
}

#endif // ARTS_DIALECT_SDE_TRANSFORMS_STATE_PASSES
