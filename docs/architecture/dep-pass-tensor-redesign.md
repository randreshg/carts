# Dep Pass Architecture: From Passive Carriers to Carrier-Authoritative

**Status**: IMPLEMENTED (Steps 9-12b), PROPOSED remainder (Steps 13-14)
**Date**: 2026-04-15 (updated)
**Context**: Branch `architecture/sde-restructuring`, post-carrier-authoritative

---

## The Problem: The Carrier Paradox

The SDE pipeline creates transient `linalg.generic` carriers in RaiseToLinalg,
threads them through 12 passes, and erases them in LowerToMemref/ConvertSdeToArts.
But **none of the dep passes actually transform the carrier**. Every pass operates
on the raw `scf.for` + `memref.load/store` body while carrying the `linalg.generic`
as passive diagnostic cargo.

> **Resolution (2026-04-15)**: The carrier paradox is resolved. The `linalg.generic`
> carrier is now **authoritative** for elementwise, matmul, and reduction patterns.
> RaiseToLinalg creates the carrier and erases the scalar body. All dep/effect
> passes operate on the carrier or its traced operands. See the updated pass table
> below. Stencils remain on the scalar body fallback path (no carrier yet).

### What Each Dep Pass Actually Does

| Pass | Carrier Role | What It Actually Operates On |
|------|-------------|------------------------------|
| **LoopInterchange** | TRANSFORM carrier via `interchangeGenericOp()` | Carrier-authoritative: permutes carrier dims. Fallback: swaps `scf.for` loops |
| **Tiling** | TRANSFORM carrier via `extract_slice`/`insert_slice` | Carrier-authoritative: tiles carrier directly. Fallback: strip-mines body |
| **StructuredSummaries** | READ via `StructuredOpAnalysis` | Reads classification attr + re-analyzes (handles tensor ops in carriers) |
| **ElementwiseFusion** | READ carrier DPS outputs for write-set | Carrier-authoritative: traces `getDpsInits()` -> `mu_memref_to_tensor` -> memref root. Fallback: `memref.store` walk |
| **BarrierElimination** | READ carrier DPS inputs/outputs for read/write sets | Carrier-authoritative: traces `getDpsInputs()`/`getDpsInits()` -> memref roots. Fallback: `memref.load/store` walk |
| **IterationSpaceDecomposition** | IGNORED (stencils have no carrier) | Pattern-matches `scf.for` + `scf.if` + `memref.load/store` counts |

**The carrier is now authoritative for elementwise/matmul/reduction.** After
RaiseToLinalg creates the carrier and erases the scalar body, all downstream
passes operate on the carrier (or its traced operands) directly. Stencils
remain on the scalar fallback path (no carrier yet).

### TensorOpt: The Most Awkward Case (Historical)

This section documents the original motivation. `TensorOpt.cpp` has since been
renamed to `Tiling.cpp` and rewritten to use carrier-authoritative transforms.

The original `TensorOpt.cpp` (636 LOC) was the clearest example of the paradox:

```
What the name said:      "Apply cost-model-driven tensor optimizations"
What it actually did:     Strip-mine su_iterate step + clone memref scalar body

What used the carrier:    isElementwiseTensorCandidate() — checked carrier iterator
                          types and indexing maps to VALIDATE that tiling was safe
What skipped the carrier: cloneScalarBodyIntoTileLoop() — explicitly filtered out
                          carrier ops with isCarrierOp()
```

The pass read the `linalg.generic` carrier to determine if tiling was safe (disjoint
write set, parallel vs reduction dims), then ignored the carrier entirely and manually
constructed a tiled `scf.for` loop around the raw memref body. This was ~500 LOC of
manual strip-mining that upstream MLIR's `scf::tileUsingSCF()` does in ~50 LOC.

**What changed**: Tiling.cpp (~900 LOC) now has a carrier-authoritative path that
tiles the `linalg.generic` directly via `extract_slice` -> tiled generic ->
`insert_slice`. The dual-rep fallback path (scf.for + scalar clone) remains for
stencils and cost-model edge cases.

---

## Architecture Evolution

### Before (pre-2026-04-15): Dual Representation

Carrier and scalar body coexisted. The carrier was a passive diagnostic copy:

