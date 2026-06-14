# Revised SDE Design: Facts in Ops and Types, Not Attributes

A design-time revision covering three goals: **eliminate the attribute/contract/knob
soup, add first-class SU movement ops, and add a general sequential `raise-to-sde`
pass.** Produced by a multi-agent investigation (18 readers) → synthesis → three
adversarial reviews (charter/correctness, over-reach/simplicity, migration
realism) → reconciliation, all re-grounded against the live tree. Every hard
constraint is preserved: SDE names only CU/SU/MU geometry (never
EDT/DB/epoch/owner-map/route/collective/runtime-policy); facts are consumed
verbatim, never recomputed; a layer with enough information transforms or fails
closed with evidence; **DB grain stays separate from CU grain.**

> **Status:** design proposal, not yet implemented. The current SDE suite is RED
> at HEAD (see `[[keystone_v4_sde_wiring_red]]`), so the migration gates on
> *delta-from-baseline*, not all-green. This document supersedes the
> prioritization in [`proposed-passes.md`](./proposed-passes.md) where they
> overlap.

**The organizing principle (confirmed empirically):** *a verifier whose evidence
is an op or a type survives and strengthens; a verifier whose job is "attribute A
agrees with independent structure B" is a symptom that A should not be an
attribute.* `VerifySdeMuLayout` R2, `VerifySdeMuAccessWindow` R2, and
`VerifySdePhysicalConsistency` exist solely to re-prove an attribute matches the
rank-expanded type that already encodes the same fact — they vanish when there is
one representation to read.

**Corrections folded in from adversarial review:**
- *(correctness)* Struck the draft's `validExtents = blockExtent + radius`
  mechanism — `validExtents` feeds `blockShapeForExpandedWindow`
  (`SdeToArtsBoundary.cpp:227`) as the **storage block extent**; inflating it
  conflates DB grain with the read window. The halo neighborhood rides
  exclusively on `su.halo` radii, projected as an acquire **slice**. Added a
  reassociation-license gate to sequential reductions. `mu_data` reclassified as
  defined+lowered+unproduced (keep op + `lowerMuData`).
- *(over-reach)* Cut `su.allreduce` (it is `reduce_scatter ∘ broadcast`).
  Specified only the **2 live ops**; the other movement families are a
  fail-closed-named roadmap, not pre-seeded dead op surface.
- *(migration)* Added a Step −1 green-baseline pin; made the shared `su_iterate`
  builder the explicit first code step (precondition for deleting any
  positionally-threaded attribute); moved `min_distributed_tile_bytes` removal
  off Step 0 to *after* `su.halo` exists.

---

## Part 1 — Minimal attribute / contract / knob set

### 1a. `su_iterate` element-space layout block (`SdeOps.td:414-459`)

| Attribute | Verdict | Replacement / principle |
| --- | --- | --- |
| `physicalOwnerDims` | **replace-with-type** | Rank-expanded `mu_alloc` grid prefix; `recoverOwnerDims` (`MuLayout.cpp:149`) reconstructs it; access-window path already reads the type (`MuAccessWindow.cpp:208`). Survives transitionally on the un-expanded coarse/iter-arg reader **and** as the loop-dim↔owner-dim binding consumed by `resolveArtsOwnerSlotMapping` (dispatch routing, not just storage) — preserve explicitly until that consumer takes the type, or partitioned EDT dispatch breaks. |
| `physicalBlockShape` | **replace-with-type** | Trailing tile dims of the rank-expanded type (`buildExpandedMuType`, `MuLayout.cpp:131-141`). **×N live site** — consumers in `LoweringFactUtils`, `CreateDbs`, `BlockContractionSplit`, `SdeToArtsBoundary` (all under `lib/carts/dialect/arts/`). Convert readers first, delete attr last (Step 8). |
| `iterationTopology` | **remove** | Pure function of owner-dim count (`DistributionPlanning.cpp:677-679`); verifier already tautological (`SdeOps.cpp:693-720`). Compute, never store. |
| `distributionKind` (on su_iterate) | **replace-with-op** | Duplicates the `su_distribute` wrapper kind; set only because the wrapper is `NoTerminator` and cannot forward iter_arg results (`DistributionPlanning.cpp:2474-2482`). Give `su_distribute` a result-forwarding form; the wrapper becomes the single carrier. |
| `physicalHaloShape` | **replace-with-op** | Real per-owner-dim window, but movement payload not writer storage. Becomes `su.halo`'s `radiusLo`/`radiusHi`. Reconstructible from `accessMin/MaxOffsets` (`RedistributionEdges.cpp:71-84`); writer-side attr is redundant. |
| `logicalWorkerSlice` | **redesign → CU structure** | The one genuinely non-type, non-movement fact: CU-grain-over-DB-block ratio (`SdeToArtsBoundary.cpp:3983-4011`). Cannot fold into the MU type (DB grain ≠ CU grain). Move onto CU-grouping structure as a group-block-count over the recovered grid, carrying its whole-number-multiple invariant. |
| `inPlaceSafe` | **keep, reclassify** | Load-bearing aliasing-legality predicate; move out of the layout block into the dependency-fact set. |
| `inPlaceSharedState` | **keep, reclassify** | Load-bearing fail-closed gate (Gauss-Seidel, `DistributionPlanning.cpp:2364-2398`). |

### 1b. `su_iterate` HPF layout contract + reduction/schedule (`SdeOps.td:392-459`)

| Attribute | Verdict | Replacement / principle |
| --- | --- | --- |
| `arrayLayout` dict | **replace-with-type** | Triple-encoded (dict + `physical*` + rank-expanded type). Collapse to the type. `kind` becomes the choice of movement op, not a string (`replicated` stays a legal layout-shape fact; only the enum string is retired). |
| `arrayId` + `array_layout_root` | **remove** | `assignStableArrayIds` is a per-module dense counter (`LayoutGraph.cpp:289`), not semantic identity. Writer↔reader join is the shared SSA root Value. Zero ARTS-proper consumers. |
| `layoutsDisagree` | **remove** | Intra-pass scaffolding (produced by LayoutAssignment, erased by sde-redistribute, rejected at boundary). When movement is an op, the redistribution pass compares producer type to consumer required type at the consumer site and emits the op — no module-scoped mark/erase round-trip. |
| `commVolumeBytes` | **remove** | Advisory estimate, recomputable from the type during partitioning. Stop committing where it can drift. |
| `reductionStrategy` + enum | **replace-with-op** | `atomic` already realized as `sde.cu_atomic`, attr erased. `tree` **never realized** (zero consumers). `local_accumulate` becomes the partial-reduction op shape. Delete the enum. |
| `repetitionStructure` + `asyncStrategy` enums | **remove** | Write-only (`BarrierElimination.cpp:177-181`); no reader. Deletion **coupled to the builder refactor** (positionally threaded through 12 `create` sites) → Step 2. |
| `schedule` + `chunkSize` (+ ScheduleRefinement, ChunkOpt) | **remove** | OMP-fidelity holdover; never read at boundary/ARTS-RT. Demote to optional OMP-frontend inputs; cost-gated passes synthesize defaults when absent. Removing them deletes two passes. |
| `structuredClassification` | **redesign → SDE-internal** | Cached `analyzeSuLoopAccesses` result, 0 boundary reads, ~106 SDE branch sites. Keep as within-SDE memoization / pure query; never promote to the boundary. |
| `pattern` (SdePattern) | **redesign** | 1:1 relabel to ARTS. Once halo→`su.halo` and reduction→`su.reduce_scatter`, most cases are redundant with op identity. Keep only residual ordering families via `su.barrier` reason. |
| `accessMin/MaxOffsets`, `ownerDims`, `spatialDims`, `writeFootprint` | **merge → one window op** | One fact (the access neighborhood), computed/verified/consumed together. Merge into the `su.halo` radii descriptor. `spatialDims` is derived. |
| `reductionAccumulators` / `reductionKinds` | **keep** | SSA iter_args + combiner identity — irreducible. |
| `nowait` | **keep** | Drives boundary completion barrier. Minimal UnitAttr, real effect. |

### 1c. Op-level attributes (`mu_access_window`, `mu_alloc`)

| Attribute | Verdict | Replacement |
| --- | --- | --- |
| `mu_access_window.blockLo` | **remove** | Producer always emits zeros (`MuAccessWindow.cpp:299`). |
| `mu_access_window.blockHi` | **replace-with-type** | `== muType.getShape()[k]` for owner dims. |
| `mu_access_window.validExtents` | **replace-with-type** | Trailing tile dims of the rank-expanded type. **CRITICAL:** feeds `blockShapeForExpandedWindow` (`SdeToArtsBoundary.cpp:227`) as the *storage block extent* — must stay `== blockExtent`, NEVER inflated by halo radius (see Part 2). |
| `mu_access_window.ownerDimCount` | **remove** | `= rank - validExtents.size()`. |
| `mu_access_window.arrayId` | **keep (transitional)** | Replace with SSA-root identity in the global `arrayId` sweep (Step 9). |
| `mu_access_window.mode` | **keep** | Real read/write/readwrite effect. |
| `mu_alloc.arrayId` | **remove** | Only `ScalarBlockReduction.cpp:460` reads it; re-point to `array_layout_root`. |

### 1d. Knobs and cost-model surface

| Knob / virtual | Verdict | Evidence |
| --- | --- | --- |
| **`min_distributed_tile_bytes`** | **remove (behavior change, NOT Step 0)** | Default 0 in-compiler; harness force-injects ~4 MiB (`batch.py:505-523`), coarsening reads, starving kernels — the named knob. Sites: `SDECostModel.h:133`, `ARTSCostModel.h:83-84`, `Compile.cpp:167/1602-1603`, `DistributionPlanning.cpp:909,995,1022,1061,2030,2111`, RuntimeConfig field/parse, cfg writer. Removal regresses the 4-MiB-tuned extralarge gemm 2n, so it lands **after `su.halo`** (Step 4), when fine grain is reachable without the cap. |
| `kAbstractBlockFactor` | **remove** | Hardcoded worker-grain proxy (`LayoutAssignment.cpp:53`). |
| `getVectorWidth` family (4) | **remove SDE copy; verify ARTS** | Zero CARTS readers; duplicate at `ARTSCostModel.h:60`. |
| `getWorkersPerLocalityGroup` | **remove SDE copy; verify ARTS** | Duplicate at `ARTSCostModel.h:72`. |
| `getL2CacheSize` | **replace-with-probe or remove** | Fabricated 256 KiB literal feeding a real Tiling clamp. (OPEN: drop clamp vs probe.) |
| `getTaskCreationCost/SyncCost/DataAccessCost/AtomicUpdateCost` | **redesign** | Collapse into one fixed structural amortization floor; restate the tree-vs-linear choice on `log2(W)` vs `W`. |
| `getMinIterationsPerWorker` (cfg field) | **replace-with-fixed-constant** | Keep the derived floor (~2-3 iters), delete the cfg knob. |
| `getLogicalWorkerCapacity`, `getWorkerLocalityGroupCount` | **keep (irreducible)** | The only two real machine facts. |

**Post-revision cost model exposes only:** `getLogicalWorkerCapacity`,
`getWorkerLocalityGroupCount`, `getInterLocalityTaskWaves`, and fixed
iteration/pipeline floors. No byte budget, no fabricated cycle costs, no dead
vector virtuals.

### 1e. Enums

- **Delete:** `SdeAsyncStrategy`, `SdeRepetitionStructure` (Step 2),
  `SdeReductionStrategy` (Step 9).
- **Replace with ops:** `SdeMovementFamily` (7 cases → 0; Part 2). Note
  `phase_redist` is the default initializer (`RedistributionEdges.h:36`) — when
  the enum is deleted, "no movement op emitted" (absence) replaces the default.
- **Trim cases:** `SdeDistributionKind` drop `cyclic`; `SdeBarrierReason` drop
  `redundant` — **coordinate with ARTS** (`CartsBarrierReasonCases` is
  index-shared, `CommonAttrs.td:33-40`).
- **Keep:** `SdePattern` (trimmed), `SdeStructuredClassification`,
  `SdeIterationTopology`, `SdeReductionKind`, `SdeAccessMode`, `SdeCuKind`,
  `SdeResourceQueryKind`.

### 1f. Ops reclassified, not removed

- **`sde.mu_data` — defined + lowered, currently no in-tree producer.** Zero
  `SdeMuDataOp::create`, but `lowerMuData` (`SdeToArtsBoundary.cpp:493`) is
  invoked from the module walk (`:4972`). Keep op + lowering + walk; correct the
  stale "mu_data-for-shared" doc claim only. If ever removed, delete op +
  `lowerMuData` + walk together as one step.

---

## Part 2 — SU movement op set

Replaces `sde.redist` + `SdeMovementFamily`. Verified defects: self-edge
hardcoded (`RedistributionEdges.cpp:502-503`, `targetOwnerDims=sourceOwnerDims`);
only `halo_like`/`reduce_scatter_like` emitted (`488-489`); boundary hard-errors
other families (`SdeToArtsBoundary.cpp:444`) and forces `source==target` for
reduce_scatter (`432`).

**Surface decision:** specify only the **2 live ops** concretely; the other
families are a fail-closed-named roadmap (specifying 5 dead ops re-creates the
dead-enum problem in op form). `su.allreduce` is **cut** — it is
`su.reduce_scatter ∘ su.broadcast`; ARTS may fuse.

### Design rules common to all movement ops

- **SU-scope only.** Legal solely as direct children of `sde.su_distribute`,
  sequenced textually between producer and consumer `su_iterate`. Add to
  `isCuForbiddenSchedulingOp` (`SdeCuStructure.h:51-53`) and the `su_distribute`
  child whitelist; keep OUT of `su_iterate` bodies. Mirrors `sde.redist` — no new
  body-legality concept.
- **No tokens.** No `!sde.completion`/`!sde.token`/`!sde.dep`. Ordering is SU
  textual/structural order; `VerifySde`'s no-dataflow-graph rule holds by
  construction (add one assertion that movement appears only under
  `su_distribute`).
- **Geometry in types, not attribute pairs.** Source/target are operand/result
  memref types in the rank-expanded block-grid convention. `target != source` is
  the structural default; an identity move is a verifier error.
- **Names no collective/DB/EDT/route.** The mnemonic is a geometric verb; ARTS
  chooses transport.

### Specified ops (live producers + ARTS realizers)

```mlir
// Stencil halo: owner-preserving, asymmetric per-logical-dim ghost widths.
// radiusLo/radiusHi REPLACE the boolean physicalHaloShape gate with a real window.
sde.su_halo %mu { arrayId, ownerDims, blockShape, radiusLo = [..], radiusHi = [..] }
    : memref<G0x..xT0x..> -> memref<G0x..xT0x..>      // owner grid AND tile preserved
```
- **Semantics:** the consumer CU reads, per owner block, the block extent grown
  by `[radiusLo, radiusHi]` per logical dim, sourced from neighboring blocks of
  the SAME array. **The radii ARE the window.**
- **Carried as a slice, not a storage extent.** `validExtents` stays
  `== blockExtent`. The grown neighborhood is a slice on the acquire: the
  boundary projects the radii into `HaloSliceAttr<lower=-radiusLo,
  upper=+radiusHi>` via `getCommittedHaloShapeForWindow` /
  `commitHaloSlice` (`DbCommitDistributedDeps.cpp:28-39`). This solves the
  ±1-stencil-dissolves-to-div/rem problem (jacobi-for/poisson-for/FEM) **without
  enlarging storage**.
- **Retires:** `halo_like` family, `redist.haloShape`, `physicalHaloShape`, the
  `accessMin/MaxOffsets`+`ownerDims`+`spatialDims`+`writeFootprint` channel, the
  readwrite-halo boolean gate.

```mlir
// Owned-axis reduction; reduced-axis collapse is DERIVED, not a redundant target.
sde.su_reduce_scatter %mu { arrayId, ownerDims, blockShape, reduceDim, reductionKind }
    : memref<..> -> memref<..>
```
- **Semantics:** reduce over `reduceDim` across owners, result re-partitioned.
  Source==target geometry is legitimate here ("the movement is the reduction"),
  expressed by operand/result sharing a type — not a hardcoded copy.
- **Retires:** `reduce_scatter_like` family, the
  `partialReduction`/`partialReductionDims`/`partialReductionOwnerDims` triple (→
  operands), `reductionStrategy`, and the boundary `source==target` re-assertion.

### Repartition ops — `su.all_to_all` (SPECIFIED, with ARTS extension) + cousins

`su.all_to_all` (genuine repartition) **structurally closes the self-edge** (both
endpoints partitioned, owner dims must differ) and is now **specified with an ARTS
realizer** — see [`arts-all-to-all.md`](./arts-all-to-all.md). Two tiers:
- **order-preserving block-regrain** (owner dims stay leading; the coarse→block
  3mm/2mm/atax/bicg case) — realizable now as a reader-pull, per-target-block
  single-writer gather CU over a **distinct target DB grid**, reusing the RMA
  DB-move transport, owner-local writer split, and CDAG gates.
- **permuted/transpose** — needs one new piece of ARTS IR (an explicit owner-dims
  attribute on `DbAllocOp`, since the route derivation encodes only *leading*
  owner dims today); **fails closed with evidence** until that lands.

Discharges the cross-node remote-writer blocker by being reader-pull (target owner
is sole writer + remote RO reads — the gemm/jacobi 2N-validated owner-local path).
Requires an SDE-committed `su_barrier` bracketing the repartition so `CreateEpochs`
materializes the producer/repartition/consumer chain; the realizer must emit
fold-constant target block indices with its own fail-closed check (the SWMR gate
silently skips dynamic indices).

`su.broadcast` (1→N), `su.gather` (N→whole), `su.scatter` (whole→N) are degenerate
cases of the same realizer (replicated source or target) — correctness primitives
(3mm, atax/bicg), not scaling wins; kept as separate SDE-gated named ops.

### The self-edge fix, end to end

The self-edge is a symptom of representing movement as (source, target)
attribute pairs when sde-layout-assignment **discards the consumer's required
read layout** (`RedistributionEdges.cpp:413-420`). The load-bearing change
(Step 5): **sde-layout-assignment commits the consumer's required read layout as
the consumer `mu_alloc`'s rank-expanded target type.** Then a repartition op
takes source = producer type, target = consumer type — two distinct types — and
`target != source` is normal. `su.halo`/`su.reduce_scatter` name no distinct
target. Delete `RedistributionEdges.cpp:502-503`.

### Verifiers retired with the attribute soup

`VerifySdeRedistribute`, `VerifySdePhysicalConsistency`,
`VerifySdeCoarseAvoidance` (entire passes); `VerifySdeMuLayout` R2 +
`VerifySdeMuAccessWindow` attr-presence arms (tautology guards). **Survivors
(strengthen):** `VerifySde`, `VerifySdeLowered`, `VerifySdeMuAccessWindowSync`,
`VerifySdeMuLayout` R1.

