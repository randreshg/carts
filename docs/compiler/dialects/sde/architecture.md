# CARTS Compiler Architecture — the revised target (consolidated)

The single coherent statement of the redesign. It supersedes, as *narrative*, the
accreted Parts 5/6/7 of [`design-revision.md`](./design-revision.md) (kept as the
detailed file:line derivations).

## Implementation status (`v4`, post S3–S7 + boundary split)

Structural SDE redesign is **live**. Remaining work is affine modernization (S4 main),
attribute collapse (S13/S14), ARTS audit (Part 8), and scaling levers (S17–S21).
**Gate:** 51/51 SDE + ARTS dialect lit tests pass.

**Done**
- `raise-to-sde` subsumes dropped `sde-parallelize`; movement is ops (`su.halo`,
  `su.reduce_scatter`, `su.all_to_all`); `sde.redist` / `SdeMovementFamily` retired.
- Op-level layout attrs removed (`physicalOwnerDims`, `physicalBlockShape`,
  `iterationTopology`, `logicalWorkerSlice`); grain is the rank-expanded type.
- Verify passes 8 → **5** (op-level fold in progress).
- **S3:** pre-planning `LowerAffine` removed; `SimplifyAffineStructures` in input
  normalization; planning-head `LowerAffine` bridge after `LayoutAssignment` removed
  (S4, 2025-06); final lowering remains in ARTS-RT (`Compile.cpp:1277`).
- **S5:** proof-derived `cu_region<parallel>`; residual serial wrappers keep
  `serial_reason`; `verify-sde` rejects untagged `<single>` in multi-trip SU.
- **S6:** second `raise-to-sde` after Tiling; re-entrancy allows promotion inside
  residual `cu_region<single>`.
- **S7:** `commVolumeBytes` deleted; structural `assignLayout`; consumers use layout
  geometry / `muBlockCount > 1`.
- **Cost model:** fabricated cycle costs removed from `SDECostModel` / `ARTSCostModel`;
  only capacity, locality, and fixed iteration floors remain.
- **Boundary:** `SdeToArtsBoundary.cpp` is pass registration only; lowering lives in
  `Transforms/boundary/*` with shared helpers in `DbUtils` / `MovementLoweringUtils`.
- **S12:** `SdeMuAccessWindowOp` + `RaiseToMuAccessWindow` + `VerifySdeMuAccessWindow`
  deleted; boundary storage, sync analysis, and verify are query-only via
  `queryAccessWindows` on rank-expanded MUs and in-CU `affine.load/store`.
- **S4b (halo):** halo neighborhood classifiers use `tryGetUnitNeighborhoodOffset`
  (affine); legacy `analyzeIndexExpr` peel-div path removed from boundary index/halo.

**Partial**
- **S4:** `SuLoopAccessAnalysis` uses upstream `MemRefAccess`/`affine.load/store` when IR
  is affine; `RaiseSCFToAffine` + `SimplifyAffineStructures` run after Tiling (S4d);
  `raise-to-sde` promotes `affine.for` via `isLoopParallel`; planning keeps affine
  through Interchange/Tiling (no bridge `LowerAffine`); Tiling strip-mines direct
  matmul column loops via upstream `tilePerfectlyNested` when inner nests are
  `affine.for` (owner tile loops still scf). Shared
  `AffineIndexUtils::tryGetAffineExpr` replaces the duplicated SDE parser;
  `appendNestedForIvs` collects `affine.for` IVs; direct matmul Interchange
  rewrites affine j-k nests to scf k-j (S4d re-raise recovers affine); hand-rolled
  `extractDimOffset` still used for lowered `memref` paths; stencil halo
  interchange uses upstream `permuteLoops` on `affine.for` nests; Interchange/Tiling
  skip waits on committed CU `groupBlockCount`, not layout-root shape recovery alone.
- Op-level verify fold: 5 SDE verify passes remain (target ≈2 + residuals).

**Open (DAG order)**
1. **S4 main** — delete `extractDimOffset`; migrate owner-level Tiling off scf
   `stripMineLoop` where possible.
