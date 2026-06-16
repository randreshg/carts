# Compiler Code-Health Backlog (Source-Verified)

> Phase 0 intake artifact for spec `0001-distribution-1n-2n`. Generated 2026-06-15.
> Each row ties a spec story/task to constitution principles, C1–C10 hazards,
> owning layer, first wrong fact, source evidence, required disposition, and
> focused validation.

Status key: **DONE** (verified in checkout), **OPEN** (work remains), **BLOCKED**
(environment or architecture gate), **DEFERRED** (explicit direction).

---

## US1 — Same-owner re-tile / halo (T005–T009) — P1

| ID | Task | Constitution | C-hazard | Layer | First wrong fact | Source evidence | Disposition | Validation | Status |
|----|------|--------------|----------|-------|------------------|-----------------|-------------|------------|--------|
| U1-1 | T006 gate AllToAll on `!sameOwnerDimSet` | P3 no pattern hacks; real transform | C3 init-vs-compute grain | SDE | Same-owner grain mis-classified as cross-owner movement | `EdgeClassify.cpp:118-119` `classifyRedistributionEdge` returns `AllToAll` only when owner sets differ; `RedistributionEdges.cpp:260-267` fail-closed on same-owner retile | Structural classifier + fail-closed diagnostic | `dekk carts lit lib/carts/dialect/sde/test/distribution/sde_redistribute_rejects_same_owner_retile.mlir` | **DONE** |
| U1-2 | T007 `reconcileSameOwnerArrayGrain` | P2 facts not plans | C3 | SDE | Writer/reader blockShape disagree at same owner dim | `BlockGrainPlan.cpp:152` `reconcileSameOwnerArrayGrain`; invoked from `BlockGrainPlan` pass `:380` | Real transform: GCD re-commit physical layout | `dekk carts lit …/sde_redistribute_reconciles_same_owner_retile` (if present) + compile stream/correlation | **DONE** (non-matmul) |
| U1-3 | T008 jacobi-for/poisson-for `su_halo` | P2 movement as ops | C6 halo / C3 init | SDE | Init 2×2 grid vs stencil row-strip → malformed all_to_all | `Redistribute.cpp:529-703` init-writer window author; `RankExpandedEdgeProject.cpp:292-399` `projectRankExpandedHaloEdge`; audit `distribution-audit-2n.md` jacobi-for/poisson-for rows | Real transform: unify owner layout + rank-expanded halo, never all_to_all | `dekk carts compile jacobi-for/poisson-for --pipeline create-dbs` | **PARTIAL** (create-dbs PASS both; poisson post-db-refinement OPEN; jacobi 1n medium Correct=YES) |
| U1-4 | T008 reconcile expanded facts without flat false-positive | P10 no coarse fallback via guessing | C6 | SDE | `recoverMuPhysicalLayoutFromExpandedType` on flat memref rewrites committed facts | `Redistribute.cpp:234-293` now uses `recognizeExpandedBlockGridMu` + rank guard | Real transform guard + fail-closed | All `sde_redistribute_*` lit (41/41) | **DONE** (this session) |
| U1-5 | T009 2n size-large Correct=YES for Class A six + pooling | P11 validation gate | — | E2E | — | `tasks.md` T009; `quickstart.md` per-class spot check | Benchmark gate | `dekk carts benchmarks run … --nodes 2 --size large` | **BLOCKED** (cluster-only 2n per `.carts/findings/README.md`) |

---

## US2 — Committed block plans → per-block DBs (T010–T013) — P1

| ID | Task | Constitution | C-hazard | Layer | First wrong fact | Source evidence | Disposition | Validation | Status |
|----|------|--------------|----------|-------|------------------|-----------------|-------------|------------|--------|
| U2-1 | T010 lit multi-block → per-block DB + owner map | P1 one responsibility | C6/C coarse funnel | ARTS | SDE block plan dropped at create-dbs | `lib/carts/dialect/arts/test/db/db_distributed_ownership_realization_marks_distributed.mlir`; `…/marks_intranode_block_db.mlir` | Lit contract + realization | `dekk carts lit lib/carts/dialect/arts/test/db/` | **DONE** (lit exists, passes) |
| U2-2 | T011 consume `arrayLayout` in CreateDbs | P3 no coarse repair | C6 | ARTS | Whole-array coarse DB despite committed blocks | `CreateDbs.cpp:836,1130` raw-bridge coarse (host/global only); `SdeToArtsBoundaryStorage.cpp` fail-closed on block-grid without layout | Real per-block single-writer DB or fail-closed | compile jacobi-for/poisson-for `--pipeline create-dbs`; lit `sde-to-arts-rejects-block-grid-without-layout.mlir` | **PARTIAL** (boundary fail-closed landed @d55d2bae1; create-dbs block DBs on jacobi/poisson; raw coarse bridge retained for stack scalars) |
| U2-3 | T012 per-block halo k±1 in ARTS path | P2 ops not attrs | C6 | ARTS | Stencil neighbor reads without halo_slice | `SdeToArtsBoundaryHaloLowering.cpp`; lit `db_commit_distributed_deps_commits_block_halo_window.mlir` | Real halo exchange on committed blocks | `dekk carts lit …/db_commit_distributed_deps_commits_block_halo_window.mlir` | **PARTIAL** (lit green; benchmark scaling unverified) |
| U2-4 | T013 2n owned blocks + scaling conv/jacobi2d/specfem | P11 | C6 | E2E | Node 2 owns zero blocks | audit Class C rows | Benchmark + artifact | 2n large campaign | **BLOCKED** (2n sandbox) |

