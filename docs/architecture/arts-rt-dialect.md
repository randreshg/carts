# Phase 1: Extract `arts_rt` Dialect -- COMPLETE

**Status**: All sub-phases implemented on branch `architecture/sde-restructuring`.
159/168 tests pass (same baseline as main). Key commits: `ef893f12` (scaffold),
`35ece54e` (op extraction), `162fceef` (pattern split).

The cleanest boundary -- 14 ops created only in pre-lowering (stage 17),
consumed only by ConvertArtsToLLVM (stage 18), with zero type dependency
on core `arts`.

## Why Separate Dialects?

The monolithic `arts` dialect mixes two fundamentally different concerns:

1. **ARTS (structural)**: Ops with REGIONS carrying task/loop/epoch semantics.
   Optimized by 23+ passes across pipeline stages 4-16. EDT bodies are fused,
   split, distributed, partitioned. DB ops are tightened, partitioned, refined.

2. **ARTS_RT (runtime API)**: FLAT ops that map 1:1 to ARTS C runtime calls.
   Created ONLY by pre-lowering (stage 17). Consumed ONLY by ConvertArtsToLLVM
   (stage 18). No optimization happens on them.

The structural ops have regions containing computation. The runtime ops are
flat call descriptors -- they're the result of "outlining" the regions into
separate functions and creating runtime API calls to launch them.

```
Pipeline flow:

  stages 4-16:  arts.edt { ... body ... }     ← structural, optimizable
                arts.for { ... loop ... }
                arts.epoch { ... sync ... }
                    |
  stage 17:     EdtLowering / EpochLowering    ← BOUNDARY
                    |
                    v
  stage 17-18:  arts_rt.edt_create(...)        ← flat, 1:1 API calls
                arts_rt.rec_dep(...)
                arts_rt.create_epoch(...)
                    |
  stage 18:     ConvertArtsToLLVM
                    |
                    v
                llvm.call @artsEdtCreate(...)  ← LLVM IR
```

## Ops to Extract (14)

| Op | Created By | C Runtime Function | Verifier/Builder |
|---|---|---|---|
| `CreateEpochOp` | EpochLowering | `arts_initialize_[and_start_]epoch` | HasVerifier |
| `WaitOnEpochOp` | EpochLowering | `arts_wait_on_handle` | -- |
| `EdtCreateOp` | EdtLowering | `arts_edt_create[_with_epoch]` | HasBuilder |
| `EdtParamPackOp` | EdtLowering | (memref packing, no C call) | -- |
| `EdtParamUnpackOp` | EdtLowering | (memref unpacking, no C call) | -- |
| `RecordDepOp` | EdtLowering | `arts_add_dependence[_at][_ex]` | HasVerifier |
| `IncrementDepOp` | EdtLowering | (noop in v2) | -- |
| `DepGepOp` | EdtLowering | (LLVM GEP on dep struct) | -- |
| `DepDbAcquireOp` | EdtLowering | (ptr from dep struct field) | -- |
| `DbGepOp` | EdtLowering | (LLVM GEP for element addr) | HasBuilder |
| `StatePackOp` | EdtLowering | (memref packing, no C call) | -- |
| `StateUnpackOp` | EdtLowering | (memref unpacking, no C call) | -- |
| `DepBindOp` | EdtLowering | (identity at runtime) | -- |
| `DepForwardOp` | EdtLowering | (identity at runtime) | -- |

`DepAccessMode` enum also moves (only used by `RecordDepOp`, `IncrementDepOp`).
`RuntimeQueryKind` stays in core (used by `RuntimeQueryOp` which stays).

## Ops That Stay in ARTS (21)

| Op | Region? | Stages | Why Stays |
|---|---|---|---|
| `EdtOp` | YES | 4-17 | Task body; 10+ opt passes touch it |
| `ForOp` | YES | 4-11 | Loop body; LoopLikeOpInterface |
| `EpochOp` | YES | 4-17 | Sync region; EpochOpt restructures |
| `CPSAdvanceOp` | YES | 16-17 | CPS continuation body |
| `DbAllocOp` | no | 7-18 | DbPartitioning, DbModeTightening, CreateDbs |
| `DbAcquireOp` | no | 7-18 | Multi-stage: CreateDbs through LLVM |
| `DbRefOp` | no | 7+ | Pure; access pattern analysis |
| `DbReleaseOp` | no | 7-18 | Created by CreateDbs (stage 7), not stage 17 |
| `DbFreeOp` | no | 7-18 | Created by CreateDbs (stage 7) |
| `DbDimOp` | no | 7+ | Pure with canonicalizer; analysis |
| `DbNumElementsOp` | no | 7-17 | Dep counting across stages |
| `DbControlOp` | no | 4-7 | OMP dep → consumed by CreateDbs |
| `OmpDepOp` | no | 2-4 | OMP metadata |
| `LoweringContractOp` | no | 8-18 | Contract read by ForLowering, DbPartitioning |
| `BarrierOp` | no | 4-18 | Structural sync primitive |
| `AtomicAddOp` | no | 4-18 | Structural reduction primitive |
| `RuntimeQueryOp` | no | 4-18 | Used by Concurrency (stage 10) |
| `GetEdtEpochGuidOp` | no | 16-18 | CPS chain epoch GUID |
| `YieldOp` | no | all | Implicit terminator |
| `UndefOp` | no | -- | Utility |
| `AllocOp` | no | -- | Utility |

