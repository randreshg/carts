# SDE Dialect Specification

**Structured Decomposition Environment** -- runtime-agnostic parallel IR.

## Overview

SDE captures parallel computation intent using three orthogonal op categories:

```
sde.cu_*  (Compute Units)    -- WHAT computation happens
sde.su_*  (Scheduling Units) -- HOW/WHEN it runs
sde.mu_*  (Memory Units)     -- data access and movement
```

SDE is NOT `arts_sde` -- it is runtime-agnostic and could target any
task-based runtime (ARTS, Legion, StarPU, GPU).

**Input is OpenMP code.** The OMP-to-SDE conversion must be natural and direct.

## Tensor-First Design

Following IREE's Flow dialect — which operates ENTIRELY in tensor space — SDE
ops are **type-polymorphic**: they natively accept tensor, memref, and scalar
types. This is the single most important design decision in SDE.

### Why Tensor Types Are Native to SDE

IREE's Flow dialect never sees memrefs. All optimization — partitioning,
dispatch formation, fusion, scheduling — happens on immutable tensor values.
Memrefs appear only after bufferization, during final codegen. SDE follows
the same principle:

| Property | Memref (current CARTS) | Tensor (SDE native) |
|---|---|---|
| Aliasing | Mutable pointers; alias analysis required | Immutable SSA values; no aliasing possible |
| Dependencies | Must be explicitly tracked | Implicit in SSA def-use chains — FREE |
| Access regions | Must be declared (offsets/sizes) | `tensor.extract_slice` IS the declaration |
| Optimization | Alias analysis limits fusion/tiling | Arbitrary fusion, tiling, composition |
| Pattern classify | Requires custom PatternDiscovery | linalg `iterator_types` classify automatically |
| Buffer decisions | Manual DB mode analysis | One-shot bufferization — proven, optimal |

### SDE Ops Are Type-Polymorphic

SDE CU ops implement `DestinationStyleOpInterface` (like `linalg.generic`),
enabling native composition with MLIR's tile-and-fuse infrastructure. Every
CU op accepts **both tensor and memref** operands:

```mlir
// TENSOR PATH: ins/outs notation, SSA deps are automatic
%A_slice = tensor.extract_slice %A[%i][%N][1] : tensor<?xf64> to tensor<?xf64>
%B_init  = tensor.extract_slice %B[%i][%N][1] : tensor<?xf64> to tensor<?xf64>

%result = sde.cu_task
    ins(%A_slice : tensor<?xf64>)
    outs(%B_init : tensor<?xf64>) {
^bb0(%a: tensor<?xf64>, %b: tensor<?xf64>):
  %computed = linalg.generic { ... } ins(%a) outs(%b) { ... } -> tensor<?xf64>
  sde.yield %computed : tensor<?xf64>
} -> tensor<?xf64>

%B_new = tensor.insert_slice %result into %B[%i][%N][1] : ...

// MEMREF FALLBACK: explicit deps, side-effecting body
%dep = sde.mu_dep <write> %B[%i] size [%N] : memref<?xf64> -> !sde.dep
sde.cu_task deps(%dep) {
^bb0(%B_view: memref<?xf64>):
  memref.store %val, %B_view[%j] : memref<?xf64>
  sde.yield
}
```

### What Standard MLIR Ops Replace in the Tensor Path

In the tensor path, standard MLIR ops replace custom SDE MU ops:

| SDE Op (memref fallback) | Standard MLIR (tensor path) |
|---|---|
| `sde.mu_dep <read> %A[%i] size [%N]` | `tensor.extract_slice %A[%i][%N][1]` |
| `sde.mu_dep <write> %B[%i] size [%N]` | `tensor.insert_slice %result into %B[%i][%N][1]` |
| `sde.mu_access <read> %A[%lo] size [%sz]` | Implicit in `tensor.extract_slice` |
| Custom dependency analysis | SSA def-use chains (free, exact) |
| Custom alias analysis | No aliasing by construction |
| Custom DB mode analysis | One-shot bufferization (optimal) |

### Two-Path Architecture

SDE does NOT require all code to be in tensor form. The pipeline is
**best-effort raising** with graceful fallback:

```
OMP-to-SDE: produces memref-based SDE ops (faithful to Polygeist)
     |
     v
RaiseToLinalg: scf.for+memref -> linalg.generic on memref (best-effort)
     |          [code that can't be raised stays as scf.for+memref]
     v
RaiseToTensor: linalg on memref -> linalg on tensor (best-effort)
     |          [code that can't be raised stays as linalg on memref]
     v
SDE Analysis: tensor bodies get alias-free SSA analysis
              memref bodies get conservative metadata-based analysis
     |
     v
Bufferize: tensor -> memref (one-shot, upstream MLIR)
     |
     v
SDE->ARTS: all back to memref for ARTS core
```

~60% of benchmarks reach the tensor path (structured loops). ~40% stay
in the memref fallback (indirect, sparse, I/O). Both paths coexist
naturally because SDE ops are type-polymorphic.

## Dialect Definition

```tablegen
def SdeDialect : Dialect {
  let name = "sde";
  let cppNamespace = "::mlir::sde";
  let summary = "Structured Decomposition Environment -- runtime-agnostic parallel IR";
  let useDefaultTypePrinterParser = 1;
  let useDefaultAttributePrinterParser = 1;
}

class Sde_Op<string mnemonic, list<Trait> traits = []>
    : Op<SdeDialect, mnemonic, traits>;
```

## Types

| Type | Mnemonic | Purpose |
|---|---|---|
| `CompletionType` | `sde.completion` | Opaque token signaling CU completion |
| `DepType` | `sde.dep` | Typed dependency handle with mode + region |

SDE defines `sde.completion` instead of reusing `async.token` because the
async dialect has runtime-specific semantics (coroutine lowering) that
conflict with SDE's runtime-agnostic design.

## Custom Effect Resources

SDE declares custom `MemoryEffects` resources for granular effect tracking:

```cpp
namespace mlir::sde {

struct ComputeResource : public Resource::Base<ComputeResource> {
  StringRef getName() final { return "ComputeResource"; }
};

struct DataResource : public Resource::Base<DataResource> {
  StringRef getName() final { return "DataResource"; }
};

struct SyncResource : public Resource::Base<SyncResource> {
  StringRef getName() final { return "SyncResource"; }
};

} // namespace mlir::sde
```

Effect declarations per op:

| Op | ComputeResource | DataResource | SyncResource |
|---|---|---|---|
| `sde.cu_region` | Write (spawns) | via RecursiveMemoryEffects | -- |
| `sde.cu_task` | Write (spawns) | via RecursiveMemoryEffects | -- |
| `sde.cu_reduce` | -- | Read+Write (accumulator) | -- |
| `sde.cu_atomic` | -- | Write | -- |
| `sde.su_iterate` | Write (spawns iters) | -- | -- |
| `sde.su_barrier` | -- | -- | Write |
| `sde.mu_dep` | -- | -- (Pure) | -- |
| `sde.mu_access` | -- | Read or Write | -- |

## Enums

```
SdeAccessMode       : read | write | readwrite
SdeCuKind           : parallel | single | task
SdeScheduleKind     : static | dynamic | guided | auto | runtime
SdeReductionKind    : add | mul | min | max | and | or | xor | custom
SdeConcurrencyScope : local | distributed
```

## Metadata Attributes

```
SdeLoopInfoAttr   : { tripCount, nestingLevel, hasInterIterationDeps, locationKey }
SdeMemAccessAttr  : { allAccessesAffine, hasStencilPattern, estimatedFootprint }
```

Pattern attributes live in `patterns/`, NOT in SDE:
```
PatternAttr   : semantic pattern family (stencil/uniform/matmul)
ContractAttr  : lowering contract with pattern + strategy
```

---

## CU (Compute Unit) Ops

Every CU makes three things explicit:

- **STATE**: Block args receive data views (tensor or memref);
  `iter_args` carry loop state; `sde.yield` forwards values
- **DEPENDENCY**: Tensor path: implicit in SSA def-use chains.
  Memref fallback: `$deps` from `sde.mu_dep` with access mode and region.
- **EFFECT**: `MemoryEffectsOpInterface` + `RecursiveMemoryEffects` trait

All CU ops implement `DestinationStyleOpInterface` with optional `ins/outs`
operands, enabling native composition with linalg tile-and-fuse.

### `sde.cu_region` -- Parallel or Single Execution Region

Captures `omp.parallel`, `omp.single`, `omp.master`.

```mlir
// Memref form (from OMP-to-SDE, before raising)
sde.cu_region parallel scope(<distributed>) {
  // body with memref.load/store
  sde.yield
}

// Tensor form (after RaiseToTensor)
%B_new = sde.cu_region parallel scope(<distributed>)
    ins(%A : tensor<?xf64>)
    outs(%B : tensor<?xf64>) -> tensor<?xf64> {
  // body with linalg.generic on tensors
  sde.yield %result : tensor<?xf64>
}
```

