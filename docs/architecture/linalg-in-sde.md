# Linalg-in-SDE: Replacing the Metadata Pipeline

## The Core Insight

The metadata pipeline exists because CARTS compiles sequentially: analyze early
(stage 2), cache as attributes, consume late (stages 5-11). This "dual
execution" pattern adds 6,477 LOC of infrastructure to solve a problem that
doesn't need to exist.

**linalg.generic is self-describing computation.** When loop bodies are raised
to linalg inside SDE ops, the information the metadata pipeline computes is
literally encoded in the IR structure:

| Metadata Field | linalg.generic Equivalent |
|---|---|
| `potentiallyParallel` | `iterator_types = ["parallel"]` |
| `hasInterIterationDeps` | SSA def-use chains (no aliasing in tensor space) |
| `dominantAccessPattern` | `indexing_maps` structure |
| `hasStencilPattern` | `indexing_maps` with `d0 + const` offsets |
| `tripCount` | Loop bounds (already on ForOp) |
| `parallelClassification` | `iterator_types` composition |
| `reorderNestTo` | Named ops (`linalg.matmul`) or `indexing_maps` analysis |
| `memrefsWithLoopCarriedDeps` | SSA — impossible by construction in tensor space |

No caching layer. No staleness. No dual execution. The IR IS the analysis.

## Three Orthogonal Layers

The fundamental architecture is three orthogonal layers composing inside SDE:

```
sde.cu_region / sde.cu_task  =  PARALLELISM  (what runs in parallel)
sde.su_iterate               =  SCHEDULING   (how iterations are distributed)
linalg.generic               =  COMPUTATION  (what the math is)
```

Each carries its own semantics. They compose naturally:

```mlir
// Jacobi stencil in SDE+linalg (after raise):
sde.cu_region parallel scope(<distributed>) {
  // su_iterate is CONSUMED — its iteration becomes linalg's domain
  linalg.generic {
    indexing_maps = [
      affine_map<(d0) -> (d0 - 1)>,  // A[i-1]  → stencil!
      affine_map<(d0) -> (d0 + 1)>,  // A[i+1]  → stencil!
      affine_map<(d0) -> (d0)>       // B[i]    → output
    ],
    iterator_types = ["parallel"]     // → parallel!
  } ins(%A, %A) outs(%B) { ... }
  sde.yield
}
```

The `sde.su_iterate` doesn't survive the raise for simple cases — its
iteration range becomes the `linalg.generic`'s domain. The scheduling policy
(`static`, `chunk`, `nowait`) moves to attributes on the `sde.cu_region`.

For sequential-outer / parallel-inner (time loops):

```mlir
sde.su_iterate (%c0) to (%T) step (%c1)           // SEQUENTIAL time
    iter_args(%A = %A_init, %B = %B_init) {        // alternating buffers = SSA swap!
  sde.cu_region parallel {                          // PARALLEL spatial
    linalg.generic { stencil indexing_maps... }     // COMPUTATION
  }
  sde.yield %B, %A                                  // swap = SSA forwarding
}
```

Three layers, three ops, three concerns. No metadata — the stencil pattern IS
the `indexing_maps`, the parallelism IS the `iterator_types`, the deps ARE the
SSA chains.

## Non-Raiseable Code: The Memref Fallback

The raise is best-effort inside SDE bodies. What raises gets linalg analysis
for free; what doesn't stays as memref with conservative handling:

```mlir
// No linalg, no loop — just an atomic
sde.cu_atomic add (%addr, %val)

// No computation at all
sde.su_barrier

// Indirect access — can't raise to linalg
sde.cu_task deps(...) {
  A[B[i]] = ...     // stays as memref.load/store
}
```

For non-raiseable code, the existing PatternDiscovery local analysis
(`collectLocalStencilEvidence()`) already works. The metadata pipeline adds
nothing here — PatternDiscovery's local analysis is MORE precise because it
runs on the actual ARTS IR, not the pre-OMP source IR.

Estimated raise rates from benchmark analysis:

