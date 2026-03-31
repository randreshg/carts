# Benchmark Stabilization History and Current Status

Date: 2026-03-31

Code checkpoint: `602dd428` (`Refine partitioning analysis and stencil lowering`)

## Purpose

This document records the current CARTS optimization and benchmark
stabilization state after the latest compiler work. It is meant to be a
practical handoff: what changed, why it changed, what was validated, what
improved, and what still needs more work.

## Ground Rules Used in This Round

- Use `carts` or `tools/carts` as the entry point.
- Keep compiler-side changes mode-agnostic where possible.
- Keep DB partitioning and lowering decisions N-D capable unless a narrower
  physical ownership model is explicitly required by the contract.
- Do not introduce ARTS runtime changes unless the compiler-side options are
  exhausted and the runtime need is clearly justified.
- Do not keep heuristics that look good in IR but regress real benchmark runs.

## High-Level Outcome

This round focused on three themes:

1. clean separation of partitioning policy from DB/application rewrites
2. preserving lowering contracts through EDT/DB rewriting so later passes can
   reason from the acquire itself
3. reducing avoidable over-decomposition for repeated intranode stencil loops

The resulting compiler state is materially better for several lagging kernels,
especially the read-only mixed-owner cases and Jacobi-like repeated stencils,
but the Seidel wavefront path is still not competitive enough and needs a
wavefront-specific follow-up.

## Major Compiler Changes

### 1. Partitioning policy and analysis cleanup

The DB partitioning flow was pushed further toward analysis-first policy and
pass-level application.

Relevant areas:

- `include/arts/analysis/graphs/db/DbNode.h`
- `include/arts/analysis/heuristics/PartitioningHeuristics.h`
- `include/arts/utils/BlockedAccessUtils.h`
- `lib/arts/analysis/db/DbAnalysis.cpp`
- `lib/arts/analysis/graphs/db/DbBlockInfoComputer.cpp`
- `lib/arts/analysis/graphs/db/PartitionBoundsAnalyzer.cpp`
- `lib/arts/analysis/heuristics/PartitioningHeuristics.cpp`
- `lib/arts/passes/opt/db/DbPartitioning.cpp`
- `lib/arts/transforms/db/DbBlockPlanResolver.cpp`
- `lib/arts/transforms/db/DbPartitionPlanner.cpp`
- `lib/arts/transforms/db/DbRewriter.cpp`
- `lib/arts/transforms/db/block/DbBlockIndexer.cpp`
- `lib/arts/transforms/db/stencil/DbStencilIndexer.cpp`
- `lib/arts/transforms/db/stencil/DbStencilRewriter.cpp`
- `lib/arts/utils/DbUtils.cpp`

What changed:

- mixed-owner read-only allocations can now prefer coarse mode when that is the
  correct ownership-preserving choice instead of being forced into a fragmented
  block layout
- more of the owner-dim, block-shape, and full-range reasoning is shared
  through common analysis/planner utilities instead of being re-derived in
  individual rewrites
- N-D block/stencil plans now preserve more authoritative information through
  indexing and rewrite stages

Why it matters:

- this improved `atax`, `bicg`, and `correlation`
- it also reduced the amount of mode- or benchmark-specific branching needed in
  later passes

### 2. Lowering contract preservation through EDT rewriting

The EDT rewrite/lowering path now keeps the semantic ownership contract alive
through task-local acquires instead of forcing later passes to infer it again.

Relevant areas:

- `include/arts/transforms/edt/EdtRewriter.h`
- `lib/arts/passes/transforms/EdtLowering.cpp`
- `lib/arts/passes/transforms/ForLowering.cpp`
- `lib/arts/transforms/edt/EdtRewriter.cpp`
- `lib/arts/transforms/edt/WorkDistributionUtils.cpp`

What changed:

- task-local acquires inherit semantic contract metadata from the loop or
  parent acquire
- lowering carries `arts.partition_hint` onto rewritten task acquires when that
  hint is still authoritative for DB planning
- task-local element slices are materialized only when the ownership contract is
  strong enough to make them sound
- DB-space and element-space slice metadata are kept distinct, especially for
  stencil reads with halo expansion

Why it matters:

- `DbPartitioning` can now reason from the rewritten acquire instead of relying
  on pass-order-sensitive inference
- this made the lowering path cleaner and more mode-agnostic
- it also reduced cases where contracts silently widened back to coarse or
  whole-range views

### 3. DbModeTightening and contract-sensitive mode cleanup

Relevant area:

- `lib/arts/passes/opt/db/DbModeTightening.cpp`

What changed:

- late mode tightening was extended so post-partitioning/post-rewrite acquires
  keep the correct effective mode instead of drifting back toward conservative
  read-only behavior

Why it matters:

- activation outputs and similar kernels now preserve `<out>` behavior after
  concurrency-opt rewrites

### 4. Repeated-stencil intranode strip coarsening

Relevant area:

- `lib/arts/analysis/heuristics/DistributionHeuristics.cpp`

What changed:

- generic nested-loop work estimation remains bounded for broad use
- repeated intranode stencil loops now use a larger per-outer-iteration work
  estimate when deciding whether to coarsen worker-owned strips