**Operands**: `kind` (parallel/single/task), `concurrency_scope` (optional),
`nowait` (unit attr), `ins` (tensor/memref), `outs` (tensor/memref),
`deps` (Variadic<sde.dep> -- memref fallback only)

**Results**: Tensor results (tensor path) and/or optional `sde.completion` token

**Region**: Single-block body terminated by `sde.yield`

**Effects**: Write on ComputeResource; RecursiveMemoryEffects propagates body

### `sde.cu_task` -- Task with Dependencies

Captures `omp.task` with `depend(in/out/inout)` clauses.

**Tensor path** -- dependencies implicit in SSA:

```mlir
// tensor.extract_slice replaces sde.mu_dep <read>
%A_slice = tensor.extract_slice %A[%i][%N][1] : tensor<?xf64> to tensor<?xf64>
%B_init  = tensor.extract_slice %B[%i][%N][1] : tensor<?xf64> to tensor<?xf64>

%result = sde.cu_task
    ins(%A_slice : tensor<?xf64>)
    outs(%B_init : tensor<?xf64>) {
^bb0(%a: tensor<?xf64>, %b: tensor<?xf64>):
  %computed = linalg.generic {
    indexing_maps = [...], iterator_types = ["parallel"]
  } ins(%a : tensor<?xf64>) outs(%b : tensor<?xf64>) {
  ^bb0(%a_elem: f64, %b_elem: f64):
    linalg.yield %val : f64
  } -> tensor<?xf64>
  sde.yield %computed : tensor<?xf64>
} -> tensor<?xf64>

// tensor.insert_slice replaces sde.mu_dep <write>
%B_new = tensor.insert_slice %result into %B[%i][%N][1] : ...
```

**Memref fallback** -- explicit `sde.mu_dep` deps:

```mlir
%dep_A = sde.mu_dep <read> %A[%i] size [%N] : memref<?xf64> -> !sde.dep
%dep_B = sde.mu_dep <write> %B[%i] size [%N] : memref<?xf64> -> !sde.dep

%done = sde.cu_task deps(%dep_A, %dep_B) {
^bb0(%A_view: memref<?xf64>, %B_view: memref<?xf64>):
  // body uses memref.load/store
  sde.yield
} : !sde.completion
```

**Key insight**: In the tensor path, `tensor.extract_slice` IS the read
declaration and `tensor.insert_slice` IS the write declaration. No custom
`sde.mu_dep` needed — standard MLIR ops carry the same information.

**Effect**: Write on ComputeResource; RecursiveMemoryEffects for body

**Result**: Tensor results (tensor path) and/or `sde.completion` token

### `sde.cu_reduce` -- Reduction Accumulation

Captures `omp.wsloop reduction(+: sum)`. **PRESERVES the combiner semantics
that the current pipeline LOSES.**

```mlir
// TENSOR PATH: accumulator is a tensor value (SSA, no aliasing)
%result = sde.cu_reduce add
    ins(%partial : tensor<f64>)
    outs(%acc : tensor<f64>)
    identity (%zero : f64) -> tensor<f64>

// TENSOR PATH: custom combiner
%result = sde.cu_reduce custom
    ins(%partial : tensor<f64>)
    outs(%acc : tensor<f64>)
    identity (%zero : f64) combiner {
^bb0(%lhs: f64, %rhs: f64):
  %r = arith.addf %lhs, %rhs : f64
  sde.yield %r : f64
} -> tensor<f64>

// MEMREF FALLBACK: accumulator is a memref (side-effecting)
sde.cu_reduce add (%acc, %partial : memref<f64>, f64)
    identity (%zero : f64)
```

In the tensor path, the accumulator is an immutable tensor value returned
via SSA — no atomic updates, no aliasing concerns. SDE-to-ARTS conversion
decides whether to implement as `arts.atomic_add` or a tree reduction.

**Effect**: Read+Write on DataResource

### `sde.cu_atomic` -- Atomic Memory Update

Captures `omp.atomic.update` (currently only add{i,f}). **Memref-only** --
atomics are inherently side-effecting operations on mutable memory locations.
There is no meaningful tensor form.

```mlir
sde.cu_atomic add (%addr, %val : memref<f64>, f64)
```