---

## US3 — FEM writer layout (T019–T020) — P2

| ID | Task | Constitution | C-hazard | Layer | First wrong fact | Source evidence | Disposition | Validation | Status |
|----|------|--------------|----------|-------|------------------|-----------------|-------------|------------|--------|
| U3-1 | T019 `WriterLayoutCommit` co-iterated writes | P2 transform ladder | C3 | SDE | Written stencil outputs lack owner layout | `WriterLayoutCommit.cpp:758+`; lit `sde_writer_layout_commit_coiterated_stencil.mlir` | Propagate read owner dims + rank-expand output MU | `dekk carts lit …/sde_writer_layout_commit_coiterated_stencil.mlir` | **DONE** (lit 41/41) |
| U3-2 | T020 FEM 2n Correct=YES stress/velocity/vel4sg | P11 | C3/C6 | E2E | Whole DB on produced state | audit Class C′ rows | Benchmark | 2n FEM suite | **BLOCKED** (2n) |

---

## US4 — Residual access windows (T015–T018) — P2

| ID | Task | Constitution | C-hazard | Layer | First wrong fact | Source evidence | Disposition | Validation | Status |
|----|------|--------------|----------|-------|------------------|-----------------|-------------|------------|--------|
| U4-1 | T015 layernorm gamma/beta windows | P2 | C1 scalar misread | SDE/ARTS boundary | Verify EDT touches replicated params without window | `SdeToArtsBoundaryDepAnalysis.cpp`; lit `verify-raw-access-covered-*.mlir` | Access-window deps on all touching EDTs | compile layernorm; boundary lit | **PARTIAL** (1n medium Correct=YES; create-dbs PASS @20260616; 2n unverified) |
| U4-2 | T016 activations residual scalar fusion | P2 | C1/C4 | SDE | Softmax/checksum scalar captured raw | `SdeToArtsBoundaryDepAnalysis.cpp` / `DepFromWindow` | Fuse into owning serial CU + window | compile activations | **OPEN** (LARGE-only boundary bugs per README) |
| U4-3 | T017 bicg init-writer window | P2 | C3/C4 | SDE | `residual_source` A init without layout root | SDE boundary path | Commit access window for init-writer | compile bicg | **OPEN** (SDE planning unblocked; boundary OPEN) |
| U4-4 | T018 volume-integral NREPS guard windows | P2 | C4 | SDE | Window-commit skips NREPS-guarded SU | `SuLoopAccessAnalysis` / boundary dep collect | Window residual compute SU | compile volume-integral | **OPEN** (1n medium PASS prior; boundary not re-verified this session) |

---

## US5 — Owner-dim reductions (T021–T022) — P2

| ID | Task | Constitution | C-hazard | Layer | First wrong fact | Source evidence | Disposition | Validation | Status |
|----|------|--------------|----------|-------|------------------|-----------------|-------------|------------|--------|
| U5-1 | T021 classify `block_contraction` / `partialReduction` | P2 | C5 | SDE | Edge selector vs verifier disagree | `RedistributionEdges.cpp:205-235`; `LayoutAssignment` / `DistributionPlanning` | Align classifier + verifier | lit `sde_redistribute_cross_owner_reductions.mlir` | **PARTIAL** (reduce_scatter lit green; batchnorm fail-closed) |
| U5-2 | T022 atax reduce_scatter → write result `y` | P2 | C5 | SDE | reduce_scatter on read `tmp` not write `y` | `Redistribute.cpp:727-765` write-target retarget; audit atax row | Retarget or precise fail-closed | compile atax @ capacity>1 → fail-closed diagnostic | **DONE** (fail-closed = correct per spec) |

---

## US6 — gemm-large EDT split (T014) — P3

