# GEMM through the revised SDE pipeline — IR evolution

How the IR for **GEMM** (`C = A·B`, 4800×4800 f32) changes as it flows through the
**revised** SDE pipeline ([`design-revision.md`](./design-revision.md)). Read
top-to-bottom: each step shows the pass, one line of what it did to *this* kernel,
and the IR after it. `[REAL]` = verbatim from the staged dumps; `[PROJECTED]` =
the revised shape grounded in those real shapes. Companion: the stencil trace in
[`pass-walkthrough-jacobi.md`](./pass-walkthrough-jacobi.md).

GEMM's spine: one dense 3-nest matmul with an inner k-reduction. Watch the k-axis
stay serial while the i/j output space becomes a 2-D owner-tiled distributed shape,
and watch the contraction operand B turn into a `su.reduce_scatter`.

---

## 0. Input (after entry normalization) — structured loops over memref

```mlir
func.func @gemm(...) {
  %A = memref.alloc() : memref<4800x4800xf32>   // + B (%B), C (%C), all init by prologue loops
  scf.for %i = %c0 to %c4800 step %c1 {
    scf.for %j = %c0 to %c4800 step %c1 {
      %acc0 = memref.alloca() : memref<f32>      // scalar accumulator
      scf.for %k = %c0 to %c4800 step %c1 {
        %a = memref.load %A[%i,%k] ; %b = memref.load %B[%k,%j]
        %p = arith.mulf %a,%b ; %s = ... addf into %acc0 ...
      }
      memref.store %acc, %C[%i,%j] : memref<4800x4800xf32>
    } } }
```
No `omp` required — `raise-to-sde` reads this directly.

> **Order:** for this OMP-annotated kernel `convert-openmp-to-sde` runs *first*
> (converts the matmul omp region), then `raise-to-sde` raises the *missing* non-omp
> init nests. raise-to-sde is described first only as the new CORE.

## 1. `raise-to-sde` **[NEW]** (folds parallelize + cu-normalization)
*Raises the i/j output nest to a 2-D `su_iterate` with a `cu_region<parallel>`
leaf; the k-loop is proven reduction-carrying and kept as the inner `scf.for`; the
prologue init nests likewise become `su_iterate`+`<parallel>`; host
timers/checksum wrap into `cu_region<single>`.*

**[PROJECTED]** (real `su_iterate` shape; the revision's change is the **`<parallel>`**
CU kind on what `Parallelize` emits as `<single>` today, and that it comes from
plain `scf`):
```mlir
%h:4 = sde.cu_region <single> -> (i64, memref<...>, memref<...>, memref<...>) {  // host setup/timers
  ... sde.yield %timer, %A, %B, %C
}
sde.su_iterate (%c0, %c0) to (%c4800, %c4800) step (%c1, %c1) {
  sde.cu_region <parallel> {                       // proof-derived parallel
    %acc = memref.alloca() : memref<f32>
    scf.for %k = %c0 to %c4800 step %c1 { ... }     // k carries the reduction -> serial inner
    memref.store %r, %C[%i,%j]
  }
}
sde.su_barrier
```

## 2. `convert-openmp-to-sde` (FRONTEND) **[REVISED]**
*Decorates the raised SU with OMP facts when present — here `schedule(<static>)`.*
**[REAL]**
```mlir
sde.su_iterate (%c0) to (%c4800) step (%c1) schedule(<static>) { sde.cu_region <parallel> { ... } }
sde.su_barrier
```

## 3. ~~`sde-loop-pattern-facts`~~ → **ANALYSIS (no longer a pass)**
*`SuLoopAccessAnalysis` recomputes the classification + reduction dims on demand;
nothing is stamped. The `[REAL]` IR below shows what the analysis **reports** for
GEMM (today an attribute; in the revision a query result).* **[REAL]**
```mlir
// analysis(su) = { classification: matmul, partialReductionDims: [2],
//                  partialReductionOwnerDims: [0,1], inPlaceSafe: true }
sde.su_iterate (%c0,%c0) to (%c4800,%c4800) step (%c1,%c1) schedule(<static>) { ... }
```

