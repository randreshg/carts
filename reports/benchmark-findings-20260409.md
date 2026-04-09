# CARTS benchmark findings (2026-04-09)

Environment used for all commands:

```bash
export PATH="/home/raherrer/miniforge3/bin:$PATH"
python3.13 -m dekk carts ...
```

Skills read and used:

- `.agents/skills/benchmark/SKILL.md`
- `.agents/skills/benchmark-triage/SKILL.md`
- `.agents/skills/build/SKILL.md`
- `.agents/skills/pass-dev/SKILL.md`

Constraints followed throughout this work:

- compiler-only changes
- no runtime changes
- no benchmark source changes

## Final compiler changes kept

1. `lib/arts/analysis/value/ValueAnalysis.cpp`
   - Added scalar memref-load constant folding for rank-0 alloc/alloca-backed
     loads.
   - This fixes the `seidel-2d` wavefront matcher when the row bound comes
     through `memref.store constant -> memref.load -> index_cast`.

2. `lib/arts/transforms/kernel/MatmulReductionPattern.cpp`
   - Extended the matmul reduction matcher to recognize the current frontend's
     scalar-accumulator form, where the `k` reduction is carried through a
     rank-0 memref alloca instead of `scf.for iter_args`.
   - Tightened output-store selection so the matcher uses the real rank-2
     `C[i,j]` store instead of the scalar accumulator's setup store.
   - This restores the `depPattern = matmul` rewrite for the current `gemm`
     frontend IR and recovers the fast `k-j` update form.

3. `lib/arts/passes/opt/db/BlockLoopStripMiningSupport.cpp`
   - Folded runtime worker-count/block-size expressions into a stable
     block-size family check.
   - This keeps neighborhood strip-mining recognition alive when the IR uses
     runtime-query-derived block hints instead of simple constants.

4. `lib/arts/analysis/heuristics/PartitioningHeuristics.cpp`
   - Added a narrow carve-out that preserves blocked layout for small 1-D
     producer/readback vectors.
   - This avoids collapsing ATAX/BiCG-style vectors back to coarse layout when
     a later read phase widens to full-range but the local producer locality is
     still valuable.

5. `lib/arts/passes/opt/db/DbPartitioning.cpp`
   - Preserved block capability on write acquires when an explicit owner/block
     contract exists and the access is direct.
   - This keeps the partitioner aligned with explicit distribution contracts
     instead of discarding block layout too early.

6. `lib/arts/analysis/heuristics/EpochHeuristics.cpp`
   - Prevented async CPS-chain lowering for repeated loops that contain host
     call sidecars.
   - This keeps benchmark-control loops on the blocking path and avoids
     relaunching scheduler topology around non-kernel work.

## Validated benchmark results

All results below are `large` / `64 threads` unless noted otherwise.

### Polybench set already investigated earlier in the session

| Benchmark | ARTS kernel (s) | OMP kernel (s) | Speedup | Results |
|---|---:|---:|---:|---|
| `polybench/atax` | 18.697178 | 19.256975 | 1.03x | `external/carts-benchmarks/results/codex-atax-final/20260409_123037/results.json` |
| `polybench/bicg` | 16.394112 | 19.182751 | 1.17x | `external/carts-benchmarks/results/codex-bicg-final2/20260409_122814/results.json` |
| `polybench/correlation` | 7.425213 | 11.567341 | 1.56x | `external/carts-benchmarks/results/codex-correlation-final/20260409_122936/results.json` |
| `polybench/jacobi2d` | 0.180650 | 29.708493 | 164.45x | `external/carts-benchmarks/results/codex-jacobi2d-current2/20260409_121637/results.json` |
| `polybench/seidel-2d` | 34.343335 | 21.044925 | 0.61x | `external/carts-benchmarks/results/codex-seidel2d-final/20260409_123202/results.json` |

### Remaining Polybench large/64 sweep