| ID | Task | Constitution | C-hazard | Layer | First wrong fact | Source evidence | Disposition | Validation | Status |
|----|------|--------------|----------|-------|------------------|-----------------|-------------|------------|--------|
| U6-1 | T014 `EdtSplitForMixedDeps` | P1 one file/pass | — | ARTS | Replicated-B init + distributed-C writer in one EDT | `EdtSplitForMixedDeps.cpp:47+`; lit `sde-to-arts-splits-owner-crossing-grouped-distributed-writer.mlir` | Split EDT; routing stays in `WriterOwnerRoute.cpp` | `dekk carts lit …/sde-to-arts-splits-owner-crossing-grouped-distributed-writer.mlir` | **DONE** (lit 37/37 ARTS) |
| U6-2 | gemm large 2n Correct=YES <50s | P11 | — | E2E | Launch consistency @ large | tasks.md T014 checkpoint | Benchmark | `dekk carts benchmarks run polybench/gemm --nodes 2 --size large` | **BLOCKED** (2n sandbox) |

---

## US7 — seidel fail-closed (T023) — P3

| ID | Task | Constitution | C-hazard | Layer | First wrong fact | Source evidence | Disposition | Validation | Status |
|----|------|--------------|----------|-------|------------------|-----------------|-------------|------------|--------|
| U7-1 | T023 in-place wavefront diagnostic | P3 fail-closed | C2 | SDE | Half-stamped halo without windows | `DistributionFailClosed.cpp:88` `emitStencilWavefrontFailClosed`; lit `sde_seidel_in_place_wavefront_fail_closed.mlir` | Fail closed before halo facts | compile seidel-2d @ 2 nodes → non-zero exit + diagnostic | **DONE** |

---

## US8 — Codegen perf (T024–T026) — P2 DEFERRED

| ID | Task | Constitution | C-hazard | Layer | First wrong fact | Source evidence | Disposition | Validation | Status |
|----|------|--------------|----------|-------|------------------|-----------------|-------------|------------|--------|
| U8-1 | T024 hoist div/rem indexing | P3 structural | codegen | ARTS-RT/codegen | Per-access `divui`/`remui` | `Codegen.cpp`; audit §4 rank-expand blowup | Strength-reduce to strided/affine | wall-time delta on conv/jacobi | **DEFERRED** (`goal-large-perf.md`; partial alias fix landed) |
| U8-2 | T025 vectorize accumulators | — | C1 | codegen | Scalar MAC loops | US8 commit `07a3c27e8` scalar-accumulator promotion | Vectorize promoted reductions | gemm AVX ops evidence in README | **PARTIAL** (compiler evidence; wall-time not captured) |
| U8-3 | T026 fuse layernorm passes | — | — | codegen | 3 scalar passes | deferred | Fuse mean/var/normalize | layernorm wall-time | **DEFERRED** |

---

## US9 — Pass splits / code health (T027–T031) — P3

| ID | Task | Constitution | C-hazard | Layer | First wrong fact | Source evidence | Disposition | Validation | Status |
|----|------|--------------|----------|-------|------------------|-----------------|-------------|------------|--------|
| U9-1 | T027 `RedistributionEdges` → EdgeClassify + RankExpandedEdgeProject | P1/P1a split-axis | — | SDE | Monolithic edge analysis | `phase11-recarve-plan.md`; files under `Analysis/EdgeClassify.cpp`, `RankExpandedEdgeProject.cpp` | Split complete | lit 41/41 SDE | **DONE** |
| U9-2 | T028 `DistributionPlanning` split | P1 | — | SDE | 1000+ line megapass | `BlockGrainPlan.cpp`, `OwnerDimSelect.cpp`, `MovementTagging.cpp`, `DistributionFailClosed.cpp` | Split complete | lit + build | **DONE** |
| U9-3 | T029 boundary dep analysis split | P1 | — | ARTS | CoarseSu + RawAccessVerify carved | `SdeToArtsBoundaryCoarseSu.cpp`, boundary tests | Split complete | ARTS lit 37/37 | **DONE** |
| U9-4 | T030 boundary access lowering split | P1 | — | ARTS | HaloRealize / EdtBuild / AccessRewrite | `SdeToArtsBoundaryHaloLowering.cpp`, etc. | Split complete | ARTS conversion lit | **DONE** |
| U9-5 | T031 SuLoopAccess / LayoutAssignment / LaunchConsistency | P1 | — | SDE/ARTS | Remaining >1000-line watch list | `phase11-split-keep-decisions.md` | Record split/keep per split-axis test | `find … \| awk '$1>1000'` audit | **DONE** (decisions recorded 2026-06-16) |

---

## Cross-cutting coarse-DB audit (C1–C10)

