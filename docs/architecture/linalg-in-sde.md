# Linalg and Tensor in SDE

## Summary

This branch does not use linalg or tensor as a diagnostic-only experiment.
`RaiseToLinalg`, `RaiseToTensor`, and `Tiling` are real SDE
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
  -> RaiseToTensor
  -> RaiseToLinalg
  -> LoopInterchange
  -> Tiling
  -> StructuredSummaries
  -> ElementwiseFusion
  -> ScopeSelection
  -> ScheduleRefinement
  -> ChunkOpt
  -> ReductionStrategy
  -> DistributionPlanning
  -> IterationSpaceDecomposition
  -> BarrierElimination
  -> LowerToMemref
  -> ConvertToCodelet
  -> ConvertSdeToArts
  -> VerifySdeLowered
  -> DCE
  -> CSE
  -> VerifyEdtCreated
```

That means linalg and tensor work happens before ARTS IR exists. The SDE phase
decides scope, schedule, chunk, reduction strategy, structural loop
classification, and the supported tensor/linalg transforms. ARTS IR only sees
the resulting SDE decisions after `ConvertSdeToArts`.

## Carrier Model

The branch uses a **carrier-authoritative** model for elementwise, matmul, and
reduction patterns. `arts_sde.su_iterate` remains the authoritative SDE
scheduling op, but the `linalg.generic` carrier inside it is now the
**source of truth for computation** — the scalar `memref.load/store` body is
erased by RaiseToLinalg after creating the carrier.

After RaiseToLinalg, downstream passes operate on the carrier directly:

- **LoopInterchange**: permutes carrier dims via `linalg::interchangeGenericOp()`
- **Tiling**: tiles the carrier via `extract_slice` → tiled `linalg.generic` → `insert_slice`
- **ElementwiseFusion**: reads carrier DPS outputs to determine write-set disjointness
- **BarrierElimination**: reads carrier DPS inputs/outputs for read/write set analysis

Each pass has a fallback path that walks `memref.load/store` for dual-rep or
stencil bodies (stencils have no carrier yet). LowerToMemref restores memref
form before ConvertSdeToArts, which consumes the recovered contract data and
stamps it onto the lowered ARTS ops.

This is why the current design preserves both properties at once:

- SDE scheduling semantics stay on `arts_sde.su_iterate`.
- Structured compute semantics are expressed through the authoritative
  linalg/tensor carrier, which downstream passes transform directly.

## Carrier Lifecycle

The `linalg.generic` carrier flows through the SDE pipeline as follows:

| Pass | Role |
|---|---|
| `RaiseToLinalg` | Creates carrier, wraps external memrefs via `sde.mu_memref_to_tensor`, erases scalar body |
| `LoopInterchange` | Permutes carrier dims via `linalg::interchangeGenericOp()` |
| `Tiling` | Tiles carrier via `extract_slice` / `insert_slice` |
| `ElementwiseFusion` | Reads carrier DPS outputs to determine write-set disjointness |
| `BarrierElimination` | Reads carrier DPS inputs/outputs for read/write set analysis |
| `LowerToMemref` | Traces carrier tensors back to memref, creates `scf.for` loops, lowers to executable form |
| `ConvertSdeToArts` | Reads carrier for contract data, erases any remaining carrier artifacts |

Stencil bodies bypass the carrier entirely — they stay on the scalar
`memref.load`/`memref.store` path and are handled by the dual-rep fallback in
each pass.

## `RaiseToLinalg`

`RaiseToLinalg` walks `arts_sde.su_iterate`, classifies structurally
recognizable loop bodies via `sde::analyzeStructuredLoop`, stamps
`structured_classification` on the SDE loop, and materializes a transient
tensor-backed `linalg.generic` carrier for the currently supported subset.

External memref operands are wrapped via `sde::SdeMuMemrefToTensorOp` (the
SDE-native boundary op for external data), not `bufferization::ToTensorOp`.

After creating the carrier, the pass **erases the scalar body** — all
`memref.load`, `memref.store`, and now-dead `arith` ops are removed
iteratively. This makes the carrier authoritative: downstream passes MUST read
the carrier, not scalar ops. Passes that also need to handle stencil or
classification-only bodies maintain a fallback path that walks `memref.load` /
`memref.store`.

### What it raises today

- Elementwise loop nests
- A narrow matmul subset
- A narrow reduction subset:
  - one-dimensional `arts_sde.su_iterate`
  - exactly one reduction accumulator
  - exactly one zero-offset load and one zero-offset store on that accumulator
  - no extra writes

### What it leaves on the fallback path

- Stencil loops (`supportsLinalgCarrier()` returns false)
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

After (carrier-authoritative — scalar body erased):

```mlir
arts_sde.su_iterate(%lb) to(%ub) step(%step)
    structured_classification(<elementwise>) {
^bb0(%i: index):
  %tA = sde.mu_memref_to_tensor %A : memref<?xf32> -> tensor<?xf32>
  %tB = sde.mu_memref_to_tensor %B : memref<?xf32> -> tensor<?xf32>
  %tC = sde.mu_memref_to_tensor %C : memref<?xf32> -> tensor<?xf32>
  %res = linalg.generic ins(%tA, %tB : tensor<?xf32>, tensor<?xf32>)
                        outs(%tC : tensor<?xf32>) {
  ^bb0(%a: f32, %b: f32, %c: f32):
    %sum = arith.addf %a, %b : f32
    linalg.yield %sum : f32
  } -> tensor<?xf32>
  arts_sde.yield
}
```

## `RaiseToTensor`

`RaiseToTensor` performs function-scope tensor mem2reg on `memref.alloca`
values. It converts local allocas to tensor SSA form so that downstream passes
can reason about value flow through `scf.for` iter_args, `cu_region`, and
`su_iterate` bodies.

This pass does NOT wrap external memrefs — that is handled by
`RaiseToLinalg` via `sde::SdeMuMemrefToTensorOp`. `RaiseToTensor` only
rewrites allocas whose users are exclusively `memref.load`, `memref.store`,
`memref.cast`, and `sde.mu_dep` (no calls, no `su_iterate` operands, etc.).

### Current behavior

- `memref.store` → `tensor.insert`, advances `currentTensor[alloca]`
- `memref.load` → `tensor.extract` from `currentTensor[alloca]`
- Region-holding ops (`scf.for`, `cu_region`, `su_iterate`) → thread current
  tensors as iter_args, recurse into body, update `currentTensor` to result
- `memref.cast` alias chains are stripped when tracing back to the alloca

### Shape of the IR

Before:

```mlir
%scratch = memref.alloca() : memref<4xf32>
memref.store %val, %scratch[%i] : memref<4xf32>
%v = memref.load %scratch[%j] : memref<4xf32>
```

After:

```mlir
%scratch = tensor.empty() : tensor<4xf32>
%t1 = tensor.insert %val into %scratch[%i] : tensor<4xf32>
%v = tensor.extract %t1[%j] : tensor<4xf32>
```

## `Tiling`

`Tiling` performs real SDE tensor-level transforms using the active
`SDECostModel`. It operates in dual mode depending on whether the body is
carrier-authoritative or dual-representation.

### Carrier-authoritative path

When the body contains an authoritative `linalg.generic` carrier (no scalar
`memref.load`/`memref.store` ops), the pass tiles the carrier directly:

1. Creates `tensor.extract_slice` on each carrier input/output
2. Builds a tiled `linalg.generic` operating on the slices
3. Writes results back via `tensor.insert_slice`

No `scf.for` tile loop is needed — the carrier's iteration domain is
partitioned through the slice/insert_slice mechanism.

### Dual-representation fallback path

When scalar ops coexist with the carrier (stencils, or classification-only
bodies), the pass creates `scf.for` tile loops and clones the scalar body into
the innermost tile loop.

### Supported transforms

- N-dimensional elementwise tiling (carrier-authoritative)
- Matmul tiling on the outer parallel dimension (carrier-authoritative)
- Stencil tiling with halo-aware bounds (dual-rep)
- Reduction tiling on parallel dimensions only (dual-rep)

### Current restrictions

- No existing explicit chunk
- Classification must already be present
- Schedule must be static or absent

### Shape of the IR

Carrier-authoritative (elementwise):

```mlir
// Before
arts_sde.su_iterate(%lb) to(%ub) step(%step)
    structured_classification(<elementwise>) {
  %tA = sde.mu_memref_to_tensor ...
  %res = linalg.generic ins(%tA : ...) outs(%tOut : ...) { ... }
  arts_sde.yield
}

