# CARTS Sub-Dialect Architecture

Progressive lowering from runtime-agnostic intent to ARTS runtime calls,
following IREE's proven multi-dialect pattern.

Current branch note:
- OpenMP belongs only at ingress via `ConvertOpenMPToSde`.
- SDE owns semantic pattern reasoning, cost-model-driven schedule/distribution/
  reduction choices, and transient tensor/linalg transforms.
- ARTS consumes the resulting contracts and then performs ARTS-native
  structural/runtime optimizations such as barrier cleanup, DB/EDT shaping, and
  lowering-oriented simplification.

```
  C/C++ + OpenMP
  +----------+
  | Polygeist | -- cgeist --> scf + memref + arith + omp
  +----------+
       |
  =====|=====================================================
  ||   v   SDE DIALECT (sde.*)  Stages 1-5                ||
  ||   CU: what runs  SU: how/when  MU: data access       ||
  ||   Semantic optimization boundary with transient       ||
  ||   linalg/tensor carriers on the supported subset      ||
  ==========================================================
       |
       |  +------------------------------------------------+
       |  | MLIR Infrastructure (composed, not reinvented)  |
       |  |                                                 |
       |  |  +----------+  +----------+  +----------+      |
       |  |  |  Linalg  |  |  Tensor  |  |  Affine  |      |
       |  |  | native:  |  | native:  |  | analysis:|      |
       |  |  | generic, |  | SSA deps |  | deps,    |      |
       |  |  | tile&fuse|  | no alias |  | parallel |      |
       |  |  +----------+  +----------+  +----------+      |
       |  |  +----------+  +----------+                     |
       |  |  | Bufferz  |  |  Vector  |                     |
       |  |  | tensor>m |  | codegen  |                     |
       |  |  +----------+  +----------+                     |
       |  +------------------------------------------------+
       |
       |  CU: sde.cu_region, sde.cu_task, sde.cu_reduce, sde.cu_atomic
       |    (current branch: region-based SDE ops, not yet destination-style)
       |  SU: sde.su_iterate, sde.su_barrier, sde.su_distribute
       |    (current branch: loop semantics + transient carrier recovery)
       |  MU: sde.mu_dep, sde.mu_access, sde.mu_reduction_decl
       |    (typed dep/completion surface plus narrow semantic annotations)
       |
  -----+--- SdeToArts Conversion ----------------------------
       |
  =====|=====================================================
  ||   v   ARTS CORE (arts.*)   Stages 6-16               ||
  ||   "Create EDT with 3 deps, partition as block-stencil"||
  ==========================================================
       |
       |  core/Transforms: EdtStructuralOpt, DbPartitioning,
       |    CreateDbs, EdtDistribution, CreateEpochs, ...
       |
       |  Arts Ops: arts.edt, arts.for, arts.epoch,
       |    arts.db_alloc, arts.db_acquire, arts.barrier
       |
  -----+--- ArtsToRt Lowering -------------------------------
       |
  =====|=====================================================
  ||   v   ARTS RT (arts_rt.*)  Stages 17-18              ||
  ||   "call artsEdtCreate(guid, paramv, 3)"              ||
  ==========================================================
       |
       |  Rt Ops: arts_rt.edt_create, arts_rt.rec_dep,
       |    arts_rt.create_epoch, arts_rt.dep_db_acquire
       |
  -----+--- ConvertArtsToLLVM -------------------------------
       |
  =====|=====================================================
  ||   v   LLVM  (llvm.*)                                 ||
  ||   ARTS C Runtime API + LLVM IR                       ||
  ==========================================================
```

## Document Index

| Document | Contents |
|---|---|
| [sde-dialect.md](sde-dialect.md) | SDE dialect specification: CU/SU/MU ops, types, attributes, effect system, OMP-to-SDE conversion |
| [arts-rt-dialect.md](arts-rt-dialect.md) | Phase 1 (COMPLETE): Extract 14 runtime ops into `arts_rt` dialect |
| [pipeline-redesign.md](pipeline-redesign.md) | Pipeline stages with tensor raise/analyze/bufferize cycle |
| [pass-placement.md](pass-placement.md) | All 69 passes classified into sde/patterns/core/rt/general/verify |
| [mlir-infrastructure.md](mlir-infrastructure.md) | MLIR dialect integration: linalg, tensor, bufferization, affine, vector |
| [external-techniques.md](external-techniques.md) | Techniques adopted from IREE, Halide, Legion, PaRSEC, Chapel, etc. |
| [implementation-order.md](implementation-order.md) | Phased execution plan with parallelization strategy |
| [cost-model.md](cost-model.md) | Cost model, 21 hardcoded thresholds, runtime awareness gaps, convergence framework |
| [op-classification.md](op-classification.md) | Complete op/attribute classification table, IREE patterns, operation lifecycle |
| [directory-map.md](directory-map.md) | Every source file mapped from current to target location across all 3 phases |
| [test-architecture.md](test-architecture.md) | Test decoupling from benchmarks: audit, industry conventions, migration strategy |