## 4. `sde-layout-assignment` **[REVISED]**
*Picks per-array BLOCK layouts minimizing comm; A `block_parallel`, B
`block_contraction` (k-axis unsplittable), C 2-D-owned; flags B disagreeing.
Revised: also commits B's consumer read layout as B's target type so the later
movement is a real source≠target.* **[REAL]**
```mlir
sde.array_layout_root read  %A : memref<4800x4800xf32> array_id(0)
sde.array_layout_root read  %B : memref<4800x4800xf32> array_id(1)
sde.array_layout_root write %C : memref<4800x4800xf32> array_id(2)
} {arrayLayout = [
     {arrayId=0, kind="block_parallel",     ownerDims=[0],   blockShape=[2400,4800], budgetBlockShape=[110,4800], role="read"},
     {arrayId=1, kind="block_contraction",  ownerDims=[0],   blockShape=[2400,4800], commVolumeBytes=46080000,    role="read"},
     {arrayId=2, kind="block_parallel",     ownerDims=[0,1], blockShape=[2400,2400], budgetBlockShape=[437,1200], role="write"}],
   commVolumeBytes = 46080000, layoutsDisagree = [1], partialReductionDims = [2], pattern = #sde.pattern<matmul>}
```

## 5. `loop-interchange`
*Scalarizes the k-accumulator into `iter_args` (loop-carried matmul form).* **[REAL]**
```mlir
%r = scf.for %k = %c0 to %c4800 step %c1 iter_args(%acc = %cst) -> (f32) {
  %a = memref.load %A[%i,%k] ; %b = memref.load %B[%k,%j]
  %t = arith.mulf %a,%b ; %n = arith.addf %acc,%t ; scf.yield %n
}
```

## 6. `tiling` **[REVISED]**
*Strip-mines the i/j output to `owner_tile_2d [437,600]`; k stays untiled. Revised:
reconciles the READER grain too (no coarse reads).* **[REAL]**
```mlir
sde.su_iterate (%c0,%c0) to (%c4800,%c4800) step (%c437, %c600) schedule(<static>) classification(<matmul>) {
  scf.for %ii = %i to %ihi step %c1 { scf.for %jj = %j to %jhi step %c1 {
    %r = scf.for %k ... iter_args ... } } }
} {iterationTopology = #sde.iteration_topology<owner_tile_2d>, physicalBlockShape = [437,600], physicalOwnerDims = [0,1]}
```

## 7–9. `elementwise-fusion` (no-op) · `distribution-planning` **[REVISED]** · `iteration-space-decomposition` (no-op)
*DistributionPlanning wraps every SU `<blocked>`; the `min_distributed_tile_bytes`
knob is gone so the block count is not collapsed.* **[REAL]**
```mlir
sde.su_distribute <blocked> {
  sde.su_iterate (%c0,%c0) to (%c4800,%c4800) step (%c437,%c600) ... classification(<matmul>) { ... }
}
```

## 10. `barrier-elimination`
*Keeps the single cross-tile reduction barrier (no timestep structure for matmul).*
**[REAL]** `sde.su_barrier {barrierReason = #sde.barrier_reason<unknown_required>}`

## 11. `sde-memory-unit-realization`
*Shared arrays become first-class `sde.mu_alloc`.* **[REAL]**
```mlir
%A = sde.mu_alloc {arrayId = 0} : memref<4800x4800xf32>
%B = sde.mu_alloc {arrayId = 1} : memref<4800x4800xf32>
%C = sde.mu_alloc {arrayId = 2} : memref<4800x4800xf32>
```

## 12–13. `atomic-reduction-realization` (no-op) · `tree-reduction-realization` **[NEW]** (no-op)
*Both no-op for GEMM: the k-reduction is an in-CU `iter_args` accumulator (loop-
carried inside the parallel tile), not an exposed array-level reduction, so there
is nothing to realize as `sde.cu_atomic` or as partial-buffer tree. (These fire on
reduction kernels like stream/layernorm, not matmul.)*

## 14. `sde-rank-expand-mu`
*The block grid becomes the memref TYPE; loads/stores get div/mod localization.
Heterogeneous per array (A row-strip, B contraction-strip, C 2-D output tile).*
**[REAL]**
```mlir
%A = sde.mu_alloc {arrayId = 0} : memref<64x75x4800xf32>
%B = sde.mu_alloc {arrayId = 1} : memref<8x600x4800xf32>
%C = sde.mu_alloc {arrayId = 2} : memref<11x8x437x600xf32>
...
%d = arith.divui %k, %c75 ; %m = arith.remui %k, %c75
%a = memref.load %A[%d, %m, %kk] : memref<64x75x4800xf32>
%c = memref.store %r, %C[%bi,%bj,%ti,%tj] : memref<11x8x437x600xf32>
```
*(Revised: the `physicalOwnerDims`/`physicalBlockShape` attrs are now redundant with
this type and slated for deletion once ARTS reads the type.)*

