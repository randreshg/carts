# SDE Optimization Opportunities: Tensor, Linalg, and Codelet Transforms

## Executive Summary

This document catalogs reusable optimizations from upstream MLIR and open-source
projects that can be integrated into the CARTS SDE dialect. The analysis was
conducted by 20 parallel research agents covering:

- Upstream MLIR linalg/tensor/SCF/affine transforms
- IREE dispatch formation and codegen pipelines
- Torch-MLIR value semantics and normalization
- Triton software pipelining
- StableHLO/XLA fusion heuristics
- Open Earth Compiler / xDSL stencil dialect
- Buddy-MLIR / MLIR-AIE distribution patterns
- One-shot bufferization and DestinationStyleOpInterface

**Critical architectural boundary**: SDE operates at the semantic level. There is
NO concept of DB (data block), acquire/release, EDT, or GUID at this level. Those
are ARTS concepts that only exist after `ConvertSdeToArts`. SDE codelets work with
pure tensor I/O via `mu_token`. Optimizations must be classified accordingly.

---

## 3-Layer Optimization Architecture

```
Layer 3: SDE DATAFLOW (inter-codelet, mu_data/mu_token graph)
  |-- Codelet fusion (eliminate intermediate tensors)
  |-- Codelet splitting (increase parallelism)
  |-- Token mode refinement (readwrite -> read/write)
  |-- Token slicing (disjoint parallel tensor access)
  |-- Double-buffering for iterative patterns
  '-- Critical path scheduling hints

Layer 2: CODELET BODY (intra-codelet, IsolatedFromAbove sandbox)
  |-- Upstream linalg transforms on carriers/body ops
  |-- Upstream tensor cleanup patterns
  |-- Canonicalization + CSE + DCE (free on IsolatedFromAbove)
  |-- Vectorization (linalg -> vector)
  '-- Register tiling / micro-kernel generation

Layer 1: SDE SCHEDULING (su_iterate + cu_region level)
  |-- TRANSFORM FIRST:
  |   |-- Classification (StructuredOpAnalysis)    [HAVE]
  |   |-- Tile using upstream TilingInterface      [KEY OPPORTUNITY]
  |   |-- Tile-and-fuse via tileConsumerAndFuseProducersUsingSCF
  |   |-- LoopInterchange via linalg carrier       [HAVE]
  |   '-- ElementwiseFusion                        [HAVE]
  |-- THEN SCHEDULE (on optimized structure):
  |   |-- ScopeSelection                           [HAVE — reorder after transforms]
  |   |-- ScheduleRefinement                       [HAVE — reorder after transforms]
  |   |-- ChunkOpt                                 [HAVE — reorder after transforms]
  |   '-- ReductionStrategy                        [HAVE — reorder after transforms]
  |-- THEN DISTRIBUTE:
  |   |-- DistributionPlanning                     [HAVE]
  |   '-- BarrierElimination                       [HAVE]
  '-- DestinationStyleOpInterface on su_iterate
```

---

## Layer 1: SDE Scheduling Optimizations

### 1.1 TilingInterface with CustomOp (HIGH PRIORITY)

**Source**: `mlir/lib/Dialect/SCF/Transforms/TileUsingInterface.cpp`

`SCFTilingOptions::LoopType::CustomOp` accepts two callbacks
(`GenerateLoopHeaderFn`, `GenerateLoopTerminatorFn`) that generate arbitrary loop
constructs as the inter-tile loop. This means we can:

```
Instead of:  scf.for (tile_lo to tile_hi) { tiled_linalg.generic }
Generate:    sde.su_iterate (tile_lo to tile_hi) { tiled_linalg.generic }
```

**Impact**: Replaces ~700 LOC of manual tiling in `TensorOpt.cpp` with upstream's
battle-tested implementation while preserving SDE scheduling semantics.

**Integration**:
1. Compute tile sizes via `SDECostModel` (keep existing logic)
2. Call `scf::tileUsingSCF()` with `CustomOp` callbacks generating `su_iterate`
3. Let upstream handle `extract_slice`/`insert_slice` mechanics
4. Run cleanup patterns: `populateLinalgTilingCanonicalizationPatterns()`

**Estimated LOC reduction**: TensorOpt from ~932 to ~200 LOC.

### 1.2 DestinationStyleOpInterface on SdeSuIterateOp (MEDIUM)

**Source**: `mlir/include/mlir/Interfaces/DestinationStyleOpInterface.h`

