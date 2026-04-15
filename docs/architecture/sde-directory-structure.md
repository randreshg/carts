# SDE Directory Structure

## Design Principles

1. **Optimize first, schedule later.** The directory structure reflects the correct
   pipeline phase order: raise → transform → analyze → fuse → schedule → distribute →
   codelet → lower.

2. **state / dep / effect** remain the primary categories within Transforms/:
   - **state**: IR form and representation changes (what the IR looks like)
   - **dep**: Structural transforms driven by data dependencies (what computes what)
   - **effect**: Scheduling and policy decisions (how it runs at runtime)

3. **Conversions** are separated from transforms. Cross-dialect boundary passes live
   in `Conversion/` subdirectories. The dialect that PRODUCES ops owns the conversion.

4. **Internal sub-grouping** within each category by semantic role. Sub-directories
   are created when a group has 2+ files now or a clear growth path.

5. **Tests mirror source structure** via subdirectories matching the source sub-groups.

---

## Structure

```
lib/arts/dialect/sde/
│
├── IR/                                     ── Dialect definition (unchanged)
│   ├── SdeDialect.cpp
│   └── SdeOps.cpp
│
├── Analysis/                               ── Pure analysis (unchanged)
│   └── StructuredOpAnalysis.cpp             ── classification + access pattern engine
│
├── Conversion/                             ── Cross-dialect boundaries [NEW directory]
│   └── OmpToSde/
│       └── ConvertOpenMPToSde.cpp           [MOVE from Transforms/]
│
├── Verify/                                 ── Verification barriers [NEW directory]
│   └── VerifySdeLowered.cpp                 [MOVE from Transforms/]
│
├── Transforms/
│   │
│   ├── state/                              ── IR form/representation changes
│   │   │
│   │   ├── raising/                         ── carrier materialization [NEW sub-group]
│   │   │   ├── RaiseToLinalg.cpp            ── memref body → linalg carrier + classification stamp
│   │   │   ├── RaiseToTensor.cpp            ── memref carrier → tensor carrier
│   │   │   └── RaiseMemrefToTensor.cpp      ── memref-level tensor raising
│   │   │
│   │   ├── codelet/                         ── codelet formation [NEW sub-group]
│   │   │   ├── ConvertToCodelet.cpp         ── cu_region iter_args → mu_data + mu_token + cu_codelet
│   │   │   └── ScalarForwarding.cpp         ── forward scalars into codelet bodies
│   │   │
│   │   └── StructuredSummaries.cpp          ── materialize analysis as IR attributes
│   │                                           (neighborhood, footprint, vectorize hints)
│   │
│   ├── dep/                                ── Structural transforms (computation shape)
│   │   │
│   │   ├── loop/                            ── loop-level transforms [NEW sub-group]
│   │   │   ├── LoopInterchange.cpp          ── dimension reordering via carrier maps
│   │   │   ├── TensorOpt.cpp               ── cost-model-driven tiling
│   │   │   └── IterationSpaceDecomposition.cpp  ── stencil boundary peeling
│   │   │
│   │   └── fusion/                          ── operation merging [NEW sub-group]
│   │       └── ElementwiseFusion.cpp        ── fuse adjacent elementwise su_iterate ops
│   │
│   └── effect/                             ── Scheduling/policy decisions
│       │
│       ├── scheduling/                      ── execution policy [NEW sub-group]
│       │   ├── ScopeSelection.cpp           ── local vs distributed scope on cu_region
│       │   ├── ScheduleRefinement.cpp       ── auto/runtime → static/dynamic/guided
│       │   ├── ChunkOpt.cpp                 ── compute chunk size from cost model
│       │   └── ReductionStrategy.cpp        ── atomic/tree/local_accumulate strategy
│       │
│       └── distribution/                    ── work placement [NEW sub-group]
│           ├── DistributionPlanning.cpp     ── blocked/owner_compute/cyclic advisory
│           └── BarrierElimination.cpp       ── eliminate safe barriers + nowait inference
│
└── test/                                   ── Co-located tests (sub-grouped)
    │
    ├── analysis/                            ── StructuredOpAnalysis tests
    │   ├── structured_analysis_imperfect_nest.mlir
    │   ├── structured_analysis_stencil_with_boundary_guard.mlir
    │   └── structured_analysis_stencil_with_invariant_prefix.mlir
    │
    ├── conversion/                          ── OmpToSde boundary tests
    │   ├── openmp_to_arts_converts_task_depend_modes_to_sde.mlir
    │   ├── openmp_to_arts_drops_arts_metadata_at_sde_boundary.mlir
    │   ├── openmp_to_arts_infers_bitwise_and_reduction.mlir
    │   ├── openmp_to_arts_preserves_schedule_chunk.mlir
    │   ├── openmp_to_arts_single_tensor_threading_contract.mlir
    │   └── record_dep_duplicate_flag_verifier.mlir
    │
    ├── raising/                             ── RaiseToLinalg, RaiseToTensor tests
    │   ├── openmp_to_arts_raise_to_linalg_*.mlir       (4 files)
    │   ├── openmp_to_arts_raise_to_tensor_*.mlir       (9 files)
    │   ├── raise_memref_dimensionality_*.mlir           (3 files)
    │   └── omp_region_helper_inlining_raise_memref.mlir
    │
    ├── loop/                                ── LoopInterchange, TensorOpt, IterSpaceDecomp tests
    │   ├── sde_loop_interchange_*.mlir                 (4 files)
    │   ├── sde_tensor_optimization_tiles_*.mlir        (5 files)
    │   ├── openmp_to_arts_tensor_optimization_*.mlir   (8 files)
    │   └── sde_iteration_space_decomposition_*.mlir    (1 file)
    │
    ├── fusion/                              ── ElementwiseFusion tests
    │   ├── openmp_to_arts_sde_elementwise_fusion.mlir
    │   ├── sde_elementwise_fusion_rejects_mismatched_bounds.mlir
    │   └── sde_elementwise_fusion_rejects_overlapping_writes.mlir
    │
    ├── scheduling/                          ── ScopeSelection, Schedule, Chunk, Reduction tests
    │   ├── sde_scope_selection_*.mlir                  (6 files)
    │   ├── openmp_to_arts_refines_*_schedule.mlir      (4 files)
    │   ├── openmp_to_arts_synthesizes_*_chunk*.mlir    (5 files)
    │   ├── sde_reduction_strategy_*.mlir               (4 files)
    │   ├── openmp_to_arts_selects_*_reduction*.mlir    (3 files)
    │   └── openmp_to_arts_avoids_atomic_for_mul_reduction_strategy.mlir
    │
    ├── distribution/                        ── DistributionPlanning, BarrierElimination tests
    │   ├── sde_distribution_planning_*.mlir            (2 files)
    │   ├── openmp_to_arts_sde_distribution_planning_*.mlir  (4 files)
    │   └── sde_barrier_*.mlir                          (4 files)
    │
    ├── codelet/                             ── ConvertToCodelet + codelet ops tests
    │   ├── ops-codelet.mlir
    │   ├── ops-codelet-invalid.mlir
    │   └── ops-codelet-rank0.mlir
    │
    ├── summaries/                           ── StructuredSummaries + derived attrs tests
    │   ├── openmp_to_arts_sde_structured_summaries_stencil.mlir
    │   ├── sde_dependency_distance_stencil.mlir
    │   └── sde_vectorization_hints_elementwise.mlir
    │
    ├── lowering/                            ── SdeToArts boundary tests
    │   ├── sde_to_arts_erases_dead_tensor_carrier_chain.mlir
    │   ├── sde_to_arts_lowers_su_distribute_cyclic.mlir
    │   ├── sde_to_arts_preserves_fallback_stencil_contract.mlir
    │   └── sde_typed_tokens_and_annotations_lower_cleanly.mlir
    │
    ├── misc/                                ── Cross-cutting or uncategorized
    │   ├── inliner_multiblock_omp_skip.mlir
    │   ├── scalar_libm_call_skips_loop_hints.mlir
    │   └── small_trip_loop_gets_full_unroll_hint.mlir
    │
    └── Inputs/                              ── Shared test inputs (unchanged)
        ├── codelet-invalid-v7.mlir
        └── codelet-invalid-v10.mlir
```