## 15. `sde-raise-to-mu-access-window` **[REVISED]**
*One window per (MU, CU, mode). GEMM is dense → 1-D `owner_dims(1)` windows, no
halo (no `su.halo`).* **[REAL]**
```mlir
sde.cu_region <parallel> {
  sde.mu_access_window read  %A : memref<64x75x4800xf32> array_id(0) owner_dims(1) block_lo [0] block_hi [64] valid [75,4800]
  sde.mu_access_window read  %B : memref<8x600x4800xf32> array_id(1) owner_dims(1) block_lo [0] block_hi [8]  valid [600,4800]
  sde.mu_access_window write %C : memref<11x8x437x600xf32> array_id(2) owner_dims(2) block_lo [0,0] block_hi [11,8] valid [437,600]
```

## 16–17. `mu-access-window-sync-opt` (no-op) · `sde-redistribute` **[REVISED]**
*The contraction read crosses owner blocks with no neighbor overhang → a reduction
movement. Revised: emit the first-class **`sde.su_reduce_scatter`** op instead of
`sde.redist <reduce_scatter_like>`.*

today **[REAL]** → revised **[PROJECTED]**:
```mlir
// [REAL]   sde.redist <reduce_scatter_like> %B : memref<8x600x4800xf32> array_id(1) from owner [0] block [1,600,4800] to owner [0] block [1,600,4800] cost 46080000
   sde.su_reduce_scatter %B {arrayId = 1, reduceDim = 2, reductionKind = add}     // [PROJECTED]
       : memref<8x600x4800xf32> -> memref<8x600x4800xf32>
```
> At **2 nodes / chained matmul** (2mm/3mm), the intermediate's consumer layout
> differs from its producer layout, so this pass instead emits
> **`sde.su_all_to_all %B : memref<P> -> memref<C>`** (distinct types) — the
> genuine repartition that closes the self-edge. ARTS realizes it reader-pull; see
> [`arts-all-to-all.md`](./arts-all-to-all.md).

## 18. Verification — **op-level (no passes)**
*The 8 `verify-sde-*` passes are gone; the matmul SU's well-formedness, window
coverage, and the `su.reduce_scatter` geometry are checked in op verifiers. Only a
1-arm `verify-sde` + `verify-sde-lowered` remain.* See
[`op-level-verification.md`](./op-level-verification.md).

## 19. Final SDE shape → boundary
```mlir
sde.su_distribute <blocked> {
  sde.su_reduce_scatter %B {arrayId=1, reduceDim=2, reductionKind=add} : memref<8x600x4800xf32> -> memref<8x600x4800xf32>
  sde.su_iterate (%c0,%c0) to (%c4800,%c4800) step (%c437,%c600) schedule(<static>) classification(<matmul>) {
    sde.array_layout_root read %A array_id(0) ; sde.array_layout_root write %C array_id(2)
    sde.cu_region <parallel> {
      sde.mu_access_window read %A ... ; ... scf.for %k ... iter_args ... ; memref.store ... %C ...
    }
  }
}
```
`SdeStorageToArtsDb → SdeAccessesToArtsDeps → FinalizeSdeToArts` realize A/B/C as
ARTS DBs (B via the reduce-scatter realizer), the matmul `cu_region<parallel>` as
owner-tiled EDTs over the 11×8 output grid.

---

### GEMM in one line per stage
raise → 2-D parallel matmul SU (k serial inner) · classify `<matmul>` ·
layout A=parallel/B=contraction/C=2D · interchange k→iter_args · tile
`owner_tile_2d[437,600]` · distribute `<blocked>` · MU alloc · rank-expand to
`[11x8x437x600]` (+div/mod) · 1-D access windows · **B → `su.reduce_scatter`**
(`su.all_to_all` when chained at 2n) · op-level verify · boundary → owner-tiled EDTs.
