# jacobi-for through the revised SDE pipeline — IR evolution

How the IR for **jacobi-for** (iterative 5-point stencil, 10240×10240 f64,
double-buffered over a timestep loop) changes as it flows through the **revised**
SDE pipeline ([`design-revision.md`](./design-revision.md)). Read top-to-bottom.
`[REAL]` = verbatim from the staged dumps; `[PROJECTED]` = the revised shape
grounded in those real shapes. Companion: the matmul trace in
[`pass-walkthrough-gemm.md`](./pass-walkthrough-gemm.md).

jacobi's spine: an **outer timestep `scf.for`** (a true serial recurrence) wrapping
two 2-D sweeps per step — a copy-back and the 5-point stencil. Watch the timestep
loop stay serial while the spatial sweep becomes a 2-D owner-tiled `owner_compute`
shape, and watch the ±1 neighbor read become a real **`sde.su_halo`** window
instead of dissolving into div/mod.

---

## 0. Input (after entry normalization) — structured loops over memref

```mlir
func.func @jacobi(...) {
  %A = memref.alloc() : memref<10240x10240xf64>   // + scratch buffers %Anew, %tmp
  scf.for %t = %c0 to %T step %c1 {                // <-- serial timestep recurrence
    scf.for %i = %c1 to %c10239 step %c1 {         // 5-point stencil sweep
      scf.for %j = %c1 to %c10239 step %c1 {
        %n = load %A[%i-1,%j] ; %s = load %A[%i+1,%j]    // ±1 neighbors
        %w = load %A[%i,%j-1] ; %e = load %A[%i,%j+1]
        %r = 0.25 * (%n+%s+%w+%e)
        memref.store %r, %Anew[%i,%j]
      } }
    // copy-back / buffer swap sweep ...
  } }
```
No `omp` required — `raise-to-sde` reads this directly.

> **Order:** for this OMP-annotated kernel `convert-openmp-to-sde` runs *first*
> (converts the stencil/copy omp sweeps), then `raise-to-sde` raises the *missing*
> non-omp init nest. raise-to-sde is described first only as the new CORE.

## 1. `raise-to-sde` **[NEW]** (folds parallelize + cu-normalization)
*Raises the two spatial sweeps to 2-D `su_iterate`+`cu_region<parallel>`; **proves
the outer timestep `scf.for` a serial recurrence and does NOT raise it**; the
per-step convergence-flag reads / bound-selects wrap into leaf `cu_region<single>`.*

**[PROJECTED]** (real SU shape; revision = `<parallel>` from plain `scf`, timestep
loop stays the enclosing serial loop):
```mlir
scf.for %t = %c0 to %T step %c1 {                  // serial — NOT raised
  %flag = sde.cu_region <single> -> (i1) { ... }   // per-step scalar control wrapped
  sde.su_iterate (%c0,%c0) to (%N,%N) step (%c1,%c1) {
    sde.cu_region <parallel> { ... 5-point stencil into %Anew ... }
  }
  sde.su_barrier
}
```

## 2. `convert-openmp-to-sde` (FRONTEND) **[REVISED]**
*Decorates with OMP facts when present (runtime trip bounds via `index_cast`, the
per-sweep barriers). Absent OMP, slots stay empty.* **[REAL]**
```mlir
sde.su_iterate (%c0) to (%18) step (%c1) { sde.cu_region <parallel> { ... } }
sde.su_barrier
```

## 3. ~~`sde-loop-pattern-facts`~~ → **ANALYSIS (no longer a pass)**
*`SuLoopAccessAnalysis` recomputes the `<stencil>` classification + halo offsets on
demand; nothing is stamped. The `[REAL]` IR shows what the analysis **reports**
(today attrs; in the revision a query). Note: once `tiling` rewrites the loop to
`outer*T+inner`, the offsets are read from `su.halo`/the type, not re-derived from
the loop (Part 5.7).* **[REAL]**
```mlir
// analysis(su) = { classification: stencil, accessMinOffsets: [-1,-1],
//                  accessMaxOffsets: [1,1], ownerDims: [0,1], writeFootprint: [1,1] }
sde.su_iterate (%c0,%c0) to (%11,%c10240) step (%c1,%c1) {
  ... %n = memref.load %A[%i, %jm1] ; %s = memref.load %A[%i, %jp1] ...
}
```