`su_iterate` with `iter_args` already has 1:1 result tying — structurally
identical to the destination-passing convention. Adding DST interface requires
implementing one method (`getDpsInits()`). This unlocks:
- Interface-based tiling/fusion from upstream
- `EmptyTensorElimination` for pure-overwrite carriers
- Interop with IREE-style dispatch formation

**Cost**: Minimal — one method override.

### 1.3 Tile-and-Fuse (MEDIUM)

**Source**: `scf::tileAndFuseProducerOfSlice()`,
`scf::tileConsumerAndFuseProducersUsingSCF()`

These upstream APIs handle producer-consumer fusion after tiling. They work on any
op implementing `TilingInterface`. If we implement `TilingInterface` on
`linalg.generic` carriers inside SDE (they already do), these APIs work directly.

**Use case**: After tiling a stencil, fuse the halo pad with the compute kernel.

### 1.4 IREE-Inspired Dispatch Formation Heuristics (MEDIUM)

**Source**: IREE `Flow::FormDispatchRegions`

| IREE Concept | SDE Equivalent | Reuse |
|---|---|---|
| Root-based clustering | What goes into one codelet | Adopt FusionGroup + affine map compat |
| ElementwiseOpFusion with operand count ceiling | SdeElementwiseFusion | Adopt operand count limit |
| SplitReduction threshold (>=128K) | ReductionStrategy | Adopt threshold heuristic |
| CollapseDimensions | Simplify distribution dims | Medium |
| FuseHorizontalContractions | Batched scientific computing | Medium |

### 1.5 XLA Fusion Heuristics (MEDIUM)

**Source**: StableHLO/XLA

Key rules:
- **IsExpensive()**: Only fuse cheap ops (elementwise, broadcast, reshape). Never
  fuse matmul/conv unless into a dispatch.
- **FusionWouldDuplicate()**: If fusing producer into multiple consumers would
  duplicate it, don't fuse — multi-output fusion instead.
- **Element count threshold**: Fuse if output < 128K elements; else tile first.

**Mapping to SDE**: Apply these rules in `SdeElementwiseFusion` to avoid
generating oversized fused codelets that hurt locality.

---

## Layer 2: Codelet Body Optimizations

**IMPORTANT**: These operate INSIDE `cu_codelet` (IsolatedFromAbove). No SDE
scheduling, no mu_data/mu_token — just pure tensor/linalg/arith/scf IR. NO DB,
no acquire/release concepts here.

### 2.1 Upstream Linalg Transform APIs (HIGH PRIORITY)

All of these operate on `linalg.generic` / `LinalgOp` and can run inside a
codelet body without parent-op assumptions:

| API | Can Run in Codelet? | What It Replaces | Priority |
|---|---|---|---|
| `linalg::vectorize()` | YES | LLVM backend vectorization | HIGH |
| `linalg::interchangeGenericOp()` | YES (already used) | N/A | HAVE |
| `linalg::splitReduction()` | YES (already used) | N/A | HAVE |
| `linalg::collapseOpIterationDims()` | YES (already used) | N/A | HAVE |
| `linalg::pack()` | YES (already used) | N/A | HAVE |
| `linalg::generalizeNamedOp()` | YES | Named -> generic normalization | MEDIUM |
| `linalg::transposeLinalgOp()` | YES | Dimension reordering | MEDIUM |
| `linalg::padAndHoistLinalgOp()` | MAYBE* | Cache alignment for matmul | MEDIUM |
| `linalg::decomposeLinalgOp()` | YES | Complex -> simple ops | LOW |
| `linalg::promoteSubViews()` | YES | Local buffer allocation | LOW |

*`padAndHoistLinalgOp` may need the hoist target to be the codelet body.

**No parent-op requirements**: These APIs take a `RewriterBase` and a `LinalgOp`.
They don't check parent types. The codelet's `IsolatedFromAbove` gives them a
clean sandbox — no interference from SDE scheduling ops.

### 2.2 Upstream Tensor Cleanup Patterns (HIGH PRIORITY)

14 HIGH-priority patterns to run as a single greedy rewrite pass inside codelet
bodies after tiling:

| Pattern | Why Critical |
|---|---|
| `populateMergeConsecutiveInsertExtractSlicePatterns` | Collapse nested slice chains from tiling |
| `populateFoldTensorEmptyPatterns` | tensor.empty through reshapes -> simplified |
| `populateReassociativeReshapeFoldingPatterns` | Fold reshape chains |
| `populateDropRedundantInsertSliceRankExpansionPatterns` | Clean up rank expansions |
| `populateBubbleUpExtractSliceOpPatterns` | Move slices above reshapes for fusion |
| `foldExtractAfterInsert` | tensor.extract(tensor.insert(v, idx), idx) -> v |
| `FoldOrthogonalPaddings` | Stencil halo padding across dims -> single pad |
| `buildIndependentOp` (tensor.pad/empty) | Hoist halo padding out of inner loops |
| `populateDecomposePadPatterns` | Break complex pad into simpler forms |
| `populateFoldTensorSubsetIntoVectorTransferPatterns` | Fuse slices into vector.transfer |
| `populateSimplifyTensorPack` | Clean up after pack() |
| `populateSimplifyTensorUnPack` | Clean up after unpack |

These are **free** — just `populate*()` calls into a greedy rewrite pass.

### 2.3 Vectorization Inside Codelets (HIGH PRIORITY — requires pipeline work)

`linalg::vectorize()` converts linalg ops to vector dialect IR:

```
linalg.generic -> vector.transfer_read + vector.contract/fma + vector.transfer_write
```

**Strategy by kernel type**:

| Kernel | Vectorization Strategy | Expected Speedup |
|---|---|---|
| Elementwise | Contiguous vector.load/store | 4-8x (AVX2/512) |
| Matmul | vector.contract -> FMA/VNNI | 4-16x |
| Stencil | vector.gather or shift+blend | 2-4x |
| Reduction | vector.multi_reduction | 2-4x |

**Vector width selection**: Drive by `SDECostModel::getVectorWidth()` (currently
returns 4 for 256-bit/64-bit). Could be made target-aware.

**Integration point**: After tiling, before bufferization. The codelet body is the
perfect place because IsolatedFromAbove guarantees no interference.

**PIPELINE GAP**: The current emission pipeline (`buildLLVMIREmissionPipeline` in
`Compile.cpp:901-912`) has NO vector-to-LLVM lowering passes. To enable MLIR-level
vectorization, the following passes must be added to the emission pipeline:

```
Required additions (in order, before ConvertPolygeistToLLVM or as post-passes):
1. ConvertVectorToSCF        — lower vector.transfer to scf loops + memref ops
2. ConvertVectorToLLVM       — lower vector ops to LLVM vector intrinsics
3. (optional) LowerVectorMask — lower vector.mask to predicated ops
4. (optional) VectorLinearize — flatten multi-dim vectors for targets without them
```

Without these, any `vector.*` ops generated by `linalg::vectorize()` would fail
during LLVM IR emission. Currently CARTS relies on LLVM backend auto-vectorization
via `LoopVectorizationHintsPass` (line 909), which is less effective than MLIR-level
vectorization but requires zero pipeline changes.

### 2.4 Register Tiling / Micro-Kernel Generation (MEDIUM)

For matmul codelet bodies:
- `linalg.pack` for VNNI/AMX layouts (already used)
- `linalg::blockPackMatmul()` for register-blocked matmul
- mr x nr micro-tiles via `vector.contract`

For stencil codelet bodies:
- Register blocking for neighbor accesses
- Shift+blend vectorization (avoid gather)

### 2.5 Standard Cleanup Passes (FREE)

Safe and beneficial inside IsolatedFromAbove:
- **Canonicalization** (all dialects) — constant folding, identity elimination
- **CSE** — common subexpression elimination
- **DCE** — dead code elimination
- **LICM** — loop-invariant code motion for inner `scf.for` loops

These can be run as a sub-pipeline: `pm.addNestedPass<cu_codelet>(...)`.

### 2.6 Loop Optimizations for Inner Loops (MEDIUM)

Inside codelet bodies, there may be `scf.for` loops (e.g., from tiling, or
original loop structure). Upstream transforms:

| API | What It Does | Safety | Priority |
|---|---|---|---|
| `scf::unrollForOp()` | Full or partial loop unrolling | SAFE | MEDIUM |
| `scf::peelForLoop()` | Split loop for aligned main body | SAFE | MEDIUM |
| `scf::pipelineForLoop()` | Software pipeline loop stages | SAFE | LOW |
| `coalescePerfectlyNestedSCFForLoops()` | Merge nested loops | SAFE | LOW |

---

## Layer 3: SDE Dataflow Optimizations (Inter-Codelet)

**IMPORTANT**: This layer operates on the mu_data/mu_token/cu_codelet graph. It
is a SEMANTIC graph — not an ARTS task graph. There are no DBs, no acquire/release,
no GUIDs at this level. Those are ARTS-level concepts that appear only AFTER
`ConvertSdeToArts`.

