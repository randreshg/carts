# Velocity 64T Large: Scale-Aware Distribution Validation

## Scope

Validated the new compiler-side scale-aware worker distribution work against:

- `specfem3d/velocity` reduced contract shape (`48^3`, `worker_threads=64`)
- `specfem3d/velocity` benchmark large shape (`288 x 288 x 384`, `NREPS=10`, `worker_threads=64`)

Constraints respected:

- compiler-only changes
- no runtime edits
- no benchmark source edits

## Compiler Findings

### Reduced contract case

Pre-lowering now preserves natural outer-loop scaling instead of collapsing the
loop with an `arts-for-opt` block hint:

- source outer loop: `for (k = 1; k < NZ - 1; ++k)` with `NZ=48`
- pre-lowering dispatch: `scf.for %arg0 = %c0 to %c46 step %c1`
- previous failure mode: underfilled dispatch caused by a `blockSize=23` hint

### Large benchmark case

For the real benchmark shape, pre-lowering now distributes one lane per worker:

- benchmark compile args: `-DNX=288 -DNY=288 -DNZ=384 -DNREPS=10`
- benchmark runtime config: `worker_threads=64`
- pre-lowering repetition loop: `scf.for %arg0 = %c0 to %c10 step %c1`
- pre-lowering worker loop: `scf.for %arg1 = %c0 to %c64 step %c1`
- per-worker slices are balanced at 6 or 5 `k` planes across 382 outer
  iterations

This confirms the compiler is no longer capping the large benchmark below the
available worker count.

## Benchmark Results

Results directory:

- `external/carts-benchmarks/results/codex-specfem3d-velocity-scale-aware/20260409_164904`

Kernel timings for `specfem3d/velocity`, `--size large`, `--threads 64`, `--runs 3`:

| Run | ARTS kernel (s) | OMP kernel (s) | Speedup |
| --- | ---: | ---: | ---: |
| 1 | 152.492531 | 8.425789 | 0.055x |
| 2 | 132.382956 | 8.979170 | 0.068x |
| 3 | 154.839491 | 8.447603 | 0.055x |

Median-filtered kernel speedup from `results.json`:

- `0.0589x`

Checksums matched OpenMP on all runs.

## Interpretation

The worker-scaling problem is fixed in lowering, but it is not the dominant
performance limiter for the large velocity benchmark.

Evidence:

- pre-lowering already emits `64` worker lanes for the large benchmark
- runtime counters still show only `641` EDT creates total across `10`
  repetitions, so the slowdown is not caused by millions of tiny tasks
- ARTS remains about `17x-18x` slower than OpenMP even after worker-scaled
  dispatch is present

## Next Compiler Direction

The next compiler optimization should target repetition amortization rather than
thread-count scaling:

1. Preserve worker-scaled dispatch.
2. Reduce per-repetition EDT/dependency setup overhead in the generated code.
3. Prefer persistent or cross-repetition worker orchestration for kernels with
   `arts.plan.repetition_structure = "full_timestep"` when legality permits.

That direction keeps full worker parallelism while attacking the remaining cost
center without relying on thread-limiting heuristics.