In the tensor path, atomics arise only AFTER bufferization (when tensor
values become memref locations). Before bufferization, what would be an
atomic is expressed as a `sde.cu_reduce` on tensor values.

**Effect**: Write on DataResource

---

## SU (Scheduling Unit) Ops

SU ops carry scheduling policy without computation content.

### `sde.su_iterate` -- Parallel Iteration Space

Captures `omp.wsloop + omp.loop_nest`, `omp.taskloop`, `scf.parallel`.
Implements `LoopLikeOpInterface` for iter_args state flow.

**Tensor path** -- `iter_args` carry tensor values, enabling natural
alternating-buffer patterns without aliasing:

```mlir
// Jacobi alternating buffers: swap A/B each iteration
%A_final, %B_final = sde.su_iterate (%c0) to (%T) step (%c1)
    iter_args(%A_iter = %A_init : tensor<?x?xf64>,
              %B_iter = %B_init : tensor<?x?xf64>) {
^bb0(%t: index, %A: tensor<?x?xf64>, %B: tensor<?x?xf64>):
  %B_new = linalg.generic {
    indexing_maps = [affine_map<(i) -> (i-1)>,
                     affine_map<(i) -> (i+1)>,
                     affine_map<(i) -> (i)>],
    iterator_types = ["parallel"]
  } ins(%A, %A : tensor<?x?xf64>, tensor<?x?xf64>)
    outs(%B : tensor<?x?xf64>) {
  ^bb0(%left: f64, %right: f64, %b: f64):
    %sum = arith.addf %left, %right : f64
    %val = arith.mulf %half, %sum : f64
    linalg.yield %val : f64
  } -> tensor<?x?xf64>
  sde.yield %B_new, %A : tensor<?x?xf64>, tensor<?x?xf64>  // swap
} -> (tensor<?x?xf64>, tensor<?x?xf64>)
```

Key insight: In tensor space, the A/B swap is just SSA value forwarding.
No aliasing, no double-buffering analysis needed. Bufferization decides
whether to allocate in-place.

**Tensor path with reduction:**

```mlir
%final = sde.su_iterate (%c0) to (%N) step (%c1)
    reduction [#sde<reduction_kind<add>>]
    iter_args(%sum = %sum_init : f64) identity (%zero : f64) {
^bb0(%i: index, %partial: f64):
  %a_slice = tensor.extract_slice %A[%i][1][1] : tensor<?xf64> to tensor<f64>
  %a = tensor.extract %a_slice[] : tensor<f64>
  %new = arith.addf %partial, %a : f64
  sde.yield %new : f64
} : f64
```

**Memref fallback** -- classic form:

```mlir
sde.su_iterate (%c1) to (%Nm1) step (%c1)
    schedule(<static>, %chunk) {
^bb0(%i: index):
  %a = memref.load %A[%i] : memref<?xf64>
  memref.store %val, %B[%i] : memref<?xf64>
  sde.yield
}
```

**Operands**: lb, ub, step, schedule, chunk_size, nowait, collapse,
iter_args (tensor/memref/scalar), reduction_kinds, reduction_identities,
loop_info

**State**: Induction var + iter_args as block args (LoopLikeOpInterface).
Tensor iter_args enable SSA-based dep tracking across iterations.

**Effect**: Write on ComputeResource

### `sde.su_barrier` -- Synchronization Point

Captures `omp.barrier` (implicit) and `omp.taskwait` (explicit tokens).

```mlir
// Implicit scope barrier (from omp.barrier)
sde.su_barrier

// Explicit token barrier (from omp.taskwait)
sde.su_barrier(%task1_done, %task2_done)
```

**Effect**: Write on SyncResource

### `sde.su_distribute` -- Distribution Hint

Advisory op for mapping work to execution resources. A lowering pass MAY
ignore it.

```mlir
sde.su_distribute scope(<distributed>) workers(64) {
  sde.su_iterate (%c0) to (%N) step (%c1) { ... }
  sde.yield
}
```

---

## MU (Memory Unit) Ops

MU ops declare data access patterns and dependencies. **In the tensor path,
standard MLIR ops replace most MU ops** — `tensor.extract_slice` is a read
declaration, `tensor.insert_slice` is a write declaration, and SSA def-use
chains ARE the dependency graph. MU ops exist primarily for the memref
fallback path and for explicit task ordering where SSA can't express the
dependency (e.g., point-to-point synchronization between tasks on different
branches of control flow).