| Category | Raiseable | Examples |
|---|---|---|
| Perfect loop nests | ~60% | jacobi, poisson, stencils, BLAS, convolutions |
| Indirect/sparse | ~0% | specfem3d, sparse matmul |
| I/O / function calls | ~0% | file read/write, MPI calls |
| Reductions | ~80% | dot products, norms (scalar iter_args) |
| Task with deps | ~40% | structured task bodies only |

## What SDE Op Bodies Look Like After OMP Conversion

Understanding the IR structure is critical for knowing where linalg raise
can and cannot work.

### sde.cu_region (from `omp.parallel`)

Body contains: `sde.su_iterate` ops (from `omp.wsloop`), memref.load/store,
arith ops, scf.for, function calls. Single-block region with `sde.yield`
terminator.

```mlir
sde.cu_region parallel scope(<distributed>) {
  sde.su_iterate (%c1) to (%Nm1) step (%c1) {
  ^bb0(%i: index):
    %a0 = memref.load %A[%im1] : memref<?xf64>
    %a1 = memref.load %A[%ip1] : memref<?xf64>
    %sum = arith.addf %a0, %a1 : f64
    memref.store %val, %B[%i] : memref<?xf64>
    sde.yield
  }
  sde.yield
}
```

**Raise target**: The inner `sde.su_iterate` body → `linalg.generic`. The
`sde.cu_region` stays as the parallelism shell.

### sde.cu_task (from `omp.task`)

Body contains: memref.load/store, arith ops, function calls. Deps come from
`sde.mu_dep` ops defined before the task. Single-block region.

```mlir
%dep_A = sde.mu_dep <read> %A[%i] size [%N] : memref<?xf64> -> !sde.dep
%dep_B = sde.mu_dep <write> %B[%i] size [%N] : memref<?xf64> -> !sde.dep
sde.cu_task deps(%dep_A, %dep_B) {
^bb0(%A_view: memref<?xf64>, %B_view: memref<?xf64>):
  // computation
  sde.yield
}
```

**Raise target**: Task body → `linalg.generic` when the body is a structured
loop nest. In tensor path, `tensor.extract_slice` replaces `sde.mu_dep`.

### sde.su_iterate (from `omp.wsloop` + `omp.loop_nest`)

Body contains: memref.load/store, arith ops, potentially nested scf.for.
Has IV as block arg. May have reduction accumulators and iter_args.

**Raise target**: The iterate body → `linalg.generic`. The scheduling policy
(schedule, chunk, nowait) moves to the parent `sde.cu_region`.

### sde.cu_atomic, sde.su_barrier, sde.mu_dep

No body (single ops). Cannot and need not be raised. These stay as-is.

## How SDE-to-ARTS Stamps Pattern Contracts From Linalg

Currently, `SdeToArtsPatterns.cpp` does a mechanical 1:1 conversion
(sde.cu_region → arts.edt, sde.su_iterate → arts.for, etc.) without
stamping any pattern contracts. PatternDiscovery at stage 5 does the pattern
analysis.

**With linalg-in-SDE**, the conversion can stamp contracts directly:

```
SDE-to-ARTS conversion:
  for each sde.su_iterate with linalg.generic body:
    1. Read iterator_types → parallel/reduction classification
    2. Read indexing_maps → stencil offsets, access patterns
    3. Stamp depPattern on the arts.for:
       - all-parallel + offset maps → stencil
       - all-parallel + identity maps → uniform
       - 2-parallel + 1-reduction + matmul shape → matmul
       - has-reduction → reduction
    4. Stamp stencil_min/max_offsets from indexing_map constants
    5. Stamp distributionPattern from depPattern
```

This eliminates both CollectMetadata AND PatternDiscovery for raiseable code.

## Pattern Contract Attributes: What Downstream Actually Needs

### Critical Finding: Only 3 Attributes Matter After Stage 5

Deep agent investigation of 26 consumer files reveals a dramatic bottleneck:
stages 7-18 read only **3 contract attributes**. Everything else is consumed
only during contract building at stage 5.

| Attribute | Callers | Used By | Decision |
|---|---|---|---|
| `depPattern` | 8 | DbPartitioning (H1 heuristics), ForLowering, EdtOrchestrationOpt | Pattern family classification |
| `distributionPattern` | 9 | EdtOrchestrationOpt, DistributionHeuristics | Worker layout strategy |
| `partitionMode` | **26** | DbPartitioningSupport (primary router) | Offset/index/size selection |