## 4. `sde-layout-assignment` **[REVISED]**
*One uniform `block_parallel` (owner[0,1], budget `[512,512]`, 400 blocks) on all
three arrays — the 2-D tiled space; comm = halo overlap, not contraction. Revised:
commits the consumer read layout as the target type (enables the movement op).*
**[REAL]**
```mlir
} {accessMaxOffsets=[1,1], accessMinOffsets=[-1,-1],
   arrayLayout = [
     {arrayId=0, kind="block_parallel", ownerDims=[0,1], budgetBlockShape=[512,512], role="read"},
     {arrayId=1, kind="block_parallel", ownerDims=[0,1], budgetBlockShape=[512,512], commVolumeBytes=419430400, role="read"},
     {arrayId=2, kind="block_parallel", ownerDims=[0,1], role="write"}],
   commVolumeBytes = 419430400, layoutsDisagree = [1], pattern = #sde.pattern<stencil_tiling_nd>}
```

## 5. `loop-interchange`
*No-op: a point stencil stores directly, no inner reduction accumulator, no halo-dim
reorder needed.* **[REAL]** (body unchanged: `memref.store %r, %Anew[%i,%j]`).

## 6. `tiling` **[REVISED]**
*Strip-mines to uniform `owner_tile [512,512]` and derives `physicalHaloShape=[1,1]`
from the offsets. Revised: reconciles the stencil READ grain to the fine budget
grain (kills the ~100× comm inflation from coarse reads).* **[REAL]**
```mlir
sde.su_iterate (%c0,%c0) to (%11,%c10240) step (%c512,%c512) classification(<stencil>) {
  scf.for %ii ... { scf.for %jj ... { ... } }
} {iterationTopology = #sde.iteration_topology<owner_tile>, physicalBlockShape = [512,512],
   physicalHaloShape = [1,1], physicalOwnerDims = [0,1], pattern = #sde.pattern<stencil_tiling_nd>}
```

## 7–9. `elementwise-fusion` (no-op) · `distribution-planning` **[REVISED]** · `iteration-space-decomposition` (no-op*)
*DistributionPlanning commits the stencil SU `<owner_compute>` (owner reads its tile
+ ±1 halo); knob removed so the 400-block grain is not collapsed.* **[REAL]**
```mlir
sde.su_distribute <owner_compute> {
  sde.su_iterate (%c0,%c0) to (%11,%c10240) step (%c512,%c512) classification(<stencil>) { ... }
}
```
*(\*ISD is a no-op despite jacobi having the interior/boundary guard — its predicate
is an `arith.select` fused row+col, not the `andi` chain the matcher needs. Repair
candidate in the revision.)*

## 10. `barrier-elimination`
*Keeps the per-timestep barrier and commits timestep-stage structure on the stencil
SUs (the iterative-stencil divergence from matmul).* **[REAL]**
```mlir
... asyncStrategy = #sde.async_strategy<advance_stage>,
    pattern = #sde.pattern<alternating_buffer_stencil>,
    repetitionStructure = #sde.repetition_structure<full_timestep> ...
sde.su_barrier {barrierReason = #sde.barrier_reason<unknown_required>}
```

## 11. `sde-memory-unit-realization`
*The three double-buffer arrays become `sde.mu_alloc` — classification-agnostic.*
**[REAL]**
```mlir
%A    = sde.mu_alloc {arrayId = 0} : memref<10240x10240xf64>
%Anew = sde.mu_alloc {arrayId = 1} : memref<10240x10240xf64>
%tmp  = sde.mu_alloc {arrayId = 2} : memref<10240x10240xf64>
```

## 12–13. `atomic-reduction-realization` (no-op) · `tree-reduction-realization` **[NEW]** (no-op)
*Both no-op: jacobi has no reduction at all.*

## 14. `sde-rank-expand-mu`
*The block grid becomes the memref TYPE — identical uniform `[20x20x512x512]` for
all three arrays; each neighbor access (center, i±1, j±1) gets its own div/mod
block, so today the halo index arithmetic balloons.* **[REAL]**
```mlir
%A    = sde.mu_alloc {arrayId = 0} : memref<20x20x512x512xf64>
%Anew = sde.mu_alloc {arrayId = 1} : memref<20x20x512x512xf64>
%tmp  = sde.mu_alloc {arrayId = 2} : memref<20x20x512x512xf64>
...
%d = arith.divui %jm1, %c512 ; %m = arith.remui %jm1, %c512
%n = memref.load %A[%bi,%d,%ti,%m] : memref<20x20x512x512xf64>   // one such block per neighbor
```

## 15. `sde-raise-to-mu-access-window` **[REVISED — the big stencil change]**
*Raises 2-D `owner_dims(2)` windows; **revised: emit `sde.su_halo` carrying real
`radiusLo`/`radiusHi`**, projected as an acquire slice (`validExtents` stays
`== blockExtent`, so DB grain is not inflated). This makes the ±1 halo a structured
window instead of the div/mod the window can't see.*