```
su_iterate {
  cu_region <parallel> {
    bufferization.to_tensor %A -> %in          |
    linalg.generic ins(%in) outs(%out) { ... } |-- CARRIER (passive, diagnostic)
    tensor.empty -> %out                        |

    memref.load %A[%i]                          |
    arith.mulf %v, %cst                         |-- BODY (active, authoritative)
    memref.store %r, %B[%i]                     |
  }
}
```

The carrier was a COPY of the body's computation structure. Dep passes transformed
the body while the carrier watched. This meant:
- LoopInterchange swapped `scf.for` loops but the carrier's indexing maps still
  reflected the pre-interchange ordering (stale)
- TensorOpt tiled the body but the carrier was erased (not tiled)
- StructuredSummaries re-analyzed the body directly (ignoring the carrier)

### After (2026-04-15): Carrier-Authoritative

The `linalg.generic` carrier IS the computation for elementwise, matmul, and
reduction patterns. The scalar body is erased after carrier creation:

```
su_iterate {
  cu_region <parallel> {
    sde.mu_memref_to_tensor %A -> %in
    %tiled = scf.for %ii ... {                     |
      %slice = tensor.extract_slice %in [%ii]...   |
      %out = tensor.empty [tile_size]               |-- CARRIER IS THE BODY
      linalg.generic ins(%slice) outs(%out) { ... } |
      tensor.insert_slice %result into %out_full    |
    }                                               |
    // NO separate memref body -- carrier IS authoritative
  }
}
```

Dep passes transform the carrier using upstream MLIR APIs. LowerToMemref then
derives the final memref body FROM the carrier via `traceToMemref()`.

---

## The Carrier Lifecycle (Current)

```
Pass 3   RaiseToLinalg:       CREATE carrier, ERASE scalar body
Pass 4   LoopInterchange:     TRANSFORM carrier via interchangeGenericOp()
Pass 5   Tiling:              TRANSFORM carrier via extract_slice/insert_slice
Pass 6   StructuredSummaries: READ classification attr
Pass 7   ElementwiseFusion:   READ carrier DPS outputs -> memref roots
Pass 14  BarrierElimination:  READ carrier DPS inputs/outputs -> memref roots
Pass 15  LowerToMemref:       LOWER carrier -> scf.for + memref.load/store
```

Carrier is born, transformed, analyzed, lowered. Every pass adds value.

---

## The Gap: Stencils Don't Raise

The carrier-first vision has one structural gap: **stencils don't have carriers**.

`RaiseToLinalg::supportsLinalgCarrier()` returns `false` for stencils because
stencil neighbor-offset access patterns (`A[i-1,j]`, `A[i+1,j]`, etc.) don't fit
`linalg.generic`'s pointwise iterator model. `linalg.generic` indexing maps are
affine functions of the iteration dimensions, but stencils need `A[i + offset, j]`
which requires explicit index computation.

Two paths to stencil carriers:

**Path A: tensor.pad + linalg.generic** (from `sde-optimization-opportunities.md` S4):
```mlir
// Pad input with halo region
%padded = tensor.pad %A low[1, 1] high[1, 1] { yield %zero }
// Use linalg.generic with convolution-like indexing maps
%result = linalg.generic
    ins(%padded : tensor<66x66xf64>)
    outs(%B : tensor<64x64xf64>)
    indexing_maps = [affine_map<(i,j) -> (i+1, j+1)>,  // center
                     affine_map<(i,j) -> (i, j)>]       // output
```

**Path B: named linalg ops** (stencil-specific):
```mlir
// Future: linalg.stencil_2d or linalg.conv_2d with constant kernel
%result = linalg.conv_2d
    ins(%A, %kernel : ...) outs(%B : ...)
```

Path A is more immediate (uses existing `tensor.pad` + standard `linalg.generic`).
Path B would require upstream MLIR changes or custom SDE ops.

**Near-term reality**: Stencils remain on the body-level path. The dep pipeline
must handle both carrier-based and body-based transforms.

---

## Dep Pass Granularity

### Three Transform Tiers