The stencil spatial attributes (`min/max_offsets`, `owner_dims`, `spatial_dims`,
`write_footprint`, `block_shape`) are read **only during contract building at
stage 5** (StencilTilingNDPattern, PatternDiscovery), never by downstream
passes directly.

The heaviest consumer is `partitionMode` (26 callers), but this is SET by
DbPartitioning, not read from metadata. It is a downstream output.

### What `depPattern` Actually Controls

`depPattern` is read by only 8 callers across 3 passes:

- **DbPartitioning** (H1.B3, H1.S2): Prefers block/stencil modes for Jacobi/wavefront
- **ForLowering** (line 261, 263): Orchestration matching for distribution
- **EdtOrchestrationOpt** (line 79-83): All-or-nothing validation gate

All 8 call sites do enum comparisons (`== stencil`, `== uniform`, etc.).
This is trivially derivable from `linalg.generic` structure.

### Derivability from linalg.generic

| Attribute | Derivable | Method |
|---|---|---|
| `depPattern` (family) | YES | `iterator_types` + `indexing_maps` shape |
| `distributionPattern` | YES | Mechanical from `depPattern` via lookup table |
| `stencil_min_offsets` | YES | Constant terms in `indexing_maps` results |
| `stencil_max_offsets` | YES | Constant terms in `indexing_maps` results |
| `spatial_dims` | YES | Parallel dims in `iterator_types` |
| `patternRevision` | N/A | Versioning counter, not analysis |
| `write_footprint` | NO | Needs runtime array sizes; heuristic decision |
| `block_shape` | NO | Heuristic partitioning decision, not analysis |

`write_footprint` and `block_shape` are NOT analysis results — they are
**heuristic decisions** made by optimization passes. They should never have
been metadata in the first place. StructuredKernelPlanAnalysis already
computes `block_shape` from loop bounds and target parameters.

### Summary: 6 of 8 attributes derivable, 2 are heuristic decisions not metadata

## Key Design Decision: Nesting, Not Replacement

`linalg.generic` nests INSIDE `sde.su_iterate` — it does NOT replace it.
These are fundamentally different concerns:

| Aspect | `sde.su_iterate` | `linalg.generic` |
|---|---|---|
| Purpose | Work distribution policy | Computation structure |
| Controls | Schedule, chunk, nowait, collapse, scope | Indexing maps, iterator types |
| Semantics | "distribute iterations across workers" | "this is a stencil/matmul" |
| State | iter_args (cross-timestep state) | ins/outs (single invocation) |
| Lowering | `arts.for` (EDTs, epochs, CPS) | `scf.for` (via linalg-to-loops) |

If `sde.su_iterate` were replaced by `linalg.generic`, the following would
be lost: schedule kind, chunk size, nowait, reduction identity/combiner,
iter_args for alternating buffers, and concurrency scope.

**The IREE precedent**: IREE's `flow.dispatch.workgroups` contains
`linalg.generic` inside its body. The dispatch defines workgroup distribution;
the linalg defines what each workgroup computes. They are orthogonal.

The pipeline flow for raised code:
```
1. sde.su_iterate body has scf.for + memref (from OMP-to-SDE)
2. RaiseToLinalg: scf.for → linalg.generic on memref (inside su_iterate)
3. RaiseToTensor: linalg on memref → linalg on tensor
4. SDE analysis: tensor-space analysis reads linalg structure + SSA deps
5. Bufferize: tensor → memref (upstream MLIR)
6. linalg-to-loops: linalg.generic → scf.for on memref
7. SDE-to-ARTS: su_iterate (now containing scf.for) → arts.for
```

At step 7, the IR is back to the form the existing pipeline expects. The
tensor/linalg window (steps 2-6) is a transient analysis phase.

## Current RaiseToLinalg: A Skeleton, Not a Transformation

The current `RaiseToLinalg.cpp` (384 LOC) is a **metadata-stamping pass**,
not a transformation pass. It:

