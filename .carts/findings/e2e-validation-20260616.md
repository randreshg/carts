# Distribution spec E2E validation - 2026-06-16 (1n session)

> Branch `v4` @ `f91d741a2` after cleanup of unrelated tracked WIP.
> Scope: 1-node medium only. Runner: `dekk carts benchmarks run <bench> --size medium --threads 64 --nodes 1 --launcher local --no-rdma --timeout 180 --trace`.

## SC status

| Criterion | Status | Evidence |
|-----------|--------|----------|
| **SC-001** (1n medium correctness) | **CLOSED** | All 21 registered benchmarks passed at medium / 64 threads / 1 node / local launcher (`20260616_061857` through `20260616_062352`) |
| **SC-002** (2n Correct or fail-closed) | **DEFERRED / OUT OF SCOPE** | User-scoped this pass to 1-node only; no 2n or cluster validation run |
| **SC-003** (<50s large) | **OUT OF SCOPE** | Large validation intentionally skipped for this pass |
| **SC-005** (lit green) | **103/109** | 6 jacobi/poisson stencil-halo boundary tests still fail closed with missing committed SDE access-window deps; documented as pre-existing on v4/base |
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

| Kernel | medium 1n / 64t | Result id | Notes |
|--------|-----------------|-----------|-------|
| stream | Correct=YES | `20260616_061857_964177` | Fresh full sweep |
| kastors-jacobi/jacobi-for | Correct=YES | `20260616_061912_039860` | Fresh full sweep |
| kastors-jacobi/poisson-for | Correct=YES | `20260616_061925_332057` | SC-001 blocker remains closed |
| ml-kernels/activations | Correct=YES | `20260616_061938_711155` | Fresh full sweep |
| ml-kernels/batchnorm | Correct=YES | `20260616_061952_554892` | Fresh full sweep |
| ml-kernels/layernorm | Correct=YES | `20260616_062006_092970` | Fresh full sweep |
| ml-kernels/pooling | Correct=YES | `20260616_062019_443844` | Fresh full sweep |
| polybench/2mm | Correct=YES | `20260616_062033_392830` | Fresh full sweep |
| polybench/3mm | Correct=YES | `20260616_062055_713556` | Fresh full sweep |
| polybench/atax | Correct=YES | `20260616_062117_693628` | Fresh full sweep |
| polybench/bicg | Correct=YES | `20260616_062131_610472` | Fresh full sweep |
| polybench/convolution-2d | Correct=YES | `20260616_062145_109664` | Fresh full sweep |
| polybench/convolution-3d | Correct=YES | `20260616_062159_197056` | Fresh full sweep |
| polybench/correlation | Correct=YES | `20260616_062212_726466` | Fresh full sweep |
| polybench/gemm | Correct=YES | `20260616_062227_223430` | Fresh full sweep |
| polybench/jacobi2d | Correct=YES | `20260616_062245_106847` | Fresh full sweep |
| polybench/seidel-2d | Correct=YES | `20260616_062258_968887` | Fresh full sweep |
| seissol/volume-integral | Correct=YES | `20260616_062312_584388` | Fresh full sweep |
| specfem3d/stress | Correct=YES | `20260616_062325_832202` | Fresh full sweep |
| specfem3d/velocity | Correct=YES | `20260616_062339_269016` | Fresh full sweep |
| sw4lite/vel4sg-base | Correct=YES | `20260616_062352_626890` | Fresh full sweep |

No 2n, cluster, large, extralarge, or megalarge validation was run in this
scoped pass.

## Lit

`dekk carts lit`: **103/109** pass; the 6 remaining jacobi/poisson
stencil-halo boundary tests fail closed while compiling with `touches a DB
without a committed SDE access-window dependency`. These failures are
pre-existing per the findings index and do not contradict the fresh single-node
medium benchmark sweep.

New: `sde_redistribute_reconciles_alternating_buffer_grain.mlir`, `sde_redistribute_reconciles_alternating_buffer_root_grain.mlir`, `sde_redistribute_cross_owner_reductions.mlir` **PASS**.

## Verification

Commands run after preserving/stashing tracked WIP:

- `dekk carts build` - PASS.
- `dekk carts pipeline --json` - PASS.
- `dekk carts benchmarks list` - PASS, 21 registered benchmarks.
- `dekk carts lit` - 103/109, known pre-existing boundary failures above.
- 21 explicit `dekk carts benchmarks run <bench> --size medium --threads 64 --nodes 1 --launcher local --no-rdma --timeout 180 --trace` invocations - all PASS / Correct=YES.
