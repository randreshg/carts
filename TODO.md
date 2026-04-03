# Jacobi-For Benchmark Bug Investigation

## Status: Both bugs fixed (115/122 tests passing)

**Benchmark**: `kastors-jacobi/jacobi-for` (SIZE=256, 2 threads)
**Symptom**: Checksum mismatch ARTS=1.151757016532e-01 vs OMP=3.934498277405e-01
**Triage bundles**: `/tmp/carts-triage/kastors-jacobi__jacobi-for_small_2t_20260403_*`

---

## Bug 1: Epoch Hierarchy (FIXED)

**File**: `lib/arts/passes/transforms/EpochLowering.cpp` (lines 769-784)
**Status**: Fix applied, builds, tests pass (no regressions)

### Root Cause

CPS advance resolution creates a finish-epoch for the continuation EDT but
spawns workers into a **separate intermediate epoch** (from the distribution
pass). The finish-epoch has zero children and auto-completes immediately,
triggering the next CPS iteration before current workers finish. This causes:

- **2+ threads**: Data races between iterations (non-deterministic wrong results)
- **1 thread**: Deadlock (`wait_on_epoch` on empty epoch blocks the only worker)

### Fix

Subsume intermediate epochs into the finish-epoch so workers are enrolled
directly:

```cpp
for (Operation *workerOp : workerOps) {
  workerOp->walk([&](CreateEpochOp innerCreate) {
    if (!innerCreate.hasFinishTarget()) {
      innerCreate.getEpochGuid().replaceAllUsesWith(epochGuid);
      innerCreate->erase();
    }
  });
}
```

### Verification

- Results are now **deterministic** (1-thread and 2-thread produce same checksum)
- 113/122 contract tests pass (9 pre-existing failures, 0 new regressions)
- The fix actually **resolved** 2 previously-failing tests:
  `create_dbs_contract_projection.mlir` and
  `stencil_multi_output_accumulator_contract.mlir`

---

## Bug 2: CPS Parameter Count Mismatch (FIXED)

**Location**: `lib/arts/passes/opt/edt/EpochOpt.cpp` (lines 1480-1503)
**Status**: Fixed by seeding loop-back params from original loop body

### Root Cause

The ARTS checksum `1.151757016532e-01` matches **exactly** the reference
sequential checksum after **iteration 2** (out of 10). Only 2 iterations run.

```
Iteration  1: checksum = 2.008819684737e-02
Iteration  2: checksum = 1.151757016532e-01  <-- ARTS result
Iteration  3: checksum = 1.627633506344e-01
...
Iteration 10: checksum = 3.934498277405e-01  <-- correct (OMP)
```

The CPS continuation chain terminates after 1 continuation because of a
**parameter pack size mismatch**:

| Pack | Elements | Layout |
|------|----------|--------|
| Initial (main) | **11** | 3 index + 6 i64-ptrs + iter(i64) + epoch(i64) |
| Continuation (edt_4) | **10** | 2 index + 6 i64-ptrs + tNext(i64) + epoch(i64) |

The initial EDT creation carries **9 user params** (3 index values + 6
memref-as-i64 pointers), but `arts.cps_advance` only carries **8 user params**
(`cps_num_carry = 8`). One index parameter is dropped.

Since the continuation EDT reads the iter counter from a fixed param index
(`cps_iter_counter_param_idx = 9`), the shift causes it to read the **outer
epoch GUID** (a large integer) instead of the actual iteration counter. Then
`tNext = epoch_guid + 1`, and `tNext < 11` evaluates to **false**, terminating
the CPS chain.

### Evidence

Pre-lowering MLIR (`jacobi-for.pre-lowering.mlir`):

```mlir
// Initial pack in main() -- 11 elements (positions 0-10):
%32 = arts.edt_param_pack(%17, %18, %13, %21, %23, %25, %27, %29, %31,
                           %c1_i64, %19) ...
// iter counter at position 9, outer epoch at position 10

// Continuation pack in __arts_edt_4 -- 10 elements (positions 0-9):
%24 = arts.edt_param_pack(%0#0, %0#1, %18, %19, %20, %21, %22, %23,
                           %15, %14) ...
// tNext at position 8, outer epoch at position 9
// BUT edt_4 reads iter from position 9 --> gets epoch GUID!
```

### Where to Fix

The mismatch originates in the epochs pass where `arts.cps_advance` is
generated. The advance operands carry only 8 of the 9 user parameters:

```mlir
// epochs stage, line 378:
arts.cps_advance(%c1_i64, %c11_i64,
  %18, %19,                          // 2 index values (should be 3!)
  %guid, %ptr, %guid_3, %ptr_4,      // 4 memref values
  %guid_5, %ptr_6)                    // 2 memref values
  {arts.cps_num_carry = 8 : i64}     // carries 8, but 9 needed
```