1. Walks `arts::ForOp` ops (not SDE ops — it runs after SDE→ARTS)
2. Collects perfectly-nested `scf.for` chains
3. Builds `AffineMap` indexing maps from memref access indices
4. Classifies: elementwise / stencil / matmul / reduction
5. Stamps 5 attributes: `arts.linalg.pattern`, `arts.linalg.iterator_types`,
   `arts.linalg.indexing_maps`, `arts.linalg.num_inputs`, `arts.linalg.num_outputs`

**Critical finding**: These 5 attributes are consumed by **zero** downstream
passes. Grep across all of `lib/arts/` confirms no file reads any
`arts.linalg.*` attribute. The pass is a dead-end prototype.

### What a Real RaiseToLinalg Would Do

A real implementation would create actual `linalg.generic` ops:

```
Before (after OMP-to-SDE):
  sde.su_iterate (%c1) to (%N) step (%c1) {
  ^bb0(%i: index):
    %a = memref.load %A[%i] : memref<?xf64>
    %b = memref.load %B[%i] : memref<?xf64>
    %c = arith.addf %a, %b : f64
    memref.store %c, %C[%i] : memref<?xf64>
    sde.yield
  }

After (RaiseToLinalg):
  linalg.generic {
    indexing_maps = [affine_map<(d0) -> (d0)>,
                     affine_map<(d0) -> (d0)>,
                     affine_map<(d0) -> (d0)>],
    iterator_types = ["parallel"]
  } ins(%A, %B : memref<?xf64>, memref<?xf64>)
    outs(%C : memref<?xf64>) {
  ^bb0(%a: f64, %b: f64, %c: f64):
    %sum = arith.addf %a, %b : f64
    linalg.yield %sum : f64
  }
```

The current pass already computes the indexing maps and iterator types
correctly — it just stamps them as attributes instead of creating the op.

### Limitations of the Current Analysis

The current RaiseToLinalg handles only:
- Perfectly-nested loops (single `scf.for` per nesting level)
- Loops without `iter_args` (no reductions)
- Bodies with only `memref.load`, `memref.store`, and pure ops
- Affine index expressions (linear in IVs with constant offsets)

This covers ~60% of benchmarks. The remaining ~40% (indirect access,
control flow in bodies, non-affine indexing) stays in the memref fallback.

## The Metadata Removal Path

### What Can Be Removed Today (No Linalg Needed)

As documented in `metadata-removal.md`, the metadata pipeline can be removed
**independently** of linalg integration:

1. **7 dead attributes** (reuse_distance, temporal_locality, etc.) — computed
   by nobody reads. ~115 LOC removal.

2. **PatternDiscovery's metadata AND gate** — `collectLocalStencilEvidence()`
   already does full local analysis. The metadata is just a confidence filter
   that rejects cases the local analysis would accept. Remove ~20 lines.

3. **LoopReordering's metadata path** — auto-detection fallback
   (`tryAutoDetectAndReorder`) already covers all cases. Remove ~15 lines.

4. **ForLowering's one metadata read** — `memrefsWithLoopCarriedDeps` is a
   trivial 10-line local computation. Add helper, remove metadata read.

After these changes: -5,970 LOC removed, +40 LOC added. No linalg needed.

### What Linalg Adds on Top

With linalg.generic in SDE bodies, the pattern pipeline simplifies further:

```
CURRENT PIPELINE (18 stages):
  Stage 2:  CollectMetadata → 35 attrs        ← REMOVE
  Stage 3:  VerifyMetadata                     ← REMOVE
  Stage 4:  OMP → SDE → ARTS
  Stage 5:  PatternDiscovery(seed)             ← SIMPLIFY (linalg bodies auto-classify)
            PatternDiscovery(refine)           ← SIMPLIFY
            KernelTransforms                   ← reads linalg structure directly

PROPOSED PIPELINE (with linalg-in-SDE):
  Stage 3.5: OMP → SDE
  Stage 3.6: RaiseToLinalg (create linalg.generic, best-effort)
  Stage 3.9: linalg-to-loops (upstream), bufferize (upstream)
  Stage 4:   SDE → ARTS (stamp contracts from linalg structure)
  Stage 5:   PatternDiscovery (ONLY for non-raised code)
             KernelTransforms
```

