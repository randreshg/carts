# SDE Dialect Architecture

This document describes the SDE layer as it exists on the
`architecture/sde-restructuring` branch, and separates that from the broader
standalone SDE design that is still planned.

## Current Status

Today the implementation is an `arts_sde` dialect inside the CARTS codebase.
It is the first optimization boundary in the OpenMP-to-ARTS path, but it is
not yet a fully runtime-neutral standalone dialect with multiple backends.

The implemented SDE-stage pipeline is:

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

Within that pipeline:

- SDE passes make scope, schedule, chunk, reduction-strategy, linalg, and
  tensor decisions before ARTS IR is created.
- SDE optimization passes stay ARTS-unaware in their rewrites.
- `ConvertSdeToArts` is the intentional boundary where SDE decisions are
  translated into ARTS IR.
- The lowering target is still only ARTS. There is no parallel lowering path
  to Legion, StarPU, GPU, or another runtime.

## Implemented Semantics

### Implemented Ops

The current dialect surface is the one defined in
`include/arts/dialect/sde/IR/SdeOps.td`.

| Op | Implemented | Notes |
|---|---|---|
| `arts_sde.cu_region` | Yes | Models parallel and single regions, carries kind, optional scope, and `nowait`. |
| `arts_sde.cu_task` | Yes | Models task regions with explicit dependency operands. |
| `arts_sde.cu_reduce` | Yes | Represents a reduction operation with reduction kind, accumulator, partial, and identity. |
| `arts_sde.cu_atomic` | Yes | Represents atomic updates in the supported narrow forms. |
| `arts_sde.su_iterate` | Yes | Models parallel iteration spaces and carries schedule, chunk, reductions, reduction strategy, and linalg classification. |
| `arts_sde.su_barrier` | Yes | Represents synchronization points, including taskwait-style token use. |
| `arts_sde.su_distribute` | Yes | Advisory distribution wrapper for SDE-owned distribution intent. |
| `arts_sde.mu_dep` | Yes | Represents explicit memref-based dependency declarations. |
| `arts_sde.mu_access` | Yes | Represents in-body access-region annotations in the memref fallback path. |
| `arts_sde.mu_reduction_decl` | Yes | Represents module-level reduction declarations for the SDE surface. |
| `arts_sde.yield` | Yes | Terminator for SDE regions. |

### Implemented Attributes and Enums

The implemented enum set is also concrete today:

- `SdeCuKind`
- `SdeAccessMode`
- `SdeReductionKind`
- `SdeReductionStrategy`
- `SdeLinalgClassification`
- `SdeScheduleKind`
- `SdeConcurrencyScope`

These are not placeholder design notes. They are used by the current passes
and appear in contracts at the SDE boundary.

### Implemented SDE Responsibilities

The branch currently implements these SDE-owned decisions:

- `ConvertOpenMPToSde` creates `arts_sde.*` IR from OpenMP constructs and
  preserves SDE-visible loop and task semantics.
- `SdeScopeSelection` assigns or inherits local versus distributed scope for
  parallel regions.
- `SdeScheduleRefinement` refines `auto` and `runtime` schedules to concrete
  SDE schedule kinds on the supported loop subset.
- `SdeChunkOptimization` synthesizes schedule chunks for supported dynamic and
  guided loops.
- `SdeReductionStrategy` chooses `atomic`, `tree`, or `local_accumulate` as an
  SDE-owned recommendation.
- `RaiseToLinalg` creates transient linalg carriers on supported memref loop
  bodies, including the current narrow reduction subset.
- `RaiseToTensor` creates transient tensor carriers on supported linalg
  carriers, including the current disjoint-init and cast-alias-safe cases.
- `SdeTensorOptimization` performs real SDE-stage tensor/linalg
  transformations on the supported elementwise and matmul subsets.
- `ConvertSdeToArts` forwards the selected SDE semantics into ARTS and erases
  transient carrier IR.

### What "Tensor in SDE" Means Today

Tensor and linalg integration is real, but it is narrower than the original
architecture spec implied.

Implemented today:

- tensor and linalg carriers are created transiently inside supported
  `arts_sde.su_iterate` regions
