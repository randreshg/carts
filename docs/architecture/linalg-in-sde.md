# Linalg and Tensor in SDE

## Summary

This branch does not use linalg or tensor as a diagnostic-only experiment.
`RaiseToLinalg`, `RaiseToTensor`, and `SdeTensorOptimization` are real SDE
passes in the OpenMP-to-ARTS pipeline. They materialize transient carriers
inside `arts_sde.su_iterate`, use those carriers to recover or transform
structured computation at the SDE level, and then let `ConvertSdeToArts`
consume and erase them before the ARTS pipeline continues.

The important boundary is:

- SDE optimization passes stay SDE-owned and ARTS-unaware.
- Tensor and linalg IR appear only as transient carriers inside SDE.
- `ConvertSdeToArts` is the boundary that translates SDE decisions into ARTS
  contracts and removes the transient carriers.

## Current Pipeline

The relevant part of `buildOpenMPToArtsPipeline` is:

```text
ConvertOpenMPToSde
  -> SdeScopeSelection
  -> SdeScheduleRefinement
  -> SdeChunkOptimization
  -> SdeReductionStrategy
  -> RaiseToLinalg
  -> RaiseToTensor
  -> SdeTensorOptimization
  -> ConvertSdeToArts
```

That means linalg and tensor work happens before ARTS IR exists. The SDE phase
decides scope, schedule, chunk, reduction strategy, structural loop
classification, and the supported tensor/linalg transforms. ARTS IR only sees
the resulting SDE decisions after `ConvertSdeToArts`.

## Carrier Model

The branch uses a carrier model, not a replacement model.

`arts_sde.su_iterate` remains the authoritative SDE scheduling op. The pass
sequence may attach one of two transient structured carriers inside that loop:

- a memref-backed `linalg.generic`
- a tensor-backed `linalg.generic`

The original loop and scalar or memref body remain authoritative through the
SDE phase. The carrier exists to make structure explicit enough for SDE-local
analysis and transform passes. After that, `ConvertSdeToArts` consumes the
carrier, stamps the recovered contract data onto the lowered ARTS ops, and
erases the carrier chain.

This is why the current design preserves both properties at once:

- SDE scheduling semantics stay on `arts_sde.su_iterate`.
- Structured compute semantics become visible to SDE passes through transient
  linalg and tensor IR.

## `RaiseToLinalg`

`RaiseToLinalg` is no longer a diagnostic pass. It walks
`arts_sde.su_iterate`, classifies structurally recognizable loop bodies, stamps
`structured_classification` on the SDE loop, and materializes a transient
memref-backed `linalg.generic` carrier for the currently supported subset.

### What it raises today

- Elementwise loop nests
- A narrow matmul subset
- A narrow reduction subset:
  - one-dimensional `arts_sde.su_iterate`
  - exactly one reduction accumulator
  - exactly one zero-offset load and one zero-offset store on that accumulator
  - no extra writes

### What it leaves on the fallback path

- Stencil loops
- Broader reduction shapes
- Multi-accumulator reductions
- Nested reduction forms outside the narrow supported slice
- Bodies that do not admit the required structured memref or loop pattern

The fallback path is still meaningful. Even when it does not build a carrier,
the pass stamps `structured_classification` on the SDE loop so that
`ConvertSdeToArts` can recover the intended contract family later without
depending on ARTS-only names during the SDE phase.

### Shape of the IR

Before:

```mlir
arts_sde.su_iterate(%lb) to(%ub) step(%step) {
^bb0(%i: index):
  %a = memref.load %A[%i] : memref<?xf32>
  %b = memref.load %B[%i] : memref<?xf32>
  %sum = arith.addf %a, %b : f32
  memref.store %sum, %C[%i] : memref<?xf32>
  arts_sde.yield
}
```

After:

```mlir
arts_sde.su_iterate(%lb) to(%ub) step(%step)
    structured_classification(<elementwise>) {
^bb0(%i: index):
  ... original memref body remains ...
  linalg.generic ins(%A, %B : memref<?xf32>, memref<?xf32>)
                 outs(%C : memref<?xf32>) {
  ^bb0(%a: f32, %b: f32, %c: f32):
    %sum = arith.addf %a, %b : f32
    linalg.yield %sum : f32
  }
  arts_sde.yield
}
```

## `RaiseToTensor`

`RaiseToTensor` rewrites supported transient memref-backed carriers into
tensor-backed `linalg.generic` carriers inside SDE. This is also real
rewriting, not analysis-only.

### Current behavior

- Inputs become `bufferization.to_tensor`.
- Outputs are initialized per output operand.
- The original loop and memref body stay authoritative.
- The pass keeps the improvement narrow and alias-safe.

### Output initialization policy

For each output operand, the pass selects one of two forms:

- `tensor.empty` for overwrite-safe outputs
- `bufferization.to_tensor` for preserved-value or in-place outputs

That policy is now broader and safer than the initial version:

- overwrite-safe elementwise and disjoint-write outputs can use `tensor.empty`
- preserved-value and read-modify-write outputs stay on the
  `bufferization.to_tensor` path
- simple `memref.cast` alias chains are stripped during the proof, so a casted
  read of the destination does not get misclassified as overwrite-safe
- narrow reduction carriers stay read-modify-write when the accumulator is both
  an input and an output

### Shape of the IR

Before:

```mlir
linalg.generic ins(%a, %b : memref<...>, memref<...>)
               outs(%out : memref<...>) { ... }
```

After, overwrite-safe:

```mlir
%ta = bufferization.to_tensor %a : memref<...> to tensor<...>
%tb = bufferization.to_tensor %b : memref<...> to tensor<...>
%tout = tensor.empty() : tensor<...>
%res = linalg.generic ins(%ta, %tb : tensor<...>, tensor<...>)
                    outs(%tout : tensor<...>) { ... }
```

After, preserved-value or in-place:

```mlir
%ta = bufferization.to_tensor %a : memref<...> to tensor<...>
%tb = bufferization.to_tensor %b : memref<...> to tensor<...>
%tout = bufferization.to_tensor %out : memref<...> to tensor<...>
%res = linalg.generic ins(%ta, %tb : tensor<...>, tensor<...>)
                    outs(%tout : tensor<...>) { ... }
```

## `SdeTensorOptimization`

`SdeTensorOptimization` performs real SDE tensor-level transforms using the
active `SDECostModel`. It does not just inspect carriers. It rewrites the
surrounding `arts_sde.su_iterate` so the optimized loop structure survives
back into executable memref or loop IR after the carrier is erased.

### What it does today

- Chooses a tile size from the active `SDECostModel`
- Rewrites the enclosing `arts_sde.su_iterate`
- Preserves executable scalar or memref lowering by cloning or rebuilding the
  loop body, not by requiring tensor IR downstream

### Supported tensor transforms today

- One-dimensional elementwise tiling
- Narrow matmul tiling on the outer parallel dimension
- Two-dimensional all-parallel elementwise tiling for a narrow perfect-nest
  subset with a disjoint write set

### Current restrictions

- no existing explicit chunk
- no reduction kernels in the tensor optimization pass
- classification must already be present
- schedule must be static or absent
- the pass still operates on a narrow executable subset rather than general
  tensorization

### Shape of the IR

Before:

```mlir
arts_sde.su_iterate(%lb) to(%ub) step(%step)
    structured_classification(<elementwise>) {
  ... tensor-backed carrier ...
}
```

After:

```mlir
arts_sde.su_iterate(%lb) to(%ub) step(%tiledStep)
    structured_classification(<elementwise>) {
  %tileUb = ...
  scf.for %iv = %tileBase to %tileUb step %step {
    ... rebuilt executable body ...
  }
  arts_sde.yield
}
```

The key point is that the pass transforms the SDE-owned loop nest, not just
the carrier.

## `ConvertSdeToArts`

`ConvertSdeToArts` is where the transient carrier model closes. This pass is
responsible for:

- lowering `arts_sde.*` ops to ARTS IR
- recovering or forwarding semantic contract data
- erasing transient memref-backed and tensor-backed carriers

### Contract recovery

At this boundary:

- if a transient `linalg.generic` carrier exists, the pass reads it and stamps
  contracts onto the lowered ARTS ops
- if no carrier survives, the pass falls back to the SDE-owned
  `structured_classification`
- fallback stencil contract data is preserved from the SDE loop itself, so a
  classification-only path does not lose persisted stencil payloads
- `reduction_strategy` is forwarded from SDE onto the lowered `arts.for` and,
  when needed, its parent `arts.edt`

### Carrier erasure

After the contract data is recovered, the pass erases the transient
memref-backed and tensor-backed carriers. Downstream ARTS passes do not need to
understand tensor IR or keep linalg alive.

That is the current contract:

- linalg and tensor are available during the SDE optimization window
- ARTS IR sees the resulting decisions, not the transient carrier chain

## Supported Subsets

The current implementation is deliberately narrow.

| Pass | Supported subset |
|---|---|
| `RaiseToLinalg` | elementwise, narrow matmul, narrow 1-D single-accumulator reduction |
| `RaiseToTensor` | supported raised carriers with per-output init selection |
| `SdeTensorOptimization` | 1-D elementwise tiling, narrow matmul tiling, 2-D disjoint-write elementwise tiling |
| `ConvertSdeToArts` | carrier-derived contract recovery plus classification-only fallback recovery |

## Current Limitations

The branch is materially beyond the old diagnostic-only design, but it is not
yet a fully general linalg or tensor optimizer.

- `RaiseToLinalg` still keeps stencil loops and broader reduction shapes on the
  fallback path.
- `RaiseToTensor` does not tensorize arbitrary memref loop bodies.
- `SdeTensorOptimization` does not yet transform stencil kernels.
- `SdeTensorOptimization` does not yet perform tensor-side reduction
  transformations.
- The current matmul and disjoint-write support is intentionally narrow and
  tied to executable subsets that can lower cleanly back into the existing
  pipeline.
- `ConvertSdeToArts` still recreates a legacy `arts.omp_dep` bridge for
  downstream users that have not yet been fully moved off that representation.

## What This Document Replaces

Older drafts of this file described `RaiseToLinalg` as diagnostic-only and
treated tensor and linalg integration as future architecture. That is no longer
accurate.

The current state is:

- `RaiseToLinalg` builds real transient memref-backed carriers
- `RaiseToTensor` builds real transient tensor-backed carriers
- `SdeTensorOptimization` performs real cost-model-guided SDE transforms
- `ConvertSdeToArts` consumes and erases those carriers before the ARTS phase

That is the current implemented design on this branch.