### 3.1 Codelet Fusion (HIGH)

Merge multiple codelets that form a producer-consumer chain:

**Before**:
```mlir
%data = sde.mu_data shared : tensor<NxT>
sde.cu_codelet(%tok1 : !sde.token<...>) -> tensor<NxT> {
  // producer: compute into tensor
}
sde.cu_codelet(%tok2 : !sde.token<...>) -> tensor<NxT> {
  // consumer: read tensor, compute
}
```

**After**: Single codelet that does both computations without intermediate
materialization.

**When profitable**: Producer result is consumed by exactly one consumer; combined
working set fits in cache.

**Safety**: No circular dependencies, compatible access modes, tokens on same
mu_data don't conflict.

**Upstream support**: `linalg::fuseElementwiseOps()` pattern registration,
`scf::tileAndFuseProducerOfSlice()` for producer-consumer within tiled code.

### 3.2 Codelet Splitting (MEDIUM)

Split one large codelet into multiple smaller ones for parallelism:

- **Dimension-based**: Split codelet along spatial dimensions -> parallel codelets
  with disjoint tensor slices via `mu_token` offset/size slicing
- **Reduction splitting**: Split reduction codelet into partial-reduce +
  final-reduce codelets
- **Pipeline splitting**: Sequential stages within a codelet -> pipeline of
  codelets

**When profitable**: Codelet body is large enough that parallelism gain exceeds
codelet creation overhead.

### 3.3 Token Mode Refinement (HIGH — cheap)

Analyze codelet bodies to downgrade access modes:
- `readwrite` -> `read` when write is dead
- `readwrite` -> `write` when input is not actually read
- This propagates downstream: a `write`-only token enables parallel access from
  other readers

**Implementation**: Walk codelet body, classify uses of each block arg. Already
partially done in `ConvertToCodelet.cpp:classifyAccess()`.

### 3.4 Token Slicing for Parallelism (MEDIUM)

If two codelets access disjoint sub-regions of the same mu_data, their tokens can
be narrowed to non-overlapping slices:

```mlir
// Instead of:
%tok1 = sde.mu_token <readwrite> %data : tensor<1000xf64>
%tok2 = sde.mu_token <readwrite> %data : tensor<1000xf64>
// With slicing:
%tok1 = sde.mu_token <readwrite> %data offsets=[0] sizes=[500]
%tok2 = sde.mu_token <readwrite> %data offsets=[500] sizes=[500]
```

Disjoint slices can execute in parallel even with `readwrite` mode.

### 3.5 Double-Buffering for Iterative Patterns (MEDIUM)

For iterative stencils (time-stepping loops), alternate between two mu_data
objects to overlap reads of step N with writes of step N+1:

```mlir
%buf_a = sde.mu_data shared : tensor<NxT>
%buf_b = sde.mu_data shared : tensor<NxT>
// Even iterations: read buf_a, write buf_b
// Odd iterations: read buf_b, write buf_a
```

This is a pure SDE-level optimization — no ARTS concepts needed.

### 3.6 Critical Path Analysis and Priority Hints (LOW-MEDIUM)

The mu_data/mu_token dependency graph defines a DAG. Assign priority attributes
to codelets based on longest-path-to-sink. `ConvertSdeToArts` can translate these
to ARTS EDT priority hints.

**ARTS runtime gap**: The runtime already supports a priority deque for task
scheduling, but the compiler currently passes `priority=0` for ALL EDTs. Wiring
critical-path priority from SDE codelet DAG analysis to EDT creation would require:
- ~200 LOC: Longest-path-to-sink analysis on the codelet DAG
- ~200 LOC: Priority attribute propagation through `ConvertSdeToArts`
- Potentially 10-30% latency improvement for DAGs with unbalanced branches

This is a pure semantic optimization — no ARTS concepts leak into SDE.

---

## Layer 1 Extended: SCF and Affine Transforms

### SCF Transforms Catalogue

| Transform | Location | SDE Reuse | Notes |
|---|---|---|---|
| `tileUsingSCF()` | SCF/TileUsingInterface.cpp | **HIGH** | Core tiling with CustomOp |
| `tileConsumerAndFuseProducersUsingSCF()` | Same | **HIGH** | Tile-and-fuse |
| `tileAndFuseConsumerOfSlice()` | Same | HIGH | Consumer fusion |
| `peelForLoop()` | SCF/Utils.cpp | MEDIUM | Alignment peeling |
| `pipelineForLoop()` | SCF/Pipelining.cpp | MEDIUM | Software pipelining |
| `ParallelLoopTiling` | SCF/ParallelLoopTiling.cpp | LOW | For scf.parallel |
| `ForLoopSpecialization` | SCF/ForToWhile.cpp | LOW | Specialize known-count |