### Key Boundary Decisions

**`BarrierOp` stays in ARTS** -- despite having no region, it's created in
stage 4 (OMP conversion) and represents a structural synchronization point.

**`RuntimeQueryOp` stays in ARTS** -- used by `Concurrency` pass (stage 10)
for worker count decisions, not just at final lowering.

**`GetEdtEpochGuidOp` stays in ARTS** -- created by EpochOpt CPS chain
(stage 16), before pre-lowering.

**`DbReleaseOp`/`DbFreeOp` stay in ARTS** -- created by `CreateDbs` (stage 7),
not just pre-lowering. They span stages 7-18.

## The Boundary is Clean

Confirmed by deep investigation: **no structural ops consume runtime ops,
no runtime ops are created before stage 17, no circular dependencies.**

```
STRUCTURAL (arts.*):
  arts.edt { body }    → optimized by 10+ passes → EdtLowering erases
  arts.for { loop }    → ForLowering lowers      → gone by stage 12
  arts.epoch { sync }  → EpochOpt restructures   → EpochLowering erases
  arts.db_alloc        → DbPartitioning etc.      → ConvertToLLVM
  arts.db_acquire      → DbModeTightening etc.    → ConvertToLLVM

RUNTIME (arts_rt.*):
  arts_rt.edt_create   ← EdtLowering creates     → ConvertToLLVM
  arts_rt.rec_dep      ← EdtLowering creates     → ConvertToLLVM
  arts_rt.create_epoch ← EpochLowering creates   → ConvertToLLVM
  arts_rt.dep_gep      ← EdtLowering creates     → ConvertToLLVM
  ... (all 14 follow this pattern)

No cross-cutting: structural never consumes runtime, runtime never created
before pre-lowering.
```

## Directory Structure

The `rt/` dialect owns its own directory with the IREE-style per-dialect
layout: `IR/`, `Conversion/`, `Transforms/`. This keeps all runtime-level
code (dialect definition, lowering FROM arts, lowering TO LLVM, and
post-lowering optimization) in a single cohesive directory.

```
include/arts/dialect/
  rt/
    IR/
      RtDialect.td            # Dialect definition + ArtsRt_Op base class
      RtOps.td                # 14 runtime ops (moved from Ops.td)
      RtDialect.h             # Public C++ header
      CMakeLists.txt
    Transforms/
      CMakeLists.txt          # (headers for post-lowering opt, if needed)

lib/arts/dialect/
  rt/
    IR/
      RtDialect.cpp           # ArtsRtDialect::initialize()
      RtOps.cpp               # Verifiers: CreateEpochOp, RecordDepOp
                               # Builders: EdtCreateOp, DbGepOp
      CMakeLists.txt
    Conversion/
      ArtsToRt/                # arts.edt/epoch/for → arts_rt.edt_create/...
        EdtLowering.cpp        # Main structural→runtime boundary
        EdtLoweringSupport.cpp # Helpers for EDT lowering
        EpochLowering.cpp      # Epoch region → create_epoch + wait
        DbLowering.cpp         # DB stack→heap rewriting
        ParallelEdtLowering.cpp # Parallel EDT → worker loop + task EDTs
        CMakeLists.txt
      RtToLLVM/                # arts_rt.* → llvm.call @arts*
        RtToLLVMPatterns.cpp   # 14 patterns from ConvertArtsToLLVMPatterns.cpp
        CMakeLists.txt
    Transforms/
      DataPtrHoisting.cpp      # Hoists lowered dep/db ptr loads
      DataPtrHoistingSupport.cpp
      GuidRangCallOpt.cpp      # arts_guid_reserve → range optimization
      RuntimeCallOpt.cpp       # Runtime call hoisting
      CMakeLists.txt
```

### Why rt/ Owns Both Conversions

