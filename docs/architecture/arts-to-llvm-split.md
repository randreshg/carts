# ArtsToLLVM / RtToLLVM Split

## Overview

The final LLVM lowering stage (stage 18, `ConvertArtsToLLVM`) converts both
**core ARTS ops** and **RT dialect ops** into LLVM dialect operations (runtime
function calls). Although this is a single pass, the conversion patterns are
split across two source files with a clear ownership boundary.

## Why Two Pattern Files?

The 3-dialect architecture is:

```
SDE (semantic contracts)  -->  core ARTS (structural)  -->  RT (runtime 1:1 mapping)
     stages 1-4                    stages 5-13                stages 14-15
```

After ArtsToRt conversion (stages 14-15), the IR contains a mix of:

1. **Core ARTS ops** that were never lowered to RT -- DB ops (`db_alloc`,
   `db_acquire`, `db_release`, `db_free`, `db_ref`, `db_dim`, `db_control`,
   `db_num_elements`), control flow (`yield`, `barrier`, `atomic_add`,
   `runtime_query`, `get_edt_epoch_guid`), and memory (`alloc`, `undef`).
2. **RT ops** created by EdtLowering/EpochLowering -- `edt_create`, `rec_dep`,
   `inc_dep`, `dep_gep`, `dep_db_acquire`, `db_gep`, `edt_param_pack`,
   `edt_param_unpack`, `create_epoch`, `wait_on_epoch`, `state_pack`,
   `state_unpack`, `dep_bind`, `dep_forward`.

These are kept separate because:

- **Core ops** carry semantic information (partition modes, contract attrs,
  access patterns) that core transforms read and refine across stages 5-13.
  They cannot move to RT without losing this multi-stage refinement.
- **RT ops** are thin 1:1 wrappers around `arts_*` runtime calls. They have
  no semantic payload -- each maps directly to one runtime function.
- Keeping patterns in separate files mirrors the dialect ownership: core
  patterns live in `core/Conversion/ArtsToLLVM/`, RT patterns live in
  `rt/Conversion/RtToLLVM/`.

## How They Connect

The `ConvertArtsToLLVM` pass (`ConvertArtsToLLVM.cpp`) runs four sequential
pattern applications in a fixed order:

```
Run 1: populateRuntimePatterns()   -- core ops: RuntimeQuery, Barrier, AtomicAdd, etc.
Run 2: populateRtToLLVMPatterns()  -- ALL RT ops: epochs, EDTs, deps, state, bind
Run 3: populateDbPatterns()        -- core DB ops: DbAlloc, DbAcquire, DbRelease, etc.
Run 4: populateOtherPatterns()     -- remaining core ops: Alloc, Yield, Undef, etc.
```

All four `populate*` functions are declared in the **shared internal header**:
`include/arts/dialect/core/Conversion/ArtsToLLVM/ConvertArtsToLLVMInternal.h`

This header also provides:

- `ArtsToLLVMPattern<OpType>` -- base class holding an `ArtsCodegen*` pointer,
  used by all patterns in both files.
- `ArtsHintBuilder` -- LLVM struct builder for route/artsId hint memrefs.
- Shared helper functions (`buildArtsHintMemref`, `resolveOuterSizesForGuid`,
  `resolveSourceOuterSizes`, `materializeStaticDbOuterShape`).

The RT patterns file (`rt/Conversion/RtToLLVM/RtToLLVMPatterns.cpp`) includes
this internal header and uses the same `ArtsToLLVMPattern` base class and
shared helpers. This means RT patterns have access to the same `ArtsCodegen`
infrastructure as core patterns without any code duplication.

## File Map

```
include/arts/dialect/core/Conversion/ArtsToLLVM/
  ConvertArtsToLLVMInternal.h    -- shared base class, helpers, populate decls

lib/arts/dialect/core/Conversion/ArtsToLLVM/
  ConvertArtsToLLVM.cpp          -- pass driver (4 sequential pattern runs)
  ConvertArtsToLLVMPatterns.cpp  -- core ARTS op patterns + shared helper defs

lib/arts/dialect/rt/Conversion/RtToLLVM/
  RtToLLVMPatterns.cpp           -- RT op patterns (populateRtToLLVMPatterns)
```

