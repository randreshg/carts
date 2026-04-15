# SDE Pipeline Walkthrough

**Status**: CURRENT (matches `Compile.cpp` on branch `architecture/sde-restructuring`)
**Date**: 2026-04-15

This document traces MLIR IR through every pass of the SDE pipeline, using
concrete code snippets to show what each pass consumes, transforms, and produces.

---

## Pipeline Overview

```
 C/OpenMP Source
      |
      v
 [Polygeist]  ── C/C++ → MLIR (memref + omp + scf + arith)
      |
      v
 ┌─────────────── SDE Pipeline (21 passes) ───────────────┐
 │                                                         │
 │  ENTRY                                                  │
 │    1. ConvertOpenMPToSde     omp.* → sde.*              │
 │                                                         │
 │  RAISING (tensor-first: mem2reg before classification)  │
 │    2. RaiseToTensor          memref.alloca → tensor SSA  │
 │    3. RaiseToLinalg          classify + linalg carriers  │
 │                                                         │
 │  DEP (structural transforms on the optimized form)      │
 │    4. LoopInterchange        reorder dims for stride-1   │
 │    5. TensorOpt              cost-model tiling           │
 │    6. StructuredSummaries    stamp neighborhood attrs    │
 │    7. ElementwiseFusion      merge compatible SUs        │
 │                                                         │
 │  EFFECT (scheduling decisions on post-optimization IR)  │
 │    8. ScopeSelection         local vs distributed        │
 │    9. ScheduleRefinement     auto → static/dynamic/...   │
 │   10. ChunkOpt               synthesize chunk sizes      │
 │   11. ReductionStrategy      atomic/tree/local_accum     │
 │                                                         │
 │  DISTRIBUTION + DECOMPOSITION                           │
 │   12. DistributionPlanning   blocked/owner_compute wrap  │
 │   13. IterationSpaceDecomp   boundary peeling            │
 │   14. BarrierElimination     elim redundant barriers     │
 │                                                         │
 │  LOWERING (tensor → memref, codelet formation)          │
 │   15. LowerToMemref          tensor SSA → memref.alloca  │
 │   16. ConvertToCodelet       <single> iter_args → codelet│
 │   17. ConvertSdeToArts       sde.* → arts.*              │
 │   18. VerifySdeLowered       reject surviving sde ops    │
 │                                                         │
 │  CLEANUP                                                │
 │   19. DCE                    dead code elimination       │
 │   20. CSE                    common subexpr elimination  │
 │   21. VerifyEdtCreated       at least one arts.edt exists│
 │                                                         │
 └─────────────────────────────────────────────────────────┘
      |
      v
 ARTS Core Pipeline (stages 4-18)
```

---

## Running Examples

Three patterns traced through the pipeline, each highlighting different pass
behaviors:

| Pattern | Source | Key Passes |
|---------|--------|-----------|
| **Elementwise** | `B[i] = A[i] * 2.0` | RaiseToLinalg carrier, ElementwiseFusion, uniform contract |
| **Stencil** | `B[i,j] = 0.25*(A[i-1,j]+A[i+1,j]+A[i,j-1]+A[i,j+1])` | StructuredSummaries neighborhood, IterationSpaceDecomp peeling |
| **Matmul** | `C[i,j] += A[i,k]*B[k,j]` | LoopInterchange, ReductionStrategy, matmul contract |

---

## Pass 1: ConvertOpenMPToSde

Converts OpenMP dialect constructs into SDE dialect ops. Each OpenMP construct
maps to a specific SDE operation category (CU/SU/MU).

### Elementwise: `B[i] = A[i] * 2.0`

**Before** (OpenMP IR from Polygeist):
```mlir
func.func @scale(%A: memref<128xf64>, %B: memref<128xf64>) {
  %c0 = arith.constant 0 : index
  %c128 = arith.constant 128 : index
  %c1 = arith.constant 1 : index
  %cst = arith.constant 2.0 : f64
  omp.parallel {
    omp.wsloop {
      omp.loop_nest (%i) : index = (%c0) to (%c128) step (%c1) {
        %v = memref.load %A[%i] : memref<128xf64>
        %r = arith.mulf %v, %cst : f64
        memref.store %r, %B[%i] : memref<128xf64>
        omp.yield
      }
    }
    omp.terminator
  }
  return
}
```

**After**:
```mlir
func.func @scale(%A: memref<128xf64>, %B: memref<128xf64>) {
  %c0 = arith.constant 0 : index
  %c128 = arith.constant 128 : index
  %c1 = arith.constant 1 : index
  %cst = arith.constant 2.0 : f64
  arts_sde.cu_region <parallel> {
    arts_sde.su_iterate (%c0) to (%c128) step (%c1) {
      arts_sde.cu_region <parallel> {
        %v = memref.load %A[%i] : memref<128xf64>
        %r = arith.mulf %v, %cst : f64
        memref.store %r, %B[%i] : memref<128xf64>
        arts_sde.yield
      }
      arts_sde.yield
    }
  }
  return
}
```