- this applies only to repeated non-wavefront stencil loops, not to the
  wavefront-specific Seidel path

Why it matters:

- prior to this change, Jacobi-like loops frequently fell back to the worker
  alignment path and emitted small `128`-row strips
- after this change, the Jacobi benchmark’s interior row loop receives a real
  `arts.partition_hint` and lowers to larger strips instead of one thin slice
  per worker

## Tests Added or Updated

Added:

- `tests/contracts/atax_concurrency_opt_preserves_block_output_modes.mlir`
- `tests/contracts/atax_read_only_mixed_owner_dims_prefer_coarse.mlir`
- `tests/contracts/matmul_reduction_inputs_preserve_full_range.mlir`

Updated:

- `tests/contracts/activations_concurrency_opt_preserves_output_modes.mlir`
- `tests/contracts/activations_host_full_view_input_stays_coarse.mlir`
- `tests/contracts/jacobi_stencil_intranode_worker_cap.mlir`
- `tests/contracts/partitioning/modes/db_partition_mode_stencil.mlir`
- `tests/contracts/partitioning/safety/poisson_for_full_range_forcing_contract.mlir`
- `tests/contracts/seidel_wavefront_block_stripmine.mlir`
- `tests/contracts/seidel_wavefront_tiling2d_lane_bias.mlir`

Focused lit validation that passed on the current tree:

- `jacobi_stencil_intranode_worker_cap`
- `jacobi_stencil_normalization`
- `seidel_wavefront_block_stripmine`
- `seidel_wavefront_keeps_frontier_parallelism`
- `seidel_wavefront_emit_ro_halo_slices`

## Benchmark Findings

### A. Last validated full 64-thread baseline

Full benchmark run:

- `carts-benchmarks/results/20260331_123439/results.json`

Summary:

- 23 benchmarks passed
- geometric mean kernel speedup: `1.2647894057982314x`

Worst laggards in that baseline were:

- `polybench/jacobi2d`
- `polybench/seidel-2d`
- `stream`
- `polybench/correlation`
- `polybench/atax`
- `polybench/bicg`
- `ml-kernels/activations`

### B. Mixed-owner coarse-policy improvement

Targeted run:

- `carts-benchmarks/results/20260331_125911/results.json`

Observed effect:

- `atax`, `bicg`, and `correlation` improved after the read-only mixed-owner
  coarse policy landed

### C. Jacobi repeated-stencil improvement

Current targeted run:

- `carts-benchmarks/results/20260331_142314/results.json`

Comparison against the earlier full baseline:

- `polybench/jacobi2d`
  - baseline: ARTS `6.474903s`, OMP `2.529598s`, speedup `0.390677x`
  - current: ARTS `5.332937s`, OMP `2.973646s`, speedup `0.557600x`
- `polybench/seidel-2d`
  - baseline: ARTS `8.023591s`, OMP `4.292427s`, speedup `0.534976x`
  - current: ARTS `8.264229s`, OMP `4.225786s`, speedup `0.511335x`

Interpretation:

- Jacobi improved materially and correctly
- Seidel stayed correct but did not improve enough

## Seidel-Specific Findings

Seidel remains the main unresolved stencil benchmark.

What we found:

- Seidel is not using the generic repeated-stencil strip coarsening path
- it arrives earlier as `depPattern = wavefront_2d`
- the wavefront macro-tiling is selected in
  `lib/arts/transforms/dep/Seidel2DWavefrontPattern.cpp`
- the chosen wavefront tile shape becomes the semantic `stencil_block_shape`
  that later lowering consumes

What we tried:

- changing the wavefront tile-budget logic in
  `DistributionHeuristics::chooseWavefront2DTilingPlan`
- changing the budget while keeping the same weighted-wavefront contract
- changing the budget while shrinking the frontier shape coherently

Result:

- both experimental wavefront retunes regressed real Seidel runs and were
  reverted
- the current tree keeps the better of the measured variants

Current conclusion:

- Seidel now needs a wavefront-specific optimization, not more generic Jacobi
  strip coarsening

## Other Remaining Laggards

The next compiler-side triage targets are still:

- `stream`
- `ml-kernels/activations`
- `polybench/correlation`

Current expectation:

- `stream` likely needs a structural fix in EDT formation or loop handling, not
  just DB partition tuning
- `activations` still needs better ARTS-aware handling around host-visible
  initialization versus worker-local output partitioning
- `correlation` still needs better ownership and reduction-sensitive layout
  choices

## Recommended Next Steps

1. Rerun the full 64-thread large benchmark sweep on the current commit before
   making more heuristic changes.
2. Triage `stream` through the pipeline first, because it is a known structural
   laggard and should be separable from stencil work.
3. Revisit `ml-kernels/activations` and `polybench/correlation` with the
   cleaned partitioning infrastructure now in place.
4. Treat Seidel as a dedicated wavefront optimization track:
   wavefront tile-count, dependency registration cost, and task-creation
   amortization should be investigated together.

## Notes on Uncommitted Artifacts

Generated compiler outputs under `tests/contracts/Output/` were intentionally
not committed. They are local inspection artifacts, not source-of-truth inputs.
