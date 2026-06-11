# CARTS Compiler Pipeline

This document mirrors the pipeline order defined in `tools/compile/Compile.cpp`.
If this file disagrees with the compiler source or `dekk carts pipeline --json`,
the live compiler wins.

For per-dialect documentation (analysis, optimizations, READMEs), see
[`dialects/sde/`](./dialects/sde/), [`dialects/arts/`](./dialects/arts/),
and [`dialects/arts-rt/`](./dialects/arts-rt/). For the target
`sde -> arts -> arts-rt` split, see
[`dialect-layering.md`](./dialect-layering.md).

Planning notes and experiment records live under `.carts/sessions/...`; this
file stays limited to the live compiler pipeline.

## CLI Introspection

- `dekk carts pipeline`: show pipeline order and pass counts.
- `dekk carts pipeline --json`: print the machine-readable manifest.
- `dekk carts compile <file> --all-pipelines`: dump stage and pass outputs for
  a focused input.
- `carts-compile <file> --pass-pipeline='builtin.module(...)'`: run a focused
  MLIR pass pipeline for registration and boundary smoke tests. This bypasses
  the staged CARTS pipeline and should not be used as normal project entrypoint
  work; use `dekk carts ...` commands for regular compilation and testing.

The JSON manifest includes both executable pipeline steps and dialect grouping
metadata. `dialect_groups.canonical` names the groups implemented by the live
stages. The compiler now runs through `sde -> arts -> arts-rt`: SDE performs
source/layout transformations, `sde-to-arts` mechanically materializes
committed storage/access/scheduling facts as ARTS objects, ARTS stages refine
the object graph, and ARTS-RT lowers the chosen graph to runtime ABI shape.
These group records are descriptive; only `pipeline`, `start_from`, and
`pipeline_sequence` list canonical stage tokens.

## Pipeline Order

Driver stages:

1. `sde-input-normalization`
2. `initial-cleanup`
3. `sde-planning`
4. `sde-to-arts`
5. `edt-dep-realization`
6. `edt-local-cleanup`
7. `create-dbs`
8. `db-opt`
9. `post-db-refinement`
10. `late-concurrency-cleanup`
11. `epochs`
12. `pre-lowering`
13. `arts-rt-to-llvm`

`--pipeline` also accepts the sentinel `complete`. `--start-from` accepts
canonical stages only.

Conditional epilogues:

- `post-o3-opt`: runs when `--O3` is active.
- `llvm-ir-emission`: runs when LLVM IR emission is requested.

## Stage Summaries

### `sde-input-normalization`

```text
PromoteTargetAttrs
LowerAffine(func)
CSE
SdeInputInliner
PolygeistCanonicalize
ScalarForwarding
PolygeistCanonicalize
SdeMemrefNormalization
SdeHandleDeps
SdeDeadStateCleanup
CSE
```

### `initial-cleanup`

```text
LowerAffine(func)
CSE(func)
PolygeistCanonicalizeFor(func)
```

### `sde-planning`

This stage covers OpenMP-to-SDE conversion and SDE-owned rewrites. It
intentionally stops before ARTS object conversion; `sde-to-arts` owns the
mechanical SDE-to-ARTS boundary.

```text
ConvertOpenMPToSde
SdeCuNormalization
Parallelize
PatternAnalysis
LayoutAssignment
LoopInterchange
Tiling
ElementwiseFusion
ScheduleRefinement
ChunkOpt
ReductionStrategy
DistributionPlanning
IterationSpaceDecomposition
BarrierElimination
MemoryUnitMaterialization
SdeCuNormalization
VerifySdePhysicalConsistency
SdeRankExpandMu
SdeScalarBlockReduction
VerifySdeMuLayout
RaiseToMuAccessWindow
VerifySdeMuAccessWindow
MuAccessWindowSyncOpt
VerifySdeMuAccessWindowSync
SdeRedistribute
VerifySdeRedistribute
SdeCoarseAvoidance
VerifySdeCoarseAvoidance
VerifySde
```

### `sde-to-arts`

This stage mechanically lowers committed SDE storage, access windows,
scheduling, and control facts directly into ARTS objects. It also rejects any
residual SDE operation after finalization.

```text
SdeStorageToArtsDb
SdeAccessesToArtsDeps
FinalizeSdeToArts
VerifyArtsObjectsOnly
```

### `edt-dep-realization`