For raised code (~60%), pattern contracts are stamped during SDE-to-ARTS
conversion. For non-raised code (~40%), PatternDiscovery's local analysis
handles it (which it already does today — the metadata AND gate only
constrains it).

### Implementation Phases

**Phase 1: Remove metadata (immediate, independent)**
- Remove 7 dead attributes
- Make PatternDiscovery trust its own local analysis
- Make LoopReordering use auto-detection only
- Add local helper in ForLowering
- Delete CollectMetadata, VerifyMetadata, VerifyMetadataIntegrity
- Delete MetadataManager, registry, attacher, analyzers
- -5,970 LOC, +40 LOC, zero functional change

**Phase 2: Upgrade RaiseToLinalg to create actual linalg.generic ops**
- Transform `sde.su_iterate` bodies into `linalg.generic`
- Use the existing `tryBuildIndexingMap` / `computeIteratorTypes` logic
- Handle `linalg-to-loops` before SDE-to-ARTS conversion
- Run after OMP-to-SDE, before SDE-to-ARTS

**Phase 3: Stamp contracts from linalg during SDE-to-ARTS**
- In `SdeToArtsPatterns.cpp`, detect linalg.generic in SDE bodies
- Derive `depPattern`, offsets, distribution from linalg structure
- Stamp contracts on the created `arts.for` ops
- PatternDiscovery becomes a fallback for non-raised code only

**Phase 4: RaiseToTensor (optional, high-value)**
- Convert `linalg.generic` on memref → `linalg.generic` on tensor
- SSA def-use chains replace all dependency analysis
- One-shot bufferize replaces DB mode analysis
- Requires alias-freedom proof for each memref

## Contract Stamping from linalg.generic: Implementation Sketch

```cpp
// In SdeToArtsPatterns.cpp, after creating arts.for from sde.su_iterate:

static std::optional<ArtsDepPattern>
classifyFromLinalg(linalg::GenericOp generic) {
  auto iterTypes = generic.getIteratorTypesArray();
  auto maps = generic.getIndexingMapsArray();

  bool allParallel = llvm::all_of(iterTypes, [](auto t) {
    return t == utils::IteratorType::parallel;
  });

  // Check for stencil: parallel iterators + constant offsets in read maps
  if (allParallel) {
    bool hasOffsets = false;
    for (unsigned i = 0; i < generic.getNumDpsInputs(); ++i) {
      if (hasConstantOffsets(maps[i]))
        hasOffsets = true;
    }
    if (hasOffsets)
      return ArtsDepPattern::stencil;
    return ArtsDepPattern::uniform;
  }

  // 2 parallel + 1 reduction in 3D → matmul
  unsigned nPar = 0, nRed = 0;
  for (auto t : iterTypes) {
    if (t == utils::IteratorType::parallel) ++nPar;
    else ++nRed;
  }
  if (nPar == 2 && nRed == 1 && iterTypes.size() == 3)
    return ArtsDepPattern::matmul;

  return std::nullopt; // fallback to PatternDiscovery
}

static void stampStencilOffsets(arts::ForOp forOp,
                                 linalg::GenericOp generic) {
  auto maps = generic.getIndexingMapsArray();
  SmallVector<int64_t> minOffsets, maxOffsets;
  // Extract constant offsets from each input map dimension
  for (unsigned i = 0; i < generic.getNumDpsInputs(); ++i) {
    for (auto result : maps[i].getResults()) {
      if (auto bin = dyn_cast<AffineBinaryOpExpr>(result)) {
        if (auto cst = dyn_cast<AffineConstantExpr>(bin.getRHS())) {
          minOffsets.push_back(std::min(minOffsets.back(), cst.getValue()));
          maxOffsets.push_back(std::max(maxOffsets.back(), cst.getValue()));
        }
      }
    }
  }
  // stamp on forOp
}
```

## Comparison: Current vs Proposed Architecture

