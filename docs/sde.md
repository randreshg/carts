# SDE Dialect Reference

The **SDE** (Semantic Dialect Extension) is an MLIR dialect that sits between
OpenMP source semantics and the ARTS core dialect in the CARTS compilation
pipeline. It preserves high-level information (reduction combiners, schedule
hints, nowait semantics) that the direct OpenMP-to-ARTS conversion loses, and
provides a structured optimization window before ARTS lowering.

- **Dialect namespace**: `arts_sde`
- **C++ namespace**: `mlir::arts::sde`
- **TableGen**: `include/arts/dialect/sde/IR/SdeOps.td`

## Pipeline

The SDE pipeline consists of 21 passes registered in `buildOpenMPToArtsPipeline`
(`tools/compile/Compile.cpp:660`). After the pipeline completes, all `arts_sde`
ops have been converted to `arts` ops and the ARTS core pipeline takes over.

### Entry

| # | Pass | CLI flag | Source |
|---|------|----------|--------|
| 1 | ConvertOpenMPToSde | `--convert-openmp-to-sde` | `lib/arts/dialect/sde/Conversion/OmpToSde/ConvertOpenMPToSde.cpp` |

Converts `omp.parallel`, `omp.wsloop`, `omp.task`, etc. into `arts_sde.*` ops.
Preserves reduction combiner kind, schedule, chunk, nowait, and task dep modes.

### Raising (tensor-first)

| # | Pass | CLI flag | Source |
|---|------|----------|--------|
| 2 | RaiseToTensor | `--raise-to-tensor` | `lib/arts/dialect/sde/Transforms/state/raising/RaiseToTensor.cpp` |
| 3 | RaiseToLinalg | `--raise-to-linalg` | `lib/arts/dialect/sde/Transforms/state/raising/RaiseToLinalg.cpp` |

**RaiseToTensor** converts `memref.alloca` values to tensor SSA threaded via
`iter_args` (mem2reg for SDE). Function arg memrefs are not raised.

**RaiseToLinalg** classifies loop bodies via `StructuredOpAnalysis` and stamps
`structuredClassification` on `su_iterate`. For elementwise, matmul, and narrow
reduction patterns, creates a carrier-authoritative `linalg.generic` and erases
the scalar body. Stencils are classified but get no carrier.

### Dep (structural transforms)

| # | Pass | CLI flag | Source |
|---|------|----------|--------|
| 4 | LoopInterchange | `--loop-interchange` | `lib/arts/dialect/sde/Transforms/dep/tensor/Interchange.cpp` |
| 5 | Tiling | `--tiling` | `lib/arts/dialect/sde/Transforms/dep/tensor/Tiling.cpp` |
| 6 | StructuredSummaries | `--structured-summaries` | `lib/arts/dialect/sde/Transforms/state/StructuredSummaries.cpp` |
| 7 | ElementwiseFusion | `--elementwise-fusion` | `lib/arts/dialect/sde/Transforms/dep/fusion/ElementwiseFusion.cpp` |

**LoopInterchange** reorders loop dims for stride-1 access. Carrier-based path
uses `linalg::interchangeGenericOp()`; halo-based path reorders stencil loops.

**Tiling** applies cost-model-driven strip-mining. Carrier-authoritative path
uses `extract_slice` / tiled `linalg.generic` / `insert_slice`; dual-rep path
uses `scf.for` + scalar clone.

**StructuredSummaries** stamps runtime-neutral neighborhood attributes
(`accessMinOffsets`, `accessMaxOffsets`, `ownerDims`, `spatialDims`,
`writeFootprint`) and vectorization hints (`sde.vector_width`,
`sde.unroll_factor`). No structural IR changes.

**ElementwiseFusion** merges consecutive elementwise `su_iterate` ops with
compatible iteration spaces into `classification(<elementwise_pipeline>)`.

### Effect (scheduling decisions)

| # | Pass | CLI flag | Source |
|---|------|----------|--------|
| 8 | ScopeSelection | `--scope-selection` | `lib/arts/dialect/sde/Transforms/effect/scheduling/ScopeSelection.cpp` |
| 9 | ScheduleRefinement | `--schedule-refinement` | `lib/arts/dialect/sde/Transforms/effect/scheduling/ScheduleRefinement.cpp` |
| 10 | ChunkOpt | `--chunk-opt` | `lib/arts/dialect/sde/Transforms/effect/scheduling/ChunkOpt.cpp` |
| 11 | ReductionStrategy | `--reduction-strategy` | `lib/arts/dialect/sde/Transforms/effect/scheduling/ReductionStrategy.cpp` |