---

## Part 3 — General `raise-to-sde` pass

Not greenfield. Three partial raisers exist: `convert-openmp-to-sde` (OMP + a
half-built `scf.parallel` pattern), `sde-parallelize` (proven-independent
rectangular `scf.for`), `sde-cu-normalization` (residual serial →
`cu_region<single>`). Affine is already lowered (`Compile.cpp:1126`), so "raise
affine" reduces to "raise scf." The recovery substrate (`SuLoopAccessAnalysis`,
`computeIteratorTypes`, `classifyPattern`, in-place legality) is already
OMP-agnostic. **The gap is a dependence-proof gap, not an IR-recovery gap.**

### Architecture: one CORE, two thin frontends, one convergence point

- **(A) raise-to-sde CORE** (new; subsumes `sde-parallelize`, the `scf.parallel`
  pattern, **and `sde-cu-normalization`**). Owns *all* CU/MU/SU construction via a
  shared `sde::buildSuIterate(loc, bounds[], body-clone)` emitting a bare SU/CU
  skeleton with **zero** optional attributes. Its mandate is the full "sequential →
  CU/MU/SU" transform, which is three responsibilities, not three passes: (a) raise
  proven-parallel loop nests into `su_iterate` + `cu_region<parallel>`; (b) wrap
  residual non-loop host/scalar source work into `cu_region<single>`; (c) normalize
  SU bodies to leaf CUs/barriers. `sde-cu-normalization` was a separate pass only as
  an artifact of the OMP-first pipeline; here (b)+(c) are sub-steps of raise-to-sde.
- **(B) convert-openmp-to-sde FRONTEND** (kept, slimmed). Matches `omp.*` only;
  calls the same core builder, then decorates with OMP-derived
  schedule/chunk/nowait/reductionKind.
- A post-realization re-normalization (today the second `sde-cu-normalization` run,
  after `memory-unit-realization`/`atomic-reduction-realization` may expose new
  host work) becomes an **internal cleanup invoked by those realization passes**,
  not a standalone pipeline stage.

### The algorithm

1. Walk loop nests **whose ancestors do not already include an
   `sde.su_iterate`/`sde.cu_region`** (the re-entrancy guard — see below).
2. **Per-axis dependence proof:** classify each axis `parallel | serial |
   unprovable` (no loop-carried RAW/WAR/WAW on any written root). Conservative
   default-accept; affine-relation extension gated behind a verify check.
3. **Per-axis emission (not all-or-nothing):** proven-parallel outer axes →
   `su_iterate` bounds (full N-D domain); dependence-carrying inner axes →
   retained as `scf.for` inside the leaf CU (legal source compute), preserving
   outer parallelism — exposes maximal legal work instead of demoting to fully
   serial.
4. **Admitted dependence families, each fail-closed:**
   - **Independent** — current Parallelize core.
   - **Reduction** — admit only with a **reassociation license** (exact/integer
     accumulator OR explicit fast-math/reassoc flag on the accumulate op); a
     plain sequential float `+=` carries no reassociation license (unlike OpenMP
     `reduction(+:)`). Kind ∈ {add,mul,…} is necessary, not sufficient. Emit a
     partial-reduce `su_iterate` + `su.reduce_scatter` over owner dims taken
     verbatim from committed layout. Dominant anti-scaling lever (stream
     checksum, layernorm/batchnorm mean/variance).
   - **In-place point-local stencil** — admit when `inPlaceSafe`; emit
     `su_iterate` + `su.halo`. **Fail closed** on `inPlaceSharedState`
     (Gauss-Seidel).

### The hardcoded-single fix

`Parallelize.cpp:638` stamps `SdeCuKind::single` on a nest just proven
dependence-free, while the `scf.parallel` path emits `parallel`
(`ConvertOpenMPToSde.cpp:830`). Load-bearing: CU kind gates `hasParallelLeafCu`
(`SdeLoopPatternFacts.cpp:40`) and `ElementwiseFusion` (`:249`), so a `<single>`
leaf is invisible to pattern-facts → interchange → tiling → fusion →
layout-assignment. Proven parallelism is silently discarded.

**Fix:** leaf CU kind is **proof-derived** — proven-independent domain →
`cu_region<parallel>`. **Target `:638` only** (`:438`/`:478` are legitimate
residual-serial wrappers). **Add a verifier:** a `cu_region<single>` directly
inside a multi-trip `su_iterate` body is rejected unless explicitly tagged
serial-by-proven-dependence (confirm `:438`/`:478` pass it). Do not also widen the
single-dim domain in the same step (both touch the batchnorm/pooling regression
site).

### Re-entrancy guard (replaces the dropped function-gate)

Dropping `enclosingFunctionHasSdeOp` makes the pass fire on clean sequential
functions, but that gate also prevented re-raising a partially-raised function.
Since the core runs then the OMP frontend, the core could double-wrap CU-body
source as a new SU (violating `verifyCuContainsNoScheduling`). **The replacement
is the precise structural guard in step 1: skip any loop nest whose ancestors
include an `sde.su_iterate`/`sde.cu_region`.**

### Semantics preserved-not-required

`schedule`/`chunk`/`nowait`/`reductionKind` stay optional `su_iterate` slots:
present (OMP) → frontend stamps; absent (sequential) → cost-gated passes
synthesize. No new attribute, no second path. Shared-var handling needs nothing
in the core: `MemoryUnitRealization` is the single access-driven `mu_alloc`
author.

### Pipeline placement

```
convert-openmp-to-sde (FRONTEND) <- omp.* regions via the shared buildSuIterate
                                    helper + source schedule/nowait/reductionKind
  -> raise-to-sde (CORE)         <- raises the MISSING non-omp scf nests; folds
                                    cu-normalization; re-entrancy guard skips the
                                    regions convert just produced
  -> [loop-pattern-facts is DELETED as a pass — now a recomputed analysis (Part 5)]
  -> layout-assignment -> loop-interchange -> tiling -> distribution-planning
     -> ... -> rank-expand-mu -> raise-to-mu-access-window -> redistribute -> boundary
```
**Convert first, then raise the missing things** (the OMP regions are already
parallel; `raise-to-sde` covers the rest). `buildSuIterate` is a shared helper
function, so the order is not constrained by "the builder must exist" — both passes
call it. raise-to-sde stays **before** layout-assignment (N-D domains feed
block-native layout) and is **layout-free**.

---

## Part 4 — Migration / sequencing plan

> **Authoritative order: [`architecture.md`](./architecture.md) "Migration — the
> unified dependency DAG" (S0–S26).** This Part 4, the §5.8 "Landing order", and
> Part 7's affine steps are the **per-area derivations** behind that DAG — consult
> them for rationale, but follow the single DAG for ordering. Where they differ, the
> DAG wins (it resolved the five-overlapping-orderings contradiction the gap audit
> found, e.g. `commVolumeBytes` is deleted *early/independently* in the DAG, not
> coupled to movement ops as the step below implies).

The suite is **RED at HEAD** — every step gates on **delta-from-baseline**, not
all-green. Cardinal rule: **convert readers to the type/op first, delete the
attribute last.** Cross-dialect note: `SdeToArtsBoundary`, `LoweringFactUtils`,
`BlockContractionSplit`, `CreateDbs`, `lowerMuData` are ARTS-owned
(`lib/carts/dialect/arts/`) — Steps that "teach the boundary" need ARTS lit too.

### The first three build+lit-gateable steps

- **Step −1 — Establish the gate (no code change).** Pin the current SDE+ARTS lit
  baseline; enumerate which tests are RED and why; capture megalarge 1n→2n on the
  unchanged binary. Define every later gate as *no-new-failures vs this baseline*.
- **Step 1 — Introduce the shared `sde::buildSuIterate` helper; delete nothing.**
  Route all 9 `SdeSuIterateOp::create` sites through one builder with
  named/defaulted optional attrs. **Output byte-identical** (lit delta == 0,
  byte-diff == 0 on the 21 kernels). Precondition for deleting any threaded attr.
- **Step 2 — Inert deletions with no threaded coupling, no behavior change.**
  Delete `SdeAsyncStrategy`/`SdeRepetitionStructure` enums+attrs (threading now
  defaulted), `mu_alloc.arrayId`, the SDE-side dead cost virtuals (after
  confirming the `ARTSCostModel` copies are independently dead). Correct the
  stale `mu_data` doc. Gate: lit delta == 0.

### Remaining steps (strictly ordered)

- **Step 3 — Leaf-CU-kind fix + verifier.** `Parallelize.cpp:638` → proof-derived
  `parallel`; add the single-inside-multi-trip verifier. Gate: byte-diff
  stability sweep + per-kernel `Correct=YES`; newly-exposed FEM owner-grid
  fail-closures enumerated as pre-existing.
- **Step 4 — `su.halo` + `su.reduce_scatter` ops; boundary learns them; THEN
  remove `min_distributed_tile_bytes`.** Add the 2 ops + verifiers; teach the
  boundary op-type dispatch; emit `su.halo` carrying radii **as a slice, not
  inflating `validExtents`**. Then delete the byte-budget knob end-to-end. Gate:
  jacobi-for/poisson-for/seidel `Correct=YES` + megalarge re-baseline (the
  4-MiB-tuned gemm regression is acceptable *here*).
- **Step 5 — Layout-assignment commits consumer required-read layout; delete the
  self-edge; re-host the fail-closed diagnostic** before Step 6 deletes the redist
  guard.
- **Step 6 — Retire `sde.redist` + `SdeMovementFamily`** (op, 7-case enum,
  `VerifySdeRedistribute`, family dispatch, `commVolumeBytes` twins,
  `phase_redist` default).
- **Step 7 — Collapse `mu_access_window` to a typed value** (`validExtents ==
  blockExtent` invariant preserved); shrink `VerifySdeMuAccessWindow`.
- **Step 8 — Delete `physicalOwnerDims`/`physicalBlockShape` (the ×N site).**
  Convert ARTS readers + `resolveArtsOwnerSlotMapping` dispatch binding first.
  Delete `VerifySdePhysicalConsistency`, `VerifySdeMuLayout` R2,
  `VerifySdeCoarseAvoidance`. Risk: dynamic-shape MUs need a constant-foldable
  element size.
- **Step 9 — Delete `arrayLayout` dict, `arrayId`, `array_layout_root`,
  `iterationTopology`, `distributionKind`, `reductionStrategy`** (migrate readers
  to the type; identity-by-SSA-value; result-forwarding `su_distribute`).
- **Step 10 — Remove ScheduleRefinement/ChunkOpt + `schedule`/`chunkSize`;
  relocate `logicalWorkerSlice` to CU structure; reclassify `inPlace*`.**
- **Step 11 — Generalize raise-to-sde.** Land the CORE pass (N-D domain,
  re-entrancy guard, per-axis split, reduction with reassociation-license gate,
  in-place stencil — each fail-closed); slim OMP to frontend; delete standalone
  `sde-parallelize`.

**Net:** `su_iterate`'s effect/schedule/layout attrs drop from ~27 to roughly
`{lowerBounds, upperBounds, steps, reductionAccumulators, reductionKinds,
nowait}` + reclassified dependency predicates; one movement op + 7-case enum → 2
named ops (+ fail-closed roadmap); ~6 of 9 verifier passes deleted or reduced to
predicates; the byte-budget knob and fabricated-cost family gone; one general
raise converging OMP and sequential inputs on a single CU/MU/SU shape.

---

## Open items (review could not fully resolve)

1. **`su.all_to_all` realizability — RESOLVED (specified), see
   [`arts-all-to-all.md`](./arts-all-to-all.md).** Order-preserving repartition is
   realizable now (reader-pull per-target-block gather over a distinct target DB);
   permuted/transpose needs an explicit owner-dims attribute on `DbAllocOp` (the
   "extend ARTS" tier) and fails closed until then. Remaining work: the owner-dim
   permutation dep-result-dim encoding + its ARTS-RT consumer, the SDE `su_barrier`
   precondition, and a 2N repartition reproducer (zero `all_to_all` test coverage
   today).
2. **Reassociation legality for sequential float reductions without a flag** — the
   conservative gate never raises a plain `+=` float reduction; whether to expose
   an opt-in reassociation pragma is an unresolved frontend policy question.
3. **Green-baseline characterization (Step −1) is a prerequisite, not done** —
   until the ~56 RED tests are sorted into pre-existing vs genuine, every
   delta-gate is itself unvalidated. The single largest realism dependency.
4. **`getL2CacheSize` clamp** — drop entirely vs carry a probed value is an
   unresolved measured decision.

---

## Part 5 — The no-contract pipeline: one principle, three fates, one fact table

Parts 1-4 enumerate *which* attributes go and *in what order*. This part states
the *rule* that decides every case, so a new pass cannot reintroduce the soup. It
restates the principle, gives the revised pass order with each pass annotated
real-transform-only, and resolves every attribute any SDE pass stamps today to one
of exactly three fates plus a dead-attr escape.

### 5.1 The principle and the three legal fates

**A pass either performs a REAL TRANSFORMATION (it changes structure, ops, or
types) or it does not mutate the IR with attributes at all.** A value written onto
an op *only so a later pass can read it back* is a metadata contract — the
anti-pattern this revision removes. The canonical instance is
`sde-loop-pattern-facts`: a registered pipeline pass
(`Compile.cpp:1143`, `buildSdePlanningPipeline`) whose attribute output
(`structuredClassification`, `pattern`, `accessMin/MaxOffsets`, `ownerDims`,
`spatialDims`, `writeFootprint`, the partial-reduction triple) is, line for line,
what `SuLoopAccessAnalysis` already recomputes from the IR
(`SuLoopAccessAnalysis.cpp:1182-1187`, `classifyPattern:653-679`,
`extractNeighborhoodAccessInfo:570-588`).

Any information a pass would otherwise stamp has exactly **three legal fates**:

1. **RECOMPUTE-ON-DEMAND ANALYSIS (fate 1).** A later pass that needs a
   classification, pattern, access offsets, owner dims, comm estimate, in-place
   legality, or topology *calls an analysis at use time*. An analysis query is not
   a contract: the fact is never committed to IR, so it cannot drift and there is
   no second authority. `SuLoopAccessAnalysis` already recomputes
   classification/reads/writes/affine maps/iterator types without consulting any
   attribute (`SuLoopAccessAnalysis.cpp:1130-1187`); it falls back to recompute
   wherever the attribute is absent today (`getWavefrontNeighborhood:357-368`).

2. **EMIT-AS-STRUCTURE (fate 2).** The decision becomes real IR immediately and
   has only one representation. Layout/grain becomes the rank-expanded `mu_alloc`
   memref **type** (`buildExpandedMuType`, `MuLayout.cpp:125-141`; read back by
   `recoverOwnerDims`, `MuLayout.cpp:148-183`) — `sde-rank-expand-mu` is the
   exemplar (`RankExpandMu.cpp:56-86`). Movement becomes a first-class SU op
   (`su.halo`/`su.reduce_scatter`/`su.all_to_all`, Part 2), its source/target
   layouts the operand/result types. A distribution choice becomes the
   `su_distribute` op kind. No intermediate attribute exists at any point.

3. **IRREDUCIBLE SOURCE SEMANTICS (fate 3).** Genuinely from the source and not
   recomputable from IR shape: a reduction combiner identity and its
   reassociation license, an OMP `nowait` the user wrote, the source `schedule`
   clause. Kept, but minimal, and only as an optional `su_iterate` slot that the
   frontend stamps and cost-gated passes synthesize when absent — never a pass
   whose job is to author it for a downstream reader.

Plus one escape for non-information: **DELETE-DEAD** — an attribute with zero
readers that branch on it (only write-then-copy-through). It is removed outright;
no recompute, no structure.

> **The verifier corollary (Part 1, restated as the test).** A verifier whose
> evidence is an op or a type survives; a verifier whose job is "attribute A
> agrees with independent structure B" is the *symptom* that A is a contract.
> `SdeOps.cpp:693-720` (`iterationTopology` agrees with owner-dim count),
> `VerifySdePhysicalConsistency`, `VerifySdeMuAccessWindow` R2,
> `VerifySdeMuLayout` R2 each re-prove that an attribute matches the type that
> already encodes it. They vanish with the attribute, not with the fact.

### 5.2 The revised pass order (each annotated real-transform-only)

Current order is inverted: `convert-openmp-to-sde` → `sde-cu-normalization` →
`sde-parallelize` → `sde-loop-pattern-facts`
(`Compile.cpp:1135-1143`). The frontend raises OMP first and the general raiser
runs after, with a standalone fact-stamping pass between them. The no-contract
order is:

**Order (corrected): convert FIRST, then raise the missing things.** The OMP
regions are already marked parallel, so convert them directly; `raise-to-sde` then
raises whatever OMP did not cover. `buildSuIterate` is a shared *helper function*,
not a pass, so it is available to the frontend regardless of pass order — there is
no "core must run first so the builder exists" constraint. This also matches the
current real order (`ConvertOpenMPToSde` at 001, then `Parallelize` at 003).

```
convert-openmp-to-sde (FRONTEND) REAL: raises the omp.* regions via the shared
                                sde::buildSuIterate helper, then decorates with
                                source-only schedule/chunk/nowait/reductionKind
                                (fate 3 / hint). No fact stamping.
  -> raise-to-sde (CORE, new)   REAL: raises the MISSING (non-omp) proven-parallel
                                scf nests -> su_iterate + cu_region<parallel>; wraps
                                residual serial -> cu_region<single>; normalizes SU
                                bodies. Folds sde-parallelize + sde-cu-normalization.
                                Owner-loop PROMOTION (rank-1 -> rank-N su_iterate,
                                today inside loop-pattern-facts:740-1188) lives here
                                as emit-as-structure. Stamps ZERO optional attrs.
  [re-entrancy guard]           raise-to-sde skips any loop nest whose ancestors
                                include an sde.su_iterate/sde.cu_region — i.e. the
                                regions convert-openmp-to-sde just produced
                                (Parallelize.cpp:226-229 already has the precise form).
  -> layout-assignment          DECISION (transient analysis): module-scoped owner
                                geometry + budget seed from affine maps; the storage
                                TYPE is materialized later by tiling/realization, not
                                here (mu_alloc does not exist yet, and the extent is
                                worker-derived at tiling — see §5.8.2). No arrayLayout
                                dict, no layoutsDisagree mark.
  -> loop-interchange           REAL: index/loop-order permutation only. Does NOT
                                re-stamp the pattern-facts bundle on synthesized ops.
  -> tiling                     REAL: strip-mine the band (stripMineLoop:1250-1289);
                                the chosen tile extent IS the loop step and the
                                trailing tile dims of the MU type. No physical* attrs.
  -> distribution-planning      REAL: wraps eligible su_iterate in su_distribute
                                (+ SdeDistributionKind) and performs realizeWavefront-
                                Skew; emits su.halo/su.reduce_scatter for movement.
                                No physical*/topology/distributionKind stamping.
  -> elementwise-fusion         REAL: loop fusion; does NOT propagate the fact bundle
                                onto the fused op.
  -> memory-unit-realization    REAL: memref.alloc/alloca -> sde.mu_alloc (rank-
                                expanded type), null-check fold, fail-closed on
                                unrealizable layout. No arrayId stamp.
  -> atomic-reduction-realization REAL: accumulator update -> sde.cu_atomic, consume-
                                and-erase reductionKinds. (Clean today.)
  -> barrier-elimination        REAL: erase redundant su_barrier (no write conflict).
                                Kept barriers carry NO reason label.
  -> [movement / window analyses queried at the boundary, no raiser passes]
  -> sde-to-arts boundary       queries queryAccessWindows / classifyBarrierSync /
                                SuLoopAccessAnalysis at lowering time.
```