**Mapping rules**:
- `omp.parallel` → `arts_sde.cu_region <parallel>`
- `omp.wsloop` + `omp.loop_nest` → `arts_sde.su_iterate` with bounds/step
- Loop body → nested `arts_sde.cu_region <parallel>` (uniform nesting)
- `omp.barrier` (implicit at wsloop end) → `arts_sde.su_barrier`
- Schedule/chunk from `omp.wsloop` → `schedule(<kind>, %chunk)` attr

### Stencil: 2D Jacobi (showing reduction inference)

**Before**:
```mlir
omp.parallel {
  omp.wsloop {
    omp.loop_nest (%i) : index = (%c1) to (%c63) step (%c1) {
      scf.for %j = %c1 to %c63 step %c1 {
        %n = memref.load %A[%i_m1, %j] : memref<64x64xf64>   // north
        %s = memref.load %A[%i_p1, %j] : memref<64x64xf64>   // south
        %w = memref.load %A[%i, %j_m1] : memref<64x64xf64>   // west
        %e = memref.load %A[%i, %j_p1] : memref<64x64xf64>   // east
        %sum = arith.addf %n, %s : f64
        %sum2 = arith.addf %sum, %w : f64
        %sum3 = arith.addf %sum2, %e : f64
        %avg = arith.mulf %sum3, %quarter : f64
        memref.store %avg, %B[%i, %j] : memref<64x64xf64>
      }
      omp.yield
    }
  }
  omp.terminator
}
```

**After**: Same structure — `omp.*` replaced by `arts_sde.*`, inner `scf.for`
preserved (only the outermost workshare loop becomes `su_iterate`).

### Matmul with reduction

**Before** (with `omp.wsloop reduction`):
```mlir
omp.wsloop reduction(@add_f64 %C_acc -> %prv : memref<f64>) {
  omp.loop_nest (%i) : index = (%c0) to (%N) step (%c1) {
    ...
    omp.yield
  }
}
```

**After** (reduction metadata preserved):
```mlir
arts_sde.su_iterate (%c0) to (%N) step (%c1)
    reduction[#arts_sde.reduction_kind<add>](%C_acc : memref<f64>) {
  ...
  arts_sde.yield
}
```

---

## Pass 2: RaiseToTensor

Converts function-level `memref.alloca` into tensor SSA values threaded through
SDE regions via `iter_args`. This is "mem2reg for SDE" — it enables downstream
passes to reason about values in SSA form rather than through memory side effects.

**Applies to**: Local allocas accessed inside `cu_region`/`su_iterate` bodies.
**Skips**: Function arguments (memref params stay as-is), non-local memory.

### Elementwise (no local state → no-op)

The elementwise example has no `memref.alloca`, so RaiseToTensor is a **no-op**.
IR is unchanged from Pass 1 output.

### Reduction accumulator (matmul partial sum)

**Before** (local accumulator for C[i,j]):
```mlir
func.func @matmul(%A: memref<32x32xf64>, %B: memref<32x32xf64>,
                   %C: memref<32x32xf64>) {
  %acc = memref.alloca() : memref<f64>        // local accumulator
  memref.store %c0_f64, %acc[] : memref<f64>  // init to 0.0
  arts_sde.cu_region <parallel> {
    arts_sde.su_iterate (%c0) to (%c32) step (%c1) {
      arts_sde.cu_region <parallel> {
        scf.for %k = %c0 to %c32 step %c1 {
          %a = memref.load %A[%i, %k] : memref<32x32xf64>
          %b = memref.load %B[%k, %j] : memref<32x32xf64>
          %old = memref.load %acc[] : memref<f64>
          %new = arith.addf %old, %mul : f64
          memref.store %new, %acc[] : memref<f64>
        }
        arts_sde.yield
      }
      arts_sde.yield
    }
  }
  return
}
```

**After** (accumulator raised to tensor SSA):
```mlir
func.func @matmul(%A: memref<32x32xf64>, %B: memref<32x32xf64>,
                   %C: memref<32x32xf64>) {
  %acc_init = arith.constant 0.0 : f64
  %acc_tensor = arts_sde.mu_alloc : tensor<f64>
  %acc_filled = tensor.insert %acc_init into %acc_tensor[] : tensor<f64>

  arts_sde.cu_region <parallel> {
    %result = arts_sde.su_iterate (%c0) to (%c32) step (%c1)
        iter_args(%acc_filled : tensor<f64>) -> tensor<f64> {
      arts_sde.cu_region <parallel> {
        scf.for %k = %c0 to %c32 step %c1 {
          %a = memref.load %A[%i, %k] : memref<32x32xf64>
          %b = memref.load %B[%k, %j] : memref<32x32xf64>
          %old = tensor.extract %iter_acc[] : tensor<f64>
          %mul = arith.mulf %a, %b : f64
          %new = arith.addf %old, %mul : f64
          %updated = tensor.insert %new into %iter_acc[] : tensor<f64>
        }
        arts_sde.yield
      }
      arts_sde.yield %updated : tensor<f64>
    }
  }
  return
}
```

