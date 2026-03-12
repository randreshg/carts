# Dependence Transforms

`ArtsDepTransforms` owns exact dependence-shape rewrites.

These transforms preserve program semantics and preserve the same computation
style, but change the execution schedule so later ARTS passes see a scalable
dependence shape.

This pass runs immediately after `ConvertOpenMPToArts`, so patterns should
match ARTS + `scf` IR, not affine and not runtime-lowering code.

## Belongs Here

- wavefront or skew rewrites for in-place stencils
- alternating-buffer timestep rewrites
- exact dependence-preserving transforms for triangular solves
- dynamic-programming wavefront schedules

## Does Not Belong Here

- cache-only loop interchange
- generic perfect-nest cleanup
- matmul reduction distribution
- DB mode selection
- acquire widening or lowering details

## Current Patterns

### `JacobiAlternatingBuffersPattern`

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

Why:

- removes an unnecessary copy phase
- preserves the same alternating-buffer semantics
- gives later DB and EDT passes a simpler dependence structure

### `Seidel2DWavefrontPattern`

Before:

```text
for t:
  parallel-for i:
    for j:
      A[i][j] = f(A[i-1][j-1], A[i-1][j], A[i-1][j+1], A[i][j-1], ...)
```

After:

```text
for t:
  for wave:
    parallel-for tiles on that wave:
      update tile in original local order
```

Why:

- plain row-parallel lowering is not a scalable dependence shape
- coarse/full-range fallback preserves correctness but kills scalability
- the weighted wavefront exposes legal parallelism before DB creation

## Future Patterns

- `TriangularSolveWavefrontPattern`
- `DynamicProgrammingWavefrontPattern`
- `GaussSeidel3DWavefrontPattern`
- `ForwardSubstitutionBlockWavefrontPattern`

## Downstream Expectation

Patterns here should stamp canonical ARTS pattern metadata so later passes do
not need to rediscover “this is wavefront Seidel” from raw memory accesses.