Two responsibilities that were separate pipeline passes become sub-steps:
`sde-cu-normalization` ((b) wrap residual serial, (c) normalize SU bodies) folds
into `raise-to-sde`, and its second, post-realization run becomes an internal
cleanup invoked by the realization passes (Part 3). `sde-loop-pattern-facts`,
`raise-to-mu-access-window`, and `sde-redistribute` are **not passes at all** in
the end state — their outputs are analyses (§5.4) or first-class ops (§5.3).

### 5.3 The fact-elimination table

Every attribute any SDE pass stamps today, its fate, the concrete mechanism, and
its consumers. **Lead row: `sde-loop-pattern-facts` is deleted as a pass.**

| Pass · attribute | Fate | Mechanism (one representation) | Consumers today |
| --- | --- | --- | --- |
| **`sde-loop-pattern-facts` (the PASS)** | **delete-as-pass → fate 1** | The pass's *only* attribute output is recomputed by `SuLoopAccessAnalysis`. Delete the pipeline stage (`Compile.cpp:1143`); keep its real owner-loop **promotions** (`SdeLoopPatternFacts.cpp:740-1188`, rank-1→rank-N `su_iterate` rebuilds) by moving them into `raise-to-sde`/Parallelize as emit-as-structure. Downstream passes query the analysis. | n/a (pass) |
| └ `structuredClassification` | fate 1 | `analyzeSuLoopAccesses(op).classification` (`SuLoopAccessAnalysis.cpp:1182-1187` via `classifyPattern:653-679`). `elementwise_pipeline` via `isOwnerLocalPipelineReduction:874+`. Never committed; SDE-internal memoization only, **0 boundary reads**. | ~106 SDE branch sites; `DistributionPlanning`, `Tiling`, `Interchange`, `BarrierElimination`, `ElementwiseFusion`, `LayoutAssignment`, `MemoryUnitRealization` |
| └ `pattern` | fate 1 | `derivePattern(summary,classification,neighborhood)` (`SdeLoopPatternFacts.cpp:1190-1217`) is pure over the same summary; move into the analysis as `SuLoopAccessSummary::pattern`. **One exception is load-bearing across the boundary** (the `alternating_buffer_stencil` refinement → `ArtsDepPattern`, `DbModeTightening.cpp:520`); it is still a recomputed classification fact, not a barrier fact — BarrierElimination must stop being a second writer to it. | `BarrierElimination`, `DistributionPlanning:1627,1694`, `Tiling:761`, `SdeToArtsBoundary:147,191`, `ChunkOpt:134` |
| └ `accessMinOffsets` / `accessMaxOffsets` | fate 1 | `extractNeighborhoodAccessInfo(summary).min/maxOffsets` (`SuLoopAccessAnalysis.cpp:587-588`); identical to the stamp at `1553/1555`. (At the boundary the neighborhood also becomes `su.halo` radii — fate 2.) | `DistributionPlanning`, `Tiling`, `Interchange:1038-1039`, `RedistributionEdges:72-74`, verifiers |
| └ `ownerDims` / `spatialDims` / `writeFootprint` | fate 1 | `extractNeighborhoodAccessInfo(summary).ownerDims/spatialDims/writeFootprint` (`SuLoopAccessAnalysis.cpp:570-573`); recovered from the nest. Stamps at `1557/1559/1561` are identical. | `DistributionPlanning`, `Tiling`, `ChunkOpt`, `SdeToArtsBoundary` |
| └ `partialReduction` (+`Dims`+`OwnerDims`) | fate 1 | `commitPartialReductionFacts` (`1225-1265`) is pure over `summary.iterTypes` + `findCompatibleSuOutputLayoutFacts` + `isOwnerLocalPipelineReduction`; `commitContractionTilingFacts` (`1391-1414`) pure over `findContractionTilingCandidate` (`SuLoopAccessAnalysis.cpp:1001`). Expose as analysis accessors; the *movement* becomes `su.reduce_scatter` operands (fate 2). | `PartialReductionSplit`, `BlockContractionSplit`, `ReductionStrategy:90`, `ChunkOpt`, `DistributionPlanning:686-687` |
| └ `inPlaceSafe` / `inPlaceSharedState` | fate 1 (reclassify) | `collectStructuredMemoryEffects(body)` + `hasInPlaceSelfRead`/`hasOnlyPointInPlaceSelfReads` (`SdeLoopPatternFacts.cpp:1564-1588`) — pure over body IR. Add an `isInPlaceSafe`/`isInPlaceSharedState` analysis pair; **fix the analysis→attr coupling** at `SuLoopAccessAnalysis.cpp:1260` (`hasRealizableOwnerStrip` reads the attr) to recompute. Load-bearing aliasing/Gauss-Seidel gate; move into the dependency-fact set. | `DistributionPlanning` (fail-closed gate), `Tiling`, `DbModeTightening:596`, `PartialReductionSplit:451` |
| **`sde-layout-assignment`** · `arrayLayout` dict (`ownerDims`/`blockShape`/`budgetBlockShape`/`muBlockCount`/`kind`/`role`) | fate 2 | The decision (owner positions + block extents) becomes the rank-expanded `mu_alloc` **type**; fold `rank-expand-mu`'s `buildExpandedMuType` into layout-assignment so there is no dict intermediary. `kind` (blockParallel/blockContraction) is structurally indistinguishable grid+tile types, consumed at decision time; `replicated` is the flat type. `movement-kind` becomes the choice of SU op. | `RedistributionEdges`, `DistributionPlanning`, `MuLayoutRewriter` (drives expansion), `MuAccessWindow`, `SdeToArtsBoundary`, all via `parseArrayLayoutFacts` |
| └ `arrayLayout.ownerDims` | fate 2 | `== recoverOwnerDims(rankExpandedType)` (`MuLayout.cpp:149`); the boundary already checks the count against the window (`SdeToArtsBoundary.cpp:1098`). | `RedistributionEdges`, `DistributionPlanning:919-925`, `SdeToArtsBoundary:1078-1098` |
| └ `arrayLayout.blockShape` / `budgetBlockShape` | fate 2 | The tile (trailing) dims of the rank-expanded type. `budgetBlockShape` is the chosen physical grain = the committed `physicalBlockShape` that drives expansion; the abstract `blockShape` (via `kAbstractBlockFactor=2`) is a pre-expansion proxy that dies with the type. | `MuLayoutRewriter:501`, `RedistributionEdges:499`, `DistributionPlanning` seed |
| └ `arrayLayout.muBlockCount` | fate 1 | `inferCuCountFromMuPartition(shape, ownerPositions, blockShape)` (`LayoutAssignment.cpp:67`) = ceilDiv over grid dims of the type; `SdeCommittedFactUtils.h:194` already recomputes it. Recompute in the partitioner at use. | `DistributionPlanning` partition evidence; reconcile helpers |
| └ `arrayLayout.commVolumeBytes` + op-level `commVolumeBytes` | fate 1 | Advisory closed-form estimate; `estimateCommVolume` (`LayoutAssignment.cpp:280-363`) is pure over shape/elemBytes/alignment; `DistributionPlanning.cpp:990` already recomputes from shape+`physicalBlockShape`. Compute at the partition site; stop committing where it can drift. Deleting the commit deletes the fusion/tiling/chunk re-stamp churn. | `DistributionPlanning:872`, `CuMuGraphPartitioning:484-550`, `SdeToArtsBoundary:1564` |
| └ `layoutsDisagree` (op-level marker) | fate 1 (delete round-trip) | Pure intra-pipeline scaffolding: LayoutAssignment stamps, `sde-redistribute` erases (`Redistribute.cpp:58`), the boundary rejects leftovers. When movement is an op, the redistribution pass compares producer type to consumer required type *at the consumer site* and emits `su.halo`/`su.reduce_scatter`/`su.all_to_all` — disagreement is recomputed at edge construction. Delete mark/erase/reject entirely. | `Redistribute.cpp:37,58`, `RedistributionEdges:367`, `SdeToArtsBoundary:3786` (rejects) |
| └ `SdeArrayLayoutRootOp` | fate 3 (transitional) | The one legitimately structural output: it pins a memref **root SSA Value** to an arrayId+mode — a dict cannot reference a Value. KEEP as the producer↔consumer join key until `arrayId` is gone (Step 9), at which point the join is by `mu_alloc` SSA identity / def-use and even this op is recompute-from-use-def. | `MuLayoutRewriter:491`, reconcile helpers, boundary join |
| **`sde-tiling`** · `physicalBlockShape` / `physicalOwnerDims` | fate 2 | The block tile extents ARE the trailing tile dims, owner dims the grid prefix, of the rank-expanded MU type; `recoverOwnerDims` (`MuLayout.cpp:148-183`) reads tiles back from the type. Tiling already retiles the loop step to the block extent (`Tiling.cpp:1250-1289`); emit the expanded type here, delete the attrs. | `RankExpandMu`, `MemoryUnitRealization`, `DistributionPlanning`, `SdeToArtsBoundary`, `MuAccessWindow`, verifiers |
| └ `iterationTopology` (`owner_strip`/`owner_tile`/`owner_tile_2d`) | fate 1 | Pure function of owner-dim count (owner_strip iff 1, owner_tile iff ≥2); set verbatim from `ownerDims.size()` (`Tiling.cpp:743,848` etc.). The verifier `SdeOps.cpp:693-720` proves the tautology. Replace `getIterationTopology()` with an inline `ownerDimCount>=2` query; the verifier is deleted. | `SdeOps.cpp` verifier, `BarrierElimination`, copy-through only elsewhere |
| └ `logicalWorkerSlice` | fate 3 (relocate to CU structure) | The CU-grain-over-DB-block ratio (DB grain ≠ CU grain, cannot fold into the MU type). Move onto CU-grouping **structure** as a group-block-count over the recovered grid, carrying the whole-number-multiple invariant (`span%blockSize==0`, `SdeToArtsBoundary.cpp:4008`). Where `slice==blockShape` (common, `Tiling.cpp:860-865`) it carries zero info — drop. | `SdeToArtsBoundary:3983-4011` (CU group block counts) |
| └ `physicalHaloShape` | fate 2 (+ fate 1 fallback) | `= max(\|accessMinOffset[d]\|,\|accessMaxOffset[d]\|)` over owner dims; `getCommittedHaloShape` already recomputes when absent (`RedistributionEdges.cpp:71-86`). At the boundary it becomes `su.halo` `radiusLo`/`radiusHi` (movement is real IR). Delete the cache; take the existing fallback. | `RedistributionEdges`, `SdeToArtsBoundary` halo realization, `MuAccessWindow`, `MuLayoutRewriter:656` |
| **`distribution-planning`** · `su_distribute` + `SdeDistributionKind` | **REAL (keep)** | First-class structural wrapper (`runOnOperation:2474-2493`); the parallelism/owner-compute decision *is* the op kind. Emit-as-structure, already real. | boundary lowering |
| └ `realizeWavefrontSkew` rewrite | **REAL (keep)** | Genuine loop-skew/wavefront transform on in-place self-read stencils (`618-753`). | n/a |
| └ `physicalOwnerDims`/`physicalBlockShape` (set in `applyPhysicalPlan`/`commitMatmul`/`commitReduction`/`commitInPlaceStencilSlice`) | fate 2 | Same as tiling: the rank-expanded MU type. The partition decision stays real; its representation is the type, never an `su_iterate` attr. Transitional exception: the loop-dim↔owner-dim binding read by `resolveArtsOwnerSlotMapping` (dispatch routing) preserved until that boundary consumer takes the type (Part 1). | 42/32 reader sites incl. boundary, `MuLayoutRewriter`, `MemoryUnitRealization`, verifiers |
| └ `distributionKind` on `su_iterate` | fate 2 | Duplicates the wrapper kind; stamped only because `su_distribute` is `NoTerminator` and cannot forward iter_arg results (`2474-2482`). Give `su_distribute` a result-forwarding form; the wrapper is the single carrier. | 14 sites incl. boundary |
| └ `iterationTopology` / `structuredClassification` (this pass also authors) | fate 1 | Same as the tiling/pattern-facts rows; this pass must stop authoring them as a side effect of layout commit. `structuredClassification` has 0 boundary reads. | as above |
| └ `min_distributed_tile_bytes` (knob) | **delete-dead** | The named byte-budget knob (default 0 in-compiler, harness injects ~4 MiB, `batch.py:505-523`, coarsening/starving). Once `su.halo` makes fine grain reachable, `chooseCuMuTileFloorPlan`/`coarsen*` are deleted. Lands **after** `su.halo` (Step 4), not Step 0. | `DistributionPlanning:909,995,1022,1061,2030,2111`, `SDECostModel.h:133` |
| **`barrier-elimination`** · barrier ERASE | **REAL (keep)** | Erases redundant `su_barrier` when adjacent SU phases have no write conflict (`647-653`, erase `687-688`). | n/a |
| └ `barrierReason` (on `su_barrier`) | fate 1 | The terminal lowering ignores the value — `arts.barrier` always lowers to `arts_yield` (`ConvertArtsRtToLLVMPatterns.cpp:262-274`), zero readers branch on it. Keep-vs-erase + the "why" is recomputable from surrounding windows via the shared `sde::classifyBarrierSync` (`AccessWindowSync.h:73`). Drop the stamp; query at use if ARTS ever needs it. | `SdeToArtsBoundary:4844` casts onto `arts.barrier` (then ignored) |
| └ `repetitionStructure` (`full_timestep`) / `asyncStrategy` (`advance_stage`) | **delete-dead** | Write-only: the value appears only at the write site (`commitRepeatedTimestepStage:177-181`); all 6 "reads" are builder copy-through that never branch; 0 readers in arts/arts-rt/codir. Remove the setters and the slots (`SdeOps.td:419-420`); byte-identical. | none (copy-through only) |
| └ `pattern` overwrite to `alternating_buffer_stencil` | fate 1 | NOT a barrier fact; `pattern` is owned by the (deleted) pattern-facts analysis and recomputable (`hasAlternatingBufferExchange:194-197` + `hasInPlaceSelfRead:161`). BarrierElimination overwriting it is a second writer to a shared classification slot — stop. | load-bearing → `ArtsDepPattern`, `DbModeTightening:520` |
| **`memory-unit-realization`** · memref → `mu_alloc` | **REAL (keep)** | `memref.alloc/alloca` → `sde.mu_alloc` (same type), RAUW, dead-alloc erase (`214-255,371-451`); null-check fold; fail-closed on unrealizable layout. | n/a |
| └ `mu_alloc.arrayId` | **delete-dead** | Both readers (`MuLayoutRewriter:106-114`, `MuAccessWindow:19-27`) already fall back to the SSA-reachable `SdeArrayLayoutRootOp` id. Stop calling `setArrayIdAttr`; the fallbacks cover every read. `ScalarBlockReduction:457-464` independently walks the root op. | `MuLayoutRewriter`, `MuAccessWindow`, `ScalarBlockReduction` (all with fallback) |
| └ `arrayId` as a module-counter identity surrogate | fate 2 | Encodes nothing the shared memref **root Value** does not. Key provenance on the SSA Value (`sameMemrefRoot` is already the real key, `MemoryUnitRealization.cpp:327-334`) across `array_layout_root`/`mu_access_window`/`redist`. The bidirectional-uniqueness verifier (`SdeOps.cpp:674-689`) becomes vacuous and is deleted. | join key across the whole provenance system + ARTS `dep_array_ids` |
| **`raise-to-mu-access-window`** (the PASS) | **delete-as-pass → fate 1** | Self-described "pure ADDITIVE raiser" (`RaiseToMuAccessWindow.cpp:14`); its only output is `queryAccessWindows(mu)` stamped into a zero-result op (`69-100`). The sole consumer (`SdeToArtsBoundary`, `4964-4993`) holds the same `mu_alloc` and can call `queryAccessWindows` itself. Delete the pass; make `queryAccessWindows` the boundary's analysis. | `SdeToArtsBoundary`, `AccessWindowSync`, verifier |
| └ `mu_access_window.blockLo` | **delete-dead** | Producer always emits zeros (`MuAccessWindow.cpp:299`); consumer synthesizes a zero vector of length `ownerDimCount`. | `SdeToArtsBoundary:1039` |
| └ `mu_access_window.blockHi` | fate 2 | `== muType.getShape().take_front(ownerDimCount)` (producer copies `exp->gridCounts`, `MuAccessWindow.cpp:300`). Read from the type. | `SdeToArtsBoundary:1041` |
| └ `mu_access_window.validExtents` | fate 2 | `== muType.getShape().drop_front(ownerDimCount)` (the trailing tile dims). **CRITICAL invariant:** stays `== blockExtent`, never halo-inflated (feeds `blockShapeForExpandedWindow` as the storage extent, `SdeToArtsBoundary.cpp:215-227`). | `SdeToArtsBoundary:219,1043,1629` |
| └ `mu_access_window.ownerDimCount` | fate 2 | `= muType.getRank() - validExtents.size()`. | `SdeToArtsBoundary` (10 sites), verifier |
| └ `mu_access_window.arrayId` / `mu_access_window.mode` | fate 3 / fate 1 | `arrayId` copied from the MU root (`MuAccessWindow.cpp:296-297`) — transitional, recomputed-from-alloc, retired with the global `arrayId` sweep. `mode` is the real read/write/readwrite effect but is itself recomputed by the analysis from per-CU memref users (`275-313`, incl. the committed-halo read split); survives only as a field if the window becomes a typed value. | boundary dep-mode + halo-read split |
| **`sde-redistribute`** (the PASS) + `redist` op | fate 2 | Recomputes edges via `collectRedistributionEdges` and stamps zero-result `sde.redist` ops (`Redistribute.cpp:76-123`). Replace with first-class `su.halo`/`su.reduce_scatter`/`su.all_to_all` (Part 2); operand type = producer layout, result type = consumer required layout. | `SdeToArtsBoundary` (validate/halo/reduce-scatter), erased at boundary |
| └ `redist.family` (`SdeMovementFamily`, 7 cases) | fate 2 | Becomes op identity (`su.halo` vs `su.reduce_scatter` vs `su.all_to_all`); only `halo_like`/`reduce_scatter_like` ever emitted (`RedistributionEdges.cpp:486-489`). | `SdeToArtsBoundary:416,441-444` |
| └ `redist.source/targetOwnerDims` + `source/targetBlockShape` | fate 2 | Source = operand memref type, target = result memref type of the movement op. Today target is hardcoded to source (`RedistributionEdges.cpp:502-503`) because layout-assignment discards the consumer required layout — the self-edge fix commits it as the consumer `mu_alloc` type (Step 5), making `target != source` the default. | `SdeToArtsBoundary` endpoints |
| └ `redist.haloShape` | fate 2 | Becomes `su.halo` `radiusLo`/`radiusHi`; the boundary projects into `HaloSliceAttr<lower=-radiusLo,upper=+radiusHi>` (a slice, not storage). Reconstructible from `accessMin/MaxOffsets` (`RedistributionEdges.cpp:71-84`). | `SdeToArtsBoundary:450,478,616` |
| └ `redist.commVolumeBytes` | fate 1 | Abstract edge cost copied from the layout fact (`RedistributionEdges.cpp:536-538`); a cost-model query over (source type, target type) at the consumer site. | `SdeToArtsBoundary:1564` |
| **`elementwise-fusion` / `loop-interchange` / `scalar-block-reduction`** · re-stamped pattern-facts bundle on synthesized ops | fate 1 | These passes' fusion/interchange/reduction-split *structure* changes are legit, but they copy the whole fact dict onto the new `su_iterate` (`ElementwiseFusion.cpp:540-562` `getRewrittenAttrs`+explicit, `Interchange.cpp:884-904`, `ScalarBlockReduction.cpp:602-640`). Stop propagating; let `SuLoopAccessAnalysis` re-classify the synthesized op. | downstream classification readers |
| └ re-stamped `physical*`/`arrayLayout` grain on synthesized ops | fate 2 | The grain of a freshly synthesized op is the rank-expanded type of its `mu_alloc`; the `SdeArrayLayoutRootOp` is the structural carrier (`Interchange.cpp:938-948`, `ScalarBlockReduction.cpp:637-640`). Drop the parallel attrs. | `RankExpandMu` gate, `DistributionPlanning`, ARTS |
| └ `arrayId` on freshly materialized `mu_alloc` (ScalarBlockReduction) | fate 3 | Stable identity of a *new* buffer (`nextInternalArrayId`, `ScalarBlockReduction.cpp:844-849`); not recomputable, the legitimate name a new allocation needs. Migrates to SSA identity with the global sweep. | ARTS DB wiring / `findArrayLayoutRoot` |
| `reductionStrategy` (`tree` never realized) | **delete-dead / fate 2** | `atomic` → `sde.cu_atomic` (consume-and-erase, clean today); `tree` has zero realizers/consumers; `local_accumulate` → partial-reduction op shape. Delete the enum. | `ReductionStrategy`, `AtomicReductionRealization` (consumes+erases) |

