# CARTS Optimization Flows by Pattern

This document complements [`pipeline.md`](./pipeline.md),
[`arts-dep-transforms.md`](./arts-dep-transforms.md),
[`arts-kernel-transforms.md`](./arts-kernel-transforms.md),
[`../heuristics/distribution.md`](../heuristics/distribution.md), and
[`../heuristics/partitioning.md`](../heuristics/partitioning.md).

Its purpose is simple: show how CARTS recognizes important loop/kernel families
and how each one moves through the optimization pipeline.

The diagrams below use the real pipeline step names from
`tools/compile/Compile.cpp`. They also distinguish:

- contract stamping: recognize a family and record typed attrs
- structural rewrite: change the loop/task shape
- lowering/partitioning: use those attrs to choose task and DB layout

## Legend

```text
PD  = PatternDiscovery
DT  = DepTransforms
LN  = LoopNormalization
KT  = KernelTransforms
ED  = EdtDistribution
FL  = ForLowering
DBP = DbPartitioning
```

## Shared Backbone

Every example below plugs into the same overall compiler skeleton:

```text
C/C++ + OpenMP
  |
  v
cgeist
  |
  v
raise-memref-dimensionality
  |
  v
initial-cleanup
  |
  v
openmp-to-arts
  |- ConvertOpenMPToSde
  |- ScopeSelection / ScheduleRefinement / ChunkOpt
  |- ReductionStrategy
  |- RaiseToLinalg / RaiseToTensor
  |- LoopInterchange / TensorOpt
  |- StructuredSummaries / ElementwiseFusion
  |- DistributionPlanning / IterationSpaceDecomposition
  |- BarrierElimination
  |- ConvertSdeToArts
  |- VerifySdeLowered / VerifyEdtCreated
  |
  v
edt-transforms
  |
  v
create-dbs -> db-opt -> edt-opt -> concurrency
  |
  v
edt-distribution
  |- ED
  |- FL
  |
  v
post-distribution-cleanup
  |- structural cleanup + epoch shaping
  |
  v
db-partitioning
  |- DBP + distributed ownership
  |
  v
post-db-refinement
  |- DB/EDT cleanup + contract validation
  |
  v
late-concurrency-cleanup
  |- strip-mining + hoisting + sink/cleanup
  |
  v
epochs -> pre-lowering -> arts-to-llvm -> LLVM IR / executable
```

## Ownership Map

```text
uniform / generic affine loop
  -> PD(seed/refine) or DbAnalysis fallback

elementwise_pipeline
  -> KT structural fusion

matmul
  -> LoopReordering + KT structural rewrite

stencil_tiling_nd / cross_dim_stencil_3d / higher_order_stencil
  -> PD(refine) + KT stencil contract stamping

jacobi_alternating_buffers / wavefront_2d
  -> DT structural rewrite

triangular / perfect-nest-linearization
  -> LN structural normalization
```

## 1. Uniform Data-Parallel Loop

Example:

```c
#pragma omp parallel for
for (int i = 0; i < N; ++i)
  C[i] = A[i] + B[i];
```

```text
OpenMP parallel-for
  |
  v
openmp-to-arts
  -> arts.edt<parallel> + arts.for(i)
  |
  v
PD(seed/refine)
  -> dep_pattern = uniform
  |
  v
create-dbs
  -> A/B/C become DB candidates
  |
  v
concurrency
  -> access_pattern = uniform
  |
  v
ED
  -> distribution_kind = block
     (or two_level / block_cyclic if machine strategy says so)
  |
  v
FL
  -> worker EDTs over contiguous chunks
  |
  v
DBP
  -> block partition
  -> div/mod localization into db_ref + local index
```

Why it helps:
- no special semantic rewrite is needed
- the compiler can still distribute work and partition DBs aggressively

## 2. Elementwise Pipeline Fusion

Example:

```c
#pragma omp parallel for
for (int i = 0; i < N; ++i)
  T[i] = relu(X[i]);

#pragma omp parallel for
for (int i = 0; i < N; ++i)
  Y[i] = alpha * T[i] + beta;

#pragma omp parallel for
for (int i = 0; i < N; ++i)
  Z[i] = exp(Y[i]);
```

```text
sibling pointwise parallel loops
  |
  v
openmp-to-arts
  -> sibling arts.edt<parallel> / arts.for stages
  |
  v
KT: ElementwisePipelinePattern
  -> fuse sibling stages
  -> stamp dep_pattern = elementwise_pipeline
  |
  v
create-dbs / db-opt
  -> fewer phase boundaries
  -> fewer transient DB uses
  |
  v
ED + FL
  -> one distributed loop instead of many tiny phases
  |
  v
DBP
  -> standard block partition on the fused iteration space
```

