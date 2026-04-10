# Metadata Pipeline Removal: Architecture Analysis

## The Problem: Dual Execution

The metadata pipeline (6,477 LOC, 30 files) is a **caching layer** that
pre-computes information at stage 2 that consumer passes recompute or could
recompute at stages 5-11. It exists because of sequential compilation — we
analyze early and cache for later — not because the information is
fundamentally unavailable at the point of use.

```
CURRENT (dual execution):

  Stage 2: CollectMetadata
           ├─ LoopAnalyzer: trip count, deps, parallelism
           ├─ MemrefAnalyzer: access patterns, footprint
           ├─ DependenceAnalyzer: inter-iteration deps
           └─ stamps 35 attributes on IR
                    ↓ (metadata survives as attributes)
  Stage 5: PatternDiscovery
           ├─ collectLocalStencilEvidence()  ← REDOES the same analysis
           ├─ reads metadata as confidence hint
           └─ stamps pattern contracts (depPattern, etc.)
                    ↓ (pattern contracts survive)
  Stages 7-18: read PATTERN CONTRACTS, NOT metadata
```

The information flows through a chain of indirection:

```
source IR → CollectMetadata → 35 attrs → PatternDiscovery → pattern contracts → consumers
             (duplicated)      (cache)    (partially redundant)  (what matters)   (read this)
```

Nobody after stage 5 reads the original metadata. They read pattern contracts.
The metadata is an intermediate cache that adds 6,477 LOC of infrastructure,
a staleness tracking system (VerifyMetadataIntegrity), and provenance markers
— all to solve a problem that doesn't need to exist.

## Evidence: Each Consumer Can Compute Locally

### PatternDiscovery (stage 5) — already self-sufficient

`PatternDiscovery.cpp:157-231` — `collectLocalStencilEvidence()` walks the
loop body, finds memory accesses, extracts constant offsets from induction
variables, counts stencil dimensions. This is the **same analysis** that
CollectMetadata does, just localized.

The metadata is only an AND gate (line 266):
```cpp
if (isStencilMemref(*meta) && localStencil.isStencil)
    ++evidence.stencilMemrefs;
```

If PatternDiscovery trusted its own local analysis alone (which is MORE
precise because it runs on the actual ARTS IR), the metadata hint is
unnecessary. The local evidence already computed:
- `isStencil` — offsets from IV analysis
- `isMultiDimStencil` — multi-dimension offset counting
- `explicitContract` — full StencilNDPatternContract with offset bounds

**Change needed**: Remove the metadata AND gate. Trust local evidence.
~20 lines changed.

### LoopReordering (stage 5) — has auto-detection fallback

`LoopReordering.cpp:83-88`:
```cpp
if (!metadataApplied) {
    if (tryAutoDetectAndReorder(artsFor, manager)) {
        reorderCount++;
    }
}
```

The auto-detection path uses `detectMatmulInitReductionLoopNest()` — a
direct IR pattern matcher in DbPatternMatchers. The metadata
`reorderNestTo` is a cached version of what auto-detection computes.

**Change needed**: Remove metadata path, use auto-detection only.
~15 lines removed.

### ForLowering (stage 11) — one trivial decision

`ForLowering.cpp:1797` reads `memrefsWithLoopCarriedDeps` — a count of
memrefs with loop-carried deps. This determines whether a loop with deps
can be parallelized as "reduction-only."

**Change needed**: Add a local 10-line helper:
```cpp
static unsigned countMemrefsWithLoopCarriedDeps(ForOp forOp) {
    unsigned count = 0;
    SmallPtrSet<Operation *, 8> seen;
    forOp.getBody()->walk([&](Operation *op) {
        Value memref = DbUtils::getAccessedMemref(op);
        if (!memref) return;
        Operation *alloc = resolveAllocLikeSource(memref);
        if (alloc && seen.insert(alloc).second) {
            // Check if this memref has both reads and writes in the loop
            if (hasReadAndWriteInLoop(forOp, alloc))
                ++count;
        }
    });
    return count;
}
```

### DbPartitioning (stage 13) — reads ZERO metadata

Grep confirms: DbPartitioning.cpp has **no matches** for getLoopMetadata,
getMemrefMetadata, LoopMetadata, or MemrefMetadata. It reads `depPattern`
and `partitionMode` which are **pattern contract attributes** — these are
stamped by PatternDiscovery, not by CollectMetadata.

### CreateDbs (stage 7) — barely uses metadata

Includes `MemrefMetadata.h` but has no meaningful metadata consumption.
Likely a stale include.

### LoopAnalysis (analysis layer) — trip count queries

Provides trip count via metadata. But `ForOp` already has bounds —
`getUpperBound()`, `getLowerBound()`, `getStep()` — which can be
constant-folded on demand.

## Dead Metadata: 7 of 35 Attributes Never Consumed

These attributes are computed by CollectMetadata but read by **no pass**:

| Attribute | LOC to compute | Consumers |
|-----------|---------------|-----------|
| `reuse_distance` | ~50 | None |
| `temporal_locality` | ~20 | None |
| `has_good_spatial_locality` | ~15 | None |
| `first_use_id` | ~10 | None |
| `last_use_id` | ~10 | None |
| `nesting_depth` | ~5 | None |
| `accessed_in_parallel_loop` | ~5 | None |

~115 LOC computing data nobody reads.

## Metadata Lifecycle: Dies at Stage 5