---

## Pipeline Flow Through the Structure

The proposed pipeline (optimize first, schedule later) maps cleanly to the
directory structure:

```
                    ┌─────────────────┐
                    │   Conversion/   │
Phase 1: CONVERT    │   OmpToSde/     │  ConvertOpenMPToSde
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │  state/raising/  │
Phase 2: RAISE      │  RaiseToLinalg   │  classification + memref carrier
                    │  RaiseToTensor   │  tensor carrier
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │    dep/loop/    │
Phase 3: TRANSFORM  │  LoopInterchange │  dimension reordering
                    │  TensorOpt      │  cost-model tiling
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
Phase 4: ANALYZE    │ state/Summaries │  StructuredSummaries (materialize attrs)
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
Phase 5: FUSE       │  dep/fusion/    │  ElementwiseFusion
                    └────────┬────────┘
                             │
                    ┌────────▼──────────┐
                    │effect/scheduling/ │
Phase 6: SCHEDULE   │  ScopeSelection   │  on POST-optimization trip counts
                    │  ScheduleRefine   │  on POST-optimization trip counts
                    │  ChunkOpt         │  on POST-optimization trip counts
                    │  ReductionStrategy│  sees POST-tiling nested loops
                    └────────┬──────────┘
                             │
                    ┌────────▼──────────────┐
                    │ effect/distribution/  │
Phase 7: DISTRIBUTE │  DistributionPlanning  │  reads scope + classification
                    │  BarrierElimination    │  reads memref access sets
                    │  dep/loop/IterSpace*   │  boundary peeling (no deps)
                    └────────┬──────────────┘
                             │
                    ┌────────▼────────┐
                    │ state/codelet/  │
Phase 8: CODELET    │ ConvertToCodelet │  cu_region → mu_data + mu_token + cu_codelet
                    │ ScalarForwarding │  forward scalars into codelet bodies
                    │ (future body opts)│
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │core/Conversion/ │
Phase 9: LOWER      │  SdeToArts/     │  SDE → ARTS + carrier erasure (stays in core/)
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
Phase 10: VERIFY    │    Verify/      │  VerifySdeLowered
                    └─────────────────┘
```