**Clean today (no residual contract, real transforms only):** `RankExpandMu`,
`CoarseAvoidance`, `MuAccessWindowSyncOpt`, `AtomicReductionRealization`,
`IterationSpaceDecomposition`. These are the model the rest converges to.

### 5.4 The analyses that replace the fact-producers

Each downstream pass that read an attribute now calls an analysis. The cost
argument is the same in every row: these are **pure, IR-local, and cached per
op**; they are recomputed at use, and the recompute is the same code the
fact-producer ran once — so the only thing deleted is the *commit*, not the
computation.

| Deleted producer | Replacement analysis | What it returns | Cost |
| --- | --- | --- | --- |
| `sde-loop-pattern-facts` (classification, pattern, offsets, owner/spatial dims, write footprint, partial-reduction) | `SuLoopAccessAnalysis` (`analyzeSuLoopAccesses` → `classifyPattern`, `extractNeighborhoodAccessInfo`; new `pattern`/`partialReductionFacts` accessors) | `SuLoopAccessSummary{classification, reads/writes, affine maps, iterTypes, neighborhood min/max offsets, owner/spatial dims, write footprint, pattern, partial-reduction}` | One walk of the SU body's affine accesses; already the production analysis; no IR mutation, cacheable on the op |
| `inPlaceSafe` / `inPlaceSharedState` | `isInPlaceSafe` / `isInPlaceSharedState` (new, over `collectStructuredMemoryEffects`) | aliasing-legality + Gauss-Seidel fail-closed predicate | One memory-effect scan of the body; fixes the `SuLoopAccessAnalysis.cpp:1260` analysis→attr coupling |
| `muBlockCount` / `commVolumeBytes` | `inferCuCountFromMuPartition` / `estimateCommVolume` queried at the partition site | CU count = ceilDiv over grid dims; abstract comm bytes from (source type, target type) | Closed-form over the type; `DistributionPlanning.cpp:990` and `SdeCommittedFactUtils.h:194` already recompute |
| `iterationTopology` | inline `recoverOwnerDims(type).size() >= 2` | owner_strip vs owner_tile | A length compare on the recovered grid |
| `mu_access_window` (the op) | `queryAccessWindows(mu)` called by the boundary | `{mode, ownerDimCount, blockHi, validExtents}` (blockLo synthesized zeros, blockHi/validExtents type-derived) | One pass over the `mu_alloc` users per CU; the producer already called it |
| `barrierReason` | `sde::classifyBarrierSync` (already shared) | Justified / Redundant / Misaligned over surrounding windows | The same write-conflict reasoning `barrier-elimination` already does; queried only if ARTS needs it |
| `layoutsDisagree` | producer-type `!=` consumer-required-type at the edge | per-edge disagreement | A type compare at edge construction; no module-scoped mark |

The recompute discipline is what removes drift: there is no window in which a
committed attribute and the structure it mirrors can disagree, so the
"attribute-agrees-with-structure" verifiers (§5.1 corollary) have nothing to
check and are deleted.

### 5.5 The irreducible remainder (the honest end state)

A small set of facts are genuine source semantics, not recomputable from IR
shape. They are kept as **optional `su_iterate` slots stamped only by the OMP
frontend** (cost-gated passes synthesize a default when absent for
sequential-origin code) — never authored by a pass for a downstream reader.

- **`reductionKinds` (combiner identity) + `reductionAccumulators` (iter_arg SSA
  values).** Inferred once from the `omp.declare_reduction` combiner
  (`ConvertOpenMPToSde.cpp:235-254`), after which the source symbol is *erased*
  (`1031-1037`) — there is no IR left to recompute from. Crucially this carries
  the **reassociation license**: a plain sequential float `+=` is generic
  `arith.addf` and licenses no reassociation, unlike OpenMP `reduction(+:)`. Kind
  ∈ {add,mul,…} is necessary, not sufficient; a sequential-origin reduction is
  admitted only with an explicit license, else fail-closed.
- **`nowait` (UnitAttr).** A programmer assertion that the implicit barrier is
  elided. Not analysis-recoverable: structurally, "no barrier written" and
  "nowait" are indistinguishable yet semantically opposite. Drives the boundary
  completion barrier (`SdeToArtsBoundary.cpp:3913,4675`).
- **`schedule` + `chunkSize` (source clauses).** The user's `schedule(kind,chunk)`
  — genuine source, not recoverable from shape. KEPT optional, NOT a
  stamp-for-downstream contract: `ScheduleRefinement.cpp:118-137` and
  `ChunkOpt.cpp:81-90` *synthesize* cost-gated defaults when the slot is absent,
  proving there is no second authority. (The passes themselves are removed in
  Step 10; the slots remain as frontend inputs.)
- **`arrayId` on a freshly materialized internal buffer** (ScalarBlockReduction's
  partial `mu_alloc`). The legitimate name a *new* allocation needs; migrates to
  SSA identity with the global sweep (Step 9).
- **`mu_access_window.arrayId` / `mode` and `SdeArrayLayoutRootOp`** are the
  transitional value↔identity bindings (§5.3); they retire to SSA-root identity at
  Step 9, at which point even these are recompute-from-use-def.

Everything else is type or recomputed.

### 5.6 What `su_iterate` / `mu_alloc` carry at the end

- **`su_iterate`:** `lowerBounds`, `upperBounds`, `steps` (the iteration space),
  `reductionAccumulators` + `reductionKinds` (irreducible iter_args + combiner
  identity), `nowait` (irreducible source effect), and the optional
  frontend-stamped `schedule`/`chunkSize` slots; plus the reclassified dependency
  predicates (`inPlace*`) which are *queried* analyses, not layout attrs. The
  distribution choice rides on the enclosing result-forwarding `su_distribute`;
  the grain rides on the `mu_alloc` types and the movement ops. Drops from ~27
  effect/schedule/layout attrs to roughly
  `{lowerBounds, upperBounds, steps, reductionAccumulators, reductionKinds, nowait}`.
- **`mu_alloc`:** the rank-expanded `[grid…, tile…]` memref **type** is the
  layout, owner dims, and block grain (`recoverOwnerDims`); the producer↔consumer
  join is the root SSA Value (transitionally `array_layout_root` + `arrayId` until
  Step 9). No `physicalOwnerDims`, `physicalBlockShape`, `arrayLayout`, or
  `arrayId` attribute.
- **Movement** is `su.halo` / `su.reduce_scatter` / `su.all_to_all` ops whose
  operand/result types are the source/target layouts and whose radii/reduce-dim
  are operands — no `redist` op, no `SdeMovementFamily` enum, no
  `physicalHaloShape`/`accessMin/MaxOffsets` channel.

### 5.7 Adversarial-review corrections (the recompute-vs-destroy boundary)

The fate table above is ~85% correct, but a recomputability review found that
several fate-1 (recompute) assignments are **refuted by the IR**: the transform the
doc places *before* the recompute destroys the affine form the analysis needs.
`extractDimOffset` (`SuLoopAccessAnalysis.cpp:1094-1118`) parses only `dim` and
`dim + const`; after `stripMineLoop` (`Tiling.cpp:1250-1289`) the IV becomes
`outer*tileStep + inner`, so offset/owner/topology recompute returns nothing, and
the code already falls back to the committed attribute
(`hasRealizableOwnerStrip` reads `getPhysicalOwnerDimsAttr()`, `SuLoopAccessAnalysis.cpp:1271-1280`).
This does not break the no-contract goal — it **relocates** the boundary between
fate 1 and fate 2:

- **`iterationTopology`, `physicalOwnerDims`, `accessMin/MaxOffsets` are fate-2,
  not fate-1, *after tiling*.** They are read back from the rank-expanded MU **type**
  (`recoverOwnerDims`) and the `su.halo` op (radii), which is valid precisely
  because tiling commits that type/op at the moment it destroys the affine loop
  form. Pre-tiling consumers (DistributionPlanning choosing halo radii) still query
  the analysis (fate 1); post-tiling consumers read the type/op. There is no
  post-tile loop-recompute regime.
- **`structuredClassification`/`pattern` on a *fused* op is emit-as-structure, not
  recompute.** `ElementwiseFusion.cpp:542-560` *forces* `elementwise_pipeline`
  because the pipeline-ness is the fusion *act*, not the merged static shape;
  re-running `classifyPattern` on the fused body need not return it. The fused op
  must be structurally distinguishable (its body literally the chained stages, from
  which `isOwnerLocalPipelineReduction` re-derives the class) — fate 2.
- **`mu_access_window.mode` is fate-1 only *after* Steps 4-5.** Its split
  (`cuNeedsSplitHaloRead`, `MuAccessWindow.cpp:249-269`) transitively reads
  `physicalHaloShape` and `layoutsDisagree` — both being deleted. It becomes
  self-contained only once those are a `su.halo` op (Step 4) and a producer≠consumer
  type (Step 5); the row must carry that sequencing dependency.
- **`logicalWorkerSlice` is fate-1 (recompute), not a new CU-group field.** The
  ratio is `span / blockSize`, and `SdeToArtsBoundary.cpp:4008` already checks
  `span % blockSize == 0` — so it is recomputable from the two recovered grids (CU
  span ÷ DB block extent), not a count stamped on a new op (which would just be a
  contract under another name).
- **`schedule`/`chunkSize` are an optional source *hint*, a fourth bucket — not
  fate-3 irreducible.** The compiler synthesizes cost-gated defaults when absent
  (`ScheduleRefinement.cpp:118-137`), so they are not irreducible source semantics
  like `nowait` or the reduction combiner identity; keeping them is fine, calling
  them irreducible blurs the rule. Genuinely irreducible (fate 3): `reductionKinds`
  + reassociation license, and `nowait`.
- **Cost honesty.** `SuLoopAccessAnalysis` is **not** cached today (no analysis-
  manager registration); the "cached per op" framing is aspirational. Dropping the
  attribute means ~106 call sites each trigger a fresh single-body affine walk —
  bounded and acceptable (linear per small body), but the design must either
  register a real invalidate-on-mutation cached analysis or state plainly that the
  recompute is per-call.

**The deeper principle this exposes:** "recompute on demand" is not universal —
a transform that rewrites the IR into a form the analysis can no longer read forces
the fact it computed to be **emitted as structure at that moment** (the type, or an
op), not re-derived later. So the no-contract architecture is: facts whose source
form *survives* stay analyses (fate 1); facts whose source form a transform
*destroys* are emitted as type/op by that transform (fate 2). Either way, **no
attribute is stamped for a later pass to read** — the goal holds; the line between
fate 1 and fate 2 is drawn by what each transform destroys.

### 5.8 Two directives resolved: re-runnable parallelize, and layout-as-type

