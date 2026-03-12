# CARTS Compiler Pipeline (Stage Order)

This document mirrors the actual `carts-compile` stage order in
`tools/run/carts-compile.cpp`.

Use it as the canonical checklist when auditing pass ordering, ownership, and
cross-stage contracts.

## Stage Order (`--stop-at`)

1. `canonicalize-memrefs`
2. `collect-metadata`
3. `initial-cleanup`
4. `openmp-to-arts`
5. `edt-transforms`
6. `loop-reordering`
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

## Per-Stage Pass Summary

### 1) canonicalize-memrefs
- `LowerAffine` (nested func)
- `CSE`
- `Inliner`
- `ArtsInliner`
- `PolygeistCanonicalize`
- `CanonicalizeMemrefs`
- `DeadCodeElimination`
- `CSE`

### 2) collect-metadata
- `replaceAffineCFG` / `RaiseSCFToAffine` (twice, nested func)
- `CSE` (nested func)
- `CollectMetadataPass`

### 3) initial-cleanup
- `LowerAffine` (nested func)
- `CSE` (nested func)
- `PolygeistCanonicalizeFor` (nested func)

### 4) openmp-to-arts
- `ConvertOpenMPtoArts`
- `DeadCodeElimination`
- `CSE`

### 5) edt-transforms
- `EdtPass(runAnalysis=false)`
- `EdtInvariantCodeMotion`
- `DeadCodeElimination`
- `SymbolDCE`
- `CSE`
- `EdtPtrRematerialization`

### 6) loop-reordering
- `LoopNormalization`
- `ArtsDepTransforms`
- `StencilBoundaryPeeling`
- `LoopReordering`
- `LoopTransforms`
- `DistributedHostLoopOutlining`
  - auto-enabled by multinode builds and by `--distributed-db`
  - outlines eligible host producer loops so they flow through the regular
    distributed `arts.for` pipeline before `CreateDbs`
- `CSE`

### 7) create-dbs
- `CreateDbs`
- `PolygeistCanonicalize`
- `CSE`
- `SymbolDCE`
- `Mem2Reg`
- `PolygeistCanonicalize`

### 8) db-opt
- `DbPass`
- `PolygeistCanonicalize`
- `CSE`
- `Mem2Reg`

### 9) edt-opt
- `PolygeistCanonicalize`
- `EdtPass(runAnalysis=true)`
- `LoopFusion`
- `CSE`

### 10) concurrency
- `PolygeistCanonicalize`
- `ConcurrencyPass`
- `ArtsForOptimization`
- `PolygeistCanonicalize`

### 11) edt-distribution
- `EdtDistributionPass`
- `ForLowering`

### 12) concurrency-opt
- `EdtPass(runAnalysis=false)`
- `DeadCodeElimination`
- `PolygeistCanonicalize`
- `CSE`
- `EpochOpt`
- `PolygeistCanonicalize`
- `CSE`
- `DbPartitioning`
- optional `DistributedDbOwnership` (enabled by `--distributed-db`)
- `DbPass`
- `BlockLoopStripMining` (nested func)
- `ArtsHoisting`
- `PolygeistCanonicalize`
- `CSE`
- `EdtAllocaSinking`
- `DeadCodeElimination`
- `Mem2Reg`

### 13) epochs
- `PolygeistCanonicalize`
- `CreateEpochs`
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
- `DataPointerHoisting`
- `PolygeistCanonicalize`
- `CSE`
- `ScalarReplacement`
- `PolygeistCanonicalize`
- `CSE`
- `EpochLowering`
- `PolygeistCanonicalize`
- `CSE`

### 15) arts-to-llvm
- `ConvertArtsToLLVM`
- `PolygeistCanonicalize`
- `CSE`
- `Mem2Reg`
- `PolygeistCanonicalize`
- `ControlFlowSink`
- `PolygeistCanonicalize`

### 16) complete extras
- optional `-O3` function-level cleanup:
  `PolygeistCanonicalize -> ControlFlowSink -> PolygeistCanonicalize -> LICM ->
  CSE -> PolygeistCanonicalize`
- optional `--emit-llvm` lowering:
  `CSE -> PolygeistCanonicalize -> ArithExpandOps -> ConvertPolygeistToLLVM ->
  AliasScopeGen -> LoopVectorizationHints -> PolygeistCanonicalize -> CSE`

## Utility Ownership Notes

- Machine/config parsing belongs to `ArtsAbstractMachine`.
- Pattern and strategy decisions belong to analysis:
  `DistributionHeuristics`, `PartitioningHeuristics`, `DbAnalysis`.
- IR lowering details belong to pass/transform layers:
  `ForLowering`, DB/EDT lowerings, `ConvertArtsToLLVM`.
- Type cast helpers should prefer shared utility methods (`ArtsCodegen` or
  `ValueUtils`) instead of pass-local duplicates.
