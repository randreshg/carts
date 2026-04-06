# Benchmark Results — Large Size, 64 Threads

**Report updated:** 2026-04-06
**Configuration:** `--size large --timeout 300 --threads 64`
**Results directory:** `external/carts-benchmarks/results/20260406_094438`
**Suite result:** `23/23 passed`, `23/23 correct`
**Geometric mean speedup:** `1.10x`
**Average speedup:** `2.65x`
**Max ARTS wall time:** `9.9s`

This file supersedes the stale 2026-04-05 report. The benchmark set was rerun
after fixing the CPS/runtime transport failures and the current large/64-thread
sweep is fully green.

## Changes Under Test

The passing sweep is backed by two production-safe fixes, not benchmark-specific
hardcoding:

1. `EpochOpt.cpp`
   Skip generic CPS loop/chain conversion for specialized dependence families
   that need dedicated lowering:
   - `jacobi_alternating_buffers`
   - `wavefront_2d`

2. `ConvertArtsToLLVM.cpp`
   Fix `DepDbAcquireOp` lowering so handle-table memrefs preserve the required
   level of indirection instead of treating payload data as a pointer.

These changes resolved the prior `jacobi2d`/`seidel-2d` CPS failures and the
runtime crash cluster seen in benchmarks such as `polybench/gemm`.

## All Benchmarks

| # | Benchmark | Status | Correct | ARTS Time | Speedup |
|---|-----------|--------|---------|-----------|---------|
| 1 | kastors-jacobi/jacobi-for | PASS | YES | 5.6s | 0.94x |
| 2 | kastors-jacobi/poisson-for | PASS | YES | 6.2s | 1.21x |
| 3 | ml-kernels/activations | PASS | YES | 2.3s | 0.58x |
| 4 | ml-kernels/batchnorm | PASS | YES | 3.5s | 1.58x |
| 5 | ml-kernels/layernorm | PASS | YES | 5.6s | 1.12x |
| 6 | ml-kernels/pooling | PASS | YES | 4.0s | 1.35x |
| 7 | monte-carlo/ensemble | PASS | YES | 9.9s | 1.12x |
| 8 | polybench/2mm | PASS | YES | 3.8s | 3.04x |
| 9 | polybench/3mm | PASS | YES | 3.1s | 2.93x |
| 10 | polybench/atax | PASS | YES | 8.6s | 0.50x |
| 11 | polybench/bicg | PASS | YES | 7.0s | 0.67x |
| 12 | polybench/convolution-2d | PASS | YES | 6.2s | 0.81x |
| 13 | polybench/convolution-3d | PASS | YES | 3.6s | 1.64x |
| 14 | polybench/correlation | PASS | YES | 4.7s | 0.53x |
| 15 | polybench/gemm | PASS | YES | 2.5s | 5.33x |
| 16 | polybench/jacobi2d | PASS | YES | 7.4s | 0.51x |
| 17 | polybench/seidel-2d | PASS | YES | 9.5s | 0.53x |
| 18 | seissol/volume-integral | PASS | YES | 1.9s | 33.88x |
| 19 | specfem3d/stress | PASS | YES | 5.5s | 0.67x |
| 20 | specfem3d/velocity | PASS | YES | 5.0s | 0.55x |
| 21 | stream | PASS | YES | 7.3s | 0.44x |
| 22 | sw4lite/rhs4sg-base | PASS | YES | 5.7s | 0.59x |
| 23 | sw4lite/vel4sg-base | PASS | YES | 5.5s | 0.49x |

## Summary

| Category | Count |
|----------|-------|
| Correct (checksum match / tolerance pass) | 23 |
| Wrong checksum | 0 |
| Runtime timeout | 0 |
| Runtime crash | 0 |
| Compile failure | 0 |

## Notes

- All benchmarks completed below the user's `30s` target in this sweep.
- The previous 2026-04-05 report is no longer representative of current
  compiler behavior.
- The CPS exclusions are a containment fix for specialized dependency patterns.
  A future dedicated lowering for `jacobi_alternating_buffers` can still improve
  quality, but it is not required for benchmark correctness.
