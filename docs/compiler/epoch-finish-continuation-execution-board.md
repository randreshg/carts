# Epoch Finish-Continuation Execution Board

Tracks implementation status of the epoch finish-continuation feature as defined
in `docs/compiler/epoch-finish-continuation-rfc.md`.

## Status Legend

- **DONE**: Implemented, tested, merged
- **IN PROGRESS**: Currently being worked on
- **TODO**: Not yet started
- **BLOCKED**: Waiting on dependency

---

## Iteration 0: Foundation (CreateEpochOp Extension + LLVM Lowering)

| ID | Task | Status | Notes |
|----|------|--------|-------|
| EF-001 | Add `--arts-epoch-finish-continuation` CLI flag | DONE | `tools/compile/Compile.cpp` |
| EF-002 | Extend `CreateEpochOp` with optional `finishEdtGuid`/`finishSlot` | DONE | `include/arts/Ops.td`, `lib/arts/Dialect.cpp` |
| EF-003 | Update `ConvertArtsToLLVM` to emit `arts_initialize_and_start_epoch(guid, slot)` | DONE | `lib/arts/passes/transforms/ConvertArtsToLLVM.cpp` |
| EF-004 | Add IR-level tests for CreateEpochOp lowering | DONE | `tests/contracts/epoch_create_lowering.mlir`, `tests/contracts/epoch_create_verifier.mlir` |

## Iteration 1: Continuation Pass + EpochLowering Wiring

| ID | Task | Status | Notes |
|----|------|--------|-------|
| EF-006 | Create `EpochContinuationPrepPass` skeleton and registration | DONE | `lib/arts/passes/transforms/EpochContinuationPrep.cpp`, `Passes.h`, `Passes.td`, `CMakeLists.txt` |
| EF-007 | Implement safe pattern matcher (`isEligible`) | DONE | Rules 1-8 implemented; includes loop guard, nested epoch guard, empty region guard, terminator-uses-tail guard |
| EF-008 | Implement tail outlining into continuation EDT | DONE | `transformToContinuation()` with DB acquire capture, IRMapping-based cloning, and explicit `arts.yield` terminator |
| EF-009 | Wire `buildEpochsPipeline` to conditionally add prep pass | DONE | `Compile.cpp`: `buildEpochsPipeline(pm, EpochFinishContinuation)` |
| EF-010 | Add lit tests for epoch continuation prep | DONE | `tests/contracts/epoch_continuation_prep_simple.mlir` — tests both CONT and WAIT paths |
| EF-015 | Update `EpochLowering` for continuation path | DONE | Moves continuation EDT before epoch, wires `CreateEpochOp finish(guid, slot)`, skips `WaitOnEpochOp` |
| EF-016 | Update `EdtLowering` for control dep +1 | DONE | Adds +1 to depCount when `arts.has_control_dep` is set; propagates continuation attributes |

## Iteration 2: End-to-End Verification (TODO)

| ID | Task | Status | Notes |
|----|------|--------|-------|
| EF-020 | Full pipeline test (C source to LLVM IR) | TODO | Compile a C test with `--arts-epoch-finish-continuation` through `--stop-at=arts-to-llvm` |
| EF-021 | Verify `arts_initialize_and_start_epoch(guid, slot)` in emitted LLVM | TODO | |
| EF-022 | Verify no `arts_wait_on_handle` for continuation epochs in LLVM | TODO | |
| EF-023 | Runtime execution test (requires ARTS runtime) | TODO | Blocked on ARTS runtime availability |

## Iteration 3: Safety Hardening (TODO)

| ID | Task | Status | Notes |
|----|------|--------|-------|
| EF-030 | Loop-inside-epoch fallback test | DONE | `tests/contracts/epoch_continuation_loop_fallback.mlir` |
| EF-031 | Nested-epoch fallback test | DONE | `tests/contracts/epoch_continuation_nested_epoch_fallback.mlir` |
| EF-032 | Flag-disabled regression test | DONE | `tests/contracts/epoch_continuation_prep_simple.mlir` WAIT path |
| EF-033 | Distributed-node safety review | TODO | Multi-node continuation semantics |

---

## Key Design Decisions

1. **Pass ordering**: `EpochContinuationPrep` runs in `buildEpochsPipeline` after `CreateEpochs` but before pre-lowering (EdtLowering + EpochLowering).

2. **Cross-pass communication**: Uses marker attributes (`arts.continuation_for_epoch`, `arts.has_control_dep`) to communicate between EpochContinuationPrep -> EdtLowering -> EpochLowering.

3. **Dominance fix**: In `EpochLowering`, the continuation `EdtCreateOp` and its operands are moved BEFORE the epoch so that `CreateEpochOp` can reference the finish GUID/slot while still dominating the worker EDTs inside the epoch.

4. **Control slot**: The finish slot index is `depCount - 1`, where `depCount` includes the +1 for the control dep added by EdtLowering.

5. **EDT body terminator**: `EpochContinuationPrep` explicitly adds `arts.yield` to the continuation EDT body since the programmatic `EdtOp` builder does not add an implicit terminator.

## Bug Fixes During Implementation

- **Rule 8 (terminator-uses-tail)**: Added guard to reject patterns where the block terminator uses values defined by tail operations. Without this, erasing tail ops causes "operation destroyed but still has uses" crash.
- **Missing `arts.yield`**: EDT body created by `builder.create<EdtOp>()` doesn't get an implicit terminator from `SingleBlockImplicitTerminator` trait since the builder creates the block manually.
- **Dominance ordering**: `CreateEpochOp` must be placed after `finishSlot` computation but before worker EDTs. Solved by moving continuation EDT and its operands before the epoch.