**Key transformations**:
- `memref.alloca` → `arts_sde.mu_alloc` (SDE tensor allocation)
- `memref.store %init, %acc[]` → `tensor.insert %init into %tensor[]`
- `memref.load %acc[]` → `tensor.extract %tensor[]`
- `memref.store %new, %acc[]` → `tensor.insert %new into %tensor[]`
- Value threaded via `iter_args` through `su_iterate`

---

## Pass 3: RaiseToLinalg

Classifies loops via `StructuredOpAnalysis` and creates transient `linalg.generic`
carriers for patterns that support them. The carrier makes loop structure explicit
for downstream optimization passes.

**Classification** (stamped as `classification(<pattern>)` attribute):
- `elementwise`: all-parallel iterators, identity/permutation indexing maps
- `matmul`: (m,k)×(k,n)→(m,n) with reduction on k
- `stencil`: all-parallel + constant-offset neighbor reads
- `reduction`: has reduction iterators

**Carrier creation** (only for `elementwise`, `matmul`, narrow `reduction`):
- Wraps scalar body in `linalg.generic` with explicit indexing maps
- Input memrefs wrapped via `bufferization.to_tensor`
- Carrier is **transient** — erased by ConvertSdeToArts

### Elementwise: carrier created

**Before** (from Pass 2 — unchanged):
```mlir
arts_sde.su_iterate (%c0) to (%c128) step (%c1) {
  arts_sde.cu_region <parallel> {
    %v = memref.load %A[%i] : memref<128xf64>
    %r = arith.mulf %v, %cst : f64
    memref.store %r, %B[%i] : memref<128xf64>
    arts_sde.yield
  }
  arts_sde.yield
}
```

**After** (classified + carrier):
```mlir
arts_sde.su_iterate (%c0) to (%c128) step (%c1)
    classification(<elementwise>) {
  arts_sde.cu_region <parallel> {
    // --- transient linalg carrier (erased at Pass 17) ---
    %in = bufferization.to_tensor %A : memref<128xf64> to tensor<128xf64>
    %out = bufferization.to_tensor %B : memref<128xf64> to tensor<128xf64>
    %0 = linalg.generic {
        indexing_maps = [affine_map<(d0) -> (d0)>,
                         affine_map<(d0) -> (d0)>],
        iterator_types = ["parallel"]}
      ins(%in : tensor<128xf64>) outs(%out : tensor<128xf64>) {
    ^bb0(%a: f64, %b: f64):
      %r = arith.mulf %a, %cst : f64
      linalg.yield %r : f64
    } -> tensor<128xf64>
    // --- original memref body unchanged below carrier ---
    %v = memref.load %A[%i] : memref<128xf64>
    %r = arith.mulf %v, %cst : f64
    memref.store %r, %B[%i] : memref<128xf64>
    arts_sde.yield
  }
  arts_sde.yield
}
```

### Stencil: classified but NO carrier

Stencils have neighbor-offset access patterns that don't fit `linalg.generic`'s
pointwise model. `supportsLinalgCarrier()` returns `false` for stencils.

```mlir
arts_sde.su_iterate (%c1) to (%c63) step (%c1)
    classification(<stencil>) {
  // body unchanged — no linalg.generic created
  arts_sde.cu_region <parallel> {
    scf.for %j = %c1 to %c63 step %c1 {
      %n = memref.load %A[%i_m1, %j] : memref<64x64xf64>
      %s = memref.load %A[%i_p1, %j] : memref<64x64xf64>
      ...
    }
    arts_sde.yield
  }
  arts_sde.yield
}
```

### Matmul: classified + carrier

```mlir
arts_sde.su_iterate (%c0) to (%c32) step (%c1)
    classification(<matmul>) {
  ...
  %0 = linalg.generic {
      indexing_maps = [affine_map<(d0, d1, d2) -> (d0, d2)>,   // A[i,k]
                       affine_map<(d0, d1, d2) -> (d2, d1)>,   // B[k,j]
                       affine_map<(d0, d1, d2) -> (d0, d1)>],  // C[i,j]
      iterator_types = ["parallel", "parallel", "reduction"]}
    ins(%A_t, %B_t : ...) outs(%C_t : ...) {
  ^bb0(%a: f64, %b: f64, %c: f64):
    %mul = arith.mulf %a, %b : f64
    %add = arith.addf %c, %mul : f64
    linalg.yield %add : f64
  } -> tensor<32x32xf64>
  ...
}
```

The `linalg.generic` indexing maps encode the contraction pattern — LoopInterchange
and TensorOpt read these maps to determine optimal loop ordering and tiling.

---

## Pass 4: LoopInterchange

Reorders loop dimensions for stride-1 innermost access. Two strategies:

- **Carrier-based** (elementwise/matmul): Analyze `linalg.generic` indexing maps
  to compute stride cost per dimension. Dims with more stride-1 operand accesses
  become innermost.