This section refines §5.2 against two directives and the IR that backs them. The
first is clean. The second is sound in intent but its literal phrasing in §5.2
("layout-assignment commits the chosen block layout as the rank-expanded mu_alloc
TYPE") is **refuted by the live data flow** and is corrected here: layout-assignment
decides the *owner geometry*, but the *block extent* (the DB grain) is genuinely a
cost-model decision unavailable until tiling, and the `mu_alloc` op does not exist
until ten passes later. The fix keeps the no-contract goal by splitting one decision
across two sites and committing each as structure at the site that has the
information — never as an attribute a later pass reads.

#### 5.8.1 Directive 1 — `parallelize` is a separate, re-runnable promotion

`raise-to-sde` is **purely structural**. It turns the missing (non-omp) sequential
`scf`/`affine` nests into CU/MU/SU units, folds `cu-normalization` for residual
containment, and wraps every raised leaf CU conservatively as `cu_region<single>`.
It runs no dependence proof, chooses no parallelism, touches no layout. (Today
`CuNormalization.cpp` is already exactly this structural single-wrapper —
`state/CuNormalization.cpp` header, no metadata written; the §5.2 core folds its (b)
and (c) in.)

`parallelize` is the **analysis-driven PROMOTION**, a distinct pass. Its promotion
criterion is the existing dependence proof in `matchParallelNest`
(`dep/loop/Parallelize.cpp:225-371`): a perfectly-nested rectangular nest whose
external stores are IV-indexed with no repeated physical dim, where no load reads a
written root (no loop-carried RAW/WAR/WAW), and whose scalar carriers are closed-form
counters. When the proof passes, parallelize promotes the leaf CU
`single` → `parallel`.

**The hardcoded-single bug is the inverse of this rule.**
`buildSuIterateForParallelNest` consumes the just-proven-independent nest and then
stamps `SdeCuKind::single` on its leaf CU (`Parallelize.cpp:638`). Because
`hasParallelLeafCu` returns true only for `SdeCuKind::parallel`
(`SdeLoopPatternFacts.cpp:35-44`), that proven parallelism is invisible to
pattern-facts, `ElementwiseFusion` (`:249`), interchange, tiling, and layout — it is
silently discarded into serial. The fix is one site: at `Parallelize.cpp:638` emit
the proof-derived kind (`parallel` for the nest `matchParallelNest` just proved).
Leave `:438` and `:478` as `single` — they are the legitimate residual-serial sibling
and in-place wrappers, not proven-independent nests. The bug restated as a rule:
**parallelize must actually promote when it proves independence.** A companion
verifier rejects a `cu_region<single>` directly inside a multi-trip `su_iterate`
body unless it is tagged serial-by-proven-dependence (the Gauss-Seidel /
`inPlaceSharedState` case from §5.4).

**Idempotence and re-entrancy** are already structurally enforced and need no new
attribute. `matchParallelNest` bails when the nest's ancestor is an
`SdeSuIterateOp` (`Parallelize.cpp:226-227`) and when its parent is another `scf.for`
(`:228-229`, top-of-chain only); `findNextParallelNest` walks all `scf.for` but the
ancestor guard skips anything already raised (`:744-756`). This yields two
properties:

- **Re-run on already-raised IR is a no-op.** The raised body's residual `scf.for`
  lives under an `su_iterate` and is skipped.
- **Re-run after a rewrite exposes new parallelism.** Tiling, fusion, and
  decomposition introduce fresh top-level `scf` bands not yet under an `su_iterate`;
  a later `parallelize` invocation catches them.

Promotion is **monotone** (`single` → `parallel`, never the reverse) and is a no-op
when the leaf is already `parallel`, so scheduling `parallelize` multiple times is
safe. The one caveat is the proof's input form: after tiling rewrites an IV to
`outer*T + inner`, `extractDimOffset` cannot parse it (`SuLoopAccessAnalysis.cpp:1094-1118`,
the §5.7 boundary). So a nest's parallelism must be **captured on the first eligible
run, before tiling rewrites its IV**; a post-tiling re-run promotes only nests whose
*outer* band is still in recoverable form, or it relies on the `parallel` CU kind
already committed on bands it raised earlier. It never demotes, so a re-run that
cannot re-prove a tiled nest simply leaves the earlier `parallel` kind intact — the
failure mode is "stays as already-decided," not "regresses to serial."

**Re-run points in the pipeline:** `parallelize` runs (1) immediately after
`raise-to-sde` (initial promotion over the structural skeleton), and (2) after
`loop-interchange` + `tiling` + `elementwise-fusion` (to promote bands those rewrites
exposed). An optional third invocation after `iteration-space-decomposition` is
warranted if that pass splits domains. Re-run (2) must read the *outer* (pre-`Mul`)
band or the committed `parallel` kind, per the caveat above.

#### 5.8.2 Directive 2 — layout-assignment emits structure, not a contract

Today `LayoutAssignment` is a pure fact producer doing **zero** IR-shape change. It
packs owner positions, block shape, budget block shape, and comm volume into an
`arrayLayout` `DictionaryAttr` and stamps `arrayLayout` + `layoutsDisagree` +
`commVolumeBytes` (`dep/loop/LayoutAssignment.cpp:659-664`), plus emits
`SdeArrayLayoutRootOp` joins (`:678-681`). No load, store, or type is touched. This
is the contract the directive removes. The decision it makes is real and needed —
which block layout per array, module-scoped, minimizing communication — but it must
be **emitted as structure**, with `layoutsDisagree` becoming a producer/consumer
type mismatch and `commVolumeBytes` recomputed from types. No `arrayLayout`
attribute ever exists.

**The decision splits into two facts with two fates.** The owner *geometry* (which
array dims are owner dims, their positions, and the contraction/parallel role) is
chosen module-scoped from the affine `ArrayAccessProfile` — pre-tiling information
that survives until tiling. The block *extent* (the DB grain) is a different number:

> **Refuted premise (verified at `Tiling.cpp:493-532`).** The committed block
> extent is `ceilDivPositive(shape, workerGrid)` where
> `workerGrid = factorWorkersAcrossDims(getTargetTileTasks(op, costModel), …)`
> (`:493-501`). The budget block shape that `LayoutAssignment` computes is applied
> only as a `min()` **cap** (`:514-518`), not as the extent. Then
> `plan.tileIterations[loopDim] = tile` (`:532`) sets the loop tile to the *same*
> number. So the live pipeline decides the DB grain **at tiling, from the
> cost-model worker count**, and `LayoutAssignment`'s extent is a coarse
> `kAbstractBlockFactor = 2` proxy (`LayoutAssignment.cpp:53,188`) that
> `reconcileArrayLayoutWithCommittedPhysicalShape` (`Tiling.cpp:871`) later
> overwrites. The hypothesis that layout-assignment can emit the *final* extent
> type is therefore false: that extent is not known there, and the number it does
> have is a placeholder that gets clobbered.

A second hard fact compounds it: there is **no `mu_alloc` to rewrite at
layout-assignment time.** `sde.mu_alloc` is first created by
`MemoryUnitRealization` from the flat logical memref type
(`state/MemoryUnitRealization.cpp:392-395`), which runs ~10 passes after
`LayoutAssignment` (`tools/compile/Compile.cpp:1146` vs `:1156`). Layout-assignment
literally cannot "rewrite each `mu_alloc` result type" — the op does not exist yet.

**The resolution** keeps the directive's goal (storage grid = type, no attribute) by
separating the *decision point* from the *type-materialization point*, which the
current code already separates across passes:

1. **Owner geometry — decided at layout-assignment, held as a transient analysis.**
   Keep PhaseA–D (`buildModuleSuAccessRelations` → `enumerateCandidates` →
   `assignLayout`, `LayoutAssignment.cpp:534-650`) as the comm-minimizing owner-dim
   chooser, including the module-scoped minimization (`estimateCommVolume`,
   `:280-363`) that picks which parallel/contraction position is the owner across all
   sibling consumers. Its output `{root → {ownerPositions, budgetBlockBytes, kind,
   contractionPosition}}` is held in an **in-memory** structure and exposed as an
   analysis accessor recomputed from `ModuleSuAccessRelations` on demand — it is pure
   over the access profiles. Tiling and distribution-planning query it where they
   today read `writeLayout->ownerDims` / `budgetBlockShape` (`Tiling.cpp:503-519`).
   This is fate 1: the loop + access form survives until tiling, so the owner choice
   is recomputable. **It is committed to no attribute.**

2. **Block extent + storage type — materialized where the cost model and the
   `mu_alloc` both exist.** The rank-expanded `[grid…, tile…]` type is built by
   `buildExpandedMuType` from `{ownerDims, blockExtents}`
   (`Utils/MuLayout.cpp:125-146`) — it never inspects a loop IV, and
   `recoverOwnerDims` reads it back from the type alone (`:148-183`). That build
   folds forward to the moment the extent is decided: tiling/distribution-planning
   compute the worker-derived grid, and the `mu_alloc` (flat from
   `MemoryUnitRealization`) is rank-expanded **directly to the committed type** via
   the existing `MuLayoutRewriter::apply` (`Utils/MuLayoutRewriter.cpp:713-797`,
   which also rewrites the CU loads/stores through `indexer.localize` div/mod and
   fails closed on any unsupported use). `kAbstractBlockFactor`, `physicalOwnerDims`,
   `physicalBlockShape`, `iterationTopology`, and `arrayLayout` are all deleted; the
   only surviving fact is the type. `recoverOwnerDims(type)` is the single reader.

3. **`layoutsDisagree` → producer-type ≠ consumer-type.** Commit the consumer's
   required-read layout as the consumer `mu_alloc` type too (the self-edge fix:
   `RedistributionEdges` currently hardcodes target = source). Disagreement is then a
   type compare at edge construction, and movement (`su.halo` / `su.reduce_scatter`)
   is emitted where operand type (producer grid) ≠ result type (consumer grid). The
   `setLayoutsDisagreeAttr` stamp, the `sde-redistribute` erase, and the boundary
   leftover-reject all disappear.

4. **`commVolumeBytes` → recompute from types.** `estimateCommVolume` is pure over
   shape / element bytes / block factor; `DistributionPlanning` already recomputes
   comm from `shape + physicalBlockShape` independently. It becomes a cost-model query
   over `(producer type, consumer type)` at the partition/edge site. No op-level
   commit.

**Does `rank-expand-mu` survive as a separate pass? No.** Its
`buildExpandedMuType` + `MuLayoutRewriter::apply` call (today driven by reading
`physicalOwnerDims`/`physicalBlockShape` off the writer,
`state/RankExpandMu.cpp:56-86`, `MuLayoutRewriter.cpp:152-160`) folds into the two
transforms that commit the physical grid and run at/after `mu_alloc` materialization
(tiling / distribution-planning, with the expansion landing at or just after
`MemoryUnitRealization`). `buildExpandedMuType`, `MuLayoutRewriter`, and
`recoverOwnerDims` remain as the shared library those passes call; the standalone
`SdeRankExpandMu` pass (`Compile.cpp:1168`) is subsumed — there is no flat-then-expand
round trip and no `physicalOwnerDims` attribute to read between them.

#### 5.8.3 DB grain ≠ CU grain: the hypothesis, resolved

The hypothesis — *layout-assignment emits the storage owner/block grid as the
`mu_alloc` type (DB grain); tiling tiles the CU loops (dispatch/worker grain) without
changing that type* — **holds for the owner geometry and the storage/dispatch split,
but not for the timing of the extent.** The corrected split:

- **Storage grain = the `mu_alloc` TYPE.** The rank-expanded `[grid…, tile…]` type
  carries the owner dims and block extent. It is immune to tiling's IV rewrite
  because `recoverOwnerDims` reads the type, never the loop
  (`MuLayout.cpp:148-183`). This is the charter's DB grain, and it is a type, not a
  contract.
- **Dispatch grain = the CU loop nest.** Tiling strip-mines the band
  (`stripMineLoop`, `Tiling.cpp:1250-1289`) — the tile extent is the loop step. This
  is the charter's CU grain.
- **The CU-over-DB ratio is recomputed, not stamped.** Where one CU dispatch covers
  several DB blocks, the ratio is `span / blockSize`; the boundary already enforces
  `span % blockSize == 0` and derives `groupBlockCounts = span / blockSize`
  (`arts/Transforms/SdeToArtsBoundary.cpp:4003-4010`). So `logicalWorkerSlice` is
  recompute (§5.7), not a new CU-group field.

Where the hypothesis is **wrong**, and the corrected split: the directive imagined
*layout-assignment* committing the storage type. But the extent it would need is the
worker-derived `ceilDiv(shape, workerGrid)` computed at tiling (`Tiling.cpp:493-501`),
and the `mu_alloc` it would rewrite is minted ten passes later. The corrected split
is therefore **decision at layout-assignment (owner geometry + node-agnostic budget
seed, held as a transient analysis), type materialized at tiling / realization (the
worker-derived extent folded into the rank-expanded `mu_alloc` type).** This is the
exact instance of the §5.7 principle: tiling destroys the affine IV form, so the
storage type must be committed at-or-by tiling — but the *owner choice that seeds it*
is decided earlier, from the intact affine maps, and never written to IR. The two
grains live at two IR objects (the `mu_alloc` type and the loop nest), committed at
two sites, with no attribute bridging them.

A consequence the charter demands and the current code violates: today
`Tiling.cpp:500` and `:532` set the storage block extent and the loop tile to the
**same** number, collapsing the two grains. Honoring DB-grain ≠ CU-grain means the
worker tile and the DB block extent become two independent numbers, reconciled by the
`min()` cap and the `span % blockSize == 0` check, not forced equal. Every owner-local
kernel where they were implicitly equal must be re-validated against the decoupled
form.

#### 5.8.4 The full revised order with both changes

This supersedes the §5.2 listing where they differ (the layout-emits-type line in
§5.2 is corrected per §5.8.2):

```
1.  convert-openmp-to-sde      FRONTEND: raise omp.* via sde::buildSuIterate;
                               source-only schedule/chunk/nowait/reductionKind. No facts.
2.  raise-to-sde               STRUCTURAL: missing scf/affine -> CU/MU/SU; fold
                               cu-normalization; leaf CU = cu_region<single>; rank-1
                               -> rank-N owner-loop promotion as structure. ZERO attrs.
3.  parallelize  [RE-RUN #1]   PROMOTION: prove independence (matchParallelNest) ->
                               cu_region<single> -> <parallel>. Fix at :638. Idempotent.
4.  layout-assignment          DECISION (transient): module-scoped owner geometry +
                               budget seed from affine maps. Commits NO attribute;
                               exposed as an on-demand analysis. layoutsDisagree and
                               commVolumeBytes are deleted (recomputed from types later).
5.  loop-interchange           LOOPS ONLY: permutation; no fact-bundle re-stamp.
6.  tiling                     LOOPS + TYPE: strip-mine CU band (tile = loop step);
                               compute worker-derived block extent; fold rank-expand-mu
                               -> rank-expanded mu_alloc TYPE. No physical*/topology/
                               logicalWorkerSlice attrs. DB grain (type) != CU grain (loop).
7.  parallelize  [RE-RUN #2]   PROMOTION on newly exposed bands (post interchange/
                               tiling/fusion). Reads OUTER band or committed <parallel>
                               kind, never the post-tile outer*T+inner inner IV.
8.  elementwise-fusion         LOOPS ONLY: fuse; fused op structurally distinguishable
                               (no fact-bundle propagation). [optional RE-RUN #3 after
                               iteration-space-decomposition]
9.  distribution-planning      REAL: su_distribute(+kind), realizeWavefrontSkew, emit
                               su.halo/su.reduce_scatter. Also a type/extent commit site
                               for its plans (same split as tiling). No physical* stamps.
10. iteration-space-decomposition
11. memory-unit-realization    TYPE MATERIALIZES: memref.alloc -> sde.mu_alloc minted in
                               the rank-expanded type from the committed grid. RankExpandMu
                               subsumed; no flat-then-expand round trip; no arrayId stamp.
12. atomic-reduction-realization / barrier-elimination   REAL, kept.
13. sde-to-arts boundary       queries recoverOwnerDims(type), queryAccessWindows,
                               classifyBarrierSync, SuLoopAccessAnalysis at lowering.
```

Re-run points: `parallelize` at step 3 (initial) and step 7 (post-rewrite),
optionally after step 10. The storage type is fixed by step 6/11 and is never mutated
by a re-run, so re-running `parallelize`/`tiling` to capture new parallelism re-tiles
loops over the **same** `mu_alloc` type — idempotent and layout-preserving by
construction.

#### 5.8.5 What `su_iterate` / `mu_alloc` carry now

Leaner than §5.6, because layout is the type and parallelism is the CU kind:

- **`su_iterate`:** `lowerBounds` / `upperBounds` / `steps`, `reductionAccumulators`
  + `reductionKinds` (fate 3), `nowait`, optional source `schedule`/`chunkSize`
  (hint), and the queried `inPlace*` dependency predicates. **No `arrayLayout`, no
  `layoutsDisagree`, no `commVolumeBytes`, no `physicalOwnerDims`, no
  `physicalBlockShape`, no `iterationTopology`, no `logicalWorkerSlice`.** Parallelism
  is the leaf `cu_region` *kind* (`single` / `parallel`), set by proof, not an
  attribute. The owner choice is a transient analysis; the comm cost is recomputed
  from types; the dispatch grain is the loop step plus a recomputed `span / blockSize`
  ratio.
- **`mu_alloc`:** the rank-expanded `[grid…, tile…]` memref **type** is the layout,
  owner dims, and block grain (`recoverOwnerDims`). Producer↔consumer join is the
  root SSA Value. No layout attribute of any kind.
- **Movement / disagreement:** `su.halo` / `su.reduce_scatter` / `su.all_to_all`
  whose operand type (producer grid) ≠ result type (consumer grid). Disagreement is a
  type compare; comm volume is recomputed from the two types.

### 5.9 Three directives, one root cause: run the SDE pipeline on the affine form

§5.7 and §5.8 are correct about *what* the no-contract pipeline must hold, but both
treat one thing as a fixed law of physics: that the affine loop form is gone by the
time SDE planning runs, so `SuLoopAccessAnalysis` must re-derive access relations from
`scf` + `memref` index arithmetic, and `extractDimOffset` cannot read the `outer*T +
inner` form tiling produces. This section refutes that premise. The affine form is
**not** intrinsically gone: it is destroyed by one pass call in the wrong place, and
restoring it dissolves three problems at once.

The three directives this resolves:

- **D1.** `parallelize` re-runs multiple times, and the re-run *after* loop-interchange
  / tiling / fusion must be **fully** effective, not the partial "outer-band-only"
  fallback §5.8.1 concedes (lines 818-832).
- **D2.** `commVolumeBytes` is **eliminated**, not recomputed. §5.8.2 item 4 and the
  §5.4 row still recompute an abstract byte estimate from types; this section deletes
  the estimate itself, because layout is a **structural derivation** with nothing to
  score.
- **D3.** The bespoke access / dependence / offset analysis is **replaced** by the
  upstream MLIR affine analyses, not re-implemented. The 1433-line
  `SuLoopAccessAnalysis` exists only because the pipeline lowers affine too early and
  then re-derives it badly.

These are not three fixes. They are one fix (keep affine, use the upstream analyses)
with three payoffs.

#### 5.9.1 The root cause, verified: affine is lowered before any SDE pass runs

The frontend emits affine. `cgeist` is invoked with `--raise-scf-to-affine`
(`tools/scripts/compile.py:480`, `:964`), so the `.mlir` handed to `carts-compile` is
`affine.for` / `affine.load` / `affine.store` over `memref`, with a tiled or
strided subscript carried as a first-class `AffineExpr` (`outer*T + inner` is just an
`AffineMap` result).

CARTS then discards that form in the very first functional pass of the very first
stage. `createLowerAffinePass()` runs in `buildSdeInputNormalizationPipeline`
(`tools/compile/Compile.cpp:1112`) and **again** in `buildInitialCleanupPipeline`
(`:1126`). Both stages run before `buildSdePlanningPipeline` (invocation order
`:1380` → `:1388` → `:1395`). Every SDE loop pass — `parallelize` (`:1140`),
`loop-interchange` (`:1147`), `tiling` (`:1148`), `elementwise-fusion` (`:1149`) —
therefore sees `scf` + `memref`, never affine. There is no `scf` → affine raise
anywhere between the frontend and SDE planning. The only later affine lowering of note
is the legitimate post-planning one at `Compile.cpp:1303`.

That single misplaced pass call is the entire reason for the rest of §5.7's
"recompute-vs-destroy boundary." `SuLoopAccessAnalysis` (1433 lines) hand-rebuilds an
`AffineMap` from `scf` IVs and `arith` index ops (`tryGetAffineExpr` /
`tryBuildIndexingMap`, `SuLoopAccessAnalysis.cpp:357-413`), and it does so
incompletely:

- `tryGetAffineExpr` admits `arith.muli` only when one operand is a constant
  (`:388-392`) and has **no** `divsi` / `remsi` case, so a dynamic stride or a
  block-localized `i floordiv T` / `i mod T` returns `nullopt` and the whole summary
  bails at the first unparseable index (`:407-413`).
- `extractDimOffset` (`:1094-1118`) walks the rebuilt expression and handles only
  `AffineDimExpr`, `AffineConstantExpr`, and the `Add` case; its switch is `default:
  return nullopt` (`:1115-1116`), so a `Mul` (the `dim*T` of a tiled IV) or a
  `FloorDiv` / `Mod` is invisible.

This is exactly why a stencil halo dissolves into div/rem with zero access-windows
after rank-expand, why owner recovery falls back to the committed attribute
(`getPhysicalOwnerDimsAttr`, `:1271-1280`), and why the directive's post-tiling
re-run cannot re-prove a tiled nest. None of it is intrinsic. It is the cost of
reading hand-lowered `scf` index arithmetic instead of the `AffineMap` the frontend
already computed.

#### 5.9.2 USE-UPSTREAM-ANALYSIS: the replacement table (D3)

Every analysis CARTS hand-rolls has an in-tree upstream equivalent that operates on
`affine.for` / `affine.load` / `affine.store` and treats `outer*T + inner`,
`floordiv`, and `mod` as native `AffineExpr`s. All are present under
`external/Polygeist/llvm-project/mlir/include/mlir/Dialect/Affine/`.

| CARTS bespoke code (DELETED) | Upstream replacement | What it fixes |
| --- | --- | --- |
| `tryGetAffineExpr` / `tryBuildIndexingMap` (`SuLoopAccessAnalysis.cpp:357-413`) | `MemRefAccess` + `getAccessRelation` building a presburger `IntegerRelation` (`Analysis/AffineAnalysis.h:82`, `:119`) | The access relation is read off the `affine.load/store` map; `%i0 + 2*%i1` and tiled `outer*T+inner` are first-class (header docstring `:100-119`). No reconstruction, no Mul-blindness. |
| `extractDimOffset` Add-only offset walk (`SuLoopAccessAnalysis.cpp:1094-1118`) | per-dim offset bounds from the access relation via `FlatAffineValueConstraints` / `ValueBoundsConstraintSet` (`Analysis/AffineAnalysis.h:32`; `Interfaces/ValueBoundsOpInterface.h`) | Halo radii / owner offsets are bounded structurally regardless of div/mod/mul. The `default: nullopt` failure (`:1115-1116`) and the `getPhysicalOwnerDimsAttr` fallback (`:1271-1280`) disappear. |
| `matchParallelNest` / `isLoopIndexedStore` IV-equality proof (`Parallelize.cpp:137-167`, `:320-372`) | `isLoopParallel(forOp, &reductions)` (`AffineAnalysis.h:53`) + `checkMemrefAccessDependence` (`:172`) at the relevant depth | Polyhedral RAW/WAR/WAW replaces the conservative "no load from a written root" over-approximation; `i+2*j` and tiled stores are admitted that `sameValue(index, iv)` (`:155`) wrongly rejects. |
| reduction detection inside `Parallelize` (`:344-354`) | `isLoopParallelAndContainsReduction` (`Analysis/Utils.h:295`) + `getSupportedReductions` (`AffineAnalysis.h:47`) | Reduction carriers are enumerated by the same analysis that proves parallelism. The scalar closed-form counter idiom (`Parallelize.cpp:283-353`) is kept — it is loop-carried scalar state, not a memref reduction. |
| `stripMineLoop` hand-built `scf` tile nest (`Tiling.cpp:1250-1289`) | `tilePerfectlyNested` / `tilePerfectlyNestedParametric` (`LoopUtils.h:105`, `:112`) | The tile encodes `outer*T+inner` as canonical `AffineExpr`s that survive subsequent access analysis, so tiling is no longer a one-way street. |
| bespoke `Interchange` (`Interchange.cpp`, scf-only) | `interchangeLoops` / `permuteLoops` / `sinkSequentialLoops` (`LoopUtils.h:118`, `:133`, `:140`) | Band reorder on affine; the reordered nest stays analyzable. |
| nest collection / IV recovery scattered through both files | `getPerfectlyNestedLoops` (`LoopUtils.h:70`), `getAffineForIVs` (`Analysis/Utils.h:276`), `extractForInductionVars` (`AffineOps.h`) | One band builder shared by promotion, interchange, and tiling. |
| `su_iterate` raise target | `affineParallelize` (`Utils.h:52`) supplies the proven-parallel + reduction-carrier facts the raise consumes | The proof produces the canonical parallel form instead of recomputing one. |

**The affine-lowering fix.** Do not lower affine at `Compile.cpp:1112` / `:1126`. Two
framings, in preference order:

- **(A1) Keep affine through SDE planning.** Remove the two pre-planning
  `createLowerAffinePass()` calls; rewrite `SdeInputNormalization` to *normalize* the
  affine form (affine canonicalize / `normalizeAffineParallel` /
  `simplifyAffineStructures`) instead of lowering it; lower to `scf` once, at the end
  of planning (the existing `Compile.cpp:1303` site, or just before
  memory-unit-realization where div/mod localization needs concrete `scf`). The SDE
  loop passes then call the upstream affine analyses directly. This is a deletion +
  relocation of an existing pass call, not new IR; it is what removes the §5.7
  destroy-boundary at its source.
- **(A2) Fallback.** If full retention is too invasive for one step, retain affine
  only across the proof (a thin affine sub-region for the nests being promoted) before
  the global lowering. (A1) is the intended end state; (A2) is a staging crutch.

**Residual non-affine.** `--raise-scf-to-affine` is not total: dynamic-bound loops
(`atoi`/`argv` sample sweeps), `affine.if` with non-affine conditions, and jagged
pointer-array memrefs degrade to `scf` even from `cgeist`. `getAccessRelation` returns
failure on these. The new analysis must **fail closed** to a serial/coarse fate on
failure (exactly as the current hand-rolled walk bailed to `nullopt`), never crash and
never silently treat failure as "no dependence." A reduced `scf` path for un-raised
loops stays as the conservative fallback.

**What is deleted.** `tryGetAffineExpr`, `tryBuildIndexingMap`, `extractDimOffset`,
and the `scf`+`arith` reconstruction core of `SuLoopAccessAnalysis`
(`SuLoopAccessAnalysis.cpp:357-413`, `:1094-1118`, and the offset/owner scans that
depend on them); the `isLoopIndexedStore` IV-equality dependence proof and the
"no load from a written root" scan in `Parallelize.cpp` (`:137-167`, `:320-372`);
`stripMineLoop` (`Tiling.cpp:1250-1289`) and the bespoke `Interchange` body. The files
shrink to a thin SDE-specific adapter over the upstream analyses (form the band, query
`getAccessRelation` / `isLoopParallel` / `checkMemrefAccessDependence`, map the result
onto CU/SU/MU structure). The queries are transient in-memory presburger calls — no
attribute, no fabricated number, compliant with the HARD RULES.

#### 5.9.3 NO `commVolumeBytes`: layout-assignment as structural derivation (D2)

D2 supersedes §5.8.2 item 4 and the §5.4 "recompute from types" row. The directive is
not "recompute the abstract byte cost later"; it is "there is nothing to compute,
because layout is not a search."

**The byte cost exists only to score a search.** `assignLayout` enumerates candidate
layouts (`LayoutAssignment.cpp:202-265`), scores each with `estimateCommVolume`
(`:280-363`, a closed-form over `kAbstractBlockFactor = 2` at `:53`, `:188`), and takes
the argmin (`:398-442`). The chosen score is stamped as the `commVolumeBytes` IR
attribute (`SdeOps.cpp:1103`; committed at `LayoutAssignment.cpp:438`, `:664`) and read
downstream by `LayoutGraph` (`:115-116`, `:306`, `:444`, `:463`), `RedistributionEdges`
(`:537-538`), `Redistribute` (`:100`), `DistributionPlanning` (`readAbstractCommVolumeBytes`,
`:872`), `CuMuGraphPartitioning` (`:484-550`), and forwarded (never branched on) at the
ARTS boundary (`SdeToArtsBoundary.cpp:1564`). It is both a fabricated number and an
attribute stamped for a later pass — the two HARD RULE violations, same family as the
already-deleted `getL2CacheSize` constant (`ARTSCostModel.h:61`).

**The decision is already structural; the score already loses to it.** The candidate
set is built purely from access relations: the owner candidate is the
`ArrayDimKind::parallelIndexed` writer dims (`LayoutAssignment.cpp:97-126`), the
contraction candidate is the `reductionIndexed` position (`:131-138`), the stencil
candidate keeps the full writer owner tile. The argmin is then overridden by structure
before any byte comparison decides anything: the contraction candidate is forced to
`selectionCost = 0` (`:408-411`), replicated is forced to `INT_MAX/4` when a full-rank
writer feeds a stencil (`:415-417`), and ties break on a fixed layout-kind ladder
(`:420-433`). The byte sum is window-dressing made to agree with a conclusion the
`ArrayDimKind` classification already reached.

**The structural derivation rule (no candidates, no score).** Replace the
enumerate-and-score body of `assignLayout` with a direct derivation from the affine
access relations:

- If a full-rank parallel **write** exists: owner dim = the parallel-written dim
  (HPF `DISTRIBUTE` on the owner-computes axis). Kind = `blockParallel`.
- Else if the array is a sibling-distributed intermediate consumed on a contraction
  axis: owner = the contraction position, **unsplit** on the parallel axis. Kind =
  `blockContraction`.
- Else (pure input, no writer): owner = the dim a parallel reader indexes (ALIGN
  target). If two competing parallel axes exist, the tiebreak is **structural** —
  own the outer (lowest-index) or largest-extent dim — never a byte count.
- Stencil readers keep the full writer owner tile; halo width = the offset range of the
  read access relations versus the write relation, read directly from the relation, not
  scored.
- Replicated is chosen **only** when there is no parallel write, no parallel read, and
  no contraction consumer (scalar / broadcast-only) — a structural predicate, not
  "cheapest candidate."

The two structural overrides (`:408-411`, `:415-417`) become explicit branch
predicates in this rule — ported as **hard fail-closed gates**, not soft preferences,
because a missed case silently falls back to replicated, which is the known
scaling-death (coarse `host_whole`) mode.

**Residual disagreement is a movement op, not a pre-scored number.** Where a producer
owner layout differs from a consumer's required layout, emit a movement op
(`su.halo` / `su.reduce_scatter` / `su.all_to_all`) at the consumer edge by comparing
producer type to consumer required type — the §5.8.2 self-edge fix. The downstream
branches that look like they need the byte count actually test only `commVolumeBytes >
0` (`RedistributionEdges.cpp:537`, `Redistribute.cpp:100`): that boolean is
`layoutsDisagree`, now a producer-type ≠ consumer-type compare, not a magnitude.

**Every consumer removed or reworked, with no recompute left behind.**

- `estimateCommVolume`, `kAbstractBlockFactor`, the candidate-scoring argmin, and the
  `commVolumeBytes` commit (`LayoutAssignment.cpp:53`, `:280-363`, `:398-442`, `:438`,
  `:659-664`) are **deleted**.
- The op attribute and its parse/print are deleted (`SdeOps.cpp:1030`, `:1103`).
- `LayoutGraph`'s `commVolumeBytes` net field and parse (`:115-116`, `:156-157`,
  `:444`, `:463`) collapse to the **structural** traffic proxy already present —
  `defaultMuTrafficBytes` derived from `tilePayloadBytes(blockShape)` or
  `staticShape * elementBytes` (`LayoutGraph.cpp:114-122`) — with its
  `commVolumeBytes` first-branch removed. This is a transient partition-pressure
  number derived from the **type** at the partition site, not an IR attribute and not
  an abstract byte budget.