Why it helps:
- removes avoidable task/DB phase boundaries
- exposes one larger uniform kernel to the back half of the pipeline

## 3. Matmul Reduction to Update Form

Example:

```c
#pragma omp parallel for collapse(2)
for (int i = 0; i < M; ++i)
  for (int j = 0; j < N; ++j) {
    double sum = 0.0;
    for (int k = 0; k < K; ++k)
      sum += A[i][k] * B[k][j];
    C[i][j] = sum;
  }
```

```text
dot-product matmul
  |
  v
LoopReordering
  -> prefer cache-friendly order for downstream matmul form
  |
  v
KT: MatmulReductionPattern
  -> dot-product form  ->  update form
  -> stamp dep_pattern = matmul
  |
  v
create-dbs
  -> matrix DBs created after the kernel shape is normalized
  |
  v
ED
  -> intranode: block
  -> internode: tiling_2d
  |
  v
FL
  -> block workers or 2-D task grid
  |
  v
DBP
  -> block / blockND layout aligned with chosen distribution
```

Why it helps:
- turns a reduction-carried form into a form that ARTS can distribute better
- keeps ownership and partitioning aligned with the real update pattern

## 4. N-D Out-of-Place Stencil

Example:

```c
#pragma omp parallel for collapse(3)
for (int x = 1; x < X - 1; ++x)
  for (int y = 1; y < Y - 1; ++y)
    for (int z = 1; z < Z - 1; ++z)
      B[x][y][z] =
          A[x][y][z] + A[x - 1][y][z] + A[x + 1][y][z];
```

```text
out-of-place stencil
  |
  v
PD(seed)
  -> generic stencil evidence
  |
  v
IterationSpaceDecomposition (in openmp-to-arts stage)
  -> isolate interior from boundary handling
  |
  v
PD(refine) + KT: StencilTilingNDPattern
  -> stamp owner_dims
  -> stamp min/max halo offsets
  -> stamp write footprint
  -> dep_pattern = stencil_tiling_nd
  |
  v
create-dbs
  -> DBs carry explicit stencil contract
  |
  v
ED
  -> block distribution over owner dims
  |
  v
DBP
  -> stencil mode (ESD)
  -> halo-aware block layout
  -> local block writes + partial read halos
```

Why it helps:
- later passes do not have to rediscover the halo shape from raw memops
- DB partitioning can keep block locality without falling back to coarse mode

## 5. Cross-Dimension 3-D Stencil

Example:

```c
#pragma omp parallel for collapse(3)
for (int x = 1; x < X - 1; ++x)
  for (int y = 1; y < Y - 1; ++y)
    for (int z = 0; z < Z; ++z)
      B[x][y][z] =
          A[x - 1][y][z] + A[x + 1][y][z] +
          A[x][y - 1][z] + A[x][y + 1][z];
```

```text
3-D stencil with halo in multiple spatial dimensions
  |
  v
PD(refine)
  -> dep_pattern = cross_dim_stencil_3d
  -> owner/halo dims recorded explicitly
  |
  v
create-dbs
  |
  v
ED
  -> block distribution
  |
  v
DBP
  -> prefer stencil/block layout over coarse fallback
  -> widen acquires to full-range only when the chosen partition dim and
     the proven stencil-owner dims do not line up tightly enough
```

Why it helps:
- keeps the compiler in the stencil-aware path for 3-D cross-dimension cases
- separates "multi-block layout" from "this acquire may need full-range"

## 6. Higher-Order Stencil

Example:

```c
#pragma omp parallel for
for (int i = 2; i < N - 2; ++i)
  B[i] = -A[i - 2] + 8*A[i - 1] - 8*A[i + 1] + A[i + 2];
```

```text
radius > 1 stencil
  |
  v
PD(refine)
  -> dep_pattern = higher_order_stencil
  -> halo radius inferred from larger offsets
  |
  v
create-dbs
  |
  v
DBP
  -> stencil mode with larger halo metadata
  |
  v
lowering
  -> same ESD-style flow, but with wider read windows
```

Why it helps:
- preserves the same stencil architecture while scaling the halo contract

## 7. Jacobi Alternating Buffers

Example:

```c
for (int t = 0; t < T; ++t) {
  #pragma omp parallel for collapse(2)
  for (int i = 0; i < N; ++i)
    for (int j = 0; j < M; ++j)
      B[i][j] = A[i][j];

  #pragma omp parallel for collapse(2)
  for (int i = 1; i < N - 1; ++i)
    for (int j = 1; j < M - 1; ++j)
      A[i][j] = stencil(B, i, j);
}
```

```text
copy phase + stencil phase
  |
  v
DT: JacobiAlternatingBuffersPattern
  -> remove redundant copy phase
  -> alternate source/destination by timestep parity
  -> stamp dep_pattern = jacobi_alternating_buffers
  |
  v
create-dbs
  |
  v
ED
  -> block-distributed workers over the same spatial domain
  |
  v
DBP
  -> read phases keep stencil semantics when needed
  -> write phases can stay block-friendly because writes are uniform
```

Why it helps:
- removes a whole parallel phase per timestep
- gives H1 heuristics a cleaner double-buffer stencil shape

Current implementation note:
- the transform currently requires an even static timestep count

## 8. Seidel 2-D Wavefront

Example:

```c
for (int t = 0; t < T; ++t) {
  #pragma omp parallel for
  for (int i = 1; i < N - 1; ++i)
    for (int j = 1; j < M - 1; ++j)
      A[i][j] = seidel9(A, i, j);
}
```

```text
in-place 2-D stencil
  |
  v
DT: Seidel2DWavefrontPattern
  -> rewrite row-parallel loop into macro-tiled wavefront EDT DAG
  -> insert epoch + predecessor structure
  -> stamp dep_pattern = wavefront_2d
  |
  v
edt-transforms
  -> preserve/simplify the explicit diagonal predecessor structure
  |
  v
create-dbs
  |
  v
ED
  -> block-style task distribution
     (not the matmul-oriented tiling_2d path)
  |
  v
DBP
  -> stencil-aware partitioning for the tiled wavefront data shape
  |
  v
pre-lowering / arts-to-llvm
  -> ARTS task DAG with epoch synchronization
```

Why it helps:
- exposes the true dependence DAG explicitly instead of forcing a fake bulk
  parallel loop
- amortizes task overhead by using macro-tiles instead of element tasks

## 9. Symmetric Triangular Normalization

Example:

```c
#pragma omp parallel for
for (int i = 0; i < M; ++i) {
  C[i][i] = 1.0;
  for (int j = i + 1; j < M; ++j) {
    double v = f(i, j);
    C[i][j] = v;
    C[j][i] = v;
  }
}
```

```text
triangular loop with mirrored writes
  |
  v
LN: SymmetricTriangularPattern
  -> triangular bounds  ->  rectangular bounds
  -> move diagonal init after the normalized loop
  -> remove mirrored write from the cloned body
  |
  v
concurrency / ED
  -> downstream passes see a regular iteration space
  |
  v
DBP
  -> regular block partition instead of mirrored cross-partition writes
```

Why it helps:
- trades extra compute for a much cleaner partitionable iteration space

## 10. Perfect-Nest Linearization

Example:

```c
#pragma omp parallel for
for (int i = 0; i < I; ++i)
  for (int j = 0; j < J; ++j)
    for (int k = 0; k < K; ++k)
      out[i][j][k] = f(in, i, j, k);
```

```text
outer arts.for + directly nested perfect scf.for nest
  |
  v
LN: PerfectNestLinearizationPattern
  -> absorb one inner loop into a linear IV
  -> recover original indices with div/rem
  |
  v
ED + FL
  -> distribute one larger flat task space
  -> avoid nested task creation
  |
  v
DBP
  -> localize accesses using the recovered indices
```

Why it helps:
- exposes more task-level parallel work
- keeps the lowering architecture simple: one task loop, recovered N-D indices

Current implementation note:
- this transform is intentionally narrow and only fires on profitable perfect
  nests

## Reading The Pipeline End-To-End

The easiest mental model is:

```text
1. Canonicalize the IR enough to analyze it safely.
2. Convert OpenMP structure into ARTS structure.
3. Discover or rewrite into a known family.
4. Stamp a typed contract on that family.
5. Build DBs only after the family is stable.
6. Choose task distribution from the family.
7. Choose DB layout from the family + access facts.
8. Lower the already-decided structure into ARTS runtime calls and then LLVM.
```

That is the central design rule in CARTS:

```text
pattern pipeline decides "what shape this kernel really is"
distribution decides "how work is split"
partitioning decides "how data is split"
lowering decides "how the chosen plan is emitted"
```