- **Halo-based** (stencil): Reorder so smallest-halo-width dimension is outermost,
  minimizing halo volume when the outer dim is partitioned.

### Matmul: j-k interchange for stride-1 B access

**Before** (j outer, k inner — B[k,j] has stride-N access):
```mlir
arts_sde.su_iterate (%c0) to (%c32) step (%c1)
    classification(<matmul>) {
  scf.for %j = %c0 to %c32 step %c1 {
    scf.for %k = %c0 to %c32 step %c1 {
      %a = memref.load %A[%i, %k] : memref<32x32xf64>
      %b = memref.load %B[%k, %j] : memref<32x32xf64>  // stride-N!
      ...
    }
  }
}
```

**After** (k outer, j inner — B[k,j] has stride-1 access):
```mlir
arts_sde.su_iterate (%c0) to (%c32) step (%c1)
    classification(<matmul>) {
  scf.for %k = %c0 to %c32 step %c1 {      // was inner
    scf.for %j = %c0 to %c32 step %c1 {    // was outer → now stride-1
      %a = memref.load %A[%i, %k] : memref<32x32xf64>
      %b = memref.load %B[%k, %j] : memref<32x32xf64>  // stride-1
      ...
    }
  }
}
```

### Elementwise / Stencil

Usually a **no-op** for 1D elementwise (single loop, nothing to interchange).
For 2D stencils, may reorder based on halo widths if asymmetric.

---

## Pass 5: TensorOpt

Cost-model-driven tiling of `su_iterate` loops. Reads the transient
`linalg.generic` carrier (if present) to determine tile sizes, then strip-mines
the outer loop. The carrier is erased after tiling.

### Elementwise: strip-mine with tile size

**Before** (128-element loop):
```mlir
arts_sde.su_iterate (%c0) to (%c128) step (%c1)
    classification(<elementwise>) {
  ...
  linalg.generic { iterator_types = ["parallel"] } ...
  memref.load %A[%i] / memref.store %B[%i]
}
```

**After** (tiled by 16):
```mlir
arts_sde.su_iterate (%c0) to (%c128) step (%c16)
    classification(<elementwise>) {
  // tile loop (scf.for covers one tile)
  %ub = arith.minui (%i + %c16), %c128
  scf.for %ii = %i to %ub step %c1 {
    %v = memref.load %A[%ii] : memref<128xf64>
    %r = arith.mulf %v, %cst : f64
    memref.store %r, %B[%ii] : memref<128xf64>
  }
  // linalg.generic carrier erased (no longer needed)
  arts_sde.yield
}
```

**Cost formula**: `tile = max(ceil(tripCount / workerCount), minItersPerWorker)`

---

## Pass 6: StructuredSummaries

Stamps runtime-neutral contract attributes on `su_iterate` ops. These attributes
are consumed by `ConvertSdeToArts` (Pass 17) to generate ARTS contracts.

**No structural IR changes** — purely attribute stamping.

### Stencil: neighborhood attributes

```mlir
arts_sde.su_iterate (%c1) to (%c63) step (%c1)
    classification(<stencil>)
    // --- STAMPED BY StructuredSummaries ---
    accessMinOffsets = [-1, -1]
    accessMaxOffsets = [1, 1]
    ownerDims = [0, 1]
    spatialDims = [0, 1]
    writeFootprint = [1, 1]
    sde.dep_distances = [1, 1]
    sde.reuse_footprint_bytes = 2560
    {
  ...
}
```

### Elementwise: vectorization hints

```mlir
arts_sde.su_iterate (%c0) to (%c128) step (%c16)
    classification(<elementwise>)
    // --- STAMPED BY StructuredSummaries ---
    sde.vector_width = 4         // f64 → 256-bit SIMD
    sde.unroll_factor = 2
    arts.InPlaceSafe              // all writes also read → safe for RMW
    {
  ...
}
```

### Matmul: reduction + vectorization

```mlir
arts_sde.su_iterate (%c0) to (%c32) step (%c1)
    classification(<matmul>)
    ownerDims = [0, 1]
    spatialDims = [0, 1]
    sde.vector_width = 4
    sde.dep_distances = [0, 0]
    {
  ...
}
```

---

## Pass 7: ElementwiseFusion

Merges consecutive elementwise `su_iterate` ops with compatible iteration spaces
into a single `elementwise_pipeline` SU.

Write-set extraction is **carrier-aware**: in carrier-authoritative bodies (no
`memref.store` ops), the pass traces `linalg.generic` DPS outputs through
`mu_memref_to_tensor` → backing memref root. Falls back to `memref.store` walk
for dual-rep/stencil bodies.

### Two chained elementwise ops (carrier-authoritative)

