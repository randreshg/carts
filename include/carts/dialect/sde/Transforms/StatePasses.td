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
    cgeist writes `polygeist.target-cpu` and `polygeist.target-features` on
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

def MemoryUnitRealization
    : Pass<"sde-memory-unit-realization", "mlir::ModuleOp"> {
  let summary = "Realize SDE shared memrefs as memory units";
  let description = [{
    Rewrites shared memref allocations used by SDE scheduling units into
    `sde.mu_alloc` storage before the dialect boundary. This pass is
    intentionally SDE-local: it creates target-neutral MU storage declarations
    and leaves conversion to concrete target objects to boundary lowering.

    Physical layout and owner-slice facts stay on the scheduling unit only when
    this pass can realize the corresponding SDE MU shape. Unsupported committed
    physical storage layouts fail closed; the pass does not strip layout
    evidence or leave a raw bridge for downstream repair.
  }];
  let dependentDialects = ["::mlir::arith::ArithDialect",
                           "::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::LLVM::LLVMDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::polygeist::PolygeistDialect"];
  let constructor =
      "mlir::carts::sde::createMemoryUnitRealizationPass()";
}

def SdeRankExpandMu
    : Pass<"sde-rank-expand-mu", "mlir::ModuleOp"> {
  let summary = "Rank-expand committed block-grid MUs and rewrite CU accesses";
  let description = [{
    Structural owner-dim/grain carrier. For every `sde.mu_alloc` governed by a
    committed, single-contiguous-owner, fully-static elementwise/stencil BLOCK
    layout (`physicalOwnerDims`/`physicalBlockShape` on the writer
    `sde.su_iterate`), this pass makes the grain STRUCTURE rather than metadata:

      * the `sde.mu_alloc` result memref is rank-expanded so the block grid is a
        leading dim of the type (e.g. `memref<128x64xf64>` with owner dim 0 and
        block `[16,64]` becomes `memref<8x16x64xf64>`), and
      * every CU `memref.load`/`memref.store` on that MU is rewritten into the
        physical `[block, intra-block, ...]` coordinate system via div/mod
        localization.

    Owner dims become `recover(structure)` (see `verify-sde-mu-layout`); no
    owner-dim/block-shape attribute is added to the `mu_alloc`. Matmul,
    reduction, in-place, dynamic, and multi-owner layouts are left in
    conservative flat form (or fail closed) — never papered over with an
    attribute promise or a compatibility attr. The pass does not recompute owner
    dims or block shape; it reads the committed layout facts verbatim.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::polygeist::PolygeistDialect"];
  let constructor = "mlir::carts::sde::createSdeRankExpandMuPass()";
}

def SdeCuNormalization
    : Pass<"sde-cu-normalization", "mlir::ModuleOp"> {
  let summary = "Wrap all non-CU source executable work in a conservative CU";
  let description = [{
    Structural CU-normalization. Wraps every run of source-executable work that
    is not already inside a CU — non-OpenMP host init loops, scalar
    verification/check code, sequential reductions, and scalar-effect work —
    into a conservative `sde.cu_region <single>`, so the SDE boundary
    invariant "all source executable work lives in a CU" (verify-sde) holds for
    non-OpenMP work too.

    `sde.su_iterate` bodies become scheduling-only: direct raw scf/source
    compute and scalar/index plumbing are moved into a CU, leaving only CUs,
    barriers, and the terminator. `sde.su_distribute` direct-child legality is
    verifier-owned because the wrapper can only contain nested SUs,
    `sde.redist`, or barriers. Existing valid CUs are left unchanged.

    The pass is structural only: it adds no CU isolation, MU token, slice, or
    consumed dependency edge, and introduces no ARTS concept. It makes no
    distribution, movement, DB-grain, or access-window decision; it reads
    current IR and rewrites structure, writing no metadata and branching on no
    downstream promise.

    Values defined by a wrapped span and consumed afterward are threaded through
    `sde.cu_region` results, so the producer-side CU containment invariant
    remains explicit in SDE IR.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createSdeCuNormalizationPass()";
}

def SdeScalarBlockReduction
    : Pass<"sde-scalar-block-reduction", "mlir::ModuleOp"> {
  let summary = "Expose scalar checksum reductions as SDE block partial work";
  let description = [{
    Rewrites legal scalar add-reduction tails over committed 1-D block-layout
    arrays into real SDE work: a per-block partial buffer, an owner-strip
    `sde.su_iterate` that fills one partial per block, and a final scalar
    combine in block order.

    This is an SDE source/state transformation, not a downstream promise. It
    runs after SDE has committed and rank-expanded physical block facts. For
    already-expanded MU inputs, the partial buffer is emitted as an expanded
    `sde.mu_alloc` with the same structural block-grid proof used by normal MU
    layout verification.

    The pass is intentionally fail-closed: it requires add reductions to rank-0
    accumulators, committed compatible 1-D block layouts for every loaded data
    array, no unsupported side effects in the loop body, and deterministic
    floating-point order through per-slot partials when a block contributes
    multiple source samples. Unsupported loops remain unchanged with no
    metadata promise.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::scf::SCFDialect"];
  let constructor = "mlir::carts::sde::createSdeScalarBlockReductionPass()";
}

def SdeRaiseToMuAccessWindow
    : Pass<"raise-to-mu-access-window", "mlir::ModuleOp"> {
  let summary = "Raise canonical per-CU MU access windows over block-grid MUs";
  let description = [{
    Structural access-window carrier. After the final SDE shape rewrites, for
    every `sde.mu_alloc` rank-expanded into a single-contiguous-owner block-grid
    MU (elementwise/stencil, fully static), this pass raises one
    `sde.mu_access_window` fact near the top of the enclosing `sde.cu_region`,
    after any same-block `sde.mu_alloc` it names, per (MU root, read/write
    mode), in the SAME block-grid coordinate system the carrier type already
    encodes.

    It is a pure ADDITIVE raiser: it reads the committed layout facts and the expanded
    type VERBATIM (never recomputing owner dims or block shape), proves the
    structure against the writer `sde.su_iterate` iteration domain, and emits
    explicit window structure. It introduces no `sde.mu_token`, slice,
    `sde.mu_dep`, or ARTS isolation concept, and writes no downstream promise.

    Out-of-scope MUs are skipped conservatively (no window, no error): dynamic,
    matmul/reduction, multi-owner, and unsupported uses. Same-CU in-place
    load/store access is raised as one `readwrite` window, except committed
    halo reads are split into separate read and write windows so ARTS never
    receives a writable halo dependency. The pass never mutates existing ops; it
    only inserts window facts.

    Production SDE shaping runs this after MU layout has been realized and
    rank-expanded, then verifies the raised windows before crossing into ARTS.
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
        elementwise/stencil BLOCK layout that proves a real grid is realized as
        structure via the shared block-grid realize gate and rewriter — the
        committed layout facts are consumed VERBATIM, never re-authored;
      * every other coarse MU is left flat because SDE has not committed a
        realizable single-owner grid for it.

    The only hard failure is committed layout facts that cannot be realized end to end:
    rather than leave a partial owner-dim promise, the pass fails closed with
    evidence. It owns no redistribution and invents no coarse fallback. It
    introduces no `sde.mu_token`, slice, `sde.mu_dep`, runtime ownership route,
    DB, or ARTS concept. Its only mutation is the block-grid rewrite (in-place load/store
    rewrites that move no op across a CU/SU boundary), so CU legality is preserved.

    Production SDE shaping runs this after access-window and redistribution
    structure is available, then verifies that in-scope committed grids did not
    remain coarse before crossing into ARTS.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::polygeist::PolygeistDialect"];
  let constructor = "mlir::carts::sde::createSdeCoarseAvoidancePass()";
}

def SdeRedistribute
    : Pass<"sde-redistribute", "mlir::ModuleOp"> {
  let summary = "Make committed layout-disagreement edges explicit sde.redist";
  let description = [{
    Consumes each representable `layoutsDisagree` marker into explicit
    `sde.redist` structure from committed SDE facts. The currently
    representable edge is a cross-owner reduction read: when a consumer reduces a
    committed home's owned axis, the pass emits a `reduce_scatter_like` movement
    over that home block layout (source == target == the committed writer-role
    home; the movement is the reduction, not a re-layout). The home layout is
    read verbatim from the committed `arrayLayout` writer-role entry and the
    family from the consumer's grounded `reductionIndexed` access kind; owner
    dims, block shape, and family are never recomputed. The array root for an
    `arrayId` is recovered through the single `assignStableArrayIds` numbering
    shared with `sde-layout-assignment`.

    Other layout disagreements fail closed in SDE. This pass introduces no
    `sde.mu_token`, slice,
    `sde.mu_dep`, runtime ownership route, DB, collective, or ARTS isolation concept.

    Production SDE shaping runs this before ARTS conversion so movement edges
    are committed as SDE structure before a downstream layer consumes them.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::sde::createSdeRedistributePass()";
}

#endif // ARTS_DIALECT_SDE_TRANSFORMS_STATE_PASSES