```
-- TIER 1: CARRIER TRANSFORMS (operate ON linalg.generic) ---------------

  dep/tensor/
    Interchange.cpp       linalg::interchangeGenericOp() on carrier     607 LOC
    Tiling.cpp            carrier-authoritative tiling via               902 LOC
                          extract_slice/insert_slice + cost model

  dep/fusion/
    ElementwiseFusion.cpp carrier DPS output tracing for write-set      297 LOC

  Each reads:  linalg.generic carrier (indexing maps, iterator types)
  Each writes: transformed linalg.generic (new maps, tiled structure)
  Fallback:    if no carrier, dual-rep scf.for path (stencils)

-- TIER 2: BODY TRANSFORMS (operate on scf.for + memref) ----------------

  dep/loop/
    IterationSpaceDecomposition.cpp    boundary peeling (stencils)       357 LOC

  Each reads:  scf.for structure, memref.load/store counts, SDE attrs
  Each writes: restructured scf.for nest
  When:        stencils and other patterns without carriers

-- TIER 3: ANALYSIS (stamp contracts, no IR changes) ---------------------

  state/
    StructuredSummaries.cpp    stamp neighborhood/footprint/vec attrs    262 LOC

  Reads:  StructuredOpAnalysis on post-optimization body OR carrier
  Writes: attributes on su_iterate
  When:   AFTER tiers 1+2 (analyzes optimized structure)
```

### Phase Status

**Phase 1: Tiling -> carrier-authoritative**: DONE

Replaced manual strip-mining with carrier-authoritative path:
`extract_slice` -> tiled `linalg.generic` -> `insert_slice`. Cost model retained.
Dual-rep fallback retained for stencils.

**Phase 2: LoopInterchange -> interchangeGenericOp()**: DONE

Carrier-authoritative path uses `linalg::interchangeGenericOp()` to permute
carrier dims directly. Matmul: swaps j-k to k-j for stride-1 B access.
Dual-rep fallback retained for stencils.

**Phase 3: ElementwiseFusion -> carrier-level**: DONE

Write-set extraction reads carrier DPS outputs (`getDpsInits()`) and traces
through `mu_memref_to_tensor` to memref roots. Dual-rep fallback retained.

**Phase (bonus): BarrierElimination -> carrier-level**: DONE (not in original proposal)

Read/write set extraction traces carrier DPS inputs/outputs to memref roots.
Located at `effect/distribution/BarrierElimination.cpp` (367 LOC).

**Phase 4: Full upstream API migration**: DEFERRED

Replace remaining custom tiling/interchange code with upstream
`scf::tileUsingSCF()` and `linalg::interchangeGenericOp()` exclusively
(removing dual-rep fallback paths). Blocked on stencil carrier support.

---

## What Upstream MLIR APIs Replace What Code

| Current Code | LOC | Upstream API | LOC | Status |
|-------------|-----|-------------|-----|--------|
| Tiling carrier-authoritative path | ~400 | `extract_slice`/`insert_slice` carrier tiling | ~400 | DONE (kept cost model + dual-rep) |
| Tiling dual-rep fallback | ~500 | Keep (stencil fallback) | ~500 | Retained |
| Interchange carrier path | ~200 | `linalg::interchangeGenericOp()` | ~200 | DONE |
| Interchange dual-rep fallback | ~400 | Keep (stencil fallback) | ~400 | Retained |
| ElementwiseFusion write-set | ~100 | Carrier DPS output tracing | ~100 | DONE |
| BarrierElimination read/write sets | ~130 | Carrier DPS input/output tracing | ~130 | DONE |
| IterationSpaceDecomp | ~350 | Keep (stencil-specific, no carrier) | ~350 | Unchanged |
| StructuredSummaries | ~260 | Keep (SDE-specific attr stamping) | ~260 | Unchanged |

**Total current dep code**: ~2163 LOC (tensor/ + loop/ + fusion/)
**Plus analysis/effect code**: ~629 LOC (StructuredSummaries + BarrierElimination)

The carrier-authoritative migration did not achieve the aggressive LOC reduction
originally projected because dual-rep fallback paths were retained for stencils.
The net LOC reduction will come when stencil carriers are implemented (Phase 4),
allowing removal of all fallback paths.

---

## Interaction with Codelet Pipeline

The `sde-optimization-opportunities.md` proposes codelet body optimizations as
nested passes on `cu_codelet` (IsolatedFromAbove):

