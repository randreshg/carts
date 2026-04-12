# Current Pipeline Layout

This document describes the pipeline that the current branch actually builds in
`tools/compile/Compile.cpp`. Earlier redesign notes in this file described a
more granular staged rollout with a separate `collect-metadata` phase and a
discrete tensor bufferization stage. That is not what the branch runs today.

## Current Stage Layout

The active compilation pipeline is:

```text
raise-memref-dimensionality
  -> initial-cleanup
  -> openmp-to-arts
  -> edt-transforms
  -> create-dbs
  -> db-opt
  -> edt-opt
  -> concurrency
  -> edt-distribution
  -> post-distribution-cleanup
  -> db-partitioning
  -> post-db-refinement
  -> late-concurrency-cleanup
  -> epochs
  -> pre-lowering
  -> arts-to-llvm
  -> complete-mlir   (epilogue, --O3 only)
  -> emit-llvm       (epilogue, --emit-llvm only)
```

The key redesign point is concentrated in `openmp-to-arts`: OpenMP lowers into
SDE, SDE-owned passes make scope/schedule/chunk/reduction and linalg/tensor
decisions there, and only then does `ConvertSdeToArts` cross into ARTS IR.

## The `openmp-to-arts` Stage

The current branch has one `openmp-to-arts` stage with 19 passes — the entire
SDE lifecycle lives inside it:

```text
ConvertOpenMPToSde
  -> ScopeSelection
  -> ScheduleRefinement
  -> ChunkOpt
  -> ReductionStrategy
  -> RaiseToLinalg
  -> RaiseToTensor
  -> LoopInterchange
  -> TensorOpt
  -> StructuredSummaries
  -> ElementwiseFusion
  -> DistributionPlanning
  -> IterationSpaceDecomposition
  -> BarrierElimination
  -> ConvertSdeToArts
  -> VerifySdeLowered
  -> DeadCodeElimination
  -> CSE
  -> VerifyEdtCreated
```

This is the SDE window. There is no separate front-end `collect-metadata`
stage before it.

## What the SDE Window Does

Within `openmp-to-arts`, the branch currently does the following:

- `ConvertOpenMPToSde` builds `arts_sde.*` regions, loops, tasks, deps, and
  reductions from OpenMP and supported SCF forms.
- `SdeScopeSelection`, `SdeScheduleRefinement`, `SdeChunkOptimization`, and
  `SdeReductionStrategy` stamp SDE-owned decisions before ARTS IR exists.
- `RaiseToLinalg` and `RaiseToTensor` build transient structured carriers
  inside SDE when the loop body matches the supported subsets.
- `SdeTensorOptimization` performs SDE-level tensor/linalg transforms on those
  transient carriers, guided by the active `SDECostModel`.
- `ConvertSdeToArts` consumes the SDE loop plus any transient carrier state,
  recovers contracts, lowers to ARTS IR, and erases the SDE-only carriers.

The original memref/loop body remains authoritative throughout the SDE phase.
The linalg/tensor carriers are transient analysis and transformation vehicles,
not a separate long-lived IR layer that survives beyond `ConvertSdeToArts`.

## No Discrete Bufferization Stage

The older redesign notes in this file described a separate stage that ran
one-shot bufferization and `linalg-to-loops` after tensor optimization. The
current branch does not schedule a discrete stage like that.

What is true today:

- `RaiseToTensor` introduces transient tensor-backed carriers inside SDE.
- `SdeTensorOptimization` rewrites those carriers while the surrounding SDE
  loop stays executable in memref/SCF form.
- `ConvertSdeToArts` consumes the transient carrier information and erases the
  carrier chain when crossing into ARTS IR.

So the branch has a real tensor/linalg optimization window in SDE, but not a
named pipeline stage that runs one-shot bufferize as a standalone boundary.

## Relationship to the Former ARTS Pattern Pipeline

There is **no `pattern-pipeline` stage** in the current pipeline. Earlier
redesign drafts placed `PatternDiscovery`, `DepTransforms`, `LoopNormalization`,
`LoopReordering`, and `KernelTransforms` in a named stage between
`openmp-to-arts` and `edt-transforms`. That stage has been removed:

- Semantic structure and optimization decisions now happen inside SDE (during
  `openmp-to-arts`) via the `Sde*` pass family.
- Contracts produced by those SDE passes are consumed later by ARTS-core
  stages (`edt-transforms`, `create-dbs`, `db-partitioning`, ...), which
  perform structural / runtime transforms — not a second semantic layer.

If you are looking at an older doc or skill that references `pattern-pipeline`
or `collect-metadata`, update it to point at stage 3 (`openmp-to-arts`) or
stage 4 (`edt-transforms`) as appropriate.

## Front-End Stages Before SDE

Before `openmp-to-arts`, the pipeline runs only two front-end stages:

```text
raise-memref-dimensionality:
  LowerAffine(func)
  -> CSE
  -> ArtsInliner
  -> PolygeistCanonicalize
  -> RaiseMemRefDimensionality
  -> HandleDeps
  -> DeadCodeElimination
  -> CSE

initial-cleanup:
  LowerAffine(func)
  -> CSE(func)
  -> PolygeistCanonicalizeFor(func)
```

There is no scheduled `collect-metadata` stage between these and
`openmp-to-arts`.

## Design Consequences

- The main optimization boundary moved up to SDE, not all the way down to
  ARTS.
- SDE decisions are made before `arts.for`, `arts.edt`, and DB structure are
  created.
- Verification at the SDE boundary is explicit: `VerifySdeLowered` and then
  `VerifyEdtCreated` close the `openmp-to-arts` stage.
- Any future redesign work should start from this single-stage SDE window,
  not from the older staged `collect-metadata -> raise -> bufferize` plan.
