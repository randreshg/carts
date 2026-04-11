# SDE as the Optimization Layer

## Summary

This branch moves the first optimization boundary from ARTS IR up into SDE IR.
OpenMP lowers into SDE first, SDE-owned passes make scope, schedule, chunk,
reduction-strategy, linalg, and tensor decisions there, and only
`ConvertSdeToArts` crosses into ARTS IR.

That split matters for layering:

- SDE optimization passes do not create `arts.for`, `arts.edt`, or other ARTS
  runtime ops.
- SDE may reason about semantic pattern families, dependence shapes, and
  distribution/reduction/tensor structure, but those remain SDE concepts rather
  than OpenMP syntax or ARTS runtime objects.
- SDE passes operate on SDE concepts: concurrency regions, iteration spaces,
  barriers, typed dependencies/completions, reductions, semantic access
  annotations, and tensor/linalg carriers.
- The cost model interface is SDE-owned (`SDECostModel`), with
  `ARTSCostModel` as one implementation injected through `AnalysisManager`.
- The original loop and memref body stays authoritative through the SDE phase.
  Transient `linalg.generic`, `bufferization.to_tensor`, and `tensor.empty`
  carriers exist only to recover and optimize structure inside SDE, then are
  erased at the SDE to ARTS boundary.

Pattern discovery is no longer an executable pass in the OpenMP-to-ARTS path.
The current pipeline derives semantic structure and optimization decisions in
SDE, lowers to ARTS, and then runs ARTS-native structural/runtime transforms on
the resulting contracts later.

## Boundary Rule

The branch-local layering rule is:

- OpenMP belongs only at ingress. `ConvertOpenMPToSde` translates frontend
  constructs into SDE semantics and then the SDE pipeline stops talking in
  OpenMP terms.
- SDE owns optimization decisions. Scope, schedule, chunk, reduction strategy,
  linalg structure, tensor carriers, and supported semantic pattern choices are
  decided in SDE IR.
- ARTS belongs only at lowering. `ConvertSdeToArts` materializes the chosen SDE
  semantics as `arts.*` IR.
- The way this branch "optimizes to ARTS" is through the SDE cost-model
  interface: SDE and cost-model-aware core heuristics query `SDECostModel`,
  while `ARTSCostModel` is the current runtime-specific implementation hidden
  behind that interface.

## Wavefront Ownership

Wavefront is a semantic dependence-pattern family, not an ARTS runtime concept.
Detecting that a loop should use a wavefront schedule, choosing tile geometry
or frontier granularity, and selecting any related distribution intent belong
to SDE and the `SDECostModel` interface.

That same rule applies to the other structured pattern families as well:
elementwise, stencil, matmul, reduction-mixed, and future tensor/linalg-native
dependence patterns should be classified and planned in SDE. ARTS may
materialize those contracts as `arts.*` structure later, but it should not be
the semantic source of truth for those choices.

The reusable analysis direction for this branch is therefore:

- structural loop and access classification in SDE
- tensor/linalg analyses and transforms in SDE
- contract forwarding at `ConvertSdeToArts`
- ARTS-native cleanup and runtime-shaping after the boundary

## Current OpenMP-to-ARTS Pipeline

As of this branch, `buildOpenMPToArtsPipeline` is:

```text
ConvertOpenMPToSde
  -> SdeScopeSelection
  -> SdeScheduleRefinement
  -> SdeChunkOptimization
  -> SdeReductionStrategy
  -> RaiseToLinalg
  -> RaiseToTensor
  -> SdeTensorOptimization
  -> SdeStructuredSummaries
  -> ConvertSdeToArts
  -> VerifySdeLowered
  -> DCE
  -> CSE
  -> VerifyEdtCreated
```

The later ARTS-side pipeline still exists separately. Despite the historical
stage name, it should be read as ARTS-native structural cleanup and
normalization, not as a second semantic-pattern decision layer:

```text
DepTransforms
  -> LoopNormalization
  -> StencilBoundaryPeeling
  -> LoopReordering
  -> KernelTransforms
  -> CSE
```

Current implementation note:

- Some ARTS-side passes still contain transitional semantic planning debt,
  especially around wavefront/distribution shaping. The architectural target
  remains that those passes consume SDE-authored contracts rather than invent
  new semantic policy after the boundary.

## Pass Boundaries