2. **S13 / S14** — physical attr collapse; `arrayLayout` dict deletion.
3. **Part 8** — ARTS 84 `OptionalAttr` audit.
4. **S17–S21** — scaling levers + megalarge 1n→2n gates.
5. **PHASE 9** — async local executor (greenfield).

## The one principle

> **A fact lives as a type or an op, or it is recomputed on demand by an upstream
> analysis. Nothing is stamped onto the IR for a later pass to read. Irregular
> code fails closed — it is never faked. SDE is sync; ARTS is distributed; the
> local executor is reused from upstream `async`.**

Every "this feels off" reduces to one defect — *a fabricated fact stamped for a
downstream reader, or upstream machinery reimplemented* — and one cure:

| Smell | What it is | Fix |
| --- | --- | --- |
| `commVolumeBytes` | fabricated cost metric driving a search | structural layout decision (no score); attr deleted |
| `sde-loop-pattern-facts` | a *pass* that stamps classification/offset facts | recomputed analysis (`SuLoopAccessAnalysis`/upstream) |
| `arrayLayout` / `physical*` | attribute layout contract | the rank-expanded `mu_alloc` **type** |
| `sde.redist` + `SdeMovementFamily` | attribute movement contract | `su.halo` / `su.reduce_scatter` / `su.all_to_all` **ops** |
| `sde.mu_access_window` | zero-result attribute-fact op (a cache) | derive access from loads + type at the boundary |
| hand-rolled `tryGetAffineExpr`/`extractDimOffset` | reimplemented affine analysis (can't parse `mod`) | upstream affine `MemRefAccess` + `ValueBounds` |
| hand-rolled local task runtime | reimplemented async | upstream MLIR `async` dialect for the node-local executor |
| `min_distributed_tile_bytes`, fabricated cost constants | knobs | deleted; grain is structural |
| 8 `verify-*` ModuleOp passes | contract babysitters | op-level ODS verifiers (the ARTS/EDT model) |

## The spine

```
 C / OpenMP ──cgeist(Polygeist, emits AFFINE)──► affine + memref (+omp)
        │   analyze on AFFINE here (before SDE wrapping): isLoopParallel,
        │   MemRefAccess/checkMemrefAccessDependence, ValueBounds, tilePerfectlyNested
        ▼
 ┌─────────────────────────────────────────────────────────────────────────┐
 │ SDE  — SYNC, structured.  CU / SU / MU over ordered IR with explicit      │
 │ su_barrier fences. Commits REAL layout (type), movement (ops), loop/data  │
 │ rewrites. No tokens, no dataflow graph, no fabricated cost, no fact attrs.│
 └─────────────────────────────────────────────────────────────────────────┘
        │  SDE→ARTS boundary (first isolation boundary)
        ▼
 ┌─────────────────────────────────────────────────────────────────────────┐
 │ ARTS — DISTRIBUTED realization. Per-block single-writer DBs, owner maps,  │
 │ %N routing, halo/all-gather/reduce/all-to-all CUs, DB modes. Derives      │
 │ per-block deps from the CU body accesses + the rank-expanded type.        │
 │   └─ node-local EDT bodies + fire-when-ready  ──►  MLIR `async` dialect    │
 │      (REUSE async.execute/await_all, async-to-async-runtime, ref-counting, │
 │       AsyncToLLVM, threadpool/coroutine runtime)                          │
 └─────────────────────────────────────────────────────────────────────────┘
        ▼
 ARTS-RT — mechanical lowering to runtime calls + GASNet RMA transport.
```

Why **SDE-sync**: every structural SDE pass reasons over *ordered* IR — order is
given, so no pass must prove race-freedom before fusing/tiling, and a barrier is a
conservative explicit fence. Async-at-SDE would force a token graph the design
forbids and would materialize the dependence facts twice. Concurrency is a
*lowering* concern, exposed when ARTS lowers the local task graph to `async`.

## The analysis layer — use upstream, don't reimplement

**Keep affine.** Polygeist already emits affine (`cgeist --raise-scf-to-affine`);
CARTS currently throws it away (`createLowerAffinePass`, `Compile.cpp:1112/1126`)
and re-derives access relations by hand, *incompletely* (`tryGetAffineExpr` has no
`Div`/`Rem`/`FloorDiv` case, gates `Mul` to constants). The fix:

- **Do not lower affine early.** Run the analysis on the affine form. Where input
  is scf, **re-raise** with Polygeist's `RaiseSCFToAffine`/`AffineCFG` (it recovers
  `div`/`rem` into affine `floordiv`/`mod` — exactly what the hand-rolled parser
  drops). Mixed affine+scf in one function is the normal MLIR state.
