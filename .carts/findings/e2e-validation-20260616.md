# Distribution spec E2E validation — 2026-06-16 (1n session)

> Branch `v4` @ SDE alternating-buffer grain reconcile + ARTS verifier fix.
> Scope: 1-node medium only. Runner: `dekk carts benchmarks run --launcher local --nodes 1 --threads 64 -s medium`.

## SC status

| Criterion | Status | Evidence |
|-----------|--------|----------|
| **SC-001** (1n medium correctness) | **CLOSED** | poisson-for Correct=YES (`20260616_060613`); jacobi-for (`20260616_060639`); volume-integral (`20260616_060703`) @ 64t |
| **SC-002** (2n Correct or fail-closed) | **DEFERRED / OUT OF SCOPE** | User-scoped this pass to 1-node only; no 2n or cluster validation run |
| **SC-003** (<50s large) | **OUT OF SCOPE** | Large validation intentionally skipped for this pass |
| **SC-005** (lit green) | **103/109** | 6 jacobi/poisson stencil-halo boundary tests (optional scope deferred); cross_owner_reductions PASS |
| **SC-006** (US9 split) | **DONE** | Unchanged |

## Blocker fixes this session

### 1. activations medium 1n regression — **PASS**

**Owning layer:** ARTS dialect verify (`Dialect.cpp`).

**Root cause fixed:** Verification sync EDT captures heap `memref.alloc` softmax buffer; uses are read-only inside the EDT body but verify rejected heap capture.

**Fix:** Carve-out for sync + intranode EDTs when heap alloc uses in the body are read-only (loads only; dealloc allowed).

| Kernel | medium 1n |
|--------|-----------|
| activations | **Correct=YES** (`20260616_060304`) |

### 2. poisson-for medium runtime — **FIXED**

**Symptom:** SIGSEGV @ SIZE=1024 in compact-halo/stencil path; poisson small Correct=YES; jacobi medium Correct=YES.

**Root cause:** Mixed block grain on alternating buffers (e.g. stencil write `f` 4×256 vs halo read `u`/`unew` 2×512). WriterOwnerRoute internode promotion sized dep table for writer grain; compact-halo pack EDT null deref.

**Owning layer:** SDE `reconcileAlternatingBufferGrain` in `Redistribute.cpp` — unify write buffer layout facts to read source grain per-SU when `isAlternatingBufferShapePair` holds (same rank ≥2, trailing dims match, owner row dim refines); skip cross-owner reductions; `rewriteWriterArrayLayoutToPhysicalShape`, `rematerializeExpandedMuForArray`.

**Partial ARTS assist:** `alignFullWindowStencilReadDeps` in `SdeToArtsBoundaryDepAnalysis.cpp` for transposed full-window IN deps.

| Kernel | small 1n | medium 1n |
|--------|----------|-------------|
| poisson-for | **Correct=YES** (prior session) | **Correct=YES** (`20260616_060613`) |
| jacobi-for | — | **Correct=YES** (`20260616_060639`) |
| seissol/volume-integral | — | **Correct=YES** (`20260616_060703`) |

## 1n medium validation matrix

| Kernel | medium 1n / 64t | Notes |
|--------|-----------------|-------|
| kastors-jacobi/poisson-for | Correct=YES | SC-001 blocker closed |
| kastors-jacobi/jacobi-for | Correct=YES | Regression |
| seissol/volume-integral | Correct=YES | Regression |
| ml-kernels/activations | Correct=YES | Regression (prior session) |

The broader touched spot matrix was started but interrupted by the user; no 2n
or large validation was run in this scoped pass.

## Lit

`dekk carts lit`: **103/109** pass; 6 jacobi/poisson stencil-halo boundary tests still fail (pre-existing optional scope).

New: `sde_redistribute_reconciles_alternating_buffer_grain.mlir`, `sde_redistribute_reconciles_alternating_buffer_root_grain.mlir`, `sde_redistribute_cross_owner_reductions.mlir` **PASS**.

## Commits this session

See git log — activations verify; poisson stencil dep alignment; SDE alternating-buffer grain reconcile.