**Before** (two separate SUs, each with a carrier):
```mlir
arts_sde.cu_region <parallel> {
  // Stage 1: B[i] = A[i] + 2.0
  arts_sde.su_iterate (%c0) to (%c128) step (%c16)
      classification(<elementwise>) {
    %a = arts_sde.mu_memref_to_tensor %A : memref<128xf32> -> tensor<128xf32>
    %b = arts_sde.mu_memref_to_tensor %B : memref<128xf32> -> tensor<128xf32>
    linalg.generic ins(%a) outs(%b) { arith.addf ... }
    arts_sde.yield
  }
  arts_sde.su_barrier                        // implicit barrier
  // Stage 2: C[i] = B[i] * 2.0
  arts_sde.su_iterate (%c0) to (%c128) step (%c16)
      classification(<elementwise>) {
    %b = arts_sde.mu_memref_to_tensor %B : memref<128xf32> -> tensor<128xf32>
    %c = arts_sde.mu_memref_to_tensor %C : memref<128xf32> -> tensor<128xf32>
    linalg.generic ins(%b) outs(%c) { arith.mulf ... }
    arts_sde.yield
  }
}
```

**After** (fused pipeline — both carriers in one body):
```mlir
arts_sde.cu_region <parallel> {
  arts_sde.su_iterate (%c0) to (%c128) step (%c16)
      classification(<elementwise_pipeline>) {
    arts_sde.cu_region <parallel> {
      // Stage 1 carrier
      %a = arts_sde.mu_memref_to_tensor %A ...
      %b = arts_sde.mu_memref_to_tensor %B ...
      linalg.generic ins(%a) outs(%b) { arith.addf ... }
      // Stage 2 carrier
      %b2 = arts_sde.mu_memref_to_tensor %B ...
      %c = arts_sde.mu_memref_to_tensor %C ...
      linalg.generic ins(%b2) outs(%c) { arith.mulf ... }
      arts_sde.yield
    }
    arts_sde.yield
  }
}
```

**Fusion constraints**: Same bounds/step, compatible schedule, disjoint write sets
(producer writes to B, consumer reads B but writes to C).

---

## Passes 8-11: Effect Passes (Scheduling Decisions)

These passes stamp scheduling-related attributes. They do NOT change IR structure
— only add/refine attributes on `su_iterate` and `cu_region`.

### Pass 8: ScopeSelection

Sets execution scope (local vs distributed) on `cu_region` ops.

```mlir
// Before
arts_sde.cu_region <parallel> { ... }

// After
arts_sde.cu_region <parallel> scope(<local>) { ... }
```

Uses cost model: estimates remote-access cost and overrides to `<local>` if
distributed execution isn't cost-justified (8x threshold).

### Pass 9: ScheduleRefinement

Refines abstract schedules (`auto`, `runtime`) to concrete kinds.

```mlir
// Before
arts_sde.su_iterate (%c0) to (%c128) step (%c16)
    schedule(<auto>) { ... }

// After
arts_sde.su_iterate (%c0) to (%c128) step (%c16)
    schedule(<static>) { ... }
```

Evaluates {static, guided, dynamic} against a cost model. For symbolic trip
counts, probes multiple candidate counts and only refines if one schedule wins
across all probes.

### Pass 10: ChunkOpt

Synthesizes chunk sizes for dynamic/guided schedules.

```mlir
// Before
arts_sde.su_iterate (%c0) to (%c128) step (%c16)
    schedule(<dynamic>) { ... }

// After
arts_sde.su_iterate (%c0) to (%c128) step (%c16)
    schedule(<dynamic>, %c8) { ... }
//                     ^^^^ synthesized chunk size
```

Formula: `chunk = clamp(max(minIters, tripCount/workerCount), 1, tripCount)`

### Pass 11: ReductionStrategy

Selects reduction synchronization strategy.

```mlir
// Before (matmul with add reduction)
arts_sde.su_iterate (...)
    reductionKinds = [#arts_sde.reduction_kind<add>] { ... }

// After
arts_sde.su_iterate (...)
    reductionKinds = [#arts_sde.reduction_kind<add>]
    reduction_strategy(<tree>) { ... }
```

Decision tree:
- Default: `tree`
- Nested sequential inner loop → `local_accumulate`
- Integral add + atomic cheaper than collective → `atomic`

---

## Pass 12: DistributionPlanning

Wraps eligible `su_iterate` in `su_distribute` container or stamps
`distributionKind` attribute based on classification and scope.

### Elementwise: blocked distribution

```mlir
// Before
arts_sde.su_iterate (%c0) to (%c128) step (%c16)
    classification(<elementwise>) scope(<local>) { ... }

// After
arts_sde.su_distribute <blocked> {
  arts_sde.su_iterate (%c0) to (%c128) step (%c16)
      classification(<elementwise>) scope(<local>) { ... }
}
```

### Stencil: owner_compute distribution

```mlir
arts_sde.su_distribute <owner_compute> {
  arts_sde.su_iterate (%c1) to (%c63) step (%c1)
      classification(<stencil>)
      accessMinOffsets = [-1, -1]
      accessMaxOffsets = [1, 1] { ... }
}
```