```
Stage 1:  (no metadata)
Stage 2:  CollectMetadata creates 35 attrs  ← BIRTH
Stage 3:  (metadata exists, nobody reads)
Stage 4:  OMP→SDE→ARTS (metadata survives transformation)
Stage 5:  PatternDiscovery reads metadata, stamps contracts  ← CONSUMPTION
          LoopReordering reads metadata (with fallback)
Stage 6:  (metadata exists, nobody reads)
Stage 7:  CreateDbs — reads contracts, NOT metadata
Stage 8:  DbOpt — reads contracts
...
Stage 11: ForLowering — reads ONE metadata field
Stage 12-18: metadata DORMANT — contracts are all that matter
```

Metadata is consumed at exactly **2.5 stages** (5, 5, 11) out of 18. The
rest of the pipeline reads pattern contracts exclusively.

## The Tensor Path Makes This Explicit

In the planned tensor/linalg future (Phase 2C), metadata becomes literally
redundant because the information IS the IR structure:

| Metadata | Tensor/Linalg Equivalent |
|----------|------------------------|
| potentiallyParallel | `iterator_types = ["parallel"]` |
| hasInterIterationDeps | SSA def-use chains (no aliasing) |
| dominantAccessPattern | `indexing_maps` structure |
| hasStencilPattern | `indexing_maps` with `d0±const` |
| tripCount | Loop bounds (already on ForOp) |
| parallelClassification | `iterator_types` composition |
| reorderNestTo | Named ops (`linalg.matmul`) or detected |
| memrefsWithLoopCarriedDeps | SSA — impossible by construction |

But you don't need tensor to remove metadata — the information is already
available in the memref IR at each consumer's stage.

## Removal Plan

### Phase A: Remove dead metadata (immediate, safe)

Remove the 7 never-consumed attributes and their computation:
- `reuse_distance`, `temporal_locality`, `has_good_spatial_locality`
- `first_use_id`, `last_use_id`
- `nesting_depth`, `accessed_in_parallel_loop`

~115 LOC removed. Zero functional impact.

### Phase B: Inline consumers (1-2 days)

Make each consumer self-sufficient:

1. **PatternDiscovery**: Remove metadata AND gate. Trust local evidence from
   `collectLocalStencilEvidence()`. Change ~20 lines.

2. **LoopReordering**: Remove metadata path. Use auto-detection
   (`detectMatmulInitReductionLoopNest`) as sole path. Remove ~15 lines.

3. **ForLowering**: Add local `countMemrefsWithLoopCarriedDeps()` helper
   (~10 LOC). Remove metadata read.

4. **LoopAnalysis**: Trip count from ForOp bounds directly. Already
   available via `getConstantUpperBound()`.

### Phase C: Remove infrastructure (after Phase B verified)

Delete the metadata pipeline:

| Component | Files | LOC |
|-----------|-------|-----|
| CollectMetadata pass | 1 | 307 |
| MetadataManager + registry + attacher | 6 | 1,527 |
| LoopAnalyzer + MemrefAnalyzer | 4 | 1,034 |
| LoopMetadata + MemrefMetadata structs | 4 | 1,145 |
| IdRegistry + LocationMetadata + ValueMetadata | 6 | 847 |
| AccessStats | 2 | 72 |
| VerifyMetadata | 1 | 193 |
| VerifyMetadataIntegrity | 1 | 177 |
| DependenceAnalyzer header | 1 | 335 |
| MetadataIO | 2 | 333 |
| **Total** | **28** | **5,970** |

Keep:
- LocationMetadata (useful for debug output) — 226 LOC
- DependenceAnalyzer (thin wrapper around MLIR affine analysis) — 335 LOC
  (but inline into consumers, not a separate infrastructure)

### Phase D: Remove stage 2 pipeline slot

- Remove `CollectMetadata` from `buildCollectMetadataPipeline()` in Compile.cpp
- Remove `VerifyMetadata` and `VerifyMetadataIntegrity` from stage 2
- Remove `AnalysisKind::MetadataManager` from analysis framework
- Remove metadata-related attributes from Attributes.td (arts.loop, arts.memref,
  arts.id, arts.metadata_provenance)
- Update tests that check for metadata attributes

## Risk Assessment

| Risk | Mitigation |
|------|-----------|
| PatternDiscovery loses confidence filtering | Local evidence is MORE precise than metadata (runs on actual ARTS IR, not pre-OMP) |
| LoopReordering loses metadata path | Auto-detection covers same cases (tryAutoDetectAndReorder is fallback today) |
| ForLowering loses memref dep count | Trivial 10-line local computation |
| Benchmarks regress | Run full benchmark suite after Phase B before Phase C |
| Stencil offset bounds lost | PatternDiscovery already computes these locally via `matchExplicitStencilNDContract()` |

## Net Impact

- **-5,970 LOC** removed (28 files)
- **+40 LOC** added (local helpers in 3 consumer passes)
- **-2 pipeline stages** (VerifyMetadata, VerifyMetadataIntegrity)
- **-1 analysis kind** (MetadataManager from AnalysisManager)
- **Faster compilation** (eliminate redundant early analysis pass)
- **No staleness problem** (no cached attributes to go stale)
- **Simpler AnalysisManager** (one fewer analysis to invalidate/track)

## Relationship to Tensor Integration (Phase 2C)

Metadata removal is **independent** of tensor integration. It can proceed now.

When Phase 2C (tensor/linalg) arrives, the benefits compound:
- No metadata → no metadata-to-tensor migration problem
- linalg `iterator_types` → replaces PatternDiscovery's local analysis too
- SSA deps → replaces DependenceAnalyzer entirely
- Named ops → replaces DbPatternMatchers for known patterns

The metadata removal clears the path for tensor integration by eliminating
a layer that would otherwise need to be maintained in parallel with the
tensor-native analysis.
