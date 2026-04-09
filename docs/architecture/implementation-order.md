# Implementation Order

## Phase Dependency Graph

```
Phase 1: Extract arts_rt (cleanest boundary, no SDE dependency)
    |
Phase 2: Create SDE dialect (depends on directory structure from Phase 1)
    |
Phase 3: Full folder reorganization (depends on both Phase 1 and 2)
```

## Phase 1: Extract `arts_rt` Dialect

See [arts-rt-dialect.md](arts-rt-dialect.md) for full details.

### Tier Breakdown

```
Tier 0: Create rt dialect scaffold (TableGen + headers)
  |-- No dependencies -- pure file creation
  |-- ~0.5h

Tier 1: Move 14 op definitions to RtOps.td
  |-- Blocked by: Tier 0
  |-- Parallelizable: Split by op group (epoch, EDT, dep/state)
  |-- ~3h

Tier 2: CMakeLists + TableGen generation
  |-- Blocked by: Tier 1
  |-- ~0.5h

Tier 3: Dialect C++ implementation
  |-- 3.1: RtDialect.cpp (register ops)
  |-- 3.2: Register in Compile.cpp
  |-- ~0.75h

Tier 4: Conversion pattern split (PARALLEL)
  |-- 4.1a: Keep core patterns in ConvertArtsToLLVMPatterns.cpp
  |-- 4.1b: Move epoch rt patterns (3)
  |-- 4.1c: Move remaining rt patterns (11)
  |-- 4.2: Wire populateRtToLLVMPatterns into pass
  |-- All 4.1x run in parallel
  |-- ~3h

Tier 5: Include/namespace updates (HIGHLY PARALLEL)
  |-- Add using declarations to 7 lowering pass files
  |-- ~0.5h (parallelized)

Tier 6: Test migration + verification
  |-- sed on 24 test files
  |-- Build + test
  |-- ~0.5h
```

**Critical path**: T0 -> T1 -> T2 -> T3 -> T4 -> T6 = ~8h
**With parallelization** (8-10 agents): ~5-6h

### Key Constraints

- `ArtsCodegen` cannot be split -- stays in core, used by all patterns
- `DepAccessMode` moves to RtOps.td (only used by rt ops)
- `ArtsToLLVMPattern<T>` stays shared via `ConvertArtsToLLVMInternal.h`
- No circular dependencies -- rt ops use only standard MLIR types

## Phase 2: Create SDE Dialect

See [sde-dialect.md](sde-dialect.md) and [pipeline-redesign.md](pipeline-redesign.md).

### Sub-Phases

```
Phase 2A: SDE scaffold (additive)
  1. Create include/arts/dialect/sde/ directory structure
  2. Write SdeDialect.td, SdeOps.td, SdeTypes.td, SdeAttrs.td
  3. Write SdeEffects.h (ComputeResource, DataResource, SyncResource)
  4. Write SdeDialect.cpp, SdeOps.cpp (verifiers, builders)
  5. Write CMakeLists.txt files
  6. Build and verify tablegen

Phase 2B: OMP-to-SDE conversion
  7. Write ConvertOpenMPToSde.cpp (12 patterns, modeled on ConvertOpenMPToArts.cpp)
  8. Write ConvertSdeToArts.cpp (reverse mapping)
  9. Register sde dialect in Compile.cpp
  10. Update pipeline in Compile.cpp: omp->sde->arts instead of omp->arts
  11. Build, test, verify identical output

Phase 2C: Tensor integration
  12. Write RaiseToLinalg.cpp (scf.for+memref -> linalg.generic)
  13. Write RaiseToTensor.cpp (linalg memref -> linalg tensor)
  14. Integrate one-shot-bufferize into pipeline
  15. Write SdeOpt.cpp (tensor-space analysis)
  16. Integrate linalg-to-loops into pipeline
  17. Build, test, verify identical output

Phase 2D: Migrate general passes to SDE
  18. Move CollectMetadata to sde/Transforms/
  19. Move LoopNormalization to sde/Transforms/
  20. Move LoopReordering to sde/Transforms/ (fix matmul-autodetect dep)
  21. Update includes project-wide
  22. Build and test
```

## Phase 3: Full Folder Reorganization

### Sub-Phases

```
Phase 3A: Move core arts to dialect/core/
  1. Move Ops.td, Attributes.td, Types.td, Dialect.td to include/arts/dialect/core/IR/
  2. Move Dialect.cpp to lib/arts/dialect/core/IR/
  3. Add forwarding compatibility headers
  4. Update all includes project-wide

Phase 3B: Move passes to dialect-owned structure
  5. Move EDT passes to core/Transforms/
  6. Move DB passes to core/Transforms/
  7. Move lowering passes to core/Conversion/ and rt/Conversion/
  8. Move codegen passes to rt/Transforms/ and general/
  9. Create patterns/ directory and move pattern passes

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

- `include/arts/dialect/rt/IR/RtDialect.td`
- `include/arts/dialect/rt/IR/RtOps.td`
- `include/arts/dialect/rt/IR/RtDialect.h`
- `lib/arts/dialect/rt/IR/RtDialect.cpp`
- `lib/arts/dialect/rt/IR/RtOps.cpp`
- `lib/arts/dialect/rt/Conversion/RtToLLVM/RtToLLVMPatterns.cpp`
- `lib/arts/passes/transforms/ConvertArtsToLLVMInternal.h`

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

- `include/arts/Ops.td` -- remove 14 ops
- `include/arts/Attributes.td` -- remove DepAccessMode
- `lib/arts/Dialect.cpp` -- remove 2 verifiers + 2 builders
- `lib/arts/passes/transforms/EdtLowering.cpp` -- using declarations
- `lib/arts/passes/transforms/EpochLowering.cpp` -- using declarations
- `lib/arts/passes/transforms/ForLowering.cpp` -- using declarations
- `lib/arts/passes/transforms/ConvertArtsToLLVMPatterns.cpp` -- extract 14 patterns
- `tools/compile/Compile.cpp` -- register dialect
- 24 test files in `tests/contracts/`