The `rt/` dialect directory contains TWO conversion directories:

- **`ArtsToRt/`**: Passes that CONSUME structural `arts.*` ops (EdtOp, EpochOp)
  and CREATE runtime `arts_rt.*` ops (EdtCreateOp, CreateEpochOp). This is
  where the structural→runtime boundary crossing happens.

- **`RtToLLVM/`**: Patterns that CONSUME `arts_rt.*` ops and CREATE `llvm.*`
  calls. These are the 14 patterns extracted from ConvertArtsToLLVMPatterns.cpp.

Following IREE's convention, conversion directories live in the TARGET dialect:
`rt/Conversion/ArtsToRt/` (arts is source, rt is target) and
`rt/Conversion/RtToLLVM/` (rt is source, LLVM is target).

## TableGen

```tablegen
def ArtsRt_Dialect : Dialect {
  let name = "arts_rt";
  let cppNamespace = "::mlir::arts::rt";
  let summary = "ARTS Runtime dialect -- 1:1 mapping to ARTS API calls";
  let useDefaultAttributePrinterParser = 1;
}

class ArtsRt_Op<string mnemonic, list<Trait> traits = []>
    : Op<ArtsRt_Dialect, mnemonic, traits>;
```

## ConvertArtsToLLVM Split

**Shared** (`ConvertArtsToLLVMInternal.h`):
- `ArtsToLLVMPattern<OpType>` template base class
- `ArtsCodegen*` reference (the only shared context)
- No circular dependencies between pattern groups

**Move to RtToLLVMPatterns.cpp** (14 patterns, ~2500 LOC):

| Pattern | Complexity | LOC | C Runtime Call |
|---|---|---|---|
| `CreateEpochPattern` | Simple | ~40 | `arts_initialize_[and_start_]epoch` |
| `WaitOnEpochPattern` | Simple | ~20 | `arts_wait_on_handle` |
| `EdtCreatePattern` | Complex | ~80 | `arts_edt_create[_with_epoch]` |
| `EdtParamPackPattern` | Simple | ~40 | (memref alloc+store) |
| `EdtParamUnpackPattern` | Simple | ~40 | (memref load+cast) |
| `RecordDepPattern` | Very Complex | ~1200 | `arts_add_dependence[_at][_ex]` |
| `IncrementDepPattern` | Trivial | ~10 | (noop in v2) |
| `DepGepOpPattern` | Simple | ~30 | (LLVM GEP) |
| `DepDbAcquireOpPattern` | Medium | ~60 | (ptr field load) |
| `DbGepOpPattern` | Simple | ~30 | (LLVM GEP) |
| `StatePackPattern` | Simple | ~40 | (memref alloc+store) |
| `StateUnpackPattern` | Simple | ~40 | (memref load+cast) |
| `DepBindPattern` | Trivial | ~10 | (identity) |
| `DepForwardPattern` | Trivial | ~10 | (identity) |

**Stay in ConvertArtsToLLVMPatterns.cpp** (core arts -> LLVM):
- `DbAllocPattern` (Very Complex, ~570 LOC, distributed callbacks)
- `DbAcquirePattern`, `DbReleasePattern`, `DbFreePattern`
- `BarrierPattern`, `AtomicAddPattern`, `RuntimeQueryPattern`
- `GetEdtEpochGuidPattern`
- `UndefPattern`, `AllocPattern`, `YieldPattern`, `DbNumElementsPattern`

**Single ConvertArtsToLLVM pass** calls both `populateCorePatterns()` and
`populateRtToLLVMPatterns()`.

---

## Cross-Cutting References (Namespace Changes Required)

These files reference RT ops OUTSIDE of the expected lowering/conversion
passes. Each needs `using` declarations when ops move to `arts::rt::`.

### Critical (template instantiation + central utility)

| File | Ops Referenced | What It Does |
|---|---|---|
| `lib/arts/utils/DbUtils.cpp` | `DbGepOp`, `DepDbAcquireOp` | Central DB utility: `getUnderlyingDbAlloc()`, `extractDbLoweringInfo<DepDbAcquireOp>()`, `getDepSizesFromDb()`. Template instantiation needs careful handling. |
| `include/arts/utils/DbUtils.h` | `DepDbAcquireOp` | Template declaration in header |

### Medium (creates or inspects RT ops in optimization)

