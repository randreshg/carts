# Sub-Dialect Restructuring Audit — 2026-04-10

10-agent parallel audit of branch `architecture/sde-restructuring` against
`docs/architecture/` specifications.

## Overall Verdict: Structurally Complete, Hygiene Gaps Remain

All 3 phases (RT extraction, SDE creation, folder reorganization) are
**functionally complete**. 168/168 tests pass. The pipeline correctly
implements `OMP -> SDE -> ARTS -> arts_rt -> LLVM`. Four categories of
gaps remain, none blocking.

---

## Phase 1: Extract `arts_rt` Dialect — COMPLETE

| Check | Status |
|-------|--------|
| RT scaffold (RtDialect.td, RtOps.td, RtDialect.h, .cpp) | Present |
| 14 ops removed from core Ops.td | Confirmed |
| DepAccessMode removed from core Attributes.td | Confirmed |
| RtToLLVMPatterns.cpp (14 patterns, 1841 LOC) | Present |
| Using declarations in 15+ consumer files | Present |
| Forwarding header (Dialect.h) exposes 16 `using rt::` decls | Correct |
| Dialect registered in Compile.cpp (registry + context) | Confirmed |
| 44 test files in tests/contracts/rt/ use `arts_rt.` prefix | Confirmed |
| Build artifacts (RtOps.h.inc 2986 lines) generated | Confirmed |

No issues found.

## Phase 2: Create SDE Dialect — COMPLETE (2C deferred)

