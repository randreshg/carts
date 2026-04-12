# CARTS Compiler Pipeline (Pipeline Order)

This document mirrors the actual `carts-compile` pipeline-step order defined in
`tools/compile/Compile.cpp` (`getStageRegistry()` and the `k*Passes` arrays).

Use it as the canonical checklist when auditing pass ordering, ownership, and
cross-step contracts. If this file disagrees with Compile.cpp, Compile.cpp
wins — open a PR to update this page.

For an example-driven view of how concrete loop/kernel families travel through
the pipeline, see [`optimization-patterns.md`](./optimization-patterns.md).

## CLI Introspection

- `dekk carts pipeline`: show pipeline order and pass counts.
- `dekk carts pipeline --pipeline=<step>`: show passes for one pipeline step.
- `carts-compile --print-pipeline-manifest-json`: print the machine-readable
  manifest consumed by the Python CLI.

## Pipeline Order (`--pipeline` / `--start-from`)

16 core stages (always run), followed by 2 epilogue stages (conditional):

1. `raise-memref-dimensionality`
2. `initial-cleanup`
3. `openmp-to-arts` *(entire SDE lifecycle lives inside this stage)*
4. `edt-transforms`
5. `create-dbs`
6. `db-opt`
7. `edt-opt`
8. `concurrency`
9. `edt-distribution`
10. `post-distribution-cleanup`
11. `db-partitioning`
12. `post-db-refinement`
13. `late-concurrency-cleanup`
14. `epochs`
15. `pre-lowering`
16. `arts-to-llvm`

Epilogue (conditional, token shown after stage name):
- `complete-mlir` — post-O3 function-level cleanup (only when `--O3`)
- `emit-llvm` — final MLIR-to-LLVM-IR emission (only when `--emit-llvm`)

## Pipeline Controls

- `--pipeline=<step>`: run up to and including the selected pipeline step.
- `--start-from=<step>`: skip all earlier pipeline steps and start at the
  selected pipeline step.
- `--all-pipelines` (`.mlir` input only): emits one `.mlir` per stage
  (`<stem>_<stage>.mlir`), the final `<stem>_complete.mlir`, `<stem>.ll`,
  and a `<stem>_passes/<NN>_<stage>/NNN_<PassClass>.mlir` hierarchy with
  one dump per executed pass. Nested pass managers (e.g. `initial-cleanup`
  running `LowerAffinePass` once per `func::FuncOp`) are captured as
  separate files. The fastest way to bisect a failing pipeline.
- Invalid ranges are rejected: `--start-from` cannot be later than
  `--pipeline`.

## Per-Step Pass Summary

### 1) raise-memref-dimensionality
1. `LowerAffine` (nested func)
2. `CSE`
3. `ArtsInliner`
4. `PolygeistCanonicalize`
5. `RaiseMemRefDimensionality`
6. `HandleDeps`
7. `DeadCodeElimination`
8. `CSE`

### 2) initial-cleanup
1. `LowerAffine` (nested func)
2. `CSE` (nested func)
3. `PolygeistCanonicalizeFor` (nested func)

### 3) openmp-to-arts
The complete SDE lifecycle (OMP → SDE → Core) runs inside this single stage.
All SDE passes live in `lib/arts/dialect/sde/Transforms/` under the
`mlir::arts::sde` namespace — the `Sde` prefix on class names has been
dropped since the dialect location already carries that information.

1. `ConvertOpenMPToSde`
2. `ScopeSelection`
3. `ScheduleRefinement`
4. `ChunkOpt`
5. `ReductionStrategy`
6. `RaiseToLinalg`
7. `RaiseToTensor`
8. `LoopInterchange`
9. `TensorOpt`
10. `StructuredSummaries`
11. `ElementwiseFusion`
12. `DistributionPlanning`
13. `IterationSpaceDecomposition`
14. `BarrierElimination`
15. `ConvertSdeToArts`
16. `VerifySdeLowered`
17. `DeadCodeElimination`
18. `CSE`
19. `VerifyEdtCreated`

### 4) edt-transforms
1. `EdtStructuralOpt(runAnalysis=false)`
2. `EdtICM`
3. `DeadCodeElimination`
4. `SymbolDCE`
5. `CSE`
6. `EdtPtrRematerialization`

### 5) create-dbs
1. `DistributedHostLoopOutlining` (conditional — multinode / `--distributed-db`)
2. `CreateDbs`
3. `CSE` (bridge cleanup, conditional)
4. `PolygeistCanonicalize`
5. `CSE`
6. `SymbolDCE`
7. `Mem2Reg`
8. `PolygeistCanonicalize`

*No structural verifier here.* The old `VerifyDbCreated` pass enforced
"any EDT ⇒ some DB must exist", which is too strict: `ConvertSdeToArts`
legitimately produces zero-memory EDTs for degenerate parallel regions
(samples whose body is only external calls / prints). Downstream stages
(`db-opt`, `db-partitioning`, `post-db-refinement`, `pre-lowering`) surface
real missing-DB bugs at their own level, so the structural smoke-test was
removed rather than relaxed.

### 6) db-opt
1. `DbModeTightening`
2. `PolygeistCanonicalize`
3. `CSE`
4. `Mem2Reg`