The `owner_compute` distribution is stencil-aware — it uses halo offsets to
determine ghost-cell exchange regions.

---

## Pass 13: IterationSpaceDecomposition

Decomposes stencil loops with boundary conditions into separate boundary and
branch-free interior loops. This enables vectorization of the interior.

### Stencil: boundary peeling

**Before** (branch in inner loop):
```mlir
scf.for %i = %c0 to %c64 step %c1 {
  scf.for %j = %c0 to %c64 step %c1 {
    %is_boundary = <compare i,j against bounds>
    scf.if %is_boundary {
      // boundary: clamp or zero-fill
      memref.store %zero, %B[%i, %j]
    } else {
      // interior: stencil compute (4+ loads)
      %n = memref.load %A[%i-1, %j]
      %s = memref.load %A[%i+1, %j]
      ...
      memref.store %avg, %B[%i, %j]
    }
  }
}
```

**After** (peeled):
```mlir
scf.for %i = %c0 to %c64 step %c1 {
  %is_boundary_row = <compare i against 0, 63>
  scf.if %is_boundary_row {
    // entire row is boundary
    scf.for %j = %c0 to %c64 step %c1 {
      memref.store %zero, %B[%i, %j]
    }
  } else {
    // left boundary column
    scf.for %j = %c0 to %c1 step %c1 {
      memref.store %zero, %B[%i, %j]
    }
    // INTERIOR: branch-free, vectorizable
    scf.for %j = %c1 to %c63 step %c1 {
      %n = memref.load %A[%i-1, %j]
      %s = memref.load %A[%i+1, %j]
      %w = memref.load %A[%i, %j-1]
      %e = memref.load %A[%i, %j+1]
      ...
      memref.store %avg, %B[%i, %j]
    }
    // right boundary column
    scf.for %j = %c63 to %c64 step %c1 {
      memref.store %zero, %B[%i, %j]
    }
  }
}
```

---

## Pass 14: BarrierElimination

Eliminates redundant `su_barrier` ops between independent scheduling units and
infers `nowait` on safe loops.

Read/write set collection is **carrier-aware**: in carrier-authoritative bodies,
the pass traces `linalg.generic` DPS inputs (read set) and outputs (write set)
through `mu_memref_to_tensor` → backing memref root. Falls back to
`memref.load/store` walk for dual-rep/stencil bodies.

### Disjoint write sets → barrier eliminated

**Before** (carrier-authoritative — write info in `getDpsInits()`):
```mlir
arts_sde.su_iterate (...) {                              // writes B
  %b = arts_sde.mu_memref_to_tensor %B ...
  linalg.generic outs(%b) { ... }
}
arts_sde.su_barrier
arts_sde.su_iterate (...) {                              // reads C (not B)
  %c = arts_sde.mu_memref_to_tensor %C ...
  linalg.generic ins(%c) { ... }
}
```

**After**:
```mlir
arts_sde.su_iterate (...) { ... }
arts_sde.su_barrier { barrier_eliminated }     // marked, ConvertSdeToArts skips
arts_sde.su_iterate (...) { ... }
```

### Reduction nowait inference

If a reduction loop only touches its accumulators and the successor doesn't read
them, `nowait` is inferred:

```mlir
// Before
arts_sde.su_iterate (...)
    reductionKinds = [add]
    reduction_strategy(<atomic>) { ... }

// After
arts_sde.su_iterate (...)
    reductionKinds = [add]
    reduction_strategy(<atomic>)
    nowait { ... }
```

---

## Pass 15: LowerToMemref

Inverse of Pass 2 (RaiseToTensor). Converts tensor SSA values back to
`memref.alloca` + `memref.load`/`memref.store` form. This is the exit from the
"tensor-first" optimization window.

**Three phases**:
1. **Rewrite**: `tensor.extract` → `memref.load`, `tensor.insert` → `memref.store`
2. **Strip iter_args**: Remove tensor `iter_args` from `su_iterate`/`cu_region`
3. **Erase dead carriers**: Remove orphaned `linalg.generic`, `bufferization.to_tensor`, `tensor.empty`, `arts_sde.mu_alloc`

### Matmul accumulator lowered back to memref

**Before** (tensor SSA from Pass 2):
```mlir
%acc_tensor = arts_sde.mu_alloc : tensor<f64>
%result = arts_sde.su_iterate (%c0) to (%c32) step (%c1)
    iter_args(%acc_tensor : tensor<f64>) -> tensor<f64> {
  %old = tensor.extract %iter_acc[] : tensor<f64>
  ...
  %updated = tensor.insert %new into %iter_acc[] : tensor<f64>
  arts_sde.yield %updated : tensor<f64>
}
```

**After** (back to memref):
```mlir
%acc = memref.alloca() : memref<f64>
memref.store %c0_f64, %acc[] : memref<f64>
arts_sde.su_iterate (%c0) to (%c32) step (%c1) {
  %old = memref.load %acc[] : memref<f64>
  ...
  memref.store %new, %acc[] : memref<f64>
  arts_sde.yield
}
// iter_args stripped, mu_alloc erased
```

