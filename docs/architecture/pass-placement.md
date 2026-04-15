# Current Pass Placement

This map reflects the pass placement used by the current branch. It replaces
the older staged plan that assumed a `collect-metadata` front-end stage and a
separate one-shot bufferization stage inside the SDE pipeline.

## Directory Ownership

| Location | Current role |
|---|---|
| `lib/arts/dialect/sde/Transforms/` | SDE boundary construction and SDE-owned optimization passes |
| `lib/arts/dialect/core/Conversion/SdeToArts/` | SDE-to-ARTS conversion boundary |
| `lib/arts/dialect/core/Transforms/` | ARTS structural transforms, DB creation/refinement, distribution, and core pre-lowering cleanup |
| `lib/arts/dialect/core/Conversion/ArtsToRt/` | Core-to-runtime lowering (`EdtLowering`, `EpochLowering`) |
| `lib/arts/dialect/core/Conversion/ArtsToLLVM/` | Final ARTS/ARTS-RT to LLVM conversion pass |
| `lib/arts/dialect/rt/Transforms/` | Runtime-side cleanup and call optimization after lowering begins |
| shared MLIR / utility passes | `LowerAffine`, `CSE`, `DCE`, `SymbolDCE`, `LICM`, `Mem2Reg`, `ScalarReplacement`, `PolygeistCanonicalize`, and related utility passes |
| verify passes | Stage-boundary validation such as `VerifySdeLowered`, `VerifyEdtCreated`, `VerifyForLowered`, `VerifyPreLowered`, `VerifyDbLowered`, and `VerifyLowered` |

## SDE Boundary Placement

The SDE pipeline is not split across multiple named stages. The active branch
uses one `openmp-to-arts` stage, and the pass placement inside it is:

```text
lib/arts/dialect/sde/Transforms/
  ConvertOpenMPToSde
  ScopeSelection
  ScheduleRefinement
  ChunkOpt
  ReductionStrategy
  RaiseToLinalg
  RaiseToTensor
  LoopInterchange
  TensorOpt
  StructuredSummaries
  ElementwiseFusion
  DistributionPlanning
  IterationSpaceDecomposition
  BarrierElimination
  VerifySdeLowered

lib/arts/dialect/core/Conversion/SdeToArts/
  ConvertSdeToArts

lib/arts/dialect/core/Transforms/
  VerifyEdtCreated

shared / utility
  DeadCodeElimination
  CSE
```

Important corrections relative to older docs:

- There is no scheduled `collect-metadata` stage before SDE conversion.
- There is no discrete one-shot bufferize stage between `RaiseToTensor` and
  `ConvertSdeToArts`.
- `ConvertOpenMPToArts` in `core/Conversion/OmpToArts/` is not the active path
  for this branch's OpenMP pipeline; the active path starts in
  `sde/Transforms/ConvertOpenMPToSde.cpp`.

## Stage-by-Stage Placement

The lists below group passes by owning area within each stage. For exact
execution order, `tools/compile/Compile.cpp` remains the source of truth.

### `raise-memref-dimensionality`

```text
shared / utility:
  LowerAffine(func)
  CSE
  ArtsInliner
  PolygeistCanonicalize
  RaiseMemRefDimensionality
  HandleDeps
  DeadCodeElimination
  CSE
```

### `initial-cleanup`

```text
shared / utility:
  LowerAffine(func)
  CSE(func)
  PolygeistCanonicalizeFor(func)
```

### `openmp-to-arts`

```text
sde/Transforms:
  ConvertOpenMPToSde
  ScopeSelection
  ScheduleRefinement
  ChunkOpt
  ReductionStrategy
  RaiseToLinalg
  RaiseToTensor
  LoopInterchange
  TensorOpt
  StructuredSummaries
  ElementwiseFusion
  DistributionPlanning
  IterationSpaceDecomposition
  BarrierElimination

core/Conversion/SdeToArts:
  ConvertSdeToArts
  VerifySdeLowered

core/Transforms:
  VerifyEdtCreated

shared / utility:
  DeadCodeElimination
  CSE
```

Note: there is **no** `pattern-pipeline` stage. Earlier drafts listed passes
like `DepTransforms`, `LoopNormalization`, `LoopReordering`, and
`KernelTransforms` in a named stage between `openmp-to-arts` and
`edt-transforms`. The current `tools/compile/Compile.cpp` does not schedule
such a stage — semantic pattern work now lives inside SDE (during
`openmp-to-arts`).

### `edt-transforms`

