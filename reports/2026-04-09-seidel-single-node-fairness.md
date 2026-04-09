# Seidel Single-Node Fairness Report

Date: 2026-04-09

## Scope

This report validates the single-node `polybench/seidel-2d` comparison between
ARTS and OpenMP at `large` size with `64` threads. The goal was to remove
benchmark-runner asymmetries before judging compiler/runtime performance.

Constraints followed during this work:

- no runtime changes
- no benchmark-source changes
- benchmark framework fixes were allowed
- compiler changes were only justified if ARTS still lagged after a fair rerun

## Benchmark Framework Changes

The benchmark framework in `external/carts-benchmarks` was updated to make the
OpenMP side match ARTS' single-node execution policy more closely and to reduce
run-order bias.

Applied changes:

- OpenMP local runs now default to pinned execution:
  - `OMP_DYNAMIC=FALSE`
  - `OMP_WAIT_POLICY=ACTIVE`
  - `OMP_PROC_BIND=close`
  - `OMP_PLACES=cores`
- existing user overrides are still respected
- mixed ARTS/OpenMP runs now alternate execution order by run number
- run artifacts now persist `run_order`
- run artifacts now persist the effective OpenMP environment overrides
- SLURM OpenMP launch path now uses the same guarded affinity defaults
- direct `make` convenience targets use the same pinned OpenMP policy
- reproducibility metadata now captures `KMP_AFFINITY` and `KMP_HW_SUBSET`

## Validation

### Environment

`dekk carts doctor` passed on 2026-04-09.

### Benchmark Runner Tests

`python3.13 -m pytest external/carts-benchmarks/tests -q`

Result: `25 passed in 1.93s`.

### Fair Mixed Rerun

Command:

```bash
env PATH="/home/raherrer/miniforge3/bin:$PATH" python3.13 -m dekk carts benchmarks run polybench/seidel-2d --size large --threads 64 --runs 3 --timeout 240 --results-dir external/carts-benchmarks/results/codex-seidel-fair-mixed-current
```

Result directory:

`external/carts-benchmarks/results/codex-seidel-fair-mixed-current/20260409_152824`

### Summary

| Metric | ARTS | OpenMP |
| --- | ---: | ---: |
| Kernel mean | 28.394437 s | 33.474055 s |
| Kernel min | 27.396882 s | 32.523813 s |
| Kernel max | 29.323959 s | 34.704482 s |
| E2E median filtered | 29.780307 s | 34.548809 s |

Derived speedup:

- kernel mean speedup: `1.1793x`
- median-filtered end-to-end speedup: `1.1835x`

### Per-Run Metadata Checks

Persisted `run_config.json` now confirms:

- alternating run order: `['arts', 'openmp']`, `['openmp', 'arts']`, `['arts', 'openmp']`
- effective OpenMP env:
  - `OMP_DYNAMIC=FALSE`
  - `OMP_WAIT_POLICY=ACTIVE`
  - `OMP_PROC_BIND=close`
  - `OMP_PLACES=cores`
  - `OMP_NUM_THREADS=64`

## Findings

1. The old Seidel comparison was unfair. ARTS was pinned while OpenMP was not
   consistently pinned in the benchmark runner.
2. The OpenMP baseline is genuinely parallel; the weird prior numbers were
   affinity/noise artifacts, not serialization.
3. After fixing pinning and alternating run order, ARTS is already faster than
   OpenMP on single-node Seidel at `large/64`.
4. Because ARTS is now ahead on the fair comparison, there is no evidence-based
   reason to land a speculative Seidel compiler heuristic change in this round.
5. Persistent/pooling-oriented compiler mechanisms exist in the tree, but the
   current bottleneck for this benchmark did not justify forcing an additional
   compiler change once the fair baseline was established.

## Recommendation

Land the benchmark-framework fairness fixes and use the new mixed rerun above as
the current single-node Seidel reference point.

If more compiler work is needed later, the next evidence-based targets are:

- wavefront tiling cost-model improvements only if Seidel regresses again under
  a fair baseline
- 3D stencil cases such as `specfem3d/velocity`, `specfem3d/stress`, and
  `sw4lite/vel4sg-base`, which remain better candidates for compiler-side
  single-node improvement than Seidel at this point
