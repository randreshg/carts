# Benchmark Results — Large Size, 64 Threads

**Report updated:** 2026-04-07
**Configuration:** `--size large --timeout 300 --threads 64`
**Results directory:** `external/carts-benchmarks/results/20260407_203029`
**Suite result:** `23/23 passed`, `23/23 correct`
**Geometric mean speedup:** `1.01x`
**Average speedup:** `2.50x`
**Max ARTS wall time:** `18.2s`

This file supersedes the stale 2026-04-05 report. The benchmark set was rerun
after the epoch continuation ABI fix and the async loop policy cleanup for
zero-local-work full-timestep loops. The current large/64-thread sweep is fully
green again.

## Changes Under Test

The passing sweep is backed by production-safe fixes, not benchmark-specific
hardcoding:

1. `EpochOpt.cpp`
   Skip generic CPS loop/chain conversion for specialized dependence families
   that need dedicated lowering:
   - `jacobi_alternating_buffers`
   - `wavefront_2d`

2. `EdtLowering.cpp` + `EpochLowering.cpp`
   Preserve and consume the structured continuation relaunch schema so CPS
   relaunches forward dependency/state slots from a proven contract instead of
   reconstructing them positionally.

3. `EpochHeuristics.cpp`
   Avoid CPS-chain selection for full-timestep loops whose continuation has
   zero expected local work when the loop still needs multi-stage orchestration
   or sequential sidecars. This keeps profitable CPS transforms available while
   falling back to the blocking shape for cases like `batchnorm`,
   `layernorm`, and `convolution-2d`.

4. `ConvertArtsToLLVM.cpp`
   Fix `DepDbAcquireOp` lowering so handle-table memrefs preserve the required
   level of indirection instead of treating payload data as a pointer.

These changes resolved the prior CPS/runtime timeout cluster while preserving
the earlier `jacobi2d`/`seidel-2d` and `polybench/gemm` fixes.

## All Benchmarks

| # | Benchmark | Status | Correct | ARTS Time | Speedup |
|---|-----------|--------|---------|-----------|---------|
| 1 | kastors-jacobi/jacobi-for | PASS | YES | 11.2s | 0.39x |
| 2 | kastors-jacobi/poisson-for | PASS | YES | 12.7s | 0.47x |
| 3 | ml-kernels/activations | PASS | YES | 5.1s | 0.42x |
| 4 | ml-kernels/batchnorm | PASS | YES | 9.4s | 0.38x |
| 5 | ml-kernels/layernorm | PASS | YES | 9.7s | 0.76x |
| 6 | ml-kernels/pooling | PASS | YES | 6.8s | 1.48x |
| 7 | monte-carlo/ensemble | PASS | YES | 18.2s | 0.65x |
| 8 | polybench/2mm | PASS | YES | 6.2s | 3.08x |
| 9 | polybench/3mm | PASS | YES | 6.1s | 2.44x |
| 10 | polybench/atax | PASS | YES | 12.9s | 0.56x |
| 11 | polybench/bicg | PASS | YES | 10.3s | 0.66x |
| 12 | polybench/convolution-2d | PASS | YES | 6.6s | 1.70x |
| 13 | polybench/convolution-3d | PASS | YES | 4.0s | 1.39x |
| 14 | polybench/correlation | PASS | YES | 4.8s | 0.54x |
| 15 | polybench/gemm | PASS | YES | 2.6s | 5.14x |
| 16 | polybench/jacobi2d | PASS | YES | 7.5s | 0.50x |
| 17 | polybench/seidel-2d | PASS | YES | 9.6s | 0.53x |
| 18 | seissol/volume-integral | PASS | YES | 2.0s | 32.26x |
| 19 | specfem3d/stress | PASS | YES | 4.9s | 0.90x |
| 20 | specfem3d/velocity | PASS | YES | 4.5s | 0.84x |
| 21 | stream | PASS | YES | 7.5s | 0.48x |
| 22 | sw4lite/rhs4sg-base | PASS | YES | 4.8s | 1.04x |
| 23 | sw4lite/vel4sg-base | PASS | YES | 4.4s | 0.93x |

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
- The CPS exclusions remain a containment fix for specialized dependency
  patterns. The newer full-timestep zero-local-work gate is policy-driven and
  applies across kernel families instead of encoding benchmark-specific
  thresholds.
