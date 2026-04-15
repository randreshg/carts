# Dep Pass Redesign: From Passive Carriers to Tensor-First Transforms

**Status**: PARTIALLY IMPLEMENTED
**Date**: 2026-04-15 (updated)
**Context**: Branch `architecture/sde-restructuring`, post-carrier-authoritative

---

## The Problem: The Carrier Paradox

The SDE pipeline creates transient `linalg.generic` carriers in RaiseToLinalg,
threads them through 12 passes, and erases them in LowerToMemref/ConvertSdeToArts.
But **none of the dep passes actually transform the carrier**. Every pass operates
on the raw `scf.for` + `memref.load/store` body while carrying the `linalg.generic`
as passive diagnostic cargo.

### What Each Dep Pass Actually Does

| Pass | Carrier Role | What It Actually Operates On |
|------|-------------|------------------------------|
| **LoopInterchange** | ✅ TRANSFORM carrier via `interchangeGenericOp()` | Carrier-authoritative: permutes carrier dims. Fallback: swaps `scf.for` loops |
| **Tiling** | ✅ TRANSFORM carrier via `extract_slice`/`insert_slice` | Carrier-authoritative: tiles carrier directly. Fallback: strip-mines body |
| **StructuredSummaries** | READ via `StructuredOpAnalysis` | Reads classification attr + re-analyzes (handles tensor ops in carriers) |
| **ElementwiseFusion** | ✅ READ carrier DPS outputs for write-set | Carrier-authoritative: traces `getDpsInits()` → `mu_memref_to_tensor` → memref root. Fallback: `memref.store` walk |
| **BarrierElimination** | ✅ READ carrier DPS inputs/outputs for read/write sets | Carrier-authoritative: traces `getDpsInputs()`/`getDpsInits()` → memref roots. Fallback: `memref.load/store` walk |
| **IterationSpaceDecomposition** | IGNORED (stencils have no carrier) | Pattern-matches `scf.for` + `scf.if` + `memref.load/store` counts |

**The carrier is now authoritative for elementwise/matmul/reduction.** After
RaiseToLinalg creates the carrier and erases the scalar body, all downstream
passes operate on the carrier (or its traced operands) directly. Stencils
remain on the scalar fallback path (no carrier yet).

### TensorOpt: The Most Awkward Case

`TensorOpt.cpp` (636 LOC) is the clearest example of the paradox:

```
What the name says:     "Apply cost-model-driven tensor optimizations"
What it actually does:   Strip-mine su_iterate step + clone memref scalar body

What uses the carrier:   isElementwiseTensorCandidate() — checks carrier iterator
                         types and indexing maps to VALIDATE that tiling is safe
What skips the carrier:  cloneScalarBodyIntoTileLoop() — explicitly filters out
                         carrier ops with isCarrierOp()
```

The pass reads the `linalg.generic` carrier to determine if tiling is safe (disjoint
write set, parallel vs reduction dims), then ignores the carrier entirely and manually
constructs a tiled `scf.for` loop around the raw memref body. This is ~500 LOC of
manual strip-mining that upstream MLIR's `scf::tileUsingSCF()` does in ~50 LOC.

---

## Current vs. Desired: The Dual-Representation Problem

### Current: Carrier as Diagnostic Copy

```
su_iterate {
  cu_region <parallel> {
    bufferization.to_tensor %A → %in          ┐
    linalg.generic ins(%in) outs(%out) { ... } ├── CARRIER (passive, diagnostic)
    tensor.empty → %out                        ┘

    memref.load %A[%i]                          ┐
    arith.mulf %v, %cst                        ├── BODY (active, authoritative)
    memref.store %r, %B[%i]                     ┘
  }
}
```

The carrier is a COPY of the body's computation structure. Dep passes transform
the body while the carrier watches. This means:
- LoopInterchange swaps `scf.for` loops but the carrier's indexing maps still
  reflect the pre-interchange ordering (stale)
- TensorOpt tiles the body but the carrier is erased (not tiled)
- StructuredSummaries re-analyzes the body directly (ignoring the carrier)

### Desired: Carrier as Authority

```
su_iterate {
  cu_region <parallel> {
    bufferization.to_tensor %A → %in
    %tiled = scf.for %ii ... {                     ┐
      %slice = tensor.extract_slice %in [%ii]...   │
      %out = tensor.empty [tile_size]               ├── CARRIER IS THE BODY
      linalg.generic ins(%slice) outs(%out) { ... } │
      tensor.insert_slice %result into %out_full    │
    }                                               ┘
    // NO separate memref body — carrier IS authoritative
  }
}
```