### `sde.mu_dep` -- Data Dependency Declaration

Replaces the current `arts.omp_dep` -> `arts.db_control` two-op chain.
**Used in the memref fallback path only.** In the tensor path,
`tensor.extract_slice`/`tensor.insert_slice` carry the same information
natively.

```mlir
// Region dependency (from depend(in: A[0:N]))
%dep = sde.mu_dep <read> %A[%c0] size [%N] : memref<?xf64> -> !sde.dep

// Single element (from depend(out: x))
%dep = sde.mu_dep <write> %x : memref<f64> -> !sde.dep

// Whole array (no offsets/sizes = entire memref)
%dep = sde.mu_dep <readwrite> %M : memref<?x?xf64> -> !sde.dep
```

**Pure** -- no side effects; produces `sde.dep` consumed by CU ops.

### `sde.mu_access` -- In-Body Access Region Annotation

**Memref fallback only.** Used INSIDE CU bodies to annotate which subregion
is actually touched (enables precise DB mode tightening in downstream passes).
In the tensor path, `tensor.extract_slice` carries the same information
natively and this op is not needed.

```mlir
%view = sde.mu_access <read> %A[%lo] size [%sz] : memref<?xf64> -> memref<?xf64>
```

### `sde.mu_reduction_decl` -- Reduction Symbol (Module-Level)

Module-level symbol declaring reduction identity + combiner. Directly
replaces `omp.declare_reduction`.

```mlir
sde.mu_reduction_decl @sum_f64 : f64 add
  identity { %z = arith.constant 0.0 : f64; sde.yield %z : f64 }
  combiner { ^bb0(%a: f64, %b: f64): %s = arith.addf %a, %b; sde.yield %s : f64 }
```

### `sde.yield` -- Universal Terminator

```mlir
sde.yield                    // void return
sde.yield %val : f64         // return value (reductions, iter_args)
```

Terminates all SDE regions. Implements `ReturnLike` + `Terminator`.

---

## OMP-to-SDE Conversion

### Conversion Pattern Table (12 patterns)

| # | OMP Op | SDE Op(s) | Key Mapping |
|---|---|---|---|
| 1 | `omp.parallel` | `sde.cu_region parallel` | scope=distributed |
| 2 | `omp.master` | `sde.cu_region single` | scope=local |
| 3 | `omp.single` | `sde.cu_region single` | scope=local |
| 4 | `omp.wsloop + loop_nest` | `sde.su_iterate` in `sde.cu_region parallel` | schedule/chunk/nowait preserved |
| 5 | `omp.task + depend` | `sde.cu_task` with `sde.mu_dep` | deps as block args |
| 6 | `omp.taskloop` | `sde.su_iterate` | no explicit schedule |
| 7 | `scf.parallel` | `sde.cu_region parallel` + `sde.su_iterate` | barrier if work follows |
| 8 | `omp.atomic.update` | `sde.cu_atomic` | reduction_kind from body |
| 9 | `omp.barrier` | `sde.su_barrier` | implicit (no tokens) |
| 10 | `omp.taskwait` | `sde.su_barrier(%tokens...)` | explicit completion tokens |
| 11 | `omp.terminator` | `sde.yield` | 1:1 |
| 12 | `@omp_get_thread_num` etc. | attribute on enclosing CU | runtime query deferred |

### Example 1: Parallel For (Jacobi Stencil)

```c
#pragma omp parallel for
for (int i = 1; i < N-1; i++)
    B[i] = 0.5 * (A[i-1] + A[i+1]);
```

After OMP-to-SDE:
```mlir
sde.cu_region parallel scope(<distributed>) {
  sde.su_iterate (%c1) to (%Nm1) step (%c1) {
  ^bb0(%i: index):
    %im1 = arith.subi %i, %c1 : index
    %ip1 = arith.addi %i, %c1 : index
    %a0 = memref.load %A[%im1] : memref<?xf64>
    %a1 = memref.load %A[%ip1] : memref<?xf64>
    %sum = arith.addf %a0, %a1 : f64
    %val = arith.mulf %half, %sum : f64
    memref.store %val, %B[%i] : memref<?xf64>
    sde.yield
  }
  sde.yield
}
```

No `sde.mu_dep` needed -- worksharing loops have implicit deps analyzed later.