// After
arts_sde.su_iterate(%lb) to(%ub) step(%tiledStep)
    structured_classification(<elementwise>) {
  %slice_in = tensor.extract_slice %tA[...] : ...
  %slice_out = tensor.extract_slice %tOut[...] : ...
  %tiled = linalg.generic ins(%slice_in : ...) outs(%slice_out : ...) { ... }
  %updated = tensor.insert_slice %tiled into %tOut[...] : ...
  arts_sde.yield
}
```

Dual-rep fallback (stencil):

```mlir
arts_sde.su_iterate(%lb) to(%ub) step(%tiledStep)
    structured_classification(<stencil>) {
  scf.for %iv = %tileBase to %tileUb step %step {
    ... cloned scalar body ...
  }
  arts_sde.yield
}
```

## `LoopInterchange`

`LoopInterchange` reorders loop dimensions to improve data locality. Like
`Tiling`, it operates in dual mode.

### Carrier-authoritative path

When the body is carrier-authoritative, the pass permutes the carrier's
iterator dimensions directly via `linalg::interchangeGenericOp()`. No
`scf.for` loops need to be swapped because the carrier encodes the iteration
order in its indexing maps.

Example: for matmul, swaps j-k to k-j for stride-1 access on B.

### Dual-rep fallback path

When scalar ops are present (stencils), the pass swaps `scf.for` loops in the
enclosing nest.

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

After the contract data is recovered, the pass erases the transient carriers:

- **Memref-backed generics**: erased directly (no SSA result users).
- **Tensor-backed carriers**: erased via iterative dead-code cascade — the
  `linalg.generic` result, `tensor.insert_slice`, `tensor.extract_slice`, and
  `sde.mu_memref_to_tensor` ops are removed in dependency order.

Downstream ARTS passes do not need to understand tensor IR or keep linalg
alive. That is the current contract:

- Linalg and tensor are available during the SDE optimization window.
- ARTS IR sees the resulting decisions, not the transient carrier chain.

## Supported Subsets

The current implementation is deliberately narrow.

| Pass | Supported subset |
|---|---|
| `RaiseToLinalg` | elementwise, narrow matmul, narrow 1-D single-accumulator reduction; scalar body erased (carrier-authoritative) |
| `RaiseToTensor` | local `memref.alloca` values raised to tensor SSA form |
| `LoopInterchange` | carrier-authoritative: `interchangeGenericOp()`; dual-rep: `scf.for` loop swap |
| `Tiling` | carrier-authoritative: `extract_slice`/`insert_slice`; dual-rep: `scf.for` + scalar clone; N-dim cost-model-driven |
| `ConvertSdeToArts` | carrier-derived contract recovery plus classification-only fallback recovery |
| `LowerToMemref` | tensor-backed carriers and tensor SSA lowered back to memref form |
| `ConvertToCodelet` | cu_region single iter_args to mu_data/mu_token/cu_codelet graph |

## Current Limitations

The branch is materially beyond the old diagnostic-only design, but it is not
yet a fully general linalg or tensor optimizer.

- `RaiseToLinalg` still keeps stencil loops and broader reduction shapes on
  the classification-only fallback path (`supportsLinalgCarrier()` returns
  false for stencils).
- `RaiseToTensor` only raises local `memref.alloca` values, not arbitrary
  memref loop bodies.
- `Tiling` handles stencils via dual-rep `scf.for` + scalar clone, not the
  carrier-authoritative path.
- `Tiling` does not yet perform tensor-side reduction transformations (ARTS
  handles partial accumulation).
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

- `RaiseToLinalg` creates carrier-authoritative `linalg.generic` carriers and
  erases the scalar body
- `RaiseToTensor` raises local allocas to tensor SSA form
- `LoopInterchange` and `Tiling` transform carriers directly or fall back to
  dual-rep scalar loop manipulation
- `LowerToMemref` restores memref form before the ARTS boundary
- `ConvertSdeToArts` consumes contract data and erases remaining carrier
  artifacts

That is the current implemented design on this branch.