Dep passes transform the carrier using upstream MLIR APIs. LowerToMemref
(or one-shot bufferization) then derives the final memref body FROM the carrier.

---

## The Gap: Stencils Don't Raise

The carrier-first vision has one structural gap: **stencils don't have carriers**.

`RaiseToLinalg::supportsLinalgCarrier()` returns `false` for stencils because
stencil neighbor-offset access patterns (`A[i-1,j]`, `A[i+1,j]`, etc.) don't fit
`linalg.generic`'s pointwise iterator model. `linalg.generic` indexing maps are
affine functions of the iteration dimensions, but stencils need `A[i + offset, j]`
which requires explicit index computation.

Two paths to stencil carriers:

**Path A: tensor.pad + linalg.generic** (from `sde-optimization-opportunities.md` §4):
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

## Proposed Dep Pass Granularity

### Three Transform Tiers

```
── TIER 1: CARRIER TRANSFORMS (operate ON linalg.generic) ──────────────

  dep/tensor/
    Interchange.cpp       linalg::interchangeGenericOp() on carrier
    Tiling.cpp            scf::tileUsingSCF() on carrier via TilingInterface
    Fusion.cpp            linalg::fuseElementwiseOps() on carriers

  Each reads:  linalg.generic carrier (indexing maps, iterator types)
  Each writes: transformed linalg.generic (new maps, tiled structure)
  Fallback:    if no carrier, skip (body-level pass handles it)

── TIER 2: BODY TRANSFORMS (operate on scf.for + memref) ───────────────

  dep/loop/
    IterationSpaceDecomposition.cpp    boundary peeling (stencils, no carrier)

  Each reads:  scf.for structure, memref.load/store counts, SDE attrs
  Each writes: restructured scf.for nest
  When:        stencils and other patterns without carriers

── TIER 3: ANALYSIS (stamp contracts, no IR changes) ───────────────────

  state/
    StructuredSummaries.cpp    stamp neighborhood/footprint/vec attrs

  Reads:  StructuredOpAnalysis on post-optimization body OR carrier
  Writes: attributes on su_iterate
  When:   AFTER tiers 1+2 (analyzes optimized structure)
```

### Migration Path: Current → Proposed

**Phase 0 (current state, DONE)**:
All dep passes operate on raw body. Carrier is passive. Pipeline reorder complete.

**Phase 1: TensorOpt → Tiling via TilingInterface** (~200 LOC change):
Replace manual strip-mining in TensorOpt with upstream `scf::tileUsingSCF()`:
```cpp
// Before (TensorOpt.cpp, ~500 LOC of manual tiling):
Value tileIterations = buildTileIterationValue(...);
auto newOp = SdeSuIterateOp::create(..., tiledSteps);
// ... manual scf.for construction, body cloning, carrier skipping ...

// After (Tiling.cpp, ~50 LOC):
SCFTilingOptions options;
options.setTileSizes(costModel.computeTileSizes(carrier));
options.setLoopType(SCFTilingOptions::LoopType::ForOp);
auto result = scf::tileUsingSCF(rewriter, cast<TilingInterface>(carrier.getOperation()), options);
```

**Key prerequisite**: `linalg.generic` already implements `TilingInterface`.
No custom interface implementation needed. The carrier IS the tiling target.

**Key question**: After tiling the carrier, the raw memref body is stale. Two
options:
- **(a) Delete the body, make carrier authoritative**: Carrier becomes the source
  of truth. LowerToMemref must bufferize the carrier into memref form. This is
  the clean architecture but requires LowerToMemref to handle tiled carriers.
- **(b) Keep dual representation, re-derive body**: After tiling the carrier, use
  the tiled structure to drive body rewriting (what TensorOpt does manually today).
  Less clean but lower risk.

**Recommendation**: Start with (b) — same behavior as today but using upstream
tiling APIs. Migrate to (a) when LowerToMemref supports carrier bufferization.