The sections below describe the current behavior of each SDE-stage pass. The
examples are schematic; they are meant to show what changes at each boundary,
not every attribute detail.

### `ConvertOpenMPToSde`

**Before**

```mlir
omp.parallel {
  omp.wsloop schedule(runtime) reduction(@add %sum -> %prv) { ... }
  omp.task depend(taskdependout -> %A) { ... }
  omp.task depend(taskdependin -> %A) { ... }
}
```

**After**

```mlir
arts_sde.cu_region <parallel> {
  arts_sde.su_iterate ... schedule(<runtime>) reduction[...] (%sum) { ... }
  %dep0 = arts_sde.mu_dep <write> %A ...
  arts_sde.cu_task deps(%dep0) { ... }
  %dep1 = arts_sde.mu_dep <read> %A ...
  arts_sde.cu_task deps(%dep1) { ... }
}
```

Current behavior:

- Converts `omp.parallel`, `omp.master`, `omp.single`, `omp.task`,
  `omp.wsloop`, `omp.taskloop`, `scf.parallel`, and supported
  `omp.atomic.update` forms into SDE ops.
- Preserves SDE-visible loop metadata: schedule kind, explicit chunk, nowait,
  reduction accumulator list, and reduction kind.
- Converts task dependency access modes from the OpenMP `depend(...)` clause
  itself into `arts_sde.mu_dep <read|write|readwrite>`.
- Uses typed SDE dependency handles at the boundary: `arts_sde.mu_dep`
  produces `!arts_sde.dep`, and `arts_sde.cu_task` consumes those typed
  dependency tokens.
- Does not copy generic ARTS bookkeeping metadata onto newly created SDE loop
  ops. At the SDE boundary, the IR is expected to contain `arts_sde.*` ops, not
  leaked `arts.*` loop metadata.

Important limitation:

- OpenMP task dependence slicing still extracts offsets and sizes from the
  legacy upstream `arts.omp_dep` carrier internally. That bridge is still an
  upstream ingress point, but the converted SDE IR no longer exposes
  `arts.omp_dep` at this pass boundary.

### `SdeScopeSelection`

**Before**

```mlir
arts_sde.cu_region <parallel> {
  arts_sde.cu_region <parallel> {
    ...
  }
}
```

**After**

```mlir
arts_sde.cu_region <parallel> scope(<local|distributed>) {
  arts_sde.cu_region <parallel> scope(<inherited-or-explicit>) {
    ...
  }
}
```

Current behavior:

- Preserves an explicit scope if one is already present.
- Fills in a missing top-level parallel scope from the active cost model
  topology.
- Lets a nested parallel region inherit the nearest enclosing parallel scope
  when the nested region has no explicit scope.
- Keeps an explicit nested scope authoritative, even when it differs from the
  inherited outer scope.

Current test coverage includes all four cases:

- explicit local preserved
- explicit distributed preserved
- nested missing scope inherits explicit local or distributed
- nested explicit local or distributed overrides the inherited outer scope

### `SdeScheduleRefinement`

**Before**

```mlir
arts_sde.su_iterate ... schedule(<auto>) { ... }
arts_sde.su_iterate ... schedule(<runtime>) { ... }
```

**After**

```mlir
arts_sde.su_iterate ... schedule(<static|guided|dynamic>) { ... }
```

Current behavior:

- Runs only on one-dimensional loops whose source schedule is still abstract
  (`auto` or `runtime`) and that do not already carry an explicit chunk.
- Uses `SDECostModel::getSchedulingOverhead(...)` to choose among
  `static`, `guided`, and `dynamic`.
- Refines constant-trip loops directly.
- Refines symbolic-trip loops only when the same schedule wins across a small
  set of cost-model-derived probe trip counts.

What it does not do:

- It does not invent chunk sizes. That is left to `SdeChunkOptimization`.
- It does not rewrite multidimensional schedules.

### `SdeChunkOptimization`

**Before**

```mlir
arts_sde.su_iterate ... schedule(<dynamic>) { ... }
arts_sde.su_iterate ... schedule(<guided>) { ... }
```

**After**

```mlir
arts_sde.su_iterate ... schedule(<dynamic>, %chunk) { ... }
arts_sde.su_iterate ... schedule(<guided>, %chunk) { ... }
```

Current behavior:

- Preserves any explicit chunk that already exists.
- Synthesizes a chunk only for one-dimensional `dynamic` or `guided` loops.
- Uses worker count and minimum useful work from the active `SDECostModel`.
- Handles both constant and symbolic trip counts.
- Uses signed-safe symbolic span computation, clamps negative runtime spans to
  zero, and avoids the earlier unsigned fast-path bug for zero-based symbolic
  loops.

This pass is validated at the SDE boundary. The contract after this pass is an
`arts_sde.su_iterate` with a concrete SDE schedule and chunk, not an
`arts.for`.

### `SdeReductionStrategy`

**Before**

```mlir
arts_sde.su_iterate ... reduction[...] (%sum) { ... }
```

**After**

```mlir
arts_sde.su_iterate ... reduction[...] (%sum)
    reduction_strategy(<atomic|tree|local_accumulate>) { ... }
```

Current behavior:

- Annotates eligible reduction-bearing `arts_sde.su_iterate` ops with an
  SDE-owned strategy recommendation.
- Uses reduction kind, worker count, and nested-loop structure from the current
  loop body plus the active cost model.
- Chooses `local_accumulate` when the reduction loop already contains a nested
  sequential loop.
- Chooses between `atomic` and `tree` for loop-uniform reductions by comparing
  atomic update cost against collective reduction cost.

Current limitation:

- This pass only stamps a strategy attribute. It does not change SDE loop
  topology, allocate reduction buffers, or build a reduction tree in SDE.

### `RaiseToLinalg`

**Before**

```mlir
arts_sde.su_iterate ... {
  %a = memref.load ...
  %b = memref.load ...
  %c = arith.addf %a, %b
  memref.store %c, ...
  arts_sde.yield
}
```

**After**

```mlir
arts_sde.su_iterate ... linalg_classification(<elementwise|matmul|stencil|reduction>) {
  ... original memref/scalar body remains ...
  linalg.generic ... ins(...) outs(...) { ... }
  arts_sde.yield
}
```

Current behavior:

- Walks `arts_sde.su_iterate`, collects perfect loop nests, and stamps an
  SDE-owned structural classification on the source loop.
- Recognizes four structural families:
  - `elementwise`
  - `matmul`
  - `stencil`
  - `reduction`
- Materializes a transient memref-backed `linalg.generic` carrier only for the
  currently supported subset.

Supported carrier raising today:

- elementwise loops
- narrow matmul loops
- a narrow reduction subset:
  - one-dimensional `arts_sde.su_iterate`
  - exactly one reduction accumulator
  - exactly one zero-offset load and one zero-offset store on that accumulator
  - no extra writes

Fallback-only today:

- stencil loops
- broader reduction shapes
- nested or multi-accumulator reduction cases outside the narrow subset

That fallback is still useful: the pass stamps `linalg_classification` on the
SDE loop itself, so `ConvertSdeToArts` can recover the intended contract later
without depending on ARTS-namespaced strings.

### `RaiseToTensor`

**Before**

```mlir
linalg.generic ins(%a, %b : memref<...>, memref<...>)
               outs(%out : memref<...>) { ... }
```

**After**

```mlir
%ta = bufferization.to_tensor %a : memref<...> to tensor<...>
%tb = bufferization.to_tensor %b : memref<...> to tensor<...>
%tout = tensor.empty() : tensor<...>           // overwrite-safe outputs only
%res = linalg.generic ins(%ta, %tb : tensor<...>, tensor<...>)
                    outs(%tout : tensor<...>) { ... }
```

Or, for preserved-value / in-place outputs:

```mlir
%tout = bufferization.to_tensor %out : memref<...> to tensor<...>
%res = linalg.generic ... outs(%tout : tensor<...>) { ... }
```

Current behavior:

- Rewrites supported transient memref-backed `linalg.generic` carriers inside
  SDE into pure-tensor `linalg.generic` form.
- Keeps the original loop and memref body authoritative.
- Chooses output initialization per output operand:
  - overwrite-safe elementwise outputs use `tensor.empty`
  - preserved-value or in-place outputs reuse `bufferization.to_tensor`
- Leaves non-elementwise outputs on the preserved-value path instead of
  inventing `tensor.empty`.

Recent correctness fix:

- The output-init decision now strips simple `memref.cast` alias chains when it
  checks whether an output is also read as an input. That fixes the narrow
  cast-through-wrapper case where an in-place destination was previously
  misclassified as overwrite-safe and incorrectly rewritten to `tensor.empty`.

