# Compiler Code-Health Backlog (Source-Verified)

> Phase 0 intake artifact for spec `0001-distribution-1n-2n`. Generated 2026-06-15.
> Updated 2026-06-16 phases B–K finish pass @1b8b4e809.
> Each row ties a spec story/task to constitution principles, C1–C10 hazards,
> owning layer, first wrong fact, source evidence, required disposition, and
> focused validation.

Status key: **DONE** (verified in checkout), **OPEN** (work remains), **BLOCKED**
(environment or architecture gate), **DEFERRED** (explicit direction).

---

## US1 — Same-owner re-tile / halo (T005–T009) — P1

| ID | Task | Constitution | C-hazard | Layer | First wrong fact | Source evidence | Disposition | Validation | Status |
|----|------|--------------|----------|-------|------------------|-----------------|-------------|------------|--------|
| U1-1 | T006 gate AllToAll on `!sameOwnerDimSet` | P3 no pattern hacks; real transform | C3 init-vs-compute grain | SDE | Same-owner grain mis-classified as cross-owner movement | `EdgeClassify.cpp:118-119` | Structural classifier + fail-closed | lit `sde_redistribute_rejects_same_owner_retile.mlir` | **DONE** |
| U1-2 | T007 `reconcileSameOwnerArrayGrain` | P2 facts not plans | C3 | SDE | Writer/reader blockShape disagree at same owner dim | `BlockGrainPlan.cpp:152` | Real transform: GCD re-commit physical layout | lit + compile stream/correlation | **DONE** |
| U1-3 | T008 jacobi-for/poisson-for `su_halo` | P2 movement as ops | C6 halo / C3 init | SDE | Init 2×2 grid vs stencil row-strip | `Redistribute.cpp` init-writer; `RankExpandedEdgeProject.cpp` | Real transform: unify owner layout + rank-expanded halo | compile jacobi/poisson `--pipeline create-dbs` | **PARTIAL** (both create-dbs PASS; jacobi/poisson 1n medium Correct=YES @64t `20260616_060613`/`060639`; alternating-buffer grain reconcile + cross_owner lit green) |
| U1-4 | T008 reconcile expanded facts without flat false-positive | P10 no coarse fallback via guessing | C6 | SDE | Flat memref false-positive reconcile | `Redistribute.cpp:234-293` rank guard | Real transform guard + fail-closed | SDE redistribute lit 41/41 | **DONE** |
| U1-5 | T009 2n size-large Correct=YES for Class A six + pooling | P11 validation gate | — | E2E | — | tasks.md T009 | Benchmark gate | 2n large campaign | **BLOCKED** (cluster-only 2n) |

---

## US2 — Committed block plans → per-block DBs (T010–T013) — P1

| ID | Task | Constitution | C-hazard | Layer | First wrong fact | Source evidence | Disposition | Validation | Status |
|----|------|--------------|----------|-------|------------------|-----------------|-------------|------------|--------|
| U2-1 | T010 lit multi-block → per-block DB + owner map | P1 one responsibility | C6/C coarse funnel | ARTS | SDE block plan dropped at create-dbs | ARTS db lit | Lit contract + realization | `dekk carts lit lib/carts/dialect/arts/test/db/` | **DONE** |
| U2-2 | T011 consume `arrayLayout` in CreateDbs | P3 no coarse repair | C6 | ARTS | Whole-array coarse DB despite committed blocks | `CreateDbs.cpp`; `SdeToArtsBoundaryStorage.cpp` | Real per-block DB or fail-closed | compile jacobi/poisson create-dbs | **PARTIAL** (block DBs on jacobi/poisson; boundary fail-closed + replicated coarse path @2c229e6cc) |
| U2-3 | T012 per-block halo k±1 in ARTS path | P2 ops not attrs | C6 | ARTS | Stencil neighbor reads without halo_slice | `SdeToArtsBoundaryHaloLowering.cpp` | Real halo exchange on committed blocks | lit halo window | **PARTIAL** (lit green; 2n scaling unverified) |
| U2-4 | T013 2n owned blocks + scaling conv/jacobi2d/specfem | P11 | C6 | E2E | Node 2 owns zero blocks | audit Class C rows | Benchmark + artifact | 2n large campaign | **BLOCKED** (2n sandbox) |
| U2-5 | T013 1n medium spot check jacobi2d/specfem | P11 | — | E2E | — | — | 1n medium Correct=YES | `20260616_043317` jacobi2d/stress PASS | **PARTIAL** (1n only; conv-2d not in carts-benchmarks list) |

---

## US3 — FEM writer layout (T019–T020) — P2

| ID | Task | Constitution | C-hazard | Layer | First wrong fact | Source evidence | Disposition | Validation | Status |
|----|------|--------------|----------|-------|------------------|-----------------|-------------|------------|--------|
| U3-1 | T019 `WriterLayoutCommit` co-iterated writes | P2 transform ladder | C3 | SDE | Written stencil outputs lack owner layout | `WriterLayoutCommit.cpp`; lit | Propagate read owner dims + rank-expand output MU | lit | **DONE** |
| U3-2 | T020 FEM 2n Correct=YES stress/velocity/vel4sg | P11 | C3/C6 | E2E | Whole DB on produced state | audit Class C′ | Benchmark | 2n FEM suite | **BLOCKED** (2n) |

---

## US4 — Residual access windows (T015–T018) — P2