| Check | Status |
|-------|--------|
| SDE scaffold (SdeDialect.td, SdeOps.td, SdeDialect.h, .cpp) | Present |
| 8 ops (CuRegion, CuTask, CuReduce, CuAtomic, SuIterate, SuBarrier, MuDep, Yield) | Confirmed |
| 5 enums (CuKind, AccessMode, ReductionKind, ScheduleKind, ConcurrencyScope) | Confirmed |
| ConvertOpenMPToSde.cpp (11 patterns, 653 LOC) | Present |
| SdeToArtsPatterns.cpp (8 patterns, 398 LOC) | Present (see gap #2) |
| CollectMetadata moved to sde/Transforms/ | Confirmed |
| VerifySdeLowered pass wired after SDE-to-ARTS | Confirmed |
| SDE dialect registered in Compile.cpp | Confirmed |
| Pipeline: OMP->SDE->ARTS replaces OMP->ARTS | Confirmed |
| RaiseToLinalg.cpp exists (structural classification) | Present (see note #3) |
| Deferred items (SdeTypes.td, SdeAttrs.td, SdeEffects.h, RaiseToTensor, SdeOpt) | Correctly absent |

## Phase 3: Full Folder Reorganization — COMPLETE

| Check | Status |
|-------|--------|
| 47 core transform passes in dialect/core/Transforms/ | All 47 present |
| Core IR (Dialect.td, Ops.td, Attributes.td, Types.td) in dialect/core/IR/ | Present |
| Forwarding .td headers at include/arts/ | Present, redirect correctly |
| ConvertOpenMPToArts in core/Conversion/OmpToArts/ | Present |
| ConvertArtsToLLVM + Patterns in core/Conversion/ArtsToLLVM/ | Present |
| EdtLowering + Support + EpochLowering in core/Conversion/ArtsToRt/ | Present |
| RtToLLVMPatterns in rt/Conversion/RtToLLVM/ | Present |
| DataPtrHoisting + Support, GuidRangCallOpt, RuntimeCallOpt in rt/Transforms/ | Present |
| Internal headers in include/arts/dialect/{core,rt}/... | Present |
| Old dirs (passes/transforms/, passes/opt/) removed | Confirmed |
| All include paths consistent, no stale #include refs | Confirmed |

## Shared Infrastructure — Undisrupted

| Layer | Files | Status |
|-------|-------|--------|
| analysis/ | 39 .cpp, 47 .h, 8 subdirs | Intact |
| utils/ | 17 .cpp, 19 .h | Intact |
| transforms/ (shared patterns) | 35 .cpp, 12 subdirs | Intact |
| codegen/ | 1 .cpp | Intact |
| passes/verify/ | 13 verify passes | Intact + VerifySdeLowered added |

## Pipeline Wiring — Fully Compliant

```
Stage 4 (OpenMPToArts):
  ConvertOpenMPToSde  ->  ConvertSdeToArts  ->  VerifySdeLowered
  (sde::)                 (sde::)                (arts::)

Stage 17 (PreLowering):
  ParallelEdtLowering  ->  DbLowering  ->  EdtLowering  ->  EpochLowering
  (core->core)              (core->core)    (core->rt)       (core->rt)

Stage 18 (ArtsToLLVM):
  populateRuntimePatterns()  ->  populateRtToLLVMPatterns()  ->  populateDbPatterns()
  (core ARTS ops)                (14 arts_rt ops)                 (core DB ops)
```

All 3 dialects registered in both `DialectRegistry` and `MLIRContext`.
Old `createConvertOpenMPToArtsPass()` completely removed from pipeline.

---

## Gap Analysis

### Gap 1 (Medium): CMake — No Per-Directory CMakeLists in core/

All 51+ source files in `lib/arts/dialect/core/` are compiled via a
**centralized monolithic** `lib/arts/passes/CMakeLists.txt` using relative
paths. No CMakeLists.txt exists in:

- `lib/arts/dialect/core/` (top-level, IR/, Transforms/, Conversion/)
- `lib/arts/dialect/rt/Transforms/`
- `lib/arts/dialect/core/Conversion/ArtsToRt/`
- `lib/arts/dialect/sde/Transforms/`

Also missing: `add_subdirectory(core)` in `lib/arts/dialect/CMakeLists.txt`.

The `include/arts/dialect/` layer has complete per-dialect CMake with proper
TableGen targets. The `lib/arts/` layer is still monolithic.

**Impact**: Build works. Doesn't match IREE per-dialect CMake pattern.
True separation requires per-directory `add_mlir_dialect_library()`.

### Gap 2 (Low): SdeToArtsPatterns.cpp Location Mismatch

| | Documented (directory-map.md) | Actual |
|---|---|---|
| Path | `lib/arts/dialect/core/Conversion/SdeToArts/` | `lib/arts/dialect/sde/Conversion/SdeToArts/` |

IREE convention: conversions live in the **target** dialect. OmpToArts is in
`core/Conversion/` (correct). SdeToArts should follow the same pattern.

### Gap 3 (Informational): RaiseToLinalg.cpp Exists Despite "Deferred" Label

`lib/arts/dialect/sde/Transforms/RaiseToLinalg.cpp` (383 LOC) exists and is
wired into the pattern pipeline. It does structural loop classification
(elementwise/matmul/stencil/reduction), not actual tensor raising. This is
effectively a Phase 2D structural annotation pass, distinct from the deferred
Phase 2C tensor-space pipeline (RaiseToTensor, SdeOpt, bufferize).

### Gap 4 (Low): EdtPtrRematerialization.cpp Duplicate

Exists in BOTH:
- `lib/arts/dialect/core/Transforms/edt/EdtPtrRematerialization.cpp` (241 LOC, implementation)
- `lib/arts/dialect/core/Transforms/EdtPtrRematerialization.cpp` (74 LOC, wrapper)

Both compiled due to CMake globbing. Potential ODR violation or dead code.

### Gap 5 (Trivial): Minor Documentation/Testing

- `lib/arts/passes/verify/README.md` missing VerifySdeLowered entry
- No dedicated lit test for VerifySdeLowered failure case
- VerifyLowered TableGen doesn't list `arts::rt::ArtsRtDialect` in dependentDialects
- SDE Conversion CMakeLists contain only placeholder comments

---

## Recommended Actions (Priority Order)

1. **Fix EdtPtrRematerialization duplicate** — merge into core/Transforms/
   or delete the wrapper. Both being compiled is wrong.

2. **Add per-directory CMakeLists stubs** — document structure, enable future
   decentralization. Even comment-only files help.

3. **Move SdeToArtsPatterns.cpp** to `core/Conversion/SdeToArts/` or update
   directory-map.md to match current location.

4. **Add VerifySdeLowered lit test** — inject residual `sde.*` op, verify
   the pass rejects it.

---

## Methodology

10 parallel exploration agents, each auditing a specific aspect:

1. RT dialect scaffold, ops, patterns
2. SDE dialect scaffold, ops, pipeline
3. Core dialect file moves (47 passes)
4. RT passes file moves (8 files)
5. CMakeLists.txt structure across all 3 dialects
6. Pipeline wiring in Compile.cpp
7. Verification passes and test infrastructure
8. Stale/residual files in old directories
9. Include paths and using declarations
10. Shared layers (analysis, utils, transforms, codegen)

Total agent tool invocations: ~562. Total agent runtime: ~30 minutes.