### Carrier erasure

Any `linalg.generic` carriers that survived from Pass 3 are erased here as dead
code (they have no users after the tensor→memref rewrite).

---

## Pass 16: ConvertToCodelet

Converts `cu_region <single>` with tensor `iter_args` into the canonical codelet
form: `mu_data` + `mu_token` + `cu_codelet`. This is the bridge between
SDE's tensor-first model and ARTS's dataflow EDT model.

### Single-writer codelet

**Before** (cu_region <single> with tensor state):
```mlir
arts_sde.cu_region <single>
    iter_args(%shared : tensor<8xi32>) -> tensor<8xi32> {
  %val = tensor.extract %shared[%c0] : tensor<8xi32>
  %new = arith.addi %val, %c1 : i32
  %updated = tensor.insert %new into %shared[%c0] : tensor<8xi32>
  arts_sde.yield %updated : tensor<8xi32>
}
```

**After** (codelet form):
```mlir
%data = arts_sde.mu_data shared : tensor<8xi32>
%token = arts_sde.mu_token <readwrite> %data
    : tensor<8xi32> -> !arts_sde.token<tensor<8xi32>>

arts_sde.cu_codelet (%token : !arts_sde.token<tensor<8xi32>>)
    -> tensor<8xi32> {
^bb0(%slice : tensor<8xi32>):
  %val = tensor.extract %slice[%c0] : tensor<8xi32>
  %new = arith.addi %val, %c1 : i32
  %updated = tensor.insert %new into %slice[%c0] : tensor<8xi32>
  arts_sde.yield %updated : tensor<8xi32>
}

arts_sde.cu_region <single> {  // rebuilt without iter_args
  arts_sde.yield
}
```

**Key**: The codelet is `IsolatedFromAbove` — all data access goes through
tokens, making it safe for asynchronous execution in the ARTS runtime.

---

## Pass 17: ConvertSdeToArts

The main SDE→ARTS bridge. Converts all `arts_sde.*` operations to `arts.*`
operations and erases transient carriers.

### Operation mapping

| SDE Op | ARTS Op |
|--------|---------|
| `cu_region <parallel>` | `arts.edt <parallel, intranode>` |
| `cu_region <single>` | `arts.edt <single, intranode>` |
| `cu_task` | `arts.edt <task>` + `arts.db_control` |
| `su_iterate` | `arts.for` with contract attributes |
| `su_barrier` | `arts.barrier` (skipped if `barrier_eliminated`) |
| `su_distribute` | inlined (body promoted) |
| `cu_codelet` | `arts.edt <task>` + `arts.db_acquire` |
| `mu_data` | `arts.db_alloc` |
| `mu_token` | `arts.db_acquire` |
| `sde.yield` | `arts.yield` |

### Contract materialization (three routes)

**Route 1 — SDE attributes** (primary): Reads `structuredClassification` +
neighborhood attrs stamped by StructuredSummaries.

```
classification(<stencil>)  → depPattern = stencil_tiling_nd
classification(<matmul>)   → depPattern = matmul
classification(<elementwise>) → depPattern = uniform
classification(<elementwise_pipeline>) → depPattern = elementwise_pipeline
```

**Route 2 — Linalg fallback**: If no classification but `linalg.generic` carrier
exists, derives pattern from iterator types + indexing maps.

**Route 3 — No contract**: Falls through to ARTS PatternDiscovery (stage 5).

### Elementwise: full conversion

**Before** (SDE IR after all passes):
```mlir
arts_sde.cu_region <parallel> scope(<local>) {
  arts_sde.su_distribute <blocked> {
    arts_sde.su_iterate (%c0) to (%c128) step (%c16)
        classification(<elementwise>)
        schedule(<static>)
        sde.vector_width = 4 {
      %ub = arith.minui (%i + %c16), %c128
      scf.for %ii = %i to %ub step %c1 {
        %v = memref.load %A[%ii] : memref<128xf64>
        %r = arith.mulf %v, %cst : f64
        memref.store %r, %B[%ii] : memref<128xf64>
      }
      // linalg.generic carrier already erased at Pass 15
      arts_sde.yield
    }
  }
}
```

**After** (ARTS IR):
```mlir
arts.edt <parallel> <intranode> {
  arts.for (%c0) to (%c128) step (%c16) {
    %ub = arith.minui (%i + %c16), %c128
    scf.for %ii = %i to %ub step %c1 {
      %v = memref.load %A[%ii] : memref<128xf64>
      %r = arith.mulf %v, %cst : f64
      memref.store %r, %B[%ii] : memref<128xf64>
    }
  } {
    depPattern = #arts.dep_pattern<uniform>,
    distribution_pattern = #arts.distribution_pattern<uniform>,
    schedule = "static",
    vectorize_width = 4 : i64
  }
}
// No arts_sde.* ops remain
```

### Stencil: full conversion with contract

