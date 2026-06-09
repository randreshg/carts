# CARTS Compiler Pipeline

This document mirrors the pipeline order defined in `tools/compile/Compile.cpp`.
If this file disagrees with the compiler source or `dekk carts pipeline --json`,
the live compiler wins.

For per-dialect documentation (analysis, optimizations, READMEs), see
[`dialects/sde/`](./dialects/sde/), [`dialects/codir/`](./dialects/codir/),
[`dialects/arts/`](./dialects/arts/), and
[`dialects/arts-rt/`](./dialects/arts-rt/). For the target
`sde -> codir -> arts -> arts-rt` split, see
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
metadata. `dialect_groups.current` names the groups implemented by the live
stages. The codelet path now runs through `sde -> codir -> arts`: SDE performs
source/layout transformations, `sde-to-codir` mechanically isolates codelets,
CODIR transforms the isolated graph, and `codir-to-arts` mechanically creates
ARTS objects. These group records are descriptive; only `pipeline`,
`start_from`, and `pipeline_sequence` list canonical stage tokens.

## Pipeline Order

Driver stages:

1. `sde-input-normalization`
2. `initial-cleanup`
3. `sde-planning`
4. `sde-to-codir`
5. `codir-graph-transforms`
6. `codir-to-arts`
7. `edt-dep-realization`
8. `edt-local-cleanup`
9. `create-dbs`
10. `db-opt`
11. `post-db-refinement`
12. `late-concurrency-cleanup`
13. `epochs`
14. `pre-lowering`
15. `arts-rt-to-llvm`

`--pipeline` also accepts the sentinel `complete`. `--start-from` accepts core
stages only.

Conditional epilogues:

- `post-o3-opt`: runs when `--O3` is active.
- `llvm-ir-emission`: runs when LLVM IR emission is requested.

## Stage Summaries

### `sde-input-normalization`

```text
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
intentionally stops before codelet materialization; `sde-to-codir` owns the
mechanical codelet boundary and `codir-to-arts` owns mechanical ARTS object
materialization.

```text
ConvertOpenMPToSde
PatternAnalysis
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
SdeRankExpandMu
RaiseToMuAccessWindow
MuAccessWindowSyncOpt
SdeRedistribute
SdeCoarseAvoidance
VerifySde
```

### `sde-to-codir`

This stage only performs the mechanical SDE-to-CODIR conversion. CODIR graph,
reduction, and storage transforms run in `codir-graph-transforms`.

```text
ConvertSdeToCodir
```

### `codir-graph-transforms`

```text
CodirCodeletDCE
ReductionDepMapping
ReductionAtomicMaterialization
DepStorageAssignment
VerifyCodir
```

### `codir-to-arts`

This stage only performs the mechanical CODIR-to-ARTS conversion. ARTS EDT dep
realization and boundary checks run in `edt-dep-realization`.

```text
ConvertSdeBoundaryToArts
ConvertCodirToArts
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
DbShortenLifetimes
DbDeadRootElimination
PartialReductionSplitMaterialization
MatmulContractionMaterialization
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
- `sde-to-codir` depends on `sde-planning`.
- `codir-graph-transforms` depends on `sde-to-codir`.
- `codir-to-arts` depends on `codir-graph-transforms`.
- `edt-dep-realization` depends on `codir-to-arts`.
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
  state rewrites, dependency/effect proofs, sync rewrites, and physical DB
  layout policy.
- CODIR is the active isolated-codelet layer. It owns explicit deps, params,
  token-local views, graph/reduction/storage transforms, and codelet capture
  verification before ARTS EDT creation.
- `CreateDbs` is now only a coarse raw-memref bridge. It rejects blocked/tiled
  raw memrefs because SDE/CODIR must perform MU/token storage and access
  rewrites before ARTS.
- `arts` owns DB/EDT/epoch orchestration and focused realization/refinement
  passes over already emitted ARTS facts (source: `lib/carts/dialect/arts/`).
- `arts-rt` lowering belongs in `pre-lowering` and `arts-rt-to-llvm`, after the
  compiler has already chosen the DB and task shape (source:
  `lib/carts/dialect/arts-rt/`).