```
Pass 16  ConvertToCodelet:     create cu_codelet from cu_region <single>
         -- CODELET BODY OPTS (nested, IsolatedFromAbove) --
         |  TensorCleanup       14 upstream patterns
         |  Canonicalize+CSE    standard cleanup
         |  Vectorization       linalg::vectorize()
         |  CodeletFusion       merge producer/consumer codelets
```

**Key distinction**:
- **Dep transforms** (this doc): Operate BEFORE codelet formation, at the
  `su_iterate` level. Transform the carrier to optimize the overall loop
  structure (tiling, interchange, fusion).
- **Codelet body opts**: Operate AFTER codelet formation, inside the codelet
  sandbox. Apply micro-optimizations (vectorization, cleanup) that benefit from
  the IsolatedFromAbove guarantee.

The dep transforms set up the right loop structure; codelet opts polish the
inner body. They compose, not compete.

---

## Directory Structure (Current)

```
dep/
  tensor/                         -- carrier-level transforms
    Interchange.cpp               607 LOC (carrier-authoritative + dual-rep fallback)
    Tiling.cpp                    902 LOC (carrier-authoritative + dual-rep fallback)
  fusion/
    ElementwiseFusion.cpp         297 LOC (carrier DPS tracing + memref fallback)
  loop/                           -- body-level transforms (stencils)
    IterationSpaceDecomposition   357 LOC (raw scf.for, no carrier)

state/
  StructuredSummaries.cpp         262 LOC (analysis, reads classification attr)
  raising/
    RaiseToLinalg.cpp             385 LOC (CREATE carrier, ERASE scalar body)
    LowerToMemref.cpp             920 LOC (LOWER carrier -> memref via traceToMemref)

effect/distribution/
  BarrierElimination.cpp          367 LOC (carrier DPS tracing for read/write sets)
```

**StructuredSummaries stays in `state/`** -- it's an analysis/contract pass, not a
structural transform.

---

## Prioritized Action Items

### Done

1. **Rename** `dep/loop/TensorOpt.cpp` -> `dep/tensor/Tiling.cpp`: DONE (moved to tensor/ tier)
2. **Carrier-authoritative tiling**: DONE (Phase 1)
3. **Carrier-authoritative interchange**: DONE (Phase 2)
4. **Carrier-level write-set in ElementwiseFusion**: DONE (Phase 3)
5. **Carrier-level read/write-set in BarrierElimination**: DONE (bonus)
6. **RaiseToLinalg erases scalar body**: DONE (carrier becomes authoritative)
7. **LowerToMemref traceToMemref()**: DONE (derives memref from carrier)

### Proposed (FUTURE)

8. **Stencil carriers**: Raise stencils to `tensor.pad` + `linalg.generic`.
   Unblocks carrier-level transforms for stencils. Removes dual-rep fallback.
   ~500 LOC. (HIGH risk -- stencil carriers are novel)
9. **Full upstream API migration**: Replace custom tiling/interchange with
   `scf::tileUsingSCF()` and `linalg::interchangeGenericOp()` exclusively.
   Requires stencil carriers first. ~300 LOC net reduction.
10. **Codelet body optimizations**: TensorCleanup, Vectorization, CodeletFusion
    as nested passes on `cu_codelet`. ~600 LOC total.

---

## Summary

**The core insight**: The SDE dep pipeline had the right ARCHITECTURE (linalg
carriers for structured analysis) but the wrong IMPLEMENTATION (carriers were
passive, transforms operated on raw memref).

**What was done (2026-04-15)**: The carrier is now the transform target, not the
diagnostic copy. RaiseToLinalg creates the carrier and erases the scalar body.
Tiling, LoopInterchange, ElementwiseFusion, and BarrierElimination all operate
on the carrier (or its traced operands). LowerToMemref derives the final memref
body FROM the carrier.

**What remains**: Stencils still lack carriers, so dual-rep fallback paths are
retained. Full LOC reduction (~50%) will come when stencil carriers are
implemented, allowing removal of all fallback code. Codelet body optimizations
(vectorization, fusion) are proposed but not yet implemented.

**Risk**: LOW for current state (carrier-authoritative is stable, dual-rep
provides fallback). MEDIUM for upstream API migration (requires stencil carriers).
HIGH for stencil carriers (novel design, `tensor.pad` + `linalg.generic` approach
needs validation).
