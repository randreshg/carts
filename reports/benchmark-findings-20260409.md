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

## Compiler changes kept

1. Removed the obsolete ready-local launch path after `arts_edt_create_ready_local...`
   was intentionally removed from the runtime API.
   - `lib/arts/passes/transforms/ForLowering.cpp`
   - `lib/arts/passes/transforms/ConvertArtsToLLVMPatterns.cpp`

2. Fixed unsafe conflicting-owner block partitioning on promoted inner views.
   - `lib/arts/passes/opt/db/DbPartitioning.cpp`
   - `lib/arts/analysis/heuristics/PartitioningHeuristics.cpp`

3. Fixed duplicate DB release lowering.
   - `lib/arts/passes/opt/general/DeadCodeElimination.cpp`
   - `lib/arts/passes/transforms/EpochLowering.cpp`

4. Fixed DB destroy lowering so `arts.db_free` lowers runtime DB destruction
   before compiler bookkeeping deallocation.
   - `lib/arts/passes/transforms/ConvertArtsToLLVMPatterns.cpp`

## Benchmarks run

### `kastors-jacobi/poisson-for`, `large`, `64`

Result: pass, checksum fixed.

- Results: `external/carts-benchmarks/results/codex-poisson-ownerdims-fix/20260409_061617`
- ARTS checksum: `2.620819845439e-02`
- OMP checksum: `2.620819845439e-02`
- ARTS kernel: `17.899195s`
- OMP kernel: `32.834002s`
- Speedup: `1.83x`

Final-tree regression rerun:

- Results: `external/carts-benchmarks/results/codex-final-poisson-regression/20260409_083418`
- Checksum still matches exactly
- ARTS kernel: `14.250541s`
- OMP kernel: `10.632664s`
- This rerun stayed correct but showed materially different timing, so the
  performance signal on this benchmark is noisy across runs on this machine.

Root cause:

- Allocation `poisson-for.c:94:16` (`f`) carried conflicting owner contracts
  across phases.
- The old block decision forced an unsafe/expensive layout.
- The kept owner-dim fixes make the allocation fall back safely when the
  conflicting contract is genuinely non-leading/write-relevant.

### `kastors-jacobi/jacobi-for`, `large`, `64`

Result: pass, no regression.

- Results: `external/carts-benchmarks/results/codex-jacobi-regression-check/20260409_061839`
- Kernel speedup observed in runner summary: about `1.80x`

### `ml-kernels/batchnorm`, `small`, `64`

Result: pass.

- Results: `external/carts-benchmarks/results/codex-batchnorm-small-smoke/20260409_072600`
- ARTS kernel: `0.002294s`
- OMP kernel: `0.251266s`
- Speedup: `109.53x`

Final-tree regression rerun:

- Results: `external/carts-benchmarks/results/codex-final-batchnorm-small/20260409_083418`
- ARTS kernel: `0.002261s`
- OMP kernel: `0.241634s`
- Speedup: `106.87x`

### `ml-kernels/batchnorm`, `large`, `64`

Result: still times out under the stock `worker_threads=64` runtime config.

- Results before owner-readonly refinement:
  `external/carts-benchmarks/results/codex-batchnorm-fix1/20260409_070829`
- Results after owner-readonly refinement:
  `external/carts-benchmarks/results/codex-batchnorm-owner-readonly-fix/20260409_074232`

What improved:

- The kept owner-readonly refinement changed the generated DB partitioning for
  the `output` tensor from mostly coarse acquires to a mixed blocked/full-range
  shape.
- Old `db-partitioning` counts:
  - `partitioning(<coarse>)`: `11`
  - `partitioning(<block>)`: `1`
- New `db-partitioning` counts:
  - `partitioning(<coarse>)`: `5`
  - `partitioning(<block>)`: `7`

What did not improve enough:

- The benchmark still reaches only `startup.batchnorm` and never prints
  `kernel.batchnorm` before the 600s timeout.
- The runtime remains in the scheduler loop when killed.

Interpretation:

- The compiler fixes removed a correctness/ownership problem, but the remaining
  large-case failure is still dominated by the generated execution topology
  under a 64-worker ARTS runtime configuration.

### `ml-kernels/activations`, `large`, `64`

Result: cleanup crash fixed, but still times out under the stock
`worker_threads=64` runtime config.

- Earlier failure with cleanup crash:
  `external/carts-benchmarks/results/codex-large64-clean/20260409_063351`
- After DB destroy lowering fix:
  `external/carts-benchmarks/results/codex-activations-fixcheck/20260409_072900`

What improved:

- The old post-checksum cleanup segfault is gone after the `arts.db_free`
  lowering fix.

What remains:

- The benchmark now times out instead of crashing.
- It prints only `startup.activations` before the external timeout kills it.

## Diagnostics that explain the remaining blocker

### `activations`, `large`, `1`

Result: pass.

- Results: `external/carts-benchmarks/results/codex-activations-1t-diagnostic/20260409_075924`
- ARTS kernel: `74.997401s`
- OMP kernel: `84.050401s`
- Speedup: `1.12x`

Interpretation:

- The generated kernel code is not fundamentally broken.
- The pathological behavior appears only once the ARTS runtime is asked to
  launch many worker threads.

### Manual runtime-thread diagnostic for `activations`

Using the old 64-task binary but forcing `worker_threads=8` in a copied
`arts.cfg`:

```text
kernel.activations: 2.781881s
checksum: 8.547615248671e+07
```

Interpretation:

- The exact same compiler-generated binary that times out with
  `worker_threads=64` completes quickly with `worker_threads=8`.
- This isolates the remaining large/64 ML-kernel failures to ARTS runtime
  worker overprovision / scheduler-spin behavior, not to incorrect compiler
  scalar code generation.

## Rejected experiment

I also tried a compiler-only worker-cap experiment that rewrote repeated
uniform kernels to lower with fewer worker lanes. It changed the IR as intended,
but it made `activations` worse once task chunking was coarsened, so it was not
kept in the final tree.

## Final state

Kept, validated compiler fixes:

- ready-local path removal
- owner-dim conflict guard and heuristic refinement
- duplicate DB release cleanup
- DB destroy lowering on `arts.db_free`

Remaining blocker for a clean full `large`/`64` sweep:

- `ml-kernels/activations`
- `ml-kernels/batchnorm`

Both are now narrowed to a runtime-worker-topology issue under the stock
64-worker ARTS config, not to an outstanding compiler correctness bug.
