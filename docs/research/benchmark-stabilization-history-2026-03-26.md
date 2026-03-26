# Benchmark Stabilization History and Current Status

Date: 2026-03-26

## Purpose

This document is the handoff record for the current CARTS benchmark
stabilization effort. It captures:

- the original scope and constraints
- the major compiler and runtime workstreams that were touched
- the last benchmark and contract results we observed
- the current staged and unstaged worktree state
- the next validation steps required before committing

## Ground Rules

- Always use `carts` or `tools/carts`.
- `carts clang` is the source of truth for the compiler toolchain path.
- Do not hardcode pipeline stage names in debugging tools. Query the manifest
  with `tools/carts pipeline` or `tools/carts pipeline --json`.
- Do not change the ARTS or CARTS distributed execution model. Any new
  dependency flags are advisory hints only.
- Submodule changes must be committed in the submodule first, then the parent
  repo should update the submodule SHA.

## Current Compiler Pipeline Reference

The current pipeline order reported by `tools/carts pipeline` is:

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

Use the manifest, not this snapshot, as the source of truth when debugging.

## Investigation History

### 1. Tooling, pipeline introspection, and agent workflow

The first thread of work was around making CARTS easier to debug and less
fragile for repeated compiler investigation.

- `tools/carts pipeline` is now the authoritative source for valid pipeline
  names and pass ordering.
- `carts triage-benchmark` was exposed from the main CLI so benchmark triage can
  be driven through CARTS instead of ad hoc scripts.
- `carts agents` and `carts install --agents` were exposed so repo-managed
  skills can be installed into a Codex-visible skills directory.
- The benchmark triage flow now aligns with the pipeline manifest and the
  benchmark runner layout instead of relying on a hardcoded stage list.

Relevant files:

- `tools/carts_cli.py`
- `tools/scripts/install.py`
- `tools/scripts/triage.py`

### 2. OpenMP helper inlining and raise-memref correctness

The next thread was correctness work around OpenMP lowering and
`raise-memref-dimensionality`.

#### OpenMP helper inlining

Problem:

- Generic MLIR inlining can assert when a call is nested inside an OMP region
  because the OMP dialect does not provide a `DialectInlinerInterface`.

What changed:

- `lib/arts/passes/opt/general/Inliner.cpp` now detects calls inside OMP
  regions.
- Single-block `func.func` callees are manually cloned inline as a conservative
  fallback.
- Multi-block callees remain intentionally uninlined instead of crashing.

Why this matters:

- This unblocks OpenMP-source benchmarks where helper routines appear inside
  OMP regions after Cgeist / Polygeist lowering.

#### N-D raise-memref dimensionality

Problem:

- The previous implementation handled only shallow aliasing patterns well.
- Wrapper alias chains created during inlining could hide the canonical root.
- `polygeist.memref2pointer` null checks could remain attached to aliased values
  instead of the canonical N-D allocation.

What changed:

- `lib/arts/passes/opt/general/RaiseMemRefDimensionality.cpp` now discovers
  wrapper alias chains recursively.
- Root tracing now accepts any alias wrapper that resolves to the same root.
- Traced `memref2pointer` users are rewritten to the canonical N-D allocation
  when they are root-equivalent.

Why this matters:

- This is the basis for robust N-D handling, not just 1-D and 2-D cases.
- It avoids regressions when helper inlining creates additional wrapper layers.

### 3. `arts.rec_dep` advisory duplicate hints, including the ESD path

This was the central runtime/compiler workstream.

#### Goal

Allow the compiler to attach advisory per-dependency flags to `arts.rec_dep`,
so the runtime can preserve duplicate-friendly read-only hints all the way to
DB acquisition time. The hint must survive both:

- standard dependence registration
- byte-sliced ESD dependence registration

#### Design

- The hint is advisory only.
- It does not change dependence semantics, scheduling semantics, or distributed
  correctness semantics.
- The current flag is `ARTS_DEP_FLAG_PREFER_DUPLICATE`.
- The flag is stored per EDT slot and consulted during DB acquisition for
  duplicate-friendly read-only handling.

#### Compiler-side changes

`RecordDepOp` was extended to carry optional `dep_flags`:

- `include/arts/Ops.td`
- `lib/arts/Dialect.cpp`

`EdtLowering` now computes the hint:

- `lib/arts/passes/transforms/EdtLowering.cpp`

Current lowering policy:

- if the allocation is distributed
- and the dependency mode is read-only
- and the allocation is safe to duplicate, typically because it is
  `arts.read_only_after_init`

then `dep_flags = 1` is attached for that dependency slot.

`ConvertArtsToLLVM` now preserves the hint through both lowering paths:

- standard dependency path: `arts_add_dependence_ex`
- byte-sliced ESD path: `arts_add_dependence_at_ex`
- no hint present: keep existing non-`_ex` calls

Relevant file:

- `lib/arts/passes/transforms/ConvertArtsToLLVM.cpp`

#### Runtime-side changes in `external/arts`

The runtime was extended so the hint is preserved through local, remote, and
out-of-order paths.

Public API and slot metadata:

- `external/arts/libs/include/public/arts.h`

Internal signaling and EDT slot metadata:

- `external/arts/libs/include/internal/arts/compute/edt.h`
- `external/arts/libs/src/core/compute/edt.c`

Event-based dependence registration, including the ESD entry point:

- `external/arts/libs/src/core/sync/event.c`

Out-of-order replay support:

- `external/arts/libs/include/internal/arts/gas/out_of_order.h`
- `external/arts/libs/src/core/gas/out_of_order.c`

Remote packet propagation:

- `external/arts/libs/include/internal/arts/remote/handler.h`
- `external/arts/libs/src/core/remote/handler.c`
- `external/arts/libs/include/internal/arts/transport/protocol.h`
- `external/arts/libs/src/core/transport/dispatcher.c`

Acquire-time duplicate-friendly read-only handling:

- `external/arts/libs/src/core/memory/db.c`

#### Current consistency status

The end-to-end story is coherent:

- `arts.rec_dep` can now carry `dep_flags`
- `EdtLowering` can derive the duplicate hint
- LLVM lowering emits `_ex` runtime calls when flags are present
- the runtime preserves mode and flags on the EDT slot
- local, remote, and OOO paths all carry the flags
- the ESD path now has a dedicated `_at_ex` lowering path

Remaining work in this area is mainly validation and test promotion, not basic
design.

### 4. Jacobi and stencil partitioning fixes

Another workstream focused on large-thread-count Jacobi and stencil behavior.

Problem:

- intranode stencil kernels could overdecompose worker ownership too far at high
  thread counts
- fallback DB block sizing could become inconsistent with the worker-coarsening
  assumptions used elsewhere in the pipeline
- the result was widened dependency windows and incorrect Jacobi-style halo
  behavior at high thread counts

What changed:

- `lib/arts/analysis/heuristics/DistributionHeuristics.cpp` now caps intranode
  stencil worker use based on a minimum owned outer span
- `lib/arts/passes/opt/db/DbPartitioning.cpp` and
  `lib/arts/transforms/db/DbBlockPlanResolver.cpp` now clamp stencil fallback
  workers for intranode stencil allocations

Result:

- the Jacobi worker-cap issue was fixed
- a dedicated contract was added to guard the regression

Relevant files:

- `include/arts/transforms/db/DbBlockPlanResolver.h`
- `lib/arts/transforms/db/DbBlockPlanResolver.cpp`
- `lib/arts/passes/opt/db/DbPartitioning.cpp`
- `tests/contracts/jacobi_stencil_intranode_worker_cap.mlir`

### 5. Seidel 2-D wavefront tuning

The main remaining performance bug is `polybench/seidel-2d` at `large / 64`.

Problem:

- the Seidel wavefront transform was choosing a macro-tile shape that looked
  reasonable in terms of raw tile counts
- but the downstream `tiling_2d` lowering expands the work further across
  column-worker lanes
- this caused the effective task count seen by the runtime to be higher than
  the transform was budgeting for

What changed:

- `lib/arts/transforms/dep/Seidel2DWavefrontPattern.cpp` now scores effective
  task counts after `tiling_2d` lane expansion instead of raw macro-tile count
- the heuristic now penalizes lane-misaligned column counts and undersubscribed
  lane usage

Current status:

- the generated shape improved relative to the original failing artifact
- the benchmark still times out at `large / 64`

Most useful current clue:

- `/tmp/seidel_inspect_latest.mlir` now shows:
  - `arts.db_alloc ... sizes[%c3, %c8]`
  - `elementSizes[%c4799, %c1200]`
  - `stencil_block_shape = [4799, 1200]`

Strong current hypothesis:

- the transform still effectively lands on a `3 x 8` macro-tile plan when a
  `2 x 8`-style plan would be preferable
- either the worker count seen by the wavefront chooser is stale or the scoring
  still prefers too many row tiles

### 6. `rhs4sg-base` correctness diagnosis

The other remaining benchmark blocker is `sw4lite/rhs4sg-base` at `large / 64`.

Problem:

- the ARTS run completes, but verification is incorrect against the OpenMP
  reference

Current observed mismatch:

- ARTS checksum: `8.987003514767e+02`
- OMP checksum: `1.527648351669e+03`

Current hypothesis:

- the bug is in the 4-D blocked or indexed path, especially in acquire
  offsets, sizes, owner-dimension handling, or local index rewriting

Important comparison artifacts:

- `/tmp/rhs4sg_concurrency-opt_fromraw.mlir`
- `/tmp/vel4sg_pipelines/vel4sg_base_concurrency-opt.mlir`

Useful current observation:

- `rhs4sg-base` allocates 4-D blocked/indexed DBs in
  `concurrency-opt`, while `vel4sg-base` is a simpler passing comparison point
  with 3-D blocked paths

### 7. Task-loop aligned-bounds cleanup

`lib/arts/transforms/edt/EdtTaskLoopLowering.cpp` was updated so task-local loop
bounds are always derived from the true loop bounds and the worker base offset,
not only when `useAlignedLowerBound` is set.

There is one related unstaged semantic follow-up in
`lib/arts/passes/transforms/ForLowering.cpp` that now always collects the
original upper bound for rewiring dependencies.

This should be validated together with its contract before commit.

### 8. Benchmark-source determinism fix

`external/carts-benchmarks/polybench/convolution-2d/convolution-2d.c` was
updated to zero the boundary of `B` before the kernel executes so checksum
comparison stays deterministic across ARTS and the OpenMP reference build.

## Current Worktree State

### Main repo staged work

The main repo currently has a large staged chunk covering:

- `RecordDepOp` dep-flags plumbing
- `EdtLowering` hint derivation
- LLVM lowering to `_ex` runtime calls
- OMP-region helper inlining fallback
- N-D raise-memref alias-chain and null-check handling
- Jacobi stencil worker-cap and fallback DB block sizing fixes
- Seidel wavefront heuristic tuning
- agent-friendly CLI changes
- contract tests for several of the above

### Main repo unstaged or untracked work

There is still some work not yet promoted into the staged chunk:

- semantic follow-up in `lib/arts/passes/transforms/ForLowering.cpp`
- untracked contracts:
  - `tests/contracts/dep_flags_edt_lowering_distributed_ro.mlir`
  - `tests/contracts/inliner_multiblock_omp_skip.mlir`
  - `tests/contracts/record_dep_esd_duplicate_flag_emit_llvm.mlir`
  - `tests/contracts/record_dep_no_flags_standard_call.mlir`
  - `tests/contracts/task_loop_aligned_bounds_unconditional.mlir`

There is also an unrelated untracked local file:

- `amd_llm_test.py`

That file should not be folded into this work.

### `external/arts` staged work

`external/arts` contains staged runtime changes for:

- public dependency flags
- EDT slot metadata propagation
- remote packet support
- out-of-order propagation
- ESD `_at_ex` support
- acquire-time duplicate-friendly RO handling

### `external/carts-benchmarks` staged work

`external/carts-benchmarks` currently contains one staged source fix:

- deterministic boundary initialization for `polybench/convolution-2d`

## Latest Recorded Validation Results

### 1. Last known full medium sweep

Results bundle:

- `external/carts-benchmarks/results/20260326_035354`

Manifest:

- command: `carts benchmarks run --size medium --threads 1,2,4,8 --timeout 120 --trace`

Summary from `results.json`:

- `total_configs = 92`
- `total_runs = 92`
- `passed = 92`
- `failed = 0`
- `failures = []`

Interpretation:

- the latest recorded broad benchmark sweep is healthy for the medium dataset
  at `1,2,4,8` threads
- this is the newest full-suite data point currently available
- it predates some current uncommitted changes, so it must be rerun after the
  worktree is finalized