**After** (ARTS IR):
```mlir
arts.edt <parallel> <intranode> {
  arts.for (%c1) to (%c63) step (%c1) {
    scf.for %j = %c1 to %c63 step %c1 {
      %n = memref.load %A[%i_m1, %j] : memref<64x64xf64>
      %s = memref.load %A[%i_p1, %j] : memref<64x64xf64>
      %w = memref.load %A[%i, %j_m1] : memref<64x64xf64>
      %e = memref.load %A[%i, %j_p1] : memref<64x64xf64>
      ...
      memref.store %avg, %B[%i, %j] : memref<64x64xf64>
    }
  } {
    depPattern = #arts.dep_pattern<stencil_tiling_nd>,
    distribution_pattern = #arts.distribution_pattern<stencil>,
    stencil_min_offsets = [-1, -1],
    stencil_max_offsets = [1, 1],
    stencil_owner_dims = [0, 1],
    stencil_spatial_dims = [0, 1],
    stencil_write_footprint = [1, 1],
    stencil_supported_block_halo
  }
}
```

---

## Pass 18: VerifySdeLowered

Verification pass — walks the module and rejects any surviving SDE ops or
transient carriers.

**Rejects**:
- Any op in the `arts_sde` dialect → `"SDE operation survived past conversion"`
- `linalg.generic` nested under `arts.for` → `"transient carrier survived"`
- `bufferization.to_tensor` under `arts.for` → `"transient carrier survived"`
- `tensor.empty` under `arts.for` → `"transient carrier survived"`

If any violations found → `signalPassFailure()`.

---

## Passes 19-21: Cleanup

Standard MLIR passes that clean up after SDE→ARTS conversion:

- **Pass 19 (DCE)**: Removes dead operations left over from carrier erasure
- **Pass 20 (CSE)**: Eliminates common subexpressions across EDT bodies
- **Pass 21 (VerifyEdtCreated)**: Verifies at least one `arts.edt` was produced

---

## Attribute Flow Summary

This table shows which pass **stamps** each attribute and which pass **consumes** it:

| Attribute | Stamped By | Consumed By | On Op |
|-----------|-----------|-------------|-------|
| `classification` | RaiseToLinalg (3) | ConvertSdeToArts (17) | `su_iterate` |
| `accessMinOffsets` | StructuredSummaries (6) | ConvertSdeToArts (17) | `su_iterate` |
| `accessMaxOffsets` | StructuredSummaries (6) | ConvertSdeToArts (17) | `su_iterate` |
| `ownerDims` | StructuredSummaries (6) | ConvertSdeToArts (17) | `su_iterate` |
| `spatialDims` | StructuredSummaries (6) | ConvertSdeToArts (17) | `su_iterate` |
| `writeFootprint` | StructuredSummaries (6) | ConvertSdeToArts (17) | `su_iterate` |
| `sde.dep_distances` | StructuredSummaries (6) | DistributionPlanning (12) | `su_iterate` |
| `sde.vector_width` | StructuredSummaries (6) | ConvertSdeToArts (17) | `su_iterate` |
| `sde.unroll_factor` | StructuredSummaries (6) | ConvertSdeToArts (17) | `su_iterate` |
| `scope` | ScopeSelection (8) | DistributionPlanning (12) | `cu_region` |
| `schedule` | ConvertOpenMPToSde (1) / ScheduleRefinement (9) | ChunkOpt (10), ConvertSdeToArts (17) | `su_iterate` |
| `chunkSize` | ChunkOpt (10) | ConvertSdeToArts (17) | `su_iterate` |
| `reductionKinds` | ConvertOpenMPToSde (1) | ReductionStrategy (11), ConvertSdeToArts (17) | `su_iterate` |
| `reduction_strategy` | ReductionStrategy (11) | DistributionPlanning (12), ConvertSdeToArts (17) | `su_iterate` |
| `barrier_eliminated` | BarrierElimination (14) | ConvertSdeToArts (17) | `su_barrier` |
| `nowait` | ConvertOpenMPToSde (1) / BarrierElimination (14) | ConvertSdeToArts (17) | `su_iterate` |

---

## The Tensor-First Sandwich

The SDE pipeline uses a **raise-optimize-lower** pattern for local state:

```
   memref world                tensor world               memref world
  ─────────────  RaiseToTensor  ─────────────  LowerToMemref  ──────────
  memref.alloca   ──────────>   tensor SSA     ──────────>   memref.alloca
  memref.load     ──────────>   tensor.extract ──────────>   memref.load
  memref.store    ──────────>   tensor.insert  ──────────>   memref.store
                                iter_args threading
                  Pass 2                                  Pass 15
                          ↕ Passes 3-14 operate on ↕
                          ↕    tensor SSA form     ↕
```

**Why**: Tensor SSA enables SSA-based analysis and optimization (no aliasing,
pure value semantics) while memref form is required by ARTS core ops downstream.
The linalg carriers created by RaiseToLinalg exist entirely within this tensor
window and are erased when lowering back to memref.