### Affine Transforms Catalogue

| Transform | SDE Reuse | Notes |
|---|---|---|
| `affine::loopTiling()` | LOW | Affine-specific, SDE uses SCF |
| `affine::loopInterchange()` | LOW | SDE uses linalg interchange |
| `affine::loopUnrolling()` | MEDIUM | Codelet inner loops |
| `affine::loopFusion()` | LOW | SDE uses linalg fusion |
| `affine::SuperVectorize` | MEDIUM | Alternative vectorization path |
| `affine::ScalarReplacement` | MEDIUM | Store-to-load forwarding |
| `affine::LoopCoalescing` | MEDIUM | Codelet inner loop simplification |

---

## Stencil-Specific Opportunities

### Open Earth Compiler / xDSL Stencil Dialect

The xDSL stencil dialect (from ETH Zurich / EPCC) is the closest match to SDE's
stencil workload:

| OEC/xDSL Concept | SDE Equivalent | Status |
|---|---|---|
| `stencil.access` (constant offsets) | `accessMinOffsets`/`accessMaxOffsets` | Already have |
| `stencil.combine` (domain merge) | `SdeIterationSpaceDecomposition` | Already have |
| `DomainSplitPass` | Block partitioning | Already have (ARTS core) |
| `ShapeOverlapPass` (halo detection) | Stencil boundary peeling | Already have |
| `stencil.buffer` (fusion barrier) | `SdeBarrierElimination` (inverse) | Already have |
| `dmp.swap` (halo exchange) | ARTS halo DB transport | Already have (ARTS) |

**Key insight**: SDE already exceeds OEC in task-based distribution, distributed
memory, and tensor-level analysis. But OEC validates the architectural approach
and its time-tiling transforms are worth studying.

### Stencil Tiling Strategies (MEDIUM-HIGH)

- **Diamond tiling**: 10-40% improvement over standard tiling for time-iterated
  stencils (Bondhugula et al.). Not yet in upstream MLIR but can be implemented
  as SDE tiling callbacks via `TilingInterface::CustomOp`.
- **Time-tiling / temporal blocking**: Compute multiple time steps per spatial
  tile. Requires careful dependency handling. SDE's tensor SSA makes this natural.
- **Skewing**: Required for legal temporal tiling. Can be implemented as affine
  index transformation inside codelet bodies.

### Stencil Representation in Linalg

Stencils can be represented as `linalg.generic` with windowed access:
- Use `tensor.extract_slice` for halo regions → pad → `linalg.generic` with
  non-trivial indexing maps
- Or use `tensor.pad` + `linalg.generic` with standard convolution-like maps

This is the path to raising stencils to linalg carriers (currently on fallback).

---

## Bufferization Strategy

### Recommended: One-Shot Bufferization at Codelet Boundary

**Strategy**: Keep tensor semantics inside codelet bodies throughout the SDE
phase. Bufferize during/after `ConvertSdeToArts` when tensor block args map to
ARTS DB acquire memrefs.

**Why**:
1. Tensor SSA inside codelets enables more optimizations (fusion, slicing, CSE)
2. `IsolatedFromAbove` makes one-shot bufferization's analysis simpler
3. The codelet block args naturally map to DB acquire pointers (zero-copy)
4. `EmptyTensorElimination` can eliminate temporary allocations

**Upstream API**: `mlir::bufferization::runOneShotBufferize()` with
`BufferizationOptions` configured for the codelet scope.

**Key insight from Torch-MLIR**: Two-phase value semantics approach:
1. `ReduceOpVariants` — normalize ops to value-semantic form (drop in-place aliases)
2. `MaximizeValueSemantics` — propagate value semantics as far as possible

SDE's current single-shot `RaiseToTensor` does both steps at once. If alias chains
become more complex (e.g., multi-indirection patterns from specfem3d-like codes),
splitting into two phases would be more robust.

**EmptyTensorElimination**: Available as
`mlir::bufferization::eliminateEmptyTensors()`. Should run after tiling inside
codelets — tiling creates `tensor.empty` for tile-sized intermediaries that may be
eliminable when the destination is already known (destination-passing style).

---

## Composable Dialect Opportunities

### Shard Dialect (HIGH — for distributed SDE)

