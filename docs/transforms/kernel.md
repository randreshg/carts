# Kernel Transforms

`ArtsKernelTransforms` owns semantics-preserving kernel-form rewrites.

These transforms change how a kernel is expressed so later ARTS passes can
distribute, partition, hoist, or localize it better. They are not just loop
cleanup, and they are not pure schedule/dependence rewrites.

## Belongs Here

- reduction-to-update rewrites
- contraction rewrites that expose better distribution
- mathematically equivalent kernel reshaping for later tiling/locality

## Does Not Belong Here

- exact wavefront or skew scheduling
- generic loop normalization
- late DB-local strip-mining

## Current Pattern

### `MatmulReductionPattern`

Before:

```text
for i:
  for j:
    sum = 0
    for k:
      sum += A[i,k] * B[k,j]
    C[i,j] = sum
```

After:

```text
for i:
  initialize C row/tile
  for k:
    a = A[i,k]
    for j:
      C[i,j] += a * B[k,j]
```

Why it belongs here:

- same final mathematical result
- different kernel form
- downstream tiling, distribution, and locality become much better

## Design Rule

If the transformed kernel looks like a different computational form of the
same math, it belongs here rather than in `ArtsDepTransforms`.

## Downstream Expectation

Patterns here should stamp canonical ARTS pattern metadata just like
dependence transforms do. The downstream consumer contract should be shared
even though the transform family is different.