### `SdeTensorOptimization`

**Before**

```mlir
arts_sde.su_iterate(%lb) to(%ub) step(%step)
    linalg_classification(<elementwise|matmul>) {
  ... tensor-backed carrier ...
}
```

**After**

```mlir
arts_sde.su_iterate(%lb) to(%ub) step(%tiled_step)
    linalg_classification(<elementwise|matmul>) {
  %tile_ub = ...
  scf.for %iv = %tile_base to %tile_ub step %step {
    ... cloned scalar/memref body ...
  }
  arts_sde.yield
}
```

Current behavior:

- Uses the active `SDECostModel` to choose a tile size from worker count and
  minimum iterations per worker.
- Rewrites the surrounding `arts_sde.su_iterate`, not just the carrier, so the
  tiled loop shape survives into `arts.for` after lowering.
- Supports two executable tensor subsets today:
  - one-dimensional elementwise loops
  - a narrow matmul subset tiled on the outer parallel dimension

Current restrictions:

- no existing chunk
- no reductions
- top-level one-dimensional `arts_sde.su_iterate`
- classification already present
- static or absent schedule only

What it does not do yet:

- no tensor-side transform for stencil loops
- no tensor-side transform for general reductions
- no multidimensional non-matmul strip-mining at the SDE level

### `SdeStructuredSummaries`

**Before**

```mlir
arts_sde.su_iterate(%c1) to(%c63) step(%c1) classification(<stencil>) {
  ... loop body ...
}
```

**After**

```mlir
arts_sde.su_iterate(%c1) to(%c63) step(%c1) classification(<stencil>) {
  ... loop body ...
} {
  accessMinOffsets = [-1, -1],
  accessMaxOffsets = [1, 1],
  ownerDims = [0, 1],
  spatialDims = [0, 1],
  writeFootprint = [0, 0]
}
```

Current behavior:

- Refreshes the SDE-owned `classification(<...>)` attribute from the current
  structured loop analysis.
- Stamps only runtime-neutral neighborhood summaries on eligible
  `arts_sde.su_iterate` ops.
- Keeps ARTS-specific contract materialization out of SDE. No
  `distribution_pattern`, `depPattern`, `stencil_*`, or other `arts.*`
  lowering attrs are authored here.
- Gives `ConvertSdeToArts` enough information to materialize the runtime
  contract at the boundary without re-deriving the semantic summary from ARTS
  IR.

### `ConvertSdeToArts`

**Before**

```mlir
arts_sde.cu_region ...
arts_sde.su_iterate ...
arts_sde.cu_task ...
arts_sde.mu_dep ...
... transient linalg/tensor carriers ...
```

**After**

```mlir
arts.edt ...
arts.for ...
arts.db_control ...
arts.barrier
arts.atomic_add
```

Current behavior:

- This is the only intended boundary where SDE lowering becomes ARTS-aware.
- Converts:
  - `arts_sde.cu_region` to `arts.edt`
  - `arts_sde.su_iterate` to `arts.for`
  - `arts_sde.cu_task` to `arts.edt <task>` plus `arts.db_control`
  - `arts_sde.su_barrier` to `arts.barrier`
  - `arts_sde.cu_atomic` to `arts.atomic_add` when the reduction kind is add
- For task dependencies consumed by `arts_sde.cu_task`, lowers `arts_sde.mu_dep`
  directly to `arts.db_control`.
- For standalone `arts_sde.mu_dep`, recreates a legacy `arts.omp_dep` for the
  downstream passes that still expect that bridge.

Contract recovery at this boundary:

- If transient `linalg.generic` carriers exist, the pass classifies and stamps
  contracts from them.
- If no carrier survives, the pass falls back to the SDE-owned
  `linalg_classification` on the source `arts_sde.su_iterate`.
- It forwards `reduction_strategy` from SDE onto the lowered `arts.for` and,
  if missing there, also onto the parent `arts.edt`.
- It erases memref-backed and tensor-backed carriers after consuming them, so
  downstream ARTS passes do not need to understand tensor IR.

## Reduction Strategy Audit

The reduction-strategy path is no longer just metadata forwarding.

What is real today:

1. `SdeReductionStrategy` stamps an SDE-owned strategy recommendation on
   `arts_sde.su_iterate`.
