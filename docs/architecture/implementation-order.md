# Implementation Order

## Phase Dependency Graph

```
Phase 1: Extract arts_rt (cleanest boundary, no SDE dependency)
    |
Phase 2: Create SDE dialect (depends on directory structure from Phase 1)
    |
Phase 3: Full folder reorganization (depends on both Phase 1 and 2)
```

## Phase 1: Extract `arts_rt` Dialect -- COMPLETE

See [arts-rt-dialect.md](arts-rt-dialect.md) for full details.

**Status**: All sub-phases complete. 159/168 tests pass (same baseline as main).

### Completed Steps

```
Phase 1A: arts_rt scaffold (ef893f12)
  - Created include/arts/dialect/rt/IR/ (RtDialect.td, RtOps.td, RtDialect.h)
  - Created lib/arts/dialect/rt/IR/ (RtDialect.cpp, RtOps.cpp)
  - CMakeLists.txt for TableGen + MLIRArtsRt library

Phase 1B: Op extraction (35ece54e, 27812b9a)
  - Removed 14 ops from include/arts/Ops.td
  - Removed DepAccessMode from include/arts/Attributes.td
  - Removed verifiers/builders from lib/arts/Dialect.cpp
  - Added using declarations to 15+ lowering/analysis files
  - Registered arts_rt dialect in Compile.cpp
  - Renamed ops in 24+ test files

Phase 1C: Conversion pattern split (162fceef)
  - Created lib/arts/dialect/rt/Conversion/RtToLLVM/RtToLLVMPatterns.cpp
    (14 patterns, 1841 lines extracted from ConvertArtsToLLVMPatterns.cpp)
  - ConvertArtsToLLVMPatterns.cpp reduced from 2982 to 1221 lines
  - 4-phase greedy rewrite: runtime -> rt -> db -> other
  - populateRtToLLVMPatterns() wired into ConvertArtsToLLVM pass

Phase 1D: Verification updates (in 1C commit)
  - VerifyLowered.cpp checks both arts.* and arts_rt.* survival
  - Transitional using declarations in include/arts/Dialect.h
```

### Key Decisions Made

- `ArtsCodegen` stays shared -- used by all conversion patterns
- `DepAccessMode` moved to RtOps.td (only used by rt ops)
- `ArtsToLLVMPattern<T>` shared via `ConvertArtsToLLVMInternal.h`
- RtToLLVMPatterns.cpp compiled as part of MLIRArtsTransforms (not
  separate library) to avoid circular ArtsCodegen dependency
- No circular dependencies -- rt ops use only standard MLIR types

## Phase 2: Create SDE Dialect -- IN PROGRESS (2A+2B COMPLETE)

See [sde-dialect.md](sde-dialect.md) and [pipeline-redesign.md](pipeline-redesign.md).

### Completed Steps

```
Phase 2A: SDE scaffold (8d3ecbf0)
  - Created include/arts/dialect/sde/IR/ (SdeDialect.td, SdeOps.td, SdeDialect.h)
  - 8 ops: CuRegion, CuTask, CuReduce, CuAtomic, SuIterate, SuBarrier, MuDep, Yield
  - 5 enums: SdeCuKind, SdeAccessMode, SdeReductionKind, SdeScheduleKind, SdeConcurrencyScope
  - Registered in Compile.cpp

Phase 2B: OMP-to-SDE-to-ARTS pipeline (cd75cf2a)
  - ConvertOpenMPToSde.cpp: 11 patterns (parallel, master, single, wsloop,
    task, taskloop, scf.parallel, atomic, terminator, barrier, taskwait)
  - SdeToArtsPatterns.cpp: 8 patterns (cu_region, cu_task, su_iterate,
    su_barrier, cu_atomic, cu_reduce, mu_dep, yield)
  - Pipeline wired in Compile.cpp: omp->sde->arts replaces omp->arts
  - VerifySdeLowered pass added (ba2ec03d): checks no sde.* ops survive
  - 159/168 tests pass (same baseline)
```

### Remaining Sub-Phases (deferred)

