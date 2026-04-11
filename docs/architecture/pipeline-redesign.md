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
  -> pattern-pipeline
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
  -> post-o3-opt
  -> llvm-ir-emission
```

The key redesign point is concentrated in `openmp-to-arts`: OpenMP lowers into
SDE, SDE-owned passes make scope/schedule/chunk/reduction and linalg/tensor
decisions there, and only then does `ConvertSdeToArts` cross into ARTS IR.

## The `openmp-to-arts` Stage

The current branch has one `openmp-to-arts` stage with 13 passes:

```text
ConvertOpenMPToSde
  -> SdeScopeSelection
  -> SdeScheduleRefinement
  -> SdeChunkOptimization
  -> SdeReductionStrategy
  -> RaiseToLinalg
  -> RaiseToTensor
  -> SdeTensorOptimization
  -> ConvertSdeToArts
  -> VerifySdeLowered
  -> DeadCodeElimination
  -> CSE
  -> VerifyEdtCreated
```

This is the current SDE window. There is no separate front-end
`collect-metadata` stage before it.

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

## Relationship to the ARTS Pattern Pipeline

Pattern refinement still exists after SDE lowering. The current
`pattern-pipeline` stage is:

```text
DepTransforms
  -> LoopNormalization
  -> StencilBoundaryPeeling
  -> LoopReordering
  -> KernelTransforms
  -> CSE
```

This means the branch now seeds early structural information in SDE, lowers to
ARTS, and then lets the existing ARTS pattern passes refine or consume those
contracts later. Pattern discovery is not a separate executable pass ahead of
SDE conversion.

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