2. `ConvertSdeToArts` forwards that strategy to the lowered `arts.for` and its
   parent `arts.edt`.
3. `EdtReductionLowering` now resolves `arts.reduction_strategy` from the
   `arts.for` first, then the parent `arts.edt`, and changes the final combine
   code path accordingly.

Current downstream consumers:

- `local_accumulate`
  - Uses the existing partial-accumulator path and performs a linear combine in
    the result EDT.
- `atomic`
  - Uses the same partial-accumulator framework, but the result EDT combines
    worker partials with `arts.atomic_add` when the reduced element type is an
    integer.
  - For non-integer element types, the current lowering falls back to the
    linear combine path.
- `tree`
  - Uses the same partial-accumulator framework, but the result EDT performs a
    pairwise combine loop before writing the final value.

What is still not implemented:

- No separate reduction topology is built in SDE.
- No dedicated tree-shaped EDT graph is built downstream yet.
- No fully atomic lowering bypasses worker partial accumulators yet.
- The comment in `EdtTransformsPass::selectReductionStrategies()` still says
  the attribute is annotation-only; that comment is stale relative to the
  current `EdtReductionLowering` behavior.

So the accurate statement is:

- `reduction_strategy` is now a real lowering input for the result-combine
  phase.
- It is not yet a fully different end-to-end reduction architecture.

## Coverage on This Branch

The branch now has focused pass-boundary coverage for the main SDE features.
The important point is where the checks live:

- SDE passes are validated at SDE boundaries.
- `arts.for` checks belong after `ConvertSdeToArts`, not inside SDE pass
  contracts.

Current coverage includes:

- scope selection
  - explicit local preserved
  - explicit distributed preserved
  - nested inheritance from explicit local or distributed
  - nested explicit override in both directions
- schedule and chunk handling
  - `auto` and `runtime` schedule refinement
  - explicit chunk preservation
  - symbolic dynamic and guided chunk synthesis with signed-safe trip counts
- task dependence ownership at the SDE boundary
  - OpenMP depend mode maps to `arts_sde.mu_dep`
  - generic ARTS loop metadata does not leak onto SDE loops
- linalg carrier raising
  - uniform elementwise
  - stencil classification fallback
  - narrow reduction carrier raising
  - negative fallback coverage for broader local-accumulate-style reductions
- tensor carrier rewriting
  - fill
  - unary elementwise
  - binary elementwise
  - in-place binary
  - multi-output elementwise
  - cast-alias in-place output preservation
  - narrow matmul carrier rewriting
- tensor-driven SDE transforms
  - elementwise tiling
  - fill tiling
  - narrow matmul outer-dimension tiling
- reduction strategy selection and forwarding
  - atomic
  - tree
  - local_accumulate

## Current Limitations

The branch is materially ahead of the original plan, but some work remains.

- Upstream OpenMP task dependence slicing still enters SDE through the legacy
  `arts.omp_dep` bridge, even though the SDE IR after conversion is cleaned up.
- `RaiseToLinalg` still keeps stencil loops and broader reduction shapes on the
  classification-only fallback path.
- `RaiseToTensor` and `SdeTensorOptimization` currently operate on the
  supported carrier subsets only; they are not yet a general tensorization and
  transformation framework for every `arts_sde.su_iterate`.
- `SdeTensorOptimization` does not yet transform reduction or stencil kernels.
- Standalone `arts_sde.mu_dep` still lowers back to `arts.omp_dep` for legacy
  downstream consumers.
- The original scalar/memref body is still the source of truth through SDE.
  The linalg/tensor path is an SDE optimization substrate, not a replacement
  executable form yet.

## Bottom Line

The current branch state is:

- SDE is the optimization layer for scope, abstract schedule refinement, chunk
  synthesis, reduction-strategy selection, structural classification, and the
  first real linalg/tensor transformations.
- SDE optimization passes stay ARTS-unaware.
- `ConvertSdeToArts` is the boundary that translates SDE decisions into ARTS
  IR and removes transient carriers.
- The tensor path is no longer analysis-only: it now performs real
  cost-model-guided transformations on supported elementwise and matmul cases.
- Reduction strategy is partially consumed downstream in real lowering, but the
  implementation still shares one partial-accumulator framework rather than
  separate end-to-end lowering topologies.