```text
core/Transforms:
  EdtStructuralOpt(runAnalysis=false)
  EdtICM
  EdtPtrRematerialization

shared / utility:
  DeadCodeElimination
  SymbolDCE
  CSE
```

### `create-dbs`

```text
core/Transforms:
  DistributedHostLoopOutlining (conditional)
  CreateDbs

shared / utility:
  CSE (bridge cleanup, conditional)
  PolygeistCanonicalize
  CSE
  SymbolDCE
  Mem2Reg
  PolygeistCanonicalize
```

### `db-opt`

```text
core/Transforms:
  DbModeTightening

shared / utility:
  PolygeistCanonicalize
  CSE
  Mem2Reg
```

### `edt-opt`

```text
core/Transforms:
  EdtStructuralOpt(runAnalysis=true)
  LoopFusion

shared / utility:
  PolygeistCanonicalize
  CSE
```

### `concurrency`

```text
core/Transforms:
  Concurrency
  ForOpt

shared / utility:
  PolygeistCanonicalize
  PolygeistCanonicalize
```

### `edt-distribution`

```text
core/Transforms:
  StructuredKernelPlan
  EdtDistribution
  EdtOrchestrationOpt
  ForLowering
  VerifyForLowered
```

### `post-distribution-cleanup`

```text
core/Transforms:
  EdtStructuralOpt(runAnalysis=false)
  EdtStructuralOpt(runAnalysis=false)
  EpochOpt

shared / utility:
  DeadCodeElimination
  PolygeistCanonicalize
  CSE
  PolygeistCanonicalize
  CSE
```

### `db-partitioning`

```text
core/Transforms:
  DbPartitioning
  DbDistributedOwnership (conditional)
  DbTransforms
```

### `post-db-refinement`

```text
core/Transforms:
  DbModeTightening
  EdtTransforms
  DbTransforms
  DbScratchElimination

verify:
  ContractValidation

shared / utility:
  PolygeistCanonicalize
  CSE
```

### `late-concurrency-cleanup`

```text
core/Transforms:
  BlockLoopStripMining(func)
  Hoisting
  EdtAllocaSinking

shared / utility:
  PolygeistCanonicalize
  CSE
  DeadCodeElimination
  Mem2Reg
```

### `epochs`

```text
core/Transforms:
  CreateEpochs
  PersistentStructuredRegion
  EpochOpt[scheduling] (conditional)

shared / utility:
  PolygeistCanonicalize
  PolygeistCanonicalize
```

### `pre-lowering`

```text
core/Transforms:
  EdtAllocaSinking
  ParallelEdtLowering
  DbLowering
  EpochOpt[scheduling] (conditional)
  VerifyPreLowered

core/Conversion/ArtsToRt:
  EdtLowering
  EpochLowering

rt/Transforms:
  DataPtrHoisting

shared / utility:
  PolygeistCanonicalize
  CSE
  PolygeistCanonicalize
  CSE
  PolygeistCanonicalize
  CSE
  LICM
  PolygeistCanonicalize
  CSE
  ScalarReplacement
  PolygeistCanonicalize
  CSE
  PolygeistCanonicalize
  CSE
```

### `arts-to-llvm`

```text
core/Conversion/ArtsToLLVM:
  ConvertArtsToLLVM

core/Transforms:
  LoweringContractCleanup
  VerifyDbLowered

rt/Transforms:
  GuidRangCallOpt
  RuntimeCallOpt
  DataPtrHoisting
  VerifyLowered

shared / utility:
  PolygeistCanonicalize
  CSE
  Mem2Reg
  PolygeistCanonicalize
  ControlFlowSink
  PolygeistCanonicalize
```

## Notes on the Current Layout

- `RaiseToLinalg`, `RaiseToTensor`, and `TensorOpt` are real SDE
  passes on this branch. They are not documentation-only placeholders.
- The tensor/linalg window is transient and SDE-owned. `ConvertSdeToArts`
  consumes it directly instead of handing off to a separately scheduled
  bufferization stage.
- The historically named ARTS `pattern-pipeline` stage has been removed.
  Semantic pattern choice, semantic cost modeling, and
  schedule/distribution/reduction-strategy decisions now live inside SDE
  (during `openmp-to-arts`). The ARTS-core stages that follow (`edt-transforms`,
  `create-dbs`, ...) consume SDE-stamped contracts instead of rediscovering
  them.
- `ParallelEdtLowering`, `DbLowering`, and `LoweringContractCleanup` are core
  ownership passes even though they sit near the late lowering boundary.