After RaiseToLinalg + RaiseToTensor, the same code becomes:
```mlir
%B_new = sde.cu_region parallel scope(<distributed>)
    ins(%A : tensor<?xf64>)
    outs(%B : tensor<?xf64>) -> tensor<?xf64> {
^bb0(%a_in: tensor<?xf64>, %b_out: tensor<?xf64>):
  %result = sde.su_iterate (%c1) to (%Nm1) step (%c1)
      iter_args(%b_iter = %b_out : tensor<?xf64>) {
  ^bb0(%i: index, %b: tensor<?xf64>):
    %stencil = linalg.generic {
      indexing_maps = [affine_map<(d0) -> (d0 - 1)>,
                       affine_map<(d0) -> (d0 + 1)>,
                       affine_map<(d0) -> (d0)>],
      iterator_types = ["parallel"]
    } ins(%a_in, %a_in : tensor<?xf64>, tensor<?xf64>)
      outs(%b : tensor<?xf64>) {
    ^bb0(%left: f64, %right: f64, %old: f64):
      %sum = arith.addf %left, %right : f64
      %val = arith.mulf %half, %sum : f64
      linalg.yield %val : f64
    } -> tensor<?xf64>
    sde.yield %stencil : tensor<?xf64>
  } -> tensor<?xf64>
  sde.yield %result : tensor<?xf64>
}
```

Key observations in the tensor form:
- Dependencies are SSA def-use chains: `%B_new` depends on `%A` and `%B`
- Access pattern is in the linalg `indexing_maps`: stencil with +/-1 offsets
- Pattern classification is automatic: `iterator_types = ["parallel"]`
- No alias analysis, no custom dependency tracking, no `sde.mu_dep`

### Example 2: Task with Dependencies

```c
#pragma omp task depend(in: A[i:N]) depend(out: B[i:N])
{ B[i..i+N] = f(A[i..i+N]); }
```

After OMP-to-SDE:
```mlir
%dep_A = sde.mu_dep <read> %A[%i] size [%N] : memref<?xf64> -> !sde.dep
%dep_B = sde.mu_dep <write> %B[%i] size [%N] : memref<?xf64> -> !sde.dep

%done = sde.cu_task deps(%dep_A, %dep_B) {
^bb0(%A_view: memref<?xf64>, %B_view: memref<?xf64>):
  // computation using acquired views
  sde.yield
} : !sde.completion
```

After raising to tensor, the same task becomes:
```mlir
%A_slice = tensor.extract_slice %A[%i][%N][1] : tensor<?xf64> to tensor<?xf64>
%B_init  = tensor.extract_slice %B[%i][%N][1] : tensor<?xf64> to tensor<?xf64>

%result = sde.cu_task
    ins(%A_slice : tensor<?xf64>)
    outs(%B_init : tensor<?xf64>) {
^bb0(%a: tensor<?xf64>, %b: tensor<?xf64>):
  %computed = linalg.generic { ... }
    ins(%a : tensor<?xf64>) outs(%b : tensor<?xf64>) { ... } -> tensor<?xf64>
  sde.yield %computed : tensor<?xf64>
} -> tensor<?xf64>

%B_new = tensor.insert_slice %result into %B[%i][%N][1] : ...
```

`tensor.extract_slice` replaces `sde.mu_dep <read>`, `tensor.insert_slice`
replaces `sde.mu_dep <write>`, and SSA deps replace the `sde.dep` token chain.

### Example 3: Parallel Reduction

```c
#pragma omp parallel for reduction(+: sum)
for (int i = 0; i < N; i++) sum += A[i];
```

After OMP-to-SDE:
```mlir
sde.cu_region parallel scope(<distributed>) {
  %final = sde.su_iterate (%c0) to (%N) step (%c1)
      reduction [#sde<reduction_kind<add>>]
      (%sum_init : f64) identity (%zero : f64) {
  ^bb0(%i: index, %partial: f64):
    %a = memref.load %A[%i] : memref<?xf64>
    %new = arith.addf %partial, %a : f64
    sde.yield %new : f64
  } : f64
  memref.store %final, %sum_addr[] : memref<f64>
  sde.yield
}
```

Reduction kind (`add`) and identity (`0.0`) are preserved -- unlike
the current pipeline which loses the combiner region.