- **Iterative re-raise (`D-a`): re-raise + re-analyze on the pre-wrap `func.func`
  after each shape-changing transform** — because the compiler's *own* transforms
  expose affine-ness (delinearization, const-prop, IV-substitution, and crucially
  the `arith` `div`/`rem` that `rank-expand-mu` emits, which `AffineCFG` recovers
  into affine `floordiv`/`mod` and `MemRefAccess` then reads natively — fixing
  CARTS's self-inflicted block-localized blindness). This is a **fixed set of ~4
  re-raise points**, and it provably **terminates** (raising is monotone scf→affine,
  `parallelize` monotone single→parallel, affine tiling keeps the form analyzable —
  no affine↔scf oscillation). It unifies with the re-runnable `parallelize`.
- **`func.func` already carries `AffineScope`**, so re-raising the *pre-wrap* nest
  is legal today; `D-a` covers all 21 with **no dialect change**. Re-raising ops
  *already nested under a CU/SU* would need `AffineScope` on `cu_region`/`su_iterate`
  (`D-b`) — a minimal, safe trait addition (`AffineScope` only, **not**
  `AutomaticAllocationScope`), but **no current transform needs it**, so `D-b` is
  **deferred** behind lit coverage for the dormant affine-in-CU-body consumers.
- **Prerequisite:** delete the two early `createLowerAffinePass` calls
  (`Compile.cpp:1112/1126`) that discard the affine form before planning; keep the
  final lowering (`:1303`).
- **Two tools, both needed:** `MemRefAccess` + `checkMemrefAccessDependence` for
  *ordering/dependence* (reads `mod` natively); `ValueBoundsConstraintSet` (already
  linked in CARTS) for *bounds/disjointness* (drops `mod`, so not a dependence
  substitute). `isLoopParallel` for parallelism.
- **Tile in affine** (`tilePerfectlyNested`): the tiled `outer*T+inner` stays a
  canonical `AffineExpr` that survives re-analysis — which is what makes
  `parallelize` re-runnable. The hand-rolled `stripMineLoop` (`arith.muli/addi`)
  is the one-way street that breaks re-analysis today.
- **Non-affine** (data-dependent gather/scatter; unported graph500/monte-carlo):
  stays scf, **fail closed for distribution** — never a fabricated static analysis.
- **Delete** the hand-rolled `tryGetAffineExpr`/`extractDimOffset` reconstruction;
  keep only thin CARTS-specific wrappers (owner-strip / contraction-tiling /
  stencil classification) over the upstream relations.

## Affine vs non-affine — contained in one place, and net-simpler

The affine/non-affine distinction does **not** introduce a dual code path. It is
**one conservative gate**, not a second analysis, and the redesign *deletes* far
more than it adds.

**All 21 core benchmarks are affine** — verified per-kernel against source + dumps:

| Category | Kernels |
| --- | --- |
| affine (natively flat) | `stream`, `activations` |
| affine-after-normalization | the other 19 — `gemm`/`2mm`/`3mm` (matmul), `jacobi-for`/`poisson-for`/`jacobi2d`/`seidel-2d`/`conv-2d`/`conv-3d` (constant-offset stencils), `atax`/`bicg`/`correlation`/`batchnorm`/`layernorm`/`pooling` (reductions), `volume-integral`/`stress`/`velocity`/`vel4sg-base` (FEM: `arr[i±1][j][k]` / dense per-element matmuls) |
| partly / genuinely non-affine | **none of the 21** (a global sweep for nested-indirect `x[y[...]]` finds only `graph500`/`lulesh`, both **unported**, not in the suite) |

- **"Looks pointer-chasing in C" ≠ non-affine access.** The jagged `double**` /
  `double***` / `float**` layouts become `memref`-of-`memref` in cgeist, then
  Polygeist memref-normalization **flattens them to dense `memref`** (e.g. poisson
  `memref<?xmemref<?xf64>>` → `memref<256x256xf64>`), so the *access* is affine.
- **The `div`/`rem` that breaks today's parser is CARTS's OWN rank-expand math**
  (`i floordiv T` / `i mod T` from block-localization), **not** source
  non-affinity. `tryGetAffineExpr` (`SuLoopAccessAnalysis.cpp:357-399`) has no
  `Div`/`Rem`/`FloorDiv` case, so it can't read the IR CARTS itself emits — a
  **self-inflicted** "non-affine" problem that upstream affine analysis (which
  reads `mod` natively) erases.

**How the difference is handled (minimal complexity):**
1. **SDE structure is dialect-agnostic** — a `cu_region` body may hold affine *or*
   scf; no SDE pass branches on affine-ness (the structural passes already walk
   scf only; affine lives at the front analysis).
2. **One analysis** — upstream affine (`MemRefAccess`/`checkMemrefAccessDependence`
   for dependence, `ValueBounds` for bounds, `isLoopParallel` for parallelism)
   where the code is affine.
3. **One fallback** — *not-affine ⇒ not-statically-distributable ⇒ serial /
   `local_only` / fail-closed*. Correct (you can't statically distribute a
   data-dependent access), and it **never fires on the 21**.

**Net effect: less code.** This *deletes* the hand-rolled `tryGetAffineExpr`/
`extractDimOffset` (~1433 lines of `SuLoopAccessAnalysis`), `sde-loop-pattern-facts`
(~1603), ~6 verify passes (~1422), and the window passes — versus *adding* only
`raise-to-sde` + the `parallelize` split, which **call upstream** analysis (no new
analysis code). Today CARTS already maintains a dual reality *badly* (an scf parser
that re-approximates affine and silently drops `mod`); the redesign collapses it to
one upstream analysis + one gate. **No kernel in the suite regresses.**

## The pass pipeline (revised, no-contract)

```
convert-openmp-to-sde        FIRST: omp.* regions → su_iterate/cu_region via the
                             shared buildSuIterate helper + source schedule/nowait/
                             reductionKind. (OMP is already parallel — convert it.)
raise-to-sde (CORE) [NEW]    raises the MISSING non-omp scf nests → su_iterate +
                             cu_region<single> (purely STRUCTURAL); wraps residual
                             host work; normalizes SU bodies. Folds cu-normalization.
                             Re-entrancy guard: skip nests already under SU/CU.
parallelize [NEW, RE-RUNS]   PROMOTES cu_region<single> → <parallel> where upstream
                             isLoopParallel/dependence proves independence. Descends
                             INTO su_iterate bodies (inverted guard); monotone, safe
                             to re-run. RUNS AGAIN after interchange/tiling/fusion —
                             reordering exposes new parallelism.
  (classification/pattern/offsets = ANALYSIS, recomputed on demand — no pass)
layout-assignment            STRUCTURAL derivation from access relations (owner =
                             parallel-written dim; contraction unsplit; stencil =
                             spatial dims + halo). Transient decision; NO arrayLayout
                             attr, NO commVolumeBytes. Materialized as the type.
loop-interchange             affine reorder; then re-run parallelize.
tiling (affine)              tilePerfectlyNested: storage grid = mu_alloc TYPE (DB
                             grain); CU loop tile = loop steps (dispatch grain) —
                             two grains, two IR objects, neither an attr. CU dispatch
                             span (coalescing N DB blocks) = an OUTER CU loop band.
                             Re-run parallelize.
elementwise-fusion           fused-op pipeline class = emit-as-structure (the fused
                             body), not a re-derived classification.
distribution-planning        su_distribute op kind (owner_compute/blocked); owner/
                             block already the type; NO min_distributed_tile_bytes.
realization                  memory-unit-realization (memref→mu_alloc, SSA-root
                             identity, no arrayId); rank-expand-mu (grid→type);
                             atomic + tree reduction realization (su.cu_atomic /
                             su.reduce_scatter); redistribute → movement OPS
                             (su.halo / su.reduce_scatter / su.all_to_all), never a
                             same-geometry self-edge.
  (no mu_access_window: the boundary derives per-CU access from loads + type)
boundary (SDE→ARTS)          SdeStorageToArtsDb / SdeAccessesToArtsDeps /
                             FinalizeSdeToArts: derive per-block deps from the CU
                             body accesses (analyzeDepOwnerAccessIndex) + type;
                             realize DBs/EDTs; realize su.all_to_all (reader-pull
                             per-target-block gather). No sde.* op survives.
```

**Verification is op-level** (the ARTS/EDT model: 0 standalone verify passes →
op verifiers / traits / conversion-legality). Two thin residuals stay
(`verify-sde` "source compute outside a CU" — foreign trigger op; `verify-sde-lowered`
"no sde.* survives" — dialect-absence). Barrier-redundancy moves into
`SdeSuBarrierOp::verify` over the *derived* access. **Order fix:** the
barrier/concurrency decision must run *after* access derivation, not before.

## Where every fact lives (the end state)

| Fact | Home |
| --- | --- |
| owner dims, block grain | rank-expanded `mu_alloc` **type** (`recoverOwnerDims`) |
| parallelism | `cu_region<parallel|single>` kind |
| dispatch span (`logicalWorkerSlice`) | an **outer CU loop band** (`outerStep/dbTile`) |
| per-CU access (which blocks) | **derived** from CU body loads + type (no `mu_access_window`) |
| movement | `su.halo` / `su.reduce_scatter` / `su.all_to_all` **ops** (source/target = types) |
| classification / pattern / offsets | recomputed **analysis** (`MemRefAccess` + thin wrappers) |
| comm estimate | **none** — §5.9.3 proves the partition objective is structural (`muBlockCount>1` / imbalance), so there is no edge-cost attr *and* no `commBetween` query; the layout decision is a transient structural derivation, not a cost search |
| ordering | explicit `su_barrier` (sync) + ARTS deps **derived** from accesses |
| reduction combiner + identity, reassociation license | **irreducible** `su_iterate` attr |
| `nowait` | **irreducible** `su_iterate` attr (no-barrier vs nowait are structurally identical) |

**`su_iterate` end state:** `{lowerBounds, upperBounds, steps, reductionAccumulators,
reductionKinds, nowait}` + optional source `schedule`/`chunk` hint. Everything else
is the type, an op, or recomputed.

## The async lowering (local executor reuse)

- The `async` dialect is shared-memory structured concurrency (threadpool +
  coroutines) — **no** distributed concept and **no extension seam**, so it can
  *not* be the substrate ARTS is "built on" for the distributed case.
- **"ARTS on async" = ARTS lowers its node-local EDT bodies + fire-when-ready
  through `async`** (reusing `async.execute`/`await_all`, `async-to-async-runtime`,
  ref-counting, `AsyncToLLVM`, the runtime) and adds the distributed layer natively.
  `async.execute` ("fires when tokens/values ready") ≈ `arts.edt`.
- **Reject** `async` as a mandatory SDE→ARTS middle stage (sync→async→distributed
  double translation that re-lifts the owner/route the async form discards).
- **The correctness boundary:** at ARTS lowering split each EDT's deps into
  **local → `async`** and **remote → ARTS DB acquire + RMA**. Barrier model:
  `su_barrier` → `async.await_all`; barrier-free, non-conflicting CUs → sibling
  `async.execute` (operands = access-derived deps); `nowait` → suppress the await.
- Two honest limits: the overlap *analysis* is net-new (async ships IR, not
  analysis), and this is greenfield wiring against a currently-dormant dialect.

## What gets deleted (the simplification)

Ops/enums: `sde.mu_access_window`, `sde.redist`, `SdeMovementFamily`,
`SdeReductionStrategy`, `SdeAsyncStrategy`, `SdeRepetitionStructure`, `arrayId`/
`array_layout_root` (→ SSA identity), the dead in-house `asyncStrategy`.
Attributes: `arrayLayout`, `physicalOwnerDims`, `physicalBlockShape`,
`iterationTopology`, `physicalHaloShape`, `commVolumeBytes`, `layoutsDisagree`,
`logicalWorkerSlice` (→ loop band), `schedule`/`chunkSize` (→ optional hint).
Passes: `sde-loop-pattern-facts`, `sde-parallelize` (→ the new split),
`schedule-refinement`, `chunk-opt`, `raise-to-mu-access-window`,
`mu-access-window-sync-opt`, and ~6 of the 8 `verify-*` passes.
Knobs/cost: `min_distributed_tile_bytes`, `kAbstractBlockFactor`, the fabricated
`getL2CacheSize`/task-cost constants, the hand-rolled `tryGetAffineExpr`/
`extractDimOffset`.

## Migration — the unified dependency DAG

Two macro-phases: **CLEANUP** (S0–S16) makes the IR no-contract; **SCALING**
(S17–S21) delivers the measured 2-node levers; **EXECUTOR** (S22–S25) is the async
greenfield. **CLEANUP green does NOT imply scaling fixed** — that is S26. Every
gate inherits the Phase-0 RED baseline.

```
PHASE 0 — GATE (no code)
  S0  Pin the RED SDE+ARTS lit baseline AS A COMMITTED ARTIFACT: per-test
      RED/GREEN, pre-existing-vs-genuine, root-cause class; the 44 SdeRedistribute
      fail-closures bucketed; megalarge 1n->2n on the UNCHANGED binary. Hard
      blocker on every later gate.

PHASE 1 — INERT (no deps)
  S1  sde::buildSuIterate helper (byte-identical).        [lit delta==0]
  S2  Inert enum/attr deletions (SdeAsyncStrategy, SdeRepetitionStructure,
      mu_alloc.arrayId). blockedBy S1.                    [lit delta==0]
  S1v op-level Phase-B pure-redundancy verifier deletions.

PHASE 2 — STRUCTURE (affine + parallelize)
  S3  Relocate LowerAffine out of pre-planning (Compile.cpp:1112/1126; keep :1303).
      [DONE]
  S4  Swap SuLoopAccessAnalysis/Parallelize/Tiling/Interchange onto upstream affine
      (MemRefAccess/checkMemrefAccessDependence/ValueBounds/tilePerfectlyNested);
      DELETE tryGetAffineExpr/extractDimOffset/stripMineLoop. blockedBy S3.
      [PARTIAL: MemRefAccess + affine nest in SuLoopAccessAnalysis; S4d re-raise after
      Tiling; planning-head LowerAffine bridge removed; hand parser for scf/memref
      and Tiling scf owner loops remain]
  S4b PORT analyzeDepOwnerAccessIndex + halo classifiers onto upstream affine.
      [DONE]
  S5  Parallelize single->parallel leaf-kind fix + verifier. **Re-DERIVE the leaf
      site empirically — the cited Parallelize.cpp:638 is a `builder.clone` body
      loop at the real path Transforms/dep/loop/, NOT the leaf-kind site.**
      [DONE]
  S6  Post-interchange/tiling parallelize re-run (D-a on pre-wrap func.func).
      blockedBy S4; value depends on S5.  [DONE]

PHASE 3 — COST ELIMINATION (independent of movement ops)
  S7  Delete commVolumeBytes ATTR structurally (§5.9.3: estimateCommVolume/
      kAbstractBlockFactor/argmin gone; consumers use tilePayloadBytes/
      muBlockCount>1). blockedBy S4. NOT coupled to su.halo.  [DONE]

PHASE 4 — MOVEMENT OPS
  S8  Add su.halo + su.reduce_scatter ops + op verifiers + boundary dispatch; halo
      as a slice (validExtents stays == blockExtent). blockedBy S4.
  S9  Remove min_distributed_tile_bytes end-to-end. blockedBy S8.
  S10 LayoutAssignment commits consumer required-read layout as target type; delete
      the self-edge; re-host fail-closed diagnostic. blockedBy S8.
  S11 Retire sde.redist + SdeMovementFamily + VerifySdeRedistribute. blockedBy S10
      + op-level Phase E.

PHASE 5 — WINDOW / PHYSICAL COLLAPSE
  S12 Collapse mu_access_window to query-only boundary access (validExtents==blockExtent).
      blockedBy S4b.  [DONE: op + raise/verify passes deleted; queryAccessWindows only]
  S13 Delete physicalOwnerDims/physicalBlockShape (×N). Convert ARTS readers
      (LoweringFactUtils/CreateDbs/BlockContractionSplit/SdeToArtsBoundary +
      resolveArtsOwnerSlotMapping) FIRST. blockedBy A1+S12.
  S14 Delete arrayLayout/arrayId/array_layout_root/iterationTopology/distributionKind/
      reductionStrategy (SSA identity). blockedBy S13.
  S15 Remove ScheduleRefinement/ChunkOpt + schedule/chunkSize; relocate
      logicalWorkerSlice to a CU loop band; reclassify inPlace*.

PHASE 6 — RAISE GENERALIZE
  S16 Generalize raise-to-sde CORE (N-D, re-entrancy guard, per-axis split,
      reduction reassoc-license, in-place stencil — each fail-closed); delete the
      standalone sde-parallelize. blockedBy S5/S6.
```
*Op-level Phases A–F interleave (B in Phase 1; D gates on S12; E gates on
S11/S13); each verify pass is deleted only after its op verifier lands. Convert
readers to the type/op FIRST, delete the attribute LAST.*

## Migration — Phase 8: the per-kernel scaling levers (cleanup ENABLES, this DELIVERS)

CLEANUP (S0–S16) makes the IR clean but does **not** fix scaling. These land the
`benchmark-grounded.md` levers, each with its own megalarge 1n→2n gate.

```
  S17 Lever A — reader-grain reconcile (jacobi-for, poisson-for, pooling, batchnorm,
      atax, bicg; the "highest value" lever, currently sequenced NOWHERE). Extend
      reconcileArrayLayoutWithCommittedPhysicalShape from writer-role to READ homes.
      **Blast radius: 10 call sites** (RankExpandMu, Interchange, LayoutAssignment,
      Tiling x3, DistributionPlanning x3) — re-gate byte-diff at each. blockedBy Ph2.
  S18 Lever B — halo-bearing read window (stress/velocity/vel4sg/jacobi-for/
      poisson-for/seidel-2d). REPLACES deleted RaiseToMuAccessWindow: the boundary
      parser projects su.halo radii into the acquire as HaloSliceAttr. **Relocate
      the MuAccessWindow.cpp:301 validExtents widening into the boundary read
      derivation — don't lose it with the pass.** blockedBy S8, S17, S19(FEM), S4b-PORT.
  S19 Lever D — FEM one writer-consistent owner grid (stress/velocity/vel4sg, today
      0-byte binaries, accepted-as-broken). LayoutAssignment commits ONE owner grid
      shared by init+compute writer SUs (compute-weighted order on permutation);
      fail closed only if no single legal order exists. PREREQ for S18 on FEM.
  S20 Lever C — genuine target!=source repartition (3mm, 2mm, atax, bicg). blockedBy
      S10, S11. Enables the all_to_all realizer (rewritten to consume su.all_to_all).
      **Cross-owner COMBINE is architecture-scale — atax/bicg stay non-scaling
      pending an ARTS combine realizer; state this in the expected outcome.**
  S21 Lever E — split block_contraction K-grain (gemm, 2mm, 3mm). blockedBy S10.

  S26 SCALING VALIDATION GATE — a PER-KERNEL megalarge 1n→2n delta for EACH of
      S17–S21 vs the S0 baseline (a single aggregate number can hide a lever
      delivering nothing). This is the ONLY gate that proves scaling was fixed.
```

## ARTS / ARTS-RT parity (the audit the SDE-only plan skipped)

```
PHASE 7 — ARTS no-contract audit (= design-revision Part 8). Per-attr 3-fates
  table for the 84 OptionalAttrs on arts.db_alloc/db_acquire/edt. Anti-pattern
  docstring verbatim at ArtsAttrs.td:190-191. Fate-1 (recompute from CU-body
  accesses): ArtsDepPattern, EdtDistributionPattern, distribution_pattern,
  partition_mode, the stencil_* family. Convert readers (LoweringFactUtils.cpp:
  67-151, PartialReductionSplit) FIRST, delete the boundary stamp LAST. Gates the
  VerifyArtsCdag fold AND S13.

  CORRECT op-level-verification.md's false premise: ARTS does NOT have zero verify
  passes (VerifyArtsCdag + VerifyArtsObjectsOnly, + 5 ARTS-RT verify passes: verify-{pre,edt,db,epoch}-lowered + verify-lowered). BUT
  ARTS IS already adequately op-verified — the only action is (i) fix the false
  grep claim, (ii) note VerifyArtsCdag's whole-module physical-layout walk is the
  one irreducible ModuleOp verifier, gated on PHASE 7 (db_alloc layout->type). Do
  NOT retrofit SDE-8-style standalone verify passes onto ARTS.

PHASE 9 — ARTS local-executor (async) lowering [greenfield: 0 usage, 0 tests].
  S22 arts.edt body -> async.execute (operands = access-derived local deps).
  S23 su_barrier -> async.await_all; nowait -> suppress await.
  S24 remote deps stay ARTS DB-acquire + RMA (locate the local/remote split in
      pass order vs arts_rt.edt_create).
  S25 wire async-to-async-runtime + AsyncToLLVM; state which ARTS-RT passes
      (edt/epoch/db-lowering) retire vs retain; re-point verify-*-lowered.
  Each BLOCKED until a standalone async lit test exists. Highest risk; gate
  independently of the SDE cleanup.
```

## Loose ends

- **Cost model — `SDECostModel` / `ARTSCostModel`:** [DONE] fabricated
  `getTaskCreationCost` / `getTaskSyncCost` / `getReductionCost` /
  `getAtomicUpdateCost` / `getDataAccessCost` deleted; fixed iteration floors
  replace cost-derived thresholds. `getL2CacheSize` was already gone with
  `sde-default-tile-floor`.
- **`arts-all-to-all.md`** must be rewritten to consume the `su.all_to_all` **op**,
  not `sde.redist family=all_to_all_like` + `commVolumeBytes` (deleted); sequence
  after S11.
- **`proposed-passes.md`** is superseded: delete `sde-default-tile-floor` (knob
  removed, not defaulted); re-anchor hypergraph grouping on `muBlockCount>1`; the
  A–E levers (this file's Phase 8) are the real spec.
- **Test-coverage is a gate, not an afterthought:** S20 (hand-authored `su.all_to_all`
  lit + 2N repartition reproducer), S22–S25 (async lit), `D-b` (affine-in-CU
  consumer lit), S6 (D-a re-raise termination test), S18 (jacobi2d HaloSliceAttr
  reproducer) — each BLOCKED until its test exists.
- **Inert passes — repair-or-delete (decide, don't leave dormant):**
  `elementwise-fusion` fires on **0/21** (the `elementwise_pipeline` class is
  authored upstream in `SuLoopAccessAnalysis`, not by the pass) → **delete** unless a
  motivating kernel appears; `iteration-space-decomposition` is a **no-op despite the
  boundary-guard structure being present** (its matcher wants an `andi` chain, the IR
  has an `arith.select` fused predicate) → **fix the matcher or delete**;
  `mu-access-window-sync-opt` (0 barriers removed) is **deleted with `mu_access_window`**
  (S12). Add each as an explicit DAG step, not a silent survivor.

## Open items (honest residue)

- **`su.all_to_all` permuted/transpose** needs a `DbAlloc` owner-dims attribute
  (the only-leading-owner-dims limit); order-preserving repartition works now.
- **Reassociation policy** for plain sequential float reductions (always-serial vs
  an opt-in pragma).
- **Step −1 RED-baseline characterization** is a prerequisite for every delta gate.
- **`AffineScope` on CU/SU ops (`D-b`) is deferred — `D-a` is the decision.**
  Iterative re-raise on the pre-wrap `func.func` covers all 21 with no dialect
  change; `D-b` (re-raising already-wrapped CU/SU bodies) is future-proofing, taken
  only behind lit coverage for the affine-in-CU-body consumers, and adds only the
  `AffineScope` trait (never `AutomaticAllocationScope`).

See [`design-revision.md`](./design-revision.md) Parts 1-7 for the file:line
derivations behind each line above, [`arts-all-to-all.md`](./arts-all-to-all.md)
for the repartition realizer, [`op-level-verification.md`](./op-level-verification.md)
for the verifier migration, and [`benchmark-grounded.md`](./benchmark-grounded.md)
for why each lever matters per kernel.
