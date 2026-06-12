# SDE Pipeline Walkthrough (revised) — GEMM vs jacobi-for

Pass-by-pass trace of the **revised** SDE pipeline ([`design-revision.md`](./design-revision.md))
over two deliberately divergent kernels: **GEMM** (dense `C = A·B`, a 3-nest
matmul with an inner k-reduction) and **jacobi-for** (an iterative timestep loop
over a 5-point stencil sweep).

Each pass shows the one-sentence **what + why**, then GEMM and jacobi. IR is
tagged:
- **`[REAL]`** — verbatim from the current staged dumps (passes whose behavior the
  revision does not change).
- **`[PROJECTED]`** — the revised shape, grounded in the real shapes (the new/
  revised passes that don't exist in the current binary yet).

Pipeline-position tags: **[NEW]**, **[REVISED]**, **[REMOVED]**. The full ordering
and the macro view are in [`pipeline-diagram.md`](./pipeline-diagram.md).

> Starting IR (both): plain `func` + `scf.for`/`affine` + `memref` (after
> `lower-affine`). GEMM = one 3-deep matmul nest (i, j, k) with the k-loop
> accumulating into a scalar; jacobi-for = an outer timestep `scf.for` containing a
> copy-back sweep and a 5-point stencil sweep, each a 2-D nest. **OpenMP pragmas
> are not required** — `raise-to-sde` discovers the parallel/memory/schedule
> structure from the sequential IR; the OMP frontend only *decorates* when present.

---

## Entry normalization (unchanged)

`promote-target-attrs → lower-affine → CSE → sde-input-inliner → canonicalize →
scalar-forwarding → sde-memref-normalization → sde-handle-deps →
dead-state-cleanup`. One sentence: normalize the frontend memref/SCF form (target
attrs, scalar SSA, pointer→memref views) so the raise step sees clean structured
loops over memref storage. Unchanged by the revision.

---

## RAISE

> **Order: convert FIRST, then raise the missing things.** For these
> OMP-annotated kernels, `convert-openmp-to-sde` handles the matmul/stencil omp
> regions first; `raise-to-sde` then raises the *non-omp* residual (the init nests)
> and folds CU-normalization. (`raise-to-sde` is described first below only because
> it is the new CORE; at runtime convert precedes it — matching the real dumps:
> `ConvertOpenMPToSde` 001, then `Parallelize`/raise 003.)

### `raise-to-sde` (CORE) **[NEW]**
Raise any **sequential** `scf.for`/`affine` nest into CU/MU/SU: prove each axis
parallel|serial and lift proven-parallel axes into `sde.su_iterate` with a
**proof-derived** `cu_region<parallel|single>` leaf, retaining dependence-carrying
inner axes as `scf.for` inside the leaf — so all legal parallelism is exposed
without needing OpenMP, and the hardcoded-`<single>` bug is gone.

GEMM **[PROJECTED]** — the matmul nest and the three init nests all raise to
`su_iterate` + `cu_region<parallel>` (today the inits come out `<single>` from
`Parallelize`); the k-axis is proven reduction-carrying and stays an inner
`scf.for`:
```mlir
sde.su_iterate (%c0, %c0) to (%c4800, %c4800) step (%c1, %c1) {
  sde.cu_region <parallel> {                       // <-- proof-derived parallel (was <single> for inits)
    scf.for %k = %c0 to %c4800 step %c1 { ... }    // k carries the reduction -> stays serial inner
  }
}
```

jacobi **[PROJECTED]** — the two per-timestep sweeps raise to
`su_iterate`+`cu_region<parallel>`; the **outer timestep `scf.for` is proven a
serial recurrence and is NOT raised** (left as the enclosing loop), exactly as the
current pipeline leaves it — but now from plain `scf`, no `omp`:
```mlir
scf.for %t = %c0 to %T step %c1 {            // serial timestep recurrence — not raised
  sde.su_iterate (%c0, %c0) to (%N, %N) step (%c1, %c1) {
    sde.cu_region <parallel> { ... 5-point stencil ... }
  }
}
```
*Subsumes* `sde-parallelize` and the `scf.parallel` half of the OMP converter.

### `convert-openmp-to-sde` (FRONTEND) **[REVISED]**
When `omp.*` is present, call the same core builder, then **decorate** the result
with OMP-derived `schedule`/`chunk`/`nowait`/`reductionKind` — "raise + decorate,"
no second raise path.

GEMM **[REAL]** (the decoration the frontend adds): `schedule(<static>)` on the
matmul SU + the closing `su_barrier`:
```mlir
sde.su_iterate (%c0) to (%c4800) step (%c1) schedule(<static>) { sde.cu_region <parallel> { ... } }
sde.su_barrier
```
jacobi **[REAL]**: same decoration; the per-sweep barriers and runtime trip bounds
(`index_cast`) are stamped. (Absent OMP, these slots stay empty and the cost-gated
passes synthesize them.)

### ~~`sde-cu-normalization`~~ **[FOLDED into `raise-to-sde`]**
Not a standalone pass in the revision: wrapping residual bare host/scalar source
work into `cu_region<single>` and normalizing SU bodies is part of `raise-to-sde`'s
"sequential → CU/MU/SU" mandate (sub-steps (b) and (c) above). The
post-realization re-normalization becomes an internal cleanup of the realization
passes, not a pipeline stage.

GEMM **[REAL]** (now done inside raise-to-sde): init/teardown host runs wrapped,
threaded via `sde.yield`. jacobi **[REAL]**: additionally the per-timestep
convergence-flag reads / bound-selects into leaf `cu_region<single>`s.

---

## DEPENDENCY axis

### ~~`sde-loop-pattern-facts`~~ **[REMOVED as a pass → recomputed ANALYSIS]**
Classification (elementwise / matmul / stencil) + access offsets + partial-reduction
dims are **no longer stamped onto the IR**. `SuLoopAccessAnalysis` recomputes them
on demand from affine maps + iterator types; downstream passes *query* the analysis
instead of reading an attribute. The `[REAL]` IR below shows what that analysis
**reports** (today these are committed attrs; in the revision they are never written
— this is still where matmul and stencil first diverge, just as a query not a fact).
Caveat: after `tiling` rewrites the loop into `outer*T+inner` form the analysis can
no longer recover offsets/owner-dims from the loop — past that point they are read
from the rank-expanded **type** / `su.halo` op (fate-2), not recomputed (Part 5.7).

GEMM **[REAL]** (what the analysis reports): `<matmul>`, `partialReductionDims=[2]`:
```mlir
sde.su_iterate ... classification(<matmul>) { ... }
  {inPlaceSafe, partialReductionDims = [2], partialReductionOwnerDims = [0,1], pattern = #sde.pattern<matmul>}
```
jacobi **[REAL]**: `classification(<stencil>)` with halo offsets — no reduction:
```mlir
sde.su_iterate ... classification(<stencil>) { ... }
  {accessMinOffsets = [-1,-1], accessMaxOffsets = [1,1], ownerDims = [0,1], pattern = #sde.pattern<stencil_tiling_nd>, writeFootprint = [1,1]}
```

### `sde-layout-assignment` **[REVISED]**
HPF-style per-array BLOCK layout from affine access relations — **revised to commit
the consumer's required read layout as the consumer `mu_alloc`'s rank-expanded
TYPE** (not just an `arrayLayout` attribute), so a later movement op gets a genuine
`source ≠ target` (this is what kills the identity self-edge).

GEMM **[REAL]** layout choice (the attrs are transitional; the revision keeps the
*choice*, moves the carrier to the type): A `block_parallel`, B `block_contraction`
(k-axis unsplittable), C `block_parallel` 2-D-owned; `layoutsDisagree=[1]`,
`commVolumeBytes=46080000`:
```mlir
{arrayId=1, kind="block_contraction", ownerDims=[0], blockShape=[2400,4800], commVolumeBytes=46080000}
```
**[PROJECTED]** revised: B's *consumer* read layout is committed as B's target type,
so the contraction read becomes a distinct-geometry movement instead of a self-edge.

jacobi **[REAL]**: one uniform `block_parallel` (owner[0,1], budget `[512,512]`,
400 blocks) on all three arrays; `commVolumeBytes=419430400` = halo overlap. The
revision keeps this and (with the knob removed, below) stops the block count from
collapsing to 4.

### `loop-interchange`
Reorder a matmul's k-reduction toward stride-1 / loop-carried form; put a stencil's
smallest-halo dim outermost. Unchanged.

GEMM **[REAL]** — **fires**: k-accumulator scalarized to `iter_args`:
```mlir
%7 = scf.for %k ... iter_args(%acc = %cst) -> (f32) { ... %14 = arith.addf %acc, %13; scf.yield %14 }
```
jacobi **[REAL]** — **no-op**: point stencil stores directly, no inner accumulator.

### `tiling` **[REVISED]**
Strip-mine parallel axes to budget/cache tiles and stamp the block grid — **revised
to reconcile the READER grain too** (today `reconcileArrayLayoutWithCommittedPhysicalShape`
is writer-only, leaving coarse reads); the reduction axis stays untiled.

GEMM **[REAL]**: matmul → `owner_tile_2d`, `physicalBlockShape=[437,600]`, k untiled:
```mlir
sde.su_iterate (%c0,%c0) to (%c4800,%c4800) step (%c437,%c600) ... {
  ... scf.for %k = %c0 to %c4800 step %c1 iter_args ... }
  {iterationTopology = #sde.iteration_topology<owner_tile_2d>, physicalBlockShape = [437,600]}
```
jacobi **[REAL]**: uniform `owner_tile` `[512,512]` + `physicalHaloShape=[1,1]`.
**[PROJECTED]** revised: the stencil/reduction **read** grain is reconciled to the
fine budget grain (no coarse read → the jacobi-for ~100× comm inflation goes away).

### `elementwise-fusion`
Fuse consecutive elementwise siblings sharing iteration space + schedule into an
`elementwise_pipeline`. **No-op on both** here (gemm's inits have disjoint spaces;
jacobi's only elementwise sibling is the stencil). *(Confirmed inert across the
suite; repair-or-delete candidate.)*

### ~~`schedule-refinement`~~ **[REMOVED]** · ~~`chunk-opt`~~ **[REMOVED]**
Demoted to optional OMP-frontend inputs (both were no-ops on gemm/jacobi anyway —
the matmul SU is already `<static>`, the stencil SUs carry no schedule). The
cost-gated synthesis is dropped with the attributes.

---

## EFFECT axis

### `distribution-planning` **[REVISED]**
Wrap each SU in `sde.su_distribute` with a kind (`matmul/elementwise → blocked`,
`stencil → owner_compute`) — **revised: the `min_distributed_tile_bytes` knob is
deleted**, so the block count is no longer collapsed (the poisson/jacobi
`muBlockCount=4` starvation goes away).

GEMM **[REAL]**: all SUs `<blocked>`:
```mlir
sde.su_distribute <blocked> { sde.su_iterate ... classification(<matmul>) { ... } }
```
jacobi **[REAL]**: stencil SU `<owner_compute>` (owner reads tile + ±1 halo):
```mlir
sde.su_distribute <owner_compute> { sde.su_iterate ... classification(<stencil>) { ... } }
```

### `iteration-space-decomposition`
Split interior/boundary guarded loops into a branch-free interior + boundary loops.
GEMM **[REAL]** — no-op (already branch-free). jacobi **[REAL]** — no-op *despite*
having the boundary guard (its predicate is an `arith.select` fused row+col, not the
`andi` chain the matcher needs — a known gap). *(Repair candidate.)*

### `barrier-elimination`
Erase provably-disjoint barriers; tag survivors; commit timestep-stage structure on
iterative stencils. Unchanged.

GEMM **[REAL]**: single barrier kept (`barrierReason=unknown_required`); no
timestep structure. jacobi **[REAL]**: per-timestep barriers kept + the stencil SUs
gain `repetitionStructure=full_timestep`, `asyncStrategy=advance_stage`, pattern →
`alternating_buffer_stencil`.

---

## STATE axis (realize facts as structure)

### `sde-memory-unit-realization`
Rewrite shared `memref.alloc` roots into `sde.mu_alloc` carrying `arrayId`.
Classification-agnostic. GEMM **[REAL]**: 3× `sde.mu_alloc` for the 4800² arrays.
jacobi **[REAL]**: 3× for the 10240² double-buffer arrays.
```mlir
%6 = sde.mu_alloc {arrayId = 0} : memref<4800x4800xf32>   // gemm
%6 = sde.mu_alloc {arrayId = 0} : memref<10240x10240xf64> // jacobi
```

### `sde-atomic-reduction-realization`
Realize `reduction_strategy(atomic)` → `sde.cu_atomic`. **No-op on both** (gemm's
accumulate is float `addf`, never atomic; jacobi has no reduction).

### `sde-tree-reduction-realization` **[NEW]**
Realize `reduction_strategy(tree)` (and admitted sequential reductions) into
per-block partial buffers + an owner-strip partial-fill SU + an ordered combine —
so a tree reduction is *realized structure*, not a downstream promise. Requires a
reassociation license.

GEMM **[PROJECTED]** — **no-op**: gemm's k-reduction is an in-CU `iter_args`
accumulator (a *loop-carried* reduction inside the parallel tile), not an exposed
array-level reduction, so there is nothing for this pass to split. jacobi
**[PROJECTED]** — **no-op**: no reduction. *(This pass is the lever for stream
checksum / layernorm·batchnorm mean-variance, not for gemm/jacobi — shown here to
make its place in the pipeline explicit.)*

### `sde-cu-normalization` (re-run) · `sde-scalar-block-reduction`
CU re-containment (jacobi wraps the bare flag-stores; gemm unchanged) and the
1-D scalar-reduction realizer (no-op on both). Unchanged.

### `sde-rank-expand-mu`
Rank-expand each `mu_alloc` so the block grid is leading TYPE dims; rewrite
loads/stores via div/mod. **This is the pass the revision leans on harder** — the
block grain *is* the type, so `physicalOwnerDims`/`physicalBlockShape` attributes
become redundant restatements (slated for deletion once consumers read the type).

GEMM **[REAL]**: heterogeneous per-array — A `[64x75x4800]`, B `[8x600x4800]`, C
`[11x8x437x600]`:
```mlir
%8 = sde.mu_alloc {arrayId = 2} : memref<11x8x437x600xf32>
%44 = memref.load %6[%div, %rem, %arg4] : memref<64x75x4800xf32>   // div/mod localized
```
jacobi **[REAL]**: identical uniform `[20x20x512x512]` for all three; one div/mod
block per neighbor (center, i±1, j±1):
```mlir
%6 = sde.mu_alloc {arrayId = 0} : memref<20x20x512x512xf64>
%34 = memref.load %7[%30,%31,%32,%33] : memref<20x20x512x512xf64>   // a neighbor load
```

### `sde-raise-to-mu-access-window` **[REVISED]**
Raise one `sde.mu_access_window` per (MU, CU, mode) — **revised: emit `sde.su_halo`
carrying real `radiusLo`/`radiusHi`** for stencil reads, with the radii projected as
an acquire **slice** (`validExtents` stays `== blockExtent`, so DB grain is not
inflated). This is what makes the ±1 halo a structured window instead of dissolving
into div/mod the window can't see.

GEMM **[REAL]**: 1-D `owner_dims(1)` write+read windows; no halo:
```mlir
sde.mu_access_window read %6 : memref<64x75x4800xf32> array_id(0) owner_dims(1) block_lo [0] block_hi [64] valid [75,4800]
```
jacobi **[REAL]** window today + **[PROJECTED]** the new `su.halo`:
```mlir
sde.mu_access_window read %6 : memref<20x20x512x512xf64> array_id(0) owner_dims(2) block_lo [0,0] block_hi [20,20] valid [512,512]
sde.su_halo %7 {arrayId=1, ownerDims=[0,1], blockShape=[1,1,512,512], radiusLo=[1,1], radiusHi=[1,1]}   // [PROJECTED]
    : memref<20x20x512x512xf64> -> memref<20x20x512x512xf64>
```

### `sde-mu-access-window-sync-opt`
Erase barriers proven Redundant over the windows. **No-op on both** (each barrier
abuts a windowless `cu_region<single>` control epilogue → OutOfScope). *(In the
revision this logic moves into `SdeSuBarrierOp::verify`.)*

### `sde-redistribute` **[REVISED]**
Turn committed layout disagreement into **first-class SU movement ops** (was
`sde.redist` + a `movement_family` attribute, always `target=source`).

GEMM — today **[REAL]** `sde.redist <reduce_scatter_like>`; revised **[PROJECTED]**
`sde.su_reduce_scatter`:
```mlir
// [REAL]      sde.redist <reduce_scatter_like> %7 ... from owner [0] block [1,600,4800] to owner [0] block [1,600,4800] cost 46080000
   sde.su_reduce_scatter %7 {arrayId=1, reduceDim=2, reductionKind=add}    // [PROJECTED]
       : memref<8x600x4800xf32> -> memref<8x600x4800xf32>
```
jacobi — today **[REAL]** `sde.redist <halo_like>`; revised the halo is the
`su.halo` raised above (the redist op disappears):
```mlir
// [REAL]      sde.redist <halo_like> %7 ... halo [1,1,0,0] cost 419430400
//  [REVISED]  no redist op — the halo IS the sde.su_halo from pass above
```
**Cross-owner repartition** (where consumer layout ≠ producer layout — the 2-node
chained-matmul / atax case): this pass emits **`sde.su_all_to_all %A : memref<P> ->
memref<C>`** with genuinely distinct types — the self-edge fix. ARTS realizes it
(reader-pull per-target-block gather); see [`arts-all-to-all.md`](./arts-all-to-all.md).
GEMM at 1 node stays `reduce_scatter`; the repartition appears at 2-node / chained.

---

## VERIFICATION **[REMOVED as passes → op-level]**
The 8 interleaved `verify-sde-*` passes (physical-consistency, mu-layout,
mu-access-window[-sync], redistribute, coarse-avoidance, …) are **deleted** and
their checks move into op verifiers / traits / conversion-legality (the ARTS/EDT
model). What survives: a 1-arm `verify-sde` ("source compute outside any CU") +
`verify-sde-lowered` ("no `sde.*` survives") as thin O(N) residuals. Full mapping:
[`op-level-verification.md`](./op-level-verification.md). `sde-coarse-avoidance`'s
verifier is deleted outright (it asserted "an optimization fired"); the transform's
fail-closed-with-evidence re-homes to layout-assignment + movement-op emission.

---

## BOUNDARY (SDE → ARTS)
`SdeStorageToArtsDb → SdeAccessesToArtsDeps → FinalizeSdeToArts`. Reads committed
owner/block/halo facts verbatim and realizes ARTS DBs/EDTs; **revised: the
`su.all_to_all` arm replaces the `:441-444` hard-error** so a genuine repartition
lowers to a reader-pull gather instead of failing. `verify-sde-lowered` (conversion
legality in spirit) confirms no `sde.*` op survives.

---

## Contrast: where matmul and stencil diverge (load-bearing passes)

| Pass | GEMM (matmul) | jacobi-for (stencil) | Why |
|---|---|---|---|
| raise-to-sde **[NEW]** | matmul + inits → `su_iterate`+`cu_region<parallel>`; k stays inner serial | sweeps → `su_iterate`+`<parallel>`; outer timestep `scf.for` stays serial | k is a reduction axis; the timestep loop is a true serial recurrence |
| loop-pattern-facts | `<matmul>` + `partialReductionDims=[2]` | `<stencil>` + halo offsets `[-1,-1]..[1,1]` | reduction vs neighbor access |
| layout-assignment **[REVISED]** | A block_parallel, **B block_contraction**, C 2-D-owned | one uniform block_parallel on all 3 | matmul k-axis unsplittable; stencil arrays share the 2-D tiled space |
| loop-interchange | **fires**: k → `iter_args` | no-op | only matmul has an inner accumulator |
| tiling **[REVISED]** | `owner_tile_2d [437,600]`, k untiled | `owner_tile [512,512]` + `physicalHaloShape=[1,1]` | preserve accumulator; stencil needs halo |
| distribution-planning **[REVISED]** | all `<blocked>` | stencil `<owner_compute>` | stencil reads outside its owner block |
| rank-expand-mu | A/B/C heterogeneous block shapes | identical `[20x20x512x512]`, halo div/mod balloons | distinct matmul roles vs interchangeable double-buffer state |
| raise-to-mu-access-window **[REVISED]** | 1-D windows, no halo | 2-D windows + **`sde.su_halo` radii** | matmul owns one strip dim; stencil owns the 2-D grid with ±1 halo |
| redistribute **[REVISED]** | **`sde.su_reduce_scatter`** (k reduction) | **`sde.su_halo`** (neighbor overhang); `su.all_to_all` at 2-node repartition | movement family = access shape |

## What the revision changes (summary)
- **`raise-to-sde` [NEW]** subsumes `sde-parallelize` + the OMP `scf.parallel` path; OMP becomes a thin decorator; the hardcoded `cu_region<single>` is now proof-derived `<parallel>`.
- **`sde-tree-reduction-realization` [NEW]** realizes the tree reduction (no-op for gemm/jacobi; the lever for reduction kernels).
- **Movement becomes ops:** `su.halo` / `su.reduce_scatter` / `su.all_to_all` replace `sde.redist` + the family attribute; the identity self-edge is gone.
- **Removed:** `schedule-refinement`, `chunk-opt`, the `reduction-strategy` enum, the `min_distributed_tile_bytes` knob, and all 8 verify passes (→ op-level).
- **Facts → types:** `physicalOwnerDims`/`physicalBlockShape`/`arrayLayout` collapse into the rank-expanded `mu_alloc` type that `rank-expand-mu` already produces.

> The `[REAL]` snippets are the committed binary's actual IR (current pipeline);
> the `[PROJECTED]` snippets are the revised target grounded in those real shapes.
> See [`design-revision.md`](./design-revision.md) Part 4 for the migration order
> that gets from one to the other.