today **[REAL]** window + revised **[PROJECTED]** `su.halo`:
```mlir
sde.cu_region <parallel> {
  sde.mu_access_window read  %A    : memref<20x20x512x512xf64> array_id(0) owner_dims(2) block_lo [0,0] block_hi [20,20] valid [512,512]
  sde.mu_access_window write %Anew : memref<20x20x512x512xf64> array_id(1) owner_dims(2) block_lo [0,0] block_hi [20,20] valid [512,512]
  sde.su_halo %A {arrayId=0, ownerDims=[0,1], blockShape=[1,1,512,512], radiusLo=[1,1], radiusHi=[1,1]}   // [PROJECTED]
      : memref<20x20x512x512xf64> -> memref<20x20x512x512xf64>
```

## 16–17. `mu-access-window-sync-opt` (no-op) · `sde-redistribute` **[REVISED]**
*The neighbor read overhangs the owner block by one element → a halo movement.
Revised: the halo IS the `sde.su_halo` raised above; the `sde.redist <halo_like>`
op disappears.*

today **[REAL]** → revised:
```mlir
// [REAL]     sde.redist <halo_like> %A : memref<20x20x512x512xf64> array_id(0) from owner [0,1] block [1,1,512,512] to owner [0,1] block [1,1,512,512] halo [1,1,0,0] cost 419430400
//  [REVISED] no redist op — the halo is the sde.su_halo op (radiusLo/radiusHi) under su_distribute
```

## 18. Verification — **op-level (no passes)**
*The 8 `verify-sde-*` passes are gone. The per-timestep barrier justification (the
RAW/WAR across the double-buffer swap) moves into `SdeSuBarrierOp::verify` (bounded
sibling scan, reusing `classifyBarrierSync`); window coverage into the
`cu_region` verifier; `su.halo` geometry into its own op verifier.* See
[`op-level-verification.md`](./op-level-verification.md).

## 19. Final SDE shape → boundary
```mlir
scf.for %t = %c0 to %T step %c1 {                  // serial timestep recurrence preserved
  sde.su_distribute <owner_compute> {
    sde.su_halo %A {arrayId=0, ownerDims=[0,1], blockShape=[1,1,512,512], radiusLo=[1,1], radiusHi=[1,1]}
        : memref<20x20x512x512xf64> -> memref<20x20x512x512xf64>
    sde.su_iterate (%c0,%c0) to (%N,%c10240) step (%c512,%c512) classification(<stencil>) {
      sde.array_layout_root read %A array_id(0) ; sde.array_layout_root write %Anew array_id(1)
      sde.cu_region <parallel> {
        sde.mu_access_window read %A ... ; ... 5-point stencil ... ; memref.store ... %Anew ...
      }
    }
  }
  sde.su_barrier {barrierReason = #sde.barrier_reason<unknown_required>}
}
```
`SdeStorageToArtsDb → SdeAccessesToArtsDeps → FinalizeSdeToArts` realize the three
arrays as per-block single-writer ARTS DBs over the 20×20 grid, the stencil
`cu_region<parallel>` as owner-compute EDTs, and the `su.halo` as a per-block halo
exchange (the `HaloSliceAttr` ARTS already projects) — once per timestep epoch.

---

### jacobi in one line per stage
raise → 2-D parallel stencil SU **inside a serial timestep `scf.for`** · classify
`<stencil>` + halo offsets · layout one uniform `block_parallel[512,512]` ·
interchange no-op · tile `owner_tile[512,512]` + `physicalHaloShape=[1,1]` ·
distribute `<owner_compute>` · barrier-elim commits `full_timestep` stage ·
MU alloc · rank-expand to `[20x20x512x512]` (+per-neighbor div/mod) · 2-D access
windows · **±1 halo → `sde.su_halo`** · op-level verify · boundary → owner-compute
EDTs + per-block halo exchange per timestep.

---

### GEMM vs jacobi — the divergence, at a glance
| | GEMM | jacobi-for |
|---|---|---|
| raised shape | 2-D parallel matmul, k serial inner | 2-D parallel sweep **inside serial timestep loop** |
| classification | `<matmul>` + `partialReductionDims` | `<stencil>` + halo offsets |
| layout | A parallel / **B contraction** / C 2-D | one uniform `block_parallel` ×3 |
| distribution | `<blocked>` | `<owner_compute>` |
| rank-expand | heterogeneous `[11x8x437x600]` etc. | uniform `[20x20x512x512]` ×3 |
| movement op | **`sde.su_reduce_scatter`** (k) / `su.all_to_all` (chained 2n) | **`sde.su_halo`** (±1) |
| barriers | one reduction barrier | one per timestep (`full_timestep` stage) |
