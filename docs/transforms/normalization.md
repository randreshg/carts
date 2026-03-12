# Normalization And Late DB Cleanup

This document explains why `LoopNormalization`, `StencilBoundaryPeeling`, and
`BlockLoopStripMining` are separate.

## `LoopNormalization`

Owns structural cleanup only.

Examples:

- symmetric triangular to rectangular normalization
- profitable perfect-nest linearization

These rewrites should not introduce a new dependence schedule. Their purpose is
to make later analysis and transforms easier.

## `StencilBoundaryPeeling`

Owns stencil interior cleanup.

Examples:

- peel first/last rows or columns
- expose a branch-free interior region

This is still normalization, not dependence rewriting. It improves the loop
domain but does not create a new schedule family.

## `BlockLoopStripMining`

Owns late DB-aware strip-mining after partitioning.

It must stay separate from normalization because it depends on facts that only
exist after DB partitioning:

- block-partitioned DB layout
- `arts.db_ref`
- proven block size
- localized access pattern

Before:

```text
for i:
  blk = i / B
  off = i % B
  view = db_ref(ptr[blk])
  use view[off]
```

After:

```text
for block:
  view = db_ref(ptr[block])
  for local:
    use view[local]
```

That is not normalization. It is a late DB-specific performance cleanup.

## Practical Rule

- if it is shape cleanup before DBs: `LoopNormalization`
- if it is stencil interior cleanup: `StencilBoundaryPeeling`
- if it is after DB partitioning and uses DB facts: `BlockLoopStripMining`
