# Phase 1: Extract `arts_rt` Dialect

The cleanest boundary -- 14 ops created only in pre-lowering (stage 17),
consumed only by ConvertArtsToLLVM (stage 18), with zero type dependency
on core `arts`.

## Ops to Extract

| Op | Ops.td Line | HasVerifier |
|---|---|---|
| CreateEpochOp | 770 | yes |
| WaitOnEpochOp | 803 | no |
| EdtCreateOp | 951 | no (has builder) |
| EdtParamPackOp | 979 | no |
| EdtParamUnpackOp | 990 | no |
| RecordDepOp | 1000 | yes |
| IncrementDepOp | 1050 | no |
| DepGepOp | 1072 | no |
| DepDbAcquireOp | 1094 | no |
| DbGepOp | 1128 | no (has builder) |
| StatePackOp | 1157 | no |
| StateUnpackOp | 1174 | no |
| DepBindOp | 1191 | no |
| DepForwardOp | 1207 | no |

`DepAccessMode` enum also moves. `RuntimeQueryKind` stays in core.

## Directory Structure

```
include/arts/dialect/rt/IR/
  RtDialect.td          # Dialect definition + ArtsRt_Op base class
  RtOps.td              # 14 runtime ops (moved from Ops.td)
  RtDialect.h           # Public C++ header
  CMakeLists.txt

lib/arts/dialect/rt/
  IR/
    RtDialect.cpp       # ArtsRtDialect::initialize()
    RtOps.cpp           # Verifiers: CreateEpochOp, RecordDepOp; Builders: EdtCreateOp, DbGepOp
    CMakeLists.txt
  Conversion/RtToLLVM/
    RtToLLVMPatterns.cpp  # 14 patterns from ConvertArtsToLLVMPatterns.cpp
    CMakeLists.txt
```

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
- `ArtsToLLVMPattern<OpType>` template
- `ArtsCodegen*` reference

**Move to RtToLLVMPatterns.cpp** (14 patterns):
- CreateEpochPattern, WaitOnEpochPattern
- EdtParamPackPattern, EdtParamUnpackPattern
- EdtCreatePattern, DepGepOpPattern
- RecordDepPattern, IncrementDepPattern
- DepDbAcquireOpPattern, DbGepOpPattern
- StatePackPattern, StateUnpackPattern
- DepBindPattern, DepForwardPattern

**Stay in ConvertArtsToLLVMPatterns.cpp** (core arts -> LLVM):
- DbAllocPattern, DbAcquirePattern, DbRefPattern, DbReleasePattern, DbFreePattern
- BarrierPattern, AtomicAddPattern, RuntimeQueryPattern
- UndefPattern, AllocPattern, YieldPattern, etc.

## Files to Modify

| File | Change |
|---|---|
| `include/arts/Ops.td` | Remove 14 ops |
| `include/arts/Attributes.td` | Remove DepAccessMode |
| `lib/arts/Dialect.cpp` | Remove 2 verifiers + 2 builders |
| `lib/arts/passes/transforms/EdtLowering.cpp` | Add `using arts::rt::*` declarations |
| `lib/arts/passes/transforms/EpochLowering.cpp` | Add `using arts::rt::*` declarations |
| `lib/arts/passes/transforms/ForLowering.cpp` | Add `using arts::rt::*` declarations |
| `lib/arts/passes/transforms/ConvertArtsToLLVMPatterns.cpp` | Extract 14 patterns |
| `lib/arts/analysis/ValueAnalysis.cpp` | Add `using arts::rt::DbGepOp` |
| `lib/arts/passes/transforms/EdtLoweringSupport.cpp` | Add `using` declaration |
| `lib/arts/passes/opt/epoch/EpochOptScheduling.cpp` | Add `using` declaration |
| `tools/compile/Compile.cpp` | Register `arts::rt::ArtsRtDialect` |
| `tools/compile/CMakeLists.txt` | Link `MLIRArtsRt` |
| `include/arts/passes/Passes.td` | Add VerifyStructuralLowered |
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
  9. Add using declarations to lowering passes
  10. Register dialect in Compile.cpp
  11. Build and fix errors

Phase 1C: Split ConvertToLLVM
  12. Extract shared header
  13. Move 14 patterns to RtToLLVMPatterns.cpp
  14. Wire populateRtToLLVMPatterns into pass
  15. Build and verify

Phase 1D: Tests
  16. Run sed on test files
  17. Run: dekk carts test
  18. Run benchmarks for regression check
```
