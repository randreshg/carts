# Transforms

This directory is the canonical documentation for CARTS transform ownership.

Use it to answer three questions:

1. What kind of transform is this?
2. Which pass should own it?
3. What facts should it expose to downstream passes?

## Transform Families

- [`dependence.md`](./dependence.md)
  Exact schedule/dependence rewrites.
  Example: wavefront Seidel, Jacobi alternating buffers.

- [`kernel.md`](./kernel.md)
  Kernel-form rewrites that expose better distribution/locality.
  Example: matmul reduction distribution.

- [`normalization.md`](./normalization.md)
  Structural loop cleanup and late DB-aware cleanup.
  Example: loop normalization, stencil boundary peeling, block strip-mining.

## Source Layout

Pass entrypoints live under domain folders in
[`lib/arts/passes/Optimizations`](/Users/randreshg/Documents/carts/lib/arts/passes/Optimizations):

- `General/`
- `Dependence/`
- `Kernel/`
- `Loop/`
- `Db/`
- `Edt/`
- `CodegenPrep/`

Reusable transform interfaces stay under
[`include/arts/Transforms`](/Users/randreshg/Documents/carts/include/arts/Transforms)
with the same domain split:

- `Dependence/DepTransform.h`
- `Kernel/KernelTransform.h`
- `Loop/LoopNormalizer.h`
- `Db/DbTransforms.h`
- `Edt/EdtTransforms.h`

Implementation sources live under
[`lib/arts/Transforms`](/Users/randreshg/Documents/carts/lib/arts/Transforms)
with matching domain folders.

## Ownership Rules

- `DepTransforms`
  Use for exact dependence-preserving schedule rewrites.

- `KernelTransforms`
  Use for semantics-preserving kernel-form rewrites that make downstream
  distribution or partitioning better.

- `LoopNormalization`
  Use for structural cleanup only. No meaningful schedule change.

- `BlockLoopStripMining`
  Keep as a late DB-aware optimization after partitioning. It is not a
  normalization pass.

## Downstream Contract

Transforms should not force later passes to rediscover high-level structure
from raw memory accesses.

The intended direction is:

- transform recognizes a stable pattern once
- transform stamps canonical ARTS metadata
- downstream DB / EDT / lowering passes consume that metadata first
- expensive recomputation is only a fallback when metadata is absent

## Related Docs

- [`../compiler/pipeline.md`](../compiler/pipeline.md)
  Full stage order and pass placement.

- [`../heuristics/partitioning.md`](../heuristics/partitioning.md)
  DB mode and partitioning heuristics.