These passes stamp scheduling attributes without changing IR structure.
ScopeSelection sets `local`/`distributed` on `cu_region`. ScheduleRefinement
refines `auto`/`runtime` to `static`/`dynamic`/`guided`. ChunkOpt synthesizes
chunk sizes. ReductionStrategy selects `atomic`/`tree`/`local_accumulate`.

### Distribution and decomposition

| # | Pass | CLI flag | Source |
|---|------|----------|--------|
| 12 | DistributionPlanning | `--distribution-planning` | `lib/arts/dialect/sde/Transforms/effect/distribution/DistributionPlanning.cpp` |
| 13 | IterationSpaceDecomposition | `--iteration-space-decomposition` | `lib/arts/dialect/sde/Transforms/dep/loop/IterationSpaceDecomposition.cpp` |
| 14 | BarrierElimination | `--barrier-elimination` | `lib/arts/dialect/sde/Transforms/effect/distribution/BarrierElimination.cpp` |

**DistributionPlanning** wraps eligible `su_iterate` in `su_distribute`:
elementwise gets `<blocked>`, stencil gets `<owner_compute>`.

**IterationSpaceDecomposition** peels boundary iterations from stencil loops,
creating branch-free interior loops suitable for vectorization.

**BarrierElimination** eliminates redundant `su_barrier` ops between SUs with
disjoint write/read sets. Carrier-aware: traces `linalg.generic` DPS
inputs/outputs through `mu_memref_to_tensor`.

### Lowering

| # | Pass | CLI flag | Source |
|---|------|----------|--------|
| 15 | LowerToMemref | `--lower-to-memref` | `lib/arts/dialect/sde/Transforms/state/raising/LowerToMemref.cpp` |
| 16 | ConvertToCodelet | `--convert-to-codelet` | `lib/arts/dialect/sde/Transforms/state/codelet/ConvertToCodelet.cpp` |
| 17 | ConvertSdeToArts | `--convert-sde-to-arts` | `lib/arts/dialect/core/Conversion/SdeToArts/SdeToArtsPatterns.cpp` |
| 18 | VerifySdeLowered | `--verify-sde-lowered` | `lib/arts/dialect/sde/Verify/VerifySdeLowered.cpp` |

**LowerToMemref** is the inverse of RaiseToTensor: tensor SSA back to
`memref.alloca` + load/store. Erases dead carriers.

**ConvertToCodelet** converts `cu_region <single>` with tensor `iter_args` into
`mu_data`/`mu_token`/`cu_codelet` graph.

