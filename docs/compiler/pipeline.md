# CARTS Compiler Pipeline (Pipeline Order)

This document mirrors the actual `carts-compile` pipeline-step order in
`tools/compile/Compile.cpp`.

Use it as the canonical checklist when auditing pass ordering, ownership, and
cross-step contracts.

For an example-driven view of how concrete loop/kernel families travel through
this pipeline, see [`optimization-patterns.md`](./optimization-patterns.md).

## CLI Introspection

- `tools/carts pipeline`: show pipeline order and pass counts.
- `tools/carts pipeline --pipeline=<step>`: show passes for one pipeline step.
- `carts-compile --print-pipeline-manifest-json`: print the machine-readable
  manifest consumed by the Python CLI.

## Pipeline Order (`--pipeline` / `--start-from`)

1. `raise-memref-dimensionality`
2. `collect-metadata`
3. `initial-cleanup`
4. `openmp-to-arts`
5. `pattern-pipeline`
6. `edt-transforms`
7. `create-dbs`
8. `db-opt`
9. `edt-opt`
10. `concurrency`
11. `edt-distribution`
12. `concurrency-opt`
13. `epochs`
14. `pre-lowering`
15. `arts-to-llvm`
16. `complete`

## Pipeline Controls

- `--pipeline=<step>`: run up to and including the selected pipeline step.
- `--start-from=<step>`: skip all earlier pipeline steps and start at the
  selected pipeline step.
- Invalid ranges are rejected: `--start-from` cannot be later than
  `--pipeline` unless `--pipeline=complete`.

## Per-Step Pass Summary

### 1) raise-memref-dimensionality
- `LowerAffine` (nested func)
- `CSE`
- `Inliner`
- `ArtsInliner`
- `PolygeistCanonicalize`
- `RaiseMemRefDimensionality`
- `HandleDeps`
- `DeadCodeElimination`
- `CSE`

### 2) collect-metadata
- `replaceAffineCFG` / `RaiseSCFToAffine` (twice, nested func)
- `CSE` (nested func)
- `CollectMetadata`
- `VerifyMetadata` (diagnose mode)

### 3) initial-cleanup
- `LowerAffine` (nested func)
- `CSE` (nested func)
- `PolygeistCanonicalizeFor` (nested func)

### 4) openmp-to-arts
- `ConvertOpenMPToArts`
- `DeadCodeElimination`
- `CSE`

### 5) pattern-pipeline
- `PatternDiscovery(seed)`
- `DepTransforms`
- `LoopNormalization`
- `StencilBoundaryPeeling`
- `LoopReordering`
- `PatternDiscovery(refine)`
- `KernelTransforms`
- `CSE`

### 6) edt-transforms
- `EdtStructuralOpt(runAnalysis=false)`
- `EdtICM`
- `DeadCodeElimination`
- `SymbolDCE`
- `CSE`
- `EdtPtrRematerialization`

### 7) create-dbs
- pre-DB contract bridging / lowering-contract seeding
- `DistributedHostLoopOutlining`
  - auto-enabled by multinode builds and by `--distributed-db`
  - outlines eligible host producer loops so they flow through the regular
    distributed `arts.for` pipeline before `CreateDbs`
- `CreateDbs`
- `CSE` (bridge cleanup, conditional)
- `PolygeistCanonicalize`
- `CSE`
- `SymbolDCE`
- `Mem2Reg`
- `PolygeistCanonicalize`
- `VerifyDbCreated`

### 8) db-opt
- `DbModeTightening`
- `PolygeistCanonicalize`
- `CSE`
- `Mem2Reg`

### 9) edt-opt
- `PolygeistCanonicalize`
- `EdtStructuralOpt(runAnalysis=true)`
- `LoopFusion`
- `CSE`

### 10) concurrency
- `PolygeistCanonicalize`
- `Concurrency`
- `ForOpt`
- `PolygeistCanonicalize`

### 11) edt-distribution
- `EdtDistribution`
- `ForLowering`
- `VerifyForLowered`

### 12) concurrency-opt
- `EdtStructuralOpt(runAnalysis=false)`
- `DeadCodeElimination`
- `PolygeistCanonicalize`
- `CSE`
- `EdtStructuralOpt(runAnalysis=false)` (second cleanup pass)
- `EpochOpt`
- `PolygeistCanonicalize`
- `CSE`
- `DbPartitioning`
- optional `DbDistributedOwnership` (enabled by `--distributed-db`)
- `DbTransforms`
- `DbModeTightening`
- `EdtTransforms`
- `ContractValidation`
- `DbScratchElimination`
- `PolygeistCanonicalize`
- `CSE`
- `BlockLoopStripMining` (nested func)
- `Hoisting`
- `PolygeistCanonicalize`
- `CSE`
- `EdtAllocaSinking`
- `DeadCodeElimination`
- `Mem2Reg`

### 13) epochs
- `PolygeistCanonicalize`
- `CreateEpochs`
- `EpochContinuationPrep` (conditional)
- `PolygeistCanonicalize`

### 14) pre-lowering
- `EdtAllocaSinking`
- `ParallelEdtLowering`
- `PolygeistCanonicalize`
- `CSE`
- `DbLowering`
- `PolygeistCanonicalize`
- `CSE`
- `EdtLowering`
- `PolygeistCanonicalize`
- `CSE`
- `LICM`
- `DataPtrHoisting`
- `PolygeistCanonicalize`
- `CSE`
- `ScalarReplacement`
- `PolygeistCanonicalize`
- `CSE`
- `EpochLowering`
- `LoweringContractCleanup`
- `PolygeistCanonicalize`
- `CSE`
- `VerifyPreLowered`

### 15) arts-to-llvm
- `ConvertArtsToLLVM`
- `GuidRangCallOpt`
- `RuntimeCallOpt`
- `DataPtrHoisting`
- `PolygeistCanonicalize`
- `CSE`
- `Mem2Reg`
- `PolygeistCanonicalize`
- `ControlFlowSink`
- `PolygeistCanonicalize`
- `VerifyLowered`

### 16) complete extras
- optional `-O3` function-level cleanup:
  `PolygeistCanonicalize -> ControlFlowSink -> PolygeistCanonicalize -> LICM ->
  CSE -> PolygeistCanonicalize`
- optional `--emit-llvm` lowering:
  `CSE -> PolygeistCanonicalize -> ArithExpandOps -> ConvertPolygeistToLLVM ->
  AliasScopeGen -> LoopVectorizationHints -> PolygeistCanonicalize -> CSE`

## Utility Ownership Notes

- Machine/config parsing belongs to `AbstractMachine`.
- Metadata belongs to `CollectMetadataPass` and the metadata managers.
- Pattern discovery and refinement belong to the pattern pipeline:
  `PatternDiscovery`, `DepTransforms`, and `KernelTransforms`.
- Pattern passes stamp contracts; DB/EDT lowering passes consume those
  contracts instead of rediscovering supported families from metadata or raw
  IR structure.
- Analysis and heuristics should speak the same contract vocabulary whenever a
  supported family is present:
  `DistributionHeuristics`, `PartitioningHeuristics`, and `DbAnalysis`.
- IR lowering details belong to pass/transform layers:
  `ForLowering`, DB/EDT lowerings, `ConvertArtsToLLVM`.
- Type cast helpers should prefer shared utility methods (`ArtsCodegen` or
  `ValueUtils`) instead of pass-local duplicates.