| Class | Primary SDE/ARTS fix layer | Live coarse path to retire | Fail-closed when illegal |
|-------|------------------------------|----------------------------|--------------------------|
| C1 scalar accumulator | SDE classify + ARTS partial DBs | `createCoarseDbBackedMemref` on reduction tails | `sawAccess=false` diagnostic |
| C2 in-place wavefront | SDE skew/diamond or fail | plain block + halos on seidel | `emitStencilWavefrontFailClosed` (**DONE**) |
| C3 init-vs-compute grain | SDE BlockGrainPlan + redist reconcile | degenerate all_to_all | same-owner retile fail (**DONE**) |
| C4 dynamic size erasure | SDE const-fold extents | single-block fallback | non-constant extent diagnostic |
| C5 cross-owner reduction | SDE contraction + ARTS combine tree | mis-targeted reduce_scatter | atax/batchnorm fail-closed (**DONE** precise) |
| C6 halo / rank-expand | SDE su_halo + ARTS halo_slice | coarse whole DB funnel | no halo window → fail (**OPEN** conv/jacobi2d) |
| C7–C10 | see `docs/vision/findings/coarse-db-taxonomy.md` | — | — |

---

## Session fixes (2026-06-15)

| Change | Files | Evidence |
|--------|-------|----------|
| Guard rank-expanded layout reconciliation | `Redistribute.cpp` | SDE lit 41/41; flat memref tests no longer silently reconciled |
| AllToAll geometry fail-closed | `RedistributionEdges.cpp` | `sde_redistribute_rejects_unrepresentable.mlir` PASS |
| Zero halo radius: recover from loads when committed=0, accept explicit zero | `RankExpandedEdgeProject.cpp` | `sde_writer_layout_commit_coiterated_stencil.mlir` + `sde_redistribute_reconciles_nested_owner_strip_halo.mlir` PASS |

## Session validation (2026-06-16, phases B–K)

| Phase | Commit | Evidence |
|-------|--------|----------|
| B US2 | `d55d2bae1` | Block-grid task-dep fail-closed + lit; SDE/ARTS/ARTS-RT lit 92/92 |
| C US1 | `c9540e173` | Init-writer grain reconcile + lit; jacobi-for/poisson-for create-dbs PASS |
| D US4 | (verify) | layernorm create-dbs PASS + 1n medium Correct=YES; activations/bicg/volume OPEN |
| E US3 | (verify) | specfem3d/stress + velocity 1n medium Correct=YES |
| F US5 | (verify) | atax + batchnorm 1n medium Correct=YES (2n fail-closed not re-run) |
| G US7 | (verify) | `sde_seidel_in_place_wavefront_fail_closed.mlir` PASS; seidel 1n medium Correct=YES |
| H US8 | DEFERRED | No codegen changes this session |
| I US9 | findings | `phase11-split-keep-decisions.md` |
| J | (this session) | build PASS; lit 42+38+12; 1n medium touched rows (see below) |

### 1n medium touched-row benchmarks (`20260616_040535`, `20260616_040639`)

| Benchmark | Correct | Notes |
|-----------|---------|-------|
| kastors-jacobi/jacobi-for | YES | OMP+ARTS PASS |
| kastors-jacobi/poisson-for | build_arts fail | create-dbs compile PASS; post-db-refinement distributed-writer error |
| ml-kernels/layernorm | YES | create-dbs PASS |
| specfem3d/stress | YES | |
| specfem3d/velocity | YES | |
| polybench/atax | YES | 1n; 2n fail-closed expected |
| ml-kernels/batchnorm | YES | 1n; 2n fail-closed expected |
| polybench/seidel-2d | YES | 1n; SDE fail-closed at 2n by design |

---

## Validation ladder snapshot

| Gate | Result |
|------|--------|
| `dekk carts build` | PASS |
| `dekk carts lit lib/carts/dialect/sde/test` | **42/42 PASS** |
| `dekk carts lit lib/carts/dialect/arts/test` | **38/38 PASS** |
| `dekk carts lit lib/carts/dialect/arts-rt/test` | **12/12 PASS** |
| `dekk carts pipeline --json` | PASS |
| 1n touched rows (medium) | 7/8 Correct (poisson-for build_arts fail) |
| 2n SC-001/SC-002 | **BLOCKED** — cluster-only |

---

## Remaining critical path to SC-001/SC-002

1. **US4 activations/bicg/volume-integral**: boundary access-window authoring (LARGE-triggered).
2. **US1 poisson-for**: post-db-refinement distributed-writer diagnostic (`poisson-for.c:105`).
3. **US2 T013 / US1 T009 / US6 gemm 2n**: cluster 2n benchmark campaign with Correct=YES + owned-block evidence.
4. **US8**: deferred perf; does not block correctness SC.