After raising to tensor:
```mlir
%final = sde.cu_region parallel scope(<distributed>)
    ins(%A : tensor<?xf64>) -> f64 {
^bb0(%a_in: tensor<?xf64>):
  %result = sde.su_iterate (%c0) to (%N) step (%c1)
      reduction [#sde<reduction_kind<add>>]
      iter_args(%sum = %sum_init : f64) identity (%zero : f64) {
  ^bb0(%i: index, %partial: f64):
    %a = tensor.extract %a_in[%i] : tensor<?xf64>
    %new = arith.addf %partial, %a : f64
    sde.yield %new : f64
  } : f64
  sde.yield %result : f64
}
```

Scalar reductions work identically in both paths — `f64` is not a tensor or
memref, so the reduction accumulator is the same. The difference is that the
input array `%A` is now a tensor value with SSA-tracked dependencies.

### Example 4: Taskwait with Completion Tokens

```c
#pragma omp task depend(out: x)
{ x = compute(); }
#pragma omp taskwait
use(x);
```

After OMP-to-SDE:
```mlir
%dep_x = sde.mu_dep <write> %x : memref<f64> -> !sde.dep
%task_done = sde.cu_task deps(%dep_x) {
^bb0(%x_view: memref<f64>):
  %val = func.call @compute() : () -> f64
  memref.store %val, %x_view[] : memref<f64>
  sde.yield
} : !sde.completion

sde.su_barrier(%task_done)   // explicit: wait for THIS task

%x_val = memref.load %x[] : memref<f64>
func.call @use(%x_val) : (f64) -> ()
```

Current pipeline maps `omp.taskwait` to bare `arts.barrier` with no link
to which tasks are awaited. SDE preserves the exact dependency.

---

## Information Preservation: SDE vs Current Pipeline

| Information | Current (OMP->ARTS) | SDE memref path | SDE tensor path |
|---|---|---|---|
| Reduction combiner | **LOST** | Preserved in `sde.su_iterate` + `sde.mu_reduction_decl` | Same |
| Nowait intent | **LOST** | Preserved as `nowait` attr | Same |
| Schedule + chunk | Partially preserved | Fully preserved on `sde.su_iterate` | Same |
| Collapse depth | **LOST** | Preserved as `collapse` attr | Same |
| Task completion | **LOST** (bare barrier) | Preserved via `sde.completion` tokens | Same |
| Dep regions | Split `omp_dep`/`db_control` | Unified in `sde.mu_dep` | `tensor.extract_slice` (native) |
| Aliasing info | Custom `DbAliasAnalysis` | Conservative analysis | No aliasing by construction |
| Access patterns | Custom `AccessAnalyzer` | `sde.mu_access` annotations | linalg `indexing_maps` (native) |
| Dependencies | Custom `EdtDataFlowAnalysis` | `sde.dep` tokens | SSA def-use chains (free) |
| Buffer reuse | Custom DB mode analysis | Manual | One-shot bufferization (optimal) |

---

## Dialect Composition

### SDE composes WITH natively (tensor-first)

SDE ops are type-polymorphic — their operands accept tensor, memref, and
scalar types. In the tensor path, linalg/tensor ops appear INSIDE SDE CU
bodies and as operands to SDE ops. This is native composition, not a
separate analysis step.

| Dialect | How |
|---|---|
| **tensor** | Native operand type for CU ins/outs; `tensor.extract_slice`/`tensor.insert_slice` replace `sde.mu_dep`; SSA def-use chains ARE dependency graphs |
| **linalg** | Structured computation inside CU bodies (`linalg.generic`, named ops); `iterator_types` classify patterns; `TilingInterface` enables tile-and-fuse |
| **bufferization** | One-shot bufferize: tensor -> memref with optimal in-place decisions; replaces manual DB alias analysis |
| **memref** | Fallback data type; load/store/alloc inside CU bodies for code that can't be raised |
| **arith** | Arithmetic ops inside CU bodies |
| **index** | Iteration bounds, offsets, sizes |
| **scf** | Serial control flow (`scf.for`, `scf.if`, `scf.while`) inside CU bodies |
| **affine** | Affine ops in CU bodies; analysis passes use affine dependence analysis |
| **func** | Functions contain SDE ops; calls inside CU bodies |
| **DLTI** | Data layout for footprint estimation |

### SDE CONSUMES (ops removed after conversion)

| Dialect | How |
|---|---|
| **omp** | All OMP ops consumed by OMP-to-SDE; none remain after |

### SDE is LOWERED TO

| Dialect | How |
|---|---|
| **arts** | Primary target: SDE-to-ARTS conversion (after bufferization back to memref) |
| **gpu** | Future: SDE-to-GPU for accelerator targets |

