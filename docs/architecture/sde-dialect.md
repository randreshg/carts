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

- **STATE**: Block args receive data views; `iter_args` carry loop state;
  `sde.yield` forwards values
- **DEPENDENCY**: `$deps` operands are `sde.dep` values from `sde.mu_dep`;
  each dep has access mode and region bounds
- **EFFECT**: `MemoryEffectsOpInterface` + `RecursiveMemoryEffects` trait

### `sde.cu_region` -- Parallel or Single Execution Region

Captures `omp.parallel`, `omp.single`, `omp.master`.

```mlir
// Parallel region (from omp.parallel)
sde.cu_region parallel scope(<distributed>) {
  // body -- may contain sde.su_iterate, sde.su_barrier, etc.
  sde.yield
}

// Single-execution region (from omp.master)
sde.cu_region single scope(<local>) {
  sde.yield
}
```

**Operands**: `kind` (parallel/single/task), `concurrency_scope` (optional),
`nowait` (unit attr), `deps` (Variadic<sde.dep>)

**Results**: Optional `sde.completion` token

**Region**: Single-block body terminated by `sde.yield`

**Effects**: Write on ComputeResource; RecursiveMemoryEffects propagates body

**State**: Block args correspond 1:1 to `$deps`, receiving runtime-acquired views

### `sde.cu_task` -- Task with Explicit Dependencies

Captures `omp.task` with `depend(in/out/inout)` clauses.

```mlir
%dep_A = sde.mu_dep <read> %A[%i] size [%N] : memref<?xf64> -> !sde.dep
%dep_B = sde.mu_dep <write> %B[%i] size [%N] : memref<?xf64> -> !sde.dep

%done = sde.cu_task deps(%dep_A, %dep_B) {
^bb0(%A_view: memref<?xf64>, %B_view: memref<?xf64>):
  // body uses acquired views
  sde.yield
} : !sde.completion
```

**State**: Block args receive data views for each dep

**Dependency**: `$deps` carry mode + region; write->read on overlapping regions
enforces happens-before

**Effect**: Write on ComputeResource; RecursiveMemoryEffects for body

**Result**: `sde.completion` token consumed by `sde.su_barrier`

### `sde.cu_reduce` -- Reduction Accumulation

Captures `omp.wsloop reduction(+: sum)`. **PRESERVES the combiner semantics
that the current pipeline LOSES.**

```mlir
// Named reduction (add, mul, etc.)
sde.cu_reduce add (%acc, %partial : memref<f64>, f64)
    identity (%zero : f64)

// Custom reduction with combiner region
sde.cu_reduce custom (%acc, %partial : memref<f64>, f64)
    identity (%zero : f64) combiner {
^bb0(%lhs: f64, %rhs: f64):
  %r = arith.addf %lhs, %rhs : f64
  sde.yield %r : f64
}
```

**Effect**: Read+Write on DataResource

### `sde.cu_atomic` -- Atomic Memory Update

Captures `omp.atomic.update` (currently only add{i,f}).

```mlir
sde.cu_atomic add (%addr, %val : memref<f64>, f64)
```

**Effect**: Write on DataResource

---

## SU (Scheduling Unit) Ops

SU ops carry scheduling policy without computation content.

### `sde.su_iterate` -- Parallel Iteration Space

Captures `omp.wsloop + omp.loop_nest`, `omp.taskloop`, `scf.parallel`.
Implements `LoopLikeOpInterface` for iter_args state flow.

```mlir
// Simple parallel loop (from omp.wsloop, static schedule)
sde.su_iterate (%c1) to (%Nm1) step (%c1)
    schedule(<static>, %chunk) {
^bb0(%i: index):
  // loop body
  sde.yield
}

// With reduction (preserves combiner!)
%final = sde.su_iterate (%c0) to (%N) step (%c1)
    reduction [#sde<reduction_kind<add>>]
    (%sum_init : f64) identity (%zero : f64) {
^bb0(%i: index, %partial: f64):
  %a = memref.load %A[%i] : memref<?xf64>
  %new = arith.addf %partial, %a : f64
  sde.yield %new : f64
} : f64
```