The upstream `mesh.shard` dialect handles distributed tensor sharding with halo
support. This maps directly to SDE's `su_distribute` + stencil halo needs.

### SparseTensor Dialect (LOW — niche)

For sparse workloads (indirect access patterns like specfem3d), sparse encoding
attributes on tensors could enable specialized lowering.

### Transform Dialect (MEDIUM — composable schedules)

58 linalg ops already mapped for Transform dialect. Could express SDE optimization
schedules as composable `transform.sequence` programs instead of hardcoded passes.

**Benefit**: Users could customize optimization strategy per kernel type via
schedule files rather than pass options.

---

## Prioritized Roadmap

### Phase 0: Pipeline Reordering (prerequisite — unlocks everything else)

0. **Reorder SDE pipeline: optimize → analyze → schedule** — Move ScopeSelection,
   ScheduleRefinement, ChunkOpt, ReductionStrategy AFTER TensorOpt +
   StructuredSummaries + ElementwiseFusion. Remove TensorOpt rejection gates.
   ~20 LOC change in Compile.cpp, ~10 LOC cleanup in TensorOpt.cpp. See
   "Pipeline Reordering" section for full analysis.

### Phase 1: Quick Wins (1-2 weeks each)

1. **Tensor cleanup pass inside codelets** — Just populate 14 upstream patterns
   into a greedy rewrite. ~50 LOC.
2. **Canonicalization+CSE+DCE sub-pipeline** for codelet bodies — ~10 LOC pipeline
   setup.
3. **Token mode refinement** — Extend `classifyAccess()` to run as a standalone
   optimization. ~100 LOC.

### Phase 2: Major Upstream Adoption (2-4 weeks each)

4. **Replace TensorOpt tiling with TilingInterface+CustomOp** — Cuts TensorOpt
   from ~932 to ~200 LOC. Major correctness improvement.
5. **Vectorization pass for codelet bodies** — Call `linalg::vectorize()` after
   tiling. ~200 LOC wrapper.
6. **DestinationStyleOpInterface on su_iterate** — One method, unlocks
   interface-based transforms.

### Phase 3: New Capabilities (1-2 months each)

7. **Codelet fusion pass** — Eliminate intermediate tensors between
   producer/consumer codelets. ~400 LOC.
8. **Stencil linalg raising** — Raise stencils to `linalg.generic` carriers using
   `tensor.pad` + convolution-like maps. ~500 LOC.
9. **One-shot bufferization at codelet boundary** — Replace current
   memref-fallback with proper tensor → memref at the SDE/ARTS boundary. ~300 LOC.

### Phase 4: Advanced (research-grade)

10. **Diamond tiling for time-iterated stencils** — Via TilingInterface callbacks.
11. **Transform dialect schedules** — Composable per-kernel optimization.
12. **Double-buffering for iterative stencils** — Pure SDE-level optimization.
13. **Codelet splitting for parallelism** — Dimension-based and reduction-based.

---

## Key Finding: TensorOpt Simplification

The single highest-impact opportunity: replacing `TensorOpt.cpp`'s manual tiling
(~700 LOC) with upstream `scf::tileUsingSCF()` using `CustomOp` callbacks. This:

1. Eliminates manual trip count computation, tile loop nesting, scalar body cloning
2. Gains correctness from upstream's battle-tested implementation
3. Preserves SDE scheduling semantics via `CustomOp` callbacks
4. Enables automatic cleanup via `populateLinalgTilingCanonicalizationPatterns()`
5. Opens the door to tile-and-fuse (`tileConsumerAndFuseProducersUsingSCF`)

**Before** (manual):
```cpp
// ~700 LOC of manual strip-mining, trip count, tile loop construction
Value tileSize = costModel.computeTileSize(...);
// ... build scf.for, clone body, adjust bounds ...
```

**After** (upstream):
```cpp
// ~50 LOC
SCFTilingOptions options;
options.setTileSizes(costModel.computeTileSizes(op));
options.setLoopType(SCFTilingOptions::LoopType::CustomOp);
options.setLoopHeaderFn(generateSuIterateHeader);
options.setLoopTerminatorFn(generateSuIterateTerminator);
auto result = scf::tileUsingSCF(rewriter, tilingOp, options);
```

---

## Pipeline Reordering: Optimize First, Schedule Later

### Problem with Current Ordering

The current SDE pipeline schedules BEFORE optimizing:

