# ArtsDepTransforms

`ArtsDepTransforms` is the compiler layer for exact dependence-shape rewrites.

Its job is simple:

- preserve program semantics
- change the loop/task schedule to expose a scalable dependency shape
- run before `CreateDbs`, so DB creation and partitioning see the improved loop structure

This pass is not for generic loop cleanup. It is not for cache-only reorderings. It is not for DB lowering details.

## Ownership

- `LoopNormalization`
  Structural normalization with no meaningful dependence-schedule change.
- `ArtsDepTransforms`
  Exact dependence-preserving rewrites that change the schedule shape.
- `LoopReordering`
  Cache-oriented loop order changes when legal.
- `LoopTransforms`
  Algebraic / reduction-distribution transforms such as matmul reshaping.
- `DbPartitioning`
  Chooses DB partitioning after the loop schedule has already been improved.

## Compiler Graph

```text
source loop nest
  -> LoopNormalization
     normalize shape only
  -> ArtsDepTransforms
     rewrite dependence schedule
  -> LoopReordering / LoopTransforms
     improve locality / distribution shape
  -> CreateDbs
     materialize DB ownership from the transformed loop
  -> DbPartitioning
     choose allocation and acquire modes
```

## Why This Pass Exists

Some kernels are limited by their dependence shape, not by missing DB heuristics.

The important case is `seidel-2d`:

- the original in-place update creates loop-carried dependences
- keeping the loop coarse or forcing full-range acquires preserves correctness
  but kills scalability
- the right answer is to rewrite the loop into a dependence-equivalent wavefront schedule before DB creation

That is exactly what `ArtsDepTransforms` is for.

## Current Pattern

### `JacobiAlternatingBuffersPattern`

This is the old Jacobi copy-elimination rewrite under a clearer dependence-oriented name.

Before:

```text
for t:
  parallel-for dst = src
  parallel-for src = stencil(dst, forcing)
```

After:

```text
for t:
  if odd(t):
    parallel-for dst = stencil(src, forcing)
  else:
    parallel-for src = stencil(dst, forcing)
```

Why it belongs here:

- it changes the dependence schedule across timestep phases
- it removes an unnecessary phase boundary
- it gives later DB and EDT passes a simpler and more scalable shape

## Planned Pattern

### `Seidel2DWavefrontPattern`

Target:

```text
for t:
  parallel-for i:
    for j:
      A[i][j] = f(A[i-1][j-1], A[i-1][j], A[i-1][j+1], A[i][j-1], ...)
```

Problem:

- plain row-parallel lowering is not a scalable dependence shape
- coarse/full-range safety keeps correctness but blocks useful partitioning

Desired rewrite:

```text
for t:
  for wavefront:
    parallel-for tiles/points on that wavefront:
      update tile/point in original row-major local order
```

For this kernel, the useful exact schedule is based on a weighted wavefront, not plain anti-diagonals.

Why:

- `i + j` is not sufficient because of the `(i-1, j+1) -> (i, j)` dependence
- a weighted schedule such as `w = 2*i + j` separates those dependences correctly

The production version should be tiled, not point-wise, so we get:

- fewer epochs / EDTs
- contiguous ownership hints for DB partitioning
- scalable dependency shape without coarse fallback

## What Belongs Here

Good fits:

- wavefront / skew transforms for in-place stencils
- alternating-buffer timestep rewrites
- exact dependence-preserving transforms for triangular solves
- dynamic-programming wavefront schedules

Poor fits:

- generic perfect-nest linearization
- cache-only interchange
- matmul reduction distribution
- DB-mode selection or acquire widening

## Interface Direction

Patterns in this pass should expose only a small amount of information to later stages:

- transformed loop structure
- optional distribution hints, for example `wavefront_2d`
- optional owner/tile metadata when the transform creates tiled wavefronts

`DbPartitioning` should consume those hints. It should not re-derive the schedule transform itself.

## Design Rule

If a transformation is justified by:

- "this loop schedule is semantically valid and exposes a better dependence shape"

it belongs in `ArtsDepTransforms`.

If it is justified by:

- "this order is more cache-friendly"
- "this reduction can be distributed differently"
- "this DB acquire can be partitioned more safely"

it belongs somewhere else.