| File | Ops Referenced | What It Does |
|---|---|---|
| `lib/arts/passes/opt/codegen/DataPtrHoistingSupport.cpp` | `DepGepOp`, `DbGepOp` | Creates new `DepGepOp` instances; detects dep-pointer loads |
| `include/arts/passes/opt/codegen/DataPtrHoistingInternal.h` | `DepGepOp` | Function signatures with `DepGepOp` parameters |
| `lib/arts/transforms/db/DbLayoutStrategy.cpp` | `DbGepOp` | Creates `DbGepOp` for element address computation |
| `lib/arts/passes/opt/epoch/EpochOptScheduling.cpp` | `DepDbAcquireOp` | Distinguishes local vs dep acquire for CPS state |
| `lib/arts/analysis/heuristics/EpochHeuristics.cpp` | `CreateEpochOp` | Continuation boundary detection |
| `lib/arts/passes/opt/codegen/AliasScopeGen.cpp` | `DepGepOp` | Fallback pattern for pre-lowering form |

### Low (defensive type checks in analysis)

| File | Ops Referenced | What It Does |
|---|---|---|
| `lib/arts/analysis/value/ValueAnalysis.cpp` | `DbGepOp` | Value tracing through pointer ops |
| `lib/arts/analysis/db/DbDistributedEligibility.cpp` | `DbGepOp` | DB handle user tracing |
| `lib/arts/Dialect.cpp` | All 14 | Op builders and verifiers (move to RtOps.cpp) |

### Lowering passes (expected, need `using` declarations)

| File | Ops Referenced |
|---|---|
| `lib/arts/passes/transforms/EdtLowering.cpp` | 10 RT ops (creates them) |
| `lib/arts/passes/transforms/EpochLowering.cpp` | 8 RT ops (creates them) |
| `lib/arts/passes/transforms/ForLowering.cpp` | ~3 RT ops |
| `include/arts/passes/transforms/EdtLoweringInternal.h` | `DepDbAcquireOp` in struct |

**Total: 15 files need `using` declarations** (all mechanical, no architectural changes).

---

## Files to Modify

| File | Change |
|---|---|
| `include/arts/Ops.td` | Remove 14 ops |
| `include/arts/Attributes.td` | Remove `DepAccessMode` |
| `lib/arts/Dialect.cpp` | Remove 2 verifiers + 2 builders |
| `lib/arts/passes/transforms/EdtLowering.cpp` | `using arts::rt::{EdtCreateOp,...}` |
| `lib/arts/passes/transforms/EpochLowering.cpp` | `using arts::rt::{CreateEpochOp,...}` |
| `lib/arts/passes/transforms/ForLowering.cpp` | `using arts::rt::{EdtCreateOp,...}` |
| `lib/arts/passes/transforms/ConvertArtsToLLVMPatterns.cpp` | Extract 14 patterns |
| `lib/arts/utils/DbUtils.cpp` | `using arts::rt::{DbGepOp,DepDbAcquireOp}` |
| `include/arts/utils/DbUtils.h` | Forward decl or `using` for template |
| `lib/arts/passes/opt/codegen/DataPtrHoistingSupport.cpp` | `using arts::rt::{DepGepOp,DbGepOp}` |
| `include/arts/passes/opt/codegen/DataPtrHoistingInternal.h` | `using arts::rt::DepGepOp` |
| `lib/arts/transforms/db/DbLayoutStrategy.cpp` | `using arts::rt::DbGepOp` |
| `lib/arts/passes/opt/epoch/EpochOptScheduling.cpp` | `using arts::rt::DepDbAcquireOp` |
| `lib/arts/analysis/heuristics/EpochHeuristics.cpp` | `using arts::rt::CreateEpochOp` |
| `lib/arts/passes/opt/codegen/AliasScopeGen.cpp` | `using arts::rt::DepGepOp` |
| `lib/arts/analysis/value/ValueAnalysis.cpp` | `using arts::rt::DbGepOp` |
| `lib/arts/analysis/db/DbDistributedEligibility.cpp` | `using arts::rt::DbGepOp` |
| `include/arts/passes/transforms/EdtLoweringInternal.h` | `using arts::rt::DepDbAcquireOp` |
| `tools/compile/Compile.cpp` | Register `arts::rt::ArtsRtDialect` |
| `tools/compile/CMakeLists.txt` | Link `MLIRArtsRt` |
| `include/arts/passes/Passes.td` | Add `VerifyStructuralLowered` |
| 24 test files in `tests/contracts/` | `arts.X` -> `arts_rt.X` (sed) |

## Test Migration