**Phase 2: LoopInterchange → linalg::interchangeGenericOp** (~100 LOC change):
Replace manual loop swapping with upstream carrier interchange:
```cpp
// Before (LoopInterchange.cpp, ~400 LOC of manual scf.for swapping):
findInnerLoopPair(body, jLoop, kLoop, initOps);
interchangeLoops(jLoop, kLoop, initOps);

// After (~30 LOC):
SmallVector<unsigned> perm = computeOptimalPermutation(carrier);
auto result = linalg::interchangeGenericOp(rewriter, carrier, perm);
// Carrier indexing maps are updated automatically.
// Body interchange derived from carrier permutation.
```

**Phase 3: ElementwiseFusion → carrier-level write-set** ✅ DONE:
Write-set extraction now reads carrier DPS outputs instead of `memref.store`:
```cpp
// Before (ElementwiseFusion.cpp, ~300 LOC of memref.store pattern matching):
SmallVector<Value> writeSet1, writeSet2;
body1.walk([&](memref::StoreOp s) { writeSet1.push_back(s.getMemref()); });
// ... disjointness check on memref roots ...

// After (~100 LOC):
// Fuse at the linalg carrier level:
linalg::LinalgOp producer = findCarrier(stage1);
linalg::LinalgOp consumer = findCarrier(stage2);
auto result = linalg::fuseElementwiseOps(rewriter, producer, consumer);
```

**Phase 4: Stencil carriers** (~500 LOC, from `sde-optimization-opportunities.md`):
Raise stencils to `tensor.pad` + `linalg.generic` carriers. This unblocks
carrier-level tiling and interchange for stencils.

---

## What Upstream MLIR APIs Replace What Code

| Current Code | LOC | Upstream API | LOC | Prerequisite |
|-------------|-----|-------------|-----|-------------|
| TensorOpt manual strip-mining | ~500 | `scf::tileUsingSCF()` | ~50 | Carrier as tiling target |
| TensorOpt trip count + tile size | ~100 | Keep (cost model is SDE-specific) | ~100 | — |
| TensorOpt candidate validation | ~150 | `TilingInterface::getLoopIteratorTypes()` | ~20 | — |
| LoopInterchange stride analysis | ~60 | Keep (stride cost is SDE-specific) | ~60 | — |
| LoopInterchange loop swapping | ~200 | `linalg::interchangeGenericOp()` | ~30 | Carrier as target |
| LoopInterchange init distribution | ~100 | Upstream handles imperfect nests | ~0 | — |
| ElementwiseFusion write-set check | ~100 | `linalg::fuseElementwiseOps()` | ~50 | Both ops have carriers |
| ElementwiseFusion body merging | ~200 | Upstream fusion handles this | ~0 | — |
| IterationSpaceDecomp | ~350 | Keep (stencil-specific, no carrier) | ~350 | — |
| StructuredSummaries | ~200 | Keep (SDE-specific attr stamping) | ~200 | — |

**Total current dep code**: ~1960 LOC
**Total after migration**: ~860 LOC (56% reduction)
**Risk**: LOW for Phases 1-2 (upstream APIs are battle-tested)

---

## The Carrier Lifecycle: Current vs. Proposed

### Current Lifecycle (passive carrier)

```
Pass 3  RaiseToLinalg:    CREATE carrier (diagnostic copy of body)
Pass 4  LoopInterchange:  READ carrier maps → swap scf.for body
Pass 5  TensorOpt:        READ carrier dims → strip-mine body (skip carrier)
Pass 6  StructuredSummaries: IGNORE carrier → analyze body directly
Pass 7  ElementwiseFusion:   IGNORE carrier → fuse by memref patterns
Pass 15 LowerToMemref:    ERASE carrier (dead code cascade)
```

Carrier is born, carried, ignored, erased. ~12 passes of overhead for zero
transformation.

### Proposed Lifecycle (active carrier)

```
Pass 3  RaiseToLinalg:    CREATE carrier (authoritative computation)
                            ── body derived from carrier, or dual-representation
Pass 4  Interchange:       TRANSFORM carrier (interchangeGenericOp)
                            → carrier indexing maps updated
                            → body rewritten from carrier (or re-derived)
Pass 5  Tiling:            TRANSFORM carrier (tileUsingSCF)
                            → carrier tiled with extract/insert_slice
                            → body rewritten from carrier (or re-derived)
Pass 6  StructuredSummaries: ANALYZE carrier (post-optimization structure)
                            → stamp contracts from tiled/interchanged carrier
Pass 7  Fusion:            TRANSFORM carrier (fuseElementwiseOps)
                            → carriers merged
                            → body merged from carrier (or re-derived)
Pass 15 LowerToMemref:    LOWER carrier → derive final memref body
                            → one-shot bufferization on carrier structure
```