**ConvertSdeToArts** is the SDE-to-ARTS boundary. Converts all `arts_sde.*` to
`arts.*`, materializes ARTS contracts from SDE attributes, and erases carriers.
See [Three-route contract materialization](#three-route-contract-materialization)
below.

**VerifySdeLowered** rejects any surviving `arts_sde.*` ops or transient
carriers.

### Cleanup

| # | Pass | Source |
|---|------|--------|
| 19 | DCE | MLIR builtin |
| 20 | CSE | MLIR builtin |
| 21 | VerifyEdtCreated | `lib/arts/dialect/core/Transforms/verify/VerifyEdtCreated.cpp` |

## Carrier-Authoritative Model

The `linalg.generic` carrier inside `su_iterate` is the source of truth for
computation in elementwise, matmul, and reduction patterns:

1. **RaiseToLinalg** creates the carrier using `mu_memref_to_tensor` for
   external memrefs, then erases the scalar body.
2. **Downstream passes** operate directly on the carrier:
   - LoopInterchange: permutes dims via `linalg::interchangeGenericOp()`
   - Tiling: `extract_slice` / tiled generic / `insert_slice`
   - ElementwiseFusion: reads DPS outputs for write-set analysis
   - BarrierElimination: reads DPS inputs/outputs for read/write sets
3. **LowerToMemref** restores memref form.
4. **ConvertSdeToArts** consumes classification + attributes, stamps ARTS
   contracts, erases surviving carriers.

Stencils remain on the fallback path: classified but no carrier. Their scalar
bodies survive through the pipeline.

### Three-route contract materialization

| Route | Condition | Result |
|-------|-----------|--------|
| Primary | `structuredClassification` + neighborhood attrs | SDE attrs from RaiseToLinalg + StructuredSummaries |
| Linalg fallback | No classification, `linalg.generic` exists | Derives pattern from iterator types + indexing maps |
| No contract | Neither | Falls through to ARTS PatternDiscovery |

Classification to ARTS contract mapping:

| SDE classification | ARTS `depPattern` |
|--------------------|-------------------|
| `elementwise` | `uniform` |
| `elementwise_pipeline` | `elementwise_pipeline` |
| `stencil` | `stencil_tiling_nd` |
| `matmul` | `matmul` |

## Tensor-First Sandwich

```
  memref world             tensor world              memref world
  ───────────  RaiseToTensor ───────────  LowerToMemref ──────────
  memref.alloca ────────> tensor SSA     ────────> memref.alloca
  memref.load   ────────> tensor.extract ────────> memref.load
  memref.store  ────────> tensor.insert  ────────> memref.store
                Pass 2                           Pass 15
                     Passes 3-14 operate on
                        tensor SSA form
```

Tensor SSA enables alias-free analysis. Memref form is required by downstream
ARTS core ops.

## Key Attributes

### On `su_iterate`

| Attribute | Type | Stamped by | Consumed by |
|-----------|------|-----------|-------------|
| `structuredClassification` | `SdeStructuredClassificationAttr` | RaiseToLinalg (3) | ConvertSdeToArts (17) |
| `accessMinOffsets` | `ArrayAttr` | StructuredSummaries (6) | ConvertSdeToArts (17) |
| `accessMaxOffsets` | `ArrayAttr` | StructuredSummaries (6) | ConvertSdeToArts (17) |
| `ownerDims` | `ArrayAttr` | StructuredSummaries (6) | ConvertSdeToArts (17) |
| `spatialDims` | `ArrayAttr` | StructuredSummaries (6) | ConvertSdeToArts (17) |
| `writeFootprint` | `ArrayAttr` | StructuredSummaries (6) | ConvertSdeToArts (17) |
| `schedule` | `SdeScheduleKindAttr` | ConvertOpenMPToSde (1) / ScheduleRefinement (9) | ChunkOpt (10), ConvertSdeToArts (17) |
| `chunkSize` | `Index` operand | ChunkOpt (10) | ConvertSdeToArts (17) |
| `reductionKinds` | `ArrayAttr` | ConvertOpenMPToSde (1) | ReductionStrategy (11), ConvertSdeToArts (17) |
| `reductionStrategy` | `SdeReductionStrategyAttr` | ReductionStrategy (11) | DistributionPlanning (12), ConvertSdeToArts (17) |
| `nowait` | `UnitAttr` | ConvertOpenMPToSde (1) / BarrierElimination (14) | ConvertSdeToArts (17) |

### On `cu_region`

| Attribute | Type | Stamped by | Consumed by |
|-----------|------|-----------|-------------|
| `concurrency_scope` | `SdeConcurrencyScopeAttr` | ScopeSelection (8) | DistributionPlanning (12) |

## Dialect Ops

### CU (Compute Unit)

| Op | Summary |
|----|---------|
| `cu_region` | Parallel, single, or task execution region (from `omp.parallel`/`omp.single`) |
| `cu_task` | Task with explicit dep tokens (from `omp.task`) |
| `cu_codelet` | IsolatedFromAbove dataflow unit; all I/O through `mu_token` operands |
| `cu_reduce` | Reduction accumulation with explicit combiner kind |
| `cu_atomic` | Atomic memory update (from `omp.atomic.update`) |

### SU (Scheduling Unit)

| Op | Summary |
|----|---------|
| `su_iterate` | Parallel iteration space (from `omp.wsloop`, `omp.taskloop`) |
| `su_distribute` | Distribution advisory wrapper (`blocked`/`owner_compute`/`cyclic`) |
| `su_barrier` | Synchronization point (from `omp.barrier`) |

### MU (Memory Unit)

| Op | Summary |
|----|---------|
| `mu_data` | Shared-data anchor (lowered to `arts.db_alloc`) |
| `mu_dep` | Data dependency declaration on memref (memref fallback path) |
| `mu_token` | Tensor-path access token for codelets (lowered to `arts.db_acquire`) |
| `mu_alloc` | Tensor allocation (SDE equivalent of `memref.alloca`) |
| `mu_memref_to_tensor` | Snapshot memref into tensor SSA at SDE boundary |
| `mu_tensor_to_memref` | Materialize tensor back to memref at SDE boundary |
| `mu_reduction_decl` | Module-level reduction symbol declaration |

### Control flow

| Op | Summary |
|----|---------|
| `yield` | Region terminator; optionally returns values for reductions/iter_args |

## Testing

SDE tests are co-located with source code (IREE-style):

```
lib/arts/dialect/sde/test/    -- ~100 SDE pass tests
```

Run all pass tests (includes SDE):

```bash
dekk carts test
```

Run a single SDE test:

```bash
dekk carts lit lib/arts/dialect/sde/test/my_test.mlir
```

## See Also

- `docs/architecture/sde-pipeline-walkthrough.md` -- detailed IR traces through each pass
- `docs/architecture/linalg-in-sde.md` -- carrier model design and limitations
- `include/arts/dialect/sde/Transforms/Passes.td` -- umbrella TableGen pass declarations
