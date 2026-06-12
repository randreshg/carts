# SDE Pass Inventory and Pipeline

There are **36 registered passes** across five `.td` files in
`include/carts/dialect/sde/Transforms/`. The pipeline itself is **not** a
dialect-internal `Passes.cpp` builder — it is assembled imperatively with
`pm.addPass(...)` in `tools/compile/Compile.cpp`. Every shape-mutating
transform is paired with a fail-closed verifier that runs immediately after it.

> Distinguish *registered pass* (a `def … : Pass<"name">` with a command-line
> name, listed here) from *internal transform* (a `.cpp` helper invoked inside a
> pass). All of `tiling`, `loop-interchange`, `elementwise-fusion`,
> `schedule-refinement`, `chunk-opt`, `reduction-strategy`, and
> `scalar-forwarding` **are** registered passes.

## Conversion / entry (`ConversionPasses.td`, 4)

| Pass | Role |
| --- | --- |
| `sde-input-inliner` | Inlines helper calls only when a callee boundary would freeze a stale storage layout before planning. |
| `sde-memref-normalization` | Raises pointer / array-of-pointer storage to multi-dim memref views; normalizes OMP task deps into memref values. |
| `sde-handle-deps` | Converts residual OMP dependency variables (static arrays, token containers, subviews, direct allocs, block args) into memref values. |
| `convert-openmp-to-sde` | **Primary SDE entry.** Lowers `omp.parallel`/`wsloop`/`loop_nest` into `sde.su_iterate` + `sde.cu_region`, preserving reduction kind+identity, `nowait`, schedule+chunk, task tokens. Produces `sde.mu_data` for `shared(...)` vars. |

## Dependency axis (`DepPasses.td`, 7)

| Pass | Role |
| --- | --- |
| `sde-parallelize` | Proves perfectly-nested rectangular `scf.for` nests dependence-free and raises serial `cu_region<single>` into `su_iterate` + nested CU; closed-form-substitutes induction counters; fail-closed on unprovable nests. |
| `sde-loop-pattern-facts` | Classifies SU loop/access patterns (uniform/stencil/wavefront/matmul/reduction/elementwise-pipeline). Prerequisite for interchange/tiling/fusion. *(File lives in `state/` but registers here.)* |
| `sde-layout-assignment` | Module-scoped HPF DISTRIBUTE/ALIGN: chooses one element-space BLOCK layout per array minimizing abstract `commVolumeBytes`; commits `arrayLayout`/`layoutsDisagree`/`commVolumeBytes` on writer **and** every reader SU. No-op when cost-model capacity ≤ 1. |
| `loop-interchange` | Cost-model loop reordering: matmul j-k → k-j for stride-1 inner; stencils put smallest-halo dim outermost. |
| `tiling` | Strip-mines parallel (not reduction) axes into logical-capacity/budget/cache tiles; rewrites SU steps + CU carrier; stamps `iterationTopology`. Skipped if CU/MU partition facts already committed. |
| `elementwise-fusion` | Fuses consecutive sibling elementwise SUs that share iteration space + schedule and write disjoint destinations → one `elementwise_pipeline` SU. Rejects unsafe read-after-write. |
| `iteration-space-decomposition` | Splits interior/boundary loops behind `if/else` into dedicated boundary loops + a branch-free interior loop (tileable, distributable). |

## Effect axis (`EffectPasses.td`, 7)

| Pass | Role |
| --- | --- |
| `schedule-refinement` | Refines `auto`/`runtime` schedules into the cheapest concrete static/dynamic/guided via cost model. |
| `chunk-opt` | Synthesizes missing chunk sizes from worker capacity + min-iters-per-worker; preserves explicit source chunks. |
| `reduction-strategy` | Annotates reductions atomic / tree / local_accumulate from cost model. (Atomic only for integer add; partial reductions → local_accumulate.) |
| `sde-atomic-reduction-realization` | Consumes `reduction_strategy(atomic)` by rewriting the leaf-CU load/add/store into an explicit `sde.cu_atomic`, then strips the consumed reduction metadata. |
| `distribution-planning` | Commits distribution: chooses `owner_compute` (stencils) / `blocked` (elementwise/matmul/reduction), wraps in `su_distribute`, authors physical owner/block/halo/topology, realizes wavefront skew, coarsens CU grouping to a tile-byte floor; fails closed on in-place stencil wavefronts. |
| `barrier-elimination` | Erases `su_barrier`s with provably-disjoint neighbor write sets; tags survivors with `SdeBarrierReason`; commits timestep-stage structure + `advance_stage` async strategy. |
| `sde-mu-access-window-sync-opt` | Erases barriers whose verdict is `Redundant` over raised MU access windows (transform dual of `verify-sde-mu-access-window-sync`, shared `classifyBarrierSync`). |

## State axis (`StatePasses.td`, 10)