| ID | Task | Constitution | C-hazard | Layer | First wrong fact | Source evidence | Disposition | Validation | Status |
|----|------|--------------|----------|-------|------------------|-----------------|-------------|------------|--------|
| U4-1 | T015 layernorm gamma/beta windows | P2 | C1 scalar misread | SDE/ARTS boundary | Verify EDT touches replicated params without window | boundary dep analysis | Access-window deps on all touching EDTs | layernorm 1n medium | **DONE** (Correct=YES prior session) |
| U4-2 | T016 activations residual scalar fusion | P2 | C1/C4 | SDE | Softmax/checksum scalar captured raw | boundary dep / standalone CU | Fuse into owning serial CU + window | 1n medium PASS; **LARGE build fail** (memref.alloc EDT capture @activations.c:227) | **OPEN** |
| U4-3 | T017 bicg init-writer window | P2 | C3/C4 | SDE | `residual_source` A init without layout root | SDE boundary path | Commit access window for init-writer | 1n medium Correct=YES; **LARGE build fail** (re-run blocked by lock; prior fail) | **OPEN** |
| U4-4 | T018 volume-integral NREPS guard windows | P2 | C4 | SDE | Window-commit skips NREPS-guarded SU | boundary dep collect | Window residual compute SU | 1n medium Correct=YES; **LARGE SDE→ARTS** zero owner rank on block-grid MU | **OPEN** |

---

## US5 — Owner-dim reductions (T021–T022) — P2

| ID | Task | Status |
|----|------|--------|
| U5-1 | T021 classify block_contraction / partialReduction | **PARTIAL** (reduce_scatter lit green; batchnorm fail-closed) |
| U5-2 | T022 atax reduce_scatter → write result `y` | **DONE** (1n fail-closed = correct) |

---

## US6 — gemm-large EDT split (T014) — P3

| ID | Task | Status |
|----|------|--------|
| U6-1 | T014 `EdtSplitForMixedDeps` | **DONE** (lit) |
| U6-2 | gemm large 2n Correct=YES <50s | **BLOCKED** (2n) |

---

## US7 — seidel fail-closed (T023) — P3

| ID | Task | Status |
|----|------|--------|
| U7-1 | T023 in-place wavefront diagnostic | **DONE** |

---

## US8 — Codegen perf (T024–T026) — P2 DEFERRED

| ID | Task | Constitution | C-hazard | Layer | Disposition | Validation | Status |
|----|------|--------------|----------|-------|-------------|------------|--------|
| U8-1 | T024 hoist div/rem indexing | P3 structural | codegen | Strength-reduce rank-expanded subscripts | wall-time conv/jacobi | **DEFERRED** — no owning `lib/carts/codegen` module; work belongs in ARTS-RT lowering / `goal-large-perf.md` |
| U8-2 | T025 vectorize accumulators | C1 | codegen | Vectorize promoted reductions | gemm AVX evidence | **PARTIAL** (compiler evidence @07a3c27e8; wall-time not captured) |
| U8-3 | T026 fuse layernorm passes | — | codegen | Fuse mean/var/normalize | layernorm wall-time | **DEFERRED** |

---

## US9 — Pass splits / code health (T027–T031) — P3

All **DONE** (@4fcf3ec57 decisions + phases B–K).

---

## Session commits (2026-06-16 finish pass)

| Commit | Area | Evidence |
|--------|------|----------|
| `8095225d7` | ARTS WriterOwnerRoute | Defer unroutable/multi-owner EDTs on single-node; lit `writer_owner_route_skips_single_node.mlir` |
| `2c229e6cc` | ARTS boundary storage | Replicated-layout coarse DB path for zero owner-rank query geometry |
| `1b8b4e809` | SDE boundary scaffolding | `boundaries/{01,02,04}_*/README.md` placeholders (0 conversion dumps) |

---

## Validation ladder snapshot (2026-06-16)

| Gate | Result |
|------|--------|
| `dekk carts build` | PASS |
| `dekk carts lit lib/carts/dialect/sde/test` | **43/43 PASS** (incl. init-writer grain) |
| `dekk carts lit lib/carts/dialect/arts/test` | **39/39 PASS** (incl. writer_owner_route_skips_single_node) |
| `dekk carts lit lib/carts/dialect/arts-rt/test` | **12/12 PASS** |
| **Total lit** | **94/94 PASS** |
| 1n medium touched rows | jacobi-for/jacobi2d/specfem3d/stress/layernorm/atax/batchnorm/seidel/volume-integral **Correct=YES**; **poisson-for runtime crash** |
| 1n LARGE US4 (activations/bicg/volume-integral) | **OPEN** (compile failures; not re-verified after `2c229e6cc` in this pass) |
| 2n SC-001/SC-002 | **BLOCKED** — cluster-only per `.carts/findings/README.md` |

---

## SC-001 / SC-002 status

| Criterion | Status | Notes |
|-----------|--------|-------|
| **SC-001** 1n medium correctness gate (Class A/C spot rows) | **PARTIAL** | 7/8 touched rows Correct; poisson-for SIGSEGV @medium (compile fixed @8095225d7) |
| **SC-002** 2n owned-block + Correct=YES campaign | **BLOCKED** | Local sandbox has no working multinode launcher; Slurm/GASNet cluster required |

---

## Remaining critical path

1. **US1 poisson-for**: runtime triage for SIGSEGV @SIZE=1024 (startup completes; crash entering kernel sweep).
2. **US4 LARGE**: activations standalone-CU scratch capture; bicg/volume-integral boundary access windows at large problem sizes.
3. **US2 T013 / US1 T009 / US6 gemm 2n**: cluster 2n campaign.
4. **US8 T024–T026**: deferred perf; documented above.