```
Phase 2C: Tensor integration (deferred — over-engineering for now)
  12. Write RaiseToLinalg.cpp (scf.for+memref -> linalg.generic)
  13. Write RaiseToTensor.cpp (linalg memref -> linalg tensor)
  14. Integrate one-shot-bufferize into pipeline
  15. Write SdeOpt.cpp (tensor-space analysis)
  16. Integrate linalg-to-loops into pipeline
  17. Build, test, verify identical output

Phase 2D: Migrate general passes to SDE (partially complete)
  18. Move CollectMetadata to sde/Transforms/ -- DONE (4d62f756)
  19. Move LoopNormalization to sde/Transforms/ (blocked — uses arts::ForOp)
  20. Move LoopReordering to sde/Transforms/ (blocked — uses DbPatternMatchers)
  21. Update includes project-wide
  22. Build and test
```

## Phase 3: Full Folder Reorganization -- IN PROGRESS (3B steps 5-8 COMPLETE)

### Completed Steps

```
Phase 3B step 7: Move lowering passes to rt/Conversion/ArtsToRt/
  - EdtLowering.cpp + EdtLoweringSupport.cpp + EdtLoweringInternal.h
  - EpochLowering.cpp
  - These are the only passes that produce arts_rt ops

Phase 3B step 8: Move codegen passes to rt/Transforms/
  - DataPtrHoisting.cpp + DataPtrHoistingSupport.cpp + DataPtrHoistingInternal.h
  - GuidRangCallOpt.cpp
  - RuntimeCallOpt.cpp
  - These operate post-lowering on arts_rt ops or LLVM runtime calls

Phase 3B steps 5-6: Move all pass .cpp files to core/
  - 47 pass files moved to lib/arts/dialect/core/Transforms/
  - EDT passes (7): EdtStructuralOpt, EdtAllocaSinking, EdtICM,
    EdtTransformsPass, EdtOrchestrationOpt, StructuredKernelPlanPass,
    PersistentStructuredRegion
  - DB passes (9): BlockLoopStripMining(+Support), ContractValidation,
    DbDistributedOwnership, DbModeTightening, DbPartitioning(+Support),
    DbScratchElimination, DbTransformsPass
  - Epoch passes (5): EpochOpt, EpochOptCpsChain, EpochOptScheduling,
    EpochOptStructural, EpochOptSupport
  - Core transforms (10): Concurrency, CreateDbs, CreateEpochs,
    DbLowering, ParallelEdtLowering, ForLowering, ForOpt,
    DistributedHostLoopOutlining, EdtDistribution, EdtPtrRematerialization
  - Pattern passes (4): PatternDiscovery, KernelTransforms,
    StencilBoundaryPeeling, DepTransforms
  - Loop passes (3): LoopFusion, LoopNormalization, LoopReordering
  - General passes (5): Inliner, RaiseMemRefDimensionality, HandleDeps,
    DeadCodeElimination, ScalarReplacement
  - Codegen passes (4): AliasScopeGen, Hoisting, LoopVectorizationHints,
    LoweringContractCleanup
  - ConvertOpenMPToArts.cpp → core/Conversion/OmpToArts/
  - ConvertArtsToLLVM.cpp + Patterns → core/Conversion/ArtsToLLVM/
  - 4 internal headers moved to include/arts/dialect/core/
  - Include guards + paths updated in all moved files

159/168 tests pass (same baseline as main)
```

### Remaining Sub-Phases

```
Phase 3A: Move core arts IR to dialect/core/IR/ (deferred — high risk, 100+ include changes)
  1. Move Ops.td, Attributes.td, Types.td, Dialect.td to include/arts/dialect/core/IR/
  2. Move Dialect.cpp to lib/arts/dialect/core/IR/
  3. Add forwarding compatibility headers
  4. Update all includes project-wide

Phase 3C: Verification
  10. Build and test
  11. Run full benchmark suite
  12. Verify no regressions
```

## Verification at Each Phase

### Phase 1 Verification

```bash
dekk carts build --clean
dekk carts test
grep -rn 'arts\.\(edt_create\|rec_dep\|dep_gep\)' tests/contracts/*.mlir  # Should be empty
dekk carts compile tests/examples/jacobi/jacobi-for.c -O3 --pipeline=pre-lowering 2>&1 | grep 'arts_rt\.'
```

### Phase 2 Verification

```bash
dekk carts build --clean
dekk carts test
dekk carts compile tests/examples/jacobi/jacobi-for.c -O3 --pipeline=pattern-pipeline 2>&1 | grep 'sde\.'
dekk carts compile tests/examples/jacobi/jacobi-for.c -O3 --pipeline=edt-transforms 2>&1 | grep -c 'sde\.'  # Should be 0
```

