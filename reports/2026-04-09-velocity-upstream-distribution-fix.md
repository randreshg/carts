# Specfem3D Velocity: Upstream Distribution Fix

## Scope

- Compiler-only changes.
- No runtime changes.
- No benchmark-source changes.
- Validation target: `specfem3d/velocity`, `large`, `64` threads.

## Root Cause

The reduced `48^3` velocity case was collapsing to `blockSize = 23` in
`ForOpt`, which reduced the worker wave to three task lanes before lowering.

Two upstream issues were involved in the coarsening decision:

1. Nested loop work was estimated with a cap that was too small to prove worker
   saturation for nested uniform kernels.
2. The coarsening gate was treating `summary.hasMatmulUpdate` as authoritative
   even when the loop already carried a semantic `uniform` distribution
   contract. For velocity, this local DB-analysis signal was a false positive
   and suppressed nested-work scaling.

## Code Changes

- `lib/arts/analysis/heuristics/DistributionHeuristics.cpp`
  - Removed the downstream `ScaleAwareDistribution` bandaid path entirely and
    kept the fix in the original coarsening heuristic.
  - Reworked nested-work estimation so the cap is derived from the amount of
    effective work needed to saturate the current worker topology.
  - Stopped letting a non-authoritative local matmul-update signal suppress
    nested-work scaling when the loop is already semantically classified as
    `uniform`.

## Contract Validation

All focused velocity regressions pass after the upstream fix:

- `tests/contracts/specfem3d_velocity_scales_to_outer_workers.mlir`
- `tests/contracts/specfem3d_velocity_large_scales_to_workers.mlir`
- `tests/contracts/specfem3d_velocity_pattern_pipeline_no_crash.mlir`

## Benchmark Run

Command:

```bash
env PATH="/home/raherrer/miniforge3/bin:$PATH" \
  python3.13 -m dekk carts benchmarks run specfem3d/velocity \
  --size large \
  --threads 64 \
  --runs 3 \
  --timeout 1200 \
  --results-dir external/carts-benchmarks/results/codex-specfem3d-velocity-upstream-fix
```

Results directory:

`external/carts-benchmarks/results/codex-specfem3d-velocity-upstream-fix/20260409_175326`

### Kernel Times

| Run | ARTS kernel (s) | OpenMP kernel (s) | Speedup |
|---|---:|---:|---:|
| 1 | 136.097022 | 9.201849 | 0.0676x |
| 2 | 124.002802 | 10.996896 | 0.0887x |
| 3 | 121.826116 | 7.993690 | 0.0656x |

### Median-Filtered Summary

| Metric | Value |
|---|---:|
| Kernel paired median-filtered speedup | 0.06761241991026078x |
| Kernel geometric mean speedup | 0.07327525428787747x |
| E2E median-filtered ARTS | 128.861212051 s |
| E2E median-filtered OpenMP | 16.752395773 s |

Runner summary:

- Geometric mean speedup (kernel): `0.07327525428787747x`
- Startup outliers: `ARTS=0`, `OpenMP=0`

## Counter Snapshot

From `run_2` cluster counters:

- `NUM_EDT_CREATE = 641`
- `NUM_EDT_FINISH = 641`
- `TIME_EDT_EXEC = 128832.463576 ms`

## Findings

- The upstream fix corrected the worker-scaling contract bug in both the
  reduced and large velocity cases.
- The real performance limiter remains compiler-generated orchestration cost,
  not lack of worker exposure.
- The task count is still only `641`, so the dominant problem is not a flood of
  tiny EDTs. The remaining gap is the repeated creation/execution of one worker
  wave per timestep.
- Existing persistence plumbing is not yet enough to solve this:
  - the structured-region gate is conservative for uniform repeated kernels,
  - and even when `arts.persistent_region` is stamped, final lowering does not
    currently change the epoch creation/runtime execution path enough to reuse a
    worker wave across repetitions.

## Recommended Next Compiler Step

Implement a true compiler-side persistent repeated-wave lowering for repeated
uniform owner-strip kernels:

- keep worker count unchanged,
- keep per-worker owner slices stable across repetitions,
- create the worker wave once,
- run the repetition loop inside the worker task body rather than recreating
  the epoch/worker wave once per timestep.

That is the change most directly suggested by the current counters and the
existing structured-plan analysis.