- `CuMuGraphPartitioning` already defaults to `tileBytes` when the abstract count is
  `<= 0` (`:483-487`, `:493-494`) and the typed-hypergraph `cutBytes` path computes
  edge weight from partition geometry without reading the attribute at all
  (`:468-476`); delete the `abstractCommVolumeBytes` branch and keep these. The
  partition objective is concurrency / work-imbalance and the `minTileBytes` floor
  (`:535-539`), which is byte-attribute-independent. `hasCommunication` becomes the
  structural predicate `muBlockCount > 1`.
- `DistributionPlanning`'s `readAbstractCommVolumeBytes` (`:872-876`) is deleted;
  remote fanout already comes from `inferCuCountFromMuPartition`, and the packet term
  uses `tilePayloadBytes`.
- `RedistributionEdges`' conditional commit (`:533-538`) and the
  `SdeToArtsBoundary` copy-through field (`:1561-1564`) are deleted; the boundary
  branches only on geometry (`:1547-1559`).

No producer, no attribute, no recompute. `commVolumeBytes` disappears from the dialect.

#### 5.9.4 `parallelize` re-runnable on the affine form (D1)

D1 and D3 are the same fix. `parallelize` is already a separate, idempotent,
re-entrant pass (§5.8.1): the ancestor-is-`su_iterate` guard (`Parallelize.cpp:226-227`)
makes a re-run on already-raised IR a no-op, promotion is monotone `single` →
`parallel`, and a re-run that cannot prove a nest leaves the prior kind intact. What
§5.8.1 could not deliver is the **post-tiling** re-run, because the proof read the
hand-lowered `scf` IV and `extractDimOffset` died on `outer*T + inner`.

On the affine form that limitation is gone:

- The promotion proof is `isLoopParallel` + `checkMemrefAccessDependence` over
  `MemRefAccess` (§5.9.2). Tiling via `tilePerfectlyNested` and interchange via
  `interchangeLoops` keep the access relations as native `AffineExpr`s, so after
  tiling the relation is still `getAccessRelation`-readable and the freshly exposed
  tile bands and interchanged outer dims are **fully** re-provable. The §5.8.1 caveat
  at lines 818-832 ("captured on the first eligible run, before tiling … promotes only
  nests whose outer band is still recoverable") is **deleted**: there is no
  capture-before-tiling restriction once the proof is on affine.
- Re-run points (refines §5.8.1 / §5.8.4): (1) after `raise-to-sde`; (2) after
  `loop-interchange` + `tiling` + `elementwise-fusion` — now a full re-prover, not the
  outer-band fallback; (3) optional, after `iteration-space-decomposition`.
- Promotion sets the leaf `cu_region` **kind** to `parallel` (the §5.8.1 fix at
  `Parallelize.cpp:638`), not an attribute and not a `commVolumeBytes` companion (the
  `nullptr` already passed at the raise site stays `nullptr`). Idempotence keys on IR
  structure (already-`su_iterate` ⇒ skip), proven by a fixed-point lit test (run
  `parallelize` twice, assert byte-identical IR). `checkMemrefAccessDependence`
  returning `Failure` is treated as "not provably parallel" (stay `single`), never as
  `NoDependence`.

The directive's hypothesis is therefore confirmed end to end: using the upstream
affine analysis both removes the re-implementation (D3) **and** makes the post-tiling
re-run fully effective (D1), because the `extractDimOffset` / `dim*T` limitation only
ever existed on the hand-lowered `scf` form.

#### 5.9.5 The dissolved §5.7 tension and the corrected fate boundary

§5.7 drew the fate-1 (recompute) / fate-2 (emit-as-type) boundary at the point where a
transform "destroys the affine form the analysis needs," and named tiling as such a
transform. **That premise was an artifact of early lowering, not of tiling.** Tiling on
the affine form (`tilePerfectlyNested`) preserves the access maps; `getAccessRelation`
re-reads `outer*T + inner` directly. So the facts §5.7 forced into fate-2
*after tiling* — `iterationTopology`, `physicalOwnerDims`, `accessMin/MaxOffsets` — can
return to **fate-1 (recompute via `getAccessRelation`)** rather than being emitted as
type because the loop form was unreadable.

Two clarifications keep this honest:

- The **storage type** is still committed by tiling / realization, because the *DB
  block extent* is a worker-derived cost-model number not known at layout-assignment
  (§5.8.2 / §5.8.3, verified at `Tiling.cpp:493-532`). That commit stands. What
  changes is that the *owner geometry, halo radii, and offsets* are no longer forced
  out of the analysis by an unreadable loop — they are recomputable from the affine
  relation right up to the lowering point.
- §5.7's deeper principle survives for transforms that genuinely destroy the affine
  form (true non-affine lowering). Tiling is simply no longer one of them once it runs
  on affine. The fate row for `commVolumeBytes` is corrected from "fate-1, recompute at
  partition site" (§5.3 line 582, §5.8.2 item 4) to **eliminated** — there is no fate,
  because there is no fact.

#### 5.9.6 The leaner end state

This refines §5.8.5. `su_iterate` carries essentially nothing about layout or
communication:

- **`su_iterate`:** `lowerBounds` / `upperBounds` / `steps`, `reductionAccumulators` +
  `reductionKinds` and reassociation license (fate 3), `nowait` (fate 3), optional
  source `schedule` / `chunkSize` (hint), and the queried `inPlace*` predicates.
  **No `arrayLayout`, no `layoutsDisagree`, no `commVolumeBytes`, no
  `physicalOwnerDims` / `physicalBlockShape` / `iterationTopology` /
  `logicalWorkerSlice`.** Parallelism is the leaf `cu_region` kind (`single` /
  `parallel`), set by the upstream proof.
- **`mu_alloc`:** the rank-expanded `[grid…, tile…]` memref **type** is the layout,
  owner dims, and block grain (`recoverOwnerDims`).
- **Layout = the type. Parallel = the CU kind. Movement = a `su.*` op whose operand
  type ≠ result type.** Every analysis behind these is upstream MLIR affine on the
  form the frontend already produced; nothing is a fabricated number; nothing is an
  attribute stamped for a later pass. The 1433-line `SuLoopAccessAnalysis`, the
  `commVolumeBytes` cost search, and the §5.7 post-tiling destroy-boundary all
  collapse out of the same single change: stop lowering affine before the SDE pipeline
  runs.

**Landing order (each build + lit gateable).** (1) `Parallelize.cpp:638`
`single` → `parallel` + the companion verifier (smallest blast radius, no affine
dependency). (2) Delete `commVolumeBytes` (D2) — the structural fallbacks already exist
in every consumer; regenerate, do not hand-edit, the lit `.mlir` CHECK lines that pin
`commVolumeBytes = N` (use `grep -rn`; `rg` misses `.mlir`). (3) Relocate
`LowerAffinePass` out of the pre-planning stages (A1). (4) Swap `SuLoopAccessAnalysis`
/ `Parallelize` / `Tiling` / `Interchange` onto the upstream affine analyses. (5) Add
the post-tiling `parallelize` re-run. The 1n/64t large gate (all 21 `Correct=YES`) and
the SDE lit suite are the regression fences; the elementwise / stencil / jacobi2d rows
are the ones that should newly stay `parallel` after tiling.

## Part 6 — CUs async by default; `su_barrier` as the only explicit sync

The fourth goal: make a CU carry **no implicit ordering from textual position**.
Ordering between CUs comes from exactly two sources — an explicit `sde.su_barrier`,
and the per-block data dependencies ARTS derives from committed
`sde.mu_access_window` facts. This is not a new model bolted on; it completes a
semantics the tree already half-enforces. `VerifySde` already fails closed on any
conflicting sibling `sde.cu_work` that relies on hidden textual order
(`VerifySde.cpp:206-237`, rule stated at `:22-24`): textual order is therefore
*already* not load-bearing for siblings — non-conflicting CUs are already logically
async, and conflicting ones already require an explicit ordering edge. The only thing
"sequential" today is the frontend, which mints an implicit `su_barrier` after every
non-`nowait` region and then erases the redundant ones. Async-by-default inverts that:
emit no barrier, and justify each survivor.

### 6.1 The model — `cu_region` / `su_iterate` execution contract restated

Within a single block, two CUs are **unordered** unless either (a) an
`sde.su_barrier` sits between them, or (b) their committed `sde.mu_access_window`
facts establish a per-block RAW/WAR/WAW that ARTS lowers to an EDT dependency. There
is no third source. The textual sequence of CUs and `su_iterate` bands in an SU body is
a canonicalization convenience, not a happens-before edge.

This is exactly the contract `VerifySde` enforces today and that the dialect shape
already supports: `SdeCuRegionOp` carries a `nowait` `UnitAttr` but no `depends-on`
operand (`SdeOps.td:228-265`); `cu_region` is `SingleBlock` + `RecursiveMemoryEffects`,
*not* `IsolatedFromAbove`, and `cu_work` is `IsolatedFromAbove` and communicates only
through `sde.mu_token` operands (`SdeOps.td:574-613`). `cu_region` bodies stay
scheduling-free (no SU op inside — `SdeOps.td:242-244`). So inter-CU ordering can come
only from enclosing SU structure, an explicit `su_barrier`, or the token/effect facts a
later analysis raises into windows. The dialect deliberately has **no generic
token/dataflow dependency graph** in SDE — `VerifySde` rejects a `su_barrier` that
carries `$tokens` (`VerifySde.cpp:152-164`), and `SdeControlTokenOp`
(`SdeOps.td:289-302`) is legal only as a `su_barrier` operand. Async-by-default must
therefore express *all* residual ordering as either a plain `su_barrier` or SU
structure; it must not introduce a token graph.

ARTS derives the data deps from windows, not from a barrier. At the boundary each
`sde.mu_access_window` becomes a `DbAcquireOp` whose mode is `in→RO`, `out/inout→EW`
(`DbUtils.cpp:744-745`, stamped by `DbCommitDistributedDeps.cpp:51-55`) and whose
per-block `blockLo/blockHi` window is the acquired block set; the acquire pointer
becomes the EDT's dependency operand (`SdeToArtsBoundary.cpp:3809-3817,3837-3839`).
ARTS-RT then serializes same-GUID/same-block acquires by mode (single EW writer,
concurrent RO readers) — that runtime serialization **is** the per-block RAW/WAR/WAW
edge, derived from the windows, with no `su_barrier`. A stencil read-halo projects to a
`HaloSliceAttr` on the acquire (`DbCommitDistributedDeps.cpp:28-41`) and
`RealizeEdtDistribution` marks the EDT `perBlockHaloExchange` / the alloc
`perBlockSingleWriterStencil` — but only when the acquire carries explicit
`element_offsets/element_sizes`, else it fails closed ("ARTS-RT must not infer halo
byte windows", `RealizeEdtDistribution.cpp:24-60`). A retained `su_barrier`, by
contrast, lowers 1:1 to a coarse global `arts.barrier` fence
(`lowerSdeControlBarrier`, `SdeToArtsBoundary.cpp:4842-4853`) — there is no per-block
lowering of a barrier. That split (windows → fine EDT deps; barrier → coarse global
fence) is precisely the lever the design exploits.

### 6.2 The inversion — `ConvertOpenMPToSde` emits async + windows; `nowait` is the default

Today `ConvertOpenMPToSde` **adds** an implicit `su_barrier` after every non-`nowait`
region and a later pass erases the redundant ones:

- `omp.master` → unconditional trailing barrier (`ConvertOpenMPToSde.cpp:480-481`;
  `master` has no `nowait` clause).
- `omp.single` → barrier `if (!getNowait())` (`:498-502` scheduling-splice path,
  `:516-520` plain path).
- `omp.wsloop` → `su_iterate` + barrier `if (!nw && hasWorkAfterInParentBlock)`
  (`:632-636`; helper at `:101-111`).
- `scf.parallel` → `su_iterate` + barrier `if (hasWorkAfterInParentBlock)` (`:834-838`).
- `omp.parallel` → `cu_region` with **no** trailing barrier (the region is the CU).

`nowait` is threaded as a `UnitAttr` (`nowaitAttr`, `:256-258`) onto the SDE op and used
to suppress the implicit barrier. Explicit source ordering survives 1:1:
`omp.barrier → su_barrier` (`:926-928`), `omp.taskwait → su_barrier` (`:941-943`).

The inversion: **stop emitting the implicit completion barrier** at the
`:480-481 / :498-520 / :632-636 / :834-838` sites; emit `nowait` by default. Keep the
explicit-source barriers verbatim — they are real source ordering. `nowait` is *not*
removed; its default polarity flips. Today `nowait` = "suppress the implicit barrier".
After the inversion `nowait` is the default, and its one surviving load-bearing effect
is the boundary completion barrier: `needsCompletionBarrier = !getNowaitAttr()`
(`SdeToArtsBoundary.cpp:3913,4675`). So "async" = `nowait` present = default; "wait" =
absence of `nowait` = the marked completion fence. No new attribute is introduced. This
also closes a known ambiguity: today "no barrier written" and `nowait` are structurally
indistinguishable yet semantically opposite (Part 5.5, and `design-revision.md:660-663`
on `nowait` not being analysis-recoverable) — under the inversion the frontend always
stamps the marker, so the absence of a barrier is a positive async assertion the
verifier can check, not an accident.

### 6.3 The barrier taxonomy — REQUIRED vs FORBIDDEN

The decision procedure already exists: `classifyBarrierSync(before, after)`
(`AccessWindowSync.cpp:108-161`) decodes each CU's windows into half-open block
rectangles per owner dim and, over shared MU roots, returns one of four verdicts. The
taxonomy below maps each verdict to a required/forbidden disposition and ties each to
what windows can or cannot express.

**FORBIDDEN — the barrier must be removed (or rejected). `classifyBarrierSync =
Redundant.**` The dependency is a pure per-block RAW/WAR/WAW already expressible as
windows, **and** both surrounding phases are fully windowed (`phaseFullyWindowed` true
on both sides, `AccessWindowSync.cpp:79-104,158-159`). RAW requires
`rectangleWithin` (consumer blocks ⊆ producer blocks); WAR/WAW need only
`rectanglesOverlap` because the after-read sources its data elsewhere
(`AccessWindowSync.cpp:40-53,127-160`). The canonical forbidden case is the coarse
per-timestep **global** stencil barrier: once `su.halo` windows exist, adjacent
timestep waves touch disjoint per-block halos, so the dep is a window and the global
fence is `Redundant`.

**A distinct third verdict — neither keep-as-ordering nor remove: `Misaligned`.** A
consumer reads blocks the producer never wrote (`AccessWindowSync.cpp:153`). This is
not an ordering need; it is a **redistribution** need (`su.halo` / `su.reduce_scatter` /
`su.all_to_all`, Part 2). The sync verifier already rejects it as "needs a
redistribution, not an ordering" (`VerifySdeMuAccessWindowSync.cpp:76-82`). Treating a
`Misaligned` barrier as removable would drop data that was never produced locally — it
must route to a movement op or fail closed.

**REQUIRED — the barrier must be kept. `classifyBarrierSync = Justified`, or a fact no
window can carry:**

- **Timestep / `full_timestep` recurrence** whose loop-carried state is scalar/control
  (continue-flags, rank-0 memrefs — already special-cased at
  `BarrierElimination.cpp:441-448` via `getUniqueStaticWrittenShape`), **not** the
  per-block data halo. The per-block halo half is window-derivable and becomes
  `perBlockHaloExchange` deps; only the true loop-carried scalar/control recurrence
  needs the barrier.
- **Reduction-combine ordering** — the partial-accumulate-then-combine sequence whose
  combiner reads partials in an order windows cannot fully express. Loop-carried SSA
  scalar state (`iter_args`, combiner identity) is invisible to the window classifier,
  which reasons only over memref roots.
- **`su.all_to_all` repartition bracketing** — the design *requires* an SDE-committed
  `su_barrier` around a repartition so `CreateEpochs` materializes the
  producer/repartition/consumer chain (Part 2, `design-revision.md:210-213`). This is
  load-bearing downstream even where no per-block window conflict spans the gap.
- **Control flow / external effects** — `scf.if` external effects, opaque/unknown-effect
  calls (`BarrierElimination.cpp:641`), and any access `classifyBarrierSync` sees as
  `Unprovable` because a window does not describe it (`AccessWindowSync.cpp:158-160`).
- **Explicit source `omp.barrier` / `omp.taskwait`** — the user wrote the ordering
  (`ConvertOpenMPToSde.cpp:926-943`).
- **The hard correctness floor:** any access with no committed window. The boundary
  already fails closed here ("touches a DB without a committed SDE access-window
  dependency", `SdeToArtsBoundary.cpp:1708`); a barrier guarding such an access must
  never be dropped.

### 6.4 Op-level verification

The rule "a barrier whose order is window-derivable is `Redundant` → reject, and a
survivor must carry a justified verdict" is already a pass: `VerifySdeMuAccessWindowSync`
(`Compile.cpp:1174`) mirrors `classifyBarrierSync` exactly — **accept**
`OutOfScope`/`Justified`, **reject** `Redundant`/`Misaligned`/`RankMismatch`/
`Unprovable` (`VerifySdeMuAccessWindowSync.cpp:62-108`). The op-level migration
(`op-level-verification.md:50-63`) relocates this into a new
`SdeSuBarrierOp::verify` (`SdeSuBarrierOp` has no verifier today,
`SdeOps.td:491-507`) implemented as a **bounded sibling scan** that calls
`classifyBarrierSync` / `partitionBarrierPhases` verbatim. The bounded-scan form is
what makes an op-seated verifier legal: redundancy is a cross-phase fact needing both
surrounding CU phases, which a strictly op-local view cannot see; the bounded sibling
scan over the CU run before/after the barrier supplies them without a token graph. Do
**not** relocate `phaseFullyWindowed` out of `classifyBarrierSync` — the `Redundant`
verdict depends on it internally (`op-level-verification.md:61-63`). The companion
`SdeControlTokenOp::verify` constrains the token's only legal use to `su_barrier`
operands; the `cu_region` region verifier checks per-CU window coverage.

The async-default safety theorem then reads: after the sync gate, every surviving
`su_barrier` is `Justified` or `OutOfScope`; every `Redundant` barrier was either
removed by `MuAccessWindowSyncOpt` (`Compile.cpp:1173`) or is a hard compile error; and
every removal required `phaseFullyWindowed` on both sides — i.e. ARTS can derive **all**
data deps. A barrier whose dep is not window-derivable is never classified `Redundant`,
so it is never removed.

### 6.5 The payoff and the invariant that makes it safe

**Per-kernel classification (from the committed `2_sde` dumps).** The surviving
barriers split into three buckets:

- *Window-derivable compute chains → REMOVE.* `stream` carries five
  `unknown_required` barriers (correcting the `benchmark-grounded.md` "stream has no
  barriers" claim): four single-array RAW/WAR at matching owner-strip grain between
  copy/scale/add/triad, all fully window-coverable and disjoint-per-block — the
  lowest-risk removals in the suite. `correlation` (the healthy 2.08× exemplar) has
  three compute RAW barriers (`mean→var`, `var→normalize`, `normalize→matmul`) that are
  textbook window-derivable reduction-combine chains → REMOVE.
- *External-effect tails → KEEP, reclassified.* `layernorm`, `bicg`, `activations`,
  `pooling`, `convolution-2d`, plus `stream`/`correlation`'s last barrier, separate the
  final compute SU from a `cu_region<single>` calling `carts_kernel_timer_accum` /
  `bench_checksum_d`. These are external effects with no MU window → KEEP, but the reason
  must move off `unknown_required` onto an honest external-effect reason; they are never
  removable and need not be global compute fences.
- *Window-shaped but not yet per-block realizable → KEEP (race if removed).* `2mm`/`3mm`
  chained-matmul intermediate (frozen `block_contraction`, identity self-edge redist
  (target == source endpoint, `RedistributionEdges.cpp:502-503`) and `batchnorm`/`atax`/`bicg` coarse
  reduction reads. The dep is window-shaped on paper but the read grain is coarse or the
  redist is a self-edge, so removal races today (the `3mm` 2n half-checksum is exactly
  this). KEEP until the reader-grain reconcile and a genuine `target != source`
  repartition land.

**The jacobi/poisson overlap win.** `jacobi-for`/`poisson-for` carry two barriers each,
mislabeled `unknown_required`, that separate the stencil `su_distribute` from a trailing
`cu_region<single>` writing only the `i1` continue-flag — rank-0 control state already
excluded from the data shape (`BarrierElimination.cpp:441-448`). The real RAW between
the copy SU and the stencil SU within the timestep loop is window-derivable (committed
±1 halo: `accessMin/MaxOffsets=[-1,-1]/[1,1]`). `jacobi2d`'s `timestep_stage_boundary`
barrier (`BarrierElimination.cpp:670-678`) separates the two double-buffer sweeps —
the canonical alternating-buffer recurrence. Under async-default, the per-block halo
frontier is re-expressed as `perBlockHaloExchange` EDT deps and the **global** fence is
dropped, letting ARTS overlap adjacent timestep waves — the headline lever the benchmark
sweep wanted.

**Why these payoffs do not fire today, and the one structural fix that unblocks them.**
`MuAccessWindowSyncOpt` removes **zero** barriers in production — not because the deps
are not derivable, but because `partitionBarrierPhases` (`AccessWindowSync.cpp:163-175`)
collects only **direct** block children that are `SdeCuRegionOp`/`SdeSuBarrierOp`, while
production compute CUs are nested two levels down inside
`su_distribute > su_iterate > cu_region`. Around every real barrier the before-phase is
empty, so `classifyBarrierSync` returns `OutOfScope` and the opt and verifier prove
nothing. The fix is to make `partitionBarrierPhases` **descend** through
`su_distribute`/`su_iterate` to the leaf CUs, reusing `findSuIterate`
(`BarrierElimination.cpp:37-58`); no new classifier is needed.

**The pipeline-order obstacle.** `BarrierElimination` runs at `Compile.cpp:1155`,
**before** `RaiseToMuAccessWindow` at `:1171` — so today's removal reasons over coarse
SU `collectStructuredMemoryEffects` write-conflicts at MU-root granularity, not over
committed windows. The window-grounded decision (`MuAccessWindowSyncOpt` + the sync
verifier) only exists at `:1173-1174`. So `BarrierElimination` re-roles into a
conservative pre-window **safety net** (keep on any whole-buffer write-conflict; it can
afford to be coarse because the frontend no longer over-produces barriers), and the
**authoritative** remove/keep decision moves to the window-grounded pair, where every
removal is backed by an actual `mu_access_window`-derived dep. Functionally this is
"insert-on-proven-need": the frontend starts from zero barriers, the conservative pass
keeps the few it cannot yet disprove, and the window pass + verifier prove the residual
set is exactly the required set.

**The invariant that makes it safe.** A barrier may be dropped **only** when
`classifyBarrierSync` returns `Redundant`, which requires `phaseFullyWindowed` on both
sides — every load/store targets a windowed MU root, so ARTS can derive every data dep.
Any unwindowed access yields `Unprovable` and the barrier is kept; any cross-owner
flow yields `Misaligned` and routes to a movement op. This is what keeps the project's
real hazards fail-closed rather than miscompiled: the ARTS RO-retire race and the
cross-node remote-writer coherence cases are precisely the not-window-derivable cases,
so they classify `Unprovable`/`Justified` and never lose their barrier. Per the
engineering standard, a kernel whose timestep-recurrence or halo dep ARTS cannot derive
from windows keeps its explicit barrier (fail closed with evidence); it is never
silently dropped.

### 6.6 Migration (smallest blast radius first)

1. **Pin the baseline.** Add a lit baseline recording current per-kernel barrier counts
   (the gate is delta-from-baseline; the SDE suite is RED at HEAD — `keystone_v4_sde_wiring_red`).
2. **Fix the latent remover (zero new concepts).** Make `partitionBarrierPhases`
   descend through `su_distribute`/`su_iterate` to the leaf `cu_region` (reuse
   `findSuIterate`). This activates the dormant `MuAccessWindowSyncOpt` +
   `VerifySdeMuAccessWindowSync` on production IR. Both must move in the **same** change
   so no barrier is removed without the fail-closed verdict that justifies it. Gate: lit
   delta == 0 plus `Correct=YES` on `stream`/`correlation` (unambiguously
   window-derivable). Delivers `stream`'s four kernel-chain barriers and
   `correlation`'s three compute barriers.
3. **Invert emission.** In `ConvertOpenMPToSde`, stop emitting the implicit barrier at
   `:480-481/:498-520/:632-636/:834-838`; emit `nowait` by default. Keep
   `omp.barrier`/`omp.taskwait`. The boundary already consumes `nowait` for completion
   deps (`SdeToArtsBoundary.cpp:3913,4675`), so removed barriers become per-block
   window deps automatically. Gate: byte-diff sweep + `Correct=YES` on all 21; the
   must-KEEP set (serial tails, chained-matmul intermediates, coarse-reduction reads)
   must remain.
4. **Validate the overlap payoff.** `jacobi-for`/`poisson-for`/`jacobi2d` 1n/64t large
   `Correct=YES`. The per-block dep must be fine-grained, so this step depends on the
   reader-grain reconcile + halo-bearing read window (Parts 2/5) — otherwise the
   derived dep is over a coarse read and yields no overlap. `jacobi2d` 1n has a known
   chained-stencil halo-bridge miscompile: the `HaloSliceAttr` on the acquire must be
   authored correctly before the global fence is dropped.
5. **Reclassify reasons.** Split `unknown_required` into `external_effect` (timer /
   checksum / dealloc / continue-flag tails) and a movement-unrealized reason
   (`2mm`/`3mm` intermediate, coarse reductions — KEEP only until the reconcile and
   real repartition land). Do **not** branch async-default logic on `barrierReason`: it
   is write-then-ignored downstream (`arts.barrier` lowers identically regardless of
   reason, `design-revision.md:596`); recompute from windows via `classifyBarrierSync`.
   `CartsBarrierReasonCases` is index-shared between SDE and ARTS
   (`CommonAttrs.td:34-40`); any enum trim must touch both dialects together.

## Part 7 — Mixed affine + non-affine input: keep affine, recover scf, fail closed

This part closes a premise that survived into Part 3 but was already refuted in
§5.9: that "the gap is a dependence-proof gap, not an IR-recovery gap"
(Part 3, the `raise-to-sde` framing). The verified picture is that there are
**two** gaps, and the IR-recovery one is the one currently doing the damage.
§5.9 established *why* (affine is lowered before any SDE pass) and *what to
delete* (the hand-rolled `SuLoopAccessAnalysis` core). Part 7 states the unified
**affine + scf** handling end to end, resolves the one dialect decision §5.9 left
implicit (do `cu_region` / `su_iterate` bodies legally hold `affine.*`), and
draws the precise line between "scf I failed to raise" and "genuinely
non-affine."

The unifying claim, restated and verified: **mixed `affine.for`/`affine.load`
and `scf.for`/`memref.load` in one `func.func` is the normal MLIR state.**
`AffineForOp` carries no parent restriction
(`external/Polygeist/llvm-project/mlir/include/mlir/Dialect/Affine/IR/AffineOps.td`,
`AffineForOp` trait list is loop/region-branch interfaces, not an affine-only
context); `affine.load`/`affine.store` constrain only their own operands to
valid dims/symbols. Analyses run per-op through interfaces. So the affine parts
get full upstream affine analysis and the non-affine parts stay `scf` **in the
same function** — no all-or-nothing IR, no uniform early lowering required.

### 7.1 The five legs, verified

1. **Keep affine where Polygeist emits it.** `cgeist` runs
   `--raise-scf-to-affine` (`tools/scripts/compile.py:480`, `:964`), so the
   `.mlir` CARTS reads is already `affine.for`/`affine.load` over `memref`, with
   `outer*T+inner` carried as an `AffineMap` result. CARTS destroys it at the
   head of its first stage (`createLowerAffinePass()` at
   `tools/compile/Compile.cpp:1112`, again at `:1126`), both before
   `buildSdePlanningPipeline` (invocation order `:1380` → `:1388` → `:1395`).
   The stage-invariant comment that bakes this in (`Compile.cpp:1108-1110`,
   "SdeInputNormalization only needs to reason about the memref+SCF form") is
   the thing to revise, not honor. **Do not lower affine before SDE planning**
   (the A1 relocation of §5.9.2): keep the late lowering at `Compile.cpp:1303`
   where affine must become LLVM-ready. Lowering is correct at the end, wrong at
   the start.

2. **Re-raise scf → affine opportunistically.** Polygeist ships the inverse of
   the discard as two standalone greedy-pattern passes already on CARTS's link
   line: `createRaiseSCFToAffinePass`
   (`external/Polygeist/lib/polygeist/Passes/RaiseToAffine.cpp:315`,
   `Passes.td:157`) raises `scf.for`/`scf.parallel` with affine-symbol steps and
   `isValidIndex`/min-max bounds, returning `failure()` on the rest so non-
   raisable loops stay `scf`; `replaceAffineCFGPass`
   (`AffineCFG.cpp:1602`, `Passes.td:24`) raises body `memref.load`/`store` via
   `isValidIndex` (`AffineCFG.cpp:985-1058`) and **recovers `DivSI`/`DivUI`/
   `RemSI` into affine `floordiv`/`mod`** (`AffineCFG.cpp:39`, `:334-436`,
   `:843-874`) — precisely the operator set the hand-rolled parser drops.
   Calling these mid-pipeline is a one-line `addPass` with zero build-graph
   change (`Compile.cpp` already calls `polygeist::create*` passes). Mixed
   affine+scf after a best-effort raise is the intended state, not a defect.

3. **One dialect-agnostic bridge for the boundary.**
   `ValueBoundsConstraintSet` spans affine + scf + arith + memref over one
   shared Presburger `FlatLinearConstraints` system, with per-dialect
   `ExternalModel`s registered via `registerAllDialects()`
   (`Compile.cpp:688`). It is **already linked and used** in CARTS
   (`lib/carts/utils/ValueAnalysis.cpp:33-65`,
   `lib/carts/dialect/arts/Utils/ValueAnalysisUtils.cpp:79-81`), so adopting it
   in SDE access analysis introduces no new dependency. It computes scf-IV
   bounds with no affine op present
   (`mlir/lib/Dialect/SCF/IR/ValueBoundsOpInterfaceImpl.cpp`, IV bounds from
   lb/ub/step), constant deltas (the `iv - offset` stencil radius CARTS already
   queries), value equality, and slice disjointness — the single-writer test.

4. **Genuinely non-affine code stays scf and fails closed for distribution.**
   A data-dependent index (`A[idx[i]]`, gather/scatter), a dynamic-step or
   dynamic-bound loop (`atoi`/`argv` sample sweeps), and jagged pointer-of-
   pointer memrefs do not satisfy `isValidIndex` (no case for a load result;
   fallthrough `false` at `AffineCFG.cpp:1056`), so they are **not raised** and
   remain `scf`/`memref`. `getAccessRelation` / `ValueBounds` return `failure`
   on them. That failure is the correct fail-closed signal: a runtime-valued
   index cannot be statically partitioned, so the only sound fates are
   serial / `local_only` / a runtime-irregular path — never a fabricated static
   owner map or block shape. This is the existing `DistributionPlanning`
   fail-closed posture (§5.9.2 residual rule), not a new mechanism.

5. **Tiling stays in affine form so re-analysis survives.** The hand-rolled
   `stripMineLoop` builds an `scf` tile nest with `arith.MulI`/`AddI`/`MinUI`
   bounds and a fresh inner IV (`Tiling.cpp:1250-1289`, `MulI` `:1265`, `AddI`
   `:1274`, `MinUI` clamp `:1276`); the tile/inner relation lives only in scf
   loop-bound structure, which `tryGetAffineExpr`'s non-const-`MulI` /
   no-`Div` gap cannot recompose. Upstream `tilePerfectlyNested`
   (`mlir/include/mlir/Dialect/Affine/LoopUtils.h`) instead moves the body
   unchanged into the point loop and expresses the inter/intra-tile relation as
   `AffineMap`s on the bounds, keeping `outer*T+inner` a canonical `AffineExpr`
   that re-reads through `getAccessRelation`. This is why `parallelize` becomes
   fully re-runnable after tiling (§5.9.4) rather than the outer-band fallback.

### 7.2 The analysis layer — two tools, one is not a substitute for the other

§5.9.2 lists both `MemRefAccess`/`getAccessRelation` and
`ValueBoundsConstraintSet` as replacements. They are **not interchangeable**, and
the difference is load-bearing for the failing kernels:

- **`ValueBoundsConstraintSet` is a bound / equality / disjointness oracle, not
  a dependence-vector analysis.** Its arith `ExternalModel` covers
  `AddI`/`SubI`/`MulI`/`FloorDivSI`/`Select`/`Constant`
  (`mlir/lib/Dialect/Arith/IR/ValueBoundsOpInterfaceImpl.cpp`,
  `registerValueBoundsOpInterfaceExternalModels` attaches exactly those six) —
  **no `RemSI`/`RemUI`, no `DivSI`, no `CeilDivSI`.** It models `floordiv` but
  drops `mod`. The post-rank-expand block-localized accesses that already defeat
  the structured-op analysis are exactly `i floordiv T` **plus** `i mod T`; a
  `ValueBounds`-only migration parses the floordiv leg and silently widens away
  the mod leg (unsupported semi-affine terms are dropped unless
  `addConservativeSemiAffineBounds` is enabled — off in CARTS's call sites).
  Use `ValueBounds` for trip counts, per-dim offsets, owner extents, and
  single-writer disjointness on the mixed/scf boundary, and treat `failure` as
  fail-closed.

- **`MemRefAccess` + `getAccessRelation` (a Presburger `IntegerRelation`) and
  `checkMemrefAccessDependence` / `isLoopParallel` are the ordering analysis.**
  Loop-carried RAW/WAR/WAW direction and the legal wavefront — the seidel-2d /
  jacobi2d / in-place question — require this, and it reads `mod` natively
  because the relation is built from the `affine.load`/`store` `AffineMap`, not
  from a one-shot arith walk. **None of these are used in CARTS today**
  (`grep -rn checkMemrefAccessDependence|MemRefAccess|isLoopParallel|tilePerfectlyNested lib include`
  returns zero hits outside `external/Polygeist`). Adopting them is the actual
  dependence-proof half of the fix and requires the affine form, hence legs 1-2.

So the analysis rule is: **affine ops → upstream affine access/dependence
analysis on their own maps; mixed/scf boundary → `ValueBoundsConstraintSet`;
genuinely non-affine → fail closed.** Never hand-roll affine-on-scf
reconstruction (`tryGetAffineExpr`/`tryBuildIndexingMap`/`extractDimOffset`,
`SuLoopAccessAnalysis.cpp:356-413`, `:1094-1118` — deleted per §5.9.2). The two
analyses are complementary: `ValueBounds` is the index/bound half, the
`IntegerRelation` machinery is the ordering half. Neither alone covers the whole
mixed boundary; the mod/rem coverage gap is the concrete reason.

### 7.3 The unresolved dialect decision: do CU/SU bodies hold affine?

§5.9 keeps affine "through SDE planning" but does not state whether the affine
survives **inside** the `sde.cu_region` / `sde.su_iterate` bodies that planning
produces. Verified: it does not, as the ops stand. `SdeCuRegionOp` is
`[SingleBlock, RecursiveMemoryEffects]` and `SdeSuIterateOp` adds
`AttrSizedOperandSegments` + `LoopLikeOpInterface` only
(`include/carts/dialect/sde/IR/SdeOps.td:228-232`, `:360-368`). **Neither
carries the `AffineScope` trait** (a `NativeOpTrait`, `OpBase.td:65` — distinct
from `AutomaticAllocationScope` at `:67`; `D-b` adds only `AffineScope`, never
`AutomaticAllocationScope`, which would change alloca free placement). This is
load-bearing two ways:

1. `affine.load`/`affine.store` inside a CU body require their index operands to
   be valid dims/symbols *relative to the nearest `AffineScope`*. With no scope
   on `cu_region`, an `affine.*` op whose symbols are CU-body SSA values is not
   well-formed in the way affine analysis expects, and the maps may not compose.

2. `createRaiseSCFToAffinePass` explicitly **refuses to raise** `scf.for` whose
   ancestor lacks `AffineScope` and is not an scf/async container, to avoid
   producing invalid IR (`RaiseToAffine.cpp:38-75`). CARTS is OMP-first
   (`ConvertOpenMPToSde`), so a re-raise attempted *after* CU/SU wrapping sees
   `cu_region`/`su_iterate` ancestors without `AffineScope` and either no-ops or
   produces invalid affine.

The decision, with the two sound options:

- **(D-a, preferred) Re-raise and run affine analysis BEFORE wrapping.** Keep /
  re-raise affine on the frontend `func.func` form, run `parallelize` / tiling /
  layout on that affine form, and lower the genuinely-affine nests into
  `su_iterate` + `cu_region` only at the point where SU/CU structure is emitted.
  The CU bodies then hold `scf`/`memref` (lowered from the proven-affine nest at
  wrap time), and the affine analyses never need to run inside a CU. This matches
  the §5.9.4 re-run points ("after `raise-to-sde`; after interchange+tiling")
  being placed on the pre-wrap nest, and needs no dialect change.

- **(D-b) Give `cu_region` / `su_iterate` the `AffineScope` trait** so
  `affine.for`/`affine.load` are legal and raisable inside CU bodies. This
  enables a post-wrap re-raise and per-CU affine analysis, but it is a dialect
  change that must be verified against every CU-body consumer (the dormant
  affine `dyn_cast` branches in `DbUtils`/`EdtUtils`/`DbModeTightening` would
  reactivate) and against the `cu_region`-bodies-are-scheduling-free invariant
  (`SdeOps.td:242-244`).

(D-a) is the lower-risk path and is consistent with the §5.9 landing order
(relocate `LowerAffine`, then swap analyses, then add the post-tiling re-run —
all on the pre-wrap nest). (D-b) is only needed if a transform must re-prove a
nest **after** it is already wrapped in CU/SU structure; defer it until a
concrete pass requires it, and gate it on the affine `dyn_cast` consumers
getting test coverage first.

### 7.4 The non-affine residue on the measured suite

The conservative/fail-closed path is correct policy, but on the 21-kernel suite
it is a **guardrail, not the headline lever**. Every registered kernel has affine
*index* arithmetic; the obstacles that look non-affine are two distinct,
recoverable causes the access bail at `SuLoopAccessAnalysis.cpp` currently
conflates:

- **Jagged pointer-of-pointer layout** (specfem3d stress/velocity, sw4lite):
  point-local stencils with affine indices `arr[i±1][j][k]` over `double***`
  malloc layout. The analyzer drops the access as opaque `**` indirection, not
  because the index is non-affine. Fix = flatten to a strided `memref` upstream;
  the affine recovery (legs 1-5) does the rest.

- **Hard dependence families** (reductions: stream/atax/bicg/layernorm/
  batchnorm; in-place stencils: seidel-2d/jacobi2d): affine indices, but the
  parallelization legality needs the ordering analysis of §7.2 (the unused
  `checkMemrefAccessDependence` path) plus the reduction / halo realization the
  rest of this doc covers.

The **genuinely non-affine, data-dependent** kernels — graph500
(`adj_list[v][adj_count[v]++]`, indirect index + per-iteration alloc) and
monte-carlo (per-sample malloc) — are **unported** and not in the 21. So
fail-closed-for-distribution is necessary for correctness when those land, but it
is not gating any current result. The dominant win on the measured suite is
keeping/recovering affine so the div/mod block-localized accesses post-rank-
expand stay analyzable (legs 1-5), with layout flattening and dependence
realization on top — not the non-affine boundary itself.

### 7.5 The Part 3 correction

Part 3's `raise-to-sde` framing asserts "Affine is already lowered
(`Compile.cpp:1126`), so 'raise affine' reduces to 'raise scf'" and "**The gap is
a dependence-proof gap, not an IR-recovery gap.**" Both halves are now corrected:

- Affine being lowered is a misplaced pass call, not a fact to design around. The
  `.mlir` CARTS receives **is** affine (`compile.py:480`); the right move is to
  preserve it (A1), not concede the discard.
- There are two gaps. The dependence-proof gap is real (no
  `checkMemrefAccessDependence` in CARTS, §7.2). But the **div/mod IR-recovery
  gap is also real and is the one currently degrading tiled accesses to
  coarse/fail-closed**: `tryGetAffineExpr` has no `DivSI`/`RemSI`/`FloorDiv` case
  and gates `MulI` to const-only (`SuLoopAccessAnalysis.cpp:387-398`), so a
  tiled `affine.load` that is fully analyzable upstream becomes unanalyzable only
  because its map was lowered away. Recovering the map (legs 1-2) closes the
  IR-recovery gap; adopting the upstream dependence analysis (§7.2) closes the
  dependence-proof gap. Part 3's `raise-to-sde` CORE, built on the same
  `SuLoopAccessAnalysis` substrate, inherits the IR-recovery defect unless its
  access recovery is moved onto affine maps + `ValueBounds` as §5.9.2 / §7.2
  specify.

## Part 8 — ARTS no-contract audit

The redesign's principle applies equally to ARTS, which carries the **larger**
stamped-contract surface: **69 `OptionalAttr` on the three core ops**
(`arts.db_alloc` 19, `arts.db_acquire` 22, `arts.edt` 28) **+ 15 on
`arts.epoch`(11) / `arts.barrier` / `arts.db_access_window` / `arts.db_release` =
84 file-wide** (`ArtsOps.td`). The `ArtsDepPattern` docstring is the anti-pattern
verbatim: *"records the semantic rewrite family so downstream passes do not need to
rediscover it from raw memory accesses"* (`ArtsAttrs.td:188-191`).

**The one discriminator vs SDE:** ARTS has the *access-neighborhood* analysis
(so `stencil_*` offsets and `read_only_after_init` genuinely **recompute**), but it
has **no pattern classifier** — `depPattern` is only ever a **1:1 translation of
`sde::SdePattern`** at the boundary (`SdeToArtsBoundaryHelpers.cpp:79-135`). So the
pattern channel is **gated-irreducible (boundary-translated, consume-and-erase)**,
*not* recompute.

### Fate of every core-op attr

- **fate-2 TYPE** (already the DB grid in the `sizes`/`elementSizes` operands,
  `DistributedDbPlacementUtils.h:241-271`): owner dims, block grain;
  `stencil_owner_dims` is a redundant restatement.
- **fate-1 RECOMPUTE** (ARTS has the analysis): `stencil_center_offset` /
  `stencil_min_offsets` / `stencil_max_offsets` / `stencil_spatial_dims` /
  `stencil_write_footprint` / `supported_block_halo` (memref access neighborhood);
  `read_only_after_init` (`DbModeTightening.cpp:1000`); `partition_mode` (grain from
  block count + access shape); `local_only`, `distributed`
  (`evaluateDistributedDbEligibility` over grid + access modes); `distribution_pattern`
  / `distribution_kind` (pure functions of `depPattern`, `LoweringFactUtils.cpp:147-149`).
- **fate-3 IRREDUCIBLE RUNTIME** (value-branched placement/coherence — the honest
  residue ARTS legitimately stamps): `route` (owner-map `ordinal % totalNodes`),
  `concurrency` (intranode/internode, authored+normalized), `type`/`edt_type`,
  `dbMode` (required), `runtime_db_mode` (per-acquire RO/EW/RW),
  `element_offsets`/`element_sizes`/`bounds_valid` (realized halo byte-window),
  `inPlaceSafe`/`inPlaceSharedState`, `perBlockReplicated`/`perBlockSingleWriterStencil`
  (realization commitments over an identical grid type), `interleaveCount` (the one
  wired source/cost codegen hint — slot live, producer currently absent).
- **fate-3 GATED — boundary-translated, consume-and-erase, NOT durable:**
  `depPattern` (translation of `sde::SdePattern`; read at the boundary, never
  re-stamped onto alloc/acquire/edt/epoch as a cache).
- **delete-dead** (zero branching readers). **[code-verified 2026-06-14 — the
  original list was partly wrong; corrected here]:**
  - **DELETED** (truly dead, byte-identical, lit+1n-21/21 green): `vectorizeWidth`
    + `unrollFactor` (copy-through only in `PartialReductionSplit`, NOT
    zero-reader as first claimed — they had a non-branching get→set copy),
    `perBlockSummingSettle` + `perBlockHaloExchange` (write-only: set in
    BlockContraction/PartialReductionSplit/RealizeEdtDistribution, no getter/string
    read anywhere, no test).
  - **NOT dead — keep**: `distribution_version` (9 live refs).
  - **deferred — write-only TODAY but tested boundary fixtures + scaffolding for
    the in-progress distributed compact-halo / owner-local-writer-split
    realization**: `compact_halo_payload`, `compactHaloPack`, `ownerLocalWriterSplit`.
    Set by `SdeToArtsBoundaryAccessLowering`, checked by 3 conversion lit tests; do
    NOT delete until the distributed realization that consumes them is either built
    or formally dropped, else the S17-area work re-adds them.
  - **lesson**: the "delete-dead" labels are not reliable without per-attr
    `get*Attr`/`set*Attr`/raw-string/test verification — verify before deleting.
- **de-attribute — pass-local scratch, must NOT be an op attr:** the
  `partialReductionSplit*` family (`partialReductionSplitRequired`/`...Dims`/
  `...Factor`/`...OwnerTaskCount`/`...TargetWorkerCount`) — produced, consumed, and
  erased entirely within `PartialReductionSplit`; move to an in-pass data structure
  (the `EdtLowering.cpp:319` fail-closed guard stays as the leak backstop).

**Coverage:** `arts.epoch` carries the *same* `depPattern`/`distribution_*`/
`stencil_*` family (11 attrs, same fates as the `edt` copies); plus
`arts.barrier.barrierReason` (recompute via `classifyBarrierSync`),
`arts.db_access_window.{arrayId,haloShape}` (SSA-root / `su.halo`),
`arts.db_release.release_type`.

### ARTS / ARTS-RT verify passes (correcting the false "zero" premise)

There are **7**, not zero: **2 ARTS** (`VerifyArtsCdag`, `VerifyArtsObjectsOnly`)
+ **5 ARTS-RT** (`verify-{pre,edt,db,epoch}-lowered`, `verify-lowered`).
`VerifyArtsCdag`'s whole-module physical-layout walk is the one genuinely
irreducible ModuleOp verifier — and it folds to op-level **only after** `db_alloc`'s
layout becomes the type (gated on this Part). The ARTS-RT `verify-*-lowered` passes
are lowering-stage gates (kept, re-pointed at the async form in PHASE 9). ARTS ops
are otherwise already op-verified at the ODS level — do **not** retrofit SDE-8-style
standalone verify passes onto ARTS.

### Highest-value action + sequencing

Replace the `acquireCarriesSubpartitionEvidence` **presence-OR**
(`EdtUtils.cpp:138-167`) — which is what keeps `distribution_version`/
`distribution_kind`/`distribution_pattern` alive as existence flags — with one
recompute over grain + access pattern, and **read `depPattern` only at the
boundary** (translate the surviving SDE fact, consume it). Convert readers
(`LoweringFactUtils.cpp:67-151`, `PartialReductionSplit`) FIRST, delete the boundary
stamps LAST. This is `architecture.md` **PHASE 7** and gates the `VerifyArtsCdag`
fold and **S13**.