```text
CURRENT (WRONG):
  RaiseToLinalg → RaiseToTensor
  → ScopeSelection       ← reads PRE-TILING trip count
  → ScheduleRefinement   ← reads PRE-TILING trip count
  → ChunkOpt             ← reads PRE-TILING trip count
  → ReductionStrategy    ← sees PRE-TILING loop structure
  → LoopInterchange      ← stencil halo path broken (needs StructuredSummaries)
  → TensorOpt            ← GATEKEPT by schedule+chunk rejection
  → StructuredSummaries  ← analyzes post-optimization form
  → ElementwiseFusion    ← fusion can fail if schedule/chunk diverged
  ...
```

**Five concrete problems:**

1. **Trip count is wrong.** ScopeSelection, ScheduleRefinement, and ChunkOpt read
   `getStaticTripCount()` to compute costs and chunk sizes. TensorOpt changes the
   `su_iterate` step from `%step` to `%step × tileIterations`, reducing the trip
   count. Effect passes compute on the ORIGINAL trip count, not the tiled one.

2. **TensorOpt is gatekept.** `isTensorOptimizationSchedule()` rejects loops with
   `schedule(dynamic)` or `schedule(guided)`. `getChunkSize()` presence also causes
   rejection. If ScheduleRefinement picks `dynamic` or ChunkOpt sets a chunk,
   TensorOpt SKIPS the loop entirely — missing a tiling opportunity.

3. **Fusion is blocked.** ElementwiseFusion matches adjacent loops by comparing
   `schedule` and `chunkSize`. If ScheduleRefinement/ChunkOpt assigned different
   values to adjacent loops (e.g., different trip counts → different chunks), fusion
   fails even though the loops were originally identical.

4. **Reduction strategy sees wrong structure.** ReductionStrategy checks
   `hasNestedSequentialLoop()`. TensorOpt creates inner `scf.for` tile loops. After
   tiling, `hasNestedSequentialLoop` returns true → `local_accumulate` is chosen.
   Before tiling, it returns false → `atomic`/`tree` is chosen. The post-tiling
   decision is MORE correct for tiled reductions.

5. **LoopInterchange stencil halo path is broken.** It reads `accessMinOffsets`
   stamped by StructuredSummaries, but runs BEFORE StructuredSummaries. The stencil
   halo interchange path never activates.

### Proposed Pipeline: Transform → Analyze → Schedule

```text
PROPOSED:
  ConvertOpenMPToSde

  ── RAISING ──
  RaiseToLinalg                  ── classification + memref carrier
  RaiseToTensor                  ── tensor carrier

  ── STRUCTURAL OPTIMIZATION (transform the computation) ──
  LoopInterchange                ── dimension reordering (reads classification from RaiseToLinalg)
  TensorOpt                      ── cost-model tiling (schedule=auto → proceeds, no chunk → proceeds)

  ── ANALYSIS (analyze the optimized form) ──
  StructuredSummaries            ── stamps neighborhood attrs ON POST-OPTIMIZATION structure

  ── FUSION (merge before scheduling to avoid schedule divergence) ──
  ElementwiseFusion              ── fuse compatible carriers

  ── SCHEDULING DECISIONS (on optimized + fused structure) ──
  ScopeSelection                 ── on post-tiling trip counts
  ScheduleRefinement             ── on post-tiling trip counts
  ChunkOpt                       ── on post-tiling trip counts
  ReductionStrategy              ── sees post-tiling nested scf.for

  ── DISTRIBUTION + DECOMPOSITION ──
  DistributionPlanning           ── reads classification + scope + neighborhood + reduction_strategy
  IterationSpaceDecomposition    ── pure pattern matching
  BarrierElimination             ── reads classification + memref access

  ── CODELET FORMATION ──
  ConvertToCodelet               ── cu_region → mu_data + mu_token + cu_codelet

  ── CODELET BODY OPTIMIZATIONS (nested on cu_codelet) ──
  │  TensorCleanup               ── 14 upstream patterns (Phase 1, ~50 LOC)
  │  Canonicalize+CSE+DCE        ── standard cleanup (Phase 1, ~10 LOC)
  │  TokenModeRefinement          ── downgrade readwrite (Phase 1, ~100 LOC)
  │  CodeletVectorization         ── linalg::vectorize() (Phase 2, ~200 LOC)
  │  CodeletFusion                ── merge producer/consumer (Phase 3, ~400 LOC)

  ── LOWERING ──
  ConvertSdeToArts               ── SDE → ARTS + carrier erasure
  VerifySdeLowered
  DCE + CSE
  VerifyEdtCreated
```

### Dependency Verification

All hard dependencies are satisfied in the proposed order:

| Dependency | Required Before → After | Satisfied? |
|---|---|---|
| `structured_classification` | RaiseToLinalg → LoopInterchange, TensorOpt, StructuredSummaries | **YES** |
| `structured_classification` (refreshed) | StructuredSummaries → ElementwiseFusion, BarrierElimination | **YES** |
| `neighborhood attrs` | StructuredSummaries → DistributionPlanning, ConvertSdeToArts | **YES** |
| `concurrency_scope` | ScopeSelection → DistributionPlanning | **YES** |
| `reduction_strategy` | ReductionStrategy → DistributionPlanning | **YES** |
| `classification=elementwise` | StructuredSummaries → ElementwiseFusion | **YES** |
| `schedule`/`chunk` matching | (not yet set) → ElementwiseFusion matches default=default | **YES** (better!) |

### Required Code Changes

**`Compile.cpp:659-685`**: Reorder the pass additions in `buildOpenMPToArtsPipeline`.

**`TensorOpt.cpp`**: Remove or simplify the `isTensorOptimizationSchedule()` and
`getChunkSize()` rejection gates. In the new order, schedule is always `auto` and
chunk is always absent when TensorOpt runs. The gates become dead code. Keep a
guard for user-specified `schedule(static, N)` from OpenMP source if needed.

**`ScheduleRefinement.cpp`**: No changes needed — it already reads trip count from
`getStaticTripCount()`, which now reflects the post-tiling value.

**`ChunkOpt.cpp`**: No changes needed — it already reads trip count from bounds.

**`ReductionStrategy.cpp`**: No changes needed — `hasNestedSequentialLoop()` now
correctly sees the tile loop.

**`ScopeSelection.cpp`**: No changes needed — cost refinement now uses post-tiling
trip counts.

The codelet body optimization passes would run as nested passes on `cu_codelet`
via `pm.addNestedPass<sde::SdeCuCodeletOp>(...)`. The `IsolatedFromAbove`
property makes them safe to nest without invalidating parent-level analysis.

### Known Issue: LoopInterchange Stencil Halo Path

LoopInterchange has two paths:
- **Phase 7A (carrier-based)**: reads `structured_classification` from RaiseToLinalg → WORKS
- **Phase 7B (stencil halo)**: reads `accessMinOffsets` from StructuredSummaries → BROKEN

Phase 7B was already broken in the current pipeline (LoopInterchange ran before
StructuredSummaries). Two possible fixes:
1. Run StructuredSummaries twice (before and after LoopInterchange) — wasteful
2. Have LoopInterchange compute halo widths internally via `StructuredOpAnalysis`
   instead of reading attributes — cleaner, ~30 LOC

## Upstream API Quick Reference

Headers to include for each category:

```cpp
// Tiling (Layer 1)
#include "mlir/Dialect/SCF/Transforms/TileUsingInterface.h"   // scf::tileUsingSCF
#include "mlir/Interfaces/TilingInterface.h"                   // TilingInterface

// Linalg transforms (Layer 2)
#include "mlir/Dialect/Linalg/Transforms/Transforms.h"         // vectorize, split, pack, etc.
#include "mlir/Dialect/Linalg/Transforms/Hoisting.h"           // pad hoisting

// Tensor cleanup (Layer 2)
#include "mlir/Dialect/Tensor/Transforms/Transforms.h"         // all populate*() patterns

// Vectorization lowering (emission pipeline)
#include "mlir/Conversion/VectorToSCF/VectorToSCF.h"           // ConvertVectorToSCF
#include "mlir/Conversion/VectorToLLVM/ConvertVectorToLLVM.h"  // ConvertVectorToLLVM

// Bufferization (SDE/ARTS boundary)
#include "mlir/Dialect/Bufferization/Transforms/OneShotAnalysis.h"
#include "mlir/Dialect/Bufferization/Transforms/Passes.h"       // eliminateEmptyTensors
```

## References

- MLIR Linalg Dialect: https://mlir.llvm.org/docs/Dialects/Linalg/
- MLIR TilingInterface: mlir/include/mlir/Interfaces/TilingInterface.h
- IREE Dispatch Formation: github.com/iree-org/iree Flow dialect
- Open Earth Compiler: github.com/spcl/open-earth-compiler
- xDSL Stencil: https://arxiv.org/html/2404.02218v2
- Diamond Tiling: Bondhugula et al., IEEE TACO
- Torch-MLIR: github.com/llvm/torch-mlir
- One-Shot Bufferization: mlir/lib/Dialect/Bufferization/