## Why Three Dialects?

The monolithic `arts` dialect (1260-line Ops.td, 32+ ops) mixes three
fundamentally different concerns:

1. **Semantic intent** -- what computation to perform, what data it touches
2. **Structural representation** -- EDT/DB/Epoch runtime abstractions
3. **Runtime API calls** -- 1:1 mapping to ARTS C functions

Each concern has different lifetime, stability, and optimization potential.
Splitting into `sde.*` -> `arts.*` -> `arts_rt.*` with progressive lowering
enables:

- **SDE**: Runtime-agnostic analysis using MLIR infrastructure (tensor, linalg, affine)
- **ARTS core**: Structural optimization on EDT/DB/Epoch without SDE noise
- **ARTS RT**: Clean codegen boundary with verifiable 1:1 API mapping

## Design Principles

- **CU/SU/MU separation**: Compute Units (what), Scheduling Units (how/when), Memory Units (data)
- **STATE, DEPENDENCY, EFFECT are first-class** in every CU op
- **Transient structured carriers**: the current branch raises supported SDE
  loop bodies to transient linalg/tensor carriers, optimizes there, and then
  consumes those carriers at `ConvertSdeToArts`
- **Compose with MLIR**: linalg/tensor/bufferization/affine integrate into the
  SDE pipeline, but not yet as destination-style tensor-native SDE ops
- **Lossless from OMP**: Preserve reduction combiners, nowait, collapse, schedule
- **Runtime-agnostic aspiration**: the current branch is more runtime-decoupled
  than raw ARTS IR, but still lowers only to ARTS today

## Directory Layout (Current — Phase 3 Complete)

Each dialect owns its own IREE-style directory with `IR/`, `Conversion/`,
and `Transforms/` subdirectories. Shared infrastructure (`verify/`,
`analysis/`, `utils/`) sits outside the dialect directories.

```
include/arts/
  Dialect.td, Ops.td, Attributes.td, Types.td               # Forwarding headers (redirect to dialect/core/IR/)
  Dialect.h                                                  # Public header (includes from new generated paths)
  dialect/
    sde/IR/        SdeDialect.td, SdeOps.td, SdeDialect.h  # SDE dialect
    rt/IR/         RtDialect.td, RtOps.td, RtDialect.h     # RT dialect
    core/
      IR/            Dialect.td, Ops.td, Attributes.td, Types.td  # Core TableGen definitions
      ...            Internal headers (BlockLoopStripMiningInternal.h, etc.)

lib/arts/
  dialect/
    sde/                   "this computation reads A, writes C with neighbor deps"
      IR/                  SdeDialect.cpp, SdeOps.cpp
      Transforms/          ConvertOpenMPToSde, CollectMetadata
      Conversion/SdeToArts/ SdeToArtsPatterns

    core/                  "create an EDT with 3 deps partitioned as block-stencil"
      IR/                  Dialect.cpp
      Transforms/          47 passes: EdtStructuralOpt, DbPartitioning, CreateDbs,
                           Concurrency, EdtDistribution, ForLowering, CreateEpochs,
                           EpochOpt, LoopNormalization, LoopReordering, PatternDiscovery,
                           KernelTransforms, Inliner, RaiseMemRefDimensionality, ...
      Conversion/
        OmpToArts/         ConvertOpenMPToArts (legacy, used as SDE fallback)
        ArtsToLLVM/        ConvertArtsToLLVM + core LLVM patterns

    rt/                    "call artsEdtCreate(guid, paramv, 3)"
      IR/                  RtDialect.cpp, RtOps.cpp (14 ops)
      Conversion/
        ArtsToRt/          EdtLowering, EpochLowering
        RtToLLVM/          RtToLLVMPatterns (14 patterns)
      Transforms/          DataPtrHoisting, GuidRangCallOpt, RuntimeCallOpt

  passes/verify/           11 verification barrier passes
  analysis/                shared analysis framework
  utils/                   shared utilities
```

### Current SDE Tensor/Linalg Window

```
lib/arts/dialect/sde/Transforms/ will add:
  RaiseToLinalg.cpp        supported scf.for+memref -> linalg.generic carriers
  RaiseToTensor.cpp        supported memref linalg -> tensor carriers
  SdeTensorOptimization.cpp tensor/linalg transformations on the supported subset
```