### 7) edt-opt
1. `PolygeistCanonicalize`
2. `EdtStructuralOpt(runAnalysis=true)`
3. `LoopFusion`
4. `CSE`

### 8) concurrency
1. `PolygeistCanonicalize`
2. `Concurrency`
3. `ForOpt`
4. `PolygeistCanonicalize`

### 9) edt-distribution
1. `EdtDistribution`
2. `EdtOrchestrationOpt`
3. `ForLowering`
4. `VerifyForLowered`

### 10) post-distribution-cleanup
1. `EdtStructuralOpt(runAnalysis=false)`
2. `DeadCodeElimination`
3. `PolygeistCanonicalize`
4. `CSE`
5. `EdtStructuralOpt(runAnalysis=false)` (second pass)
6. `EpochOpt`
7. `PolygeistCanonicalize`
8. `CSE`

### 11) db-partitioning
1. `DbPartitioning`
2. `DbDistributedOwnership` (conditional — `--distributed-db`)
3. `DbTransforms`

### 12) post-db-refinement
1. `DbModeTightening`
2. `EdtTransforms`
3. `DbTransforms`
4. `ContractValidation`
5. `DbScratchElimination`
6. `PolygeistCanonicalize`
7. `CSE`

### 13) late-concurrency-cleanup
1. `BlockLoopStripMining` (nested func)
2. `Hoisting`
3. `PolygeistCanonicalize`
4. `CSE`
5. `EdtAllocaSinking`
6. `DeadCodeElimination`
7. `Mem2Reg`

### 14) epochs
1. `PolygeistCanonicalize`
2. `CreateEpochs`
3. `EpochOpt[scheduling]` (conditional)
4. `PolygeistCanonicalize`

### 15) pre-lowering
1. `EdtAllocaSinking`
2. `ParallelEdtLowering`
3. `EpochOpt[scheduling]` (conditional)
4. `PolygeistCanonicalize`
5. `CSE`
6. `DbLowering`
7. `PolygeistCanonicalize`
8. `CSE`
9. `EdtLowering`
10. `PolygeistCanonicalize`
11. `CSE`
12. `LICM`
13. `DataPtrHoisting`
14. `PolygeistCanonicalize`
15. `CSE`
16. `ScalarReplacement`
17. `PolygeistCanonicalize`
18. `CSE`
19. `EpochLowering`
20. `PolygeistCanonicalize`
21. `CSE`
22. `VerifyPreLowered`

### 16) arts-to-llvm
1. `ConvertArtsToLLVM`
2. `LoweringContractCleanup`
3. `GuidRangCallOpt`
4. `RuntimeCallOpt`
5. `DataPtrHoisting`
6. `PolygeistCanonicalize`
7. `CSE`
8. `Mem2Reg`
9. `PolygeistCanonicalize`
10. `ControlFlowSink`
11. `PolygeistCanonicalize`
12. `VerifyLowered`

### Epilogue A) `complete-mlir` (post-O3, only when `--O3`)
1. `PolygeistCanonicalize`
2. `ControlFlowSink`
3. `PolygeistCanonicalize`
4. `LICM`
5. `CSE`
6. `PolygeistCanonicalize`

### Epilogue B) `emit-llvm` (only when `--emit-llvm`)
1. `CSE`
2. `PolygeistCanonicalize`
3. `ArithExpandOps`
4. `ConvertPolygeistToLLVM`
5. `ConvertIndexToLLVM`
6. `ReconcileUnrealizedCasts`
7. `AliasScopeGen`
8. `LoopVectorizationHints`
9. `PolygeistCanonicalize`
10. `CSE`

## Stage Dependencies

- `initial-cleanup` depends on `raise-memref-dimensionality`
- `openmp-to-arts` depends on `initial-cleanup`
- `edt-transforms` depends on `openmp-to-arts`
- `create-dbs` depends on `openmp-to-arts` *(sibling of `edt-transforms`)*
- `db-opt`, `edt-opt`, `concurrency` all depend on `create-dbs`
- `edt-distribution` depends on `concurrency`
- `post-distribution-cleanup`, `db-partitioning` depend on `edt-distribution`
- `post-db-refinement` depends on `db-partitioning`
- `late-concurrency-cleanup` depends on `post-db-refinement`
- `pre-lowering` depends on `epochs` AND `late-concurrency-cleanup`
- `arts-to-llvm` depends on `pre-lowering`

## Utility Ownership Notes

- Machine/config parsing belongs to `AbstractMachine`.
- Semantic pattern classification happens inside `openmp-to-arts` — the SDE
  passes (`SdeScopeSelection`, `SdeScheduleRefinement`, `SdeDistributionPlanning`,
  etc.) seed contracts that later ARTS-core stages consume. There is no
  separate "pattern-pipeline" stage; legacy references elsewhere are stale.
- Analysis and heuristics speak contract vocabulary:
  `DistributionHeuristics`, `PartitioningHeuristics`, `DbAnalysis`.
- IR lowering details belong to pass/transform layers:
  `ForLowering` (inside `edt-distribution`), DB/EDT lowerings (inside
  `pre-lowering`), `ConvertArtsToLLVM` (inside `arts-to-llvm`).
- Type cast helpers should prefer shared utility methods (`ArtsCodegen` or
  `ValueUtils`) instead of pass-local duplicates.