---

## SDE-to-ARTS Lowering

**Precondition**: Before SDE-to-ARTS conversion, one-shot bufferization
converts all tensor operands back to memref. After bufferization, SDE ops
are in memref form and the lowering is mechanical. `linalg-to-loops` converts
any remaining linalg ops to `scf.for`.

| SDE Op | ARTS Op(s) | Information Added |
|---|---|---|
| `sde.cu_region` (parallel) | `arts.edt <parallel>` + `arts.barrier` (if no nowait) | EdtType, EdtConcurrency |
| `sde.cu_region` (single) | `arts.edt <single>` | Intranode concurrency |
| `sde.cu_task` | `arts.edt <task>` + deps as `arts.db_control` | Dep slot bindings |
| `sde.cu_reduce` | `arts.atomic_add` or tree reduction | Based on reduction_kind |
| `sde.cu_atomic` | `arts.atomic_add` | Direct 1:1 |
| `sde.su_iterate` | `arts.for` (inside parent `arts.edt`) | ForScheduleKind |
| `sde.su_barrier` (implicit) | `arts.barrier` | No tokens |
| `sde.su_barrier` (explicit) | `arts.barrier` / `arts.wait_on_epoch` | Tokens -> epochs |
| `sde.su_distribute` | Attrs on `arts.edt` (internode + workers) | Advisory |
| `sde.mu_dep` | `arts.omp_dep` + `arts.db_control` | Two-op chain |
| `sde.mu_access` | Consumed by CreateDbs analysis | Not directly lowered |
| `sde.mu_reduction_decl` | Inlined into `arts.for` reduction operands | Combiner resolved |
| `sde.completion` type | Implicit (epoch-based) | Tokens -> epoch waits |
| `sde.yield` | `arts.yield` | 1:1 |

---

## Directory Layout

```
include/arts/dialect/sde/IR/
  SdeDialect.td
  SdeOps.td              # CU, SU, MU, yield ops
  SdeTypes.td            # CompletionType, DepType
  SdeAttrs.td            # Enums + metadata attributes
  SdeEffects.h           # ComputeResource, DataResource, SyncResource
  SdeDialect.h
  CMakeLists.txt

include/arts/dialect/sde/Transforms/
  CMakeLists.txt

lib/arts/dialect/sde/
  IR/
    SdeDialect.cpp
    SdeOps.cpp
  Analysis/
    metadata/
    ValueAnalysis.cpp
    SdeLoopAnalysis.cpp
  Transforms/
    ConvertOpenMPToSde.cpp   # 12 OMP->SDE patterns
    RaiseToLinalg.cpp        # scf.for+memref -> linalg.generic
    RaiseToTensor.cpp        # linalg on memref -> linalg on tensor
    SdeOpt.cpp               # tensor-space analysis + optimization
    LoopNormalization.cpp
    LoopReordering.cpp
    CollectMetadata.cpp

lib/arts/dialect/core/Conversion/SdeToArts/
  SdeToArtsPatterns.cpp      # SDE->ARTS lowering
```

---

## MLIR Interfaces SDE Implements

| Interface | On Which Ops | Purpose |
|---|---|---|
| `DestinationStyleOpInterface` | `sde.cu_region`, `sde.cu_task`, `sde.cu_reduce` (tensor path) | ins/outs notation; enables tile-and-fuse composition with linalg; outs are "destinations" that bufferization can potentially allocate in-place |
| `MemoryEffectsOpInterface` | All CU/SU/MU ops | Per-op effect declarations |
| `LoopLikeOpInterface` | `sde.su_iterate` | iter_args state flow (tensor or memref) |
| `RecursiveMemoryEffects` | `sde.cu_region`, `sde.cu_task` | Propagate body effects |
| `RegionBranchOpInterface` | `sde.cu_region`, `sde.cu_task`, `sde.su_iterate` | Control flow into/out of SDE regions |

## Critical Source Files (Current Codebase)

| File | Relevance |
|---|---|
| `lib/arts/passes/transforms/ConvertOpenMPToArts.cpp` | 12 OMP patterns SDE must replace |
| `include/arts/Ops.td` | ARTS op defs that SDE ops lower to |
| `include/arts/Attributes.td` | ARTS enums that SDE enums parallel |
| `include/arts/Dialect.td` | TableGen pattern for Sde_Op class |
| `include/arts/utils/OperationAttributes.h` | Attribute name pattern for SdeAttrNames.h |