---

## What Moves Where

### Source File Moves

| File | From | To | Reason |
|------|------|----|--------|
| ConvertOpenMPToSde.cpp | Transforms/ | Conversion/OmpToSde/ | Cross-dialect conversion, not a transform |
| VerifySdeLowered.cpp | Transforms/ | Verify/ | Verification barrier, not a transform |
| RaiseToLinalg.cpp | state/ | state/raising/ | Sub-group: carrier materialization |
| RaiseToTensor.cpp | state/ | state/raising/ | Sub-group: carrier materialization |
| RaiseMemrefToTensor.cpp | state/ | state/raising/ | Sub-group: carrier materialization |
| ConvertToCodelet.cpp | state/ | state/codelet/ | Sub-group: codelet formation |
| ScalarForwarding.cpp | state/ | state/codelet/ | Sub-group: codelet formation |
| StructuredSummaries.cpp | state/ | state/ (stays) | Top-level state — analysis materialization |
| LoopInterchange.cpp | dep/ | dep/loop/ | Sub-group: loop transforms |
| TensorOpt.cpp | dep/ | dep/loop/ | Sub-group: loop transforms |
| IterationSpaceDecomposition.cpp | dep/ | dep/loop/ | Sub-group: loop transforms |
| ElementwiseFusion.cpp | dep/ | dep/fusion/ | Sub-group: operation merging |
| ScopeSelection.cpp | effect/ | effect/scheduling/ | Sub-group: execution policy |
| ScheduleRefinement.cpp | effect/ | effect/scheduling/ | Sub-group: execution policy |
| ChunkOpt.cpp | effect/ | effect/scheduling/ | Sub-group: execution policy |
| ReductionStrategy.cpp | effect/ | effect/scheduling/ | Sub-group: execution policy |
| DistributionPlanning.cpp | effect/ | effect/distribution/ | Sub-group: work placement |
| BarrierElimination.cpp | effect/ | effect/distribution/ | Sub-group: work placement |