### 2. Last known contracts result

Last known contracts status from the investigation:

- `tools/carts lit --suite contracts` passed `52/52`

Important caveat:

- this happened before the newest untracked contract coverage was added
- contracts must be rerun after the remaining tests are staged

### 3. Large `64-thread` targeted failures from the investigation

#### `polybench/seidel-2d`

Results bundle:

- `external/carts-benchmarks/results/20260325_050621`

Observed result:

- `build_arts = pass`
- `build_omp = pass`
- `run_arts = timeout`
- timeout duration about `180.17 sec`

Artifact paths:

- `external/carts-benchmarks/results/20260325_050621/default/polybench/seidel-2d/64t_1n/run_1/arts.log`
- `/tmp/carts-bench-seidel-inspect/20260325_053936/default/polybench/seidel-2d/64t_1n/artifacts/seidel-2d.mlir`
- `/tmp/seidel_inspect_latest.mlir`

#### `sw4lite/rhs4sg-base`

Results bundle:

- `external/carts-benchmarks/results/20260325_050621`

Observed result:

- `build_arts = pass`
- `build_omp = pass`
- `run_arts = pass`
- `run_omp = pass`
- verification: `correct = false`
- mismatch note:
  `Checksum mismatch: ARTS=8.987003514767e+02, OMP=1.527648351669e+03`

Artifact paths:

- `external/carts-benchmarks/results/20260325_050621/default/sw4lite/rhs4sg-base/64t_1n/run_1/arts.log`
- `external/carts-benchmarks/results/20260325_050621/default/sw4lite/rhs4sg-base/64t_1n/run_1/omp.log`
- `/tmp/rhs4sg_concurrency-opt_fromraw.mlir`
- `/tmp/vel4sg_pipelines/vel4sg_base_concurrency-opt.mlir`

### 4. Jacobi regression status

The Jacobi intranode worker-cap regression was diagnosed and fixed.

Evidence:

- dedicated contract added:
  `tests/contracts/jacobi_stencil_intranode_worker_cap.mlir`
- previous targeted validation indicated the Jacobi issue was resolved after
  the worker-cap fix

This still needs to be rerun in the final validation pass because there are
newer, uncommitted changes in the worktree.

## What Is Done

- CLI support exists for `carts triage-benchmark` and agent installation.
- `RecordDepOp` can carry advisory `dep_flags`.
- the compiler lowers duplicate hints through both standard and ESD paths.
- the ARTS runtime preserves the flags through local, remote, and OOO paths.
- acquire-time duplicate-friendly RO support exists in ARTS.
- OMP-region helper inlining no longer has to crash on simple helpers.
- raise-memref dimensionality is significantly more robust for N-D alias chains.
- the Jacobi high-thread stencil worker-cap issue has a fix and a contract.
- a broad medium sweep from 2026-03-26 passed `92/92`.

## What Is Not Done

- the current worktree has not been fully revalidated after the latest staged
  and unstaged changes.
- the untracked contracts still need to be reviewed and promoted.
- `polybench/seidel-2d` still times out at `large / 64`.
- `sw4lite/rhs4sg-base` still has a `large / 64` checksum mismatch.
- the work is not yet committed in logical slices.

## Required Next Steps

### 1. Normalize the worktree

- decide whether the current `ForLowering.cpp` follow-up is part of the desired
  fix
- stage or discard the untracked contracts deliberately
- keep `amd_llm_test.py` out of this effort

### 2. Rebuild and rerun contracts

Use:

```bash
tools/carts build
tools/carts lit --suite contracts
```

### 3. Revalidate the duplicate-hint path

Specifically rerun the contract coverage for:

- verifier behavior for `dep_flags`
- no-flags fallback to the standard runtime call
- `_ex` lowering for standard deps
- `_at_ex` lowering for ESD deps
- `EdtLowering` automatic hint derivation

### 4. Revalidate OMP helper inlining and aligned task-loop bounds

Specifically rerun the contracts for:

- single-block helper inlining inside OMP regions
- intentional multi-block helper skip behavior
- unconditional aligned-bounds loop window derivation

### 5. Rerun the benchmark sweeps

First rerun the broad medium sweep that most recently passed:

```bash
tools/carts benchmarks run --size medium --threads 1,2,4,8 --timeout 120 --trace
```