**Operands**: lb, ub, step, schedule, chunk_size, nowait, collapse,
reduction_accumulators, reduction_kinds, reduction_identities, loop_info

**State**: Induction var + reduction accumulators as block args (LoopLikeOpInterface)

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

MU ops declare data access patterns and dependencies.

### `sde.mu_dep` -- Data Dependency Declaration

Replaces the current `arts.omp_dep` -> `arts.db_control` two-op chain.

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

Used INSIDE CU bodies to annotate which subregion is actually touched
(enables precise DB mode tightening in downstream passes).

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

| Information | Current (OMP->ARTS) | SDE (OMP->SDE->ARTS) |
|---|---|---|
| Reduction combiner region | **LOST** | Preserved in `sde.su_iterate` + `sde.mu_reduction_decl` |
| Nowait intent | **LOST** (barrier decision at conversion) | Preserved as `nowait` attr |
| Schedule + chunk size | Partially preserved | Fully preserved on `sde.su_iterate` |
| Collapse depth | **LOST** | Preserved as `collapse` attr |
| Task completion tracking | **LOST** (bare `arts.barrier`) | Preserved via `sde.completion` tokens |
| Dep regions (offsets/sizes) | Split into `arts.omp_dep` -> `arts.db_control` | Unified in `sde.mu_dep` |

---

## Dialect Composition

### SDE composes WITH (uses directly)

| Dialect | How |
|---|---|
| **memref** | Data operands are `AnyMemRef`; load/store/alloc inside CU bodies |
| **arith** | Arithmetic ops inside CU bodies |
| **index** | Iteration bounds, offsets, sizes |
| **scf** | Serial control flow (`scf.for`, `scf.if`, `scf.while`) inside CU bodies |
| **affine** | Affine ops in CU bodies; analysis passes use affine analysis |
| **func** | Functions contain SDE ops; calls inside CU bodies |
| **DLTI** | Data layout for footprint estimation |

### SDE uses for ANALYSIS (raise/analyze/lower cycle)

| Dialect | How |
|---|---|
| **linalg** | scf.for+memref raised to linalg.generic for structured analysis |
| **tensor** | linalg on memref raised to tensor for alias-free SSA dep analysis |
| **bufferization** | One-shot bufferize: tensor -> memref with optimal in-place decisions |

### SDE CONSUMES (ops removed after conversion)

| Dialect | How |
|---|---|
| **omp** | All OMP ops consumed by OMP-to-SDE; none remain after |

### SDE is LOWERED TO

| Dialect | How |
|---|---|
| **arts** | Primary target: SDE-to-ARTS conversion |
| **gpu** | Future: SDE-to-GPU for accelerator targets |

---

## SDE-to-ARTS Lowering

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
| `MemoryEffectsOpInterface` | All CU/SU/MU ops | Per-op effect declarations |
| `LoopLikeOpInterface` | `sde.su_iterate` | iter_args state flow |
| `RecursiveMemoryEffects` | `sde.cu_region`, `sde.cu_task` | Propagate body effects |
| `DestinationStyleOpInterface` | linalg ops inside CU bodies | ins/outs read/write tracking |

## Critical Source Files (Current Codebase)

| File | Relevance |
|---|---|
| `lib/arts/passes/transforms/ConvertOpenMPToArts.cpp` | 12 OMP patterns SDE must replace |
| `include/arts/Ops.td` | ARTS op defs that SDE ops lower to |
| `include/arts/Attributes.td` | ARTS enums that SDE enums parallel |
| `include/arts/Dialect.td` | TableGen pattern for Sde_Op class |
| `include/arts/utils/OperationAttributes.h` | Attribute name pattern for SdeAttrNames.h |