### No Moves (stay in place)

| File | Location | Reason |
|------|----------|--------|
| SdeDialect.cpp, SdeOps.cpp | IR/ | Dialect definition, correct |
| StructuredOpAnalysis.cpp | Analysis/ | Pure analysis, correct |
| SdeToArtsPatterns.cpp | core/Conversion/SdeToArts/ | Creates core ops, owned by core |

### Include Structure

```
include/arts/dialect/sde/
├── IR/
│   ├── SdeDialect.h
│   ├── SdeOps.td
│   └── SdeTypes.td
├── Analysis/
│   └── StructuredOpAnalysis.h
└── Transforms/
    ├── Passes.td              ── umbrella (includes phase sub-files)
    ├── StatePasses.td         ── state pass declarations
    ├── DepPasses.td           ── dep pass declarations
    ├── EffectPasses.td        ── effect pass declarations
    ├── ConversionPasses.td    ── conversion pass declarations
    ├── VerifyPasses.td        ── verification pass declarations
    └── Passes.h               ── ALL pass create functions + shared helpers
```

Passes.td is an umbrella that includes 5 phase-grouped sub-files. Passes.h
remains a single header — consumers include one header, get all passes.

---

## Future Growth Paths

The sub-grouping anticipates these additions (from sde-optimization-opportunities.md):

### state/raising/ (carrier expansion)
- `StencilRaising.cpp` — raise stencils to linalg carriers via tensor.pad

### state/codelet/ (codelet body optimizations)
- `TensorCleanup.cpp` — 14 upstream tensor cleanup patterns
- `CodeletVectorization.cpp` — linalg::vectorize() inside codelet bodies
- `CodeletBufferization.cpp` — one-shot bufferize at codelet boundary

### dep/loop/ (tiling improvements)
- `TileAndFuse.cpp` — upstream TilingInterface+CustomOp replaces TensorOpt manual tiling
- `StencilTiling.cpp` — diamond tiling for time-iterated stencils

### dep/fusion/ (inter-codelet merging)
- `CodeletFusion.cpp` — merge producer/consumer codelets
- `CodeletSplitting.cpp` — split large codelets for parallelism

### effect/scheduling/ (extended policy)
- `TokenModeRefinement.cpp` — downgrade readwrite → read/write

### effect/distribution/ (extended placement)
- `CriticalPathPriority.cpp` — priority hints from codelet DAG analysis
- `DoubleBuffering.cpp` — ping-pong buffers for iterative stencils

---

## Category Definitions

### state/ — IR Form Changes

Passes that change the IR representation without changing the computation's
semantics or execution model. They answer: "What does the IR look like?"