### Phase 3 Verification

```bash
dekk carts build --clean
dekk carts test
dekk carts benchmarks run --size large --timeout 300 --threads 16
```

## Critical Files

### Must Create (Phase 1)

```
include/arts/dialect/rt/
  IR/
    RtDialect.td               # NEW: dialect definition
    RtOps.td                   # NEW: 14 op definitions
    RtDialect.h                # NEW: public C++ header
    CMakeLists.txt             # NEW: TableGen targets

lib/arts/dialect/rt/
  IR/
    RtDialect.cpp              # NEW: initialize()
    RtOps.cpp                  # NEW: verifiers + builders
    CMakeLists.txt             # NEW: MLIRArtsRt library
  Conversion/
    RtToLLVM/
      RtToLLVMPatterns.cpp     # NEW: 14 patterns extracted from ConvertArtsToLLVM
      CMakeLists.txt
```

Note: In Phase 1, the lowering passes (EdtLowering, EpochLowering, etc.)
stay in their current location (`lib/arts/passes/transforms/`). They move
to `lib/arts/dialect/rt/Conversion/ArtsToRt/` in Phase 3 (full folder
reorganization). Phase 1 is purely about extracting the 14 ops and their
LLVM patterns, not about moving passes.

### Must Create (Phase 2)

- `include/arts/dialect/sde/IR/SdeDialect.td`
- `include/arts/dialect/sde/IR/SdeOps.td`
- `include/arts/dialect/sde/IR/SdeTypes.td`
- `include/arts/dialect/sde/IR/SdeAttrs.td`
- `include/arts/dialect/sde/IR/SdeEffects.h`
- `include/arts/dialect/sde/IR/SdeDialect.h`
- `lib/arts/dialect/sde/IR/SdeDialect.cpp`
- `lib/arts/dialect/sde/IR/SdeOps.cpp`
- `lib/arts/dialect/sde/Transforms/ConvertOpenMPToSde.cpp`
- `lib/arts/dialect/sde/Transforms/RaiseToLinalg.cpp`
- `lib/arts/dialect/sde/Transforms/RaiseToTensor.cpp`
- `lib/arts/dialect/sde/Transforms/SdeOpt.cpp`
- `lib/arts/dialect/core/Conversion/SdeToArts/SdeToArtsPatterns.cpp`

### Must Modify (Phase 1)

Op extraction:
- `include/arts/Ops.td` -- remove 14 ops
- `include/arts/Attributes.td` -- remove DepAccessMode
- `lib/arts/Dialect.cpp` -- remove 2 verifiers + 2 builders
- `lib/arts/passes/transforms/ConvertArtsToLLVMPatterns.cpp` -- extract 14 patterns

Namespace updates (15 files, see [arts-rt-dialect.md](arts-rt-dialect.md) cross-ref table):
- `lib/arts/passes/transforms/EdtLowering.cpp` -- using declarations (10 ops)
- `lib/arts/passes/transforms/EpochLowering.cpp` -- using declarations (8 ops)
- `lib/arts/passes/transforms/ForLowering.cpp` -- using declarations
- `lib/arts/utils/DbUtils.cpp` + header -- using + template instantiation
- `lib/arts/passes/opt/codegen/DataPtrHoistingSupport.cpp` + header -- using
- `lib/arts/transforms/db/DbLayoutStrategy.cpp` -- using
- `lib/arts/passes/opt/epoch/EpochOptScheduling.cpp` -- using
- `lib/arts/analysis/heuristics/EpochHeuristics.cpp` -- using
- `lib/arts/passes/opt/codegen/AliasScopeGen.cpp` -- using
- `lib/arts/analysis/value/ValueAnalysis.cpp` -- using
- `lib/arts/analysis/db/DbDistributedEligibility.cpp` -- using
- `include/arts/passes/transforms/EdtLoweringInternal.h` -- using

Registration:
- `tools/compile/Compile.cpp` -- register `arts::rt::ArtsRtDialect`
- `tools/compile/CMakeLists.txt` -- link `MLIRArtsRt`

Tests:
- 24 test files in `tests/contracts/` -- sed rename