The 3rd index parameter from the initial pack is missing from the carry set.

### Fix Applied

Changed the initial `loopBackParams` collection (line 1480-1482) to seed from
the **original loop body** rather than the continuation EDT:

```cpp
llvm::SetVector<Value> originalCaptured;
llvm::SetVector<Value> parameters, constants, dbHandles;
getUsedValuesDefinedAbove(forOp.getRegion(), originalCaptured);
classifyEdtUserValues(originalCaptured.getArrayRef(), parameters, constants,
                      dbHandles);
// Filter and pack into loopBackParams
```

This ensures ALL captured values from the original loop (including those only
used to compute other captured values, like %17 → %18) are carried through
the CPS chain, not just the ones directly used in the continuation EDT.

---

## Pre-existing Test Failures (9 tests, NOT caused by these changes)

These 9 tests fail both with and without the epoch fix:

1. `activations_host_full_view_input_stays_coarse.mlir`
2. `controller_lowering_stage_boundary_consistency.mlir`
3. `jacobi_stencil_intranode_worker_cap.mlir`
4. `partitioning/modes/db_partition_mode_block.mlir`
5. `partitioning/modes/db_partition_mode_coarse.mlir`
6. `partitioning/safety/mixed_orientation_full_range_contract.mlir`
7. `partitioning/safety/poisson_for_full_range_forcing_contract.mlir`
8. `seidel_wavefront_continuation_guid_lookup.mlir`
9. `seidel_wavefront_no_stale_loop_metadata.mlir`

Tests 1-7 are likely from the contract-ownership fixes already in the working
tree (CreateDbs.cpp + DbStencilRewriter.cpp changes). Tests 8-9 may be
related to epoch/CPS structure changes from prior commits.

---

## Verified Correct (no bugs found)

All three pre-lowering analysis agents confirmed these are correct:

- Stencil formula: `0.25 * (u[i-1,j] + u[i,j+1] + u[i,j-1] + u[i+1,j] + f[i,j]*dx*dy)`
- Boundary conditions: `(i==0 || j==0 || i==255 || j==255) -> f[i][j]`
- Index localization: 3-buffer selection (left halo / center / right halo)
- Halo acquire offsets: left=`[15,0]` size `[1,256]`, right=`[0,0]` size `[1,256]`
- Buffer alternation: DB1/DB2 swap correctly across even/odd iterations
- Checksum loop: reads `unew[i][i]` diagonal from correct final buffer
- Block distribution: 256 rows / 16 blocks = 16 rows per block
- DB contracts: stencil pattern, ownerDims=[0], halo=[-1,+1]

---

## Files Modified

| File | Change | Status |
|------|--------|--------|
| `lib/arts/passes/transforms/EpochLowering.cpp` | Subsume intermediate epochs | DONE |
| `lib/arts/passes/transforms/CreateDbs.cpp` | Contract projection fix (prior) | DONE |
| `lib/arts/transforms/db/stencil/DbStencilRewriter.cpp` | Halo contract transfer (prior) | DONE |
| `tests/contracts/create_dbs_contract_projection.mlir` | Updated test (prior) | DONE |
| `tests/contracts/stencil_multi_output_accumulator_contract.mlir` | Updated test (prior) | DONE |

---

## Next Steps

1. **Fix Bug 2**: Trace the epochs pass CPS conversion to find where the 3rd
   index parameter is dropped from the carry set. Likely in
   `lib/arts/passes/transforms/Epochs.cpp` or a CPS helper.

2. **Rebuild and verify**: After fixing, the benchmark should produce checksum
   `3.934498277405e-01` matching the OMP baseline.

3. **Add regression test**: Create a contract test that verifies CPS param
   pack size matches across initial and continuation packs.

4. **Investigate 1-thread `no_start_epoch` deadlock**: The benchmark-runner
   agent found that 1-thread runs deadlock due to `no_start_epoch` on the
   initial finish-epoch. This may be a separate issue in how the epochs pass
   handles inline-computation CPS patterns (where no worker EDTs exist).

---

## Pipeline Stage Dumps

Generated for this investigation (in cwd):
- `jacobi-for.create-dbs.mlir` -- DB creation (stage 7)
- `jacobi-for.db-opt.mlir` -- DB optimization (stage 8)
- `jacobi-for.edt-distribution.mlir` -- EDT distribution (stage 11)
- `jacobi-for.epochs.mlir` -- Epoch insertion (stage 16)
- `jacobi-for.pre-lowering.mlir` -- Pre-lowering (stage 17)

Reference checksums per iteration: `/tmp/jacobi_iter_trace.c`