Carrier is born, transformed, analyzed, lowered. Every pass adds value.

---

## Directory Structure: Current vs. Proposed

### Current

```
dep/
  loop/
    LoopInterchange.cpp          564 LOC  (reads carrier, swaps scf.for)
    TensorOpt.cpp                636 LOC  (reads carrier, tiles body)
    IterationSpaceDecomposition  357 LOC  (raw scf.for, no carrier)
  fusion/
    ElementwiseFusion.cpp        ~300 LOC (raw memref, no carrier)
```

### Proposed

```
dep/
  tensor/                         ── carrier-level transforms (upstream APIs)
    Interchange.cpp               ~130 LOC (linalg::interchangeGenericOp)
    Tiling.cpp                    ~200 LOC (scf::tileUsingSCF + cost model)
    Fusion.cpp                    ~150 LOC (linalg::fuseElementwiseOps)
  loop/                           ── body-level transforms (stencils, fallback)
    IterationSpaceDecomposition   ~350 LOC (unchanged)
```

**StructuredSummaries stays in `state/`** — it's an analysis/contract pass, not a
structural transform.

---

## Interaction with Codelet Pipeline

The `sde-optimization-opportunities.md` proposes codelet body optimizations as
nested passes on `cu_codelet` (IsolatedFromAbove):

```
Pass 16  ConvertToCodelet:     create cu_codelet from cu_region <single>
         ── CODELET BODY OPTS (nested, IsolatedFromAbove) ──
         │  TensorCleanup       14 upstream patterns
         │  Canonicalize+CSE    standard cleanup
         │  Vectorization       linalg::vectorize()
         │  CodeletFusion       merge producer/consumer codelets
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

## Prioritized Action Items

### Immediate (next commit)

1. **Rename** `dep/loop/TensorOpt.cpp` → `dep/loop/Tiling.cpp` (or keep, but
   acknowledge the naming is misleading)
2. **Update doc**: Mark the carrier as "READ-only diagnostic" in the pipeline
   walkthrough until Phase 1 lands

### Short-term (Phase 1, 1-2 weeks)

3. **Replace TensorOpt tiling** with `scf::tileUsingSCF()` targeting the
   carrier. Keep cost model. Keep dual representation (re-derive body from
   tiled structure). ~200 LOC net change.
4. **Add StructuredSummaries carrier path**: Analyze `linalg.generic` carrier
   directly when available, fall back to StructuredOpAnalysis for body-only
   loops. ~50 LOC.

### Medium-term (Phase 2, 2-4 weeks)

5. **Replace LoopInterchange** with `linalg::interchangeGenericOp()` on carrier.
   Keep stride cost analysis. ~100 LOC net change.
6. **Replace ElementwiseFusion** with carrier-level fusion using
   `linalg::fuseElementwiseOps()`. ~200 LOC net change.

### Long-term (Phase 3-4, 1-2 months)

7. **Make carrier authoritative**: LowerToMemref derives body from carrier via
   one-shot bufferization. Delete dual-representation body. ~300 LOC.
8. **Stencil carriers**: Raise stencils to `tensor.pad` + `linalg.generic`.
   Unblocks carrier-level transforms for stencils. ~500 LOC.
9. **Codelet body optimizations**: TensorCleanup, Vectorization, CodeletFusion
   as nested passes on `cu_codelet`. ~600 LOC total.

---

## Summary

**The core insight**: The SDE dep pipeline has the right ARCHITECTURE (linalg
carriers for structured analysis) but the wrong IMPLEMENTATION (carriers are
passive, transforms operate on raw memref). Fixing this means making the carrier
the transform target, not the diagnostic copy, using upstream MLIR's battle-tested
linalg/tensor/SCF transform APIs.

**Net result**: ~1100 LOC deleted, upstream correctness guarantees inherited,
stencil support unlocked, codelet body optimization enabled.

**Risk**: LOW for Phases 1-2 (upstream APIs are stable, dual representation
provides fallback). MEDIUM for Phase 3 (carrier-authoritative requires
LowerToMemref changes). HIGH for Phase 4 (stencil carriers are novel).