## Op Ownership Summary

### Core ARTS ops (lowered by ConvertArtsToLLVMPatterns.cpp)

| Op | Why it stays in core |
|----|---------------------|
| `DbAllocOp` | Created by CreateDbs (stage 7), carries partition_mode/access_pattern refined through stage 10 |
| `DbAcquireOp` | Created by CreateDbs (stage 7), carries partition hints + dep patterns read by stages 5-14 |
| `DbReleaseOp` | Paired with DbAcquireOp |
| `DbFreeOp` | Paired with DbAllocOp |
| `DbRefOp` | Used across DB transform stages |
| `DbDimOp` | Canonicalized by DB-specific folds |
| `DbControlOp` | Control dependency, created during OmpToArts |
| `DbNumElementsOp` | Used by EdtLowering for dep counts |
| `LoweringContractOp` | First-class contract carrier, read by all downstream stages |
| `EdtOp` | Body region restructured by core transforms (stages 5-13) |
| `EpochOp` | Restructured by EpochOpt (stage 13) |
| `ForOp` | Normalized/reordered (stages 6-7), lowered by ForLowering (stage 16) |
| `CPSAdvanceOp` | CPS chain placeholder, resolved by EpochLowering (stage 15) |
| `RuntimeQueryOp` | Direct runtime call |
| `BarrierOp` | Direct runtime call |
| `AtomicAddOp` | Direct LLVM atomicrmw |
| `YieldOp` | Implicit terminator for EdtOp/EpochOp/ForOp/CPSAdvanceOp |
| `AllocOp` | Direct LLVM alloca |
| `UndefOp` | General-purpose SSA placeholder |
| `OmpDepOp` | OpenMP dependency descriptor |
| `GetEdtEpochGuidOp` | CPS chain epoch propagation |

### RT ops (lowered by RtToLLVMPatterns.cpp)

| Op | Runtime call |
|----|-------------|
| `CreateEpochOp` | `arts_epoch_create` / `arts_epoch_create_no_start` |
| `WaitOnEpochOp` | `arts_wait_on_handle` |
| `EdtCreateOp` | `arts_edt_create` |
| `EdtParamPackOp` | memref alloc + store (paramv packing) |
| `EdtParamUnpackOp` | memref load + cast (paramv unpacking) |
| `RecordDepOp` | `arts_add_dependence` |
| `IncrementDepOp` | `arts_signal_edt` |
| `DepGepOp` | pointer arithmetic on depv struct |
| `DepDbAcquireOp` | depv struct access + `arts_db_acquire` |
| `DbGepOp` | byte-stride pointer arithmetic |
| `StatePackOp` | memref alloc + store (CPS state) |
| `StateUnpackOp` | memref load + cast (CPS state) |
| `DepBindOp` | pass-through (GUID = slot at LLVM level) |
| `DepForwardOp` | pass-through (identity at LLVM level) |

## Shared Helpers (No Duplication)

All helper functions used by both pattern files are defined once in
`ConvertArtsToLLVMPatterns.cpp` inside namespace
`mlir::arts::convert_arts_to_llvm` and declared in
`ConvertArtsToLLVMInternal.h`:

- `buildArtsHintMemref()` -- builds route/artsId hint struct memref
- `resolveOuterSizesForGuid()` -- resolves DB outer dimensions from alloc/acquire chain
- `resolveSourceOuterSizes()` -- resolves sizes from sourceGuid/sourcePtr
- `materializeStaticDbOuterShape()` -- materializes static shape as index constants

The `ArtsToLLVMPattern<OpType>` base class is also shared. RT patterns use it
identically to core patterns -- no parallel class hierarchy, no duplication.