```
CURRENT (dual execution, 6477 LOC infrastructure):

  source → Polygeist → OMP IR
                          ↓
  Stage 2: CollectMetadata (LoopAnalyzer, MemrefAnalyzer, DependenceAnalyzer)
           → stamps 35 attributes on IR
           → VerifyMetadata, VerifyMetadataIntegrity
                          ↓
  Stage 4: OMP → SDE → ARTS
                          ↓
  Stage 5: PatternDiscovery REDOES the same analysis locally
           → AND-gates with metadata (line 266)
           → stamps pattern contracts
                          ↓
  Stages 7-18: read PATTERN CONTRACTS (never read metadata again)


PROPOSED (linalg-native, zero metadata):

  source → Polygeist → OMP IR
                          ↓
  Stage 3.5: OMP → SDE (preserves combiner, nowait, schedule)
                          ↓
  Stage 3.6: RaiseToLinalg (best-effort, ~60% raised)
             → creates linalg.generic inside SDE bodies
             → stencil/uniform/matmul/reduction = IR structure
                          ↓
  Stage 3.9: linalg-to-loops + bufferize (upstream MLIR, zero CARTS code)
                          ↓
  Stage 4: SDE → ARTS
           → stamps contracts FROM linalg structure (raised code)
           → PatternDiscovery handles fallback (non-raised code)
                          ↓
  Stages 7-18: read PATTERN CONTRACTS (same as today)
```

## Why This Works: Evidence from the Codebase

### PatternDiscovery is already self-sufficient

`PatternDiscovery.cpp:157-231` — `collectLocalStencilEvidence()` walks loop
bodies, finds memory accesses, extracts constant offsets from induction
variables, counts stencil dimensions. This is the SAME analysis that
CollectMetadata does, just localized. The metadata is only an AND gate
(line 266): `if (isStencilMemref(*meta) && localStencil.isStencil)`.

### RaiseToLinalg already computes the right information

`RaiseToLinalg.cpp:103-168` — `tryGetAffineExpr()` and `tryBuildIndexingMap()`
compute the exact `AffineMap` structures that `linalg.generic` uses. The
pass just stamps them as attributes instead of creating the op.

### SDE-to-ARTS conversion is mechanical

`SdeToArtsPatterns.cpp` — 8 patterns, all 1:1 mechanical conversions. Adding
contract stamping during conversion is a natural extension (~50 LOC).

### Downstream passes read contracts, not metadata

26 files read `depPattern`. 0 files read raw metadata after stage 5.
DbPartitioning, EdtDistribution, StructuredKernelPlan — all read pattern
contracts. The metadata dies at stage 5. The contracts are what matter.

### 7 of 35 metadata attributes are dead code

`reuse_distance`, `temporal_locality`, `has_good_spatial_locality`,
`first_use_id`, `last_use_id`, `nesting_depth`, `accessed_in_parallel_loop`
— computed by CollectMetadata, consumed by nobody. ~115 LOC wasted.

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| PatternDiscovery loses confidence filtering | Low | Low | Local evidence is MORE precise (runs on ARTS IR, not pre-OMP) |
| Stencil offset bounds inaccurate from linalg | Low | Medium | `indexing_maps` contain exact offsets — more reliable than heuristic |
| Non-raised code misclassified | Low | Low | PatternDiscovery local analysis handles these today |
| linalg-to-loops changes loop structure | Medium | Medium | Run after linalg analysis but before SDE-to-ARTS |
| Benchmark regression | Low | High | Run full suite after Phase 1 before Phase 2 |

## Net Impact

| Metric | Current | After Phase 1 (metadata removal) | After Phase 3 (linalg contracts) |
|---|---|---|---|
| Metadata infrastructure LOC | 6,477 | 0 | 0 |
| Pattern discovery LOC | 445 | ~380 (remove AND gate) | ~200 (fallback only) |
| RaiseToLinalg LOC | 384 (dead) | 384 (dead) | ~500 (real transformation) |
| Contract stamping LOC | 0 | 0 | ~80 (in SDE-to-ARTS) |
| Pipeline stages for metadata | 3 (collect, verify, verify-integrity) | 0 | 0 |
| Analysis kinds in AnalysisManager | includes MetadataManager | -1 | -1 |
| Compile time | baseline | faster (skip stage 2 analysis) | faster + better (linalg fusion) |