| Benchmark | ARTS kernel (s) | OMP kernel (s) | Speedup | Results |
|---|---:|---:|---:|---|
| `polybench/2mm` | 6.624318 | 34.726508 | 5.24x | `external/carts-benchmarks/results/codex-polybench-rest-final/20260409_123909/results.json` |
| `polybench/3mm` | 5.595386 | 28.178585 | 5.04x | `external/carts-benchmarks/results/codex-polybench-rest-final/20260409_123909/results.json` |
| `polybench/convolution-2d` | 14.935216 | 21.112726 | 1.41x | `external/carts-benchmarks/results/codex-polybench-rest-final/20260409_123909/results.json` |
| `polybench/convolution-3d` | 6.712390 | 14.273046 | 2.13x | `external/carts-benchmarks/results/codex-polybench-rest-final/20260409_123909/results.json` |
| `polybench/gemm` | 3.668198 | 24.087218 | 6.57x | `external/carts-benchmarks/results/codex-gemm-accumfix2/20260409_130137/results.json` |

The fresh `2mm/3mm/convolution-2d/convolution-3d/gemm` sweep originally
showed `gemm` at only `0.69x`. The final `gemm` rerun above is after the
scalar-accumulator matmul matcher fix.

### Epoch-heuristic-sensitive canaries

| Benchmark | ARTS kernel (s) | OMP kernel (s) | Speedup | Results |
|---|---:|---:|---:|---|
| `ml-kernels/activations` | 3.063067 | 8.404514 | 2.74x | `external/carts-benchmarks/results/codex-activations-epochfix/20260409_104546/results.json` |
| `ml-kernels/batchnorm` | 11.605655 | 19.667343 | 1.69x | `external/carts-benchmarks/results/codex-batchnorm-rebuilt/20260409_120713/results.json` |

## Root-cause notes

### `polybench/seidel-2d`

Original failure mode:

- The benchmark previously timed out because the specialized Seidel wavefront
  transform failed to recover a constant row extent.
- The bound came through a rank-0 scalar memref load, which the old
  `ValueAnalysis` constant folder could not reduce.

Validated fix:

- `ValueAnalysis` now folds rank-0 alloc/alloca-backed scalar loads when the
  defining store carries a compile-time constant.
- With that change, `Seidel2DWavefrontPattern` takes the wavefront path instead
  of falling back to the sequential-style lowering.

Current status:

- The timeout is fixed and the benchmark is correct.
- The generated IR now contains the intended wavefront structure, but the final
  `large/64` result is still only `0.61x`.
- I investigated additional heuristic changes for wavefront saturation and did
  not find a second compiler-only optimization that improved `seidel-2d`
  without regressing it again.

Conclusion:

- `seidel-2d` is no longer broken, but it remains the slowest benchmark in the
  final validated large/64 set.
- The remaining gap is now a wavefront-tiling / scheduling-quality problem,
  not the original missing-transform bug.

### `polybench/gemm`

Observed regression:

- A fresh `large/64` run on the current tree initially dropped to `0.69x`,
  with ARTS at `44.256919s` vs OMP at `30.523767s`.

Root cause:

- The current frontend emits the reduction through a scalar rank-0 memref
  accumulator instead of an `scf.for iter_args` result.
- The existing `MatmulReductionPattern` only matched the iter-args form, so
  `gemm` stayed `depPattern = uniform` and missed the `k-j` matmul rewrite.

Validated fix:

- The matcher now accepts the scalar-accumulator form and binds the real
  post-reduction rank-2 output store.
- `KernelTransformsPass` again rewrites the kernel into the tiled `k-j` update
  form and stamps `depPattern = matmul`.

Result:

- Final validated rerun: `6.57x`, with ARTS at `3.668198s` vs OMP at
  `24.087218s`.

## Final assessment

What improved materially in this session:

- `polybench/gemm` was recovered from a real regression back to a strong
  matmul result.
- `polybench/seidel-2d` was recovered from timeout/fallback to a correct
  wavefront execution.
- The rest of the Polybench large/64 sweep is performant on the final tree.

What remains open:

- `polybench/seidel-2d` is still underperforming at `0.61x`.
- I did not find a validated compiler-only seidel optimization beyond the
  bound-fold fix, so that remains the main unresolved performance gap.