Then rerun the two known large `64-thread` blockers in isolated results
directories:

```bash
tools/carts benchmarks run polybench/seidel-2d --size large --threads 64 --runs 1 --timeout 180 --trace --results-dir /tmp/carts-bench-seidel-rerun
tools/carts benchmarks run sw4lite/rhs4sg-base --size large --threads 64 --runs 1 --timeout 180 --trace --results-dir /tmp/carts-bench-rhs4sg-rerun
```

After those pass, perform the final `64-thread` sweep across small, medium, and
large sizes:

```bash
tools/carts benchmarks run --size small --threads 64 --runs 1 --timeout 180 --trace
tools/carts benchmarks run --size medium --threads 64 --runs 1 --timeout 180 --trace
tools/carts benchmarks run --size large --threads 64 --runs 1 --timeout 180 --trace
```

### 6. Use triage when a rerun fails

Preferred command:

```bash
tools/carts triage-benchmark <benchmark> --size <size> --threads <threads> --timeout <sec>
```

This keeps the investigation aligned with:

- the benchmark runner artifact layout
- the pipeline manifest from `tools/carts pipeline --json`
- CARTS-native commands instead of ad hoc shell scripts

### 7. Commit in logical slices

Recommended commit order:

1. `external/arts` dependency flag runtime support
2. `external/carts-benchmarks` deterministic benchmark-source fix
3. parent repo compiler `dep_flags` support and tests
4. parent repo OMP and raise-memref fixes and tests
5. parent repo Jacobi and Seidel heuristic fixes after final benchmark
   validation

## Key Artifacts and Files

### Benchmark result bundles

- `external/carts-benchmarks/results/20260326_035354`
- `external/carts-benchmarks/results/20260325_050621`

### Seidel artifacts

- `external/carts-benchmarks/results/20260325_050621/default/polybench/seidel-2d/64t_1n/run_1/arts.log`
- `/tmp/carts-bench-seidel-inspect/20260325_053936/default/polybench/seidel-2d/64t_1n/artifacts/seidel-2d.mlir`
- `/tmp/seidel_inspect_latest.mlir`

### `rhs4sg-base` artifacts

- `external/carts-benchmarks/results/20260325_050621/default/sw4lite/rhs4sg-base/64t_1n/run_1/arts.log`
- `external/carts-benchmarks/results/20260325_050621/default/sw4lite/rhs4sg-base/64t_1n/run_1/omp.log`
- `/tmp/rhs4sg_concurrency-opt_fromraw.mlir`
- `/tmp/vel4sg_pipelines/vel4sg_base_concurrency-opt.mlir`

### Most relevant source files

- `include/arts/Ops.td`
- `lib/arts/Dialect.cpp`
- `lib/arts/passes/transforms/EdtLowering.cpp`
- `lib/arts/passes/transforms/ConvertArtsToLLVM.cpp`
- `lib/arts/passes/opt/general/Inliner.cpp`
- `lib/arts/passes/opt/general/RaiseMemRefDimensionality.cpp`
- `lib/arts/analysis/heuristics/DistributionHeuristics.cpp`
- `lib/arts/passes/opt/db/DbPartitioning.cpp`
- `lib/arts/transforms/db/DbBlockPlanResolver.cpp`
- `lib/arts/transforms/dep/Seidel2DWavefrontPattern.cpp`
- `lib/arts/transforms/edt/EdtTaskLoopLowering.cpp`
- `lib/arts/passes/transforms/ForLowering.cpp`
- `external/arts/libs/include/public/arts.h`
- `external/arts/libs/src/core/compute/edt.c`
- `external/arts/libs/src/core/sync/event.c`
- `external/arts/libs/src/core/memory/db.c`

## Bottom Line

The broad medium benchmark sweep is in good shape, the duplicate-hint
implementation is now present across compiler and runtime layers, and the
Jacobi worker-cap regression has a fix. The project is not done yet because the
current worktree still needs a clean validation pass, and two large `64-thread`
benchmark issues remain the critical blockers:

- `polybench/seidel-2d` still times out
- `sw4lite/rhs4sg-base` still verifies incorrectly

The next thread should start by normalizing the worktree, rerunning contracts,
and then rerunning benchmarks with special focus on those two blockers.