- tensor/linalg structure is used for actual SDE-stage optimization, not only
  analysis
- the original memref/scalar loop body remains authoritative through the SDE
  phase
- transient carrier IR is removed before or during `ConvertSdeToArts`

Not implemented today:

- SDE ops do not have tensor-style `ins`/`outs` operand schemas
- SDE ops do not implement a destination-style tensor programming model
- the OpenMP-to-ARTS pipeline does not run a standalone one-shot bufferization
  stage between SDE and ARTS

### Runtime-Agnostic Aspiration, Stated Honestly

The implemented semantics are more runtime-agnostic than raw ARTS IR, but the
current system is not runtime-agnostic end to end.

Honest current state:

- SDE pass logic operates on SDE concepts instead of constructing `arts.for`
  or `arts.edt` directly.
- SDE schedule, scope, reduction, and tensor/linalg decisions are made before
  ARTS lowering.
- The dialect name, namespace, and only lowering target are still ARTS-owned.
- OpenMP task dependence slicing still uses a legacy `arts.omp_dep` bridge
  internally at frontend ingress before SDE IR is cleaned up.
- No alternate backend exists for the same SDE IR.

## Current Semantic Surface

The branch now ships a narrow but real SDE semantic surface beyond the original
loop/schedule shell.

### Implemented SDE Ops

| Op | Current role | Current status |
|---|---|---|
| `sde.su_distribute` | Runtime-neutral advisory wrapper for distribution intent | Implemented as an advisory region op; current lowering inlines it away |
| `sde.mu_access` | In-body access-region annotation for memref fallback | Implemented as an annotation op; current lowering erases it |
| `sde.mu_reduction_decl` | Module-level reduction symbol with type, kind, optional identity, optional custom combiner | Implemented; current lowering erases it after the SDE phase |

### Implemented SDE Types

| Type | Intended purpose | Current status |
|---|---|---|
| `sde.completion` | Typed completion token for waits and completion surfaces | Implemented as `!arts_sde.completion` |
| `sde.dep` | Typed dependency handle for explicit region dependencies | Implemented as `!arts_sde.dep` |

The branch no longer uses generic `AnyType`/`i64` placeholders for the core
SDE dependency surface:

- `arts_sde.mu_dep` produces `!arts_sde.dep`
- `arts_sde.cu_task` consumes `!arts_sde.dep`
- `arts_sde.su_barrier` accepts `!arts_sde.completion`

### Implemented MLIR Interfaces

- `LoopLikeOpInterface` is now implemented on `arts_sde.su_iterate`

### Still Planned But Not Implemented Interfaces

These remain part of the longer-term dialect direction:

- `DestinationStyleOpInterface`
- `RegionBranchOpInterface`

The current ops still rely mainly on region structure plus the existing memory
effect traits defined in `SdeOps.td`.

### Planned But Not Implemented Tensor-Native Semantics

The earlier design docs described a more tensor-native SDE surface than the
current branch provides. The following are still planned rather than shipped:

- first-class tensor `ins`/`outs` on SDE ops
- tensor-native dependency modeling as the primary SDE surface, instead of a
  best-effort raised substrate
- fully generic tensor-stage optimization across all SDE loop shapes
- replacing the memref-authoritative loop body with a tensor-authoritative SDE
  executable form

### Planned But Not Implemented Backend Neutrality

The architecture still aspires to a runtime-neutral SDE layer, but these are
still future work:

- a backend-neutral SDE namespace and packaging story
- non-ARTS lowering targets
- backend-independent distribution and resource semantics
- a fully standalone SDE dialect that can be reused outside CARTS

## Practical Reading Guide

When reading current code and tests, treat `arts_sde` as:

- an implemented optimization boundary above ARTS
- a real place for schedule, reduction, linalg, and tensor decisions
- a partially runtime-decoupled layer
- not yet the full standalone SDE dialect described in earlier speculative
  documents

When reading older architecture material that describes typed dependencies,
destination-style SDE ops, or multiple runtime targets, treat that as planned
design work unless the feature is also visible in the current branch code.