```bash
sed -i \
  -e 's/arts\.edt_create/arts_rt.edt_create/g' \
  -e 's/arts\.edt_param_pack/arts_rt.edt_param_pack/g' \
  -e 's/arts\.edt_param_unpack/arts_rt.edt_param_unpack/g' \
  -e 's/arts\.rec_dep/arts_rt.rec_dep/g' \
  -e 's/arts\.inc_dep/arts_rt.inc_dep/g' \
  -e 's/arts\.dep_gep/arts_rt.dep_gep/g' \
  -e 's/arts\.dep_db_acquire/arts_rt.dep_db_acquire/g' \
  -e 's/arts\.db_gep/arts_rt.db_gep/g' \
  -e 's/arts\.create_epoch/arts_rt.create_epoch/g' \
  -e 's/arts\.wait_on_epoch/arts_rt.wait_on_epoch/g' \
  -e 's/arts\.state_pack/arts_rt.state_pack/g' \
  -e 's/arts\.state_unpack/arts_rt.state_unpack/g' \
  -e 's/arts\.dep_bind/arts_rt.dep_bind/g' \
  -e 's/arts\.dep_forward/arts_rt.dep_forward/g' \
  tests/contracts/*.mlir
```

## ARTS C Runtime Function Reference

Complete mapping of RT ops to C runtime functions:

| ARTS_RT Op | C Function | Signature |
|---|---|---|
| `edt_create` | `artsEdtCreate` | `arts_guid_t(arts_edt_t, uint32_t paramc, uint64_t *paramv, uint32_t depc, arts_hint_t*)` |
| `rec_dep` | `artsAddDependence[At][Ex]` | `void(arts_guid_t src, arts_guid_t dst, uint32_t slot, arts_db_access_mode_t [, uint64_t off, uint64_t len])` |
| `create_epoch` | `artsInitializeAndStartEpoch` | `arts_guid_t(arts_guid_t finish_edt, uint32_t slot)` |
| `wait_on_epoch` | `artsWaitOnHandle` | `bool(arts_guid_t epoch)` |
| `get_edt_epoch_guid` | `artsGetEdtEpochGuid` | `arts_guid_t()` |
| `edt_param_pack` | (none -- memref alloc+store) | N/A |
| `edt_param_unpack` | (none -- memref load+cast) | N/A |
| `state_pack` | (none -- memref alloc+store) | N/A |
| `state_unpack` | (none -- memref load+cast) | N/A |
| `dep_gep` | (none -- LLVM GEP on `arts_edt_dep_t`) | N/A |
| `dep_db_acquire` | (none -- ptr field load from dep struct) | N/A |
| `db_gep` | (none -- LLVM GEP for element addr) | N/A |
| `dep_bind` | (none -- identity, slot binding) | N/A |
| `dep_forward` | (none -- identity, CPS forwarding) | N/A |
| `inc_dep` | (none -- noop in v2) | N/A |

The `arts_edt_dep_t` runtime struct:
```c
struct arts_edt_dep_t {
  arts_guid_t guid;
  void *ptr;
  arts_db_access_mode_t mode;
  uint32_t flags;
  uint64_t slice_offset;  // ESD byte offset
  uint64_t slice_size;    // ESD byte size
};
```

## Implementation Order

```
Phase 1A: Create scaffold (additive, no breakage)
  1. Create directory structure
  2. Write RtDialect.td, RtOps.td
  3. Write RtDialect.h, RtDialect.cpp, RtOps.cpp
  4. Write CMakeLists.txt files
  5. Build and verify tablegen

Phase 1B: Migrate ops (breaking, mechanical)
  6. Remove 14 ops from Ops.td
  7. Remove DepAccessMode from Attributes.td
  8. Remove verifiers/builders from Dialect.cpp
  9. Add using declarations to 15 files
  10. Register dialect in Compile.cpp
  11. Build and fix errors

Phase 1C: Split ConvertToLLVM
  12. Extract shared header (ArtsToLLVMPattern base + ArtsCodegen*)
  13. Move 14 patterns to RtToLLVMPatterns.cpp
  14. Wire populateRtToLLVMPatterns into pass
  15. Build and verify

Phase 1D: Tests
  16. Run sed on 24 test files
  17. Run: dekk carts test
  18. Run benchmarks for regression check
```

## Verification

```bash
# Build
dekk carts build --clean

# All tests pass
dekk carts test

# No stale arts.X references for migrated ops
grep -rn 'arts\.\(edt_create\|rec_dep\|dep_gep\|create_epoch\|wait_on_epoch\)' \
  tests/contracts/*.mlir  # Should return nothing

# arts_rt ops appear in pre-lowering output
dekk carts compile tests/examples/jacobi/jacobi-for.c -O3 \
  --pipeline=pre-lowering 2>&1 | grep 'arts_rt\.'

# Spot-check benchmark
dekk carts benchmarks run --filter jacobi-for --size small --threads 4
```