| Pass | Role |
| --- | --- |
| `sde-promote-target-attrs` | Copies `polygeist.target-*` into neutral `carts.target-*` on the module. |
| `scalar-forwarding` | Forwards constant-init rank-0 memref scalars back to SSA across region boundaries that block mem2reg. |
| `sde-dead-state-cleanup` | Removes dead dialect-neutral helper IR before planning. |
| `sde-memory-unit-realization` | Rewrites shared `memref.alloc`/`alloca` roots into `sde.mu_alloc` (carrying `arrayId`); fails closed on unrealizable committed layouts rather than stripping evidence. |
| `sde-rank-expand-mu` | For each `mu_alloc` with a committed single-owner static block layout, rank-expands the memref type so the block grid is a leading dim and rewrites loads/stores via div/mod. **Wired at `Compile.cpp:1168`** (corrects any "unwired" note). Fails closed; leaves matmul/reduction/in-place/dynamic flat. |
| `sde-cu-normalization` | Wraps runs of non-CU source work into `cu_region<single>`; normalizes SU bodies to leaf CUs/barriers/terminator. **Runs three times.** |
| `sde-scalar-block-reduction` | Rewrites legal scalar add-reduction tails over 1-D block arrays into a per-block partial MU buffer + owner-strip SU + block-ordered combine. |
| `raise-to-mu-access-window` | Additive/idempotent raiser: inserts one `sde.mu_access_window` per (MU root, mode) per `cu_region`; splits committed halo reads into separate read/write windows. |
| `sde-coarse-avoidance` | Realizes the committed finest grain as structure; leaves genuinely-coarse MUs flat for the verifier to diagnose; fails closed only on unrealizable committed layouts. |
| `sde-redistribute` | Consumes representable `layoutsDisagree` markers into explicit `sde.redist` (today: the `reduce_scatter_like` cross-owner reduction read); fail-closed diagnostic otherwise. |

## Verify gates (`VerifyPasses.td`, 8)

`verify-sde-physical-consistency` (before rank-expand), `verify-sde-mu-layout`,
`verify-sde-mu-access-window`, `verify-sde-mu-access-window-sync`,
`verify-sde-redistribute`, `verify-sde-coarse-avoidance`, `verify-sde`
(aggregate cross-op boundary gate), `verify-sde-lowered` (no `sde.*` op survives
conversion). Each reads committed facts verbatim, mutates nothing, and ends in
`signalPassFailure` on any violation. They share the same realize-gate
predicates (`isBlockGridRealizable`, `classifyBarrierSync`,
`queryAccessWindows`) as the transforms they police, so a transform and its
verifier can never silently desync.

## Pipeline order (`tools/compile/Compile.cpp`)

Three imperative builders:

1. **`buildSdeInputNormalizationPipeline`** (~1103): `sde-promote-target-attrs`
   → lower-affine → CSE → `sde-input-inliner` → canonicalize →
   `scalar-forwarding` → `sde-memref-normalization` → `sde-handle-deps` →
   `sde-dead-state-cleanup`.
2. **`buildSdePlanningPipeline`** (~1133): `convert-openmp-to-sde` →
   `sde-cu-normalization` → `sde-parallelize` → `sde-loop-pattern-facts` →
   `sde-layout-assignment` → `loop-interchange` → `tiling` →
   `elementwise-fusion` → `schedule-refinement` → `chunk-opt` →
   `reduction-strategy` → `distribution-planning` →
   `iteration-space-decomposition` → `barrier-elimination` →
   `sde-memory-unit-realization` → `sde-atomic-reduction-realization` →
   `sde-cu-normalization` → **`verify-sde-physical-consistency`** →
   `sde-rank-expand-mu` (+ `verify-sde-mu-layout`) → `sde-scalar-block-reduction`
   → `raise-to-mu-access-window` (+ `verify-sde-mu-access-window`) →
   `sde-mu-access-window-sync-opt` (+ sync verify) → `sde-redistribute`
   (+ `verify-sde-redistribute`) → `sde-coarse-avoidance`
   (+ `verify-sde-coarse-avoidance`) → `verify-sde`.
3. **`buildSdeToArtsPipeline`** (~1184): `SdeStorageToArtsDb` →
   `SdeAccessesToArtsDeps` → `FinalizeSdeToArts` → `VerifyArtsObjectsOnly`. These
   boundary passes live in the **`arts::` namespace**
   (`lib/carts/dialect/arts/Transforms/SdeToArtsBoundary.cpp`, 5191 lines): they
   read committed SDE owner/block/halo facts verbatim, realize ARTS DBs/deps,
   inline `cu_region`, and erase the SDE structural fact ops. `verify-sde-lowered`
   guarantees no `sde.*` op survives.

## Cost-model gating

`schedule-refinement`, `chunk-opt`, `reduction-strategy`,
`distribution-planning`, `barrier-elimination`, `sde-layout-assignment`, and
`tiling` are no-ops when the `SDECostModel` is null or
`logicalWorkerCapacity ≤ 1` — single-worker compiles deliberately keep the
serial lowering. A textual `mlir-opt`-style pipeline with no cost model silently
skips them. The two consumer passes (`sde-atomic-reduction-realization`,
`sde-mu-access-window-sync-opt`) take no cost model.