- **raising/**: Materialize structured carriers from scalar/memref loop bodies.
  Input: memref ops. Output: linalg.generic carriers.
- **codelet/**: Form the codelet abstraction from cu_region structure.
  Input: cu_region with iter_args. Output: mu_data + mu_token + cu_codelet.
- **StructuredSummaries**: Materialize analysis results as IR attributes so
  downstream passes can read them without re-running analysis.

### dep/ — Structural Transforms

Passes that change the shape of the computation based on data dependency analysis.
They answer: "What computes what?"

- **loop/**: Transform loop structure — reorder dimensions (interchange),
  strip-mine into tiles (TensorOpt), decompose iteration space into
  interior/boundary regions (IterationSpaceDecomposition).
- **fusion/**: Merge operations that are data-compatible — fuse adjacent
  elementwise loops into a single pipelined loop.

### effect/ — Scheduling Decisions

Passes that make policy decisions about how the computation executes at runtime,
without changing its structure. They answer: "How does it run?"

- **scheduling/**: Per-loop execution policy — parallelism scope (single vs
  parallel), scheduling algorithm (static/dynamic/guided), chunk granularity,
  reduction strategy (atomic/tree/local_accumulate).
- **distribution/**: Cross-worker placement — distribution kind
  (blocked/owner_compute/cyclic), barrier elimination (remove provably-safe
  synchronization), nowait inference.

---

## Relationship to SdeToArts (core/Conversion/)

`SdeToArtsPatterns.cpp` stays in `core/Conversion/SdeToArts/` because it creates
ARTS core ops (ForOp, EdtOp, EpochOp, etc.). In the IREE ownership model, the
dialect that PRODUCES ops owns the conversion.

However, the pass CONSUMES attributes from every SDE sub-group:

| SDE Sub-group | Attributes Consumed by SdeToArts |
|---|---|
| state/raising/ | `structured_classification` (route 2 classifyFromLinalg fallback) |
| state/StructuredSummaries | `accessMinOffsets`, `accessMaxOffsets`, `ownerDims`, `spatialDims`, `writeFootprint` (route 1) |
| dep/fusion/ | `classification=elementwise_pipeline` (route mapping) |
| effect/scheduling/ | `schedule`, `chunkSize`, `reduction_strategy`, `reduction_kinds` |
| effect/distribution/ | `su_distribute` wrapper ops, `barrier_eliminated` markers, `nowait` |

This cross-cutting consumption is expected — the conversion boundary is where all
SDE decisions are materialized into ARTS contracts.

---

## Build System Impact

### CMakeLists.txt Changes

The `lib/arts/dialect/sde/Transforms/CMakeLists.txt` currently lists source files
flat. After reorganization, it should use `GLOB` or explicit subdirectory listing:

```cmake
# lib/arts/dialect/sde/CMakeLists.txt
add_subdirectory(Analysis)
add_subdirectory(Conversion)
add_subdirectory(IR)
add_subdirectory(Transforms)
add_subdirectory(Verify)
```

```cmake
# lib/arts/dialect/sde/Transforms/CMakeLists.txt
add_mlir_dialect_library(ArtsSdeTransforms
  # state/
  state/raising/RaiseToLinalg.cpp
  state/raising/RaiseToTensor.cpp
  state/raising/RaiseMemrefToTensor.cpp
  state/codelet/ConvertToCodelet.cpp
  state/codelet/ScalarForwarding.cpp
  state/StructuredSummaries.cpp

  # dep/
  dep/loop/LoopInterchange.cpp
  dep/loop/TensorOpt.cpp
  dep/loop/IterationSpaceDecomposition.cpp
  dep/fusion/ElementwiseFusion.cpp

  # effect/
  effect/scheduling/ScopeSelection.cpp
  effect/scheduling/ScheduleRefinement.cpp
  effect/scheduling/ChunkOpt.cpp
  effect/scheduling/ReductionStrategy.cpp
  effect/distribution/DistributionPlanning.cpp
  effect/distribution/BarrierElimination.cpp

  DEPENDS
  ArtsSdeOpsIncGen
  ArtsSdeTransformsPassIncGen
)
```

```cmake
# lib/arts/dialect/sde/Conversion/CMakeLists.txt (NEW)
add_mlir_dialect_library(ArtsSdeConversion
  OmpToSde/ConvertOpenMPToSde.cpp

  DEPENDS
  ArtsSdeOpsIncGen
)
```

### Include Paths

No include path changes needed. All passes continue to include:
```cpp
#include "arts/dialect/sde/Transforms/Passes.h"
```

The `#include` path is the same regardless of which `state/raising/` or
`effect/scheduling/` subdirectory the source file lives in.