```text
RealizeEdtDistributionPlan
VerifySdeLowered
VerifyArtsObjectsOnly
```

### `edt-local-cleanup`

```text
EdtAllocaSinking
EdtInlineNoDepTasks
ArtsDeadCodeElimination
SymbolDCE
CSE(arts.edt)
EdtPtrRematerialization
```

### `create-dbs`

```text
CreateDbs
PolygeistCanonicalize
CSE(arts.edt)
SymbolDCE
Mem2Reg
PolygeistCanonicalize
```

### `db-opt`

```text
DbModeTightening
PolygeistCanonicalize
CSE(arts.edt)
Mem2Reg
```

### `post-db-refinement`

```text
DbModeTightening
DbOwnerMapRealization
EdtDeadDepElimination
DbConsolidateStencilHalos
DbStorageBridgeCopyPlacement
DbShortenLifetimes
DbDeadRootElimination
PartialReductionSplit
BlockContractionSplit
DbScratchElimination
PolygeistCanonicalize
CSE(arts.edt)
DistributedLaunchConsistency
```

### `late-concurrency-cleanup`

```text
Hoisting
PolygeistCanonicalize
CSE(arts.edt)
EdtAllocaSinking
ArtsDeadCodeElimination
Mem2Reg
```

### `epochs`

```text
PolygeistCanonicalize
CreateEpochs
EpochAmortizeRepeatedLoop
EpochTailContinuation
PolygeistCanonicalize
DbCommitDistributedDeps (conditional)
VerifyArtsCdag
```

### `pre-lowering`

This stage is implemented by ARTS-RT lowering code even though it still consumes
the abstract ARTS object graph.

```text
EdtAllocaSinking
PolygeistCanonicalize
CSE(arts.edt)
DbLowering
PolygeistCanonicalize
CSE(arts.edt)
EdtLowering
PolygeistCanonicalize
CSE
VerifyEdtLowered
LICM
DataPtrHoisting
PolygeistCanonicalize
CSE
ScalarReplacement
PolygeistCanonicalize
CSE
EpochLowering
PolygeistCanonicalize
CSE
VerifyEpochLowered
VerifyPreLowered
```

### `arts-rt-to-llvm`

This is the ARTS-RT-owned runtime ABI and LLVM-facing lowering stage. The
canonical manifest token is `arts-rt-to-llvm`; no legacy stage alias is
accepted.

```text
LowerAffine(func)
ConvertArtsRtToLLVM
GuidRangeCallOpt
RuntimeCallOpt
DataPtrHoisting
PolygeistCanonicalize
CSE
Mem2Reg
PolygeistCanonicalize
ControlFlowSink
PolygeistCanonicalize
VerifyDbLowered
VerifyLowered
```

## Stage Dependencies

- `initial-cleanup` depends on `sde-input-normalization`.
- `sde-planning` depends on `initial-cleanup`.
- `sde-to-arts` depends on `sde-planning`.
- `edt-dep-realization` depends on `sde-to-arts`.
- `edt-local-cleanup` depends on `edt-dep-realization`.
- `create-dbs` depends on `edt-dep-realization`.
- `db-opt` depends on `create-dbs`.
- `post-db-refinement` depends on `create-dbs`.
- `late-concurrency-cleanup` depends on `post-db-refinement`.
- `epochs` depends on `post-db-refinement`.
- `pre-lowering` depends on `epochs` and `late-concurrency-cleanup`.
- `arts-rt-to-llvm` depends on `pre-lowering`.

## Ownership Notes

- SDE inside `sde-planning` owns semantic decomposition, `PatternAnalysis`,
  state rewrites, dependency/effect proofs, sync rewrites, and physical
  MU/access-window layout policy.
- `sde-to-arts` owns mechanical materialization of committed SDE MU/CU/SU facts
  into ARTS DB/acquire/EDT objects.
- ARTS owns explicit deps, params, token-local views, DB/EDT/epoch
  orchestration, owner maps, grouped execution, and focused
  realization/refinement passes over already emitted ARTS facts (source:
  `lib/carts/dialect/arts/`).
- `CreateDbs` consumes ARTS facts; it must not choose blocked/tiled raw-memref
  storage policy that SDE failed to make real.
- `arts-rt` lowering belongs in `pre-lowering` and `arts-rt-to-llvm`, after the
  compiler has already chosen the DB and task shape (source:
  `lib/carts/dialect/arts-rt/`).
